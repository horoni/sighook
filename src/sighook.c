/*
 * Copyright (c) 2026 horoni (https://github.com/horoni)
 * SPDX-License-Identifier: BSD-2-Clause
 */

#define _GNU_SOURCE
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <threads.h>
#include <signal.h>
#include <ucontext.h>
#include <stdatomic.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <linux/membarrier.h>

#include "sighook.h"
#include "emit.h"

#define BRK_FUNC_HOOK 0xd4201dc0 /* brk #0xee */
#define MAX_HOOKS 128

typedef enum {
    SG_HOOK_CTX,
    SG_HOOK_DETOUR,
} sg_type;

struct hook_entry {
    void        *target;
    void        *hook;
    void        *trampoline;
    uint32_t     insn;
    sg_type      type;
    atomic_bool  in_use;
    atomic_bool  active;
};

static struct hook_entry g_hooks[MAX_HOOKS];

static struct sigaction g_old_trap;
static atomic_int       g_trap_refs = 0;

static char      *g_tramp_pool = NULL;
static atomic_int g_tramp_idx = 0;

static inline void flush_cache(void *start, void *end) {
    uint64_t addr = (uint64_t)start & ~0xF;
    for (; addr < (uint64_t)end; addr += 16) {
        __asm__ volatile("dc cvau, %0\n ic ivau, %0\n" :: "r"(addr) : "memory");
    }
    __asm__ volatile("dsb ish\n isb\n" ::: "memory");
}

static void unified_trap_handler(int sig, siginfo_t *info, void *context) {
    atomic_fetch_add_explicit(&g_trap_refs, 1, memory_order_acquire);
    ucontext_t *uc = (ucontext_t *)context;
    uint64_t pc = uc->uc_mcontext.pc;
    uint32_t insn = *(uint32_t *)pc;

    if (insn != BRK_FUNC_HOOK) {
        if ((insn & 0xFFE0001F) != 0xD4200000) {
            atomic_fetch_sub_explicit(&g_trap_refs, 1, memory_order_release);
            return;
        }
        goto end;
    }

    for (int i = 0; i < MAX_HOOKS; i++) {
        if (false == atomic_load_explicit(&g_hooks[i].active,
                                          memory_order_acquire))
            continue;
        if (g_hooks[i].target != (void *)pc)
            continue;

        if (SG_HOOK_CTX == g_hooks[i].type) {
            uc->uc_mcontext.pc = (uint64_t)g_hooks[i].trampoline;
            ((hook_cb_t)g_hooks[i].hook)(uc);
        } else if (SG_HOOK_DETOUR == g_hooks[i].type) {
            uc->uc_mcontext.pc = (uint64_t)g_hooks[i].hook;
        }
        atomic_fetch_sub_explicit(&g_trap_refs, 1, memory_order_release);
        return;
    }

end:
    atomic_fetch_sub_explicit(&g_trap_refs, 1, memory_order_release);

    if (g_old_trap.sa_flags & SA_SIGINFO) {
        g_old_trap.sa_sigaction(sig, info, context);
    } else if (g_old_trap.sa_handler == SIG_DFL) {
        signal(sig, SIG_DFL);
        raise(sig);
    } else if (g_old_trap.sa_handler != SIG_IGN) {
        g_old_trap.sa_handler(sig);
    }
}

bool sg_init(void) {
    g_tramp_pool = mmap(NULL, sysconf(_SC_PAGESIZE) * 4,
                        PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (g_tramp_pool == MAP_FAILED) return false;

    syscall(__NR_membarrier, MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED, 0, 0);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = unified_trap_handler;
    sa.sa_flags = SA_SIGINFO;
    sigfillset(&sa.sa_mask);

    sigaction(SIGTRAP, &sa, &g_old_trap);
    return true;
}

static inline bool sg_install(void *address, void *hook, void **origin, sg_type type) {
    if (!address || !hook || !g_tramp_pool) return false;

    int idx = atomic_fetch_add_explicit(&g_tramp_idx, 1, memory_order_relaxed);
    if (idx >= MAX_HOOKS) return false;

    char *tramp = &g_tramp_pool[idx * 64];
    int bytes = emit_trampoline(address, tramp);
    if (bytes < 0) return false;
    flush_cache(tramp, &tramp[bytes]);

    if (origin) {
        *origin = tramp;
    }
    
    for (int i = 0; i < MAX_HOOKS; i++) {
        bool expected = false;

        if (atomic_compare_exchange_strong_explicit(
                &g_hooks[i].in_use, &expected, true, memory_order_acq_rel,
                memory_order_relaxed)) {
          g_hooks[i].target = address;
          g_hooks[i].hook = hook;
          g_hooks[i].trampoline = tramp;
          g_hooks[i].insn = *(uint32_t *)address;
          g_hooks[i].type = type;

          atomic_thread_fence(memory_order_release);

          uintptr_t page_size = sysconf(_SC_PAGESIZE);
          uintptr_t page = (uintptr_t)address & ~(page_size - 1);
          mprotect((void *)page, page_size, PROT_READ | PROT_WRITE | PROT_EXEC);

          atomic_store_explicit((_Atomic uint32_t *)address, BRK_FUNC_HOOK, memory_order_release);

          flush_cache(address, (char *)address + 4);
          mprotect((void *)page, page_size, PROT_READ | PROT_EXEC);

          syscall(__NR_membarrier, MEMBARRIER_CMD_PRIVATE_EXPEDITED, 0, 0);

          atomic_store_explicit(&g_hooks[i].active, true, memory_order_release);
          
          return true;
        }
    }

    return false;
}

bool sg_inline(void *address, hook_cb_t hook) {
    return sg_install(address, (void *)hook, NULL, SG_HOOK_CTX);
}

bool sg_detour(void *address, void *replace_call, void **origin_call) {
    return sg_install(address, replace_call, origin_call, SG_HOOK_DETOUR);
}

bool sg_unhook(void *address) {
    if (!address) return false;

    for (int i = 0; i < MAX_HOOKS; i++) {
        if (atomic_load_explicit(&g_hooks[i].in_use, memory_order_acquire) &&
            g_hooks[i].target == address) {

            uintptr_t page_size = sysconf(_SC_PAGESIZE);
            uintptr_t page = (uintptr_t)address & ~(page_size - 1);
            mprotect((void *)page, page_size, PROT_READ | PROT_WRITE | PROT_EXEC);

            atomic_store_explicit((_Atomic uint32_t *)address, g_hooks[i].insn, memory_order_release);

            flush_cache(address, (char *)address + 4);
            mprotect((void *)page, page_size, PROT_READ | PROT_EXEC);

            syscall(__NR_membarrier, MEMBARRIER_CMD_PRIVATE_EXPEDITED, 0, 0);

            while (atomic_load_explicit(&g_trap_refs, memory_order_acquire)) {
                thrd_yield();
            }

            atomic_store_explicit(&g_hooks[i].active, false, memory_order_release);
            g_hooks[i].target = NULL;
            atomic_store_explicit(&g_hooks[i].in_use, false, memory_order_release);

            return true;
        }
    }

    return false;
}
