/*
    AETHER — Dynamic Air Exciter
    Aquarii Audio / Amanorsac Studio

    Pure C++17 DSP core. No JUCE dependency — unit-testable on its own.

    Signal flow (per channel, after optional M/S encode):

        in ──┬──────────────────────────────────────────────┐ dry
             │                                              │
             ├─► Presence peak EQ ─► Air shelf EQ ──┐       │
             │                                      ▼       │
             └─► HP ─► Glow saturator ─► HP ─► (+) ─► boost │
                                                      │     │
                Guard sidechain (5–9 kHz env) ──► ×gr ┘     │
                                                      ▼     │
                                     out = dry + gr·(boost − dry)
                                                      │
                                       Auto-gain ─► Mix ─► Trim

    What beats Fresh Air:
      • Tunable band centres (Fresh Air's are fixed)
      • "Glow" — true harmonic excitation, not just EQ
      • "Guard" — program-dependent sibilance/harshness limiter on the boost
      • "Focus" — Mid/Side placement of the air (center vocal / wide pads)
      • Loudness-matched auto-gain so "brighter ≠ louder" when auditioning
      • Oversampling handled by the host wrapper (2×/4×) for alias-free Glow
*/

#pragma once
#include <cmath>
#include <algorithm>
#include <array>

namespace aether
{

constexpr double kPi = 3.14159265358979323846;

// ---------------------------------------------------------------- Biquad ---
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    inline double process (double x) noexcept
    {
        // Transposed direct form II
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() noexcept { z1 = z2 = 0; }

    void setPeak (double fs, double freq, double Q, double gainDb) noexcept
    {
        const double A  = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * kPi * std::clamp (freq, 10.0, fs * 0.49) / fs;
        const double c  = std::cos (w0), s = std::sin (w0);
        const double al = s / (2.0 * Q);
        const double a0 = 1.0 + al / A;
        b0 = (1.0 + al * A) / a0;  b1 = (-2.0 * c) / a0;  b2 = (1.0 - al * A) / a0;
        a1 = (-2.0 * c) / a0;      a2 = (1.0 - al / A) / a0;
    }

    void setHighShelf (double fs, double freq, double slope, double gainDb) noexcept
    {
        const double A  = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * kPi * std::clamp (freq, 10.0, fs * 0.49) / fs;
        const double c  = std::cos (w0), s = std::sin (w0);
        const double al = (s / 2.0) * std::sqrt ((A + 1.0 / A) * (1.0 / slope - 1.0) + 2.0);
        const double sq = 2.0 * std::sqrt (A) * al;
        const double a0 = (A + 1.0) - (A - 1.0) * c + sq;
        b0 =  A * ((A + 1.0) + (A - 1.0) * c + sq) / a0;
        b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * c) / a0;
        b2 =  A * ((A + 1.0) + (A - 1.0) * c - sq) / a0;
        a1 =  2.0 * ((A - 1.0) - (A + 1.0) * c) / a0;
        a2 = ((A + 1.0) - (A - 1.0) * c - sq) / a0;
    }

    void setHighPass (double fs, double freq, double Q) noexcept
    {
        const double w0 = 2.0 * kPi * std::clamp (freq, 10.0, fs * 0.49) / fs;
        const double c  = std::cos (w0), s = std::sin (w0);
        const double al = s / (2.0 * Q);
        const double a0 = 1.0 + al;
        b0 = ((1.0 + c) / 2.0) / a0;  b1 = (-(1.0 + c)) / a0;  b2 = ((1.0 + c) / 2.0) / a0;
        a1 = (-2.0 * c) / a0;         a2 = (1.0 - al) / a0;
    }

    void setBandPass (double fs, double freq, double Q) noexcept
    {
        const double w0 = 2.0 * kPi * std::clamp (freq, 10.0, fs * 0.49) / fs;
        const double c  = std::cos (w0), s = std::sin (w0);
        const double al = s / (2.0 * Q);
        const double a0 = 1.0 + al;
        b0 = al / a0;  b1 = 0.0;  b2 = -al / a0;
        a1 = (-2.0 * c) / a0;  a2 = (1.0 - al) / a0;
    }

