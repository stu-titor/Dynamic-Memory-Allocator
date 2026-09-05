# Dynamic Memory Allocator

A custom dynamic memory allocator written in C.

This project implements basic versions of `malloc` and `free` using segregated free lists, block splitting, deferred coalescing, and heap expansion.

## Features

* Custom `umalloc()` and `ufree()` functions
* 16-byte aligned memory blocks
* 8 size-based free-list bins
* Block splitting to reduce wasted space
* Deferred coalescing of adjacent free blocks
* Heap expansion using `csbrk()`
* Trace-based correctness and performance testing

## Main Files

* `umalloc.c` - main allocator implementation
* `umalloc.h` - allocator structures, constants, and function declarations
* `runner.c` - runs allocator correctness tests
* `heap_runner.c` - checks heap behavior
* `performance.c` - measures allocator performance
* `driver.py` - runs the provided test suite and reports results
* `traces/` - allocation/free traces used for testing
* `Makefile` - builds the project

## Building

Compile the project with:

```bash
make
```

This builds the main testing programs:

```text
runner
heap_runner
performance
bump_test
```

For a debug build:

```bash
make debug
```

## Running Tests

Run the Python test driver with:

```bash
python3 driver.py
```

You can also run individual trace files with the runner:

```bash
./runner -r traces/<trace-file>
```

To measure the performance of a trace:

```bash
./performance traces/<trace-file>
```

## How It Works

The allocator keeps free memory blocks in separate bins based on block size.

When `umalloc()` is called, the allocator:

1. Aligns the requested size to 16 bytes.
2. Searches the appropriate free-list bins for a usable block.
3. Splits a larger block when possible.
4. Attempts to coalesce free blocks if no block is available.
5. Expands the heap if more memory is needed.

When `ufree()` is called, the block is marked as free and returned to its appropriate free-list bin.
