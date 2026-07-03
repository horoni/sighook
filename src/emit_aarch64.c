/*
 * Copyright (c) 2026 horoni (https://github.com/horoni)
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

void emit_quad(uint32_t *tramp_out, int *words, uint64_t quad) {
    /* .quad quad*/
    memcpy(&tramp_out[(*words)], &quad, sizeof(quad));
    *words += 2;
}

void emit_abs_jmp(uint32_t *tramp_out, int *words, uint64_t target) {
    /* LDR X16, .+8
     * BR  X16
     * .quad target
     */
    tramp_out[(*words)++] = 0x58000050;
    tramp_out[(*words)++] = 0xD61F0200;
    emit_quad(tramp_out, words, target);
}

/* relocate replaced instruction */
int relocate_insn(uint32_t insn, uint64_t pc, uint32_t *tramp_out) {
    int words = 0;

    /* ADR, ADRP */
    if ((insn & 0x1F000000) == 0x10000000) {
        bool is_adrp = (insn & 0x80000000) != 0;
        uint32_t rd = insn & 0x1F;
        uint32_t immlo = (insn >> 29) & 0x3;
        uint32_t immhi = (insn >> 5) & 0x7FFFF;

        int64_t offset = (((int64_t)((immhi << 2) | immlo)) << 43) >> 43;
        uint64_t target = is_adrp ? ((pc & ~0xFFFULL) + (offset << 12)) : (pc + offset);

        /*  0: LDR   Xd,   .+8
         *  4: B     .+12
         *  8: .quad target
         */
        tramp_out[words++] = 0x58000040 | rd;
        tramp_out[words++] = 0x14000003;
        emit_quad(tramp_out, &words, target);
        return words;
    }

    /* (literal) LDR 32/64, LDRSW, PRFM */
    if ((insn & 0x3F000000) == 0x18000000) {
        uint32_t rt = insn & 0x1F;
        uint64_t imm19 = (insn >> 5) & 0x7FFFF;
        int64_t offset = ((int64_t)(imm19 << 45)) >> 43;
        uint64_t target = pc + offset;

        /*  0: LDR   X16,   .+8
         *  4: B     .+12
         *  8: .quad target
         */
        tramp_out[words++] = 0x58000050;
        tramp_out[words++] = 0x14000003;
        emit_quad(tramp_out, &words, target);
        switch (insn & 0xC0000000) {
        /* LDR Wt, [X16] */
        case 0x00000000: tramp_out[words++] = 0xB9400200 | rt; break;
        /* LDR Xt, [X16] */
        case 0x40000000: tramp_out[words++] = 0xF9400200 | rt; break;
        /* LDRSW Xt, [X16] */
        case 0x80000000: tramp_out[words++] = 0xB9800200 | rt; break;
        /* PRFM prfop, [X16] */
        case 0xC0000000: tramp_out[words++] = 0xF9800200 | rt; break;
        default: return -1;
        }
        return words;
    }

    /* (literal) (SIMD) LDR */
    if ((insn & 0x3F000000) == 0x1C000000) {
        uint32_t rt = insn & 0x1F;
        uint64_t imm19 = (insn >> 5) & 0x7FFFF;
        int64_t offset = ((int64_t)(imm19 << 45)) >> 43;
        uint64_t target = pc + offset;

        /*  0: LDR   X16,   .+8
         *  4: B     .+12
         *  8: .quad target
         */
        tramp_out[words++] = 0x58000050;
        tramp_out[words++] = 0x14000003;
        emit_quad(tramp_out, &words, target);
        switch (insn & 0xC0000000) {
        /* LDR St, [X16] */
        case 0x00000000: tramp_out[words++] = 0xBD400200 | rt; break;
        /* LDR Dt, [X16] */
        case 0x40000000: tramp_out[words++] = 0xFD400200 | rt; break;
        /* LDR Qt, [X16] */
        case 0x80000000: tramp_out[words++] = 0x3DC00200 | rt; break;
        default: return -1;
        }
        return words;
    }

    /* B, BL */
    if ((insn & 0x7c000000) == 0x14000000) {
        bool is_bl = insn >> 31;
        uint64_t imm26 = insn & 0x3FFFFFF;
        int64_t offset = ((int64_t)(imm26 << 38)) >> 36;

        uint64_t target = pc + offset;

        if (is_bl) {
            uint64_t lr = pc + 4;
            /*  0: LDR   X30, .+20
             *  4: LDR   X16, .+8
             *  8: BR    X16
             * 12: .quad target
             * 20: .quad lr 
             */
            tramp_out[words++] = 0x580000BE;
            emit_abs_jmp(tramp_out, &words, target);
            emit_quad(tramp_out, &words, lr);
        } else {
            /*  0: LDR   X16, .+8
             *  4: BR    X16
             *  8: .quad target
             */
            emit_abs_jmp(tramp_out, &words, target);
        }
        return words;
    }

    /* B.cond */
    if ((insn & 0xFF000000) == 0x54000000) {
        uint64_t imm19 = (insn >> 5) & 0x7FFFF;
        int64_t offset = ((int64_t)(imm19 << 45)) >> 43;
        uint64_t target = pc + offset;

        uint32_t inv_cond = (insn & 0xF) ^ 1;
        uint32_t inv_insn = (insn & 0xFFFFFFF0) | inv_cond;
        inv_insn = (inv_insn & ~0x00FFFFE0) | (5 << 5);

        /*  0: B.inv_cond .+20
         *  4: LDR         X16, .+8
         *  8: BR          X16
         * 12: .quad       target
         */
        tramp_out[words++] = inv_insn;
        emit_abs_jmp(tramp_out, &words, target);
        return words;
    }

    /* CBZ / CBNZ */
    if ((insn & 0x7E000000) == 0x34000000) {
        uint64_t imm19 = (insn >> 5) & 0x7FFFF;
        int64_t offset = ((int64_t)(imm19 << 45)) >> 43;
        uint64_t target = pc + offset;

        uint32_t inv_insn = insn ^ (1 << 24);
        inv_insn = (inv_insn & ~0x00FFFFE0) | (5 << 5);

        /*  0: CBZ(invert) .+20
         *  4: LDR          X16, .+8
         *  8: BR           X16
         * 12: .quad        target
         */
        tramp_out[words++] = inv_insn;
        emit_abs_jmp(tramp_out, &words, target);
        return words;
    }

    /* TBZ / TBNZ */
    if ((insn & 0x7E000000) == 0x36000000) {
        uint64_t imm14 = (insn >> 5) & 0x3FFF;
        int64_t offset = ((int64_t)(imm14 << 50)) >> 48;
        uint64_t target = pc + offset;
        
        uint32_t inv_insn = insn ^ (1 << 24);
        inv_insn = (inv_insn & ~0x0007FFE0) | (5 << 5);

        /*  0: TBZ(invert) .+20
         *  4: LDR          X16, .+8
         *  8: BR           X16
         * 12: .quad        target
         */
        tramp_out[words++] = inv_insn;
        emit_abs_jmp(tramp_out, &words, target);
        return words;
    }

    tramp_out[words++] = insn;
    return words;
}
