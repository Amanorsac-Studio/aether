#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "LicenseCrypto.h"
#include <atomic>
#include <functional>

/*
    Amanorsac Studio — License Integration Standard §4, §6, §7.

    Everything that touches the network happens off the audio thread; the audio thread
    only ever reads one atomic flag (§7).
*/
namespace aether::license
{

/** Machine-readable outcome of an activation attempt, mapped from §4's error codes.
    The UI turns these into wording; a raw HTTP status is never shown to a customer. */
enum class Result
{
    activated,            // proof verified and stored
    keyNotFound,          // 404 no_such_license
    deviceLimitReached,   // 409 device_limit_reached
    serverUnavailable,    // 502 / 503 / no connection
    verificationFailed,   // server answered, but the signature did not verify (R3/R4)
    invalidRequest        // 400, i.e. our own bug
};

juce::String describe (Result, const juce::String& serverMessage = {});

//==============================================================================
class LicenseClient : private juce::Timer
{
public:
    /** @param productId  folder and entropy name, e.g. "AETHER"
        @param keyPrefix  first block of a key for this product, e.g. "AETH" */
    LicenseClient (juce::String productId, juce::String keyPrefix);
    ~LicenseClient() override;

    /** Read by the audio thread and by the editor. Nothing else gates the plugin. */
    bool isLicensed() const noexcept { return licensed.load(); }

    /** Loads any stored proof, unlocks if it is still valid, then heartbeats in the
        background. Safe to call from the constructor of the processor. */
    void start();

    /** Activation from the UI. Runs on a background thread; `onFinished` is called on
        the message thread. */
    void activateAsync (const juce::String& licenseKey, std::function<void (Result, juce::String)> onFinished);

    /** §4.2 / R10 — frees the seat, then clears local state. */
    void deactivateAsync (std::function<void (bool)> onFinished);

    juce::String getStoredKey() const   { return storedKey; }
    juce::String getDeviceId() const    { return deviceId; }
    juce::int64  getGraceUntil() const  { return currentProof.graceUntil; }

    /** True while a stored proof is past expiresAt but still inside the grace window,
        which is when the UI shows a quiet "connect soon" note (§7). */
    bool isRunningOnGrace() const;

    /** Where device.id, license-key.dat and license-proof.dat live (§6). */
    juce::File getStorageFolder() const;

    /** Development override only. The shipped default is https://amanorsac.studio (R1). */
    static juce::String getBaseUrl();

    std::function<void()> onLicenseStateChanged;

private:
    void timerCallback() override;
    Result performActivation (const juce::String& licenseKey, juce::String& serverMessage);
    void loadFromDisk();
    void storeProof (const juce::String& proofText, const juce::String& licenseKey);
    void clearStoredState();
    void setLicensed (bool);

    juce::String productId, keyPrefix;
    juce::String deviceId, storedKey;
    Proof currentProof;
    std::atomic<bool> licensed { false };
    std::atomic<bool> busy { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LicenseClient)
};

} // namespace aether::license
