/*
 * Copyright (c) 2026 horoni (https://github.com/horoni)
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <assert.h>

#include "sighook.h"
#include "test_asm.h"

volatile int g_hook_hits = 0;

void test_hook_cb(ucontext_t *uc) {
    (void)uc;
    g_hook_hits++;
}

int main(void) {
    assert(sg_init() == true);

    /* Test ADR */
    g_hook_hits = 0;
    assert(sg_inline((void *)test_adr, test_hook_cb) == true);
    assert(test_adr() == 0x1122334455667788);
    assert(g_hook_hits == 1);
    printf("[OK] ADR\n");

    /* Test LDR */
    g_hook_hits = 0;
    assert(sg_inline((void *)test_ldr, test_hook_cb) == true);
    assert(test_ldr() == 0xDEADBEEFCAFEBABE);
    assert(g_hook_hits == 1);
    printf("[OK] LDR (literal)\n");

    /* Test LDR SIMD */
    g_hook_hits = 0;
    assert(sg_inline((void *)test_ldr_simd, test_hook_cb) == true);
    assert(test_ldr_simd() == 0xCAFEBABEDEADBEEF);
    assert(g_hook_hits == 1);
    printf("[OK] LDR SIMD (literal)\n");

    /* Test B */
    g_hook_hits = 0;
    assert(sg_inline((void *)test_b, test_hook_cb) == true);
    assert(test_b() == 42);
    assert(g_hook_hits == 1);
    printf("[OK] B\n");

    /* CBZ */
    g_hook_hits = 0;
    assert(sg_inline((void *)test_cbz, test_hook_cb) == true);
    
    assert(test_cbz(0) == 88); 
    assert(test_cbz(1) == 77); 
    
    assert(g_hook_hits == 2);
    printf("[OK] CBZ\n");

    /* Test TBZ */
    g_hook_hits = 0;
    assert(sg_inline((void *)test_tbz, test_hook_cb) == true);
    
    assert(test_tbz(0) == 200);
    assert(test_tbz(8) == 100);
    
    assert(g_hook_hits == 2);
    printf("[OK] TBZ\n");

    /* B.cond */
    g_hook_hits = 0;
    assert(sg_inline((void *)test_bcond_hook_point, test_hook_cb) == true);
    
    assert(test_bcond(5) == 1);
    assert(test_bcond(99) == 0);
    
    assert(g_hook_hits == 2);
    printf("[OK] B.cond\n");

    return 0;
}
