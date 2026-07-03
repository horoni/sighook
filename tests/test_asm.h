/*
 * Copyright (c) 2026 horoni (https://github.com/horoni)
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdint.h>

extern uint64_t test_adr(void);

extern uint64_t test_ldr(void);

extern uint64_t test_ldr_simd(void);

extern uint64_t test_b(void);

extern uint64_t test_cbz(uint64_t arg);

extern uint64_t test_tbz(uint64_t arg);

extern uint64_t test_bcond(uint64_t arg);
extern void test_bcond_hook_point(void);
