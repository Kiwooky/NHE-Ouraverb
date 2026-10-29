######################################
#
# nhe-ouraverb-stereo  (path B package file): Ouraverb Stereo, from the same repo as the mono one
#
# Builds the plugin from a GitHub repo at a given commit. Upload this file at
# https://builder.mod.audio/buildroot for an install link, or put it in
# mod-plugin-builder at plugins/package/nhe-ouraverb-stereo/nhe-ouraverb-stereo.mk.
#
# 1. DPF is a git submodule at dpf/; the hook below fetches it.
# 2. Push, then set NHE_OURAVERB_STEREO_VERSION to the FULL 40-character commit hash
#    (GitHub: Commits -> copy button, or `git rev-parse HEAD`).
# The filename must match the variable prefix: nhe-ouraverb-stereo.mk -> NHE_OURAVERB_STEREO_.
#
######################################

# <- after your first push, replace the zeros with the full 40-character commit hash
NHE_OURAVERB_STEREO_VERSION = 860a64a379f7b46b1541cfa60a0b6a45877212cc
NHE_OURAVERB_STEREO_SITE = https://github.com/Kiwooky/NHE-Ouraverb.git
NHE_OURAVERB_STEREO_SITE_METHOD = git
NHE_OURAVERB_STEREO_GIT_SUBMODULES = y
NHE_OURAVERB_STEREO_BUNDLES = nhe-ouraverb-stereo.lv2

# fetch git submodules (DPF), as MOD's own packages do (mod-plugin-builder)
NHE_OURAVERB_STEREO_PRE_DOWNLOAD_HOOKS += MOD_PLUGIN_BUILDER_DOWNLOAD_WITH_SUBMODULES

NHE_OURAVERB_STEREO_TARGET_MAKE = $(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) NOOPT=true NAME=nhe-ouraverb-stereo -C $(@D)

define NHE_OURAVERB_STEREO_BUILD_CMDS
	$(NHE_OURAVERB_STEREO_TARGET_MAKE)
endef

define NHE_OURAVERB_STEREO_INSTALL_TARGET_CMDS
	$(NHE_OURAVERB_STEREO_TARGET_MAKE) install DESTDIR=$(TARGET_DIR) PREFIX=/usr
endef

$(eval $(generic-package))
