#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
CC="${CC:-clang}"
C_ORACLE=/tmp/miniz-zip-source-trace
trap 'rm -f "$C_ORACLE" /tmp/miniz-zip-source-trace.{zip,c.txt,sx.txt}' EXIT

"$CC" -std=c99 -O3 -DNDEBUG \
    -DMINIZ_USE_UNALIGNED_LOADS_AND_STORES=1 -DMINIZ_LITTLE_ENDIAN=1 -DMINIZ_HAS_64BIT_REGISTERS=1 \
    -Itests -I"$MINIZ_SRC" tests/zip_source_trace_miniz.c \
    "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" "$MINIZ_SRC/miniz_zip.c" \
    -o "$C_ORACLE"
"$C_ORACLE" > /tmp/miniz-zip-source-trace.c.txt
if [[ -n "${SX_OPT:-}" ]]; then
    "$SX_BIN" run tests/zip_source_trace.sx --opt "$SX_OPT" > /tmp/miniz-zip-source-trace.sx.txt
else
    "$SX_BIN" run tests/zip_source_trace.sx > /tmp/miniz-zip-source-trace.sx.txt
fi
diff -u /tmp/miniz-zip-source-trace.c.txt /tmp/miniz-zip-source-trace.sx.txt
echo "ZIP callback source init/extract/iterator read trace parity ok"
