#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
if [[ -z "${KEEP_ZIP_ALIAS_FIXTURES:-}" ]]; then
    trap 'rm -f /tmp/c-miniz-zip-alias-surface /tmp/{miniz,sx}-zip-alias-surface.bin /tmp/{miniz,sx}-zip-alias-{base,inplace}.zip /tmp/{miniz,sx}-zip-alias-{prefix,cfile,extract,raw}.bin /tmp/miniz-zip-alias-{missing,failed-new}.zip' EXIT
fi

clang -std=c99 -O2 -DMINIZ_NO_TIME -Itests -I"$MINIZ_SRC" tests/zip_alias_surface_miniz.c \
    "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_zip.c" "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" \
    -o /tmp/c-miniz-zip-alias-surface
/tmp/c-miniz-zip-alias-surface > /tmp/miniz-zip-alias-surface.bin
if [[ -n "${SX_OPT:-}" ]]; then
    "$SX_BIN" run tests/zip_alias_surface.sx --opt "$SX_OPT"
else
    "$SX_BIN" run tests/zip_alias_surface.sx
fi
cmp /tmp/miniz-zip-alias-surface.bin /tmp/sx-zip-alias-surface.bin
echo "ZIP public alias/helper surface exact parity ok"
