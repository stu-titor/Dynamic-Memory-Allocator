/**************************************************************************
 * C S 429 MM-lab
 * 
 * heap_runner.c - runs a series of tests to ensure check_bin() is correct
 * 
 * Copyright (c) 2025 A. Lieou, J. Upadhyaya  All rights reserved.
 * May not be used, modified, or copied without permission.
 **************************************************************************/

#include "check_heap.h"
#include "csbrk.h"
#include "support.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <string.h>

extern heap_order order;
extern bool circular;
extern mem_block_header_t *free_heads[BIN_COUNT];
extern char msg[MAXLINE];

mem_block_header_t* test_blocks = NULL;

#define block_count 16
uint64_t sizes[block_count];
bool verbose = false;

const char* ordering[] = {
    "SIZE",
    "MEMORY",
    "RANDOM",
    "OTHER"
};

/**
 * Important Note: 
 * 
 * In these tests size == payload only. However, this will not cause any issues if
 * your implementation does not make this assumption. 
 * 
 * You might "undershoot" the end of a block when checking for overlap; that is fine.
 * In those tests, we are not expecting you to detect overlap anyways. 
 * 
 * On the tests where there is overlap, the overlap is severe enough to be detected
 * by an implementation that assumes size is: just payload, payload + header, or 
 * payload + header + footer. 
 */

/**
 * print_free_list - prints the current state of the free list being tested
 * 
 * @pre must build_free_list before using this function
 * 
 * @note size here represents PAYLOAD only
 * 
 * STUDENTS: use this function to print the free list before/during/after tests for debugging
 */
void print_free_list() {
    if (!test_blocks) return;

    printf("\n   Free List:\n\n");

    mem_block_header_t* temp = test_blocks;
    do {
        // address : size : allocation status : next
        printf("      %p : 0x%lx : %lx : %p\n", 
                temp, temp->block_metadata & ~0xF, temp->block_metadata & 0x1, temp->next);
        temp = temp->next;
    } while (temp && (circular ? temp != test_blocks : 1));
    printf("\n");
}

/**
 * swap - swaps two blocks in the free list
 * 
 * @pre must build_free_list before using this function; indices are in bounds
 * 
 * This function is used to create an out-of-order error. 
 */
void swap(int index1, int index2) {
    if (!test_blocks || index1 == index2) return;

    if (index1 > index2) {
        int temp = index1;
        index1 = index2;
        index2 = temp;
    }

    mem_block_header_t *prev1 = NULL, *block1 = test_blocks;
    mem_block_header_t *prev2 = NULL, *block2 = test_blocks;

    for (int i = 0; i < index1 && block1; ++i) {
        prev1 = block1;
        block1 = block1->next;
        if (circular && block1 == test_blocks) break;
    }

    for (int i = 0; i < index2 && block2; ++i) {
        prev2 = block2;
        block2 = block2->next;
        if (circular && block2 == test_blocks) break;
    }

    if (!block1 || !block2 || (circular && (block1 == test_blocks || block2 == test_blocks))) return;

    if (block1->next == block2) {
        if (prev1) {
            prev1->next = block2;
        } else {
            test_blocks = block2;
        }

        block1->next = block2->next;
        block2->next = block1;

        if (circular && block2->next == test_blocks) test_blocks = block1;
        return;
    }

    if (prev1) {
        prev1->next = block2;
    } else {
        test_blocks = block2;
    }

    if (prev2) {
        prev2->next = block1;
    }

    mem_block_header_t* temp_next = block1->next;
    block1->next = block2->next;
    block2->next = temp_next;

    if (circular && block1->next == test_blocks) test_blocks = block2;
}

/**
 * total_size - calculate the total size that a free list will require
 */
size_t total_size() {
    size_t total = 0;
    for (int i = 0; i < block_count; ++i) {
        total += (sizeof(mem_block_header_t) * 2) + sizes[i];
    }
    return total;
}

/**
 * build_free_list - builds a free list based on the order and termination global variables
 * 
 * @note whether or not you include the header in block_metadata does NOT matter
 * 
 * @pre the globals order and circular must be set according to the student's design
 * @post a test free list is created
 * 
 */
