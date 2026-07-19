#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
trap 'rm -f /tmp/c-miniz-zip-lifecycle /tmp/{miniz,sx}-zip-lifecycle.bin /tmp/miniz-zip-lifecycle-base.zip' EXIT

clang -std=c99 -O2 -DMINIZ_NO_TIME -Itests -I"$MINIZ_SRC" \
    tests/zip_lifecycle_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_zip.c" \
    "$MINIZ_SRC/miniz_tinfl.c" "$MINIZ_SRC/miniz_tdef.c" \
    -o /tmp/c-miniz-zip-lifecycle
/tmp/c-miniz-zip-lifecycle
if [[ -n "${SX_OPT:-}" ]]; then
    "$SX_BIN" run tests/zip_lifecycle.sx --opt "$SX_OPT"
else
    "$SX_BIN" run tests/zip_lifecycle.sx
fi
cmp /tmp/miniz-zip-lifecycle.bin /tmp/sx-zip-lifecycle.bin
echo "ZIP init/reinit/readable-writer/finalize/end lifecycle parity ok"
