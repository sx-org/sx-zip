# API and migration contract

This document pins the native sx shape of the miniz 3.1.2 port. The current
single-file module is a staging layout; the final stdlib split is described
below. C ABI compatibility is not a goal, but 100% of the behavior reachable
through the pinned C library is. The exhaustive completion criteria live in
[`COVERAGE.md`](COVERAGE.md).

## Source-to-sx map

| miniz area | Staging sx API | Final stdlib home |
| --- | --- | --- |
| `mz_adler32`, `mz_crc32` | `adler32`, `crc32` | `std/compress.sx` (or a checksum namespace re-exported there) |
| `tinfl_decompress*` | `inflate`, `Inflater`, `ZlibInflater` | `std/compress.sx` |
| `tdefl_compress*` | `deflate`, `deflate_fixed`, `deflate_dynamic`, `deflate_stored`, `Deflater`, `StoredDeflater`, `ZlibDeflater`, `GzipDeflater` | `std/compress.sx` |
| `compress*`, `uncompress*` | `compress`, `decompress` (zlib framing) | `std/compress.sx` |
| gzip framing | `gzip_compress`, `gzip_compress_options`, `gzip_decompress`, `GzipInflater` | `std/compress.sx` |
| `mz_zip_reader*` | `open_zip*`, `ZipReader`, `OwnedZipReader`, `ZipEntry`, random-read/path/handle/callback/iterator/seek/file adapters | `std/zip.sx` |
| `mz_zip_writer*` | `ZipWriter`, `ZipStreamWriter`, `ZipSink`, `ZipInputSource`, memory/path/handle/update/ZIP64 adapters | `std/zip.sx` |
| add/append helpers | `append_zip` and the exact in-place/raw-copy workflows | `std/zip.sx` |
| `tdefl_write_image_to_png*` | `write_png` | `std/png.sx`, layered on compression |

The final imports should read naturally as namespaces:

```sx
compress :: #import "modules/std/compress.sx";
zip      :: #import "modules/std/zip.sx";

plain := try compress.zlib_decompress(packed, .{ max_output = 8 * 1024 * 1024 });
archive := try zip.open(bytes);
```

The staging names `compress`/`decompress` preserve miniz familiarity. Before
stdlib migration they will gain unambiguous `zlib_` aliases; stdlib review will
choose one canonical spelling and retain the other only if it adds real value.

## State and streaming

`Inflater` is the bounded-memory resumable raw RFC 1951 decoder.
`ZlibInflater` adds incremental RFC 1950 header and Adler validation;
`GzipInflater` adds optional fields/FHCRC, concatenated members, CRC/ISIZE, and
an explicit final-input signal. Their 32 KiB history window uses the explicit
allocator, their Huffman tables live in the state, and the caller owns all
input/output windows:

```sx
inflater := Inflater.init(limit);
defer inflater.deinit();
progress := try inflater.step(input, output);
input = input[progress.consumed ..];
output = output[progress.produced ..];
```

`step` returns consumed/produced counts. Its status is one of
`need_input`, `need_output`, or `done`; malformed data remains an error channel,
not a status or zero-value result. `end` turns unfinished end-of-input into
`UnexpectedEnd`. `reset` reuses the allocated history window, and `deinit`
releases it idempotently. The one-shot raw, zlib, and gzip decode helpers drive these states
rather than keeping second decoders.

`Deflater` provides the same caller-window/status contract for levels 0–9. It
compresses bounded 65,535-byte blocks and repacks their exact valid bits, so
fixed/dynamic blocks remain legal across output-window and block boundaries.
Its block buffer plus bounded scratch/pending block storage use the explicit
allocator. `StoredDeflater` is the simpler level-0 specialization and owns the
same-sized bounded block buffer. `ZlibDeflater` and deterministic `GzipDeflater` add incremental
checksums and framing. `finish=true` latches the final input window, and callers
re-present any unconsumed tail after `need_output`.

The one-shot raw, zlib, and deterministic gzip helpers drive these states.
Metadata-bearing `gzip_compress_options` remains a one-shot framing convenience
over the same raw `Deflater`.

`ZipStreamWriter` writes local records, compressed data, and data descriptors
to a `ZipSink` callback immediately. It retains only central-directory
metadata, uses bounded `Deflater` scratch per entry, and never opens a path.
Sink failure raises `Io` after any already-accepted bytes, so this callback API
is explicitly non-atomic. `ZipWriter` remains the atomic in-memory alternative.
Both writers expose path and caller-owned handle adapters. Path adds use the
source file's modification time by default; the explicit no-time option models
`MINIZ_NO_TIME`. Handle adds follow miniz's CFILE contract: reads start at
absolute offset zero and `max_size=0` creates an empty descriptor-backed entry.
Path extraction restores the entry's DOS timestamp after a successful close.
`ZipWriter.init_readable` and `init_zip64_readable` map
`MZ_ZIP_FLAG_WRITE_ALLOW_READING`; `read_archive_data` is valid while such a
writer retains its heap image. `finalize` writes the central directory in place
and retains readable storage, while `finish` performs the heap-finalize form
and transfers the archive bytes to the caller. Checked `end` methods reproduce
miniz's first-end/second-end state and error transitions.

