#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "miniz.h"

static int put_u32(uint32_t value) {
    unsigned char bytes[4] = {
        (unsigned char)value, (unsigned char)(value >> 8),
        (unsigned char)(value >> 16), (unsigned char)(value >> 24),
    };
    return fwrite(bytes, 1, sizeof(bytes), stdout) == sizeof(bytes);
}

static int emit_file_archive(const char *path) {
    mz_zip_archive zip;
    mz_zip_archive reader;
    void *archive = NULL;
    size_t archive_size = 0;
    struct stat extracted_stat;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) { fprintf(stderr, "file init %d\n", mz_zip_get_last_error(&zip)); return 0; }
    if (!mz_zip_writer_add_file(&zip, "file.txt", path, NULL, 0, 6)) { fprintf(stderr, "file add %d\n", mz_zip_get_last_error(&zip)); mz_zip_writer_end(&zip); return 0; }
    if (!mz_zip_writer_finalize_heap_archive(&zip, &archive, &archive_size)) { fprintf(stderr, "file finalize %d\n", mz_zip_get_last_error(&zip)); mz_zip_writer_end(&zip); return 0; }
    if (!mz_zip_writer_end(&zip)) { fprintf(stderr, "file end %d\n", mz_zip_get_last_error(&zip)); mz_free(archive); return 0; }
    if (archive_size > UINT32_MAX || !put_u32((uint32_t)archive_size) ||
        fwrite(archive, 1, archive_size, stdout) != archive_size) {
        mz_free(archive); return 0;
    }
    memset(&reader, 0, sizeof(reader));
    if (!mz_zip_reader_init_mem(&reader, archive, archive_size, 0) ||
        !mz_zip_reader_extract_to_file(&reader, 0, "/tmp/miniz-file-adapter-extracted.bin", 0) ||
        !mz_zip_reader_end(&reader) ||
        stat("/tmp/miniz-file-adapter-extracted.bin", &extracted_stat) != 0 ||
        !put_u32((uint32_t)extracted_stat.st_mtime)) {
        mz_free(archive); return 0;
    }
    remove("/tmp/miniz-file-adapter-extracted.bin");
    mz_free(archive);
    return 1;
}

static int emit_cfile_archive(const char *path) {
    mz_zip_archive zip;
    FILE *source;
    void *archive = NULL;
    size_t archive_size = 0;
    int ok;
    memset(&zip, 0, sizeof(zip));
    source = fopen(path, "rb");
    if (!source) return 0;
    if (fseek(source, 5, SEEK_SET) != 0) { fclose(source); return 0; }
    ok = mz_zip_writer_init_heap(&zip, 0, 0);
    if (!ok) fprintf(stderr, "cfile init %d\n", mz_zip_get_last_error(&zip));
    if (ok) {
        ok = mz_zip_writer_add_cfile(&zip, "whole-from-zero.bin", source, 36, NULL, NULL, 0,
                                     MZ_NO_COMPRESSION | MZ_ZIP_FLAG_ASCII_FILENAME,
                                     NULL, 0, NULL, 0);
        if (!ok) fprintf(stderr, "cfile add %d\n", mz_zip_get_last_error(&zip));
    }
    if (ok) { ok = mz_zip_writer_finalize_heap_archive(&zip, &archive, &archive_size); if (!ok) fprintf(stderr, "cfile finalize %d\n", mz_zip_get_last_error(&zip)); }
    if (ok) { ok = mz_zip_writer_end(&zip); if (!ok) fprintf(stderr, "cfile end %d\n", mz_zip_get_last_error(&zip)); }
    else if (zip.m_pState) mz_zip_writer_end(&zip);
    fclose(source);
    if (!ok) return 0;
    if (archive_size > UINT32_MAX || !put_u32((uint32_t)archive_size) ||
        fwrite(archive, 1, archive_size, stdout) != archive_size) {
        mz_free(archive); return 0;
    }
    mz_free(archive);
    return 1;
}

static int emit_zero_cfile_archive(const char *path) {
    mz_zip_archive zip;
    FILE *source;
    void *archive = NULL;
    size_t archive_size = 0;
    int ok;
    memset(&zip, 0, sizeof(zip));
    source = fopen(path, "rb");
    if (!source) return 0;
    if (fseek(source, 5, SEEK_SET) != 0) { fclose(source); return 0; }
    ok = mz_zip_writer_init_heap(&zip, 0, 0) &&
         mz_zip_writer_add_cfile(&zip, "zero-max.bin", source, 0, NULL, NULL, 0,
                                 MZ_NO_COMPRESSION | MZ_ZIP_FLAG_ASCII_FILENAME,
                                 NULL, 0, NULL, 0) &&
         mz_zip_writer_finalize_heap_archive(&zip, &archive, &archive_size) &&
         mz_zip_writer_end(&zip);
    fclose(source);
    if (!ok) return 0;
    if (archive_size > UINT32_MAX || !put_u32((uint32_t)archive_size) ||
        fwrite(archive, 1, archive_size, stdout) != archive_size) {
        mz_free(archive); return 0;
    }
    mz_free(archive);
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    return emit_file_archive(argv[1]) && emit_cfile_archive(argv[1]) &&
           emit_zero_cfile_archive(argv[1]) ? 0 : 1;
}
