/**************************************************************************
 * C S 429 MM-lab
 *
 * coalesce_check.c - checks for a deferred coalesce implementation
 **************************************************************************/

#include "coalesce_check.h"
static int in_umalloc_context = 0;
static int in_ufree_context = 0;
static int in_uinit = 0;
static int num_deferred_coalesces = 0;
static int num_immediate_coalesces = 0;
static bool called_uinit = false;


void* __wrap_umalloc(size_t size) {
    in_umalloc_context++;
    void* res = __real_umalloc(size);
    in_umalloc_context--;
    return res;
}

void __wrap_ufree(void *ptr) {
    in_ufree_context++;
    __real_ufree(ptr);
    in_ufree_context--;
}

void record_coalesce() {
    if (in_umalloc_context > 0) {
        num_deferred_coalesces++;
    } else if (in_ufree_context > 0) {
        num_immediate_coalesces++;
    }
}

void reset_coalesce_globals() {
    in_umalloc_context = 0;
    in_ufree_context = 0;
    in_uinit = 0;
    num_deferred_coalesces = 0;
    num_immediate_coalesces = 0;
    called_uinit = false;
}



size_t add_padding(size_t size) {
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

/*
 * get_size - gets the size of the block.
 */
size_t get_size_block(mem_block_header_t *block) {
    assert(block != NULL);
    return block->block_metadata & ~(ALIGNMENT-1);
}

int test_coalesce() {
    uinit();
    // Smallest padded block in the biggest bin
    // size_t small_block_size = add_padding(bin_limits[BIN_COUNT - 2] + 16);
    size_t small_block_size = 16;
    // Larger padded block that will be in the biggest bin
    size_t large_block_size = add_padding(160000);
    // size_t large_block_size = 2 * small_block_size;
    // if you need to allocate a bigger sized block
    // (arbitrary setting to double the size)
    coalesce_container_struct* head = calloc(1, sizeof(coalesce_container_struct));
    
    coalesce_container_struct* curr = head;
    int num_elems = get_bin_size(BIN_COUNT - 1);

    // do {
    // arbitrarily do 100,000 allocations of 16 byte blocks
    for (int i = 0; i < 100000; i++) {
        // Do a single malloc to force lazy loading to extend initially
        // If solution doesn't implement lazy loading, it should be fine
        if (num_elems == 1) {
            if (get_size_block(free_heads[BIN_COUNT - 1]) <= small_block_size) {
                break;
            }
        }

        // We need to loop through all of the elements to check that we cannot
        // fit a small_block_sized block into the bin, and break if that's the
        // case

        // allocate small sized blocks in largest bin
        // to the point where you need to extend or coalesce
        void* ptr = umalloc(small_block_size);
        curr->ptr = ptr;
        if (i < 99999) { 
            curr->next = calloc(1, sizeof(coalesce_container_struct));
            curr = curr->next;
        }
        
        num_elems = get_bin_size(BIN_COUNT - 1);
    }

    // free all those small sized blocks
    
    curr = head;
    while (head) {
        if (head->ptr != NULL) {
            ufree(head->ptr);
        }
        head = head->next;
        free(curr);       
        curr = head; 
    }

    // count the number of free blocks in the entire heap
    int initial_num_elems = 0;
    for (int i = 0; i < BIN_COUNT; i++) {
        initial_num_elems += get_bin_size(i);
    }

    // allocate and free large sized block, that should force a coalesce
    void* large_ptr = umalloc(large_block_size);
    ufree(large_ptr);

    // Count total number of free blocks in the entire heap
    int final_num_elems = 0;
    for (int i = 0; i < BIN_COUNT; i++) {
        final_num_elems += get_bin_size(i);
    }

    // if the final number of free blocks in entire heap is
    // less than the initial number of free blocks in the entire
    // heap, then we have coalesced properly
    printf("Deferred Coalesce Test\n\n");
    if (initial_num_elems < 100000) {
        printf("[FAILED] - get_bin_size() Check\n");
        printf("Initial Total # of Free Blocks should be greater or equal to 100,000\n");
    } else if (final_num_elems < initial_num_elems)
        printf("[PASSED] - Deferred Coalesce Check & get_bin_size() Check\n");
    else {
        printf("[FAILED] - Deferred Coalesce Check\n");
        printf("Initial Total # of Free Blocks should be less than Final Total # of Free Blocks\n");
    }
    printf("Initial Size: %d, Final Size: %d\n", initial_num_elems, final_num_elems);
    
    return final_num_elems < initial_num_elems;
}
// 0 - 32, 33 - 64, 65 - 128, 129 - 2048, 2049 - 16384, 16384 - inf
void print_coalesce_stats() {
    printf("COALESCE_STATS Deferred: %d Immediate: %d\n", num_deferred_coalesces, num_immediate_coalesces);
}