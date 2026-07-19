#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
CC="${CC:-clang}"
C_ORACLE=/tmp/sx-miniz-strategy-oracle
C_PORTABLE=/tmp/sx-miniz-strategy-portable-oracle
trap 'rm -f "$C_ORACLE" "$C_PORTABLE" /tmp/sx-miniz-strategy-{sx,c,c-portable}-*.zlib' EXIT

if [[ -n "${SX_OPT:-}" ]]; then
    "$SX_BIN" run tests/strategy.sx --opt "$SX_OPT"
else
    "$SX_BIN" run tests/strategy.sx
fi

if command -v "$CC" >/dev/null 2>&1 &&
   [[ -f "$MINIZ_SRC/miniz.c" && -f "$MINIZ_SRC/miniz_tdef.c" ]]; then
    "$CC" -std=c99 -O3 -DNDEBUG \
        -DMINIZ_USE_UNALIGNED_LOADS_AND_STORES=1 \
        -DMINIZ_LITTLE_ENDIAN=1 -DMINIZ_HAS_64BIT_REGISTERS=1 \
        -Itests -I"$MINIZ_SRC" tests/strategy_miniz.c \
        "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" \
        -o "$C_ORACLE"
    "$C_ORACLE"
    "$CC" -std=c99 -O3 -DNDEBUG -DORACLE_PREFIX='"c-portable"' \
        -Itests -I"$MINIZ_SRC" tests/strategy_miniz.c \
        "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" \
        -o "$C_PORTABLE"
    "$C_PORTABLE"
    for sx_path in /tmp/sx-miniz-strategy-sx-*.zlib; do
        c_path=${sx_path/-sx-/-c-}
        cmp "$sx_path" "$c_path"
    done
    echo "strategy byte identity ok"
else
    echo "strategy C oracle skipped (compiler or MINIZ_SRC unavailable)"
fi
