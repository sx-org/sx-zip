# Deflate performance findings

This log records accepted and rejected performance work so experiments are not
repeated without materially new evidence. docs/BENCHMARK_RESULTS.md remains
the generated report for the latest accepted candidate; failed candidate
measurements belong here instead.

## Status

The objective to beat matched miniz C `-O3` in every benchmark row was
cancelled on 2026-07-20. The accepted implementation remains `d7ef9cd`, which
wins 7 of 12 measured latency rows. The benchmark and regression gate remain
available as engineering tools, but this objective is no longer a completion
gate for the port or stdlib integration.

## Acceptance rules

- Compressed output must match pinned miniz C through the final byte.
- SX uses --opt 3 --cpu generic; C uses -O3 with the matched CPU.
- Preliminary screens use 9 paired, rotated process trials; acceptance uses 31.
- A row regression is significant when it exceeds both 1% and 10,000 ns.
- Regressions may pass only when outweighed by a meaningful paired-suite gain.
- Never commit a generated benchmark report for a rejected candidate.

## Accepted work

- 51b1a9a: split encode/decode executables and add the literal-dense dynamic
  inflate path. Incompressible level-1 decode improved by about 22.2%.
- ed8f2f9: copy byte-aligned stored-block payloads in at most two spans.
  The 31-pair suite improved by 5,658,000 ns (9.24%) with no significant
  regression.
- d7ef9cd: verify normal and fast matches eight bytes at a time, recovering
  the exact 16-bit mismatch boundary only inside a differing group. The
  31-pair suite improved by 384,000 ns with no significant regression.

## Fast-parser flag experiments rejected after d7ef9cd

All variants kept all six compressed streams byte-identical.

- Flag-aligned literal-run loop: incompressible level-1 encode improved by
  about 477,000 ns in 9 trials, but two significant layout regressions left
  only a 60,000 ns (0.11%) suite gain. Rejected by the tradeoff gate.
- Register-cached flag byte: optimized assembly was 29 lines smaller and
  incompressible level-1 improved by 368,000 ns in the 31-pair run, but shared
  normal-path regressions produced a 21,000 ns (0.04%) suite loss and one
  significant regression. Rejected by the tradeoff gate.
- Adaptive zero-flag mode: incompressible level-1 improved by about 362,000 ns
  in 9 trials, but three significant regressions produced a 6,000 ns (0.01%)
  suite loss. Rejected by the tradeoff gate.
- An exact parser-frequency model of the two benchmark corpora counted
  1,048,181 incompressible level-1 probes, only 231 entries into the candidate
  verifier, and 131,023 flag-slot rollovers. Translating that rollover to C's
  branchless form—three conditional selects/increments and no redundant zero
  store—made LLVM emit the same ARM64 `csel/cinc` sequence and removed five
  assembly lines. All streams remained byte-identical, but the 9-pair suite
  regressed by 188,000 ns (0.34%): 84,000 ns gross gains, 308,000 ns gross
  losses, and one significant row. Incompressible level-1 itself regressed by
  30,000 ns. The source and report were reverted; do not retry branchless flag
  rollover without a way to isolate the fast parser's register allocation from
  the shared inlined codec.
- Carrying miniz's explicit fast-parser `cur_pos` ring index instead of
  recomputing `lookahead_pos & 32767` made the optimized program 25 assembly
  lines smaller and kept all streams exact, but regressed the 9-pair suite by
  230,000 ns (0.42%) with two significant rows. The extra live register changes
  allocation across the inlined codec; do not retry this source-level C local.
- Narrowing only `process_fast`'s bounded SSA state, token indices, and match
  arithmetic to miniz C's 32-bit `mz_uint` width avoided the earlier struct
  layout change and made the program 21 assembly lines smaller. It still
  regressed the byte-exact 9-pair suite by 444,000 ns (0.81%) with three
  significant rows. Width narrowing changes register allocation without making
  ARM64 integer operations cheaper; the candidate was reverted.
- A 128-bit scalar match comparison could not be tested because SX currently
  has no u128 type. This is not a correctness blocker; SIMD vector types are
  the next wider-comparison route to investigate.
