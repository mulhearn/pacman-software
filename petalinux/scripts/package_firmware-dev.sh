#! /bin/bash
#
#  package_firmware-dev.sh
#
# Package the BOOT.BIN inputs for firmware developers:  the FSBL, U-Boot, the device tree and the
# bitstream of this build, with the bootgen.bif and README.txt from src/firmware-dev.  Run in the
# full project directory after package.sh.  Writes e.g. firmware-dev-v3_6_0.tar.gz (release build)
# or firmware-dev-v3_6_1-candidate.tar.gz, unpacking to firmware-dev/.
#
# If bootgen is on the PATH (Vivado 2023.2), the packaged inputs are first checked to rebuild
# images/linux/BOOT.BIN bitwise.
#

set -o pipefail

VH=../pkg/pacman-apps/files/src/drivers-common/version.h
[ -f $VH ] || { echo "no $VH:  run in the project directory" >&2; exit 1; }
v(){ awk -v k=PACMAN_SOFTWARE_$1 '$1 == "#define" && $2 == k {print $3}' $VH; }
VERSION=v$(v MAJOR)_$(v MINOR)_$(v PATCH)
[ "$(v RELEASE)" = 1 ] || VERSION=$VERSION-candidate
TAR=firmware-dev-$VERSION.tar.gz

TMP=$(mktemp -d) || exit 1
trap 'rm -rf -- "$TMP"' EXIT
D=$TMP/firmware-dev
mkdir $D || exit 1
cp -p ../../products/pacman_fsbl.elf $D/pacman_fsbl.elf || exit 1
cp -p project-spec/hw-description/pacman.bit $D/pacman.bit || exit 1
cp -p images/linux/u-boot.elf $D/u-boot.elf || exit 1
cp -p images/linux/system.dtb $D/system.dtb || exit 1
cp -p ../src/firmware-dev/bootgen.bif ../src/firmware-dev/README.txt $D/ || exit 1

if command -v bootgen > /dev/null; then
  (cd $D && bootgen -image bootgen.bif -arch zynq -o $TMP/BOOT.BIN > $TMP/bootgen.log 2>&1) \
    || { cat $TMP/bootgen.log >&2; exit 1; }
  cmp -s $TMP/BOOT.BIN images/linux/BOOT.BIN \
    || { echo "the packaged inputs do not rebuild images/linux/BOOT.BIN" >&2; exit 1; }
  echo "checked:  bootgen rebuilds images/linux/BOOT.BIN bitwise"
else
  echo "bootgen not on the PATH:  BOOT.BIN check skipped"
fi

# Root-owned, modes and times fixed (the bitstream's time), the same tarball from the same build.
tar -C $TMP --owner=0 --group=0 --numeric-owner --mode=a+rX,go-w --sort=name \
  --mtime=@$(stat -c %Y $D/pacman.bit) -cf - firmware-dev | gzip -n > $TAR || exit 1

echo "-> $TAR"
sha256sum $TAR
