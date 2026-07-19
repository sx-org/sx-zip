#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"

static const unsigned char repeated[] =
    "miniz exact miniz exact miniz exact miniz exact miniz exact";
static const unsigned char local_extra[] = { 0xfe, 0xca, 3, 0, 'a', 'b', 'c' };
static const unsigned char central_extra[] = { 0xef, 0xbe, 2, 0, 'X', 'Y' };

struct capture { unsigned char *data; size_t size, capacity; };

static size_t capture_write(void *opaque, mz_uint64 offset, const void *buf, size_t n) {
    struct capture *capture = (struct capture *)opaque;
    size_t wanted = capture->size + n;
    (void)offset;
    if (wanted > capture->capacity) {
        size_t capacity = capture->capacity ? capture->capacity * 2 : 256;
        unsigned char *next;
        while (capacity < wanted) capacity *= 2;
        next = (unsigned char *)realloc(capture->data, capacity);
        if (!next) return 0;
        capture->data = next;
        capture->capacity = capacity;
    }
    memcpy(capture->data + capture->size, buf, n);
    capture->size += n;
    return n;
}

static size_t source_read(void *opaque, mz_uint64 offset, void *buf, size_t n) {
    const unsigned char *data = (const unsigned char *)opaque;
    size_t size = sizeof(repeated) - 1;
    if (offset >= size) return 0;
    if (n > size - (size_t)offset) n = size - (size_t)offset;
    memcpy(buf, data + offset, n);
    return n;
}

static int save_archive(mz_zip_archive *zip, const char *path) {
    void *data = NULL;
    size_t size = 0;
    FILE *file;
    int ok = mz_zip_writer_finalize_heap_archive(zip, &data, &size) != 0;
    ok = mz_zip_writer_end(zip) && ok;
    if (!ok) { mz_free(data); return 0; }
    file = fopen(path, "wb");
    if (!file) { mz_free(data); return 0; }
    ok = fwrite(data, 1, size, file) == size;
    ok = fclose(file) == 0 && ok;
    mz_free(data);
    return ok;
}

static int descriptor_archive(void) {
    static const unsigned char stored[] = "stored payload";
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) return 0;
    if (!mz_zip_writer_add_mem_ex_v2(&zip, "stored.txt", stored, sizeof(stored) - 1,
            "stored comment", 14, MZ_NO_COMPRESSION, 0, 0, NULL,
            (const char *)local_extra, sizeof(local_extra),
            (const char *)central_extra, sizeof(central_extra))) return 0;
    if (!mz_zip_writer_add_mem(&zip, "deflated.txt", repeated, sizeof(repeated) - 1, 6)) return 0;
    if (!mz_zip_writer_add_mem(&zip, "empty/", NULL, 0, MZ_NO_COMPRESSION)) return 0;
    return save_archive(&zip, "/tmp/miniz-zip-exact-descriptor.zip");
}

static int patched_archive(void) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) return 0;
    if (!mz_zip_writer_add_read_buf_callback(&zip, "callback.txt", source_read,
            (void *)repeated, sizeof(repeated) - 1, NULL, "callback comment", 16,
            9 | MZ_ZIP_FLAG_WRITE_HEADER_SET_SIZE | MZ_ZIP_FLAG_ASCII_FILENAME,
            (const char *)local_extra, sizeof(local_extra),
            (const char *)central_extra, sizeof(central_extra))) return 0;
    return save_archive(&zip, "/tmp/miniz-zip-exact-patched.zip");
}

static int raw_archive(void) {
    mz_zip_archive zip;
    void *packed;
    size_t packed_size = 0;
    mz_uint32 sum = (mz_uint32)mz_crc32(MZ_CRC32_INIT, repeated, sizeof(repeated) - 1);
    packed = tdefl_compress_mem_to_heap(repeated, sizeof(repeated) - 1, &packed_size,
        tdefl_create_comp_flags_from_zip_params(9, -15, MZ_DEFAULT_STRATEGY));
    if (!packed) return 0;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) { mz_free(packed); return 0; }
    if (!mz_zip_writer_add_mem_ex(&zip, "raw.txt", packed, packed_size,
            "raw comment", 11, 9 | MZ_ZIP_FLAG_COMPRESSED_DATA,
            sizeof(repeated) - 1, sum)) { mz_free(packed); return 0; }
    mz_free(packed);
    return save_archive(&zip, "/tmp/miniz-zip-exact-raw.zip");
}

