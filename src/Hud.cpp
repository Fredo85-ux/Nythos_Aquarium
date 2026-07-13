// =============================================================================
// Hud.cpp  -  Cyber HUD rendering
// =============================================================================
#include "Hud.h"
#include <cstdio>
#include <ctime>

namespace nyx {

static D2D1_COLOR_F RGBA(float r, float g, float b, float a) { return D2D1::ColorF(r, g, b, a); }

bool Hud::EnsureResources(ID2D1RenderTarget* rt, IDWriteFactory* dw) {
    if (rt == rt_ && brPanel_) return true;   // already built for this RT
    rt_ = rt;

    // Device-dependent brushes are recreated whenever the RT changes (resize).
    brPanel_.Reset(); brStroke_.Reset(); brWhite_.Reset(); brMuted_.Reset();
    brAccent_.Reset(); brGreen_.Reset(); brAmber_.Reset(); brRed_.Reset(); brBarBg_.Reset();

    rt->CreateSolidColorBrush(RGBA(0.03f, 0.06f, 0.10f, 0.55f), &brPanel_);
    rt->CreateSolidColorBrush(RGBA(0.20f, 0.55f, 0.75f, 0.45f), &brStroke_);
    rt->CreateSolidColorBrush(RGBA(0.92f, 0.96f, 1.00f, 1.00f), &brWhite_);
    rt->CreateSolidColorBrush(RGBA(0.55f, 0.70f, 0.82f, 1.00f), &brMuted_);
    rt->CreateSolidColorBrush(RGBA(0.30f, 0.80f, 1.00f, 1.00f), &brAccent_);
    rt->CreateSolidColorBrush(RGBA(0.30f, 0.90f, 0.55f, 1.00f), &brGreen_);
    rt->CreateSolidColorBrush(RGBA(1.00f, 0.75f, 0.25f, 1.00f), &brAmber_);
    rt->CreateSolidColorBrush(RGBA(1.00f, 0.35f, 0.35f, 1.00f), &brRed_);
    rt->CreateSolidColorBrush(RGBA(1.00f, 1.00f, 1.00f, 0.12f), &brBarBg_);

    // Text formats scale with screen size (built once; size is RT-independent).
    if (!tfBrand_) {
        D2D1_SIZE_F sz = rt->GetSize();
        scale_ = (std::min)(sz.width, sz.height) / 1080.0f;
        auto mk = [&](float px, DWRITE_FONT_WEIGHT w, const wchar_t* face, ComPtr<IDWriteTextFormat>& out) {
            dw->CreateTextFormat(face, nullptr, w, DWRITE_FONT_STYLE_NORMAL,
                                 DWRITE_FONT_STRETCH_NORMAL, px * scale_, L"en-us", &out);
        };
        mk(30, DWRITE_FONT_WEIGHT_BOLD,      L"Segoe UI",       tfBrand_);
        mk(17, DWRITE_FONT_WEIGHT_SEMI_BOLD, L"Segoe UI",       tfTitle_);
        mk(16, DWRITE_FONT_WEIGHT_NORMAL,    L"Segoe UI",       tfLabel_);
        mk(16, DWRITE_FONT_WEIGHT_SEMI_BOLD, L"Consolas",       tfValue_);
        mk(13, DWRITE_FONT_WEIGHT_NORMAL,    L"Segoe UI",       tfSmall_);
        mk(64, DWRITE_FONT_WEIGHT_LIGHT,     L"Segoe UI",       tfClock_);
        mk(18, DWRITE_FONT_WEIGHT_NORMAL,    L"Segoe UI",       tfDate_);
        mk(18, DWRITE_FONT_WEIGHT_NORMAL,    L"Segoe UI",       tfTip_);
    }
    return brPanel_ != nullptr;
}

void Hud::Panel(const D2D1_RECT_F& r, float a) {
    float rad = 12.0f * scale_;
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(r, rad, rad);
    brPanel_->SetOpacity(0.55f * a);
    rt_->FillRoundedRectangle(rr, brPanel_.Get());
    brStroke_->SetOpacity(0.45f * a);
    rt_->DrawRoundedRectangle(rr, brStroke_.Get(), 1.2f * scale_);
}

static void Text(ID2D1RenderTarget* rt, const ComPtr<IDWriteTextFormat>& f, const std::wstring& s,
                 D2D1_RECT_F r, ID2D1Brush* b,
                 DWRITE_TEXT_ALIGNMENT al = DWRITE_TEXT_ALIGNMENT_LEADING) {
    f->SetTextAlignment(al);
    rt->DrawTextW(s.c_str(), (UINT32)s.size(), f.Get(), r, b, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void Hud::Render(ID2D1RenderTarget* rt, IDWriteFactory* dw, const HudInput& in, bool previewMode) {
    if (!rt) return;
    if (!EnsureResources(rt, dw)) return;

    rt->BeginDraw();

    D2D1_SIZE_F sz = rt->GetSize();
    float W = sz.width, H = sz.height;
    float s = scale_;
    float m = 28 * s;                          // outer margin
    bool tiny = previewMode || W < 480;        // skip detail panels when tiny

    wchar_t buf[256];

    // --- Brand panel (top-left) -----------------------------------------
    {
        D2D1_RECT_F r = { m, m, m + (tiny ? W * 0.9f : 360 * s), m + 78 * s };
        Panel(r);
        Text(rt, tfBrand_, L"NYTHOS", { r.left + 18 * s, r.top + 10 * s, r.right, r.bottom }, brWhite_.Get());
        Text(rt, tfSmall_, L"CYBER AQUARIUM", { r.left + 20 * s, r.top + 50 * s, r.right, r.bottom }, brAccent_.Get());
    }

    if (!tiny) {
        // --- System Health (left) --------------------------------------
        D2D1_RECT_F r = { m, m + 96 * s, m + 320 * s, m + 320 * s };
        Panel(r);
        float x = r.left + 18 * s, y = r.top + 14 * s, vx = r.right - 18 * s, lh = 30 * s;
        Text(rt, tfTitle_, L"SYSTEM HEALTH", { x, y, vx, y + 24 * s }, brAccent_.Get());
        y += 34 * s;
        bool calm = in.sys.threatScore < 50 && !in.threatActive;
        Text(rt, tfLabel_, calm ? L"●  All Systems Operational"
                                : L"●  Elevated Activity",
             { x, y, vx, y + 22 * s }, calm ? brGreen_.Get() : brAmber_.Get());
        y += 36 * s;

        auto row = [&](const wchar_t* label, const std::wstring& val, ID2D1Brush* vb) {
            Text(rt, tfLabel_, label, { x, y, vx, y + lh }, brMuted_.Get());
            Text(rt, tfValue_, val, { x, y, vx, y + lh }, vb, DWRITE_TEXT_ALIGNMENT_TRAILING);
            y += lh;
        };
        swprintf_s(buf, L"%.0f%%", in.sys.cpuPercent);
        row(L"CPU Load", buf, in.sys.cpuPercent > 80 ? brRed_.Get() : brGreen_.Get());
        swprintf_s(buf, L"%llu / %llu MB", in.sys.memUsedMB, in.sys.memTotalMB);
        row(L"Memory", buf, in.sys.memPercent > 85 ? brAmber_.Get() : brGreen_.Get());
        swprintf_s(buf, L"%d", in.sys.processCount);
        row(L"Processes", buf, brGreen_.Get());
        swprintf_s(buf, L"%.1f h", in.sys.uptimeHours);
        row(L"Uptime", buf, brMuted_.Get());
    }

    // --- Threat status (top-right) --------------------------------------
    if (!tiny) {
        float pw = 300 * s;
        D2D1_RECT_F r = { W - m - pw, m, W - m, m + 132 * s };
        Panel(r);
        float x = r.left + 18 * s, y = r.top + 14 * s, vx = r.right - 18 * s;
        bool hot = in.threatActive || in.sys.threatScore > 60;
        Text(rt, tfTitle_, hot ? L"⚠  THREAT MONITOR" : L"✓  THREAT MONITOR",
             { x, y, vx, y + 24 * s }, hot ? brRed_.Get() : brAccent_.Get());
        y += 38 * s;
        swprintf_s(buf, L"Risk Score: %.0f", in.sys.threatScore);
        Text(rt, tfLabel_, buf, { x, y, vx, y + 22 * s }, hot ? brRed_.Get() : brMuted_.Get());
        y += 30 * s;
        // risk bar
        D2D1_RECT_F barBg = { x, y, vx, y + 10 * s };
        rt->FillRoundedRectangle(D2D1::RoundedRect(barBg, 5 * s, 5 * s), brBarBg_.Get());
        float frac = in.sys.threatScore / 100.0f;
        D2D1_RECT_F bar = { x, y, x + (vx - x) * frac, y + 10 * s };
        rt->FillRoundedRectangle(D2D1::RoundedRect(bar, 5 * s, 5 * s),
                                 hot ? brRed_.Get() : brGreen_.Get());
    }

    // --- Clock (bottom-right) -------------------------------------------
    {
        SYSTEMTIME lt; GetLocalTime(&lt);
        int h12 = lt.wHour % 12; if (h12 == 0) h12 = 12;
        const wchar_t* ap = lt.wHour < 12 ? L"AM" : L"PM";
        swprintf_s(buf, L"%d:%02d %s", h12, lt.wMinute, ap);

        float pw = tiny ? W * 0.5f : 300 * s, ph = tiny ? 60 * s : 110 * s;
        D2D1_RECT_F r = { W - m - pw, H - m - ph, W - m, H - m };
        Panel(r);
        if (tiny) {
            Text(rt, tfDate_, buf, { r.left, r.top + 16 * s, r.right - 14 * s, r.bottom },
                 brWhite_.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING);
        } else {
            Text(rt, tfClock_, buf, { r.left + 18 * s, r.top + 8 * s, r.right - 18 * s, r.top + 78 * s },
                 brWhite_.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING);
            const wchar_t* days[] = { L"Sunday", L"Monday", L"Tuesday", L"Wednesday",
                                      L"Thursday", L"Friday", L"Saturday" };
            const wchar_t* mon[] = { L"January", L"February", L"March", L"April", L"May", L"June",
                                     L"July", L"August", L"September", L"October", L"November", L"December" };
            swprintf_s(buf, L"%s, %s %d, %d", days[lt.wDayOfWeek], mon[lt.wMonth - 1], lt.wDay, lt.wYear);
            Text(rt, tfDate_, buf, { r.left + 18 * s, r.top + 80 * s, r.right - 18 * s, r.bottom },
                 brMuted_.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING);
        }
    }

    // --- Educational tip (bottom-right, above clock) --------------------
    if (in.showTips && in.tipAlpha > 0.01f && !tiny && !in.tip.empty()) {
        float pw = 360 * s;
        D2D1_RECT_F r = { W - m - pw, H - m - 110 * s - 14 * s - 120 * s,
                          W - m, H - m - 110 * s - 14 * s };
        float a = in.tipAlpha;
        Panel(r, a);
        brAccent_->SetOpacity(a);
        Text(rt, tfTitle_, L"✨  TIP OF THE TANK",
             { r.left + 18 * s, r.top + 12 * s, r.right - 16 * s, r.top + 34 * s }, brAccent_.Get());
        brWhite_->SetOpacity(a * 0.95f);
        Text(rt, tfTip_, in.tip,
             { r.left + 18 * s, r.top + 42 * s, r.right - 16 * s, r.bottom - 12 * s }, brWhite_.Get());
        brAccent_->SetOpacity(1.0f);
        brWhite_->SetOpacity(1.0f);
    }

    // --- Footer ---------------------------------------------------------
    if (!tiny) {
        Text(rt, tfSmall_, L"Stay secure. Stay curious. Stay in the tank.",
             { m, H - 26 * s, W * 0.6f, H }, brMuted_.Get());
        swprintf_s(buf, L"NYTHOS CYBER AQUARIUM  ·  %d fish swimming  ·  v1.0.0", in.fishAlive);
        Text(rt, tfSmall_, buf, { W * 0.4f, H - 26 * s, W - m, H }, brMuted_.Get(),
             DWRITE_TEXT_ALIGNMENT_TRAILING);
    }

    rt->EndDraw();   // device-removed is handled by target recreation on resize
}

} // namespace nyx
