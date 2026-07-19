#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"

static const unsigned char plain[] =
    "zip error oracle zip error oracle zip error oracle";

static uint16_t get16(const unsigned char *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t get32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void put16(unsigned char *p, uint16_t value) {
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}

static void put32(FILE *file, uint32_t value) {
    unsigned char p[4];
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16);
    p[3] = (unsigned char)(value >> 24);
    fwrite(p, 1, sizeof(p), file);
}

static void record(FILE *file, int ok, mz_zip_archive *zip) {
    put32(file, (uint32_t)ok);
    put32(file, (uint32_t)mz_zip_get_last_error(zip));
}

static size_t fail_write(void *opaque, mz_uint64 offset, const void *data,
                         size_t size) {
    (void)opaque; (void)offset; (void)data; (void)size;
    return 0;
}

static int build_archive(void **data, size_t *size) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) return 0;
    if (!mz_zip_writer_add_mem(&zip, "Dir/File.BIN", plain, sizeof(plain) - 1, 6)) {
        mz_zip_writer_end(&zip);
        return 0;
    }
    if (!mz_zip_writer_finalize_heap_archive(&zip, data, size)) {
        mz_zip_writer_end(&zip);
        return 0;
    }
    return mz_zip_writer_end(&zip);
}

enum variant {
    V_BASE,
    V_METHOD,
    V_ENCRYPTED,
    V_STRONG,
    V_PATCHED,
    V_BAD_LOCAL,
    V_BAD_CRC,
    V_BAD_SIZE,
    V_BAD_NAME,
    V_NO_EOCD,
    V_MULTIDISK,
    V_MASKED,
    V_BAD_CENTRAL
};

static unsigned char *variant_copy(const unsigned char *base, size_t size,
                                   enum variant variant) {
    unsigned char *copy = (unsigned char *)malloc(size);
    size_t eocd = size - 22;
    size_t central;
    size_t local = 0;
    size_t payload;
    uint16_t flags;
    if (!copy) return NULL;
    memcpy(copy, base, size);
    central = get32(copy + eocd + 16);
    payload = 30 + get16(copy + 26) + get16(copy + 28);
    switch (variant) {
        case V_METHOD:
            put16(copy + central + 10, 99);
            put16(copy + local + 8, 99);
            break;
        case V_ENCRYPTED:
            flags = (uint16_t)(get16(copy + central + 8) | 1);
            put16(copy + central + 8, flags);
            put16(copy + local + 6, flags);
            break;
        case V_STRONG:
            flags = (uint16_t)(get16(copy + central + 8) | 0x40);
            put16(copy + central + 8, flags);
            put16(copy + local + 6, flags);
            break;
        case V_PATCHED:
            flags = (uint16_t)(get16(copy + central + 8) | 0x20);
            put16(copy + central + 8, flags);
            put16(copy + local + 6, flags);
            break;
        case V_BAD_LOCAL:
            copy[0] ^= 1;
            break;
        case V_BAD_CRC:
            copy[central + 16] ^= 1;
            copy[local + 14] ^= 1;
            break;
        case V_BAD_SIZE:
            copy[central + 24]++;
            copy[local + 22]++;
            break;
        case V_BAD_NAME:
            copy[local + 30] ^= 1;
            break;
        case V_NO_EOCD:
            copy[eocd] ^= 1;
            break;
        case V_MULTIDISK:
            put16(copy + eocd + 4, 2);
            put16(copy + eocd + 6, 2);
            break;
        case V_MASKED:
            flags = (uint16_t)(get16(copy + central + 8) | 0x2000);
            put16(copy + central + 8, flags);
            break;
        case V_BAD_CENTRAL:
            copy[central] ^= 1;
            break;
        case V_BASE:
            (void)payload;
            break;
    }
    return copy;
}

static int write_variant(const unsigned char *base, size_t size,
                         enum variant variant, const char *name) {
    unsigned char *copy = variant_copy(base, size, variant);
    FILE *file;
    if (!copy) return 0;
    file = fopen(name, "wb");
    if (!file) {
        free(copy);
        return 0;
    }
    if (fwrite(copy, 1, size, file) != size || fclose(file) != 0) {
        free(copy);
        return 0;
    }
    free(copy);
    return 1;
}

