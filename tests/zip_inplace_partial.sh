#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
CC="${CC:-clang}"
oracle=/tmp/miniz-zip-inplace-partial
c_out=/tmp/miniz-zip-inplace-partial.bin
sx_out=/tmp/sx-zip-inplace-partial.bin
trap 'rm -f "$oracle" "$c_out" "$sx_out" /tmp/{miniz,sx}-zip-inplace-partial.zip' EXIT
trap '' XFSZ

"$CC" -std=c99 -O3 -DNDEBUG -DMINIZ_NO_TIME -Itests -I"$MINIZ_SRC" \
    tests/zip_inplace_partial_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_zip.c" \
    "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" -o "$oracle"
"$oracle" | /bin/cat > "$c_out"
if [[ -n "${SX_OPT:-}" ]]; then
    "$SX_BIN" run tests/zip_inplace_partial.sx --opt "$SX_OPT" | /bin/cat > "$sx_out"
else
    "$SX_BIN" run tests/zip_inplace_partial.sx | /bin/cat > "$sx_out"
fi
cmp "$c_out" "$sx_out"
echo "ZIP in-place partial-write/error/bytes parity ok"
