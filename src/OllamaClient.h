// =============================================================================
// OllamaClient.h  -  Non-blocking local-LLM tip generator
// -----------------------------------------------------------------------------
// Talks to a local Ollama server (default 127.0.0.1:11434) on a background
// thread so the render loop never stalls. Requests an aquatic-themed
// cybersecurity tip from the configured model (e.g. nythos-qwen) and exposes
// the result via a thread-safe getter. If Ollama is unreachable the HUD simply
// falls back to its built-in fact list — the screensaver always works offline.
// =============================================================================
#pragma once
#include <string>
#include <atomic>
#include <mutex>
#include <thread>

namespace nyx {

class OllamaClient {
public:
    OllamaClient() = default;
    ~OllamaClient();

    void Configure(const std::wstring& host, int port, const std::wstring& model);

    // Kick off an async generation. No-op if one is already in flight.
    void RequestTip();

    // Returns true and fills `out` if a fresh tip is ready (consumes it).
    bool TryGetTip(std::wstring& out);

    bool Busy() const { return busy_.load(); }

private:
    void Worker(std::wstring prompt);

    std::wstring host_ = L"127.0.0.1";
    int          port_ = 11434;
    std::wstring model_ = L"nythos-qwen:latest";

    std::atomic<bool> busy_{false};
    std::atomic<bool> ready_{false};
    std::mutex        mtx_;
    std::wstring      result_;
    std::thread       worker_;
};

} // namespace nyx
