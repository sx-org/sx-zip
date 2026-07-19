#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"

#define VARIANT_COUNT 51
#define ZIP64_VARIANT_COUNT 24

static uint16_t rd16(const unsigned char *p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }
static uint32_t rd32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void wr16(unsigned char *p, uint16_t v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); }
static void wr32m(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}
static void wr64(unsigned char *p, uint64_t v) { wr32m(p, (uint32_t)v); wr32m(p + 4, (uint32_t)(v >> 32)); }
static void put32(FILE *f, uint32_t v) { unsigned char b[4]; wr32m(b, v); fwrite(b, 1, 4, f); }
static void record(FILE *f, int ok, mz_zip_archive *zip) {
    put32(f, (uint32_t)ok); put32(f, (uint32_t)mz_zip_get_last_error(zip));
}
static void dummy(FILE *f) { put32(f, 0); put32(f, 0); }

static int build(void **data, size_t *size) {
    static const char payload[] = "reader corruption matrix payload";
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    return mz_zip_writer_init_heap(&zip, 0, 0) &&
           mz_zip_writer_add_mem(&zip, "entry", payload, sizeof(payload) - 1, 0) &&
           mz_zip_writer_finalize_heap_archive(&zip, data, size) &&
           mz_zip_writer_end(&zip);
}

static int build_zip64(void **data, size_t *size) {
    static const char payload[] = "zip64 corruption payload";
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    return mz_zip_writer_init_heap_v2(&zip, 0, 0, MZ_ZIP_FLAG_WRITE_ZIP64) &&
           mz_zip_writer_add_mem(&zip, "entry", payload, sizeof(payload) - 1, 0) &&
           mz_zip_writer_finalize_heap_archive(&zip, data, size) &&
           mz_zip_writer_end(&zip);
}

static unsigned char *variant(const unsigned char *base, size_t base_size,
                              unsigned which, size_t *out_size) {
    size_t size = base_size, eocd = base_size - 22;
    uint32_t central = rd32(base + eocd + 16), central_size = rd32(base + eocd + 12);
    unsigned char *p;
    if (which == 1) {
        p = (unsigned char *)calloc(1, 10); *out_size = 10; return p;
    }
    if (which == 39 || which == 41) size += 5;
    if (which == 40) size += 5;
    p = (unsigned char *)calloc(1, size);
    if (!p) return NULL;
    if (which == 40) {
        memcpy(p + 5, base, base_size);
        *out_size = size;
        return p;
    }
    memcpy(p, base, base_size);
    if (which == 41) { wr16(p + eocd + 20, 5); memcpy(p + base_size, "hello", 5); }
    switch (which) {
        case 0: case 39: case 41: break;
        case 2: p[eocd] ^= 1; break;
        case 3: wr16(p + eocd + 20, 0xffff); break;
        case 4: wr16(p + eocd + 4, 1); break;
        case 5: wr16(p + eocd + 6, 1); break;
        case 6: wr16(p + eocd + 8, 0); break;
        case 7: wr16(p + eocd + 8, 0xffff); wr16(p + eocd + 10, 0xffff); break;
        case 8: wr32m(p + eocd + 12, 0xffffffffU); break;
        case 9: wr32m(p + eocd + 16, 0xffffffffU); break;
        case 10: wr32m(p + eocd + 12, 45); break;
        case 11: wr32m(p + eocd + 16, (uint32_t)base_size + 1); break;
        case 12: wr32m(p + eocd + 12, (uint32_t)base_size); break;
        case 13: wr32m(p + eocd + 12, central_size + 1); break;
        case 14: p[central] ^= 1; break;
        case 15: wr16(p + central + 28, 0xffff); break;
        case 16: wr16(p + central + 30, 0xffff); break;
        case 17: wr16(p + central + 32, 0xffff); break;
        case 18: wr16(p + central + 34, 1); break;
        case 19: wr32m(p + central + 42, (uint32_t)base_size + 1); break;
        case 20: wr32m(p + central + 42, central); break;
        case 21: wr16(p + central + 10, 8); break;
        case 22: wr32m(p + central + 20, rd32(p + central + 20) + 1000); break;
        case 23: wr32m(p + central + 24, rd32(p + central + 24) + 1); break;
        case 24: p[0] ^= 1; break;
        case 25: wr16(p + 6, rd16(p + 6) ^ 1); break;
        case 26: wr16(p + 8, 8); break;
        case 27: wr16(p + 26, 0xffff); break;
        case 28: wr16(p + 28, 0xffff); break;
        case 29: p[14] ^= 1; break;
        case 30: wr32m(p + 18, rd32(p + 18) + 1); break;
        case 31: wr32m(p + 22, rd32(p + 22) + 1); break;
        case 32: p[30] ^= 1; break;
        case 33: wr16(p + central + 8, rd16(p + central + 8) | 1); break;
        case 34: wr16(p + central + 8, rd16(p + central + 8) | 0x20); break;
        case 35: wr16(p + eocd + 8, 0); wr16(p + eocd + 10, 0); break;
        case 36: wr16(p + eocd + 8, 2); wr16(p + eocd + 10, 2); break;
        case 37: wr32m(p + eocd + 16, central - 1); break;
        case 38: wr32m(p + eocd + 12, central_size + 20); break;
        case 42: wr32m(p + 18, 0); break;
        case 43: wr32m(p + 22, 0); break;
        case 44: wr32m(p + central + 16, rd32(p + central + 16) ^ 0x5a); break;
        case 45: wr16(p + central + 8, rd16(p + central + 8) | 0x2000); break;
        case 46: wr16(p + central + 34, 2); break;
        case 47: wr16(p + central + 34, 0xffff); break;
        case 48: wr16(p + eocd + 4, 1); wr16(p + eocd + 6, 1); break;
        case 49: wr16(p + eocd + 4, 2); wr16(p + eocd + 6, 2); break;
        case 50: wr16(p + 6, rd16(p + 6) | 0x2000); break;
        default: break;
    }
    *out_size = size;
    return p;
}

