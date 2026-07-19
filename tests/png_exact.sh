#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SX_BIN=${SX_BIN:-/Users/agra/projects/sx/zig-out/bin/sx}
SX_OPT=${SX_OPT:-0}
MINIZ_DIR=${MINIZ_DIR:-/Users/agra/projects/miniz}
CC=${CC:-cc}
C_BIN=/tmp/miniz-png-exact
C_OUT=/tmp/miniz-png-exact.bin
SX_OUT=/tmp/sx-miniz-png-exact.bin
trap 'rm -f "$C_BIN" "$C_OUT" "$SX_OUT"' EXIT

"$CC" -std=c99 -O3 -DNDEBUG -I"$ROOT/tests" -I"$MINIZ_DIR" \
    "$ROOT/tests/png_exact_miniz.c" \
    "$MINIZ_DIR/miniz.c" "$MINIZ_DIR/miniz_tdef.c" "$MINIZ_DIR/miniz_tinfl.c" \
    -o "$C_BIN"
"$C_BIN" > "$C_OUT"
"$SX_BIN" run "$ROOT/tests/png_exact.sx" --opt "$SX_OPT"
cmp "$C_OUT" "$SX_OUT"
echo "png exact oracle ok"
