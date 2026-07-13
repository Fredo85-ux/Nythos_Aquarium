// =============================================================================
// post.hlsl  -  HDR post-processing chain
// -----------------------------------------------------------------------------
// Pipeline: HDR scene -> bright-pass -> separable Gaussian blur (H then V) ->
// composite (scene + bloom) with ACES filmic tonemap, subtle water distortion,
// chromatic edge and grain. Keeps the look soft and cinematic, not blown out.
//
// Entry points:  VSFullscreen, PSBright, PSBlurH, PSBlurV, PSComposite
// =============================================================================

cbuffer Frame : register(b0)
{
    float2 uResolution;
    float  uTime;
    float  uAspect;
    float  uLighting;
    float  uThreat;
    int    uTheme;
    float  _pad0;
};

Texture2D    gTex   : register(t0);
Texture2D    gBloom : register(t1);
SamplerState gSamp  : register(s0);

struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

VSOut VSFullscreen(uint vid : SV_VertexID)
{
    VSOut o;
    float2 p = float2((vid << 1) & 2, vid & 2);
    o.uv  = float2(p.x, p.y);
    o.pos = float4(p * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}

float3 ACES(float3 x)
{
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// Bright-pass: keep only the energetic highlights for the bloom.
float4 PSBright(VSOut i) : SV_Target
{
    float3 c = gTex.Sample(gSamp, i.uv).rgb;
    float luma = dot(c, float3(0.299, 0.587, 0.114));
    float knee = smoothstep(0.55, 1.1, luma);
    return float4(c * knee, 1.0);
}

static const float kW[5] = { 0.227027, 0.194595, 0.121622, 0.054054, 0.016216 };

float4 PSBlurH(VSOut i) : SV_Target
{
    float2 texel = 1.0 / uResolution;
    float3 r = gTex.Sample(gSamp, i.uv).rgb * kW[0];
    [unroll] for (int k = 1; k < 5; ++k)
    {
        float2 off = float2(texel.x * k * 1.5, 0);
        r += gTex.Sample(gSamp, i.uv + off).rgb * kW[k];
        r += gTex.Sample(gSamp, i.uv - off).rgb * kW[k];
    }
    return float4(r, 1.0);
}

float4 PSBlurV(VSOut i) : SV_Target
{
    float2 texel = 1.0 / uResolution;
    float3 r = gTex.Sample(gSamp, i.uv).rgb * kW[0];
    [unroll] for (int k = 1; k < 5; ++k)
    {
        float2 off = float2(0, texel.y * k * 1.5);
        r += gTex.Sample(gSamp, i.uv + off).rgb * kW[k];
        r += gTex.Sample(gSamp, i.uv - off).rgb * kW[k];
    }
    return float4(r, 1.0);
}

float4 PSComposite(VSOut i) : SV_Target
{
    // Gentle water distortion of the sample coordinate.
    float2 uv = i.uv;
    float2 warp;
    warp.x = sin(uv.y * 40.0 + uTime * 0.8) * 0.0009;
    warp.y = cos(uv.x * 38.0 - uTime * 0.7) * 0.0009;
    uv += warp;

    float3 scene = gTex.Sample(gSamp, uv).rgb;
    float3 bloom = gBloom.Sample(gSamp, uv).rgb;

    float3 col = scene + bloom * 0.9;
    col = ACES(col * (1.05 * uLighting));

    // subtle film grain so flat areas never band
    float g = frac(sin(dot(i.uv * uResolution, float2(12.9898, 78.233)) + uTime) * 43758.5453);
    col += (g - 0.5) * 0.015;

    return float4(col, 1.0);
}
