#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "AetherLookAndFeel.h"
#include "License/LicenseClient.h"

// Company About screen (Master Standard §4/§5): version, credits, third-party licences, legal links.
// Backdrop-blur-style overlay with one dismiss pattern (click outside, Esc, or the close button).
class AboutPanel : public juce::Component
{
public:
    explicit AboutPanel (aether::license::LicenseClient& licenseClient) : client (licenseClient)
    {
        // R10 — buyers need this to move to another computer.
        deactivateBtn.setButtonText ("Deactivate this computer");
        deactivateBtn.onClick = [this]
        {
            deactivateBtn.setEnabled (false);
            deactivateBtn.setButtonText ("Deactivating\xe2\x80\xa6");
            client.deactivateAsync ([this] (bool ok)
            {
                deactivateBtn.setEnabled (true);
                deactivateBtn.setButtonText (ok ? "Deactivated" : "Couldn't reach the server");
            });
        };
        addAndMakeVisible (deactivateBtn);

        setWantsKeyboardFocus (true);
        closeBtn.setButtonText ("Close");
        closeBtn.onClick = [this] { dismiss(); };
        addAndMakeVisible (closeBtn);

        site.setButtonText ("amanorsac.studio");
        site.setURL (juce::URL ("https://amanorsac.studio"));
        site.setFont (AetherFonts::semi (12.f), false);
        addAndMakeVisible (site);

        licences.setMultiLine (true);
        licences.setReadOnly (true);
        licences.setScrollbarsShown (true);
        licences.setCaretVisible (false);
        licences.setFont (AetherFonts::value (10.5f));
        licences.setText (
            "THIRD-PARTY SOFTWARE\n"
            "--------------------\n"
            "JUCE (juce.com) - JUCE licence / GPLv3\n"
            "VST3 SDK - Steinberg Media Technologies GmbH (VST is a trademark of Steinberg)\n"
            "Inter typeface - (c) The Inter Project Authors, SIL Open Font License 1.1\n"
            "JetBrains Mono typeface - (c) The JetBrains Mono Project Authors, SIL Open Font License 1.1\n\n"
            "LEGAL\n"
            "-----\n"
            "EULA, Terms of Service and Privacy Policy: https://amanorsac.studio/legal\n"
            "(c) 2026 Amanorsac Studio. All rights reserved.\n");
        addAndMakeVisible (licences);
        setVisible (false);
    }

    void show()    { setVisible (true); toFront (true); grabKeyboardFocus(); }
    void dismiss() { setVisible (false); if (onDismiss) onDismiss(); }
    std::function<void()> onDismiss;

    void paint (juce::Graphics& g) override
    {
        using namespace AetherColours;
        g.fillAll (juce::Colour (0xcc07091a));                       // dimmed backdrop
        auto card = cardBounds();
        g.setColour (juce::Colour (0xff10142c));
        g.fillRoundedRectangle (card, 16.f);
        g.setColour (border);
        g.drawRoundedRectangle (card.reduced (0.5f), 16.f, 1.f);

        auto r = card.reduced (24.f);
        g.setColour (accentHot);
        g.setFont (AetherFonts::bold (26.f).withExtraKerningFactor (0.2f));
        g.drawText ("AETHER", r.removeFromTop (30.f), juce::Justification::centredLeft);
        g.setColour (textMuted);
        g.setFont (AetherFonts::caption (10.f));
        g.drawText ("DYNAMIC AIR EXCITER   \xc2\xb7   VERSION " JucePlugin_VersionString, r.removeFromTop (16.f), juce::Justification::centredLeft);
        r.removeFromTop (10.f);
        g.setColour (text);
        g.setFont (AetherFonts::ui (12.5f));
        g.drawFittedText ("Made by Stephen \"Nene\" Amanor Sackey at Amanorsac Studio, Charlottesville VA.\n"
                          "DSP, design and code: Amanorsac Studio. Built with JUCE.",
                          r.removeFromTop (40.f).toNearestInt(), juce::Justification::topLeft, 3);
    }

    void resized() override
    {
        auto r = cardBounds().reduced (24.f).toNearestInt();
        r.removeFromTop (100);
        auto bottom = r.removeFromBottom (28);
        closeBtn.setBounds (bottom.removeFromRight (90));
        bottom.removeFromRight (8);
        deactivateBtn.setBounds (bottom.removeFromRight (190));
        site.setBounds (bottom.removeFromLeft (140));
        r.removeFromBottom (10);
        licences.setBounds (r);
    }

    void mouseDown (const juce::MouseEvent& e) override { if (! cardBounds().contains (e.position)) dismiss(); }
    bool keyPressed (const juce::KeyPress& k) override  { if (k == juce::KeyPress::escapeKey) { dismiss(); return true; } return false; }

private:
    juce::Rectangle<float> cardBounds() const
    {
        return getLocalBounds().toFloat().withSizeKeepingCentre (juce::jmin (520.f, getWidth() * 0.8f), juce::jmin (360.f, getHeight() * 0.85f));
    }
    aether::license::LicenseClient& client;
    juce::TextButton closeBtn, deactivateBtn;
    juce::HyperlinkButton site;
    juce::TextEditor licences;
};
