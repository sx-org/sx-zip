#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
trap 'rm -f /tmp/c-miniz-zip-writer-parameters /tmp/{miniz,sx}-zip-writer-parameters.bin' EXIT
clang -std=c99 -O2 -DMINIZ_NO_TIME -Itests -I"$MINIZ_SRC" \
    tests/zip_writer_parameters_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_zip.c" \
    "$MINIZ_SRC/miniz_tinfl.c" "$MINIZ_SRC/miniz_tdef.c" -o /tmp/c-miniz-zip-writer-parameters
/tmp/c-miniz-zip-writer-parameters
if [[ -n "${SX_OPT:-}" ]]; then "$SX_BIN" run tests/zip_writer_parameters.sx --opt "$SX_OPT"; else "$SX_BIN" run tests/zip_writer_parameters.sx; fi
cmp /tmp/miniz-zip-writer-parameters.bin /tmp/sx-zip-writer-parameters.bin
echo "ZIP writer parameter and metadata-boundary parity ok"
