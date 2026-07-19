#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"

struct source { const unsigned char *data; size_t size, calls, short_at; };

static size_t source_read(void *opaque, mz_uint64 offset, void *buf, size_t size) {
    struct source *s = (struct source *)opaque;
    size_t take;
    s->calls++;
    take = offset >= s->size ? 0 : (size_t)MZ_MIN((mz_uint64)size, s->size - offset);
    if (s->calls == s->short_at && take) take--;
    if (take) memcpy(buf, s->data + offset, take);
    return take;
}

static size_t reject_write(void *opaque, mz_uint64 offset, const void *buf, size_t size) {
    (void)opaque; (void)offset; (void)buf; (void)size; return 0;
}

static void put32(FILE *f, uint32_t value) {
    unsigned char b[4] = {(unsigned char)value, (unsigned char)(value >> 8),
                          (unsigned char)(value >> 16), (unsigned char)(value >> 24)};
    fwrite(b, 1, sizeof(b), f);
}

static void record(FILE *f, int ok, mz_zip_archive *zip, struct source *s) {
    put32(f, (uint32_t)ok); put32(f, (uint32_t)mz_zip_get_last_error(zip)); put32(f, (uint32_t)s->calls);
}

static int build(void **data, size_t *size) {
    static const char payload[] = "source failure payload source failure payload source failure payload";
    mz_zip_archive zip;
    FILE *f;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0) ||
        !mz_zip_writer_add_mem_ex_v2(&zip, "x", payload, sizeof(payload)-1, NULL, 0,
            MZ_BEST_COMPRESSION, 0, 0, NULL, NULL, 0, NULL, 0) ||
        !mz_zip_writer_finalize_heap_archive(&zip, data, size) || !mz_zip_writer_end(&zip)) return 0;
    f = fopen("/tmp/miniz-zip-source-failures.zip", "wb");
    if (!f) return 0;
    return fwrite(*data, 1, *size, f) == *size && fclose(f) == 0;
}

int main(void) {
    void *archive = NULL;
    size_t archive_size = 0, made;
    unsigned char output[256];
    mz_zip_archive zip;
    mz_zip_reader_extract_iter_state *iter;
    struct source source;
    FILE *out;
    if (!build(&archive, &archive_size)) return 1;
    out = fopen("/tmp/miniz-zip-source-failures.bin", "wb");
    if (!out) return 1;

    memset(&source, 0, sizeof(source)); source.data = archive; source.size = archive_size; source.short_at = 1;
    memset(&zip, 0, sizeof(zip)); zip.m_pRead = source_read; zip.m_pIO_opaque = &source;
    record(out, mz_zip_reader_init(&zip, archive_size, 0), &zip, &source);

    source.calls = 0; source.short_at = 0;
    memset(&zip, 0, sizeof(zip)); zip.m_pRead = source_read; zip.m_pIO_opaque = &source;
    record(out, mz_zip_reader_init(&zip, archive_size, 0), &zip, &source);
    source.calls = 0; source.short_at = 1;
    record(out, mz_zip_reader_extract_to_mem(&zip, 0, output, sizeof(output), 0), &zip, &source);
    source.calls = 0; source.short_at = 0;
    record(out, mz_zip_reader_extract_to_callback(&zip, 0, NULL, NULL, 0), &zip, &source);
    source.calls = 0; source.short_at = 0;
    record(out, mz_zip_reader_extract_to_callback(&zip, 0, reject_write, NULL, 0), &zip, &source);

    source.calls = 0; source.short_at = 1;
    iter = mz_zip_reader_extract_iter_new(&zip, 0, 0);
    record(out, iter != NULL, &zip, &source);
    if (iter) mz_zip_reader_extract_iter_free(iter);

    source.calls = 0; source.short_at = 0;
    iter = mz_zip_reader_extract_iter_new(&zip, 0, 0);
    record(out, iter != NULL, &zip, &source);
    if (!iter) return 1;
    source.calls = 0; source.short_at = 1;
    made = mz_zip_reader_extract_iter_read(iter, output, sizeof(output));
    record(out, made != 0, &zip, &source);
    put32(out, (uint32_t)mz_zip_reader_extract_iter_free(iter));
    record(out, mz_zip_reader_end(&zip), &zip, &source);

    mz_free(archive);
    return fclose(out) != 0;
}
