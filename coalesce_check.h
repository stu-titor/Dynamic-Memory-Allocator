#include <inttypes.h>
#include "check_heap.h"
#include "csbrk.h"
#include "support.h"
// #include "umalloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

extern mem_block_header_t *free_heads[BIN_COUNT];
extern size_t bin_limits[BIN_COUNT];
extern bool circular;
extern int uinit();
void* __real_umalloc(size_t size);
void __real_ufree(void *ptr);
// extern int get_bin_size(mem_block_header_t *free_head);

void print_coalesce_stats();
void reset_coalesce_globals();
int test_coalesce();

typedef struct coalesce_container_struct {
    void* ptr;
    struct coalesce_container_struct* next;
} coalesce_container_struct;