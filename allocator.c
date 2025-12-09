#include "allocator.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

uint8_t UNUSED_PATTERN[] = {0xA1, 0xB2, 0xC3, 0xD4, 0xE5};
freeBlockHeader *freeListHead = NULL;
uint8_t *g_heap = NULL;
size_t g_heap_size = 0;

// Heap as a block of memory allocated outside of this file (in runme.c) and
// passed to mm_init Requirements:
// - Allocate and free memory (Malloc, Free)
// - Detect bit-flips (metadata and payload) [Corruption Detection]
// - Fail safely when corruption is detected (no undefined behavior i.e crashing)
// - Recover where possible by isolating/quarantining damaged memory regions
// - Operate WITHOUT malloc, free, or other standard C allocation functions

// Constraints:
// - Heap Size and the original heap pointer are provided to mm_init
// - Alignments: All returned pointers must meet the specified byte-alignment
// requirement (40 Bytes)
// - When storms occur, random bits in the heap will be flipped. Your allocator
// must detect these corruptions and fail safely if corrupted (return NULL) and
// quarantine suspect blocks instead of reusing and merging them.
// - Double-free detection (and other memory error handling)

// Each "Allocated Block":
// [Padding][Header][Payload]
// Padding: Variable Size to align Payload to 40 bytes, filled with UNUSED_PATTERN
// Header: Size (of payload), Status Flag, Checksum, Padding Amount
// Payload: User Data

// Each "Free Block":
// [Header][FreeBlockStruct][Unused Space]
// Header: Size (of entire free block inc Header and freeBlock), Status Flag, Checksum, Padding Amount (0)
// FreeBlockStruct: Next Pointer, Previous Pointer, Pointer to Header

// Helper Functions
void quaranBlock(header *head) {
  head->status = 2;
}

size_t paddingCalc(header *first_byte) {
  uintptr_t addr = (uintptr_t)first_byte;  // Header as an integer address
  uintptr_t after_header = addr + sizeof(header);  // If a header was added
  after_header -=
      g_heap ? (uintptr_t)g_heap : 0;  // Adjust relative to heap start
  size_t misalignment =
      after_header %
      40;  // Calculate misalignment (Distance from previous multiple of 40)
  if (misalignment == 0) {
    return (size_t)0;
  }
  return (size_t)40 - misalignment;  // Distance to next multiple of 40 bytes
}

size_t blockSize(header *hdr) {
  return sizeof(header) + hdr->padding + hdr->size;  // Total block size
}

// uint8_t *payloadFinder(header *hdr) {
//     uintptr_t addr = (uintptr_t)hdr; // Header as an integer address
//     uintptr_t after_header = addr + sizeof(header); // Address right after
//     the header size_t misalignment = after_header % 40; // Calculate
//     misalignment size_t padding = 0; if (misalignment != 0) {
//         padding = 40 - misalignment; // Calculate required padding
//     }
//     return (uint8_t *)(after_header + padding); // Return pointer to the
//     payload
// }

uint8_t *payloadFinder(header *hdr) {
  return ((uint8_t *)hdr + sizeof(header));
}

header *searchBestFree(size_t size_requested) {
  freeBlock *curr = freeListHead;
  header *best = NULL;
  size_t best_size = SIZE_MAX;

  while (curr != NULL) {  // Loop through free blocks
    header *currHeader = curr->hdr;
    if (currHeader->status == 0) {
      size_t size_needed =
          paddingCalc(currHeader) + sizeof(header) + size_requested;
      // printf("BESTFIT | %zu | Checking block at address: %p\n", required,
      // (void*)currHeader);
      if (currHeader->size >= size_needed && currHeader->size < best_size) {
        // Found a suitable block
        best_size = currHeader->size;
        best = currHeader;
        // printf("BESTFIT | Found a new best suitable block at address: %p\n",
        // (void*)currHeader); printf("BESTFIT | Size: %zu\n",
        // (size_t)currHeader->size);
      }
    }
    curr = curr->next;  // Next block
  }
  return best;
}

uint8_t checkSumCalc(header *h) {
  if (h == NULL) {
    return 0;
  }
  uint32_t sum = 0;
  uint8_t *data = (uint8_t *)&h->size;
  for (size_t i = 0; i < sizeof(h->size); i++) {
    sum += data[i];
  }
  sum += h->status;
  data = payloadFinder(h);
  if (data != NULL && h->size > 0) {
    for (size_t i = 0; i < h->size; i++) {
      sum += data[i];
    }
  }
  return (uint8_t)sum;
}

