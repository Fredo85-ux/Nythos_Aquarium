// =============================================================================
// Engine.cpp
// =============================================================================
#include "Engine.h"

namespace nyx {

bool Engine::Init(HWND hwnd, const Settings& s, bool previewMode) {
    settings_ = s;
    preview_  = previewMode;

    if (!renderer_.Init(hwnd, previewMode)) return false;

    float aspect = (float)renderer_.Width() / (float)std::max<UINT>(renderer_.Height(), 1);
    fish_.Init(0xA9F1u ^ (unsigned)(uintptr_t)hwnd, settings_.EffectiveFishCount(), aspect, settings_.threatCreatures);
    particles_.Init(0x51EEDu ^ (unsigned)(uintptr_t)hwnd, aspect, settings_.bubbleDensity);
    sysmon_.Init();
    tips_.Init(settings_.useOllama, settings_.ollamaHost, settings_.ollamaPort, settings_.ollamaModel);
    tips_.SetEnabled(settings_.educationalTips);

    QueryPerformanceFrequency(&freq_);
    QueryPerformanceCounter(&last_);
    return true;
}

void Engine::Resize(UINT w, UINT h) {
    renderer_.Resize(w, h);
    float aspect = (float)w / (float)std::max<UINT>(h, 1);
    fish_.Resize(aspect);
    particles_.Resize(aspect);
}

void Engine::Frame() {
    if (!renderer_.Valid()) return;

    // --- timing ---
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    float dt = float(double(now.QuadPart - last_.QuadPart) / double(freq_.QuadPart));
    last_ = now;
    if (dt > 0.1f) dt = 0.1f;        // guard against stalls (alt-tab, etc.)
    time_ += dt;

    // --- telemetry & simulation ---
    sysmon_.Update(time_);
    const SysSnapshot& sn = sysmon_.Snapshot();
    float loadFactor = sn.cpuPercent / 100.0f;

    tips_.Update(time_, dt);
    fish_.Update(dt, time_, loadFactor);
    particles_.Update(dt, (float)time_);

    // occasional food to spark curiosity / feeding behaviour (no fixed timer)
    if (time_ > nextFood_) {
        fish_.AddFood({ rng_.Range(-1.4f, 1.4f), rng_.Range(0.3f, 0.9f), rng_.Range(0.2f, 0.7f) });
        nextFood_ = time_ + rng_.Range(6.0f, 16.0f);
    }

    // threat scene tint follows shark/malware presence
    float target = fish_.SharkActive() ? 1.0f : 0.0f;
    threatTint_ = Damp(threatTint_, target, 1.2f, dt);

    // --- render ---
    FrameParams fp;
    fp.time     = (float)time_;
    fp.aspect   = (float)renderer_.Width() / (float)std::max<UINT>(renderer_.Height(), 1);
    fp.lighting = settings_.lightingIntensity;
    fp.threat   = threatTint_;
    fp.theme    = (int)settings_.theme;

    renderer_.BeginScene(fp);
    renderer_.DrawBackground();
    renderer_.DrawFish(fish_.Instances());
    renderer_.DrawParticles(particles_.Instances());
    renderer_.PostProcess();

    if (settings_.cyberHud) {
        HudInput in;
        in.sys          = sn;
        in.tip          = tips_.CurrentTip();
        in.tipAlpha     = tips_.Alpha();
        in.showTips     = settings_.educationalTips;
        in.threatActive = fish_.SharkActive();
        in.fishAlive    = fish_.AliveCount();
        hud_.Render(renderer_.D2DTarget(), renderer_.DWrite(), in, preview_);
    }

    renderer_.Present();
}

void Engine::Shutdown() {
    renderer_.Shutdown();
}

} // namespace nyx
