// =============================================================================
// SystemMonitor.h  -  Real system telemetry for the cyber HUD
// -----------------------------------------------------------------------------
// Polls actual Windows counters (CPU load, memory pressure, process count,
// uptime) on a low-frequency cadence. The HUD reads the latest snapshot; the
// fish ecosystem can also react to load (a busier system = livelier "threat"
// behaviour), which ties the aquarium to the machine's real state.
// =============================================================================
#pragma once
#include <cstdint>

namespace nyx {

struct SysSnapshot {
    float cpuPercent     = 0.0f;   // 0..100, smoothed
    float memPercent     = 0.0f;   // 0..100 physical RAM in use
    uint64_t memUsedMB   = 0;
    uint64_t memTotalMB  = 0;
    int   processCount   = 0;
    float threatScore    = 12.0f;  // synthesized 0..100 "risk" indicator
    double uptimeHours   = 0.0;
};

class SystemMonitor {
public:
    void Init();
    // Call every frame; it self-throttles to ~2 Hz internally.
    void Update(double timeSeconds);
    const SysSnapshot& Snapshot() const { return snap_; }

private:
    void Sample();

    SysSnapshot snap_;
    double      lastSample_ = -100.0;

    // CPU delta tracking via GetSystemTimes.
    uint64_t prevIdle_ = 0, prevKernel_ = 0, prevUser_ = 0;
    bool     havePrev_ = false;
};

} // namespace nyx
