#pragma once
#include "PluginProcessor.h"

namespace adlib
{
    const juce::Colour bg      { 0xff0f0f14 };
    const juce::Colour panel   { 0xff171720 };
    const juce::Colour outline { 0xff272735 };
    const juce::Colour accent  { 0xffff6a2b };
    const juce::Colour textDim { 0xff8a8a9c };
    const juce::Colour textHi  { 0xffeaeaf2 };
}

class AdlibLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AdlibLookAndFeel()
    {
        using namespace adlib;
        setColour (juce::Slider::rotarySliderFillColourId, accent);
        setColour (juce::Slider::rotarySliderOutlineColourId, outline);
        setColour (juce::Slider::textBoxTextColourId, textHi);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff12121a));
        setColour (juce::Slider::textBoxOutlineColourId, outline);
        setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.5f));
        setColour (juce::CaretComponent::caretColourId, accent);
        setColour (juce::Label::textColourId, textDim);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff12121a));
        setColour (juce::ComboBox::outlineColourId, outline);
        setColour (juce::ComboBox::textColourId, textHi);
        setColour (juce::ComboBox::arrowColourId, accent);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff12121a));
        setColour (juce::PopupMenu::textColourId, textHi);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace adlib;
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.f);
        const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float arcR = r - 3.f;
        const float ang = startAngle + pos * (endAngle - startAngle);
        const bool on = s.isEnabled();

        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.f, startAngle, endAngle, true);
        g.setColour (outline);
        g.strokePath (track, juce::PathStrokeType (4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.f, startAngle, ang, true);
        g.setColour (on ? accent : textDim.withAlpha (0.5f));
        g.strokePath (val, juce::PathStrokeType (4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float kr = r - 11.f;
        g.setColour (juce::Colour (0xff1e1e2a));
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f);
        g.setColour (outline.brighter (0.2f));
        g.drawEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f, 1.2f);

        juce::Path p;
        p.addRoundedRectangle (-1.8f, -kr + 3.f, 3.6f, kr * 0.45f, 1.8f);
        g.setColour (textHi);
        g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        using namespace adlib;
        auto r = b.getLocalBounds().toFloat().reduced (1.f);
        const bool on = b.getToggleState();
        g.setColour (on ? accent : juce::Colour (0xff22222d));
        g.fillRoundedRectangle (r, 4.f);
        g.setColour (on ? juce::Colours::black : textDim);
        g.setFont (juce::Font (juce::FontOptions (12.f, juce::Font::bold)));
        g.drawText (b.getButtonText(), r, juce::Justification::centred);
    }
};

class AdlibEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AdlibEditor (AdlibAudioProcessor&);
    ~AdlibEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    void setupKnob (Knob&, const char* paramId, const juce::String& text);
    static void placeKnob (Knob&, juce::Rectangle<int> cell);

    struct Panel { juce::String name; juce::Rectangle<int> bounds; };

    AdlibAudioProcessor& proc;
    AdlibLookAndFeel lnf;

    Knob hpf, drive, p1, p1mix, p2, p2mix;
    Knob dtime, dfb, dtone, dmix, rsize, rmix;
    Knob width, mix, out;

    juce::ToggleButton syncButton { "SYNC" };
    juce::ComboBox noteBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> noteAtt;

    std::vector<Panel> panels;
    juce::Rectangle<int> hintRect;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdlibEditor)
};
