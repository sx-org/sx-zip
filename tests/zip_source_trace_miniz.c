#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"

struct trace {
    const unsigned char *data;
    size_t size;
    uint64_t calls, total, max_request;
    uint32_t hash;
};

struct sink {
    uint64_t calls, total, max_chunk;
    uint32_t hash, checksum;
    int phase;
};

static void put64(unsigned char *p, uint64_t value) {
    int i;
    for (i = 0; i < 8; ++i) p[i] = (unsigned char)(value >> (i * 8));
}

static void mix(uint32_t *hash, uint64_t offset, uint64_t size) {
    unsigned char record[16];
    put64(record, offset); put64(record + 8, size);
    *hash = (uint32_t)mz_crc32(*hash, record, sizeof(record));
}

static void reset_trace(struct trace *trace) {
    trace->calls = trace->total = trace->max_request = 0;
    trace->hash = 0;
}

static size_t trace_read(void *opaque, mz_uint64 offset, void *output, size_t size) {
    struct trace *trace = (struct trace *)opaque;
    size_t take = offset >= trace->size ? 0 : (size_t)MZ_MIN((mz_uint64)size, trace->size - offset);
    trace->calls++; trace->total += size;
    if (size > trace->max_request) trace->max_request = size;
    mix(&trace->hash, offset, size);
    memcpy(output, trace->data + offset, take);
    return take;
}

static size_t sink_write(void *opaque, mz_uint64 offset, const void *data, size_t size) {
    struct sink *sink = (struct sink *)opaque;
    sink->calls++; sink->total += size;
    if (size > sink->max_chunk) sink->max_chunk = size;
    mix(&sink->hash, offset, size);
    sink->checksum = (uint32_t)mz_crc32(sink->checksum, data, size);
    printf("chunk %d %llu %zu\n", sink->phase, (unsigned long long)offset, size);
    return size;
}

static int make_fixture(void) {
    unsigned char *payload = (unsigned char *)malloc(1024 * 1024);
    mz_zip_archive zip;
    uint32_t state = 0x31415926U;
    size_t i;
    int ok;
    if (!payload) return 0;
    for (i = 0; i < 1024 * 1024; ++i) {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        payload[i] = (i % 97 < 80) ? (unsigned char)('A' + i % 7) : (unsigned char)state;
    }
    memset(&zip, 0, sizeof(zip));
    ok = mz_zip_writer_init_file(&zip, "/tmp/miniz-zip-source-trace.zip", 0) &&
         mz_zip_writer_add_mem(&zip, "large-stored.bin", payload, 1024 * 1024, MZ_NO_COMPRESSION) &&
         mz_zip_writer_add_mem(&zip, "large-deflated.bin", payload, 1024 * 1024, MZ_BEST_COMPRESSION) &&
         mz_zip_writer_finalize_archive(&zip);
    ok = mz_zip_writer_end(&zip) && ok;
    free(payload);
    return ok;
}

static unsigned char *read_all(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    unsigned char *data;
    long end;
    if (!file || fseek(file, 0, SEEK_END) || (end = ftell(file)) < 0 || fseek(file, 0, SEEK_SET)) return NULL;
    data = (unsigned char *)malloc((size_t)end);
    if (!data || fread(data, 1, (size_t)end, file) != (size_t)end || fclose(file)) { free(data); return NULL; }
    *size = (size_t)end;
    return data;
}

static void print_trace(const char *tag, const struct trace *trace) {
    printf("%s-read %llu %llu %llu %u\n", tag,
           (unsigned long long)trace->calls, (unsigned long long)trace->total,
           (unsigned long long)trace->max_request, trace->hash);
}

static void print_sink(const char *tag, const struct sink *sink) {
    printf("%s-out %llu %llu %llu %u %u\n", tag,
           (unsigned long long)sink->calls, (unsigned long long)sink->total,
           (unsigned long long)sink->max_chunk, sink->hash, sink->checksum);
}

