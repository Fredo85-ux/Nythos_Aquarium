// =============================================================================
// Engine.h  -  Per-window orchestrator
// -----------------------------------------------------------------------------
// Binds together every module (Renderer, FishSystem, ParticleSystem,
// SystemMonitor, EducationalTips, Hud) for a single output window. One Engine
// instance exists per monitor in fullscreen mode, or one for the preview pane.
// Owns its own clock so each monitor animates smoothly and independently.
// =============================================================================
#pragma once
#include "Common.h"
#include "Settings.h"
#include "Renderer.h"
#include "Fish.h"
#include "ParticleSystem.h"
#include "SystemMonitor.h"
#include "EducationalTips.h"
#include "Hud.h"

namespace nyx {

class Engine {
public:
    bool Init(HWND hwnd, const Settings& s, bool previewMode);
    void Resize(UINT w, UINT h);
    void Frame();                 // update + render one frame
    void Shutdown();
    bool Valid() const { return renderer_.Valid(); }

private:
    Settings  settings_;
    Renderer  renderer_;
    FishSystem      fish_;
    ParticleSystem  particles_;
    SystemMonitor   sysmon_;
    EducationalTips tips_;
    Hud       hud_;

    bool   preview_ = false;
    double time_ = 0;
    LARGE_INTEGER freq_{}, last_{};
    float  threatTint_ = 0;
    double nextFood_ = 4.0;
    Rng    rng_{0xBEEF1234u};
};

} // namespace nyx
