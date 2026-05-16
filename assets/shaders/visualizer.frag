#version 410 core
in  vec2 vUv;
out vec4 fragColor;

uniform vec2  uResolution;
uniform float uTime;
uniform float uBass,     uMid,     uHigh;
uniform float uBassPeak, uMidPeak, uHighPeak;
uniform float uBassAvg,  uMidAvg,  uHighAvg;
uniform float uRms;
uniform float uOnsetPulse;
uniform float uBeatPhase;
uniform float uDpiScale;
uniform sampler2D uPrevFrame;

// ---- 8 knob uniforms (0..1, default 0.5) -----------------------------------
uniform float uDrive;   // master gain on all audio-driven amplitudes
uniform float uBassK;   // bass amount: form scale / halo width
uniform float uHue;     // very subtle warm/cool tint at output (monochrome shader)
uniform float uGrain;   // grain amplitude scale
uniform float uWarp;    // domain warp + bend angle scale
uniform float uTrail;   // feedback decay   → mix(0.65, 0.99, uTrail)
uniform float uPop;     // onset spatial effect strength
uniform float uTemp;    // ember warm-orange ↔ pure white tint balance

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

vec2 curlNoise (vec2 p)
{
    const float e = 0.012;
    float n1 = valueNoise (p + vec2 (0.0,  e));
    float n2 = valueNoise (p - vec2 (0.0,  e));
    float n3 = valueNoise (p + vec2 ( e, 0.0));
    float n4 = valueNoise (p - vec2 ( e, 0.0));
    return vec2 (n1 - n2, -(n3 - n4)) / (2.0 * e);
}

float sdCircle (vec2 p, float r) { return length (p) - r; }

float sdBox (vec2 p, vec2 b)
{
    vec2 d = abs (p) - b;
    return length (max (d, 0.0)) + min (max (d.x, d.y), 0.0);
}

