#include "LicenseCrypto.h"

#if JUCE_WINDOWS
 #include <windows.h>
 #include <bcrypt.h>
 #ifndef STATUS_SUCCESS
  #define STATUS_SUCCESS ((NTSTATUS) 0x00000000L)
 #endif
#elif JUCE_MAC
 #include <Security/Security.h>
 #include <CoreFoundation/CoreFoundation.h>
#endif

namespace aether::license
{

juce::MemoryBlock base64UrlDecode (const juce::String& text)
{
    juce::String standard = text.replaceCharacter ('-', '+').replaceCharacter ('_', '/');
    while (standard.length() % 4 != 0) standard << '=';

    juce::MemoryOutputStream out;
    if (! juce::Base64::convertFromBase64 (out, standard))
        return {};

    return out.getMemoryBlock();
}

//==============================================================================
#if JUCE_WINDOWS

bool verifySignature (const void* message, size_t messageBytes, const std::uint8_t* signature64,
                      const std::uint8_t* publicKey65)
{
    // CNG wants BCRYPT_ECCKEY_BLOB: magic, key length, then X and Y with the leading
    // 0x04 of the SEC1 point dropped.
    struct KeyBlob { BCRYPT_KEY_BLOB header; ULONG cbKey; std::uint8_t xy[64]; };
    static_assert (sizeof (BCRYPT_KEY_BLOB) == sizeof (ULONG), "unexpected CNG blob layout");

    std::vector<std::uint8_t> blob (sizeof (ULONG) * 2 + 64);
    *reinterpret_cast<ULONG*> (blob.data())     = BCRYPT_ECDSA_PUBLIC_P256_MAGIC;
    *reinterpret_cast<ULONG*> (blob.data() + 4) = 32;
    std::memcpy (blob.data() + 8, publicKey65 + 1, 64);

    BCRYPT_ALG_HANDLE algorithm = nullptr, hashAlgorithm = nullptr;
    BCRYPT_KEY_HANDLE key = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    bool ok = false;
    std::uint8_t digest[32] {};

    if (BCryptOpenAlgorithmProvider (&algorithm, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) == STATUS_SUCCESS
        && BCryptImportKeyPair (algorithm, nullptr, BCRYPT_ECCPUBLIC_BLOB, &key,
                                blob.data(), (ULONG) blob.size(), 0) == STATUS_SUCCESS
        && BCryptOpenAlgorithmProvider (&hashAlgorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == STATUS_SUCCESS
        && BCryptCreateHash (hashAlgorithm, &hash, nullptr, 0, nullptr, 0, 0) == STATUS_SUCCESS
        && BCryptHashData (hash, (PUCHAR) message, (ULONG) messageBytes, 0) == STATUS_SUCCESS
        && BCryptFinishHash (hash, digest, sizeof (digest), 0) == STATUS_SUCCESS)
    {
        // BCryptVerifySignature takes the raw 64-byte r‖s directly — no DER wrapping.
        ok = BCryptVerifySignature (key, nullptr, digest, sizeof (digest),
                                    (PUCHAR) signature64, 64, 0) == STATUS_SUCCESS;
    }

    if (hash != nullptr)          BCryptDestroyHash (hash);
    if (hashAlgorithm != nullptr) BCryptCloseAlgorithmProvider (hashAlgorithm, 0);
    if (key != nullptr)           BCryptDestroyKey (key);
    if (algorithm != nullptr)     BCryptCloseAlgorithmProvider (algorithm, 0);
    return ok;
}

//==============================================================================
#elif JUCE_MAC

namespace
{
    // Security.framework's ...X962SHA256 algorithm expects DER, so wrap r‖s as
    // SEQUENCE { INTEGER r, INTEGER s } with the minimal-length integer encoding.
    void appendDerInteger (std::vector<std::uint8_t>& out, const std::uint8_t* value, size_t length)
    {
        size_t start = 0;
        while (start + 1 < length && value[start] == 0) ++start;      // strip leading zeros
        const bool needsPad = (value[start] & 0x80) != 0;             // keep it positive
        const size_t contentLength = length - start + (needsPad ? 1 : 0);

        out.push_back (0x02);
        out.push_back ((std::uint8_t) contentLength);
        if (needsPad) out.push_back (0x00);
        out.insert (out.end(), value + start, value + length);
    }

    std::vector<std::uint8_t> rawSignatureToDer (const std::uint8_t* signature64)
    {
        std::vector<std::uint8_t> body;
        appendDerInteger (body, signature64, 32);
        appendDerInteger (body, signature64 + 32, 32);

        std::vector<std::uint8_t> der;
        der.push_back (0x30);
        der.push_back ((std::uint8_t) body.size());
        der.insert (der.end(), body.begin(), body.end());
        return der;
    }
}

bool verifySignature (const void* message, size_t messageBytes, const std::uint8_t* signature64,
                      const std::uint8_t* publicKey65)
{
    // SecKeyCreateWithData takes the 65-byte SEC1 point as it is.
    CFDataRef keyData = CFDataCreate (nullptr, publicKey65, 65);
    if (keyData == nullptr) return false;

    const void* keys[]   = { kSecAttrKeyType, kSecAttrKeyClass };
    const void* values[] = { kSecAttrKeyTypeECSECPrimeRandom, kSecAttrKeyClassPublic };
    CFDictionaryRef attributes = CFDictionaryCreate (nullptr, keys, values, 2,
                                                     &kCFTypeDictionaryKeyCallBacks,
                                                     &kCFTypeDictionaryValueCallBacks);

    bool ok = false;
    if (attributes != nullptr)
    {
        if (SecKeyRef key = SecKeyCreateWithData (keyData, attributes, nullptr))
        {
            const auto der = rawSignatureToDer (signature64);
            CFDataRef signature = CFDataCreate (nullptr, der.data(), (CFIndex) der.size());
            CFDataRef payload   = CFDataCreate (nullptr, (const UInt8*) message, (CFIndex) messageBytes);

            if (signature != nullptr && payload != nullptr)
                ok = SecKeyVerifySignature (key, kSecKeyAlgorithmECDSASignatureMessageX962SHA256,
                                            payload, signature, nullptr);

            if (signature != nullptr) CFRelease (signature);
            if (payload != nullptr)   CFRelease (payload);
            CFRelease (key);
        }
        CFRelease (attributes);
    }
    CFRelease (keyData);
    return ok;
}

//==============================================================================
#else

bool verifySignature (const void*, size_t, const std::uint8_t*, const std::uint8_t*)
{
    // AETHER ships on Windows and macOS. Refusing here keeps any other build honest:
    // an unverifiable proof must never read as licensed (R3/R4).
    jassertfalse;
    return false;
}

#endif

//==============================================================================
bool verifyProof (const juce::String& proofText, Proof& result)
{
    if (! kSigningKeyConfigured) return false;

    const int dot = proofText.indexOfChar ('.');            // split on the FIRST dot
    if (dot <= 0 || dot >= proofText.length() - 1) return false;

    const auto body      = base64UrlDecode (proofText.substring (0, dot));
    const auto signature = base64UrlDecode (proofText.substring (dot + 1));

    if (body.getSize() == 0 || signature.getSize() != 64)   // raw r‖s only (§5 step 3)
        return false;

    if (! verifySignature (body.getData(), body.getSize(),
                           static_cast<const std::uint8_t*> (signature.getData())))
        return false;

    // Only now is it safe to look at the contents.
    const juce::String json (juce::CharPointer_UTF8 ((const char*) body.getData()),
                             juce::CharPointer_UTF8 ((const char*) body.getData() + body.getSize()));
    juce::var parsed;
    if (juce::JSON::parse (json, parsed).failed() || ! parsed.isObject())
        return false;

    result.deviceKey  = parsed.getProperty ("deviceKey", {}).toString();
    result.licenseKey = parsed.getProperty ("licenseKey", {}).toString();
    result.issuedAt   = (juce::int64) parsed.getProperty ("issuedAt", 0);
    result.expiresAt  = (juce::int64) parsed.getProperty ("expiresAt", 0);
    result.graceUntil = (juce::int64) parsed.getProperty ("graceUntil", 0);

    return result.deviceKey.isNotEmpty() && result.licenseKey.isNotEmpty() && result.graceUntil > 0;
}

} // namespace aether::license
