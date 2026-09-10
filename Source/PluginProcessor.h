#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "AetherDSP.h"
#include "License/LicenseClient.h"

namespace ParamID
{
    static constexpr auto presence     = "presence";
    static constexpr auto presenceFreq = "presenceFreq";
    static constexpr auto air          = "air";
    static constexpr auto airFreq      = "airFreq";
    static constexpr auto glow         = "glow";
    static constexpr auto guard        = "guard";
    static constexpr auto focus        = "focus";
    static constexpr auto mix          = "mix";
    static constexpr auto trim         = "trim";
    static constexpr auto autoGain     = "autoGain";
    static constexpr auto oversample   = "oversample";
    static constexpr auto bypass       = "bypass";
}

//==============================================================================
// Lock-free FIFO that hands post-processing audio to the analyser on the UI thread.
class SpectrumFifo
{
public:
    static constexpr int fftOrder = 11;              // 2048-point
    static constexpr int fftSize  = 1 << fftOrder;

    void push (const float* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            if (fifoIndex == fftSize)
            {
                if (! nextBlockReady.load())
                {
                    std::copy (fifo.begin(), fifo.end(), fftData.begin());
                    nextBlockReady.store (true);
                }
                fifoIndex = 0;
            }
            fifo[(size_t) fifoIndex++] = data[i];
        }
    }

    bool pull (float* dest) noexcept   // dest must hold fftSize floats
    {
        if (! nextBlockReady.load()) return false;
        std::copy (fftData.begin(), fftData.end(), dest);
        nextBlockReady.store (false);
        return true;
    }

private:
    std::array<float, fftSize> fifo {}, fftData {};
    int fifoIndex = 0;
    std::atomic<bool> nextBlockReady { false };
};

//==============================================================================
class AetherAudioProcessor : public juce::AudioProcessor
{
public:
    AetherAudioProcessor();
    ~AetherAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //-------------------------------------------------------------- UI access
    juce::AudioProcessorValueTreeState apvts;
    SpectrumFifo inputFifo, outputFifo;

    float getGainReductionDb() const noexcept { return grDb.load(); }
    float getAutoGainDb()      const noexcept { return agDb.load(); }
    float getOutputPeakDb (int ch) const noexcept { return outPeak[(size_t) juce::jlimit (0, 1, ch)].load(); }
    double getResponseDb (double freq) const noexcept { return core.responseDb (freq); }
    double getCurrentSampleRate() const noexcept { return hostSampleRate; }

    //-------------------------------------------------------------- Licensing
    // Standard §7: the audio thread reads one atomic flag and nothing else.
    aether::license::LicenseClient& getLicense() noexcept { return license; }
    bool isLicensed() const noexcept { return license.isLicensed(); }

    struct Preset { const char* name; aether::Params p; };
    static const std::vector<Preset>& getFactoryPresets();

    //-------------------------------------------------------------- User presets
    // Runtime data lives in Documents/Amanorsac Studio/AETHER/ (company data-path convention)
    static juce::File getDataDir();
    static juce::File getUserPresetDir();
    juce::Array<juce::File> listUserPresets() const;
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& file);
    juce::String getCurrentPresetName() const { return currentPresetName; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateParams();
    void selectOversampling (int factorIndex);   // realtime-safe: no allocation
    void renderThroughEngine (juce::AudioBuffer<float>&, int numCh, int numSamples);

    aether::Processor core;
    aether::license::LicenseClient license { "AETHER", "AETH" };
    aether::LinearSmoother licenseFade;      // silences the output when unlicensed, without clicking
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers;   // [0]=none, [1]=2x, [2]=4x — built in prepareToPlay
    juce::dsp::Oversampling<float>* oversampler = nullptr;
    int currentOversampleIndex = -1;
    int pendingOversampleIndex = -1;   // set when the user changes OS; applied after a fade-out
    double hostSampleRate = 48000.0;
    int maxBlockSize = 512;
    int currentProgram = 0;
    juce::String currentPresetName { "Init" };

    std::atomic<float> grDb { 0 }, agDb { 0 };
    std::array<std::atomic<float>, 2> outPeak { -100.f, -100.f };

    std::atomic<float>* pPresence {}, *pPresenceFreq {}, *pAir {}, *pAirFreq {}, *pGlow {}, *pGuard {},
                       *pFocus {}, *pMix {}, *pTrim {}, *pAutoGain {}, *pOversample {}, *pBypass {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AetherAudioProcessor)
};
