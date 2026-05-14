# Opal — Visual Direction Notes

> Companion to [`OPAL_TECHNICAL_SPEC.md`](OPAL_TECHNICAL_SPEC.md). The spec's §6 (Visual Design Language) is frozen; this document captures *live* art-direction notes that emerged after the spec was sealed, plus quality targets for shader implementation in Stage 4.

## Aesthetic range — two poles, one shader

The MVP ships **one fragment shader** (spec §4.1). It should be flexible enough via knob configuration to span between two distinct aesthetic poles:

### Pole A — "Soft Eno gradient" (spec §6.1 stated DNA)
- Multi-stop color gradient, low-frequency, slowly rotating hue
- Large SDF silhouette form
- Smooth domain warp at low amplitude
- Light hash-grain dither
- Long feedback decay (0.95+)
- References: Brian Eno *77 Million Paintings*, Teenage Engineering OP-1, Risograph posters, Manuel Rossner sculpture surfaces

### Pole B — "Violent particle storm" (reference video, 2026-05-14)
- Pure monochrome, B&W, very high contrast
- Micro-particle / granular texture — thousands of fine sparks/embers
- Soft central glow with center-out radial flow
- Mirror symmetry (top-bottom or radial)
- Aggressive, short-decay feedback creating ribbon-like wisps
- References: cinder/ember photography, [reference screen recording — not committed]

### Pole C — "Cinematic monolith" (reference video set, 2026-05-14)
- Pure monochrome with rare ember-orange accents (smoke + sparks pass only)
- Dominant single silhouette occupying most of the frame
- Surface character: topographic contour lines, vertical drip streaks, brutalist mass
- Atmospheric haze / smoke (low-frequency value noise, very slow drift)
- Symmetric or mass-dominant composition (neoclassical / brutalist / topographic registers)
- References: 5 Higgsfield AI-generated videos (hf_20260506_*.mp4, not committed) showing topographic mountains, classical ruins explosion, brutalist tower, neoclassical pantheon

Pole A is `DRIVE`, `BASS`, `WARP` low + `TRAIL` high + saturation high. Pole B is `DRIVE`, `GRAIN`, `POP` high + `WARP` high + `TEMP` at the desaturated end. Pole C is `DRIVE`, `BASS`, `TRAIL` high + `TEMP` cold/desaturated + `WARP` low + `GRAIN` moderate — a still, massive register dominated by silhouette and atmosphere rather than motion. The shader interpolates continuously between all three.

## Shader layer additions to spec §6.2

Spec §6.2 stack: gradient → SDF → warp → feedback → onset pulse → grain. **Add four more layers** to cover Pole B and Pole C:

7. **Particle field** (Pole B) — high-frequency Worley/value noise sampled with multi-octave FBM, sharpened via `smoothstep(threshold, threshold+ε, n)` to produce particle-like dots. Flow advected by curl-noise or radial vector field driven by audio onset. Mixed over the gradient layer.

8. **Symmetry fold** (optional, knob-gated) — `uv.y = abs(uv.y - 0.5) + 0.5` for horizontal mirror; `uv = abs(uv - 0.5) + 0.5` for quadrant kaleidoscope; polar coord wrap for radial kaleidoscope. Implementation cost is one uniform branch and a few coordinate ops.

9. **Multi-threshold contour lines** (Pole C) — sample SDF at evenly spaced thresholds, draw thin AA-stroked isolines on top of the form. Produces the topographic / "wireframe over mass" register seen in the Pole C mountain video. Line spacing and width modulated by bass.

10. **FBM atmospheric haze** (Pole C) — 3-4 octave value noise sampled at low frequency, very slow temporal drift, composited as semi-transparent overlay. Provides volumetric / smoke depth without true 3D. Density modulated by mid-band energy or RMS.

Order in the pipeline: feedback prev frame (decay) → warp → gradient base → FBM haze → SDF form (composited) → contour lines on form → particle field (Pole B) → symmetry fold → onset pulse → tonemap → grain (DPI-aware).

## Quality targets (Stage 4+)

- **Resolution-independent** — all spatial parameters expressed in normalized UV or screen-aspect-corrected units; uniform `vec2 uResolution` available to fragment shader. Look identical at 720p, 1080p, 4K.
- **60 fps locked** — VSync on, render budget < 16.6 ms even at 4K. Particle-field octaves must scale with resolution if needed.
- **sRGB framebuffer** — explicit `GL_FRAMEBUFFER_SRGB` enable; do gradient math in linear space, write sRGB. Avoids the muddy-look bug flagged in spec §10.3.
- **Anti-aliased SDF edges** — `smoothstep(-fwidth(d), fwidth(d), d)` not hard threshold. Critical for the large silhouette form not looking pixelated.
- **Tonemap** — Reinhard or simple ACES on final output so bright highlights from feedback + onset pulse don't clip to flat white. Preserves the "glow" quality visible in Pole B reference.
- **DPI-aware grain** — grain frequency multiplied by display scale factor (Retina = 2.0) so the texture reads the same physically on Retina vs. external 1× monitors.
- **Frame-to-frame stability** — the feedback FBO must use linear filtering and the same internal format as the main draw, otherwise feedback accumulates color banding and drifts.

## Audio→visual mapping doctrine (extends spec §6.3)

Spec §6.3's rule — "hit 2-3 parameters hard, keep the rest still" — holds. With the two-pole range:

**Pole A defaults:**
- Bass → SDF scale (form breathes)
- Onset → brightness pump
- Mid → hue rotation (slow, continuous)
- High → grain mix

**Pole B defaults:**
- Bass → particle radial velocity (outward push)
- Onset → central glow flash + symmetry fold rotation
- Mid → feedback decay rate (longer = more wisps)
- High → particle density / threshold

Both share Drive as master multiplier, Trail as feedback decay, Pop as onset strength. The knob mapping does not change between poles; what changes is the underlying shader response curve which the `TEMP` knob effectively tilts between.

## Not in scope for v0.1

- Multiple shader presets / "shader bank" (spec §4.2 ❌). One shader, knob-configurable.
- User-loadable shaders / live shader editor (spec §4.2 ❌).
- 3D / camera animation (out of single-fragment-shader scope).
- True particle systems with GPU compute (would require vertex shader + transform feedback or compute shader — overkill for MVP).

## What this document is *not*

This is not a re-spec. It captures aesthetic context Murat shared after the technical spec was frozen, plus quality targets I want to hold myself to during Stage 4 implementation. The technical spec (formats, build system, threading model, audio analysis) is authoritative.
