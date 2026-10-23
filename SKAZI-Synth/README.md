# SKAZI v1.0, by Asher Swissa

SKAZI is a psytrance, acid and wavetable synthesizer. It comes as VST3 for Windows (x64), and as VST3 and AU for macOS as a universal binary (Apple Silicon + Intel). Each platform also gets a standalone app.

## Features

- **PANIC kill switch:** a neon red button in the toolbar. It sends MIDI All Notes Off (CC 123) and Reset All Controllers (CC 121). It also flushes every delay and reverb buffer, zeroes all envelopes, oscillator and LFO state, and clears the zero-delay-feedback filter states and comb buffers, so runaway audio stops in one block. Incoming CC 120 does the same, and CC 121 and CC 123 are handled too.
- **Phase Lock** per oscillator: every note-on restarts the oscillator at exactly 0°, legato included, for tight psy basslines.
- **Oversampling:** the whole voice engine (oscillators, warp, saturation, filters) can run at 1x, 2x (default), 4x or 8x, with polyphase IIR anti-alias filtering.
- **Analog warmth:** gentle non-linear saturation before and after the filters.
- **Audio-rate modulation:** LFOs run up to 500 Hz. The 16-slot mod matrix is evaluated every (oversampled) sample.
- **8 LFOs, 4 envelopes, 16 voices** (plus a dedicated sequencer voice).
- **3,000 factory presets with a tag browser.** 16 root folders: 145 BPM Subs (350), Techno Bass (250), House Bass (200), Deep Bass (200), Acid Leads (250), Hard Leads (300), LFO Madness (300), FM Plucks (200), FX (200), Acid Lines, Bass, Lead, Pad, Pluck, Keys and Sequence. You can search names and tags and filter by folder and multiple tags. Save your own presets with tags; they're stored as XML (`*.skazi`) in `Documents/SKAZI/Presets`. `factory_presets.json` lists the whole library's metadata.
- **Centre console:** a real-time oscilloscope with light trails, a 3D wavetable view of oscillator 1 that reacts to the audio level, and an FFT spectrum with peak trails.
- **Five skins:** Hyper (black hardware body, red LCD and LED keys; the default), Robot, Chrome, Vintage and Neon.
- **Resizable window**, from 50% to 150% at a fixed aspect ratio, rendered through OpenGL. Builds as C++20 on JUCE 8.

## Engine

- **Oscillators:** 2 oscillators with Sine, Saw, Square and Triangle (band-limited with polyBLEP), plus 4 morphing wavetables with band-limited mip-maps and frame interpolation.
  - Up to 16 unison voices, with detune and blend.
  - Phase controls: phase, random phase and Phase Lock.
  - FM, AM and ring warp from the other oscillator.
- **Sub and noise:** a sub oscillator, and white or pink noise with a pitch control.
- **Filters:** 2 filters, with ZDF ladder (Acid 24) and TPT state-variable LP/HP/BP 12/24, Notch and Comb modes. Drive, key tracking and envelope amount on each; serial, parallel or split routing.
- **Effects:** distortion (Tube, Soft, Hard, Bitcrush), chorus / phaser / flanger, delay (synced or free time, ping-pong), FDN reverb, a compressor with an OTT 3-band mode, and a 3-band EQ.
- **Step sequencer:** 16 steps, with accent, slide, gate, shuffle and key transpose.

## Performance

Measured with `RenderTest --stress` in a worst case: a 16-note chord, 2 × 16-voice unison wavetables, two filters and every effect on.

| Oversampling | Share of one CPU core |
|---|---|
| 1x | ~21 % |
| 2x | ~43 % |
| 4x | ~82 % |
| 8x | ~165 % |

Typical presets use a small fraction of this. Use 4x or 8x for mono basses and leads.

## Building

See `.github/workflows/build.yml`, which builds both installers on GitHub Actions. To build locally on Windows:

```
cmake -S . -B C:/acdb -G "Visual Studio 17 2022" -A x64 -DJUCE_DIR=C:/JUCE -DSKAZI_BUILD_TESTS=ON
cmake --build C:/acdb --config Release
```

Tools (built with `-DSKAZI_BUILD_TESTS=ON`):

- `RenderTest <dir>`: renders demos, writes `factory_presets.json`, checks PANIC and takes snapshots of every skin and tab.
- `RenderTest <dir> --calibrate`: plays all presets and writes `PresetGains.h`.
- `RenderTest <dir> --stress`: runs the CPU worst case.
