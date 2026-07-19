#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"

#define CASE_COUNT 14
struct sink { unsigned char bytes[4096]; size_t size; };

static FILE *out;
static void wr32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}
static uint16_t rd16(const unsigned char *p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }
static uint32_t rd32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void u32(uint32_t v) { unsigned char b[4]; wr32(b, v); fwrite(b, 1, 4, out); }
static void result(int ok, mz_zip_archive *zip) { u32((uint32_t)ok); u32((uint32_t)mz_zip_get_last_error(zip)); }
static size_t sink_write(void *opaque, mz_uint64 offset, const void *data, size_t size) {
    struct sink *sink = (struct sink *)opaque;
    if (offset > sizeof(sink->bytes) || size > sizeof(sink->bytes) - (size_t)offset) return 0;
    memcpy(sink->bytes + (size_t)offset, data, size);
    if ((size_t)offset + size > sink->size) sink->size = (size_t)offset + size;
    return size;
}
static size_t memory_read(void *opaque, mz_uint64 offset, void *buffer, size_t size) {
    const char *data = (const char *)opaque;
    size_t total = strlen(data);
    if (offset >= total) return 0;
    if (size > total - (size_t)offset) size = total - (size_t)offset;
    memcpy(buffer, data + (size_t)offset, size); return size;
}
static int build(void **archive, size_t *size, int zip64, int callback) {
    static const char payload[] = "raw-copy-payload-raw-copy-payload";
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap_v2(&zip, 0, 0, zip64 ? MZ_ZIP_FLAG_WRITE_ZIP64 : 0)) return 0;
    if (callback) {
        if (!mz_zip_writer_add_read_buf_callback(&zip, "local-name", memory_read, (void *)payload,
                sizeof(payload) - 1, NULL, NULL, 0,
                MZ_NO_COMPRESSION | MZ_ZIP_FLAG_ASCII_FILENAME,
                NULL, 0, NULL, 0)) return 0;
    } else if (!mz_zip_writer_add_mem(&zip, "local-name", payload, sizeof(payload) - 1,
                                      MZ_NO_COMPRESSION | MZ_ZIP_FLAG_ASCII_FILENAME)) return 0;
    return mz_zip_writer_finalize_heap_archive(&zip, archive, size) && mz_zip_writer_end(&zip);
}
static void mutate(unsigned char *data, size_t size, unsigned which) {
    size_t eocd = size - 22;
    uint32_t central = rd32(data + eocd + 16);
    if (which == 1) data[0] ^= 1;
    else if (which == 2) { data[8] = 99; data[9] = 0; }
    else if (which == 3) data[30] ^= 0x20;
    else if (which == 4) { data[6] |= 8; }
    else if (which == 5) { uint16_t n = rd16(data + 26); data[26] = (unsigned char)(n + 1); data[27] = (unsigned char)((n + 1) >> 8); }
    else if (which == 6) { uint16_t n = rd16(data + 28); data[28] = (unsigned char)(n + 1); data[29] = (unsigned char)((n + 1) >> 8); }
    else if (which >= 10 && which <= 12) {
        uint32_t compressed = rd32(data + central + 20);
        size_t descriptor = 30 + rd16(data + 26) + rd16(data + 28) + compressed;
        if (which == 10) data[descriptor + 4] ^= 0x5a;
        else if (which == 11) data[descriptor + 8] ^= 0x33;
        else data[descriptor] ^= 1;
    }
}
static void run(const unsigned char *fixture, size_t size, unsigned which, int destination_zip64) {
    unsigned char *copy = (unsigned char *)malloc(size);
    mz_zip_archive source, writer;
    void *archive = NULL;
    size_t archive_size = 0;
    int ok, final_ok = 0;
    struct sink sink;
    mz_zip_archive stream;
    if (!copy) exit(2);
    memcpy(copy, fixture, size); mutate(copy, size, which);
    memset(&source, 0, sizeof(source)); memset(&writer, 0, sizeof(writer));
    if (!mz_zip_reader_init_mem(&source, copy, size, 0) ||
        !mz_zip_writer_init_heap_v2(&writer, 0, 0, destination_zip64 ? MZ_ZIP_FLAG_WRITE_ZIP64 : 0)) exit(3);
    ok = mz_zip_writer_add_from_zip_reader(&writer, &source, which == 7 ? 99 : 0);
    result(ok, &writer);
    if (ok) final_ok = mz_zip_writer_finalize_heap_archive(&writer, &archive, &archive_size);
    result(final_ok, &writer); u32((uint32_t)archive_size);
    if (archive_size) fwrite(archive, 1, archive_size, out);
    mz_free(archive); mz_zip_writer_end(&writer);

    memset(&sink, 0, sizeof(sink)); memset(&stream, 0, sizeof(stream));
    stream.m_pWrite = sink_write; stream.m_pIO_opaque = &sink;
    if (!mz_zip_writer_init_v2(&stream, 0, destination_zip64 ? MZ_ZIP_FLAG_WRITE_ZIP64 : 0)) exit(4);
    ok = mz_zip_writer_add_from_zip_reader(&stream, &source, which == 7 ? 99 : 0);
    result(ok, &stream); final_ok = ok ? mz_zip_writer_finalize_archive(&stream) : 0;
    result(final_ok, &stream); u32((uint32_t)sink.size);
    if (sink.size) fwrite(sink.bytes, 1, sink.size, out);
    mz_zip_writer_end(&stream); mz_zip_reader_end(&source); free(copy);
}
int main(void) {
    void *base = NULL, *zip64 = NULL, *descriptor = NULL;
    size_t base_size = 0, zip64_size = 0, descriptor_size = 0;
    FILE *fixtures;
    unsigned i;
    if (!build(&base, &base_size, 0, 0) || !build(&zip64, &zip64_size, 1, 0) ||
        !build(&descriptor, &descriptor_size, 0, 1)) return 1;
    fixtures = fopen("/tmp/miniz-zip-add-from-reader-fixtures.bin", "wb");
    out = fopen("/tmp/miniz-zip-add-from-reader-matrix.bin", "wb");
    if (!fixtures || !out) return 1;
    { unsigned char b[4]; wr32(b, (uint32_t)base_size); fwrite(b, 1, 4, fixtures); fwrite(base, 1, base_size, fixtures);
      wr32(b, (uint32_t)zip64_size); fwrite(b, 1, 4, fixtures); fwrite(zip64, 1, zip64_size, fixtures);
      wr32(b, (uint32_t)descriptor_size); fwrite(b, 1, 4, fixtures); fwrite(descriptor, 1, descriptor_size, fixtures); }
    for (i = 0; i < CASE_COUNT; ++i) {
        if (i == 8) run((const unsigned char *)zip64, zip64_size, i, 0);
        else if (i >= 9) run((const unsigned char *)descriptor, descriptor_size, i, i == 13);
        else run((const unsigned char *)base, base_size, i, 0);
    }
    mz_free(base); mz_free(zip64); mz_free(descriptor);
    return fclose(fixtures) != 0 || fclose(out) != 0;
}
