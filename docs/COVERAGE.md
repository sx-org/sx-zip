# Upstream miniz coverage ledger

This ledger is the completion authority for the port. “100%” means that every
reachable library behavior in the pinned upstream source has a native sx
equivalent and differential evidence. A different sx API shape is allowed; an
omitted behavior is not. The project cannot be called complete while any row
below is open or while the checked-in public-symbol manifest differs from the
pinned headers.

## Immutable source baseline

Reference: miniz 3.1.2 commit
`77d0dce8627735138c51770d1799a1ef48f2117d`.

| File | Lines | SHA-256 |
| --- | ---: | --- |
| `miniz.c` | 646 | `f23204efee2bfaffb72a974d2e28c64c3c1d9787144b5c5890f129c465e467bd` |
| `miniz.h` | 615 | `f0b67217bdd8e45b74eb9b7c2c7ae20ed6760a1d76f3276b93617039ebd082df` |
| `miniz_common.h` | 89 | `f1ba29821c8caef83585b328196aa346c5da7c82890a70a3d10d77c6cec0ed39` |
| `miniz_tdef.c` | 1,602 | `85b93205232d51d092f24041934d951f11f9edc02df1f475e8ceb7b281a1e7f6` |
| `miniz_tdef.h` | 199 | `d62da02953c3834dfcbde4db026c6d9345013ef9b0e53185131016a3a4e48c33` |
| `miniz_tinfl.c` | 778 | `2296ebd21ef9af5ebbefd5b454d4bb67d8fb4af2d24945e7fb32114374f75a76` |
| `miniz_tinfl.h` | 150 | `da4850920fdf09f8877d9affabe1ebba1852ce4b1ebbd121f2ba7a7ea8e57e5d` |
| `miniz_zip.c` | 4,895 | `b70e502c425e2a31146178c1466dccaf1874390009950774be2be9bd4ec908a9` |
| `miniz_zip.h` | 454 | `b062a5def545445c73fd1d664e35a89165dfb0d996ab848f507244e819cfcf04` |
| **Total** | **9,428** | |

`tests/upstream_surface.sh` pins the revision and compares all 112
`MINIZ_EXPORT` declarations plus the six public streaming-extract declarations
against `tests/upstream_symbols.txt`. This detects upstream surface drift; it
does not by itself prove implementation coverage.

## Completion rules

A row is complete only when all of these are true:

1. Every named upstream behavior has an sx mapping, including success, error,
   reset/reuse, partial-input/output, and ownership behavior.
2. Differential tests exercise the mapping against the pinned C source. For
   output-producing codec paths, exact bytes are required wherever upstream is
   deterministic.
3. All applicable flags, strategies, flush modes, fast/portable compile-time
   paths, boundary values, allocation failures, and malformed inputs are
   represented.
4. Both default and `--opt 3` sx suites pass. The C oracle is built both in its
   portable configuration and its fastest little-endian/unaligned/64-bit
   configuration.
5. No build or runtime dependency is added to the sx implementation. C, Python,
   and system archive tools remain test-only oracles.

## Public entry-point ledger

The manifest contains each of the 118 names individually. The grouped rows
below are complete only when every listed name has explicit mapping and test
evidence.

