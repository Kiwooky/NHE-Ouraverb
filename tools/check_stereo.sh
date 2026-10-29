#!/usr/bin/env bash
# Runs tools/check.sh on Ouraverb Stereo. The stereo plugin shares the mono source and face,
# and check.sh wants each in one folder, so stage a merged copy first. It also builds the mono
# plugin (native and Duo) so the tests can hold the stereo one against it on both machines.
#   bash tools/check_stereo.sh [--no-qemu]
set -euo pipefail
cd "$(dirname "$0")/.."
HERE="$PWD/tools"
CACHE="${PB_CACHE:-$HOME/.cache/mod-playbook}"
S="$(mktemp -d)"; trap 'rm -rf "$S"' EXIT
mkdir -p "$S/plugins/nhe-ouraverb-stereo" "$S/bundle"
cp plugins/nhe-ouraverb-stereo/DistrhoPluginInfo.h "$S/plugins/nhe-ouraverb-stereo/"
cp plugins/nhe-ouraverb/OuraverbPlugin.cpp "$S/plugins/nhe-ouraverb-stereo/OuraverbStereoPlugin.cpp"
sed 's#^FILES_DSP.*#FILES_DSP = OuraverbStereoPlugin.cpp#' plugins/nhe-ouraverb-stereo/Makefile > "$S/plugins/nhe-ouraverb-stereo/Makefile"
mkdir -p "$S/bundle/nhe-ouraverb-stereo.lv2"
cp -r bundle/nhe-ouraverb.lv2/modgui "$S/bundle/nhe-ouraverb-stereo.lv2/"   # the shared face,
cp -r bundle/nhe-ouraverb-stereo.lv2/. "$S/bundle/nhe-ouraverb-stereo.lv2/"  # then stereo's own images

# the mono plugin to compare against (check.sh fetches DPF into the cache; do the same here)
if [ -d "$CACHE/dpf" ]; then
  python3 "$HERE/assemble_recipe.py" plugins/nhe-ouraverb bundle/nhe-ouraverb.lv2 "$S/nhe-ouraverb.mk" >/dev/null
  for T in native arm32; do
    rm -rf "$S/dpf-$T"; cp -r "$CACHE/dpf" "$S/dpf-$T"
    C=""; [ "$T" != native ] && C="CROSS=$T"
    make -s -f "$HERE/harness.mk" RECIPE="$S/nhe-ouraverb.mk" PREFIX=NHE_OURAVERB SRC="$S/dpf-$T" OUT="$S/mono-$T" $C >"$S/mono-$T.log" 2>&1 \
      || { cat "$S/mono-$T.log"; echo "FAIL building the mono plugin ($T)"; exit 1; }
  done
  export PB_SO_MONO="$S/mono-native/nhe-ouraverb.lv2/nhe-ouraverb_dsp.so"
  export PB_SO_MONO_ARM="$S/mono-arm32/nhe-ouraverb.lv2/nhe-ouraverb_dsp.so"
else
  echo "(no DPF in $CACHE yet: run tools/check.sh once; the comparison with mono is skipped)"
fi

bash tools/check.sh --src "$S/plugins/nhe-ouraverb-stereo" --bundle "$S/bundle/nhe-ouraverb-stereo.lv2" \
    --test tests/test_nhe_ouraverb_stereo.py "$@"
