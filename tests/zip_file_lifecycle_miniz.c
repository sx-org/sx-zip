#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "miniz.h"

#define PATH_ARCHIVE "/tmp/miniz-file-lifecycle-path.zip"
#define CFILE_ARCHIVE "/tmp/miniz-file-lifecycle-cfile.bin"

static FILE *out;

static void u32(uint32_t value) {
    unsigned char b[4] = {(unsigned char)value, (unsigned char)(value >> 8),
                          (unsigned char)(value >> 16), (unsigned char)(value >> 24)};
    fwrite(b, 1, sizeof(b), out);
}

static void u64(uint64_t value) { u32((uint32_t)value); u32((uint32_t)(value >> 32)); }

static void result(mz_bool ok, mz_zip_archive *zip) {
    u32((uint32_t)ok); u32((uint32_t)mz_zip_get_last_error(zip));
}

static void snapshot(mz_zip_archive *zip) {
    u32((uint32_t)mz_zip_get_mode(zip));
    u32((uint32_t)mz_zip_get_type(zip));
    u32((uint32_t)mz_zip_reader_get_num_files(zip));
    u64(mz_zip_get_archive_size(zip));
    u64(mz_zip_get_archive_file_start_offset(zip));
    u64((uint64_t)mz_zip_get_central_dir_size(zip));
    u32((uint32_t)mz_zip_is_zip64(zip));
    u32(mz_zip_get_cfile(zip) != NULL);
}

static int blob(const char *path) {
    FILE *file = fopen(path, "rb");
    unsigned char buffer[4096];
    long size;
    if (!file || fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0 || size > (long)sizeof(buffer)) return 0;
    u32((uint32_t)size);
    if (fread(buffer, 1, (size_t)size, file) != (size_t)size ||
        fwrite(buffer, 1, (size_t)size, out) != (size_t)size) return 0;
    return fclose(file) == 0;
}

static int path_case(void) {
    static const char payload[] = "stateful-file-writer-payload";
    mz_zip_archive zip;
    unsigned char magic[4] = {0};
    size_t made;
    memset(&zip, 0, sizeof(zip));
    result(mz_zip_writer_init_file_v2(&zip, PATH_ARCHIVE, 17,
                                      MZ_ZIP_FLAG_WRITE_ALLOW_READING), &zip);
    snapshot(&zip);
    result(mz_zip_writer_add_mem_ex_v2(&zip, "entry", payload, sizeof(payload) - 1,
                                       NULL, 0, MZ_NO_COMPRESSION | MZ_ZIP_FLAG_ASCII_FILENAME,
                                       0, 0, NULL, NULL, 0, NULL, 0), &zip);
    made = mz_zip_read_archive_data(&zip, 17, magic, sizeof(magic));
    u64(made); fwrite(magic, 1, sizeof(magic), out); u32((uint32_t)mz_zip_get_last_error(&zip));
    result(mz_zip_writer_finalize_archive(&zip), &zip); snapshot(&zip);
    result(mz_zip_writer_end(&zip), &zip); snapshot(&zip);
    return blob(PATH_ARCHIVE);
}

static int cfile_case(void) {
    static const char payload[] = "borrowed-handle-deflate-payload-borrowed-handle-deflate-payload";
    static const char prefix[] = "PFX!!";
    static const char suffix[] = "END";
    mz_zip_archive zip;
    unsigned char magic[4] = {0};
    FILE *file = fopen(CFILE_ARCHIVE, "w+b");
    size_t made;
    if (!file || fwrite(prefix, 1, sizeof(prefix) - 1, file) != sizeof(prefix) - 1) return 0;
    memset(&zip, 0, sizeof(zip));
    result(mz_zip_writer_init_cfile(&zip, file, MZ_ZIP_FLAG_WRITE_ALLOW_READING), &zip);
    snapshot(&zip);
    result(mz_zip_writer_add_mem_ex_v2(&zip, "entry", payload, sizeof(payload) - 1,
                                       NULL, 0, 6 | MZ_ZIP_FLAG_ASCII_FILENAME,
                                       0, 0, NULL, NULL, 0, NULL, 0), &zip);
    made = mz_zip_read_archive_data(&zip, 0, magic, sizeof(magic));
    u64(made); fwrite(magic, 1, sizeof(magic), out); u32((uint32_t)mz_zip_get_last_error(&zip));
    result(mz_zip_writer_finalize_archive(&zip), &zip); snapshot(&zip);
    result(mz_zip_writer_end(&zip), &zip);
    u32(fwrite(suffix, 1, sizeof(suffix) - 1, file) == sizeof(suffix) - 1);
    if (fclose(file) != 0) return 0;
    return blob(CFILE_ARCHIVE);
}

static int close_failure_cases(void) {
    mz_zip_archive zip;
    FILE *file;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_file(&zip, "/tmp/miniz-file-lifecycle-close.zip", 0) ||
        !mz_zip_writer_finalize_archive(&zip)) return 0;
    file = mz_zip_get_cfile(&zip);
    if (!file || close(fileno(file)) != 0) return 0;
    result(mz_zip_writer_end(&zip), &zip);

    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_file(&zip, PATH_ARCHIVE, 0)) return 0;
    file = mz_zip_get_cfile(&zip);
    if (!file || close(fileno(file)) != 0) return 0;
    result(mz_zip_reader_end(&zip), &zip);
    return 1;
}

int main(void) {
    out = fopen("/tmp/miniz-file-lifecycle.bin", "wb");
    if (!out) return 1;
    if (!path_case() || !cfile_case() || !close_failure_cases()) return 1;
    return fclose(out) != 0;
}
