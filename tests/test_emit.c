/*
 * Copyright (c) 2026 horoni (https://github.com/horoni)
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <stdint.h>

#include "sighook.h"

#include "assert_.h"

extern uint64_t test_adr(void);
extern uint64_t test_ldr(void);
extern uint64_t test_ldr_simd(void);
extern uint64_t test_b(void);
extern uint64_t test_cbz(uint64_t arg);
extern uint64_t test_tbz(uint64_t arg);
extern uint64_t test_bcond(uint64_t arg);
extern void test_bcond_hook_point(void);

volatile int g_hook_hits = 0;

void test_hook_cb(ucontext_t *uc) {
    (void)uc;
    g_hook_hits++;
}

int main(void) {
    ASSERT(sg_init() == true);

    /* Test ADR */
    g_hook_hits = 0;
    ASSERT(sg_inline((void *)test_adr, test_hook_cb) == true);
    ASSERT(test_adr() == 0x1122334455667788);
    ASSERT(g_hook_hits == 1);
    printf("[OK] ADR\n");

    /* Test LDR */
    g_hook_hits = 0;
    ASSERT(sg_inline((void *)test_ldr, test_hook_cb) == true);
    ASSERT(test_ldr() == 0xDEADBEEFCAFEBABE);
    ASSERT(g_hook_hits == 1);
    printf("[OK] LDR (literal)\n");

    /* Test LDR SIMD */
    g_hook_hits = 0;
    ASSERT(sg_inline((void *)test_ldr_simd, test_hook_cb) == true);
    ASSERT(test_ldr_simd() == 0xCAFEBABEDEADBEEF);
    ASSERT(g_hook_hits == 1);
    printf("[OK] LDR SIMD (literal)\n");

    /* Test B */
    g_hook_hits = 0;
    ASSERT(sg_inline((void *)test_b, test_hook_cb) == true);
    ASSERT(test_b() == 42);
    ASSERT(g_hook_hits == 1);
    printf("[OK] B\n");

    /* CBZ */
    g_hook_hits = 0;
    ASSERT(sg_inline((void *)test_cbz, test_hook_cb) == true);

    ASSERT(test_cbz(0) == 88);
    ASSERT(test_cbz(1) == 77);

    ASSERT(g_hook_hits == 2);
    printf("[OK] CBZ\n");

    /* Test TBZ */
    g_hook_hits = 0;
    ASSERT(sg_inline((void *)test_tbz, test_hook_cb) == true);

    ASSERT(test_tbz(0) == 200);
    ASSERT(test_tbz(8) == 100);

    ASSERT(g_hook_hits == 2);
    printf("[OK] TBZ\n");

    /* B.cond */
    g_hook_hits = 0;
    ASSERT(sg_inline((void *)test_bcond_hook_point, test_hook_cb) == true);

    ASSERT(test_bcond(5) == 1);
    ASSERT(test_bcond(99) == 0);

    ASSERT(g_hook_hits == 2);
    printf("[OK] B.cond\n");

    return 0;
}

__asm__(
    ".text\n"
    ".align 4\n"
    ".global test_adr\n"
    "test_adr:\n"
    "    adr x0, 1f\n"
    "    ldr x0, [x0]\n"
    "    ret\n"
    "1:\n"
    "    .quad 0x1122334455667788\n"
);

__asm__(
    ".text\n"
    ".align 4\n"
    ".global test_ldr\n"
    "test_ldr:\n"
    "    ldr x0, 1f\n"
    "    ret\n"
    "1:\n"
    "    .quad 0xDEADBEEFCAFEBABE\n"
);

__asm__(
    ".text\n"
    ".align 4\n"
    ".global test_ldr_simd\n"
    "test_ldr_simd:\n"
    "    ldr d0, 1f\n"
    "    fmov x0, d0\n"
    "    ret\n"
    "1:\n"
    "    .quad 0xCAFEBABEDEADBEEF\n"
);

__asm__(
    ".text\n"
    ".align 4\n"
    ".global test_b\n"
    "test_b:\n"
    "    b 1f\n"
    "    mov x0, #0\n"
    "    ret\n"
    "1:\n"
    "    mov x0, #42\n"
    "    ret\n"
);

__asm__(
    ".text\n"
    ".align 4\n"
    ".global test_cbz\n"
    "test_cbz:\n"
    "    cbz x0, 1f\n"
    "    mov x0, #77\n" /* x0 != 0 */
    "    ret\n"
    "1:\n"
    "    mov x0, #88\n" /* x0 == 0 */
    "    ret\n"
);

__asm__(
    ".text\n"
    ".align 4\n"
    ".global test_tbz\n"
    "test_tbz:\n"
    "    tbz x0, #3, 1f\n"
    "    mov x0, #100\n" /* if (x0 & 0b100) */
    "    ret\n"
    "1:\n"
    "    mov x0, #200\n"
    "    ret\n"
);

__asm__(
    ".text\n"
    ".align 4\n"
    ".global test_bcond\n"
    "test_bcond:\n"
    "    cmp x0, #5\n"
    ".global test_bcond_hook_point\n"
    "test_bcond_hook_point:\n"
    "    b.eq 1f\n"
    "    mov x0, #0\n" /* x0 != 5 */
    "    ret\n"
    "1:\n"
    "    mov x0, #1\n"
    "    ret\n"
);

