#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"

static void put32(FILE *f, uint32_t v) {
    unsigned char b[4] = {(unsigned char)v, (unsigned char)(v >> 8),
                          (unsigned char)(v >> 16), (unsigned char)(v >> 24)};
    fwrite(b, 1, 4, f);
}

static void put_state(FILE *f, int ok, mz_zip_archive *zip) {
    put32(f, (uint32_t)ok);
    put32(f, (uint32_t)mz_zip_get_last_error(zip));
}

static int write_file(const char *path, const void *data, size_t size) {
    FILE *f = fopen(path, "wb");
    int ok = f && fwrite(data, 1, size, f) == size;
    return f && fclose(f) == 0 && ok;
}

static uint16_t rd16(const unsigned char *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t rd32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int corrupt_central_crc(unsigned char *data, size_t size, unsigned wanted) {
    size_t eocd = size, at;
    unsigned index = 0;
    while (eocd >= 22) {
        eocd--;
        if (rd32(data + eocd) == 0x06054b50U) break;
    }
    if (eocd < 22 || rd32(data + eocd) != 0x06054b50U) return 0;
    at = rd32(data + eocd + 16);
    while (at + 46 <= eocd && rd32(data + at) == 0x02014b50U) {
        if (index++ == wanted) {
            data[at + 16] ^= 0x5a;
            return 1;
        }
        at += 46 + rd16(data + at + 28) + rd16(data + at + 30) + rd16(data + at + 32);
    }
    return 0;
}

static int build(void **archive, size_t *archive_size) {
    static const unsigned char stored[] = "stored iterator payload";
    unsigned char *large = (unsigned char *)malloc(100000);
    mz_zip_archive zip;
    size_t i;
    if (!large) return 0;
    for (i = 0; i < 100000; ++i) large[i] = (unsigned char)((i * 37U + (i >> 7)) & 255U);
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0) ||
        !mz_zip_writer_add_mem(&zip, "stored", stored, sizeof(stored) - 1, MZ_NO_COMPRESSION) ||
        !mz_zip_writer_add_mem(&zip, "Dir/deflated", large, 100000, MZ_BEST_COMPRESSION) ||
        !mz_zip_writer_finalize_heap_archive(&zip, archive, archive_size) ||
        !mz_zip_writer_end(&zip)) {
        free(large);
        return 0;
    }
    free(large);
    return write_file("/tmp/miniz-zip-iterator.zip", *archive, *archive_size);
}

static void no_read_case(FILE *out, mz_zip_archive *zip, unsigned index, unsigned flags) {
    mz_zip_reader_extract_iter_state *it;
    mz_zip_clear_last_error(zip);
    it = mz_zip_reader_extract_iter_new(zip, index, flags);
    put_state(out, it != NULL, zip);
    put_state(out, it && mz_zip_reader_extract_iter_free(it), zip);
}

static void read_case(FILE *out, mz_zip_archive *zip, unsigned index, unsigned flags,
                      size_t first_size, int drain) {
    unsigned char buf[120000];
    mz_zip_reader_extract_iter_state *it;
    size_t made, total = 0;
    mz_uint32 crc = MZ_CRC32_INIT;
    mz_zip_clear_last_error(zip);
    it = mz_zip_reader_extract_iter_new(zip, index, flags);
    put_state(out, it != NULL, zip);
    if (!it) return;
    made = mz_zip_reader_extract_iter_read(it, buf, first_size);
    total += made;
    crc = (mz_uint32)mz_crc32(crc, buf, made);
    put32(out, (uint32_t)made);
    put32(out, crc);
    put32(out, (uint32_t)mz_zip_get_last_error(zip));
    if (drain) {
        do {
            made = mz_zip_reader_extract_iter_read(it, buf, 997);
            total += made;
            crc = (mz_uint32)mz_crc32(crc, buf, made);
        } while (made);
        put32(out, (uint32_t)total);
        put32(out, crc);
        put32(out, (uint32_t)mz_zip_get_last_error(zip));
    }
    put_state(out, mz_zip_reader_extract_iter_free(it), zip);
}

static int corrupt_case(FILE *out, const unsigned char *archive, size_t archive_size, unsigned index) {
    unsigned char *copy = (unsigned char *)malloc(archive_size);
    unsigned char buf[120000];
    mz_zip_reader_extract_iter_state *it;
    mz_zip_archive zip;
    size_t made;
    if (!copy) return 0;
    memcpy(copy, archive, archive_size);
    if (!corrupt_central_crc(copy, archive_size, index)) return 0;
    memset(&zip, 0, sizeof(zip));
    put_state(out, mz_zip_reader_init_mem(&zip, copy, archive_size, 0), &zip);
    it = mz_zip_reader_extract_iter_new(&zip, index, 0);
    put_state(out, it != NULL, &zip);
    made = it ? mz_zip_reader_extract_iter_read(it, buf, sizeof(buf)) : 0;
    put32(out, (uint32_t)made);
    put32(out, (uint32_t)mz_zip_get_last_error(&zip));
    put_state(out, it && mz_zip_reader_extract_iter_free(it), &zip);
    put_state(out, mz_zip_reader_end(&zip), &zip);
    free(copy);
    return 1;
}