void build_free_list() {
    size_t total = total_size();
    test_blocks = (mem_block_header_t*) malloc(total + 1024);
    char* current = (char*) test_blocks;
    char* boundary = (char*) test_blocks + total;
    mem_block_header_t* last = NULL;

    switch (order) {
        case ORD_MEM: {
            for (int i = 0; i < block_count; ++i) {
                mem_block_header_t* block = (mem_block_header_t*) current;
                block->block_metadata = sizes[i];

                current += (sizeof(mem_block_header_t) * 2) + sizes[i];
                if (current < boundary) {
                    block->next = (mem_block_header_t*) current;
                } else {
                    block->next = NULL;
                }

                last = block;
            }
            break;
        }

        case ORD_RAND: {
            int indices[block_count];
            for (int i = 0; i < block_count; ++i) {
                indices[i] = i;
            }

            for (int i = block_count - 1; i > 0; --i) {
                int j = rand() % (i + 1);
                int temp = indices[i];
                indices[i] = indices[j];
                indices[j] = temp;
            }

            for (int i = 0; i < block_count; ++i) {
                mem_block_header_t* block = (mem_block_header_t*) current;
                block->block_metadata = sizes[indices[i]];

                current += (sizeof(mem_block_header_t) * 2) + sizes[indices[i]];
                if (current < boundary) {
                    block->next = (mem_block_header_t*) current;
                } else {
                    block->next = NULL;
                }

                last = block;
            }
            break;
        }

        case ORD_SIZE: {
            for (int i = 0; i < block_count - 1; ++i) {
                for (int j = 0; j < block_count - i - 1; ++j) {
                    if ((sizes[j] & ~0xF) > (sizes[j + 1] & ~0xF)) {
                        uint64_t temp = sizes[j];
                        sizes[j] = sizes[j + 1];
                        sizes[j + 1] = temp;
                    }
                }
            }

            for (int i = 0; i < block_count; ++i) {
                mem_block_header_t* block = (mem_block_header_t*) current;
                block->block_metadata = sizes[i];

                current += (sizeof(mem_block_header_t) * 2) + sizes[i];
                if (current < boundary) {
                    block->next = (mem_block_header_t*) current;
                } else {
                    block->next = NULL;
                }

                last = block;
            }

            break;
        }

        case ORD_OTHER:
        default:
            test_blocks = NULL;
            return;
    }

    if (circular && last) {
        last->next = test_blocks;
    }

    if (verbose) print_free_list();
}

/**
 * implicit_check - sets the allocated bit (somewhat randomly) and ensures check_heap fails
 * 
 * @pre must build_free_list before calling this function
 */
int implicit_check() {
    if (!test_blocks) return -1;
    mem_block_header_t* current = test_blocks;
    do {
        current->block_metadata &= ~0x1;
        current = current->next;
    } while (current && (circular ? current != test_blocks : 1));

    current = test_blocks;
    do {
        if (rand() % 2 || current == test_blocks) {
            current->block_metadata |= 0x1;
        }
        current = current->next;
    } while (current && (circular ? current != test_blocks : 1));

    int result = check_bin(test_blocks);
    if (result != HEAP_FAILURE) {
        printf("  Failed to detect implicit allocation error. \n");
        if (verbose) print_free_list();

        current = test_blocks;
        do {
            current->block_metadata &= ~0x1;
            current = current->next;
        } while (current && (circular ? current != test_blocks : 1));
        return -1; 
    }

    current = test_blocks;
    do {
        current->block_metadata &= ~0x1;
        current = current->next;
    } while (current && (circular ? current != test_blocks : 1));

    result = check_bin(test_blocks);
    if (result != HEAP_SUCCESS) {
        printf("  Detected failure on correct list after implicit check.\n");
        if (verbose) print_free_list();
        return -1;
    }

    return 0;
}

/**
 * alignment_check - causes block addresses to be misaligned and ensures check_bin fails
 * 
 * @pre must build_free_list before calling this function
 */