    // Magnitude response in dB at a given frequency (for the UI curve)
    double magnitudeDb (double fs, double freq) const noexcept
    {
        const double w = 2.0 * kPi * freq / fs;
        const double cw = std::cos (w), sw = std::sin (w), c2 = std::cos (2 * w), s2 = std::sin (2 * w);
        const double nr = b0 + b1 * cw + b2 * c2, ni = -(b1 * sw + b2 * s2);
        const double dr = 1.0 + a1 * cw + a2 * c2, di = -(a1 * sw + a2 * s2);
        const double mag = std::sqrt ((nr * nr + ni * ni) / std::max (1e-20, dr * dr + di * di));
        return 20.0 * std::log10 (std::max (mag, 1e-9));
    }
};

// ------------------------------------------------------ Envelope follower ---
struct EnvelopeFollower
{
    double att = 0, rel = 0, env = 0;
    void prepare (double fs, double attackMs, double releaseMs) noexcept
    {
        att = std::exp (-1.0 / (fs * attackMs  * 0.001));
        rel = std::exp (-1.0 / (fs * releaseMs * 0.001));
    }
    inline double process (double x) noexcept
    {
        const double a = std::abs (x);
        env = (a > env) ? att * env + (1.0 - att) * a : rel * env + (1.0 - rel) * a;
        return env;
    }
    void reset() noexcept { env = 0; }
};

// ------------------------------------------------------- Linear smoother ---
// Per-sample ramp. Block-rate parameter updates step the signal, which is exactly
// what a "zipper" or a click is; anything that scales the audio directly (mix, trim,
// the engine fade) is ramped sample by sample instead.
struct LinearSmoother
{
    double current = 0.0, target = 0.0, step = 0.0;
    int    countdown = 0;

    void reset (double value) noexcept { current = target = value; step = 0.0; countdown = 0; }

    void setTarget (double newTarget, int rampSamples) noexcept
    {
        target = newTarget;
        if (rampSamples <= 0 || std::abs (target - current) < 1e-12)
        {
            current = target; countdown = 0; step = 0.0; return;
        }
        countdown = rampSamples;
        step = (target - current) / (double) rampSamples;
    }

    inline double next() noexcept
    {
        if (countdown > 0) { current += step; if (--countdown == 0) current = target; }
        return current;
    }

    bool isSmoothing() const noexcept { return countdown > 0; }
};

// ---------------------------------------------------------------- Params ---
struct Params
{
    float presence     = 0.0f;   // 0..1
    float presenceFreq = 4000.f; // Hz  (1.5k .. 8k)
    float air          = 0.0f;   // 0..1
    float airFreq      = 12000.f;// Hz  (6k .. 18k)
    float glow         = 0.0f;   // 0..1  harmonic excitation
    float guard        = 0.5f;   // 0..1  sibilance guard strength
    float focus        = 0.0f;   // -1 (center) .. 0 (stereo) .. +1 (sides)
    float mix          = 1.0f;   // 0..1
    float trimDb       = 0.0f;   // -12..+12
    bool  autoGain     = true;
    bool  msMode       = false;  // derived: focus != 0
};

// ------------------------------------------------------------ One channel ---
class Channel
{
public:
    void prepare (double fs) noexcept
    {
        sampleRate = fs;
        for (auto* b : { &presenceEq, &airEq, &excHp1, &excHp2, &guardBp }) b->reset();
        guardEnv.prepare (fs, 1.0, 60.0);
        excHp1.setHighPass (fs, 3000.0, 0.707);
        excHp2.setHighPass (fs, 5000.0, 0.707);
        guardBp.setBandPass (fs, 7000.0, 0.9);
        guardThreshold = 0.08;  // ~ -22 dBFS in the 5–9k band before the guard leans in
    }

    // Update filter coefficients from (already smoothed) parameter values.
    // scale = per-channel multiplier for boost amount (used by Focus / M-S)
    void update (const Params& p, double scale) noexcept
    {
        const double presGain = 9.0  * p.presence * scale;  // up to +9 dB
        const double airGain  = 12.0 * p.air      * scale;  // up to +12 dB
        presenceEq.setPeak      (sampleRate, p.presenceFreq, 0.7, presGain);
        airEq.setHighShelf      (sampleRate, p.airFreq, 0.6, airGain);
        excHp1.setHighPass      (sampleRate, std::max (2000.0, p.airFreq * 0.55), 0.707);
        excHp2.setHighPass      (sampleRate, std::max (2500.0, p.airFreq * 0.7),  0.707);
        glowAmt  = p.glow * scale;
        drive    = 1.0 + 6.0 * p.glow;
        guardAmt = p.guard;
        currentPresGain = presGain; currentAirGain = airGain;
    }

