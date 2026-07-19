#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
trap 'rm -f /tmp/c-miniz-zip64-descriptor /tmp/{miniz,sx}-zip64-descriptor.bin /tmp/miniz-zip64-descriptor-{signed,unsigned}.zip' EXIT

clang -std=c99 -O2 -DMINIZ_NO_TIME -Itests -I"$MINIZ_SRC" \
    tests/zip64_descriptor_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_zip.c" \
    "$MINIZ_SRC/miniz_tinfl.c" "$MINIZ_SRC/miniz_tdef.c" \
    -o /tmp/c-miniz-zip64-descriptor
/tmp/c-miniz-zip64-descriptor
if [[ -n "${SX_OPT:-}" ]]; then
    "$SX_BIN" run tests/zip64_descriptor.sx --opt "$SX_OPT"
else
    "$SX_BIN" run tests/zip64_descriptor.sx
fi
cmp /tmp/miniz-zip64-descriptor.bin /tmp/sx-zip64-descriptor.bin
echo "ZIP64 signed/signatureless descriptor parse/extract/validate parity ok"