int alignment_check() {
    if (!test_blocks) return -1;
    mem_block_header_t* current = test_blocks;
    do {
        if (!current || !current->next) break;

        mem_block_header_t* original_next = current->next;
        uintptr_t misaligned_address = (uintptr_t) original_next + 1;
        current->next = (mem_block_header_t*) misaligned_address;

        int result = check_bin(test_blocks);
        current->next = original_next;

        if (result != HEAP_FAILURE) {
            printf("  Failed to detect misalignment at block %p\n", current->next);
            if (verbose) print_free_list();
            return -1;
        }

        result = check_bin(test_blocks);
        if (result != HEAP_SUCCESS) {
            printf("  Detected failure on correct list after misalignment at block %p\n", current);
            if (verbose) print_free_list();
            return -1;
        }

        current = original_next;
    } while (current && (circular ? current != test_blocks : 1));
    return 0;
}

/**
 * overlap_check - causes blocks to overlap with each other and ensures that check_bin fails
 * 
 * @pre must build_free_list before calling this function
 */
int overlap_check() {
    if (!test_blocks) return -1;
    mem_block_header_t* current = test_blocks;

    do {
        if (!current || !current->next) break;
        size_t original_size = current->block_metadata;

        // no matter what "size" means for your implementation, this 
        // will cause overlap. 
        current->block_metadata *= 100;

        int result = check_bin(test_blocks);
        current->block_metadata = original_size;

        if (result != HEAP_FAILURE) {
            printf("  Failed to detect overlap at block %p\n", current);
            if (verbose) print_free_list();
            return -1;
        }

        result = check_bin(test_blocks);
        if (result != HEAP_SUCCESS) {
            printf("  Detected failure on correct list after overlap at block %p\n", current);
            if (verbose) print_free_list();
            return -1;
        }

        current = current->next;
    } while (current && (circular ? current->next != test_blocks : 1));

    return 0;
}

/**
 * ooo_check - causes an out-of-order error and ensures that check_bin fails
 * 
 * @pre must build_free_list before calling this function
 */
int ooo_check() {
    if (!test_blocks) return -1;
    for (int i = 0; i < 10; ++i) {
        int index1 = (rand() % (block_count - 1)) + 1;
        int index2 = (rand() % (block_count - 1)) + 1;
        while (index1 == index2) index2 = (rand() % (block_count - 1)) + 1;

        swap(index1, index2);
        int result = check_bin(test_blocks);
        if (result != HEAP_FAILURE) {
            printf("  Failed to detect a out of order blocks\n");
            if (verbose) print_free_list();
            swap(index1, index2);
            return -1;
        }

        swap(index1, index2);
        result = check_bin(test_blocks);
        if (result != HEAP_SUCCESS) {
            printf("  Detected failure on correct list after out of order swap\n");
            if (verbose) print_free_list();
            return -1;
        }
    }
    return 0;
}

/**
 * ooo_check_size - causes an out of order error and ensures that check_bin fails
 * 
 * @pre must build_free_list before calling this function
 */
int ooo_check_size() {
    if (!test_blocks) return -1;
    for (int i = 0; i < 10; ++i) {
        int index1, index2;
        mem_block_header_t* block1 = NULL;
        mem_block_header_t* block2 = NULL;

        // ensure two blocks of different sizes are swapped
        do {
            index1 = (rand() % (block_count - 1)) + 1;
            index2 = (rand() % (block_count - 1)) + 1;

            mem_block_header_t* temp = test_blocks;
            for (int j = 0; temp && j < block_count; ++j) {
                if (j == index1) block1 = temp;
                if (j == index2) block2 = temp;
                temp = temp->next;
            }
        } while (index1 == index2 || 
                 !block1 || !block2 || 
                 (block1->block_metadata & ~0xF) == (block2->block_metadata & ~0xF));

        swap(index1, index2);
        int result = check_bin(test_blocks);
        if (result != HEAP_FAILURE) {
            printf("  Failed to detect out-of-order blocks after swap\n");
            if (verbose) print_free_list();
            swap(index1, index2);
            return -1;
        }

        swap(index1, index2);
        result = check_bin(test_blocks);
        if (result != HEAP_SUCCESS) {
            printf("  Detected failure on correct list after out-of-order swap\n");
            if (verbose) print_free_list();
            return -1;
        }
    }
    return 0;
}

