# Opal

Audio-reactive visual generator. Runs as a **VST3** plugin inside Ableton Live (and other VST3 hosts) and as a **macOS standalone** application. Produces modern-minimal generative visuals locked to the beat and energy contour of incoming audio.

> **Status:** v0.1 in development. See [`docs/OPAL_TECHNICAL_SPEC.md`](docs/OPAL_TECHNICAL_SPEC.md) for the full technical spec.

## Build (macOS)

Requirements: Xcode + Command Line Tools, CMake ≥ 3.25, git with submodule support.

```sh
git clone --recurse-submodules <repo-url> Opal
cd Opal
cmake -B Builds -G Xcode
cmake --build Builds --config Debug --target Opal_Standalone
cmake --build Builds --config Debug --target Opal_VST3
```

`COPY_PLUGIN_AFTER_BUILD` is on, so the VST3 lands in `~/Library/Audio/Plug-Ins/VST3/Opal.vst3` automatically.

Run the standalone:

```sh
open Builds/Opal_artefacts/Debug/Standalone/Opal.app
```

## Stack

- **JUCE 8** (AGPLv3 tier) — plugin framework
- **CMake** + **Pamplejuce**-derived project layout — build system & CI scaffold
- **OpenGL 4.1 Core / GLSL 410** — render path (macOS ceiling; Metal in v2+)
- **Catch2** — unit tests for DSP

## License

[AGPLv3](LICENSE). Forked initially from [Pamplejuce](https://github.com/sudara/pamplejuce) (MIT) by Sudara Williams.
