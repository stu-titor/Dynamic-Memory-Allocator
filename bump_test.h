#include <inttypes.h>
#include "check_heap.h"
#include "csbrk.h"
#include "support.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

size_t total_heap();
void print_all();

extern mem_block_header_t *free_heads[BIN_COUNT];
extern bool circular;
