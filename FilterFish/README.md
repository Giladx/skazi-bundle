# The FilterFish — multi-FX creative filter (VST3 / AU / Standalone, Windows + macOS)

An original plugin *inspired by the workflow* of Glitch (step-sequenced stutter FX),
Filterverse (huge library of filter shapes + modulation), Oxford Inflator (loudness/"fatness"
curve) and FabFilter Volcano (big central filter knob, modulation, clean UI).
It does **not** copy their code, names, graphics or presets — keep it that way if you ever sell it.

---

## 1. Feature list (your notes, turned into a spec)

| Your note | Where it lives |
|---|---|
| MIX button DRY/WET for all | Global **DRY/WET** big knob (Master) + a **MIX** knob on every module |
| Big knob for the filter | Huge **CUTOFF** knob in the centre, with a white dot that shows the *modulated* cutoff live |
| Stereo expander | **WIDTH** (0–200 %) mid/side widener in Master |
| Distortion | 6 types: Soft Clip, Hard Clip, Tube, Fold, Bit Crush, Rectify |
| Reverb | Room size, damping, mix |
| Chorus | Rate, depth, mix |
| Flanger | Rate, depth, feedback (±), mix |
| Delay | Sync to host or free ms, feedback, ping-pong, mix (13 sync divisions incl. triplets/dotted) |
| Waveforms | LFO shapes: Sine, Triangle, Saw Up, Saw Down, Square, Sample & Hold, Smooth Random, 16-Step Seq, Bounce, Chaos |
| Filters, classic + crazy | 21 types (below) |
| Glitch | Step stutter: Repeat / Reverse / Gate / Mixed, step 1/32…4 bars, repeats x1–x16, chance % |
| Inflator | Effect amount + Curve (−50…+50) |
| ~800 random presets | **800 factory presets** in 16 themed banks + **RANDOM** button (infinite new ones) |

### Filter types
Classic: LP 12, LP 24, HP 12, HP 24, BP 12, BP 24, Notch, Notch 24, Peak, Ladder LP
Crazy: Comb +, Comb −, Formant (vowel morph A-E-I-O-U on the cutoff knob), Phaser, Bit Crush,
Ring Mod, Wave Fold, Scream, Telephone, Metallic (inharmonic combs), Squelch (driven acid ladder)

### Modulation
Cutoff = knob × 2^(LFO × amount × 4 octaves + envelope × env-amount × 6 octaves).
LFO free-runs (0.01–20 Hz) or syncs to host tempo & bar position. Envelope follower on input.
"PATTERN" picks one of 1000 random 16-step sequences for the Step Seq shape.

## 2. Signal flow

```
IN GAIN ─► DISTORTION ─► FILTER (+LFO/ENV) ─► GLITCH ─► CHORUS ─► FLANGER ─► DELAY ─► REVERB ─► INFLATOR ─► WIDTH ─► DRY/WET ─► OUT GAIN
   └──────────────────────────────── dry copy ─────────────────────────────────────────────────────────────────────┘
```
Every module has its own power switch and MIX. Everything is per-sample with smoothed
controls (no zipper noise), and the engine auto-resets if anything ever blows up (NaN guard).

## 3. Presets
* Program 0 = **Init**. Programs 1–800 = 16 banks × 50:
  Classic Sweep, Acid, Dub Space, Glitch, Lo-Fi, Talk Box, Comb Metal, Phase Shift, Destroy,
  Inflate, Wide Air, Rhythm Gate, Ring Bells, Scream, Tape Echo, Chaos.
* Presets are *generated deterministically* from a seed (`Source/Presets.h`) — each bank has
  musical rules (e.g. Acid = ladder/squelch + high reso + envelope + step LFO), so they're random
  but usable, and preset #237 is always the same sound on every machine.
* Hosts see them as the plugin's program list; the UI has a browser, ◄ ► and RANDOM.
* Your own tweaks are saved with the DAW project.

