#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# tools/distro (README.md): the disk image the 8C2's firmware boots over
# AoE, from build.sh's OUT: a DOS partition table, partition 1 ext2
# (boot.conf, zImage, initrd.img: what the firmware reads, as on the
# 8C2's own /boot), partition 2 ext4 (OUT/root).  No root needed: the
# file systems are filled by mke2fs -d inside a user namespace, so every
# file is root's.
#
#   mkimage.sh OUT [--boot-mib N]
{
set -e
D=$(cd "$(dirname "$0")" && pwd)
out=$(cd "$1" && pwd); shift
bootmib=256
[ "$1" = --boot-mib ] && { bootmib=$2; shift 2; }
for f in zImage initrd.img root; do
	[ -e "$out/$f" ] || { echo "mkimage.sh: no $out/$f" >&2; exit 1; }
done
W=$out/image-work
rm -rf "$W"; mkdir -p "$W/boot"
cp "$D/boot.conf" "$out/zImage" "$out/initrd.img" "$W/boot/"
rootmib=$(( $(du -sm "$out/root" | cut -f1) * 3 / 2 + 512 ))
asroot() { bwrap --dev-bind / / --unshare-user --uid 0 --gid 0 "$@"; }
asroot mke2fs -q -t ext2 -L relcc2-boot -d "$W/boot" "$W/boot.ext2" "${bootmib}M"
asroot mke2fs -q -t ext4 -L relcc2-root -d "$out/root" "$W/root.ext4" "${rootmib}M"
img=$out/distro.img
rm -f "$img"
truncate -s $(( (1 + bootmib + rootmib) * 1024 * 1024 )) "$img"
sfdisk -q "$img" <<SF
label: dos
start=1MiB, size=${bootmib}MiB, type=83, bootable
start=$(( 1 + bootmib ))MiB, type=83
SF
dd if="$W/boot.ext2" of="$img" bs=1M seek=1 conv=notrunc status=none
dd if="$W/root.ext4" of="$img" bs=1M seek=$(( 1 + bootmib )) conv=notrunc status=none
rm -rf "$W"
sfdisk -l "$img" | tail -n 3
ls -l "$img"
}; exit
