#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"

#define BASE_PATH "/tmp/miniz-zip-alias-base.zip"
#define PREFIX_PATH "/tmp/miniz-zip-alias-prefix.bin"
#define EXTRACT_PATH "/tmp/miniz-zip-alias-extract.bin"
#define RAW_PATH "/tmp/miniz-zip-alias-raw.bin"
#define INPLACE_PATH "/tmp/miniz-zip-alias-inplace.zip"
#define FAILED_NEW_PATH "/tmp/miniz-zip-alias-failed-new.zip"

struct memory_sink { unsigned char data[4096]; size_t size; };

static void u32(uint32_t value) {
    unsigned char b[4] = {(unsigned char)value, (unsigned char)(value >> 8),
                          (unsigned char)(value >> 16), (unsigned char)(value >> 24)};
    fwrite(b, 1, sizeof(b), stdout);
}

static void bytes(const void *data, size_t size) {
    u32((uint32_t)size); fwrite(data, 1, size, stdout);
}

static size_t write_sink(void *opaque, mz_uint64 offset, const void *data, size_t size) {
    struct memory_sink *sink = (struct memory_sink *)opaque;
    if (offset > sizeof(sink->data) || size > sizeof(sink->data) - (size_t)offset) return 0;
    memcpy(sink->data + (size_t)offset, data, size);
    if ((size_t)offset + size > sink->size) sink->size = (size_t)offset + size;
    return size;
}

static int write_file(const char *path, const void *data, size_t size, const void *prefix, size_t prefix_size) {
    FILE *file = fopen(path, "wb");
    if (!file) return 0;
    if ((prefix_size && fwrite(prefix, 1, prefix_size, file) != prefix_size) ||
        fwrite(data, 1, size, file) != size) return 0;
    return fclose(file) == 0;
}

static int file_blob(const char *path) {
    unsigned char buffer[8192];
    FILE *file = fopen(path, "rb");
    long size;
    if (!file || fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
        size > (long)sizeof(buffer) || fseek(file, 0, SEEK_SET) != 0) return 0;
    if (fread(buffer, 1, (size_t)size, file) != (size_t)size || fclose(file) != 0) return 0;
    bytes(buffer, (size_t)size);
    return 1;
}

static int file_exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    fclose(file); return 1;
}

static int build(void **archive, size_t *archive_size) {
    static const char a[] = "alias-surface-store";
    static const char b[] = "alias-surface-deflate-alias-surface-deflate-alias-surface-deflate";
    mz_zip_archive zip;
    mz_zip_zero_struct(&zip);
    return mz_zip_writer_init_heap(&zip, 0, 0) &&
           mz_zip_writer_add_mem_ex_v2(&zip, "Dir/A.txt", a, sizeof(a) - 1, "note", 4,
                                       MZ_NO_COMPRESSION | MZ_ZIP_FLAG_ASCII_FILENAME,
                                       0, 0, NULL, NULL, 0, NULL, 0) &&
           mz_zip_writer_add_mem_ex_v2(&zip, "b.bin", b, sizeof(b) - 1, NULL, 0,
                                       6 | MZ_ZIP_FLAG_ASCII_FILENAME,
                                       0, 0, NULL, NULL, 0, NULL, 0) &&
           mz_zip_writer_finalize_heap_archive(&zip, archive, archive_size) &&
           mz_zip_writer_end(&zip);
}

