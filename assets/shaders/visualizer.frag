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

void main()
{
    vec2 p = vUv * 2.0 - 1.0;
    float aspect = uResolution.x / max (uResolution.y, 1.0);
    p.x *= aspect;

    float r = length (p);

    // Background: vertical gradient × radial vignette so corners drop dark
    // and the centre reads as the warmer part of the palette.
    float vertical = vUv.y;
    float vignette = 1.0 - smoothstep (0.5, 1.5, r);
    float bgT      = mix (0.04, 0.40, vertical) * vignette;
    vec3  col      = samplePalette (bgT);

    // Primary SDF form: main blob plus a smaller companion smin'd in, slowly
    // drifting under the influence of bass and time. Bass also grows the
    // base radius — the form breathes with the kick drum.
    float baseR  = 0.30 + uBass * 0.25;
    vec2  drift  = vec2 (sin (uTime * 0.32) * 0.16,
                         cos (uTime * 0.27) * 0.10) * (0.5 + uBass * 0.6);
    float d1 = sdCircle (p,         baseR);
    float d2 = sdCircle (p - drift, baseR * 0.55);
    float d  = smin (d1, d2, 0.30);

    // Form colour reaches into the warmer end of the palette as bass /
    // onset increase, so transients literally brighten the blob.
    float formT = 0.55 + uBass * 0.35 + uOnsetPulse * 0.15;
    vec3  formColour = samplePalette (formT);

    // Soft halo outside the form — exponential falloff with bass-controlled
    // width. Reads as photographic bloom around the silhouette.
    float haloWidth = 0.10 + uBass * 0.30;
    float halo      = exp (-max (d, 0.0) * (8.0 / haloWidth));
    col += formColour * halo * (0.20 + uBass * 0.45);

    // Anti-aliased fill using screen-space derivatives — 1 px transition
    // around the SDF zero crossing, so the edge stays crisp at any DPI.
    float aa = 1.0 - smoothstep (-fwidth (d), fwidth (d), d);
    col = mix (col, formColour, aa);

    // Onset → brief overall brightness pump.
    col *= 1.0 + uOnsetPulse * 0.45;

    // Time drift so silence never feels dead.
    col += 0.010 * vec3 (sin (uTime * 0.6),
                         cos (uTime * 0.5),
                         sin (uTime * 0.7 + 1.5));

    // Beat phase subtly modulates amplitude in proportion to current RMS.
    col *= 1.0 + 0.04 * sin (uBeatPhase * 6.2831853) * uRms;

    // Reinhard tonemap keeps highlights in [0,1).
    col = col / (1.0 + col);

    fragColor = vec4 (col, 1.0);
}
