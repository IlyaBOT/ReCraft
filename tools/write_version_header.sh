#!/bin/sh
# Plain POSIX shell: also usable by Snow Leopard's system make/toolchain.
set -eu
version=$(cat "$1")
case "$version" in
    ''|*[!A-Za-z0-9.+-]*) echo "Invalid VERSION" >&2; exit 1 ;;
esac
mkdir -p "$(dirname "$2")"
stamp=$(date -u '+%Y-%m-%d %H:%M UTC')
revision=$(cd "$(dirname "$1")" && git rev-parse --short=9 HEAD 2>/dev/null) || revision=source
printf '#ifndef RECRAFT_VERSION_H\n#define RECRAFT_VERSION_H\n#define RECRAFT_VERSION "%s"\n#define RECRAFT_BUILD_DATE "%s"\n#define RECRAFT_BUILD_REVISION "%s"\n#endif\n' "$version" "$stamp" "$revision" > "$2"
