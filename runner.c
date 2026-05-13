/**************************************************************************
 * C S 429 MM-lab
 *
 * runner.c - Runs the traces and evaluates the umalloc package for correctness
 * and utilization.
 *
 * Copyright (c) 2021 M. Hinton. All rights reserved.
 * May not be used, modified, or copied without permission.
 **************************************************************************/

// #include "csbrk.h"
// #include "support.h"
// #include "check_heap.h"
#include "coalesce_check.h"
// #include <sys/mman.h>

int verbose = 0;
extern char msg[MAXLINE]; /* for whenever we need to compose an error message */
extern size_t sbrk_bytes;
extern const char author[];
extern size_t select_bin(size_t);
extern mem_block_header_t *free_heads[BIN_COUNT];

/*
 * usage - Explain the command line arguments
 */
static void usage(void)
{
    fprintf(stderr, "Usage: mdriver [-rhvucd] file\n");
    fprintf(stderr, "Options\n");
    fprintf(stderr, "\t-r         Run the trace to completion (bypass interface).\n");
    fprintf(stderr, "\t-h         Print this message.\n");
    fprintf(stderr, "\t-v         Print additional debug info.\n");
    fprintf(stderr, "\t-u         Display heap utilization.\n");
    fprintf(stderr, "\t-c         Runs the user provided heap check after every op.\n");
    fprintf(stderr, "\t-d         Runs the deferred coalesce check.\n");
}

/*
 * copy_id - Writes the block id out to the payload. To be used for correctness
 * checks.
 */
static void copy_id(size_t *block, size_t block_size, size_t id)
{
    size_t words = block_size / sizeof(size_t);
    for (size_t i = 0; i < words; i++)
    {
        block[i] = id;
    }
}

/*
 * check_id - Checks the block contains the block id, repeated the number of
 * words can fit.
 */
static int check_id(size_t *block, size_t block_size, size_t id)
{
    size_t words = block_size / sizeof(size_t);
    for (size_t i = 0; i < words; i++)
    {
        if (block[i] != id)
        {
            return -1;
        }
    }

    return 0;
}

/*
 * check_correctness - Checks if every block that is mark allocated has the
 * correct id written out. If this fails, means that an allocated payload
 * was affected by the umalloc package.
 */
static int check_correctness(trace_t *trace, size_t curr_op)
{
    for (size_t block_id = 0; block_id < trace->num_ids; block_id++)
    {
        allocated_block_t *block = &trace->blocks[block_id];
        if (block->is_allocated)
        {
            if (check_id(block->payload, block->block_size, block->content_val) == -1)
            {
                sprintf(msg, "umalloc corrupted block id %lu.", block_id);
                malloc_error(curr_op, msg);
                return -1;
            }
        }
    }

    if (verbose)
    {
        printf("line %ld passed the correctness check.\n", LINENUM(curr_op));
    }

    return 0;
}

size_t curr_bytes_in_use;
size_t max_bytes_in_use;

/*
 * UTILIZATION_SCORE - the utilization score represents how well the umalloc
 * package uses the bytes requested from sbrk. For example, if 100 bytes are
 * requested from sbrk, and the user requested 80 bytes, there will be a
 * utilization score of 80%.
 */
#define UTILIZATION_SCORE 100.0 * max_bytes_in_use / sbrk_bytes

/*
 * run_trace_line - Runs a single line in the trace. Checking if all the
 * correctness checks are still satisfied after the check. Checks if the returned
 * payload is aligned to 16 bytes, hasn't affected any other blocks, and rests
 * within the sbrk range. Runs the user created check heap function and prints
 * the current utilization score if requested.
 */