| State | Area | Upstream entry points | Required sx coverage/evidence |
| --- | --- | --- | --- |
| Complete | Version, allocation, error strings | `mz_version`, `mz_free`, `mz_error` | The version and every known/unknown zlib-compatible error string match an exact C transcript; allocator-native release and allocation-failure status contracts pass the exhaustive allocation gate. |
| Complete | Checksums | `mz_adler32`, `mz_crc32` | Exact null-source initialization, 1/4/8/5,552-byte boundaries, incremental values, the eight-byte Adler loop, and miniz's four-byte-unrolled 256-entry CRC path match C. |
| Complete | zlib deflate lifecycle | `mz_deflateInit`, `mz_deflateInit2`, `mz_deflateReset`, `mz_deflate`, `mz_deflateEnd`, `mz_deflateBound` | Caller-owned init/init2, both supported raw/zlib windows, invalid method/window/memory parameters, reset/end, empty/no-output/post-finish calls, totals/Adler, allocation failure, and none/partial/sync/full/finish byte/status behavior pass exact C transcripts. |
| Complete | zlib compression helpers | `mz_compress`, `mz_compress2`, `mz_compressBound` | A consolidated exact C transcript covers the public default, levels −2/−1/0/1/6/9/10/11/99, fixed destinations, short output, bounds, fastest bytes, and allocator failure. |
| Complete | zlib inflate lifecycle | `mz_inflateInit`, `mz_inflateInit2`, `mz_inflateReset`, `mz_inflate`, `mz_inflateEnd` | Exact transcripts cover caller-owned raw/zlib init, invalid windows, reset/end, all five flush values, partial/sync aliasing, first/repeated finish, zero/short output, the 32 KiB wrapping dictionary and pending-output drain, totals/Adler, allocation failure, malformed headers/blocks, preset-dictionary rejection, bad checksums, truncation, and failed-state follow-up calls. |
| Complete | zlib decompression helpers | `mz_uncompress`, `mz_uncompress2` | The consolidated helper transcript matches short output, consumed-source reporting, accepted trailing input, strict framing, malformed/truncated data, exact statuses/bytes, and allocation failure. |
| Complete | tdefl lifecycle | `tdefl_init`, `tdefl_compress`, `tdefl_compress_buffer`, `tdefl_get_prev_return_status`, `tdefl_get_adler32`, `tdefl_create_comp_flags_from_zip_params` | Exact flag construction, caller windows, levels −1/0/1/6/10, all strategies, and every flush mode pass. A dedicated transcript matches zero-output finish, 17-byte output drains, retained status, Adler, callback chunking/CRC, callback rejection, failure follow-up, post-done calls, and finish-mode mismatch in default and `--opt 3`. |
| Complete | tdefl allocation/output adapters | `tdefl_compress_mem_to_heap`, `tdefl_compress_mem_to_mem`, `tdefl_compress_mem_to_output`, `tdefl_compressor_alloc`, `tdefl_compressor_free` | Allocator, tiny fixed-window, callback, owned-result, retained status/Adler, callback failure, and one-at-a-time allocation failure forms pass with exact bytes and zero leaks. |
| Complete | PNG writer | `tdefl_write_image_to_png_file_in_memory`, `tdefl_write_image_to_png_file_in_memory_ex` | Both public entry points, channels 1–4, levels 0/1/6/10 and above-10 clamping, both row directions, every upstream dimension/channel rejection, exact C bytes, and all 20 allocation failures pass. The native stride extension separately rejects short/overlapping/overflowing layouts. |
| Complete | tinfl core | `tinfl_decompress` | Raw/zlib, wrapping/non-wrapping, 7-byte input/13-byte output coroutine boundaries, every machine path, Adler mismatch, truncation, reserved block/literal/distance codes, stored LEN/NLEN mismatch, impossible distance, oversubscribed dynamic trees, and exact negative-status consumed/produced/Adler values pass. |
| Complete | tinfl adapters | `tinfl_decompress_mem_to_heap`, `tinfl_decompress_mem_to_mem`, `tinfl_decompress_mem_to_callback`, `tinfl_decompressor_alloc`, `tinfl_decompressor_free` | Owned, fixed-window, callback, output exhaustion, callback failure, terminal lifecycle, and one-at-a-time allocation failures pass with zero leaks. |
| Complete | ZIP reader initialization | `mz_zip_reader_init`, `mz_zip_reader_init_mem`, `mz_zip_reader_init_file`, `mz_zip_reader_init_file_v2`, `mz_zip_reader_init_cfile`, `mz_zip_reader_end`, `mz_zip_zero_struct` | Memory, bounded callback, path, subrange/prefix, and borrowed-handle forms match C. The exact gates cover 4 KiB backward EOCD reads, short reads, central-only retention, invalid/repeated init/end, retained state, every start/size form, seek/read failures, and owned close failure. |
| Complete | ZIP lifecycle and introspection | `mz_zip_end`, `mz_zip_get_mode`, `mz_zip_get_type`, `mz_zip_reader_get_num_files`, `mz_zip_get_archive_size`, `mz_zip_get_archive_file_start_offset`, `mz_zip_get_cfile`, `mz_zip_read_archive_data`, `mz_zip_get_central_dir_size`, `mz_zip_is_zip64` | Exact memory, heap, callback, path, and CFILE transcripts cover invalid/reading/writing/finalized modes, retained values after end, central sizes, ZIP64, raw reads, readable writers, reader conversion, and path/CFILE seek/flush/close failures. |
| Complete | ZIP errors | `mz_zip_set_last_error`, `mz_zip_peek_last_error`, `mz_zip_clear_last_error`, `mz_zip_get_last_error`, `mz_zip_get_error_string` | All 33 strings and unknown fallback match. Exact reader, writer, callback, iterator, seek, allocation, filesystem, storage-phase, corruption, validation, retry, and cleanup transcripts cover every native error value and state operation. |
| Complete | ZIP lookup/stat | `mz_zip_reader_is_file_a_directory`, `mz_zip_reader_is_file_encrypted`, `mz_zip_reader_is_file_supported`, `mz_zip_reader_get_filename`, `mz_zip_reader_locate_file`, `mz_zip_reader_locate_file_v2`, `mz_zip_reader_file_stat` | Full C enumeration covers case/default/ignore-path/comment lookup, every index/name, metadata/extras/attributes, encrypted/unsupported entries, heap-sort/binary duplicate selection, and do-not-sort linear selection. |
| Complete | ZIP memory extraction | `mz_zip_reader_extract_to_mem_no_alloc`, `mz_zip_reader_extract_file_to_mem_no_alloc`, `mz_zip_reader_extract_to_mem`, `mz_zip_reader_extract_file_to_mem`, `mz_zip_reader_extract_to_heap`, `mz_zip_reader_extract_file_to_heap` | Index/name, raw unknown methods, fixed/owned buffers, short output, unsupported flags, CRC/size/local-header failures, all 75 ZIP32/ZIP64 corruption variants, signed/signatureless descriptors, and allocation failure match C. The 1 MiB callback case uses caller scratch, exact 4 KiB reads, and zero extraction allocations. |
| Complete | ZIP callback/iterator extraction | `mz_zip_reader_extract_to_callback`, `mz_zip_reader_extract_file_to_callback`, `mz_zip_reader_extract_iter_new`, `mz_zip_reader_extract_file_iter_new`, `mz_zip_reader_extract_iter_read`, `mz_zip_reader_extract_iter_free` | Stored/deflated/raw callback and 997-byte iterator traces match C, including 64 KiB input, 32 KiB ring splits, final zero read, invalid names/indices, zero/partial/full/drained/no-read cleanup, deferred CRC failure, unsupported variants, short source reads, rejected sinks, and repeated failed cleanup. |
| Complete | ZIP streaming seek extraction | `mz_zip_streaming_extract_begin`, `mz_zip_streaming_extract_get_size`, `mz_zip_streaming_extract_get_cur_ofs`, `mz_zip_streaming_extract_seek`, `mz_zip_streaming_extract_read`, `mz_zip_streaming_extract_end` | The native implementation of upstream's declaration-only TODO surface covers stored, deflated, raw-compressed, and empty entries; forward/backward/random seek, zero/EOF reads, invalid bounds/index, reset/replay, end/repeated-end/read-after-end, offsets, output, and leak-free cleanup pass. |
| Complete | ZIP filesystem extraction | `mz_zip_reader_extract_to_file`, `mz_zip_reader_extract_file_to_file`, `mz_zip_reader_extract_to_cfile`, `mz_zip_reader_extract_file_to_cfile` | Path and borrowed-handle sinks cover index/name lookup and all extraction flags through the common extractor, exact full-write loops, directory/unsupported rejection, open/write/closed-handle/close failures, error state, and DOS timestamp restoration. |
| Complete | ZIP validation | `mz_zip_validate_file`, `mz_zip_validate_archive`, `mz_zip_validate_mem_archive`, `mz_zip_validate_file_archive` | Header-only/full, memory/path, locate, CRC-enabled/skipped, ordinary/ZIP64, all unsupported formats, and the 75-case EOCD/ZIP64/central/local corruption matrix match exact C success and error classes. |
| Complete | ZIP writer initialization | `mz_zip_writer_init`, `mz_zip_writer_init_v2`, `mz_zip_writer_init_heap`, `mz_zip_writer_init_heap_v2`, `mz_zip_writer_init_file`, `mz_zip_writer_init_file_v2`, `mz_zip_writer_init_cfile`, `mz_zip_writer_init_from_reader`, `mz_zip_writer_init_from_reader_v2` | Heap/memory, callback, path, borrowed handle, ZIP64, prefix reserve, start offsets, readable writers, invalid/reinit forms, allocation/seek/open/write/close failures, and reader conversion/read-while-writing match exact C state and bytes. |
| Complete | ZIP writer add | `mz_zip_writer_add_mem`, `mz_zip_writer_add_mem_ex`, `mz_zip_writer_add_mem_ex_v2`, `mz_zip_writer_add_read_buf_callback`, `mz_zip_writer_add_file`, `mz_zip_writer_add_cfile`, `mz_zip_writer_add_from_zip_reader` | Memory/callback/path/handle/raw-copy forms cover null/empty/oversize names, extras/comments boundaries, invalid levels/flags/source sizes, stored/deflated/precompressed data, patch/descriptors, all timestamps, alignment, automatic ZIP64 size/offset, allocation and every source/write/seek/close phase failure with exact C output/state. |
| Complete | ZIP writer finalize | `mz_zip_writer_finalize_archive`, `mz_zip_writer_finalize_heap_archive`, `mz_zip_writer_end` | Descriptor, patched, raw, aligned, forced/automatic size-count-offset ZIP64, and 4,097-byte prefix archives are byte-identical. Every local/name/payload/descriptor/central/EOCD rejection, rollback, retry, retained state/count, allocation, final stdio flush, owned close, and repeated end transition matches C. |
| Complete | ZIP in-place helpers | `mz_zip_add_mem_to_archive_file_in_place`, `mz_zip_add_mem_to_archive_file_in_place_v2`, `mz_zip_extract_archive_file_to_heap`, `mz_zip_extract_archive_file_to_heap_v2` | Existing/non-existing updates, v2 comments/errors, heap extraction, raw-record preservation, exact rebuilt bytes, file limits, and injected full-block/final-tail partial-write cleanup/error behavior match C. |

