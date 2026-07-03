/*
 * Copyright (c) 2026 horoni (https://github.com/horoni)
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef HORONI_SIGHOOK_H
#define HORONI_SIGHOOK_H

#include <signal.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*hook_cb_t)(ucontext_t *ctx);

bool sg_init(void);

/* hook called in signal context, you must use signal-safe functions */
bool sg_inline(void *address, hook_cb_t hook);

bool sg_detour(void *address, void *replace_call, void **origin_call);

#ifdef __cplusplus
}
#endif

#endif /* HORONI_SIGHOOK_H */