static int run_trace_line(trace_t *trace, size_t curr_op, int utilization, int run_check_heap)
{

    // if (curr_op % 5 == 0)
    // {
    //     void *ret = sbrk(4096);
    //     mprotect(ret, 4096, PROT_NONE);
    // }
    traceop_t op = trace->ops[curr_op];
    if (op.type == ALLOC)
    {
        trace->blocks[op.index].is_allocated = true;
        trace->blocks[op.index].content_val = curr_op;
        trace->blocks[op.index].block_size = op.size;

        if (verbose)
        {
            printf("line %ld: umalloc: id %d, Allocating %d bytes\n", LINENUM(curr_op), op.index, op.size);
        }

        trace->blocks[op.index].payload = umalloc(op.size);

        if (op.size > 2097152L) {
            trace->blocks[op.index].is_allocated = false;
            if (trace->blocks[op.index].payload != NULL) {
                malloc_error(curr_op, "umalloc did not return null for out of memory error");
                return -1;
            }
        }

        else {
            curr_bytes_in_use += op.size;

            if (trace->blocks[op.index].payload == NULL)
            {
                malloc_error(curr_op, "umalloc failed.");
                return -1;
            }

            if (((size_t)trace->blocks[op.index].payload) % ALIGNMENT != 0)
            {
                malloc_error(curr_op, "umalloc returned an unaligned payload.");
                return -1;
            }

            if (check_malloc_output(trace->blocks[op.index].payload, trace->blocks[op.index].block_size) == -1)
            {
                printf("line %ld: umalloc allocated a block out of bounds.\n", LINENUM(curr_op));
                return -1;
            }
            copy_id((size_t *)trace->blocks[op.index].payload, trace->blocks[op.index].block_size, curr_op);
        }

    }
    else
    {
        trace->blocks[op.index].is_allocated = false;

        if (verbose)
        {
            printf("line %ld: ufree: id %d\n", LINENUM(curr_op), op.index);
        }

        ufree(trace->blocks[op.index].payload);
        curr_bytes_in_use -= trace->blocks[op.index].block_size;
    }

    if (curr_bytes_in_use > max_bytes_in_use)
    {
        max_bytes_in_use = curr_bytes_in_use;
    }

    if (run_check_heap)
    {
        if (check_heap() != 0)
        {
            malloc_error(curr_op, "check heap failed.");
            return -1;
        }
        else
        {
            printf("Passed check heap.\n");
        }
    }

    if (check_correctness(trace, curr_op) == -1)
    {
        printf("line %ld failed the correctness check.\n", LINENUM(curr_op));
        return -1;
    }

    if (verbose && utilization)
    {
        printf("Current Utilization percentage: %.2f\n", UTILIZATION_SCORE);
    }

    return 0;
}

/*
 * auto_run_trace - Starting from curr_op, runs the trace to completetion.
 * Printing the utlilization and running check_heap if requested.
 */
static int auto_run_trace(trace_t *trace, int utilization, int run_check_heap, size_t curr_op)
{

    if (curr_op >= trace->num_ops)
    {
        printf("Trace run to completetion.\n");
        return curr_op;
    }

    for (; curr_op < trace->num_ops; curr_op++)
    {
        if (run_trace_line(trace, curr_op, utilization, run_check_heap) == -1)
        {
            printf("umalloc package failed.\n");
            exit(1);
        }
    }

    printf("umalloc package passed correctness check.\n");

    if (utilization)
    {
        printf("Final Utilization percentage: %.2f\n", UTILIZATION_SCORE);
    }
    return curr_op;
}

/*
 * help - Prints the help information for the Trace Runner.
 */
void help()
{
    printf("----------------MSIM Help-----------------------\n");
    printf("go               -  run trace to completion         \n");
    printf("run n            -  execute trace for n ops\n");
    printf("check            -  run the heap_check                \n");
    printf("util             -  display current heap utilization   \n");
    printf("help             -  display this help menu            \n");
    printf("quit             -  exit the program                  \n\n");
}

/*
 * interactive_run_trace - Interactive mode to run a trace, supported commands
 * provided in the help function.
 */
