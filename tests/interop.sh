#!/usr/bin/env bash
set -euo pipefail

# Python is an independent development oracle here, not a project/runtime
# dependency. The library itself remains pure sx and imports only the stdlib.
SX_BIN="${SX_BIN:-sx}"
run_sx() {
    if [[ -n "${SX_OPT:-}" ]]; then
        "$SX_BIN" run "$1" --opt "$SX_OPT"
    else
        "$SX_BIN" run "$1"
    fi
}
SX_ARCHIVE=/tmp/sx-miniz-interop.zip
PY_ARCHIVE=/tmp/python-miniz-interop.zip
SX_ZLIB=/tmp/sx-miniz-interop.zlib
PY_ZLIB=/tmp/python-miniz-interop.zlib
SX_GZIP=/tmp/sx-miniz-interop.gz
PY_GZIP=/tmp/python-miniz-interop.gz
SX_PNG=/tmp/sx-miniz-interop.png
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
trap 'rm -f "$SX_ARCHIVE" "$PY_ARCHIVE" "$SX_ZLIB" "$PY_ZLIB" "$SX_GZIP" "$PY_GZIP" "$SX_PNG" /tmp/sx-miniz-interop-{1,2,3,4}.png /tmp/sx-miniz-corpus.zip /tmp/sx-miniz-stream.deflate /tmp/sx-miniz-all-symbols.bin /tmp/sx-miniz-oracle /tmp/miniz-miniz-interop.{zlib,zip} /tmp/miniz-miniz-interop-zip64.zip' EXIT

run_sx tests/emit_zip.sx
run_sx tests/emit_zlib.sx
run_sx tests/emit_gzip.sx
run_sx tests/emit_png.sx
run_sx tests/emit_miniz_corpus.sx
run_sx tests/emit_stream.sx
run_sx tests/emit_all_symbols.sx
if command -v unzip >/dev/null 2>&1; then
    unzip -tqq "$SX_ARCHIVE"
    [[ "$(unzip -Z1 "$SX_ARCHIVE")" == $'alpha.txt\nnested/repeat.txt' ]]
    [[ "$(unzip -p "$SX_ARCHIVE" alpha.txt)" == "alpha" ]]
fi
python3 - <<'PY'
from pathlib import Path
import gzip
import io
import zlib
import zipfile

path = Path('/tmp/sx-miniz-interop.zip')
with zipfile.ZipFile(path) as zf:
    assert zf.testzip() is None
    assert zf.namelist() == ['alpha.txt', 'nested/repeat.txt']
    assert zf.read('alpha.txt') == b'alpha'
    assert zf.read('nested/repeat.txt') == b'repeat repeat repeat repeat repeat'

