/*
 * Copyright (c) 2026 horoni (https://github.com/horoni)
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef HORONI_SIGHOOK_EMIT_H
#define HORONI_SIGHOOK_EMIT_H

#include <stdint.h>

static inline void flush_cache(void *start, void *end) {
#ifdef __aarch64__
    uint64_t addr = (uint64_t)start & ~0xF;
    for (; addr < (uint64_t)end; addr += 16) {
        __asm__ volatile("dc cvau, %0\n ic ivau, %0\n" :: "r"(addr) : "memory");
    }
    __asm__ volatile("dsb ish\n isb\n" ::: "memory");
#else
    #error "Unsupported arch"
#endif
}

/* returns amount of bytes written. -1 on error. */
int emit_trampoline(void *address, void *tramp_out);

#endif /* HORONI_SIGHOOK_EMIT_H */
