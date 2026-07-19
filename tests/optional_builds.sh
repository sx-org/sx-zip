#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"

tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT
cc -std=c99 -fno-builtin -c tests/no_malloc_guard.c -o "$tmp_dir/no_malloc_guard.o"

MINIZ_SRC="${MINIZ_SRC:-../miniz}"
if [[ -d "$MINIZ_SRC/.git" ]]; then
    build_c_config() {
        local name=$1
        shift
        cc -std=c99 -O2 -Itests -I"$MINIZ_SRC" "$@" \
            tests/optional_config_probe.c "$MINIZ_SRC/miniz.c" \
            "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" \
            "$MINIZ_SRC/miniz_zip.c" -o "$tmp_dir/config-$name"
        "$tmp_dir/config-$name"
    }

    build_c_config no-malloc -DMINIZ_NO_MALLOC
    build_c_config no-stdio -DMINIZ_NO_STDIO
    build_c_config no-time -DMINIZ_NO_TIME
    build_c_config no-archive -DMINIZ_NO_ARCHIVE_APIS
    build_c_config no-writing -DMINIZ_NO_ARCHIVE_WRITING_APIS
    build_c_config no-deflate -DMINIZ_NO_DEFLATE_APIS
    build_c_config no-inflate -DMINIZ_NO_INFLATE_APIS
    build_c_config no-zlib -DMINIZ_NO_ZLIB_APIS
    build_c_config compatible-singular -DMINIZ_NO_ZLIB_COMPATIBLE_NAME
    build_c_config compatible-plural -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES
    build_c_config header-file-only -DMINIZ_HEADER_FILE_ONLY
    build_c_config minimal \
        -DMINIZ_NO_MALLOC -DMINIZ_NO_STDIO -DMINIZ_NO_TIME \
        -DMINIZ_NO_DEFLATE_APIS -DMINIZ_NO_INFLATE_APIS \
        -DMINIZ_NO_ZLIB_APIS

    cc -std=c99 -O2 -DMINIZ_NO_TIME -Itests -I"$MINIZ_SRC" \
        tests/optional_no_time_miniz.c "$MINIZ_SRC/miniz.c" \
        "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" \
        "$MINIZ_SRC/miniz_zip.c" -o "$tmp_dir/no-time-c"
    "$tmp_dir/no-time-c" > "$tmp_dir/no-time-c.zip"
    if [[ -n "${SX_OPT:-}" ]]; then
        "$SX_BIN" run tests/optional_no_time.sx --opt "$SX_OPT" > "$tmp_dir/no-time-sx.zip"
    else
        "$SX_BIN" run tests/optional_no_time.sx > "$tmp_dir/no-time-sx.zip"
    fi
    cmp "$tmp_dir/no-time-c.zip" "$tmp_dir/no-time-sx.zip"
fi

build_and_run() {
    local source=$1
    local name=$2
    if [[ "$name" == no-malloc-no-stdio-no-time ]]; then
        if [[ -n "${SX_OPT:-}" ]]; then
            "$SX_BIN" build "$source" --opt "$SX_OPT" --lflags "$tmp_dir/no_malloc_guard.o" -o "$tmp_dir/$name"
        else
            "$SX_BIN" build "$source" --lflags "$tmp_dir/no_malloc_guard.o" -o "$tmp_dir/$name"
        fi
    else
        if [[ -n "${SX_OPT:-}" ]]; then
            "$SX_BIN" build "$source" --opt "$SX_OPT" -o "$tmp_dir/$name"
        else
            "$SX_BIN" build "$source" -o "$tmp_dir/$name"
        fi
    fi
    "$tmp_dir/$name"
}

build_and_run tests/optional_builds.sx no-malloc-no-stdio-no-time
build_and_run tests/optional_reader_only.sx reader-only
build_and_run tests/optional_deflate_only.sx deflate-only
build_and_run tests/optional_inflate_only.sx inflate-only
build_and_run tests/optional_header_export.sx header-export

# Cross-compile a reachable filesystem surface for every target family used by
# sx. Checking emitted imports ensures path/handle/time/delete adapters survive
# optimization instead of passing because an unused module was merely parsed.
emit_surface_ir() {
    local source=$1
    local target=$2
    local output=$3
    if [[ -n "${SX_OPT:-}" ]]; then
        "$SX_BIN" ir "$source" --target "$target" --opt "$SX_OPT" > "$output"
    else
        "$SX_BIN" ir "$source" --target "$target" > "$output"
    fi
}

