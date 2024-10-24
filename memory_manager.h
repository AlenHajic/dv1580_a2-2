#include <stdio.h>
#include <string.h>
#include <stddef.h> // For size_t

// Helps C++ compilers to handle C header files
#ifdef __cplusplus
extern "C"
{
#endif

typedef struct BlockMeta {
    size_t size;           
    int isFree;            
    struct BlockMeta* next;  
} BlockMeta;

void mem_init(size_t size);
void* mem_alloc(size_t size);
void mem_free(void* block);
void* mem_resize(void* block, size_t size);
void mem_deinit();


#ifdef __cplusplus
}
#endif

