// =============================================================================
// Renderer.cpp  -  DirectX 11 rendering core implementation
// =============================================================================
#include "Renderer.h"
#include "Shaders.gen.h"     // embedded HLSL source (generated at build time)
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace nyx {

// Frame constant buffer — must mirror the HLSL `cbuffer Frame : register(b0)`.
struct FrameCB {
    float resX, resY, time, aspect;
    float lighting, threat; int theme; float pad0;
};

// --- shader compilation from embedded source ---------------------------------
static bool CompileBlob(const char* src, const char* name, const char* entry,
                        const char* target, ComPtr<ID3DBlob>& blob) {
    ComPtr<ID3DBlob> err;
    UINT flags = D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_ENABLE_STRICTNESS;
    HRESULT hr = D3DCompile(src, strlen(src), name, nullptr, nullptr,
                            entry, target, flags, 0, &blob, &err);
    if (FAILED(hr)) {
        if (err) NyxLog(L"[SHADER %S/%S] %S", name, entry, (const char*)err->GetBufferPointer());
        return false;
    }
    return true;
}

bool Renderer::Init(HWND hwnd, bool previewMode) {
    preview_ = previewMode;
    RECT rc; GetClientRect(hwnd, &rc);
    width_  = std::max<UINT>(rc.right - rc.left, 8);
    height_ = std::max<UINT>(rc.bottom - rc.top, 8);

    if (!CreateDeviceAndSwapChain(hwnd)) return false;
    if (!CreateShaders())                return false;

    // --- frame constant buffer ---
    D3D11_BUFFER_DESC cbd{};
    cbd.ByteWidth = sizeof(FrameCB);
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    HR_RETURN(device_->CreateBuffer(&cbd, nullptr, &frameCB_));

    // --- blend states ---
    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    HR_RETURN(device_->CreateBlendState(&bd, &blendAlpha_));

    bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;       // additive
    HR_RETURN(device_->CreateBlendState(&bd, &blendAdd_));

    D3D11_BLEND_DESC od{};
    od.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    HR_RETURN(device_->CreateBlendState(&od, &blendOpaque_));

    // --- sampler ---
    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    HR_RETURN(device_->CreateSamplerState(&sd, &sampLinear_));

    // --- rasterizer (no cull, solid) ---
    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    HR_RETURN(device_->CreateRasterizerState(&rd, &raster_));

    // --- depth disabled (painter's order via CPU sort) ---
    D3D11_DEPTH_STENCIL_DESC dd{};
    dd.DepthEnable = FALSE;
    dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    HR_RETURN(device_->CreateDepthStencilState(&dd, &depthOff_));

    // --- Direct2D + DirectWrite for the HUD ---
    HR_RETURN(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                __uuidof(ID2D1Factory),
                                reinterpret_cast<void**>(d2dFactory_.GetAddressOf())));
    HR_RETURN(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                  reinterpret_cast<IUnknown**>(dwrite_.GetAddressOf())));

    if (!CreateTargets()) return false;
    NyxLog(L"[Renderer] init %ux%u tearing=%d preview=%d", width_, height_, allowTearing_, preview_);
    return true;
}

bool Renderer::CreateDeviceAndSwapChain(HWND hwnd) {
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    // flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL want[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL got{};
    HR_RETURN(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                want, _countof(want), D3D11_SDK_VERSION,
                                &device_, &got, &ctx_));

    ComPtr<IDXGIDevice> dxgiDev;
    HR_RETURN(device_.As(&dxgiDev));
    ComPtr<IDXGIAdapter> adapter;
    HR_RETURN(dxgiDev->GetAdapter(&adapter));
    ComPtr<IDXGIFactory2> factory2;
    HR_RETURN(adapter->GetParent(IID_PPV_ARGS(&factory2)));

    // Tearing (VRR / uncapped) support check.
    ComPtr<IDXGIFactory5> factory5;
    if (SUCCEEDED(factory2.As(&factory5))) {
        BOOL t = FALSE;
        if (SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &t, sizeof(t))))
            allowTearing_ = (t == TRUE);
    }

    DXGI_SWAP_CHAIN_DESC1 scd{};
    scd.Width = width_;
    scd.Height = height_;
    scd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;     // BGRA so D2D can share the surface
    scd.SampleDesc.Count = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 2;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    scd.Flags = allowTearing_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    HR_RETURN(factory2->CreateSwapChainForHwnd(device_.Get(), hwnd, &scd, nullptr, nullptr, &swap_));
    factory2->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    return true;
}

