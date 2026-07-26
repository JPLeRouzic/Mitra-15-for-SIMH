
#include "mitra_defs.h"
#include "mitra_io.h"
#include "mitra_cpu.h"

extern int susp_stack_ptr;
extern SuspContext susp_stack[];
extern UNIT cpu_unit ;
extern t_addr cpt_base;
extern t_value M[];

static int get_highest_interrupt(void);

/* ========== Trap mechanism (manual section II-8.3) ========== *
 *
 * Trap causes: VM=mode violation(0), PM=protect violation(1), AI=non-existing addr(2),
 *              PA=parity(3), II=invalid instruction(4), ES=I/O error(5), watchdog(6)
 *
 * ========== Trap Implementation (Section II-8.3) ========== *
 *
 * Trap processing micro-program:
 * 1. Protects bytes 4-9 (words 2,3,4) with faulting context
 * 2. Sets trap cause bit in memory word 1
 * 3. Calls supervisor section 0 via PRTS at address 12
 *
 *  1. Sets the corresponding bit in absolute memory WORD 1 (byte address 2).
 *     Bit layout in word 1 (bit 0=MSB in Mitra convention):
 *       bit 0 = VM (mode violation)
 *       bit 1 = PM (memory protection)
 *       bit 2 = AI (non-existing address)
 *       bit 3 = PA (parity)
 *       bit 4 = II (invalid instruction)
 *       bit 5 = ES (I/O error)
 *  2. Saves L-G', P-G', and indicators into bytes 4-9 (words 2,3,4 in byte addressing,
 *     i.e. WORD addresses 2,3,4 if byte=word on Mitra, but since the machine is
 *     word-addressable "bytes 4-9" = word addresses 2 and 3 and 4):
 *       M[word_addr 2] = P - G'  (faulting instruction address)
 *       M[word_addr 3] = L - G'
 *       M[word_addr 4] = indicators (C, OV, cpu_state.MS packed)
 *  3. Calls supervisor section 0 via PRTS at absolute address 12 (decimal):
 *       cpu_state.PR and cpu_state.MS are forced to 1 (master mode, protection override)
 *       L = PRTS[section_0_Lbase] + G
 *       P = PRTS[section_0_Pbase] + G
 *
 * Trap constants for trap argument:
 *   TRAP_VM=0, TRAP_PM=1, TRAP_AI=2, TRAP_PA=3, TRAP_II=4, TRAP_ES=5
 */
#define TRAP_VM 0 /* Mode violation */
#define TRAP_PM 1 /* Memory protection violation */
#define TRAP_AI 2 /* Non-existing address */
#define TRAP_PA 3 /* Parity error */
#define TRAP_II 4 /* Invalid (non-implemented) instruction */
#define TRAP_ES 5 /* I/O error */