// 0 = Valid, 1 = Invalid
int checkBlock(header *h) {
  if (h == NULL) {
    quaranBlock(h);
    return 1;  // Invalid
  }
  if (h->checksum != (uint8_t)(~(h->checksumNOT))) {
    quaranBlock(h);
    return 1;  // Invalid
  }
  uint8_t computedSum = checkSumCalc(h);
  if (computedSum != h->checksum || (uint8_t)(~computedSum) != h->checksumNOT) {
    quaranBlock(h);
    return 1;  // Invalid
  }
  computedSum = h->checksum ^ h->checksumNOT;
  if (computedSum != h->checksumXOR) {
    quaranBlock(h);
    return 1;  // Invalid
  }
  return 0;  // Valid
}

// Free list management functions
void insert_free(freeBlock **head, freeBlock *block) {  // Add a new free block
  block->next = *head;  // Next block is the current head of the list
  block->prev = NULL;
  if (*head != NULL) {  // If there even is an old header
    (*head)->prev =
        block;  // Make the old head's previous point to the new block
  }
  *head = block;  // Update header to new block
}

void remove_free(freeBlock **head,
                 freeBlock *block) {  // Remove block from the free list
  if (block->prev !=
      NULL) {  // If the removed is not a head (Could be middle or tail)
    block->prev->next = block->next;
  } else {                // It's a head
    *head = block->next;  // Update head to the next block
  }
  if (block->next !=
      NULL) {  // If the removed is not a tail (Could be header or middle)
    block->next->prev = block->prev;
  }
  block->next = block->prev = NULL;  // Clear pointers
}

// Visualization Functions
void printWholeHeap() {
  printf("===== Whole Heap Dump =====\n");
  for (size_t i = 0; i < g_heap_size; i++) {
    printf("%02X ", g_heap[i]);
    if ((i + 1) % 16 == 0) {
      printf("\n");
    }
  }
  printf("===== End of Whole Heap Dump =====\n");
}

void printBlock(header *hdr) {
  uint8_t *payload = (uint8_t *)hdr;
  size_t searchSize = hdr->size;
  if (hdr->status == 1) {
    searchSize = blockSize(hdr);
    payload = (uint8_t *)hdr - hdr->padding;
  }
  printf("===== Block %p (%zu Bytes) (H: %p) 16 Rows =====\n", payload,
         searchSize, (void *)hdr);
  for (size_t i = 0; i < searchSize; i++) {
    printf("%02X ", payload[i]);
    if ((i + 1) % 16 == 0) {
      printf("\n");
    }
  }
  printf("===== End of Block =====\n");
}

// Visualization Functions
void printFreeList() {
  printf("===== Free List =====\n");
  freeBlock *curr = freeListHead;
  int idx = 0;
  while (curr != NULL) {
    if (curr == freeListHead) {
      printf("*");
    }
    header *hdr = curr->hdr;
    printf(
        "Block %d: FreeBlock Addr: %p | Header Addr: %p | Size: %zu | status: "
        "%u | Checksum: %u\n",
        idx++, (void *)curr, (void *)hdr, hdr->size, hdr->status,
        hdr->checksum);
    curr = curr->next;
  }
  printf("===== End of Free List =====\n");
}

void printHeap() {
  printf("===== Heap Dump %p =====\n", (void *)g_heap);
  uintptr_t addr = (uintptr_t)g_heap;  // Start of the heap
  uintptr_t end = addr + g_heap_size;  // End of the heap
  // Check if current byte is UNUSED_PATTERN (DANGEROUS WAY OF DOING THIS...)
  while (addr < end) {
    while (*((uint8_t *)addr) == UNUSED_PATTERN[0] || *((uint8_t *)addr) == UNUSED_PATTERN[1] ||
           *((uint8_t *)addr) == UNUSED_PATTERN[2] || *((uint8_t *)addr) == UNUSED_PATTERN[3] ||
           *((uint8_t *)addr) ==
               UNUSED_PATTERN[4]) {  // Find the first block (header) VERY UNSAFE
      // printf("Current Byte: %02X\n", *((uint8_t *)addr));
      addr++;  // Move to the next byte
               // printf("Detected 0x33 Padding, moving to next byte\n");
    }
    // printf("Current Byte: %02X\n", *((uint8_t *)addr));
    header *hdr = (header *)addr;
    if ((uint8_t *)hdr < g_heap || (uint8_t *)hdr >= g_heap + g_heap_size) {
      printf("Reached invalid header address: %p\n", (void *)hdr);
      break;  // Invalid header address
    }
    if (hdr->status == 1) {
      printf("Total Size: %zu | ", blockSize(hdr));
    } else if (hdr->status == 0) {
      printf("FREE BLOCK | ");
    } else {
      printf("CORRUPTED BLOCK | ");
    }
    printf(
        "Header: %p | Payload: %p | Payload Size: %zu | Padding: %u | status: "
        "%u | Checksum: %u / %u / %u\n",
        (void *)hdr, (void *)payloadFinder(hdr), hdr->size, hdr->padding,
        hdr->status, hdr->checksum, hdr->checksumNOT, hdr->checksumXOR);
    if (hdr->status == 1) {  // Allocated
      addr = (uintptr_t)(hdr) + hdr->size + sizeof(header);
    } else if (hdr->status == 0) {  // If it's free
      addr = (uintptr_t)(hdr) + hdr->size;
    } else {
      return;  // Corrupted block, stop printing
    }
  }
  printf("===== End of Heap Dump =====\n");
}

