#include <stdatomic.h>
#include <sys/mman.h>
#include <unistd.h>

#include "sighook.h"

#define POOL_SIZE sysconf(_SC_PAGESIZE) * 4
#define ALIGN(x)      (((x) + 7) & ~7)
#define VAL_BUSY(val) ((size_t)(val) & 1)
#define VAL_MASK(val) ((size_t)(val) | 1)
#define VAL_PURE(val) ((size_t)(val) & ~((size_t)1))

static char *g_mmap_pool = NULL;
static atomic_bool g_mmap_inited = false;

typedef struct _block_info {
    size_t size;
    struct _block_info *prev, *next;
} block_info;

void *mmap_alloc(size_t size) {
    block_info *tmp_block = NULL, *free_block = NULL;
    size_t remain_size;

    if (!atomic_load(&g_mmap_inited)) {
        g_mmap_pool = mmap(NULL, POOL_SIZE,
                           PROT_READ | PROT_WRITE | PROT_EXEC,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (g_mmap_pool == MAP_FAILED) return NULL;

        tmp_block = (block_info *)g_mmap_pool;
        tmp_block->prev = tmp_block->next = NULL;
        tmp_block->size = POOL_SIZE - sizeof(block_info);
        free_block = tmp_block;

        atomic_store(&g_mmap_inited, true);
    }

    tmp_block = (block_info *)g_mmap_pool;
    while(tmp_block && !free_block) {
        if(!VAL_BUSY(tmp_block->size) && VAL_PURE(tmp_block->size) >= size)
            free_block = tmp_block;
        tmp_block = tmp_block->next;
    }

    if(!free_block)
        return NULL;

    size = ALIGN(size);
    remain_size = free_block->size;
    free_block->size = VAL_MASK(size);

    /* Create next block_info */
    if(remain_size >= size + sizeof(block_info) + ALIGN(1)) {
        tmp_block = (block_info *)((size_t)free_block + sizeof(block_info) + size);
        tmp_block->size = remain_size - size - sizeof(block_info);
        tmp_block->next = free_block->next;
        tmp_block->prev = free_block;

        if (free_block->next)
            free_block->next->prev = tmp_block;
        free_block->next = tmp_block;
    }

    return free_block + 1;
}

void mmap_free(void *address, size_t size) {
    block_info *block, *tmp_block;

    (void)size;

    if (address >= (void *)g_mmap_pool && address <= (void *)(g_mmap_pool + POOL_SIZE)) {
        block = (block_info *)address - 1;

        /* Reset flag */
        block->size = VAL_PURE(block->size);

        /* Merge with next block */
        if(block->next && !VAL_BUSY(block->next->size)) {
            tmp_block = block->next;
            block->size += VAL_PURE(tmp_block->size) + sizeof(block_info);
            if(tmp_block->next) {
                tmp_block->next->prev = block;
                block->next = tmp_block->next;
            }
        }
        /* Merge with previous block */
        if(block->prev && !VAL_BUSY(block->prev->size)) {
            tmp_block = block->prev;
            tmp_block->size += VAL_PURE(block->size) + sizeof(block_info);
            if(block->next) {
                block->next->prev = tmp_block;
                tmp_block->next = block->next;
            }
            else {
                tmp_block->next = NULL;
            }
        }
    }
}

const sg_allocator_t sg_alloc_mmap = {.alloc = mmap_alloc, .free = mmap_free};
