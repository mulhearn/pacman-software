#!/bin/sh
#
# Report each tile's VDDA and VDDD voltage and current, through the pacman_cmdserver.
#
# usage:
#   report_power.sh
#

PACMAN_UTIL=/home/root/utils/pacman_util.py

READS=""
for T in 0 1 2 3 4 5 6 7 8 9; do
    for OFF in 0x30 0x40 0x50 0x60; do
        READS="$READS --read $((0x00200000 + OFF + T))"
    done
done

# Values from the reply, in the order requested
$PACMAN_UTIL $READS | awk '
  /msg_type: REP/ { rep = 1; next }
  rep && /READ:/  { v[n++] = $NF }
  END {
    for (t = 0; t < 10; t++) {
      gsub(/[()]/, "", v[4*t]); gsub(/[()]/, "", v[4*t+1])
      gsub(/[()]/, "", v[4*t+2]); gsub(/[()]/, "", v[4*t+3])
      printf("TILE %2d:  VDDA %5d mV %5d mA   VDDD %5d mV %5d mA\n",
             t + 1, v[4*t], v[4*t+2], v[4*t+1], v[4*t+3])
    }
  }'