t_stat mitra_trap(int trap, uint16 pc, uint16 * trappc) {
    uint16 trap_word, prts_ptr, sect0_Lbase, sect0_Pbase;
    uint16 ind_word;

    static const char *trap_names[] = {
        "VM(mode violation)", "PM(memory protection)", "AI(non-existing address)",
        "PA(parity error)", "II(invalid instruction)", "ES(I/O error)", "WD(watchdog)"
    };
    MLOG("[TRAP] cause=%d %s  faulting_pc=%05o\n", trap,
             (trap >= 0 && trap <= TRAP_WD) ? trap_names[trap] : "UNKNOWN", pc);
    mitra_log_regs("trap-in");

    /* Step 1: Set trap cause bit in absolute memory word 1 (byte address 2) */
    trap_word = read_word(1);
    trap_word |= (0x8000u >> trap);
    write_word(1, trap_word);

    /* Step 2: 
     *Protect bytes 4-9 with faulting context
     * Save faulting context in words 2, 3, 4 
     * "bytes 4-9" = word addresses 2, 3, 4 (2 bytes per word) */
    write_word(2, (pc - GPRIME) & 0x7FFF);
    write_word(3, (cpu_state.reg_block[cpu_state.curr_bloc].L - GPRIME) & 0x7FFF);
    /* Indicators word: cpu_state.PR, cpu_state.MA, cpu_state.MS, OV, C */
    ind_word = ((cpu_state.PR & 1) << 15) | ((cpu_state.MA & 1) << 14) |
        ((cpu_state.MS & 1) << 13) | ((cpu_state.reg_block[cpu_state.curr_bloc].OV & 1) << 12) | ((cpu_state.reg_block[cpu_state.curr_bloc].C &
            1) << 11);
    write_word(4, ind_word);

    /* Step 3: Call supervisor section 0
     * Absolute address 12 (decimal) holds the PRTS pointer.
     * PRTS entry for section N is at PRTS_base - 4*N:
     *   word 0 (at PRTS_base - 4*N)   = P-base of section
     *   word 1 (at PRTS_base - 4*N+2) = L-base of section
     * Section 0 entry is at PRTS_base itself (N=0). 
     */
    prts_ptr = read_word(6);  /* PRTS pointer at address 6 */
    if (prts_ptr >= MAX_MEM_WORDS) {
        MLOG("[TRAP] ** FATAL ** PRTS pointer %05o out of range (MAX_MEM_WORDS=%05o), cannot dispatch trap %d\n",
                 prts_ptr, MAX_MEM_WORDS, trap);
        return SCPE_STOP;  /* Fatal: no PRTS */
    }
    
    sect0_Pbase = read_word(prts_ptr);
    sect0_Lbase = read_word(prts_ptr + 1);
    
    /* Force master mode with protection override */
    cpu_state.MS = 1;
    cpu_state.PR = 1;
    cpu_state.MA = 1;
    
    cpu_state.reg_block[cpu_state.curr_bloc].L = (sect0_Lbase + cpu_state.reg_block[cpu_state.curr_bloc].G) & 0x7FFF;
    cpu_state.reg_block[cpu_state.curr_bloc].P = (sect0_Pbase + cpu_state.reg_block[cpu_state.curr_bloc].G) & 0x7FFF;
    
    *trappc = pc;
    cpu_state.trap_pending = FALSE;
    cpu_state.trp_req_bits = 0;

    MLOG("[TRAP] -> launching supervisor section 0: PRTS=%05o Pbase=%05o Lbase=%05o  new P=%05o new L=%05o (forced cpu_state.MS=1 cpu_state.PR=1 cpu_state.MA=1)\n",
             prts_ptr, sect0_Pbase, sect0_Lbase, cpu_state.reg_block[cpu_state.curr_bloc].P, cpu_state.reg_block[cpu_state.curr_bloc].L);
    mitra_log_regs("trap-out");
    
    return SCPE_OK;
}

/* ========== Suspension System (Section II-8.2) ========== */
/*
 * Suspension system interrupts micro-program to handle urgent I/O.
 * Stack depth: 4 levels
 * Saves: cpu_state.U, J, T registers and B, Tz, To, Ao indicators
 */
t_stat mitra_suspension_request(uint16 susp_level) {
    if (susp_level >= 32) {
        MLOG("[SUSP] request level=%d ** REJECTED (out of 0..31 range) **\n", susp_level);
        return SCPE_ARG;
    }

    MLOG("[SUSP] request level=%d queued (pending mask was %08X)\n", susp_level, cpu_state.susp_req_bits);
    cpu_state.susp_req_bits |= (1u << susp_level);
    cpu_state.susp_pending = TRUE;
    
    return SCPE_OK;
}