## Compression-level policy

- Level 0 emits stored blocks.
- Levels 1–3 use miniz tdefl's greedy parser; levels 4–9 use its lazy parser.
- `CompressionStrategy` exposes default, filtered, Huffman-only, RLE, and fixed
  behavior; `FlushMode` exposes no-flush, sync, full, and finish semantics.
- Match search uses miniz's level probe table and remains capped independently
  of input size. The rolling 32 KiB dictionary survives caller chunk edges.
- Blocks use tdefl's bounded token-buffer flush heuristic, fixed codes below 48
  source bytes, dynamic trees otherwise, and stored fallback when a compressed
  candidate would expand and the raw bytes remain in the dictionary.
- Level 10 preserves miniz's 1,500-probe “uber” mode. Values above 10 clamp to
  10; negative zlib-compatible levels select miniz's default-level flags.

Output is deterministic for identical bytes, options, and implementation
version. For the pinned miniz 3.1.2 reference and default zlib strategy,
byte-for-byte tdefl identity is a port requirement; the differential benchmark
fails on the first corpus/level stream that differs.

`append_zip` is atomic with respect to the caller: it reads and validates the
existing ZIP32/ZIP64 archive, copies each old local record without
inflate/recompress, rebuilds the central directory into new owned storage, and
leaves the original bytes untouched on failure. `zip_writer_from_reader`
preserves the pre-central byte region and starts adding at the previous central
directory offset, while filesystem in-place helpers expose miniz's explicitly
non-atomic partial-write contract.

## Ownership and lifetime

- Every returned `string` from compression, decompression, extraction, archive
  finalization, or PNG writing owns its backing storage through the allocator
  passed to that operation. The caller releases `result.ptr` through the same
  allocator.
- `ZipReader` borrows the archive byte string. `ZipEntry.name` and `.comment`
  are views into it. Neither may outlive or mutate independently of the archive.
- `ZipErrorState`, `zip_error_code`, `zip_error_string`, and
  `zip_error_string_value` expose miniz's numeric/string error table (including
  the unknown-value fallback). `ZipReader` integrates set/peek/clear/take state
  with stat-like inspection, lookup, extraction, and validation operations.
- ZIP lookup follows miniz's default case-folded heap-sort/binary-search
  selection; disabling sorting uses linear central-directory order. The reader
  never writes to the filesystem. Callers
  choosing to materialize names can apply `ZipEntry.is_safe_path`, which rejects
  absolute, drive-prefixed, and parent-traversal paths.
- `ZipWriter.init` returns state by value. `finish` transfers its output buffer
  to the caller and invalidates further additions. `deinit` releases unfinished
  state; calling it after `finish` is safe.
- `ZipStreamWriter` borrows its `ZipSink` callback/context for its lifetime and
  owns only central metadata. Callback byte views are valid for that call only;
  `finish` emits EOCD but transfers no buffer, and `deinit` is idempotent.
- Streaming input/output slices are borrowed for one `step` call only.
- Codec streaming states own their bounded history/block buffers through the
  allocator passed to `init`; callers must invoke the idempotent `deinit`.
- Allocation failure follows the stdlib `Allocator` contract. Size arithmetic
  and configured resource limits are validated before allocation.

## Errors

The staging `Error` set has distinct tags for malformed input, truncation,
checksum failure, unsupported features, output limits, missing entries,
archive overflow, and use after finalization. The stdlib split may use narrower
named sets per module, but it must preserve these distinctions.

There are no permissive fallbacks. In particular:

- bad zlib/gzip checksums never return bytes;
- encrypted and multi-disk archives never masquerade as supported archives;
- unknown compression methods never masquerade as stored entries;
- invalid Huffman trees/distances and truncated bitstreams fail;
- decompression stops before exceeding `max_output`.

## Compatibility and full-scope rule

The wire contract is every behavior enabled in the pinned miniz 3.1.2 library,
including its zlib-compatible strategies/flush modes, tinfl/tdefl callback and
fixed-buffer paths, ZIP32, ZIP64, file/callback/iterator/streaming archive APIs,
validation, raw-copy append, and preset-dictionary status behavior. There are
no port-level non-goals. Encryption and multi-disk handling remain unsupported
only because upstream rejects them; equivalent rejection is itself required and
tested behavior.

The port has no C imports, vendored binaries, package-manager dependencies,
system compression calls, or subprocesses. Upstream miniz, Python's standard
library, and system archive tools may be used only as independent development
oracles in tests.

## Provenance

Reference: miniz 3.1.2, commit
`77d0dce8627735138c51770d1799a1ef48f2117d`, under the MIT license reproduced
in `THIRD_PARTY_NOTICES.md`. The implementation is written in sx and follows
the formats and behavior; it is not a generated C translation.
