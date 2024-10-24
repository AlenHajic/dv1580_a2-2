#include <stdint.h>  // For uint16_t
#include <stddef.h>  // For size_t
#include <pthread.h>

// Ensure compatibility with C++ compilers
#ifdef __cplusplus
extern "C" {
#endif

// Struct for the nodes used in the linked list
typedef struct Node {
    uint16_t data;        // The data for each node
    struct Node* next;    // Pointer to the next node in the list
} Node;

void list_init(Node** head, size_t size);
void list_insert(Node** head, uint16_t data);
void list_insert_after(Node* prev_node, uint16_t data);
void list_insert_before(Node** head, Node* next_node, uint16_t data);
void list_delete(Node** head, uint16_t data);
Node* list_search(Node** head, uint16_t data);
void list_display(Node** head);
void list_display_range(Node** head, Node* start_node, Node* end_node);
int list_count_nodes(Node** head);
void list_cleanup(Node** head);

#ifdef __cplusplus
}
#endif
