// =============================================================================
// OllamaClient.cpp
// =============================================================================
#include "OllamaClient.h"
#include "Common.h"
#include <winhttp.h>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace nyx {

OllamaClient::~OllamaClient() {
    if (worker_.joinable()) worker_.join();
}

void OllamaClient::Configure(const std::wstring& host, int port, const std::wstring& model) {
    host_ = host; port_ = port; model_ = model;
}

// --- tiny helpers ------------------------------------------------------------
static std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}
static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
    return w;
}

// Minimal JSON string-value extractor for {"response":"..."} — avoids pulling
// in a JSON library. Handles \n, \", \\ escapes which is all Ollama emits here.
static std::string ExtractJsonString(const std::string& body, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    size_t k = body.find(pat);
    if (k == std::string::npos) return {};
    size_t colon = body.find(':', k + pat.size());
    if (colon == std::string::npos) return {};
    size_t q = body.find('"', colon);
    if (q == std::string::npos) return {};
    std::string out;
    for (size_t i = q + 1; i < body.size(); ++i) {
        char c = body[i];
        if (c == '\\' && i + 1 < body.size()) {
            char n = body[++i];
            switch (n) { case 'n': out += ' '; break; case 't': out += ' '; break;
                         case '"': out += '"'; break; case '\\': out += '\\'; break;
                         case 'r': break; default: out += n; }
        } else if (c == '"') break;
        else out += c;
    }
    return out;
}

void OllamaClient::RequestTip() {
    if (busy_.exchange(true)) return; // already running
    if (worker_.joinable()) worker_.join();

    // Vary the angle so repeated calls don't echo the same tip.
    static const wchar_t* topics[] = {
        L"phishing", L"TLS and HTTPS", L"least privilege", L"DNS security",
        L"CVE and patching", L"zero trust", L"ransomware defense", L"MFA",
        L"a root CA and certificates", L"password managers", L"network segmentation",
        L"backups (the 3-2-1 rule)", L"lateral movement", L"social engineering"
    };
    static unsigned pick = 0;
    std::wstring topic = topics[(pick++) % (sizeof(topics) / sizeof(topics[0]))];

    std::wstring prompt =
        L"You are the calm AI of the Nythos Cyber Aquarium. In ONE short sentence "
        L"(max 22 words), give a cybersecurity tip about " + topic +
        L", using a gentle aquatic/ocean metaphor. No preamble, no quotes, no emojis.";

    worker_ = std::thread(&OllamaClient::Worker, this, std::move(prompt));
}

bool OllamaClient::TryGetTip(std::wstring& out) {
    if (!ready_.load()) return false;
    std::lock_guard<std::mutex> lk(mtx_);
    out = result_;
    ready_.store(false);
    return !out.empty();
}

void OllamaClient::Worker(std::wstring prompt) {
    auto finish = [&](const std::wstring& text) {
        { std::lock_guard<std::mutex> lk(mtx_); result_ = text; }
        if (!text.empty()) ready_.store(true);
        busy_.store(false);
    };

    HINTERNET hSession = WinHttpOpen(L"NythosCyberAquarium/1.0",
                                     WINHTTP_ACCESS_TYPE_NO_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { finish(L""); return; }

    // Short timeouts: a stalled LLM must never hold a thread for long.
    WinHttpSetTimeouts(hSession, 3000, 3000, 4000, 30000);

    HINTERNET hConnect = WinHttpConnect(hSession, host_.c_str(), (INTERNET_PORT)port_, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); finish(L""); return; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/generate",
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); finish(L""); return; }

    // Build the JSON request body.
    std::string p = WideToUtf8(prompt);
    std::string esc;
    for (char c : p) { if (c == '"' || c == '\\') esc += '\\'; if (c == '\n') { esc += "\\n"; continue; } esc += c; }
    std::string model = WideToUtf8(model_);
    std::string body =
        "{\"model\":\"" + model + "\",\"prompt\":\"" + esc +
        "\",\"stream\":false,\"options\":{\"temperature\":0.8,\"num_predict\":60}}";

    const wchar_t* headers = L"Content-Type: application/json\r\n";
    BOOL ok = WinHttpSendRequest(hRequest, headers, (DWORD)-1L,
                                 (LPVOID)body.data(), (DWORD)body.size(),
                                 (DWORD)body.size(), 0);
    if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);

    std::string response;
    if (ok) {
        DWORD avail = 0;
        do {
            avail = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &avail) || avail == 0) break;
            std::vector<char> buf(avail + 1, 0);
            DWORD read = 0;
            if (!WinHttpReadData(hRequest, buf.data(), avail, &read)) break;
            response.append(buf.data(), read);
            if (response.size() > 65536) break; // safety cap
        } while (avail > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    std::string text = ExtractJsonString(response, "response");
    // Trim whitespace.
    size_t a = text.find_first_not_of(" \t\r\n");
    size_t b = text.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) { finish(L""); return; }
    text = text.substr(a, b - a + 1);

    finish(Utf8ToWide(text));
}

} // namespace nyx