bool Renderer::CreateShaders() {
    auto vs = [&](const char* src, const char* name, const char* entry, ComPtr<ID3D11VertexShader>& out) {
        ComPtr<ID3DBlob> b;
        if (!CompileBlob(src, name, entry, "vs_5_0", b)) return false;
        return SUCCEEDED(device_->CreateVertexShader(b->GetBufferPointer(), b->GetBufferSize(), nullptr, &out));
    };
    auto ps = [&](const char* src, const char* name, const char* entry, ComPtr<ID3D11PixelShader>& out) {
        ComPtr<ID3DBlob> b;
        if (!CompileBlob(src, name, entry, "ps_5_0", b)) return false;
        return SUCCEEDED(device_->CreatePixelShader(b->GetBufferPointer(), b->GetBufferSize(), nullptr, &out));
    };

    if (!vs(shaders::scene,    "scene",    "VSFullscreen", vsFullscreen_)) return false;
    if (!ps(shaders::scene,    "scene",    "PSBackground", psBackground_)) return false;
    if (!vs(shaders::fish,     "fish",     "VSFish",       vsFish_))       return false;
    if (!ps(shaders::fish,     "fish",     "PSFish",       psFish_))       return false;
    if (!vs(shaders::particle, "particle", "VSParticle",   vsParticle_))   return false;
    if (!ps(shaders::particle, "particle", "PSParticle",   psParticle_))   return false;
    if (!ps(shaders::post,     "post",     "PSBright",     psBright_))      return false;
    if (!ps(shaders::post,     "post",     "PSBlurH",      psBlurH_))       return false;
    if (!ps(shaders::post,     "post",     "PSBlurV",      psBlurV_))       return false;
    if (!ps(shaders::post,     "post",     "PSComposite",  psComposite_))   return false;
    return true;
}

bool Renderer::CreateTargets() {
    // Back buffer RTV.
    ComPtr<ID3D11Texture2D> back;
    HR_RETURN(swap_->GetBuffer(0, IID_PPV_ARGS(&back)));
    HR_RETURN(device_->CreateRenderTargetView(back.Get(), nullptr, &backRTV_));

    // HDR scene target (full res).
    D3D11_TEXTURE2D_DESC td{};
    td.Width = width_; td.Height = height_; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    HR_RETURN(device_->CreateTexture2D(&td, nullptr, &hdrTex_));
    HR_RETURN(device_->CreateRenderTargetView(hdrTex_.Get(), nullptr, &hdrRTV_));
    HR_RETURN(device_->CreateShaderResourceView(hdrTex_.Get(), nullptr, &hdrSRV_));

    // Bloom ping-pong (half res).
    bloomW_ = std::max<UINT>(width_ / 2, 4);
    bloomH_ = std::max<UINT>(height_ / 2, 4);
    td.Width = bloomW_; td.Height = bloomH_;
    for (int i = 0; i < 2; ++i) {
        HR_RETURN(device_->CreateTexture2D(&td, nullptr, &bloomTex_[i]));
        HR_RETURN(device_->CreateRenderTargetView(bloomTex_[i].Get(), nullptr, &bloomRTV_[i]));
        HR_RETURN(device_->CreateShaderResourceView(bloomTex_[i].Get(), nullptr, &bloomSRV_[i]));
    }

    // D2D render target bound to the back buffer (for the HUD overlay).
    ComPtr<IDXGISurface> surf;
    HR_RETURN(swap_->GetBuffer(0, IID_PPV_ARGS(&surf)));
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
        96.0f, 96.0f);
    HR_RETURN(d2dFactory_->CreateDxgiSurfaceRenderTarget(surf.Get(), &props, &d2dRT_));
    return true;
}

void Renderer::ReleaseTargets() {
    d2dRT_.Reset();
    backRTV_.Reset();
    hdrRTV_.Reset(); hdrSRV_.Reset(); hdrTex_.Reset();
    for (int i = 0; i < 2; ++i) { bloomRTV_[i].Reset(); bloomSRV_[i].Reset(); bloomTex_[i].Reset(); }
}

