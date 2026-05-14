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

void main()
{
    vec2 p = vUv * 2.0 - 1.0;
    float aspect = uResolution.x / max (uResolution.y, 1.0);
    p.x *= aspect;

    float r = length (p);

    // 1. Radial base — dark indigo fading to a slightly warmer mid-tone
    vec3 col = mix (vec3 (0.04, 0.05, 0.09),
                    vec3 (0.10, 0.12, 0.18),
                    1.0 - smoothstep (0.0, 1.5, r));

    // 2. Bass-driven central glow (Gaussian)
    float bassRadius = 0.30 + uBass * 0.45;
    float glow = exp (-(r * r) / (bassRadius * bassRadius));
    col += vec3 (0.40, 0.55, 0.90) * glow * (0.25 + uBass * 0.75);

    // 3. High-driven sparkle — cheap pseudo-hash
    float sparkle = sin (vUv.x * 137.0) * sin (vUv.y * 113.0);
    sparkle = pow (max (0.0, sparkle), 8.0);
    col += vec3 (0.90, 0.95, 1.00) * sparkle * uHigh * 0.4;

    // 4. Mid → cheap hue twist via channel swap mix
    col = mix (col, col.bgr, uMid * 0.30);

    // 5. Onset → brief overall brightness pump
    col *= 1.0 + uOnsetPulse * 0.55;

    // 6. Time drift so silence isn't dead
    col += 0.012 * vec3 (sin (uTime * 0.6),
                         cos (uTime * 0.5),
                         sin (uTime * 0.7 + 1.5));

    // 7. Beat phase subtly modulates output amplitude
    col *= 1.0 + 0.04 * sin (uBeatPhase * 6.2831853) * uRms;

    // Reinhard tonemap
    col = col / (1.0 + col);

    fragColor = vec4 (col, 1.0);
}
