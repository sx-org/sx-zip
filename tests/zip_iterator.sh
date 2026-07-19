#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
if [[ -z "${KEEP_ZIP_ITERATOR_FIXTURES:-}" ]]; then
    trap 'rm -f /tmp/c-miniz-zip-iterator /tmp/{miniz,sx}-zip-iterator.bin /tmp/miniz-zip-iterator{,-corrupt-0,-corrupt-1,-unsupported-0,-unsupported-1,-unsupported-2}.zip' EXIT
fi
clang -std=c99 -O2 -DMINIZ_NO_TIME -Itests -I"$MINIZ_SRC" \
    tests/zip_iterator_miniz.c "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_zip.c" \
    "$MINIZ_SRC/miniz_tinfl.c" "$MINIZ_SRC/miniz_tdef.c" -o /tmp/c-miniz-zip-iterator
/tmp/c-miniz-zip-iterator
cp /tmp/miniz-zip-iterator.zip /tmp/miniz-zip-iterator-corrupt-0.zip
cp /tmp/miniz-zip-iterator.zip /tmp/miniz-zip-iterator-corrupt-1.zip
python3 - <<'PY'
import struct
for index in (0, 1):
    path = f"/tmp/miniz-zip-iterator-corrupt-{index}.zip"
    with open(path, "rb") as source:
        data = bytearray(source.read())
    eocd = data.rfind(b"PK\x05\x06")
    at = struct.unpack_from("<I", data, eocd + 16)[0]
    for current in range(index + 1):
        assert data[at:at + 4] == b"PK\x01\x02"
        if current == index:
            data[at + 16] ^= 0x5a
            break
        name, extra, comment = struct.unpack_from("<HHH", data, at + 28)
        at += 46 + name + extra + comment
    with open(path, "wb") as target:
        target.write(data)
PY
if [[ -n "${SX_OPT:-}" ]]; then "$SX_BIN" run tests/zip_iterator.sx --opt "$SX_OPT"; else "$SX_BIN" run tests/zip_iterator.sx; fi
cmp /tmp/miniz-zip-iterator.bin /tmp/sx-zip-iterator.bin
echo "ZIP iterator lifecycle/free/CRC parity ok"
