#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
SX_OPT="${SX_OPT:-}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
CC="${CC:-clang}"
C_ORACLE=/tmp/sx-miniz-core-surface-oracle
C_OUT=/tmp/miniz-core-surface.bin
SX_OUT=/tmp/sx-miniz-core-surface.bin
trap 'rm -f "$C_ORACLE" "$C_OUT" "$SX_OUT"' EXIT

"$CC" -std=c99 -O3 -DNDEBUG -Itests -I"$MINIZ_SRC" \
    tests/core_surface_miniz.c "$MINIZ_SRC/miniz.c" \
    "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" \
    -o "$C_ORACLE"
"$C_ORACLE" > "$C_OUT"
if [[ -n "$SX_OPT" ]]; then
    "$SX_BIN" run tests/core_surface.sx --opt "$SX_OPT"
else
    "$SX_BIN" run tests/core_surface.sx
fi
cmp "$C_OUT" "$SX_OUT"
echo "version/error/checksum/bound exact oracle ok"
