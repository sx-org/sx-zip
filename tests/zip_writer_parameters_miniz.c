#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"

static FILE *out;
static void u32(uint32_t v) {
    unsigned char b[4] = {(unsigned char)v, (unsigned char)(v >> 8),
                          (unsigned char)(v >> 16), (unsigned char)(v >> 24)};
    fwrite(b, 1, 4, out);
}
static void record(int ok, mz_zip_archive *zip) {
    u32((uint32_t)ok); u32((uint32_t)mz_zip_get_last_error(zip));
}
static int start(mz_zip_archive *zip) {
    memset(zip, 0, sizeof(*zip));
    return mz_zip_writer_init_heap(zip, 0, 0);
}
static void stop(mz_zip_archive *zip) { if (zip->m_pState) mz_zip_writer_end(zip); }
static size_t empty_read(void *opaque, mz_uint64 offset, void *buffer, size_t size) {
    (void)opaque; (void)offset; (void)buffer; (void)size;
    return 0;
}
static void add_case(const char *name, const void *data, size_t size, unsigned flags) {
    mz_zip_archive zip;
    if (!start(&zip)) exit(2);
    record(mz_zip_writer_add_mem(&zip, name, data, size, flags), &zip);
    stop(&zip);
}
static void extra_case(const void *local, mz_uint local_size, const void *central, mz_uint central_size) {
    mz_zip_archive zip;
    void *archive = NULL;
    size_t size = 0;
    int ok;
    if (!start(&zip)) exit(3);
    ok = mz_zip_writer_add_mem_ex_v2(&zip, "x", NULL, 0, NULL, 0,
        MZ_NO_COMPRESSION | MZ_ZIP_FLAG_ASCII_FILENAME, 0, 0, NULL,
        (const char *)local, local_size, (const char *)central, central_size);
    record(ok, &zip);
    if (ok) {
        ok = mz_zip_writer_finalize_heap_archive(&zip, &archive, &size);
        record(ok, &zip); u32((uint32_t)size); mz_free(archive);
    }
    stop(&zip);
}

int main(void) {
    mz_zip_archive zip;
    unsigned char *large = (unsigned char *)calloc(1, 65536);
    char *long_name = (char *)malloc(65537);
    void *archive = NULL;
    size_t archive_size = 0;
    int ok;
    if (!large || !long_name) return 1;
    memset(long_name, 'n', 65536); long_name[65536] = 0;
    out = fopen("/tmp/miniz-zip-writer-parameters.bin", "wb"); if (!out) return 1;

    memset(&zip, 0, sizeof(zip));
    record(mz_zip_writer_init(&zip, 0), &zip);

    memset(&zip, 0, sizeof(zip));
    record(mz_zip_writer_add_mem(&zip, "x", NULL, 0, 0), &zip);

    memset(&zip, 0, sizeof(zip)); zip.m_file_offset_alignment = 3;
    record(mz_zip_writer_init_heap(&zip, 0, 0), &zip); stop(&zip);

    add_case("/absolute", NULL, 0, MZ_NO_COMPRESSION);
    add_case("dir/", "x", 1, MZ_NO_COMPRESSION);
    add_case("level", "payload", 7, 11);
    add_case(long_name, NULL, 0, MZ_NO_COMPRESSION);
    extra_case(large, 65535, NULL, 0);
    extra_case(large, 65536, NULL, 0);
    extra_case(NULL, 0, large, 65536);

    if (!start(&zip)) return 4;
    record(mz_zip_writer_add_mem_ex(&zip, "not-raw", "x", 1, NULL, 0,
                                    MZ_NO_COMPRESSION, 1, 0), &zip);
    stop(&zip);

    if (!start(&zip) || !mz_zip_writer_add_mem(&zip, "x", NULL, 0, MZ_NO_COMPRESSION)) return 5;
    ok = mz_zip_writer_finalize_heap_archive(&zip, &archive, &archive_size); record(ok, &zip); mz_free(archive);
    record(mz_zip_writer_add_mem(&zip, "late", NULL, 0, MZ_NO_COMPRESSION), &zip);
    record(mz_zip_writer_finalize_archive(&zip), &zip);
    record(mz_zip_writer_end(&zip), &zip);
    record(mz_zip_writer_end(&zip), &zip);

    if (!start(&zip)) return 6;
    record(mz_zip_writer_add_read_buf_callback(&zip, "empty", NULL, NULL, 0,
        NULL, NULL, 0, MZ_NO_COMPRESSION | MZ_ZIP_FLAG_ASCII_FILENAME,
        NULL, 0, NULL, 0), &zip);
    stop(&zip);

    if (!start(&zip)) return 7;
    record(mz_zip_writer_add_read_buf_callback(&zip, "x", empty_read, NULL, 0,
        NULL, NULL, 0, MZ_NO_COMPRESSION | MZ_ZIP_FLAG_ASCII_FILENAME,
        (const char *)large, 65536, NULL, 0), &zip);
    ok = mz_zip_writer_finalize_heap_archive(&zip, &archive, &archive_size);
    record(ok, &zip); u32((uint32_t)archive_size); mz_free(archive);
    stop(&zip);

    free(long_name); free(large);
    return fclose(out) != 0;
}
