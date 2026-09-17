#include "LicenseClient.h"

#if JUCE_WINDOWS
 #include <windows.h>
 #include <dpapi.h>
#elif JUCE_MAC
 #include <Security/Security.h>
#endif

namespace aether::license
{

using namespace juce;

//==============================================================================
String describe (Result r, const String& serverMessage)
{
    switch (r)
    {
        case Result::activated:           return "Activated.";
        case Result::keyNotFound:         return "That key wasn't recognised. Check it against My Apps on amanorsac.studio.";
        case Result::deviceLimitReached:  return serverMessage.isNotEmpty() ? serverMessage
                                                 : "This key is already on its allowed number of computers. "
                                                   "Remove one in My Apps, or deactivate it on that machine.";
        case Result::serverUnavailable:   return serverMessage.isNotEmpty() ? serverMessage
                                                 : "Couldn't reach the licence server. Check your connection and try again.";
        case Result::verificationFailed:  return "The licence could not be verified. Please contact support@amanorsac.studio.";
        case Result::invalidRequest:      return "The activation request was rejected. Please contact support@amanorsac.studio.";
    }
    return {};
}

//==============================================================================
// Encrypted at rest so a copied file is useless on another machine (R7).
namespace
{
   #if JUCE_WINDOWS
    bool writeSecret (const File& f, const String& value, const String& entropy)
    {
        auto entropyUtf8 = entropy.toRawUTF8();
        DATA_BLOB e { (DWORD) strlen (entropyUtf8), (BYTE*) entropyUtf8 };
        DATA_BLOB in { (DWORD) strlen (value.toRawUTF8()), (BYTE*) value.toRawUTF8() }, out {};
        if (! CryptProtectData (&in, L"AETHER", &e, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
            return false;
        const bool ok = f.replaceWithData (out.pbData, out.cbData);
        LocalFree (out.pbData);
        return ok;
    }

    String readSecret (const File& f, const String& entropy)
    {
        MemoryBlock raw;
        if (! f.existsAsFile() || ! f.loadFileAsData (raw) || raw.getSize() == 0) return {};
        auto entropyUtf8 = entropy.toRawUTF8();
        DATA_BLOB e { (DWORD) strlen (entropyUtf8), (BYTE*) entropyUtf8 };
        DATA_BLOB in { (DWORD) raw.getSize(), (BYTE*) raw.getData() }, out {};
        if (! CryptUnprotectData (&in, nullptr, &e, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
            return {};   // profile moved or password reset: caller deletes it quietly
        String value (CharPointer_UTF8 ((const char*) out.pbData), (size_t) out.cbData);
        LocalFree (out.pbData);
        return value;
    }

   #elif JUCE_MAC
    // Keychain, one generic-password item per file name.
    bool writeSecret (const File& f, const String& value, const String& service)
    {
        const auto account = f.getFileName().toStdString();
        const auto svc     = service.toStdString();
        SecKeychainItemRef existing = nullptr;
        if (SecKeychainFindGenericPassword (nullptr, (UInt32) svc.size(), svc.c_str(),
                                            (UInt32) account.size(), account.c_str(),
                                            nullptr, nullptr, &existing) == errSecSuccess && existing != nullptr)
        {
            const auto ok = SecKeychainItemModifyAttributesAndData (existing, nullptr,
                                (UInt32) strlen (value.toRawUTF8()), value.toRawUTF8()) == errSecSuccess;
            CFRelease (existing);
            f.replaceWithText ("keychain");     // marker so presence checks still work
            return ok;
        }
        const auto ok = SecKeychainAddGenericPassword (nullptr, (UInt32) svc.size(), svc.c_str(),
                            (UInt32) account.size(), account.c_str(),
                            (UInt32) strlen (value.toRawUTF8()), value.toRawUTF8(), nullptr) == errSecSuccess;
        if (ok) f.replaceWithText ("keychain");
        return ok;
    }

    String readSecret (const File& f, const String& service)
    {
        if (! f.existsAsFile()) return {};
        const auto account = f.getFileName().toStdString();
        const auto svc     = service.toStdString();
        UInt32 length = 0; void* data = nullptr;
        if (SecKeychainFindGenericPassword (nullptr, (UInt32) svc.size(), svc.c_str(),
                                            (UInt32) account.size(), account.c_str(),
                                            &length, &data, nullptr) != errSecSuccess || data == nullptr)
            return {};
        String value (CharPointer_UTF8 ((const char*) data), (size_t) length);
        SecKeychainItemFreeContent (nullptr, data);
        return value;
    }
   #else
    bool writeSecret (const File& f, const String& value, const String&) { return f.replaceWithText (value); }
    String readSecret (const File& f, const String&) { return f.existsAsFile() ? f.loadFileAsString() : String(); }
   #endif
}

//==============================================================================
LicenseClient::LicenseClient (String product, String prefix)
    : productId (std::move (product)), keyPrefix (std::move (prefix))
{
}

LicenseClient::~LicenseClient() { stopTimer(); }

String LicenseClient::getBaseUrl()
{
    // R1: the shipped default is the live server. The override exists for development
    // only and is never the fallback in a release build.
    if (auto env = SystemStats::getEnvironmentVariable ("AMANORSAC_LICENSE_URL", {}); env.isNotEmpty())
        return env.trimCharactersAtEnd ("/");

    return "https://amanorsac.studio";
}

File LicenseClient::getStorageFolder() const
{
   #if JUCE_WINDOWS
    auto root = File::getSpecialLocation (File::windowsLocalAppData);
   #else
    auto root = File::getSpecialLocation (File::userApplicationDataDirectory);
   #endif
    return root.getChildFile ("Amanorsac Studio").getChildFile (productId);
}

void LicenseClient::setLicensed (bool shouldBeLicensed)
{
    if (licensed.exchange (shouldBeLicensed) == shouldBeLicensed) return;

    if (onLicenseStateChanged != nullptr)
        MessageManager::callAsync ([cb = onLicenseStateChanged] { cb(); });
}

//==============================================================================
void LicenseClient::loadFromDisk()
{
    auto folder = getStorageFolder();
    folder.createDirectory();

    // Device id: random, generated once, never anything hardware or personal (R6).
    auto idFile = folder.getChildFile ("device.id");
    deviceId = idFile.existsAsFile() ? idFile.loadFileAsString().trim() : String();

    if (deviceId.length() < 16 || deviceId.length() > 128)
    {
        deviceId = Uuid().toDashedString();
        idFile.replaceWithText (deviceId);
    }

    const auto entropy = productId + "/license/v1";
    storedKey = readSecret (folder.getChildFile ("license-key.dat"), entropy);

    auto proofFile = folder.getChildFile ("license-proof.dat");
    const auto proofText = readSecret (proofFile, entropy);

    if (proofText.isNotEmpty() && verifyProof (proofText, currentProof))
    {
        if (currentProof.isCurrentlyValidFor (deviceId, storedKey, Time::currentTimeMillis()))
        {
            setLicensed (true);
            return;
        }
        // Past the grace window: behave as "key but no proof" (§7).
        proofFile.deleteFile();
    }
    else if (proofText.isNotEmpty())
    {
        proofFile.deleteFile();   // corrupt or undecryptable — drop it quietly
    }

    currentProof = {};
    setLicensed (false);
}

void LicenseClient::storeProof (const String& proofText, const String& licenseKey)
{
    auto folder = getStorageFolder();
    folder.createDirectory();
    const auto entropy = productId + "/license/v1";

    // license-proof.dat is written only after a proof verified; its presence is the
    // evidence that activation truly succeeded (§6).
    writeSecret (folder.getChildFile ("license-key.dat"), licenseKey, entropy);
    writeSecret (folder.getChildFile ("license-proof.dat"), proofText, entropy);
    storedKey = licenseKey;
}

void LicenseClient::clearStoredState()
{
    auto folder = getStorageFolder();
    folder.getChildFile ("license-key.dat").deleteFile();
    folder.getChildFile ("license-proof.dat").deleteFile();
    storedKey = {};
    currentProof = {};
    setLicensed (false);
}

bool LicenseClient::isRunningOnGrace() const
{
    if (! licensed.load() || currentProof.expiresAt <= 0) return false;
    const auto now = Time::currentTimeMillis();
    return now > currentProof.expiresAt && now <= currentProof.graceUntil;
}

//==============================================================================
void LicenseClient::start()
{
    loadFromDisk();

    // Heartbeat about once an hour while open, and activate at startup when there is
    // a key but no usable proof (R9).
    startTimer (60 * 60 * 1000);

    if (storedKey.isNotEmpty())
        activateAsync (storedKey, nullptr);
}

void LicenseClient::timerCallback()
{
    if (storedKey.isNotEmpty())
        activateAsync (storedKey, nullptr);
}

//==============================================================================
Result LicenseClient::performActivation (const String& licenseKey, String& serverMessage)
{
    DynamicObject::Ptr payload (new DynamicObject());
    payload->setProperty ("licenseKey", licenseKey);
    payload->setProperty ("deviceKey", deviceId);
    payload->setProperty ("deviceLabel", SystemStats::getComputerName().substring (0, 120));

    const auto body = JSON::toString (var (payload.get()));

    URL url (getBaseUrl() + "/licenses/activate");
    url = url.withPOSTData (body);

    int statusCode = 0;
    StringPairArray responseHeaders;
    auto options = URL::InputStreamOptions (URL::ParameterHandling::inPostData)
                       .withExtraHeaders ("Content-Type: application/json")
                       .withConnectionTimeoutMs (12000)
                       .withResponseHeaders (&responseHeaders)
                       .withStatusCode (&statusCode)
                       .withNumRedirectsToFollow (3);

    auto stream = url.createInputStream (options);
    if (stream == nullptr)
        return Result::serverUnavailable;

    const auto response = stream->readEntireStreamAsString();

    var parsed;
    JSON::parse (response, parsed);
    const auto errorCode = parsed.isObject() ? parsed.getProperty ("error", {}).toString() : String();
    serverMessage        = parsed.isObject() ? parsed.getProperty ("message", {}).toString() : String();

    if (statusCode < 200 || statusCode >= 300)
    {
        if (errorCode == "no_such_license")       return Result::keyNotFound;
        if (errorCode == "device_limit_reached")
        {
            // Name the machines, so the customer knows which one to free (§4.1).
            if (auto* devices = parsed.getProperty ("devices", {}).getArray())
            {
                StringArray names;
                for (auto& d : *devices) names.add (d.getProperty ("device_name", "Unknown").toString());
                if (! names.isEmpty())
                    serverMessage = "This key is already on: " + names.joinIntoString (", ")
                                  + ". Remove one in My Apps, or use Deactivate on that machine.";
            }
            return Result::deviceLimitReached;
        }
        if (errorCode == "invalid_request")       return Result::invalidRequest;
        return Result::serverUnavailable;
    }

    // A 200 is not a licence. Only a verified signature is (R3).
    const auto proofText = parsed.isObject() ? parsed.getProperty ("proof", {}).toString() : String();
    Proof proof;
    if (proofText.isEmpty() || ! verifyProof (proofText, proof))
        return Result::verificationFailed;

    // The proof must be for this device and this key (R5).
    if (! proof.isCurrentlyValidFor (deviceId, licenseKey.trim().toUpperCase(), Time::currentTimeMillis())
        && ! proof.isCurrentlyValidFor (deviceId, licenseKey, Time::currentTimeMillis()))
        return Result::verificationFailed;

    currentProof = proof;
    storeProof (proofText, proof.licenseKey);
    setLicensed (true);
    return Result::activated;
}

void LicenseClient::activateAsync (const String& licenseKeyRaw, std::function<void (Result, String)> onFinished)
{
    if (busy.exchange (true))
    {
        if (onFinished != nullptr) onFinished (Result::serverUnavailable, "Already checking — one moment.");
        return;
    }

    // Send the key exactly as the customer typed it, whitespace trimmed and nothing else.
    // Standard section 4.1 gives no normalisation rule, so upper-casing it was mine to invent:
    // a key containing any lower-case character was being sent as a different string than the
    // one issued, the server answered 404 no_such_license, and the app told the customer their
    // key was not recognised. The proof check below stays case-insensitive.
    const auto licenseKey = licenseKeyRaw.trim();

    Thread::launch ([this, licenseKey, onFinished]
    {
        String serverMessage;
        const auto result = performActivation (licenseKey, serverMessage);
        busy.store (false);

        // A failed heartbeat must never delete a valid stored proof (R9) — nothing above
        // clears state on failure, so there is nothing to undo here.
        if (onFinished != nullptr)
            MessageManager::callAsync ([onFinished, result, serverMessage] { onFinished (result, serverMessage); });
    });
}

void LicenseClient::deactivateAsync (std::function<void (bool)> onFinished)
{
    const auto key = storedKey;
    if (key.isEmpty())
    {
        clearStoredState();
        if (onFinished != nullptr) onFinished (true);
        return;
    }

    Thread::launch ([this, key, onFinished]
    {
        DynamicObject::Ptr payload (new DynamicObject());
        payload->setProperty ("licenseKey", key);
        payload->setProperty ("deviceKey", deviceId);

        URL url (getBaseUrl() + "/licenses/deactivate");
        url = url.withPOSTData (JSON::toString (var (payload.get())));

        int statusCode = 0;
        auto options = URL::InputStreamOptions (URL::ParameterHandling::inPostData)
                           .withExtraHeaders ("Content-Type: application/json")
                           .withConnectionTimeoutMs (12000)
                           .withStatusCode (&statusCode);

        auto stream = url.createInputStream (options);
        const bool reached = stream != nullptr;
        if (reached) stream->readEntireStreamAsString();

        // 200 frees the seat; 404 means the server never had it. Either way the local
        // state goes (R10). Anything else leaves the seat alone and reports failure.
        const bool ok = reached && (statusCode == 404 || (statusCode >= 200 && statusCode < 300));

        MessageManager::callAsync ([this, ok, onFinished]
        {
            if (ok) clearStoredState();
            if (onFinished != nullptr) onFinished (ok);
        });
    });
}

} // namespace aether::license
