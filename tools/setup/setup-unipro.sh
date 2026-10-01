#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Unpack the Unipro LLVM 20 debs (x86_64 host, Elbrus target) into
# toolchains/unipro and build a stub libxml2.so.2 so the tools run without
# libxml2 (only the Windows manifest code uses it).
#
# Usage: tools/setup/setup-unipro.sh [DEB_DIR]
#   DEB_DIR defaults to ./x86_64 (llvm20_*.deb, clang20_*.deb).
#
# Afterwards run toolchains/unipro/bin/clang, llc, ...
# (wrappers that set LD_LIBRARY_PATH).
set -e

root=$(cd "$(dirname "$0")/../.." && pwd)
debs=${1:-$root/x86_64}
out=$root/toolchains/unipro

# Without dpkg (e.g. on Gentoo) unpack by hand: a .deb is an ar archive
# holding data.tar.*.
unpack() {
	if command -v dpkg-deb >/dev/null; then
		dpkg-deb -x "$1" "$2"
		return
	fi
	tmp=$(mktemp -d "$out/deb.XXXXXX")
	(cd "$tmp" && ar x "$1")
	tar -xf "$tmp"/data.tar.* -C "$2"
	rm -rf "$tmp"
}

debs=$(cd "$debs" && pwd)
rm -rf "$out"
mkdir -p "$out/pkg" "$out/stub" "$out/bin"
for deb in "$debs"/llvm20_*.deb "$debs"/clang20_*.deb; do
	unpack "$deb" "$out/pkg"
done
llvm=$out/pkg/usr/lib/llvm-20

syms="xmlAddChild xmlCopyNamespace xmlDocDumpFormatMemoryEnc xmlDocGetRootElement
xmlDocSetRootElement xmlFreeDoc xmlFreeNode xmlFreeNs xmlNewDoc xmlNewNs xmlNewProp
xmlSetGenericErrorFunc xmlStrdup xmlUnlinkNode"
{
	for s in $syms; do echo "void $s(void) {}"; done
	echo "void xmlReadMemory(void) {}"
	echo "void (*xmlFree)(void *) = 0;"
} > "$out/stub/stub.c"
{
	printf 'LIBXML2_2.4.30 { global: '
	for s in $syms; do printf '%s; ' "$s"; done
	echo 'xmlFree; local: *; };'
	echo 'LIBXML2_2.6.0 { global: xmlReadMemory; } LIBXML2_2.4.30;'
} > "$out/stub/ver.map"
cc -shared -fPIC -o "$out/stub/libxml2.so.2" -Wl,--version-script="$out/stub/ver.map" \
	-Wl,-soname,libxml2.so.2 "$out/stub/stub.c"

for tool in clang llc llvm-mc llvm-objdump llvm-readelf llvm-dwarfdump opt; do
	cat > "$out/bin/$tool" <<EOF
#!/bin/sh
LD_LIBRARY_PATH="$out/stub:$llvm/lib\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}" exec "$llvm/bin/$tool" "\$@"
EOF
	chmod +x "$out/bin/$tool"
done

"$out/bin/llc" --version | sed -n '1p'
