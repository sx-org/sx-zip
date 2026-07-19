#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include "miniz.h"

#define PATH "/tmp/miniz-zip-inplace-partial.zip"

static void u32(uint32_t v) {
    unsigned char b[4] = {(unsigned char)v, (unsigned char)(v >> 8),
                          (unsigned char)(v >> 16), (unsigned char)(v >> 24)};
    fwrite(b, 1, 4, stdout);
}

int main(void) {
    static const char payload[] = "partial-write-payload-partial-write-payload-partial-write-payload";
    mz_zip_archive zip;
    void *archive = NULL;
    size_t archive_size = 0;
    mz_zip_error err = MZ_ZIP_UNDEFINED_ERROR;
    struct rlimit limit;
    FILE *file;
    unsigned char *bytes;
    long size;
    int ok;

    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0) ||
        !mz_zip_writer_add_mem(&zip, "base", "base-data", 9,
                               MZ_NO_COMPRESSION | MZ_ZIP_FLAG_ASCII_FILENAME) ||
        !mz_zip_writer_finalize_heap_archive(&zip, &archive, &archive_size) ||
        !mz_zip_writer_end(&zip)) return 1;
    file = fopen(PATH, "wb");
    if (!file || fwrite(archive, 1, archive_size, file) != archive_size || fclose(file) != 0) return 1;
    mz_free(archive);

    if (getrlimit(RLIMIT_FSIZE, &limit) != 0) return 1;
    limit.rlim_cur = archive_size + 10;
    if (setrlimit(RLIMIT_FSIZE, &limit) != 0) return 1;
    ok = mz_zip_add_mem_to_archive_file_in_place_v2(
        PATH, "new-entry", payload, sizeof(payload) - 1, NULL, 0,
        MZ_NO_COMPRESSION | MZ_ZIP_FLAG_ASCII_FILENAME, &err);
    u32((uint32_t)ok); u32((uint32_t)err);

    file = fopen(PATH, "rb");
    if (!file || fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) return 1;
    bytes = (unsigned char *)malloc((size_t)size);
    if (!bytes || fread(bytes, 1, (size_t)size, file) != (size_t)size || fclose(file) != 0) return 1;
    u32((uint32_t)size); fwrite(bytes, 1, (size_t)size, stdout); free(bytes);
    return 0;
}
