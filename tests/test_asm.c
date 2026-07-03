/*
 * Copyright (c) 2026 horoni (https://github.com/horoni)
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdint.h>

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
extern uint64_t test_adr(void);

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
extern uint64_t test_ldr(void);

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
extern uint64_t test_ldr_simd(void);

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
extern uint64_t test_b(void);

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
extern uint64_t test_cbz(uint64_t arg);

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
extern uint64_t test_tbz(uint64_t arg);

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
extern uint64_t test_bcond(uint64_t arg);
extern void test_bcond_hook_point(void);