// Initialize the allocator over a provided memory block.
// Returns 0 on success, non-zero on failure.
int mm_init(uint8_t *heap, size_t heap_size) {
  printf("Init | Address of heap: %p\n", (void *)heap);
  // Find default heap pattern:
  if (!heap || heap_size < 5) {
    return -1;  // Failure
  }
  uint8_t pattern[5];
  for (size_t i = 0; i < 5; ++i) {
    printf("Init | Reading pattern byte %zu: %02X\n", i, heap[i]);
    pattern[i] = heap[i];
  }
  // Check that the pattern repeats (at least for the first 20 bytes)
  for (size_t i = 0; i < 20; ++i) {
    if (heap[i] != pattern[i % 5]) {
      return -1;  // Failure
    }
  }
  // Set pattern
  for (size_t i = 0; i < 5; ++i) {
    UNUSED_PATTERN[i] = pattern[i];
  }

  // Ensure everywhere can read the heap
  g_heap = heap;
  g_heap_size = heap_size;

  // Basic sanity checks
  if (g_heap == NULL || g_heap_size < sizeof(header)) {
    return -1;  // Failure
  }

  // Create initial free block (whole heap)
  header *initialHeader = (header *)g_heap;
  printf("Init | Address of initialHeader: %p\n", (void *)initialHeader);
  initialHeader->size = heap_size;
  initialHeader->status = 0;   // Free
  initialHeader->padding = 0;  // Padding to the freeBlock pointer

  // Initialize free list with the initial free block
  freeBlock *initialFreeBlock = (freeBlock *)payloadFinder(initialHeader);
  initialFreeBlock->next = NULL;
  initialFreeBlock->prev = NULL;
  initialFreeBlock->hdr = initialHeader;
  freeListHead = initialFreeBlock;
  initialHeader->checksum = checkSumCalc(initialHeader);  // Compute checksum
  initialHeader->checksumNOT = ~initialHeader->checksum;  // Inverse checksum
  initialHeader->checksumXOR =
      initialHeader->checksum ^ initialHeader->checksumNOT;

  printf("Init | Address of freeListHead: %p\n", (void *)freeListHead);
  return 0;  // Success
}