## Internal/configuration path ledger

These are required even when several paths feed one public sx API.

The implementation-function inventory is also mechanical: the source audit
extracts all 180 function definitions from the four pinned `.c` files, unions
them with the six header-only streaming-extract declarations, and requires all
186 names to appear in this ledger. This prevents private helper code from
being hidden behind public-symbol coverage.

| State | Source implementation functions | Required sx coverage/evidence |
| --- | --- | --- |
| Complete | `tdefl_radix_sort_syms`, `tdefl_calculate_minimum_redundancy`, `tdefl_huffman_enforce_max_code_size`, `tdefl_optimize_huffman_table`, `tdefl_start_dynamic_block`, `tdefl_start_static_block`, `tdefl_compress_lz_codes`, `tdefl_compress_block`, `tdefl_flush_block`, `tdefl_find_match`, `tdefl_compress_fast`, `tdefl_record_literal`, `tdefl_record_match`, `tdefl_compress_normal`, `tdefl_flush_output_buffer`, `tdefl_output_buffer_putter` | Native Huffman optimization, block selection/emission, both parsers, match search, packed tokens, output draining, and helper growth paths are covered by exact codec, lifecycle, machine-path, allocation, and 210-stream all-symbol gates. |
| Complete | `tinfl_clear_tree` | Native tree reset/build state is covered across stored/fixed/dynamic, malformed, resumable, allocation, and all machine paths. |
| Complete | `mz_write_le16`, `mz_write_le32`, `mz_write_le64`, `mz_zip_array_range_check`, `mz_zip_array_init`, `mz_zip_array_clear`, `mz_zip_array_ensure_capacity`, `mz_zip_array_reserve`, `mz_zip_array_resize`, `mz_zip_array_ensure_room`, `mz_zip_array_push_back` | Native endian writers and allocator-backed arrays cover exact archive bytes, all growth boundaries, and exhaustive allocation failure. |
| Complete | `mz_utf8z_to_widechar`, `mz_fopen`, `mz_freopen`, `mz_stat`, `mz_stat64`, `mz_file_read_func_stdio`, `mz_zip_file_read_func`, `mz_zip_file_write_callback`, `mz_zip_file_write_func`, `mz_zip_dos_to_time_t`, `mz_zip_time_t_to_dos_time`, `mz_zip_get_file_modified_time`, `mz_zip_set_file_times` | Exact POSIX path/handle/current/file/specified/extracted timestamp transcripts cover seek/read/write/flush/close failures. Reachability IR gates prove the Windows UTF-8/wide path, file-I/O, seek, attribute, and time APIs and the Linux, macOS, iOS, iOS-simulator, wasm/Emscripten, and Android filesystem surfaces. |
| Complete | `mz_zip_set_error`, `mz_zip_reader_init_internal`, `mz_zip_reader_filename_less`, `mz_zip_reader_sort_central_dir_offsets_by_filename`, `mz_zip_reader_locate_header_sig`, `mz_zip_reader_eocd64_valid`, `mz_zip_reader_read_central_dir`, `mz_zip_reader_end_internal`, `mz_zip_mem_read_func`, `mz_zip_get_cdh`, `mz_zip_file_stat_internal`, `mz_zip_string_equal`, `mz_zip_filename_compare`, `mz_zip_locate_file_binary_search` | Exact lifecycle, error, lookup, source-trace, and 75-case corruption transcripts cover bounded 4 KiB EOCD/ZIP64 scanning, central-only callback storage, indexing/stat, sorted binary lookup, duplicate selection, unsorted linear lookup, and teardown. |
| Complete | `mz_zip_reader_extract_to_mem_no_alloc1`, `mz_zip_compute_crc32_callback` | Fixed/callback extraction, caller-owned input scratch, CRC-enabled/disabled validation, storage failures, raw/stored/deflated entries, malformed records, and deferred iterator failure match C. A 1 MiB callback source matches C's exact 4 KiB read sequence with zero extraction allocations. |
| Complete | `mz_zip_heap_write_func`, `mz_zip_writer_end_internal`, `mz_zip_writer_add_put_buf_callback`, `mz_zip_writer_create_zip64_extra_data`, `mz_zip_writer_create_local_dir_header`, `mz_zip_writer_create_central_dir_header`, `mz_zip_writer_add_to_central_dir`, `mz_zip_writer_validate_archive_name`, `mz_zip_writer_compute_padding_needed_for_file_alignment`, `mz_zip_writer_write_zeros`, `mz_zip_writer_update_zip64_extension_block` | Exact heap/callback/file output covers local, central, descriptor, padding, ZIP64, update, retry, and finalization paths. Phase-failure transcripts cover fixed-header/name separation, source reads, compression output, random header patching, central/EOCD writes, logical-size rollback, allocation, flush, seek, close, and retained counts. |

