#!/usr/bin/env bash
# Path B: add DPF to the repo as a git submodule at ./dpf, pinned to the commit
# the playbook tests against. This is DPF's recommended setup, and the one MOD's
# own packages use: the package .mk fetches submodules with mod-plugin-builder's
# MOD_PLUGIN_BUILDER_DOWNLOAD_WITH_SUBMODULES hook (verified on builder.mod.audio).
#   tools/add_dpf_submodule.sh            run from the repo root, then commit
set -euo pipefail
DPF_SHA=61d38eb638449647fb8395a35c5b8dab7e981ba7
[ -d .git ] || { echo "run this from the root of your plugin's git repo"; exit 1; }
[ -e dpf ] && { echo "dpf/ already exists (an old vendored copy? remove it with 'git rm -r dpf' first)"; exit 1; }
git submodule add https://github.com/DISTRHO/DPF.git dpf
git -C dpf checkout -q "$DPF_SHA"
git add .gitmodules dpf              # record the pinned commit before updating, or update resets it
git submodule update --init --recursive dpf
[ "$(git -C dpf rev-parse HEAD)" = "$DPF_SHA" ] || { echo "dpf/ is not at $DPF_SHA"; exit 1; }
echo "DPF $DPF_SHA added as a submodule at dpf/. Commit, then push."