    inline double process (double x) noexcept
    {
        // Static tone shaping
        double boost = airEq.process (presenceEq.process (x));

        // Glow: asymmetric soft saturation of the top end → even+odd harmonics
        if (glowAmt > 1e-4)
        {
            double e = excHp1.process (x) * drive;
            e = softClip (e + 0.12 * e * e);     // slight asymmetry = even harmonics
            e = excHp2.process (e);             // strip anything below the air region
            boost += e * glowAmt * 0.45;
        }

        // Guard: dynamic gain reduction on the *boost delta* when 5–9k gets hot
        const double delta = boost - x;
        const double env   = guardEnv.process (guardBp.process (boost));
        double gr = 1.0;
        if (guardAmt > 1e-4)
        {
            const double over = std::max (0.0, env / guardThreshold - 1.0);
            gr = 1.0 / (1.0 + over * 2.5 * guardAmt);
        }
        lastGr = gr;
        return x + delta * gr;
    }

    double getLastGainReduction() const noexcept { return lastGr; }

    // For UI curve: combined static EQ response in dB
    double responseDb (double freq) const noexcept
    {
        return presenceEq.magnitudeDb (sampleRate, freq) + airEq.magnitudeDb (sampleRate, freq);
    }

private:
    static inline double softClip (double v) noexcept
    {
        // Classic cubic soft clip: smooth up to |v| = 1, then flat at ±2/3
        if (v >  1.0) return  2.0 / 3.0;
        if (v < -1.0) return -2.0 / 3.0;
        return v - v * v * v / 3.0;
    }

    double sampleRate = 48000.0;
    Biquad presenceEq, airEq, excHp1, excHp2, guardBp;
    EnvelopeFollower guardEnv;
    double glowAmt = 0, drive = 1, guardAmt = 0, guardThreshold = 0.08, lastGr = 1;
    double currentPresGain = 0, currentAirGain = 0;
};

// ------------------------------------------------------------ Processor ----
// Stereo processor with M/S focus, auto-gain, mix and trim.
class Processor
{
public:
    void prepare (double fs, int /*maxBlock*/) noexcept
    {
        sampleRate = fs;
        for (auto& c : ch) c.prepare (fs);
        dryRms.prepare (fs, 5.0, 400.0);
        wetRms.prepare (fs, 5.0, 400.0);
        // Filter coefficients are recomputed once per block; this is the per-block
        // coefficient of a ~30 ms one-pole, so the glide is the same in wall-clock
        // time whatever the host's sample rate and buffer size.
        paramTau = 0.030;
        reset();
    }

    void reset() noexcept
    {
        dryRms.reset(); wetRms.reset();
        autoGainLin = 1.0;
        smoothed = target;
        mixSmoother.reset (target.mix);
        trimSmoother.reset (std::pow (10.0, target.trimDb / 20.0));
        engineGain.reset (1.0);
    }

    // Ramps the whole processor to silence and back. Used around anything that
    // discontinuously resets state (oversampling changes), so the seam is inaudible.
    void beginEngineFade (double toGain, double milliseconds) noexcept
    {
        engineGain.setTarget (toGain, (int) std::max (1.0, sampleRate * milliseconds * 0.001));
    }
    bool isEngineFading() const noexcept { return engineGain.isSmoothing(); }
    bool isMixSmoothing()  const noexcept { return mixSmoother.isSmoothing(); }
    double getCurrentMix() const noexcept { return mixSmoother.current; }
    double getEngineGain() const noexcept { return engineGain.current; }
    void setEngineGain (double g) noexcept { engineGain.reset (g); }

    void setParams (const Params& p) noexcept { target = p; }

