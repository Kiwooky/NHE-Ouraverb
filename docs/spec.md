# Ouraverb — spec

| | |
| --- | --- |
| Name / maker | Ouraverb · New Horizon |
| URI | `urn:mod-cookbook:nhe-ouraverb` (path A; frozen once shared) |
| Where it starts | Imagined: outdoor acoustics, built from known blocks (multitap delay, cross-fed loop, allpass diffusion, comb) |
| Traits | tail, feedback loop, buffer, Mix knob, stereo out (mono in) |
| Category | ReverbPlugin |
| Target | MOD Duo (OS 1.14 RC); builds for Duo X / Dwarf too |
| Face | Niels's artwork (build 2): orbital display, sliders, OU + readout |

## The idea

Outdoors there's no room to wrap the sound, so you don't get a dense wash. You get a few
distinct echoes from distant surfaces, each one darker than the last. The **place** is the
geometry (where the surfaces are); the knobs are size, material and sky.

## Controls

| # | Control | Range | Default | Mapping |
| --- | --- | --- | --- | --- |
| 3 | Place | Fjord · Canyon · Valley · Forest · Colosseum | Fjord | Loads a tap table. **Append only**: MOD saves the index |
| 4 | Expanse | 2 … 200 % (log) | 100 % | Scales every time in the place. 300 ms glide (tape-like bend) *guess* |
| 5 | Surface | 0 … 10 | 8 | Snow (0) → foliage (~3) → rock (10). With s = 1 − Surface/10: reflectivity 1 − 0.75·s^1.3; damping 18 kHz·e^(−3.4 s); diffuse share min(1, s/0.6); scatter up to 8 % of tap time *guess* |
| 6 | Air | 0 … 10 | 5 | Dense (0) → open (10). Loss per path: fc = 20 kHz ÷ (1 + 1.2·(10 − Air)·t), two poles. Far echoes darker *guess* |
| 7 | Bounce | 0 … 10 | 4 | Feedback per wall: 0.95·b^0.6·reflectivity·(1 + 0.25·Cover), capped 0.93. 0 = one answer per wall *guess* |
| 8 | Cover | 0 … 100 % | 0 % | How much sky is closed: canopy comb mix and feedback (Cover × Cover-density reflectivity) |
| 9 | Cover density | 0 … 10 | 3 | Canvas (0: 1.2 kHz, reflectivity 0.55) → ice (10: 9 kHz, 0.9) |
| 10 | Cover height | 1 … 60 m (log) | 20 m | Canopy round trip 2·H ÷ 343 m/s. 20 m ≈ 117 ms flutter, 2 m ≈ 86 Hz drone. 120 ms glide (sweeps flange) |
| 11 | Mix | 0 … 100 % | 35 % | Dry at unity to 50 %, wet full from 50 % |
| 12 | Tone | 0 … 10 | 5 | t = (Tone − 5) ÷ 5. Early reflections: two first-order shelves at 450 Hz, lows × (1 − 0.9 t⁺ + 0.3 t⁻), highs × (1 + 0.25 t⁺ − 0.75 t⁻). Per lap: lows × (1 − 0.6 t⁺), highs × (1 − 0.6 t⁻). The lows removed on the bright side feed the rumble: two smeared lines (0.29 / 0.43 s × √Expanse, 400 Hz low-pass, Householder mix), RT (5 + 12 × Bounce/10) × √Expanse s, capped 16 s. Exact no-op at 5 *guess, tuned by test* |
| 13 | Tails | off/on | on | In the settings. On: bypass mutes the input, echoes ring out. Off: wet fades, lines are cleared |
| 14 | Input Level (output) | 0 … 1 | | For the face: peak follower, instant rise, ~180 ms fall, −48 … −12 dBFS → 0 … 1, ^1.5 (Trance Atlantic's) |
| 15 | Bypass | | | In plugin, 10 ms crossfade, dry at unity |

## Face

| Element | Drives / shows |
| --- | --- |
| OU button + readout | Steps Place; readout shows the place name (pre-rendered DotMatrix strip) |
| Ceiling cover slider | `cover`: drag or click along the track (3 px dead zone before a drag) |
| Cover density slider | `cover_density` |
| Knobs | tone, mix, expanse, surface, air, bounce, cover_height ("Height") |
| Orbital display: cover (bottom) | scale = cover ÷ 100 from the centre; opacity = 0.1 + 0.9 × cover_density ÷ 10 (hidden at cover 0) |
| Orbital display: dynamics | opacity = input_level |
| Orbital display: height (top) | opacity = (ln(cover_height) ÷ ln(60))³: darkens most near the top (0.39 at the default 20 m) |
| Lights | Tails (switch), On/Off (bypass light) |

Place change: ducks the wet (~40 ms), swaps the geometry, fades back in (~150 ms). The
input line is shared, so the new place answers what you just played straight away.

## Places (times at Expanse 100 %)

| Place | Walls (L / R round trip) | Floor | Other surfaces | Notes |
| --- | --- | --- | --- | --- |
| Fjord | 2.6 s / 3.2 s | water, 7 ms, strong | 0.42, 1.35, 3.7, 4.4 s | about 1 km wide, you off-centre |
| Canyon | 0.90 / 1.25 s | 4 ms | 6 ledges, 0.31 … 2.9 s | |
| Valley | 2.1 / 2.9 s, quieter | 12 ms | 6 slopes, 0.9 … 4.6 s | |
| Forest | 60 / 85 ms, quiet | 3 ms | 10 trunks, 13 … 270 ms | |
| Colosseum | 0.55 / 0.58 s | arena sand, 6 ms | 18, 29, 50 ms, far side 1.1 s | 16 seating rows from 35 ms, 4.2 ms apart, spacing +2.5 % per row: a chirp falling ~200 → 140 Hz |

Taps have a pan and an interaural delay (up to 0.65 ms); each wall is heard mostly on its own
side with the far ear 0.6 ms late.

## Factory presets

Inner City (Fjord at 3 %: building fronts 13 m apart) · Box Canyon · Quarry (Colosseum at
180 %, rock) · Fjord Under Sail · Rainforest · Snowfield · Ice Cathedral · Drone Fjord.

### Landmarks

| Preset | Based on | Settings | Documented vs guessed |
| --- | --- | --- | --- |
| Pompeii Arena | Amphitheatre of Pompeii (c. 70 BC): 135 × 104 m overall, arena 66.7 × 35.1 m sunk 6 m, ~20,000 seats. Pink Floyd filmed *Live at Pompeii* there in October 1971 with no audience: bare stone, nobody to soak up the sound | Colosseum at 80 %, Surface 9, Air 6, Bounce 4 | Dimensions documented (Wikipedia). No acoustic measurements of the amphitheatre found; Pompeii's Large Theatre measured < 1 s RT today, ≤ 2 s as built (Iannace et al. 2011). Expanse is a compromise: the true size is ~0.7× the Colosseum, but Expanse also scales the seating rows, which would push the row chirp too high |
| Epidaurus | Greek theatre: 31 lower rows, 0.746 m deep, 0.367 m high; the rows ring around 220 Hz (A3) and late reverb is short and quiet | Colosseum at 105 % (rows start ~222 Hz), rock, Air 7, Bounce 1, Mix 30 | Row size and ringing documented (Lokki et al. 2013). Place geometry beyond the rows is the Colosseum's, not Epidaurus's |
| Königssee | Bavarian lake between sheer rock walls; the boat's trumpeter plays to the Echowand and it answers up to seven times | Fjord at 60 %, rock, Air 7, Bounce 7 | Seven repeats documented (Wikipedia). Echo time and wall distances guessed |

Not modelled (enclosed, not outdoors): Inchindown oil tanks (record 112 s echo), Hamilton
Mausoleum (~15 s). El Castillo at Chichen Itza (clap → falling chirp off the staircase) needs a
new place: a staircase alone, with no arena around it.

## Promises (each is a test in `tests/test_nhe_ouraverb.py`)

- Fjord walls land at 2.6 s (left) and 3.2 s (right), at 44.1, 48 and 96 kHz; the water bounce 7 ms after.
- Expanse 50 % halves every time; Expanse 3 % puts the walls at 78 / 96 ms.
- Bounce 0: no second crossing. Bounce 10: a strong one.
- Colosseum: ≥ 12 row reflections with growing spacing.
- Surface: rock > foliage > snow in level; snow much darker; foliage smears an echo.
- Air 0 (dense): the 3.7 s echo much darker than the 0.42 s one.
- Canopy at 2 m: peaks at 86 Hz multiples; level within ±4 dB of Cover 0; Cover height swept live without ticks.
- Every scene and preset within ±3 dB (presets ±4 dB) of bypass, no ticks.
- Worst case (rock, Bounce 10, ice canopy closed): peak < 1.0, decays at least 20 dB by 38 s.
- Place changes, Expanse moves: no clicks or ticks.
