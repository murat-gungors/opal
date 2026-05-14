# CLAUDE.md

Guidance for Claude Code working in this repository.

## What Opal is

Opal is an audio-reactive visual generator. Standalone macOS app + VST3 plugin from the same codebase. Visuals lock to the beat and energy contour of incoming audio. Minimal UI: a full-screen visualizer area + 8 knobs.

The authoritative spec lives at [`docs/OPAL_TECHNICAL_SPEC.md`](docs/OPAL_TECHNICAL_SPEC.md). Read it before making non-trivial decisions.

## Stage-gated development

Implementation proceeds stage-by-stage. **Do not jump ahead.** Each stage must leave a compilable, testable, demonstrable artifact, and prior-stage behavior must remain intact:

- Stage 0 — empty JUCE scaffold (Standalone + VST3 build, load in Ableton)
- Stage 1 — host transport readout (BPM, ppq, isPlaying as debug overlay)
- Stage 2 — audio analysis (FFT, 3-band envelope, RMS, spectral flux onset; debug overlay bars)
- Stage 3 — OpenGL shader pipeline (audio uniforms → simple reactive gradient)
- Stage 4 — visual engine layers (SDF, domain warp, feedback, grain — spec §6.2)
- Stage 5 — 8 knobs wired as `AudioProcessorValueTreeState` parameters + shader uniforms
- Stage 6 — UI polish, performance, pluginval

At the end of each stage: pause, summarize what changed, give the user explicit test instructions, wait for approval before proceeding.

## Real-time safety (audio thread)

In `processBlock` and anything it calls — **no allocation, no locks, no blocking work**:

- No `new`/`delete`, no `std::vector::push_back`, no `std::map::insert`
- No `std::mutex`, no `juce::CriticalSection` (use lock-free primitives)
- No `juce::String` construction (it allocates), no `cout`/`printf` (DBG strips in release but still allocates)
- Pre-allocate in constructor or `prepareToPlay`; reuse buffers thereafter

Audio→render data flow (decided in Stage 1 planning): **one `std::atomic<float>` per analysis metric** (`bassLevel`, `midLevel`, `highLevel`, `rms`, etc.) plus a `std::atomic<uint32_t>` for monotonic onset counter. Single-struct snapshot was rejected because `std::atomic<T>` for `T > 16 bytes` falls back to a compiler-internal lock on most platforms. A frame of visually-imperceptible per-metric mismatch is acceptable.

## Build (macOS, the only target until v0.2)

```sh
cmake -B Builds -G Xcode
cmake --build Builds --config Debug --target Opal_Standalone
cmake --build Builds --config Debug --target Opal_VST3
```

Primary dev loop = **Standalone** (faster than reloading in Ableton on every change). Touch Ableton only at milestone validations (every 1–2 stages).

`COPY_PLUGIN_AFTER_BUILD` is on — the VST3 lands in `~/Library/Audio/Plug-Ins/VST3/Opal.vst3` automatically.

### FileProvider xattr workaround

The project tree lives under a directory managed by a macOS FileProvider (iCloud/Dropbox/OneDrive-style sync). These providers attach `com.apple.FinderInfo` to build artifacts, which makes ad-hoc codesigning fail with `"resource fork, Finder information, or similar detritus not allowed"`. `CMakeLists.txt` strips xattrs from each bundle in a `POST_BUILD` step before Xcode's codesign phase. If codesign starts failing again, this is the first place to look.

## Tests

```sh
cmake --build Builds --config Debug --target Tests
ctest --test-dir Builds --output-on-failure
```

DSP modules (Stage 2+) get Catch2 unit tests. GUI/OpenGL is verified by eye.

## Plugin configuration (CMakeLists.txt — already set)

| Setting | Value |
|---|---|
| `PROJECT_NAME` / `PRODUCT_NAME` | `Opal` |
| `COMPANY_NAME` | `Edition8` |
| `BUNDLE_ID` | `com.edition8.opal` |
| `PLUGIN_MANUFACTURER_CODE` | `Ed8C` |
| `PLUGIN_CODE` | `Opal` |
| `FORMATS` | `Standalone VST3` (no AU/AUv3/CLAP in v0.1) |

## Stack

- JUCE 8 (AGPLv3) — submodule at `JUCE/`
- C++20
- OpenGL 4.1 Core / GLSL 410 (macOS ceiling; Metal in v2+)
- Catch2 (via CPM) for tests
- `melatonin_inspector` for in-editor UI debugging (kept for Stage 4+)

## Project layout

- `source/` — plugin source (`PluginProcessor`, `PluginEditor`, plus modules added per stage)
- `tests/` — Catch2 unit tests
- `benchmarks/` — Catch2 benchmarks
- `cmake/` — CMake helper modules (forked from `sudara/cmake-includes`)
- `modules/` — JUCE-style modules (currently just `melatonin_inspector`)
- `JUCE/` — JUCE framework (submodule)
- `assets/` — embedded via `juce_add_binary_data`
- `docs/` — spec and design notes
- `packaging/` — installer resources

## Code style

`.clang-format` is checked in (Allman braces, 4-space indent, no column limit). Run formatting before commits.

Warnings are errors. Don't let them accumulate. LSP/clangd may report false-positive "undeclared identifier" errors for JUCE module includes — ignore unless the actual compiler fails.

## Lineage

Forked initially from [Pamplejuce](https://github.com/sudara/pamplejuce) (MIT, by Sudara Williams) for the CI/build scaffold. The `cmake/` and `modules/melatonin_inspector/` submodules track upstream.