with zipfile.ZipFile('/tmp/sx-miniz-corpus.zip') as zf:
    assert zf.testzip() is None
    for distribution in ('random', 'repeat'):
        for level, size in ((0, 0), (1, 1), (6, 258), (9, 32768), (9, 65536)):
            data = zf.read(f'{distribution}-l{level}-n{size}.bin')
            assert len(data) == size
            if distribution == 'repeat':
                expected = bytes(ord('a') + ((i // 97 + i) % 11) for i in range(size))
            else:
                state = 0x12345678
                expected = bytearray()
                for _ in range(size):
                    state = (state ^ (state << 13)) & 0xffffffff
                    state = state ^ (state >> 17)
                    state = (state ^ (state << 5)) & 0xffffffff
                    expected.append(state & 0xff)
                expected = bytes(expected)
            assert data == expected

class Unseekable(io.BytesIO):
    def seekable(self):
        return False
    def seek(self, *args):
        raise OSError('not seekable')

zip_sink = Unseekable()
with zipfile.ZipFile(zip_sink, 'w') as zf:
    zf.comment = b'python archive comment'
    zf.writestr('stored.txt', b'from python', compress_type=zipfile.ZIP_STORED)
    info = zipfile.ZipInfo('nested/deflated.txt')
    info.compress_type = zipfile.ZIP_DEFLATED
    info.extra = b'\xfe\xca\x03\x00abc'
    info.comment = b'python entry comment'
    zf.writestr(info, b'python python python python python')
Path('/tmp/python-miniz-interop.zip').write_bytes(zip_sink.getvalue())

state = 0x31415926
skew = bytearray()
for _ in range(5000):
    state = (state ^ (state << 13)) & 0xffffffff
    state = state ^ (state >> 17)
    state = (state ^ (state << 5)) & 0xffffffff
    skew.append(ord('A') + state % 10)
assert zlib.decompress(Path('/tmp/sx-miniz-interop.zlib').read_bytes()) == bytes(skew)
phrase = b'Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. '
Path('/tmp/python-miniz-interop.zlib').write_bytes(zlib.compress(phrase * 100, 9))
streamed = zlib.decompress(Path('/tmp/sx-miniz-stream.deflate').read_bytes(), -15)
assert streamed == bytes(ord('A') + i % 7 for i in range(70000))
all_symbols = Path('/tmp/sx-miniz-all-symbols.bin').read_bytes()
at = cases = 0
while at < len(all_symbols):
    expected = int.from_bytes(all_symbols[at:at+4], 'little')
    packed_size = int.from_bytes(all_symbols[at+4:at+8], 'little')
    packed = all_symbols[at+8:at+8+packed_size]
    plain = zlib.decompress(packed, -15)
    assert len(plain) == expected and set(plain) == {ord('A')}
    at += 8 + packed_size
    cases += 1
assert at == len(all_symbols) and cases == 210
assert gzip.decompress(Path('/tmp/sx-miniz-interop.gz').read_bytes()) == b'sx gzip sx gzip sx gzip'
first = b'python gzip member one'
header = bytearray(b'\x1f\x8b\x08\x1e' + b'\x00\x00\x00\x00' + b'\x00\xff')
extra = b'independent-extra'
header += len(extra).to_bytes(2, 'little') + extra
header += b'payload.txt\x00'
header += b'python fixture\x00'
header += (zlib.crc32(header) & 0xffff).to_bytes(2, 'little')
raw_encoder = zlib.compressobj(9, zlib.DEFLATED, -15)
raw = raw_encoder.compress(first) + raw_encoder.flush()
trailer = zlib.crc32(first).to_bytes(4, 'little') + (len(first) & 0xffffffff).to_bytes(4, 'little')
Path('/tmp/python-miniz-interop.gz').write_bytes(bytes(header) + raw + trailer + gzip.compress(b' + member two', mtime=0))

def decode_png(path, channels):
    png = Path(path).read_bytes()
    assert png[:8] == b'\x89PNG\r\n\x1a\n'
    at = 8
    idat = bytearray()
    width = height = color_type = None
    chunks = []
    while at < len(png):
        size = int.from_bytes(png[at:at+4], 'big')
        kind = png[at+4:at+8]
        payload = png[at+8:at+8+size]
        checksum = int.from_bytes(png[at+8+size:at+12+size], 'big')
        assert zlib.crc32(kind + payload) & 0xffffffff == checksum
        chunks.append(kind)
        if kind == b'IHDR':
            width = int.from_bytes(payload[:4], 'big')
            height = int.from_bytes(payload[4:8], 'big')
            color_type = payload[9]
        if kind == b'IDAT': idat += payload
        at += 12 + size
    assert chunks == [b'IHDR', b'IDAT', b'IEND']
    raw_png = zlib.decompress(bytes(idat))
    row_bytes = width * channels
    rows = []
    prev = bytearray(row_bytes)
    for row in range(height):
        base = row * (row_bytes + 1)
        kind = raw_png[base]
        assert 0 <= kind <= 4
        encoded = raw_png[base + 1:base + 1 + row_bytes]
        decoded = bytearray(row_bytes)
        for col, value in enumerate(encoded):
            left = decoded[col - channels] if col >= channels else 0
            up = prev[col]
            upper_left = prev[col - channels] if col >= channels else 0
            if kind == 0: predictor = 0
            elif kind == 1: predictor = left
            elif kind == 2: predictor = up
            elif kind == 3: predictor = (left + up) // 2
            else:
                p = left + up - upper_left
                pa, pb, pc = abs(p-left), abs(p-up), abs(p-upper_left)
                predictor = left if pa <= pb and pa <= pc else (up if pb <= pc else upper_left)
            decoded[col] = (value + predictor) & 0xff
        rows.append(bytes(decoded))
        prev = decoded
    return width, height, color_type, rows

assert decode_png('/tmp/sx-miniz-interop.png', 3) == (1, 2, 2, [b'\x00\x00\xff', b'\xff\x00\x00'])
expected_types = [0, 4, 2, 6]
expected_rows = [
    [bytes([1, 2]), bytes([3, 4])],
    [bytes([1, 11, 2, 12]), bytes([3, 13, 4, 14])],
    [bytes([1, 11, 21, 2, 12, 22]), bytes([3, 13, 23, 4, 14, 24])],
    [bytes([1, 11, 21, 31, 2, 12, 22, 32]), bytes([3, 13, 23, 33, 4, 14, 24, 34])],
]
for channels in range(1, 5):
    decoded = decode_png(f'/tmp/sx-miniz-interop-{channels}.png', channels)
    assert decoded == (2, 2, expected_types[channels - 1], expected_rows[channels - 1])
PY
run_sx tests/read_python_zip.sx
run_sx tests/read_python_zlib.sx
run_sx tests/read_python_gzip.sx
if command -v clang >/dev/null 2>&1 && [[ -f "$MINIZ_SRC/miniz.c" ]]; then
    clang -std=c99 -O2 -Itests -I"$MINIZ_SRC" tests/miniz_oracle.c \
        "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_zip.c" \
        "$MINIZ_SRC/miniz_tinfl.c" "$MINIZ_SRC/miniz_tdef.c" \
        -o /tmp/sx-miniz-oracle
    /tmp/sx-miniz-oracle
    run_sx tests/read_miniz.sx
fi
echo "zlib/gzip/zip/png interoperability ok"
