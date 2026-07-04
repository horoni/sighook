/*
 * Copyright (c) 2026 horoni (https://github.com/horoni)
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>

#include "sighook.h"

#include "assert_.h"

volatile int g_hook_hits = 0;
volatile int dummy = 0;

void test_hook_cb(ucontext_t *uc) {
    (void)uc;
    g_hook_hits++;
}

__attribute__((noinline))
int add(int a, int b) {
    return a + b;
}

int main(void) {
    ASSERT(sg_init(&sg_alloc_mmap) == true);

    ASSERT(sg_inline((void *)add, (hook_cb_t)test_hook_cb) == true);

    dummy = add(1, 2);
    dummy = add(2, 2);

    ASSERT(g_hook_hits == 2);

    ASSERT(sg_unhook((void *)add) == true);

    dummy = add(3, 2);
    dummy = add(4, 2);

    ASSERT(g_hook_hits == 2);

    printf("[OK] Unhook\n");

    return 0;
}
