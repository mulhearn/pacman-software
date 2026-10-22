# pacman_fwcheck, bare metal:  the Vitis workspace fwcheck/, made from the top of pacman-software:
#   xsct apps/fwcheck/install.tcl
#   cd fwcheck && xsct run.tcl        (JTAG:  resets the board;  output on the serial console)
# The source is the Linux one (petalinux/pkg/pacman-apps/files/src/apps/pacman_fwcheck/main.c), on the drivers
# of the bare-metal menu (apps/menu/src).
if {[file exists "fwcheck"] == 1} {
    file delete -force fwcheck
}
setws fwcheck
app create -name fwcheck -hw hardware/pacman.xsa -os standalone -proc ps7_cortexa9_0 -lang C -template {Empty Application(C)}
foreach f {hw_access.c hw_access.h global.c global.h rxtx.c rxtx.h rxtx_menu.c rxtx_menu.h dma.c dma.h jumper.c jumper.h
           asic.c asic.h atc.h iic.h} {
    file copy -force apps/menu/src/$f fwcheck/fwcheck/src/
}
file copy -force petalinux/pkg/pacman-apps/files/src/apps/pacman_fwcheck/main.c fwcheck/fwcheck/src/fwcheck.c
file copy -force apps/fwcheck/run.tcl fwcheck/
app build fwcheck