- A Vector(16, u8) match verifier emitted genuine ARM64 NEON (two 128-bit
  loads, sub.16b, a two-half OR reduction, and a scalar zero branch) and made
  the optimized program 102 assembly lines smaller. All bytes remained exact,
  but the 9-pair suite regressed by 584,000 ns (1.06%) with three significant
  regressions. The reduction costs more than two scalar u64 comparisons; do
  not retry this formulation without a cheaper horizontal all-equal primitive.
- A temporary SX `vector_all_equal` intrinsic supplied that missing horizontal
  primitive through LLVM vector reductions. Preliminary vector-value forms
  generated real ARM64 NEON and kept all six streams byte-identical, but none
  passed the 9-pair gate:
  - lane equality plus `llvm.vector.reduce.and` selected `cmeq.16b`, invert,
    `umaxv.16b`, and a scalar branch; the suite regressed by 235,000 ns (0.43%)
    with two significant regressions;
  - integer XOR plus `llvm.vector.reduce.umax` selected the minimal
    `eor.16b`, `umaxv.16b`, and scalar branch; the suite regressed by 351,000 ns
    (0.64%) with one significant regression;
  - the same XOR/max test over four u32 lanes selected `eor.16b`, `umaxv.4s`,
    and a scalar branch; the suite regressed by 356,000 ns (0.65%) with one
    significant regression.
  Those preliminary forms reinterpreted `dict + pos + 2` as
  `[*]Vector(...)`, which overstated its alignment to LLVM and is not a sound
  portable contract even though ARM64 accepted the resulting loads. That type
  is a many-pointer to 16-byte vector elements: indexing advances by a whole
  vector and dereferencing promises the vector's ABI alignment. The preliminary
  measurements are therefore excluded from the SIMD conclusion. The final test
  instead used a temporary pointer-taking intrinsic that accepted `[*]u8` and
  emitted two `<16 x i8>` loads with explicit LLVM `align 1`, XOR, reduce-umax,
  and a zero comparison. Optimized assembly still had the minimal direct-load
  sequence (`ldr/ldur q`, `eor.16b`, `umaxv.16b`, `fmov`, branch), with no temporary
  copy, spill, or helper call.
  A focused `--opt 3` audit of the sound form passed all 256 byte-alignment
  pairs, every differing lane, and every exact miniz mismatch-recovery position
  from byte 2 through 257. All six benchmark streams were byte-identical, but
  the 9-pair suite regressed by 394,000 ns (0.73%) with three significant
  regressions. The temporary compiler intrinsic and all miniz SIMD variants
  were reverted.
  On this ARM64 target, even the minimal NEON horizontal test loses to the
  accepted pair of scalar u64 comparisons; retry only with materially different
  hardware evidence or an algorithm that amortizes the reduction.
- An out-of-line libc memcmp equality precheck for the normal parser preserved
  exact bytes but regressed the 9-pair suite by 2,883,000 ns (5.25%) with five
  significant regressions. Saving live parser state across the call and then
  rescanning mismatches costs far more than the accepted inline u64 verifier.
- Precomputing miniz's normal-parser probe-group budgets removed the ARM64
  multiply/shift ceiling-division sequence from the match-search hot path, but
  every byte-exact 9-pair formulation failed the tradeoff gate. Two adjacent
  u32 fields regressed 345,000 ns (0.63%, three significant rows); packing both
  budgets into the original eight-byte field regressed 162,000 ns (0.29%, two
  significant rows); and precomputing only the long-match budget regressed
  752,000 ns (1.36%, five significant rows). The smaller instruction sequence
  changes register allocation and block layout enough to outweigh its local
  saving. A broader conversion of the bounded match/parser state to u32 also
  preserved all six streams but regressed 1,275,000 ns (2.30%, five significant
  rows). All four forms were reverted; do not retry type narrowing or probe
  precomputation without a way to hold the surrounding optimized layout stable.
- A temporary compiler-side `noinline` on `Deflater.process_fast` produced a
  direct `internal fastcc` call with no export, C ABI, wrapper, or function
  pointer. A true paired compiler A/B on identical `d7ef9cd` source improved
  incompressible level-1 encode by about 390,000 ns, but the 9-pair suite gained
  only 59,000 ns (0.11%) and had three significant regressions. Source variants
  measured with both archived and candidate source compiled by that same
  noinline compiler also failed: register-cached flags gained 109,000 ns (0.20%,
  one regression), an exact second-literal batch gained 280,000 ns (0.51%, three
  regressions), and a flag-aligned literal-run loop regressed 23,000 ns (0.04%,
  one regression). All streams stayed byte-identical. The temporary compiler,
  harness, and source changes were reverted; direct internal noinline does not
  solve the layout coupling on this backend.
