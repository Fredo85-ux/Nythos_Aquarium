// =============================================================================
// Renderer.h  -  DirectX 11 rendering core
// -----------------------------------------------------------------------------
// Owns the device, swap chain (flip-model, tearing/VRR capable), the HDR scene
// target and the bloom chain. Draw order each frame:
//
//     [HDR target]  background -> fish (instanced) -> particles (instanced)
//     [post]        bright-pass -> blur H -> blur V
//     [backbuffer]  composite (tonemap + bloom + distortion)
//     [backbuffer]  HUD (Direct2D, drawn by the Hud module)
//     Present
//
// Shaders are compiled at runtime from embedded source (Shaders.gen.h) so the
// .scr is fully self-contained — nothing to ship alongside it.
// =============================================================================
#pragma once
#include "Common.h"
#include "Fish.h"
#include "ParticleSystem.h"
#include <d3d11_1.h>
#include <dxgi1_6.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <vector>

namespace nyx {

struct FrameParams {
    float time     = 0;
    float aspect   = 1.777f;
    float lighting = 1.0f;
    float threat   = 0.0f;   // 0..1 scene threat tint
    int   theme    = 0;
};

class Renderer {
public:
    bool Init(HWND hwnd, bool previewMode);
    void Shutdown();
    void Resize(UINT w, UINT h);

    void BeginScene(const FrameParams& fp);          // bind HDR target, clear
    void DrawBackground();
    void DrawFish(const std::vector<FishInstance>& inst);
    void DrawParticles(const std::vector<ParticleInstance>& inst);
    void PostProcess();                              // bloom + composite to backbuffer

    // Direct2D render target bound to the current back buffer, for the HUD.
    ID2D1RenderTarget* D2DTarget() const { return d2dRT_.Get(); }
    IDWriteFactory*    DWrite()    const { return dwrite_.Get(); }

    void Present();

    UINT Width()  const { return width_; }
    UINT Height() const { return height_; }
    bool Valid()  const { return device_ != nullptr; }

private:
    bool CreateDeviceAndSwapChain(HWND hwnd);
    bool CreateShaders();
    bool CreateTargets();
    void ReleaseTargets();
    bool EnsureInstanceCapacity(ComPtr<ID3D11Buffer>& buf, ComPtr<ID3D11ShaderResourceView>& srv,
                                UINT& cap, UINT need, UINT stride);
    void UpdateFrameCB(const FrameParams& fp);
    void FullscreenPass(ID3D11PixelShader* ps, ID3D11RenderTargetView* rtv,
                        ID3D11ShaderResourceView* src0, ID3D11ShaderResourceView* src1,
                        UINT vpW, UINT vpH);

    // core
    ComPtr<ID3D11Device>           device_;
    ComPtr<ID3D11DeviceContext>    ctx_;
    ComPtr<IDXGISwapChain1>        swap_;
    ComPtr<ID3D11RenderTargetView> backRTV_;
    UINT width_ = 0, height_ = 0;
    bool allowTearing_ = false;
    bool preview_ = false;

    // HDR scene + bloom
    ComPtr<ID3D11Texture2D>          hdrTex_;
    ComPtr<ID3D11RenderTargetView>   hdrRTV_;
    ComPtr<ID3D11ShaderResourceView> hdrSRV_;
    ComPtr<ID3D11Texture2D>          bloomTex_[2];
    ComPtr<ID3D11RenderTargetView>   bloomRTV_[2];
    ComPtr<ID3D11ShaderResourceView> bloomSRV_[2];
    UINT bloomW_ = 0, bloomH_ = 0;

    // shaders
    ComPtr<ID3D11VertexShader> vsFullscreen_, vsFish_, vsParticle_;
    ComPtr<ID3D11PixelShader>  psBackground_, psFish_, psParticle_;
    ComPtr<ID3D11PixelShader>  psBright_, psBlurH_, psBlurV_, psComposite_;

    // state objects
    ComPtr<ID3D11Buffer>            frameCB_;
    ComPtr<ID3D11BlendState>        blendAlpha_, blendAdd_, blendOpaque_;
    ComPtr<ID3D11SamplerState>      sampLinear_;
    ComPtr<ID3D11RasterizerState>   raster_;
    ComPtr<ID3D11DepthStencilState> depthOff_;

    // dynamic instance buffers (StructuredBuffer)
    ComPtr<ID3D11Buffer>            fishBuf_;
    ComPtr<ID3D11ShaderResourceView> fishSRV_;
    UINT fishCap_ = 0;
    ComPtr<ID3D11Buffer>            partBuf_;
    ComPtr<ID3D11ShaderResourceView> partSRV_;
    UINT partCap_ = 0;

    // Direct2D / DirectWrite for HUD
    ComPtr<ID2D1Factory>      d2dFactory_;
    ComPtr<ID2D1RenderTarget> d2dRT_;
    ComPtr<IDWriteFactory>    dwrite_;

    FrameParams lastFp_;
};

} // namespace nyx
