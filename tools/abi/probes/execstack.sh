#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Executable stacks, the GNU-stack note and nested functions
# (spec/abi/elf.md, spec/abi/source-compat.md). Compiles, assembles and
# links only; nothing is run.
#   1. lcc 1.29.16 and 1.31.05 (v3, v5.8c2, v6.2c3): is there a
#      .note.GNU-stack in the assembly and the object, and which
#      PT_GNU_STACK do an executable, a shared library and a static
#      executable get, also with -z execstack / -z noexecstack and
#      -Wa,--execstack / -Wa,--noexecstack
#   2. MCST gas and ld: objects without the note, with a "" note and with
#      an "x" note, alone and mixed; ld -r; ld's warnings; and what
#      Unipro's clang emits, if toolchains/unipro is installed
#   3. GNU C nested functions in lcc's C front end; lfortran's internal
#      procedures passed as arguments (the Fortran form of the same thing)
#   4. __builtin___clear_cache, and what libgcc_s exports for trampolines
#   5. MCST libffi's closure trampoline (execstack/ffitramp.s) disassembled,
#      and the same trampoline as gas text (execstack/tramp-text.s)
#
# Usage: tools/abi/probes/execstack.sh
#        tools/abi/probes/execstack.sh --remote [USER@]HOST [--ssh-port N]
# Needs the lcc 1.29.16 cross compilers in /opt/mcst and the 1.31.05 ones
# in /opt/mcst or as toolchains/lcc-1.31.05-e2k-<target> (run under
# bwrap, as tools/setup/fetch-lcc.sh installs them). Output and scratch
# go to toolchains/execstack/.
# With --remote it only counts PT_GNU_STACK in the target system's own
# ELF files (/bin, /usr/bin, /lib64, /usr/lib64, /usr/libexec) with the
# machine's own readelf, and prints the personality and the [stack] and
# [heap] permissions of the system's cat (a program without PT_GNU_STACK)
# from /proc/self; over SSH, it reads files and writes nothing there.
# It prints to stdout; the runs elf.md cites (4C, 8C2, 2C3, 2026-10-12)
# were saved as toolchains/execstack/remote-{4c,8c2,2c3}.txt.

root=$(cd "$(dirname "$0")/../../.." && pwd)

remote=
port=
while [ $# -gt 0 ]; do
	case $1 in
	--remote) remote=$2; shift 2 ;;
	--ssh-port) port=$2; shift 2 ;;
	*) echo "unknown argument $1" >&2; exit 2 ;;
	esac
done
if [ -n "$remote" ]; then
	"$root/tools/setup/e2k-ssh" ${port:+-p "$port"} "$remote" sh -s <<'EOF_REMOTE'
for d in /bin /usr/bin /lib64 /usr/lib64 /usr/libexec; do
	[ -d "$d" ] || continue
	if [ -L "$d" ]; then
		echo "$d: link to $(readlink "$d")"
		continue
	fi
	n=0 none=0 rw=0 rwe=0
	for f in $(find "$d" -maxdepth 1 -type f); do
		[ "$(head -c4 "$f" 2>/dev/null | od -An -c | tr -d ' ')" = '177ELF' ] ||
			continue
		n=$((n + 1))
		s=$(readelf -lW "$f" 2>/dev/null | awk '/GNU_STACK/ { print $7 }')
		case $s in
		'') none=$((none + 1)) ;;
		RWE) rwe=$((rwe + 1)); echo "  RWE $f" ;;
		RW) rw=$((rw + 1)); echo "  RW $f" ;;
		*) echo "  $s $f" ;;
		esac
	done
	echo "$d: $n ELF files: no PT_GNU_STACK $none, RW $rw, RWE $rwe"
done
c=$(command -v cat)
echo "kernel $(uname -r); $c: $(readelf -lW "$c" | grep -c GNU_STACK) PT_GNU_STACK"
echo "personality of cat: $(cat /proc/self/personality)"
# cat opens /proc/self/maps, so these are cat's own mappings (grep's
# would be those of grep).
cat /proc/self/maps | grep -E '\[(stack|heap)\]' | awk '{ print "  " $2, $6 }'
EOF_REMOTE
	exit
fi
src=$root/tools/abi/probes/execstack
out=$root/toolchains/execstack
mkdir -p "$out"
cd "$out" || exit 1

