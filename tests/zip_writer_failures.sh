#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
if [[ -z "${KEEP_ZIP_WRITER_FAILURE_FIXTURES:-}" ]]; then
    trap 'rm -f /tmp/c-miniz-zip-writer-failures /tmp/{miniz,sx}-zip-writer-failures.bin' EXIT
fi
clang -std=c99 -O2 -DMINIZ_NO_TIME -Itests -I"$MINIZ_SRC" \
    tests/zip_writer_failures_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_zip.c" \
    "$MINIZ_SRC/miniz_tinfl.c" "$MINIZ_SRC/miniz_tdef.c" -o /tmp/c-miniz-zip-writer-failures
/tmp/c-miniz-zip-writer-failures
if [[ -n "${SX_OPT:-}" ]]; then "$SX_BIN" run tests/zip_writer_failures.sx --opt "$SX_OPT"; else "$SX_BIN" run tests/zip_writer_failures.sx; fi
cmp /tmp/miniz-zip-writer-failures.bin /tmp/sx-zip-writer-failures.bin
echo "ZIP writer callback phase-failure parity ok"