// Allocate a block with ALIGN-byte aligned payload. Returns NULL on failure.
// Allocate a block with ALIGN-byte aligned payload. Returns NULL on failure.
void *mm_malloc(size_t size) {
  // SMALL OPTIMIZATION: AFTER CALCULATING BEST FIT, CHECK IF THERE'S A PREVIOUS BLOCK TO ABSORB THE PADDING INSTEAD OF PLACING IT

  // When allocating, need to assign a header (metadata) of size and status and
  // a payload of size bytes.
  void *returned = NULL;
  if (size == 0 || size > g_heap_size - sizeof(header)) {
    printf("Malloc | Invalid size requested: %zu\n", size);
    printf("Returning: %p\n", returned);
    return returned;
  }

  printf("Malloc | Req For: %zu\n", size);
  // if (size % 40 != 0) size += 40 - (size % 40); // Round up to nearest
  // multiple of 40

  // printf("Malloc | Looking for a block to fit allocated: %zu Bytes\n", size);
  //  Find a space in the heap
  header *best_fit = searchBestFree(size);
  if (best_fit == NULL) {
    printf("Malloc | No suitable block found for size: %zu\n", size);
    return NULL;  // Suitable block wasn't found
  }
  freeBlock *freeBlk = (freeBlock *)payloadFinder(
      best_fit);                        // Find the original free block struct
  remove_free(&freeListHead, freeBlk);  // Remove from free list

  size_t padding = paddingCalc(best_fit);

  size_t total_block_size = padding + sizeof(header) + size;
  size_t remaining_size = best_fit->size - total_block_size;

  printf(
      "Malloc | First byte found at: %p | Allocation: %zu | Free Block Size: "
      "%zu | Requested Size: %zu\n",
      (void *)best_fit, total_block_size, best_fit->size, size);
  printf("Malloc | Padding needed: %zu\n", padding);
  header *newHead = (header *)((int8_t *)best_fit + padding);
  printf("Malloc | Header after padding will be at: %p\n", (void *)newHead);
  printf("Malloc | Payload will be at: %p\n", (void *)payloadFinder(newHead));

  // Check if we can split the block
  size_t min_split_size =
      sizeof(header) +
      sizeof(freeBlock);  // Minimum size to split off a new free block
  if (remaining_size >= min_split_size) {
    // Create a new free header after the allocation
    header *new_free_header =
        (header *)((uint8_t *)best_fit + total_block_size);
    new_free_header->size = remaining_size;
    new_free_header->status = 0;  // Free
    new_free_header->padding = 0;

    // Create new free block struct and insert into free list
    freeBlock *new_free_block = (freeBlock *)payloadFinder(new_free_header);
    new_free_block->hdr = new_free_header;
    insert_free(&freeListHead, new_free_block);
    new_free_header->checksum = checkSumCalc(new_free_header);
    new_free_header->checksumNOT = ~new_free_header->checksum;
  } else {
    size +=
        remaining_size;  // Absorb the remaining space into the allocated block
  }

  // Update allocated block size
  newHead->size = size;
  newHead->status = 1;  // Allocated
  newHead->padding = (uint8_t)padding;

  // Erase padding
  uint8_t *pad_start = (uint8_t *)best_fit;
  for (size_t i = 0; i < padding; i++) {
    size_t offset_from_heap_start = (size_t)(pad_start - g_heap);
    pad_start[i] = UNUSED_PATTERN[(i + offset_from_heap_start) % 5];
  }

  newHead->checksum = checkSumCalc(newHead);  // Update checksum
  newHead->checksumNOT = ~newHead->checksum;
  newHead->checksumXOR = newHead->checksum ^ newHead->checksumNOT;
  returned = (void *)payloadFinder(newHead);
  return returned;  // Return pointer to payload
}