emit_surface_ir tests/windows_surface.sx windows "$tmp_dir/windows.ll"
for symbol in CreateFileW ReadFile WriteFile SetFilePointerEx GetFileAttributesW DeleteFileW GetFileTime SetFileTime; do
    if ! rg -q "@$symbol" "$tmp_dir/windows.ll"; then
        echo "Windows archive surface did not retain $symbol" >&2
        exit 1
    fi
done
if ! rg -q 'target triple = "x86_64-unknown-windows-msvc"' "$tmp_dir/windows.ll"; then
    echo "Windows archive surface used the wrong target triple" >&2
    exit 1
fi

check_posix_surface() {
    local target=$1
    local source=$2
    local triple=$3
    local with_time=$4
    local ir="$tmp_dir/$target.ll"
    emit_surface_ir "$source" "$target" "$ir"
    for symbol in open read write close lseek; do
        if ! rg -q "@$symbol" "$ir"; then
            echo "$target archive surface did not retain $symbol" >&2
            exit 1
        fi
    done
    if ! rg -q "target triple = \"$triple" "$ir"; then
        echo "$target archive surface used the wrong target triple" >&2
        exit 1
    fi
    if [[ "$with_time" == yes ]]; then
        for symbol in fstat localtime_r mktime utime; do
            if ! rg -q "@$symbol" "$ir"; then
                echo "$target archive timestamp surface did not retain $symbol" >&2
                exit 1
            fi
        done
    fi
}

check_posix_surface linux   tests/windows_surface.sx 'x86_64-unknown-linux-gnu"' yes
check_posix_surface macos   tests/windows_surface.sx 'aarch64-apple-darwin' yes
check_posix_surface ios     tests/windows_surface.sx 'arm64-apple-ios14.0"' yes
check_posix_surface ios-sim tests/windows_surface.sx 'arm64-apple-ios14.0-simulator"' yes
check_posix_surface wasm    tests/windows_surface.sx 'wasm32-unknown-emscripten"' no
check_posix_surface android tests/android_surface.sx 'aarch64-unknown-linux-android21"' yes

# The all-in-one consumer is stack-allocated and memory-only. Its undefined
# symbols must not contain allocator, stdio, filesystem metadata, or time APIs.
if nm -u "$tmp_dir/no-malloc-no-stdio-no-time" |
    rg -q '_(malloc|calloc|realloc|free|fopen|freopen|fread|fwrite|fclose|fseek|ftell|stat|fstat|utime|time|localtime|mktime)(\$|@|\b)'; then
    echo "optional build unexpectedly links allocator/stdio/time symbols" >&2
    nm -u "$tmp_dir/no-malloc-no-stdio-no-time" >&2
    exit 1
fi

# Feature-elided consumers should not retain opposite codec or ZIP machinery.
if nm "$tmp_dir/reader-only" | rg -q '(Deflater|Tdefl|ZipWriter|create_zip)'; then
    echo "reader-only build retained writer/deflate machinery" >&2
    exit 1
fi
if nm "$tmp_dir/deflate-only" | rg -q '(Inflater|Tinfl|ZipReader|ZipWriter)'; then
    echo "deflate-only build retained inflate/archive machinery" >&2
    exit 1
fi
if nm "$tmp_dir/inflate-only" | rg -q '(Deflater|Tdefl|ZipReader|ZipWriter)'; then
    echo "inflate-only build retained deflate/archive machinery" >&2
    exit 1
fi

probe_status=0
if [[ -n "${SX_OPT:-}" ]]; then
    "$SX_BIN" build tests/optional_compatible_name_probe.sx --opt "$SX_OPT" -o "$tmp_dir/compatible-name" >"$tmp_dir/probe.out" 2>&1 || probe_status=$?
else
    "$SX_BIN" build tests/optional_compatible_name_probe.sx -o "$tmp_dir/compatible-name" >"$tmp_dir/probe.out" 2>&1 || probe_status=$?
fi
if [[ "$probe_status" == 0 ]]; then
    echo "zlib-compatible name unexpectedly exists" >&2
    exit 1
fi
if ! rg -q "deflateInit" "$tmp_dir/probe.out"; then
    echo "compatible-name rejection did not identify the missing name" >&2
    cat "$tmp_dir/probe.out" >&2
    exit 1
fi

echo "optional builds: no-malloc no-stdio no-time reader-only no-deflate no-inflate no-archive no-writing no-zlib header-only export compatible-names windows/linux/macos/ios/wasm/android surfaces ok"
