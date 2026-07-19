#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
SX_OPT="${SX_OPT:-}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
CC="${CC:-clang}"
C_ORACLE=/tmp/sx-miniz-inflate-matrix-oracle
C_OUT=/tmp/miniz-inflate-matrix.bin
SX_OUT=/tmp/sx-miniz-inflate-matrix.bin
trap 'rm -f "$C_ORACLE" "$C_OUT" "$SX_OUT"' EXIT

"$CC" -std=c99 -O3 -DNDEBUG \
    -DMINIZ_USE_UNALIGNED_LOADS_AND_STORES=1 \
    -DMINIZ_LITTLE_ENDIAN=1 \
    -DMINIZ_HAS_64BIT_REGISTERS=1 \
    -Itests -I"$MINIZ_SRC" tests/inflate_matrix_miniz.c \
    "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" \
    -o "$C_ORACLE"
"$C_ORACLE" > "$C_OUT"
if [[ -n "$SX_OPT" ]]; then
    "$SX_BIN" run tests/inflate_matrix.sx --opt "$SX_OPT"
else
    "$SX_BIN" run tests/inflate_matrix.sx
fi
cmp "$C_OUT" "$SX_OUT"
echo "zlib inflate flush, dictionary, and failure matrix oracle ok"