t_stat mitra_suspension_process(void) {
    if (!cpu_state.susp_pending || susp_stack_ptr >= SUSP_STACK_DEPTH) {
        if (cpu_state.susp_pending)
            MLOG("[SUSP] process: pending mask=%08X but stack full (depth=%d) -> deferred\n",
                     cpu_state.susp_req_bits, susp_stack_ptr);
        return SCPE_OK;
    }
    
    /* Find highest priority suspension request */
    int i;
    for (i = 31; i >= 0; i--) {
        if (cpu_state.susp_req_bits & (1u << i)) {
            cpu_state.susp_active_level = i;
            break;
        }
    }

    MLOG("[SUSP] accepting level=%d (highest of pending mask %08X), stack depth before=%d\n",
             cpu_state.susp_active_level, cpu_state.susp_req_bits, susp_stack_ptr);
    
    /* Save current micro-processor state to suspension stack */
    SuspContext *ctx = &susp_stack[susp_stack_ptr++];
    ctx->U_reg = cpu_state.U;
    ctx->J_reg = cpu_state.curr_bloc;  /* J register selects block */
    ctx->T_reg = 0;          /* T register (micro-PC) - simulated */
    ctx->B_ind = 0;          /* Micro-processor indicators */
    ctx->Tz_ind = 0;
    ctx->To_ind = 0;
    ctx->Ao_ind = 0;
    ctx->saved_bloc = cpu_state.curr_bloc;
    
    /* Clear the request bit */
    cpu_state.susp_req_bits &= ~(1u << cpu_state.susp_active_level);
    
    /* Execute suspension micro-program (device-specific handler) */
    /* This would call the appropriate device suspension handler */
    MLOG("[SUSP] dispatching to device handler io_suspension_dispatch(level=%d), saved cpu_state.U=%06o block=%d\n",
             cpu_state.susp_active_level, ctx->U_reg, ctx->J_reg);
    io_suspension_dispatch(cpu_state.susp_active_level);
    MLOG("[SUSP] returned from io_suspension_dispatch(level=%d)\n", cpu_state.susp_active_level);
    
    /* Restore micro-processor state */
    if (susp_stack_ptr > 0) {
        SuspContext *rctx = &susp_stack[--susp_stack_ptr];
        cpu_state.U = rctx->U_reg;
        cpu_state.curr_bloc = rctx->saved_bloc;
        MLOG("[SUSP] restored cpu_state.U=%06o block=%d, stack depth now=%d, remaining pending mask=%08X\n",
                 cpu_state.U, cpu_state.curr_bloc, susp_stack_ptr, cpu_state.susp_req_bits);
    }
    
    /* Check if more suspensions pending */
    if (cpu_state.susp_req_bits == 0) {
        cpu_state.susp_pending = FALSE;
    }
    
    return SCPE_OK;
}

/*
 * Process suspension request from micro-program level
 * Called from CPU when suspension is pending
 */
void io_suspension_dispatch(uint16 susp_level) {
    /* Route to appropriate device handler based on suspension level */
    /* Suspension levels are device-specific */
    MLOG("[IO-SUSP] dispatch level=%d\n", susp_level);
    switch (susp_level) {
        case 0:  /* Example: DRI disk suspension */
            /* dri_suspension_handler(); */
            MLOG("  -> level 0 (DRI disk) ** no handler registered, request silently dropped **\n");
            break;
        case 1:  /* Example: SAGEM disk suspension */
            /* sagem_suspension_handler(); */
            MLOG("  -> level 1 (SAGEM disk) ** no handler registered, request silently dropped **\n");
            break;
        case 2:  /* Example: Printer suspension */
            /* printer_suspension_handler(); */
            MLOG("  -> level 2 (Printer) ** no handler registered, request silently dropped **\n");
            break;
        /* Add more device suspensions as needed */
        default:
            MLOG("  -> level %d ** unrecognized suspension level, no handler at all **\n", susp_level);
            break;
    }
}


/* ========== Interrupt System (Section II-8.1) ========== */
/*
 * Normal interrupt acceptance:
 * 1. Save context at address from CPT[int_level]
 * 2. Load new context from CPT[int_level]
 * 3. Update R8 (current level register)
 *
 * High-speed interrupt:
 * 1. Save indicators in block 0, register 6
 * 2. Switch to reserved block
 * 3. Load indicators from reserved block
 */
