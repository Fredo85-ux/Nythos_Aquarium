// =============================================================================
// Settings.h  -  User configuration, persisted to the registry
// -----------------------------------------------------------------------------
// All tunables the settings dialog exposes live here. Values are stored under
// HKCU\Software\Nythos\CyberAquarium so they survive reinstall and roam with
// the user. Load() fills in defaults for any missing keys.
// =============================================================================
#pragma once
#include <string>

namespace nyx {

enum class Theme    { Reef, Abyss, Cyberpunk };
enum class Quality  { Performance, Balanced, Ultra };

struct Settings {
    // --- Ecosystem -------------------------------------------------------
    int    fishCount      = 140;     // autonomous agents (clamped to quality budget)
    float  bubbleDensity  = 1.0f;    // multiplier on bubble/particle emitters
    bool   threatCreatures = true;   // malware fish + ransomware shark events

    // --- Presentation ----------------------------------------------------
    Theme   theme         = Theme::Reef;
    Quality quality       = Quality::Ultra;
    float   lightingIntensity = 1.0f;
    bool    cyberHud      = true;
    bool    educationalTips = true;

    // --- Audio -----------------------------------------------------------
    bool   music          = false;
    float  volume         = 0.4f;

    // --- AI tips ---------------------------------------------------------
    bool        useOllama   = true;
    std::wstring ollamaModel = L"nythos-qwen:latest";
    std::wstring ollamaHost  = L"127.0.0.1";
    int          ollamaPort  = 11434;

    // Persistence
    void Load();
    void Save() const;

    // Effective fish budget after applying the quality cap.
    int  EffectiveFishCount() const;
};

} // namespace nyx
