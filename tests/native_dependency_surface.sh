#!/usr/bin/env bash
set -euo pipefail

imports=$(mktemp)
libraries=$(mktemp)
trap 'rm -f "$imports" "$libraries"' EXIT

rg -o '#import "[^"]+"' miniz.sx modules/miniz -g '*.sx' |
    sed 's/.*#import "//; s/"$//' | LC_ALL=C sort -u > "$imports"

while IFS= read -r import; do
    case "$import" in
        ../../miniz.sx|modules/miniz/windows_fs.sx|modules/std/core.sx|modules/std/fs.sx|modules/std/list.sx)
            ;;
        *)
            echo "unexpected native miniz import: $import" >&2
            exit 1
            ;;
    esac
done < "$imports"

rg -o '#library "[^"]+"' miniz.sx modules/miniz -g '*.sx' |
    sed 's/.*#library "//; s/"$//' | LC_ALL=C sort -u > "$libraries"
if [[ $(wc -l < "$libraries" | tr -d ' ') != 2 ]] ||
   ! rg -qx 'c' "$libraries" || ! rg -qx 'kernel32' "$libraries"; then
    echo "unexpected native miniz library dependency surface:" >&2
    cat "$libraries" >&2
    exit 1
fi

if rg -n '#import "[^"]+\.(c|h|o|a|so|dylib|dll)"|#library "(z|zlib|miniz)"' \
    miniz.sx modules/miniz -g '*.sx'; then
    echo "native miniz source imports an external compression implementation" >&2
    exit 1
fi

echo "native dependency surface: sx stdlib plus target libc/kernel32 only; external compression libraries=0"