bu=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/binutils/bin
readelf=$bu/e2k-linux-readelf
objdump=$bu/e2k-linux-objdump
nm=$bu/e2k-linux-nm

# Run TOOL (lcc, lfortran) of lcc VERSION for TARGET.
tool() {
	_d=lcc-$1.e2k-$2.linux-6.1
	_w=$root/toolchains/lcc-$1-e2k-$2/opt/mcst/$_d
	_n=$3
	shift 3
	if [ -x "/opt/mcst/$_d/bin/$_n" ]; then
		"/opt/mcst/$_d/bin/$_n" "$@"
	elif [ -d "$_w" ]; then
		bwrap --dev-bind / / --tmpfs /opt --ro-bind "$_w" "/opt/mcst/$_d" \
			--chdir "$PWD" "/opt/mcst/$_d/bin/$_n" "$@"
	else
		echo "no $_d" >&2
		return 127
	fi
}

# PT_GNU_STACK of a linked file: its flags, or "none".
stack() {
	s=$($readelf -lW "$1" 2>/dev/null | awk '/GNU_STACK/ { print $7 }')
	echo "${s:-none}"
}

# .note.GNU-stack of an object: "none", or the section's objdump flags.
note() {
	s=$($objdump -h "$1" 2>/dev/null |
		awk '/\.note\.GNU-stack/ { getline; sub(/^ +/, ""); print }')
	echo "${s:-none}"
}

# First line of a compiler's error output, or "accepted".
verdict() {
	e=$(grep -m1 -A1 -E 'error|Error' "$1" | tr -s ' \t\n' ' ' |
		sed 's|"[^"]*/\([^/"]*\)"|"\1"|')
	echo "${e:-accepted}"
}

echo "== 1. lcc: the note and PT_GNU_STACK"
for ver in 1.29.16 1.31.05; do
	for t in v3 v5.8c2 v6.2c3; do
		p=$ver-$t
		tool "$ver" "$t" lcc -O2 -S "$src/plain.c" -o "plain-$p.s" ||
			continue
		tool "$ver" "$t" lcc -O2 -c "$src/plain.c" -o "plain-$p.o"
		tool "$ver" "$t" lcc -O2 -c "$src/helper.c" -o "helper-$p.o"
		tool "$ver" "$t" lcc -O2 -fPIC -c "$src/helper.c" -o "helperS-$p.o"
		tool "$ver" "$t" lcc "plain-$p.o" "helper-$p.o" -o "exe-$p"
		tool "$ver" "$t" lcc -shared "helperS-$p.o" -o "lib-$p.so"
		tool "$ver" "$t" lcc -static "plain-$p.o" "helper-$p.o" -o "static-$p"
		tool "$ver" "$t" lcc -Wl,-z,execstack "plain-$p.o" "helper-$p.o" -o "x-$p"
		tool "$ver" "$t" lcc -Wl,-z,noexecstack "plain-$p.o" "helper-$p.o" -o "nx-$p"
		tool "$ver" "$t" lcc -shared -Wl,-z,noexecstack "helperS-$p.o" -o "libnx-$p.so"
		tool "$ver" "$t" lcc -O2 -Wa,--noexecstack -c "$src/plain.c" -o "wanx-$p.o"
		tool "$ver" "$t" lcc -O2 -Wa,--execstack -c "$src/plain.c" -o "wax-$p.o"
		tool "$ver" "$t" lcc "wanx-$p.o" "helper-$p.o" -o "wanx-$p" 2>/dev/null
		tool "$ver" "$t" lcc "wax-$p.o" "helper-$p.o" -o "wax-$p" 2>"wax-$p.ld"
		if grep -q 'GNU-stack' "plain-$p.s"; then a=yes; else a=no; fi
		echo "lcc $ver $t: note in .s $a, in .o: $(note "plain-$p.o")"
		echo "  executable $(stack "exe-$p"), shared $(stack "lib-$p.so"), static $(stack "static-$p")"
		echo "  -z execstack $(stack "x-$p"), -z noexecstack $(stack "nx-$p"), shared -z noexecstack $(stack "libnx-$p.so")"
		echo "  -Wa,--noexecstack: .o $(note "wanx-$p.o"), executable $(stack "wanx-$p")"
		echo "  -Wa,--execstack: .o $(note "wax-$p.o"), executable $(stack "wax-$p")"
		sed 's/^/    ld: /' "wax-$p.ld"
	done
