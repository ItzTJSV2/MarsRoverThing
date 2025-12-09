// mm_bench.c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "allocator.h"

#define OPS 300000   // number of operations

int main() {
    uint8_t *heap = malloc(10 * 1024 * 1024); // 10MB heap
    if (!heap) return 1;

    if (mm_init(heap, 10 * 1024 * 1024) != 0) {
        printf("mm_init failed\n");
        return 1;
    }

    void **ptrs = malloc(sizeof(void*) * OPS);

    // --- ALLOC PHASE ---
    for (int i = 0; i < OPS; i++) {
        ptrs[i] = mm_malloc(64);
    }

    // --- FREE PHASE ---
    for (int i = 0; i < OPS; i++) {
        mm_free(ptrs[i]);
    }

    // --- REALLOC-LIKE PHASE ---
    for (int i = 0; i < OPS; i++) {
        void *p = mm_malloc(32);
        mm_write(p, 0, "AAAA", 4);  // small write test
        mm_free(p);

        p = mm_malloc(128);         // "realloc to larger"
        mm_free(p);
    }

    free(ptrs);
    free(heap);
    return 0;
}
