#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "miniz.h"

struct sink {
    unsigned char *data;
    size_t capacity, total, offered, calls;
    int reject;
};

static void u32(uint32_t value) {
    unsigned char b[4] = {(unsigned char)value, (unsigned char)(value >> 8),
                          (unsigned char)(value >> 16), (unsigned char)(value >> 24)};
    fwrite(b, 1, sizeof(b), stdout);
}

static mz_bool deflate_put(const void *data, int len, void *opaque) {
    struct sink *sink = (struct sink *)opaque;
    sink->calls++;
    sink->offered += (size_t)len;
    if (sink->reject || sink->total + (size_t)len > sink->capacity) return MZ_FALSE;
    memcpy(sink->data + sink->total, data, (size_t)len);
    sink->total += (size_t)len;
    return MZ_TRUE;
}

static int inflate_put(const void *data, int len, void *opaque) {
    struct sink *sink = (struct sink *)opaque;
    sink->calls++;
    sink->offered += (size_t)len;
    if (sink->reject || sink->total + (size_t)len > sink->capacity) return 0;
    memcpy(sink->data + sink->total, data, (size_t)len);
    sink->total += (size_t)len;
    return 1;
}

int main(void) {
    static unsigned char source[100000], compressed[150000], callback_compressed[150000];
    static unsigned char plain[100000], callback_plain[100000];
    tdefl_compressor *compressor;
    tinfl_decompressor *decompressor;
    struct sink sink;
    size_t compressed_size, result, input_size;
    int flags, ok;
    uint32_t state = 0x9e3779b9U;
    size_t i;

    for (i = 0; i < sizeof(source); ++i) {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        source[i] = (unsigned char)((i % 251 < 211) ? (i * 17 + i / 97) : state);
    }
    flags = tdefl_create_comp_flags_from_zip_params(6, -15, MZ_DEFAULT_STRATEGY);

    compressor = tdefl_compressor_alloc();
    u32(compressor != NULL);
    if (compressor) {
        u32(tdefl_init(compressor, NULL, NULL, flags) == TDEFL_STATUS_OKAY);
        tdefl_compressor_free(compressor);
    } else u32(0);
    tdefl_compressor_free(NULL);

    compressed_size = tdefl_compress_mem_to_mem(compressed, sizeof(compressed), source, sizeof(source), flags);
    u32((uint32_t)compressed_size);
    fwrite(compressed, 1, compressed_size, stdout);
    u32((uint32_t)tdefl_compress_mem_to_mem(compressed, 7, source, sizeof(source), flags));

    memset(&sink, 0, sizeof(sink)); sink.data = callback_compressed; sink.capacity = sizeof(callback_compressed);
    ok = tdefl_compress_mem_to_output(source, sizeof(source), deflate_put, &sink, flags);
    u32((uint32_t)ok); u32((uint32_t)sink.calls); u32((uint32_t)sink.total); u32((uint32_t)sink.offered);
    fwrite(callback_compressed, 1, sink.total, stdout);
    memset(&sink, 0, sizeof(sink)); sink.data = callback_compressed; sink.capacity = sizeof(callback_compressed); sink.reject = 1;
    ok = tdefl_compress_mem_to_output(source, sizeof(source), deflate_put, &sink, flags);
    u32((uint32_t)ok); u32((uint32_t)sink.calls); u32((uint32_t)sink.total); u32((uint32_t)sink.offered);

    decompressor = tinfl_decompressor_alloc();
    u32(decompressor != NULL);
    if (decompressor) { tinfl_init(decompressor); tinfl_decompressor_free(decompressor); }
    tinfl_decompressor_free(NULL);

    result = tinfl_decompress_mem_to_mem(plain, sizeof(plain), compressed, compressed_size, 0);
    u32((uint32_t)result); u32((uint32_t)mz_crc32(MZ_CRC32_INIT, plain, result));
    result = tinfl_decompress_mem_to_mem(plain, 99999, compressed, compressed_size, 0);
    u32(result == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED);

    memset(&sink, 0, sizeof(sink)); sink.data = callback_plain; sink.capacity = sizeof(callback_plain);
    input_size = compressed_size;
    ok = tinfl_decompress_mem_to_callback(compressed, &input_size, inflate_put, &sink, 0);
    u32((uint32_t)ok); u32((uint32_t)input_size); u32((uint32_t)sink.calls);
    u32((uint32_t)sink.total); u32((uint32_t)sink.offered);
    u32((uint32_t)mz_crc32(MZ_CRC32_INIT, callback_plain, sink.total));

    memset(&sink, 0, sizeof(sink)); sink.data = callback_plain; sink.capacity = sizeof(callback_plain); sink.reject = 1;
    input_size = compressed_size;
    ok = tinfl_decompress_mem_to_callback(compressed, &input_size, inflate_put, &sink, 0);
    u32((uint32_t)ok); u32((uint32_t)input_size); u32((uint32_t)sink.calls);
    u32((uint32_t)sink.total); u32((uint32_t)sink.offered);
    return 0;
}
