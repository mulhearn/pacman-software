#
# This file is the pacman package recipe.
#

SUMMARY = "PACMAN petalinux applications"
SECTION = "PETALINUX/apps"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
DEPENDS = "zeromq i2c-tools"
INHIBIT_PACKAGE_STRIP = "1"

SRC_URI = " \
   file://src \
   file://include \
   file://Makefile \
   file://pylib \
   file://utils \
   file://tests \
   file://etc \
"

S = "${WORKDIR}"
homedir = "/home/root"

inherit python3-dir

#inherit update-rc.d

do_compile() {
	oe_runmake
}

do_install() {
	# Install bin directory:
	install -d ${D}${bindir}

	# Install app binaries (strip .elf extension)
	for elf in $(find ${S}/bin -name "*.elf"); do
	    app=$(basename "${elf}" .elf)
	    install -m 0755 "${elf}" "${D}${bindir}/${app}"
	done

	# Install home directory:
	install -d ${D}${homedir}

	# Install pylib
	install -d ${D}${homedir}/pylib
	install -m 0755 ${S}/pylib/* ${D}${homedir}/pylib/
	# pylib on Python's path in every context (login, ssh commands, services)
	install -d ${D}${PYTHON_SITEPACKAGES_DIR}
	echo "${homedir}/pylib" > ${D}${PYTHON_SITEPACKAGES_DIR}/pacman.pth

	# Install utility scripts
	install -d ${D}${homedir}/utils
	install -m 0755 ${S}/utils/* ${D}${homedir}/utils/

	# Install test scripts
	install -d ${D}${homedir}/tests
	install -m 0755 ${S}/tests/* ${D}${homedir}/tests/

	# Install init scripts:
	install -d ${D}${sysconfdir}/init.d
	install -m 0755 ${S}/utils/pacman_server.sh ${D}${sysconfdir}/init.d/pacman_server
	install -m 0755 ${S}/utils/pacman_gpio_init.sh ${D}${sysconfdir}/init.d/pacman_gpio_init
	# sshd settings
	install -d ${D}${sysconfdir}/ssh/sshd_config.d
	install -m 0644 ${S}/etc/sshd_config.d/pacman.conf ${D}${sysconfdir}/ssh/sshd_config.d/
	# expected by legacy users here as well:
	install -m 0755 ${S}/utils/pacman_server.sh ${D}${bindir}/pacman_server
	# embedded-Linux checkout of the Trenz module:
	install -m 0755 ${S}/utils/pacman_lnxcheck.sh ${D}${bindir}/pacman_lnxcheck

}


# run on target, post install:
pkg_postinst_ontarget:${PN} () {
    echo "Registering pacman init scripts..."

    if [ -x /etc/init.d/pacman_gpio_init ]; then
        update-rc.d pacman_gpio_init defaults
    fi
    if [ -x /etc/init.d/pacman_server ]; then
        update-rc.d pacman_server defaults
    fi
}

FILES:${PN} += "${sysconfdir}/*"
FILES:${PN} += "${homedir}/pylib/*"
FILES:${PN} += "${PYTHON_SITEPACKAGES_DIR}/pacman.pth"
FILES:${PN} += "${homedir}/utils/*"
FILES:${PN} += "${homedir}/tests/*"
FILES:${PN} += "${bindir}/*"

RDEPENDS:${PN} = "python3-core python3-pyzmq"
