#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"

struct fail_state { size_t call, fail_at; };

static void *fail_alloc(void *opaque, size_t items, size_t size) {
    struct fail_state *state = (struct fail_state *)opaque;
    state->call++;
    if (state->call == state->fail_at) return NULL;
    return calloc(items, size);
}

static void fail_free(void *opaque, void *address) {
    (void)opaque;
    free(address);
}

int main(void) {
    mz_stream stream;
    struct fail_state state;
    mz_zip_archive writer, reader;
    void *archive = NULL, *plain;
    size_t archive_size = 0, plain_size = 0;
    int writer_ok;

    memset(&stream, 0, sizeof(stream));
    state.call = 0; state.fail_at = 1;
    stream.zalloc = fail_alloc; stream.zfree = fail_free; stream.opaque = &state;
    printf("deflate_init=%d\n", mz_deflateInit(&stream, 6));

    memset(&stream, 0, sizeof(stream));
    state.call = 0; state.fail_at = 1;
    stream.zalloc = fail_alloc; stream.zfree = fail_free; stream.opaque = &state;
    printf("inflate_init=%d\n", mz_inflateInit(&stream));

    memset(&writer, 0, sizeof(writer));
    state.call = 0; state.fail_at = 1;
    writer.m_pAlloc = fail_alloc; writer.m_pFree = fail_free; writer.m_pAlloc_opaque = &state;
    writer_ok = mz_zip_writer_init_heap(&writer, 0, 0);
    printf("writer_init=%d,%d\n", writer_ok, (int)mz_zip_peek_last_error(&writer));

    memset(&writer, 0, sizeof(writer));
    if (!mz_zip_writer_init_heap(&writer, 0, 0)) return 1;
    if (!mz_zip_writer_add_mem(&writer, "x", "payload", 7, MZ_NO_COMPRESSION)) return 1;
    if (!mz_zip_writer_finalize_heap_archive(&writer, &archive, &archive_size)) return 1;
    if (!mz_zip_writer_end(&writer)) return 1;

    memset(&reader, 0, sizeof(reader));
    if (!mz_zip_reader_init_mem(&reader, archive, archive_size, 0)) return 1;
    state.call = 0; state.fail_at = 1;
    reader.m_pAlloc = fail_alloc; reader.m_pFree = fail_free; reader.m_pAlloc_opaque = &state;
    plain = mz_zip_reader_extract_to_heap(&reader, 0, &plain_size, 0);
    printf("reader_extract=%d,%d\n", plain != NULL, (int)mz_zip_peek_last_error(&reader));
    free(plain);
    mz_zip_reader_end(&reader);
    mz_free(archive);
    return 0;
}
