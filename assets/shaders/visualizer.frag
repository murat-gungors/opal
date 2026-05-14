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
    f = f * f * (3.0 - 2.0 * f);
    return mix (mix (hash (i),                    hash (i + vec2 (1.0, 0.0)), f.x),
                mix (hash (i + vec2 (0.0, 1.0)),  hash (i + vec2 (1.0, 1.0)), f.x), f.y);
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

vec2 mirrorFold (vec2 uv)
{
    return vec2 (0.5 - abs (uv.x - 0.5), uv.y);
}

mat2 rot (float a)
{
    float c = cos (a), s = sin (a);
    return mat2 (c, -s, s, c);
}

// =====================================================================
// Main
// =====================================================================

void main()
{
    // ---- Feedback prev frame ---------------------------------------------
    const float feedbackDecay = 0.88;
    vec3 prev = texture (uPrevFrame, vUv).rgb * feedbackDecay;

    // ---- Symmetry + base coords ------------------------------------------
    vec2  uvSym = mirrorFold (vUv);
    vec2  p     = uvSym * 2.0 - 1.0;
    float aspect = uResolution.x / max (uResolution.y, 1.0);
    p.x *= aspect;

    // ---- Domain warp (mid-driven) ----------------------------------------
    float warpAmp = 0.04 + uMid * 0.08;
    vec2  warp    = vec2 (sin (p.y * 2.6 + uTime * 0.42),
                          cos (p.x * 2.6 + uTime * 0.36)) * warpAmp;
    vec2  pw      = p + warp;

    // ---- Slow evolving composition phases --------------------------------
    // Four oscillators at deliberately irrational-ish frequencies so the
    // composition almost never repeats. Each phase is a 0..1 control.
    float phaseDisperse = 0.5 + 0.5 * sin (uTime * 0.071);
    float phaseBend     = 0.5 + 0.5 * sin (uTime * 0.053 + 1.7);
    float phaseMerge    = 0.5 + 0.5 * sin (uTime * 0.091 + 3.2);
    float phaseDeform   = 0.5 + 0.5 * sin (uTime * 0.113 + 0.8);

    // ---- Bend: slow shear/rotation of the field, modulated by mid --------
    float bendAngle = mix (-0.30, 0.30, phaseBend) * (0.35 + uMid * 0.65);
    vec2  pwb       = rot (bendAngle) * pw;

    // ---- Three SDF forms placed inside the mirrored half-space -----------
    // Mirror fold maps vUv to the left half, so all centres need x <= 0 to
    // remain visible. After the mirror their reflections appear on the
    // right, giving an extra two "ghosts" for free — 5 visual forms total.
    float baseR = 0.35 + uBass * 0.20;

    // Disperse phase pushes the flanking forms outward over a 90 s cycle.
    float disp = mix (0.05, 0.55, phaseDisperse);

    vec2 c1 = vec2 (0.0, 0.0)
            + vec2 (sin (uTime * 0.13) * 0.04, cos (uTime * 0.11) * 0.05);

    vec2 c2 = vec2 (-disp * 0.7, -disp * 0.35)
            + vec2 (cos (uTime * 0.17 + 1.2) * 0.07, sin (uTime * 0.15) * 0.05);

    vec2 c3 = vec2 (-disp * 0.4, disp * 0.55)
            + vec2 (sin (uTime * 0.19 + 2.7) * 0.06, cos (uTime * 0.16 + 0.4) * 0.07);

    // Per-form sizes breathe asynchronously.
    float r1 = baseR              * (0.9 + 0.15 * sin (uTime * 0.21));
    float r2 = baseR * 0.65       * (0.8 + 0.20 * sin (uTime * 0.27 + 1.1));
    float r3 = baseR * 0.50       * (0.7 + 0.25 * sin (uTime * 0.31 + 2.3));

    float d1 = sdCircle (pwb - c1, r1);
    float d2 = sdCircle (pwb - c2, r2);
    float d3 = sdCircle (pwb - c3, r3);

    // ---- Merge: smin smoothness oscillates ------------------------------
    // Low k → forms read distinct. High k → forms melt into a single mass.
    float mergeK = mix (0.10, 0.55, phaseMerge);
    float d      = smin (smin (d1, d2, mergeK), d3, mergeK);

    // ---- Deform: low-freq FBM displacement on the merged distance -------
    // Onset bumps deform amount so transients literally rumple the silhouette.
    float deformAmp = mix (0.015, 0.060, phaseDeform) + uOnsetPulse * 0.040;
    float deform    = (fbm (pwb * 2.5 + uTime * 0.20) - 0.5) * deformAmp;
    d += deform;

    // ---- Atmospheric haze (FBM, slow drift) ------------------------------
    vec2  hazeUv = uvSym * 2.2 + vec2 (uTime * 0.025, uTime * 0.018);
    float haze   = smoothstep (0.25, 0.85, fbm (hazeUv));

    // ---- Greyscale background gradient ----------------------------------
    float verticalShade = mix (0.06, 0.28, vUv.y);
    float vignette      = 1.0 - smoothstep (0.7, 1.4, length (p));
    float bg            = verticalShade * vignette;
    bg += haze * (0.18 + uMid * 0.22);

    // ---- Inside-form shading --------------------------------------------
    float formFill = mix (0.40, 0.80, smoothstep (-0.15, 0.55, vUv.y))
                   + uBass * 0.10
                   + uOnsetPulse * 0.18;

    // ---- Topographic contour lines on the form --------------------------
    float contour = 0.0;
    if (d < 0.0)
    {
        // Contour spacing also evolves slowly — dense ↔ open over time.
        float spacing = mix (0.020, 0.040, 0.5 + 0.5 * sin (uTime * 0.040));
        float dx      = abs (fract (d / spacing - 0.5) - 0.5) * spacing;
        float lineW   = fwidth (d) * 1.4;
        contour       = 1.0 - smoothstep (0.0, lineW, dx);
    }
    formFill += contour * 0.30;

    // ---- Vertical drip streaks (only on/near form, top half emphasis) ---
    float drip = 0.0;
    if (d < 0.04)
    {
        float colF  = vUv.x * 36.0;
        float colI  = floor (colF);
        float colH  = hash (vec2 (colI, 17.0));
        if (colH > 0.32)
        {
            float dripTop  = 0.55 + colH * 0.30;
            float falloff  = exp (-max (0.0, dripTop - vUv.y) * 4.0);
            float upper    = step (vUv.y, dripTop);
            float cCentre  = abs (fract (colF) - 0.5);
            float cWidth   = smoothstep (0.30, 0.10, cCentre);
            drip = falloff * upper * cWidth * (colH - 0.32) * 1.4;
        }
    }
    formFill -= drip * 0.32;
    formFill  = clamp (formFill, 0.0, 1.0);

    // ---- Composite form onto background ---------------------------------
    float formMask = 1.0 - smoothstep (-fwidth (d), fwidth (d), d);
    vec3  scene    = mix (vec3 (bg), vec3 (formFill), formMask);

    // ---- Soft halo around the form (bass-modulated bloom) ---------------
    float haloWidth = 0.16 + uBass * 0.28;
    float halo      = exp (-max (d, 0.0) * (8.0 / haloWidth));
    scene += vec3 (halo * (0.10 + uBass * 0.22));

    // ---- Sparse ember particles (with subtle warm tint) -----------------
    {
        vec2  q    = uvSym * vec2 (90.0, 60.0);
        vec2  cell = floor (q);
        vec2  sub  = fract (q) - 0.5;
        float h    = hash (cell + 23.0);
        if (h > 0.985)
        {
            float dEmb  = length (sub);
            float pulse = 0.55 + 0.45 * sin (uTime * 4.0 + h * 60.0);
            float emb   = smoothstep (0.18, 0.0, dEmb) * pulse;
            vec3  emberColour = mix (vec3 (1.0), vec3 (1.0, 0.55, 0.22), 0.35);
            scene += emberColour * emb * (0.45 + uHigh * 0.65);
        }
    }

    // ---- Feedback (additive — tonemap absorbs overshoot) -----------------
    scene += prev;

    // ---- Onset / beat-phase amplitude tweaks -----------------------------
    scene *= 1.0 + uOnsetPulse * 0.25;
    scene *= 1.0 + 0.03 * sin (uBeatPhase * 6.2831853) * uRms;

    // ---- Reinhard tonemap ------------------------------------------------
    scene = scene / (1.0 + scene);

    // ---- DPI-aware hash grain (top layer, after tonemap) ----------------
    vec2  grainP   = vUv * uResolution.xy * 0.5 * uDpiScale + uTime * 50.0;
    float n        = hash (grainP) * 2.0 - 1.0;
    float grainAmp = 0.020 + uHigh * 0.045;
    scene += vec3 (n) * grainAmp;

    fragColor = vec4 (scene, 1.0);
}
