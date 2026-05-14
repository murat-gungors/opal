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

// 4-stop palette — deep indigo, cool purple, warm magenta, pale gold.
// Stage 5 will let the TEMP knob swing between cool and warm palettes.
const vec3 PALETTE[4] = vec3[](
    vec3 (0.02, 0.03, 0.10),
    vec3 (0.18, 0.14, 0.38),
    vec3 (0.78, 0.32, 0.55),
    vec3 (1.00, 0.85, 0.65)
);

vec3 samplePalette (float t)
{
    t = clamp (t, 0.0, 1.0) * 3.0;
    int   i = int (floor (t));
    float f = fract (t);
    if (i >= 3) return PALETTE[3];
    return mix (PALETTE[i], PALETTE[i + 1], smoothstep (0.0, 1.0, f));
}

// Signed distance to a circle of radius r centred at the origin.
float sdCircle (vec2 p, float r) { return length (p) - r; }

// Polynomial smooth-min (k controls smoothness of the blend).
float smin (float a, float b, float k)
{
    float h = clamp (0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix (b, a, h) - k * h * (1.0 - h);
}

// Cheap IQ hash — returns a 0..1 pseudo-random scalar per input vector.
float hash (vec2 p)
{
    return fract (sin (dot (p, vec2 (127.1, 311.7))) * 43758.5453);
}

void main()
{
    vec2 p = vUv * 2.0 - 1.0;
    float aspect = uResolution.x / max (uResolution.y, 1.0);
    p.x *= aspect;

    // -------- Domain warp ---------------------------------------------------
    // sin(p.yx * freq + time) deforms the coordinate field before any SDF or
    // gradient lookup, so background and form both flow with one motion.
    // Mid-band energy scales the amplitude — visuals literally swim with the
    // mid content of the track. Baseline amp keeps a subtle drift even at
    // silence so the picture never feels frozen.
    float warpAmp  = 0.04 + uMid * 0.08;
    float warpFreq = 3.2;
    vec2  warp = vec2 (sin (p.y * warpFreq + uTime * 0.55),
                       cos (p.x * warpFreq + uTime * 0.42)) * warpAmp;
    vec2  pw = p + warp;
    float r  = length (pw);

    // -------- Background ----------------------------------------------------
    float vertical = vUv.y;
    float vignette = 1.0 - smoothstep (0.5, 1.5, r);
    float bgT      = mix (0.04, 0.40, vertical) * vignette;
    vec3  col      = samplePalette (bgT);

    // -------- SDF form ------------------------------------------------------
    float baseR  = 0.30 + uBass * 0.25;
    vec2  drift  = vec2 (sin (uTime * 0.32) * 0.16,
                         cos (uTime * 0.27) * 0.10) * (0.5 + uBass * 0.6);
    float d1 = sdCircle (pw,         baseR);
    float d2 = sdCircle (pw - drift, baseR * 0.55);
    float d  = smin (d1, d2, 0.30);

    float formT      = 0.55 + uBass * 0.35 + uOnsetPulse * 0.15;
    vec3  formColour = samplePalette (formT);

    float haloWidth = 0.10 + uBass * 0.30;
    float halo      = exp (-max (d, 0.0) * (8.0 / haloWidth));
    col += formColour * halo * (0.20 + uBass * 0.45);

    float aa = 1.0 - smoothstep (-fwidth (d), fwidth (d), d);
    col = mix (col, formColour, aa);

    // -------- Onset / drift / beat phase ------------------------------------
    col *= 1.0 + uOnsetPulse * 0.45;

    col += 0.010 * vec3 (sin (uTime * 0.6),
                         cos (uTime * 0.5),
                         sin (uTime * 0.7 + 1.5));

    col *= 1.0 + 0.04 * sin (uBeatPhase * 6.2831853) * uRms;

    // -------- Tonemap -------------------------------------------------------
    col = col / (1.0 + col);

    // -------- Hash-grain dither (top layer) ---------------------------------
    // Per-pixel pseudo-random noise sits AFTER tonemap so any banding in the
    // compressed output gets dithered out. The * 0.5 makes grain cells span
    // ~2 logical pixels — visible without looking like static. + uTime * 60
    // re-rolls each frame. High-band energy boosts amplitude so brittle /
    // sibilant content gets more texture. Phase 4.5 will make this DPI-aware
    // (Retina ×2) once we control the framebuffer scale.
    float n = hash (vUv * uResolution.xy * 0.5 + uTime * 60.0) * 2.0 - 1.0;
    float grainAmp = 0.02 + uHigh * 0.06;
    col += vec3 (n) * grainAmp;

    fragColor = vec4 (col, 1.0);
}
