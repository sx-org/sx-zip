#include "miniz.h"

int main(void) {
    static const unsigned char bytes[] = "123456789";
    return mz_crc32(0, bytes, sizeof(bytes) - 1) == 0xcbf43926U ? 0 : 1;
}
