#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"

cc -std=c99 -O2 -Itests -I"$MINIZ_SRC" \
    tests/zip_stdio_failures_miniz.c "$MINIZ_SRC/miniz.c" \
    "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" \
    "$MINIZ_SRC/miniz_zip.c" -o /tmp/c-miniz-zip-stdio-failures
/tmp/c-miniz-zip-stdio-failures
if [[ -n "${SX_OPT:-}" ]]; then
    "$SX_BIN" run tests/zip_stdio_failures.sx --opt "$SX_OPT"
else
    "$SX_BIN" run tests/zip_stdio_failures.sx
fi
cmp /tmp/miniz-zip-stdio-failures.bin /tmp/sx-miniz-zip-stdio-failures.bin
echo "ZIP stdio open/stat/seek/read/write failure parity ok"
