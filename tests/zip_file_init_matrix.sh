#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
CC="${CC:-clang}"
oracle=/tmp/miniz-zip-file-init-matrix
trap 'rm -f "$oracle" /tmp/{miniz,sx}-zip-file-init-matrix.bin /tmp/miniz-zip-file-init-{container,tiny}.bin /tmp/miniz-zip-file-init-missing.bin' EXIT

"$CC" -std=c99 -O3 -DNDEBUG -Itests -I"$MINIZ_SRC" \
    tests/zip_file_init_matrix_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_zip.c" \
    "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" -o "$oracle"
"$oracle"
if [[ -n "${SX_OPT:-}" ]]; then
    "$SX_BIN" run tests/zip_file_init_matrix.sx --opt "$SX_OPT"
else
    "$SX_BIN" run tests/zip_file_init_matrix.sx
fi
cmp /tmp/miniz-zip-file-init-matrix.bin /tmp/sx-zip-file-init-matrix.bin
echo "ZIP path/CFILE init start/size/error matrix parity ok"
