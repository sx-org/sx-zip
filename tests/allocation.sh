#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
trap 'rm -f /tmp/c-miniz-allocation /tmp/miniz-allocation.txt /tmp/sx-allocation.txt' EXIT

clang -std=c99 -O2 -Itests -I"$MINIZ_SRC" \
    tests/allocation_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_zip.c" \
    "$MINIZ_SRC/miniz_tinfl.c" "$MINIZ_SRC/miniz_tdef.c" \
    -o /tmp/c-miniz-allocation
/tmp/c-miniz-allocation > /tmp/miniz-allocation.txt

if [[ -n "${SX_OPT:-}" ]]; then
    "$SX_BIN" run tests/allocation_parity.sx --opt "$SX_OPT" > /tmp/sx-allocation.txt
    "$SX_BIN" run tests/allocation.sx --opt "$SX_OPT"
else
    "$SX_BIN" run tests/allocation_parity.sx > /tmp/sx-allocation.txt
    "$SX_BIN" run tests/allocation.sx
fi

cmp /tmp/miniz-allocation.txt /tmp/sx-allocation.txt
echo "allocation status parity ok"
