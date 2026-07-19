#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
trap 'rm -f /tmp/c-miniz-zip-file-update /tmp/{miniz,sx}-zip-file-update{,-base}.{zip,bin} /tmp/{miniz,sx}-zip-reserved.zip' EXIT

clang -std=c99 -O2 -DMINIZ_NO_TIME -Itests -I"$MINIZ_SRC" \
    tests/zip_file_update_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_zip.c" \
    "$MINIZ_SRC/miniz_tinfl.c" "$MINIZ_SRC/miniz_tdef.c" \
    -o /tmp/c-miniz-zip-file-update
/tmp/c-miniz-zip-file-update
if [[ -n "${SX_OPT:-}" ]]; then
    "$SX_BIN" run tests/zip_file_update.sx --opt "$SX_OPT"
else
    "$SX_BIN" run tests/zip_file_update.sx
fi
cmp /tmp/miniz-zip-file-update.bin /tmp/sx-zip-file-update.bin
cmp /tmp/miniz-zip-file-update.zip /tmp/sx-zip-file-update.zip
cmp /tmp/miniz-zip-reserved.zip /tmp/sx-zip-reserved.zip
echo "ZIP READ_ALLOW_WRITING in-place conversion parity ok"
