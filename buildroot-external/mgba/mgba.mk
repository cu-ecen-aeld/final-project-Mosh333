################################################################################
#
# mGBA Emulator
#
################################################################################

MGBA_VERSION = 0.10.4
MGBA_SITE = https://github.com/mgba-emu/mgba/archive/refs/tags
MGBA_SOURCE = ${MGBA_VERSION}.tar.gz
MGBA_LICENSE = MPL-2.0
MGBA_LICENSE_FILES = COPYING


# see https://github.com/mgba-emu/mgba?tab=readme-ov-file#dependencies
# Look under buildroot/package for the full package list
MGBA_DEPENDENCIES = sdl2 libpng zlib elfutils qt5base sqlite libedit libzip ffmpeg lua json-c

# see https://github.com/mgba-emu/mgba/blob/master/CMakeLists.txt for the flags
#-DBUILD_HEADLESS=ON
#-DUSE_EDITLINE=ON
#-DUSE_READLINE=ON
# Ensure the following libraries are not on host system when compiling:
# sudo apt install libsdl2-dev build-essential
# sudo apt remove --purge libsdl2-dev build-essential; 
# sudo apt autoremove --purge; 
# dpkg -l | grep libsdl2-dev; dpkg -l | grep build-essential; 

define MGBA_CONFIGURE_CMDS
    cmake -S $(@D) -B $(@D)/build \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_SYSTEM_PROCESSOR=armv8-a \
        -DCMAKE_C_COMPILER="$(TARGET_CC)" \
        -DCMAKE_CXX_COMPILER="$(TARGET_CXX)" \
        -DCMAKE_INSTALL_LIBDIR=/usr/lib \
        -DZLIB_LIBRARY="$(STAGING_DIR)/usr/lib/libz.so" \
        -DZLIB_INCLUDE_DIR="$(STAGING_DIR)/usr/include" \
        -DCMAKE_PREFIX_PATH="$(STAGING_DIR)/usr/lib/cmake/Qt5" \
        -DBUILD_UI=ON \
        -DBUILD_SDL=ON \
        -DBUILD_QT=ON \
        -DUSE_EPOXY=ON \
        -DUSE_DISCORD_RPC=OFF \
        -DENABLE_SCRIPTING=ON \
        -DUSE_LUA=5.4 \
        -DLUA_LIBRARY="$(STAGING_DIR)/usr/lib/liblua.so" \
        -DLUA_INCLUDE_DIR="$(STAGING_DIR)/usr/include" \
        -DLIBELF_INCLUDE_DIR="$(STAGING_DIR)/usr/include" \
        -DLIBELF_LIBRARY="$(STAGING_DIR)/usr/lib/libelf.so" \
	    -DSQLITE3_LIBRARY="$(STAGING_DIR)/usr/lib/libsqlite3.so" \
        -DSQLITE3_INCLUDE_DIR="$(STAGING_DIR)/usr/include" \
        -DLIBEDIT_LIBRARY="$(STAGING_DIR)/usr/lib/libedit.so" \
        -DLIBEDIT_INCLUDE_DIR="$(STAGING_DIR)/usr/include" \
	    -DSDL2_LIBRARY="$(STAGING_DIR)/usr/lib/libSDL2.so" \
        -DSDL2_INCLUDE_DIR="$(STAGING_DIR)/usr/include/SDL2" \
        -DSDL2MAIN_LIBRARY="$(STAGING_DIR)/usr/lib/libSDL2main.a" \
        -DSDL_INCLUDE_DIR="$(STAGING_DIR)/usr/include/SDL2" \
        -DCMAKE_EXE_LINKER_FLAGS="-lSDL2" \
        -DPNG_LIBRARY="$(STAGING_DIR)/usr/lib/libpng.so" \
        -DPNG_PNG_INCLUDE_DIR="$(STAGING_DIR)/usr/include"


endef

define MGBA_BUILD_CMDS
    $(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/build
endef

define MGBA_INSTALL_TARGET_CMDS
    $(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/build install DESTDIR=$(TARGET_DIR)
endef

$(eval $(cmake-package))

