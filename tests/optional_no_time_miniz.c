#include <stdio.h>
#include "miniz.h"

int main(void) {
    static const unsigned char payload[] = "optional-build-surfaces: optional-build-surfaces";
    mz_zip_archive zip;
    void *archive = NULL;
    size_t archive_size = 0;
    mz_zip_zero_struct(&zip);
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) return 1;
    if (!mz_zip_writer_add_mem(&zip, "payload.txt", payload, sizeof(payload) - 1, 6)) return 2;
    if (!mz_zip_writer_add_mem(&zip, "empty", NULL, 0, 0)) return 3;
    if (!mz_zip_writer_finalize_heap_archive(&zip, &archive, &archive_size)) return 4;
    if (!mz_zip_writer_end(&zip)) return 5;
    if (fwrite(archive, 1, archive_size, stdout) != archive_size) return 6;
    mz_free(archive);
    return 0;
}
