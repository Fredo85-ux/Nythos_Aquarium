// =============================================================================
// Settings.cpp  -  Registry load/save
// =============================================================================
#include "Settings.h"
#include "Common.h"

namespace nyx {

static const wchar_t* kKey = L"Software\\Nythos\\CyberAquarium";

// --- small registry helpers --------------------------------------------------
static DWORD RegGetDword(HKEY h, const wchar_t* name, DWORD def) {
    DWORD val = def, size = sizeof(val), type = 0;
    if (RegQueryValueExW(h, name, nullptr, &type, (LPBYTE)&val, &size) != ERROR_SUCCESS || type != REG_DWORD)
        return def;
    return val;
}
static std::wstring RegGetStr(HKEY h, const wchar_t* name, const std::wstring& def) {
    wchar_t buf[512]; DWORD size = sizeof(buf), type = 0;
    if (RegQueryValueExW(h, name, nullptr, &type, (LPBYTE)buf, &size) != ERROR_SUCCESS || type != REG_SZ)
        return def;
    return std::wstring(buf, (size / sizeof(wchar_t)) ? (size / sizeof(wchar_t) - 1) : 0);
}
static void RegSetDword(HKEY h, const wchar_t* name, DWORD v) {
    RegSetValueExW(h, name, 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
}
static void RegSetStr(HKEY h, const wchar_t* name, const std::wstring& v) {
    RegSetValueExW(h, name, 0, REG_SZ, (const BYTE*)v.c_str(), DWORD((v.size() + 1) * sizeof(wchar_t)));
}

// Encode a float as fixed-point x1000 so we can keep everything in DWORDs.
static DWORD F2D(float f) { return (DWORD)(int)(f * 1000.0f); }
static float D2F(DWORD d) { return (int)d / 1000.0f; }

void Settings::Load() {
    HKEY h;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kKey, 0, KEY_READ, &h) != ERROR_SUCCESS)
        return; // keep defaults

    fishCount        = (int)RegGetDword(h, L"FishCount", fishCount);
    bubbleDensity    = D2F(RegGetDword(h, L"BubbleDensity", F2D(bubbleDensity)));
    threatCreatures  = RegGetDword(h, L"ThreatCreatures", threatCreatures) != 0;
    theme            = (Theme)RegGetDword(h, L"Theme", (DWORD)theme);
    quality          = (Quality)RegGetDword(h, L"Quality", (DWORD)quality);
    lightingIntensity= D2F(RegGetDword(h, L"Lighting", F2D(lightingIntensity)));
    cyberHud         = RegGetDword(h, L"CyberHud", cyberHud) != 0;
    educationalTips  = RegGetDword(h, L"EduTips", educationalTips) != 0;
    music            = RegGetDword(h, L"Music", music) != 0;
    volume           = D2F(RegGetDword(h, L"Volume", F2D(volume)));
    useOllama        = RegGetDword(h, L"UseOllama", useOllama) != 0;
    ollamaModel      = RegGetStr(h, L"OllamaModel", ollamaModel);
    ollamaHost       = RegGetStr(h, L"OllamaHost", ollamaHost);
    ollamaPort       = (int)RegGetDword(h, L"OllamaPort", ollamaPort);

    RegCloseKey(h);
}

void Settings::Save() const {
    HKEY h; DWORD disp;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kKey, 0, nullptr, 0, KEY_WRITE, nullptr, &h, &disp) != ERROR_SUCCESS)
        return;

    RegSetDword(h, L"FishCount", fishCount);
    RegSetDword(h, L"BubbleDensity", F2D(bubbleDensity));
    RegSetDword(h, L"ThreatCreatures", threatCreatures);
    RegSetDword(h, L"Theme", (DWORD)theme);
    RegSetDword(h, L"Quality", (DWORD)quality);
    RegSetDword(h, L"Lighting", F2D(lightingIntensity));
    RegSetDword(h, L"CyberHud", cyberHud);
    RegSetDword(h, L"EduTips", educationalTips);
    RegSetDword(h, L"Music", music);
    RegSetDword(h, L"Volume", F2D(volume));
    RegSetDword(h, L"UseOllama", useOllama);
    RegSetStr  (h, L"OllamaModel", ollamaModel);
    RegSetStr  (h, L"OllamaHost", ollamaHost);
    RegSetDword(h, L"OllamaPort", ollamaPort);

    RegCloseKey(h);
}

int Settings::EffectiveFishCount() const {
    int cap = 0;
    switch (quality) {
        case Quality::Performance: cap = 80;  break;
        case Quality::Balanced:    cap = 200; break;
        case Quality::Ultra:       cap = 600; break;
    }
    int n = fishCount < 8 ? 8 : fishCount;
    return n > cap ? cap : n;
}

} // namespace nyx