int main(void) {
    mz_zip_archive zip;
    struct trace trace;
    struct sink sink;
    unsigned char *archive, *fixed, buffer[997], read_buffer[4096];
    size_t archive_size, made;
    mz_zip_reader_extract_iter_state *iter;
    uint64_t iter_total;
    uint32_t iter_crc;
    if (!make_fixture()) return 1;
    archive = read_all("/tmp/miniz-zip-source-trace.zip", &archive_size);
    if (!archive) return 1;
    memset(&trace, 0, sizeof(trace)); trace.data = archive; trace.size = archive_size;
    memset(&zip, 0, sizeof(zip)); zip.m_pRead = trace_read; zip.m_pIO_opaque = &trace;
    if (!mz_zip_reader_init(&zip, archive_size, 0)) return 1;
    print_trace("init", &trace);

    reset_trace(&trace); memset(&sink, 0, sizeof(sink)); sink.phase = 1;
    if (!mz_zip_reader_extract_file_to_callback(&zip, "large-stored.bin", sink_write, &sink, 0)) return 1;
    print_trace("stored", &trace); print_sink("stored", &sink);

    reset_trace(&trace); memset(&sink, 0, sizeof(sink)); sink.phase = 2;
    if (!mz_zip_reader_extract_file_to_callback(&zip, "large-deflated.bin", sink_write, &sink, 0)) return 1;
    print_trace("deflate", &trace); print_sink("deflate", &sink);

    reset_trace(&trace);
    iter = mz_zip_reader_extract_file_iter_new(&zip, "large-deflated.bin", 0);
    if (!iter) return 1;
    iter_total = 0; iter_crc = 0;
    while ((made = mz_zip_reader_extract_iter_read(iter, buffer, sizeof(buffer))) != 0) {
        iter_total += made;
        iter_crc = (uint32_t)mz_crc32(iter_crc, buffer, made);
    }
    if (!mz_zip_reader_extract_iter_free(iter)) return 1;
    print_trace("iter", &trace);
    printf("iter-out %llu %u\n", (unsigned long long)iter_total, iter_crc);

    reset_trace(&trace); memset(&sink, 0, sizeof(sink)); sink.phase = 3;
    if (!mz_zip_reader_extract_file_to_callback(&zip, "large-deflated.bin", sink_write, &sink,
                                                MZ_ZIP_FLAG_COMPRESSED_DATA)) return 1;
    print_trace("raw-deflate", &trace); print_sink("raw-deflate", &sink);

    reset_trace(&trace);
    iter = mz_zip_reader_extract_file_iter_new(&zip, "large-deflated.bin", MZ_ZIP_FLAG_COMPRESSED_DATA);
    if (!iter) return 1;
    iter_total = 0; iter_crc = 0;
    while ((made = mz_zip_reader_extract_iter_read(iter, buffer, sizeof(buffer))) != 0) {
        iter_total += made;
        iter_crc = (uint32_t)mz_crc32(iter_crc, buffer, made);
    }
    if (!mz_zip_reader_extract_iter_free(iter)) return 1;
    print_trace("raw-iter", &trace);
    printf("raw-iter-out %llu %u\n", (unsigned long long)iter_total, iter_crc);

    fixed = (unsigned char *)malloc(1024 * 1024);
    if (!fixed) return 1;
    reset_trace(&trace);
    if (!mz_zip_reader_extract_file_to_mem_no_alloc(&zip, "large-deflated.bin", fixed,
            1024 * 1024, 0, read_buffer, sizeof(read_buffer))) return 1;
    print_trace("noalloc", &trace);
    printf("noalloc-out %u\n", (unsigned)mz_crc32(MZ_CRC32_INIT, fixed, 1024 * 1024));
    free(fixed);

    if (!mz_zip_reader_end(&zip)) return 1;
    free(archive);
    return 0;
}
