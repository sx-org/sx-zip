#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
CC="${CC:-clang}"
oracle=/tmp/miniz-zip-add-from-reader-matrix
trap 'rm -f "$oracle" /tmp/{miniz,sx}-zip-add-from-reader-matrix.bin /tmp/miniz-zip-add-from-reader-fixtures.bin' EXIT

"$CC" -std=c99 -O3 -DNDEBUG -DMINIZ_NO_TIME -Itests -I"$MINIZ_SRC" \
    tests/zip_add_from_reader_matrix_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_zip.c" \
    "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" -o "$oracle"
"$oracle"
if [[ -n "${SX_OPT:-}" ]]; then
    "$SX_BIN" run tests/zip_add_from_reader_matrix.sx --opt "$SX_OPT"
else
    "$SX_BIN" run tests/zip_add_from_reader_matrix.sx
fi
cmp /tmp/miniz-zip-add-from-reader-matrix.bin /tmp/sx-zip-add-from-reader-matrix.bin
echo "ZIP add-from-reader malformed/raw-copy/descriptor parity ok"