float smin (float a, float b, float k)
{
    float h = clamp (0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix (b, a, h) - k * h * (1.0 - h);
}

vec2 mirrorFold (vec2 uv) { return vec2 (0.5 - abs (uv.x - 0.5), uv.y); }

mat2 rot (float a) { float c = cos (a), s = sin (a); return mat2 (c, -s, s, c); }

// =====================================================================
// Main
// =====================================================================

void main()
{
    // ---- Drive-scaled audio values ----------------------------------------
    // uDrive 0..1 acts as a master gain — 0 silences all audio response, 0.5
    // is the calibrated default, 1 doubles every reaction. The shader uses
    // these scaled values everywhere it would otherwise sample the raw bus.
    const float driveScale = 2.0;   // knob max → 2× default response
    float drv      = uDrive * driveScale;
    float bassD    = uBass     * drv;
    float midD     = uMid      * drv;
    float highD    = uHigh     * drv;
    float bassPkD  = uBassPeak * drv;
    float midPkD   = uMidPeak  * drv;
    float bassAvgD = uBassAvg  * drv;
    float midAvgD  = uMidAvg   * drv;

    // ---- Curl-advected feedback ------------------------------------------
    float feedbackDecay = mix (0.65, 0.99, uTrail);
    vec2  flow = curlNoise (vUv * 3.0 + uTime * 0.13) * 0.0028;
    vec3  prev = texture (uPrevFrame, vUv - flow).rgb * feedbackDecay;

    // ---- Mirror + warped coords ------------------------------------------
    vec2  uvSym  = mirrorFold (vUv);
    vec2  p      = uvSym * 2.0 - 1.0;
    float aspect = uResolution.x / max (uResolution.y, 1.0);
    p.x *= aspect;

    float warpScale = uWarp * 2.0;
    float warpAmp   = (0.035 + midD * 0.07) * warpScale;
    vec2  warp      = vec2 (sin (p.y * 2.6 + uTime * 0.42),
                            cos (p.x * 2.6 + uTime * 0.36)) * warpAmp;
    vec2  pw        = p + warp;

    // ---- Composition phases ----------------------------------------------
    float phaseDisperse = 0.5 + 0.5 * sin (uTime * 0.071);
    float phaseBend     = 0.5 + 0.5 * sin (uTime * 0.053 + 1.7);
    float phaseMerge    = 0.5 + 0.5 * sin (uTime * 0.091 + 3.2);
    float phaseDeform   = 0.5 + 0.5 * sin (uTime * 0.113 + 0.8);

    float bendAngle = mix (-0.28, 0.28, phaseBend) * (0.30 + midD * 0.60) * warpScale;
    vec2  pwb       = rot (bendAngle) * pw;

    // ---- Five SDF forms ---------------------------------------------------
    float disp    = mix (0.10, 0.55, phaseDisperse);
    float bassToR = uBassK * 2.0;                       // BASS knob amount
    float baseR   = 0.32 + bassD * 0.15 * bassToR;

    vec2 c1 = vec2 (0.0, 0.0)
            + vec2 (sin (uTime * 0.13) * 0.04, cos (uTime * 0.11) * 0.04);
    vec2 c2 = vec2 (-disp * 0.70, -disp * 0.35)
            + vec2 (cos (uTime * 0.17 + 1.2) * 0.06, sin (uTime * 0.15) * 0.05);
    vec2 c3 = vec2 (-disp * 0.40,  disp * 0.55)
            + vec2 (sin (uTime * 0.19 + 2.7) * 0.05, cos (uTime * 0.16 + 0.4) * 0.06);
    vec2 c4 = vec2 (-disp * 0.90,  disp * 0.20)
            + vec2 (sin (uTime * 0.22) * 0.04, cos (uTime * 0.13 + 1.7) * 0.05);
    vec2 c5 = vec2 (-disp * 0.55, -disp * 0.65)
            + vec2 (cos (uTime * 0.18 + 0.9) * 0.04, sin (uTime * 0.14 + 2.2) * 0.04);

    float r1 = baseR              * (0.95 + 0.10 * sin (uTime * 0.21));
    float r2 = baseR * 0.62       * (0.85 + 0.15 * sin (uTime * 0.27 + 1.1));
    float r3 = baseR * 0.55       * (0.85 + 0.15 * sin (uTime * 0.31 + 2.3));
    float r4 = baseR * 0.35       * (0.80 + 0.20 * sin (uTime * 0.35 + 0.5));
    float r5 = baseR * 0.40       * (0.80 + 0.20 * sin (uTime * 0.29 + 1.7));

    float d1 = sdCircle (pwb - c1, r1);
    float d2 = sdCircle (pwb - c2, r2);
    float d3 = sdCircle (pwb - c3, r3);
    float d4 = sdCircle (pwb - c4, r4);
    float d5 = sdCircle (pwb - c5, r5);

    float mergeK = mix (0.06, 0.18, phaseMerge);
    float d      = smin (smin (smin (smin (d1, d2, mergeK), d3, mergeK), d4, mergeK), d5, mergeK);

    float popScale  = uPop * 2.0;
    float deformAmp = mix (0.012, 0.035, phaseDeform) + uOnsetPulse * 0.020 * popScale;
    d += (fbm (pwb * 2.5 + uTime * 0.20) - 0.5) * deformAmp;

    // ---- Architectural vertical bars -------------------------------------
    float columns = 0.0;
    {
        vec2  cp     = pwb * 1.2;
        float period = 0.40;
        float cellX  = mod (cp.x + period, 2.0 * period) - period;
        float colD   = sdBox (vec2 (cellX, cp.y - 0.20), vec2 (0.015, 0.55));
        float colMask = 1.0 - smoothstep (-fwidth (colD), fwidth (colD), colD);
        columns = colMask * smoothstep (0.18, 0.55, midAvgD) * 0.16;
    }

    // ---- Atmospheric haze ------------------------------------------------
    vec2  hazeUv = uvSym * 2.0 + vec2 (uTime * 0.022, uTime * 0.017);
    float haze   = smoothstep (0.25, 0.85, fbm (hazeUv));

    // ---- Background ------------------------------------------------------
    float vertical = mix (0.05, 0.22, vUv.y);
    float vignette = 1.0 - smoothstep (0.6, 1.4, length (p));
    float bg       = vertical * vignette;
    bg += haze * (0.13 + midAvgD * 0.25);
    bg += columns;

    // ---- Form fill (texture + contour + drips) ---------------------------
    float surface  = fbm (pwb * 6.0 + uTime * 0.04);
    float formFill = mix (0.18, 0.42, smoothstep (-0.20, 0.55, vUv.y))
                   + bassD * 0.05 * bassToR
                   + uOnsetPulse * 0.05 * popScale;
    formFill *= 0.80 + 0.20 * surface;

    float contour = 0.0;
    if (d < 0.0)
    {
        float spacing = mix (0.020, 0.040, 0.5 + 0.5 * sin (uTime * 0.040));
        float dx      = abs (fract (d / spacing - 0.5) - 0.5) * spacing;
        float lineW   = fwidth (d) * 1.4;
        contour       = 1.0 - smoothstep (0.0, lineW, dx);
    }
    formFill += contour * 0.22;

    float drip = 0.0;
    if (d < 0.04)
    {
        float colF = vUv.x * 36.0;
        float colI = floor (colF);
        float colH = hash (vec2 (colI, 17.0));
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
    formFill -= drip * 0.30;
    formFill  = clamp (formFill, 0.0, 1.0);

    // ---- Composite form --------------------------------------------------
    float formMask = 1.0 - smoothstep (-fwidth (d), fwidth (d), d);
    vec3  scene    = mix (vec3 (bg), vec3 (formFill), formMask);

    // ---- Halo ------------------------------------------------------------
    float haloWidth = 0.08 + bassD * 0.10 * bassToR;
    float halo      = exp (-max (d, 0.0) * (8.0 / haloWidth));
    scene += vec3 (halo * (0.05 + bassD * 0.08 * bassToR + bassPkD * 0.10 * popScale));

    // ---- Background star field (asymmetric) -----------------------------
    {
        vec2  starP = vUv * vec2 (140.0, 100.0);
        vec2  cell  = floor (starP);
        vec2  sub   = fract (starP) - 0.5;
        float h     = hash (cell + 31.0);
        if (h > 0.992)
        {
            float dStar = length (sub);
            float pulse = 0.5 + 0.5 * sin (uTime * 3.0 + h * 80.0);
            scene += vec3 (smoothstep (0.20, 0.0, dStar) * pulse * 0.30);
        }
    }

    // ---- Ember field (asymmetric, density via midPeak) -------------------
    {
        vec2  q     = vUv * vec2 (90.0, 60.0);
        vec2  cell  = floor (q);
        vec2  sub   = fract (q) - 0.5;
        float h     = hash (cell + 23.0);
        float thr   = mix (0.988, 0.972, midPkD * popScale);
        if (h > thr)
        {
            float dEmb  = length (sub);
            float pulse = 0.55 + 0.45 * sin (uTime * 4.0 + h * 60.0);
            float emb   = smoothstep (0.18, 0.0, dEmb) * pulse;
            // TEMP knob: 0 → pure-white embers (cold); 1 → strongly orange-warm.
            vec3  emberC = mix (vec3 (1.0), vec3 (1.0, 0.55, 0.22), uTemp * 0.6);
            scene += emberC * emb * (0.22 + highD * 0.35);
        }
    }

    // ---- Feedback ---------------------------------------------------------
    scene += prev;

    // ---- Beat-phase × RMS micro modulation -------------------------------
    scene *= 1.0 + 0.02 * sin (uBeatPhase * 6.2831853) * uRms;

    // ---- Reinhard tonemap ------------------------------------------------
    scene = scene / (1.0 + scene);

    // ---- HUE knob: subtle warm/cool tint (monochrome shader) -------------
    // uHue 0 → cool blue cast; 0.5 → neutral; 1 → warm sepia cast.
    float warmth  = (uHue - 0.5) * 2.0;  // -1..+1
    vec3  hueTint = vec3 (1.0 + 0.08 * warmth,
                          1.0,
                          1.0 - 0.06 * warmth);
    scene *= hueTint;

    // ---- DPI-aware grain (GRAIN knob scales amplitude) -------------------
    float grainScale = uGrain * 2.0;
    vec2  grainP   = vUv * uResolution.xy * 0.5 * uDpiScale + uTime * 50.0;
    float n        = hash (grainP) * 2.0 - 1.0;
    float grainAmp = (0.018 + highD * 0.035) * grainScale;
    scene += vec3 (n) * grainAmp;

    fragColor = vec4 (scene, 1.0);
}
