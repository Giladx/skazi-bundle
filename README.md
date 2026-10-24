# SKAZI Bundle

All SKAZI plugins by Asher Swissa in one place, and one installer for all of them.
Every plugin uses the vendor name **SKAZI**, so DAWs list them together.

| Folder | Plugin | Type |
|---|---|---|
| `SKAZI-Synth` | SKAZI | Synth (instrument) |
| `Shaksuka` | SHAKSUKA | Random chop & glitch FX |
| `PitaPump` | PITA PUMP | Sidechain LFO tool |
| `FilterFish` | The FilterFish | Filter FX |
| `Subich` | SUBICH | Bass + sub bass synth: 2 osc, sub, 2 filters, 2 LFOs, wheels, 1000 presets |
| `Bamba` | BAMBA | Riser & FX maker (uplifters, impacts, rolls, lasers...) |
| `Tahini` | TAHINI | Space delay + reverb with duck and freeze |
| `Zhug` | ZHUG | Saturator + widener, 4x oversampled, multiband |
| `Bully` | BULLY | Wavetable synth: 3 osc with dual warp, 2 filters, 10 LFOs, chaos, mod matrix, FX rack, arp, analog character (drift, saturation, boost, spread), 462 presets |

`Common` holds the shared metal skin, knobs, preset bar and A/B code used by SUBICH, BAMBA, TAHINI and ZHUG.

Plugin IDs (manufacturer + plugin codes) are unchanged, so existing DAW projects keep loading them.

## Build the installer (Windows)

Needs Visual Studio 2022 (C++ workload), CMake and Git.

```
powershell -ExecutionPolicy Bypass -File build_bundle.ps1
```

Output: `dist\SKAZI Bundle Setup.exe`, plus every plugin's VST3 and app in `dist\Plugins`.
JUCE is downloaded automatically (8.0.4 and 8.0.15, the versions each plugin was built with).
`-PluginsOnly` builds just the plugins, `-InstallerOnly` re-packs the last built plugins.

## What the installer does

- Lets you tick which plugins to install
- VST3s go to `C:\Program Files\Common Files\VST3\SKAZI\`
- Standalone apps go to `C:\Program Files\SKAZI\` with a SKAZI Start Menu folder (desktop shortcuts optional)
- Removes older separate installs of the same plugins (SKAZI 1.2, SHAKSUKA, The FilterFish) so nothing shows up twice
- Shows up as "SKAZI Bundle" in Windows "Add or remove programs"; the same exe uninstalls (`/uninstall`)
- `/silent` installs everything without a window

The installer is not code-signed, so Windows SmartScreen shows "Windows protected your PC":
click **More info, then Run anyway**.

## Logo

Save the SKAZI logo as `Resources\skazi_logo.png` (and in each plugin's `Resources` folder) and rebuild;
until then a drawn stand-in is used.

## Mac (OS X / macOS)

`mac/build_mac.sh` builds all plugins for Apple Silicon + Intel and makes **`dist/SKAZI Bundle.pkg`**:

- VST3 to `/Library/Audio/Plug-Ins/VST3/SKAZI/`
- Audio Units to `/Library/Audio/Plug-Ins/Components/` (Logic, GarageBand, Ableton)
- Apps to `/Applications/SKAZI/`, with **Uninstall SKAZI Bundle.command**

It must run on a Mac (`bash mac/build_mac.sh`). Without a Mac, push this folder to GitHub:
`.github/workflows/build-bundle.yml` builds the Mac `.pkg` and the Windows `.exe` on GitHub's
machines; download them from the run's **Artifacts**.

The Mac installer is not signed with an Apple Developer ID, so the first time macOS blocks it:
right-click the `.pkg`, choose **Open**, then **Open** again (or System Settings > Privacy & Security > Open Anyway).

Before pushing to GitHub, remove the `SKAZI-Synth/.git` folder from this copy (the full history stays
in `Documents\ACIDANCE\ACIDANCE`), otherwise GitHub treats it as an empty sub-project.
