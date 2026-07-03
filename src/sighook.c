/*
 * Copyright (c) 2026 horoni (https://github.com/horoni)
 * SPDX-License-Identifier: BSD-2-Clause
 */

#define _GNU_SOURCE
#include "sighook.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <ucontext.h>
#include <stdatomic.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <linux/membarrier.h>

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
    sg_type      type;
    atomic_bool  in_use;
    atomic_bool  active;
};

static struct hook_entry g_hooks[MAX_HOOKS];

static struct sigaction g_old_trap;

static uint32_t  *g_tramp_pool = NULL;
static atomic_int g_tramp_idx = 0;

static inline void emit_abs_jmp(uint32_t *tramp_out, int *words, uint64_t target);
static int relocate_insn_aarch64(uint32_t insn, uint64_t orig_pc, uint32_t *tramp_out);

static inline void flush_cache(void *start, void *end) {
    uint64_t addr = (uint64_t)start & ~0xF;
    for (; addr < (uint64_t)end; addr += 16) {
        __asm__ volatile("dc cvau, %0\n ic ivau, %0\n" :: "r"(addr) : "memory");
    }
    __asm__ volatile("dsb ish\n isb\n" ::: "memory");
}

static void unified_trap_handler(int sig, siginfo_t *info, void *context) {
  ucontext_t *uc = (ucontext_t *)context;
  uint64_t pc = uc->uc_mcontext.pc;

  if (*(uint32_t *)pc != BRK_FUNC_HOOK)
    goto end;
  for (int i = 0; i < MAX_HOOKS; i++) {
    if (false == atomic_load_explicit(&g_hooks[i].active, memory_order_acquire))
      continue;
    if (g_hooks[i].target != (void *)pc)
      continue;

    if (SG_HOOK_CTX == g_hooks[i].type) {
      uc->uc_mcontext.pc = (uint64_t)g_hooks[i].trampoline;
      ((hook_cb_t)g_hooks[i].hook)(uc);
    } else if (SG_HOOK_DETOUR == g_hooks[i].type) {
      uc->uc_mcontext.pc = (uint64_t)g_hooks[i].hook;
    }
    return;
  }

end:
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
    uint32_t orig_insn = *(uint32_t *)address;

    int idx = atomic_fetch_add_explicit(&g_tramp_idx, 1, memory_order_relaxed);
    if (idx >= MAX_HOOKS) return false;

    uint32_t *tramp = &g_tramp_pool[idx * 16];

    int words = relocate_insn_aarch64(orig_insn, (uint64_t)address, tramp);
    if (words < 0) {
        return false;
    }

    if (words % 2 != 0) tramp[words++] = 0xd503201f;

    uint64_t dest = (uint64_t)address + 4;
    emit_abs_jmp(tramp, &words, dest);

    flush_cache(tramp, &tramp[words]);

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

static inline void emit_quad(uint32_t *tramp_out, int *words, uint64_t quad) {
    /* .quad quad*/
    memcpy(&tramp_out[(*words)], &quad, sizeof(quad));
    *words += 2;
}

static inline void emit_abs_jmp(uint32_t *tramp_out, int *words, uint64_t target) {
    /* LDR X16, .+8
     * BR  X16
     * .quad target
     */
    tramp_out[(*words)++] = 0x58000050;
    tramp_out[(*words)++] = 0xD61F0200;
    emit_quad(tramp_out, words, target);
}

/* relocate replaced instruction */
static int relocate_insn_aarch64(uint32_t insn, uint64_t pc, uint32_t *tramp_out) {
    int words = 0;

    /* ADR, ADRP */
    if ((insn & 0x1F000000) == 0x10000000) {
        bool is_adrp = (insn & 0x80000000) != 0;
        uint32_t rd = insn & 0x1F;
        uint32_t immlo = (insn >> 29) & 0x3;
        uint32_t immhi = (insn >> 5) & 0x7FFFF;

        int64_t offset = (((int64_t)((immhi << 2) | immlo)) << 43) >> 43;
        uint64_t target = is_adrp ? ((pc & ~0xFFFULL) + (offset << 12)) : (pc + offset);

        /*  0: LDR   Xd,   .+8
         *  4: B     .+12
         *  8: .quad target
         */
        tramp_out[words++] = 0x58000040 | rd;
        tramp_out[words++] = 0x14000003;
        emit_quad(tramp_out, &words, target);
        return words;
    }

    /* (literal) LDR 32/64, LDRSW, PRFM */
    if ((insn & 0x3F000000) == 0x18000000) {
        uint32_t rt = insn & 0x1F;
        uint64_t imm19 = (insn >> 5) & 0x7FFFF;
        int64_t offset = ((int64_t)(imm19 << 45)) >> 43;
        uint64_t target = pc + offset;

        /*  0: LDR   X16,   .+8
         *  4: B     .+12
         *  8: .quad target
         */
        tramp_out[words++] = 0x58000050;
        tramp_out[words++] = 0x14000003;
        emit_quad(tramp_out, &words, target);
        switch (insn & 0xC0000000) {
        /* LDR Wt, [X16] */
        case 0x00000000: tramp_out[words++] = 0xB9400200 | rt; break;
        /* LDR Xt, [X16] */
        case 0x40000000: tramp_out[words++] = 0xF9400200 | rt; break;
        /* LDRSW Xt, [X16] */
        case 0x80000000: tramp_out[words++] = 0xB9800200 | rt; break;
        /* PRFM prfop, [X16] */
        case 0xC0000000: tramp_out[words++] = 0xF9800200 | rt; break;
        default: return -1;
        }
        return words;
    }

    /* (literal) (SIMD) LDR */
    if ((insn & 0x3F000000) == 0x1C000000) {
        uint32_t rt = insn & 0x1F;
        uint64_t imm19 = (insn >> 5) & 0x7FFFF;
        int64_t offset = ((int64_t)(imm19 << 45)) >> 43;
        uint64_t target = pc + offset;

        /*  0: LDR   X16,   .+8
         *  4: B     .+12
         *  8: .quad target
         */
        tramp_out[words++] = 0x58000050;
        tramp_out[words++] = 0x14000003;
        emit_quad(tramp_out, &words, target);
        switch (insn & 0xC0000000) {
        /* LDR St, [X16] */
        case 0x00000000: tramp_out[words++] = 0xBD400200 | rt; break;
        /* LDR Dt, [X16] */
        case 0x40000000: tramp_out[words++] = 0xFD400200 | rt; break;
        /* LDR Qt, [X16] */
        case 0x80000000: tramp_out[words++] = 0x3DC00200 | rt; break;
        default: return -1;
        }
        return words;
    }

    /* B, BL */
    if ((insn & 0x7c000000) == 0x14000000) {
        bool is_bl = insn >> 31;
        uint64_t imm26 = insn & 0x3FFFFFF;
        int64_t offset = ((int64_t)(imm26 << 38)) >> 36;

        uint64_t target = pc + offset;

        if (is_bl) {
            uint64_t lr = pc + 4;
            /*  0: LDR   X30, .+20
             *  4: LDR   X16, .+8
             *  8: BR    X16
             * 12: .quad target
             * 20: .quad lr 
             */
            tramp_out[words++] = 0x580000BE;
            emit_abs_jmp(tramp_out, &words, target);
            emit_quad(tramp_out, &words, lr);
        } else {
            /*  0: LDR   X16, .+8
             *  4: BR    X16
             *  8: .quad target
             */
            emit_abs_jmp(tramp_out, &words, target);
        }
        return words;
    }

    /* B.cond */
    if ((insn & 0xFF000000) == 0x54000000) {
        uint64_t imm19 = (insn >> 5) & 0x7FFFF;
        int64_t offset = ((int64_t)(imm19 << 45)) >> 43;
        uint64_t target = pc + offset;

        uint32_t inv_cond = (insn & 0xF) ^ 1;
        uint32_t inv_insn = (insn & 0xFFFFFFF0) | inv_cond;
        inv_insn = (inv_insn & ~0x00FFFFE0) | (5 << 5);

        /*  0: B.inv_cond .+20
         *  4: LDR         X16, .+8
         *  8: BR          X16
         * 12: .quad       target
         */
        tramp_out[words++] = inv_insn;
        emit_abs_jmp(tramp_out, &words, target);
        return words;
    }

    /* CBZ / CBNZ */
    if ((insn & 0x7E000000) == 0x34000000) {
        uint64_t imm19 = (insn >> 5) & 0x7FFFF;
        int64_t offset = ((int64_t)(imm19 << 45)) >> 43;
        uint64_t target = pc + offset;

        uint32_t inv_insn = insn ^ (1 << 24);
        inv_insn = (inv_insn & ~0x00FFFFE0) | (5 << 5);

        /*  0: CBZ(invert) .+20
         *  4: LDR          X16, .+8
         *  8: BR           X16
         * 12: .quad        target
         */
        tramp_out[words++] = inv_insn;
        emit_abs_jmp(tramp_out, &words, target);
        return words;
    }

    /* TBZ / TBNZ */
    if ((insn & 0x7E000000) == 0x36000000) {
        uint64_t imm14 = (insn >> 5) & 0x3FFF;
        int64_t offset = ((int64_t)(imm14 << 50)) >> 48;
        uint64_t target = pc + offset;
        
        uint32_t inv_insn = insn ^ (1 << 24);
        inv_insn = (inv_insn & ~0x0007FFE0) | (5 << 5);

        /*  0: TBZ(invert) .+20
         *  4: LDR          X16, .+8
         *  8: BR           X16
         * 12: .quad        target
         */
        tramp_out[words++] = inv_insn;
        emit_abs_jmp(tramp_out, &words, target);
        return words;
    }

    tramp_out[words++] = insn;
    return words;
}
