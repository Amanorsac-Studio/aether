/*  MinGW cross-build shim. JUCE's Windows code uses MSVC intrinsics (__cpuid, __cpuidex,
    _mm_*, byte-swap helpers). MinGW ships MSVC-compatible declarations in <intrin.h>, but
    JUCE only includes it under _MSC_VER, so pull it in here. Force-included by
    cmake/mingw-w64-x86_64.cmake; the MSVC build never sees this file.  */
#pragma once
#if defined(__GNUC__) && !defined(_MSC_VER)
 #include <intrin.h>
#endif
