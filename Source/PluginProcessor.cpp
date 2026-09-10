#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
static String hzText (float v, int) { return v >= 1000.f ? String (v / 1000.f, 2) + " kHz" : String ((int) v) + " Hz"; }
static String pctText (float v, int) { return String ((int) std::round (v * 100.f)) + " %"; }
static String dbText (float v, int) { return String (v, 1) + " dB"; }

AudioProcessorValueTreeState::ParameterLayout AetherAudioProcessor::createLayout()
{
    using P = AudioParameterFloat;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    ps.push_back (std::make_unique<P> (ParameterID { ParamID::presence, 1 }, "Presence",
        NormalisableRange<float> (0.f, 1.f, 0.001f), 0.f, AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::presenceFreq, 1 }, "Presence Freq",
        NormalisableRange<float> (1500.f, 8000.f, 1.f, 0.5f), 4000.f, AudioParameterFloatAttributes().withStringFromValueFunction (hzText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::air, 1 }, "Air",
        NormalisableRange<float> (0.f, 1.f, 0.001f), 0.f, AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::airFreq, 1 }, "Air Freq",
        NormalisableRange<float> (6000.f, 18000.f, 1.f, 0.6f), 12000.f, AudioParameterFloatAttributes().withStringFromValueFunction (hzText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::glow, 1 }, "Glow",
        NormalisableRange<float> (0.f, 1.f, 0.001f), 0.f, AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::guard, 1 }, "Guard",
        NormalisableRange<float> (0.f, 1.f, 0.001f), 0.5f, AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::focus, 1 }, "Focus",
        NormalisableRange<float> (-1.f, 1.f, 0.01f), 0.f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            if (std::abs (v) < 0.01f) return String ("Stereo");
            return v < 0 ? "Center " + String ((int) (-v * 100)) + " %" : "Sides " + String ((int) (v * 100)) + " %";
        })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::mix, 1 }, "Mix",
        NormalisableRange<float> (0.f, 1.f, 0.001f), 1.f, AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::trim, 1 }, "Trim",
        NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f, AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));
    ps.push_back (std::make_unique<AudioParameterBool>   (ParameterID { ParamID::autoGain, 1 }, "Auto Gain", true));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::oversample, 1 }, "Oversampling",
        StringArray { "Off", "2x", "4x" }, 1));
    ps.push_back (std::make_unique<AudioParameterBool>   (ParameterID { ParamID::bypass, 1 }, "Bypass", false));
    return { ps.begin(), ps.end() };
}

//==============================================================================
AetherAudioProcessor::AetherAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AETHER", createLayout())
{
    pPresence     = apvts.getRawParameterValue (ParamID::presence);
    pPresenceFreq = apvts.getRawParameterValue (ParamID::presenceFreq);
    pAir          = apvts.getRawParameterValue (ParamID::air);
    pAirFreq      = apvts.getRawParameterValue (ParamID::airFreq);
    pGlow         = apvts.getRawParameterValue (ParamID::glow);
    pGuard        = apvts.getRawParameterValue (ParamID::guard);
    pFocus        = apvts.getRawParameterValue (ParamID::focus);
    pMix          = apvts.getRawParameterValue (ParamID::mix);
    pTrim         = apvts.getRawParameterValue (ParamID::trim);
    pAutoGain     = apvts.getRawParameterValue (ParamID::autoGain);
    pOversample   = apvts.getRawParameterValue (ParamID::oversample);
    pBypass       = apvts.getRawParameterValue (ParamID::bypass);

    license.start();
}

//==============================================================================
void AetherAudioProcessor::selectOversampling (int factorIndex)
{
    factorIndex = jlimit (0, 2, factorIndex);
    currentOversampleIndex = factorIndex;
    oversampler = oversamplers[(size_t) factorIndex].get();
    if (oversampler != nullptr) oversampler->reset();
    const int mult = 1 << factorIndex;
    core.prepare (hostSampleRate * mult, maxBlockSize * mult);
    setLatencySamples (oversampler ? (int) oversampler->getLatencyInSamples() : 0);
}

void AetherAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostSampleRate = sampleRate;
    maxBlockSize   = samplesPerBlock;
    oversamplers[0].reset();
    for (size_t f = 1; f < 3; ++f)
    {
        oversamplers[f] = std::make_unique<dsp::Oversampling<float>> (2, f,
            dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        oversamplers[f]->initProcessing ((size_t) samplesPerBlock);
    }
    pendingOversampleIndex = -1;
    licenseFade.reset (license.isLicensed() ? 1.0 : 0.0);
    selectOversampling ((int) pOversample->load());
}