| State | Source behavior | Required variants |
| --- | --- | --- |
| Complete | tdefl parsing | Level-1 4 KiB fast hash, normal rolling hash chains, greedy/lazy, deterministic and pattern-seeded nondeterministic initialization, filtered, RLE, Huffman-only, and every public probe level are byte-identical across normal/less-memory and fastest/portable machine builds. |
| Complete | tdefl block construction | Raw/stored, static/fixed, dynamic, block-size selection, stored fallback, and code-length RLE match exact C streams across the strategy and boundary oracles. A constructed 210-stream gate emits the base and maximum extra-bit value of all 29 length symbols and all 30 distance symbols through both fixed and dynamic tdefl emitters; native sx inflate, Python zlib, and pinned upstream tinfl independently recover every stream. |
| Complete | tdefl flushing | No/partial/sync/full/finish semantics, the partial/sync alias, full-flush dictionary reset, callback backpressure/failure, and tiny caller windows have exact output/status evidence. |
| Complete | tdefl machine paths | portable byte accesses, unaligned accesses, `MINIZ_UNALIGNED_USE_MEMCPY`, 32-bit registers, 64-bit packed LZ emission, less-memory configuration. Exact outputs pass for portable, unaligned, memcpy, forced-32-bit, 64-bit packed, and `TDEFL_LESS_MEMORY` C builds. |
| Complete | tinfl block decode | Stored/fixed/dynamic blocks, fast lookup and slow fallback, every legal length/distance symbol, reserved symbols, impossible distances, and wrapping/non-wrapping output are covered by exact oracles and constructed streams. |
| Complete | tinfl framing/status | Raw/zlib headers, preset-dictionary rejection, Adler calculation/mismatch, has-more-input behavior, every status, negative-status progress, and resumable coroutine boundaries match C. |
| Complete | tinfl machine paths | portable byte accesses, unaligned accesses/memcpy, 32-bit and 64-bit bit-buffer paths. Per-call status/consumed/produced traces match all four C machine builds. |
| Complete | ZIP32 and ZIP64 parsing | EOCD search, locator/EOCD64, ZIP64 extended fields, 32-bit and ZIP64 signed/signatureless descriptors, prepended/trailing/subrange offsets, central/local consistency, masked headers, multidisk rejection, and all 75 corruption variants match C. |
| Complete | ZIP32 and ZIP64 writing | Forced ZIP64, patched and descriptor headers, raw compressed input, separate local/central extras, alignment, a 4,097-byte reserved prefix, automatic 64-bit size/count/offset transitions, parameter boundaries, rollback, and every injected output phase are byte-identical to C. |
| Complete | ZIP flags | Exact gates cover all ten public ZIP flags with meaningful state/output behavior: case-sensitive, ignore-path, compressed-data across fixed/callback/iterator forms, do-not-sort duplicate selection, validate-locate, validate-headers-only, forced ZIP64, write-allow-reading, read-allow-writing in-place conversion, ASCII filename, and header-set-size. |
| Complete | ZIP storage backends | Memory/heap, bounded random-read callbacks, sequential/random-write callbacks, filesystem paths, borrowed handles, subranges, mutable-file conversion, and write-from-reader raw copy have exact C evidence. Large initialization retains only central metadata; read-while-writing, seek/write/flush/close failure, short I/O, and cleanup paths are covered. |
| Complete | ZIP unsupported parity | Ordinary/strong encryption, compressed-patch, unsupported methods, raw unknown methods, masked local headers, multidisk forms, validation, extraction, callback/iterator, path, and handle adapters match C success/error behavior. |
| Complete | Optional build surfaces | Compile/run gates cover no-malloc, no-stdio, no-time, reader-only/no-writing, no-deflate, no-inflate, no-archive, and no-zlib/high-level modular imports without changing enabled behavior. |