static unsigned char *zip64_variant(const unsigned char *base, size_t size,
                                    unsigned which) {
    size_t eocd = size - 22, locator = eocd - 20, eocd64 = locator - 56;
    uint64_t central = (uint64_t)rd32(base + eocd64 + 48) |
                       ((uint64_t)rd32(base + eocd64 + 52) << 32);
    unsigned char *p = (unsigned char *)malloc(size);
    if (!p) return NULL;
    memcpy(p, base, size);
    switch (which) {
        case 0: break;
        case 1: p[locator] ^= 1; break;
        case 2: wr32m(p + locator + 4, 1); break;
        case 3: wr32m(p + locator + 16, 2); break;
        case 4: wr64(p + locator + 8, eocd64 + 1); break;
        case 5: p[eocd64] ^= 1; break;
        case 6: wr64(p + eocd64 + 4, 43); break;
        case 7: wr32m(p + eocd64 + 16, 1); break;
        case 8: wr32m(p + eocd64 + 20, 1); break;
        case 9: wr64(p + eocd64 + 24, 0); break;
        case 10: wr64(p + eocd64 + 32, 2); break;
        case 11: wr64(p + eocd64 + 24, UINT64_C(0x100000000)); wr64(p + eocd64 + 32, UINT64_C(0x100000000)); break;
        case 12: wr64(p + eocd64 + 40, UINT64_C(0x100000000)); break;
        case 13: wr64(p + eocd64 + 48, central + 1); break;
        case 14: wr16(p + eocd + 4, 1); break;
        case 15: wr16(p + eocd + 6, 1); break;
        case 16: wr32m(p + locator + 16, 0); break;
        case 17: wr64(p + eocd64 + 4, UINT64_C(0x100000000)); break;
        case 18: wr32m(p + central + 42, 0xffffffffU); break;
        case 19: wr32m(p + central + 20, 0xffffffffU); break;
        case 20: wr32m(p + central + 24, 0xffffffffU); break;
        case 21: wr16(p + central + 34, 0xffff); break;
        case 22: wr16(p + eocd64 + 14, 99); break;
        case 23: wr64(p + eocd64 + 24, 1); wr64(p + eocd64 + 32, 1); break;
        default: break;
    }
    return p;
}

static void run(FILE *out, const unsigned char *data, size_t size) {
    mz_zip_archive zip;
    mz_zip_archive_file_stat stat;
    unsigned char buf[256];
    int ok;
    memset(&zip, 0, sizeof(zip));
    ok = mz_zip_reader_init_mem(&zip, data, size, 0);
    record(out, ok, &zip);
    if (!ok) { dummy(out); dummy(out); dummy(out); dummy(out); dummy(out); return; }
    mz_zip_clear_last_error(&zip); record(out, mz_zip_reader_file_stat(&zip, 0, &stat), &zip);
    mz_zip_clear_last_error(&zip); record(out, mz_zip_validate_file(&zip, 0, 0), &zip);
    mz_zip_clear_last_error(&zip); record(out, mz_zip_validate_file(&zip, 0, MZ_ZIP_FLAG_VALIDATE_HEADERS_ONLY), &zip);
    mz_zip_clear_last_error(&zip); record(out, mz_zip_reader_extract_to_mem(&zip, 0, buf, sizeof(buf), 0), &zip);
    record(out, mz_zip_reader_end(&zip), &zip);
}

int main(void) {
    void *base_void = NULL, *zip64_void = NULL;
    unsigned char *base, *zip64, *copy;
    size_t base_size = 0, zip64_size = 0, size;
    unsigned i;
    FILE *fixtures, *out;
    if (!build(&base_void, &base_size) || !build_zip64(&zip64_void, &zip64_size)) return 1;
    base = (unsigned char *)base_void;
    zip64 = (unsigned char *)zip64_void;
    fixtures = fopen("/tmp/miniz-zip-corruption-fixtures.bin", "wb");
    out = fopen("/tmp/miniz-zip-corruption.bin", "wb");
    if (!fixtures || !out) return 1;
    put32(fixtures, VARIANT_COUNT + ZIP64_VARIANT_COUNT);
    for (i = 0; i < VARIANT_COUNT; ++i) {
        copy = variant(base, base_size, i, &size);
        if (!copy) return 1;
        put32(fixtures, (uint32_t)size); fwrite(copy, 1, size, fixtures);
        run(out, copy, size);
        free(copy);
    }
    for (i = 0; i < ZIP64_VARIANT_COUNT; ++i) {
        copy = zip64_variant(zip64, zip64_size, i);
        if (!copy) return 1;
        put32(fixtures, (uint32_t)zip64_size); fwrite(copy, 1, zip64_size, fixtures);
        run(out, copy, zip64_size);
        free(copy);
    }
    mz_free(base);
    mz_free(zip64);
    return fclose(fixtures) != 0 || fclose(out) != 0;
}
