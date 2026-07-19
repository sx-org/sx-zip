#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
trap 'rm -f /tmp/miniz-zip-exact-*.zip /tmp/sx-zip-exact-*.zip /tmp/miniz-zip-exact-locate.bin /tmp/sx-zip-exact-locate.bin /tmp/c-miniz-zip-exact /tmp/c-miniz-zip-time' EXIT

clang -std=c99 -O2 -DMINIZ_NO_TIME -Itests -I"$MINIZ_SRC" \
    tests/zip_exact_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_zip.c" \
    "$MINIZ_SRC/miniz_tinfl.c" "$MINIZ_SRC/miniz_tdef.c" \
    -o /tmp/c-miniz-zip-exact
/tmp/c-miniz-zip-exact
clang -std=c99 -O2 -Itests -I"$MINIZ_SRC" \
    tests/zip_time_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_zip.c" \
    "$MINIZ_SRC/miniz_tinfl.c" "$MINIZ_SRC/miniz_tdef.c" \
    -o /tmp/c-miniz-zip-time
TZ=UTC /tmp/c-miniz-zip-time
if [[ -n "${SX_OPT:-}" ]]; then
    "$SX_BIN" run tests/zip_exact.sx --opt "$SX_OPT"
    TZ=UTC "$SX_BIN" run tests/zip_time.sx --opt "$SX_OPT"
else
    "$SX_BIN" run tests/zip_exact.sx
    TZ=UTC "$SX_BIN" run tests/zip_time.sx
fi

for variant in descriptor patched raw zip64 auto count size align update precompressed64 precompressed32max; do
    cmp "/tmp/miniz-zip-exact-${variant}.zip" "/tmp/sx-zip-exact-${variant}.zip"
done
cmp /tmp/miniz-zip-exact-locate.bin /tmp/sx-zip-exact-locate.bin
cmp /tmp/miniz-zip-exact-time.zip /tmp/sx-zip-exact-time.zip
echo "zip writer descriptor/patched/raw/aligned/update/precompressed/timestamp/forced+automatic-offset+count+size-zip64 exact oracle ok"
