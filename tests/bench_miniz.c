#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "miniz.h"

#define BENCH_WARMUPS 3u
#define BENCH_SAMPLES 15u

static uint64_t mono_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

static int run_case(const char *label, const unsigned char *data, size_t size, int level) {
    size_t packed_size = 0, plain_size = 0;
    uint64_t encode_ns, decode_ns;
    mz_uint flags = tdefl_create_comp_flags_from_zip_params(level, 15, MZ_DEFAULT_STRATEGY);
    uint64_t encode_samples[BENCH_SAMPLES], decode_samples[BENCH_SAMPLES];
    unsigned char *packed = NULL;
    size_t run, i;

    for (run = 0; run < BENCH_WARMUPS; ++run) {
        unsigned char *value = (unsigned char *)tdefl_compress_mem_to_heap(
            data, size, &packed_size, (int)flags);
        if (!value) return 0;
        mz_free(value);
    }
    for (run = 0; run < BENCH_SAMPLES; ++run) {
        uint64_t started = mono_ns();
        unsigned char *value = (unsigned char *)tdefl_compress_mem_to_heap(
            data, size, &packed_size, (int)flags);
        encode_samples[run] = mono_ns() - started;
        if (!value) return 0;
        mz_free(value);
    }
    for (i = 1; i < BENCH_SAMPLES; ++i) {
        uint64_t value = encode_samples[i];
        size_t j = i;
        while (j && encode_samples[j - 1] > value) {
            encode_samples[j] = encode_samples[j - 1];
            --j;
        }
        encode_samples[j] = value;
    }
    encode_ns = encode_samples[BENCH_SAMPLES / 2u];

    packed = (unsigned char *)tdefl_compress_mem_to_heap(
        data, size, &packed_size, (int)flags);
    if (!packed) return 0;

    for (run = 0; run < BENCH_WARMUPS; ++run) {
        unsigned char *plain = (unsigned char *)tinfl_decompress_mem_to_heap(
            packed, packed_size, &plain_size, TINFL_FLAG_PARSE_ZLIB_HEADER);
        if (!plain) goto fail;
        if (plain_size != size || memcmp(data, plain, size) != 0) {
            mz_free(plain);
            goto fail;
        }
        mz_free(plain);
    }
    for (run = 0; run < BENCH_SAMPLES; ++run) {
        uint64_t started = mono_ns();
        unsigned char *plain = (unsigned char *)tinfl_decompress_mem_to_heap(
            packed, packed_size, &plain_size, TINFL_FLAG_PARSE_ZLIB_HEADER);
        decode_samples[run] = mono_ns() - started;
        if (!plain) goto fail;
        if (plain_size != size || memcmp(data, plain, size) != 0) {
            mz_free(plain);
            goto fail;
        }
        mz_free(plain);
    }
    for (i = 1; i < BENCH_SAMPLES; ++i) {
        uint64_t value = decode_samples[i];
        size_t j = i;
        while (j && decode_samples[j - 1] > value) {
            decode_samples[j] = decode_samples[j - 1];
            --j;
        }
        decode_samples[j] = value;
    }
    decode_ns = decode_samples[BENCH_SAMPLES / 2u];

    printf("%s level=%d input=%zu packed=%zu ratio_permille=%zu "
           "encode_ns=%llu decode_ns=%llu encode_KiB_s=%llu decode_KiB_s=%llu\n",
           label, level, size, packed_size, size ? packed_size * 1000u / size : 0u,
           (unsigned long long)encode_ns, (unsigned long long)decode_ns,
           (unsigned long long)(encode_ns ? size * 1000000000u / 1024u / encode_ns : 0u),
           (unsigned long long)(decode_ns ? size * 1000000000u / 1024u / decode_ns : 0u));

    {
        char path[128];
        FILE *file;
        snprintf(path, sizeof(path), "/tmp/sx-miniz-bench-c-%s-%d.zlib",
                 label[0] == 'r' ? "repeat" : "random", level);
        file = fopen(path, "wb");
        if (!file) goto fail;
        if (fwrite(packed, 1, packed_size, file) != packed_size) {
            fclose(file);
            goto fail;
        }
        if (fclose(file) != 0) goto fail;
    }

    mz_free(packed);
    return 1;

fail:
    mz_free(packed);
    return 0;
}

int main(void) {
    const size_t size = 1024u * 1024u;
    unsigned char *data = (unsigned char *)malloc(size);
    uint32_t state;
    size_t i;
    int ok;
    if (!data) return 1;

    for (i = 0; i < size; ++i)
        data[i] = (unsigned char)('a' + ((i / 97u + i) % 11u));
    ok = run_case("repetitive", data, size, 1) &&
         run_case("repetitive", data, size, 6) &&
         run_case("repetitive", data, size, 9);

    state = 0x12345678u;
    for (i = 0; i < size; ++i) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        data[i] = (unsigned char)(state & 0xffu);
    }
    ok = ok && run_case("incompressible", data, size, 1) &&
         run_case("incompressible", data, size, 6) &&
         run_case("incompressible", data, size, 9);

    free(data);
    return ok ? 0 : 1;
}