// Free a previously-allocated pointer (ignore NULL).
// Must detect double-free.
// Check to see if the block is corrupted before freeing.
// Check to see if the previous and next blocks are free and merge if possible.
void mm_free(void *ptr) {
  if (ptr == NULL) {  // Check the pointer is real
    printf("Free | Invalid pointer.\n");
    return;  // Ignore NULL
  }
  if ((uint8_t *)ptr < g_heap || (uint8_t *)ptr >= g_heap + g_heap_size) {
    printf("Free | Invalid pointer (not in heap).\n");
    return;  // Ignore NULL
  }
  // Get header from payload pointer
  header *hdr = (header *)((uint8_t *)ptr - sizeof(header));
  if ((uint8_t *)hdr < g_heap ||
      (uint8_t *)hdr >=
          g_heap + g_heap_size) {  // Check the supposed header is in the heap
    printf("Free | Invalid header from calc.\n");
    return;  // Ignore NULL
  }
  uint8_t *blockStart = ((uint8_t *)ptr - hdr->padding - sizeof(header));
  if ((uint8_t *)hdr < g_heap ||
      (uint8_t *)blockStart >=
          g_heap + g_heap_size) {  // Check the supposed header is in the heap
    printf("Free | Invalid blockStart from calc.\n");
    return;  // Ignore NULL
  }

  printf("Free | Payload to free at: %p\n", (void *)ptr);
  printf("Free | I found the header to free at: %p\n", (void *)hdr);
  printf("Free | The start of the block is at: %p\n", (void *)(blockStart));

  // Validate block
  if (hdr->status != 1) {
    printf("Free | I think it's already free\n");
    return;  // Double free or invalid/broken block
  }
  if (checkBlock(hdr) != 0) {
    printf("Free | I think it's corrupted...\n");
    return;  // Corrupted block
  }

  printf("Freeing block at: %p | Size: %zu\n", (void *)hdr, blockSize(hdr));

  // Look for the next block's first byte
  header *next_block_addr =
      (header *)((uint8_t *)hdr + hdr->size + sizeof(header));
  printf("Free | Next Block Addr Calc: %p\n", (void *)next_block_addr);

  // Look for neighbours
  header *prev = NULL;
  header *next = NULL;
  // Traverse the free list
  freeBlock *curr = freeListHead;
  while (curr != NULL) {
    printf("----\n");
    header *currHdr = curr->hdr;
    uint8_t *currEnd = (uint8_t *)currHdr + currHdr->size;
    printf("Block %p ends at %p\n", (void *)currHdr, (void *)currEnd);
    if (currEnd == blockStart) {
      printf("Found previous block to merge with at: %p\n", (void *)currHdr);
      prev = currHdr;
    }
    if (currHdr == next_block_addr) {
      printf("Found next block to merge with at: %p\n", (void *)currHdr);
      next = currHdr;
    }
    printf(
        "Looking at Block | Addr: %p | Header Addr: %p | Size: %zu | status: "
        "%u | Checksum: %u\n",
        (void *)curr, (void *)currHdr, currHdr->size, currHdr->status,
        currHdr->checksum);
    curr = curr->next;
  }
  printf("----\n");

  header *newHeader = (header *)blockStart;
  size_t newSize = blockSize(hdr);
  // Check if we can coalesce with next block
  if (next != NULL) {
    printf("Free | Opportunity to merge with next block at: %p | Size: %zu\n",
           (void *)next_block_addr, next_block_addr->size);

    // Remove next block from free list
    freeBlock *next_fb = (freeBlock *)payloadFinder(next_block_addr);
    remove_free(&freeListHead, next_fb);

    // Merge sizes
    newSize += next_block_addr->size;
    printf("Free | Updated our block and removed next block! New Size: %zu\n",
           newSize);
  } else {
    printf("Free | No next block to merge with\n");
  }
  // Check if we can coalesce with previous block
  if (prev != NULL) {
    printf("Opportunity to merge with previous block at: %p | Size: %zu\n",
           (void *)prev, prev->size);

    // Remove previous block from free list
    freeBlock *prev_fb = (freeBlock *)payloadFinder(prev);
    remove_free(&freeListHead, prev_fb);

    // Merge sizes
    prev->size = prev->size + newSize;
    printf(
        "Free | Updated previous block and removed our block! New Size: %zu\n",
        prev->size);

    // Update newHeader to now be pointing where prev is
    newHeader = prev;
    newSize = prev->size;
  } else {
    printf("Free | No previous block to merge with\n");
  }
  // Update block as free
  printf("Free | Finishing block %p with size %zu\n", (void *)newHeader,
         newSize);
  newHeader->size = newSize;
  newHeader->status = 0;  // Free
  newHeader->padding = 0;

  // Put in the new free block into the free list
  freeBlock *newFreeBlock = (freeBlock *)payloadFinder(newHeader);
  newFreeBlock->hdr = newHeader;
  insert_free(&freeListHead, newFreeBlock);
  printf("Free | Inserting free block at: %p\n", (void *)newFreeBlock);

  uint8_t *wipe_start = (uint8_t *)newFreeBlock + sizeof(freeBlock);
  size_t wipe_area_size = newHeader->size - sizeof(freeBlock) - sizeof(header);
  printf("Free | Wiping from %p for %zu bytes\n", (void *)wipe_start,
         wipe_area_size);
  if (wipe_area_size > 0) {
    for (size_t i = 0; i < wipe_area_size; ++i) {
      size_t absolute_offset = (uint8_t *)wipe_start - g_heap;
      wipe_start[i] = UNUSED_PATTERN[(absolute_offset + i) % 5];
    }
  }

  newHeader->checksum = checkSumCalc(newHeader);
  newHeader->checksumNOT = ~newHeader->checksum;
  newHeader->checksumXOR = newHeader->checksum ^ newHeader->checksumNOT;
}

// Safely read data from an allocated block at offset bytes into buf.
// Returns the number of bytes read, or -1 if corruption or invalid pointer
// detected.
int mm_read(void *ptr, size_t offset, void *buf, size_t len) {
  // Basic checks
  if (ptr == NULL || buf == NULL) {  // Check the pointers are real
    return -1;
  }
  if ((uint8_t *)ptr < g_heap ||
      (uint8_t *)ptr >= g_heap + g_heap_size) {  // Check pointer is in heap
    printf("Read | Invalid pointer (not in heap).\n");
    return -1;  // Ignore NULL
  }
  // Get header from payload pointer
  header *hdr = (header *)((uint8_t *)ptr - sizeof(header));
  if ((uint8_t *)hdr < g_heap ||
      (uint8_t *)hdr >= g_heap + g_heap_size) {  // Check the supposed header
                                                 // from payload is in the heap
    printf("Read | Invalid header from calc.\n");
    return -1;  // Ignore NULL
  }
  // Validate block
  if (checkBlock(hdr) != 0) {  // Check for corruption
    printf("Read | I think it's corrupted...\n");
    return -1;  // Corrupted block
  }
  if (hdr->status != 1) {  // Check if allocated
    printf("Read | I think it's already free\n");
    return -1;  // Double free or invalid/broken block
  }
  if (len == 0 || offset == hdr->size) {
    return 0;  // Nothing to read
  }

  // Perform the read
  size_t count = 0;
  uint8_t *payload = (uint8_t *)ptr + offset;
  size_t available = hdr->size - offset;
  size_t to_read = (len < available) ? len : available;
  memcpy(buf, payload, to_read);
  count = to_read;
  return count;  // Return number of bytes read
}

