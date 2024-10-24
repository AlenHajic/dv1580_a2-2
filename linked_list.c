#include "memory_manager.h"
#include "linked_list.h"

void list_init(Node** head, size_t size) {
    *head = NULL;
    size_t total_pool_size = sizeof(Node) * size;
    printf("Initializing memory pool with size: %zu bytes\n", total_pool_size);
    mem_init(total_pool_size);
}

void list_insert(Node** head, uint16_t data) {
    Node* new_node = (Node*)mem_alloc(sizeof(Node));
    if (new_node == NULL) {
        printf("Error: Memory allocation failed.\n");
        return;
    }

    new_node->data = data;
    new_node->next = NULL;

    if (*head == NULL) {
        *head = new_node;
    } else {
        Node* current = *head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_node;
    }

    printf("Inserted node with data: %u\n", data);
}

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

void list_cleanup(Node** head) {
    Node* current = *head;
    while (current != NULL) {
        Node* next = current->next;
        mem_free(current);
        current = next;
    }
    *head = NULL;
}
