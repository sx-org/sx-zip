#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "miniz.h"

static FILE *out;
static void u32(uint32_t v) {
    unsigned char b[4] = {(unsigned char)v, (unsigned char)(v >> 8),
                          (unsigned char)(v >> 16), (unsigned char)(v >> 24)};
    fwrite(b, 1, 4, out);
}
static void record(int ok, mz_zip_archive *zip) {
    u32((uint32_t)ok); u32((uint32_t)mz_zip_get_last_error(zip));
    u32((uint32_t)mz_zip_get_archive_size(zip)); u32((uint32_t)mz_zip_get_mode(zip));
}

int main(void) {
    static char file_buffer[65536];
    mz_zip_archive zip;
    FILE *file = tmpfile();
    out = fopen("/tmp/miniz-zip-finalize-flush.bin", "wb");
    if (!file || !out || setvbuf(file, file_buffer, _IOFBF, sizeof(file_buffer)) != 0) return 1;
    memset(&zip, 0, sizeof(zip));
    record(mz_zip_writer_init_cfile(&zip, file, 0), &zip);
    if (close(fileno(file)) != 0) return 2;
    record(mz_zip_writer_add_mem(&zip, "entry", "payload", 7, MZ_NO_COMPRESSION), &zip);
    record(mz_zip_writer_finalize_archive(&zip), &zip);
    record(mz_zip_writer_end(&zip), &zip);
    fclose(file);
    return fclose(out) != 0;
}
