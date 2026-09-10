#include "PluginEditor.h"

using namespace juce;
using namespace AetherColours;

static constexpr int kW = 820, kH = 500;

//==============================================================================
AetherAudioProcessorEditor::AetherAudioProcessorEditor (AetherAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    for (Component* c : std::initializer_list<Component*> { &spectrum, &meter, &presence, &presenceFreq, &air, &airFreq, &glow, &guard,
                                                            &focus, &mix, &trim, &autoGainBtn, &bypassBtn, &oversampleBox, &presetBox,
                                                            &aboutBtn })
        addAndMakeVisible (c);
    addChildComponent (about);
    // The activation screen covers everything until the licence flag turns true (§7).
    addChildComponent (activation);
    activation.setVisible (! proc.isLicensed());
    proc.getLicense().onLicenseStateChanged = [this]
    {
        activation.setVisible (! proc.isLicensed());
        if (proc.isLicensed()) activation.toBack(); else activation.toFront (true);
    };
    about.onDismiss = [this] { aboutBtn.grabKeyboardFocus(); };

    oversampleBox.addItemList ({ "OS Off", "OS 2x", "OS 4x" }, 1);
    oversampleBox.setTooltip ("Oversampling keeps Glow alias-free. 2x is the sweet spot.");
    oversampleBox.setTitle ("Oversampling");
    osAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ParamID::oversample, oversampleBox);

    presetBox.setTooltip ("Presets — factory, your own (saved in Documents/Amanorsac Studio/AETHER/Presets), or save the current sound.");
    presetBox.setTitle ("Presets");
    presetBox.onClick = [this] { showPresetMenu(); };
    presetBox.getProperties().set ("dropdown", true);
    refreshPresetBox();

    aboutBtn.setTooltip ("About AETHER — version, credits, licences");
    aboutBtn.setTitle ("About");
    aboutBtn.onClick = [this] { about.show(); };

    bypassBtn.setTitle ("Bypass");
    autoGainBtn.setTitle ("Auto Gain");

    presence.slider.setTooltip ("Upper-mid lift (bite, articulation). Tune the centre with FREQ. Shift-drag for fine control.");
    air.slider.setTooltip ("Top-octave shelf — silk and sparkle. Tune where it starts with FREQ. Shift-drag for fine control.");
    glow.slider.setTooltip ("Harmonic excitation: generates new top end instead of just boosting what's there.");
    guard.slider.setTooltip ("Program-dependent limiter on the boost. Pulls back when 5–9 kHz gets harsh.");
    focus.slider.setTooltip ("Where the air lives: left = centre (vocal), right = sides (width).");
    mix.slider.setTooltip ("Parallel blend of the processed signal.");
    trim.slider.setTooltip ("Output trim.");
    autoGainBtn.setTooltip ("Loudness-match the output to the input so brighter never fools you into 'louder'.");

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) kW / kH);
    setResizeLimits (620, 378, 1400, 853);
    setSize (kW, kH);
}

AetherAudioProcessorEditor::~AetherAudioProcessorEditor() { setLookAndFeel (nullptr); }

//==============================================================================
void AetherAudioProcessorEditor::refreshPresetBox()
{
    presetBox.setButtonText (proc.getCurrentPresetName());
}

void AetherAudioProcessorEditor::showPresetMenu()
{
    PopupMenu menu;
    menu.setLookAndFeel (&lnf);
    menu.addSectionHeader ("FACTORY");
    const auto& factory = AetherAudioProcessor::getFactoryPresets();
    for (int i = 0; i < (int) factory.size(); ++i)
        menu.addItem (100 + i, factory[(size_t) i].name, true, proc.getCurrentPresetName() == factory[(size_t) i].name);

    auto user = proc.listUserPresets();
    if (! user.isEmpty())
    {
        menu.addSectionHeader ("USER");
        for (int i = 0; i < user.size(); ++i)
            menu.addItem (1000 + i, user[i].getFileNameWithoutExtension(), true,
                          proc.getCurrentPresetName() == user[i].getFileNameWithoutExtension());
    }
    menu.addSeparator();
    menu.addItem (1, String::fromUTF8 ("Save preset\xe2\x80\xa6"));
    menu.addItem (2, "Show presets folder");

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (presetBox).withMinimumWidth (presetBox.getWidth()),
        [this, user] (int result)
        {
            if (result == 0) return;
            if (result == 1)      savePresetDialog();
            else if (result == 2) { auto d = AetherAudioProcessor::getUserPresetDir(); d.createDirectory(); d.revealToUser(); }
            else if (result >= 1000) proc.loadUserPreset (user[result - 1000]);
            else if (result >= 100)  proc.setCurrentProgram (result - 100);
            refreshPresetBox();
        });
}

