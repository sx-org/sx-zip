#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"

static uint16_t r16(const unsigned char *p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }
static uint32_t r32(const unsigned char *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint64_t r64(const unsigned char *p) { return (uint64_t)r32(p) | ((uint64_t)r32(p + 4) << 32); }
static void w32(unsigned char *p, uint32_t v) { p[0]=(unsigned char)v; p[1]=(unsigned char)(v>>8); p[2]=(unsigned char)(v>>16); p[3]=(unsigned char)(v>>24); }
static void w64(unsigned char *p, uint64_t v) { w32(p,(uint32_t)v); w32(p+4,(uint32_t)(v>>32)); }

static int write_file(const char *path, const void *data, size_t size) {
    FILE *f = fopen(path, "wb");
    int ok = f && fwrite(data, 1, size, f) == size && fclose(f) == 0;
    return ok;
}

static const char descriptor_payload[] = "zip64 descriptor payload zip64 descriptor payload";

static size_t source_read(void *opaque, mz_uint64 offset, void *buf, size_t size) {
    size_t length = sizeof(descriptor_payload) - 1;
    (void)opaque;
    if (offset >= length) return 0;
    if (size > length - (size_t)offset) size = length - (size_t)offset;
    memcpy(buf, descriptor_payload + offset, size);
    return size;
}

static int build(void **data, size_t *size) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0) ||
        !mz_zip_writer_add_read_buf_callback(&zip, "wide.bin", source_read, NULL,
            0x100000000ULL, NULL, NULL, 0, MZ_BEST_COMPRESSION,
            NULL, 0, NULL, 0) ||
        !mz_zip_writer_finalize_heap_archive(&zip, data, size)) return 0;
    return mz_zip_writer_end(&zip);
}

static int check(const unsigned char *data, size_t size, FILE *out) {
    mz_zip_archive zip;
    unsigned char decoded[sizeof(descriptor_payload)-1], raw[256];
    mz_zip_archive_file_stat st;
    uint32_t values[8];
    memset(&zip, 0, sizeof(zip));
    values[0] = (uint32_t)mz_zip_reader_init_mem(&zip, data, size, 0);
    values[1] = (uint32_t)mz_zip_get_last_error(&zip);
    values[2] = (uint32_t)mz_zip_is_zip64(&zip);
    values[3] = (uint32_t)mz_zip_reader_file_stat(&zip, 0, &st);
    values[4] = (uint32_t)mz_zip_reader_extract_to_mem(&zip, 0, decoded, sizeof(decoded), 0);
    values[5] = (uint32_t)(values[4] && memcmp(decoded, descriptor_payload, sizeof(decoded)) == 0);
    values[6] = (uint32_t)mz_zip_reader_extract_to_mem(&zip, 0, raw, sizeof(raw), MZ_ZIP_FLAG_COMPRESSED_DATA);
    values[7] = (uint32_t)mz_zip_validate_file(&zip, 0, 0);
    if (!mz_zip_reader_end(&zip)) return 0;
    return fwrite(values, sizeof(values), 1, out) == 1;
}

int main(void) {
    void *signed_void = NULL;
    unsigned char *signed_data, *unsigned_data;
    size_t signed_size = 0, unsigned_size;
    size_t eocd, locator, zip64, central, local, name_len, extra_len, payload_at, descriptor;
    uint64_t comp_size;
    FILE *out;
    if (!build(&signed_void, &signed_size)) { fprintf(stderr, "build failed\n"); return 1; }
    signed_data = (unsigned char *)signed_void;
    eocd = signed_size - 22; locator = eocd - 20; zip64 = (size_t)r64(signed_data + locator + 8);
    central = (size_t)r64(signed_data + zip64 + 48);
    local = (size_t)r32(signed_data + central + 42);
    name_len = r16(signed_data + local + 26); extra_len = r16(signed_data + local + 28);
    payload_at = local + 30 + name_len + extra_len;
    name_len = r16(signed_data + central + 28); extra_len = r16(signed_data + central + 30);
    if (r32(signed_data + central + 20) == 0xffffffff &&
        r16(signed_data + central + 46 + name_len) == 1 && extra_len >= 20)
        comp_size = r64(signed_data + central + 46 + name_len + 12);
    else
        comp_size = r32(signed_data + central + 20);
    descriptor = payload_at + (size_t)comp_size;
    if (r32(signed_data + descriptor) != 0x08074b50) { fprintf(stderr, "descriptor failed at=%zu sig=%x comp=%llu payload=%zu\n", descriptor, r32(signed_data + descriptor), (unsigned long long)comp_size, payload_at); return 1; }

    unsigned_size = signed_size - 4;
    unsigned_data = (unsigned char *)malloc(unsigned_size);
    if (!unsigned_data) return 1;
    memcpy(unsigned_data, signed_data, descriptor);
    memcpy(unsigned_data + descriptor, signed_data + descriptor + 4, signed_size - descriptor - 4);
    central -= 4; zip64 -= 4; locator -= 4; eocd -= 4;
    w64(unsigned_data + zip64 + 48, central);
    w64(unsigned_data + locator + 8, zip64);
    w32(unsigned_data + eocd + 16, (uint32_t)central);

    if (!write_file("/tmp/miniz-zip64-descriptor-signed.zip", signed_data, signed_size) ||
        !write_file("/tmp/miniz-zip64-descriptor-unsigned.zip", unsigned_data, unsigned_size)) return 1;
    out = fopen("/tmp/miniz-zip64-descriptor.bin", "wb");
    if (!out || !check(signed_data, signed_size, out) || !check(unsigned_data, unsigned_size, out)) return 1;
    free(unsigned_data); mz_free(signed_data);
    return fclose(out) != 0;
}
