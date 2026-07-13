// =============================================================================
// Hud.h  -  Cyber HUD overlay (Direct2D / DirectWrite)
// -----------------------------------------------------------------------------
// Draws the semi-transparent Nythos dashboard on top of the rendered aquarium:
// brand panel, system-health readout (fed by real telemetry), threat status,
// threat feed, a large bottom-right clock, and the fading educational tip.
// Everything is sized off the smaller screen dimension so it scales cleanly
// across DPI settings and multi-monitor resolutions.
// =============================================================================
#pragma once
#include "Common.h"
#include "SystemMonitor.h"
#include <d2d1.h>
#include <dwrite.h>
#include <string>

namespace nyx {

struct HudInput {
    SysSnapshot  sys;
    std::wstring tip;
    float        tipAlpha = 0;
    bool         showTips = true;
    bool         threatActive = false;
    int          fishAlive = 0;
};

class Hud {
public:
    void Render(ID2D1RenderTarget* rt, IDWriteFactory* dw, const HudInput& in, bool previewMode);

private:
    bool EnsureResources(ID2D1RenderTarget* rt, IDWriteFactory* dw);
    void Panel(const D2D1_RECT_F& r, float titleAlpha = 1.0f);

    ID2D1RenderTarget* rt_ = nullptr;   // raw, not owned (detects RT recreation)
    float scale_ = 1.0f;

    ComPtr<ID2D1SolidColorBrush> brPanel_, brStroke_, brWhite_, brMuted_,
                                 brAccent_, brGreen_, brAmber_, brRed_, brBarBg_;
    ComPtr<IDWriteTextFormat> tfBrand_, tfTitle_, tfLabel_, tfValue_, tfSmall_,
                              tfClock_, tfDate_, tfTip_;
};

} // namespace nyx
