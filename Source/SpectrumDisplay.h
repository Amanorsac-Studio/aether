#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "AetherLookAndFeel.h"

// Live analyser: input spectrum (violet), output spectrum (gold), the static boost curve,
// and a "Guard" lamp that dims the 5–9 kHz zone when the sibilance limiter leans in.
class SpectrumDisplay : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumDisplay (AetherAudioProcessor& p)
        : proc (p), fft (SpectrumFifo::fftOrder),
          window ((size_t) SpectrumFifo::fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        inSmoothed.fill (-100.f); outSmoothed.fill (-100.f);
        setOpaque (false);
        startTimerHz (40);
    }

private:
    static constexpr int   numBins = SpectrumFifo::fftSize / 2;
    static constexpr float minFreq = 40.f, maxFreq = 20000.f, minDb = -84.f, maxDb = 6.f;

    void timerCallback() override
    {
        bool changed = false;
        if (proc.inputFifo.pull (scratch.data()))  { analyse (inSmoothed);  changed = true; }
        if (proc.outputFifo.pull (scratch.data())) { analyse (outSmoothed); changed = true; }
        // let the display decay smoothly even when the host stops sending audio
        grNow += (proc.getGainReductionDb() - grNow) * 0.3f;
        if (changed || std::abs (grNow) > 0.05f) repaint();
        else if (++idleTicks % 8 == 0) repaint();   // keep the curve overlay live for param drags
    }

    void analyse (std::array<float, numBins>& target)
    {
        std::fill (fftBuf.begin(), fftBuf.end(), 0.f);
        std::copy (scratch.begin(), scratch.end(), fftBuf.begin());
        window.multiplyWithWindowingTable (fftBuf.data(), (size_t) SpectrumFifo::fftSize);
        fft.performFrequencyOnlyForwardTransform (fftBuf.data(), true);
        const float norm = 2.f / (float) SpectrumFifo::fftSize;
        for (int i = 0; i < numBins; ++i)
        {
            const float db = juce::Decibels::gainToDecibels (fftBuf[(size_t) i] * norm, -120.f);
            auto& s = target[(size_t) i];
            s = db > s ? s + (db - s) * 0.6f : s + (db - s) * 0.12f;   // fast attack, slow release
        }
    }

    float xForFreq (float f, juce::Rectangle<float> r) const
    {
        return r.getX() + r.getWidth() * std::log (f / minFreq) / std::log (maxFreq / minFreq);
    }
    float yForDb (float db, juce::Rectangle<float> r) const
    {
        return r.getBottom() - r.getHeight() * juce::jlimit (0.f, 1.f, (db - minDb) / (maxDb - minDb));
    }

    juce::Path buildSpectrumPath (const std::array<float, numBins>& data, juce::Rectangle<float> r) const
    {
        juce::Path p;
        const double fs = proc.getCurrentSampleRate();
        const float binHz = (float) (fs / SpectrumFifo::fftSize);
        bool started = false;
        // Walk pixels, take the max bin in each pixel column for a clean log plot
        const int cols = (int) r.getWidth();
        for (int c = 0; c <= cols; ++c)
        {
            const float f0 = minFreq * std::pow (maxFreq / minFreq, (float) c / (float) cols);
            const float f1 = minFreq * std::pow (maxFreq / minFreq, (float) (c + 1) / (float) cols);
            int b0 = juce::jlimit (1, numBins - 1, (int) (f0 / binHz)), b1 = juce::jlimit (b0, numBins - 1, (int) (f1 / binHz));
            float v = -200.f;
            for (int b = b0; b <= b1; ++b) v = juce::jmax (v, data[(size_t) b]);
            // slight tilt so pink-ish material reads flat (+3 dB/oct reference)
            v += 3.f * std::log2 (f0 / 1000.f) * 0.5f;
            const float x = r.getX() + (float) c, y = yForDb (v, r);
            if (! started) { p.startNewSubPath (x, y); started = true; } else p.lineTo (x, y);
        }
        return p;
    }

    // Boost-curve axis: ±12 dB, centred on the plot
    float yForBoostDb (float db, juce::Rectangle<float> r) const
    {
        return r.getCentreY() - r.getHeight() * 0.5f * juce::jlimit (-1.f, 1.f, db / 12.f);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace AetherColours;
        auto r = getLocalBounds().toFloat();

        // Dark glass panel with a soft cyan haze at the top
        g.setColour (scopeBg);
        g.fillRoundedRectangle (r, 12.f);
        g.setGradientFill (juce::ColourGradient (scopeBg2, 0, r.getY(), scopeBg, 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 12.f);
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.10f), r.getCentreX(), r.getY(), accent.withAlpha (0.f), r.getCentreX(), r.getY() + r.getHeight() * 0.6f, true));
        g.fillRoundedRectangle (r, 12.f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (r.reduced (0.5f), 12.f, 1.f);

        auto plot = r.reduced (12.f, 10.f).withTrimmedBottom (16.f).withTrimmedLeft (30.f).withTrimmedTop (14.f);

        // Grid: octave-ish verticals + dB rows
        g.setFont (AetherFonts::value (9.f));
        for (float f : { 100.f, 1000.f, 5000.f, 10000.f, 15000.f })
        {
            const float x = xForFreq (f, plot);
            g.setColour (juce::Colours::white.withAlpha (std::abs (f - 1000.f) < 1.f ? 0.10f : 0.06f));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (scopeText.withAlpha (0.8f));
            g.drawText (f >= 1000.f ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f),
                        (int) x - 14, (int) plot.getBottom() + 2, 28, 12, juce::Justification::centred);
        }
        for (float f = 20.f; f < 20000.f; f *= 10.f)
            for (int m = 2; m < 10; ++m)
            {
                const float ff = f * (float) m; if (ff < minFreq || ff > maxFreq) continue;
                g.setColour (juce::Colours::white.withAlpha (0.03f));
                g.drawVerticalLine ((int) xForFreq (ff, plot), plot.getY(), plot.getBottom());
            }
        for (float db : { 12.f, 6.f, 0.f, -6.f, -12.f })
        {
            const float y = yForBoostDb (db, plot);
            g.setColour (juce::Colours::white.withAlpha (db == 0.f ? 0.14f : 0.06f));
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
            g.setColour (scopeText.withAlpha (0.8f));
            g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), (int) plot.getX() - 30, (int) y - 6, 26, 12, juce::Justification::centredRight);
        }
        g.setColour (scopeText.withAlpha (0.6f));
        g.drawText ("dB", (int) plot.getX() - 30, (int) plot.getBottom() + 2, 26, 12, juce::Justification::centredRight);
        g.drawText ("Hz", (int) plot.getRight() - 4, (int) plot.getBottom() + 2, 22, 12, juce::Justification::centredLeft);

        // Guard lamp: 5–9 kHz zone glows as gain reduction grows
        {
            const float x0 = xForFreq (5000.f, plot), x1 = xForFreq (9000.f, plot);
            const float amt = juce::jlimit (0.f, 1.f, -grNow / 9.f);
            auto zone = juce::Rectangle<float> (x0, plot.getY(), x1 - x0, plot.getHeight());
            if (amt > 0.02f)
            {
                juce::ColourGradient gg (danger.withAlpha (0.20f * amt), zone.getCentreX(), zone.getY(), danger.withAlpha (0.f), zone.getCentreX(), zone.getBottom(), false);
                g.setGradientFill (gg); g.fillRect (zone);
                g.setColour (danger.withAlpha (0.5f + 0.5f * amt));
                g.setFont (AetherFonts::caption (9.f));
                g.drawText ("GUARD " + juce::String (grNow, 1) + " dB", zone.withHeight (14.f).translated (0, 2.f), juce::Justification::centred);
            }
        }

        g.saveState();
        g.reduceClipRegion (plot.toNearestInt());

        // Input spectrum — soft grey line
        {
            auto p = buildSpectrumPath (inSmoothed, plot);
            auto fill = p; fill.lineTo (plot.getRight(), plot.getBottom()); fill.lineTo (plot.getX(), plot.getBottom()); fill.closeSubPath();
            g.setGradientFill (juce::ColourGradient (scopeIn.withAlpha (0.10f), 0, plot.getY(), scopeIn.withAlpha (0.f), 0, plot.getBottom(), false));
            g.fillPath (fill);
            g.setColour (scopeIn.withAlpha (0.55f)); g.strokePath (p, juce::PathStrokeType (1.f));
        }
        // Output spectrum — cyan glow
        {
            auto p = buildSpectrumPath (outSmoothed, plot);
            auto fill = p; fill.lineTo (plot.getRight(), plot.getBottom()); fill.lineTo (plot.getX(), plot.getBottom()); fill.closeSubPath();
            g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.22f), 0, plot.getY(), accent.withAlpha (0.f), 0, plot.getBottom(), false));
            g.fillPath (fill);
            g.setColour (accent.withAlpha (0.10f)); g.strokePath (p, juce::PathStrokeType (7.f));
            g.setColour (accent.withAlpha (0.25f)); g.strokePath (p, juce::PathStrokeType (3.f));
            g.setColour (accentHot);               g.strokePath (p, juce::PathStrokeType (1.3f));
        }
        // Boost curve on the ±12 dB axis
        {
            juce::Path curve;
            const int cols = (int) plot.getWidth();
            for (int c = 0; c <= cols; c += 2)
            {
                const float f = minFreq * std::pow (maxFreq / minFreq, (float) c / (float) cols);
                const float y = yForBoostDb ((float) proc.getResponseDb (f), plot);
                if (c == 0) curve.startNewSubPath (plot.getX(), y); else curve.lineTo (plot.getX() + (float) c, y);
            }
            g.setColour (accent.withAlpha (0.25f)); g.strokePath (curve, juce::PathStrokeType (5.f));
            g.setColour (accentHot.withAlpha (0.95f)); g.strokePath (curve, juce::PathStrokeType (1.6f));
        }
        g.restoreState();

        // Legend
        g.setFont (AetherFonts::caption (9.f));
        auto leg = r.reduced (12.f, 10.f).removeFromTop (12.f).removeFromRight (190.f);
        auto chip = [&] (juce::Colour c, const juce::String& t, float w)
        {
            auto a = leg.removeFromLeft (w);
            g.setColour (c); g.fillEllipse (a.removeFromLeft (8.f).reduced (1.f));
            g.setColour (scopeText); g.drawText (t, a.withTrimmedLeft (4.f), juce::Justification::centredLeft);
        };
        chip (scopeIn.withAlpha (0.7f), "IN", 40.f); chip (accentHot, "OUT", 50.f); chip (accent, "BOOST", 70.f);
    }

    AetherAudioProcessor& proc;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, SpectrumFifo::fftSize> scratch {};
    std::array<float, SpectrumFifo::fftSize * 2> fftBuf {};
    std::array<float, numBins> inSmoothed {}, outSmoothed {};
    float grNow = 0.f;
    int idleTicks = 0;
};
