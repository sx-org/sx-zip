#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "miniz.h"

static int put_u32(uint32_t value) {
    unsigned char bytes[4] = {
        (unsigned char)value,
        (unsigned char)(value >> 8),
        (unsigned char)(value >> 16),
        (unsigned char)(value >> 24),
    };
    return fwrite(bytes, 1, 4, stdout) == 4;
}

int main(void) {
    static const unsigned levels[] = { 0, 1, 6, 10 };
    unsigned char pixels[3 * 2 * 4];
    int channels, flip;
    unsigned level_index;
    for (channels = 1; channels <= 4; ++channels) {
        int i;
        for (i = 0; i < 3 * 2 * channels; ++i)
            pixels[i] = (unsigned char)((i * 37 + channels * 19) & 255);
        for (level_index = 0; level_index < sizeof(levels) / sizeof(levels[0]); ++level_index) {
            for (flip = 0; flip <= 1; ++flip) {
                size_t size = 0;
                void *png = tdefl_write_image_to_png_file_in_memory_ex(
                    pixels, 3, 2, channels, &size, levels[level_index], flip);
                if (!png || size > UINT32_MAX || !put_u32((uint32_t)size) ||
                    fwrite(png, 1, size, stdout) != size) {
                    mz_free(png);
                    return 1;
                }
                mz_free(png);
            }
        }
    }
    /* Invoke the default wrapper itself, not only the _ex implementation. */
    {
        size_t size = 0;
        void *png = tdefl_write_image_to_png_file_in_memory(pixels, 3, 2, 4, &size);
        if (!png || !put_u32((uint32_t)size) || fwrite(png, 1, size, stdout) != size) {
            mz_free(png); return 1;
        }
        mz_free(png);
    }
    /* Unsigned levels above ten clamp to the hidden maximum level. */
    {
        static const unsigned high_levels[] = { 11, 99 };
        unsigned n;
        for (n = 0; n < sizeof(high_levels) / sizeof(high_levels[0]); ++n) {
            size_t size = 0;
            void *png = tdefl_write_image_to_png_file_in_memory_ex(
                pixels, 3, 2, 4, &size, high_levels[n], MZ_FALSE);
            if (!png || !put_u32((uint32_t)size) || fwrite(png, 1, size, stdout) != size) {
                mz_free(png); return 1;
            }
            mz_free(png);
        }
    }
    /* Every explicit argument rejection in the upstream helper. */
    {
        static const int invalid[][3] = {
            { 0, 2, 4 }, { -1, 2, 4 }, { 65536, 2, 4 },
            { 3, 0, 4 }, { 3, -1, 4 }, { 3, 65536, 4 },
            { 3, 2, 0 }, { 3, 2, 5 },
        };
        unsigned n;
        for (n = 0; n < sizeof(invalid) / sizeof(invalid[0]); ++n) {
            size_t size = (size_t)0xdeadbeefU;
            void *png = tdefl_write_image_to_png_file_in_memory_ex(
                pixels, invalid[n][0], invalid[n][1], invalid[n][2], &size, 6, MZ_FALSE);
            if (!put_u32(png != NULL) || !put_u32((uint32_t)size)) {
                mz_free(png); return 1;
            }
            mz_free(png);
        }
    }
    return 0;
}
