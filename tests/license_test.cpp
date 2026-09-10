/*
    Licence verification test — Standard §5 and §9 (A11).

    This exercises the shipping code in Source/License/LicenseCrypto.cpp, not a copy of
    it, against a known-good ECDSA P-256 vector. SecondOut 1.3.0 shipped a build that
    said "Activated." on an unverifiable proof and locked out every buyer (§11); these
    checks are what stop that happening again.

    Built as the aether_license_test target on Windows and macOS.
*/
#include "License/LicenseCrypto.h"
#include "license_test_vector.h"
#include <juce_core/juce_core.h>
#include <cstdio>
#include <vector>

using namespace aether::license;
namespace tv = aether::license::testvector;

int main()
{
    int failures = 0;
    auto check = [&] (bool ok, const char* what)
    {
        std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
        if (! ok) ++failures;
    };

    // --- the verification primitive -----------------------------------------
    check (verifySignature (tv::body, sizeof (tv::body), tv::signature, tv::publicKey),
           "a valid P-256 signature verifies");

    {
        std::vector<std::uint8_t> tampered (tv::signature, tv::signature + 64);
        tampered[10] ^= 0x01;
        check (! verifySignature (tv::body, sizeof (tv::body), tampered.data(), tv::publicKey),
               "a tampered signature is rejected");
    }
    {
        std::vector<std::uint8_t> tampered (tv::body, tv::body + sizeof (tv::body));
        tampered[5] ^= 0x01;
        check (! verifySignature (tampered.data(), sizeof (tv::body), tv::signature, tv::publicKey),
               "a tampered body is rejected");
    }
    check (! verifySignature (tv::body, sizeof (tv::body), tv::signature),
           "a proof signed by another key is rejected by the studio key");

    // --- base64url ------------------------------------------------------------
    {
        const auto decoded = base64UrlDecode ("YWJjZGU");            // "abcde", unpadded
        check (decoded.getSize() == 5 && juce::String ((const char*) decoded.getData(), 5) == "abcde",
               "base64url decodes without padding");
        const auto alphabet = base64UrlDecode ("_-8");     // 0xff 0xef, via the - and _ aliases
        check (alphabet.getSize() == 2
                 && ((const std::uint8_t*) alphabet.getData())[0] == 0xff
                 && ((const std::uint8_t*) alphabet.getData())[1] == 0xef,
               "base64url maps - and _ to 62 and 63");
    }

    // --- the whole proof path -------------------------------------------------
    {
        Proof p;
        // verifyProof uses the compiled-in studio key, so this vector must NOT verify.
        check (! verifyProof (tv::proof, p),
               "verifyProof refuses a proof that is not signed by the studio key");

        check (! verifyProof ("not-a-proof", p), "a malformed proof is rejected");
        check (! verifyProof ("YWJj.YWJj", p), "a short signature is rejected");
    }

    // --- validity window ------------------------------------------------------
    {
        Proof p;
        p.deviceKey = "dev"; p.licenseKey = "AETH-1"; 
        p.issuedAt = 1'000'000'000'000LL;
        p.expiresAt = p.issuedAt + 48LL * 3600 * 1000;
        p.graceUntil = p.issuedAt + 30LL * 24 * 3600 * 1000;

        check (p.isCurrentlyValidFor ("dev", "AETH-1", p.issuedAt + 1000),
               "a fresh proof is valid");
        check (p.isCurrentlyValidFor ("dev", "AETH-1", p.expiresAt + 1000),
               "the grace window keeps working offline past expiry");
        check (! p.isCurrentlyValidFor ("dev", "AETH-1", p.graceUntil + 1),
               "past the grace window it stops");
        check (! p.isCurrentlyValidFor ("other-device", "AETH-1", p.issuedAt + 1000),
               "a proof for another device is rejected");
        check (! p.isCurrentlyValidFor ("dev", "AETH-2", p.issuedAt + 1000),
               "a proof for another key is rejected");
        check (! p.isCurrentlyValidFor ("dev", "AETH-1", p.issuedAt - 72LL * 3600 * 1000),
               "a clock wound far back is rejected");
    }

    // --- R2: exactly one signing key in the build -----------------------------
    check (kSigningKeyConfigured && kSigningKey[0] == 0x04 && kSigningKey[1] == 0xcd
             && kSigningKey[2] == 0xa5 && kSigningKey[3] == 0x7d,
           "the compiled-in key is the studio key from the standard");

    std::printf ("\n%s (%d failure%s)\n", failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED",
                 failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
