// mallocBench.c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static inline long long ns_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

void bench_small(size_t n) {
    void **ptrs = malloc(sizeof(void*) * n);

    long long t0 = ns_time();
    for (size_t i = 0; i < n; i++)
        ptrs[i] = malloc(32);
    for (size_t i = 0; i < n; i++)
        free(ptrs[i]);
    long long t1 = ns_time();

    printf("[malloc] Small allocs: %.2f ms\n", (t1 - t0)/1e6);
    free(ptrs);
}

void bench_random(size_t n) {
    void **ptrs = malloc(sizeof(void*) * n);

    long long t0 = ns_time();
    for (size_t i = 0; i < n; i++) {
        size_t sz = (rand() % 4000) + 16;
        ptrs[i] = malloc(sz);
        if ((rand() % 3) == 0)
            free(ptrs[i]);
    }
    long long t1 = ns_time();

    printf("[malloc] Random allocs: %.2f ms\n", (t1 - t0)/1e6);
    free(ptrs);
}

void bench_realloc(size_t n) {
    void **ptrs = malloc(sizeof(void*) * n);

    long long t0 = ns_time();
    for (size_t i = 0; i < n; i++) {
        ptrs[i] = malloc(32);
        ptrs[i] = realloc(ptrs[i], 1024);
    }
    long long t1 = ns_time();

    printf("[malloc] Realloc: %.2f ms\n", (t1 - t0)/1e6);
    free(ptrs);
}

int main() {
    srand(123);
    bench_small(400000);
    bench_random(200000);
    bench_realloc(200000);
    return 0;
}
