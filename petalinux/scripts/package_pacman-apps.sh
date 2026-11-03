#! /bin/bash
#
#  package_pacman-apps.sh
#
# Package the pacman-apps rpm as a tarball, for software-only updates of an installed PACMAN.
# Run in the project directory (full or ramdisk) after build.sh or build_pacman-apps.sh.
# Writes e.g. pacman-apps-v3_6_0.tar.gz (release build) or pacman-apps-v3_6_1-candidate.tar.gz.
#
# On the PACMAN:
#
#   $ tar -xzf pacman-apps-v3_6_0.tar.gz -C / && /etc/init.d/pacman_server restart
#

set -o pipefail

RPM=$(ls -t build/tmp/deploy/rpm/cortexa*_neon/pacman-apps-1.0-r*.cortexa*_neon.rpm 2>/dev/null \
  | head -1)
[ -n "$RPM" ] || { echo "no pacman-apps rpm in build/tmp/deploy:  build first" >&2; exit 1; }

VH=../pkg/pacman-apps/files/src/drivers-common/version.h
[ -f $VH ] || { echo "no $VH:  run in the project directory" >&2; exit 1; }
[ $VH -nt "$RPM" ] && echo "WARNING:  $VH is newer than $RPM:  rebuild?" >&2
v(){ awk -v k=PACMAN_SOFTWARE_$1 '$1 == "#define" && $2 == k {print $3}' $VH; }
VERSION=v$(v MAJOR)_$(v MINOR)_$(v PATCH)
[ "$(v RELEASE)" = 1 ] || VERSION=$VERSION-candidate
TAR=pacman-apps-$VERSION.tar.gz

# Root-owned entries, no "./" entry (it would reset the owner and mode of / on the PACMAN), and the
# same tarball from the same rpm.
TMP=$(mktemp -d) || exit 1
trap 'rm -rf -- "$TMP"' EXIT
(cd "$TMP" && rpm2cpio "$OLDPWD/$RPM" | cpio -idm --quiet) || exit 1
find "$TMP" -mindepth 1 -type d -exec touch -r "$RPM" {} + || exit 1
tar -C "$TMP" --owner=0 --group=0 --numeric-owner --sort=name -cf - $(ls -A "$TMP") \
  | gzip -n > $TAR || exit 1

echo "$RPM -> $TAR"
sha256sum $TAR
