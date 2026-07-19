#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
trap 'rm -f /tmp/c-miniz-zip-source-failures /tmp/{miniz,sx}-zip-source-failures.bin /tmp/miniz-zip-source-failures.zip' EXIT
clang -std=c99 -O2 -DMINIZ_NO_TIME -Itests -I"$MINIZ_SRC" \
    tests/zip_source_failures_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_zip.c" \
    "$MINIZ_SRC/miniz_tinfl.c" "$MINIZ_SRC/miniz_tdef.c" -o /tmp/c-miniz-zip-source-failures
/tmp/c-miniz-zip-source-failures
if [[ -n "${SX_OPT:-}" ]]; then "$SX_BIN" run tests/zip_source_failures.sx --opt "$SX_OPT"; else "$SX_BIN" run tests/zip_source_failures.sx; fi
cmp /tmp/miniz-zip-source-failures.bin /tmp/sx-zip-source-failures.bin
echo "ZIP callback short-read/output/iterator failure parity ok"