// Safely write data into an allocated block at offset bytes from src.
// Returns the number of bytes written, or -1 if corruption or invalid pointer
// detected.
int mm_write(void *ptr, size_t offset, const void *src, size_t len) {
  // Basic checks
  if (ptr == NULL || src == NULL) {  // Check the pointers are real
    return -1;
  }
  if ((uint8_t *)ptr < g_heap ||
      (uint8_t *)ptr >= g_heap + g_heap_size) {  // Check pointer is in heap
    printf("Write | Invalid pointer (not in heap).\n");
    return -1;  // Ignore NULL
  }
  // Get header from payload pointer
  header *hdr = (header *)((uint8_t *)ptr - sizeof(header));
  if ((uint8_t *)hdr < g_heap ||
      (uint8_t *)hdr >= g_heap + g_heap_size) {  // Check the supposed header
                                                 // from payload is in the heap
    printf("Write | Invalid header from calc.\n");
    return -1;  // Ignore NULL
  }
  // Validate block
  if (checkBlock(hdr) != 0) {  // Check for corruption
    printf("Write | I think it's corrupted...\n");
    return -1;  // Corrupted block
  }
  if (hdr->status != 1) {  // Check if allocated
    printf("Write | I think it's already free\n");
    return -1;  // Double free or invalid/broken block
  }
  if (len + offset != hdr->size) {
    return -1;
  }
  if (len == 0 || offset == hdr->size) {
    return 0;  // Nothing to write
  }
  // Perform the write
  size_t count = 0;
  uint8_t *payload = (uint8_t *)ptr + offset;
  size_t available = hdr->size - offset;

  size_t to_write = (len < available) ? len : available;
  memcpy(payload, src, to_write);
  count = to_write;

  // Update checksum after write
  hdr->checksum = checkSumCalc(hdr);
  hdr->checksumNOT = ~hdr->checksum;
  hdr->checksumXOR = hdr->checksum ^ hdr->checksumNOT;
  return count;  // Return number of bytes written
}

