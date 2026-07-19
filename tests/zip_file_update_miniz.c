#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "miniz.h"

static FILE *trace;

static void put32(uint32_t value) {
    unsigned char b[4] = {(unsigned char)value, (unsigned char)(value >> 8),
                          (unsigned char)(value >> 16), (unsigned char)(value >> 24)};
    fwrite(b, 1, sizeof(b), trace);
}

static void record(int ok, mz_zip_archive *zip) {
    put32((uint32_t)ok);
    put32((uint32_t)mz_zip_get_last_error(zip));
    put32((uint32_t)mz_zip_get_mode(zip));
    put32((uint32_t)mz_zip_get_type(zip));
    put32((uint32_t)mz_zip_reader_get_num_files(zip));
}

static int create_base(const char *path) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_file(&zip, path, 0) ||
        !mz_zip_writer_add_mem_ex_v2(&zip, "base.txt", "base payload", 12,
            "old", 3, MZ_NO_COMPRESSION, 0, 0, NULL, NULL, 0, NULL, 0) ||
        !mz_zip_writer_finalize_archive(&zip)) return 0;
    return mz_zip_writer_end(&zip);
}

static int create_reserved(const char *path) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_file(&zip, path, 4097) ||
        !mz_zip_writer_add_mem_ex_v2(&zip, "x", "reserved", 8, NULL, 0,
            MZ_NO_COMPRESSION, 0, 0, NULL, NULL, 0, NULL, 0) ||
        !mz_zip_writer_finalize_archive(&zip)) return 0;
    return mz_zip_writer_end(&zip);
}

int main(void) {
    const char *base = "/tmp/miniz-zip-file-update-base.zip";
    const char *updated = "/tmp/miniz-zip-file-update.zip";
    mz_zip_archive zip;
    unsigned char output[32];
    if (!create_base(base) || !create_base(updated) ||
        !create_reserved("/tmp/miniz-zip-reserved.zip")) return 1;
    trace = fopen("/tmp/miniz-zip-file-update.bin", "wb");
    if (!trace) return 1;
    memset(&zip, 0, sizeof(zip));
    record(mz_zip_reader_init_file_v2(&zip, updated,
           MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY | MZ_ZIP_FLAG_READ_ALLOW_WRITING,
           0, 0), &zip);
    record(mz_zip_writer_init_from_reader_v2(&zip, updated, MZ_ZIP_FLAG_READ_ALLOW_WRITING), &zip);
    record(mz_zip_reader_extract_to_mem(&zip, 0, output, 12, 0) &&
           memcmp(output, "base payload", 12) == 0, &zip);
    record(mz_zip_writer_add_mem_ex_v2(&zip, "added.txt", "new payload", 11,
           "new", 3, MZ_NO_COMPRESSION, 0, 0, NULL, NULL, 0, NULL, 0), &zip);
    record(mz_zip_writer_finalize_archive(&zip), &zip);
    record(mz_zip_writer_end(&zip), &zip);
    return fclose(trace) != 0;
}
