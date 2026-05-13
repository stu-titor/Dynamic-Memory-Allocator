# Makefile
CC = gcc
DEBUG_FLAG = -O0
DEPLOY_FLAG = -O2
OPT_FLAG = $(DEPLOY_FLAG) # -O0 for use with GDB, -O2 for testing performance
CFLAGS = -Wall $(OPT_FLAG) -Werror -g3
WRAP_FLAGS = -Wl,--wrap=umalloc -Wl,--wrap=ufree

# --- Build Macros ---

# 1. Logic for standard/tracked build (injects record_coalesce)
define BUILD_UMALLOC_TRACKED
	cp umalloc.c umalloc_tracked.c
	sed -i '1i void record_coalesce(void);' umalloc_tracked.c
	sed -i 's/bool coalesce().*{/bool coalesce() { record_coalesce();/g' umalloc_tracked.c
	sed -i '/bool coalesce()/!b;n;s/{/{ record_coalesce();/' umalloc_tracked.c
	$(CC) $(CFLAGS) -c umalloc_tracked.c -o umalloc.o
	rm umalloc_tracked.c
endef

# 2. Logic for debug build (simple compile, no injection)
define BUILD_UMALLOC_SIMPLE
	$(CC) $(CFLAGS) -c umalloc.c -o umalloc.o
endef

# Default to tracked build
BUILD_UMALLOC = $(BUILD_UMALLOC_TRACKED)

# --------------------

all: runner heap_runner performance bump_test

support.o: support.c support.h

err_handler.o: err_handler.c err_handler.h 

# umalloc.o now uses the variable defined above
umalloc.o: umalloc.c umalloc.h
	$(BUILD_UMALLOC)

check_heap.o: check_heap.c check_heap.h
	$(CC) $(CFLAGS) -c check_heap.c

# 'debug' overrides OPT_FLAG and the BUILD_UMALLOC command
debug: OPT_FLAG=$(DEBUG_FLAG)
debug: BUILD_UMALLOC=$(BUILD_UMALLOC_SIMPLE)
debug: clean all

deploy: OPT_FLAG=$(DEPLOY_FLAG)
deploy: clean all

runner: runner.c csbrk.o umalloc.o check_heap.o err_handler.o support.o coalesce_check.o
	$(CC) $(CFLAGS) $(WRAP_FLAGS) -o runner runner.c csbrk.o umalloc.o check_heap.o err_handler.o support.o coalesce_check.o

heap_runner: heap_runner.o check_heap.o csbrk.o umalloc.o err_handler.o support.o coalesce_check.o
	$(CC) $(CFLAGS) $(WRAP_FLAGS) -o heap_runner heap_runner.o check_heap.o csbrk.o umalloc.o err_handler.o support.o coalesce_check.o

bump_test: bump_test.o check_heap.o csbrk.o umalloc.o err_handler.o support.o coalesce_check.o
	$(CC) $(CFLAGS) $(WRAP_FLAGS) -o bump_test bump_test.o check_heap.o csbrk.o umalloc.o err_handler.o support.o coalesce_check.o

debug_heap_runner: OPT_FLAG=$(DEBUG_FLAG)
debug_heap_runner: heap_runner

bump_test.o: bump_test.c bump_test.h
	$(CC) $(CFLAGS) -c bump_test.c

heap_runner.o: heap_runner.c check_heap.h
	$(CC) $(CFLAGS) -c heap_runner.c

coalesce_check.o: coalesce_check.c coalesce_check.h
	$(CC) $(CFLAGS) -c coalesce_check.c

performance: performance.c csbrk.o umalloc.o support.o err_handler.o check_heap.o
	$(CC) $(CFLAGS) $(WRAP_FLAGS) -o performance performance.c csbrk.o umalloc.o err_handler.o support.o check_heap.o coalesce_check.o

unittest: unittest.o support.o umalloc.o csbrk.o err_handler.o check_heap.o
	$(CC) $(CFLAGS) $(WRAP_FLAGS) -o unittest unittest.c umalloc.o support.o csbrk.o err_handler.o check_heap.o coalesce_check.o

clean:
	rm -f *.so runner heap_runner coalesce_check bump_test gprof_performance performance *.gcda gmon.out \
		support.o err_handler.o umalloc.o check_heap.o gprof_umalloc.o heap_runner.o bump_test.o coalesce_check.o