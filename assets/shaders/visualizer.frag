#version 410 core
in  vec2 vUv;
out vec4 fragColor;

uniform vec2  uResolution;
uniform float uTime;
uniform float uBass;
uniform float uMid;
uniform float uHigh;
uniform float uRms;
uniform float uOnsetPulse;
uniform float uBeatPhase;
uniform float uDpiScale;
uniform sampler2D uPrevFrame;

// =====================================================================
// Helpers
// =====================================================================

float hash (vec2 p)
{
    return fract (sin (dot (p, vec2 (127.1, 311.7))) * 43758.5453);
}

float valueNoise (vec2 p)
{
    vec2 i = floor (p);
    vec2 f = fract (p);
    f = f * f * (3.0 - 2.0 * f); // smoothstep
    return mix (mix (hash (i),               hash (i + vec2 (1.0, 0.0)), f.x),
                mix (hash (i + vec2 (0.0, 1.0)), hash (i + vec2 (1.0, 1.0)), f.x), f.y);
}

float fbm (vec2 p)
{
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; ++i)
    {
        v += a * valueNoise (p);
        p *= 2.03;
        a *= 0.5;
    }
    return v;
}

float sdCircle (vec2 p, float r) { return length (p) - r; }

float smin (float a, float b, float k)
{
    float h = clamp (0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix (b, a, h) - k * h * (1.0 - h);
}

// Mirror UV around x=0.5 — gives left-right symmetric output.
vec2 mirrorFold (vec2 uv)
{
    return vec2 (0.5 - abs (uv.x - 0.5), uv.y);
}

// =====================================================================
// Main
// =====================================================================

void main()
{
    // ---- 0. Previous frame feedback (sampled in linear-space FBO) ----------
    // Trail knob will modulate this decay in Stage 5; constant for now.
    const float feedbackDecay = 0.88;
    vec3 prev = texture (uPrevFrame, vUv).rgb * feedbackDecay;

    // ---- 1. Symmetry fold + warped coords ---------------------------------
    vec2  uvSym = mirrorFold (vUv);
    vec2  p     = uvSym * 2.0 - 1.0;
    float aspect = uResolution.x / max (uResolution.y, 1.0);
    p.x *= aspect;

    float warpAmp  = 0.03 + uMid * 0.06;
    vec2  warp     = vec2 (sin (p.y * 2.6 + uTime * 0.42),
                           cos (p.x * 2.6 + uTime * 0.36)) * warpAmp;
    vec2  pw       = p + warp;

    // ---- 2. Atmospheric haze (FBM, slow drift) ----------------------------
    vec2  hazeUv = uvSym * 2.2 + vec2 (uTime * 0.025, uTime * 0.018);
    float haze   = fbm (hazeUv);
    haze = smoothstep (0.25, 0.85, haze);

    // ---- 3. Greyscale background gradient (sky-to-ground) -----------------
    float verticalShade = mix (0.06, 0.28, vUv.y);
    float vignette      = 1.0 - smoothstep (0.7, 1.4, length (p));
    float bg            = verticalShade * vignette;
    bg += haze * (0.18 + uMid * 0.22);

    // ---- 4. Dominant SDF monolith (much bigger than Stage 4.3) ------------
    float baseR  = 0.55 + uBass * 0.20;
    vec2  drift  = vec2 (sin (uTime * 0.18) * 0.08,
                         cos (uTime * 0.15) * 0.04);
    float d1 = sdCircle (pw - drift,                          baseR);
    float d2 = sdCircle (pw - drift - vec2 (0.0, -0.20),      baseR * 0.65);
    float d  = smin (d1, d2, 0.40);

    // ---- 5. Inside-form base shading (gradient + bass + onset pump) -------
    float formFill = mix (0.42, 0.78, smoothstep (-0.15, 0.55, vUv.y))
                   + uBass * 0.10
                   + uOnsetPulse * 0.20;

    // ---- 6. Topographic contour lines (v1 reference) ----------------------
    float contour = 0.0;
    if (d < 0.0)
    {
        float spacing  = 0.030;
        float dx       = abs (fract (d / spacing - 0.5) - 0.5) * spacing;
        float lineW    = fwidth (d) * 1.4;
        contour = 1.0 - smoothstep (0.0, lineW, dx);
    }
    formFill += contour * 0.28;

    // ---- 7. Vertical drip streaks (v4 reference) --------------------------
    // Periodic vertical darkening, stronger near the top of the form.
    float drip = 0.0;
    if (d < 0.05)
    {
        float colF  = vUv.x * 36.0;
        float colI  = floor (colF);
        float colH  = hash (vec2 (colI, 17.0));
        if (colH > 0.35)
        {
            float dripTop  = 0.55 + colH * 0.30;
            float falloff  = exp (-max (0.0, dripTop - vUv.y) * 4.0);
            float upper    = step (vUv.y, dripTop);
            float colCentre = abs (fract (colF) - 0.5);
            float colWidth = smoothstep (0.30, 0.10, colCentre);
            drip = falloff * upper * colWidth * (colH - 0.35) * 1.3;
        }
    }
    formFill -= drip * 0.30;

    formFill = clamp (formFill, 0.0, 1.0);

    // ---- 8. Composite form onto background --------------------------------
    float formMask = 1.0 - smoothstep (-fwidth (d), fwidth (d), d);
    vec3  scene    = mix (vec3 (bg), vec3 (formFill), formMask);

    // ---- 9. Soft halo around the form -------------------------------------
    float haloWidth = 0.14 + uBass * 0.28;
    float halo      = exp (-max (d, 0.0) * (8.0 / haloWidth));
    scene += vec3 (halo * (0.10 + uBass * 0.22));

    // ---- 10. Sparse ember particles (v2 reference — bright dots) ---------
    // Mostly bright-white sparks with a hint of warm-orange tint, so the
    // overall image stays monochrome but the embers read as v2-style heat.
    {
        vec2  q       = uvSym * vec2 (90.0, 60.0);
        vec2  cell    = floor (q);
        vec2  sub     = fract (q) - 0.5;
        float h       = hash (cell + 23.0);
        if (h > 0.985)
        {
            float dEmb  = length (sub);
            float pulse = 0.55 + 0.45 * sin (uTime * 4.0 + h * 60.0);
            float emb   = smoothstep (0.18, 0.0, dEmb) * pulse;
            vec3  emberColour = mix (vec3 (1.0),
                                     vec3 (1.0, 0.55, 0.22),
                                     0.35); // mostly white, hint of orange
            scene += emberColour * emb * (0.45 + uHigh * 0.65);
        }
    }

    // ---- 11. Feedback (additive — tonemap absorbs overshoot) --------------
    scene += prev;

    // ---- 12. Onset → overall brightness pump ------------------------------
    scene *= 1.0 + uOnsetPulse * 0.25;

    // ---- 13. Beat phase × RMS subtle modulation ---------------------------
    scene *= 1.0 + 0.03 * sin (uBeatPhase * 6.2831853) * uRms;

    // ---- 14. Reinhard tonemap ---------------------------------------------
    scene = scene / (1.0 + scene);

    // ---- 15. DPI-aware hash grain (top layer, after tonemap) --------------
    // Multiply the lookup by uDpiScale so the perceived grain size is the
    // same number of physical pixels on Retina and external 1× monitors.
    vec2  grainP = vUv * uResolution.xy * 0.5 * uDpiScale + uTime * 50.0;
    float n      = hash (grainP) * 2.0 - 1.0;
    float grainAmp = 0.020 + uHigh * 0.045;
    scene += vec3 (n) * grainAmp;

    fragColor = vec4 (scene, 1.0);
}
