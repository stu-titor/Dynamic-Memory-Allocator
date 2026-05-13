/**************************************************************************
 * C S 429 MM-lab
 *
 * bump_test.c - checks for bump allocators and implicit free lists
 *
 * Copyright (c) 2025 A. Lieou All rights reserved.
 * May not be used, modified, or copied without permission.
 **************************************************************************/

#include "bump_test.h"

/*
 * get_size - gets the size of the block.
 */
size_t get_size_blk(mem_block_header_t *block) {
    assert(block != NULL);
    return block->block_metadata & ~(15);
}

/**
 * Calculates the total size of the heap.
 */
size_t total_heap() {
    size_t total = 0;
    for (int i = 0; i < BIN_COUNT; ++i) {
        mem_block_header_t *bin = free_heads[i];
        do {
            if (!bin)
                break;
            total += get_size_blk(bin);
            bin = bin->next;
        } while (bin != NULL && (circular ? bin != free_heads[i] : 1));
    }
    return total;
}

/**
 * Looks for an address on the free list (does not have to be the start
 * or end of a block; but it must exist somewhere within a block).
 */
bool find_address(mem_block_header_t *address) {
    for (int i = 0; i < BIN_COUNT; ++i) {
        mem_block_header_t *bin = free_heads[i];
        do {
            if (!bin)
                break;
            char *start = (char *)bin;
            char *end = start + get_size_blk(bin) + (sizeof(mem_block_header_t) * 2);
            if ((unsigned long)start <= (unsigned long)address && (unsigned long)address <= (unsigned long)end)
                return true;
            bin = bin->next;
        } while (bin != NULL && (circular ? bin != free_heads[i] : 1));
    }
    return false;
}

/**
 * For student debugging, print out the contents of all the bins.
 */
void print_all() {
    printf("\nPrinting Free List\n");
    for (int i = 0; i < BIN_COUNT; ++i) {
        mem_block_header_t *bin = free_heads[i];
        if (bin)
            printf("Bin %d\n", i);
        do {
            if (!bin)
                break;
            printf("\taddr: %p, metadata: %ld", bin, bin->block_metadata);
            if (bin->block_metadata & 0x1)
                printf(" -- ALLOCATED BLOCK\n");
            else
                printf("\n");
            bin = bin->next;
        } while (bin != NULL && (circular ? bin != free_heads[i] : 1));
    }
    printf("\n");
}

/**
 * Tests that all blocks on the free list are free.
 */
int test_implicit() {
    for (int i = 0; i < BIN_COUNT; ++i) {
        mem_block_header_t *bin = free_heads[i];
        do {
            if (!bin)
                break;
            if (bin->block_metadata & 0x1) {
                printf("ERROR: implicit free list detected!");
                printf(" Found an allocated block!\n");
                print_all();
                return -1;
            }
            bin = bin->next;
        } while (bin != NULL && (circular ? bin != free_heads[i] : 1));
    }
    return 0;
}

/**
 * Checks for bump allocation: blocks must be added back to the free list
 * and umalloc must try to reuse blocks before extending.
 */
int test_bump() {
    /**
     * Test 1: are blocks being added back to the free list?
     *
     * allocate a block, make sure the block got removed, free that block,
     * and then make sure that pointer is somewhere in the free list
     */
    for (unsigned i = 1; i <= 10; ++i) {
        mem_block_header_t *returned = umalloc(PAGESIZE / 2 * i);
        if (test_implicit()) {
            return -1;
        }

        ufree(returned);

        if (!find_address(returned)) {
            printf("ERROR: bump allocation detected!");
            printf(" Failed to find: %p\n", returned);
            print_all();
            return -1;
        }
    }

    /**
     * Test 2: are blocks being reused?
     *
     * allocate a block and free that block.
     *
     * then, check the total heap size and allocate again (same size). if the
     * total heap size doesn't decrease, you are not reusing blocks.
     */
    mem_block_header_t *returned = umalloc(64);
    if (test_implicit()) {
        return -1;
    }
    ufree(returned);

    size_t before = total_heap();
    assert(before >= 64);

    returned = umalloc(64);
    size_t after = total_heap();

    if (test_implicit()) {
        return -1;
    }
    ufree(returned);

    // There must be enough memory on the heap for the request.
    // Therefore, the heap size must decrease.
    if (after >= before) {
        printf("ERROR: bump allocation detected!");
        printf(" Failed to reuse available block!\n");
        printf("Total Heap Before: %ld, Total Heap After: %ld\n", before, after);
        return -1;
    }

    return 0;
}

/**
 * Runs the implicit free list and bump allocator tests.
 */
static int test() {
    uinit();
    if (test_implicit()) {
        return -1;
    }
    if (test_bump()) {
        return -1;
    }
    // if (test_implicit()) {
    //     return -1;
    // }
    return 0;
}

int main(int argc, char **argv) {
    if (test()) {
        printf("Tests Failed.\n");
        return -1;
    } else {
        printf("Tests Passed!\n");
        return 0;
    }
}