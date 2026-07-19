#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "miniz.h"

static FILE *out;

static void u32(uint32_t value) {
    unsigned char b[4] = {(unsigned char)value, (unsigned char)(value >> 8),
                          (unsigned char)(value >> 16), (unsigned char)(value >> 24)};
    fwrite(b, 1, sizeof(b), out);
}

static void result(int ok, mz_zip_archive *zip) {
    u32((uint32_t)ok);
    u32((uint32_t)mz_zip_get_last_error(zip));
}

static int build_archive(void **data, size_t *size) {
    static const char payload[] = "filesystem adapter failure payload filesystem adapter failure payload";
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0) ||
        !mz_zip_writer_add_mem_ex_v2(&zip, "payload", payload, sizeof(payload) - 1,
                                     NULL, 0, 6, 0, 0, NULL, NULL, 0, NULL, 0) ||
        !mz_zip_writer_finalize_heap_archive(&zip, data, size)) return 0;
    return mz_zip_writer_end(&zip);
}

int main(void) {
    void *archive = NULL, *empty_archive = NULL;
    size_t archive_size = 0, empty_archive_size = 0;
    mz_zip_archive zip;
    mz_zip_archive_file_stat stat;
    FILE *file;

    out = fopen("/tmp/miniz-zip-stdio-failures.bin", "wb");
    if (!out || !build_archive(&archive, &archive_size)) return 1;

    memset(&zip, 0, sizeof(zip));
    result(mz_zip_reader_init_mem(&zip, archive, archive_size, 0), &zip);
    result(mz_zip_reader_extract_to_file(&zip, 0, "/tmp/miniz-missing-dir/out", 0), &zip);
    file = tmpfile();
    if (!file || setvbuf(file, NULL, _IONBF, 0) != 0) return 2;
    if (close(fileno(file)) != 0) return 3;
    result(mz_zip_reader_extract_to_cfile(&zip, 0, file, 0), &zip);
    fclose(file);
    result(mz_zip_reader_end(&zip), &zip);

    memset(&zip, 0, sizeof(zip));
    result(mz_zip_writer_init_heap(&zip, 0, 0), &zip);
    result(mz_zip_writer_add_file(&zip, "missing", "/tmp/miniz-no-such-input",
                                  NULL, 0, 6), &zip);
    result(mz_zip_writer_end(&zip), &zip);

    memset(&zip, 0, sizeof(zip));
    result(mz_zip_writer_init_file(&zip, "/tmp/miniz-missing-dir/out.zip", 0), &zip);

    file = tmpfile();
    if (!file || setvbuf(file, NULL, _IONBF, 0) != 0) return 4;
    memset(&zip, 0, sizeof(zip));
    result(mz_zip_writer_init_cfile(&zip, file, 0), &zip);
    if (close(fileno(file)) != 0) return 5;
    result(mz_zip_writer_add_mem(&zip, "entry", "data", 4, 0), &zip);
    result(mz_zip_writer_finalize_archive(&zip), &zip);
    result(mz_zip_writer_end(&zip), &zip);
    fclose(file);

    file = tmpfile();
    if (!file || setvbuf(file, NULL, _IONBF, 0) != 0 ||
        fwrite("source", 1, 6, file) != 6 || close(fileno(file)) != 0) return 6;
    memset(&zip, 0, sizeof(zip));
    result(mz_zip_writer_init_heap(&zip, 0, 0), &zip);
    result(mz_zip_writer_add_cfile(&zip, "short", file, 6, NULL, NULL, 0, 0,
                                   NULL, 0, NULL, 0), &zip);
    result(mz_zip_writer_finalize_heap_archive(&zip, &empty_archive, &empty_archive_size), &zip);
    result(mz_zip_writer_end(&zip), &zip);
    fclose(file);
    memset(&zip, 0, sizeof(zip));
    result(mz_zip_reader_init_mem(&zip, empty_archive, empty_archive_size, 0), &zip);
    if (mz_zip_reader_file_stat(&zip, 0, &stat)) u32((uint32_t)stat.m_uncomp_size);
    else u32(0xffffffffU);
    result(mz_zip_reader_end(&zip), &zip);

    mz_free(empty_archive);
    mz_free(archive);
    return fclose(out) != 0;
}
