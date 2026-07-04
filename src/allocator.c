#include <stdatomic.h>
#include <sys/mman.h>
#include <unistd.h>

#include "sighook.h"

#define MAX_HOOKS 128

static char *g_mmap_pool = NULL;
static atomic_bool g_mmap_own[MAX_HOOKS];

void *mmap_alloc(size_t size) {
    if (!g_mmap_pool || g_mmap_pool == MAP_FAILED) {
        g_mmap_pool = mmap(NULL, sysconf(_SC_PAGESIZE) * 4,
                           PROT_READ | PROT_WRITE | PROT_EXEC,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (g_mmap_pool == MAP_FAILED) return NULL;
    }

    for (int i = 0; i < MAX_HOOKS; i++) {
        bool expected = false;
        if (atomic_compare_exchange_strong(&g_mmap_own[i], &expected, true)) {
            return &g_mmap_pool[i * 64];
        }
    }
    return NULL;
}

void mmap_free(void *address, size_t size) {
    if (address >= (void *)g_mmap_pool && address <= (void *)(g_mmap_pool + sysconf(_SC_PAGESIZE) * 4)) {
        int idx = ((char *)address - g_mmap_pool) / 64;
        atomic_store(&g_mmap_own[idx], false);
    }
}

const sg_allocator_t sg_alloc_mmap = {.alloc = mmap_alloc, .free = mmap_free};
