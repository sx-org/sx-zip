#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "miniz.h"

static const unsigned char plain[] =
    "zlib lifecycle zlib lifecycle zlib lifecycle zlib lifecycle";

static void put32(FILE *file, uint32_t value) {
    unsigned char bytes[4];
    bytes[0] = (unsigned char)value; bytes[1] = (unsigned char)(value >> 8);
    bytes[2] = (unsigned char)(value >> 16); bytes[3] = (unsigned char)(value >> 24);
    fwrite(bytes, 1, 4, file);
}

static void record(FILE *file, int status, size_t consumed, size_t produced,
                   mz_ulong total_in, mz_ulong total_out, mz_ulong adler) {
    put32(file, (uint32_t)status); put32(file, (uint32_t)consumed);
    put32(file, (uint32_t)produced); put32(file, (uint32_t)total_in);
    put32(file, (uint32_t)total_out); put32(file, (uint32_t)adler);
}

int main(void) {
    unsigned char packed[4096], decoded[4096];
    size_t packed_size = 0;
    mz_stream stream;
    FILE *file = fopen("/tmp/miniz-zlib-lifecycle.bin", "wb");
    int status;
    if (!file) return 1;

    memset(&stream, 0, sizeof(stream));
    status = mz_deflateInit2(&stream, 6, MZ_DEFLATED, 15, 9, MZ_FILTERED);
    record(file, status, 0, 0, stream.total_in, stream.total_out, stream.adler);
    stream.next_out = packed; stream.avail_out = 0;
    status = mz_deflate(&stream, MZ_NO_FLUSH);
    record(file, status, 0, 0, stream.total_in, stream.total_out, stream.adler);
    stream.next_in = NULL; stream.avail_in = 0; stream.next_out = packed; stream.avail_out = sizeof(packed);
    status = mz_deflate(&stream, MZ_NO_FLUSH);
    record(file, status, 0, sizeof(packed) - stream.avail_out, stream.total_in, stream.total_out, stream.adler);
    stream.next_out = packed + stream.total_out; stream.avail_out = sizeof(packed) - stream.total_out;
    status = mz_deflate(&stream, MZ_NO_FLUSH);
    record(file, status, 0, 0, stream.total_in, stream.total_out, stream.adler);
    put32(file, (uint32_t)mz_deflateReset(&stream));
    stream.next_in = plain; stream.avail_in = sizeof(plain) - 1;
    stream.next_out = packed; stream.avail_out = sizeof(packed);
    {
        mz_uint before_in = stream.avail_in, before_out = stream.avail_out;
        status = mz_deflate(&stream, MZ_FINISH);
        packed_size = sizeof(packed) - stream.avail_out;
        record(file, status, before_in - stream.avail_in, before_out - stream.avail_out,
               stream.total_in, stream.total_out, stream.adler);
    }
    put32(file, (uint32_t)packed_size); fwrite(packed, 1, packed_size, file);
    stream.next_in = NULL; stream.avail_in = 0; stream.next_out = packed; stream.avail_out = sizeof(packed);
    put32(file, (uint32_t)mz_deflate(&stream, MZ_NO_FLUSH));
    put32(file, (uint32_t)mz_deflate(&stream, MZ_FINISH));
    put32(file, (uint32_t)mz_deflateEnd(&stream));

    memset(&stream, 0, sizeof(stream));
    put32(file, (uint32_t)mz_deflateInit2(&stream, 6, MZ_DEFLATED, 14, 9, MZ_DEFAULT_STRATEGY));
    memset(&stream, 0, sizeof(stream));
    put32(file, (uint32_t)mz_deflateInit2(&stream, 6, MZ_DEFLATED, 15, 0, MZ_DEFAULT_STRATEGY));
    memset(&stream, 0, sizeof(stream));
    status = mz_deflateInit2(&stream, 99, MZ_DEFLATED, -15, 9, MZ_FIXED);
    put32(file, (uint32_t)status);
    if (status == MZ_OK) put32(file, (uint32_t)mz_deflateEnd(&stream));

    memset(&stream, 0, sizeof(stream));
    status = mz_inflateInit2(&stream, 15);
    put32(file, (uint32_t)status);
    stream.next_in = NULL; stream.avail_in = 0; stream.next_out = decoded; stream.avail_out = sizeof(decoded);
    status = mz_inflate(&stream, MZ_NO_FLUSH);
    record(file, status, 0, 0, stream.total_in, stream.total_out, stream.adler);
    put32(file, (uint32_t)mz_inflateReset(&stream));
    stream.next_in = packed; stream.avail_in = (mz_uint)packed_size;
    stream.next_out = decoded; stream.avail_out = sizeof(decoded);
    {
        mz_uint before_in = stream.avail_in, before_out = stream.avail_out;
        status = mz_inflate(&stream, MZ_FINISH);
        record(file, status, before_in - stream.avail_in, before_out - stream.avail_out,
               stream.total_in, stream.total_out, stream.adler);
    }
    put32(file, (uint32_t)(sizeof(plain) - 1)); fwrite(decoded, 1, sizeof(plain) - 1, file);
    stream.next_in = NULL; stream.avail_in = 0; stream.next_out = decoded; stream.avail_out = sizeof(decoded);
    put32(file, (uint32_t)mz_inflate(&stream, MZ_FINISH));
    put32(file, (uint32_t)mz_inflateReset(&stream));
    stream.next_in = packed; stream.avail_in = (mz_uint)packed_size;
    stream.next_out = decoded; stream.avail_out = 2;
    {
        mz_uint before_in = stream.avail_in, before_out = stream.avail_out;
        status = mz_inflate(&stream, MZ_FINISH);
        record(file, status, before_in - stream.avail_in, before_out - stream.avail_out,
               stream.total_in, stream.total_out, stream.adler);
    }
    stream.next_in = packed + stream.total_in; stream.avail_in = (mz_uint)(packed_size - stream.total_in);
    stream.next_out = decoded; stream.avail_out = sizeof(decoded);
    put32(file, (uint32_t)mz_inflate(&stream, MZ_FINISH));

    put32(file, (uint32_t)mz_inflateReset(&stream));
    stream.next_in = NULL; stream.avail_in = 0;
    stream.next_out = decoded; stream.avail_out = sizeof(decoded);
    status = mz_inflate(&stream, MZ_NO_FLUSH);
    record(file, status, 0, 0, stream.total_in, stream.total_out, stream.adler);
    {
        size_t output_at = 0;
        int calls = 0;
        do {
            mz_uint before_in, before_out;
            size_t produced;
            stream.next_in = packed + stream.total_in;
            stream.avail_in = (mz_uint)(packed_size - stream.total_in);
            stream.next_out = decoded + output_at;
            stream.avail_out = 2;
            before_in = stream.avail_in; before_out = stream.avail_out;
            status = mz_inflate(&stream, MZ_NO_FLUSH);
            produced = before_out - stream.avail_out;
            record(file, status, before_in - stream.avail_in, produced,
                   stream.total_in, stream.total_out, stream.adler);
            fwrite(decoded + output_at, 1, produced, file);
            output_at += produced;
            calls++;
        } while (status != MZ_STREAM_END && calls < 128);
        put32(file, (uint32_t)output_at);
    }
    put32(file, (uint32_t)mz_inflateEnd(&stream));
    memset(&stream, 0, sizeof(stream));
    put32(file, (uint32_t)mz_inflateInit2(&stream, 14));

    return fclose(file) != 0;
}
