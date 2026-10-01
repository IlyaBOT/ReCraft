#!/bin/sh
# Plain POSIX shell: also usable by Snow Leopard's system make/toolchain.
set -eu
version=$(cat "$1")
case "$version" in
    ''|*[!A-Za-z0-9.+-]*) echo "Invalid VERSION" >&2; exit 1 ;;
esac
mkdir -p "$(dirname "$2")"
printf '#ifndef RECRAFT_VERSION_H\n#define RECRAFT_VERSION_H\n#define RECRAFT_VERSION "%s"\n#endif\n' "$version" > "$2"
