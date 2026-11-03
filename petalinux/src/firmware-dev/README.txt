# Firmware developers:

1) Build your own firmware

2) Obtain your own pacman.bit file, e.g. from pacman.xsa product via:

> unzip pacman.xsa

3) Copy here, overwriting pacman.bit

4) Use bootgen tool from Vivado to create the BOOT.BIN file:

bootgen -image bootgen.bif -arch zynq -o BOOT.BIN
