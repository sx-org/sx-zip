#include <stddef.h>

static volatile long long miniz_heap_calls;

void *malloc(size_t size) {
    (void)size;
    miniz_heap_calls++;
    return NULL;
}

void *calloc(size_t count, size_t size) {
    (void)count;
    (void)size;
    miniz_heap_calls++;
    return NULL;
}

void *realloc(void *pointer, size_t size) {
    (void)pointer;
    (void)size;
    miniz_heap_calls++;
    return NULL;
}

void free(void *pointer) {
    (void)pointer;
    miniz_heap_calls++;
}

long long miniz_heap_call_count(void) {
    return miniz_heap_calls;
}
