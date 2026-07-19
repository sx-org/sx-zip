#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
trap 'rm -f /tmp/c-miniz-zip-finalize-flush /tmp/{miniz,sx}-zip-finalize-flush.bin' EXIT
clang -std=c99 -O2 -DMINIZ_NO_TIME -Itests -I"$MINIZ_SRC" \
    tests/zip_finalize_flush_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_zip.c" \
    "$MINIZ_SRC/miniz_tinfl.c" "$MINIZ_SRC/miniz_tdef.c" -o /tmp/c-miniz-zip-finalize-flush
/tmp/c-miniz-zip-finalize-flush
if [[ -n "${SX_OPT:-}" ]]; then "$SX_BIN" run tests/zip_finalize_flush.sx --opt "$SX_OPT"; else "$SX_BIN" run tests/zip_finalize_flush.sx; fi
cmp /tmp/miniz-zip-finalize-flush.bin /tmp/sx-zip-finalize-flush.bin
echo "ZIP final stdio flush-failure parity ok"
