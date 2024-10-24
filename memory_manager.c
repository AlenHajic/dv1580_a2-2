#include "memory_manager.h"

#define MIN_SIZE 16    

void* memoryPool = NULL;
BlockMeta* freeList = NULL;  // Pointer to the head of the free list

void mem_init(size_t size) {
    memoryPool = malloc(size);
    if (!memoryPool) {
        printf("Failed to initialize memory pool.\n");
        exit(1);
    }

    freeList = (BlockMeta*)memoryPool;
    freeList->size = size - sizeof(BlockMeta);
    freeList->isFree = 1;
    freeList->next = NULL;

    printf("Memory pool initialized with size: %zu\n", size);
}

void* mem_alloc(size_t size) {
    BlockMeta* current = freeList;
    BlockMeta* previous = NULL;

    while (current != NULL) {
        if (current->isFree && current->size >= size) {
            size_t remainingSize = current->size - size;

            if (remainingSize >= MIN_SIZE + sizeof(BlockMeta)) {
                BlockMeta* newBlock = (BlockMeta*)((char*)current + sizeof(BlockMeta) + size);
                newBlock->size = remainingSize;
                newBlock->isFree = 1;
                newBlock->next = current->next;

                current->size = size;
                current->isFree = 0;
                current->next = newBlock;
            } else {
                current->isFree = 0;
            }

            printf("Allocating memory block of size: %zu\n", size);
            return (char*)current + sizeof(BlockMeta);
        }

        previous = current;
        current = current->next;
    }

    printf("Error: No suitable block found for size %zu\n", size);
    return NULL;
}

void mem_free(void* ptr) {
    if (ptr == NULL) return;

    BlockMeta* block = (BlockMeta*)((char*)ptr - sizeof(BlockMeta));
    block->isFree = 1;

    BlockMeta* current = freeList;
    while (current != NULL) {
        if (current->isFree && current->next != NULL && current->next->isFree) {
            current->size += current->next->size + sizeof(BlockMeta);
            current->next = current->next->next;
        }
        current = current->next;
    }

    printf("Memory block freed.\n");
}

void* mem_resize(void* ptr, size_t newSize) {
    if (ptr == NULL) {
        return mem_alloc(newSize);
    }

    BlockMeta* block = (BlockMeta*)((char*)ptr - sizeof(BlockMeta));

    if (block->size >= newSize) {
        return ptr;
    }

    BlockMeta* nextBlock = block->next;
    if (nextBlock != NULL && nextBlock->isFree && block->size + nextBlock->size >= newSize) {
        block->size += nextBlock->size + sizeof(BlockMeta);
        block->next = nextBlock->next;
        return ptr;
    }

    void* newBlock = mem_alloc(newSize);
    if (newBlock != NULL) {
        memcpy(newBlock, ptr, block->size);
        mem_free(ptr);
    }

    return newBlock;
}

void mem_deinit() {
    free(memoryPool);
    memoryPool = NULL;
    freeList = NULL;
    printf("Memory pool deinitialized.\n");
}
