# OurAVerb: developer notes

Not needed to play it; for anyone building, testing or changing it. Every control mapping is in
[`docs/spec.md`](docs/spec.md); history in [`CHANGELOG.md`](CHANGELOG.md).

URI `urn:mod-cookbook:nhe-ouraverb` (kept from the prototype so pedalboards and presets keep
working) · mono in, stereo out · ReverbPlugin · DPF as a git submodule at `dpf/`, pinned to the
commit the [MOD plugin playbook](https://github.com/Kiwooky/MOD-plugin-playbook) tests against.

## Build and test

```sh
git clone --recursive https://github.com/Kiwooky/NHE-Ouraverb.git
make                                   # bin/nhe-ouraverb.lv2
tools/check.sh --src plugins/nhe-ouraverb --bundle bundle/nhe-ouraverb.lv2 --test tests/test_nhe_ouraverb.py
python3 tools/face_click_test.py bundle/nhe-ouraverb.lv2
python3 tools/face_prototype.py bundle/nhe-ouraverb.lv2 bench.html   # face test bench in a browser
```

`check.sh` builds native, Duo (arm32) and Duo X/Dwarf (arm64) through the builder's own hooks,
compares the hand-written TTL with DPF's generator, and runs the audio tests natively and on the
Duo build under qemu (74 tests). Packages: `g++ g++-arm-linux-gnueabihf g++-aarch64-linux-gnu
qemu-user lilv-utils`, Python `numpy scipy rdflib` (plus `playwright pillow` for the face tools).

**Releasing:** push, then set `NHE_OURAVERB_VERSION` in `mod-plugin-builder/nhe-ouraverb/nhe-ouraverb.mk`
to the full 40-character commit hash and upload that file at builder.mod.audio. Bump
`lv2:microVersion` (TTL) and `d_version()` (code) together on every release: MOD caches the face
and ports by version.

**Places are append-only.** MOD saves the Place index in pedalboards and presets; a new place goes
at the end of the list, never in between.

## Signal flow

in → 60 Hz high-pass → one input line → the place's taps (other surfaces: each with time, level,
pan, interaural delay and two-pole air loss) + the Colosseum's step train (seating rows) + the
walls (two cross-fed loop lines with a floor tap, so the echo ping-pongs) → Tone (two first-order
shelves at 450 Hz on the early reflections, one per lap in the walls; the lows a bright place
won't reflect feed a long, smeared rumble) → Surface's diffuse part (4 allpasses per side) → the
cover (filtered feedback comb, Height apart) → soft limit on the wet → Mix.

## Measured (native and Duo build under qemu, identical)

- Fjord walls at 2.600 / 3.200 s at 44.1, 48 and 96 kHz; water bounce +7 ms; Expanse 3 % → 78 / 96 ms.
- Colosseum: 16 row reflections; the chirp starts about 233 Hz and falls about an octave.
- Surface (wall echo, noise burst): foliage −15 dB, snow −28 dB re rock; centroid 9.0 → 1.7 kHz.
- Air 0 (dense): 0.42 s echo 4.6 kHz centroid, 3.7 s echo 1.1 kHz (Air 10, open: 9.0 kHz).
- Tone 10: first echo −11 dB of lows, highs +1.4 dB; lows 5 … 8 s later +6.7 dB (the rumble).
  Tone 0: −11 dB of highs. Tone 5 is sample-for-sample identical to the build before Tone.
- Cover at 2 m: 86 Hz comb peaks 14 dB over the troughs; Cover 100 is +0.3 dB vs Cover 0.
- Loudness vs bypass (RMS, guitar level): places +0.1 … +0.6 dB; presets within ±4 dB, except
  Bottom of the Well (+6.4) and Testing Booth (+5.5), kept as made on the unit.
- Worst cases (rock, Bounce 10, closed ice cover; and Tone 10 at Expanse 200): peaks ≤ 0.75,
  every tail decays.
- CPU, relative only (Duo build under qemu): Fjord defaults about 1.0× NHE Taj Mahal; Tone off
  centre adds about 0.4×; heaviest case (Forest, closed cover, Tone 10) about 1.95×. Moving Expanse
  or Surface costs more while it glides. *The Duo's own meter is the real number.*
- Memory: three 4 MB lines (sized for 96 kHz), the rumble's 1 MB, plus 0.5 MB.

## Face

Artwork by Niels, reusing parts of the Trance Atlantic face. 2x images drawn at 650 × 400. The
Place readout is pre-rendered from the DotMatrix font (no web font shipped). The orb's ripples
follow the plugin's Input Level output (`modgui:monitoredOutputs`). The orb layers are pinned to
their own compositor layers (`will-change`) so the ripples don't make the whole panel flicker.

## Known issues

- **Frozen ripples on MOD OS 1.14:** if the orb stops moving, the web interface has missed a
  `data_ready` handshake (the sound is fine). Reload with every other MOD tab closed, or see the
  Trance Atlantic notes for the console fix.
