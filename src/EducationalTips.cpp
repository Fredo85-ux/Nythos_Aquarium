// =============================================================================
// EducationalTips.cpp
// =============================================================================
#include "EducationalTips.h"
#include "NyxMath.h"

namespace nyx {

void EducationalTips::Init(bool useOllama, const std::wstring& host, int port, const std::wstring& model) {
    useOllama_ = useOllama;
    ollama_.Configure(host, port, model);

    // Built-in fact pool — works fully offline. Aquatic framing matches theme.
    pool_ = {
        L"CVE is a public catalog of known software weaknesses — like charting reefs so ships avoid the rocks.",
        L"DNS is the ocean's current map: it turns names into addresses. Poison the current and ships drift astray.",
        L"A Root CA is the lighthouse of trust — every certificate's chain leads back to one you already trust.",
        L"TLS encrypts data in transit, like a sealed capsule that only the right diver can open.",
        L"Least privilege: give each fish only the depth it needs. Fewer doors, fewer ways in.",
        L"MFA is a second current to swim against — a stolen password alone can't reach the surface.",
        L"Patch early. An unpatched system is a cracked tank: small leaks sink the whole reef.",
        L"Phishing lures glitter like bait. Hover before you bite — check the link's true destination.",
        L"Backups are your spawning grounds: the 3-2-1 rule keeps the ecosystem alive after a storm.",
        L"Zero Trust assumes every fish could be a predator — verify each, every time, everywhere.",
        L"Ransomware freezes your reef and demands a ransom. Offline backups thaw it without paying.",
        L"Network segmentation builds reef walls: a breach in one cove can't flood the whole tank.",
        L"Lateral movement is a predator slipping reef to reef. Segment and monitor to cut it off.",
        L"A password manager is your shell: one strong guard for many delicate, unique keys.",
    };

    cursor_ = 0;
    current_ = pool_.empty() ? L"" : pool_[0];
    alpha_ = 0;
    nextChange_ = 5.0;
}

void EducationalTips::NextTip() {
    // Prefer a fresh AI tip if one arrived; otherwise cycle the built-in pool.
    if (haveAi_ && !pendingAi_.empty()) {
        current_ = pendingAi_;
        pendingAi_.clear();
        haveAi_ = false;
    } else if (!pool_.empty()) {
        cursor_ = (cursor_ + 1) % (int)pool_.size();
        current_ = pool_[cursor_];
    }
}

void EducationalTips::Update(double time, float dt) {
    if (!enabled_) { alpha_ = Damp(alpha_, 0.0f, 3.0f, dt); return; }

    // Pull a new AI tip occasionally (well ahead of when we'll show it).
    if (useOllama_ && !haveAi_ && (time - lastOllamaReq_) > 45.0 && !ollama_.Busy()) {
        lastOllamaReq_ = time;
        ollama_.RequestTip();
    }
    std::wstring aiTip;
    if (ollama_.TryGetTip(aiTip) && !aiTip.empty()) { pendingAi_ = aiTip; haveAi_ = true; }

    // Fade cycle: each tip shows for ~18s — ~2s fade in, hold, ~2s fade out.
    double cyclePos = time - (nextChange_ - 18.0);
    if (time >= nextChange_) {
        NextTip();
        nextChange_ = time + 18.0;
        cyclePos = 0;
    }
    float target;
    if (cyclePos < 2.0)        target = (float)(cyclePos / 2.0);          // fade in
    else if (cyclePos > 16.0)  target = (float)((18.0 - cyclePos) / 2.0); // fade out
    else                       target = 1.0f;                             // hold
    alpha_ = Damp(alpha_, Clamp(target, 0, 1), 5.0f, dt);
}

} // namespace nyx
