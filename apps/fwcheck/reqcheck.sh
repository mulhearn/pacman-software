#!/bin/bash
# reqcheck.sh [requirements.txt]:  the requirements pacman_fwcheck reports are exactly the "sim+hw" ones of the
# firmware's requirements (default:  the pacman-firmware checkout next to this repo).

cd "$(dirname "$0")/../.."
REQS=${1:-../pacman-firmware/docs/requirements.txt}
SRC=petalinux/pkg/pacman-apps/files/src/apps/pacman_fwcheck/main.c
diff <(grep -E '^[A-Z]+-[0-9]+\.[0-9]+ +sim\+hw' "$REQS" | awk '{print $1}') \
     <(grep -oE '^  \{"[A-Z]+-[0-9]+\.[0-9]+"' "$SRC" | tr -d ' {"') \
  && echo "reqcheck:  pacman_fwcheck covers the sim+hw requirements of $REQS"
