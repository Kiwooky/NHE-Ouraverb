# Changelog

## 1.0.10 (build 10) — Ouraverb Stereo
- New plugin in the same repo: **Ouraverb Stereo** (`urn:mod-cookbook:nhe-ouraverb-stereo`),
  stereo in, stereo out, its own package file `mod-plugin-builder/nhe-ouraverb-stereo/nhe-ouraverb-stereo.mk`.
  It is Niels's two-instance setup in one plugin: the left output is a mono Ouraverb fed the left
  input, the right output one fed the right input. Same controls, presets and face, with
  Niels's STEREO badge over the logo; no input-mode switch (patch one source into both inputs
  for the mono sound).
- Checked: each output ignores the other input exactly; the same signal on both inputs matches the
  mono plugin to within one 24-bit step (the last float bit, about −145 dBFS) natively and on the
  Duo build; two inputs match the two-instance setup the same way.
- CPU (Duo build under qemu): about 1.6× the mono plugin at Fjord defaults, 1.7× at Colosseum
  with a cover, 1.8× at the heaviest (Forest, closed cover, Tone 10). Two instances would be 2×.
  Each side runs its own walls, rows and rumble; what's saved is the work for the side it doesn't
  output (taps, diffuser, cover). With Tone off centre each side needs both sides' taps.
- Mono: the DSP moved into an `Engine` class shared by both plugins. Output sample-for-sample
  identical to build 9 and the same speed (checked against a build of build 9). Version bump only.

## 1.0.9 (build 9) — port group order
- Port groups get `lv2:index`, so MOD's settings panel lists them Location, Distance, Cover, Mix
  (it sorted them alphabetically before). New thumbnail from the new faceplate.

## 1.0.8 (build 8) — new faceplate, Tone on the face, port groups
- Now a GitHub repo (playbook path B): top-level Makefile, package `.mk`, DPF as a submodule,
  check workflow, playbook tools. URI unchanged. Package file built locally for Duo (arm32) and
  Duo X/Dwarf (arm64); not yet built from GitHub (needs the first push).
- Niels's new faceplate: Tone knob (warm .. bright) beside Mix; OU, readout and the knob rows
  moved to match. All other art reused.
- Port groups for MOD's settings panel: **Location** (Place, Expanse, Surface), **Distance**
  (Cover height, Air, Bounce, Tone), **Cover** (Ceiling cover, Cover density), **Mix** (Mix, Tails).
- Niels's presets made on the unit: **Bottom of the Well**, **Testing Booth**. Both play louder
  than bypass (+6.4 and +5.5 dB: Expanse 2 % stacks the reflections within a few ms); kept as made.
- Recipe 816 KB (the new panel image is 176 KB); under the builder's 888 KB ceiling.

## 1.0.7 (build 7) — Tone (settings only, for testing)
From playing it in real hills: "high pitches work a bit better in hills than low sounds, although
loud booms bounce a long time", and a missing tone control for places like the city or the
Ice Cathedral.
- New **Tone** control, 0 … 10, default 5 (settings only, no face element yet). A surface only
  reflects wavelengths smaller than itself, so: **bright** (6 … 10) reflections lose their lows
  below ~450 Hz (−11 dB at 10 on the first echo, highs +1.4 dB) and those lows feed a slow,
  smeared rumble that outlasts the echoes (+6.7 dB of lows 5 … 8 s later); **warm** (0 … 4) the
  highs are absorbed instead (−11 dB at 0) and the low mids come back a little fuller.
- **Tone 5 is sample-for-sample identical to build 6** (checked against a build of build 6).
- New port `tone` before Tails. **Re-add the plugin on the pedalboard.**
- Presets: Ice Cathedral Tone 8, Inner City Tone 7; new **Hill Country** (Valley 70 %, Surface 5,
  Air 6, Bounce 5, Tone 8).
- CPU, Duo build under emulation: Tone adds about 0.4× Taj Mahal when off centre, nothing at 5.
  Heaviest case (Forest, closed canopy, Tone 10) 1.95× Taj Mahal. *Estimate; the Duo's meter is
  the real number.*

## 1.0.6 (build 6) — footswitch position
- Footswitch raised 2.5 px (5 px in the 2x art), approved in the face bench.

## 1.0.5 (build 5) — steady panel
- The orbit layers stay on their own compositor layers (`will-change`, hidden backface), so the
  ripples no longer make the whole panel flicker and dim. Not reproducible in headless Chrome
  (software rendering); checked by eye.

## 1.0.4 (build 4) — latching footswitch
- Footswitch art latches: the "off" art while bypassed, the "on" art while the effect runs
  (build 3 showed the pressed art only while held).

## 1.0.3 (build 3) — face polish
- Niels's slider cap and optimised orbit images (the height layer is a black alpha ramp again;
  build 2's conversion had resampled it). Images now ship as supplied.
- Density renamed **Surface** (port symbol `surface`), as printed on the face.
  **Re-add the plugin on the pedalboard.**
- Cover layer never fully clear while Ceiling cover is up: opacity 10 … 100 % over Cover density.
- Height layer follows the knob position cubed, so it darkens most near the top (39 % at 20 m).

## 1.0.2 (build 2) — the face
- Pedal face from Niels's artwork: orbital display, Ceiling cover and Cover density sliders,
  OU button and dot-matrix Place readout, Mix, Expanse, Surface, Air, Bounce and Height knobs,
  On/Off and Tails lights, footswitch.
- Orbital display, bottom to top: cover (size = Ceiling cover, opacity = Cover density),
  dynamics (the new Input Level output, Trance Atlantic's follower), height (opacity = Cover
  height's knob position).
- New output port `input_level` before the bypass port. **Re-add the plugin on the pedalboard.**
- **Air reversed to match the face:** 0 = dense (far echoes lose their top), 10 = open. The
  default (5) is unchanged; presets carried over.
- `tools/face_prototype.py`: a one-file browser test bench of the face (mod-ui's widget code,
  simulated playing, presets, readouts).

## 1.0.1 (build 1) — renamed controls, landmark presets
From the first listen ("pretty good sounds"; the controls needed renaming):
- Scene → **Place**, Distance → **Expanse**, Surface → **Density** (now reversed: 0 = snow,
  10 = rock, so the number grows with density; default 8 = the old 2), Material → **Cover
  density**, Height → **Cover height**. Port symbols renamed to match; Cover density now
  comes before Cover height. **Re-add the plugin on the pedalboard.**
- New presets: Pompeii Arena, Epidaurus, Königssee. Existing presets carried over.
- No change to the sound at equivalent settings.
- Caught by the gate before upload: on the Duo build, Density 10 (rock) gave NaN (silence).
  `1 − d·0.1` came out a hair below zero on ARM; now written `(10 − d)·0.1`, exact at 10.

## 1.0.0 (build 0) — first prototype
- Scenes Fjord, Canyon, Valley, Forest, Colosseum; Distance, Surface, Air, Bounce; canopy
  (Cover, Height, Material); Mix; Tails; in-plugin bypass. Mono in, stereo out. No face.
- Eight factory presets.
- 53 tests pass natively and on the Duo build under qemu.
