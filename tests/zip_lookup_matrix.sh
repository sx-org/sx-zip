#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
trap 'rm -f /tmp/c-miniz-zip-lookup-matrix /tmp/{miniz,sx}-zip-lookup-matrix.bin /tmp/miniz-zip-lookup-matrix.zip' EXIT

clang -std=c99 -O2 -DMINIZ_NO_TIME -Itests -I"$MINIZ_SRC" \
    tests/zip_lookup_matrix_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_zip.c" \
    "$MINIZ_SRC/miniz_tinfl.c" "$MINIZ_SRC/miniz_tdef.c" -o /tmp/c-miniz-zip-lookup-matrix
/tmp/c-miniz-zip-lookup-matrix
if [[ -n "${SX_OPT:-}" ]]; then "$SX_BIN" run tests/zip_lookup_matrix.sx --opt "$SX_OPT"; else "$SX_BIN" run tests/zip_lookup_matrix.sx; fi
cmp /tmp/miniz-zip-lookup-matrix.bin /tmp/sx-zip-lookup-matrix.bin
echo "ZIP lookup/stat/name enumeration parity ok"
