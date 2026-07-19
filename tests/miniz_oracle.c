#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "miniz.h"

static unsigned char *read_all(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    long n;
    unsigned char *data;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET)) { fclose(f); return NULL; }
    data = (unsigned char *)malloc((size_t)n);
    if (n && (!data || fread(data, 1, (size_t)n, f) != (size_t)n)) { free(data); fclose(f); return NULL; }
    fclose(f);
    *size = (size_t)n;
    return data;
}

static int write_all(const char *path, const void *data, size_t size) {
    FILE *f = fopen(path, "wb");
    int ok;
    if (!f) return 0;
    ok = fwrite(data, 1, size, f) == size;
    ok = fclose(f) == 0 && ok;
    return ok;
}

static int verify_sx_zlib(void) {
    size_t packed_size = 0;
    unsigned char *packed = read_all("/tmp/sx-miniz-interop.zlib", &packed_size);
    unsigned char plain[5000];
    mz_ulong plain_size = sizeof(plain);
    uint32_t state = 0x31415926u;
    size_t i;
    if (!packed || mz_uncompress(plain, &plain_size, packed, (mz_ulong)packed_size) != MZ_OK || plain_size != sizeof(plain)) { free(packed); return 0; }
    free(packed);
    for (i = 0; i < sizeof(plain); ++i) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        if (plain[i] != (unsigned char)('A' + state % 10)) return 0;
    }
    return 1;
}

static int verify_sx_zip(void) {
    mz_zip_archive zip;
    size_t size = 0;
    char *data;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_file(&zip, "/tmp/sx-miniz-interop.zip", 0)) return 0;
    data = (char *)mz_zip_reader_extract_file_to_heap(&zip, "alpha.txt", &size, 0);
    if (!data || size != 5 || memcmp(data, "alpha", 5)) { mz_free(data); mz_zip_reader_end(&zip); return 0; }
    mz_free(data);
    data = (char *)mz_zip_reader_extract_file_to_heap(&zip, "nested/repeat.txt", &size, 0);
    if (!data || size != 34 || memcmp(data, "repeat repeat repeat repeat repeat", 34)) { mz_free(data); mz_zip_reader_end(&zip); return 0; }
    mz_free(data);
    return mz_zip_reader_end(&zip) != 0;
}

struct corpus_case { const char *name; size_t size; int repetitive; };

static int verify_sx_corpus(void) {
    static const struct corpus_case cases[] = {
        {"random-l0-n0.bin", 0, 0}, {"random-l1-n1.bin", 1, 0},
        {"random-l6-n258.bin", 258, 0}, {"random-l9-n32768.bin", 32768, 0},
        {"random-l9-n65536.bin", 65536, 0}, {"repeat-l0-n0.bin", 0, 1},
        {"repeat-l1-n1.bin", 1, 1}, {"repeat-l6-n258.bin", 258, 1},
        {"repeat-l9-n32768.bin", 32768, 1}, {"repeat-l9-n65536.bin", 65536, 1},
    };
    mz_zip_archive zip;
    size_t c;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_file(&zip, "/tmp/sx-miniz-corpus.zip", 0)) return 0;
    for (c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c) {
        size_t size = 0, i;
        unsigned char *data = (unsigned char *)mz_zip_reader_extract_file_to_heap(&zip, cases[c].name, &size, 0);
        uint32_t state = 0x12345678u;
        if ((!data && size) || size != cases[c].size) { mz_free(data); mz_zip_reader_end(&zip); return 0; }
        for (i = 0; i < size; ++i) {
            unsigned char expected;
            if (cases[c].repetitive) expected = (unsigned char)('a' + ((i / 97 + i) % 11));
            else {
                state ^= state << 13;
                state ^= state >> 17;
                state ^= state << 5;
                expected = (unsigned char)(state & 0xff);
            }
            if (data[i] != expected) { mz_free(data); mz_zip_reader_end(&zip); return 0; }
        }
        mz_free(data);
    }
    return mz_zip_reader_end(&zip) != 0;
}

