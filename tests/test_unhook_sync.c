/*
 * Copyright (c) 2026 horoni (https://github.com/horoni)
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdatomic.h>
#include <stdio.h>
#include <threads.h>
#include <unistd.h>

#include "sighook.h"

#include "assert_.h"

#define THREAD_COUNT 7
#define ITERS 1000000

volatile atomic_int g_hook_hits = 0;
volatile atomic_int dummy = 0;

__attribute__((noinline))
void inc(void) {
    atomic_fetch_add_explicit(&dummy, 1, memory_order_relaxed);
}

int worker(void *arg) {
    (void)arg;
    for(int i = 0; i<ITERS; ++i)
        inc();
    return 0;
}

void test_hook_cb(ucontext_t *uc) {
    (void)uc;
    atomic_fetch_add_explicit(&g_hook_hits, 1, memory_order_relaxed);
}

int main(void) {
    ASSERT(sg_init() == true);
    ASSERT(sg_inline((void *)inc, (hook_cb_t)test_hook_cb) == true);

    thrd_t thrs[THREAD_COUNT];
    for (int i = 0; i < THREAD_COUNT; ++i)
        ASSERT(thrd_create(&thrs[i], worker, NULL) == thrd_success);

    usleep(100000);
    
    ASSERT(sg_unhook((void *)inc) == true);

    for (int i = 0; i < THREAD_COUNT; ++i) {
        thrd_join(thrs[i], NULL);
    }

    int total_dummy = atomic_load(&dummy);
    int total_hits = atomic_load(&g_hook_hits);

    ASSERT(total_dummy == THREAD_COUNT * ITERS);
    ASSERT(total_hits > 0);
    ASSERT(total_hits < total_dummy);
    
    printf("[OK] Unhook sync %d %d\n", total_dummy, total_hits);
    return 0;
}
