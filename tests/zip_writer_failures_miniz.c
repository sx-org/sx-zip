#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "miniz.h"

typedef struct {
    unsigned fail_kind;
    unsigned calls;
    size_t accepted;
    mz_uint32 crc;
    int saw_local;
    int saw_descriptor;
    unsigned local_count;
} sink_state;

typedef struct {
    unsigned mode;
    unsigned calls;
    mz_uint32 trace;
} source_state;

static uint32_t rd32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static unsigned kind_of(sink_state *s, const void *buf, size_t size) {
    uint32_t sig = size >= 4 ? rd32((const unsigned char *)buf) : 0;
    if (sig == 0x04034b50U) {
        s->saw_local = 1;
        s->local_count++;
        return s->local_count > 1 ? 6 : 1;
    }
    if (sig == 0x08074b50U) { s->saw_descriptor = 1; return 3; }
    if (sig == 0x02014b50U) return 4;
    if (sig == 0x06054b50U) return 5;
    if (s->saw_local && !s->saw_descriptor) return 2;
    return 0;
}

static size_t sink_write(void *opaque, mz_uint64 ofs, const void *buf, size_t size) {
    sink_state *s = (sink_state *)opaque;
    unsigned kind;
    (void)ofs;
    s->calls++;
    kind = kind_of(s, buf, size);
    if (kind == s->fail_kind) return 0;
    s->accepted += size;
    s->crc = (mz_uint32)mz_crc32(s->crc, (const unsigned char *)buf, size);
    return size;
}

static size_t source_read(void *opaque, mz_uint64 ofs, void *buf, size_t size) {
    source_state *s = (source_state *)opaque;
    unsigned char *p = (unsigned char *)buf;
    size_t take = size, i;
    s->calls++;
    s->trace = (mz_uint32)mz_crc32(s->trace, (const unsigned char *)&ofs, sizeof(ofs));
    s->trace = (mz_uint32)mz_crc32(s->trace, (const unsigned char *)&size, sizeof(size));
    if (s->mode == 1 || (s->mode == 3 && s->calls == 2)) return size + 1;
    if (s->mode == 2) return 0;
    if (take > 7) take = 7;
    if (ofs >= 123) take = 0;
    else if (take > 123 - (size_t)ofs) take = 123 - (size_t)ofs;
    for (i = 0; i < take; ++i) p[i] = (unsigned char)(((size_t)ofs + i) * 29U + 7U);
    return take;
}

static void put32(FILE *f, uint32_t v) {
    unsigned char b[4] = {(unsigned char)v, (unsigned char)(v >> 8),
                          (unsigned char)(v >> 16), (unsigned char)(v >> 24)};
    fwrite(b, 1, 4, f);
}

static void put64(FILE *f, mz_uint64 v) {
    put32(f, (uint32_t)v); put32(f, (uint32_t)(v >> 32));
}

static void result(FILE *f, int ok, mz_zip_archive *zip) {
    put32(f, (uint32_t)ok); put32(f, (uint32_t)mz_zip_get_last_error(zip));
}

static void snapshot(FILE *f, mz_zip_archive *zip, sink_state *sink) {
    put32(f, (uint32_t)mz_zip_get_mode(zip));
    put32(f, mz_zip_reader_get_num_files(zip));
    put64(f, mz_zip_get_archive_size(zip));
    put64(f, mz_zip_get_central_dir_size(zip));
    put32(f, (uint32_t)mz_zip_is_zip64(zip));
    put32(f, sink->calls); put64(f, sink->accepted); put32(f, sink->crc);
    put32(f, (uint32_t)mz_zip_peek_last_error(zip));
}

static void run_case(FILE *out, unsigned fail_kind) {
    static const char payload[] =
        "writer callback failure payload writer callback failure payload "
        "writer callback failure payload writer callback failure payload";
    sink_state sink;
    mz_zip_archive zip;
    int ok;
    memset(&sink, 0, sizeof(sink)); sink.fail_kind = fail_kind;
    memset(&zip, 0, sizeof(zip)); zip.m_pWrite = sink_write; zip.m_pIO_opaque = &sink;
    result(out, mz_zip_writer_init(&zip, 0), &zip);
    ok = mz_zip_writer_add_mem_ex_v2(&zip, "entry", payload, sizeof(payload) - 1,
        NULL, 0, MZ_BEST_COMPRESSION, 0, 0, NULL, NULL, 0, NULL, 0);
    result(out, ok, &zip); snapshot(out, &zip, &sink);
    ok = mz_zip_writer_finalize_archive(&zip);
    result(out, ok, &zip); snapshot(out, &zip, &sink);
    if (fail_kind == 4 || fail_kind == 5) {
        sink.fail_kind = 0;
        ok = mz_zip_writer_finalize_archive(&zip);
        result(out, ok, &zip); snapshot(out, &zip, &sink);
    }
    result(out, mz_zip_writer_end(&zip), &zip); snapshot(out, &zip, &sink);
}

static void run_source_case(FILE *out, unsigned mode, unsigned level) {
    sink_state sink;
    source_state source;
    mz_zip_archive zip;
    int ok;
    memset(&sink, 0, sizeof(sink));
    memset(&source, 0, sizeof(source)); source.mode = mode;
    memset(&zip, 0, sizeof(zip)); zip.m_pWrite = sink_write; zip.m_pIO_opaque = &sink;
    result(out, mz_zip_writer_init(&zip, 0), &zip);
    ok = mz_zip_writer_add_read_buf_callback(&zip, "source", source_read, &source,
        123, NULL, NULL, 0, level, NULL, 0, NULL, 0);
    result(out, ok, &zip); snapshot(out, &zip, &sink);
    put32(out, source.calls); put32(out, source.trace);
    ok = mz_zip_writer_finalize_archive(&zip);
    result(out, ok, &zip); snapshot(out, &zip, &sink);
    result(out, mz_zip_writer_end(&zip), &zip); snapshot(out, &zip, &sink);
}

static void run_patch_case(FILE *out, unsigned fail_kind) {
    sink_state sink;
    source_state source;
    mz_zip_archive zip;
    int ok;
    memset(&sink, 0, sizeof(sink)); sink.fail_kind = fail_kind;
    memset(&source, 0, sizeof(source));
    memset(&zip, 0, sizeof(zip)); zip.m_pWrite = sink_write; zip.m_pIO_opaque = &sink;
    result(out, mz_zip_writer_init(&zip, 0), &zip);
    ok = mz_zip_writer_add_read_buf_callback(&zip, "patched", source_read, &source,
        123, NULL, NULL, 0, 9 | MZ_ZIP_FLAG_WRITE_HEADER_SET_SIZE,
        NULL, 0, NULL, 0);
    result(out, ok, &zip); snapshot(out, &zip, &sink);
    put32(out, source.calls); put32(out, source.trace);
    ok = mz_zip_writer_finalize_archive(&zip);
    result(out, ok, &zip); snapshot(out, &zip, &sink);
    result(out, mz_zip_writer_end(&zip), &zip); snapshot(out, &zip, &sink);
}

int main(void) {
    FILE *out = fopen("/tmp/miniz-zip-writer-failures.bin", "wb");
    unsigned kind;
    if (!out) return 1;
    for (kind = 0; kind <= 5; ++kind) run_case(out, kind);
    run_source_case(out, 0, 9);
    run_source_case(out, 1, 0);
    run_source_case(out, 1, 9);
    run_source_case(out, 2, 0);
    run_source_case(out, 2, 9);
    run_source_case(out, 3, 9);
    run_patch_case(out, 0);
    run_patch_case(out, 6);
    return fclose(out) != 0;
}