- Outlining both `Deflater.process_fast` and `Deflater.process_normal` as direct
  internal calls made the program 81 assembly lines smaller and improved
  incompressible level-1 encode by a paired 355,000 ns. It also slowed the
  incompressible normal parser by 1,125,000 ns at level 6 and 1,078,000 ns at
  level 9, for a 1,902,000 ns (3.46%) suite regression with four significant
  rows. C's function split is therefore beneficial only for the fast parser;
  the SX normal parser still benefits materially from inlining.
- Outlining the complete normal fill/parse/flush driver—rather than the
  per-token `process_normal` helper—put `process_normal` inline inside one
  direct `internal fastcc noinline` routine, matching C's function granularity.
  It added only seven optimized assembly lines and kept every stream exact, but
  did not promote mutable deflater state across token iterations. In nine paired
  trials, repetitive level-6 encode gained only 7,000 ns and level-9 lost
  18,000 ns. The suite regressed by 273,000 ns (0.49%) with 93,000 ns gross
  gains, 263,000 ns gross losses, and three significant rows. Whole-driver
  outlining therefore does not solve the normal parser's state-reload problem;
  the compiler, harness, source, and failed report were reverted.
- Combining fast-only direct `internal fastcc noinline` with the exact
  branchless flag rollover isolated the source change inside the outlined
  function and made the program 36 assembly lines smaller than the accepted
  build. All bytes remained exact, but incompressible level-1 gained only
  60,000 ns and the suite gained just 24,000 ns (0.04%), with 172,000 ns gross
  gains, 290,000 ns gross losses, and three significant regressions. The
  branchless form cancels most of the previously measured fast-only outlining
  benefit rather than compounding it. Both temporary compilers, harness
  support, source changes, and generated reports were reverted.
- Outlining only the rare fast-parser 258-byte verifier behind a direct
  `internal fastcc noinline` helper kept the incompressible miss loop call-free
  and all six streams exact. The minimal shared-literal-path form nevertheless
  regressed the 9-pair suite by 89,000 ns (0.16%) with one significant row. An
  early-miss form improved incompressible level-1 by 96,000 ns locally, but its
  duplicated token block regressed the suite by 389,000 ns (0.71%) with five
  significant rows. The compiler and source experiments were reverted; cold
  verifier outlining does not overcome call and layout costs.
- The normal dictionary-fill loop reloads runtime `hash_shift` and `hash_mask`
  fields per byte because current SX IR cannot prove the dictionary/hash stores
  do not alias the deflater object. Hoisting both into SSA locals removed the two
  ARM64 loads and made the program 43 assembly lines smaller, but gained only
  57,000 ns (0.10%) across nine paired trials and had two significant
  regressions. Splitting the loop by `less_memory` mode did produce immediate
  `lsl #4/#5` and mask operations like C, but duplicating the loop regressed the
  suite by 1,428,000 ns (2.58%) with four significant rows. Both forms kept all
  streams exact and were reverted. Revisit only with compiler alias metadata or
  specialization that does not duplicate or perturb the surrounding loop.
- A temporary `unlikely(bool) -> bool` compiler intrinsic emitted
  `llvm.expect.i1(condition, false)`. LLVM `--opt 3` consumed it into 1:2000
  branch weights with no runtime call, and ARM64 block placement moved the
  fast-parser literal-miss token path directly after the 24-bit candidate
  comparison instead of after the 256-byte match verifier. This was the intended
  layout, and all six compressed streams remained byte-identical, but the
  9-pair suite regressed by 216,000 ns (0.40%) with zero paired row gains and
  two significant regressions. Incompressible level-1 itself changed by
  +11,000 ns, so the rearrangement did not recover any of its roughly 1-ms gap.
  The intrinsic, source hint, generated compiler, and benchmark report were
  reverted. Do not retry a static likely/unlikely hint at this branch; input
  classes exercise opposite sides and the whole-function register/layout
  changes cost more than fallthrough placement saves.