void interactive_run_trace(trace_t *trace, int utilization, int run_check_heap)
{
    char buffer[20];
    int ops_to_run;
    int ret;
    size_t curr_op = 0;

    while (1)
    {
        printf("MM> ");

        int size = scanf("%s", buffer);
        if (size == 0)
        {
            buffer[0] = 's';
        }
        printf("\n");

        switch (buffer[0])
        {
        case 'G':
        case 'g':
            curr_op = auto_run_trace(trace, utilization, run_check_heap, curr_op);
            break;

        case 'C':
        case 'c':
            printf("Running check_heap.\n");
            ret = check_heap();
            if (ret != 0)
                printf("check_heap returned non zero exit code.\n");
            break;

        case 'D':
        case 'd':
            printf("Displaying accumulated coalesce statistics:\n");
            print_coalesce_stats(); 
            break;
        case 'h':
        case 'H':
            help();
            break;

        case 'Q':
        case 'q':
            printf("Bye.\n");
            exit(0);

        case 'U':
        case 'u':
            printf("Current Utilization percentage: %.2f\n", UTILIZATION_SCORE);
            break;

        case 'R':
        case 'r':
            size = scanf("%d", &ops_to_run);
            if (size == 0)
            {
                break;
            }

            if (curr_op >= trace->num_ops)
            {
                printf("Trace run to completion.\n");
                break;
            }

            for (int op = 0; op < ops_to_run; op++)
            {
                if (run_trace_line(trace, curr_op, utilization, run_check_heap) == -1)
                {
                    printf("umalloc package failed.\n");
                    exit(1);
                }
                curr_op++;
                if (curr_op == trace->num_ops)
                {
                    printf("umalloc package passed correctness check.\n");
                    break;
                }
            }

            if (utilization && curr_op >= trace->num_ops)
            {
                printf("Final Utilization percentage: %.2f\n", UTILIZATION_SCORE);
            }

            break;

        default:
            printf("Invalid Command\n");
            break;
        }
    }
}

int which_bin(mem_block_header_t *bin)
{
    for (int i = 0; i < BIN_COUNT; ++i)
    {
        if (bin == free_heads[i])
            return i;
    }
    return -1;
}

bool uses_bins()
{
    int counts[BIN_COUNT];
    for (int i = 0; i < BIN_COUNT; ++i)
    {
        counts[i] = 0;
    }

    for (int i = 0; i < BIN_COUNT; ++i)
    {
        if (!free_heads[i])
            free_heads[i] = csbrk(PAGESIZE);
    }

    const uint64_t test_sizes[] = {
        // Tiny sizes (every 8 bytes from 32 to 512)
        8, 16, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120, 128, 136, 144, 152, 160, 168, 176, 184, 192, 200, 208, 216, 224, 232, 240, 248, 256, 264, 272, 280, 288, 296, 304, 312, 320, 328, 336, 344, 352, 360, 368, 376, 384, 392, 400, 408, 416, 424, 432, 440, 448, 456, 464, 472, 480, 488, 496, 504, 512,

        // Small allocations (every 16 bytes from 512 to 2048)
        528, 544, 560, 576, 592, 608, 624, 640, 656, 672, 688, 704, 720, 736, 752, 768, 784, 800, 816, 832, 848, 864, 880, 896, 912, 928, 944, 960, 976, 992, 1008, 1024,
        1040, 1056, 1072, 1088, 1104, 1120, 1136, 1152, 1168, 1184, 1200, 1216, 1232, 1248, 1264, 1280, 1296, 1312, 1328, 1344, 1360, 1376, 1392, 1408, 1424, 1440, 1456, 1472, 1488, 1504, 1520, 1536,
        1552, 1568, 1584, 1600, 1616, 1632, 1648, 1664, 1680, 1696, 1712, 1728, 1744, 1760, 1776, 1792, 1808, 1824, 1840, 1856, 1872, 1888, 1904, 1920, 1936, 1952, 1968, 1984, 2000, 2016, 2032, 2048,

        // Medium allocations (every 32 bytes from 2048 to 8192)
        2080, 2112, 2144, 2176, 2208, 2240, 2272, 2304, 2336, 2368, 2400, 2432, 2464, 2496, 2528, 2560,
        2592, 2624, 2656, 2688, 2720, 2752, 2784, 2816, 2848, 2880, 2912, 2944, 2976, 3008, 3040, 3072,
        3104, 3136, 3168, 3200, 3232, 3264, 3296, 3328, 3360, 3392, 3424, 3456, 3488, 3520, 3552, 3584,
        3616, 3648, 3680, 3712, 3744, 3776, 3808, 3840, 3872, 3904, 3936, 3968, 4000, 4032, 4064, 4096,

        // Large allocations (every 64 bytes from 4096 to 16384)
        4160, 4224, 4288, 4352, 4416, 4480, 4544, 4608, 4672, 4736, 4800, 4864, 4928, 4992, 5056, 5120,
        5184, 5248, 5312, 5376, 5440, 5504, 5568, 5632, 5696, 5760, 5824, 5888, 5952, 6016, 6080, 6144,
        6208, 6272, 6336, 6400, 6464, 6528, 6592, 6656, 6720, 6784, 6848, 6912, 6976, 7040, 7104, 7168,
        7232, 7296, 7360, 7424, 7488, 7552, 7616, 7680, 7744, 7808, 7872, 7936, 8000, 8064, 8128, 8192,

        // Extra-large allocations (every 128 bytes from 8192 to 32768)
        8320, 8448, 8576, 8704, 8832, 8960, 9088, 9216, 9344, 9472, 9600, 9728, 9856, 9984, 10112, 10240,
        10368, 10496, 10624, 10752, 10880, 11008, 11136, 11264, 11392, 11520, 11648, 11776, 11904, 12032, 12160, 12288,
        12416, 12544, 12672, 12800, 12928, 13056, 13184, 13312, 13440, 13568, 13696, 13824, 13952, 14080, 14208, 14336,
        14464, 14592, 14720, 14848, 14976, 15104, 15232, 15360, 15488, 15616, 15744, 15872, 16000, 16128, 16256, 16384,

        // Huge allocations (every 512 bytes from 16384 to 64000)
        16896, 17408, 17920, 18432, 18944, 19456, 19968, 20480, 20992, 21504, 22016, 22528, 23040, 23552, 24064, 24576,
        25088, 25600, 26112, 26624, 27136, 27648, 28160, 28672, 29184, 29696, 30208, 30720, 31232, 31744, 32256, 32768,
        33792, 34816, 35840, 36864, 37888, 38912, 39936, 40960, 41984, 43008, 44032, 45056, 46080, 47104, 48128, 49152,
        50176, 51200, 52224, 53248, 54272, 55296, 56320, 57344, 58368, 59392, 60416, 61440, 62464, 63488, 64000};

    const uint64_t size = sizeof(test_sizes) / sizeof(test_sizes[0]);

    for (int i = 0; i < size; ++i)
    {
        mem_block_header_t *bin = free_heads[select_bin(test_sizes[i])];
        int index = which_bin(bin);

        if (index == -1)
            return false;

        counts[index]++;
    }

    for (int i = 0; i < BIN_COUNT; ++i)
    {
        if (counts[i] == 0)
        {
            printf("Did not use bin #%d, %p\n", i, free_heads[i]);
            return false;
        }
    }

    return true;
}

