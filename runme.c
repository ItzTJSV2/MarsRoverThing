#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "allocator.h"

int main(int argc, char* argv[]) {
  unsigned int seed = 0;
  int storm = 0;
  size_t size = 1024;  // Default heap size 64KB
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
  uint8_t* heap_memory = (uint8_t*)malloc(size);
  // Fill entire heap as unused pattern
  uint8_t CUSTOM_PATTERN[] = {0xE1, 0xD2, 0xC3, 0xB4, 0xA5};
  for (size_t i = 0; i < size; ++i) {
    heap_memory[i] = CUSTOM_PATTERN[i % 5];
  }
  // Initialize the memory manager
  mm_init(heap_memory, size);

  printf("Starting allocator tests...\n");

  // --------- Test 1: Basic Allocation ---------
  printf("Test 1: Basic allocation...\n");
  void* a = mm_malloc(16);
  void* b = mm_malloc(32);
  void* c = mm_malloc(64);
  assert(a != NULL && b != NULL && c != NULL);
  mm_free(a);
  mm_free(b);
  mm_free(c);
  printf("Test 1 passed.\n");

  // --------- Test 2: Free Immediately ---------
  printf("Test 2: Free immediately...\n");
  a = mm_malloc(128);
  mm_free(a);
  b = mm_malloc(128);
  assert(b == a);  // Should reuse freed block if allocator works
  mm_free(b);
  printf("Test 2 passed.\n");

  // --------- Test 3: Coalescing ---------
  printf("Test 3: Coalescing...\n");
  a = mm_malloc(32);
  b = mm_malloc(32);
  c = mm_malloc(32);
  mm_free(b);
  mm_free(a);
  void* d = mm_malloc(64);  // Should fit into coalesced a+b
  assert(d == a);
  mm_free(c);
  mm_free(d);
  printf("Test 3 passed.\n");

  // --------- Test 4: Splitting ---------
  printf("Test 4: Splitting...\n");
  a = mm_malloc(128);
  mm_free(a);
  b = mm_malloc(64);
  c = mm_malloc(64);
  assert(b == a && c != NULL && c != b);
  mm_free(b);
  mm_free(c);
  printf("Test 4 passed.\n");

  // --------- Test 5: Edge Cases ---------
  printf("Test 5: Edge cases...\n");
  a = mm_malloc(0);
  assert(a == NULL);  // 0-size allocation returns NULL
  b = mm_malloc(1);
  assert(b != NULL);
  mm_free(b);
  mm_free(a);
  printf("Test 5 passed.\n");

  // --------- Test 6: Multiple Alloc/Free Sequence ---------
  printf("Test 6: Multiple allocation/free sequence...\n");
  printHeap();
  void* blocks[10];
  for (int i = 0; i < 10; i++) blocks[i] = mm_malloc(64 * (i + 1));
  for (int i = 0; i < 10; i += 2) mm_free(blocks[i]);
  for (int i = 1; i < 10; i += 2) mm_free(blocks[i]);
  printf("Test 6 passed.\n");

  // --------- Test 7: Invalid Free ---------
  printf("Test 7: Invalid free...\n");
  int x;
  mm_free(&x);  // Allocator should handle gracefully
  printf("Test 7 passed.\n");

  // --------- Test 8: Realloc of a NULL pointer ---------
  printf("Test 8: Realloc of a NULL pointer...\n");
  a = mm_realloc(NULL, 128);
  assert(a != NULL);  // Check to see if a malloc was made
  mm_free(a);
  printf("Test 8 passed.\n");

  // --------- Test 9: Realloc to a larger size ---------
  printf("Test 9: Realloc to a larger size...\n");
  a = mm_malloc(64);
  b = mm_realloc(a, 128);
  assert(b != NULL && b == a);
  mm_free(b);
  printf("Test 9 passed.\n");

  // --------- Test 10: Realloc to a smaller size ---------
  printf("Test 10: Realloc to a smaller size...\n");
  a = mm_malloc(128);
  b = mm_realloc(a, 64);
  assert(b != NULL && b == a);
  mm_free(b);
  printf("Test 10 passed.\n");

  // --------- Test 11: Realloc to a 0 size ---------
  printf("Test 11: Realloc to a 0 size\n");
  a = mm_malloc(128);
  b = mm_realloc(a, 0);
  assert(b == NULL);
  printf("Test 11 passed.\n");
  printf("All tests passed successfully!\n");
  return 0;
}