static int zip64_archive(void) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap_v2(&zip, 0, 0, MZ_ZIP_FLAG_WRITE_ZIP64)) return 0;
    if (!mz_zip_writer_add_mem(&zip, "zip64.txt", repeated, sizeof(repeated) - 1, 6)) return 0;
    return save_archive(&zip, "/tmp/miniz-zip-exact-zip64.zip");
}

static int automatic_zip64_archive(void) {
    static const unsigned char payload[] = "payload";
    struct capture capture = { 0 };
    mz_zip_archive zip;
    FILE *file;
    int ok;
    memset(&zip, 0, sizeof(zip));
    zip.m_pWrite = capture_write;
    zip.m_pIO_opaque = &capture;
    if (!mz_zip_writer_init(&zip, 0xffffffffULL)) return 0;
    ok = mz_zip_writer_add_mem(&zip, "large-offset.txt", payload, sizeof(payload) - 1, MZ_NO_COMPRESSION) != 0;
    ok = mz_zip_writer_finalize_archive(&zip) && ok;
    ok = mz_zip_writer_end(&zip) && ok;
    if (!ok) { free(capture.data); return 0; }
    file = fopen("/tmp/miniz-zip-exact-auto.zip", "wb");
    if (!file) { free(capture.data); return 0; }
    ok = fwrite(capture.data, 1, capture.size, file) == capture.size;
    ok = fclose(file) == 0 && ok;
    free(capture.data);
    return ok;
}

static int automatic_zip64_count_archive(void) {
    mz_zip_archive zip;
    mz_uint i;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) return 0;
    for (i = 0; i <= 65535; ++i) {
        if (!mz_zip_writer_add_mem(&zip, "e", NULL, 0, MZ_NO_COMPRESSION)) return 0;
    }
    return save_archive(&zip, "/tmp/miniz-zip-exact-count.zip");
}

static int automatic_zip64_size_hint_archive(void) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) return 0;
    if (!mz_zip_writer_add_read_buf_callback(&zip, "large-hint.txt", source_read,
            (void *)repeated, 0x100000000ULL, NULL, NULL, 0, 6,
            NULL, 0, NULL, 0)) return 0;
    return save_archive(&zip, "/tmp/miniz-zip-exact-size.zip");
}

static int aligned_archive(void) {
    static const unsigned char payload[] = "aligned payload";
    struct capture capture = { 0 };
    mz_zip_archive zip;
    FILE *file;
    int ok;
    memset(&zip, 0, sizeof(zip));
    zip.m_pWrite = capture_write;
    zip.m_pIO_opaque = &capture;
    zip.m_file_offset_alignment = 8;
    if (!mz_zip_writer_init(&zip, 3)) return 0;
    ok = mz_zip_writer_add_mem(&zip, "aligned.txt", payload, sizeof(payload) - 1, MZ_NO_COMPRESSION) != 0;
    ok = mz_zip_writer_finalize_archive(&zip) && ok;
    ok = mz_zip_writer_end(&zip) && ok;
    if (!ok) { free(capture.data); return 0; }
    file = fopen("/tmp/miniz-zip-exact-align.zip", "wb");
    if (!file) { free(capture.data); return 0; }
    ok = fwrite(capture.data, 1, capture.size, file) == capture.size;
    ok = fclose(file) == 0 && ok;
    free(capture.data);
    return ok;
}

