# Buildroot Gameboy Advance Emulator Console

## Team Member
Moshiur (Mosh333) - Project will be completed individually

## Overview
See the [project wiki](https://github.com/cu-ecen-aeld/final-project-Mosh333/wiki/Project-Overview) for more details.

## Schedule
See the [project schedule dashboard](https://github.com/users/Mosh333/projects/1/views/1) and [project schedule wiki](https://github.com/cu-ecen-aeld/final-project-Mosh333/wiki/Final-Project-Schedule#project-backlog) for more details.


---

## Prerequisites
- Raspberry Pi 3B+
- Minimum of 4GB SD card for storage
- Access to the [Buildroot repository, branch 2024.11.x](https://github.com/buildroot/buildroot/tree/2024.11.x)
- A Linux-based build environment with Git
- A 1080P monitor with built-in speakers
- An 8bitd0 ultimate C 2.4G Wireless Controller
- Keyboard and Mouse

## Project Setup

1. Obtain this repo:
```
gh repo clone cu-ecen-aeld/final-project-Mosh333
```
2. Run make:
```
make
```
3. After the compilation is complete, modify `buildroot/output/target/etc/inittab` and `buildroot/output/images/rpi-firmware/config.txt` as mentioned in https://github.com/cu-ecen-aeld/final-project-Mosh333/blob/main/board/gba_emulator/inittab_readme and https://github.com/cu-ecen-aeld/final-project-Mosh333/blob/main/board/gba_emulator/config_readme.txt.
4. Plug in a USB based SD card reader and load it into Ubuntu (`Devices` -> `USB` and click the USB device).
5. To flash the final image, first confirm:
```
lsblk
```
```
NAME   MAJ:MIN RM   SIZE RO TYPE MOUNTPOINT
...
sdb      8:16   1  14.9G  0 disk 
└─sdb1   8:17   1  14.9G  0 part /media/moshiur/9AAAB1FAAAB1D2CD
```

If the USB is `/dev/sdb`, then run:
```
sudo umount /dev/sdb*
```
```
umount: /dev/sdb: not mounted.
```
Then run:
```
sudo dd if=output/images/sdcard.img of=/dev/sdb bs=4M status=progress conv=fsync
```
```
473956352 bytes (474 MB, 452 MiB) copied, 2 s, 235 MB/s
133+1 records in
133+1 records out
557842944 bytes (558 MB, 532 MiB) copied, 123.584 s, 4.5 MB/s
```
Then eject:
```
sudo eject /dev/sdb
```
Once that's done, unplug from the Ubuntu VM:
```
`Devices` -> `USB` and click the USB device
```
To make sure the USB SD card reader is formatted, plug it back into the VM to confirm:
```
(`Devices` -> `USB` and click the USB device)
```
`lsblk` should confirm it's formatted correctly:
```
NAME   MAJ:MIN RM   SIZE RO TYPE MOUNTPOINT
...
sdb      8:16   1  14.9G  0 disk 
├─sdb1   8:17   1    32M  0 part /media/moshiur/7EEE-DF79
└─sdb2   8:18   1   500M  0 part /media/moshiur/rootfs
```
Again, remove from VM (`Devices` -> `USB` and click the USB device) and unplug from Windows.

6. Remove the sd card from the USB reader and plug it into the Raspberry Pi 3B+
7. Turn on the Raspberry Pi 3B+ and the 8Bitdo Controller. First the first boot up, the system will restart once so make sure to turn on the controller back on.
8. The final start up screen should look like 🎉:
![mgba_ui_menu](https://github.com/user-attachments/assets/f8998e2f-fdd0-423d-8191-e93cbf7af8c2)
9. At the time of writing, `Play`, `Download`, `Background` and `Quit` features are implemented.
10. The `Play` submenu should look like:
![1000011318](https://github.com/user-attachments/assets/8cfea705-29b4-4405-8b1a-1c7cd4608c97)

11. After selecting a game, the `mgba-qt` program should launch automatically:
![1000011319](https://github.com/user-attachments/assets/a002754b-b100-41ce-9063-64aba1164646)

12. The `Download` feature only downloads games specified in https://github.com/Mosh333/sample-roms/tree/refs/heads/master/remote_gba_repo. It may be modified in the [download_roms.sh script](https://github.com/cu-ecen-aeld/final-project-Mosh333/blob/main/board/gba_emulator/overlay/root/download_roms.sh).
13. The `Background` feature allows users to modify the background wallpaper in the `mgba_menu`:
![1000011299](https://github.com/user-attachments/assets/76e72d75-3b71-4d35-a59a-1401f0a1dec9)
![1000011298](https://github.com/user-attachments/assets/fa094f8e-b3ab-4671-9eb2-fc21f823f530)
![1000011297](https://github.com/user-attachments/assets/59cee4a2-da37-4d3b-acbf-5a3461814a91)

14. The `Quit` feature allows users to power off the Raspberry Pi.

---

## Some useful facts

Some useful workflows that helped me complete this project.

1. You can SSH into the Raspberry Pi (assuming it has ethernet connection to internet) using:
```
ssh root@<pi_ip_addres>
```
The password for `root` user is `root`. This can be configured via `make menuconfig`.

2. You can recompile only the mgba and mgba_menu programs using:
```
make mgba-dirclean mgba_menu-dirclean && make
```
This is useful as you try to iterate changes to either `mgba` or the `mgba_menu` programs.

To list all possible options, run:
```
make show-targets
```
```
alsa-lib alsa-utils brcmfmac_sdio-firmware-rpi busybox dejavu dropbear e2fsprogs elfutils eudev evtest expat ffmpeg fontconfig freetype gcc-final glibc host-acl host-attr host-autoconf host-automake host-blake3 host-ccache host-cmake host-dosfstools host-e2fsprogs host-eudev host-fakeroot host-genimage host-hiredis host-kmod host-libtool host-libzlib host-m4 host-makedevs host-mkpasswd host-mtools host-patchelf host-pkgconf host-skeleton host-tar host-util-linux host-xxhash host-zlib host-zstd hwdata ifupdown-scripts initscripts iw json-c kmod libdisplay-info libdrm libedit libegl libepoxy libevdev libexif libgbm libgl libgles libglew libglu libnl libopenssl libpciaccess libpng libpthread-stubs libxcb libxcrypt libxkbcommon libzip libzlib linux linux-headers lua luainterpreter matchbox matchbox-lib mcookie mesa3d mesa3d-demos mgba mgba_menu minizip minizip-zlib mtdev ncurses openssl pcre2 pixman qt5base qt5multimedia qt5script qt5svg readline rpi-firmware sdl2 sdl2_gfx sdl2_image sdl2_mixer sdl2_net sdl2_ttf skeleton skeleton-init-common skeleton-init-sysv sqlite toolchain toolchain-buildroot udev urandom-scripts util-linux util-linux-libs wget wireless_tools wpa_supplicant xapp_xauth xapp_xinit xapp_xkbcomp xcb-proto xcb-util xcb-util-image xcb-util-keysyms xcb-util-renderutil xcb-util-wm xdata_xbitmaps xdriver_xf86-input-evdev xdriver_xf86-video-fbturbo xfont_encodings xfont_font-alias xfont_font-cursor-misc xfont_font-misc-misc xfont_font-util xkeyboard-config xlib_libICE xlib_libSM xlib_libX11 xlib_libXau xlib_libXcursor xlib_libXdamage xlib_libXdmcp xlib_libXext xlib_libXfixes xlib_libXfont2 xlib_libXft xlib_libXi xlib_libXinerama xlib_libXmu xlib_libXrandr xlib_libXrender xlib_libXres xlib_libXt xlib_libXxf86vm xlib_libfontenc xlib_libxcvt xlib_libxkbfile xlib_libxshmfence xlib_xtrans xorgproto xserver_xorg-server xutil_util-macros zlib zziplib rootfs-ext2 rootfs-tar
```

3. To configure the project in Buildroot using the UI, run (from `buildroot` dir):
```
make menuconfig
```
or alternatively if you want to configure via the file:
```
vim .config
```
then
```
make menuconfig
```
then save&exit.
Afterwards.
```
make savedefconfig
```
A file called `defconfig` should contain the flags enabled in your config file. Save this in the `board/gba_emulator` dir for safe keeping.

4. To configure the Kernel, Driver and other low level configurations in Buildroot using the UI, run (from `buildroot` dir):
```
make linux-menuconfig
```
or alternatively if you want to configure via the file:
```
vim output/build/linux-*/.config
```
then
```
make linux-menuconfig
```
then save&exit.
Make a copy of this `linux.config` file in the `board/gba_emulator` dir for safe keeping. Use this command:
```
cp output/build/linux-*/.config ../board/gba_emulator/linux.config
```

5. To force clean everything, one may perform:
```
make linux-dirclean
make clean
make
```
Please note the manually editted files may lose its configurations: `buildroot/output/target/etc/inittab` and `buildroot/output/images/rpi-firmware/config.txt` as mentioned in https://github.com/cu-ecen-aeld/final-project-Mosh333/blob/main/board/gba_emulator/inittab_readme and https://github.com/cu-ecen-aeld/final-project-Mosh333/blob/main/board/gba_emulator/config_readme.txt.

6. To include additional GBA ROMs, store them in:
```
board/gba_emulator/overlay/mgba_rom_files/

```