## Compile-time configuration ledger

`tests/upstream_surface.sh` mechanically extracts every `MINIZ_*` identifier
from the pinned implementation and headers and requires all 18 names below.
Platform detection, declaration aliases, and feature-elision switches remain in
the inventory even when their native sx mapping is an import boundary instead
of a preprocessor option.

| State | Upstream configuration | Required sx mapping/evidence |
| --- | --- | --- |
| Complete | `MINIZ_X86_OR_X64_CPU`, `MINIZ_LITTLE_ENDIAN`, `MINIZ_HAS_64BIT_REGISTERS`, `MINIZ_USE_UNALIGNED_LOADS_AND_STORES`, `MINIZ_UNALIGNED_USE_MEMCPY` | Portable/unaligned, memcpy, 32-bit, and 64-bit codec C builds have exact status and byte transcripts against the single target-correct native sx implementation. |
| Complete | `MINIZ_DISABLE_ZIP_READER_CRC32_CHECKS` | Per-call native extraction and validation options exercise both CRC-enabled and CRC-disabled behavior. |
| Complete | `MINIZ_NO_TIME` | Native callers can omit current/source timestamps; exact no-time C bytes pass and the filesystem/time-free module boundary has a compile/run gate. |
| Complete | `MINIZ_NO_MALLOC`, `MINIZ_NO_STDIO`, `MINIZ_NO_ARCHIVE_APIS`, `MINIZ_NO_ARCHIVE_WRITING_APIS`, `MINIZ_NO_DEFLATE_APIS`, `MINIZ_NO_INFLATE_APIS`, `MINIZ_NO_ZLIB_APIS` | Dependency-free native module boundaries for allocator-free, filesystem-free, reader-only, codec-only, and low-level-only consumers compile and run in every meaningful combination. |
| Complete | `MINIZ_HEADER_FILE_ONLY`, `MINIZ_EXPORT` | Native single-source/module visibility equivalents are documented and compile-tested. |
| Complete | `MINIZ_NO_ZLIB_COMPATIBLE_NAME`, `MINIZ_NO_ZLIB_COMPATIBLE_NAMES` | Both upstream spelling surfaces remain in the mechanical manifest and a compile gate proves the native namespace introduces no ambient zlib-compatible aliases. |