done

echo
echo "== 1b. lcc 1.29.16 v3 sysroot: e2k ELF files with a note or PT_GNU_STACK"
sys=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1
n=0 y=0
for f in $(find "$sys/fs" "$sys/lcc-home" -type f \
		\( -name '*.so*' -o -name '*.o' -o -name '*.a' -o -perm -u+x \)); do
	case $($readelf -h "$f" 2>/dev/null | awk -F: '/Machine/ { print $2 }') in
	*Elbrus*) ;;
	*) continue ;;
	esac
	n=$((n + 1))
	case $f in
	*.o|*.a) s=$($readelf -SW "$f" 2>/dev/null | grep -c 'GNU-stack') ;;
	*) s=$($readelf -lW "$f" 2>/dev/null | grep -c 'GNU_STACK') ;;
	esac
	if [ "$s" != 0 ]; then
		y=$((y + 1))
		echo "  has it: $f"
	fi
done
echo "  $n e2k ELF files, $y with a note or PT_GNU_STACK"
# The ASan runtime's one object with a note decides an ASan program's segment.
tool 1.29.16 v3 lcc -fsanitize=address -O2 "$src/plain.c" "$src/helper.c" \
	-o asan-exe 2>/dev/null
echo "  lcc -fsanitize=address executable: $(stack asan-exe)"
# The driver passes -z to ld itself.
tool 1.29.16 v3 lcc -z noexecstack plain-1.29.16-v3.o helper-1.29.16-v3.o -o znx
echo "  lcc -z noexecstack (no -Wl): $(stack znx)"

echo
echo "== 2. MCST gas and ld"
p=1.29.16-v3
cp "plain-$p.s" none.s
{ cat "plain-$p.s"; echo '	.section .note.GNU-stack,"",@progbits'; } > rw.s
{ cat "plain-$p.s"; echo '	.section .note.GNU-stack,"x",@progbits'; } > x.s
for k in none rw x; do
	$bu/e2k-linux-as -mcpu=elbrus-v3 -mptr64 "$k.s" -o "gas-$k.o"
	echo "gas $k.s: .o $(note "gas-$k.o")"
done
tool 1.29.16 v3 lcc -O2 -S "$src/helper.c" -o helper.s
{ cat helper.s; echo '	.section .note.GNU-stack,"",@progbits'; } > helper-rw.s
$bu/e2k-linux-as -mcpu=elbrus-v3 -mptr64 helper.s -o gas-helper-none.o
$bu/e2k-linux-as -mcpu=elbrus-v3 -mptr64 helper-rw.s -o gas-helper-rw.o
link() {
	name=$1
	shift
	tool 1.29.16 v3 lcc "$@" -o "$name" 2>"$name.ld"
	echo "link $*: $(stack "$name")"
	sed 's/^/    ld: /' "$name.ld"
}
link mix-none gas-none.o gas-helper-none.o
link mix-rw-none gas-rw.o gas-helper-none.o
link mix-none-rw gas-none.o gas-helper-rw.o
link mix-x-none gas-x.o gas-helper-none.o
link mix-x-rw gas-x.o gas-helper-rw.o
link mix-x-noexec -Wl,-z,noexecstack gas-x.o gas-helper-rw.o
$bu/e2k-linux-ld -r gas-x.o gas-helper-rw.o -o r-x-rw.o 2>r-x-rw.ld
echo "ld -r gas-x.o gas-helper-rw.o: .o $(note r-x-rw.o)"
sed 's/^/    ld: /' r-x-rw.ld
$bu/e2k-linux-ld -r gas-none.o gas-helper-none.o -o r-none.o
echo "ld -r gas-none.o gas-helper-none.o: .o $(note r-none.o)"
clang=$root/toolchains/unipro/bin/clang
if [ -x "$clang" ]; then
	"$clang" --target=e2k-linux-gnu -march=elbrus-v3 -O2 -S \
		"$src/plain.c" -o plain-unipro.s 2>/dev/null
	echo "Unipro clang: $(grep 'GNU-stack' plain-unipro.s | tr -s ' \t' ' ')"
fi