void Renderer::Resize(UINT w, UINT h) {
    if (!swap_ || (w == width_ && h == height_) || w == 0 || h == 0) return;
    width_ = w; height_ = h;
    ctx_->OMSetRenderTargets(0, nullptr, nullptr);
    ReleaseTargets();
    swap_->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN,
                         allowTearing_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
    CreateTargets();
}

void Renderer::UpdateFrameCB(const FrameParams& fp) {
    lastFp_ = fp;
    D3D11_MAPPED_SUBRESOURCE m{};
    if (SUCCEEDED(ctx_->Map(frameCB_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        FrameCB* cb = (FrameCB*)m.pData;
        cb->resX = (float)width_; cb->resY = (float)height_;
        cb->time = fp.time; cb->aspect = fp.aspect;
        cb->lighting = fp.lighting; cb->threat = fp.threat;
        cb->theme = fp.theme; cb->pad0 = 0;
        ctx_->Unmap(frameCB_.Get(), 0);
    }
    ID3D11Buffer* cbs[] = { frameCB_.Get() };
    ctx_->VSSetConstantBuffers(0, 1, cbs);
    ctx_->PSSetConstantBuffers(0, 1, cbs);
}

static void SetViewport(ID3D11DeviceContext* ctx, UINT w, UINT h) {
    D3D11_VIEWPORT vp{}; vp.Width = (float)w; vp.Height = (float)h; vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);
}

void Renderer::BeginScene(const FrameParams& fp) {
    ctx_->RSSetState(raster_.Get());
    ctx_->OMSetDepthStencilState(depthOff_.Get(), 0);
    UpdateFrameCB(fp);

    ID3D11RenderTargetView* rtv = hdrRTV_.Get();
    ctx_->OMSetRenderTargets(1, &rtv, nullptr);
    SetViewport(ctx_.Get(), width_, height_);
    const float clear[4] = { 0, 0, 0, 1 };
    ctx_->ClearRenderTargetView(hdrRTV_.Get(), clear);
}

void Renderer::DrawBackground() {
    float bf[4] = { 0,0,0,0 };
    ctx_->OMSetBlendState(blendOpaque_.Get(), bf, 0xFFFFFFFF);
    ctx_->IASetInputLayout(nullptr);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx_->VSSetShader(vsFullscreen_.Get(), nullptr, 0);
    ctx_->PSSetShader(psBackground_.Get(), nullptr, 0);
    ctx_->Draw(3, 0);
}

bool Renderer::EnsureInstanceCapacity(ComPtr<ID3D11Buffer>& buf, ComPtr<ID3D11ShaderResourceView>& srv,
                                      UINT& cap, UINT need, UINT stride) {
    if (need <= cap && buf) return true;
    UINT newCap = cap ? cap : 64;
    while (newCap < need) newCap *= 2;

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = newCap * stride;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bd.StructureByteStride = stride;
    buf.Reset(); srv.Reset();
    HR_RETURN(device_->CreateBuffer(&bd, nullptr, &buf));

    D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.Format = DXGI_FORMAT_UNKNOWN;
    sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    sd.Buffer.FirstElement = 0;
    sd.Buffer.NumElements = newCap;
    HR_RETURN(device_->CreateShaderResourceView(buf.Get(), &sd, &srv));
    cap = newCap;
    return true;
}

void Renderer::DrawFish(const std::vector<FishInstance>& inst) {
    if (inst.empty()) return;
    if (!EnsureInstanceCapacity(fishBuf_, fishSRV_, fishCap_, (UINT)inst.size(), sizeof(FishInstance))) return;

    D3D11_MAPPED_SUBRESOURCE m{};
    if (SUCCEEDED(ctx_->Map(fishBuf_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        memcpy(m.pData, inst.data(), inst.size() * sizeof(FishInstance));
        ctx_->Unmap(fishBuf_.Get(), 0);
    }

    float bf[4] = { 0,0,0,0 };
    ctx_->OMSetBlendState(blendAlpha_.Get(), bf, 0xFFFFFFFF);
    ctx_->IASetInputLayout(nullptr);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx_->VSSetShader(vsFish_.Get(), nullptr, 0);
    ctx_->PSSetShader(psFish_.Get(), nullptr, 0);
    ID3D11ShaderResourceView* srvs[] = { fishSRV_.Get() };
    ctx_->VSSetShaderResources(0, 1, srvs);
    ctx_->DrawInstanced(4, (UINT)inst.size(), 0, 0);

    ID3D11ShaderResourceView* nullSRV[] = { nullptr };
    ctx_->VSSetShaderResources(0, 1, nullSRV);
}

void Renderer::DrawParticles(const std::vector<ParticleInstance>& inst) {
    if (inst.empty()) return;
    if (!EnsureInstanceCapacity(partBuf_, partSRV_, partCap_, (UINT)inst.size(), sizeof(ParticleInstance))) return;

    D3D11_MAPPED_SUBRESOURCE m{};
    if (SUCCEEDED(ctx_->Map(partBuf_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        memcpy(m.pData, inst.data(), inst.size() * sizeof(ParticleInstance));
        ctx_->Unmap(partBuf_.Get(), 0);
    }

    float bf[4] = { 0,0,0,0 };
    ctx_->OMSetBlendState(blendAdd_.Get(), bf, 0xFFFFFFFF);
    ctx_->IASetInputLayout(nullptr);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx_->VSSetShader(vsParticle_.Get(), nullptr, 0);
    ctx_->PSSetShader(psParticle_.Get(), nullptr, 0);
    ID3D11ShaderResourceView* srvs[] = { partSRV_.Get() };
    ctx_->VSSetShaderResources(0, 1, srvs);
    ctx_->DrawInstanced(4, (UINT)inst.size(), 0, 0);

    ID3D11ShaderResourceView* nullSRV[] = { nullptr };
    ctx_->VSSetShaderResources(0, 1, nullSRV);
}

void Renderer::FullscreenPass(ID3D11PixelShader* ps, ID3D11RenderTargetView* rtv,
                              ID3D11ShaderResourceView* src0, ID3D11ShaderResourceView* src1,
                              UINT vpW, UINT vpH) {
    ID3D11ShaderResourceView* nullSRV[2] = { nullptr, nullptr };
    ctx_->PSSetShaderResources(0, 2, nullSRV);    // clear hazards first

    ctx_->OMSetRenderTargets(1, &rtv, nullptr);
    SetViewport(ctx_.Get(), vpW, vpH);
    float bf[4] = { 0,0,0,0 };
    ctx_->OMSetBlendState(blendOpaque_.Get(), bf, 0xFFFFFFFF);
    ctx_->IASetInputLayout(nullptr);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx_->VSSetShader(vsFullscreen_.Get(), nullptr, 0);
    ctx_->PSSetShader(ps, nullptr, 0);
    ID3D11ShaderResourceView* srvs[2] = { src0, src1 };
    ctx_->PSSetShaderResources(0, 2, srvs);
    ID3D11SamplerState* samps[] = { sampLinear_.Get() };
    ctx_->PSSetSamplers(0, 1, samps);
    ctx_->Draw(3, 0);

    ctx_->PSSetShaderResources(0, 2, nullSRV);
}

void Renderer::PostProcess() {
    // Unbind HDR as render target so we can read it.
    ctx_->OMSetRenderTargets(0, nullptr, nullptr);

    // bright-pass: HDR -> bloom[0]
    FullscreenPass(psBright_.Get(), bloomRTV_[0].Get(), hdrSRV_.Get(), nullptr, bloomW_, bloomH_);
    // blur H: bloom[0] -> bloom[1]
    FullscreenPass(psBlurH_.Get(), bloomRTV_[1].Get(), bloomSRV_[0].Get(), nullptr, bloomW_, bloomH_);
    // blur V: bloom[1] -> bloom[0]
    FullscreenPass(psBlurV_.Get(), bloomRTV_[0].Get(), bloomSRV_[1].Get(), nullptr, bloomW_, bloomH_);
    // composite: HDR + bloom -> back buffer (full res)
    FullscreenPass(psComposite_.Get(), backRTV_.Get(), hdrSRV_.Get(), bloomSRV_[0].Get(), width_, height_);
}

void Renderer::Present() {
    UINT flags = allowTearing_ ? DXGI_PRESENT_ALLOW_TEARING : 0;
    swap_->Present(0, flags);   // uncapped; frame limiter lives in the main loop
}

void Renderer::Shutdown() {
    if (ctx_) ctx_->ClearState();
    ReleaseTargets();
}

} // namespace nyx
