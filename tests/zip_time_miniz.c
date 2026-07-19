#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "miniz.h"

int main(void) {
    static const unsigned char payload[] = "timestamp payload";
    mz_zip_archive zip;
    time_t modified = (time_t)1704164645;
    void *archive = NULL;
    size_t archive_size = 0;
    FILE *file;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) return 1;
    if (!mz_zip_writer_add_mem_ex_v2(&zip, "time.txt", payload, sizeof(payload) - 1,
            NULL, 0, MZ_NO_COMPRESSION, 0, 0, &modified,
            NULL, 0, NULL, 0)) return 1;
    if (!mz_zip_writer_finalize_heap_archive(&zip, &archive, &archive_size)) return 1;
    if (!mz_zip_writer_end(&zip)) return 1;
    file = fopen("/tmp/miniz-zip-exact-time.zip", "wb");
    if (!file) return 1;
    if (fwrite(archive, 1, archive_size, file) != archive_size) return 1;
    if (fclose(file)) return 1;
    mz_free(archive);
    return 0;
}
