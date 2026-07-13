// =============================================================================
// ScreenSaverHost.cpp
// =============================================================================
#include "ScreenSaverHost.h"
#include "Engine.h"
#include <vector>
#include <memory>
#include <functional>
#include <windowsx.h>
#include <timeapi.h>

#pragma comment(lib, "winmm.lib")

namespace nyx {

static const wchar_t* kClass   = L"NythosCyberAquariumWnd";
static bool           g_quit   = false;
static POINT          g_origin = { -1, -1 };   // first observed cursor pos (fullscreen)
static bool           g_fullscreen = false;

// One window <-> one Engine.
struct Window { HWND hwnd = nullptr; std::unique_ptr<Engine> engine; };

static void RequestQuit() { g_quit = true; PostQuitMessage(0); }

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Engine* eng = reinterpret_cast<Engine*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_SIZE:
        if (eng && wp != SIZE_MINIMIZED)
            eng->Resize(LOWORD(lp), HIWORD(lp));
        return 0;

    case WM_SETCURSOR:
        if (g_fullscreen) { SetCursor(nullptr); return TRUE; }   // hide cursor
        break;

    case WM_MOUSEMOVE:
        if (g_fullscreen) {
            POINT p{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ClientToScreen(hwnd, &p);
            if (g_origin.x < 0) { g_origin = p; }
            else if (abs(p.x - g_origin.x) > 6 || abs(p.y - g_origin.y) > 6) RequestQuit();
        }
        return 0;

    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
    case WM_KEYDOWN:     case WM_SYSKEYDOWN:
        if (g_fullscreen) RequestQuit();
        return 0;

    case WM_CLOSE:
        RequestQuit();
        return 0;

    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void RegisterClassOnce(HINSTANCE hInst) {
    static bool done = false;
    if (done) return;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = kClass;
    RegisterClassExW(&wc);
    done = true;
}

// Monitor enumeration callback collects each monitor's pixel rectangle.
static BOOL CALLBACK MonitorEnum(HMONITOR, HDC, LPRECT rc, LPARAM data) {
    auto* rects = reinterpret_cast<std::vector<RECT>*>(data);
    rects->push_back(*rc);
    return TRUE;
}

// Shared main loop with a frame limiter (keeps CPU low at high refresh rates).
static int RunLoop(std::vector<Window>& windows, int targetFps, std::function<bool()> keepGoing) {
    timeBeginPeriod(1);
    LARGE_INTEGER freq; QueryPerformanceFrequency(&freq);
    const double frameTarget = 1.0 / targetFps;

    MSG msg{};
    while (!g_quit) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { g_quit = true; break; }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (g_quit || (keepGoing && !keepGoing())) break;

        LARGE_INTEGER a; QueryPerformanceCounter(&a);
        for (auto& w : windows) if (w.engine) w.engine->Frame();
        LARGE_INTEGER b; QueryPerformanceCounter(&b);

        double elapsed = double(b.QuadPart - a.QuadPart) / double(freq.QuadPart);
        double remain = frameTarget - elapsed;
        if (remain > 0.0015) Sleep((DWORD)((remain - 0.001) * 1000.0));   // yield CPU
    }

    timeEndPeriod(1);
    return 0;
}

int RunFullscreen(HINSTANCE hInst, const Settings& settings) {
    g_fullscreen = true;
    RegisterClassOnce(hInst);

    std::vector<RECT> mons;
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnum, (LPARAM)&mons);
    if (mons.empty()) {
        RECT r{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
        mons.push_back(r);
    }

    std::vector<Window> windows;
    for (const RECT& r : mons) {
        Window w;
        w.hwnd = CreateWindowExW(WS_EX_TOPMOST, kClass, L"Nythos Cyber Aquarium",
                                 WS_POPUP | WS_VISIBLE,
                                 r.left, r.top, r.right - r.left, r.bottom - r.top,
                                 nullptr, nullptr, hInst, nullptr);
        if (!w.hwnd) continue;
        w.engine = std::make_unique<Engine>();
        if (!w.engine->Init(w.hwnd, settings, /*preview*/ false)) {
            DestroyWindow(w.hwnd);
            continue;
        }
        SetWindowLongPtrW(w.hwnd, GWLP_USERDATA, (LONG_PTR)w.engine.get());
        ShowWindow(w.hwnd, SW_SHOW);
        windows.push_back(std::move(w));
    }
    if (windows.empty()) return 1;

    ShowCursor(FALSE);
    SetForegroundWindow(windows[0].hwnd);

    int targetFps = (settings.quality == Quality::Performance) ? 60 : 144;
    RunLoop(windows, targetFps, nullptr);

    ShowCursor(TRUE);
    for (auto& w : windows) { if (w.engine) w.engine->Shutdown(); if (w.hwnd) DestroyWindow(w.hwnd); }
    return 0;
}

int RunPreview(HINSTANCE hInst, HWND parent, const Settings& settings) {
    if (!IsWindow(parent)) return 1;
    g_fullscreen = false;
    RegisterClassOnce(hInst);

    RECT rc; GetClientRect(parent, &rc);
    Window w;
    w.hwnd = CreateWindowExW(0, kClass, L"", WS_CHILD | WS_VISIBLE,
                             0, 0, rc.right, rc.bottom, parent, nullptr, hInst, nullptr);
    if (!w.hwnd) return 1;
    w.engine = std::make_unique<Engine>();
    if (!w.engine->Init(w.hwnd, settings, /*preview*/ true)) return 1;
    SetWindowLongPtrW(w.hwnd, GWLP_USERDATA, (LONG_PTR)w.engine.get());

    std::vector<Window> windows;
    windows.push_back(std::move(w));

    // Preview runs at a modest rate and stops when the host pane disappears.
    RunLoop(windows, 60, [parent]() { return IsWindow(parent) != FALSE; });

    for (auto& win : windows) { if (win.engine) win.engine->Shutdown(); if (win.hwnd) DestroyWindow(win.hwnd); }
    return 0;
}

} // namespace nyx
