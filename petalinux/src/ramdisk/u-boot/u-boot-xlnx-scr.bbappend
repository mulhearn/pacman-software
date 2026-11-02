# PACMAN (ramdisk build):  also build the TFTP boot script boot.scr.uimg (see boot.cmd.tftp), next to boot.scr.
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI += "file://boot.cmd.tftp"

# Keep the ramdisk and device tree below the 256 MB DMA buffer at 0x20000000
do_compile:append() {
    sed -i '1i setenv initrd_high 0x20000000\nsetenv fdt_high 0x20000000' "${WORKDIR}/boot.cmd"
    mkimage -A arm -T script -C none -n "Boot script" -d "${WORKDIR}/boot.cmd" boot.scr
    mkimage -A arm -T script -C none -n "PACMAN TFTP boot" -d "${WORKDIR}/boot.cmd.tftp" boot.scr.uimg
}

do_deploy:append() {
    install -m 0644 boot.scr.uimg ${DEPLOYDIR}/boot.scr.uimg
}
