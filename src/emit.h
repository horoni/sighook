/*
 * Copyright (c) 2026 horoni (https://github.com/horoni)
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef HORONI_SIGHOOK_EMIT_H
#define HORONI_SIGHOOK_EMIT_H

/* returns amount of bytes written. -1 on error. */
int emit_trampoline(void *address, void *tramp_out);

#endif /* HORONI_SIGHOOK_EMIT_H */
