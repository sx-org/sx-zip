#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
trap 'rm -f /tmp/miniz-zlib-lifecycle.bin /tmp/sx-zlib-lifecycle.bin /tmp/c-miniz-zlib-lifecycle' EXIT

clang -std=c99 -O2 -Itests -I"$MINIZ_SRC" \
    tests/zlib_lifecycle_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_zip.c" \
    "$MINIZ_SRC/miniz_tinfl.c" "$MINIZ_SRC/miniz_tdef.c" \
    -o /tmp/c-miniz-zlib-lifecycle
/tmp/c-miniz-zlib-lifecycle
if [[ -n "${SX_OPT:-}" ]]; then
    "$SX_BIN" run tests/zlib_lifecycle.sx --opt "$SX_OPT"
else
    "$SX_BIN" run tests/zlib_lifecycle.sx
fi
cmp /tmp/miniz-zlib-lifecycle.bin /tmp/sx-zlib-lifecycle.bin
echo "zlib deflate/inflate lifecycle status oracle ok"