void AetherAudioProcessorEditor::savePresetDialog()
{
    saveDialog = std::make_unique<AlertWindow> ("Save preset", "Name this sound. It goes to Documents/Amanorsac Studio/AETHER/Presets.",
                                                MessageBoxIconType::NoIcon, this);
    saveDialog->setLookAndFeel (&lnf);
    saveDialog->addTextEditor ("name", proc.getCurrentPresetName() == "Init" ? String ("My Air") : proc.getCurrentPresetName(), "Preset name");
    saveDialog->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    saveDialog->enterModalState (true, ModalCallbackFunction::create ([this] (int r)
    {
        if (r == 1)
        {
            const auto name = saveDialog->getTextEditorContents ("name").trim();
            if (name.isNotEmpty()) proc.saveUserPreset (name);
        }
        saveDialog->setLookAndFeel (nullptr);
        saveDialog.reset();
        refreshPresetBox();
    }), false);
}

//==============================================================================
void AetherAudioProcessorEditor::paint (Graphics& g)
{
    const float s = (float) getWidth() / kW;
    auto r = getLocalBounds().toFloat();
    g.fillAll (bg);                                                     // dark chassis

    // Header bar (silver) and deck (brushed silver) — analyser sits between them as its own component
    auto headerBar = r.removeFromTop (58 * s).reduced (10 * s, 0).withTrimmedTop (10 * s);
    auto deckBar   = r.removeFromBottom (232 * s).reduced (10 * s, 0).withTrimmedBottom (10 * s);

    auto silver = [&] (Rectangle<float> a, float corner, Colour top, Colour bottom)
    {
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (a.translated (0, 2.f * s), corner);
        g.setGradientFill (ColourGradient (top, a.getX(), a.getY(), bottom, a.getX(), a.getBottom(), false));
        g.fillRoundedRectangle (a, corner);
        // brushed texture: fine horizontal strokes
        g.setColour (Colours::white.withAlpha (0.10f));
        for (float y = a.getY() + 2; y < a.getBottom(); y += 3.f) g.drawHorizontalLine ((int) y, a.getX() + corner, a.getRight() - corner);
        g.setColour (Colours::black.withAlpha (0.04f));
        for (float y = a.getY() + 3.5f; y < a.getBottom(); y += 3.f) g.drawHorizontalLine ((int) y, a.getX() + corner, a.getRight() - corner);
        g.setColour (Colours::white.withAlpha (0.55f));
        g.drawRoundedRectangle (a.reduced (0.5f), corner, 1.f);
        g.setColour (Colours::black.withAlpha (0.25f));
        g.drawRoundedRectangle (a.expanded (0.5f), corner + 0.5f, 1.f);
    };
    silver (headerBar, 8 * s, header, header.darker (0.06f));
    silver (deckBar,   8 * s, panel,  panelDark);

    // Wordmark, motif, subtitle
    const Font wordmark = AetherFonts::bold (28.f * s).withExtraKerningFactor (0.32f);
    const float wordW = wordmark.getStringWidthFloat ("AETHER");
    g.setColour (textPrimary);
    g.setFont (wordmark);
    g.drawText ("AETHER", headerBar.withTrimmedLeft (26 * s), Justification::centredLeft);
    {
        Path wave;
        const float x0 = 26 * s + wordW + 16 * s, cy = headerBar.getCentreY(), h = 9 * s;
        for (int i = 0; i < 9; ++i)
        {
            const float bh = h * (i == 4 ? 1.f : i == 3 || i == 5 ? 0.7f : i == 2 || i == 6 ? 0.45f : 0.25f);
            wave.addRectangle (x0 + i * 2.6f * s, cy - bh, 1.3f * s, bh * 2);
        }
        g.setColour (textPrimary.withAlpha (0.8f));
        g.fillPath (wave);
    }
    g.setColour (textMuted);
    g.setFont (AetherFonts::caption (10.f * s).withExtraKerningFactor (0.3f));
    g.drawText ("DYNAMIC AIR EXCITER", headerBar.withTrimmedLeft (26 * s + wordW + 50 * s), Justification::centredLeft);

    // Section captions with rules
    auto sections = deckBar.withTrimmedTop (12 * s).withTrimmedBottom (22 * s);
    auto caption = [&] (Rectangle<float> a, const String& t)
    {
        auto row = a.removeFromTop (16 * s).withTrimmedLeft (18 * s).withTrimmedRight (14 * s);
        const Font f = AetherFonts::caption (10.f * s).withExtraKerningFactor (0.3f);
        g.setFont (f); g.setColour (textPrimary);
        const float tw = f.getStringWidthFloat (t);
        g.drawText (t, row, Justification::centredLeft);
        g.setColour (textMuted.withAlpha (0.45f));
        g.drawHorizontalLine ((int) row.getCentreY(), row.getX() + tw + 10 * s, row.getRight());
    };
    auto tone = sections.removeFromLeft (sections.getWidth() * 0.44f);
    auto chr  = sections.removeFromLeft (sections.getWidth() * 0.62f);
    caption (tone, "TONE"); caption (chr, "CHARACTER"); caption (sections, "OUTPUT");
    g.setColour (textMuted.withAlpha (0.35f));
    g.drawVerticalLine ((int) tone.getRight(), tone.getY() + 8 * s, deckBar.getBottom() - 24 * s);
    g.drawVerticalLine ((int) chr.getRight(),  chr.getY() + 8 * s,  deckBar.getBottom() - 24 * s);

    // Footer: company mark
    g.setColour (textMuted);
    g.setFont (AetherFonts::caption (9.f * s).withExtraKerningFactor (0.22f));
    g.drawText (String::fromUTF8 ("AMANORSAC STUDIO   \xc2\xb7   v" JucePlugin_VersionString),
                deckBar.removeFromBottom (24 * s).withTrimmedRight (24 * s).withTrimmedBottom (6 * s), Justification::centredRight);
}

