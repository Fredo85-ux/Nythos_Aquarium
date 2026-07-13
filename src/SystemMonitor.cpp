// =============================================================================
// SystemMonitor.cpp
// =============================================================================
#include "SystemMonitor.h"
#include "NyxMath.h"
#include "Common.h"
#include <psapi.h>
#include <tlhelp32.h>

#pragma comment(lib, "psapi.lib")

namespace nyx {

static uint64_t FT2U64(const FILETIME& ft) {
    ULARGE_INTEGER u; u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime;
    return u.QuadPart;
}

void SystemMonitor::Init() {
    Sample();          // prime CPU deltas
    havePrev_ = false; // first real read happens next Update
}

void SystemMonitor::Update(double t) {
    if (t - lastSample_ < 0.5) return; // throttle to ~2 Hz
    lastSample_ = t;
    Sample();
}

void SystemMonitor::Sample() {
    // --- CPU via system times -------------------------------------------
    FILETIME idleFt, kernFt, userFt;
    if (GetSystemTimes(&idleFt, &kernFt, &userFt)) {
        uint64_t idle = FT2U64(idleFt), kern = FT2U64(kernFt), user = FT2U64(userFt);
        if (havePrev_) {
            uint64_t dIdle = idle - prevIdle_;
            uint64_t dKern = kern - prevKernel_;
            uint64_t dUser = user - prevUser_;
            uint64_t total = dKern + dUser;          // kernel already includes idle
            float load = total ? float(total - dIdle) / float(total) * 100.0f : 0.0f;
            snap_.cpuPercent = Damp(snap_.cpuPercent, Clamp(load, 0, 100), 6.0f, 0.5f);
        }
        prevIdle_ = idle; prevKernel_ = kern; prevUser_ = user;
        havePrev_ = true;
    }

    // --- Memory ----------------------------------------------------------
    MEMORYSTATUSEX ms{}; ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        snap_.memPercent  = (float)ms.dwMemoryLoad;
        snap_.memTotalMB  = ms.ullTotalPhys / (1024 * 1024);
        snap_.memUsedMB   = (ms.ullTotalPhys - ms.ullAvailPhys) / (1024 * 1024);
    }

    // --- Process count (cheap snapshot) ----------------------------------
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
        int count = 0;
        if (Process32FirstW(snap, &pe)) { do { ++count; } while (Process32NextW(snap, &pe)); }
        snap_.processCount = count;
        CloseHandle(snap);
    }

    // --- Uptime ----------------------------------------------------------
    snap_.uptimeHours = GetTickCount64() / 3600000.0;

    // --- Synthesized "threat score" --------------------------------------
    // A gentle composite of real load so the HUD reflects the machine without
    // ever pretending to be a real security verdict. Stays calm on idle boxes.
    float t = 8.0f
            + snap_.cpuPercent * 0.30f
            + (snap_.memPercent - 50.0f) * 0.20f
            + (snap_.processCount > 200 ? (snap_.processCount - 200) * 0.05f : 0.0f);
    snap_.threatScore = Damp(snap_.threatScore, Clamp(t, 2, 96), 3.0f, 0.5f);
}

} // namespace nyx
