################################################################################
# mgba_menu – Qt‑Widgets front‑end built with qmake
################################################################################

MGBA_MENU_VERSION      = 1.0
MGBA_MENU_SITE         = $(TOPDIR)/../buildroot-external/mgba_menu/src
MGBA_MENU_SITE_METHOD  = local
MGBA_MENU_DEPENDENCIES = qt5base

# Tell the helper which .pro file(s) to process
MGBA_MENU_QMAKE_PROFILES = mgba_menu.pro

# ---------------------------------------------------------------------------
# Install: just drop the binary into /usr/bin on the rootfs
# ---------------------------------------------------------------------------
define MGBA_MENU_INSTALL_TARGET_CMDS
	$(INSTALL) -D $(@D)/mgba_menu $(TARGET_DIR)/usr/bin/mgba_menu
endef

# Nothing needs to go to the staging dir for sdk/sysroot
define MGBA_MENU_INSTALL_STAGING_CMDS
endef

$(eval $(qmake-package))
