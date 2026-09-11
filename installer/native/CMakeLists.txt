# AETHER installer — a standalone native Win32 executable with the payload embedded.
# Built by installer/build-installer.ps1 after the plugin's Release build; not part of
# the plugin build itself, because the payload has to exist before the .rc is compiled.
cmake_minimum_required(VERSION 3.22)
project(AETHER_Setup VERSION 1.0.0 LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "The AETHER installer is a Windows executable; configure with a Windows or MinGW toolchain.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED PAYLOAD_ZIP)
    set(PAYLOAD_ZIP "${CMAKE_CURRENT_SOURCE_DIR}/payload.zip")
endif()
if(NOT EXISTS "${PAYLOAD_ZIP}")
    message(FATAL_ERROR "Installer payload not found: ${PAYLOAD_ZIP}\nRun installer/pack_payload.py first.")
endif()

# setup.rc references payload.zip relative to itself, so stage everything in one place.
set(STAGE "${CMAKE_CURRENT_BINARY_DIR}/stage")
file(MAKE_DIRECTORY "${STAGE}")
configure_file("${PAYLOAD_ZIP}"                            "${STAGE}/payload.zip"   COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/setup.rc"      "${STAGE}/setup.rc"      COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/setup.manifest" "${STAGE}/setup.manifest" COPYONLY)

add_executable(AETHER_Setup WIN32
    AetherSetup.cpp
    miniz/miniz.c
    miniz/miniz_tdef.c
    miniz/miniz_tinfl.c
    miniz/miniz_zip.c
    "${STAGE}/setup.rc")

set_target_properties(AETHER_Setup PROPERTIES OUTPUT_NAME "AETHER_Setup")
target_include_directories(AETHER_Setup PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(AETHER_Setup PRIVATE UNICODE _UNICODE
    _WIN32_WINNT=0x0A00 WINVER=0x0A00 NTDDI_VERSION=0x0A000006)
target_link_libraries(AETHER_Setup PRIVATE shlwapi ole32 uuid comctl32 gdi32 user32 advapi32 shell32)

if(MSVC)
    target_compile_options(AETHER_Setup PRIVATE /O2 /W3 /utf-8)
    # setup.rc already embeds setup.manifest as RT_MANIFEST id 1. Left to itself the
    # MSVC linker embeds a second, generated one, and CVTRES then stops with
    # "CVT1100: duplicate resource. type:MANIFEST" followed by LNK1123. MinGW's
    # linker generates nothing, which is why the cross-build never hit this.
    target_link_options(AETHER_Setup PRIVATE /ENTRY:wWinMainCRTStartup /MANIFEST:NO)
    # Static CRT so the installer runs on a machine with no Visual C++ redistributable.
    set_property(TARGET AETHER_Setup PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
else()
    target_compile_options(AETHER_Setup PRIVATE -O2 -municode)
    target_link_options(AETHER_Setup PRIVATE -municode -static -static-libgcc -static-libstdc++ -s)
endif()
