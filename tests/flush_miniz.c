#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"

struct sink { unsigned char *data; size_t size, capacity; };

static mz_bool put(const void *data, int size, void *opaque) {
    struct sink *sink = (struct sink *)opaque;
    size_t needed = sink->size + (size_t)size;
    if (needed > sink->capacity) {
        size_t capacity = sink->capacity ? sink->capacity * 2 : 256;
        unsigned char *next;
        while (capacity < needed) capacity *= 2;
        next = (unsigned char *)realloc(sink->data, capacity);
        if (!next) return MZ_FALSE;
        sink->data = next;
        sink->capacity = capacity;
    }
    memcpy(sink->data + sink->size, data, (size_t)size);
    sink->size = needed;
    return MZ_TRUE;
}

static int emit(const char *name, tdefl_flush flush) {
    static const char first[] = "first first first first first: sync boundary";
    static const char second[] = "second second second second second: final boundary";
    tdefl_compressor compressor;
    struct sink sink = {0};
    char path[96];
    FILE *file;
    int ok;
    int flags = (int)tdefl_create_comp_flags_from_zip_params(6, -15, MZ_DEFAULT_STRATEGY);
    if (tdefl_init(&compressor, put, &sink, flags) != TDEFL_STATUS_OKAY) return 0;
    if (tdefl_compress_buffer(&compressor, first, sizeof(first) - 1, flush) != TDEFL_STATUS_OKAY) { free(sink.data); return 0; }
    if (tdefl_compress_buffer(&compressor, second, sizeof(second) - 1, TDEFL_FINISH) != TDEFL_STATUS_DONE) { free(sink.data); return 0; }
    snprintf(path, sizeof(path), "/tmp/sx-miniz-flush-c-%s.deflate", name);
    file = fopen(path, "wb");
    if (!file) { free(sink.data); return 0; }
    ok = fwrite(sink.data, 1, sink.size, file) == sink.size;
    ok = fclose(file) == 0 && ok;
    free(sink.data);
    return ok;
}

static int emit_zlib(const char *name, int flush) {
    static const unsigned char first[] = "first first first first first: sync boundary";
    static const unsigned char second[] = "second second second second second: final boundary";
    unsigned char packed[4096];
    mz_stream stream;
    char path[96];
    FILE *file;
    int status, end_status, ok;
    memset(&stream, 0, sizeof(stream));
    if (mz_deflateInit2(&stream, 6, MZ_DEFLATED, -15, 9, MZ_DEFAULT_STRATEGY) != MZ_OK) return 0;
    stream.next_in = first;
    stream.avail_in = (mz_uint)(sizeof(first) - 1);
    stream.next_out = packed;
    stream.avail_out = (mz_uint)sizeof(packed);
    status = mz_deflate(&stream, flush);
    if (status != MZ_OK || stream.avail_in != 0) { mz_deflateEnd(&stream); return 0; }
    stream.next_in = second;
    stream.avail_in = (mz_uint)(sizeof(second) - 1);
    status = mz_deflate(&stream, MZ_FINISH);
    if (status != MZ_STREAM_END || stream.avail_in != 0) { mz_deflateEnd(&stream); return 0; }
    snprintf(path, sizeof(path), "/tmp/sx-miniz-flush-c-zlib-%s.deflate", name);
    file = fopen(path, "wb");
    if (!file) { mz_deflateEnd(&stream); return 0; }
    ok = fwrite(packed, 1, stream.total_out, file) == stream.total_out;
    ok = fclose(file) == 0 && ok;
    end_status = mz_deflateEnd(&stream);
    return ok && end_status == MZ_OK;
}

int main(void) {
    return emit("sync", TDEFL_SYNC_FLUSH) && emit("full", TDEFL_FULL_FLUSH) &&
           emit_zlib("none", MZ_NO_FLUSH) &&
           emit_zlib("partial", MZ_PARTIAL_FLUSH) &&
           emit_zlib("sync", MZ_SYNC_FLUSH) &&
           emit_zlib("full", MZ_FULL_FLUSH) ? 0 : 1;
}