t_stat mitra_interrupt_accept(uint16 int_level, t_bool high_speed) {
    if (int_level >= 32) {
        MLOG("[INT] accept level=%d ** REJECTED (out of 0..31 range) **\n", int_level);
        return SCPE_ARG;
    }

    MLOG("[INT] accept level=%d (vector=%u) high_speed_requested=%d HSINT_unit_flag=%d\n",
             int_level, (int_level < 32) ? int_vec[int_level] : 0,
             cpu_state.high_speed, (cpu_unit.flags & UNIT_HSINT) ? 1 : 0);
    mitra_log_regs("int-in");
    
    if (cpu_state.high_speed && (cpu_unit.flags & UNIT_HSINT)) {
        /* High-speed interrupt (5μs) */
        MLOG("[INT] taking FAST/high-speed path: register-block switch %d -> 6\n", cpu_state.curr_bloc);
        /* Save current indicators in block 0, register 6 */
        uint16 ind_word = ((cpu_state.PR & 1) << 15) | ((cpu_state.MA & 1) << 14) |
                         ((cpu_state.MS & 1) << 13) | ((cpu_state.reg_block[0].OV & 1) << 12) |
                         ((cpu_state.reg_block[0].C & 1) << 11);
        
        /* Store in reserved block (block 6 for high-speed) */
        cpu_state.reg_block[6].V = ind_word;  /* Use V register for indicator save */
        
        /* Switch to reserved block */
        cpu_state.curr_bloc = 6;
        
        /* Load indicators from reserved block */
        ind_word = cpu_state.reg_block[6].V;
        cpu_state.PR = (ind_word >> 15) & 1;
        cpu_state.MA = (ind_word >> 14) & 1;
        cpu_state.MS = (ind_word >> 13) & 1;
        cpu_state.reg_block[cpu_state.curr_bloc].OV = (ind_word >> 12) & 1;
        cpu_state.reg_block[cpu_state.curr_bloc].C = (ind_word >> 11) & 1;
        mitra_log_regs("int-fast-out");
        
    } else {
        MLOG("[INT] taking NORMAL path (context saved/loaded through memory CPT)\n");
        /* Normal interrupt (30μs) */
        cpt_base = (t_addr) M[10];  /* CPT at absolute address 10 */
        if (cpt_base >= MAX_MEM_WORDS) {
            MLOG("[INT] ** FATAL ** CPT base %05o (from M[10]) out of range (MAX_MEM_WORDS=%05o)\n", cpt_base, MAX_MEM_WORDS);
            return SCPE_STOP;
        }
        
        uint16 ctx_ptr = read_word(cpt_base + int_level);
        if (ctx_ptr >= MAX_MEM_WORDS) {
            MLOG("[INT] ** FATAL ** context pointer CPT[%d]=%05o out of range (MAX_MEM_WORDS=%05o)\n",
                     int_level, ctx_ptr, MAX_MEM_WORDS);
            return SCPE_STOP;
        }
        MLOG("[INT] cpt_base=%05o ctx_ptr=CPT[%d]=%05o : saving outgoing context, loading incoming\n",
                 cpt_base, int_level, ctx_ptr);
        
        /* Save current context */
        uint16 ind_word = ((cpu_state.PR & 1) << 15) | ((cpu_state.MA & 1) << 14) |
                         ((cpu_state.MS & 1) << 13) | ((cpu_state.reg_block[cpu_state.curr_bloc].OV & 1) << 12) |
                         ((cpu_state.reg_block[cpu_state.curr_bloc].C & 1) << 11);
        
        write_word(ctx_ptr, ind_word);
        write_word(ctx_ptr + 1, cpu_state.reg_block[cpu_state.curr_bloc].X);
        write_word(ctx_ptr + 2, cpu_state.reg_block[cpu_state.curr_bloc].E);
        write_word(ctx_ptr + 3, cpu_state.reg_block[cpu_state.curr_bloc].A);
        write_word(ctx_ptr + 4, cpu_state.reg_block[cpu_state.curr_bloc].G);
        write_word(ctx_ptr + 5, cpu_state.reg_block[cpu_state.curr_bloc].L);
        write_word(ctx_ptr + 6, cpu_state.reg_block[cpu_state.curr_bloc].P);
        
        /* Switch to new level */
        MLOG("[INT] switching current level %d -> %d\n", cpu_state.int_lvl, int_level);
        cpu_state.int_lvl = int_level;
        
        /* Load new context */
        ind_word = read_word(ctx_ptr);
        cpu_state.PR = (ind_word >> 15) & 1;
        cpu_state.MA = (ind_word >> 14) & 1;
        cpu_state.MS = (ind_word >> 13) & 1;
        cpu_state.reg_block[cpu_state.curr_bloc].OV = (ind_word >> 12) & 1;
        cpu_state.reg_block[cpu_state.curr_bloc].C = (ind_word >> 11) & 1;
        cpu_state.reg_block[cpu_state.curr_bloc].X = read_word(ctx_ptr + 1);
        cpu_state.reg_block[cpu_state.curr_bloc].E = read_word(ctx_ptr + 2);
        cpu_state.reg_block[cpu_state.curr_bloc].A = read_word(ctx_ptr + 3);
        cpu_state.reg_block[cpu_state.curr_bloc].G = read_word(ctx_ptr + 4);
        cpu_state.reg_block[cpu_state.curr_bloc].L = read_word(ctx_ptr + 5);
        cpu_state.reg_block[cpu_state.curr_bloc].P = read_word(ctx_ptr + 6);
        MLOG("[INT] program launched at level %d: P=%05o L=%05o (from CPT[%d]=%05o)\n",
                 int_level, cpu_state.reg_block[cpu_state.curr_bloc].P, cpu_state.reg_block[cpu_state.curr_bloc].L, int_level, ctx_ptr);
        mitra_log_regs("int-out");
    }
    
    /* Clear interrupt request */
    cpu_state.intrp_level &= ~(1u << int_level);
    MLOG("[INT] cleared request bit for level=%d, remaining pending mask=%08X\n", int_level, cpu_state.intrp_level);
    
    return SCPE_OK;
}