- Extracting the complete level-1 fill/parse/flush driver, then applying direct
  compiler-side `internal fastcc noinline` to that driver, put `process_fast`
  inside one C-like out-of-line routine while leaving the normal parser inline.
  The optimized program was 40 assembly lines smaller and every stream remained
  byte-identical. Incompressible level-1 encode improved by a paired 115,000 ns,
  but the 9-pair suite regressed by 221,000 ns (0.40%): 214,000 ns gross gains,
  491,000 ns gross losses, and four significant regressions. Incompressible
  level-6 encode lost 247,000 ns, level-9 lost 187,000 ns in the aggregate, and
  repetitive level-1 encode/decode also regressed. Moving the whole fast driver
  out of line is still insufficient to isolate the shared codec's layout and
  register-allocation effects; the compiler, harness, source, and failed report
  were reverted.

The repeated result is important: avoiding one flag-byte read/shift/write per
literal is locally valuable, but even small inlined fast-path changes move the
shared normal/emitter blocks enough to erase the gain. Revisit this family only
with a genuinely out-of-line or SIMD formulation whose assembly layout is
measured before benchmarking.

## Earlier rejected experiments

- Exact two-literal miss batching improved incompressible level-1 by roughly
  572,000 ns, but its 314,000 ns (0.56%) paired-suite gain was below the
  threshold and it had three significant regressions.
- Variable-length dynamic-emitter batching regressed the suite by 309,000 ns.
- Removing the next table initialization regressed the suite by 208,000 ns.
- A guarded fourth dynamic-emitter literal changed the suite by only -7,000 ns
  (-0.01%), which is noise.
- Internal abi(.c) does not prevent LLVM from inlining process_fast.
  Exported wrappers and indirect process_fast calls degraded performance.
- Direct full-capacity output, packed distance stores, cached Huffman pointers,
  tail-recursion changes, and earlier stored-copy variants did not pass the
  paired benchmark gate.

## Profiling evidence

- A temporary symmetric phase profile of the accepted implementation and
  pinned miniz C isolates the remaining encode deficit inside the LZ token
  parser. After subtracting nested block generation, approximate exclusive
  parser times were 1.777 ms SX versus 1.555 ms C for repetitive level 6,
  1.783 ms versus 1.590 ms for repetitive level 9, and 3.455 ms versus
  2.293 ms for incompressible level 1. These 222,000 ns, 193,000 ns, and
  1,162,000 ns differences closely account for the respective end-to-end
  encode gaps. Setup was only 5,000-17,000 ns, Adler-32 was approximately
  330,000 ns on both implementations, and SX block generation was already
  competitive (and substantially faster than C for incompressible levels 6
  and 9). If this objective resumes, compare the complete parser loops' state
  ownership, aliasing, IR, register retention, and assembly before attempting
  another isolated source transformation.
- A paired one-byte setup/teardown probe measured SX deflate at 16,000-18,000
  ns and miniz C at 10,000-11,000 ns across levels 1, 6, and 9. The roughly
  6,000-8,000 ns fixed-cost gap is far too small to explain either the
  160,000-ns repetitive normal-parser gap or the roughly 1-ms incompressible
  level-1 gap; consolidating the four SX workspace allocations cannot close the
  benchmark by itself.
- Repetitive level-6: normal parser about 1.825 ms, raw deflate about 1.883 ms,
  framed compression about 2.178 ms. The remaining encode gap is parser-bound.
- Exact temporary normal-parser counters show why one optimization cannot cover
  both corpus classes. Repetitive level-6/9 makes only 4,114/4,113 match calls,
  but walks 378,793/379,485 chain entries and executes 182,622/184,785 accepted
  eight-byte verifier groups; 4,059 candidates in each run reach the full
  258-byte match. Incompressible level-6/9 makes 1,048,085 match calls and
  2,620,800/2,633,793 chain probes, but only 1,880 candidates reach a verifier
  group. Repetitive performance is chain/long-match bound; incompressible
  performance is per-position setup and failed-chain bound.
- Incompressible level-1: fast parser about 1.7 ms, dynamic block emission
  about 0.8 ms, Adler-32 about 0.34 ms.
- C and SX Adler-32 timings are approximately equal.
- A one-shot full-capacity destination does not improve incompressible level-1.
- Level-1 incompressible input emits dynamic blocks: its 64 KiB token block is
  larger than the 32 KiB history window, so stored fallback is not selected.
