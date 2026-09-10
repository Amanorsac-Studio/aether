#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "LicenseClient.h"
#include "../AetherLookAndFeel.h"

/*
    The activation screen (Standard §7). It covers the editor while the plugin is
    unlicensed and disappears the moment the licensed flag goes true — without the
    customer having to reopen anything (acceptance check A3).
*/
class ActivationPanel : public juce::Component,
                        private juce::Timer
{
public:
    explicit ActivationPanel (aether::license::LicenseClient& c) : client (c)
    {
        keyEditor.setJustification (juce::Justification::centred);
        keyEditor.setFont (AetherFonts::value (17.f));
        keyEditor.setTextToShowWhenEmpty ("AETH-0000-0000-0000", AetherColours::textMuted);
        keyEditor.setInputRestrictions (19, "ABCDEFabcdef0123456789-");
        keyEditor.onReturnKey = [this] { activate(); };
        keyEditor.onTextChange = [this] { status.setText ({}, juce::dontSendNotification); };
        addAndMakeVisible (keyEditor);

        activateButton.setButtonText ("Activate");
        activateButton.onClick = [this] { activate(); };
        addAndMakeVisible (activateButton);

        buyButton.setButtonText ("Get a licence");
        buyButton.setURL (juce::URL ("https://amanorsac.studio"));
        buyButton.setFont (AetherFonts::semi (13.f), false);
        addAndMakeVisible (buyButton);

        status.setJustificationType (juce::Justification::centredTop);
        status.setFont (AetherFonts::ui (13.f));
        status.setColour (juce::Label::textColourId, AetherColours::textMuted);
        addAndMakeVisible (status);

        startTimerHz (4);   // the view follows the flag (§7)
    }

    void paint (juce::Graphics& g) override
    {
        using namespace AetherColours;
        auto r = getLocalBounds().toFloat();
        g.fillAll (bg);

        auto card = r.withSizeKeepingCentre (juce::jmin (460.f, r.getWidth() - 40.f),
                                             juce::jmin (300.f, r.getHeight() - 40.f));
        g.setColour (juce::Colour (0xff161c24));
        g.fillRoundedRectangle (card, 14.f);
        g.setColour (accent.withAlpha (0.35f));
        g.drawRoundedRectangle (card.reduced (0.5f), 14.f, 1.f);

        auto text = card.reduced (28.f);
        g.setColour (juce::Colour (0xffe9eef3));
        g.setFont (AetherFonts::bold (26.f).withExtraKerningFactor (0.24f));
        g.drawText ("AETHER", text.removeFromTop (32.f), juce::Justification::centred);

        g.setColour (textMuted);
        g.setFont (AetherFonts::caption (10.f));
        g.drawText ("DYNAMIC AIR EXCITER", text.removeFromTop (18.f), juce::Justification::centred);

        text.removeFromTop (10.f);
        g.setColour (juce::Colour (0xffb7c6d0));
        g.setFont (AetherFonts::ui (13.f));
        g.drawFittedText ("Enter the licence key from My Apps on amanorsac.studio.",
                          text.removeFromTop (34.f).toNearestInt(), juce::Justification::centredTop, 2);
    }

    void resized() override
    {
        auto card = getLocalBounds().withSizeKeepingCentre (juce::jmin (460, getWidth() - 40),
                                                            juce::jmin (300, getHeight() - 40));
        auto inner = card.reduced (28);
        inner.removeFromTop (104);
        keyEditor.setBounds (inner.removeFromTop (38).reduced (20, 0));
        inner.removeFromTop (10);
        activateButton.setBounds (inner.removeFromTop (34).reduced (70, 0));
        inner.removeFromTop (6);
        status.setBounds (inner.removeFromTop (40));
        buyButton.setBounds (inner.removeFromTop (22));
    }

private:
    void timerCallback() override
    {
        if (client.isLicensed() && isVisible())
            setVisible (false);
    }

    void activate()
    {
        const auto key = keyEditor.getText().trim().toUpperCase();
        if (key.isEmpty()) return;

        activateButton.setEnabled (false);
        status.setColour (juce::Label::textColourId, AetherColours::textMuted);
        status.setText ("Checking\xe2\x80\xa6", juce::dontSendNotification);

        client.activateAsync (key, [this] (aether::license::Result result, juce::String serverMessage)
        {
            activateButton.setEnabled (true);
            const bool ok = result == aether::license::Result::activated;
            status.setColour (juce::Label::textColourId,
                              ok ? AetherColours::accent : juce::Colour (0xffff8a7a));
            // Never report success on anything but a verified proof (R4).
            status.setText (aether::license::describe (result, serverMessage), juce::dontSendNotification);
            if (ok) setVisible (false);
        });
    }

    aether::license::LicenseClient& client;
    juce::TextEditor keyEditor;
    juce::TextButton activateButton;
    juce::HyperlinkButton buyButton;
    juce::Label status;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ActivationPanel)
};