    // Interleaved-free: process planar L/R buffers in place. numCh 1 or 2.
    void process (float* const* io, int numCh, int numSamples) noexcept
    {
        // Block-rate smoothing of parameters (we recompute coefficients once per block)
        blockCoeff = std::exp (-(double) numSamples / (sampleRate * paramTau));
        smoothTowards (smoothed.presence,     target.presence);
        smoothTowards (smoothed.presenceFreq, target.presenceFreq);
        smoothTowards (smoothed.air,          target.air);
        smoothTowards (smoothed.airFreq,      target.airFreq);
        smoothTowards (smoothed.glow,         target.glow);
        smoothTowards (smoothed.guard,        target.guard);
        smoothTowards (smoothed.focus,        target.focus);
        smoothTowards (smoothed.mix,          target.mix);
        smoothTowards (smoothed.trimDb,       target.trimDb);
        smoothed.autoGain = target.autoGain;

        // Stereo always runs Mid/Side. Switching topology when Focus crosses zero would
        // hand each filter a different signal from one sample to the next, which clicks;
        // at Focus 0 both halves are simply scaled by 1, so nothing is lost by staying here.
        const bool ms = numCh == 2;
        // Focus: -1 → all boost on Mid, +1 → all boost on Side
        const double midScale  = std::clamp (1.0 - smoothed.focus, 0.0, 1.0);
        const double sideScale = std::clamp (1.0 + smoothed.focus, 0.0, 1.0);
        ch[0].update (smoothed, ms ? midScale  : 1.0);
        ch[1].update (smoothed, ms ? sideScale : 1.0);

        // Ramp the sample-scaling parameters across the block instead of stepping per block.
        mixSmoother.setTarget (target.mix, numSamples);
        trimSmoother.setTarget (std::pow (10.0, target.trimDb / 20.0), numSamples);

        if (numCh == 1)
        {
            float* d = io[0];
            for (int i = 0; i < numSamples; ++i)
            {
                const double mix = mixSmoother.next(), trim = trimSmoother.next(), eg = engineGain.next();
                const double x = d[i];
                double y = ch[0].process (x);
                y = applyAutoGain (x, y);
                d[i] = (float) ((x + (y - x) * mix) * trim * eg);
            }
            return;
        }

        float* L = io[0]; float* R = io[1];
        for (int i = 0; i < numSamples; ++i)
        {
            const double l = L[i], r = R[i];
            double a, b;
            if (ms) { a = 0.5 * (l + r); b = 0.5 * (l - r); } else { a = l; b = r; }
            a = ch[0].process (a);
            b = ch[1].process (b);
            double yl, yr;
            if (ms) { yl = a + b; yr = a - b; } else { yl = a; yr = b; }

            // Auto-gain on the L+R energy
            const double dryE = 0.5 * (l + r), wetE = 0.5 * (yl + yr);
            const double g = applyAutoGainStereo (dryE, wetE);
            yl *= g; yr *= g;

            // engineGain fades the whole output, dry included: the dry path runs through
            // the oversampler too, so it is just as discontinuous when the engine is reset.
            const double m = mixSmoother.next(), t = trimSmoother.next() * engineGain.next();
            L[i] = (float) ((l + (yl - l) * m) * t);
            R[i] = (float) ((r + (yr - r) * m) * t);
        }
    }

    // ---- metering / UI helpers
    double getGainReductionDb() const noexcept
    {
        return 20.0 * std::log10 (std::max (1e-6, std::min (ch[0].getLastGainReduction(), ch[1].getLastGainReduction())));
    }
    double getAutoGainDb() const noexcept { return 20.0 * std::log10 (std::max (1e-6, autoGainLin)); }
    double responseDb (double freq) const noexcept { return ch[0].responseDb (freq); }
    const Params& getSmoothedParams() const noexcept { return smoothed; }

private:
    template <typename T>
    inline void smoothTowards (T& v, T t) noexcept
    {
        // One-pole toward the target, once per block. blockCoeff is derived from the
        // real block length so a knob sweep glides over ~30 ms regardless of host settings.
        v = (T) (t + (v - t) * blockCoeff);
    }

    inline double applyAutoGain (double x, double y) noexcept
    {
        if (! smoothed.autoGain) { autoGainLin = 1.0; return y; }
        const double dr = dryRms.process (x), wr = wetRms.process (y);
        if (wr > 1e-5 && dr > 1e-5)
        {
            double g = dr / wr;
            g = std::clamp (g, 0.5, 2.0);          // ±6 dB max compensation
            autoGainLin += (g - autoGainLin) * 0.0005;
        }
        return y * autoGainLin;
    }

    inline double applyAutoGainStereo (double x, double y) noexcept
    {
        if (! smoothed.autoGain) { autoGainLin = 1.0; return 1.0; }
        const double dr = dryRms.process (x), wr = wetRms.process (y);
        if (wr > 1e-5 && dr > 1e-5)
        {
            double g = std::clamp (dr / wr, 0.5, 2.0);
            autoGainLin += (g - autoGainLin) * 0.0005;
        }
        return autoGainLin;
    }

    double sampleRate = 48000.0;
    double paramTau = 0.030, blockCoeff = 0.0;
    std::array<Channel, 2> ch;
    EnvelopeFollower dryRms, wetRms;
    LinearSmoother mixSmoother, trimSmoother, engineGain;
    double autoGainLin = 1.0;
    Params target, smoothed;
};

} // namespace aether
