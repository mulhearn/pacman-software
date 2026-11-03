#!/bin/sh
#
# Disable all tiles, turn off tile power and set every tile's DACs to 0, through the
# pacman_cmdserver.
#
# usage:
#   power_down.sh
#

PACMAN_UTIL=/home/root/utils/pacman_util.py

DACS=""
for T in 0 1 2 3 4 5 6 7 8 9; do
    DACS="$DACS --write $((0x00200010 + T)) 0 --write $((0x00200020 + T)) 0"
done

$PACMAN_UTIL \
    --write 0x001002F0 0 \
    --write 0x00100110 0 \
    $DACS > /dev/null
