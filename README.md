# sx-zip

> **Historical reference:** this repository is a completed staging port and is
> no longer under active development. It is retained as a reference for future
> compression and archive work in the [`sx`](https://github.com/sx-org/sx)
> standard library.

This tree contains a dependency-free, native `sx` port of the complete enabled
surface of [miniz 3.1.2](https://github.com/richgel999/miniz). Future integration
and API evolution belong in the sx repository; this repository preserves the
standalone implementation, compatibility tests, differential oracles, coverage
ledger, and performance investigation history.

The retained tree is the reference snapshot. Historical experiment documents
may mention commit identifiers from before the repository history was
consolidated; those identifiers are provenance labels, not stable revision
links.

The public API follows sx conventions instead of preserving miniz's C ABI:
state is initialized by value, fallible operations use typed error channels,
and every owned result is allocated by an explicit `Allocator` (defaulting to
`context.allocator`). No C import, system compression library, build package,
or runtime dependency is used.

## Required surface

- Adler-32 and CRC-32, including incremental updates.
- Raw RFC 1951 DEFLATE: one-shot and resumable inflate/deflate.
- RFC 1950 zlib and RFC 1952 gzip framing.
- In-memory, callback, iterator, file-backed, streaming, and in-place ZIP32 and
  ZIP64 archive reading, writing, validation, extraction, and append workflows.
- The optional miniz PNG writer as a separable layer.

The pinned upstream source is the scope: no supported upstream path is excluded.
Features that miniz itself rejects, including encrypted and multi-disk archives,
must be rejected with equivalent behavior rather than silently accepted.

## Retained status

The repository now contains the native checksum, raw DEFLATE/inflate,
zlib/gzip, ZIP32/ZIP64, and optional PNG layers. Codec states are resumable over
caller windows. ZIP coverage includes memory, random-read and sequential-write
callbacks, path and absolute-offset caller-owned handles, fixed/owned/callback/iterator/seek
extraction, validation, raw-copy append, stateful update, in-place mutation,
alignment, file/archive timestamp propagation, and forced/automatic ZIP64.
Random-read, path, and caller-owned handle readers retain only the central
directory and use upstream-sized 4 KiB EOCD, 64 KiB input, and 32 KiB inflate
windows. Exhaustive ZIP storage/error matrices and all cross-platform and
optional-build paths are covered by exact differential and target-reachability
gates. The standalone port is complete according to the exhaustive
[`docs/COVERAGE.md`](docs/COVERAGE.md) ledger. Standard-library and HTTP
integration were deliberately left to the sx repository.

The separate objective to beat matched miniz C `-O3` in every benchmark row
was cancelled after the retained implementation reached 7 wins across the 12
encode/decode rows. It is not a completeness requirement. The exact benchmark
harness and accepted measurements remain available as regression and compiler
engineering references; see
[`docs/BENCHMARK_RESULTS.md`](docs/BENCHMARK_RESULTS.md) and
[`docs/PERFORMANCE_FINDINGS.md`](docs/PERFORMANCE_FINDINGS.md).

Compression levels 0–10 follow miniz's production `tdefl` policy. Level 0 uses
stored blocks. Levels 1–3 use greedy parsing and levels 4–9 use lazy parsing;
level 10 is miniz's slower “uber” mode. The match-probe table is the upstream
`{0, 1, 6, 32, 16, 32, 128, 256, 512, 768, 1500}` table. The encoder retains
its 32 KiB dictionary across arbitrary caller
chunks, uses miniz's bounded LZ token buffer and block-flush heuristic, emits
fixed blocks for tiny payloads and dynamic trees otherwise, and falls back to
stored blocks when compression would expand data. Values above 10 are treated
as 10; a negative zlib-compatible level selects miniz's default-level flags.

## Ownership

Functions returning `string` return owned byte storage. Release it with the
same allocator used for the call:

```sx
#import "modules/std.sx";
#import "modules/std/mem.sx";
mz :: #import "miniz.sx";

packed := mz.compress("hello");
defer context.allocator.dealloc_bytes(packed.ptr);

plain := try mz.decompress(packed);
defer context.allocator.dealloc_bytes(plain.ptr);
```

Views returned by `ZipReader.entry` point into the caller-owned archive buffer
and remain valid only while that buffer remains alive. `extract` returns owned
storage. The default decompression limit is 1 GiB; callers processing untrusted
data should normally pass a smaller application-specific limit.

Streaming codec states own bounded allocator-backed history/block buffers.
Call `deinit` when finished; it is safe to call repeatedly.

## Reproducing verification

With a compatible local sx compiler, the archived self-contained tests can be
run with:

```sh
SX_BIN=../sx/zig-out/bin/sx tests/run.sh
```

Set `SX_OPT=3` to run the same behavioral and interoperability suite through
the compiler's aggressive LLVM optimization pipeline:

```sh
SX_OPT=3 SX_BIN=../sx/zig-out/bin/sx tests/run.sh
```

The suite includes deterministic, malformed-input, every-byte truncation,
boundary, randomized, split streaming, and allocation-cleanup cases. It runs
bidirectional Python-standard-library checks for zlib, gzip, ZIP, raw streaming
DEFLATE, and PNG; Info-ZIP is used when present. When the sibling `../miniz`
checkout and `clang` are available, an optional oracle verifies both directions
against upstream miniz 3.1.2. These are development oracles only, not project
or runtime dependencies. The suite verifies all 118 public names, all 180
implementation-function definitions, the six header-only streaming
declarations, and all 18 compile-time configuration identifiers against the
coverage ledger. Every implemented upstream public entry point is invoked by
its exact name in a C oracle; the six upstream declaration-only streaming APIs
are implemented and covered natively. The ledger fails closed on any
`Partial` or `Open` row.

The non-gating microbenchmark can be run separately:

```sh
BENCH_CPU=generic SX_BIN=../sx/zig-out/bin/sx tests/bench.sh
```

It reports monotonic nanosecond zlib encode/decode time, throughput, and ratio
for repetitive and incompressible 1 MiB inputs at levels 1, 6, and 9.
Debug-mode timing is not representative. When `clang` and the sibling
`../miniz` checkout are available, the runner also builds the upstream miniz
3.1.2 C comparison with `-O3`, applies the same explicit CPU target to both
compilers, and requires byte-for-byte identical streams.
This is an optional development benchmark, not a build or runtime dependency.
The benchmark methodology is documented in
[`docs/BENCHMARKS.md`](docs/BENCHMARKS.md); the final retained generated report
is [`docs/BENCHMARK_RESULTS.md`](docs/BENCHMARK_RESULTS.md).

## Provenance and license

The behavioral/source reference is miniz 3.1.2 at commit
`77d0dce8627735138c51770d1799a1ef48f2117d`. The implementation is licensed
under the MIT license; see [LICENSE](LICENSE). The original miniz copyright
notices and license terms are preserved in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
