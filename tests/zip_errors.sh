#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
trap 'rm -f /tmp/c-miniz-zip-errors /tmp/{miniz,sx}-zip-errors.bin /tmp/miniz-zip-errors-*.zip' EXIT

clang -std=c99 -O2 -DMINIZ_NO_TIME -Itests -I"$MINIZ_SRC" \
    tests/zip_errors_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_zip.c" \
    "$MINIZ_SRC/miniz_tinfl.c" "$MINIZ_SRC/miniz_tdef.c" \
    -o /tmp/c-miniz-zip-errors
/tmp/c-miniz-zip-errors
if [[ -n "${SX_OPT:-}" ]]; then
    "$SX_BIN" run tests/zip_errors.sx --opt "$SX_OPT"
else
    "$SX_BIN" run tests/zip_errors.sx
fi
cmp /tmp/miniz-zip-errors.bin /tmp/sx-zip-errors.bin
echo "zip error strings/state/unsupported/extraction/validation oracle ok"
