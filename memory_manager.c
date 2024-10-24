#include "memory_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN_SIZE 16  // Minimum size for a memory block to avoid fragmentation

void* memoryPool = NULL;  // Pointer to the start of the memory pool
BlockMeta* freeList = NULL;  // Pointer to the head of the free block list

// Initialize the memory pool
void mem_init(size_t size) {
    memoryPool = malloc(size);
    if (!memoryPool) {
        printf("Failed to initialize memory pool.\n");
        exit(1);
    }

    // Initialize the first block in the free list
    freeList = (BlockMeta*)memoryPool;
    freeList->size = size - sizeof(BlockMeta);  // The block size is the total size minus metadata
    freeList->isFree = 1;
    freeList->next = NULL;

    printf("Memory pool initialized with size: %zu bytes\n", size);
}

// Allocate a block of memory
void* mem_alloc(size_t size) {
    if (size <= 0) return NULL;

    // Align size to a minimum boundary (e.g., 16 bytes) to avoid fragmentation
    size = (size + (MIN_SIZE - 1)) & ~(MIN_SIZE - 1);

    BlockMeta* current = freeList;
    BlockMeta* previous = NULL;

    while (current != NULL) {
        if (current->isFree && current->size >= size) {
            // Split the block if it's larger than the requested size + metadata
            size_t remainingSize = current->size - size - sizeof(BlockMeta);

            if (remainingSize >= MIN_SIZE) {
                // Create a new block for the remaining free memory
                BlockMeta* newBlock = (BlockMeta*)((char*)current + sizeof(BlockMeta) + size);
                newBlock->size = remainingSize;
                newBlock->isFree = 1;
                newBlock->next = current->next;

                current->size = size;
                current->isFree = 0;
                current->next = newBlock;
            } else {
                // If the remaining size is too small to be a new block, just allocate the entire block
                current->isFree = 0;
            }

            printf("Allocating memory block of size: %zu\n", size);
            return (char*)current + sizeof(BlockMeta);  // Return a pointer to the memory after the metadata
        }

        previous = current;
        current = current->next;
    }

    printf("Error: No suitable block found for size %zu\n", size);
    return NULL;  // Return NULL if no suitable block was found
}

// Free a block of memory
void mem_free(void* ptr) {
    if (ptr == NULL) return;

    BlockMeta* block = (BlockMeta*)((char*)ptr - sizeof(BlockMeta));
    block->isFree = 1;

    // Merge with the next block if it's free
    if (block->next != NULL && block->next->isFree) {
        block->size += block->next->size + sizeof(BlockMeta);
        block->next = block->next->next;
    }

    // Merge with the previous block if it's free
    BlockMeta* current = freeList;
    while (current != NULL) {
        if (current->next == block && current->isFree) {
            current->size += block->size + sizeof(BlockMeta);
            current->next = block->next;
            break;
        }
        current = current->next;
    }

    printf("Memory block freed.\n");
}

// Resize an allocated block
void* mem_resize(void* ptr, size_t newSize) {
    if (ptr == NULL) {
        return mem_alloc(newSize);  // If the pointer is NULL, behave like mem_alloc
    }

    BlockMeta* block = (BlockMeta*)((char*)ptr - sizeof(BlockMeta));

    // If the current block is already big enough, return the same pointer
    if (block->size >= newSize) {
        return ptr;
    }

    // If the next block is free and large enough, merge it
    if (block->next != NULL && block->next->isFree && block->size + block->next->size >= newSize) {
        block->size += block->next->size + sizeof(BlockMeta);
        block->next = block->next->next;
        return ptr;
    }

    // Allocate a new block, copy data to it, and free the old block
    void* newBlock = mem_alloc(newSize);
    if (newBlock != NULL) {
        memcpy(newBlock, ptr, block->size);  // Copy the old data to the new block
        mem_free(ptr);  // Free the old block
    }
    return newBlock;
}

// Deinitialize the memory pool and free the entire memory pool
void mem_deinit() {
    free(memoryPool);  // Free the memory pool
    memoryPool = NULL;
    freeList = NULL;
    printf("Memory pool deinitialized.\n");
}
