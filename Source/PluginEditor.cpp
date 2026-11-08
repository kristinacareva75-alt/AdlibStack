#include "PluginEditor.h"

AdlibEditor::AdlibEditor (AdlibAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    setupKnob (hpf,   "hpf",   "LOW CUT");
    setupKnob (drive, "drive", "DRIVE");
    setupKnob (p1,    "p1",    "V1 PITCH");
    setupKnob (p1mix, "p1mix", "V1 LEVEL");
    setupKnob (p2,    "p2",    "V2 PITCH");
    setupKnob (p2mix, "p2mix", "V2 LEVEL");
    setupKnob (dtime, "dtime", "TIME");
    setupKnob (dfb,   "dfb",   "FEEDBACK");
    setupKnob (dtone, "dtone", "TONE");
    setupKnob (dmix,  "dmix",  "MIX");
    setupKnob (rsize, "rsize", "SIZE");
    setupKnob (rmix,  "rmix",  "MIX");
    setupKnob (width, "width", "WIDTH");
    setupKnob (mix,   "mix",   "DRY / WET");
    setupKnob (out,   "out",   "OUTPUT");

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("dnote")))
        noteBox.addItemList (choice->choices, 1);
    addAndMakeVisible (noteBox);
    addAndMakeVisible (syncButton);
    syncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "dsync", syncButton);
    noteAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "dnote", noteBox);

    setSize (760, 580);
    timerCallback();
    startTimerHz (10);
}

AdlibEditor::~AdlibEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void AdlibEditor::setupKnob (Knob& k, const char* paramId, const juce::String& text)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 20);
    k.slider.setTextBoxIsEditable (true);   // клик по числу -> ввод с клавиатуры
    k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);

    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    k.label.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
}

void AdlibEditor::placeKnob (Knob& k, juce::Rectangle<int> cell)
{
    k.label.setBounds (cell.removeFromTop (16));
    k.slider.setBounds (cell);
}

void AdlibEditor::timerCallback()
{
    const bool sync = proc.apvts.getRawParameterValue ("dsync")->load() > 0.5f;
    dtime.slider.setEnabled (! sync);
    dtime.slider.setAlpha (sync ? 0.35f : 1.f);
    dtime.label.setAlpha (sync ? 0.35f : 1.f);
    noteBox.setEnabled (sync);
    noteBox.setAlpha (sync ? 1.f : 0.35f);
}

void AdlibEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (52);

    const int gap = 10, rowH = 160, capH = 26;
    const int cellW = area.getWidth() / 6;

    auto rowRect = [&] (int r)
    {
        return juce::Rectangle<int> (area.getX(), area.getY() + r * (rowH + gap), cellW * 6, rowH);
    };
    auto cell = [&] (juce::Rectangle<int> row, int idx)
    {
        return juce::Rectangle<int> (row.getX() + idx * cellW, row.getY() + capH, cellW, rowH - capH - 4).reduced (2, 0);
    };

    panels.clear();
    auto addPanel = [&] (const juce::String& n, juce::Rectangle<int> row, int from, int count)
    {
        panels.push_back ({ n, juce::Rectangle<int> (row.getX() + from * cellW, row.getY(), cellW * count, rowH).reduced (3, 0) });
    };

    const auto r1 = rowRect (0), r2 = rowRect (1), r3 = rowRect (2);

    addPanel ("INPUT", r1, 0, 2);
    addPanel ("VOICES", r1, 2, 4);
    placeKnob (hpf,   cell (r1, 0));
    placeKnob (drive, cell (r1, 1));
    placeKnob (p1,    cell (r1, 2));
    placeKnob (p1mix, cell (r1, 3));
    placeKnob (p2,    cell (r1, 4));
    placeKnob (p2mix, cell (r1, 5));

    addPanel ("DELAY", r2, 0, 4);
    addPanel ("REVERB", r2, 4, 2);
    placeKnob (dtime, cell (r2, 0));
    placeKnob (dfb,   cell (r2, 1));
    placeKnob (dtone, cell (r2, 2));
    placeKnob (dmix,  cell (r2, 3));
    placeKnob (rsize, cell (r2, 4));
    placeKnob (rmix,  cell (r2, 5));

    const auto dp = panels[2].bounds;
    noteBox.setBounds (dp.getRight() - 10 - 110, dp.getY() + 4, 110, 22);
    syncButton.setBounds (noteBox.getX() - 6 - 64, dp.getY() + 4, 64, 22);

    addPanel ("OUTPUT", r3, 0, 3);
    placeKnob (width, cell (r3, 0));
    placeKnob (mix,   cell (r3, 1));
    placeKnob (out,   cell (r3, 2));

    hintRect = juce::Rectangle<int> (r3.getX() + cellW * 3 + 14, r3.getY() + 30, cellW * 3 - 20, 100);
}

void AdlibEditor::paint (juce::Graphics& g)
{
    using namespace adlib;
    g.fillAll (bg);

    g.setColour (textHi);
    g.setFont (juce::Font (juce::FontOptions (26.f, juce::Font::bold)));
    g.drawText ("ADLIB STACK", 20, 14, 300, 32, juce::Justification::centredLeft);
    g.setColour (accent);
    g.fillRoundedRectangle (20.f, 48.f, 46.f, 3.f, 1.5f);
    g.setColour (textDim);
    g.setFont (juce::Font (juce::FontOptions (12.f)));
    g.drawText ("ad-lib vocal chain  -  vol. 2", 76, 42, 240, 16, juce::Justification::centredLeft);

    for (auto& p : panels)
    {
        auto b = p.bounds.toFloat();
        g.setColour (panel);
        g.fillRoundedRectangle (b, 8.f);
        g.setColour (outline);
        g.drawRoundedRectangle (b.reduced (0.5f), 8.f, 1.f);
        g.setColour (accent);
        g.setFont (juce::Font (juce::FontOptions (12.f, juce::Font::bold)));
        g.drawText (p.name, p.bounds.getX() + 12, p.bounds.getY() + 4, 120, 22, juce::Justification::centredLeft);
    }

    g.setColour (textDim);
    g.setFont (juce::Font (juce::FontOptions (12.5f)));
    g.drawFittedText ("Клик по числу под ручкой - ввести значение с клавиатуры.\n"
                      "Двойной клик по ручке - сброс.\n"
                      "SYNC - задержка по темпу проекта.",
                      hintRect, juce::Justification::topLeft, 4);
}
