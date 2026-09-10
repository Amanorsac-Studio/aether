cmake_minimum_required(VERSION 3.22)
project(AETHER VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---- JUCE: use a local checkout if present (./JUCE or ../JUCE), else fetch it.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt")
    add_subdirectory(../JUCE JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

option(AETHER_BUILD_AAX "Build AAX (needs the AAX SDK)" OFF)
option(AETHER_BUILD_STANDALONE "Also build a standalone app (off: AETHER ships as a plugin only)" OFF)
set(AETHER_FORMATS VST3)
if(APPLE)
    list(APPEND AETHER_FORMATS AU)
endif()
if(AETHER_BUILD_AAX)
    list(APPEND AETHER_FORMATS AAX)
endif()
if(AETHER_BUILD_STANDALONE)
    list(APPEND AETHER_FORMATS Standalone)
endif()

# Bundled fonts (company typography: Inter + JetBrains Mono, shipped locally — no runtime font fetch)
juce_add_binary_data(AetherResources SOURCES
    Resources/fonts/Inter-Regular.ttf
    Resources/fonts/Inter-SemiBold.ttf
    Resources/fonts/Inter-Bold.ttf
    Resources/fonts/JetBrainsMono-Medium.ttf)
set_target_properties(AetherResources PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The VST3 manifest helper JUCE runs after linking is itself a Windows executable, so it
# cannot run on a Linux cross-build host. Hosts load the plugin fine without moduleinfo.json.
if(CMAKE_CROSSCOMPILING)
    set(AETHER_VST3_MANIFEST VST3_AUTO_MANIFEST FALSE)
endif()

juce_add_plugin(AETHER
    ${AETHER_VST3_MANIFEST}
    COMPANY_NAME            "Amanorsac Studio"
    COMPANY_WEBSITE         "https://amanorsac.studio"
    BUNDLE_ID               "studio.amanorsac.aether"
    PLUGIN_MANUFACTURER_CODE Amsc
    PLUGIN_CODE              Aeth
    FORMATS                  ${AETHER_FORMATS}
    PRODUCT_NAME             "AETHER"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  TRUE
    VST3_CATEGORIES          Fx EQ
    AU_MAIN_TYPE             kAudioUnitType_Effect)

target_sources(AETHER PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/License/LicenseCrypto.cpp
    Source/License/LicenseClient.cpp)

target_include_directories(AETHER PRIVATE Source)

target_compile_definitions(AETHER PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# MinGW cross-builds (container/CI only): mingw-w64 has no nserror.h for the WMA reader,
# and AETHER never decodes WMA. MSVC builds are unaffected.
# Licensing (License Integration Standard §5-§7) uses the OS crypto and key stores:
# CNG + DPAPI on Windows, Security.framework on macOS.
if(WIN32)
    target_link_libraries(AETHER PRIVATE bcrypt crypt32)
elseif(APPLE)
    target_link_libraries(AETHER PRIVATE "-framework Security" "-framework CoreFoundation")
endif()

if(MINGW)
    target_compile_definitions(AETHER PUBLIC JUCE_USE_WINDOWS_MEDIA_FORMAT=0)
    # JUCE names its Windows import libraries with MSVC-only `#pragma comment(lib, ...)`,
    # which MinGW ignores, so list them explicitly for the cross-build.
    target_link_libraries(AETHER PRIVATE
        d2d1 d3d11 dxgi dcomp dwrite dwmapi dxguid
        wininet winmm imm32 version sapi dbghelp ksuser ksguid
        bcrypt crypt32
        ole32 oleaut32 uuid shlwapi comdlg32 gdi32 gdiplus user32 advapi32 shell32
        ws2_32 wsock32 rpcrt4 comctl32 crypt32 opengl32 msimg32 setupapi propsys strmiids)
endif()

target_link_libraries(AETHER PRIVATE
    AetherResources
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# ---- Licence verification test (Windows / macOS: it uses the OS crypto libraries)
if(WIN32 OR APPLE)
    juce_add_console_app(aether_license_test PRODUCT_NAME "aether_license_test")
    target_sources(aether_license_test PRIVATE
        tests/license_test.cpp
        Source/License/LicenseCrypto.cpp)
    target_include_directories(aether_license_test PRIVATE Source tests)
    target_compile_definitions(aether_license_test PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(aether_license_test PRIVATE juce::juce_core)
    if(WIN32)
        target_link_libraries(aether_license_test PRIVATE bcrypt crypt32)
        if(MINGW)
            # Same story as the plugin: JUCE names these with MSVC-only pragmas.
            target_link_libraries(aether_license_test PRIVATE wininet version ws2_32 wsock32 rpcrt4 shlwapi ole32 uuid dbghelp winmm)
        endif()
    else()
        target_link_libraries(aether_license_test PRIVATE "-framework Security" "-framework CoreFoundation")
    endif()
    add_test(NAME license COMMAND aether_license_test)
endif()

# ---- DSP unit test (no JUCE needed)
add_executable(aether_dsp_test tests/dsp_test.cpp)
target_include_directories(aether_dsp_test PRIVATE Source)
enable_testing()
add_test(NAME dsp COMMAND aether_dsp_test)