static int open_variant(mz_zip_archive *zip, const unsigned char *base,
                        size_t size, enum variant variant,
                        unsigned char **owned) {
    *owned = variant_copy(base, size, variant);
    if (!*owned) return 0;
    memset(zip, 0, sizeof(*zip));
    return mz_zip_reader_init_mem(zip, *owned, size, 0);
}

static void close_variant(mz_zip_archive *zip, unsigned char *owned) {
    mz_zip_reader_end(zip);
    free(owned);
}

int main(void) {
    void *base_void = NULL;
    unsigned char *base;
    size_t size = 0;
    unsigned char output[1024];
    unsigned char *owned;
    mz_zip_archive zip;
    mz_zip_archive_file_stat stat;
    FILE *file;
    int i;

    if (!build_archive(&base_void, &size)) return 1;
    base = (unsigned char *)base_void;
    if (!write_variant(base, size, V_BASE, "/tmp/miniz-zip-errors-base.zip") ||
        !write_variant(base, size, V_METHOD, "/tmp/miniz-zip-errors-method.zip") ||
        !write_variant(base, size, V_ENCRYPTED, "/tmp/miniz-zip-errors-encrypted.zip") ||
        !write_variant(base, size, V_STRONG, "/tmp/miniz-zip-errors-strong.zip") ||
        !write_variant(base, size, V_PATCHED, "/tmp/miniz-zip-errors-patched.zip") ||
        !write_variant(base, size, V_BAD_LOCAL, "/tmp/miniz-zip-errors-bad-local.zip") ||
        !write_variant(base, size, V_BAD_CRC, "/tmp/miniz-zip-errors-bad-crc.zip") ||
        !write_variant(base, size, V_BAD_SIZE, "/tmp/miniz-zip-errors-bad-size.zip") ||
        !write_variant(base, size, V_BAD_NAME, "/tmp/miniz-zip-errors-bad-name.zip") ||
        !write_variant(base, size, V_NO_EOCD, "/tmp/miniz-zip-errors-no-eocd.zip") ||
        !write_variant(base, size, V_MULTIDISK, "/tmp/miniz-zip-errors-multidisk.zip") ||
        !write_variant(base, size, V_MASKED, "/tmp/miniz-zip-errors-masked.zip") ||
        !write_variant(base, size, V_BAD_CENTRAL, "/tmp/miniz-zip-errors-bad-central.zip")) {
        mz_free(base);
        return 1;
    }

    file = fopen("/tmp/miniz-zip-errors.bin", "wb");
    if (!file) {
        mz_free(base);
        return 1;
    }

    /* Every public error string, including the out-of-range fallback. */
    for (i = 0; i <= MZ_ZIP_TOTAL_ERRORS; ++i) {
        const char *text = mz_zip_get_error_string((mz_zip_error)i);
        put32(file, (uint32_t)i);
        put32(file, (uint32_t)strlen(text));
        fwrite(text, 1, strlen(text), file);
    }
    {
        const char *text = mz_zip_get_error_string((mz_zip_error)999);
        put32(file, 999);
        put32(file, (uint32_t)strlen(text));
        fwrite(text, 1, strlen(text), file);
    }

    memset(&zip, 0, sizeof(zip));
    record(file, mz_zip_reader_init_mem(&zip, base, size, 0), &zip);
    put32(file, (uint32_t)mz_zip_set_last_error(&zip, MZ_ZIP_UNSUPPORTED_METHOD));
    put32(file, (uint32_t)mz_zip_peek_last_error(&zip));
    put32(file, (uint32_t)mz_zip_clear_last_error(&zip));
    put32(file, (uint32_t)mz_zip_peek_last_error(&zip));
    put32(file, (uint32_t)mz_zip_set_last_error(&zip, MZ_ZIP_CRC_CHECK_FAILED));
    put32(file, (uint32_t)mz_zip_get_last_error(&zip));
    put32(file, (uint32_t)mz_zip_peek_last_error(&zip));
    record(file, mz_zip_reader_is_file_a_directory(&zip, 99), &zip);
    record(file, mz_zip_reader_is_file_encrypted(&zip, 99), &zip);
    record(file, mz_zip_reader_is_file_supported(&zip, 99), &zip);
    record(file, mz_zip_reader_get_filename(&zip, 99, (char *)output, sizeof(output)) != 0, &zip);
    record(file, mz_zip_reader_file_stat(&zip, 99, &stat), &zip);
    record(file, mz_zip_reader_extract_to_mem(&zip, 99, output, sizeof(output), 0), &zip);
    record(file, mz_zip_reader_locate_file(&zip, "missing", NULL, 0) >= 0, &zip);
    record(file, mz_zip_reader_extract_to_mem(&zip, 0, output, 1, 0), &zip);
    record(file, mz_zip_reader_is_file_supported(&zip, 0), &zip);
    record(file, mz_zip_reader_locate_file(&zip, "dir/file.bin", NULL, 0) >= 0, &zip);
    record(file, mz_zip_reader_locate_file(&zip, "dir/file.bin", NULL, MZ_ZIP_FLAG_CASE_SENSITIVE) >= 0, &zip);
    record(file, mz_zip_reader_locate_file(&zip, "Dir/File.BIN", NULL, MZ_ZIP_FLAG_CASE_SENSITIVE) >= 0, &zip);
    record(file, mz_zip_reader_locate_file(&zip, "File.BIN", NULL, 0) >= 0, &zip);
    record(file, mz_zip_reader_locate_file(&zip, "File.BIN", NULL, MZ_ZIP_FLAG_IGNORE_PATH) >= 0, &zip);
    record(file, mz_zip_reader_locate_file(&zip, "file.bin", NULL, MZ_ZIP_FLAG_IGNORE_PATH | MZ_ZIP_FLAG_CASE_SENSITIVE) >= 0, &zip);
    record(file, mz_zip_validate_archive(&zip, 0), &zip);
    record(file, mz_zip_validate_archive(&zip, MZ_ZIP_FLAG_VALIDATE_HEADERS_ONLY), &zip);
    record(file, mz_zip_validate_archive(&zip, MZ_ZIP_FLAG_VALIDATE_LOCATE_FILE_FLAG), &zip);
    record(file, mz_zip_validate_archive(&zip, MZ_ZIP_FLAG_VALIDATE_HEADERS_ONLY | MZ_ZIP_FLAG_VALIDATE_LOCATE_FILE_FLAG), &zip);
    record(file, mz_zip_reader_end(&zip), &zip);
    record(file, mz_zip_reader_end(&zip), &zip);

    if (!open_variant(&zip, base, size, V_METHOD, &owned)) return 1;
    record(file, mz_zip_reader_is_file_supported(&zip, 0), &zip);
    record(file, mz_zip_reader_extract_to_mem(&zip, 0, output, sizeof(output), 0), &zip);
    record(file, mz_zip_reader_extract_to_mem(&zip, 0, output, sizeof(output), MZ_ZIP_FLAG_COMPRESSED_DATA), &zip);
    close_variant(&zip, owned);

    if (!open_variant(&zip, base, size, V_ENCRYPTED, &owned)) return 1;
    record(file, mz_zip_reader_is_file_encrypted(&zip, 0), &zip);
    record(file, mz_zip_reader_is_file_supported(&zip, 0), &zip);
    record(file, mz_zip_reader_extract_to_mem(&zip, 0, output, sizeof(output), 0), &zip);
    record(file, mz_zip_reader_extract_to_mem(&zip, 0, output, sizeof(output), MZ_ZIP_FLAG_COMPRESSED_DATA), &zip);
    close_variant(&zip, owned);

    if (!open_variant(&zip, base, size, V_STRONG, &owned)) return 1;
    record(file, mz_zip_reader_is_file_encrypted(&zip, 0), &zip);
    record(file, mz_zip_reader_is_file_supported(&zip, 0), &zip);
    record(file, mz_zip_reader_extract_to_mem(&zip, 0, output, sizeof(output), 0), &zip);
    record(file, mz_zip_reader_extract_to_mem(&zip, 0, output, sizeof(output), MZ_ZIP_FLAG_COMPRESSED_DATA), &zip);
    close_variant(&zip, owned);

    if (!open_variant(&zip, base, size, V_PATCHED, &owned)) return 1;
    record(file, mz_zip_reader_is_file_supported(&zip, 0), &zip);
    record(file, mz_zip_reader_extract_to_mem(&zip, 0, output, sizeof(output), 0), &zip);
    record(file, mz_zip_validate_file(&zip, 0, MZ_ZIP_FLAG_VALIDATE_HEADERS_ONLY), &zip);
    close_variant(&zip, owned);

    if (!open_variant(&zip, base, size, V_BAD_LOCAL, &owned)) return 1;
    record(file, mz_zip_reader_extract_to_mem(&zip, 0, output, sizeof(output), 0), &zip);
    record(file, mz_zip_validate_file(&zip, 0, MZ_ZIP_FLAG_VALIDATE_HEADERS_ONLY), &zip);
    close_variant(&zip, owned);

    if (!open_variant(&zip, base, size, V_BAD_CRC, &owned)) return 1;
    record(file, mz_zip_reader_extract_to_mem(&zip, 0, output, sizeof(output), 0), &zip);
    record(file, mz_zip_validate_file(&zip, 0, MZ_ZIP_FLAG_VALIDATE_HEADERS_ONLY), &zip);
    close_variant(&zip, owned);

    if (!open_variant(&zip, base, size, V_BAD_SIZE, &owned)) return 1;
    record(file, mz_zip_reader_extract_to_mem(&zip, 0, output, sizeof(output), 0), &zip);
    record(file, mz_zip_validate_file(&zip, 0, MZ_ZIP_FLAG_VALIDATE_HEADERS_ONLY), &zip);
    close_variant(&zip, owned);

    if (!open_variant(&zip, base, size, V_BAD_NAME, &owned)) return 1;
    record(file, mz_zip_reader_extract_to_mem(&zip, 0, output, sizeof(output), 0), &zip);
    record(file, mz_zip_validate_file(&zip, 0, MZ_ZIP_FLAG_VALIDATE_HEADERS_ONLY), &zip);
    close_variant(&zip, owned);

    {
        unsigned char tiny[10] = {0};
        memset(&zip, 0, sizeof(zip));
        record(file, mz_zip_reader_init_mem(&zip, tiny, sizeof(tiny), 0), &zip);
    }
    owned = variant_copy(base, size, V_NO_EOCD);
    memset(&zip, 0, sizeof(zip));
    record(file, mz_zip_reader_init_mem(&zip, owned, size, 0), &zip);
    free(owned);
    owned = variant_copy(base, size, V_MULTIDISK);
    memset(&zip, 0, sizeof(zip));
    record(file, mz_zip_reader_init_mem(&zip, owned, size, 0), &zip);
    free(owned);
    owned = variant_copy(base, size, V_MASKED);
    memset(&zip, 0, sizeof(zip));
    record(file, mz_zip_reader_init_mem(&zip, owned, size, 0), &zip);
    free(owned);
    owned = variant_copy(base, size, V_BAD_CENTRAL);
    memset(&zip, 0, sizeof(zip));
    record(file, mz_zip_reader_init_mem(&zip, owned, size, 0), &zip);
    free(owned);

    {
        void *writer_data = NULL;
        size_t writer_size = 0;
        memset(&zip, 0, sizeof(zip));
        record(file, mz_zip_writer_init_heap(&zip, 0, 0), &zip);
        record(file, mz_zip_writer_add_mem(&zip, "", "x", 1, 0), &zip);
        record(file, mz_zip_writer_add_mem(&zip, "/bad", "x", 1, 0), &zip);
        record(file, mz_zip_writer_add_mem(&zip, "level", "payload", 7, 11), &zip);
        record(file, mz_zip_writer_add_mem(&zip, "dir/", "x", 1, 0), &zip);
        record(file, mz_zip_writer_finalize_heap_archive(&zip, &writer_data, &writer_size), &zip);
        mz_free(writer_data);
        record(file, mz_zip_writer_finalize_archive(&zip), &zip);
        record(file, mz_zip_writer_end(&zip), &zip);
        record(file, mz_zip_writer_end(&zip), &zip);
    }
    memset(&zip, 0, sizeof(zip));
    zip.m_file_offset_alignment = 3;
    record(file, mz_zip_writer_init_heap(&zip, 0, 0), &zip);

    memset(&zip, 0, sizeof(zip));
    zip.m_pWrite = fail_write;
    zip.m_pIO_opaque = NULL;
    record(file, mz_zip_writer_init(&zip, 0), &zip);
    record(file, mz_zip_writer_add_mem(&zip, "x", "x", 1, 0), &zip);
    record(file, mz_zip_writer_end(&zip), &zip);
    record(file, mz_zip_writer_end(&zip), &zip);

    mz_free(base);
    return fclose(file) != 0;
}
