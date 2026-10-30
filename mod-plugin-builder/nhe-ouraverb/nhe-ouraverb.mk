######################################
#
# nhe-ouraverb  (path B package file)
#
# Builds the plugin from a GitHub repo at a given commit. Upload this file at
# https://builder.mod.audio/buildroot for an install link, or put it in
# mod-plugin-builder at plugins/package/nhe-ouraverb/nhe-ouraverb.mk.
#
# 1. DPF is a git submodule at dpf/; the hook below fetches it.
# 2. Push, then set NHE_OURAVERB_VERSION to the FULL 40-character commit hash
#    (GitHub: Commits -> copy button, or `git rev-parse HEAD`).
# The filename must match the variable prefix: nhe-ouraverb.mk -> NHE_OURAVERB_.
#
######################################

# <- after your first push, replace the zeros with the full 40-character commit hash
NHE_OURAVERB_VERSION = ada20a9c89e12423ba709dfa60422990bcc3bb84
NHE_OURAVERB_SITE = https://github.com/Kiwooky/NHE-Ouraverb.git
NHE_OURAVERB_SITE_METHOD = git
NHE_OURAVERB_GIT_SUBMODULES = y
NHE_OURAVERB_BUNDLES = nhe-ouraverb.lv2

# fetch git submodules (DPF), as MOD's own packages do (mod-plugin-builder)
NHE_OURAVERB_PRE_DOWNLOAD_HOOKS += MOD_PLUGIN_BUILDER_DOWNLOAD_WITH_SUBMODULES

NHE_OURAVERB_TARGET_MAKE = $(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) NOOPT=true -C $(@D)

define NHE_OURAVERB_BUILD_CMDS
	$(NHE_OURAVERB_TARGET_MAKE)
endef

define NHE_OURAVERB_INSTALL_TARGET_CMDS
	$(NHE_OURAVERB_TARGET_MAKE) install DESTDIR=$(TARGET_DIR) PREFIX=/usr
endef

$(eval $(generic-package))
