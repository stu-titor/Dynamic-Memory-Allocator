#include "umalloc.h"
#include "csbrk.h"
#include <stdio.h>
#include <assert.h>
#include "ansicolors.h"

const char author[] = ANSI_BOLD ANSI_COLOR_RED "Simon Shrestha" ANSI_RESET;

mem_block_header_t *free_heads[BIN_COUNT];
size_t bin_limits[BIN_COUNT - 1];

/*
 * The following helpers can be used to interact with the mem_block_header_t
 * struct, they can be adjusted as necessary.
 */

/*
 * block_metadata - returns true if a block is marked as allocated.
 */
bool is_allocated(mem_block_header_t *block) {
    assert(block != NULL);
    return block->block_metadata & 0x1;
}

/*
 * allocate - marks a block as allocated.
 */
void allocate(mem_block_header_t *block) {
    assert(block != NULL);
    block->block_metadata |= 0x1;
}

/*
 * deallocate - marks a block as unallocated.
 */
void deallocate(mem_block_header_t *block) {
    assert(block != NULL);
    block->block_metadata &= ~0x1;
}

/*
 * get_size - gets the size of the block.
 */
size_t get_size(mem_block_header_t *block) {
    assert(block != NULL);
    return block->block_metadata & ~(ALIGNMENT-1);
}

/*
 * get_next - gets the next block.
 */
mem_block_header_t *get_next(mem_block_header_t *block) {
    assert(block != NULL);
    return block->next;
}


/*
 * get_payload - gets the payload of the block.
 */
void *get_payload(mem_block_header_t *block) {
    assert(block != NULL);
    return (void*)(block + 1);
}

/*
 * get_header - given a payload, returns the block.
 */
mem_block_header_t *get_header(void *payload) {
    assert(payload != NULL);
    return ((mem_block_header_t *)payload) - 1;
}

/*
* The following are helper functions that can be implemented to assist in your
* design, but they are not required. 
*/

/*
 * set_block_metadata
 * Optional helper method that can be used to initialize the fields for the 
 * memory block struct. 
 */
void set_block_metadata(mem_block_header_t *block, size_t size, bool alloc) {
    block->block_metadata = size;

    if(alloc) allocate(block);
}

static void add_block(mem_block_header_t *block) {
    unsigned long block_address = (unsigned long)block;

    int index = select_bin(get_size(block));
    mem_block_header_t *current = free_heads[index];

    //case for if freed block should be at the start
    if(current == NULL || (unsigned long)current > block_address) { 
        block->next = current;
        free_heads[index] = block;
    } else {
        //add block in memory address order
        while(current->next != NULL) {
            unsigned long next_address = (unsigned long)current->next;
            if(next_address > block_address) {
                block->next = current->next;
                current->next = block;
                return;
            }
            current = current->next;
        }

        //case for adding at the end
        block->next = NULL;
        current->next = block; 
    }
}

/*
 * find - finds a free block that can satisfy the umalloc request.
 */
mem_block_header_t *find(size_t total_size) {
    for(int i = select_bin(total_size); i < BIN_COUNT; i++) {
        mem_block_header_t *current_block = free_heads[i];
        if(current_block == NULL) continue;

        if(get_size(current_block) >= total_size){
            free_heads[i] = current_block->next;
            return current_block;
        } 

        mem_block_header_t *prev = current_block;
        current_block = current_block->next;
        while(current_block != NULL) {
            if(get_size(current_block) >= total_size) {
                prev->next = current_block->next;
                return current_block;
            }
            prev = current_block;
            current_block = current_block->next;
        }
    }
	return NULL;
}

/*
 * extend - extends the heap if more memory is required.
 */
mem_block_header_t *extend(size_t size) {
    size_t space = (NUM_PAGESIZE << 1) * PAGESIZE;
    void *address = csbrk(space);
    if(address == NULL) return NULL;

    size_t offset = 0;
    int index = select_bin(size);
    size_t block_size = (index == BIN_COUNT - 1) ? bin_limits[BIN_COUNT - 2] << 1 : bin_limits[index]; //Size of each free block in the bin
    size_t num_blocks = space / block_size; //Number of blocks for this bin

    mem_block_header_t *new_head = NULL;
    mem_block_header_t *current = NULL;
    mem_block_header_t *prev = NULL;
    for(int j = 0; j < num_blocks; j++) {
        current = (mem_block_header_t *)((char *)address + offset);
        set_block_metadata(current, block_size, false);
        current->next = NULL;
        
        if(new_head == NULL) new_head = current;  
        else prev->next = current;  
        
        prev = current;
        offset += block_size;
    }

    if(free_heads[index] == NULL) {
        free_heads[index] = new_head;
    } else if((unsigned long)free_heads[index] > (unsigned long) new_head) {
        current->next = free_heads[index];
        free_heads[index] = new_head;
    } else {
        current = free_heads[index];
        while(current->next != NULL) {
            current = current->next;
        }
        current->next = new_head;
    }

    return find(size);
}

