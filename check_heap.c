#include "check_heap.h"

extern mem_block_header_t *free_head;
extern mem_block_header_t *free_heads[BIN_COUNT];

int check_heap() {
    for (int i = 0; i < BIN_COUNT; ++i) {
        int result = check_bin(free_heads[i]);
        if (result) return result;
    }
    return HEAP_SUCCESS;
}

/*
 * STUDENT TODO: set these variables according to your heap design (tests will fail if you do not
 * set these variables!)
 *    - order:      how is your free list ordered?
 *    - circular:   true if your free list is circular; false otherwise
 */
heap_order order = ORD_MEM;
bool circular = false;


/*
 * get_bin_size - used to count the number of free blocks in a bin
 *
 * STUDENT TODO: this function is required to be completed for checkpoint 1
 *       - Ensure that each free block in the current bin is counted
 * 
 * Should return the number of free blocks in the current bin.
*/
int get_bin_size(size_t bin_idx) {
    mem_block_header_t *current = free_heads[bin_idx];
    if(current == NULL) return 0;

    int count = 0;
    while(current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

/*
 * check_bin -  used to check that the heap is still in a consistent state.
 * 
 * STUDENT TODO: this function is required to be completed for checkpoint 1
 * 
 *      - Ensure that the free block list is in the order you expect it to be in
 *        (if your list is randomly ordered, this check is not required). x
 * 
 *      - Check if any free blocks overlap with each other. 
 * 
 *      - Ensure that each free block is aligned. x
 * 
 *      - Ensure that all blocks on the free list are free (no implicit free lists) x 
 *
 * Should return HEAP_SUCCESS if the heap is consistent or HEAP_FAILURE if an error 
 * is detected.
 */
int check_bin(mem_block_header_t *free_head) {
    while(free_head != NULL) {
        unsigned long address = (unsigned long)free_head;
        if(address % ALIGNMENT != 0) return HEAP_FAILURE; //Check blocks for alignment 

        if((free_head->block_metadata & 0x1) != 0) return HEAP_FAILURE; //Make sure block is free
    
        unsigned long this_end = address + get_size(free_head);
        unsigned long next_start = (unsigned long)free_head->next; 
        
        if(!(free_head->next == NULL || next_start > this_end)) return HEAP_FAILURE; //mem order

        for(int i = 0; i < BIN_COUNT; i++) {
            mem_block_header_t *current = free_heads[i];
            while(current != NULL) {
                unsigned long current_start = (unsigned long)current;
                if(current_start % ALIGNMENT != 0) return HEAP_FAILURE; 

                unsigned long current_end = current_start + get_size(current);

                if(current == free_head){
                    current = current->next;
                    continue; //skip this block
                } 

                if(!(address > current_end || this_end < current_start)) return HEAP_FAILURE;
                current = current->next;
            }
        }

        free_head = free_head->next;
    }
    return HEAP_SUCCESS;
}