bool AetherAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    return out == layouts.getMainInputChannelSet();
}

void AetherAudioProcessor::updateParams()
{
    aether::Params p;
    p.presence     = pPresence->load();
    p.presenceFreq = pPresenceFreq->load();
    p.air          = pAir->load();
    p.airFreq      = pAirFreq->load();
    p.glow         = pGlow->load();
    p.guard        = pGuard->load();
    p.focus        = pFocus->load();
    p.mix          = pMix->load();
    p.trimDb       = pTrim->load();
    p.autoGain     = pAutoGain->load() > 0.5f;

    // Bypass is a ramp to fully dry, not a branch: switching the signal path in one
    // sample clicks, and skipping the oversampler would also jump the latency.
    if (pBypass->load() > 0.5f)
    {
        p.mix    = 0.0f;
        p.trimDb = 0.0f;
    }

    core.setParams (p);
}

void AetherAudioProcessor::renderThroughEngine (AudioBuffer<float>& buffer, int numCh, int n)
{
    if (oversampler != nullptr)
    {
        dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
        auto os = oversampler->processSamplesUp (block);
        float* ptrs[2] = { os.getChannelPointer (0), numCh > 1 ? os.getChannelPointer (1) : nullptr };
        core.process (ptrs, numCh, (int) os.getNumSamples());
        oversampler->processSamplesDown (block);
    }
    else
    {
        core.process (buffer.getArrayOfWritePointers(), numCh, n);
    }
}

void AetherAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int numCh = jmin (2, buffer.getNumChannels());
    const int n     = buffer.getNumSamples();

    for (int ch = numCh; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);

    inputFifo.push (buffer.getReadPointer (0), n);

    updateParams();

    // Changing the oversampling factor resets the filters and the oversampler, and
    // changes the reported latency. Doing that mid-signal is an audible click, so the
    // engine is faded to silence first, switched at the bottom of the fade, then faded
    // back in. About 8 ms out and 8 ms back — reads as a breath, not a click.
    const int wantedOs = (int) pOversample->load();

    if (wantedOs != currentOversampleIndex && pendingOversampleIndex < 0)
    {
        pendingOversampleIndex = wantedOs;
        core.beginEngineFade (0.0, 8.0);
    }

    renderThroughEngine (buffer, numCh, n);

    if (pendingOversampleIndex >= 0 && ! core.isEngineFading())
    {
        if (core.getEngineGain() <= 1.0e-6)
        {
            // Bottom of the fade: swap the engine while it is silent, then come back up.
            selectOversampling (pendingOversampleIndex);
            core.setEngineGain (0.0);
            core.beginEngineFade (1.0, 8.0);
        }
        else
        {
            pendingOversampleIndex = -1;   // fade back in finished
        }
    }

    // Unlicensed: ramp the processed signal away rather than cutting it, so the plugin
    // stays silent-but-civil in the host instead of clicking every buffer.
    licenseFade.setTarget (license.isLicensed() ? 1.0 : 0.0, n);
    if (licenseFade.current < 1.0 || licenseFade.isSmoothing())
    {
        float* chans[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };
        for (int i = 0; i < n; ++i)
        {
            const auto g = (float) licenseFade.next();
            chans[0][i] *= g;
            if (chans[1] != nullptr) chans[1][i] *= g;
        }
    }

    outputFifo.push (buffer.getReadPointer (0), n);

    const bool fullyBypassed = core.getCurrentMix() <= 1.0e-6 && ! core.isMixSmoothing();
    grDb.store (fullyBypassed ? 0.f : (float) core.getGainReductionDb());
    agDb.store (fullyBypassed ? 0.f : (float) core.getAutoGainDb());
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float pk = Decibels::gainToDecibels (buffer.getMagnitude (ch, 0, n), -100.f);
        const float prev = outPeak[(size_t) ch].load();
        outPeak[(size_t) ch].store (pk > prev ? pk : prev - 0.7f * (n / 512.f + 0.5f));   // simple ballistic fall
    }
}