int main(int argc, char **argv)
{

    char c;
    int autorun = 0, run_check_heap = 0, display_utilization = 0, run_deferred_check = 0;;

    /*
     * Read and interpret the command line arguments
     */
    while ((c = getopt(argc, argv, "rvhcud")) != EOF)
    {
        switch (c)
        {
        case 'r': /* Generate summary info for the autograder */
            autorun = 1;
            break;
        case 'v': /* Print per-trace performance breakdown */
            verbose = 1;
            break;
        case 'h': /* Print this message */
            usage();
            exit(0);
        case 'c':
            run_check_heap = 1;
            break;
        case 'u':
            display_utilization = 1;
            break;
        case 'd': /* Check for deferred coalescing */
            run_deferred_check = 1;
            break;
        default:
            usage();
            exit(1);
        }
    }

    char *file = argv[optind];

    if(run_deferred_check) {
        test_coalesce();
        return 0;
    }

    if (file == NULL)
    {
        usage();
        appl_error("Missing file parameters.");
    }

    if (verbose)
    {
        if (autorun)
        {
            printf("Auto Run Enabled.\n");
        }

        if (display_utilization)
        {
            printf("Displaying Utilization.\n");
        }

        if (run_check_heap)
        {
            printf("Running Check Heap After Each Op.\n");
        }
    }

    printf("Welcome to the MM lab runner\n\n");
    printf("Author: %s\n", author);

    if (BIN_COUNT < 4)
    {
        printf("BIN_COUNT is below required count.\n");
        exit(5);
    }


    trace_t *trace = read_trace(file, verbose);
    if (uinit() == -1)
    {
        malloc_error(-3, "uinit failed.");
        exit(4);
    }
    curr_bytes_in_use = 0;
    max_bytes_in_use = 0;
    if (autorun)
    {
        auto_run_trace(trace, display_utilization, run_check_heap, 0);
    }
    else
    {
        interactive_run_trace(trace, display_utilization, run_check_heap);
    }
    print_coalesce_stats();
    free_trace(trace);

    if (!uses_bins())
    {
        printf("Did not use all bins.\n");
        exit(3);
    }

    if(run_deferred_check) {
        printf("%d\n", test_coalesce());
    }
}