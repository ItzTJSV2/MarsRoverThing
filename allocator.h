#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

// TO DO:
// CHECK REALLOC
// FIX MALLOC/FREE (SOMEHOW BROKEN IN AUTOGRADER)

// Structs
typedef struct header {  // Should be 16 bytes allocated for this, but it's
                         // really 13 bytes padded to 16
  size_t size;           // Size of the payload | 8 bytes
  uint8_t status;  // Free status, 0=Free, 1=Allocated, Else=Quarantined(Assume
                   // corrupted) | 1 byte
  uint8_t checksum;     // Corruption detection | 1 byte
  uint8_t checksumNOT;  // Corruption detection | 1 byte
  uint8_t checksumXOR;  // Corruption detection | 1 byte
  uint8_t padding;      // Padding to align payload to 40 bytes | 1 byte
} header;

typedef struct freeBlock {  // Should be 24 bytes allocated for this on 64-bit
  struct freeBlock* next;   // 8 Bytes
  struct freeBlock* prev;   // 8 Bytes
  header* hdr;              // 8 Bytes
} freeBlock;

typedef freeBlock freeBlockHeader;

extern uint8_t UNUSED_PATTERN[5];
extern freeBlockHeader* freeListHead;
extern uint8_t* g_heap;
extern size_t g_heap_size;

// Helper Functions
size_t paddingCalc(header* first_byte);
size_t blockSize(header* hdr);
uint8_t* payloadFinder(header* hdr);
header* searchBestFree(size_t size);
uint8_t checkSumCalc(header* h);
int checkBlock(header* h);

// Free List Functions:
void insert_free(freeBlock** head, freeBlock* block);
void remove_free(freeBlock** head, freeBlock* block);

// Debug Print Functions
void printWholeHeap();
void printBlock(header* hdr);
void printFreeList();
void printHeap();

// API Functions:
int mm_init(uint8_t* heap, size_t heap_size);
void* mm_malloc(size_t size);
int mm_read(void* ptr, size_t offset, void* buf, size_t len);
int mm_write(void* ptr, size_t offset, const void* src, size_t len);
void mm_free(void* ptr);

// Optional (bonus) functions:
void* mm_realloc(void* ptr, size_t new_size);
void mm_heap_stats(void);

#endif
