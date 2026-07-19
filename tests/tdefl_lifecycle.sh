#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
CC="${CC:-clang}"
C_ORACLE=/tmp/miniz-tdefl-lifecycle
trap 'rm -f "$C_ORACLE" /tmp/{miniz,sx-miniz}-tdefl-lifecycle.txt' EXIT

"$CC" -std=c99 -O3 -DNDEBUG \
    -DMINIZ_USE_UNALIGNED_LOADS_AND_STORES=1 -DMINIZ_LITTLE_ENDIAN=1 -DMINIZ_HAS_64BIT_REGISTERS=1 \
    -Itests -I"$MINIZ_SRC" tests/tdefl_lifecycle_miniz.c \
    "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" -o "$C_ORACLE"
"$C_ORACLE" > /tmp/miniz-tdefl-lifecycle.txt
if [[ -n "${SX_OPT:-}" ]]; then
    "$SX_BIN" run tests/tdefl_lifecycle.sx --opt "$SX_OPT" > /tmp/sx-miniz-tdefl-lifecycle.txt
else
    "$SX_BIN" run tests/tdefl_lifecycle.sx > /tmp/sx-miniz-tdefl-lifecycle.txt
fi
diff -u /tmp/miniz-tdefl-lifecycle.txt /tmp/sx-miniz-tdefl-lifecycle.txt
echo "tdefl caller-window, callback, retained-status, and finish lifecycle parity ok"
