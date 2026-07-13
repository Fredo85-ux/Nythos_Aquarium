// =============================================================================
// main.cpp  -  Screensaver entry point and command-line dispatch
// -----------------------------------------------------------------------------
// Windows invokes a .scr with one of:
//     /s            run full-screen
//     /p <hwnd>     render the small preview into window <hwnd>
//     /c[:<hwnd>]   show the configuration dialog (parented to <hwnd>)
//     (no args)     also treated as configuration
// The handle may be the next argument or follow a ':' in the same token.
// =============================================================================
#include "Common.h"
#include "Settings.h"
#include "ScreenSaverHost.h"
#include <shellapi.h>
#include <cwchar>

#pragma comment(lib, "shell32.lib")
// Opt into the modern common controls (nice-looking settings dialog).
#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using namespace nyx;

enum class Mode { Config, Saver, Preview };

static HWND ParseHandle(const wchar_t* tok, int argc, wchar_t** argv, int& i) {
    // Handle may follow ':' in the same token, or be the next argument.
    const wchar_t* colon = wcschr(tok, L':');
    if (colon && colon[1]) return (HWND)(uintptr_t)_wcstoui64(colon + 1, nullptr, 10);
    if (i + 1 < argc)      return (HWND)(uintptr_t)_wcstoui64(argv[++i], nullptr, 10);
    return nullptr;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    // Per-monitor DPI awareness so monitor rects and rendering are pixel-exact.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    Mode mode = Mode::Config;
    HWND target = nullptr;

    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; ++i) {
        wchar_t c0 = argv[i][0], c1 = argv[i][1] ? towlower(argv[i][1]) : 0;
        if (c0 != L'/' && c0 != L'-') continue;
        if (c1 == L's')      { mode = Mode::Saver; }
        else if (c1 == L'p') { mode = Mode::Preview; target = ParseHandle(argv[i], argc, argv, i); }
        else if (c1 == L'c') { mode = Mode::Config;  target = ParseHandle(argv[i], argc, argv, i); }
    }
    if (argv) LocalFree(argv);

    Settings settings;
    settings.Load();

    switch (mode) {
        case Mode::Saver:   return RunFullscreen(hInst, settings);
        case Mode::Preview: return RunPreview(hInst, target, settings);
        case Mode::Config:  return RunConfig(hInst, target);
    }
    return 0;
}
