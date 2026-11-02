#! /bin/bash

petalinux-package --boot --force --fpga --u-boot --fsbl ../../products/pacman_fsbl.elf

# TFTP boot script (ramdisk build only), from the deploy directory:  petalinux does not copy it
cp -v build/tmp/deploy/images/*/boot.scr.uimg images/linux/ 2>/dev/null || true
