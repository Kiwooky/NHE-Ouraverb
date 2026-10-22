# Ouraverb (New Horizon)

Outdoor spaces for MOD (Duo, Duo X, Dwarf). **A reverb about boundaries, made of tails.**
Fjords, canyons, valleys, forests and a stepped amphitheatre, with a canopy you can pull
over the sky. Prototype; face by Niels.

Repo: https://github.com/Kiwooky/NHE-Ouraverb · URI `urn:mod-cookbook:nhe-ouraverb` (kept from the
prototype so pedalboards and presets keep working) · GPL-3.0-or-later code, artwork © New Horizon
Electronics (`ARTWORK-LICENSE.md`) · mono in, stereo out · ReverbPlugin

## Build and install

- **On a MOD:** upload `mod-plugin-builder/nhe-ouraverb/nhe-ouraverb.mk` at builder.mod.audio with
  `NHE_OURAVERB_VERSION` set to the full commit hash you want built.
- **Gate** (builds for Duo, Duo X and Dwarf, compares the TTL with DPF, runs the tests natively
  and on the Duo build under qemu):
  `tools/check.sh --src plugins/nhe-ouraverb --bundle bundle/nhe-ouraverb.lv2 --test tests/test_nhe_ouraverb.py`
- **Repo build:** `git clone --recursive`, then `make` (bundle in `bin/nhe-ouraverb.lv2`).
- **Face:** `python3 tools/face_click_test.py bundle/nhe-ouraverb.lv2`; browser test bench:
  `python3 tools/face_prototype.py bundle/nhe-ouraverb.lv2 bench.html`.

DPF is a git submodule at `dpf/`, pinned to the commit the MOD plugin playbook tests against.

## The idea

A room wraps the sound, so reflections pile up into a wash. Outdoors there's no lid: you get
a few distinct echoes from far-off surfaces, each one darker than the last. The **place** is
its shape; the knobs set its size, what it's made of, and the sky.

## Controls

Names as printed on the face (the port's own name in brackets where it differs).

| Control | Range | Default | What it does |
| --- | --- | --- | --- |
| Place (OU button + readout) | Fjord · Canyon · Valley · Forest · Colosseum | Fjord | The shape of the place. OU steps through them |
| Expanse | 2 … 200 %, tight → vast | 100 % | How far away the walls are. Glides like tape when moved. A Fjord at 3 % is a city street |
| Surface | 0 … 10, soft → hard | 8 | What the walls are made of: fresh snow (0) → foliage (~3) → rock (10). Softer is quieter, darker, smeared into clouds |
| Air | 0 … 10, dense → open | 5 | Dense air eats the top end on the way; far echoes lose more. Open keeps them bright |
| Bounce | 0 … 10, absorb → reflect | 4 | How long the echo keeps crossing between the walls. 0 = one answer per wall |
| Ceiling cover (Cover) | 0 … 100 % | 0 % | Pull a canopy over the sky |
| Cover density | 0 … 10 | 3 | The canopy: canvas (0: dark, leaky) → ice (10: bright, glassy) |
| Height (Cover height) | 1 … 60 m | 20 m | Where the canopy hangs: 20 m flutters, 2 m drones (~86 Hz). Sweep it live for a flange |
| Mix | 0 … 100 % | 35 % | Dry stays at unity up to 50 %, the wet is full from 50 % |
| Tone | 0 … 10, warm → bright | 5 | Which pitches the place throws back. Bright: thin, crisp echoes, the lows roll on as a long rumble (hills, glass, ice). Warm: the highs are absorbed (wood, earth, forest floor). 5 = neutral |
| Tails | light on the face | on | Bypass lets the echoes ring out (off: they fade and are cleared) |

**The orbital display**, bottom to top: the cover grows with Ceiling cover and gets more solid
with Cover density (never below 10 % while it's out); the ripples follow your playing (Input
Level output); the height layer darkens the orb as Height goes up, most near the top.

Bypass is in the plugin (10 ms crossfade, dry at unity). Changing Place ducks the wet, swaps
the geometry and fades back in; the new place answers what you just played.

Settings panel groups: Location (Place, Expanse, Surface), Distance (Height, Air, Bounce, Tone),
Cover (Ceiling cover, Cover density), Mix (Mix, Tails).

Factory presets: Inner City, Box Canyon, Quarry, Fjord Under Sail, Rainforest, Snowfield,
Ice Cathedral, Drone Fjord, Hill Country; three landmarks: Pompeii Arena, Epidaurus, Königssee
(what each is based on: `docs/spec.md`); and Niels's own Bottom of the Well and Testing Booth.

## Signal flow

in → 60 Hz high-pass → one input line → the place's taps (other surfaces: each with time,
level, pan, interaural delay and two-pole air loss) + the Colosseum's step train (seating
rows) + the walls (two cross-fed loop lines with a floor tap, so the echo ping-pongs) →
Surface's diffuse part (4 allpasses per side) → the cover (filtered feedback comb, Cover height
apart) → soft limit on the wet → Mix. Details and every mapping in `docs/spec.md`; all
mappings are ours and a *guess* until your ears say otherwise.

## Measured (tools/check.sh: native and Duo build under qemu, 57 tests, identical)

- Fjord walls at 2.600 / 3.200 s at 44.1, 48 and 96 kHz; water bounce +7 ms; Expanse 3 % → 78 / 96 ms.
- Colosseum: 16 row reflections, spacing 5.0 → 7.0 ms (a chirp falling ~200 → 140 Hz).
- Surface (wall echo, noise burst): foliage −15 dB, snow −28 dB re rock; centroid 9.0 → 1.7 kHz.
- Air 0 (dense): 0.42 s echo 4.6 kHz centroid, 3.7 s echo 1.1 kHz (Air 10, open: 9.0 kHz).
- Canopy at 2 m: 86 Hz comb peaks 14 dB over the troughs; Cover 100 is +0.3 dB vs Cover 0.
- Seating-row chirp starts about 233 Hz at Expanse 100 and falls about an octave across the rows.
- Loudness vs bypass (RMS, guitar level): scenes +0.1 … +0.6 dB; presets −0.0 … +3.1 dB.
- Worst case (rock, Bounce 10, closed ice canopy at 2 m): peak ≤ 0.71, decays 24 dB (Valley) to silence (Forest) over 31 s.
- Stereo correlation: Fjord 0.00, Canyon 0.02, Valley 0.11, Forest 0.01, Colosseum 0.39.
- CPU, relative only (Duo build under qemu): Fjord defaults 1.1× Taj Mahal; Colosseum with
  canopy 1.5×; Forest with canopy 1.8×. Moving Expanse or Surface costs more while it glides.
  *Not measured on a Duo.*
- Memory: three 4 MB lines (sized for 96 kHz) plus 0.5 MB.

## Face

Artwork by Niels (New Horizon Electronics), reusing parts of the Trance Atlantic face. 2x
images drawn at 650 × 400. The Place readout is pre-rendered from his DotMatrix font (no web
font shipped). Test bench: `python3 tools/face_prototype.py bundle/nhe-ouraverb.lv2 bench.html`;
click test: `tools/face_click_test.py bundle/nhe-ouraverb.lv2` (0 failures).

## Not done yet

Face not yet seen on a unit. Place list is append-only from here on. Build 1 renamed and reordered ports: re-add
the plugin on the pedalboard.