static int unsupported_case(FILE *out, const unsigned char *archive, size_t archive_size, unsigned which) {
    unsigned char *copy = (unsigned char *)malloc(archive_size);
    size_t eocd = archive_size - 22, central;
    char path[64];
    mz_zip_archive zip;
    mz_zip_reader_extract_iter_state *it;
    if (!copy) return 0;
    memcpy(copy, archive, archive_size);
    central = rd32(copy + eocd + 16);
    if (which == 0) {
        copy[8] = copy[central + 10] = 99; copy[9] = copy[central + 11] = 0;
    } else {
        uint16_t flag = (uint16_t)(rd16(copy + 6) | (which == 1 ? 1 : 0x20));
        copy[6] = copy[central + 8] = (unsigned char)flag;
        copy[7] = copy[central + 9] = (unsigned char)(flag >> 8);
    }
    snprintf(path, sizeof(path), "/tmp/miniz-zip-iterator-unsupported-%u.zip", which);
    if (!write_file(path, copy, archive_size)) return 0;
    memset(&zip, 0, sizeof(zip));
    put_state(out, mz_zip_reader_init_mem(&zip, copy, archive_size, 0), &zip);
    it = mz_zip_reader_extract_iter_new(&zip, 0, 0);
    put_state(out, it != NULL, &zip); if (it) put_state(out, mz_zip_reader_extract_iter_free(it), &zip);
    it = mz_zip_reader_extract_iter_new(&zip, 0, MZ_ZIP_FLAG_COMPRESSED_DATA);
    put_state(out, it != NULL, &zip); if (it) put_state(out, mz_zip_reader_extract_iter_free(it), &zip);
    put_state(out, mz_zip_reader_end(&zip), &zip);
    free(copy);
    return 1;
}

int main(void) {
    void *archive = NULL;
    size_t archive_size = 0;
    mz_zip_reader_extract_iter_state *it;
    mz_zip_archive zip;
    FILE *out;
    if (!build(&archive, &archive_size)) return 1;
    out = fopen("/tmp/miniz-zip-iterator.bin", "wb");
    if (!out) return 1;
    memset(&zip, 0, sizeof(zip));
    put_state(out, mz_zip_reader_init_mem(&zip, archive, archive_size, 0), &zip);

    it = mz_zip_reader_extract_iter_new(&zip, 99, 0);
    put_state(out, it != NULL, &zip);
    it = mz_zip_reader_extract_file_iter_new(&zip, "missing", 0);
    put_state(out, it != NULL, &zip);
    it = mz_zip_reader_extract_file_iter_new(&zip, "deflated", MZ_ZIP_FLAG_IGNORE_PATH);
    put_state(out, it != NULL, &zip);
    if (it) put_state(out, mz_zip_reader_extract_iter_free(it), &zip);
    it = mz_zip_reader_extract_file_iter_new(&zip, "dir/deflated", MZ_ZIP_FLAG_CASE_SENSITIVE);
    put_state(out, it != NULL, &zip);

    no_read_case(out, &zip, 0, 0);
    read_case(out, &zip, 0, 0, 0, 0);
    read_case(out, &zip, 0, 0, 1, 0);
    read_case(out, &zip, 0, 0, 120000, 0);
    read_case(out, &zip, 0, 0, 1, 1);
    no_read_case(out, &zip, 0, MZ_ZIP_FLAG_COMPRESSED_DATA);
    read_case(out, &zip, 0, MZ_ZIP_FLAG_COMPRESSED_DATA, 1, 0);

    no_read_case(out, &zip, 1, 0);
    read_case(out, &zip, 1, 0, 0, 0);
    read_case(out, &zip, 1, 0, 1, 0);
    read_case(out, &zip, 1, 0, 120000, 0);
    read_case(out, &zip, 1, 0, 1, 1);
    no_read_case(out, &zip, 1, MZ_ZIP_FLAG_COMPRESSED_DATA);
    read_case(out, &zip, 1, MZ_ZIP_FLAG_COMPRESSED_DATA, 1, 0);

    put_state(out, mz_zip_reader_end(&zip), &zip);
    if (!corrupt_case(out, archive, archive_size, 0) ||
        !corrupt_case(out, archive, archive_size, 1) ||
        !unsupported_case(out, archive, archive_size, 0) ||
        !unsupported_case(out, archive, archive_size, 1) ||
        !unsupported_case(out, archive, archive_size, 2)) return 1;
    mz_free(archive);
    return fclose(out) != 0;
}