/*
 * DIT - De-activate Interrupt (Section VII-12)
 * Returns from interrupt subroutine
 */
t_stat mitra_interrupt_return(t_bool high_speed) {
    MLOG("[INT-RET] %s requested\n", cpu_state.high_speed ? "DITR (fast)" : "DIT (normal)");
    mitra_log_regs("ditret-in");
    if (cpu_state.high_speed) {
        /* DITR - Return from high-speed interrupt */
        /* Save indicators in reserved block */
        uint16 ind_word = ((cpu_state.PR & 1) << 15) | ((cpu_state.MA & 1) << 14) |
                         ((cpu_state.MS & 1) << 13) | ((cpu_state.reg_block[cpu_state.curr_bloc].OV & 1) << 12) |
                         ((cpu_state.reg_block[cpu_state.curr_bloc].C & 1) << 11);
        cpu_state.reg_block[cpu_state.curr_bloc].V = ind_word;
        
        /* Return to block 0 */
        cpu_state.curr_bloc = 0;
        
        /* Restore indicators from block 0, register 6 */
        ind_word = cpu_state.reg_block[6].V;
        cpu_state.PR = (ind_word >> 15) & 1;
        cpu_state.MA = (ind_word >> 14) & 1;
        cpu_state.MS = (ind_word >> 13) & 1;
        cpu_state.reg_block[cpu_state.curr_bloc].OV = (ind_word >> 12) & 1;
        cpu_state.reg_block[cpu_state.curr_bloc].C = (ind_word >> 11) & 1;
        MLOG("[INT-RET] DITR complete, register block switched back to 0\n");
        mitra_log_regs("ditret-fast-out");
        
    } else {
        /* DIT - Return from normal interrupt */
        cpt_base = M[10];
        if (cpt_base >= MAX_MEM_WORDS) {
            MLOG("[INT-RET] ** FATAL ** CPT base %05o out of range\n", cpt_base);
            return SCPE_STOP;
        }
        
        uint16 ctx_ptr = read_word(cpt_base + cpu_state.int_lvl);
        if (ctx_ptr >= MAX_MEM_WORDS) {
            MLOG("[INT-RET] ** FATAL ** context pointer CPT[%d]=%05o out of range\n", cpu_state.int_lvl, ctx_ptr);
            return SCPE_STOP;
        }
        MLOG("[INT-RET] leaving level=%d, saving its context to CPT[%d]=%05o\n", cpu_state.int_lvl, cpu_state.int_lvl, ctx_ptr);
        
        /* Save current context */
        uint16 ind_word = ((cpu_state.PR & 1) << 15) | ((cpu_state.MA & 1) << 14) |
                         ((cpu_state.MS & 1) << 13) | ((cpu_state.reg_block[cpu_state.curr_bloc].OV & 1) << 12) |
                         ((cpu_state.reg_block[cpu_state.curr_bloc].C & 1) << 11);
        
        write_word(ctx_ptr, ind_word);
        write_word(ctx_ptr + 1, cpu_state.reg_block[cpu_state.curr_bloc].X);
        write_word(ctx_ptr + 2, cpu_state.reg_block[cpu_state.curr_bloc].E);
        write_word(ctx_ptr + 3, cpu_state.reg_block[cpu_state.curr_bloc].A);
        write_word(ctx_ptr + 4, cpu_state.reg_block[cpu_state.curr_bloc].G);
        write_word(ctx_ptr + 5, cpu_state.reg_block[cpu_state.curr_bloc].L);
        write_word(ctx_ptr + 6, cpu_state.reg_block[cpu_state.curr_bloc].P);
        
        /* Find next highest pending interrupt */
        int next_lvl = -1;
        for (int i = 31; i >= 0; i--) {
            if (cpu_state.intrp_level & (1u << i)) {
                next_lvl = i;
                break;
            }
        }
        
        if (next_lvl >= 0) {
            /* Accept next interrupt */
            MLOG("[INT-RET] another interrupt pending, resuming level=%d\n", next_lvl);
            cpu_state.int_lvl = next_lvl;
            ctx_ptr = read_word(cpt_base + cpu_state.int_lvl);
            
            ind_word = read_word(ctx_ptr);
            cpu_state.PR = (ind_word >> 15) & 1;
            cpu_state.MA = (ind_word >> 14) & 1;
            cpu_state.MS = (ind_word >> 13) & 1;
            cpu_state.reg_block[cpu_state.curr_bloc].OV = (ind_word >> 12) & 1;
            cpu_state.reg_block[cpu_state.curr_bloc].C = (ind_word >> 11) & 1;
            cpu_state.reg_block[cpu_state.curr_bloc].X = read_word(ctx_ptr + 1);
            cpu_state.reg_block[cpu_state.curr_bloc].E = read_word(ctx_ptr + 2);
            cpu_state.reg_block[cpu_state.curr_bloc].A = read_word(ctx_ptr + 3);
            cpu_state.reg_block[cpu_state.curr_bloc].G = read_word(ctx_ptr + 4);
            cpu_state.reg_block[cpu_state.curr_bloc].L = read_word(ctx_ptr + 5);
            cpu_state.reg_block[cpu_state.curr_bloc].P = read_word(ctx_ptr + 6);
            MLOG("[INT-RET] program resumed at level %d: P=%05o L=%05o\n",
                     next_lvl, cpu_state.reg_block[cpu_state.curr_bloc].P, cpu_state.reg_block[cpu_state.curr_bloc].L);
        } else {
            /* Return to level 0 */
            MLOG("[INT-RET] no more interrupts pending, returning to level 0\n");
            cpu_state.int_lvl = 0;
        }
        mitra_log_regs("ditret-out");
    }
    
    return SCPE_OK;
}


