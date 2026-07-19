#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"

#define CONTAINER "/tmp/miniz-zip-file-init-container.bin"
#define TINY "/tmp/miniz-zip-file-init-tiny.bin"

static FILE *out;
static void u32(uint32_t v) {
    unsigned char b[4] = {(unsigned char)v, (unsigned char)(v >> 8),
                          (unsigned char)(v >> 16), (unsigned char)(v >> 24)};
    fwrite(b, 1, 4, out);
}
static void u64(uint64_t v) { u32((uint32_t)v); u32((uint32_t)(v >> 32)); }
static void result(int ok, mz_zip_archive *zip) {
    u32((uint32_t)ok); u32((uint32_t)mz_zip_get_last_error(zip));
}
static int fixtures(size_t *archive_size) {
    mz_zip_archive zip;
    void *archive = NULL;
    FILE *file;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0) ||
        !mz_zip_writer_add_mem(&zip, "entry", "payload", 7, MZ_NO_COMPRESSION) ||
        !mz_zip_writer_finalize_heap_archive(&zip, &archive, archive_size) ||
        !mz_zip_writer_end(&zip)) return 0;
    file = fopen(CONTAINER, "wb");
    if (!file || fwrite("PFX!!", 1, 5, file) != 5 ||
        fwrite(archive, 1, *archive_size, file) != *archive_size ||
        fwrite("END", 1, 3, file) != 3 || fclose(file) != 0) return 0;
    mz_free(archive);
    file = fopen(TINY, "wb");
    if (!file || fwrite("tiny", 1, 4, file) != 4 || fclose(file) != 0) return 0;
    return 1;
}
static void run(const char *path, mz_uint flags, mz_uint64 start, mz_uint64 size) {
    mz_zip_archive zip;
    int ok;
    memset(&zip, 0, sizeof(zip));
    ok = mz_zip_reader_init_file_v2(&zip, path, flags, start, size);
    result(ok, &zip);
    if (!ok) {
        u32(0); u32(0); u64(0); u64(0); u32(0); result(0, &zip);
        return;
    }
    u32((uint32_t)mz_zip_get_mode(&zip));
    u32((uint32_t)mz_zip_get_type(&zip));
    u64(mz_zip_get_archive_size(&zip));
    u64(mz_zip_get_archive_file_start_offset(&zip));
    u32(mz_zip_reader_get_num_files(&zip));
    result(mz_zip_reader_end(&zip), &zip);
}
static void run_cfile(mz_uint64 start, mz_uint64 size) {
    mz_zip_archive zip;
    FILE *file = fopen(CONTAINER, "rb");
    int ok;
    if (!file || fseek(file, (long)start, SEEK_SET) != 0) exit(2);
    memset(&zip, 0, sizeof(zip));
    ok = mz_zip_reader_init_cfile(&zip, file, size, 0);
    result(ok, &zip);
    if (!ok) {
        u32(0); u32(0); u64(0); u64(0); u32(0); result(0, &zip);
        fclose(file); return;
    }
    u32((uint32_t)mz_zip_get_mode(&zip));
    u32((uint32_t)mz_zip_get_type(&zip));
    u64(mz_zip_get_archive_size(&zip));
    u64(mz_zip_get_archive_file_start_offset(&zip));
    u32(mz_zip_reader_get_num_files(&zip));
    result(mz_zip_reader_end(&zip), &zip);
    fclose(file);
}
int main(void) {
    size_t archive_size = 0;
    if (!fixtures(&archive_size)) return 1;
    out = fopen("/tmp/miniz-zip-file-init-matrix.bin", "wb");
    if (!out) return 1;
    run(CONTAINER, 0, 5, archive_size);
    run(CONTAINER, MZ_ZIP_FLAG_READ_ALLOW_WRITING, 5, archive_size);
    run(CONTAINER, 0, 5, 0);
    run(CONTAINER, 0, 5, 1);
    run(CONTAINER, 0, 5, archive_size + 100);
    run(CONTAINER, 0, archive_size + 100, archive_size);
    run(TINY, 0, 0, 0);
    run("/tmp/miniz-zip-file-init-missing.bin", 0, 0, 0);
    run_cfile(5, archive_size);
    run_cfile(5, 0);
    run_cfile(5, 1);
    run_cfile(archive_size + 100, archive_size);
    return fclose(out) != 0;
}
