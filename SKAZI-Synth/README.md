# SKAZI v1.3, by Asher Swissa

SKAZI is a psytrance, acid and wavetable synthesizer. It comes as VST3 for Windows (x64), and as VST3 and AU for macOS as a universal binary (Apple Silicon + Intel). Each platform also gets a standalone app.

## Features

- **PANIC kill switch:** a neon red button in the toolbar. It sends MIDI All Notes Off (CC 123) and Reset All Controllers (CC 121). It also flushes every delay and reverb buffer, zeroes all envelopes, oscillator and LFO state, and clears the zero-delay-feedback filter states and comb buffers, so runaway audio stops in one block. Incoming CC 120 does the same, and CC 121 and CC 123 are handled too.
- **Phase Lock** per oscillator: every note-on restarts the oscillator at exactly 0°, legato included, for tight psy basslines.
- **Tight timing:** notes start on their exact sample. The oversampling filters' delay (3 samples at 48 kHz) is measured, padded to the same value at every factor and reported to the host, so the DAW compensates it and switching 1x..8x never moves the sound. Host blocks bigger than announced are processed in chunks instead of cutting notes. Synced LFOs lock to the song position, or to the RUN clock in the standalone app. `TimingTest` checks all of this.
- **Arpeggiator:** Up, Down, Up/Down, Down/Up, Random, Played, Chord and Converge, 1 to 4 octaves, any rate from 1/32 to 4 bars, gate and swing. It follows the song grid while the DAW plays and starts on the first key when it doesn't. A key that lands just after a step boundary (a DAW note a few samples late, or a key played by hand) plays that step at once instead of a step late; the step sequencer does the same.
- **RANDOM:** every press rolls every section again: oscillators, filters, envelopes, all 8 LFOs and the mod matrix, the whole effects rack and the arp, inside ranges that suit the sound's role. Each result is test-played offline; a silent one is rolled again, and the volume is set so every press comes out about as loud as the factory presets with peaks under full scale. `RandomTest` checks 500 presses.
- **Oversampling:** the whole voice engine (oscillators, warp, saturation, filters) and the distortion can run at 1x, 2x (default), 4x or 8x, with polyphase IIR anti-alias filtering.
- **Analog warmth:** gentle non-linear saturation before and after the filters.
- **Audio-rate modulation:** LFOs run up to 500 Hz. The 16-slot mod matrix is evaluated every (oversampled) sample.
- **8 LFOs, 4 envelopes, 16 voices** (plus a dedicated sequencer voice).
- **3,650 factory presets with a tag browser.** 19 root folders: 145 BPM Subs (350), Techno Bass (250), House Bass (200), Deep Bass (200), Acid Leads (250), Hard Leads (300), LFO Madness (300), FM Plucks (200), FX (200), Acid Lines, Bass, Lead, Pad, Pluck, Keys, Sequence, and new in 1.3: Arp Machines (250: psy, trance, acid, FM, techno chord, 1/32 rolling, dub, octave bass, glass and evolving arps), LFO Rhythms (250: trance gates, wobbles, sidechain pumps, stutters, evolving pads, S&H bleeps, tremolo, growls, step-LFO melodies, ring rhythms) and Distorted (150 basses and leads, one for each distortion type). Older folders keep their preset numbers and names, so saved projects load as before. You can search names and tags and filter by folder and multiple tags. Save your own presets with tags; they're stored as XML (`*.skazi`) in `Documents/SKAZI/Presets`. `factory_presets.json` lists the whole library's metadata.
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
- **Effects rack, in signal order:** distortion with 10 types (Tube, Soft, Hard, Bitcrush, Fuzz, Fold, Rectify, Diode, Scream and Destroy, the hardest: huge gain, fold, clip and crush) plus a tone control; an FX filter (LP, HP, BP, Notch, Acid) on the whole mix with its cutoff in the mod matrix; chorus / phaser / flanger / ring / tremolo / auto-pan; delay (synced or free time, ping-pong); FDN reverb; a compressor with an OTT 3-band mode; stereo width with bass mono; and a 3-band EQ.
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
- `TimingTest`: latency reporting, note offsets inside blocks, big host blocks, synced LFOs, sequencer and arp on the grid.
- `RandomTest [presses]`: RANDOM changes every section, is never silent and never clips; every distortion type stays finite and DC-free.
- `SeqGateTest`: the sequencer only plays while keys are held.
