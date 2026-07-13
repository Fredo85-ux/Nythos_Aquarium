// =============================================================================
// fish.hlsl  -  Instanced procedural fish
// -----------------------------------------------------------------------------
// One DrawInstanced call draws the entire school. Per-instance data lives in a
// StructuredBuffer (no input layout needed). The vertex shader places an
// oriented billboard quad in the tank; the pixel shader sculpts the fish with
// an SDF whose centerline undulates along the body (smooth swimming) and bends
// with the turn rate. Far fish fade into the depth fog; cyber creatures glow.
//
// Entry points:  VSFish, PSFish
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

struct FishInstance
{
    float3 pos;     float  scale;
    float4 tint;
    float  heading; float  phase; float bend; float beat;
    float  species; float  glow;  float pad0; float pad1;
};
StructuredBuffer<FishInstance> gFish : register(t0);

struct VSOut
{
    float4 pos     : SV_POSITION;
    float2 lc      : TEXCOORD0;   // local body coords [-1,1]
    float4 tint    : TEXCOORD1;
    float4 anim    : TEXCOORD2;   // phase, bend, species, glow
    float2 depthA  : TEXCOORD3;   // depth(z), fog amount
};

VSOut VSFish(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    FishInstance f = gFish[iid];

    // Triangle-strip quad corners in local body space.
    float2 corners[4] = { float2(-1.35, -1), float2(1.15, -1),
                          float2(-1.35,  1), float2(1.15,  1) };
    float2 lc = corners[vid];

    // Shark is longer & taller; scale the local box a touch.
    bool isShark = (f.species > 5.5);
    float2 box = isShark ? float2(1.5, 0.7) : float2(1.0, 0.62);

    // Depth-driven parallax scale: near fish bigger, far fish smaller.
    float depthScale = lerp(1.15, 0.55, saturate(f.pos.z));
    float2 local = lc * box * f.scale * depthScale;

    // Orient along heading.
    float c = cos(f.heading), s = sin(f.heading);
    float2 world = float2(local.x * c - local.y * s,
                          local.x * s + local.y * c) + f.pos.xy;

    VSOut o;
    o.pos    = float4(world.x / uAspect, world.y, 0.0, 1.0);
    o.lc     = lc;
    o.tint   = f.tint;
    o.anim   = float4(f.phase, f.bend, f.species, f.glow);
    float fog = saturate(f.pos.z) * 0.85;
    o.depthA = float2(f.pos.z, fog);
    return o;
}

// signed helpers
float sdEllipseApprox(float2 p, float2 r) { return length(p / r) - 1.0; }

float4 PSFish(VSOut i) : SV_Target
{
    float2 p = i.lc;                 // [-1.35,1.15] x [-1,1]
    float phase = i.anim.x;
    float bend  = i.anim.y;
    int   species = (int)(i.anim.z + 0.5);
    float glow  = i.anim.w;
    bool  isShark = species == 6;

    // forward = +x (head). tail weight grows toward -x.
    float tailW = smoothstep(0.2, -1.2, p.x);

    // Undulating centerline: tail sweeps, body bends into turns.
    float wave = sin(phase - p.x * 3.0);
    float cy = wave * (isShark ? 0.10 : 0.22) * tailW + bend * 0.22 * (p.x * p.x);
    float y = p.y - cy;

    // Body thickness profile (fat middle, pointed snout, slim caudal peduncle).
    float bx = isShark ? 1.15 : 0.92;
    float prof = saturate(1.0 - pow(saturate(abs(p.x) / bx), isShark ? 1.7 : 1.5));
    float bodyH = (isShark ? 0.34 : 0.40) * pow(prof, 0.55);

    float alpha = 0.0;
    float3 col = i.tint.rgb;

    // --- body ---
    float bodyD = abs(y) - bodyH;
    float body = smoothstep(0.05, -0.02, bodyD) * step(p.x, bx) * step(-1.05, p.x);
    alpha = max(alpha, body);

    // dorsal/ventral shading (back darker, belly lighter)
    float shade = saturate(0.5 - y * 1.3);
    col = lerp(col * 0.55, col * 1.15 + 0.04, shade);

    // --- caudal (tail) fin ---
    float tailX = isShark ? -1.0 : -0.85;
    float tf = smoothstep(0.0, -0.35, p.x - tailX);             // region behind body
    float fanH = (isShark ? 0.85 : 0.6) * smoothstep(tailX - 0.5, tailX, p.x + 0.0);
    float tailFin = tf * smoothstep(0.42, 0.0, abs(y) - fanH * (0.2 + 0.8 * smoothstep(tailX, tailX - 0.45, p.x)));
    // forked look
    tailFin *= (1.0 - 0.6 * smoothstep(0.18, 0.0, abs(y)) * step(p.x, tailX - 0.18));
    float3 finCol = col * 0.8;
    if (tailFin > body) col = finCol;
    alpha = max(alpha, tailFin * 0.95);

    // --- dorsal fin (top) ---
    float df = smoothstep(0.0, 0.1, p.x + 0.3) * smoothstep(0.5, 0.1, p.x);
    float dfTop = bodyH + (isShark ? 0.55 : 0.28) * df;
    float dorsal = smoothstep(0.04, 0.0, abs(y - dfTop * 0.0) - 0.0); // base
    float dorsalFin = step(0.0, y) * smoothstep(0.03, 0.0, (y - dfTop)) * step(dfTop - 0.5, y) * df;
    if (dorsalFin > 0.3 && dorsalFin > body) { col = lerp(col, col * 0.7, 0.5); alpha = max(alpha, dorsalFin * 0.85); }

    // --- clownfish stripes (species 2) ---
    if (species == 2)
    {
        float band = smoothstep(0.05, 0.0, abs(p.x - 0.35)) +
                     smoothstep(0.06, 0.0, abs(p.x + 0.05)) +
                     smoothstep(0.05, 0.0, abs(p.x + 0.55));
        col = lerp(col, float3(0.98, 0.98, 1.0), saturate(band) * body * 0.9);
    }
    // neon tetra (species 3) lateral stripe
    if (species == 3)
        col = lerp(col, float3(0.2, 0.9, 1.0), smoothstep(0.10, 0.0, abs(y + 0.02)) * body * 0.7);

    // --- eye (near head) ---
    float2 ep = float2(p.x - bx * 0.62, y - 0.06);
    float eye = smoothstep(0.06, 0.045, length(ep));
    float pupil = smoothstep(0.03, 0.018, length(ep));
    col = lerp(col, float3(0.95, 0.95, 0.95), eye * body);
    col = lerp(col, float3(0.02, 0.02, 0.03), pupil * body);

    // --- emissive glow for cyber creatures ---
    if (glow > 0.01) col += i.tint.rgb * glow * (0.6 + 0.4 * sin(uTime * 6.0));

    // --- depth fog: fade far fish into the water colour ---
    float fog = i.depthA.y;
    float3 water = lerp(float3(0.06, 0.22, 0.34), float3(0.015, 0.05, 0.11), saturate(i.depthA.x));
    col = lerp(col, water, fog * 0.6);

    // simple top-down lighting sheen
    col += saturate(0.6 - y) * 0.06 * uLighting * body;

    float outA = alpha * i.tint.a * (1.0 - fog * 0.35);
    if (outA < 0.01) discard;
    return float4(col, outA);
}
