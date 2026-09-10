#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

// AETHER visual language: frosted silver chassis, dark glass analyser, cyan light (approved design).
// Skeleton follows the Amanorsac Studio component contract (knob states, focus-visible, Shift = fine,
// double-click reset, scroll + keyboard); the skin (colours, glow) is AETHER's own.
namespace AetherColours
{
    // Token roles (Master Standard §3) — AETHER "frosted silver + cyan light" skin (approved design)
    inline const juce::Colour bg           { 0xff0e1116 };   // outer chassis
    inline const juce::Colour panel        { 0xffd9dee3 };   // brushed silver deck
    inline const juce::Colour panelDark    { 0xffc6cdd3 };
    inline const juce::Colour header       { 0xffe6eaee };
    inline const juce::Colour raised       { 0xffeef1f4 };   // pills / dropdowns
    inline const juce::Colour border       { 0xff9aa5ae };
    inline const juce::Colour borderSoft   { 0x3314202a };
    inline const juce::Colour accent       { 0xff35e0f0 };   // cyan light
    inline const juce::Colour accentHot    { 0xffa9f4fa };
    inline const juce::Colour accentDeep   { 0xff0aa9bb };
    inline const juce::Colour info         { 0xff35e0f0 };
    inline const juce::Colour textPrimary  { 0xff1b2733 };   // dark navy
    inline const juce::Colour textMuted    { 0xff5b6b78 };
    inline const juce::Colour danger       { 0xffff5a4a };
    inline const juce::Colour scopeBg      { 0xff0a1620 };   // analyser glass
    inline const juce::Colour scopeBg2     { 0xff0d1d28 };
    inline const juce::Colour scopeText    { 0xffb7c6d0 };
    inline const juce::Colour scopeIn      { 0xffc9d6de };
    inline const juce::Colour knobLight    { 0xfff4f6f8 };
    inline const juce::Colour knobDark     { 0xffb3bcc4 };
    inline const juce::Colour knobRim      { 0xff7f8a94 };
    inline const juce::Colour track        { 0xff8f9aa4 };

    // Aliases used by the UI code
    inline const juce::Colour bgTop = bg, bgBottom = bg, panelStroke = borderSoft, gold = accent, goldHot = accentHot,
                              violet = scopeIn, violetDeep = accentDeep, cyan = accent, text = textPrimary, textDim = textMuted,
                              knobBody = knobDark, accent2 = accent, accent2Deep = accentDeep;
}

// Company typography system: Inter for UI, JetBrains Mono for readouts. Bundled — no runtime font fetch.
struct AetherFonts
{
    static const juce::Typeface::Ptr& inter()      { static auto t = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_ttf,      BinaryData::InterRegular_ttfSize);      return t; }
    static const juce::Typeface::Ptr& interSemi()  { static auto t = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_ttf,     BinaryData::InterSemiBold_ttfSize);     return t; }
    static const juce::Typeface::Ptr& interBold()  { static auto t = juce::Typeface::createSystemTypefaceFor (BinaryData::InterBold_ttf,         BinaryData::InterBold_ttfSize);         return t; }
    static const juce::Typeface::Ptr& mono()       { static auto t = juce::Typeface::createSystemTypefaceFor (BinaryData::JetBrainsMonoMedium_ttf, BinaryData::JetBrainsMonoMedium_ttfSize); return t; }

    static juce::Font ui   (float size)  { return juce::Font (juce::FontOptions (inter()).withHeight (size)); }
    static juce::Font semi (float size)  { return juce::Font (juce::FontOptions (interSemi()).withHeight (size)); }
    static juce::Font bold (float size)  { return juce::Font (juce::FontOptions (interBold()).withHeight (size)); }
    static juce::Font value (float size) { return juce::Font (juce::FontOptions (mono()).withHeight (size)); }
    static juce::Font caption (float size) { return semi (size).withExtraKerningFactor (0.14f); }
};

class AetherLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AetherLookAndFeel()
    {
        using namespace AetherColours;
        setDefaultSansSerifTypeface (AetherFonts::inter());
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, text);
        setColour (juce::ComboBox::backgroundColourId, raised);
        setColour (juce::ComboBox::outlineColourId, border);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::arrowColourId, accent);
        setColour (juce::ComboBox::focusedOutlineColourId, accent);
        setColour (juce::PopupMenu::backgroundColourId, raised);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accentDeep);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::PopupMenu::headerTextColourId, textMuted);
        setColour (juce::TooltipWindow::backgroundColourId, textPrimary);
        setColour (juce::TooltipWindow::textColourId, raised);
        setColour (juce::TooltipWindow::outlineColourId, textPrimary);
        setColour (juce::TextButton::buttonColourId, raised);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, text);
        setColour (juce::HyperlinkButton::textColourId, accentDeep);
        setColour (juce::AlertWindow::backgroundColourId, header);
        setColour (juce::AlertWindow::textColourId, text);
        setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::outlineColourId, border);
        setColour (juce::TextEditor::focusedOutlineColourId, accentDeep);
        setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.4f));
    }

    juce::Font getLabelFont (juce::Label&) override           { return AetherFonts::ui (12.f); }
    juce::Font getComboBoxFont (juce::ComboBox&) override     { return AetherFonts::ui (13.f); }
    juce::Font getPopupMenuFont() override                    { return AetherFonts::ui (13.f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return AetherFonts::semi (12.f); }
    juce::Font getAlertWindowMessageFont() override           { return AetherFonts::ui (14.f); }
    juce::Font getAlertWindowTitleFont() override             { return AetherFonts::bold (17.f); }

    static juce::Colour accentFor (juce::Component& c)
    {
        auto v = c.getProperties()["accent"];
        return v.isVoid() ? AetherColours::accent : juce::Colour ((juce::uint32) (juce::int64) v);
    }

    // Focus-visible ring (Master Standard §4 — the most commonly missing state)
    static void drawFocusRing (juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour c)
    {
        g.setColour (c.withAlpha (0.9f));
        g.drawRoundedRectangle (r.expanded (2.f), corner + 2.f, 1.5f);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace AetherColours;
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.f);
        const float size   = juce::jmin (bounds.getWidth(), bounds.getHeight());
        auto area          = bounds.withSizeKeepingCentre (size, size);
        const auto centre  = area.getCentre();
        const float radius = size * 0.5f;
        const float arcW   = juce::jmax (2.5f, size * 0.06f);
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const bool hover   = s.isMouseOverOrDragging();
        const bool drag    = s.isMouseButtonDown();
        const bool enabled = s.isEnabled();
        const float angle  = startAngle + pos * (endAngle - startAngle);
        const float midAng = startAngle + 0.5f * (endAngle - startAngle);
        const float amt    = bipolar ? std::abs (pos - 0.5f) * 2.f : pos;

        // Recessed track ring
        juce::Path trackPath;
        trackPath.addCentredArc (centre.x, centre.y, radius - arcW, radius - arcW, 0.f, startAngle, endAngle, true);
        g.setColour (track.withAlpha (0.55f));
        g.strokePath (trackPath, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Cyan value arc with glow
        juce::Path val;
        const float from = bipolar ? midAng : startAngle;
        val.addCentredArc (centre.x, centre.y, radius - arcW, radius - arcW, 0.f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        if (amt > 0.005f || bipolar)
        {
            g.setColour (accent.withAlpha (enabled ? 0.28f + (drag ? 0.12f : hover ? 0.06f : 0.f) : 0.08f));
            g.strokePath (val, juce::PathStrokeType (arcW * 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (accent.withAlpha (enabled ? 0.55f : 0.15f));
            g.strokePath (val, juce::PathStrokeType (arcW * 1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (enabled ? accentHot.interpolatedWith (accent, 0.35f) : accent.withAlpha (0.3f));
            g.strokePath (val, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Knob: drop shadow, rim, brushed face
        auto body = area.reduced (arcW * 2.4f);
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.fillEllipse (body.translated (0, body.getHeight() * 0.05f).expanded (body.getWidth() * 0.02f));
        juce::ColourGradient rim (knobRim.brighter (0.5f), body.getX(), body.getY(), knobRim.darker (0.3f), body.getRight(), body.getBottom(), false);
        g.setGradientFill (rim);
        g.fillEllipse (body);
        auto face = body.reduced (body.getWidth() * 0.06f);
        juce::ColourGradient faceGrad (knobLight, face.getX() + face.getWidth() * 0.3f, face.getY(),
                                       knobDark.brighter (hover ? 0.12f : 0.f), face.getRight(), face.getBottom(), true);
        g.setGradientFill (faceGrad);
        g.fillEllipse (face);
        // brushed concentric rings
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        for (float rr = face.getWidth() * 0.12f; rr < face.getWidth() * 0.5f; rr += juce::jmax (2.5f, face.getWidth() * 0.055f))
            g.drawEllipse (face.withSizeKeepingCentre (rr * 2, rr * 2), 0.6f);
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        for (float rr = face.getWidth() * 0.15f; rr < face.getWidth() * 0.5f; rr += juce::jmax (2.5f, face.getWidth() * 0.055f))
            g.drawEllipse (face.withSizeKeepingCentre (rr * 2, rr * 2), 0.5f);
        // specular sweep
        juce::ColourGradient spec (juce::Colours::white.withAlpha (0.35f), face.getX(), face.getY(),
                                   juce::Colours::transparentWhite, face.getCentreX(), face.getCentreY(), false);
        g.setGradientFill (spec);
        g.fillEllipse (face);

        // Pointer (dark navy)
        const float pLen = face.getWidth() * 0.40f, pW = juce::jmax (2.f, size * 0.035f);
        juce::Point<float> tip (centre.x + std::sin (angle) * pLen, centre.y - std::cos (angle) * pLen);
        juce::Point<float> base (centre.x + std::sin (angle) * pLen * 0.55f, centre.y - std::cos (angle) * pLen * 0.55f);
        g.setColour (textPrimary.withAlpha (enabled ? 1.f : 0.4f));
        g.drawLine ({ base, tip }, pW);

        if (s.hasKeyboardFocus (false))
        {
            g.setColour (accentDeep.withAlpha (0.9f));
            g.drawEllipse (area.expanded (2.f), 1.5f);
        }
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool down) override
    {
        using namespace AetherColours;
        auto r = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();
        auto pill = r.reduced (1.f);
        if (on)
        {
            g.setColour (accent.withAlpha (0.35f));
            g.fillRoundedRectangle (pill.expanded (2.f), pill.getHeight() * 0.5f + 2.f);
        }
        g.setColour (on ? accent.withAlpha (down ? 0.55f : 0.40f) : (down ? raised.darker (0.1f) : raised));
        g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
        g.setColour (on ? accentDeep : (hover ? textMuted : border));
        g.drawRoundedRectangle (pill, pill.getHeight() * 0.5f, 1.f);
        g.setColour (b.isEnabled() ? textPrimary : textMuted);
        g.setFont (AetherFonts::caption (11.f));
        g.drawText (b.getButtonText().toUpperCase(), r, juce::Justification::centred);
        if (b.hasKeyboardFocus (false)) drawFocusRing (g, pill, pill.getHeight() * 0.5f, accentDeep);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down) override
    {
        using namespace AetherColours;
        auto r = b.getLocalBounds().toFloat().reduced (1.f);
        g.setColour (down ? raised.darker (0.12f) : hover ? raised.brighter (0.05f) : raised);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (hover ? textMuted : border);
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.f);
        if (b.hasKeyboardFocus (false)) drawFocusRing (g, r, r.getHeight() * 0.5f, accentDeep);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool down, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace AetherColours;
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId).withMultipliedAlpha (down ? 1.6f : 1.f));
        g.fillRoundedRectangle (r, h * 0.5f);
        g.setColour (box.isMouseOver() ? textMuted : border);
        g.drawRoundedRectangle (r, h * 0.5f, 1.f);
        juce::Path arrow;
        const float ax = (float) w - h * 0.62f, ay = h * 0.5f;
        arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour (textPrimary);
        g.fillPath (arrow);
        if (box.hasKeyboardFocus (false)) drawFocusRing (g, r, h * 0.5f, accentDeep);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (10, 0, box.getWidth() - box.getHeight() - 12, box.getHeight());
        label.setFont (getComboBoxFont (box));
    }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.setColour (findColour (juce::PopupMenu::backgroundColourId));
        g.fillRoundedRectangle (0, 0, (float) w, (float) h, 8.f);
        g.setColour (AetherColours::border);
        g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 8.f, 1.f);
    }

    // Dropdown-style text button (preset selector): pill + arrow, like the combo box
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        using namespace AetherColours;
        auto r = b.getLocalBounds().toFloat();
        g.setColour (b.isEnabled() ? textPrimary : textMuted);
        const bool dropdown = b.getProperties()["dropdown"];
        g.setFont (dropdown ? AetherFonts::ui (13.f) : AetherFonts::semi (12.f));
        if (dropdown)
        {
            g.drawText (b.getButtonText(), r.withTrimmedLeft (12.f).withTrimmedRight (r.getHeight()), juce::Justification::centredLeft);
            juce::Path arrow; const float ax = r.getRight() - r.getHeight() * 0.62f, ay = r.getCentreY();
            arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
            g.fillPath (arrow);
        }
        else g.drawText (b.getButtonText().toUpperCase(), r, juce::Justification::centred);
    }
};
