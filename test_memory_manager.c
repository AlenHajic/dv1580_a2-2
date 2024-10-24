#include <pthread.h>
#include <sys/time.h>
#include <math.h>
#include <stdbool.h>
#include "memory_manager.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "common_defs.h"


#include <unistd.h>

#define debug 0

#include "gitdata.h"

my_barrier_t barrier; // Declare our custom barrier

// Data structure to pass arguments to threads
typedef struct
{
    int thread_id;         // Unique ID for each thread
    size_t block_size;     // Size of each block to allocate
    int iterations;        // Number of times to allocate and free each block
    int num_blocks;        // Number of blocks to allocate in total
    int max_block_size;    // Maximum size of a block
    void **block_pointers; // Array to hold pointers to allocated blocks
    bool simulate_work;    // Flag to simulate work in the thread, i.e. put the thread to sleep for a while
} thread_data_t;

// Structure to hold test function parameters
typedef struct
{
    int num_threads;
    size_t memory_size;
    int iterations;
    int num_blocks;
    size_t block_size;
    bool simulate_work;
} TestParams;

// Function to calculate memory allocations for threads based on redistribution logic
size_t *calculate_thread_allocations(int num_threads, size_t total_memory)
{
    if (num_threads <= 0)
        return NULL;

    // Allocate array for storing memory allocations per thread
    size_t *allocations = malloc(num_threads * sizeof(size_t));
    if (allocations == NULL)
    {
        fprintf(stderr, "Memory allocation failed.\n");
        return NULL;
    }

    // Base allocation for each thread
    size_t base_allocation = total_memory / num_threads;

    // Assign base allocation initially
    for (int i = 0; i < num_threads; i++)
    {
        allocations[i] = base_allocation;
    }

    // Calculate redistribution from the first half to the second half of the threads
    int half = num_threads / 2;
    for (int i = 0; i < half; i++)
    {
        size_t fraction = allocations[i] * (half - i) / half;
        allocations[i] -= fraction;
        if (allocations[i] <= 0)
        {
            allocations[i] = 256;
        }
        allocations[num_threads - i - 1] += fraction;
    }

    return allocations;
}
/*
    This is a generic test function that can be used to test any function from the single-threaded test cases in a multithreading context.
    The function takes a pointer to the test function, the number of threads to create, the size of the memory pool, and the name of the function being tested (used for printing purposes only).
*/

