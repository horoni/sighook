/*
 * Copyright (c) 2026 horoni (https://github.com/horoni)
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef HORONI_SIGHOOK_EMIT_H
#define HORONI_SIGHOOK_EMIT_H

#include <stdint.h>
#include <stdbool.h>

void emit_quad(uint32_t *tramp_out, int *words, uint64_t quad);
void emit_abs_jmp(uint32_t *tramp_out, int *words, uint64_t target);
int relocate_insn(uint32_t insn, uint64_t orig_pc, uint32_t *tramp_out);

#endif /* HORONI_SIGHOOK_EMIT_H */
