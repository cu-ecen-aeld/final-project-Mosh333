#!/bin/sh
set -x

ROM_DIR="/root/mgba_rom_files"
BASE_URL="https://github.com/Mosh333/sample-roms/raw/refs/heads/master/remote_gba_repo"

ROMS="
pokemon_leaf_green_usa
pokemon_ruby_usa
pokemon_sapphire_usa
yu-gi-oh_gx_duel_academy_usa
"

for rom in $ROMS; do
    mkdir -p "$ROM_DIR/$rom"
    FILE="$ROM_DIR/$rom/$rom.gba"
    if [ -f "$FILE" ]; then
        echo "$rom.gba already exists, skipping."
    else
        echo "Downloading $rom.gba..."
        wget --no-check-certificate -q -O "$FILE" "$BASE_URL/$rom.gba"
    fi
done

