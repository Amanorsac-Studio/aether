/*
    AETHER — Windows installer
    Amanorsac Studio

    A single self-contained Win32 EXE. The payload (the VST3 bundle plus the README) is
    embedded as a ZIP resource and unpacked with miniz. No .NET runtime, no admin rights,
    no external dependencies.

    AETHER ships as a plugin only, so this installs, per user:
      VST3        -> %LOCALAPPDATA%\Programs\Common\VST3\Amanorsac Studio\AETHER.vst3
      Uninstaller -> %LOCALAPPDATA%\Programs\Amanorsac Studio\AETHER\
    plus an Apps & Features entry that runs that uninstaller.

    Silent install:  AETHER_Setup.exe /S  [/D=<support dir>] [/VST3=<vst3 dir>]
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <objbase.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <cstdio>

#include "miniz/miniz.h"

// ---------------------------------------------------------------- constants --
static const wchar_t* kProduct   = L"AETHER";
static const wchar_t* kCompany   = L"Amanorsac Studio";
static const wchar_t* kVersion   = L"1.0.0";
static const wchar_t* kTagline   = L"Dynamic Air Exciter";
static const wchar_t* kRegKey    = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\AmanorsacStudioAETHER";
static const wchar_t* kWndClass  = L"AetherSetupWindow";

// Palette matched to the plugin: dark chassis, silver deck, cyan light.
static const COLORREF kBg        = RGB(0x0e, 0x11, 0x16);
static const COLORREF kPanel     = RGB(0xd9, 0xde, 0xe3);
static const COLORREF kText      = RGB(0xe9, 0xee, 0xf3);
static const COLORREF kTextDim   = RGB(0x8b, 0x98, 0xa4);
static const COLORREF kAccent    = RGB(0x35, 0xe0, 0xf0);
static const COLORREF kAccentDim = RGB(0x0a, 0xa9, 0xbb);
static const COLORREF kInk       = RGB(0x1b, 0x27, 0x33);

// ------------------------------------------------------------------- state --
struct Installer
{
    std::wstring installRoot, vst3Root;
    bool  registerUninstall = true;
    bool  done = false, failed = false;
    std::wstring message;
};
static Installer g;

// -------------------------------------------------------------- utilities ---
static std::wstring knownFolder (REFKNOWNFOLDERID id)
{
    PWSTR p = nullptr; std::wstring out;
    if (SUCCEEDED (SHGetKnownFolderPath (id, 0, nullptr, &p)) && p) { out = p; CoTaskMemFree (p); }
    return out;
}

static std::wstring join (const std::wstring& a, const std::wstring& b)
{
    if (a.empty()) return b;
    return a.back() == L'\\' ? a + b : a + L"\\" + b;
}

static bool makeDirs (const std::wstring& path)
{
    if (path.empty()) return false;
    if (PathFileExistsW (path.c_str())) return true;
    const size_t slash = path.find_last_of (L'\\');
    if (slash != std::wstring::npos && slash > 2) makeDirs (path.substr (0, slash));
    return CreateDirectoryW (path.c_str(), nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}

static void removeTree (const std::wstring& path)
{
    if (! PathFileExistsW (path.c_str())) return;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW (join (path, L"*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE)
    {
        do {
            const std::wstring name = fd.cFileName;
            if (name == L"." || name == L"..") continue;
            const std::wstring child = join (path, name);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) removeTree (child);
            else { SetFileAttributesW (child.c_str(), FILE_ATTRIBUTE_NORMAL); DeleteFileW (child.c_str()); }
        } while (FindNextFileW (h, &fd));
        FindClose (h);
    }
    RemoveDirectoryW (path.c_str());
}

static std::wstring widen (const char* s)
{
    if (s == nullptr) return {};
    const int n = MultiByteToWideChar (CP_UTF8, 0, s, -1, nullptr, 0);
    std::wstring w (n > 0 ? n - 1 : 0, L'\0');
    if (n > 0) MultiByteToWideChar (CP_UTF8, 0, s, -1, &w[0], n);
    return w;
}

// --------------------------------------------------------------- payload ----
static bool payloadBytes (const void** data, size_t* size)
{
    HRSRC res = FindResourceW (nullptr, MAKEINTRESOURCEW (1), L"PAYLOAD");
    if (res == nullptr) return false;
    HGLOBAL h = LoadResource (nullptr, res);
    if (h == nullptr) return false;
    *data = LockResource (h);
    *size = SizeofResource (nullptr, res);
    return *data != nullptr && *size > 0;
}

// Unpacks the embedded zip into destRoot, mapping the archive's top-level folders.
static bool extractPayload (const std::wstring& vst3Root, const std::wstring& installRoot, std::wstring& err)
{
    const void* data = nullptr; size_t size = 0;
    if (! payloadBytes (&data, &size)) { err = L"The installer payload is missing or damaged."; return false; }

    mz_zip_archive zip; memset (&zip, 0, sizeof (zip));
    if (! mz_zip_reader_init_mem (&zip, data, size, 0)) { err = L"The installer payload could not be opened."; return false; }

    const mz_uint count = mz_zip_reader_get_num_files (&zip);
    bool ok = true;

    for (mz_uint i = 0; i < count && ok; ++i)
    {
        mz_zip_archive_file_stat st;
        if (! mz_zip_reader_file_stat (&zip, i, &st)) { ok = false; break; }

        std::wstring name = widen (st.m_filename);
        for (auto& c : name) if (c == L'/') c = L'\\';

        // Route by the archive's top-level folder.
        std::wstring dest;
        if (name.rfind (L"VST3\\", 0) == 0) dest = join (vst3Root, name.substr (5));
        else                                dest = join (installRoot, name);

        if (mz_zip_reader_is_file_a_directory (&zip, i)) { makeDirs (dest); continue; }

        const size_t slash = dest.find_last_of (L'\\');
        if (slash != std::wstring::npos) makeDirs (dest.substr (0, slash));

        size_t outSize = 0;
        void* out = mz_zip_reader_extract_to_heap (&zip, i, &outSize, 0);
        if (out == nullptr) { ok = false; break; }

        HANDLE f = CreateFileW (dest.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE) { mz_free (out); ok = false; err = L"Could not write:\n" + dest; break; }
        DWORD written = 0;
        ok = WriteFile (f, out, (DWORD) outSize, &written, nullptr) != 0 && written == outSize;
        CloseHandle (f);
        mz_free (out);
    }

    mz_zip_reader_end (&zip);
    if (! ok && err.empty()) err = L"The installer could not unpack its files.";
    return ok;
}

// --------------------------------------------------------------- registry ---
static void setRegString (HKEY key, const wchar_t* name, const std::wstring& value)
{
    RegSetValueExW (key, name, 0, REG_SZ, (const BYTE*) value.c_str(), (DWORD) ((value.size() + 1) * sizeof (wchar_t)));
}

static void registerUninstaller (const std::wstring& installRoot, const std::wstring& vst3Root)
{
    HKEY key = nullptr;
    if (RegCreateKeyExW (HKEY_CURRENT_USER, kRegKey, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return;

    const std::wstring uninstaller = join (installRoot, L"Uninstall AETHER.exe");
    setRegString (key, L"DisplayName",     std::wstring (kProduct) + L" by " + kCompany);
    setRegString (key, L"DisplayVersion",  kVersion);
    setRegString (key, L"Publisher",       kCompany);
    setRegString (key, L"InstallLocation", installRoot);
    setRegString (key, L"DisplayIcon",     uninstaller);
    setRegString (key, L"UninstallString", L"\"" + uninstaller + L"\"");
    setRegString (key, L"QuietUninstallString", L"\"" + uninstaller + L"\" /S");
    setRegString (key, L"URLInfoAbout",    L"https://amanorsac.studio");
    setRegString (key, L"Vst3Location",    vst3Root);
    DWORD one = 1;
    RegSetValueExW (key, L"NoModify", 0, REG_DWORD, (const BYTE*) &one, sizeof (one));
    RegSetValueExW (key, L"NoRepair", 0, REG_DWORD, (const BYTE*) &one, sizeof (one));
    RegCloseKey (key);
}

// ---------------------------------------------------------------- install ---
static bool runInstall (std::wstring& err)
{
    if (! makeDirs (g.vst3Root))    { err = L"Could not create:\n" + g.vst3Root; return false; }
    if (! makeDirs (g.installRoot)) { err = L"Could not create:\n" + g.installRoot; return false; }

    // Replace any previous AETHER bundle, leaving sibling plugins untouched.
    removeTree (join (g.vst3Root, L"AETHER.vst3"));

    if (! extractPayload (g.vst3Root, g.installRoot, err)) return false;

    // The installer doubles as its own uninstaller when copied in and run with /U.
    wchar_t self[MAX_PATH] {};
    GetModuleFileNameW (nullptr, self, MAX_PATH);
    CopyFileW (self, join (g.installRoot, L"Uninstall AETHER.exe").c_str(), FALSE);

    if (g.registerUninstall) registerUninstaller (g.installRoot, g.vst3Root);
    return true;
}

static void runUninstall (bool silent)
{
    std::wstring installRoot = g.installRoot, vst3Root = g.vst3Root;
    HKEY key = nullptr;
    if (RegOpenKeyExW (HKEY_CURRENT_USER, kRegKey, 0, KEY_READ, &key) == ERROR_SUCCESS)
    {
        wchar_t buf[MAX_PATH]; DWORD sz = sizeof (buf); DWORD type = 0;
        if (RegQueryValueExW (key, L"InstallLocation", nullptr, &type, (BYTE*) buf, &sz) == ERROR_SUCCESS) installRoot = buf;
        sz = sizeof (buf);
        if (RegQueryValueExW (key, L"Vst3Location", nullptr, &type, (BYTE*) buf, &sz) == ERROR_SUCCESS) vst3Root = buf;
        RegCloseKey (key);
    }

    if (! silent && MessageBoxW (nullptr,
            L"Remove AETHER?\n\nThe VST3 plugin will be deleted.\n"
            L"Your presets in Documents\\Amanorsac Studio\\AETHER are kept.",
            L"Uninstall AETHER", MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    removeTree (join (vst3Root, L"AETHER.vst3"));
    RegDeleteKeyW (HKEY_CURRENT_USER, kRegKey);

    // Delete everything except the running uninstaller, then schedule that for reboot.
    wchar_t self[MAX_PATH] {};
    GetModuleFileNameW (nullptr, self, MAX_PATH);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW (join (installRoot, L"*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE)
    {
        do {
            const std::wstring name = fd.cFileName;
            if (name == L"." || name == L"..") continue;
            const std::wstring child = join (installRoot, name);
            if (_wcsicmp (child.c_str(), self) == 0) continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) removeTree (child);
            else DeleteFileW (child.c_str());
        } while (FindNextFileW (h, &fd));
        FindClose (h);
    }
    MoveFileExW (self, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);

    if (! silent)
        MessageBoxW (nullptr, L"AETHER was removed.\n\nYour presets were kept.", L"Uninstall AETHER", MB_OK | MB_ICONINFORMATION);
}

// ------------------------------------------------------------------- UI -----
enum { ID_INSTALL = 100, ID_CLOSE };

static HFONT fTitle, fTag, fBody, fSmall, fButton;
static HWND  hInstall, hClose, hStatus;

static HFONT makeFont (int height, int weight, bool ui = true)
{
    return CreateFontW (height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                        ui ? L"Segoe UI" : L"Consolas");
}

static void paintWindow (HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint (hwnd, &ps);
    RECT rc; GetClientRect (hwnd, &rc);

    HDC mem = CreateCompatibleDC (dc);
    HBITMAP bmp = CreateCompatibleBitmap (dc, rc.right, rc.bottom);
    HBITMAP old = (HBITMAP) SelectObject (mem, bmp);

    HBRUSH bg = CreateSolidBrush (kBg);
    FillRect (mem, &rc, bg);
    DeleteObject (bg);

    // Header band
    RECT header = { 0, 0, rc.right, 132 };
    HBRUSH hb = CreateSolidBrush (RGB (0x14, 0x1a, 0x22));
    FillRect (mem, &header, hb);
    DeleteObject (hb);

    // Cyan rule under the header
    RECT rule = { 0, 132, rc.right, 134 };
    HBRUSH rb = CreateSolidBrush (kAccentDim);
    FillRect (mem, &rule, rb);
    DeleteObject (rb);

    SetBkMode (mem, TRANSPARENT);

    SelectObject (mem, fTitle);
    SetTextColor (mem, kText);
    RECT t = { 36, 34, rc.right - 36, 84 };
    DrawTextW (mem, L"AETHER", -1, &t, DT_LEFT | DT_SINGLELINE);

    // Waveform motif beside the wordmark
    SIZE ws {}; GetTextExtentPoint32W (mem, L"AETHER", 6, &ws);
    const int wx = 36 + ws.cx + 18, wy = 34 + ws.cy / 2;
    HBRUSH ab = CreateSolidBrush (kAccent);
    const int bars[9] = { 4, 7, 11, 16, 22, 16, 11, 7, 4 };
    for (int i = 0; i < 9; ++i)
    {
        RECT b = { wx + i * 7, wy - bars[i], wx + i * 7 + 3, wy + bars[i] };
        FillRect (mem, &b, ab);
    }
    DeleteObject (ab);

    SelectObject (mem, fTag);
    SetTextColor (mem, kTextDim);
    RECT tag = { 38, 86, rc.right - 36, 112 };
    DrawTextW (mem, L"DYNAMIC AIR EXCITER      \x2022      AMANORSAC STUDIO      \x2022      VERSION 1.0.0", -1, &tag, DT_LEFT | DT_SINGLELINE);

    SelectObject (mem, fBody);
    SetTextColor (mem, kText);
    RECT body = { 36, 164, rc.right - 36, 250 };
    DrawTextW (mem,
        L"AETHER is a VST3 plugin. It installs for your Windows user account only \x2014 no administrator "
        L"rights needed.\n\n"
        L"The plugin goes to your per-user VST3 folder, alongside your other Amanorsac Studio plugins. "
        L"When it is done, rescan VST3 plugins in your DAW and AETHER will be there.",
        -1, &body, DT_LEFT | DT_WORDBREAK);

    SelectObject (mem, fSmall);
    SetTextColor (mem, kTextDim);
    RECT paths = { 36, 262, rc.right - 36, 330 };
    std::wstring detail = L"Plugin\x2003\x2003" + g.vst3Root + L"\\AETHER.vst3\n"
                          L"Presets\x2003" + knownFolder (FOLDERID_Documents) + L"\\Amanorsac Studio\\AETHER";
    DrawTextW (mem, detail.c_str(), -1, &paths, DT_LEFT | DT_WORDBREAK);

    BitBlt (dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
    SelectObject (mem, old);
    DeleteObject (bmp);
    DeleteDC (mem);
    EndPaint (hwnd, &ps);
}

static LRESULT CALLBACK wndProc (HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
        case WM_CTLCOLORSTATIC:
        {
            HDC dc = (HDC) wp;
            SetBkMode (dc, TRANSPARENT);
            SetTextColor (dc, (HWND) lp == hStatus ? kAccent : kText);
            static HBRUSH br = CreateSolidBrush (kBg);
            return (LRESULT) br;
        }

        case WM_COMMAND:
        {
            const int id = LOWORD (wp);
            if (id == ID_CLOSE) { DestroyWindow (hwnd); return 0; }
            if (id == ID_INSTALL)
            {
                EnableWindow (hInstall, FALSE);
                SetWindowTextW (hStatus, L"Installing\x2026");
                UpdateWindow (hwnd);

                std::wstring err;
                const bool ok = runInstall (err);
                if (ok)
                {
                    SetWindowTextW (hStatus, L"Installed. Restart your DAW and rescan VST3 plugins.");
                    SetWindowTextW (hClose, L"Done");
                    g.done = true;
                }
                else
                {
                    SetWindowTextW (hStatus, L"Installation failed.");
                    MessageBoxW (hwnd, err.c_str(), L"AETHER Setup", MB_OK | MB_ICONERROR);
                    EnableWindow (hInstall, TRUE);
                }
                return 0;
            }
            return 0;
        }

        case WM_PAINT:   paintWindow (hwnd); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_DESTROY: PostQuitMessage (0); return 0;
    }
    return DefWindowProcW (hwnd, msg, wp, lp);
}

static int runGui (HINSTANCE inst)
{
    WNDCLASSEXW wc {};
    wc.cbSize = sizeof (wc);
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor (nullptr, IDC_ARROW);
    wc.lpszClassName = kWndClass;
    wc.hIcon = LoadIcon (nullptr, IDI_APPLICATION);
    RegisterClassExW (&wc);

    fTitle  = makeFont (46, FW_BOLD);
    fTag    = makeFont (15, FW_SEMIBOLD);
    fBody   = makeFont (18, FW_NORMAL);
    fSmall  = makeFont (15, FW_NORMAL, false);
    fButton = makeFont (18, FW_SEMIBOLD);

    const int w = 640, h = 460;
    const int x = (GetSystemMetrics (SM_CXSCREEN) - w) / 2;
    const int y = (GetSystemMetrics (SM_CYSCREEN) - h) / 2;

    HWND hwnd = CreateWindowExW (0, kWndClass, L"AETHER Setup",
                                 WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                 x, y, w, h, nullptr, nullptr, inst, nullptr);
    if (hwnd == nullptr) return 1;

    RECT rc; GetClientRect (hwnd, &rc);

    hStatus = CreateWindowExW (0, L"STATIC", L"",
                               WS_CHILD | WS_VISIBLE | SS_LEFT,
                               36, 372, rc.right - 72, 22, hwnd, nullptr, inst, nullptr);
    SendMessageW (hStatus, WM_SETFONT, (WPARAM) fBody, TRUE);

    hInstall = CreateWindowExW (0, L"BUTTON", L"Install",
                                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                rc.right - 176, rc.bottom - 54, 140, 36, hwnd, (HMENU) ID_INSTALL, inst, nullptr);
    SendMessageW (hInstall, WM_SETFONT, (WPARAM) fButton, TRUE);

    hClose = CreateWindowExW (0, L"BUTTON", L"Cancel",
                              WS_CHILD | WS_VISIBLE,
                              rc.right - 320, rc.bottom - 54, 130, 36, hwnd, (HMENU) ID_CLOSE, inst, nullptr);
    SendMessageW (hClose, WM_SETFONT, (WPARAM) fButton, TRUE);

    ShowWindow (hwnd, SW_SHOW);
    UpdateWindow (hwnd);

    MSG msg;
    while (GetMessageW (&msg, nullptr, 0, 0) > 0)
    {
        if (! IsDialogMessageW (hwnd, &msg)) { TranslateMessage (&msg); DispatchMessageW (&msg); }
    }
    return 0;
}

// ------------------------------------------------------------------ entry ---
int WINAPI wWinMain (HINSTANCE inst, HINSTANCE, LPWSTR, int)
{
    CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED);

    const std::wstring localAppData = knownFolder (FOLDERID_LocalAppData);
    g.vst3Root    = join (join (join (localAppData, L"Programs"), L"Common\\VST3"), kCompany);
    g.installRoot = join (join (localAppData, L"Programs"), std::wstring (kCompany) + L"\\" + kProduct);

    bool silent = false, uninstall = false;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW (GetCommandLineW(), &argc);
    for (int i = 1; i < argc; ++i)
    {
        std::wstring a = argv[i];
        if (_wcsicmp (a.c_str(), L"/S") == 0)                       silent = true;
        else if (_wcsicmp (a.c_str(), L"/U") == 0)                  uninstall = true;
        else if (a.rfind (L"/D=", 0) == 0)                          g.installRoot = a.substr (3);
        else if (a.rfind (L"/VST3=", 0) == 0)                       g.vst3Root = a.substr (6);
    }
    if (argv) LocalFree (argv);

    // When run from the install folder, this same binary acts as the uninstaller.
    wchar_t self[MAX_PATH] {};
    GetModuleFileNameW (nullptr, self, MAX_PATH);
    if (StrStrIW (self, L"Uninstall AETHER.exe") != nullptr) uninstall = true;

    int rc = 0;
    if (uninstall)
    {
        runUninstall (silent);
    }
    else if (silent)
    {
        std::wstring err;
        if (! runInstall (err))
        {
            rc = 1;
            wchar_t tmp[MAX_PATH] {};
            GetTempPathW (MAX_PATH, tmp);
            if (FILE* f = _wfopen (join (tmp, L"AetherSetupError.txt").c_str(), L"w, ccs=UTF-8"))
            {
                fputws (err.c_str(), f);
                fclose (f);
            }
        }
    }
    else
    {
        rc = runGui (inst);
    }

    CoUninitialize();
    return rc;
}
