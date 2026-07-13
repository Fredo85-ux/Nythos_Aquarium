// =============================================================================
// particle.hlsl  -  Instanced bubbles and suspended motes
// -----------------------------------------------------------------------------
// Billboard point sprites from a StructuredBuffer. Bubbles get a refractive rim
// + highlight; motes are soft round specks. Additive-ish blend handled by the
// renderer's blend state.
//
// Entry points:  VSParticle, PSParticle
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

struct ParticleInstance
{
    float3 pos;  float size;
    float4 color;
    float  kind; float pad0; float pad1; float pad2;
};
StructuredBuffer<ParticleInstance> gParticles : register(t0);

struct VSOut
{
    float4 pos   : SV_POSITION;
    float2 uv    : TEXCOORD0;     // [-1,1] sprite space
    float4 color : TEXCOORD1;
    float  kind  : TEXCOORD2;
};

VSOut VSParticle(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    ParticleInstance p = gParticles[iid];
    float2 corners[4] = { float2(-1, -1), float2(1, -1), float2(-1, 1), float2(1, 1) };
    float2 c = corners[vid];

    float depthScale = lerp(1.2, 0.5, saturate(p.pos.z));
    float2 world = p.pos.xy + c * p.size * depthScale;

    VSOut o;
    o.pos   = float4(world.x / uAspect, world.y, 0, 1);
    o.uv    = c;
    o.color = p.color;
    o.kind  = p.kind;
    return o;
}

float4 PSParticle(VSOut i) : SV_Target
{
    float r = length(i.uv);
    if (r > 1.0) discard;

    if (i.kind < 0.5)
    {
        // Bubble: bright thin rim + small specular highlight, hollow centre.
        float rim   = smoothstep(1.0, 0.82, r) * smoothstep(0.6, 0.95, r);
        float hi    = smoothstep(0.35, 0.0, length(i.uv - float2(-0.35, 0.35)));
        float body  = smoothstep(1.0, 0.9, r) * 0.12;
        float a = (rim * 0.9 + hi * 0.8 + body) * i.color.a;
        float3 c = i.color.rgb + hi * 0.5;
        return float4(c, a);
    }
    else
    {
        // Mote: soft round speck.
        float a = smoothstep(1.0, 0.0, r) * i.color.a;
        return float4(i.color.rgb, a);
    }
}