static int duplicate_lookup(void) {
    mz_zip_archive zip;
    void *archive = NULL;
    size_t archive_size = 0;
    mz_uint32 values[2];
    FILE *file;
    int i, ok = 1;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) return 0;
    for (i = 0; i < 3; ++i) {
        if (!mz_zip_writer_add_mem(&zip, "duplicate", NULL, 0, MZ_NO_COMPRESSION)) return 0;
    }
    ok = mz_zip_writer_finalize_heap_archive(&zip, &archive, &archive_size) && ok;
    ok = mz_zip_writer_end(&zip) && ok;
    if (!ok) { mz_free(archive); return 0; }
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_mem(&zip, archive, archive_size, 0)) { mz_free(archive); return 0; }
    values[0] = (mz_uint32)mz_zip_reader_locate_file(&zip, "duplicate", NULL, 0);
    mz_zip_reader_end(&zip);
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_mem(&zip, archive, archive_size, MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY)) { mz_free(archive); return 0; }
    values[1] = (mz_uint32)mz_zip_reader_locate_file(&zip, "duplicate", NULL, 0);
    mz_zip_reader_end(&zip);
    mz_free(archive);
    file = fopen("/tmp/miniz-zip-exact-locate.bin", "wb");
    if (!file) return 0;
    ok = fwrite(values, sizeof(values), 1, file) == 1;
    ok = fclose(file) == 0 && ok;
    return ok;
}

static int update_from_reader_archive(void) {
    static const unsigned char added[] = "new payload";
    unsigned char check[sizeof(repeated) - 1];
    mz_zip_archive zip;
    void *archive = NULL, *updated = NULL;
    size_t archive_size = 0, updated_size = 0;
    FILE *file;
    int ok;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) return 0;
    if (!mz_zip_writer_add_mem(&zip, "base.txt", repeated, sizeof(repeated) - 1, 6)) return 0;
    ok = mz_zip_writer_finalize_heap_archive(&zip, &archive, &archive_size) != 0;
    ok = mz_zip_writer_end(&zip) && ok;
    if (!ok) { mz_free(archive); return 0; }

    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_mem(&zip, archive, archive_size, 0)) { mz_free(archive); return 0; }
    if (!mz_zip_writer_init_from_reader(&zip, NULL)) { mz_zip_reader_end(&zip); mz_free(archive); return 0; }
    if (!mz_zip_reader_extract_to_mem(&zip, 0, check, sizeof(check), 0) ||
        memcmp(check, repeated, sizeof(check)) != 0) { mz_zip_writer_end(&zip); return 0; }
    if (!mz_zip_writer_add_mem(&zip, "added.txt", added, sizeof(added) - 1, 6)) {
        mz_zip_writer_end(&zip); return 0;
    }
    ok = mz_zip_writer_finalize_heap_archive(&zip, &updated, &updated_size) != 0;
    ok = mz_zip_writer_end(&zip) && ok;
    if (!ok) { mz_free(updated); return 0; }
    file = fopen("/tmp/miniz-zip-exact-update.zip", "wb");
    if (!file) { mz_free(updated); return 0; }
    ok = fwrite(updated, 1, updated_size, file) == updated_size;
    ok = fclose(file) == 0 && ok;
    mz_free(updated);
    return ok;
}

static int automatic_zip64_precompressed_archive(void) {
    static const unsigned char compressed[] = { 1, 2, 3 };
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) return 0;
    if (!mz_zip_writer_add_mem_ex(&zip, "huge.bin", compressed, sizeof(compressed),
            NULL, 0, 6 | MZ_ZIP_FLAG_COMPRESSED_DATA,
            0x100000001ULL, 0x12345678U)) return 0;
    return save_archive(&zip, "/tmp/miniz-zip-exact-precompressed64.zip");
}

static int exact_max_precompressed_archive(void) {
    static const unsigned char compressed[] = { 1, 2, 3 };
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) return 0;
    if (!mz_zip_writer_add_mem_ex(&zip, "max.bin", compressed, sizeof(compressed),
            NULL, 0, 6 | MZ_ZIP_FLAG_COMPRESSED_DATA,
            0xffffffffULL, 0x12345678U)) return 0;
    return save_archive(&zip, "/tmp/miniz-zip-exact-precompressed32max.zip");
}

int main(void) {
    return descriptor_archive() && patched_archive() && raw_archive() &&
           zip64_archive() && automatic_zip64_archive() &&
           automatic_zip64_count_archive() && automatic_zip64_size_hint_archive() &&
           aligned_archive() && duplicate_lookup() &&
           update_from_reader_archive() &&
           automatic_zip64_precompressed_archive() &&
           exact_max_precompressed_archive() ? 0 : 1;
}