void print_design() {
    printf("Your free list is in %s order ", ordering[order]);
    if (circular) printf("and is circular.\n\n");
    else printf("and is null terminated.\n\n");
}

void usage() {
    printf("Usage: heap_runner [-rhvuc] file\n");
    printf("Options\n");
    printf("\t-h         Prints this message.\n");
    printf("\t-t <num>   Runs only the specified test number (implicit, alignment, overlap, order).\n");
    printf("\t-v         Prints free_list and additional information.\n");
}

int run_all() {
    int score = 5;
    int result = check_bin(free_heads[0]);

    if (result) {
        printf("  Failed correct list check\n");
        if (verbose) print_free_list();
        printf("\n");
        --score;
    }

    if (implicit_check() == -1) {
        printf("  Failed implicit check\n");
        --score;
    } 
    else printf("  Passed implicit check\n");
    printf("\n");

    if (alignment_check() == -1) {
        printf("  Failed alignment check\n");
        --score;
    }
    else printf("  Passed alignment check\n");
    printf("\n");

    if (overlap_check() == -1) {
        printf("  Failed overlap check\n");
        --score;
    } 
    else printf("  Passed overlap check\n");
    printf("\n");

    if (order == ORD_MEM && ooo_check() == -1) {
        printf("  Failed order check\n");
        --score;
    } else if (order == ORD_SIZE && ooo_check_size() == -1) {
        printf("  Failed order check\n");
        --score;
    }
    else printf("  Passed order check\n");
    printf("\n");

    if (score == 5) printf("All Tests Passed!\n");
    else printf("Tests Passed: %d/5\n", score);

    return (5 - score);
}

int run_test(uint8_t test) {
    switch (test) {
        case 1:
            if (implicit_check() == -1) {
                printf("  Failed implicit check\n");
                return -1;
            } 
            else printf("  Passed implicit check\n");
            printf("\n");
            return 0;
        case 2:
            if (alignment_check() == -1) {
                printf("  Failed alignment check\n");
                return -1;
            }
            else printf("  Passed alignment check\n");
            printf("\n");
            return 0;
        case 3:
            if (overlap_check() == -1) {
                printf("  Failed overlap check\n");
                return -1;
            } 
            else printf("  Passed overlap check\n");
            printf("\n");
            return 0;
        case 4:
            if (order == ORD_MEM && ooo_check() == -1) {
                printf("  Failed order check\n");
                return -1;
            } else if (order == ORD_SIZE && ooo_check_size() == -1) {
                printf("  Failed order check\n");
                return -1;
            }
            else printf("  Passed order check\n");
            printf("\n");
            return 0;
        default:
            return -1;
    }
}

int main(int argc, char **argv) {

    uint8_t test = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "-h") == 0) {
            usage();
            return 0;
        } else if (strcmp(argv[i], "-t") == 0) {
            if (i + 1 < argc) {
                test = atoi(argv[i + 1]);
                if (test <= 0 || test >= 5) {
                    printf("Error: Test number must be a positive integer [1,4].\n");
                    return -1;
                }
                i++;
            } else {
                printf("Error: -t flag requires a test number [1,4].\n");
                return -1;
            }
        }
    }

    if (verbose) print_design();

    printf("Starting check_heap Tests...\n");
    srand(0xBEEFCAFE);

    for (int i = 0; i < 16; ++i) {
        sizes[i] = (rand() % 4 + 4) * 16;
    }

    build_free_list();
    free_heads[0] = test_blocks;

    if (free_heads[0] == NULL) {
        printf("No tests available for your implementation.");
        printf("The TAs will manually grade your submission after the due date.\n");
        return -1;
    }

    if (test) {
        return run_test(test);
    } else {
        return run_all();
    }
}