#include "linked_list.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <stddef.h>
#include <math.h>
#include "common_defs.h"
#include "gitdata.h"
#include <pthread.h>

typedef struct
{
    Node **head; // Pointer to the head of the linked list
    Node *prev_node;
    int start_value; // Value of the node to insert after
    int thread_id;   // Unique ID for each thread
    int num_nodes;   // Number of nodes to insert
} thread_data_t;

typedef struct
{
    int num_threads;
    int num_nodes;
} TestParams;

// Function to capture stdout output.
void capture_stdout(char *buffer, size_t size, void (*func)(Node **, Node *, Node *), Node **head, Node *start_node, Node *end_node)
{
    // Save the original stdout
    FILE *original_stdout = stdout;

    // Open a temporary file to capture stdout
    FILE *fp = tmpfile(); // tmpfile() creates a temporary file
    if (fp == NULL)
    {
        printf("Failed to open temporary file for capturing stdout.\n");
        return;
    }

    // Redirect stdout to the temporary file
    stdout = fp;

    // Call the function whose output we want to capture
    func(head, start_node, end_node);

    // Flush the output to the temporary file
    fflush(fp);

    // Reset the file position to the beginning
    rewind(fp);

    // Read the content of the temporary file into the buffer
    fread(buffer, 1, size - 1, fp); // Leave space for null terminator
    buffer[size - 1] = '\0';        // Ensure the buffer is null-terminated

    // Close the temporary file
    fclose(fp);

    // Restore the original stdout
    stdout = original_stdout;
}

// ********* Test basic linked list operations *********

void *thread_insert_function(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;
    for (int i = 0; i < data->num_nodes; i++)
    {
        list_insert(data->head, data->start_value + i);
    }
    return NULL;
}

