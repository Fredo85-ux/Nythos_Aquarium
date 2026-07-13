// =============================================================================
// Common.h  -  Shared utilities: COM smart pointer, HRESULT checking, logging
// =============================================================================
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wrl/client.h>   // Microsoft::WRL::ComPtr
#include <string>
#include <cstdio>
#include <cstdint>
#include <algorithm>

// Convenience alias used throughout the renderer.
template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

// -------------------------------------------------------------------------
// Lightweight debug logging. Writes to the debugger output (DbgView / VS).
// Compiled out in release-without-logging if NYX_LOG is undefined.
// -------------------------------------------------------------------------
#define NYX_LOG 1
#if NYX_LOG
inline void NyxLog(const wchar_t* fmt, ...) {
    wchar_t buf[1024];
    va_list args;
    va_start(args, fmt);
    _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, args);
    va_end(args);
    OutputDebugStringW(buf);
    OutputDebugStringW(L"\n");
}
#else
inline void NyxLog(const wchar_t*, ...) {}
#endif

// -------------------------------------------------------------------------
// HRESULT helper. On failure we log and return false from the caller. The
// renderer is written to degrade gracefully rather than crash a screensaver.
// -------------------------------------------------------------------------
#define HR_RETURN(expr)                                                     \
    do {                                                                    \
        HRESULT _hr = (expr);                                              \
        if (FAILED(_hr)) {                                                  \
            NyxLog(L"[FAIL 0x%08X] %S", (unsigned)_hr, #expr);            \
            return false;                                                   \
        }                                                                   \
    } while (0)

#define HR_CHECK(expr)                                                      \
    do {                                                                    \
        HRESULT _hr = (expr);                                              \
        if (FAILED(_hr)) NyxLog(L"[WARN 0x%08X] %S", (unsigned)_hr, #expr);\
    } while (0)
