#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
CC="${CC:-clang}"
C_ORACLE=/tmp/sx-miniz-flush-oracle
trap 'rm -f "$C_ORACLE" /tmp/sx-miniz-flush-{sx,c}-{sync,full}.deflate /tmp/sx-miniz-flush-{sx,c}-zlib-{none,partial,sync,full}.deflate' EXIT

if [[ -n "${SX_OPT:-}" ]]; then
    "$SX_BIN" run tests/flush.sx --opt "$SX_OPT"
else
    "$SX_BIN" run tests/flush.sx
fi

if command -v "$CC" >/dev/null 2>&1 &&
   [[ -f "$MINIZ_SRC/miniz.c" && -f "$MINIZ_SRC/miniz_tdef.c" ]]; then
    "$CC" -std=c99 -O3 -DNDEBUG \
        -DMINIZ_USE_UNALIGNED_LOADS_AND_STORES=1 \
        -DMINIZ_LITTLE_ENDIAN=1 -DMINIZ_HAS_64BIT_REGISTERS=1 \
        -Itests -I"$MINIZ_SRC" tests/flush_miniz.c \
        "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" \
        -o "$C_ORACLE"
    "$C_ORACLE"
    cmp /tmp/sx-miniz-flush-sx-sync.deflate /tmp/sx-miniz-flush-c-sync.deflate
    cmp /tmp/sx-miniz-flush-sx-full.deflate /tmp/sx-miniz-flush-c-full.deflate
    cmp /tmp/sx-miniz-flush-sx-zlib-none.deflate /tmp/sx-miniz-flush-c-zlib-none.deflate
    cmp /tmp/sx-miniz-flush-sx-zlib-partial.deflate /tmp/sx-miniz-flush-c-zlib-partial.deflate
    cmp /tmp/sx-miniz-flush-sx-zlib-sync.deflate /tmp/sx-miniz-flush-c-zlib-sync.deflate
    cmp /tmp/sx-miniz-flush-sx-zlib-full.deflate /tmp/sx-miniz-flush-c-zlib-full.deflate
    echo "flush byte identity ok"
else
    echo "flush C oracle skipped (compiler or MINIZ_SRC unavailable)"
fi
