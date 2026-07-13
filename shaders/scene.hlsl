// =============================================================================
// scene.hlsl  -  Procedural underwater reef background
// -----------------------------------------------------------------------------
// A fullscreen pass that paints the whole tank: ocean-depth gradient, drifting
// volumetric god rays, animated caustic shimmer, a noise-sculpted reef floor
// silhouette, depth fog and a soft vignette. Everything animates off uTime with
// incommensurate frequencies so it never visibly loops.
//
// Entry points:  VSFullscreen  (shared fullscreen triangle)
//                PSBackground
// =============================================================================

cbuffer Frame : register(b0)
{
    float2 uResolution;
    float  uTime;
    float  uAspect;
    float  uLighting;
    float  uThreat;     // 0..1, gently warms the water during a "threat" event
    int    uTheme;      // 0 Reef, 1 Abyss, 2 Cyberpunk
    float  _pad0;
};

struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

// Fullscreen triangle — no vertex buffer needed.
VSOut VSFullscreen(uint vid : SV_VertexID)
{
    VSOut o;
    float2 p = float2((vid << 1) & 2, vid & 2);   // (0,0)(2,0)(0,2)
    o.uv  = p;                                     // 0..2
    o.pos = float4(p * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}

// --- noise toolkit -----------------------------------------------------------
float hash21(float2 p)
{
    p = frac(p * float2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return frac(p.x * p.y);
}
float vnoise(float2 p)
{
    float2 i = floor(p), f = frac(p);
    float2 u = f * f * (3.0 - 2.0 * f);
    float a = hash21(i + float2(0, 0));
    float b = hash21(i + float2(1, 0));
    float c = hash21(i + float2(0, 1));
    float d = hash21(i + float2(1, 1));
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}
float fbm(float2 p)
{
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 5; ++i) { v += a * vnoise(p); p *= 2.02; a *= 0.5; }
    return v;
}

// Caustic network: domain-warped sine grid, animated.
float caustics(float2 uv, float t)
{
    float2 p = uv * 6.0;
    p += float2(fbm(p * 0.5 + t * 0.05), fbm(p * 0.5 - t * 0.04)) * 1.5;
    float c = 0.0;
    c += sin(p.x * 1.3 + t * 0.9) * sin(p.y * 1.1 - t * 0.7);
    c += sin(p.x * 0.7 - t * 0.6 + 1.7) * sin(p.y * 1.7 + t * 0.5);
    c = c * 0.25 + 0.5;
    return pow(saturate(c), 3.0);
}

float3 ThemeDeep(int theme)
{
    if (theme == 1) return float3(0.01, 0.02, 0.05);   // Abyss
    if (theme == 2) return float3(0.03, 0.02, 0.08);   // Cyberpunk
    return float3(0.015, 0.05, 0.11);                  // Reef
}
float3 ThemeShallow(int theme)
{
    if (theme == 1) return float3(0.04, 0.10, 0.18);
    if (theme == 2) return float3(0.10, 0.06, 0.22);
    return float3(0.06, 0.22, 0.34);
}

float4 PSBackground(VSOut i) : SV_Target
{
    float2 uv = i.uv;                       // 0..1 across the visible screen
    float2 suv = float2(uv.x * uAspect, uv.y);
    float t = uTime;

    // --- depth gradient (top lit, bottom dark) --------------------------
    float depth = saturate(uv.y);
    float3 col = lerp(ThemeShallow(uTheme), ThemeDeep(uTheme), pow(depth, 0.8));

    // --- volumetric god rays from upper area ----------------------------
    float2 lightDir = normalize(float2(0.15, 1.0));
    float rays = 0.0;
    float2 sp = suv;
    [unroll] for (int s = 0; s < 6; ++s)
    {
        sp -= lightDir * 0.06;
        float n = fbm(float2(sp.x * 3.0 + t * 0.06, sp.y * 1.2 - t * 0.10));
        rays += n;
    }
    rays /= 6.0;
    float rayMask = saturate(1.0 - uv.y) * saturate(1.0 - abs(suv.x - uAspect * 0.45) * 0.5);
    col += float3(0.12, 0.24, 0.30) * rays * rayMask * 2.2 * uLighting;

    // --- caustic shimmer (stronger near the surface) --------------------
    float ca = caustics(suv, t);
    col += float3(0.12, 0.22, 0.26) * ca * saturate(1.0 - uv.y * 1.3) * uLighting * 1.3;

    // =====================================================================
    //  LAYERED REEF  (side-on view: the reef rises from the bottom edge,
    //  with depth built from a hazy far ridge, colourful mid coral, swaying
    //  kelp, a sandy bed, and dark foreground coral framing the corners).
    //  uv.y: 0 = surface (top), 1 = tank floor (bottom).
    // =====================================================================
    float fx = suv.x;

    // --- (1) far reef ridge: a distinct hazy silhouette band behind coral -
    float farH    = 0.42 + 0.12 * fbm(float2(fx * 1.1, 11.0)) + 0.05 * sin(fx * 1.7 + 1.3);
    float farLine = 1.0 - farH;
    float farMask = smoothstep(farLine - 0.06, farLine + 0.02, uv.y);
    float3 farCol = float3(0.04, 0.12, 0.18);                            // murky blue ridge
    col = lerp(col, farCol, farMask * 0.85);

    // --- (2) swaying kelp / sea grass rising tall from the reef ----------
    float kelp = 0.0;
    [unroll] for (int kk = 0; kk < 13; ++kk)
    {
        float seed = frac(sin(kk * 91.7) * 4137.13);
        float kx   = (seed * 2.0 - 1.0) * uAspect * 0.98;
        float rootH = 0.28 + 0.26 * frac(sin(kk * 33.3) * 982.4);        // strand height
        float rootY = 1.0;                                              // grows from floor
        float topY  = rootY - rootH;
        float grow  = smoothstep(rootY, topY, uv.y);                    // 0 root .. 1 tip
        float sway  = sin(t * (0.6 + seed) + kk + uv.y * 6.0) * 0.06 * grow;
        float dx    = abs(fx - (kx + sway));
        float strand = smoothstep(0.020, 0.004, dx)
                     * smoothstep(rootY + 0.01, rootY - 0.02, uv.y)
                     * step(topY, uv.y);
        kelp = max(kelp, strand * (0.45 + 0.55 * grow));
    }
    float3 kelpCol = lerp(float3(0.04, 0.20, 0.10), float3(0.10, 0.42, 0.20), 0.5);
    col = lerp(col, kelpCol, kelp * 0.9);

    // --- (3) main coral reef: tall chunky mounds, vivid colour, AO + rim --
    float midH    = 0.26 + 0.14 * fbm(float2(fx * 2.3, 4.0)) + 0.06 * fbm(float2(fx * 6.5, 9.0));
    float midLine = 1.0 - midH;
    float midMask = smoothstep(midLine - 0.025, midLine + 0.010, uv.y);

    float hueN = fbm(float2(fx * 3.1, 2.0));
    float3 coralGreen  = float3(0.16, 0.50, 0.32);
    float3 coralPurple = float3(0.52, 0.22, 0.60);
    float3 coralOrange = float3(0.80, 0.40, 0.16);
    float3 midCol = lerp(coralGreen, coralPurple, smoothstep(0.30, 0.70, hueN));
    midCol = lerp(midCol, coralOrange, smoothstep(0.60, 0.92, fbm(float2(fx * 5.0, 7.0))));
    midCol *= 0.65 + 0.55 * fbm(float2(fx * 24.0, uv.y * 24.0));         // bumpy texture
    // ambient occlusion toward the base so mounds feel rounded; lift overall
    midCol *= lerp(0.55, 1.25, smoothstep(midLine + 0.12, midLine - 0.02, uv.y));
    midCol = midCol * (0.7 + 0.5 * uLighting) + 0.025;                   // brighten + ambient
    col = lerp(col, midCol, midMask);

    // rim light + caustics catching the coral crest
    float crest = smoothstep(midLine + 0.06, midLine - 0.01, uv.y) * midMask;
    col += (float3(0.18, 0.38, 0.34) + float3(0.20, 0.26, 0.28) * ca) * crest * uLighting;

    // --- (4) sandy bed lit by caustics at the very bottom ----------------
    float sandLine = 0.965;
    float sandMask = smoothstep(sandLine - 0.02, sandLine + 0.01, uv.y) * (1.0 - midMask * 0.9);
    float3 sand = lerp(float3(0.16, 0.20, 0.20), float3(0.24, 0.27, 0.25),
                       fbm(float2(fx * 26.0, uv.y * 10.0)));
    sand += caustics(suv * 1.6, t * 1.1) * 0.16;                         // dappled light
    col = lerp(col, sand, sandMask);

    // --- (5) foreground coral framing the lower corners (depth) ----------
    float edge   = smoothstep(uAspect * 0.70, uAspect * 1.00, abs(fx));
    float framH  = 0.50 + 0.34 * fbm(float2(fx * 3.5, 20.0));
    float framLine = 1.0 - framH;
    float framMask = edge * smoothstep(framLine - 0.03, framLine + 0.03, uv.y);
    col = lerp(col, float3(0.010, 0.030, 0.045), framMask * 0.92);       // near-black silhouette

    // --- threat warmth (subtle red push during cyber events) ------------
    col = lerp(col, col * float3(1.25, 0.85, 0.85) + float3(0.04, 0.0, 0.0), uThreat * 0.35);

    // --- vignette -------------------------------------------------------
    float2 q = uv - 0.5;
    float vig = smoothstep(0.95, 0.35, length(q * float2(uAspect, 1.0)));
    col *= lerp(0.65, 1.0, vig);

    return float4(col, 1.0);
}
