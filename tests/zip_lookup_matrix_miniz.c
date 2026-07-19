#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"

static FILE *out;
static void u32(uint32_t v) {
    unsigned char b[4] = {(unsigned char)v, (unsigned char)(v >> 8),
                          (unsigned char)(v >> 16), (unsigned char)(v >> 24)};
    fwrite(b, 1, 4, out);
}
static void bytes(const void *p, size_t n) { u32((uint32_t)n); fwrite(p, 1, n, out); }
static void result(int ok, mz_zip_archive *zip) {
    u32((uint32_t)ok); u32((uint32_t)mz_zip_get_last_error(zip));
}

static int add(mz_zip_archive *zip, const char *name, const char *comment) {
    static const unsigned char payload[] = "lookup-stat-payload";
    return mz_zip_writer_add_mem_ex_v2(zip, name,
        name[strlen(name) - 1] == '/' ? NULL : payload,
        name[strlen(name) - 1] == '/' ? 0 : sizeof(payload) - 1,
        comment, (mz_uint16)strlen(comment), MZ_NO_COMPRESSION,
        0, 0, NULL, NULL, 0, NULL, 0);
}

static int build(void **data, size_t *size) {
    static const char *names[] = {
        "Alpha.TXT", "alpha.txt", "dir/Leaf.bin", "win\\Back.dat",
        "C:Colon", "dup", "dup", "dup", "folder/", "plain"
    };
    static const char *comments[] = {
        "A", "", "leaf-note", "back-note", "colon-note",
        "", "note", "", "dir-note", "plain-note"
    };
    mz_zip_archive zip;
    unsigned i;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) return 0;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        if (!add(&zip, names[i], comments[i])) return 0;
    if (!mz_zip_writer_finalize_heap_archive(&zip, data, size)) return 0;
    return mz_zip_writer_end(&zip);
}

static void enumerate(mz_zip_archive *zip) {
    mz_uint i;
    for (i = 0; i < mz_zip_reader_get_num_files(zip); ++i) {
        mz_zip_archive_file_stat st;
        char small[4], full[520];
        size_t n;
        mz_zip_clear_last_error(zip); result(mz_zip_reader_is_file_a_directory(zip, i), zip);
        mz_zip_clear_last_error(zip); result(mz_zip_reader_is_file_encrypted(zip, i), zip);
        mz_zip_clear_last_error(zip); result(mz_zip_reader_is_file_supported(zip, i), zip);
        mz_zip_clear_last_error(zip); u32((uint32_t)mz_zip_reader_get_filename(zip, i, NULL, 0));
        u32((uint32_t)mz_zip_get_last_error(zip));
        memset(small, 0xcc, sizeof(small));
        mz_zip_clear_last_error(zip); n = mz_zip_reader_get_filename(zip, i, small, sizeof(small));
        u32((uint32_t)n); u32((uint32_t)mz_zip_get_last_error(zip)); bytes(small, sizeof(small));
        memset(full, 0xcc, sizeof(full));
        mz_zip_clear_last_error(zip); n = mz_zip_reader_get_filename(zip, i, full, sizeof(full));
        u32((uint32_t)n); u32((uint32_t)mz_zip_get_last_error(zip)); bytes(full, n);
        mz_zip_clear_last_error(zip);
        if (!mz_zip_reader_file_stat(zip, i, &st)) { result(0, zip); continue; }
        result(1, zip);
        u32(st.m_file_index); u32((uint32_t)st.m_central_dir_ofs);
        u32(st.m_version_made_by); u32(st.m_version_needed); u32(st.m_bit_flag); u32(st.m_method);
        u32(st.m_crc32); u32((uint32_t)st.m_comp_size); u32((uint32_t)st.m_uncomp_size);
        u32(st.m_internal_attr); u32(st.m_external_attr); u32((uint32_t)st.m_local_header_ofs);
        u32(st.m_comment_size); u32(st.m_is_directory); u32(st.m_is_encrypted); u32(st.m_is_supported);
        bytes(st.m_filename, strlen(st.m_filename)); bytes(st.m_comment, st.m_comment_size);
    }
}

static void locate(mz_zip_archive *zip, const char *name, const char *comment, mz_uint flags) {
    mz_uint32 index = 0xdeadbeefU;
    int ok;
    mz_zip_clear_last_error(zip);
    ok = mz_zip_reader_locate_file_v2(zip, name, comment, flags, &index);
    u32((uint32_t)ok); u32(index); u32((uint32_t)mz_zip_get_last_error(zip));
}

static void queries(mz_zip_archive *zip) {
    locate(zip, "ALPHA.TXT", NULL, 0);
    locate(zip, "Alpha.TXT", NULL, MZ_ZIP_FLAG_CASE_SENSITIVE);
    locate(zip, "alpha.txt", NULL, MZ_ZIP_FLAG_CASE_SENSITIVE);
    locate(zip, "Leaf.bin", NULL, MZ_ZIP_FLAG_IGNORE_PATH);
    locate(zip, "Back.dat", NULL, MZ_ZIP_FLAG_IGNORE_PATH);
    locate(zip, "Colon", NULL, MZ_ZIP_FLAG_IGNORE_PATH);
    locate(zip, "dup", NULL, 0);
    locate(zip, "dup", "", 0);
    locate(zip, "dup", "note", 0);
    locate(zip, "DUP", "NOTE", 0);
    locate(zip, "dup", "missing", 0);
    locate(zip, "missing", NULL, 0);
}

int main(void) {
    void *archive = NULL;
    size_t size = 0;
    mz_zip_archive zip;
    FILE *fixture;
    if (!build(&archive, &size)) return 1;
    fixture = fopen("/tmp/miniz-zip-lookup-matrix.zip", "wb");
    out = fopen("/tmp/miniz-zip-lookup-matrix.bin", "wb");
    if (!fixture || !out || fwrite(archive, 1, size, fixture) != size || fclose(fixture) != 0) return 2;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_mem(&zip, archive, size, 0)) return 3;
    enumerate(&zip); queries(&zip); mz_zip_reader_end(&zip);
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_mem(&zip, archive, size, MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY)) return 4;
    queries(&zip); mz_zip_reader_end(&zip);
    mz_free(archive);
    return fclose(out) != 0;
}
