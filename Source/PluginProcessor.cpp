#include "PluginProcessor.h"
#include "PluginEditor.h"

static const juce::StringArray kNoteNames { "1/2", "1/4", "1/4 dotted", "1/4 triplet", "1/8", "1/8 dotted",
                                            "1/8 triplet", "1/16", "1/16 dotted", "1/16 triplet", "1/32" };
static const float kNoteBeats[] = { 2.f, 1.f, 1.5f, 2.f / 3.f, 0.5f, 0.75f, 1.f / 3.f, 0.25f, 0.375f, 1.f / 6.f, 0.125f };

juce::AudioProcessorValueTreeState::ParameterLayout AdlibAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // scale: множитель для показа (0..1 -> 0..100 %), dec: знаков после запятой
    auto add = [&] (const char* id, const char* name, float lo, float hi, float def,
                    float skew, float step, const juce::String& unit, float scale, int dec)
    {
        auto attrs = juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([unit, scale, dec] (float v, int)
                { return juce::String (v * scale, dec) + (unit.isEmpty() ? juce::String() : " " + unit); })
            .withValueFromStringFunction ([scale] (const juce::String& t)
                { return t.getFloatValue() / scale; });

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> (lo, hi, step, skew), def, attrs));
    };

    add ("hpf",   "Low Cut",         40.f, 600.f, 140.f, 0.4f, 1.f, "Hz", 1.f, 0);
    add ("drive", "Drive",           0.f, 1.f, 0.25f, 1.f, 0.01f, "%", 100.f, 0);
    add ("p1",    "Voice 1 Pitch",  -24.f, 24.f, -12.f, 1.f, 1.f, "st", 1.f, 0);
    add ("p1mix", "Voice 1 Level",   0.f, 1.f, 0.5f, 1.f, 0.01f, "%", 100.f, 0);
    add ("p2",    "Voice 2 Pitch",  -24.f, 24.f, 12.f, 1.f, 1.f, "st", 1.f, 0);
    add ("p2mix", "Voice 2 Level",   0.f, 1.f, 0.25f, 1.f, 0.01f, "%", 100.f, 0);

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "dsync", 1 }, "Delay Sync", true));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "dnote", 1 }, "Delay Note", kNoteNames, 4));

    add ("dtime", "Delay Time",      60.f, 1000.f, 300.f, 0.5f, 1.f, "ms", 1.f, 0);
    add ("dfb",   "Feedback",        0.f, 0.9f, 0.45f, 1.f, 0.01f, "%", 100.f, 0);
    add ("dtone", "Delay Tone",      1000.f, 12000.f, 4500.f, 0.4f, 10.f, "Hz", 1.f, 0);
    add ("dmix",  "Delay Mix",       0.f, 1.f, 0.3f, 1.f, 0.01f, "%", 100.f, 0);
    add ("rsize", "Reverb Size",     0.f, 1.f, 0.75f, 1.f, 0.01f, "%", 100.f, 0);
    add ("rmix",  "Reverb Mix",      0.f, 1.f, 0.35f, 1.f, 0.01f, "%", 100.f, 0);
    add ("width", "Stereo Width",    0.f, 2.f, 1.3f, 1.f, 0.01f, "%", 100.f, 0);
    add ("mix",   "Dry/Wet",         0.f, 1.f, 1.f, 1.f, 0.01f, "%", 100.f, 0);
    add ("out",   "Output",         -24.f, 12.f, 0.f, 1.f, 0.1f, "dB", 1.f, 1);
    return layout;
}

AdlibAudioProcessor::AdlibAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

bool AdlibAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainInputChannelSet()  == juce::AudioChannelSet::stereo();
}

void AdlibAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    for (int c = 0; c < 2; ++c)
    {
        hpf[c].prepare (spec);
        hpf[c].setType (juce::dsp::StateVariableTPTFilterType::highpass);
        hpf[c].reset();
        shift1[c].prepare (sr);
        shift2[c].prepare (sr);
        delay[c].prepare ((int) (sr * 3.0));
        fbLp[c] = 0.f;
    }
    delaySamples.reset (sr, 0.15);
    delaySamples.setCurrentAndTargetValue ((float) (0.3 * sr));
    reverb.reset();
    dryBuf.setSize (2, samplesPerBlock);
    voiceBuf.setSize (2, samplesPerBlock);
    revBuf.setSize (2, samplesPerBlock);
}

void AdlibAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2) return;

    for (auto* b : { &dryBuf, &voiceBuf, &revBuf })
        if (b->getNumSamples() < n) b->setSize (2, n, false, false, true);

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const float drive = get ("drive");
    const float g     = 1.f + drive * 9.f;
    const float gComp = 1.f / std::sqrt (g);
    const float ratio1 = std::pow (2.f, get ("p1") / 12.f);
    const float ratio2 = std::pow (2.f, get ("p2") / 12.f);
    const float p1mix = get ("p1mix"), p2mix = get ("p2mix");
    const float fb = get ("dfb"), dmix = get ("dmix");
    const float a = std::exp (-juce::MathConstants<float>::twoPi * get ("dtone") / (float) sr);
    const float rmix = get ("rmix"), width = get ("width"), mix = get ("mix");
    const float outGain = juce::Decibels::decibelsToGain (get ("out"));

    // темп из DAW (если хост не отдаёт — 120 BPM)
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = *b;
    if (bpm < 20.0 || bpm > 400.0) bpm = 120.0;

    const bool sync = get ("dsync") > 0.5f;
    const int noteIdx = juce::jlimit (0, (int) kNoteNames.size() - 1, (int) std::round (get ("dnote")));
    const double delayMs = sync ? (double) kNoteBeats[noteIdx] * 60000.0 / bpm : (double) get ("dtime");
    delaySamples.setTargetValue (juce::jlimit (32.f, (float) (sr * 2.9), (float) (delayMs * 0.001 * sr)));
    for (int c = 0; c < 2; ++c) hpf[c].setCutoffFrequency (get ("hpf"));

    juce::Reverb::Parameters rp;
    rp.roomSize = get ("rsize");
    rp.damping = 0.4f;
    rp.wetLevel = 1.f;
    rp.dryLevel = 0.f;
    rp.width = 1.f;
    reverb.setParameters (rp);

    for (int c = 0; c < 2; ++c)
        dryBuf.copyFrom (c, 0, buffer, c, 0, n);

    for (int i = 0; i < n; ++i)
    {
        float voice[2];
        for (int c = 0; c < 2; ++c)
        {
            float x = hpf[c].processSample (0, dryBuf.getSample (c, i));
            x = std::tanh (g * x) * gComp;
            voice[c] = x + p1mix * shift1[c].process (x, ratio1)
                         + p2mix * shift2[c].process (x, ratio2);
        }

        // ping-pong delay
        const float d = delaySamples.getNextValue();
        const float dl = delay[0].read (d), dr = delay[1].read (d);
        fbLp[0] = (1.f - a) * dl + a * fbLp[0];
        fbLp[1] = (1.f - a) * dr + a * fbLp[1];
        const float in = 0.5f * (voice[0] + voice[1]);
        delay[0].write (in + fbLp[1] * fb);
        delay[1].write (fbLp[0] * fb);

        const float l = voice[0] + dmix * dl;
        const float r = voice[1] + dmix * dr;
        voiceBuf.setSample (0, i, l);
        voiceBuf.setSample (1, i, r);
        revBuf.setSample (0, i, l);
        revBuf.setSample (1, i, r);
    }

    reverb.processStereo (revBuf.getWritePointer (0), revBuf.getWritePointer (1), n);

    for (int i = 0; i < n; ++i)
    {
        float l = voiceBuf.getSample (0, i) + rmix * revBuf.getSample (0, i);
        float r = voiceBuf.getSample (1, i) + rmix * revBuf.getSample (1, i);

        const float m = 0.5f * (l + r), s = 0.5f * (l - r) * width;
        l = m + s;
        r = m - s;

        buffer.setSample (0, i, (dryBuf.getSample (0, i) * (1.f - mix) + l * mix) * outGain);
        buffer.setSample (1, i, (dryBuf.getSample (1, i) * (1.f - mix) + r * mix) * outGain);
    }
}

juce::AudioProcessorEditor* AdlibAudioProcessor::createEditor()
{
    return new AdlibEditor (*this);
}

void AdlibAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void AdlibAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AdlibAudioProcessor();
}
