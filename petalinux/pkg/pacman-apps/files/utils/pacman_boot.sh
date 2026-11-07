#!/bin/sh
#
# pacman_boot:  PACMAN tasks at the end of each boot.  Init script (/etc/init.d/pacman_boot, started last
# in runlevels 2-5):  "start" runs the tasks in the background, so the boot is not delayed;  "stop" does
# nothing.  Without an argument the tasks run in the foreground (e.g. by hand:  /etc/init.d/pacman_boot).
#
# Tasks:
#   last_boot   where this PACMAN came up:  the MAC and IPv4 address of eth0 (waiting up to 60 s for
#               DHCP) in last-boot.txt on the SD card's FAT boot partition, readable on any computer.
#
case "$1" in
    start) "$0" > /dev/null 2>&1 &  exit 0 ;;
    stop)  exit 0 ;;
esac

BOOTDEV=/dev/mmcblk0p1   # the SD card's FAT boot partition
IF=eth0

# boot_partition_write <file>:  write stdin to <file> on the boot partition.  Mounted only for the
# write (if not mounted already), then synced and unmounted, so a power-off cannot leave it inconsistent.
boot_partition_write () {
    [ -b $BOOTDEV ] || return 0
    mnt=$(awk -v d=$BOOTDEV '$1 == d {print $2; exit}' /proc/mounts)
    own=""
    if [ -z "$mnt" ]; then
        mnt=/run/pacman_boot
        mkdir -p $mnt && mount -t vfat $BOOTDEV $mnt || return 0
        own=1
    fi
    cat > "$mnt/$1"
    sync
    [ -n "$own" ] && umount $mnt
    return 0
}

last_boot () {
    addr=""
    i=0
    while [ $i -lt 60 ]; do
        addr=$(ip -4 addr show dev $IF 2>/dev/null | awk '/inet /{print $2; exit}')
        [ -n "$addr" ] && break
        sleep 1
        i=$((i + 1))
    done
    [ -n "$addr" ] || addr="none (no DHCP address within 60 s)"
    {
        echo "PACMAN last boot (written by pacman_boot at each boot)"
        echo "host name:   $(hostname)"
        echo "$IF MAC:    $(cat /sys/class/net/$IF/address 2>/dev/null)"
        echo "$IF IPv4:   $addr"
        echo "board time:  $(date) (no real-time clock:  may be wrong)"
    } | boot_partition_write last-boot.txt
}

last_boot
exit 0
