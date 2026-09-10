#pragma once
#include <juce_core/juce_core.h>
#include <cstdint>

/*
    Amanorsac Studio — License Integration Standard §5.

    Verifies a signed proof: base64url(body) "." base64url(signature), where the
    signature is a raw 64-byte ECDSA P-256 r‖s (IEEE P1363, NOT DER) over the decoded
    body bytes exactly as received, hashed with SHA-256.

    The signature check is the whole security model (§1), so this file does the one
    thing that matters and does it with the OS crypto library rather than anything
    hand-rolled: CNG on Windows, Security.framework on macOS.
*/
namespace aether::license
{

/** The studio public key: raw SEC1 uncompressed point, 65 bytes (0x04 ‖ X ‖ Y).
    Standard §5 / R2 — this is the same for every Amanorsac product and MUST be
    present byte for byte, with no development key anywhere in the binary. */
inline constexpr std::uint8_t kSigningKey[65] = {
    0x04, 0xcd, 0xa5, 0x7d, 0x1c, 0xc8, 0xa6, 0xe2, 0x71, 0xd5, 0x48, 0x49,
    0xce, 0x55, 0xd5, 0x03, 0x77, 0x56, 0x66, 0x90, 0xfd, 0xb6, 0x95, 0x45,
    0xa4, 0x1a, 0x92, 0xc4, 0x77, 0xda, 0xcb, 0x00, 0x0d, 0x2c, 0x06, 0x0b,
    0xa8, 0x3f, 0xbd, 0x9b, 0x70, 0x85, 0xaf, 0xff, 0xc0, 0x42, 0xd4, 0x00,
    0x7e, 0x5b, 0x96, 0xfe, 0x68, 0xff, 0xec, 0x91, 0x11, 0xf6, 0x21, 0x00,
    0x79, 0xfc, 0x43, 0x59, 0x52
};
inline constexpr bool kSigningKeyConfigured = true;

/** The verified contents of a proof (§5). All times are ms since the Unix epoch. */
struct Proof
{
    juce::String deviceKey, licenseKey;
    juce::int64  issuedAt = 0, expiresAt = 0, graceUntil = 0;

    /** Licensed means: signature verified (checked by the caller), device and key
        match, we are inside the grace window, and the clock has not been wound back
        more than 48 hours before the proof was issued. */
    bool isCurrentlyValidFor (const juce::String& expectedDevice,
                              const juce::String& expectedKey,
                              juce::int64 nowMs) const noexcept
    {
        if (deviceKey != expectedDevice || licenseKey != expectedKey) return false;
        if (nowMs > graceUntil)                                       return false;
        if (nowMs < issuedAt - 48LL * 60LL * 60LL * 1000LL)           return false;
        return true;
    }
};

/** base64url → bytes. '-' for '+', '_' for '/', padding optional (§5 step 2). */
juce::MemoryBlock base64UrlDecode (const juce::String& text);

/** ECDSA P-256 / SHA-256 over `message`, signature as raw 64-byte r‖s.
    `publicKey65` is the SEC1 uncompressed point; it defaults to the studio key and is
    a parameter only so the verification path itself can be tested against a known
    vector. Production callers never pass anything else. */
bool verifySignature (const void* message, size_t messageBytes,
                      const std::uint8_t* signature64,
                      const std::uint8_t* publicKey65 = kSigningKey);

/** Splits, decodes and verifies a proof string, then parses the body.
    Returns false unless the signature verified — a well-formed body is never
    enough (R3), and the caller must not treat an HTTP 200 as proof. */
bool verifyProof (const juce::String& proofText, Proof& result);

} // namespace aether::license
