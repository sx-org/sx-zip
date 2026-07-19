#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
run_sx() {
    if [[ -n "${SX_OPT:-}" ]]; then
        "$SX_BIN" run "$1" --opt "$SX_OPT"
    else
        "$SX_BIN" run "$1"
    fi
}

tests/upstream_surface.sh
tests/native_dependency_surface.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/optional_builds.sh
run_sx tests/smoke.sx
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/core_surface.sh
run_sx tests/codec.sx
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/strategy.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/flush.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/png_exact.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/tdefl_flags.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/tdefl_lifecycle.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/tinfl_status.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/lowlevel_adapters.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/zlib_lifecycle.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/zlib_helpers.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/inflate_matrix.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/zip_exact.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/zip_file_adapter.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/zip_file_init_matrix.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/zip_file_lifecycle.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/zip_stdio_failures.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/zip_alias_surface.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/zip_inplace_partial.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/zip_lookup_matrix.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/zip_errors.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/zip_corruption.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/zip_lifecycle.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/zip_file_update.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/zip_writer_failures.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/zip_writer_parameters.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/zip_add_from_reader_matrix.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/zip_finalize_flush.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/zip64_descriptor.sh
run_sx tests/gzip.sx
run_sx tests/zip.sx
run_sx tests/zip_source_bounded.sx
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/zip_source_failures.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/zip_iterator.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/zip_source_trace.sh
run_sx tests/zip_seek.sx
run_sx tests/malformed.sx
run_sx tests/truncation.sx
run_sx tests/boundary.sx
run_sx tests/codes.sx
run_sx tests/random.sx
run_sx tests/upstream_fuzz.sx
run_sx tests/stream.sx
run_sx tests/png.sx
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/allocation.sh
SX_BIN="$SX_BIN" SX_OPT="${SX_OPT:-}" tests/interop.sh
