#include <stdatomic.h>
#include <sys/mman.h>
#include <unistd.h>

#include "sighook.h"

#define POOL_SIZE sysconf(_SC_PAGESIZE) * 4
#define ALIGN(x)      (((x) + 7) & ~7)
#define VAL_BUSY(val) ((size_t)(val) & 1)
#define VAL_MASK(val) ((size_t)(val) | 1)
#define VAL_PURE(val) ((size_t)(val) & ~((size_t)1))
#define PTR_BUSY(ptr) VAL_BUSY((ptr))
#define PTR_MASK(ptr) (block_info *)VAL_MASK((ptr))
#define PTR_PURE(ptr) (block_info *)VAL_PURE((ptr))

static char *g_mmap_pool = NULL;
static atomic_bool g_mmap_inited = false;

typedef struct _block_info {
    int size;
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
        if(!VAL_BUSY(tmp_block->size))
            free_block = tmp_block;
        tmp_block = tmp_block->next;
    }

    if(!free_block)
        return NULL;

    size = ALIGN(size);
    remain_size = free_block->size;
    free_block->size = VAL_MASK(size);

    /* Create next block_info */
    if(remain_size >= size + sizeof(block_info) + 8 &&
       free_block->next == NULL) {
        tmp_block = (block_info *)((size_t)free_block + size);
        tmp_block->prev = PTR_MASK(free_block);
        tmp_block->next = NULL;
        tmp_block->size = remain_size - sizeof(block_info) * 2;

        free_block->next = tmp_block;
    }

    return free_block + 1;
}

void mmap_free(void *address, size_t size) {
    (void)size;
    if (address >= (void *)g_mmap_pool && address <= (void *)(g_mmap_pool + POOL_SIZE)) {
        /* TODO */
    }
}

const sg_allocator_t sg_alloc_mmap = {.alloc = mmap_alloc, .free = mmap_free};
