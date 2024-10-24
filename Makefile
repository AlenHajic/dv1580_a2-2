# Compiler and Linking Variables
CC = gcc
CFLAGS = -Wall -fPIC
LIB_NAME = libmemory_manager.so

# Source and Object Files
SRC = memory_manager.c
OBJ = $(SRC:.c=.o)
LIST_SRC = linked_list.c
LIST_OBJ = $(LIST_SRC:.c=.o)

# Default target
all: mmanager list test_mmanager test_list

# Rule to create the dynamic library
$(LIB_NAME): $(OBJ)
	$(CC) -shared -o $@ $(OBJ)

# Rule to compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build the memory manager
mmanager: $(LIB_NAME)

# Build the linked list
list: $(LIST_OBJ)

# Test target to run the memory manager test program
test_mmanager: $(LIB_NAME)
	$(CC) -o test_memory_manager test_memory_manager.c -L. -lmemory_manager

test_list: $(LIB_NAME) linked_list.o
    $(CC) $(CFLAGS) -o test_linked_list linked_list.c test_linked_list.c -L. -lmemory_manager $(LDFLAGS)
    cp test_linked_list test_linked_listCG

# Run tests
run_tests: run_test_mmanager run_test_list

# Run test cases for the memory manager
run_test_mmanager:
	./test_memory_manager

# Run test cases for the linked list
run_test_list:
	./test_linked_list

# Clean target to clean up build files
clean:
	rm -f $(OBJ) $(LIST_OBJ) $(LIB_NAME) test_memory_manager test_linked_list
