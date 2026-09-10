# Cross-compile AETHER for Windows x64 from Linux using MinGW-w64.
# Used for container/CI builds; the supported Windows build is make_installer.bat with MSVC.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc-posix)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++-posix)
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Static-link the GCC/C++ runtimes so nothing needs MinGW DLLs on the user's machine.
set(CMAKE_EXE_LINKER_FLAGS_INIT    "-static-libgcc -static-libstdc++ -static")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++ -static")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++ -static")

# JUCE 8's juce_core relies on <cstring> being pulled in transitively by MSVC/libstdc++ headers;
# MinGW's libstdc++ does not, so force-include it for this toolchain only.
# JUCE 8 needs Direct2D 1.3 / DirectWrite 3 headers, which mingw-w64 11 lacks. A mingw-w64 v12
# header tree is searched LAST (-idirafter), so it only supplies the headers v11 is missing.
set(MINGW12_INCLUDE "/usr/local/mingw12/include")
set(CMAKE_CXX_FLAGS_INIT "-include cstring -include cstdio -include cwchar -flarge-source-files -fpermissive -DJUCE_MINGW_NO_PRAGMA_WARNING_SCOPES -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 -DNTDDI_VERSION=0x0A000006 -isystem ${MINGW12_INCLUDE} -include ${CMAKE_CURRENT_LIST_DIR}/mingw-msvc-intrin-shim.h")
set(CMAKE_C_FLAGS_INIT   "-include string.h -include stdio.h -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 -DNTDDI_VERSION=0x0A000006 -isystem ${MINGW12_INCLUDE}")
