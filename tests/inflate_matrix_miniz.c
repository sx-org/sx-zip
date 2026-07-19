#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"

static int put_u32(uint32_t value) {
    unsigned char bytes[4] = {
        (unsigned char)value, (unsigned char)(value >> 8),
        (unsigned char)(value >> 16), (unsigned char)(value >> 24),
    };
    return fwrite(bytes, 1, sizeof(bytes), stdout) == sizeof(bytes);
}

static int record_call(mz_stream *stream, const unsigned char *input,
                       size_t input_size, unsigned char *output,
                       size_t output_size, int flush) {
    mz_uint before_in = (mz_uint)input_size, before_out = (mz_uint)output_size;
    int status;
    stream->next_in = input;
    stream->avail_in = before_in;
    stream->next_out = output;
    stream->avail_out = before_out;
    status = mz_inflate(stream, flush);
    return put_u32((uint32_t)status) &&
           put_u32(before_in - stream->avail_in) &&
           put_u32(before_out - stream->avail_out) &&
           put_u32((uint32_t)stream->total_in) &&
           put_u32((uint32_t)stream->total_out) &&
           put_u32((uint32_t)stream->adler) &&
           put_u32((uint32_t)mz_crc32(0, output, before_out - stream->avail_out));
}

static int one_call_case(const unsigned char *input, size_t input_size,
                         int window_bits, int flush, size_t output_size) {
    unsigned char output[80000];
    mz_stream stream;
    int ok;
    memset(&stream, 0, sizeof(stream));
    if (!put_u32((uint32_t)mz_inflateInit2(&stream, window_bits))) return 0;
    ok = record_call(&stream, input, input_size, output, output_size, flush);
    ok = put_u32((uint32_t)mz_inflateEnd(&stream)) && ok;
    return ok;
}

int main(void) {
    unsigned char plain[70000], packed[80000], raw[80000], bad[80000];
    unsigned char output[80000];
    static const unsigned char fdict[] = { 0x78, 0x20 };
    static const unsigned char bad_header[] = { 0x00, 0x00 };
    static const unsigned char reserved_block[] = { 0x78, 0x01, 0x07, 0, 0, 0, 1 };
    mz_ulong packed_size = sizeof(packed);
    size_t raw_size = 0, i;
    mz_stream stream;
    int status, calls;

    for (i = 0; i < sizeof(plain); ++i)
        plain[i] = (unsigned char)('a' + ((i / 31) % 7));
    if (mz_compress2(packed, &packed_size, plain, sizeof(plain), 6) != MZ_OK) return 1;
    {
        void *raw_heap = tdefl_compress_mem_to_heap(plain, sizeof(plain), &raw_size, 128);
        if (!raw_heap || raw_size > sizeof(raw)) { mz_free(raw_heap); return 1; }
        memcpy(raw, raw_heap, raw_size);
        mz_free(raw_heap);
    }

    /* MZ_PARTIAL_FLUSH is an exact alias for MZ_SYNC_FLUSH on inflate. */
    if (!one_call_case(packed, 3, 15, MZ_NO_FLUSH, 7) ||
        !one_call_case(packed, 3, 15, MZ_PARTIAL_FLUSH, 7) ||
        !one_call_case(packed, 3, 15, MZ_SYNC_FLUSH, 7)) return 1;

    /* MZ_FULL_FLUSH is rejected without consuming input or changing first-call state. */
    memset(&stream, 0, sizeof(stream));
    if (!put_u32((uint32_t)mz_inflateInit(&stream)) ||
        !record_call(&stream, packed, packed_size, output, sizeof(output), MZ_FULL_FLUSH) ||
        !record_call(&stream, packed, packed_size, output, sizeof(output), MZ_FINISH) ||
        !put_u32((uint32_t)mz_inflateEnd(&stream))) return 1;

    /* Exercise the 32 KiB wrapping dictionary and pending-output drain path. */
    memset(&stream, 0, sizeof(stream));
    if (!put_u32((uint32_t)mz_inflateInit(&stream))) return 1;
    if (!record_call(&stream, packed, packed_size, output, 0, MZ_NO_FLUSH)) return 1;
    status = MZ_OK;
    calls = 0;
    while (status != MZ_STREAM_END && calls++ < 16) {
        mz_uint before_in, before_out = 17000;
        const unsigned char *next = packed + stream.total_in;
        before_in = (mz_uint)(packed_size - stream.total_in);
        stream.next_in = next; stream.avail_in = before_in;
        stream.next_out = output; stream.avail_out = before_out;
        status = mz_inflate(&stream, MZ_NO_FLUSH);
        if (!put_u32((uint32_t)status) || !put_u32(before_in - stream.avail_in) ||
            !put_u32(before_out - stream.avail_out) || !put_u32((uint32_t)stream.total_in) ||
            !put_u32((uint32_t)stream.total_out) || !put_u32((uint32_t)stream.adler) ||
            !put_u32((uint32_t)mz_crc32(0, output, before_out - stream.avail_out))) return 1;
    }
    if (status != MZ_STREAM_END || !put_u32((uint32_t)mz_inflateEnd(&stream))) return 1;

    /* Raw mode and every framing/data failure family. */
    if (!one_call_case(raw, raw_size, -15, MZ_FINISH, sizeof(output)) ||
        !one_call_case(bad_header, sizeof(bad_header), 15, MZ_FINISH, sizeof(output)) ||
        !one_call_case(fdict, sizeof(fdict), 15, MZ_FINISH, sizeof(output)) ||
        !one_call_case(reserved_block, sizeof(reserved_block), 15, MZ_FINISH, sizeof(output))) return 1;

    memcpy(bad, packed, packed_size);
    bad[packed_size - 1] ^= 1;
    if (!one_call_case(bad, packed_size, 15, MZ_FINISH, sizeof(output))) return 1;

    memset(&stream, 0, sizeof(stream));
    if (!put_u32((uint32_t)mz_inflateInit(&stream)) ||
        !record_call(&stream, packed, packed_size - 1, output, sizeof(output), MZ_FINISH) ||
        !record_call(&stream, NULL, 0, output, sizeof(output), MZ_FINISH) ||
        !record_call(&stream, NULL, 0, output, sizeof(output), MZ_SYNC_FLUSH) ||
        !record_call(&stream, NULL, 0, output, sizeof(output), MZ_FULL_FLUSH) ||
        !put_u32((uint32_t)mz_inflateEnd(&stream))) return 1;
    return 0;
}