void testAcrossConfigurations(void (*test_func)(TestParams), TestParams params)
{
    int num_threads[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
    size_t *mem_sizes;
    int *repetitions;

    int count = 1;
    int rcount = 1;

    // If mem_size is 0, we run with single memory size, ie. the function requires a fixes size mem_size
    if (params.memory_size > 0)
    {
        mem_sizes = malloc(sizeof(size_t));
        mem_sizes[0] = params.memory_size;
    }
    else
    {
        mem_sizes = malloc(4 * sizeof(size_t));
        mem_sizes[0] = 1024;
        mem_sizes[1] = 2048;
        mem_sizes[2] = 4096;
        mem_sizes[3] = 8192;
        count = 4;
    }

    // If iterations is -1 (i.e. not set), we run with single iteration.
    if (params.iterations > 0)
    {
        repetitions = malloc(4 * sizeof(int));
        repetitions[0] = 10;
        repetitions[1] = 100;
        repetitions[2] = 500;
        repetitions[3] = 1000;
        rcount = 4;
    }
    else
    {
        repetitions = malloc(sizeof(int));
        repetitions[0] = 1;
    }

    // Run the test function for all combinations of num_threads, mem_sizes, and repetitions
    for (int i = 0; i < sizeof(num_threads) / sizeof(num_threads[0]); i++)
    {
        for (int j = 0; j < count; j++)
        {
            for (int z = 0; z < rcount; z++)
            {
                params.num_threads = num_threads[i];
                params.memory_size = mem_sizes[j];
                params.iterations = repetitions[z];
                test_func(params);
            }
        }
    }

    free(mem_sizes);
    free(repetitions);
}

void run_concurrent_test(void *(*test_func)(void *), TestParams params, char *function_name)
{
    printf_yellow("  Testing \"%s\" (threads: %d, mem_size: %zu) ---> ", function_name, params.num_threads, params.memory_size);
    mem_init(params.memory_size);
    pthread_t threads[params.num_threads];
    my_barrier_init(&barrier, params.num_threads);
    thread_data_t params_t[params.num_threads];

    // Create threads to run the test function concurrently
    for (int i = 0; i < params.num_threads; i++)
    {
        params_t[i].thread_id = i;
        params_t[i].block_size = params.memory_size / params.num_threads;
        int rc = pthread_create(&threads[i], NULL, (void *(*)(void *))test_func, &params_t[i]);
        my_assert(rc == 0); // Ensure thread creation was successful
    }

    // Wait for all threads to complete
    for (int i = 0; i < params.num_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }
    mem_deinit();
    my_barrier_destroy(&barrier);
    printf_green("[PASS].\n");
}

void sanityCheck(size_t size, char *block, char expected_value)
{
    if (block == NULL)
        return;

    for (int i = 0; i < size; ++i)
    {
        my_assert(block[i] == expected_value);
    }
}

void *test_alloc_and_free(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;

    // Allocate and fill two blocks of memory with unique patterns
    size_t block1_size = data->block_size / 4;
    char *block1 = (char *)mem_alloc(block1_size);
    my_assert(block1 != NULL);
    memset(block1, data->thread_id, block1_size); // Unique pattern using thread_id

    size_t block2_size = block1_size * 3; // Corrected from undefined block2_size variable
    char *block2 = (char *)mem_alloc(block2_size);
    my_assert(block2 != NULL);
    memset(block2, data->thread_id + block1_size, block2_size); // Another unique pattern offset by 100

    my_barrier_wait(&barrier); // Assuming barrier is defined somewhere globally

    // Check integrity of the data before freeing
    sanityCheck(block1_size, block1, data->thread_id);
    sanityCheck(block2_size, block2, data->thread_id + block1_size);

    mem_free(block1);
    mem_free(block2);

    return NULL;
}

void *test_zero_alloc_and_free(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;

    void *block1 = mem_alloc(0);
    my_assert(block1 != NULL);
    void *block2 = mem_alloc(200);
    my_assert(block2 != NULL);

    memset(block2, data->thread_id, 200); // Unique pattern using thread_id

    my_barrier_wait(&barrier);

    sanityCheck(200, block2, data->thread_id);

    mem_free(block1);
    mem_free(block2);

    return NULL;
}

/*
 * This function is used to test the allocation of random blocks of memory and then freeing them in a multithreading context.
 * The test passes if all allocations and deallocations are successful.
 */

void *thread_alloc_free(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;
    int block_size;

    // Allocation phase
    for (int i = 0; i < data->num_blocks; i++)
    {
        block_size = rand() % data->max_block_size;
        data->block_pointers[i] = mem_alloc(block_size);
        my_assert(data->block_pointers[i] != NULL); // Make sure the allocation was successful
    }

    // De-allocation phase
    for (int i = 0; i < data->num_blocks; i++)
    {
        mem_free(data->block_pointers[i]);
    }

    return NULL;
}

void test_random_blocks_multithread(TestParams params)
{
    printf_yellow("  Testing \"mem_alloc\" and mem_free for random blocks (threads: %d, max_block_size: %zu) ---> ", params.num_threads, params.block_size);
    srand(time(NULL)); // Initialize random seed

    int total_blocks = 1000 + rand() % 10000;
    int mem_size = total_blocks * params.block_size;

    mem_init(mem_size);

    pthread_t threads[params.num_threads];
    thread_data_t thread_data[params.num_threads];
    void *block_pointers[total_blocks]; // Array to hold pointers to allocated blocks

    // Prepare thread data
    for (int i = 0; i < params.num_threads; i++)
    {
        thread_data[i].num_blocks = total_blocks / params.num_threads;
        thread_data[i].max_block_size = params.block_size;
        thread_data[i].block_pointers = &block_pointers[i * thread_data[i].num_blocks];
    }

    // Launch threads
    for (int i = 0; i < params.num_threads; i++)
    {
        pthread_create(&threads[i], NULL, thread_alloc_free, &thread_data[i]);
    }

    // Wait for all threads to finish
    for (int i = 0; i < params.num_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    mem_deinit(); // Clean up memory manager after all operations
    printf_green("[PASS].\n");
}

/*
 * This function is used to test the resizing of memory blocks in a multithreading context.
 * Each thread will allocate a block of memory, resize it, and then free it.
 * The test passes if all threads complete successfully.
 */
void *thread_resize(void *arg)
{
    size_t initial_size = (size_t)arg;
    size_t new_size = initial_size * 2; // Example: double the initial size

    void *block = mem_alloc(initial_size);
    if (block == NULL)
    {
        printf_red("Failed to allocate initial block of size %zu\n", initial_size);
        return (void *)1;
    }

    void *resized_block = mem_resize(block, new_size);
    if (resized_block == NULL)
    {
        printf_red("Failed to resize block from %zu to %zu bytes\n", initial_size, new_size);
        return (void *)1;
    }

    // Optionally, verify the resized block
    memset(resized_block, 0xAA, new_size); // Use the resized memory

    mem_free(resized_block); // Free the resized memory block
    return (void *)0;
}

void test_resize_multithread(TestParams params)
{
    printf_yellow("  Testing \"mem_resize\" (threads: %d) ---> ", params.num_threads);

    pthread_t threads[params.num_threads];
    size_t initial_size = 100; // Each thread starts with 100 bytes

    mem_init(1024 * params.num_threads); // Initialize enough memory for all threads to work comfortably

    // Launch threads to perform the resize operation
    for (int i = 0; i < params.num_threads; i++)
    {
        if (pthread_create(&threads[i], NULL, thread_resize, (void *)initial_size) != 0)
        {
            perror("Failed to create thread");
            exit(EXIT_FAILURE);
        }
    }

    // Wait for all threads to finish
    int failures = 0;
    void *status;
    for (int i = 0; i < params.num_threads; i++)
    {
        pthread_join(threads[i], &status);
        if ((long)status != 0)
        {
            failures++;
        }
    }

    mem_deinit(); // Clean up the memory manager

    if (failures == 0)
    {
        printf_green("[PASS]\n");
    }
    else
    {
        printf_red("[FAIL]: Some resize operations failed.\n");
    }
}

void *alloc_exceeding_memory(void *arg)
{
    size_t size_to_allocate = (size_t)arg;
    void *block = mem_alloc(size_to_allocate);
    if (block != NULL)
    {
        printf_red("Allocation should have failed but succeeded\n");
        return (void *)1; // Return error
    }
    return (void *)0; // Return success
}

void test_exceed_single_allocation_multithread(TestParams params)
{
    printf_yellow("  Testing \"allocation exceeding pool size\" (threads: %d) ---> ", params.num_threads);

    pthread_t threads[params.num_threads];
    size_t size_to_allocate = 2048; // Each thread will try to allocate 2KB

    mem_init(1024); // Initialize with 1KB of memory, intentionally less than required per thread

    // Create threads that will each try to allocate more memory than available
    for (int i = 0; i < params.num_threads; i++)
    {
        if (pthread_create(&threads[i], NULL, alloc_exceeding_memory, (void *)size_to_allocate) != 0)
        {
            perror("Failed to create thread");
            exit(EXIT_FAILURE);
        }
    }

    // Join threads and check results
    int fail_count = 0;
    void *status;
    for (int i = 0; i < params.num_threads; i++)
    {
        pthread_join(threads[i], &status);
        if ((long)status != 0)
        {
            fail_count++;
        }
    }

    mem_deinit(); // Clean up the memory manager

    if (fail_count == 0)
    {
        printf_green("[PASS].\n");
    }
    else
    {
        printf_red("[FAIL]: Some threads incorrectly succeeded in allocation.\n");
    }
}

void *cumulative_alloc(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;

    size_t block_size = data->block_size / 128;
    char **blocks = (char **)malloc(128 * sizeof(char *));
    intptr_t returnval = 0;
    for (int i = 0; i < 128; i++)
    {
        blocks[i] = mem_alloc(block_size);
        if (blocks[i] == NULL)
        {
            // if (debug)
            printf_yellow("    Allocation failed as expected for size %zu in thread %d\n", block_size, data->thread_id);
            returnval = 1; // Expected failure
            break;
        }
        // Optional: simulate some usage
        // memset(blocks[i], 0xAA, block_size);
    }
    my_barrier_wait(&barrier);
    for (int i = 0; i < 128; i++)
        mem_free(blocks[i]);
    free(blocks);

    return (void *)returnval; // Unexpected success
}
void test_exceed_cumulative_allocation_multithread(TestParams params)
{
    printf_yellow("  Testing \"cumulative allocations exceeding pool size\" (threads: %d, mem_size: %zu) ---> \n", params.num_threads, params.memory_size);

    size_t *sizes = calculate_thread_allocations(params.num_threads, params.memory_size + params.num_threads / 2);

    my_barrier_init(&barrier, params.num_threads);

    pthread_t threads[params.num_threads];

    thread_data_t thread_data[params.num_threads];
    mem_init(params.memory_size); // Initialize with 1KB of memory

    // Create threads that will attempt to allocate memory
    for (int i = 0; i < params.num_threads; i++)
    {
        thread_data[i].thread_id = i;
        thread_data[i].block_size = sizes[i];

        if (pthread_create(&threads[i], NULL, cumulative_alloc, &thread_data[i]))
        {
            perror("Failed to create thread");
            exit(EXIT_FAILURE);
        }
    }

    // Join threads and check results
    int pass_count = 0;
    void *status;
    for (int i = 0; i < params.num_threads; i++)
    {
        pthread_join(threads[i], &status);
        if ((long)status == 1)
        { // Expected failure
            pass_count++;
        }
    }
    free(sizes);
    mem_deinit();                 // Clean up the memory manager
    my_barrier_destroy(&barrier); // Destroy the barrier
    if (pass_count >= 1)
    { // At least one thread failed to allocate beyond the limit as expected
        printf_green("[PASS]: At least one thread failed to allocate beyond the limit as expected.\n");
    }
    else
    {
        printf_red("[FAIL]: All allocations succeeded, but should not have.\n");
    }
}

/*
 * This function is used to test the allocation of memory beyond the total available memory pool.
 * The test passes if all allocations fail as expected.
 */

void *thread_overcommit_alloc(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;

    // Initial allocation attempt
    void *initial_block = mem_alloc(data->block_size);
    if (initial_block == NULL)
    {
        if (debug)
            printf_red("    Thread %d failed to allocate initial %zu bytes\n", data->thread_id, data->block_size);
        return (void *)1; // Indicate failure in initial allocation
    }
    if (debug)
        printf_yellow("    Thread %d successfully allocated %zu bytes initially\n", data->thread_id, data->block_size);

    // Synchronize all threads here
    my_barrier_wait(&barrier);

    // Attempt to allocate additional memory, which is expected to fail
    void *extra_block = mem_alloc(100); // Small extra amount intended to fail
    if (extra_block != NULL)
    {
        if (debug)
            printf_red("    Thread %d unexpectedly succeeded in allocating extra memory\n", data->thread_id);
        mem_free(extra_block); // Cleanup if allocation was unexpectedly successful
        my_barrier_wait(&barrier);
        mem_free(initial_block); // Clean up initial block
        return (void *)1;        // Unexpected success in overcommit scenario
    }

    // Second synchronization after allocation attempt
    my_barrier_wait(&barrier);

    // Cleanup and confirm expected behavior
    if (debug)
        printf_yellow("    Thread %d correctly failed to allocate extra memory as expected\n", data->thread_id);
    mem_free(initial_block); // Clean up initial block
    return (void *)0;        // Expected behavior confirmed
}

void test_memory_overcommit_multithread(TestParams params)
{
    printf_yellow("  Testing \"memory overcommitment\" (threads: %d, mem_size: %zu) ---> ", params.num_threads, params.memory_size);

    pthread_t threads[params.num_threads];
    thread_data_t params_t[params.num_threads];

    my_barrier_init(&barrier, params.num_threads); // Initialize the barrier

    size_t memory_per_thread = params.memory_size / params.num_threads; // Each thread tries to allocate 1KB
    mem_init(params.memory_size);                                       // Initialize with 1KB of memory, intentionally less than required per thread

    // Setup thread parameters and create threads
    for (int i = 0; i < params.num_threads; i++)
    {
        params_t[i].thread_id = i;
        params_t[i].block_size = memory_per_thread;
        if (pthread_create(&threads[i], NULL, thread_overcommit_alloc, (void *)&params_t[i]) != 0)
        {
            perror("Failed to create thread");
            exit(EXIT_FAILURE);
        }
    }

    // Wait for all threads to finish and gather results
    int failures = 0;
    void *status;
    for (int i = 0; i < params.num_threads; i++)
    {
        pthread_join(threads[i], &status);
        if ((long)status == 1)
        { // Check for expected failures
            failures++;
        }
    }

    mem_deinit();                 // Clean up the memory manager
    my_barrier_destroy(&barrier); // Destroy the barrier

    if (failures == 0)
    {
        printf_green("[PASS].\n");
    }
    else
    {
        printf_red("[FAIL]: Some threads unexpectedly succeeded in allocating memory beyond the limit.\n");
    }
}

void *thread_repeated_fit_reuse(void *arg)
{
    thread_data_t *params = (thread_data_t *)arg;
    size_t size = params->block_size;
    int iterations = params->iterations;
    void *block = NULL;

    for (int i = 0; i < iterations; i++)
    {
        block = mem_alloc(size);
        if (block == NULL)
        {
            if (debug)
                printf_red("    Thread %ld failed to allocate block of %zu bytes on iteration %d\n", (long)pthread_self(), size, i);
            return (void *)1;
        }

        mem_free(block);
    }

    return (void *)0;
}

void test_repeated_fit_reuse_multithread(TestParams params)
{
    //  int iterations, int num_threads, int mem_size, int num_blocks
    printf_yellow("  Testing \"repeated exact fit reuse\" (num_threads: %d, memory_size: %zu, repeat: %d) ---> ", params.num_threads, params.memory_size, params.iterations);

    pthread_t threads[params.num_threads];
    thread_data_t params_t[params.num_threads];
    size_t block_size = params.memory_size / params.num_threads; // Size of each memory block

    mem_init(params.memory_size); // Initialize with 1KB of memory, enough for all threads if they reuse properly

    // Prepare parameters for each thread
    for (int i = 0; i < params.num_threads; i++)
    {
        params_t[i].block_size = block_size;
        params_t[i].iterations = params.iterations;
    }

    // Launch threads to perform the repeated reuse test
    for (int i = 0; i < params.num_threads; i++)
    {
        if (pthread_create(&threads[i], NULL, thread_repeated_fit_reuse, &params_t[i]) != 0)
        {
            perror("Failed to create thread");
            exit(EXIT_FAILURE);
        }
    }

    // Wait for all threads to finish and collect results
    int failures = 0;
    void *status;
    for (int i = 0; i < params.num_threads; i++)
    {
        pthread_join(threads[i], &status);
        if ((long)status != 0)
        {
            failures++;
        }
    }

    mem_deinit(); // Clean up the memory manager

    if (failures == 0)
    {
        printf_green("[PASS].\n");
    }
    else
    {
        printf_red("[FAIL]: Some threads failed to consistently reuse blocks.\n");
    }
}

void *repeated_allocate_and_free(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;
    int cycles = data->iterations; // Use iterations from thread data for number of cycles

    for (int i = 0; i < cycles; i++)
    {
        void *block = mem_alloc(data->block_size); // Allocate using the block_size from thread data
        if (block == NULL)
        {
            if (debug)
                printf_red("    Thread %d failed to allocate %zu bytes\n", data->thread_id, data->block_size);
            continue; // Skip freeing and proceed to the next cycle
        }
        if (debug)
            printf_yellow("    Thread %d allocated and will now free %zu bytes\n", data->thread_id, data->block_size);
        my_barrier_wait(&barrier); // Synchronize after allocation

        mem_free(block); // Free the block to create fragmentation

        my_barrier_wait(&barrier); // Synchronize after free
    }

    return NULL;
}

// Function for threads to attempt allocation in the fragmented memory
void *repeated_allocate_in_fragment(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;
    int cycles = data->iterations; // Number of allocation attempts per thread

    for (int i = 0; i < cycles; i++)
    {
        my_barrier_wait(&barrier); // Wait until fragmentation is created

        // Attempt to allocate in the fragmented space
        void *block = mem_alloc(data->block_size);
        if (block != NULL)
        {
            if (debug)
                printf_yellow("    Thread %d allocated %zu bytes in fragmented memory\n", data->thread_id, data->block_size);
            mem_free(block);
        }
        else
        {
            if (debug)
                printf_red("    Thread %d failed to allocate %zu bytes in fragmented memory\n", data->thread_id, data->block_size);
        }

        my_barrier_wait(&barrier); // Synchronize before the next cycle
    }

    return NULL;
}

void test_memory_fragmentation_multithread(TestParams params)
{
    printf_yellow("  Testing \"memory fragmentation handling\" (threads: %d, mem_size: %zu, iterations: %d) ---> ", params.num_threads, params.memory_size, params.iterations);
    mem_init(params.memory_size); // Initialize with specified memory size to accommodate load

    pthread_t threads[params.num_threads];
    thread_data_t params_t[params.num_threads]; // Array of thread data

    my_barrier_init(&barrier, params.num_threads); // Initialize the barrier

    // Dynamically calculate block size based on available memory and the number of threads
    size_t base_block_size = params.memory_size / (params.num_threads * 3); // Adjust factor if necessary to prevent over-allocation

    // Setup thread parameters
    for (int i = 0; i < params.num_threads; i++)
    {
        params_t[i].thread_id = i;
        params_t[i].block_size = base_block_size * (i % 3 + 1); // Multiplicative factor to vary block size: 1x, 2x, 3x
        params_t[i].iterations = params.iterations;             // Set the number of allocation-free cycles

        if (i % 2 == 0)
        {
            // Even-indexed threads create fragmentation
            pthread_create(&threads[i], NULL, repeated_allocate_and_free, &params_t[i]);
        }
        else
        {
            // Odd-indexed threads attempt to fill fragmented spaces
            pthread_create(&threads[i], NULL, repeated_allocate_in_fragment, &params_t[i]);
        }
    }

    // Wait for all threads to finish
    for (int i = 0; i < params.num_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    mem_deinit(); // Clean up the memory manager
    my_barrier_destroy(&barrier);
    printf_green("[PASS].\n");
}

void *thread_function(void *arg)
{
    thread_data_t *params = (thread_data_t *)arg;
    int thread_id = params->thread_id;
    int num_allocations = params->num_blocks;
    size_t block_size = params->block_size;

    char **blocks = (char **)malloc(num_allocations * sizeof(char *));
    my_assert(blocks != NULL); // Check that allocation was successful

    for (int i = 0; i < num_allocations; i++)
    {
        // Allocate memory
        blocks[i] = (char *)mem_alloc(block_size);
        my_assert(blocks[i] != NULL); // Check allocation was successful
        // printf("Thread %d: Allocated block %d at %p = %d\n", thread_id, i, blocks[i], thread_id * num_allocations + i);
        // Write a unique pattern based on thread_id and index i
        memset(blocks[i], thread_id * num_allocations + i, block_size);

        // Optionally, simulate some work or delay
        if (params->simulate_work)
            usleep(rand() % 1000);
    }

    // my_barrier_wait(&barrier);

    for (int i = 0; i < num_allocations; i++)
    {
        sanityCheck(block_size, blocks[i], (char)(thread_id * num_allocations + i));

        // Free memory
        mem_free(blocks[i]);
    }
    // Free the dynamically allocated array of pointers
    free(blocks);

    return NULL;
}

void run_concurrency_test(TestParams params)
{
    printf_yellow("  Running concurrency test with %d threads, %d allocations per thread, and block size %zu bytes --> ", params.num_threads, params.num_blocks / params.num_threads, params.block_size);
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL); // Start timing
    pthread_t threads[params.num_threads];
    thread_data_t params_t[params.num_threads];
    my_barrier_init(&barrier, params.num_threads);
    // Initialize your memory manager here
    mem_init(params.num_blocks * params.block_size); // Initialize with enough memory for the test

    // Create multiple threads to perform memory operations
    for (int i = 0; i < params.num_threads; i++)
    {
        params_t[i].thread_id = i;
        params_t[i].num_blocks = params.num_blocks / params.num_threads;
        params_t[i].block_size = params.block_size;
        params_t[i].simulate_work = params.simulate_work;
        pthread_create(&threads[i], NULL, thread_function, &params_t[i]);
    }

    // Join all threads
    for (int i = 0; i < params.num_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // Clean up the memory manager here if needed
    mem_deinit();

    gettimeofday(&end_time, NULL); // End timing
    my_barrier_destroy(&barrier);  // Destroy the barrier
    // Calculate elapsed time
    long seconds = end_time.tv_sec - start_time.tv_sec;
    long micros = ((seconds * 1000000) + end_time.tv_usec) - (start_time.tv_usec);
    printf_yellow("Time: %ld microseconds.\t", micros);

    printf_green("[PASS].\n");
}

/* repeated from A1, as there were solutions that has issues */

void test_looking_for_out_of_bounds()
{
    printf("  Testing outofbounds (errors not tracked/detected here) \n");

    printf("ALLOCATION 5000\n");
    mem_init(5000); // Initialize with 1024 bytes
    printf("ALLOCATED 5000\n");
    void *block0 = mem_alloc(512); // Edge case: zero allocation
    assert(block0 != NULL);        // Depending on handling, this could also be NULL

    void *block1 = mem_alloc(512); // 0-1024
    assert(block1 != NULL);

    void *block2 = mem_alloc(1024); // 1024-2048
    assert(block2 != NULL);

    void *block3 = mem_alloc(2048); // 2048-4096
    assert(block3 != NULL);

    void *block4 = mem_alloc(904); // 4096-5000
    assert(block4 != NULL);

    printf("BLOCK0; %p, 512\n", block0);
    printf("BLOCK1; %p, 512\n", block1);
    printf("BLOCK2; %p, 1024\n", block2);
    printf("BLOCK3; %p, 2048\n", block3);
    printf("BLOCK4; %p, 904\n", block4);

    mem_free(block0);
    mem_free(block1);
    mem_deinit();
    printf("[PASS].\n");
}

int main(int argc, char *argv[])
{
#ifdef VERSION
    printf("Build Version; %s \n", VERSION);
#endif
    printf("Git Version; %s/%s \n", git_date, git_sha);

    if (argc < 2)
    {
        printf("Usage: %s <test function>\n", argv[0]);
        printf("Available test functions:\n");

        printf("  0. tests various functions with a base number of threads\n");
        printf("  1. tests various functions across variious configurations (number of threads, memory sizes,  iterations)\n");
        printf("  2. stress tests various functions with various configurations. This may take some time (especially if simulate_work flag is set to true.\n");
        printf("  3. test_looking_for_out_of_bounds, needs LD_PRELOAD=./libmymalloc.so .\n\n");
        return 1;
    }

    // used for case 19
    int base_num_threads = 4;
    int allocs;
    size_t blockSize;
    bool simulate_work = false; // set this to true to see the benefits of multithreading

    switch (atoi(argv[1]))
    {
    case -1:
        printf("No tests will be executed.\n");
        break;
    case 0:
        // Running all tests with a base number of threads
        printf("\n*** Testing various functions with a base number of threads: ***\n");
        run_concurrent_test(test_alloc_and_free, (TestParams){.num_threads = base_num_threads, .memory_size = 1024}, "mem_alloc and mem_free");
        run_concurrent_test(test_zero_alloc_and_free, (TestParams){.num_threads = base_num_threads, .memory_size = 1024}, "zero alloc and free");

        test_resize_multithread((TestParams){.num_threads = base_num_threads});

        test_exceed_single_allocation_multithread((TestParams){.num_threads = base_num_threads});
        test_exceed_cumulative_allocation_multithread((TestParams){.num_threads = base_num_threads, .memory_size = 1024}); // TODO: Fix this to be able to run with various configurations

        test_memory_overcommit_multithread((TestParams){.num_threads = base_num_threads, .memory_size = 1024});

        for (int i = 0; i < 4; i++)
            test_repeated_fit_reuse_multithread((TestParams){.num_threads = base_num_threads, .memory_size = 1024, .iterations = pow(10, i)});

        test_memory_fragmentation_multithread((TestParams){.num_threads = base_num_threads, .memory_size = 2048});
        test_random_blocks_multithread((TestParams){.num_threads = base_num_threads, .block_size = 1024});

        break;

    case 1:
        printf("\n*** Testing various functions across variious configurations (number of threads, memory sizes,  iterations): ***\n");
        testAcrossConfigurations(test_resize_multithread, (TestParams){.memory_size = 1024});
        testAcrossConfigurations(test_exceed_single_allocation_multithread, (TestParams){.memory_size = 1024});

        for (int i = 1; i < 6; i++)
            test_exceed_cumulative_allocation_multithread((TestParams){.num_threads = pow(2, i), .memory_size = pow(2, 11 + i)});
        break;
        testAcrossConfigurations(test_memory_overcommit_multithread, (TestParams){.memory_size = 1024});
        testAcrossConfigurations(test_repeated_fit_reuse_multithread, (TestParams){.iterations = 1});
        testAcrossConfigurations(test_memory_fragmentation_multithread, (TestParams){.iterations = 1});
        testAcrossConfigurations(test_random_blocks_multithread, (TestParams){.memory_size = 1024, .block_size = 1024});

        break;
    case 2:
        printf("\n*** Scalability testing: ***\n");

        printf("Testing mem_alloc and mem_free\n");

        for (int i = 1; i < 4; i++)
        {
            for (int j = 1; j < 5; j++)
            {
                run_concurrent_test(test_alloc_and_free, (TestParams){.num_threads = i, .memory_size = pow(2, 9 + j)}, "mem_alloc and mem_free");
            }
        }

        printf("Testing random blocks\n");
        for (int i = 2; i < 6; i++)
        {
            test_random_blocks_multithread((TestParams){.num_threads = pow(2, i), .block_size = 1024});
        }

        allocs = (int)pow(2, 15);
        blockSize = (int)pow(2, 7);
        // run_concurrency_test(1, 3, 100);

        printf("Testing large number of blocks of fixed size\n");
        for (int i = 0; i < 9; i++)
            run_concurrency_test((TestParams){.num_threads = pow(2, i), .num_blocks = allocs, .block_size = blockSize, .simulate_work = simulate_work});
        break;

    case 3:
        printf("Test 3.\n");
        test_looking_for_out_of_bounds();
        break;

    default:
        printf("Invalid test function\n");
        break;
    }
    return 0;
}
