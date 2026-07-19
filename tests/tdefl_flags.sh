#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
CC="${CC:-clang}"
C_NORMAL=/tmp/miniz-tdefl-flags
C_LESS=/tmp/miniz-tdefl-flags-less
C_FAST=/tmp/miniz-tdefl-flags-fast
C_MEMCPY=/tmp/miniz-tdefl-flags-memcpy
C_32=/tmp/miniz-tdefl-flags-32
trap 'rm -f "$C_NORMAL" "$C_LESS" "$C_FAST" "$C_MEMCPY" "$C_32" /tmp/{miniz,sx-miniz}-tdefl-flags{,-less}.bin /tmp/miniz-tdefl-flags-{fast,memcpy,32}.bin' EXIT

"$CC" -std=c99 -O3 -DNDEBUG -Itests -I"$MINIZ_SRC" \
    tests/tdefl_flags_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" \
    -o "$C_NORMAL"
"$CC" -std=c99 -O3 -DNDEBUG -DTDEFL_LESS_MEMORY=1 -Itests -I"$MINIZ_SRC" \
    tests/tdefl_flags_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" \
    -o "$C_LESS"
"$CC" -std=c99 -O3 -DNDEBUG -DMINIZ_USE_UNALIGNED_LOADS_AND_STORES=1 -DMINIZ_LITTLE_ENDIAN=1 -DMINIZ_HAS_64BIT_REGISTERS=1 -Itests -I"$MINIZ_SRC" \
    tests/tdefl_flags_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" -o "$C_FAST"
"$CC" -std=c99 -O3 -DNDEBUG -DMINIZ_USE_UNALIGNED_LOADS_AND_STORES=1 -DMINIZ_UNALIGNED_USE_MEMCPY=1 -DMINIZ_LITTLE_ENDIAN=1 -DMINIZ_HAS_64BIT_REGISTERS=1 -Itests -I"$MINIZ_SRC" \
    tests/tdefl_flags_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" -o "$C_MEMCPY"
"$CC" -std=c99 -O3 -DNDEBUG -U__x86_64__ -U__LP64__ -U_LP64 -DMINIZ_USE_UNALIGNED_LOADS_AND_STORES=1 -DMINIZ_LITTLE_ENDIAN=1 -Itests -I"$MINIZ_SRC" \
    tests/tdefl_flags_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" -o "$C_32"
"$C_NORMAL" > /tmp/miniz-tdefl-flags.bin
"$C_LESS" > /tmp/miniz-tdefl-flags-less.bin
"$C_FAST" > /tmp/miniz-tdefl-flags-fast.bin
"$C_MEMCPY" > /tmp/miniz-tdefl-flags-memcpy.bin
"$C_32" > /tmp/miniz-tdefl-flags-32.bin
if [[ -n "${SX_OPT:-}" ]]; then
    "$SX_BIN" run tests/tdefl_flags.sx --opt "$SX_OPT"
else
    "$SX_BIN" run tests/tdefl_flags.sx
fi
cmp /tmp/miniz-tdefl-flags.bin /tmp/sx-miniz-tdefl-flags.bin
cmp /tmp/miniz-tdefl-flags-less.bin /tmp/sx-miniz-tdefl-flags-less.bin
cmp /tmp/miniz-tdefl-flags-fast.bin /tmp/sx-miniz-tdefl-flags.bin
cmp /tmp/miniz-tdefl-flags-memcpy.bin /tmp/sx-miniz-tdefl-flags.bin
cmp /tmp/miniz-tdefl-flags-32.bin /tmp/sx-miniz-tdefl-flags.bin
echo "tdefl flag, less-memory, portable, memcpy, and 32/64-bit identity ok"