// Optional (bonus) functions:
// Resize a previously allocated block to new_size bytes,
// preserving data. [See additional credit]
// On error, return NULL pointer
void *mm_realloc(void *ptr, size_t new_size) {
  // Check pointer
  printf("\nREALLOC | Got request for realloc at %p to new size %zu\n", (void *)ptr,
         new_size);
  if (ptr == NULL) {  // Check the pointer is real
    return mm_malloc(new_size);  // Just malloc new block
  }
  if (new_size == 0) {
    mm_free(ptr);  // Same size anyways
    return NULL;
  }
  if ((uint8_t *)ptr < g_heap ||
      (uint8_t *)ptr >= g_heap + g_heap_size) {  // Check pointer is in heap
    printf("Realloc | Invalid pointer (not in heap).\n");
    return NULL;  // Ignore NULL
  }
  // Get header from payload pointer
  header *hdr = (header *)((uint8_t *)ptr - sizeof(header));
  if ((uint8_t *)hdr < g_heap ||
      (uint8_t *)hdr >= g_heap + g_heap_size) {  // Check the supposed header
                                                 // from payload is in the heap
    printf("Realloc | Invalid header from calc.\n");
    return NULL;  // Ignore NULL
  }
  printf("REALLOC | Found header at %p | Current Size: %zu\n", (void *)hdr,
         hdr->size);
  // Validate block
  if (checkBlock(hdr) != 0) {  // Check for corruption
    printf("Realloc | I think it's corrupted...\n");
    return NULL;  // Corrupted block
  }
  if (hdr->status != 1) {  // Check if allocated
    printf("Write | I think it's already free\n");
    return NULL;  // Double free or invalid/broken block
  }
  if (new_size == hdr->size) {
    return ptr;  // Same size anyways
  }

  // Pre-calc
  // Check if there's a space after the block to expand into
  uint8_t *blockStart = ((uint8_t *)ptr - hdr->padding - sizeof(header));
  header *next_block_addr =
      (header *)((uint8_t *)ptr + hdr->size);
  header *next = NULL;
  header *prev = NULL;
  // Traverse the free list
  freeBlock *curr = freeListHead;
  while (curr != NULL) {
  printf("----\n");
    header *currHdr = curr->hdr;
    uint8_t *currEnd = (uint8_t *)currHdr + currHdr->size;
    printf("Block %p ends at %p\n", (void *)currHdr, (void *)currEnd);
    if (currEnd == blockStart) {
      printf("Found prev block to merge with at: %p\n", (void *)currHdr);
      prev = currHdr;
    }
    if (currHdr == next_block_addr) {
      printf("Found next block to merge with at: %p\n", (void *)currHdr);
      next = currHdr;
    }
    printf(
       "Looking at Block | Addr: %p | Header Addr: %p | Size: %zu | status: "
        "%u | Checksum: %u\n",
       (void *)curr, (void *)currHdr, currHdr->size, currHdr->status,
        currHdr->checksum);
    curr = curr->next;
  }
  printf("----\n");
  // Logic to resize
  if (new_size > hdr->size) { // Make the block bigger
    printf("Realloc | Trying to expand block from %zu to %zu\n", hdr->size,
      new_size);
    if (next != NULL) {
      // Try to merge with next block and see if we can fit
      printf("Realloc | Found space to expand into adjacent next block\n");
      if (next_block_addr->size > (new_size - hdr->size)) { // Enough Space
        // Remove next block from free list
        freeBlock *next_fb = (freeBlock *)payloadFinder(next_block_addr);
        size_t oldFreeSize = next_block_addr->size;
        remove_free(&freeListHead, next_fb);

        // If there's enough space left over, create a new free block
        if (oldFreeSize - (new_size - hdr->size) > sizeof(header) +
            sizeof(freeBlock)) {
          header *newFreeHeader =
            (header *)((uint8_t *)next_block_addr + new_size - hdr->size);
          newFreeHeader->size = oldFreeSize - (new_size - hdr->size);
          newFreeHeader->status = 0;  // Free
          newFreeHeader->padding = 0;
          // Create new free block struct and insert into free list
          freeBlock *new_free_block = (freeBlock *)payloadFinder(newFreeHeader);
          new_free_block->hdr = newFreeHeader;
          insert_free(&freeListHead, new_free_block);
          newFreeHeader->checksum = checkSumCalc(newFreeHeader);
          newFreeHeader->checksumNOT = ~newFreeHeader->checksum;
          newFreeHeader->checksumXOR =
          newFreeHeader->checksum ^ newFreeHeader->checksumNOT;

          // Update current block size
          hdr->size = new_size;
          hdr->checksum = checkSumCalc(hdr);  // Update checksum
          hdr->checksumNOT = ~hdr->checksum;
          hdr->checksumXOR = hdr->checksum ^ hdr->checksumNOT;
          return ptr;   
        }
        printf("Realloc | Not enough space left over to create new free block\n");
        next = NULL;
      } else {
        printf("Realloc | Not enough space in next block to expand into\n");
        next = NULL; // Can't use next block
      }   
    }
    if (prev != NULL) {
      size_t expansion = new_size - hdr->size;
      printf("Realloc | Found space to expand into adjacent previous block\n");

      // Attempt to find enough space in the previous block to place the header into
      uint8_t *new_start_payload =
        (uint8_t *)ptr - (expansion);
        uint8_t *new_start_head =
        (uint8_t *)new_start_payload - sizeof(header);
      printf("Expansion: %zu | New Start Payload: %p\n", expansion,
        (void *)new_start_payload);
      // Calculate offset assuming we place the header + payload with padding=0
      size_t padding = paddingCalc((header *)new_start_head);
      printf("Realloc | Padding calculated for %p header as %zu\n", (void *)(new_start_payload - sizeof(header)), padding);
      // Check if there's enough space in the previous block
      if (sizeof(header) + sizeof(freeBlock) <= prev->size - padding - (expansion)) {
        // Resize previous block and recalc its checksum
        size_t oldSizeAllocated = hdr->size;
        prev->size = prev->size - (expansion) + hdr->padding - padding;
        prev->checksum = checkSumCalc(prev);  // Update checksum
        prev->checksumNOT = ~prev->checksum;
        prev->checksumXOR = prev->checksum ^ prev->checksumNOT; 
        // Move header to new location
        header *new_hdr = (header *)(new_start_payload - sizeof(header));
        new_hdr->size = new_size;
        new_hdr->status = 1; // Allocated
        new_hdr->padding = (uint8_t)padding;
        // Move payload data
        memmove((uint8_t *)new_hdr + sizeof(header) + padding, ptr, hdr->size);
        // Wipe data after the old data location
        uint8_t *wipe_start = (uint8_t *)new_hdr + oldSizeAllocated + sizeof(header);
        size_t wipe_area_size = expansion;
        printf("Free | Wiping from %p for %zu bytes\n", (void *)wipe_start,
          wipe_area_size);
        if (wipe_area_size > 0) {
          for (size_t i = 0; i < wipe_area_size; ++i) {
            size_t absolute_offset = (uint8_t *)wipe_start - g_heap;
            wipe_start[i] = UNUSED_PATTERN[(absolute_offset + i) % 5];
          }
        }
        new_hdr->checksum = checkSumCalc(new_hdr);  // Update checksum
        new_hdr->checksumNOT = ~new_hdr->checksum;
        new_hdr->checksumXOR = new_hdr->checksum ^ new_hdr->checksumNOT;
        // Return new pointer
        return (void *)((uint8_t *)new_hdr + sizeof(header)); 
      } else {
        printf("Realloc | Not enough space in previous block to expand into\n");
        return NULL;
      }
    }
    // If not, try to malloc a new block, copy data, free old block
    void *new_ptr = mm_malloc(new_size);
    if (new_ptr != NULL) {
      size_t to_copy = (hdr->size < new_size) ? hdr->size : new_size;
      memcpy(new_ptr, ptr, to_copy);
      mm_free(ptr);
      return new_ptr;
    }
    printf("Realloc | Couldn't find enough space to expand block\n");
    return NULL;
  } else { // Make the block smaller
    size_t reduction = hdr->size - new_size;
    // Check if we can do it coalesce manually (since free requires 24+16 bytes to auto-coalesce)
    if (next != NULL) { // Coalesce with the next free block

      // Delete the old free block
      freeBlock *next_fb = (freeBlock *)payloadFinder(next);
      size_t oldFreeSize = next->size;
      remove_free(&freeListHead, next_fb);

      // Create a new free block where the new end of the block is
      header* newFreeBlock = (header *)(ptr + new_size);
      newFreeBlock->padding = 0;
      newFreeBlock->status = 0;
      newFreeBlock->size = reduction + oldFreeSize;
      // Create new free block struct and insert into free list
      freeBlock *new_free_block = (freeBlock *)payloadFinder(newFreeBlock);
      new_free_block->hdr = newFreeBlock;
      insert_free(&freeListHead, new_free_block);
      // Checksum
      newFreeBlock->checksum = checkSumCalc(newFreeBlock);
      newFreeBlock->checksumNOT = ~newFreeBlock->checksum;
      newFreeBlock->checksumXOR = newFreeBlock->checksum ^ newFreeBlock->checksumNOT;

      // Update the header
      hdr->size = new_size;
      hdr->checksum = checkSumCalc(hdr);  // Update checksum
      hdr->checksumNOT = ~hdr->checksum;
      hdr->checksumXOR = hdr->checksum ^ hdr->checksumNOT;
      // Return old pointer
      return ptr;
    }
    if (prev != NULL) { // Coalesce with the previous block
      // Idk ask pip, worst case just leave it
      // Problem is copying the data since if you move the header first, the data is deleted...
    }
    // If not, check to see if we can just create a new block afterwards
    if (reduction >= 40) {
      header* newFreeBlock = (header *)(ptr + new_size);
      newFreeBlock->padding = 0;
      newFreeBlock->status = 0;
      newFreeBlock->size = reduction;
      // Create new free block struct and insert into free list
      freeBlock *new_free_block = (freeBlock *)payloadFinder(newFreeBlock);
      new_free_block->hdr = newFreeBlock;
      insert_free(&freeListHead, new_free_block);
      // Checksum
      newFreeBlock->checksum = checkSumCalc(newFreeBlock);
      newFreeBlock->checksumNOT = ~newFreeBlock->checksum;
      newFreeBlock->checksumXOR = newFreeBlock->checksum ^ newFreeBlock->checksumNOT;

      // Update the header
      hdr->size = new_size;
      hdr->checksum = checkSumCalc(hdr);  // Update checksum
      hdr->checksumNOT = ~hdr->checksum;
      hdr->checksumXOR = hdr->checksum ^ hdr->checksumNOT;
      return ptr;
    }
    // If not, try to malloc and find a new space
    void *new_ptr = mm_malloc(new_size);
    if (new_ptr != NULL) {
      size_t to_copy = (hdr->size < new_size) ? hdr->size : new_size;
      memcpy(new_ptr, ptr, to_copy);
      mm_free(ptr);
      return new_ptr;
    }
    // If not, just leave it as is (too small to split)
    return NULL;
  }
  return ptr;
}

// Output current heap usage and integrity statistics
// for debugging (No Credit, helper function).
void mm_heap_stats(void);
// -> print* functions, printBlock(), printHeap(), printWholeHeap(), printFreeList()