void test_list_insert_multithread(TestParams *params)
{
    printf_yellow("  Testing list_insert (threads: %d, nodes: %d) ---> ", params->num_threads, params->num_nodes);

    Node *head = NULL;
    list_init(&head, sizeof(Node) * params->num_nodes);

    pthread_t *threads = malloc(params->num_threads * sizeof(pthread_t));
    thread_data_t *thread_data = malloc(params->num_threads * sizeof(thread_data_t));
    int nodes_per_thread = params->num_nodes / params->num_threads;
    // Initialize threads and data structures
    for (int i = 0; i < params->num_threads; i++)
    {
        thread_data[i].head = &head;
        thread_data[i].start_value = i * nodes_per_thread;
        thread_data[i].num_nodes = nodes_per_thread;
        if (pthread_create(&threads[i], NULL, thread_insert_function, &thread_data[i]))
        {
            perror("Failed to create thread");
        }
    }

    // Join all threads
    for (int i = 0; i < params->num_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    my_assert(list_count_nodes(&head) == params->num_nodes);
    // Verify and clean up
    // Note: Verification can be complex in multithreaded contexts due to node order variations
    printf_green("[PASS].\n");
    list_cleanup(&head);

    free(threads);
    free(thread_data);
}

void *thread_insert_after_function(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;
    for (int i = 0; i < data->num_nodes; i++)
    {
        list_insert_after(data->prev_node, data->start_value + i);
    }
    return NULL;
}

void test_list_insert_after_multithread(TestParams *params)
{
    printf_yellow("  Testing list_insert_after (threads: %d, nodes: %d) ---> ", params->num_threads, params->num_nodes);

    Node *head = NULL;
    list_init(&head, sizeof(Node) * (params->num_nodes + 1)); // +1 for the initial node
    list_insert(&head, 10);                                   // Initial node to insert after

    pthread_t *threads = malloc(params->num_threads * sizeof(pthread_t));
    thread_data_t *thread_data = malloc(params->num_threads * sizeof(thread_data_t));
    int nodes_per_thread = params->num_nodes / params->num_threads;

    // Initialize threads and data structures
    for (int i = 0; i < params->num_threads; i++)
    {
        thread_data[i].head = &head;
        thread_data[i].prev_node = head; // Always insert after the first node
        thread_data[i].start_value = i * nodes_per_thread;
        thread_data[i].num_nodes = nodes_per_thread;
        if (pthread_create(&threads[i], NULL, thread_insert_after_function, &thread_data[i]))
        {
            perror("Failed to create thread");
        }
    }

    // Join all threads
    for (int i = 0; i < params->num_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // Verify node count
    my_assert(list_count_nodes(&head) == params->num_nodes + 1); // +1 for the initial node

    // Cleanup and pass message
    list_cleanup(&head);
    free(threads);
    free(thread_data);
    printf_green("[PASS].\n");
}

void *thread_insert_before(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;

    for (int i = 0; i < data->num_nodes; i++)
    {
        // Data calculation could be adjusted to fit specific patterns or test cases
        uint16_t insert_data = (data->thread_id + 1) * 100 + i; // Example pattern
        list_insert_before(data->head, data->prev_node, insert_data);
    }
    return NULL;
}

void test_list_insert_before_multithreaded(TestParams *params)
{
    printf_yellow("  Testing list_insert_before with %d threads, each inserting %d nodes ---> ", params->num_threads, params->num_nodes);
    Node *head = NULL;
    list_init(&head, sizeof(Node) * (params->num_threads + params->num_nodes + 1)); // Allocate enough space

    Node **nodes = malloc(sizeof(Node *) * (params->num_threads + 1)); // Array of pointers to Node
    list_insert(&head, 0);                                             // Insert the initial head node
    nodes[0] = head;                                                   // Save head node pointer

    // Insert additional nodes to serve as insertion targets
    for (int i = 1; i <= params->num_threads; i++)
    {
        list_insert(&head, i * 10);    // Sequentially increasing data
        nodes[i] = nodes[i - 1]->next; // Save pointer to the newly added node
    }

    pthread_t threads[params->num_threads];
    thread_data_t thread_data[params->num_threads];

    // Set up thread data and create threads for inserting nodes
    for (int i = 0; i < params->num_threads; i++)
    {
        thread_data[i].head = &head;
        thread_data[i].prev_node = nodes[i];                                // Each thread starts at a different initial node
        thread_data[i].num_nodes = params->num_nodes / params->num_threads; // Distribute nodes evenly
        pthread_create(&threads[i], NULL, thread_insert_before, &thread_data[i]);
    }

    // Join all threads
    for (int i = 0; i < params->num_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // Optional: Verify the list structure, node count, etc.
    int expected_count = params->num_threads + params->num_nodes + 1; // Initial nodes + inserted nodes + head
    my_assert(list_count_nodes(&head) == expected_count);
    list_cleanup(&head);

    free(nodes); // Free the dynamically allocated nodes array
    printf_green("[PASS].\n");
}

void *thread_delete_function(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;
    for (int i = 0; i < data->num_nodes; i++)
    {
        // Compute data value assuming unique values for simplicity
        uint16_t data_value = data->thread_id * data->num_nodes + i; // Adjust the multiplier if necessary to match insertion pattern
        list_delete(data->head, data_value);
    }
    return NULL;
}

void test_list_delete_multithreaded(TestParams *params)
{
    printf_yellow("  Testing list_delete with %d threads, nodes: %d ---> ", params->num_threads, params->num_nodes);
    Node *head = NULL;
    list_init(&head, sizeof(Node) * (params->num_threads * params->num_nodes));

    // Insert nodes into the list
    for (int i = 0; i < params->num_nodes; i++)
    {
        list_insert(&head, i); // Ensure unique values for simplicity
    }

    pthread_t *threads = malloc(params->num_threads * sizeof(pthread_t));
    thread_data_t *thread_data = malloc(params->num_threads * sizeof(thread_data_t));
    int nodes_per_thread = params->num_nodes / params->num_threads;

    // Initialize threads and their respective data
    for (int i = 0; i < params->num_threads; i++)
    {
        thread_data[i].head = &head;
        thread_data[i].num_nodes = nodes_per_thread;
        thread_data[i].thread_id = i;
        pthread_create(&threads[i], NULL, thread_delete_function, &thread_data[i]);
    }

    // Join all threads
    for (int i = 0; i < params->num_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // Verify the remaining list is empty if all nodes were to be deleted
    my_assert(list_count_nodes(&head) == 0); // Assuming all nodes are supposed to be deleted

    printf_green("[PASS].\n");

    list_cleanup(&head);
    free(threads);
    free(thread_data);
}

void test_list_delete()
{
    printf_yellow("  Testing list_delete ---> ");
    Node *head = NULL;
    list_init(&head, sizeof(Node) * 2);
    list_insert(&head, 10);
    list_insert(&head, 20);
    list_delete(&head, 10);
    my_assert(head->data == 20);
    list_delete(&head, 20);
    my_assert(head == NULL);

    list_cleanup(&head);
    printf_green("[PASS].\n");
}

void test_list_search()
{
    printf_yellow("  Testing list_search ---> ");
    Node *head = NULL;
    list_init(&head, sizeof(Node) * 2);
    list_insert(&head, 10);
    list_insert(&head, 20);
    Node *found = list_search(&head, 10);
    my_assert(found->data == 10);

    Node *not_found = list_search(&head, 30);
    my_assert(not_found == NULL);

    list_cleanup(&head);
    printf_green("[PASS].\n");
}

void test_list_display()
{
    printf_yellow("  Testing list_display ... \n");
    Node *head = NULL;

    int Nnodes = 5 + rand() % 5;
#ifdef DEBUG
    printf_yellow("   Testing %d nodes.\n", Nnodes);
#endif

    list_init(&head, sizeof(Node) * Nnodes);

    int randomLow = rand() % Nnodes;

    //    randomLow=0;

    int randomHigh = randomLow + rand() % (Nnodes - randomLow);
    while (randomHigh == 0)
    {
        randomHigh = randomLow + rand() % (Nnodes - randomLow);
    }

#ifdef DEBUG
    int Delta = randomHigh - randomLow;
    printf("Random [%d,%d] delta= %d \n", randomLow, randomHigh, Delta);
#endif

    char *stringFull = malloc(1024);
    char *string2Last = malloc(1024);
    char *string1third = malloc(1024);
    char *stringRandom = malloc(1024);

    sprintf(stringFull, "[");
    sprintf(string2Last, "[");

    Node *Low = NULL;
    Node *High = NULL;
    char LowValue[10];
    char HighValue[10];

    int values[Nnodes];
    for (int i = 0; i < Nnodes; i++)
    {
        values[i] = 0;
    }
    for (int k = 0; k < Nnodes; k++)
    {
        values[k] = 10 + rand() % 90;
        list_insert(&head, values[k]);
        if (k == randomLow && !Low)
        {
            Low = list_search(&head, values[k]);
            sprintf(LowValue, "%d", values[k]);
        }
        if (k == randomHigh && !High)
        {
            High = list_search(&head, values[k]);
            sprintf(HighValue, "%d", values[k]);
        }
        sprintf(stringFull + strlen(stringFull), "%d", values[k]);
        if (k < (Nnodes - 1))
        {
            sprintf(stringFull + strlen(stringFull), ", ");
        }
        else
        {
            sprintf(stringFull + strlen(stringFull), "]");
        }
    }

#ifdef DEBUG
    printf("LowValue=%s, HighValue=%s\n", LowValue, HighValue);
    printf("RefFull:'%s'\n", stringFull);
#endif

    // [N0, N1, ...., NL]
    sprintf(string2Last + 1, "%s", strchr(stringFull, ',') + 2);

    //    printf("ref2Last: '%s\n", string2Last);

    // [N0, N1, ...., NL]
    char *Third = stringFull;
    for (int i = 0; i < 3; i++)
    {
        Third = strchr(Third, ',');
        Third += 1;
    }
    int LenToThird = ((Third - stringFull));

    strncpy(string1third, stringFull, LenToThird - 1);
    sprintf(string1third + strlen(string1third), "]");

    // printf("ref1third: '%s\n", string1third);

    char *start = 0;
    char *first = 0;
    char *last = 0;

    // Find first random node.
    start = strstr(stringFull, LowValue);
    first = strstr(stringFull, HighValue);
    if (strlen(first) > 3)
    {
        // We have atleast one number after us.
#ifdef DEBUG
        printf("Last isnt last.\n");
#endif
        last = strstr(first, " ");
    }
    else
    {
        // We are the last number.
#ifdef DEBUG
        printf("Last was last.\n");
#endif
        last = strstr(first, "]");
    }

    ptrdiff_t LenToFirst = ((char *)start - (char *)stringFull);
    ptrdiff_t LenToLast = ((char *)last - (char *)stringFull);

#ifdef DEBUG
    printf("random starts at %p (offset=%ld) and ends at %p (offset=%ld).\n", start, LenToFirst, last, LenToLast);
    printf("random: '%s' \n", start);

#endif

    char *blob = malloc(1024);
    strncpy(blob, start, LenToLast - LenToFirst);

    sprintf(stringRandom, "[%s", blob);
    if (!strchr(stringRandom, ']'))
    {
        sprintf(stringRandom + strlen(stringRandom), "]");
    }
    if (strstr(stringRandom, ",]"))
    {
        // Solution change ",]" to "]\0";
        char *ptr = strstr(stringRandom, ",]");
        sprintf(ptr, "]");
        memset(ptr + 1, 0, 1);
#ifdef DEBUG
        printf("We have a problem Huston.\n");
        printf("Fixed ,] issue.\n");
#endif
    }

    if (strstr(stringRandom, ", ]"))
    {
#ifdef DEBUG
        printf("We have a problem Huston2.\n");
#endif
    }

    //    printf("RefRandom: '%s' \n\n", stringRandom);

    char buffer[1024] = {0}; // Buffer to capture the output

    // Test case 1: Displaying full list
    capture_stdout(buffer, sizeof(buffer), (void (*)(Node **, Node *, Node *))list_display_range, &head, NULL, NULL);
    my_assert(strcmp(buffer, stringFull) == 0);
    printf("\tFull list: %s\n", buffer);

    // Test case 2: Displaying list from second node to end
    memset(buffer, 0, sizeof(buffer)); // Clear buffer
    capture_stdout(buffer, sizeof(buffer), (void (*)(Node **, Node *, Node *))list_display_range, &head, head->next, NULL);
    my_assert(strcmp(buffer, string2Last) == 0);
    printf("\tFrom second node to end: %s\n", buffer);

    // Test case 3: Displaying list from first node to third node
    memset(buffer, 0, sizeof(buffer)); // Clear buffer
    capture_stdout(buffer, sizeof(buffer), (void (*)(Node **, Node *, Node *))list_display_range, &head, head, head->next->next);
    my_assert(strcmp(buffer, string1third) == 0);
    printf("\tFrom first node to third node: %s\n", buffer);

    // Test case 4: Displaying random nodes
    memset(buffer, 0, sizeof(buffer)); // Clear buffer
    capture_stdout(buffer, sizeof(buffer), (void (*)(Node **, Node *, Node *))list_display_range, &head, Low, High);
    my_assert(strcmp(buffer, stringRandom) == 0);
    printf("\tK random node(s): %s\n", buffer);

    list_cleanup(&head);
    printf_green("  ... [PASS].\n");
}

void test_list_count_nodes()
{
    printf_yellow("  Testing list_count_nodes ---> ");
    Node *head = NULL;
    list_init(&head, sizeof(Node) * 3);
    list_insert(&head, 10);
    list_insert(&head, 20);
    list_insert(&head, 30);

    int count = list_count_nodes(&head);
    my_assert(count == 3);

    list_cleanup(&head);
    printf_green("[PASS].\n");
}

void test_list_cleanup()
{
    printf_yellow("  Testing list_cleanup ---> ");
    Node *head = NULL;
    list_init(&head, sizeof(Node) * 3);
    list_insert(&head, 10);
    list_insert(&head, 20);
    list_insert(&head, 30);

    list_cleanup(&head);
    my_assert(head == NULL);
    printf_green("[PASS].\n");
}

// ********* Stress and edge cases *********

void test_list_insert_loop(int count)
{
    printf_yellow("  Testing list_insert loop ---> ");
    Node *head = NULL;
    list_init(&head, sizeof(Node) * count);
    for (int i = 0; i < count; i++)
    {
        list_insert(&head, i);
    }

    Node *current = head;
    for (int i = 0; i < count; i++)
    {
        my_assert(current->data == i);
        current = current->next;
    }

    list_cleanup(&head);
    printf_green("[PASS].\n");
}

void test_list_insert_after_loop(int count)
{
    printf_yellow("  Testing list_insert_after loop ---> ");
    Node *head = NULL;
    list_init(&head, sizeof(Node) * (count + 1));
    list_insert(&head, 12345);

    Node *node = list_search(&head, 12345);
    for (int i = 0; i < count; i++)
    {
        list_insert_after(node, i);
    }

    Node *current = head;
    my_assert(current->data == 12345);
    current = current->next;

    for (int i = count - 1; i >= 0; i--)
    {
        my_assert(current->data == i);
        current = current->next;
    }

    list_cleanup(&head);
    printf_green("[PASS].\n");
}

void test_list_delete_loop(int count)
{
    printf_yellow("  Testing list_delete loop ---> ");
    Node *head = NULL;
    list_init(&head, sizeof(Node) * count);
    for (int i = 0; i < count; i++)
    {
        list_insert(&head, i);
    }

    for (int i = 0; i < count; i++)
    {
        list_delete(&head, i);
    }

    my_assert(head == NULL);

    list_cleanup(&head);
    printf_green("[PASS].\n");
}

void test_list_search_loop(int count)
{
    printf_yellow("  Testing list_search loop ---> ");
    Node *head = NULL;
    list_init(&head, sizeof(Node) * count);
    for (int i = 0; i < count; i++)
    {
        list_insert(&head, i);
    }

    for (int i = 0; i < count; i++)
    {
        Node *found = list_search(&head, i);
        my_assert(found->data == i);
    }

    list_cleanup(&head);
    printf_green("[PASS].\n");
}

void test_list_edge_cases()
{
    printf_yellow("  Testing list edge cases ---> ");
    Node *head = NULL;
    list_init(&head, sizeof(Node) * 3);

    // Insert at head
    list_insert(&head, 10);
    my_assert(head->data == 10);

    // Insert after
    Node *node = list_search(&head, 10);
    list_insert_after(node, 20);
    my_assert(node->next->data == 20);

    // Insert before
    list_insert_before(&head, node, 15);

    my_assert(head->data == 15);
    my_assert(head->next->data == 10);
    my_assert(head->next->next->data == 20);

    // Delete
    list_delete(&head, 15);
    my_assert(node->next->data == 20);

    // Search
    Node *found = list_search(&head, 20);
    my_assert(found->data == 20);

    list_cleanup(&head);
    printf_green("[PASS].\n");
}

// Main function to run all tests
int main(int argc, char *argv[])
{

    int base_num_threads = 4;

    srand(time(NULL));
#ifdef VERSION
    printf("Build Version; %s \n", VERSION);
#endif
    printf("Git Version; %s/%s \n", git_date, git_sha);
    if (argc < 2)
    {
        printf("Usage: %s <test function>\n", argv[0]);
        printf("Available test functions:\n");
        printf("Basic Operations with a base number of threads (4) and nodes (1024):\n");
        printf(" 1. test_list_insert - Test basic list insert operations with a base number of threads\n");
        printf(" 2. test_list_insert_after - Test list insert after a given node\n");
        printf(" 3. test_list_insert_before - Test list insert before a given node\n");
        printf(" 4. test_list_delete - Test delete operation\n");

        printf("\nStress testing basic operations with various numbers of threads and nodes:\n");
        printf(" 5. test_list - Test multiple configurations\n");
        printf(" 6. test_list_insert_after - Test multiple insertions after a given node\n");
        printf(" 7. test_list_insert_after - Test multiple insertions after a given node\n");
        printf(" 8. test_list_delete - Test multiple detelions\n");
        printf(" 0. Run all tests\n");
        return 1;
    }

    switch (atoi(argv[1]))
    {
    case -1:
        printf("No tests will be executed.\n");
        break;
    case 0:
        printf("Testing Basic Operations with base number of threads:\n");
        test_list_insert_multithread(&(TestParams){.num_threads = base_num_threads, .num_nodes = 1024});
        test_list_insert_after_multithread(&(TestParams){.num_threads = base_num_threads, .num_nodes = 1024});
        test_list_insert_before_multithreaded(&(TestParams){.num_threads = base_num_threads, .num_nodes = 1024});
        test_list_delete_multithreaded(&(TestParams){.num_threads = base_num_threads, .num_nodes = 1024});

        printf("\nStress testing basic operations with various numbers of threads and nodes:\n");
        for (int i = 0; i < 9; i++)      // from 2^0 = 1 up to 2^8 = 256 threads
            for (int j = 8; j < 15; j++) // from 2^8 = 256 up to 2^14 = 16384 nodes
            {
                test_list_insert_multithread(&(TestParams){.num_threads = pow(2, i), .num_nodes = pow(2, j)});
                test_list_insert_after_multithread(&(TestParams){.num_threads = pow(2, i), .num_nodes = pow(2, j)});
                test_list_insert_before_multithreaded(&(TestParams){.num_threads = pow(2, i), .num_nodes = pow(2, j)});
                test_list_delete_multithreaded(&(TestParams){.num_threads = pow(2, i), .num_nodes = pow(2, j)});
            }

        break;
    case 1:
        test_list_insert_multithread(&(TestParams){.num_threads = base_num_threads, .num_nodes = 1024});
        break;
    case 2:
        test_list_insert_after_multithread(&(TestParams){.num_threads = base_num_threads, .num_nodes = 1024});
        break;
    case 3:
        test_list_insert_before_multithreaded(&(TestParams){.num_threads = base_num_threads, .num_nodes = 1024});
        break;
    case 4:
        test_list_delete_multithreaded(&(TestParams){.num_threads = base_num_threads, .num_nodes = 1024});
        break;
    case 5:
        for (int i = 0; i < 9; i++)      // from 2^0 = 1 up to 2^8 = 256 threads
            for (int j = 8; j < 15; j++) // from 2^8 = 256 up to 2^14 = 16384 nodes
                test_list_insert_multithread(&(TestParams){.num_threads = pow(2, i), .num_nodes = pow(2, j)});
        break;
    case 6:
        for (int i = 0; i < 9; i++)      // from 2^0 = 1 up to 2^8 = 256 threads
            for (int j = 8; j < 15; j++) // from 2^8 = 256 up to 2^14 = 16384 nodes
                test_list_insert_after_multithread(&(TestParams){.num_threads = pow(2, i), .num_nodes = pow(2, j)});
        break;
    case 7:
        for (int i = 0; i < 9; i++)      // from 2^0 = 1 up to 2^8 = 256 threads
            for (int j = 8; j < 15; j++) // from 2^8 = 256 up to 2^14 = 16384 nodes
                test_list_insert_before_multithreaded(&(TestParams){.num_threads = pow(2, i), .num_nodes = pow(2, j)});
        break;
    case 8:
        for (int i = 0; i < 9; i++)      // from 2^0 = 1 up to 2^8 = 256 threads
            for (int j = 8; j < 14; j++) // from 2^8 = 256 up to 2^14 = 16384 nodes
                test_list_delete_multithreaded(&(TestParams){.num_threads = pow(2, i), .num_nodes = pow(2, j)});
        break;

    default:
        printf("Invalid test function\n");
        break;
    }

    return 0;
}
