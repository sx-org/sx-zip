# Local performance baseline

This is a development baseline, not a cross-machine performance claim. It is
intended to make large regressions in the staging port visible before stdlib
migration.

Recorded 2026-07-20 on arm64 macOS 26.4 (Darwin 25.4.0), using sx compiler
commit `6f572b4e` with the LLVM PassBuilder `default<O3>` pipeline, the native sx
tdefl/tinfl port, and upstream miniz 3.1.2 commit
`77d0dce8627735138c51770d1799a1ef48f2117d`:

```sh
BENCH_CPU=generic SX_BIN=/Users/agra/projects/sx/zig-out/bin/sx tests/bench.sh
```

Every run that reaches measurement rewrites
[`BENCHMARK_RESULTS.md`](BENCHMARK_RESULTS.md) with exact nanosecond timings,
the full-file byte-identity gate, derived SX/C speedups, a throughput table,
and both programs' raw output. Set `BENCHMARK_MARKDOWN` to choose another
report path.

Each table entry is the median of three process runs. Within a process, the
harness measures ten codec calls with a monotonic nanosecond clock and reports
their integer average. The input is 1 MiB; ratio is packed bytes divided by
input bytes. Both measurements include allocation performed by their
heap-returning one-shot zlib encode/decode paths; release of the returned
buffers is outside the timed interval. `BENCH_CPU=generic` is passed to both
SX and C so this comparison does not mix SX's generic default with clang's
host-specific default.

| Input | Level | Implementation | Packed bytes | Ratio | Encode ns | Encode KiB/s | Decode ns | Decode KiB/s |
| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Repetitive | 1 | sx `--opt 3 --cpu generic` | 32,890 | 3.1% | 786,600 | 1,301,805 | 671,800 | 1,524,263 |
| Repetitive | 1 | miniz C fast `-O3`, generic | 32,890 | 3.1% | 682,900 | 1,499,487 | 721,200 | 1,419,855 |
| Repetitive | 6 | sx `--opt 3 --cpu generic` | 5,658 | 0.5% | 2,242,700 | 456,592 | 489,300 | 2,092,785 |
| Repetitive | 6 | miniz C fast `-O3`, generic | 5,658 | 0.5% | 1,952,300 | 524,509 | 466,800 | 2,193,658 |
| Repetitive | 9 | sx `--opt 3 --cpu generic` | 5,655 | 0.5% | 2,226,100 | 459,997 | 469,000 | 2,183,368 |
| Repetitive | 9 | miniz C fast `-O3`, generic | 5,655 | 0.5% | 1,965,100 | 521,093 | 460,500 | 2,223,669 |
| Incompressible | 1 | sx `--opt 3 --cpu generic` | 1,049,567 | 100.0% | 5,110,000 | 200,391 | 3,005,600 | 340,697 |
| Incompressible | 1 | miniz C fast `-O3`, generic | 1,049,567 | 100.0% | 3,476,700 | 294,532 | 3,088,700 | 331,531 |
| Incompressible | 6 | sx `--opt 3 --cpu generic` | 1,048,752 | 100.0% | 25,229,000 | 40,588 | 465,600 | 2,199,312 |
| Incompressible | 6 | miniz C fast `-O3`, generic | 1,048,752 | 100.0% | 22,587,600 | 45,334 | 504,900 | 2,028,124 |
| Incompressible | 9 | sx `--opt 3 --cpu generic` | 1,048,752 | 100.0% | 25,044,400 | 40,887 | 446,400 | 2,293,906 |
| Incompressible | 9 | miniz C fast `-O3`, generic | 1,048,752 | 100.0% | 22,417,800 | 45,677 | 487,800 | 2,099,220 |

The sx streams are byte-for-byte identical to upstream miniz C in all six
corpus/level comparisons, not merely equal in size. `tests/bench.sh` fails if
any byte differs. The identity path includes tdefl's 16-bit zero-sentinel hash
chains, exact wrapped-position rejection, packed 64 KiB flag/code buffer,
two-pass stable radix sort, length/distance lookup tables, and 16-bit frequency
tables.

The C oracle explicitly enables `MINIZ_USE_UNALIGNED_LOADS_AND_STORES=1`,
`MINIZ_LITTLE_ENDIAN=1`, and `MINIZ_HAS_64BIT_REGISTERS=1`. This selects
miniz's fastest level-1 parser and 64-bit packed LZ emitter on arm64. The
explicit CPU setting then holds target tuning equal between the two compilers.

The sx encoder now has the exact 4 KiB level-1 hash parser, normal rolling
hash, 16-bit chains, 257-byte mirrored dictionary tail, word-at-a-time match
comparison, packed token buffer, reusable output block, and batched 64-bit
emitter. Encode is about 1.13–1.15× C on repetitive data, 1.12× at normal
levels on incompressible data, and 1.47× at level 1 on incompressible data.

The inflater uses tinfl's 16-bit 10-bit lookup entries, negative-node long-code
tree, 32-bit refill/two-literal loop, retained stored-block read-ahead, fixed
tree reuse, and in-place tree construction. Decode is now within roughly 9%
of C across this matrix and is faster in four of the six median comparisons.

An optimized IR comparison was used to close source-level gaps. The remaining
encoder difference correlates with backend information that the C IR has and
the current sx IR does not: clang attaches TBAA metadata throughout tdefl,
while SX currently emits no TBAA for these accesses, and SX's retained codec
counters are predominantly 64-bit where miniz C uses 32-bit `mz_uint`. This is
not evidence of a scoped-out C parser or emitter path; all fastest-variant
source decisions and all six output streams are exact-oracle gated.

An earlier compiler baseline used the same `--opt 3` spelling but only
forwarded the setting to LLVM's target machine. The current compiler runs the
full `default<O3>` middle-end pipeline before code generation.

The deterministic inputs and output verification live in `tests/bench.sx` and
`tests/bench_miniz.c`. Debug builds are deliberately excluded because they
materially distort the bit-at-a-time decoder and hash-chain encoder costs. The
C comparison remains an optional development oracle and adds no build or
runtime dependency to the sx module.
