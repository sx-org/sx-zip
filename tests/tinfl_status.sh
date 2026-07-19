#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
CC="${CC:-clang}"
C_BIN=/tmp/miniz-tinfl-status
C_FAST=/tmp/miniz-tinfl-status-fast
C_MEMCPY=/tmp/miniz-tinfl-status-memcpy
C_32=/tmp/miniz-tinfl-status-32
trap 'rm -f "$C_BIN" "$C_FAST" "$C_MEMCPY" "$C_32" /tmp/{miniz,sx-miniz}-tinfl-status.bin /tmp/miniz-tinfl-status-{fast,memcpy,32}.bin' EXIT

"$CC" -std=c99 -O3 -DNDEBUG -Itests -I"$MINIZ_SRC" \
    tests/tinfl_status_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" \
    -o "$C_BIN"
"$CC" -std=c99 -O3 -DNDEBUG -DMINIZ_USE_UNALIGNED_LOADS_AND_STORES=1 -DMINIZ_LITTLE_ENDIAN=1 -DMINIZ_HAS_64BIT_REGISTERS=1 -Itests -I"$MINIZ_SRC" \
    tests/tinfl_status_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" -o "$C_FAST"
"$CC" -std=c99 -O3 -DNDEBUG -DMINIZ_USE_UNALIGNED_LOADS_AND_STORES=1 -DMINIZ_UNALIGNED_USE_MEMCPY=1 -DMINIZ_LITTLE_ENDIAN=1 -DMINIZ_HAS_64BIT_REGISTERS=1 -Itests -I"$MINIZ_SRC" \
    tests/tinfl_status_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" -o "$C_MEMCPY"
"$CC" -std=c99 -O3 -DNDEBUG -U__x86_64__ -U__LP64__ -U_LP64 -DMINIZ_USE_UNALIGNED_LOADS_AND_STORES=1 -DMINIZ_LITTLE_ENDIAN=1 -Itests -I"$MINIZ_SRC" \
    tests/tinfl_status_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" -o "$C_32"
"$C_BIN" > /tmp/miniz-tinfl-status.bin
"$C_FAST" > /tmp/miniz-tinfl-status-fast.bin
"$C_MEMCPY" > /tmp/miniz-tinfl-status-memcpy.bin
"$C_32" > /tmp/miniz-tinfl-status-32.bin
if [[ -n "${SX_OPT:-}" ]]; then
    "$SX_BIN" run tests/tinfl_status.sx --opt "$SX_OPT"
else
    "$SX_BIN" run tests/tinfl_status.sx
fi
cmp /tmp/miniz-tinfl-status.bin /tmp/sx-miniz-tinfl-status.bin
cmp /tmp/miniz-tinfl-status-fast.bin /tmp/sx-miniz-tinfl-status.bin
cmp /tmp/miniz-tinfl-status-memcpy.bin /tmp/sx-miniz-tinfl-status.bin
cmp /tmp/miniz-tinfl-status-32.bin /tmp/sx-miniz-tinfl-status.bin
echo "tinfl status/window and portable/memcpy/32/64-bit oracle ok"
