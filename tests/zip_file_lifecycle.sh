#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
trap 'rm -f /tmp/c-miniz-zip-file-lifecycle /tmp/{miniz,sx}-file-lifecycle.bin /tmp/{miniz,sx}-file-lifecycle-{path,close}.zip /tmp/{miniz,sx}-file-lifecycle-cfile.bin' EXIT

clang -std=c99 -O2 -DMINIZ_NO_TIME -Itests -I"$MINIZ_SRC" \
    tests/zip_file_lifecycle_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_zip.c" \
    "$MINIZ_SRC/miniz_tinfl.c" "$MINIZ_SRC/miniz_tdef.c" -o /tmp/c-miniz-zip-file-lifecycle
/tmp/c-miniz-zip-file-lifecycle
if [[ -n "${SX_OPT:-}" ]]; then
    "$SX_BIN" run tests/zip_file_lifecycle.sx --opt "$SX_OPT"
else
    "$SX_BIN" run tests/zip_file_lifecycle.sx
fi
cmp /tmp/miniz-file-lifecycle.bin /tmp/sx-file-lifecycle.bin
echo "ZIP path/cfile writer and close-failure lifecycle parity ok"
