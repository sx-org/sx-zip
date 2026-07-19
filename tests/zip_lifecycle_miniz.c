#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"

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
    u32((uint32_t)mz_zip_peek_last_error(zip));
}

static int make_base(void **bytes, size_t *size) {
    mz_zip_archive zip;
    FILE *file;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0) ||
        !mz_zip_writer_add_mem_ex_v2(&zip, "x", "abc", 3, NULL, 0,
                                     MZ_NO_COMPRESSION, 0, 0, NULL,
                                     NULL, 0, NULL, 0) ||
        !mz_zip_writer_finalize_heap_archive(&zip, bytes, size) ||
        !mz_zip_writer_end(&zip)) return 0;
    file = fopen("/tmp/miniz-zip-lifecycle-base.zip", "wb");
    if (!file) return 0;
    if (fwrite(*bytes, 1, *size, file) != *size || fclose(file) != 0) return 0;
    return 1;
}

int main(void) {
    mz_zip_archive zip;
    void *base = NULL, *copy;
    size_t base_size = 0, made;
    unsigned char buf[512];

    if (!make_base(&base, &base_size)) return 1;
    out = fopen("/tmp/miniz-zip-lifecycle.bin", "wb");
    if (!out) return 1;

    memset(&zip, 0, sizeof(zip));
    snapshot(&zip); u32((uint32_t)mz_zip_end(&zip)); snapshot(&zip);

    result(mz_zip_reader_init_mem(&zip, base, base_size, 0), &zip); snapshot(&zip);
    memset(buf, 0, sizeof(buf));
    made = mz_zip_read_archive_data(&zip, 0, buf, 4);
    u64(made); fwrite(buf, 1, 4, out); u32((uint32_t)mz_zip_get_last_error(&zip));
    result(mz_zip_reader_init_mem(&zip, base, base_size, 0), &zip); snapshot(&zip);
    result(mz_zip_reader_end(&zip), &zip); snapshot(&zip);
    result(mz_zip_reader_end(&zip), &zip); snapshot(&zip);
    result(mz_zip_reader_init_mem(&zip, base, base_size, 0), &zip); snapshot(&zip);
    result(mz_zip_reader_end(&zip), &zip);

    memset(&zip, 0, sizeof(zip));
    result(mz_zip_writer_init_heap(&zip, 0, 0), &zip); snapshot(&zip);
    made = mz_zip_read_archive_data(&zip, 0, buf, 4);
    u64(made); u32((uint32_t)mz_zip_get_last_error(&zip));
    result(mz_zip_writer_end(&zip), &zip);

    memset(&zip, 0, sizeof(zip));
    result(mz_zip_writer_init_heap_v2(&zip, 0, 0, MZ_ZIP_FLAG_WRITE_ALLOW_READING), &zip); snapshot(&zip);
    result(mz_zip_writer_add_mem_ex_v2(&zip, "x", "abc", 3, NULL, 0, MZ_NO_COMPRESSION,
                                       0, 0, NULL, NULL, 0, NULL, 0), &zip); snapshot(&zip);
    memset(buf, 0, sizeof(buf));
    made = mz_zip_read_archive_data(&zip, 0, buf, 4);
    u64(made); fwrite(buf, 1, 4, out); u32((uint32_t)mz_zip_get_last_error(&zip));
    result(mz_zip_writer_finalize_archive(&zip), &zip); snapshot(&zip);
    made = mz_zip_read_archive_data(&zip, 0, buf, sizeof(buf));
    u64(made); u32((uint32_t)mz_crc32(MZ_CRC32_INIT, buf, made));
    result(mz_zip_writer_add_mem(&zip, "y", "z", 1, 0), &zip);
    result(mz_zip_writer_end(&zip), &zip); snapshot(&zip);
    result(mz_zip_writer_end(&zip), &zip); snapshot(&zip);

    memset(&zip, 0, sizeof(zip));
    result(mz_zip_writer_init_heap_v2(&zip, 0, 0,
           MZ_ZIP_FLAG_WRITE_ZIP64 | MZ_ZIP_FLAG_WRITE_ALLOW_READING), &zip); snapshot(&zip);
    result(mz_zip_writer_finalize_archive(&zip), &zip); snapshot(&zip);
    made = mz_zip_read_archive_data(&zip, 0, buf, sizeof(buf));
    u64(made); u32((uint32_t)mz_crc32(MZ_CRC32_INIT, buf, made));
    result(mz_zip_writer_end(&zip), &zip);

    copy = malloc(base_size);
    if (!copy) return 1;
    memcpy(copy, base, base_size);
    memset(&zip, 0, sizeof(zip));
    result(mz_zip_reader_init_mem(&zip, copy, base_size, 0), &zip); snapshot(&zip);
    result(mz_zip_writer_init_from_reader(&zip, NULL), &zip); snapshot(&zip);
    made = mz_zip_read_archive_data(&zip, 0, buf, 4);
    u64(made); fwrite(buf, 1, 4, out); u32((uint32_t)mz_zip_get_last_error(&zip));
    result(mz_zip_writer_add_mem_ex_v2(&zip, "y", "def", 3, NULL, 0, MZ_NO_COMPRESSION,
                                       0, 0, NULL, NULL, 0, NULL, 0), &zip);
    result(mz_zip_writer_finalize_archive(&zip), &zip); snapshot(&zip);
    made = mz_zip_read_archive_data(&zip, 0, buf, sizeof(buf));
    u64(made); u32((uint32_t)mz_crc32(MZ_CRC32_INIT, buf, made));
    result(mz_zip_writer_end(&zip), &zip);

    mz_free(base);
    return fclose(out) != 0;
}
