#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
if [[ -z "${KEEP_ZIP_CORRUPTION_FIXTURES:-}" ]]; then
    trap 'rm -f /tmp/c-miniz-zip-corruption /tmp/{miniz,sx}-zip-corruption.bin /tmp/miniz-zip-corruption-fixtures.bin' EXIT
fi
clang -std=c99 -O2 -DMINIZ_NO_TIME -Itests -I"$MINIZ_SRC" \
    tests/zip_corruption_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_zip.c" \
    "$MINIZ_SRC/miniz_tinfl.c" "$MINIZ_SRC/miniz_tdef.c" -o /tmp/c-miniz-zip-corruption
/tmp/c-miniz-zip-corruption
if [[ -n "${SX_OPT:-}" ]]; then "$SX_BIN" run tests/zip_corruption.sx --opt "$SX_OPT"; else "$SX_BIN" run tests/zip_corruption.sx; fi
cmp /tmp/miniz-zip-corruption.bin /tmp/sx-zip-corruption.bin
echo "ZIP EOCD/central/local corruption error parity ok"
