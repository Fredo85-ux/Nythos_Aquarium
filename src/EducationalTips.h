// =============================================================================
// EducationalTips.h  -  Rotating cybersecurity facts with fade in/out
// -----------------------------------------------------------------------------
// Owns the "current tip" shown in the HUD. Tips cycle on a relaxed cadence,
// each one fading in, holding, then fading out (never an abrupt swap). The
// pool is the built-in fact list; when Ollama is enabled and reachable, fresh
// AI-written aquatic tips are merged in seamlessly.
// =============================================================================
#pragma once
#include "OllamaClient.h"
#include <string>
#include <vector>

namespace nyx {

class EducationalTips {
public:
    void Init(bool useOllama, const std::wstring& host, int port, const std::wstring& model);
    void Update(double time, float dt);

    const std::wstring& CurrentTip() const { return current_; }
    float Alpha() const { return alpha_; }   // 0..1 for fade rendering
    bool  Enabled() const { return enabled_; }
    void  SetEnabled(bool e) { enabled_ = e; }

private:
    void NextTip();

    std::vector<std::wstring> pool_;
    std::wstring current_;
    float  alpha_ = 0;
    double nextChange_ = 6.0;     // time of next transition
    int    cursor_ = 0;
    bool   enabled_ = true;

    bool          useOllama_ = false;
    OllamaClient  ollama_;
    double        lastOllamaReq_ = -100.0;
    std::wstring  pendingAi_;
    bool          haveAi_ = false;
};

} // namespace nyx
