# pacman_fwcheck, bare metal:  the Vitis workspace fwcheck/, made from the top of pacman-software:
#   xsct apps/fwcheck/install.tcl
#   cd fwcheck && xsct run.tcl        (JTAG:  resets the board;  output on the serial console)
# The source is the Linux one (petalinux/pkg/pacman-apps/files/src/apps/pacman_fwcheck/main.c), on the Linux drivers
# (drivers-common, platform/hw_access.h) and the bare-metal hw_access.c (apps/menu/src).
if {[file exists "fwcheck"] == 1} {
    file delete -force fwcheck
}
setws fwcheck
app create -name fwcheck -hw hardware/pacman.xsa -os standalone -proc ps7_cortexa9_0 -lang C -template {Empty Application(C)}
foreach f {global.c global.h rxtx.c rxtx.h rxtx_menu.c rxtx_menu.h dma.c dma.h jumper.c jumper.h asic.c asic.h atc.h
           iic.c iic.h iic_devices.c iic_devices.h iic_hw1v4.c iic_hw1v5.c iic_sim.c check.c check.h} {
    file copy -force petalinux/pkg/pacman-apps/files/src/drivers-common/$f fwcheck/fwcheck/src/
}
file copy -force petalinux/pkg/pacman-apps/files/src/platform/hw_access.h fwcheck/fwcheck/src/
file copy -force apps/menu/src/hw_access.c fwcheck/fwcheck/src/
file copy -force petalinux/pkg/pacman-apps/files/src/apps/pacman_fwcheck/main.c fwcheck/fwcheck/src/fwcheck.c
file copy -force apps/fwcheck/run.tcl fwcheck/
app build fwcheck
