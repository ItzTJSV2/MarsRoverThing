#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "allocator.h"

int main(int argc, char *argv[]) {
  unsigned int seed = 0;
  int storm = 0;
  size_t size = 1024;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
      seed = (unsigned int)atoi(argv[++i]);
    } else if (strcmp(argv[i], "--storm") == 0 && i + 1 < argc) {
      storm = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
      size = (size_t)atol(argv[++i]);
    } else {
      printf("Unknown or incomplete argument: %s\n", argv[i]);
      return 1;
    }
  }

  printf("seed = %u\nstorm = %d\nsize = %zu\n", seed, storm, size);
  uint8_t *heap_memory = (uint8_t *)malloc(size);
  // Fill entire heap as unused pattern
  uint8_t CUSTOM_PATTERN[] = {0xE1, 0xD2, 0xC3, 0xB4, 0xA5};
  for (size_t i = 0; i < size; ++i) {
    heap_memory[i] = CUSTOM_PATTERN[i % 5];
  }
  // Initialize the memory manager
  mm_init(heap_memory, size);

  void *p1, *p2, *p3, *p4, *p1b;

  printf("\n=== Test 1: Simple allocation ===\n");
  p2 = mm_malloc(640);
  p1 = mm_malloc(64);
  if (!p1) {
    printf("Allocation 64 failed\n");
    return 1;
  }
  printf("Allocated 64 bytes at %p\n", p1);
  mm_free(p2);

  printFreeList();
  printHeap();

  printf("\n=== Test 1b: Free reallocation ===\n");
  p1b = mm_realloc(p1, 640);
  if (!p2) {
    printf("Allocation 640 failed | NULL\n");
    return 1;
  } else if (p1b == p1) {
    printf("Reallocation returned same pointer %p\n", p1b);
  } else {
    printf("Reallocation returned new pointer %p\n", p1b);
  }
  printHeap();
  
  // printf("\n=== Test 2: Multiple allocations ===\n");
  // p2 = mm_malloc(32);
  // p3 = mm_malloc(128);
  // p4 = mm_malloc(16);
  // printf("Allocated 32 bytes at %p\n", p2);
  // printf("Allocated 128 bytes at %p\n", p3);
  // printf("Allocated 16 bytes at %p\n", p4);

  // printHeap();

  // printf("\n=== Test 3: Free in reverse order ===\n");
  // printf("Freed: %p\n", p4);
  // mm_free(p4);
  // printHeap();
  // printf("Freed: %p\n", p3);
  // mm_free(p3);
  // printHeap();
  // printf("Freed: %p\n", p2);
  // mm_free(p2);
  // printHeap();
  // printf("Freed: %p\n", p1);
  // mm_free(p1);
  // printf("Freed all blocks in reverse order\n");

  // printHeap();

  // printf("\n=== Test 4: Allocate entire heap ===\n");
  // void *p_full = mm_malloc(size - 64);  // leave room for header & alignment
  // printf("Allocated full heap block: %p\n", p_full);
  // if (!p_full) printf("Allocation failed as expected if not enough space\n");
  // printHeap();
  // printf("Freeing full heap block: %p\n", p_full);
  // mm_free(p_full);

  // printHeap();

  // printf("\n=== Test 5: Free NULL pointer ===\n");
  // mm_free(NULL);
  // printf("Freeing NULL pointer passed\n");

  // printHeap();

  // printf("\n=== Test 6: Fragmentation test ===\n");
  // void *a = mm_malloc(100);
  // void *b = mm_malloc(200);
  // void *c = mm_malloc(50);
  // printHeap();
  // mm_free(b);  // free middle block
  // printf("Freed middle block: %p\n", b);
  // printHeap();
  // void *d = mm_malloc(180);  // should reuse freed block
  // printf("Allocated 180 bytes at %p (should reuse freed block)\n", d);
  // printHeap();

  // // Free remaining blocks
  // mm_free(a);
  // printHeap();
  // mm_free(c);
  // printHeap();
  // mm_free(d);
  // printf("Freed all remaining blocks\n");

  // printHeap();

  // printf("=== Test 8: Allocate Whole Heap ===\n");
  // void *g = mm_malloc(size);
  // printf("Allocated 0 bytes at %p (should be NULL)\n", g);
  // printf("\n=== All tests completed ===\n");
  // printWholeHeap();
  return 0;
}