static int verify_sx_stream(void) {
    size_t packed_size = 0, plain_size = 0, i;
    unsigned char *packed = read_all("/tmp/sx-miniz-stream.deflate", &packed_size);
    unsigned char *plain;
    if (!packed) return 0;
    plain = (unsigned char *)tinfl_decompress_mem_to_heap(packed, packed_size, &plain_size, 0);
    free(packed);
    if (!plain || plain_size != 70000) { mz_free(plain); return 0; }
    for (i = 0; i < plain_size; ++i) {
        if (plain[i] != (unsigned char)('A' + i % 7)) { mz_free(plain); return 0; }
    }
    mz_free(plain);
    return 1;
}

static uint32_t read_u32(const unsigned char *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static int verify_all_symbol_streams(void) {
    size_t bundle_size = 0, at = 0, cases = 0;
    unsigned char *bundle = read_all("/tmp/sx-miniz-all-symbols.bin", &bundle_size);
    if (!bundle) return 0;
    while (at < bundle_size) {
        uint32_t expected, packed_size;
        size_t plain_size = 0, i;
        unsigned char *plain;
        if (bundle_size - at < 8) { free(bundle); return 0; }
        expected = read_u32(bundle + at); packed_size = read_u32(bundle + at + 4); at += 8;
        if (packed_size > bundle_size - at) { free(bundle); return 0; }
        plain = (unsigned char *)tinfl_decompress_mem_to_heap(bundle + at, packed_size, &plain_size, 0);
        if (!plain || plain_size != expected) { mz_free(plain); free(bundle); return 0; }
        for (i = 0; i < plain_size; ++i) {
            if (plain[i] != 'A') { mz_free(plain); free(bundle); return 0; }
        }
        mz_free(plain); at += packed_size; cases++;
    }
    free(bundle);
    return cases == 210;
}

static int emit_miniz_zlib(void) {
    static const char message[] = "from upstream miniz zlib";
    mz_ulong capacity = mz_compressBound((mz_ulong)(sizeof(message) - 1));
    mz_ulong size = capacity;
    unsigned char *packed = (unsigned char *)malloc(capacity);
    int ok;
    if (!packed) return 0;
    ok = mz_compress2(packed, &size, (const unsigned char *)message, (mz_ulong)(sizeof(message) - 1), MZ_BEST_COMPRESSION) == MZ_OK;
    if (ok) ok = write_all("/tmp/miniz-miniz-interop.zlib", packed, (size_t)size);
    free(packed);
    return ok;
}

static int emit_miniz_zip(void) {
    static const char stored[] = "from upstream miniz";
    static const char repeated[] = "miniz miniz miniz miniz miniz miniz";
    mz_zip_archive zip;
    int ok = 1;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_file(&zip, "/tmp/miniz-miniz-interop.zip", 0)) return 0;
    ok = ok && mz_zip_writer_add_mem(&zip, "stored.txt", stored, sizeof(stored) - 1, MZ_NO_COMPRESSION);
    ok = ok && mz_zip_writer_add_mem(&zip, "nested/repeated.txt", repeated, sizeof(repeated) - 1, MZ_BEST_COMPRESSION);
    ok = ok && mz_zip_writer_finalize_archive(&zip);
    ok = mz_zip_writer_end(&zip) && ok;
    return ok;
}

static int emit_miniz_zip64(void) {
    static const char payload[] = "from upstream miniz zip64";
    mz_zip_archive zip;
    void *archive = NULL;
    size_t archive_size = 0;
    int ok = 1;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap_v2(&zip, 0, 0, MZ_ZIP_FLAG_WRITE_ZIP64)) return 0;
    ok = ok && mz_zip_writer_add_mem(&zip, "zip64.txt", payload, sizeof(payload) - 1, MZ_BEST_COMPRESSION);
    ok = ok && mz_zip_writer_finalize_heap_archive(&zip, &archive, &archive_size);
    ok = mz_zip_writer_end(&zip) && ok;
    if (ok) ok = write_all("/tmp/miniz-miniz-interop-zip64.zip", archive, archive_size);
    mz_free(archive);
    return ok;
}

int main(void) {
    return verify_sx_zlib() && verify_sx_zip() && verify_sx_corpus() && verify_sx_stream() &&
           verify_all_symbol_streams() &&
           emit_miniz_zlib() && emit_miniz_zip() && emit_miniz_zip64() ? 0 : 1;
}