//==============================================================================
const std::vector<AetherAudioProcessor::Preset>& AetherAudioProcessor::getFactoryPresets()
{
    static const std::vector<Preset> presets = []
    {
        std::vector<Preset> v;
        auto mk = [&] (const char* name, float pres, float pf, float air, float af, float glow, float guard, float focus)
        {
            aether::Params p; p.presence = pres; p.presenceFreq = pf; p.air = air; p.airFreq = af;
            p.glow = glow; p.guard = guard; p.focus = focus; v.push_back ({ name, p });
        };
        mk ("Init",                 0.00f, 4000.f, 0.00f, 12000.f, 0.00f, 0.50f,  0.0f);
        mk ("Lead Vocal Lift",      0.25f, 3500.f, 0.45f, 12000.f, 0.20f, 0.70f, -0.3f);
        mk ("Choir Halo",           0.10f, 5000.f, 0.55f, 11000.f, 0.30f, 0.60f,  0.5f);
        mk ("Piano Sparkle",        0.15f, 4500.f, 0.40f, 13000.f, 0.15f, 0.40f,  0.0f);
        mk ("Acoustic Guitar Air",  0.20f, 6000.f, 0.50f, 10000.f, 0.25f, 0.50f,  0.2f);
        mk ("Drum Bus Sheen",       0.10f, 4000.f, 0.35f, 14000.f, 0.40f, 0.30f,  0.0f);
        mk ("Mix Bus Polish",       0.08f, 4200.f, 0.25f, 15000.f, 0.10f, 0.50f,  0.15f);
        mk ("Mastering Whisper",    0.04f, 3800.f, 0.15f, 16000.f, 0.05f, 0.60f,  0.1f);
        mk ("Wide Pad Shimmer",     0.00f, 4000.f, 0.60f, 10000.f, 0.35f, 0.40f,  1.0f);
        mk ("Sibilant Vocal Rescue",0.20f, 3200.f, 0.50f, 12500.f, 0.10f, 1.00f, -0.2f);
        return v;
    }();
    return presets;
}

int AetherAudioProcessor::getNumPrograms() { return (int) getFactoryPresets().size(); }
const String AetherAudioProcessor::getProgramName (int index)
{
    const auto& ps = getFactoryPresets();
    return isPositiveAndBelow (index, (int) ps.size()) ? ps[(size_t) index].name : String();
}

void AetherAudioProcessor::setCurrentProgram (int index)
{
    const auto& ps = getFactoryPresets();
    if (! isPositiveAndBelow (index, (int) ps.size())) return;
    currentProgram = index;
    const auto& p = ps[(size_t) index].p;
    auto set = [&] (const char* id, float v)
    {
        if (auto* param = apvts.getParameter (id)) param->setValueNotifyingHost (param->convertTo0to1 (v));
    };
    set (ParamID::presence, p.presence);  set (ParamID::presenceFreq, p.presenceFreq);
    set (ParamID::air, p.air);            set (ParamID::airFreq, p.airFreq);
    set (ParamID::glow, p.glow);          set (ParamID::guard, p.guard);
    set (ParamID::focus, p.focus);
    currentPresetName = ps[(size_t) index].name;
}

//==============================================================================
File AetherAudioProcessor::getDataDir()
{
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Amanorsac Studio").getChildFile ("AETHER");
}
File AetherAudioProcessor::getUserPresetDir() { return getDataDir().getChildFile ("Presets"); }

Array<File> AetherAudioProcessor::listUserPresets() const
{
    Array<File> files;
    getUserPresetDir().findChildFiles (files, File::findFiles, false, "*.aetherpreset");
    files.sort();
    return files;
}

bool AetherAudioProcessor::saveUserPreset (const String& name)
{
    auto dir = getUserPresetDir();
    if (! dir.createDirectory()) return false;
    auto state = apvts.copyState();
    state.setProperty ("presetName", name, nullptr);
    state.setProperty ("version", JucePlugin_VersionString, nullptr);
    auto xml = state.createXml();
    if (xml == nullptr) return false;
    const bool ok = xml->writeTo (dir.getChildFile (File::createLegalFileName (name) + ".aetherpreset"));
    if (ok) currentPresetName = name;
    return ok;
}

bool AetherAudioProcessor::loadUserPreset (const File& file)
{
    auto xml = XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    auto tree = ValueTree::fromXml (*xml);
    currentPresetName = tree.getProperty ("presetName", file.getFileNameWithoutExtension()).toString();
    // Apply through the parameters so the host sees the change (not a silent replaceState)
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        auto child = tree.getChild (i);
        if (! child.hasType ("PARAM")) continue;
        if (auto* param = apvts.getParameter (child.getProperty ("id").toString()))
            param->setValueNotifyingHost (param->convertTo0to1 ((float) child.getProperty ("value")));
    }
    return true;
}

//==============================================================================
void AetherAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void AetherAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            currentPresetName = tree.getProperty ("presetName", "Init").toString();
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* AetherAudioProcessor::createEditor() { return new AetherAudioProcessorEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AetherAudioProcessor(); }
