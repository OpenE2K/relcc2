#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Download an MCST lcc cross compiler from the archive at setwd.ws/sp and
# install it under toolchains/lcc-VERSION-TARGET, with a wrapper script.
#
# Usage: tools/setup/fetch-lcc.sh [VERSION] [TARGET]
#   VERSION defaults to 1.29.16, TARGET to e2k-v5 (others: e2k-v3, e2k-v4,
#   e2k-v6, or CPU-specific ones such as e2k-v5.8c2; see the archive).
#
# lcc looks for its components under a fixed /opt/mcst/lcc-... path. The
# wrapper toolchains/lcc-VERSION-TARGET/lcc uses bwrap to mount the unpacked
# tree there for lcc's own process only, so no root access is needed.
# Use it like lcc itself: it compiles, assembles and links against the
# bundled sysroot (fs/).
set -e

version=${1:-1.29.16}
target=${2:-e2k-v5}
root=$(cd "$(dirname "$0")/../.." && pwd)
series=$(echo "$version" | cut -d. -f1-2)
name=lcc-$version.$target.linux-6.1
url=https://setwd.ws/sp/$series/$version/cross-sp-$version.$target.linux-6.1_64.tar.xz
out=$root/toolchains/lcc-$version-$target

command -v bwrap >/dev/null || { echo "bwrap (bubblewrap) is required" >&2; exit 1; }

rm -rf "$out"
mkdir -p "$out"
curl -fL --progress-bar -o "$out/lcc.tar.xz" "$url"
tar -xJf "$out/lcc.tar.xz" -C "$out"
rm "$out/lcc.tar.xz"
tree=$out/opt/mcst/$name
test -x "$tree/bin/lcc" || { echo "unexpected archive layout" >&2; exit 1; }

for tool in lcc l++ ldis; do
	cat > "$out/$tool" <<EOF
#!/bin/sh
exec bwrap --dev-bind / / --tmpfs /opt --ro-bind "$tree" "/opt/mcst/$name" \\
	--chdir "\$PWD" "/opt/mcst/$name/bin/$tool" "\$@"
EOF
	chmod +x "$out/$tool"
done

"$out/lcc" --version | sed -n '1p'
