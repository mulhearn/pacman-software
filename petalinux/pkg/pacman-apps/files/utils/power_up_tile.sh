#!/bin/sh
#
# Power up one tile at the nominal ASIC voltages, through the pacman_cmdserver (as the power
# submenu of pacman_menu).
#
# usage:
#   power_up_tile.sh <tile 1-10>
#

PACMAN_UTIL=/home/root/utils/pacman_util.py

# Nominal DAC codes (as pacman_menu)
VDDA_DN=0xE2FF
VDDD_DN=0x75FF

TILE=$1
case "$TILE" in
  [1-9]|10) ;;
  *) echo "usage:  power_up_tile.sh <tile 1-10>"; return 1 2> /dev/null || exit 1 ;;
esac
T=$((TILE - 1))

# DACs, then tile power, then the tile
$PACMAN_UTIL \
    --write $((0x00200010 + T)) $VDDA_DN \
    --write $((0x00200020 + T)) $VDDD_DN \
    --write 0x00100114 0 \
    --write 0x00100214 $T > /dev/null
