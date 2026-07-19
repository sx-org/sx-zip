#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
if [[ -z "${KEEP_LOWLEVEL_ADAPTER_FIXTURES:-}" ]]; then
    trap 'rm -f /tmp/c-miniz-lowlevel-adapters /tmp/{miniz,sx}-lowlevel-adapters.bin' EXIT
fi

clang -std=c99 -O2 -Itests -I"$MINIZ_SRC" tests/lowlevel_adapters_miniz.c \
    "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" \
    -o /tmp/c-miniz-lowlevel-adapters
/tmp/c-miniz-lowlevel-adapters > /tmp/miniz-lowlevel-adapters.bin
if [[ -n "${SX_OPT:-}" ]]; then
    "$SX_BIN" run tests/lowlevel_adapters.sx --opt "$SX_OPT"
else
    "$SX_BIN" run tests/lowlevel_adapters.sx
fi
cmp /tmp/miniz-lowlevel-adapters.bin /tmp/sx-lowlevel-adapters.bin
echo "tdefl/tinfl fixed-buffer/callback/allocator adapter parity ok"