int main(void) {
    static const char prefix[] = "PREFIX!";
    unsigned char fixed[256], scratch[65536];
    void *archive = NULL, *heap;
    size_t archive_size = 0, heap_size = 0;
    mz_zip_archive reader, writer, source;
    mz_zip_error err;
    mz_uint32 index = 99;
    FILE *file, *reader_file;
    struct memory_sink sink;

    if (!build(&archive, &archive_size) ||
        !write_file(BASE_PATH, archive, archive_size, NULL, 0) ||
        !write_file(PREFIX_PATH, archive, archive_size, prefix, sizeof(prefix) - 1)) return 1;

    reader_file = fopen(PREFIX_PATH, "rb");
    if (!reader_file || fseek(reader_file, sizeof(prefix) - 1, SEEK_SET) != 0) return 1;
    mz_zip_zero_struct(&reader);
    u32((uint32_t)mz_zip_reader_init_cfile(&reader, reader_file, archive_size, 0));
    u32((uint32_t)mz_zip_get_type(&reader)); u32((uint32_t)mz_zip_get_archive_file_start_offset(&reader));
    u32((uint32_t)mz_zip_reader_locate_file_v2(&reader, "a.TXT", "note", MZ_ZIP_FLAG_IGNORE_PATH, &index)); u32(index);
    index = 99;
    u32((uint32_t)mz_zip_reader_locate_file_v2(&reader, "A.txt", "bad", MZ_ZIP_FLAG_IGNORE_PATH, &index)); u32(index);
    memset(fixed, 0, sizeof(fixed));
    u32((uint32_t)mz_zip_reader_extract_to_mem_no_alloc(&reader, 0, fixed, 19, 0, scratch, sizeof(scratch)));
    u32((uint32_t)mz_crc32(MZ_CRC32_INIT, fixed, 19));
    memset(fixed, 0, sizeof(fixed));
    u32((uint32_t)mz_zip_reader_extract_file_to_mem(&reader, "b.bin", fixed, 65, 0));
    u32((uint32_t)mz_crc32(MZ_CRC32_INIT, fixed, 65));

    file = fopen("/tmp/miniz-zip-alias-cfile.bin", "w+b");
    if (!file || fwrite("OUT", 1, 3, file) != 3) return 1;
    u32((uint32_t)mz_zip_reader_extract_to_cfile(&reader, 0, file, 0));
    u32((uint32_t)mz_zip_reader_extract_file_to_cfile(&reader, "b.bin", file, 0));
    if (fclose(file) != 0 || !file_blob("/tmp/miniz-zip-alias-cfile.bin")) return 1;
    file = fopen(RAW_PATH, "w+b");
    if (!file) return 1;
    u32((uint32_t)mz_zip_reader_extract_file_to_cfile(&reader, "b.bin", file, MZ_ZIP_FLAG_COMPRESSED_DATA));
    if (fclose(file) != 0 || !file_blob(RAW_PATH)) return 1;
    u32((uint32_t)mz_zip_reader_extract_file_to_file(&reader, "Dir/A.txt", EXTRACT_PATH, 0));
    if (!file_blob(EXTRACT_PATH)) return 1;
    u32((uint32_t)mz_zip_reader_extract_file_to_file(&reader, "A.txt", EXTRACT_PATH, MZ_ZIP_FLAG_IGNORE_PATH));
    if (!file_blob(EXTRACT_PATH)) return 1;
    u32((uint32_t)mz_zip_reader_extract_file_to_file(&reader, "a.txt", EXTRACT_PATH,
                                                     MZ_ZIP_FLAG_IGNORE_PATH | MZ_ZIP_FLAG_CASE_SENSITIVE));
    u32((uint32_t)mz_zip_get_last_error(&reader));
    u32((uint32_t)mz_zip_reader_end(&reader));
    if (fclose(reader_file) != 0) return 1;

    err = MZ_ZIP_UNDEFINED_ERROR;
    u32((uint32_t)mz_zip_validate_mem_archive(archive, archive_size, 0, &err)); u32((uint32_t)err);
    err = MZ_ZIP_UNDEFINED_ERROR;
    u32((uint32_t)mz_zip_validate_file_archive(BASE_PATH, 0, &err)); u32((uint32_t)err);

    memset(&sink, 0, sizeof(sink)); mz_zip_zero_struct(&writer);
    writer.m_pWrite = write_sink; writer.m_pIO_opaque = &sink;
    u32((uint32_t)mz_zip_writer_init_v2(&writer, 0, 0));
    u32((uint32_t)mz_zip_writer_add_mem_ex_v2(&writer, "u", "user", 4, NULL, 0,
                                              MZ_NO_COMPRESSION | MZ_ZIP_FLAG_ASCII_FILENAME,
                                              0, 0, NULL, NULL, 0, NULL, 0));
    u32((uint32_t)mz_zip_writer_finalize_archive(&writer));
    u32((uint32_t)mz_zip_writer_end(&writer)); bytes(sink.data, sink.size);

    mz_zip_zero_struct(&source); mz_zip_zero_struct(&writer);
    if (!mz_zip_reader_init_mem(&source, archive, archive_size, 0) ||
        !mz_zip_writer_init_heap(&writer, 0, 0)) return 1;
    u32((uint32_t)mz_zip_writer_add_from_zip_reader(&writer, &source, 1));
    if (!mz_zip_writer_finalize_heap_archive(&writer, &heap, &heap_size) ||
        !mz_zip_writer_end(&writer) || !mz_zip_reader_end(&source)) return 1;
    bytes(heap, heap_size); mz_free(heap);

    if (!write_file(INPLACE_PATH, archive, archive_size, NULL, 0)) return 1;
    u32((uint32_t)mz_zip_add_mem_to_archive_file_in_place(INPLACE_PATH, "one", "111", 3, NULL, 0,
                                                          MZ_NO_COMPRESSION | MZ_ZIP_FLAG_ASCII_FILENAME));
    err = MZ_ZIP_UNDEFINED_ERROR;
    u32((uint32_t)mz_zip_add_mem_to_archive_file_in_place_v2(INPLACE_PATH, "two", "2222", 4, "two-note", 8,
                                                             MZ_NO_COMPRESSION | MZ_ZIP_FLAG_ASCII_FILENAME, &err));
    u32((uint32_t)err); if (!file_blob(INPLACE_PATH)) return 1;
    heap = mz_zip_extract_archive_file_to_heap(INPLACE_PATH, "one", &heap_size, 0);
    u32(heap != NULL); if (heap) { bytes(heap, heap_size); mz_free(heap); } else bytes("", 0);
    err = MZ_ZIP_UNDEFINED_ERROR;
    heap = mz_zip_extract_archive_file_to_heap_v2(INPLACE_PATH, "two", "two-note", &heap_size, 0, &err);
    u32(heap != NULL); u32((uint32_t)err); if (heap) { bytes(heap, heap_size); mz_free(heap); } else bytes("", 0);

    err = MZ_ZIP_UNDEFINED_ERROR;
    u32((uint32_t)mz_zip_validate_mem_archive(NULL, 0, 0, &err)); u32((uint32_t)err);
    err = MZ_ZIP_UNDEFINED_ERROR;
    u32((uint32_t)mz_zip_validate_mem_archive(archive, 10, 0, &err)); u32((uint32_t)err);
    err = MZ_ZIP_UNDEFINED_ERROR;
    u32((uint32_t)mz_zip_validate_file_archive("/tmp/miniz-zip-alias-missing.zip", 0, &err)); u32((uint32_t)err);

    heap_size = 99; err = MZ_ZIP_UNDEFINED_ERROR;
    heap = mz_zip_extract_archive_file_to_heap_v2("/tmp/miniz-zip-alias-missing.zip", "x", NULL, &heap_size, 0, &err);
    u32(heap != NULL); u32((uint32_t)err); u32((uint32_t)heap_size);
    heap_size = 99; err = MZ_ZIP_UNDEFINED_ERROR;
    heap = mz_zip_extract_archive_file_to_heap_v2(INPLACE_PATH, "missing", NULL, &heap_size, 0, &err);
    u32(heap != NULL); u32((uint32_t)err); u32((uint32_t)heap_size);
    heap_size = 99; err = MZ_ZIP_UNDEFINED_ERROR;
    heap = mz_zip_extract_archive_file_to_heap_v2(INPLACE_PATH, "two", "wrong", &heap_size, 0, &err);
    u32(heap != NULL); u32((uint32_t)err); u32((uint32_t)heap_size);

    if (!mz_zip_add_mem_to_archive_file_in_place(INPLACE_PATH, "one", "later", 5, NULL, 0,
                                                  MZ_NO_COMPRESSION | MZ_ZIP_FLAG_ASCII_FILENAME)) return 1;
    heap = mz_zip_extract_archive_file_to_heap(INPLACE_PATH, "one", &heap_size, 0);
    u32(heap != NULL); if (heap) { bytes(heap, heap_size); mz_free(heap); } else bytes("", 0);

    err = MZ_ZIP_UNDEFINED_ERROR;
    u32((uint32_t)mz_zip_add_mem_to_archive_file_in_place_v2(
        "/tmp/miniz-missing-dir/out.zip", "x", "x", 1, NULL, 0,
        MZ_NO_COMPRESSION, &err));
    u32((uint32_t)err); u32((uint32_t)file_exists("/tmp/miniz-missing-dir/out.zip"));
    remove(FAILED_NEW_PATH); err = MZ_ZIP_UNDEFINED_ERROR;
    u32((uint32_t)mz_zip_add_mem_to_archive_file_in_place_v2(
        FAILED_NEW_PATH, "/bad", "x", 1, NULL, 0, MZ_NO_COMPRESSION, &err));
    u32((uint32_t)err); u32((uint32_t)file_exists(FAILED_NEW_PATH));
    err = MZ_ZIP_UNDEFINED_ERROR;
    u32((uint32_t)mz_zip_add_mem_to_archive_file_in_place_v2(
        INPLACE_PATH, "/bad", "x", 1, NULL, 0, MZ_NO_COMPRESSION, &err));
    u32((uint32_t)err);

    mz_free(archive);
    return 0;
}