## Complete proof set

Every row above is `Complete`. `tests/upstream_surface.sh` fails closed if a
row returns to `Partial` or `Open`, if any pinned public or private function
disappears from the ledger, or if any compile-time configuration identifier is
lost. The behavioral suites then require the named exact, malformed-input,
allocation, interoperability, optional-build, and platform-reachability
evidence in both default and `--opt 3` compiler modes.

Current differential evidence additionally includes exact version/error/checksum/
bound, zlib-helper, full inflate-flush/failure, and allocation-status transcripts; fastest/portable/memcpy/
32-bit/64-bit and less-memory C codec builds; levels −1/0/1/6/10; all five
compression strategies; raw sync/full flushes; all-six nanosecond benchmark
streams; exact PNG bytes; exact tinfl coroutine traces; synthetic and upstream
ZIP64 inputs; an exact zlib-compatible init/reset/finish/end status transcript;
an exact caller-window/callback tdefl lifecycle transcript;
and byte-identical descriptor, patched, precompressed, aligned, update-from-
reader, forced-ZIP64, automatic-offset-ZIP64, automatic-count-ZIP64, and
automatic-size-ZIP64 writer archives; pattern-seeded nondeterministic tdefl;
all 210 fixed/dynamic tdefl length/distance base and extra-bit-maximum streams;
exact callback ZIP initialization/extraction/iterator read and output traces;
an exact ZIP init/reinit/readable-writer/finalize/end transcript; exact raw-
compressed callback/iterator and 4 KiB no-allocation callback reads;
reserved/malformed tinfl status progress; current/specified/source-file and
extraction-restored timestamps; and absolute-offset/zero-size CFILE semantics.
A binary ZIP error oracle additionally
matches all error strings/state operations and representative unsupported and
corrupt reader transitions. The default and `--opt 3` suites pass with these
gates.

A source-instrumented default C run executes every one of the 176 standalone
functions emitted by clang from the pinned source (the mechanical source
inventory remains 180 definitions plus six declaration-only streaming APIs).
Line coverage is retained as diagnostic evidence rather than used as a scope
substitute: unreachable target/config branches are covered by their dedicated
C build variants and native target reachability gates.
