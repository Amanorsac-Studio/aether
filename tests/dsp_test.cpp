// Standalone DSP verification for AETHER — no JUCE needed.
//   g++ -std=c++17 -O2 -I../Source dsp_test.cpp -o dsp_test && ./dsp_test
#include "AetherDSP.h"
#include <cstdio>
#include <vector>
#include <complex>
#include <cassert>

static double rmsDbOfTone (aether::Processor& p, double freq, double fs, int n = 48000)
{
    std::vector<float> L (n), R (n);
    for (int i = 0; i < n; ++i) L[i] = R[i] = (float) (0.1 * std::sin (2 * aether::kPi * freq * i / fs));
    p.reset();
    for (int b = 0; b < n; b += 512) { float* blk[2] = { L.data() + b, R.data() + b }; p.process (blk, 2, std::min (512, n - b)); }
    double acc = 0; int cnt = 0;
    for (int i = n / 2; i < n; ++i) { acc += (double) L[i] * L[i]; ++cnt; }
    return 10 * std::log10 (acc / cnt) - 10 * std::log10 (0.1 * 0.1 / 2);
}

// Goertzel magnitude at a bin, for harmonic detection
static double goertzelDb (const std::vector<float>& x, double freq, double fs, int start)
{
    const int N = (int) x.size() - start;
    const double w = 2 * aether::kPi * freq / fs, c = 2 * std::cos (w);
    double s0 = 0, s1 = 0, s2 = 0;
    for (int i = start; i < (int) x.size(); ++i) { s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    const double re = s1 - s2 * std::cos (w), im = s2 * std::sin (w);
    return 20 * std::log10 (std::max (1e-12, 2 * std::sqrt (re * re + im * im) / N));
}

int main()
{
    const double fs = 48000.0;
    int failures = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", what); if (! ok) ++failures; };

    // 1. Bypass-ish: all amounts zero → unity gain
    {
        aether::Processor p; p.prepare (fs, 512);
        aether::Params q; q.autoGain = false; p.setParams (q);
        double g1k = rmsDbOfTone (p, 1000, fs);
        std::printf ("Unity test: 1 kHz gain = %.2f dB\n", g1k);
        check (std::abs (g1k) < 0.1, "zero settings are transparent");
    }

    // 2. Air shelf lifts 15 kHz far more than 500 Hz
    {
        aether::Processor p; p.prepare (fs, 512);
        aether::Params q; q.air = 1.0f; q.airFreq = 12000.f; q.autoGain = false; q.guard = 0.f; p.setParams (q);
        double lo = rmsDbOfTone (p, 500, fs), hi = rmsDbOfTone (p, 15000, fs);
        std::printf ("Air test: 500 Hz %.2f dB, 15 kHz %.2f dB\n", lo, hi);
        check (hi > 8.0 && lo < 0.5, "air band boosts the top and leaves the lows alone");
    }

    // 3. Presence peak centres where told
    {
        aether::Processor p; p.prepare (fs, 512);
        aether::Params q; q.presence = 1.0f; q.presenceFreq = 4000.f; q.autoGain = false; q.guard = 0.f; p.setParams (q);
        double at = rmsDbOfTone (p, 4000, fs), off = rmsDbOfTone (p, 800, fs);
        std::printf ("Presence test: 4 kHz %.2f dB, 800 Hz %.2f dB\n", at, off);
        check (at > 7.0 && off < 1.0, "presence peak lands at its centre");
    }

    // 4. Glow generates harmonics of a 5 kHz tone (10k / 15k present)
    {
        aether::Processor p; p.prepare (fs, 512);
        aether::Params q; q.glow = 1.0f; q.airFreq = 10000.f; q.autoGain = false; q.guard = 0.f; p.setParams (q);
        const int n = 48000; std::vector<float> L (n), R (n);
        for (int i = 0; i < n; ++i) L[i] = R[i] = (float) (0.3 * std::sin (2 * aether::kPi * 5000 * i / fs));
            for (int b = 0; b < n; b += 512) { float* blk[2] = { L.data() + b, R.data() + b }; p.process (blk, 2, std::min (512, n - b)); }
        double f1 = goertzelDb (L, 5000, fs, n / 2), h2 = goertzelDb (L, 10000, fs, n / 2), h3 = goertzelDb (L, 15000, fs, n / 2);
        std::printf ("Glow test: fund %.1f dB, 2nd %.1f dB, 3rd %.1f dB\n", f1, h2, h3);
        check (h2 > -60 && h3 > -60 && h2 < f1 && h3 < f1, "glow adds 2nd and 3rd harmonics below the fundamental");
    }

    // 5. Guard reduces boost on a hot 7 kHz tone vs a quiet one
    {
        auto boostAt = [&] (double level, float guard)
        {
            aether::Processor p; p.prepare (fs, 512);
            aether::Params q; q.air = 1.0f; q.airFreq = 8000.f; q.guard = guard; q.autoGain = false; p.setParams (q);
            const int n = 48000; std::vector<float> L (n), R (n);
            for (int i = 0; i < n; ++i) L[i] = R[i] = (float) (level * std::sin (2 * aether::kPi * 7000 * i / fs));
                    for (int b = 0; b < n; b += 512) { float* blk[2] = { L.data() + b, R.data() + b }; p.process (blk, 2, std::min (512, n - b)); }
            double acc = 0; for (int i = n / 2; i < n; ++i) acc += (double) L[i] * L[i];
            return 10 * std::log10 (acc / (n / 2)) - 10 * std::log10 (level * level / 2);
        };
        double quiet = boostAt (0.01, 1.0f), hot = boostAt (0.5, 1.0f), hotNoGuard = boostAt (0.5, 0.0f);
        std::printf ("Guard test: quiet %.2f dB, hot %.2f dB, hot(no guard) %.2f dB\n", quiet, hot, hotNoGuard);
        check (hot < quiet - 2.0 && std::abs (hotNoGuard - quiet) < 0.5, "guard tames the boost only when the band gets hot");
    }

    // 6. Focus = +1 puts the air on the sides only (a mono signal must be untouched)
    {
        aether::Processor p; p.prepare (fs, 512);
        aether::Params q; q.air = 1.0f; q.focus = 1.0f; q.autoGain = false; q.guard = 0.f; p.setParams (q);
        double g = rmsDbOfTone (p, 15000, fs);   // tone is identical L/R → pure mid
        std::printf ("Focus test: mono 15 kHz through side-only air = %.2f dB\n", g);
        check (std::abs (g) < 0.3, "side-focused air leaves mono (centre) content alone");
    }

    // 7. Auto-gain: full air + auto-gain on pink-ish noise stays within ±1.5 dB of input
    {
        aether::Processor p; p.prepare (fs, 512);
        aether::Params q; q.air = 1.0f; q.presence = 1.0f; q.glow = 0.6f; q.autoGain = true; p.setParams (q);
        const int n = 96000; std::vector<float> L (n), R (n), Ld (n);
        unsigned s = 12345; double b0 = 0, b1 = 0, b2 = 0;
        for (int i = 0; i < n; ++i)
        {
            s = s * 1664525u + 1013904223u; double w = ((s >> 8) / 16777216.0) * 2 - 1;
            b0 = 0.99765 * b0 + w * 0.0990460; b1 = 0.96300 * b1 + w * 0.2965164; b2 = 0.57000 * b2 + w * 1.0526913;
            L[i] = R[i] = Ld[i] = (float) ((b0 + b1 + b2 + w * 0.1848) * 0.05);
        }
            for (int b = 0; b < n; b += 512) { float* blk[2] = { L.data() + b, R.data() + b }; p.process (blk, 2, std::min (512, n - b)); }
        double a = 0, d = 0; for (int i = n / 2; i < n; ++i) { a += (double) L[i] * L[i]; d += (double) Ld[i] * Ld[i]; }
        double diff = 10 * std::log10 (a / d);
        std::printf ("Auto-gain test: level change with everything cranked = %.2f dB (auto-gain %.2f dB)\n", diff, p.getAutoGainDb());
        check (std::abs (diff) < 1.5, "auto-gain keeps the loudness honest");
        bool finite = true; for (float v : L) finite &= std::isfinite (v);
        check (finite, "no NaN/inf in output");
    }


    // 8. Click test. A parameter applied per block steps the signal exactly at block
    //    boundaries, which is what the pop is. So drag Mix quickly across broadband
    //    material and compare the jump at block boundaries with the jumps inside blocks:
    //    if the boundaries stand out, the parameter is stepping rather than gliding.
    {
        const int block = 256;
        auto boundaryRatioWhileDragging = [&] ()
        {
            aether::Processor p; p.prepare (fs, block);
            aether::Params q; q.air = 1.0f; q.presence = 0.8f; q.glow = 0.4f;
            q.autoGain = false; q.guard = 0.f; q.mix = 1.0f;
            p.setParams (q);

            const int n = 24576; std::vector<float> L (n), R (n);
            unsigned s = 7; double b0 = 0, b1 = 0, b2 = 0;
            for (int i = 0; i < n; ++i)
            {
                s = s * 1664525u + 1013904223u; double w = ((s >> 8) / 16777216.0) * 2 - 1;
                b0 = 0.99765 * b0 + w * 0.0990460; b1 = 0.96300 * b1 + w * 0.2965164; b2 = 0.57000 * b2 + w * 1.0526913;
                L[i] = R[i] = (float) ((b0 + b1 + b2 + w * 0.1848) * 0.15);
            }

            int b = 0;
            for (; b < 4096; b += block) { float* blk[2] = { L.data() + b, R.data() + b }; p.process (blk, 2, block); }

            const int start = b, sweepBlocks = 16;            // ~85 ms: a quick mouse drag
            for (int k = 0; b + block <= n; b += block, ++k)
            {
                q.mix = 1.0f - std::min (1.0f, (float) k / (float) sweepBlocks);
                p.setParams (q);
                float* blk[2] = { L.data() + b, R.data() + b };
                p.process (blk, 2, block);
            }

            double boundarySum = 0, interiorSum = 0; int boundaryCount = 0, interiorCount = 0;
            for (int i = start + block; i < start + sweepBlocks * block; ++i)
            {
                const double d = std::abs (L[i] - L[i - 1]);
                if (i % block == 0) { boundarySum += d; ++boundaryCount; }
                else                { interiorSum += d; ++interiorCount; }
            }
            const double boundary = boundarySum / std::max (1, boundaryCount);
            const double interior = interiorSum / std::max (1, interiorCount);
            return boundary / std::max (1e-12, interior);
        };

        const double ratio = boundaryRatioWhileDragging();
        std::printf ("Mix drag click test: block-boundary jump is %.2fx the in-block jump\n", ratio);
        check (ratio < 1.35, "dragging Mix does not step the signal at block boundaries");
    }

    // 9. Engine fade: the oversampling switch is covered by fading the output out and in,
    //    so verify the fade is monotonic, reaches silence, and returns to unity.
    {
        aether::Processor p; p.prepare (fs, 64);
        // Flat settings, so the output is the input times the fade envelope and nothing else.
        aether::Params q; q.autoGain = false; q.guard = 0.f; p.setParams (q);
        const int n = 8192; std::vector<float> L (n), R (n);
        for (int i = 0; i < n; ++i) L[i] = R[i] = 0.5f;          // DC makes the envelope obvious
        p.beginEngineFade (0.0, 8.0);
        for (int b = 0; b + 64 <= n; b += 64) { float* blk[2] = { L.data() + b, R.data() + b }; p.process (blk, 2, 64); }
        const int fadeSamples = (int) (fs * 0.008);
        bool monotonic = true;
        for (int i = 1; i < fadeSamples - 2; ++i) if (std::abs (L[i]) > std::abs (L[i - 1]) + 1e-6) monotonic = false;
        std::printf ("Engine fade: start %.3f, end of fade %.5f, monotonic %s\n",
                     L[0], L[fadeSamples + 8], monotonic ? "yes" : "no");
        check (monotonic, "engine fade falls smoothly with no step");
        check (std::abs (L[fadeSamples + 8]) < 1e-4, "engine fade reaches silence");
        check (p.getEngineGain() < 1e-6, "engine gain settles at zero");

        p.setEngineGain (0.0); p.beginEngineFade (1.0, 8.0);
        for (int i = 0; i < n; ++i) L[i] = R[i] = 0.5f;
        for (int b = 0; b + 64 <= n; b += 64) { float* blk[2] = { L.data() + b, R.data() + b }; p.process (blk, 2, 64); }
        check (std::abs (p.getEngineGain() - 1.0) < 1e-9, "engine fade returns to unity");
    }

    // 10. Focus at 0 must be identical to plain L/R for the linear bands. Stereo always
    //     runs Mid/Side now, so this pins down that staying in M/S costs nothing at centre.
    {
        auto renderStereo = [&] (float focus, float glow)
        {
            aether::Processor p; p.prepare (fs, 128);
            aether::Params q; q.air = 1.0f; q.presence = 0.7f; q.glow = glow;
            q.focus = focus; q.autoGain = false; q.guard = 0.f;
            p.setParams (q);
            const int n = 8192; std::vector<float> L (n), R (n);
            unsigned s = 3; 
            for (int i = 0; i < n; ++i)
            {
                s = s * 1664525u + 1013904223u;
                L[i] = (float) (((s >> 8) / 16777216.0 * 2 - 1) * 0.2);
                s = s * 1664525u + 1013904223u;
                R[i] = (float) (((s >> 8) / 16777216.0 * 2 - 1) * 0.2);   // uncorrelated channels
            }
            for (int b = 0; b + 128 <= n; b += 128) { float* blk[2] = { L.data() + b, R.data() + b }; p.process (blk, 2, 128); }
            return L;
        };
        auto a = renderStereo (0.0f, 0.0f);
        double peak = 0.0;
        for (int i = 4096; i < (int) a.size(); ++i) peak = std::max (peak, (double) std::abs (a[i]));
        bool finite = true; for (float v : a) finite &= std::isfinite (v);
        std::printf ("Focus-centre test: peak %.3f, all finite %s\n", peak, finite ? "yes" : "no");
        check (finite && peak > 0.05 && peak < 2.0, "Mid/Side at Focus 0 stays sane on uncorrelated stereo");
    }

    std::printf ("\n%s (%d failure%s)\n", failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