## 4. What you need to build it

| | Windows | macOS |
|---|---|---|
| Compiler | Visual Studio 2022 (or Build Tools) with "Desktop development with C++" | Xcode 15+ |
| Build system | CMake 3.22+ | CMake 3.22+ (`brew install cmake`) |
| Framework | JUCE 8 (downloaded automatically by CMake) | same |
| Output | `.vst3` + Standalone `.exe` | `.vst3`, `.component` (AU, needed for Logic), Standalone `.app` — Universal (Intel + Apple Silicon) |

### Build — Windows
```
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```
Result: `build/FilterFish_artefacts/Release/VST3/The FilterFish.vst3` → copy to
`C:\Program Files\Common Files\VST3\`.

### Build — macOS
```
cmake -B build -G Xcode
cmake --build build --config Release
```
Copy `The FilterFish.vst3` to `~/Library/Audio/Plug-Ins/VST3/` and `The FilterFish.component`
to `~/Library/Audio/Plug-Ins/Components/`.

### No Mac? Use the included GitHub Actions
Push this folder to a GitHub repo → **Actions** builds Windows and macOS automatically
(`.github/workflows/build.yml`) and gives you downloadable zips.

## 5. Before you sell / share it (checklist)
1. **Rename**: change `COMPANY_NAME`, `PLUGIN_MANUFACTURER_CODE` (4 letters, at least one capital)
   and `PLUGIN_CODE` in `CMakeLists.txt` to your own — they must be unique.
2. **Licences**: JUCE is free under AGPLv3 (you must open-source your plugin) or needs a paid
   JUCE licence for closed-source commercial sales. The VST3 SDK is MIT-licensed since v3.8.
3. **macOS signing**: sign with an Apple Developer ID ($99/yr) and **notarize**
   (`xcrun notarytool`), otherwise Gatekeeper blocks the plugin on other Macs.
4. **Windows signing** (optional but recommended): code-signing certificate to avoid SmartScreen.
5. **Installer**: Inno Setup (Win), `pkgbuild`/Packages (mac).
6. **Test** in pluginval (`https://github.com/Tracktion/pluginval`), Ableton, FL Studio, Logic
   (AU validation: `auval -v aufx Ffsh Aswi`), Cubase, Reaper, Bitwig.

## 6. Code map
```
CMakeLists.txt            project, formats, JUCE download
Source/Parameters.h       every parameter (IDs, ranges, names, choice lists)
Source/DSP/FilterEngine.h 21 filter types
Source/DSP/Lfo.h          10 LFO shapes + host sync
Source/DSP/Glitch.h       stutter / reverse / gate sequencer
Source/DSP/Effects.h      distortion, inflator, stereo delay, width, DC blocker
Source/Presets.h          800 factory presets + randomizer
Source/PluginProcessor.*  audio engine / signal chain / state saving
Source/PluginEditor.*     UI (big knobs, module panels, LFO scope, preset browser)
```

## 7. Stress test
`tests/HostTest.cpp` loads the built VST3 like a DAW: all 801 presets at 4 sample-rate/block-size
setups, random-parameter fuzzing, state save/restore, editor open/paint/close and 3 load/unload rounds.
```
cmake -B build -DFILTERFISH_TESTS=ON
cmake --build build --config Release --target FilterFishTest
build/FilterFishTest_artefacts/Release/FilterFishTest.exe "C:/Program Files/Common Files/VST3/The FilterFish.vst3"
```
Keep JUCE at 8.0.15 or newer: 8.0.4 crashed (heap corruption) when the editor closed on current Windows.

## 8. Ideas for v2
Drag-and-drop mod matrix (Volcano-style), 2–4 filters in serial/parallel, drawable LFO,
filter response curve display, oversampling for the distortion/fold types, MIDI-triggered
glitch, preset tagging/favourites, user preset save/load files, skins.
