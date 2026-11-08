#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// Простой гранулярный питч-шифтер (две перекрывающиеся линии задержки)
struct PitchShifter
{
    std::vector<float> buf;
    int w = 0;
    float phase = 0.f, win = 1.f;

    void prepare (double sr)
    {
        win = (float) (0.045 * sr);
        buf.assign ((size_t) win * 2 + 8, 0.f);
        w = 0;
        phase = 0.f;
    }

    float tap (float p) const
    {
        const int sz = (int) buf.size();
        float r = (float) w - (p * win + 1.f);
        while (r < 0.f) r += (float) sz;
        const int i0 = (int) r % sz;
        const float f = r - std::floor (r);
        return buf[(size_t) i0] * (1.f - f) + buf[(size_t) ((i0 + 1) % sz)] * f;
    }

    float process (float x, float ratio)
    {
        buf[(size_t) w] = x;
        phase += (1.f - ratio) / win;
        phase -= std::floor (phase);
        float p2 = phase + 0.5f;
        if (p2 >= 1.f) p2 -= 1.f;
        const float s1 = std::sin (juce::MathConstants<float>::pi * phase);
        const float s2 = std::sin (juce::MathConstants<float>::pi * p2);
        const float out = tap (phase) * s1 * s1 + tap (p2) * s2 * s2;
        w = (w + 1) % (int) buf.size();
        return out;
    }
};

struct DelayLine
{
    std::vector<float> b;
    int w = 0;

    void prepare (int maxSamples) { b.assign ((size_t) maxSamples + 4, 0.f); w = 0; }
    void write (float x) { b[(size_t) w] = x; w = (w + 1) % (int) b.size(); }
    float read (float d) const
    {
        const int sz = (int) b.size();
        float r = (float) w - d;
        while (r < 0.f) r += (float) sz;
        const int i0 = (int) r % sz;
        const float f = r - std::floor (r);
        return b[(size_t) i0] * (1.f - f) + b[(size_t) ((i0 + 1) % sz)] * f;
    }
};

class AdlibAudioProcessor : public juce::AudioProcessor
{
public:
    AdlibAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Adlib Stack"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    double sr = 44100.0;
    juce::dsp::StateVariableTPTFilter<float> hpf[2];
    PitchShifter shift1[2], shift2[2];
    DelayLine delay[2];
    float fbLp[2] { 0.f, 0.f };
    juce::SmoothedValue<float> delaySamples;
    juce::Reverb reverb;
    juce::AudioBuffer<float> dryBuf, voiceBuf, revBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdlibAudioProcessor)
};