echo
echo "== 3. Nested functions"
for ver in 1.29.16 1.31.05; do
	for t in v3 v5.8c2 v6.2c3; do
		for c in nested-call nested-addr nested-noref; do
			tool "$ver" "$t" lcc -O2 -S "$src/$c.c" -o "$c-$ver-$t.s" \
				>"$c-$ver-$t.err" 2>&1
			echo "lcc $ver $t $c.c: $(verdict "$c-$ver-$t.err")"
		done
	done
done
for ver in 1.29.16 1.31.05; do
	for o in -fnested-functions -ftrampolines -ftrampoline-impl=heap; do
		tool "$ver" v3 lcc -O2 "$o" -S "$src/plain.c" -o /dev/null \
			>"opt-$ver.err" 2>&1
		echo "lcc $ver $o: $(verdict "opt-$ver.err")"
	done
done
if [ -x "$clang" ]; then
	"$clang" --target=e2k-linux-gnu -march=elbrus-v3 -O2 -S \
		"$src/nested-addr.c" -o /dev/null >nested-unipro.err 2>&1
	echo "Unipro clang nested-addr.c: $(verdict nested-unipro.err)"
fi
for ver in 1.29.16 1.31.05; do
	t=v3
	tool "$ver" "$t" lfortran -O2 -S "$src/internal.f90" \
		-o "internal-$ver.s" >"internal-$ver.err" 2>&1
	echo "lfortran $ver $t internal.f90: $(verdict "internal-$ver.err")"
	# How the address of the internal procedure is passed, and where the
	# parent's frame goes.
	grep -n -E 'thunk|_ext_|^\s*\.section|^\s*\.local' "internal-$ver.s" |
		sed 's/^/    /'
	tool "$ver" "$t" lfortran -O2 -frecursive -S "$src/internal.f90" \
		-o "internal-rec-flag-$ver.s" >/dev/null 2>&1
	echo "  with -frecursive: $(grep -c '_ext_' "internal-rec-flag-$ver.s") lines use the static slot"
	tool "$ver" "$t" lfortran -O2 -fopenmp -S "$src/internal.f90" \
		-o "internal-omp-$ver.s" >/dev/null 2>&1
	echo "  with -fopenmp the slot is in: $(grep -B5 '^__outer__add__ext_1:' "internal-omp-$ver.s" | grep -E '\.section|\.bss|\.tbss' | tr -s ' \t' ' ')"
	tool "$ver" "$t" lfortran -O2 -S "$src/internal-rec.f90" \
		-o "internal-rec-$ver.s" >"internal-rec-$ver.err" 2>&1
	echo "lfortran $ver $t internal-rec.f90: $(verdict "internal-rec-$ver.err")"
done

echo
echo "== 4. __builtin___clear_cache and libgcc"
for ver in 1.29.16 1.31.05; do
	tool "$ver" v3 lcc -O2 -S "$src/clearcache.c" -o "clearcache-$ver.s"
	echo "lcc $ver: $(grep -E 'disp|call' "clearcache-$ver.s" | tr -s ' \t' ' ' | tr '\n' ';')"
done
lib=$sys/lcc-home/1.29.16/e2k-v3-linux.cross/lib64
echo "libgcc_s.so: $($nm -D "$lib/libgcc_s.so" | grep -E 'clear_cache|nested_func|enable_execute' | tr -s ' ' | tr '\n' ';')"
echo "libgcc.a: $($nm "$lib/libgcc.a" 2>/dev/null | grep -E 'clear_cache|nested_func|enable_execute' | tr -s ' ' | tr '\n' ';')"

echo
echo "== 5. MCST libffi's closure trampoline"
$bu/e2k-linux-as -mcpu=elbrus-v3 -mptr64 "$src/ffitramp.s" -o ffitramp.o
$objdump -d ffitramp.o | sed -n '/<tramp64>/,$p'
$bu/e2k-linux-as -mcpu=elbrus-v3 -mptr64 "$src/tramp-text.s" -o tramp-text.o
$bu/e2k-linux-objcopy -O binary -j .text ffitramp.o ffitramp.bin
$bu/e2k-linux-objcopy -O binary -j .text tramp-text.o tramp-text.bin
if cmp -s -n 40 ffitramp.bin tramp-text.bin; then
	echo "tramp-text.s assembles to the same 40 bytes as libffi's words"
else
	echo "tramp-text.s differs from libffi's words:"
	cmp -l -n 40 ffitramp.bin tramp-text.bin
fi