/*
 * split - splits a given block in parts, one allocated, one free.
 */
mem_block_header_t *split(mem_block_header_t *block, size_t new_block_size) {
    if ((get_size(block) - new_block_size) < ALIGNMENT) {
        return block;
    }

    unsigned long address = (unsigned long)block;
    mem_block_header_t *free_block = (mem_block_header_t *)((char *)address + new_block_size);

    set_block_metadata(free_block, get_size(block) - new_block_size, false);
    free_block->next = NULL;
    add_block(free_block);

    set_block_metadata(block, new_block_size, false);

	return block;
}

/*
* The following are functions that are required to be implemented for correctness. 
*/

/*
 * select_bin - selects a free list bin to use based on the 
 * block size. Returns an index
 * REQUIRED   
 */
size_t select_bin(size_t size) {
    for(int i = 0; i < BIN_COUNT - 1; i++) {
        if(size <= bin_limits[i]) return i;
    }
    return BIN_COUNT - 1;
}

/**
 * set_bin_limits - initializes global bin_limits[] array
 * Set the limit as the max sized block you can allocate using a block
 * from that bin
 * REQUIRED
 */
void set_bin_limits() {
    for(int i = 0; i < BIN_COUNT - 1; i++) {
        bin_limits[i] = (ALIGNMENT * ALIGNMENT) << i;
    }
}

/**
 * coalesce - attempts to coalesce within a single bin
 * returns false if no coalesce is done at all, else return true
 * REQUIRED
 */
bool coalesce() {
    bool coalesce_status = false;
    for(int i = 0; i < BIN_COUNT; i++) {
        mem_block_header_t *prev_block = NULL;
        mem_block_header_t *current_block = free_heads[i];
        if(current_block == NULL) continue;
        mem_block_header_t *next_block = current_block->next;

        while(next_block != NULL) {
            unsigned long current_end = (unsigned long)current_block + get_size(current_block);
            unsigned long next_start = (unsigned long)next_block;

            if(current_end == next_start) {
                coalesce_status = true;
                set_block_metadata(current_block, get_size(current_block) + get_size(next_block), false);

                if(prev_block == NULL) free_heads[i] = next_block->next;
                else prev_block->next = next_block->next;

                current_block->next = NULL;
                add_block(current_block);

                //restart
                prev_block = NULL;
                current_block = free_heads[i];
                if(current_block == NULL) break;
                next_block = current_block->next;
            } else {
                //continue
                prev_block = current_block;
                current_block = next_block;
                next_block = next_block->next;
            }
        }
    }
	return coalesce_status;
}


/*
 * uinit - Used initialize metadata required to manage the heap
 * along with allocating initial memory.
 * REQUIRED
 */
int uinit() {
    set_bin_limits();

    size_t space = PAGESIZE * NUM_PAGESIZE;
    size_t bin_space = space / NUM_PAGESIZE; //Space allocated for each bin
    void *address = csbrk(space);
    if(address == NULL) return -1;

    for(int i = 0; i < BIN_COUNT; i++) {
        free_heads[i] = NULL;
    }

    size_t offset = 0;
    for(int i = 0; i < NUM_PAGESIZE; i++) {
        size_t block_size = bin_limits[i]; //Size of each free block in the bin
        size_t num_blocks = bin_space / block_size; //Number of blocks for this bin

        mem_block_header_t *prev = NULL;
        for(int j = 0; j < num_blocks; j++) {
            mem_block_header_t *current = (mem_block_header_t *)((char *)address + offset);
            set_block_metadata(current, block_size, false);
            current->next = NULL;
            
            if(prev == NULL) free_heads[i] = current;  
            else prev->next = current;  
            
            prev = current;
            offset += block_size;
        }
    }
    return 0;
}

/*
 * umalloc -  allocates size bytes and returns a pointer to the allocated memory.
 * REQUIRED
 */
void *umalloc(size_t size) {
    size_t total_size = size + sizeof(mem_block_header_t);

    int remainder = total_size % ALIGNMENT;
    if(remainder != 0) {
        total_size += ALIGNMENT - remainder;
    }

	mem_block_header_t *block = find(total_size);
    if(block == NULL) {
        coalesce();
        block = find(total_size);
        
        if(block == NULL) block = extend(total_size);

        if(block == NULL) {
            void *address = csbrk(total_size);
            if(address == NULL) return NULL;

            block = (mem_block_header_t *)address;
            set_block_metadata(block, total_size, false);
            block->next = NULL;
        }
    }

    block = split(block, total_size);
    allocate(block);

    return get_payload(block);
}

/**
 * ufree - frees the memory space pointed to by ptr.
 * ptr - pointer to payload of the memory to be freed, 
 * must have been called by a previous malloc call.
 * REQUIRED
 */
void ufree(void *ptr) {
    if(ptr != NULL) {
        mem_block_header_t *block = get_header(ptr);
        deallocate(block);
        add_block(block);
    }
}