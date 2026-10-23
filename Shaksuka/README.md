# SHAKSUKA by Asher Swissa

Random chop / slice multi-FX plugin (VST3 on Windows, VST3 + AU on Mac).

It records the incoming audio into a buffer and, on every grid step, randomly fires
a mix of effects on top of it. The pattern comes from the seed, so the same seed and
settings always give the same chops; EVOLVE slowly mutates it as it loops.

## Controls

| Section | Control | What it does |
|---|---|---|
| Header | RANDOM | Rolls a whole new setup (grid, groove, chaos, effects, seed), always on the grid |
| | DICE / SEED | New random pattern with the same settings / pick a pattern number |
| | ◀ Preset ▶ | 509 presets: 9 factory + 500 in 10 styles (Psy Stutter, Glitch, Tape & Pitch, Dub & Space, Lo-Fi, Breaks, Techno Gate, Trap Rolls, Drunk Groove, Extreme), all on the grid |
| | A / B, Undo / Redo | Compare two setups, undo changes |
| Slicing | SYNC / FREE | Lock slices to the host tempo, or use free time in ms |
| | GRID | 1/1 to 1/64, triplets (T) and dotted (D) |
| | BARS | Pattern length: 1, 2 or 4 bars |
| | FREE MS | Slice length in free mode (10 to 2000 ms) |
| | LENGTH VAR | Chance that a chop holds for 2, 4 or 8 steps |
| Groove | TYPE | Straight, Swing, Shuffle (triplet), Drunk (random swing), Push |
| | SWING | Swing amount (50 to 80 %) |
| | HUMANIZE | Random late timing on each chop |
| Chaos | CHAOS | Chance that a step gets effected |
| | STACK | How many processing FX can layer on one chop (1 to 5) |
| | EVOLVE | How fast the pattern mutates while it loops |
| Output | MIX, GAIN | Dry/wet and output level |

**Effects rack.** Each effect has an on/off switch and an amount (how often it is picked).
The rack lights up whatever is playing right now.

- Source FX (one at a time): Repeat, Stutter, Reverse, Scramble, Tape Stop, Pitch
- Processing FX (stack together): Crush, Filter sweep, Gate, Ring mod, Auto-pan

## Building

Needs CMake 3.22+ and a C++17 compiler (Visual Studio 2022 on Windows, Xcode on Mac).
JUCE 8.0.4 is downloaded automatically.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Plugins land in `build/Shaksuka_artefacts/Release/` (VST3, AU on Mac, Standalone).

On Windows, keep the build folder on a short path (for example `C:\shak\build`):
Visual Studio fails on paths longer than 260 characters.

**Mac without a Mac:** push this folder to GitHub. The workflow in
`.github/workflows/build.yml` builds Windows and Mac (universal AU + VST3) and
attaches them as downloads on the Actions run. Unsigned Mac plugins need
`xattr -cr SHAKSUKA.component` (or `.vst3`) before the DAW will load them.

**Logo:** save the SKAZI logo as `Resources/skazi_logo.png` and re-run CMake.
Until then the editor draws a vector stand-in.

**Engine test:** configure with `-DSHAK_TESTS=ON` and run `ShaksukaEngineTest`.
