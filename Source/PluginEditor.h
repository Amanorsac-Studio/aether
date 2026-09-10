#pragma once
#include "PluginProcessor.h"
#include "AetherLookAndFeel.h"
#include "SpectrumDisplay.h"
#include "AboutPanel.h"
#include "License/ActivationPanel.h"

//==============================================================================
// A labelled rotary with the value readout under it; accent colour drives the ring/glow.
class AetherKnob : public juce::Component
{
public:
    AetherKnob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& title,
                juce::Colour accent, float titleSize = 11.f)
        : attachment (s, id, slider)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.getProperties().set ("accent", (juce::int64) accent.getARGB());
        slider.setDoubleClickReturnValue (true, s.getParameter (id)->convertFrom0to1 (s.getParameter (id)->getDefaultValue()));
        slider.onValueChange = [this] { value.setText (slider.getTextFromValue (slider.getValue()), juce::dontSendNotification); };
        slider.setPopupDisplayEnabled (false, false, nullptr);
        // Company knob contract: drag + scroll-wheel + keyboard, double-click reset, Shift = fine adjustment
        slider.setScrollWheelEnabled (true);
        slider.setWantsKeyboardFocus (true);
        slider.setVelocityModeParameters (0.12, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
        slider.setTitle (title);
        slider.setDescription (title);
        addAndMakeVisible (slider);

        label.setText (title, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (AetherFonts::caption (titleSize));
        label.setColour (juce::Label::textColourId, AetherColours::textPrimary);
        addAndMakeVisible (label);

        value.setJustificationType (juce::Justification::centred);
        value.setFont (AetherFonts::semi (11.f));
        value.setColour (juce::Label::textColourId, AetherColours::textPrimary);
        juce::ignoreUnused (accent);
        value.setText (slider.getTextFromValue (slider.getValue()), juce::dontSendNotification);
        addAndMakeVisible (value);
    }

    void resized() override
    {
        // Title, knob and readout stack tightly around the knob (no dead space on tall bounds)
        auto r = getLocalBounds();
        const int knobSize = juce::jmin (r.getWidth(), r.getHeight() - 30);
        const int totalH   = 14 + knobSize + 14;
        auto stack = r.withSizeKeepingCentre (r.getWidth(), totalH);
        label.setBounds (stack.removeFromTop (14));
        value.setBounds (stack.removeFromBottom (14));
        slider.setBounds (stack.withSizeKeepingCentre (knobSize, knobSize));
    }

    juce::Slider slider;

private:
    juce::Label label, value;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

//==============================================================================
class OutputMeter : public juce::Component, private juce::Timer
{
public:
    explicit OutputMeter (AetherAudioProcessor& p) : proc (p) { startTimerHz (30); }
private:
    void timerCallback() override
    {
        for (int i = 0; i < 2; ++i) { const float t = proc.getOutputPeakDb (i); lvl[i] += (t - lvl[i]) * (t > lvl[i] ? 0.8f : 0.15f); }
        ag += (proc.getAutoGainDb() - ag) * 0.2f;
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        using namespace AetherColours;
        auto r = getLocalBounds().toFloat();
        g.setFont (AetherFonts::caption (9.5f));
        g.setColour (textPrimary);
        g.drawText ("OUT", r.removeFromTop (12.f), juce::Justification::centred);
        auto agArea = r.removeFromBottom (13.f);
        g.setFont (AetherFonts::value (9.5f));
        g.setColour (textPrimary);
        g.drawText ((ag >= 0 ? "+" : "") + juce::String (ag, 1) + " dB", agArea, juce::Justification::centred);
        auto lr = r.removeFromBottom (12.f);
        r = r.reduced (2.f, 3.f);
        const float gap = 6.f, w = (r.getWidth() - gap) * 0.5f;
        const int segs = 24;
        const float segH = r.getHeight() / segs;
        for (int i = 0; i < 2; ++i)
        {
            auto bar = juce::Rectangle<float> (r.getX() + i * (w + gap), r.getY(), w, r.getHeight());
            g.setColour (juce::Colour (0xff1a222b)); g.fillRoundedRectangle (bar.expanded (2.f, 2.f), 3.f);
            const float norm = juce::jlimit (0.f, 1.f, (lvl[i] + 60.f) / 60.f);
            const int lit = (int) std::round (norm * segs);
            for (int sIdx = 0; sIdx < segs; ++sIdx)
            {
                auto seg = juce::Rectangle<float> (bar.getX(), bar.getBottom() - (sIdx + 1) * segH, bar.getWidth(), segH).reduced (0.5f, 0.8f);
                const bool on = sIdx < lit;
                const bool hot = sIdx >= segs - 2;
                juce::Colour c = hot ? danger : accent;
                g.setColour (on ? c : c.withAlpha (0.12f));
                g.fillRect (seg);
            }
            g.setFont (AetherFonts::value (9.f)); g.setColour (textPrimary);
            g.drawText (i == 0 ? "L" : "R", lr.withX (bar.getX()).withWidth (bar.getWidth()), juce::Justification::centred);
        }
    }
    AetherAudioProcessor& proc;
    float lvl[2] { -100.f, -100.f }, ag = 0.f;
};

//==============================================================================
class AetherAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit AetherAudioProcessorEditor (AetherAudioProcessor&);
    ~AetherAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void refreshPresetBox();
    void showPresetMenu();
    void savePresetDialog();
    std::unique_ptr<juce::AlertWindow> saveDialog;

    AetherAudioProcessor& proc;
    AetherLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    SpectrumDisplay spectrum { proc };
    OutputMeter meter { proc };
    AboutPanel about { proc.getLicense() };
    ActivationPanel activation { proc.getLicense() };
    juce::TextButton aboutBtn { "i" };

    AetherKnob presence { proc.apvts, ParamID::presence, "PRESENCE", AetherColours::violet, 12.f };
    AetherKnob presenceFreq { proc.apvts, ParamID::presenceFreq, "FREQ", AetherColours::violet, 9.5f };
    AetherKnob air { proc.apvts, ParamID::air, "AIR", AetherColours::gold, 12.f };
    AetherKnob airFreq { proc.apvts, ParamID::airFreq, "FREQ", AetherColours::gold, 9.5f };
    AetherKnob glow { proc.apvts, ParamID::glow, "GLOW", juce::Colour (0xffFF9F6B) };
    AetherKnob guard { proc.apvts, ParamID::guard, "GUARD", AetherColours::cyan };
    AetherKnob focus { proc.apvts, ParamID::focus, "FOCUS", juce::Colour (0xffC8B5FF) };
    AetherKnob mix { proc.apvts, ParamID::mix, "MIX", AetherColours::gold };
    AetherKnob trim { proc.apvts, ParamID::trim, "TRIM", AetherColours::gold };

    juce::ToggleButton autoGainBtn { "Auto Gain" }, bypassBtn { "Bypass" };
    juce::ComboBox oversampleBox;
    juce::TextButton presetBox;   // styled like a dropdown, opens a sectioned menu (Factory / User / Save)
    juce::AudioProcessorValueTreeState::ButtonAttachment autoGainAtt { proc.apvts, ParamID::autoGain, autoGainBtn };
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAtt { proc.apvts, ParamID::bypass, bypassBtn };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> osAtt;   // created after items are added

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AetherAudioProcessorEditor)
};
