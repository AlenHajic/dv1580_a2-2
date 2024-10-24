#include "memory_manager.h"
#include "linked_list.h"

// Initialize the linked list
void list_init(Node** head, size_t size) {
    *head = NULL;

    // Allocate a bigger memory pool to handle more inserts (adjust based on your needs)
    size_t total_pool_size = sizeof(Node) * size;
    mem_init(total_pool_size);  // Initialize the memory manager with a larger size.
}

// Insert a new node at the end of the list
void list_insert(Node** head, uint16_t data) {
    // Allocate memory for the new node using the memory manager
    Node* new_node = (Node*)mem_alloc(sizeof(Node));

    // Check if memory allocation was successful
    if (new_node == NULL) {
        printf("Error: Memory allocation failed.\n");
        return;
    }

    // Initialize the new node with the given data and set next to NULL
    new_node->data = data;
    new_node->next = NULL;

    // If the list is empty, the new node becomes the head
    if (*head == NULL) {
        *head = new_node;
    } else {
        // Otherwise, find the last node and append the new node at the end
        Node* current = *head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_node;
    }

    printf("Inserted node with data: %u\n", data);
}

// Insert a new node after a given node
void list_insert_after(Node* prev_node, uint16_t data) {
    if (prev_node == NULL) {
        printf("Error: Previous node cannot be NULL.\n");
        return;
    }

    Node* new_node = (Node*)mem_alloc(sizeof(Node));
    if (new_node == NULL) {
        printf("Error: Memory allocation failed.\n");
        return;
    }

    new_node->data = data;
    new_node->next = prev_node->next;
    prev_node->next = new_node;
}

// Insert a new node before a given node
void list_insert_before(Node** head, Node* next_node, uint16_t data) {
    if (*head == NULL || next_node == NULL) {
        printf("Error: List is empty or next_node is NULL.\n");
        return;
    }

    Node* new_node = (Node*)mem_alloc(sizeof(Node));
    if (new_node == NULL) {
        printf("Error: Memory allocation failed.\n");
        return;
    }

    new_node->data = data;

    if (*head == next_node) {
        new_node->next = *head;
        *head = new_node;
    } else {
        Node* current = *head;
        while (current->next != next_node) {
            if (current->next == NULL) {
                printf("Error: next_node not found.\n");
                return;
            }
            current = current->next;
        }
        new_node->next = next_node;
        current->next = new_node;
    }
}

// Delete a node with the specified data
void list_delete(Node** head, uint16_t data) {
    if (*head == NULL) {
        printf("Error: List is empty.\n");
        return;
    }

    Node* current = *head;
    Node* previous = NULL;

    while (current != NULL && current->data != data) {
        previous = current;
        current = current->next;
    }

    if (current == NULL) {
        printf("Error: Node with data %u not found.\n", data);
        return;
    }

    if (previous == NULL) {
        *head = current->next;
    } else {
        previous->next = current->next;
    }

    mem_free(current);
}

// Search for a node with the specified data
Node* list_search(Node** head, uint16_t data) {
    Node* current = *head;

    while (current != NULL) {
        if (current->data == data) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

// Display all elements in the linked list
void list_display(Node** head) {
    Node* current = *head;

    // Start the list display with the opening bracket
    printf("[");

    while (current != NULL) {
        // Print the current node's data
        printf("%u", current->data);

        // If this is not the last node, print a comma and space
        if (current->next != NULL) {
            printf(", ");
        }

        // Move to the next node
        current = current->next;
    }

    // Close the list display with the closing bracket
    printf("]\n");
}

// Display elements in a specific range
void list_display_range(Node** head, Node* start_node, Node* end_node) {
    Node* current = (start_node == NULL) ? *head : start_node;

    // Start the list display with the opening bracket
    printf("[");

    while (current != NULL) {
        // Print the current node's data
        printf("%u", current->data);

        // Stop if we've reached the end node
        if (current == end_node) {
            break;
        }

        // Move to the next node
        current = current->next;

        // If this is not the last node, print a comma and space
        if (current != NULL) {
            printf(", ");
        }
    }

    // Close the list display with the closing bracket
    printf("]\n");
}

// Count the number of nodes in the linked list
int list_count_nodes(Node** head) {
    Node* current = *head;
    int count = 0;

    while (current != NULL) {
        count++;
        current = current->next;
    }

    return count;
}

// Cleanup the list and free all nodes
void list_cleanup(Node** head) {
    Node* current = *head;

    while (current != NULL) {
        Node* next = current->next;
        mem_free(current);
        current = next;
    }

    *head = NULL;
}