void AetherAudioProcessorEditor::resized()
{
    const float s = (float) getWidth() / kW;
    auto r = getLocalBounds();
    about.setBounds (r);
    activation.setBounds (r);

    auto header = r.removeFromTop ((int) (58 * s)).withTrimmedTop ((int) (10 * s)).reduced ((int) (22 * s), (int) (11 * s));
    aboutBtn.setBounds (header.removeFromRight ((int) (26 * s)));
    header.removeFromRight ((int) (8 * s));
    bypassBtn.setBounds (header.removeFromRight ((int) (74 * s)));
    header.removeFromRight ((int) (8 * s));
    oversampleBox.setBounds (header.removeFromRight ((int) (86 * s)));
    header.removeFromRight ((int) (8 * s));
    presetBox.setBounds (header.removeFromRight ((int) (190 * s)));

    auto deck = r.removeFromBottom ((int) (232 * s));
    spectrum.setBounds (r.reduced ((int) (10 * s), (int) (6 * s)));

    deck.removeFromBottom ((int) (32 * s));
    deck.removeFromTop ((int) (30 * s));
    deck = deck.reduced ((int) (10 * s), 0);
    auto tone      = deck.removeFromLeft ((int) (deck.getWidth() * 0.44f));
    auto character = deck.removeFromLeft ((int) (deck.getWidth() * 0.62f));
    auto output    = deck;

    {
        auto a = tone.reduced ((int) (16 * s), (int) (4 * s));
        auto half = a.removeFromLeft (a.getWidth() / 2);
        auto heroL = half.removeFromLeft ((int) (half.getWidth() * 0.72f));
        presence.setBounds (heroL);
        presenceFreq.setBounds (half.withSizeKeepingCentre (half.getWidth(), (int) (78 * s)).translated (0, (int) (30 * s)));
        auto heroR = a.removeFromLeft ((int) (a.getWidth() * 0.72f));
        air.setBounds (heroR);
        airFreq.setBounds (a.withSizeKeepingCentre (a.getWidth(), (int) (78 * s)).translated (0, (int) (30 * s)));
    }
    {
        auto a = character.reduced ((int) (10 * s), (int) (14 * s));
        const int w = a.getWidth() / 3;
        glow.setBounds (a.removeFromLeft (w));
        guard.setBounds (a.removeFromLeft (w));
        focus.setBounds (a);
    }
    {
        auto a = output.reduced ((int) (10 * s), (int) (14 * s));
        auto meterArea = a.removeFromRight ((int) (44 * s));
        meter.setBounds (meterArea);
        auto knobs = a.removeFromTop ((int) (a.getHeight() * 0.74f));
        const int w = knobs.getWidth() / 2;
        mix.setBounds (knobs.removeFromLeft (w));
        trim.setBounds (knobs);
        autoGainBtn.setBounds (a.withSizeKeepingCentre ((int) (100 * s), (int) (24 * s)));
    }
}
