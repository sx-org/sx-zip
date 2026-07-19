#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
SX_OPT="${SX_OPT:-}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
CC="${CC:-clang}"
C_ORACLE=/tmp/sx-miniz-file-adapter-oracle
C_OUT=/tmp/miniz-file-adapter.bin
SX_OUT=/tmp/sx-miniz-file-adapter.bin
SOURCE=/tmp/sx-miniz-file-source.bin
trap 'rm -f "$C_ORACLE" "$C_OUT" "$SX_OUT" "$SOURCE" /tmp/miniz-file-adapter-extracted.bin /tmp/sx-file-adapter-extracted.bin' EXIT

printf 'absolute-offset-file-adapter-payload' > "$SOURCE"
TZ=UTC touch -t 200102030405.06 "$SOURCE"
"$CC" -std=c99 -O3 -DNDEBUG -Itests -I"$MINIZ_SRC" \
    tests/zip_file_adapter_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_zip.c" \
    "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" -o "$C_ORACLE"
TZ=UTC "$C_ORACLE" "$SOURCE" > "$C_OUT"
if [[ -n "$SX_OPT" ]]; then
    TZ=UTC "$SX_BIN" run tests/zip_file_adapter.sx --opt "$SX_OPT"
else
    TZ=UTC "$SX_BIN" run tests/zip_file_adapter.sx
fi
cmp "$C_OUT" "$SX_OUT"
echo "zip file mtime and absolute cfile adapter exact oracle ok"
