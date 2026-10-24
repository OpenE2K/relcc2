# A small E2K system built by the PoC alone

This builds a bootable system in which every binary comes from the PoC
compiler. MCST's Linux and glibc (`OpenE2K`), binutils and gdb
(`OpenE2K/binutils-gdb`, `OpenE2K/gdb`) and libffi (`OpenE2K/libffi`)
are built from source with the usual upstream packages: busybox,
dropbear, ncurses, expat, GMP, MPFR and MPC. Nothing comes from lcc's
trees or from an installed Elbrus system. The system checks the kernel,
glibc and the compilers together on real hardware, and it is where the
native tests run: glibc's testsuite, the compat suite and
`tools/quadmath`.

It boots on the Elbrus-8C2 over AoE (ATA over Ethernet). The 8C2's
firmware reads a disk the workstation exports with `vblade`, as it reads
its own `/boot`. The serial console is the BMC's `ttyS1` (the AM85C30
port, which `config-mcst-e8c2` builds in) at 115200 baud, and the VGA
console is `tty1`.

## Building

```
# the cross on a sysroot of our own: glibc for the 8C2, then the cross
toolchains/poc06/c2.sh 8c2 toolchains/poc-cross/cross-V-8c2/gcc-e2k-V-8c2/bin \
    -j $(nproc) --disable-werror
S=toolchains/poc06/sysroot-8c2
cp -a toolchains/poc06/glibc-gcc-8c2 $S
cp -a toolchains/poc06/linux-headers/include/. $S/usr/include/
toolchains/poc-cross/cross-cpu.sh 8c2 --sysroot $S --rev REV -j $(nproc)

# the system: OUT/root, OUT/image.boot, OUT/initrd.img, OUT/distro.img
tools/distro/build.sh toolchains/poc-cross/cross-V-8c2-own/gcc-e2k-V-8c2
```

`build.sh` runs its steps in order (its header lists them) and resumes at
one with `--stage STEP`; each writes `OUT/logs/STEP.log`. No step needs
root: `mkimage.sh` fills the file systems with `mke2fs -d` inside a user
namespace (`bwrap --unshare-user --uid 0`), so the files in the image
belong to root.

The first build (2026-10-24, PoC `0.5+43`, the `cross-0.5+43-8c2-own`
cross on OpenE2K glibc 2.35-29.023 built for the 8C2): a root tree of
1.1 GB (9,217 files, 967 kernel modules), an initramfs of 6.9 MB
compressed (23 MB), `image.boot` of 67 MB and `distro.img` of 2.5 GB.
The `check` step's audit: no file names `/opt/mcst`, none carries lcc's
mark (`elbrus_optimizing_compiler_v...`, a weak symbol lcc puts in every
object; it writes no `.comment`), and all 5,967 ELF files' `.comment`
is `GCC: (relcc2 PoC 0.5+43 efd6f1c4975)`. Not booted yet.

## The image

| Part | What |
|---|---|
| partition 1, ext2 | `boot.conf`, `image.boot`, `initrd.img`: what the firmware reads, laid out as on the 8C2's own `/boot` |
| partition 2, ext4 | the whole system (`OUT/root`): the compilers, gdb, headers, the kernel's modules |
| `initrd.img` | busybox, glibc's shared libraries and dropbear: the system comes up from memory and takes SSH before anything is mounted |

`boot.conf` (the firmware's syntax: `label=`, `partition=0` for the
first partition, `image=`, `initrd=`, `cmdline=`) boots the initramfs
with `console=tty0 console=ttyS1,115200n8 rdinit=/sbin/init`. busybox
init (`rootfs/etc/inittab`) puts a root shell on both consoles, brings
up `eth1` at the 8C2's own address (`rootfs/etc/network.conf`) and
starts dropbear, which takes relcc2's key for root.

## Booting

On the workstation, on the 8C2's network (AoE is not routed):

```
sudo vblade 0 1 IFACE toolchains/distro/out/distro.img   # IFACE: enp34s0 here
```

The 8C2's firmware then boots shelf 0, slot 1 over AoE (the owner picks
it through the BMC). Its installed system and disks are not touched;
while ours runs, nothing else can use the machine. From here:

```
tools/setup/e2k-ssh -o UserKnownHostsFile=toolchains/distro/known_hosts \
    -o StrictHostKeyChecking=accept-new root@geofront.lan
```

(Its host key is new on every boot, and differs from the installed
system's: keep it out of the usual `known_hosts`.)

## Pitfalls met while building it

- **libtool relinks at install.** A shared library that depends on
  another of the same build (GMP's `libgmpxx` on `libgmp`) is relinked
  with `-L/usr/lib64`, the host's: "libc.so.6: file in wrong format".
  GMP, MPFR and MPC, which only gdb needs, are static, in a prefix of
  their own.
- **glibc's `libc.so` is a linker script.** It names `/lib64/libc.so.6`,
  which ld takes from the sysroot only when the script is in it; found by
  a `-L` into the system tree, it means the host's file. gdb links its
  dependencies from a tree without glibc (`build/depsroot`).
- **`lib-link.m4` searches `$prefix`.** Without `--with-libexpat-prefix`
  gdb's configure found the host's `/usr/lib64/libexpat.so`; with it,
  it hardcodes the build directory as an rpath unless `--disable-rpath`.
- **C23.** The readline that binutils and gdb carry declares `extern
  int tputs ();`, which C23 (GCC's default since 15) takes for no
  arguments; their C code is built with `-std=gnu17`.
