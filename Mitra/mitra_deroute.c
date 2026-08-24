
#include "mitra_defs.h"
#include "mitra_io.h"
#include "mitra_cpu.h"

extern int susp_stack_ptr;
extern UNIT cpu_unit ;
extern t_addr cpt_base;
extern t_value M[];
extern int susp_stack_ptr;

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

/* mitra_deroute.c */
int mitra_resolve_trap_cause(uint32 trp_req_bits) {
    int i;
    for (i = TRAP_VM; i <= TRAP_ES; i++) {
        if (trp_req_bits & (1u << i))
            return i;
    }
    return -1;
}

const char *mitra_trap_name(int trap) {
    static const char *trap_names[] = {
        "VM(mode violation)", "PM(memory protection)", "AI(non-existing address)",
        "PA(parity error)", "II(invalid instruction)", "ES(I/O error)", "WD(watchdog)"
    };
    return (trap >= 0 && trap <= TRAP_WD) ? trap_names[trap] : "UNKNOWN";
}

/*
 * Resolve the context-save pointer for a given interrupt level from the
 * in-memory Context Pointer Table (CPT). CPT base is stored at absolute
 * word address 10 (manual II-8.1). Always goes through read_word() so
 * bounds checks / traps / tracing stay consistent with every other
 * memory access in the simulator.
 *
 * Returns:
 *   SCPE_OK        - *ctx_ptr_out is valid
 *   SCPE_ARG       - level out of 0..31 range
 *   STOP_ILLVEC    - no program connected to this level (CPT entry == 0)
 *   SCPE_STOP      - CPT base or entry address outside physical memory
 */
t_stat cpt_lookup(uint16 level, uint16 *ctx_ptr_out) {
    uint16 base, ptr;

    if (level >= 32)
        return SCPE_ARG;

    base = read_word(10);
    if (base >= MAX_MEM_WORDS) {
        sim_printf("\n[INT] ** FATAL ** CPT base %#05x (from M[10]) out of range (MAX_MEM_WORDS=%#05x)\n",
                 base, MAX_MEM_WORDS);
        return SCPE_STOP;
    }

    ptr = read_word(base + level);
    if (ptr >= MAX_MEM_WORDS) {
        sim_printf("\n[INT] ** FATAL ** context pointer CPT[%d]=%#05x out of range (MAX_MEM_WORDS=%#05x)\n",
                 level, ptr, MAX_MEM_WORDS);
        return SCPE_STOP;
    }
    if (ptr == 0)
        return STOP_ILLVEC;   /* no program connected to this level */

    *ctx_ptr_out = ptr;
    return SCPE_OK;
}

/*
* The trap processing micro-program:
* - protects bytes 4 to 9 of the memory which contain L- and P-register values and the indicators status of the context of the instruction which initiated the trap;
* - signals the cause of the trap by setting a bit in memory word 2;
* - performs a call to supervisor section 0.
* Now I don't understand this code
*/
t_stat mitra_trap(int trap, uint16 pc) {
    uint16 trap_word, prts_ptr, sect0_Lbase, sect0_Pbase;
    uint16 ind_word;

    static const char *trap_names[] = {
        "VM(mode violation)", "PM(memory protection)", "AI(non-existing address)",
        "PA(parity error)", "II(invalid instruction)", "ES(I/O error)", "WD(watchdog)"
    };
    sim_printf("\n[TRAP] cause=%d %s  faulting_pc=%#05x\n", trap,
             (trap >= 0 && trap <= TRAP_WD) ? trap_names[trap] : "UNKNOWN", pc);
    sim_printf("trap-in\n");

    /* Step 1: Set trap cause bit in absolute memory word 1 (byte address 2) */
    trap_word = read_word(1);
    trap_word |= (0x8000u >> trap); //  the bit layout is "bit 0 = VM, bit 1 = PM, bit 2 = AI, bit 3 = PA, bit 4 = II, bit 5 = ES". 
    write_word(1, trap_word); // FIXME IS IT 0 OR 1?

    /* Step 2: 
     *Protect bytes 4-9 with faulting context
     * Save faulting context in words 2, 3, 4 
     * "bytes 4-9" = word addresses 2, 3, 4 (2 bytes per word) 
     */
    write_word(2, (pc - GPRIME) & 0x7FFF); // 
    write_word(3, (cpu_state.reg_L - GPRIME) & 0x7FFF);
    /* Indicators word: cpu_state.PR, cpu_state.MA, cpu_state.MS, OV, C */
    ind_word = ((cpu_state.PR & 1) << 15) | ((cpu_state.MA & 1) << 14) |
        ((cpu_state.MS & 1) << 13) | ((cpu_state.OV & 1) << 12) | ((cpu_state.C &
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
        sim_printf("\n[TRAP] ** FATAL ** PRTS pointer %#05x out of range (MAX_MEM_WORDS=%#05x), cannot dispatch trap %d\n",
                 prts_ptr, MAX_MEM_WORDS, trap);
        return SCPE_STOP;  /* Fatal: no PRTS */
    }
    
    sect0_Pbase = read_word(prts_ptr);
    sect0_Lbase = read_word(prts_ptr + 1);
    
    /* Force master mode with protection override */
    cpu_state.MS = 1;
    cpu_state.PR = 1;
    cpu_state.MA = 1;
    
    cpu_state.reg_L = (sect0_Lbase + cpu_state.reg_G) & 0x7FFF;
    cpu_state.reg_P = (sect0_Pbase + cpu_state.reg_G) & 0x7FFF; // Performs a call to supervisor section 0.
    
//    *trappc = pc;
    cpu_state.trap_pending = FALSE;
    cpu_state.trp_req_bits = 0;

    sim_printf("\n[TRAP] -> launching supervisor section 0: PRTS=%#05x Pbase=%#05x Lbase=%#05x  new P=%#05x new L=%#05x (forced cpu_state.MS=1 cpu_state.PR=1 cpu_state.MA=1)\n",
             prts_ptr, sect0_Pbase, sect0_Lbase, cpu_state.reg_P, cpu_state.reg_L);
    sim_printf("trap-out\n");
    
    return SCPE_OK;
}

/* ========== Suspension System (Section II-8.2) ========== */
/*
 * Suspension system interrupts micro-program to handle urgent I/O.
 * Stack depth: 4 levels
 * Saves: U, J, T registers and B, Tz, To, Ao indicators
 */
t_stat mitra_suspension_request(uint16 susp_level) {
    if (susp_level >= 32) {
        sim_printf("\n[SUSP] request level=%d ** REJECTED (out of 0..31 range) **\n", susp_level);
        return SCPE_ARG;
    }

    sim_printf("\n[SUSP] request level=%d queued (pending mask was %08X)\n", susp_level, cpu_state.susp_req_bits);
    cpu_state.susp_req_bits |= (1u << susp_level);
    cpu_state.susp_pending = TRUE;
    
    return SCPE_OK;
}

t_stat mitra_suspension_process(void) {
    if (!cpu_state.susp_pending || susp_stack_ptr >= SUSP_STACK_DEPTH) {
        if (cpu_state.susp_pending)
            sim_printf("\n[SUSP] process: pending mask=%08X but stack full (depth=%d) -> deferred\n",
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

    sim_printf("\n[SUSP] accepting level=%d (highest of pending mask %08X), stack depth before=%d\n",
             cpu_state.susp_active_level, cpu_state.susp_req_bits, susp_stack_ptr);
    
    /* Save current micro-processor state to suspension stack */
    cpu_state.SuspensionStack[susp_stack_ptr].U_reg = cpu_state.U;
    cpu_state.SuspensionStack[susp_stack_ptr].J_reg = cpu_state.J_reg;  /* J register selects block */
    cpu_state.SuspensionStack[susp_stack_ptr].T_reg = 0;          /* T register (micro-PC) - simulated */
    cpu_state.SuspensionStack[susp_stack_ptr].B_ind = 0;          /* Micro-processor indicators */
    cpu_state.SuspensionStack[susp_stack_ptr].Tz_ind = 0;
    cpu_state.SuspensionStack[susp_stack_ptr].To_ind = 0;
    cpu_state.SuspensionStack[susp_stack_ptr].Ao_ind = 0;
    cpu_state.SuspensionStack[susp_stack_ptr].saved_bloc = cpu_state.SuspensionStack[susp_stack_ptr].J_reg;
    susp_stack_ptr++;
    
    /* Clear the request bit */
    cpu_state.susp_req_bits &= ~(1u << cpu_state.susp_active_level);
    
    /* Execute suspension micro-program (device-specific handler) */
    /* This would call the appropriate device suspension handler */
    sim_printf("\n[SUSP] dispatching to device handler io_suspension_dispatch(level=%d), saved cpu_state.U=%06o block=%d\n",
             cpu_state.susp_active_level, cpu_state.SuspensionStack[susp_stack_ptr].U_reg, cpu_state.SuspensionStack[susp_stack_ptr].J_reg);
    io_suspension_dispatch(cpu_state.susp_active_level);
    sim_printf("\n[SUSP] returned from io_suspension_dispatch(level=%d)\n", cpu_state.susp_active_level);
    
    /* Restore micro-processor state */
    if (susp_stack_ptr > 0) {
        --susp_stack_ptr;
        cpu_state.U = cpu_state.SuspensionStack[susp_stack_ptr].U_reg;
        cpu_state.SuspensionStack[susp_stack_ptr].J_reg = cpu_state.SuspensionStack[susp_stack_ptr].saved_bloc;
        sim_printf("\n[SUSP] restored cpu_state.U=%06o block=%d, stack depth now=%d, remaining pending mask=%08X\n",
                 cpu_state.U, cpu_state.SuspensionStack[susp_stack_ptr].J_reg, susp_stack_ptr, cpu_state.susp_req_bits);
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
 * FIXME The io_suspension_dispatch() function must route DRI_SUSP_LEVEL (defined as 0 in IO_DRI_fix_disk.c) to dri_suspension_handler(unit)
 */
void io_suspension_dispatch(uint16 susp_level) {
    /* Route to appropriate device handler based on suspension level */
    /* Suspension levels are device-specific */
    sim_printf("\n[IO-SUSP] dispatch level=%d\n", susp_level);
    switch (susp_level) {
        case 0:  /* Example: DRI disk suspension */
            /* dri_suspension_handler(); */
            sim_printf("\n  -> level 0 (DRI disk) ** no handler registered, request silently dropped **\n");
            break;
        case 1:  /* Example: SAGEM disk suspension */
            /* sagem_suspension_handler(); */
            sim_printf("\n  -> level 1 (SAGEM disk) ** no handler registered, request silently dropped **\n");
            break;
        case 2:  /* Example: Printer suspension */
            /* printer_suspension_handler(); */
            sim_printf("\n  -> level 2 (Printer) ** no handler registered, request silently dropped **\n");
            break;
        /* Add more device suspensions as needed */
        default:
            sim_printf("\n  -> level %d ** unrecognized suspension level, no handler at all **\n", susp_level);
            break;
    }
}


/* ========== Interrupt System (Section II-8.1) ========== */
/*
The Mitra-15 does not simply save a few registers on the stack when an interrupt occurs as most microprocessors do.
Instead, every interrupt level owns one or more complete execution contexts.
Each program must reserve a memory area where are stored contexts for each interrupt.
Register 8 contains the priority level of the running task.
Interrupts are not processed immediately when a peripheral raised them.
It's only when the CPU reaches a special microinstruction that the interrupt maybe accepted.
It depends:
	- is an interrupt pending?
	- are interrupts enabled?
	- is the new interrupt priority higher than the current level?
* If yes, the interrupt is accepted.
* If the interrupt level is lower than that of the current program the interrupt is placed in waiting state until the upper level is deactivated.
The acceptance of an interrupt causes a branch to the corresponding subroutine.
Each interrupt level is associated to a memory address containing the context pointer.
The CPU loads the new context of the task associated with this interrupt level.
=> The interrupt handler is not merely a function, it is a complete task.
It has its own registers, program counter, variables.
At the end of the interupting task, The OIT instruction acknowledges the interrupt, saves its context and selects another runnable task.
one interrupt level can contain several tasks. OIT rotates between them.

/* High-speed interrupts *
Normally the CPU store registers in memory, then loads the new context from memory, which costs time.
Instead, the optional high-speed interrupt uses another hardware register block.
When this "high-speed task" is acknowledged, the control is returned to the interrupted task through a special OITR instruction by-passing the usual context swapping in
block O.

• Acknowledgment of a normal interrupt is performed by the final DIT instruction of the interrupt subroutine
and includes the following operations:
- Context of currently processed interrupt is saved in core memory.
- Corresponding level is de-activated.
- Waiting level is accepted and R8 is updated.
- Context elements corresponding to the new level are loaded in the registers.
*/
t_stat mitra_interrupt_accept(uint16 int_level, t_bool high_speed) {
    if (int_level >= 32) {
        sim_printf("\n[INT] accept level=%d ** REJECTED (out of 0..31 range) **\n", int_level);
        return SCPE_ARG;
    }

    sim_printf("\n[INT] accept level=%d high_speed_requested=%d HSINT_unit_flag=%d\n",
             int_level, cpu_state.high_speed, (cpu_unit.flags & UNIT_HSINT) ? 1 : 0);
    sim_printf("int-in\n");
    
    if (cpu_state.high_speed && (cpu_unit.flags & UNIT_HSINT)) {
    	/*
    	* Fast interrupt (5μs) registers are not saved, but indicators are saved
    	* Acceptance of the high-speed interrupt includes the following operations:
	- Normal interrupts are placed in waiting status until acknowledgment of the high-speed interrupt.
	- Current indicators are saved in register 6 of block O.
	- R12 is loaded with the number of the block which is reserved for high-speed interrupt processing.
	- Indicators are loaded with the contents of register 6 in the reserved block.
	*/
        sim_printf("\n[INT] taking FAST/high-speed path: register-block switch %d -> 6\n", cpu_state.SuspensionStack[susp_stack_ptr].J_reg);
        
        /* Save current indicators in block 0, register 6 */
        cpu_state.reg_block[0][6] = ((cpu_state.PR & 1) << 15) | ((cpu_state.MA & 1) << 14) |
                         ((cpu_state.MS & 1) << 13) | ((cpu_state.OV & 1) << 12) |
                         ((cpu_state.C & 1) << 11);
        
        /* R12 is loaded with the number of the block which is reserved for high-speed interrupt processing */
        cpu_state.reg_12 = 6 ; // FIXME I bet this number is not hard coded but set somehow
        
        /* Switch to reserved block */
        cpu_state.SuspensionStack[susp_stack_ptr].J_reg = 6;
        
        /* Load indicators from reserved block */
        uint16 ind_word = cpu_state.reg_block[cpu_state.reg_12][6];
        cpu_state.PR = (ind_word >> 15) & 1;
        cpu_state.MA = (ind_word >> 14) & 1;
        cpu_state.MS = (ind_word >> 13) & 1;
        cpu_state.OV = (ind_word >> 12) & 1;
        cpu_state.C = (ind_word >> 11) & 1;
        
        /* Load shim registers from reserved block */
        cpu_state.reg_A = cpu_state.reg_block[cpu_state.reg_12][3]; // A
        cpu_state.reg_E = cpu_state.reg_block[cpu_state.reg_12][4]; // E
        cpu_state.reg_X = cpu_state.reg_block[cpu_state.reg_12][5]; // X
        cpu_state.reg_L = cpu_state.reg_block[cpu_state.reg_12][1]; // L
        cpu_state.reg_G = cpu_state.reg_block[cpu_state.reg_12][2]; // G
        cpu_state.reg_P = cpu_state.reg_block[cpu_state.reg_12][0]; // P

        sim_printf("int-fast-out\n");
        
    } else {
    	/*
    	* Normal interrupt (30μs)
    	* Registers of block 0 are saved in memory
	* Acceptance of a normal interrupt includes the following operations :
	- Context of currently executed level (specified by R8) is saved in core memory.
	- Calling level (Na) is accepted and R8 is updated.
	- Context elements corresponding to Na are loaded in the registers.
	*/
        sim_printf("\n[INT] taking NORMAL path (context saved/loaded through memory CPT)\n");
        
	uint16 ctx_ptr;
	t_stat lk = cpt_lookup(int_level, &ctx_ptr);
	if (lk != SCPE_OK) {
	    sim_printf("\n[INT] ** cannot dispatch interrupt level=%d (cpt_lookup=%d) **\n", int_level, lk);
	    return lk;
	}
        sim_printf("\n[INT] cpt_base=%#05x ctx_ptr=CPT[%d]=%#05x : saving outgoing context, loading incoming\n",
                 cpt_base, int_level, ctx_ptr);
        
        /* Save current context */
        uint16 ind_word = ((cpu_state.PR & 1) << 15) | ((cpu_state.MA & 1) << 14) |
                         ((cpu_state.MS & 1) << 13) | ((cpu_state.OV & 1) << 12) |
                         ((cpu_state.C & 1) << 11);
        
        write_word(ctx_ptr, ind_word);
        write_word(ctx_ptr + 1, cpu_state.reg_X);
        write_word(ctx_ptr + 2, cpu_state.reg_E);
        write_word(ctx_ptr + 3, cpu_state.reg_A);
        write_word(ctx_ptr + 4, cpu_state.reg_G);
        write_word(ctx_ptr + 5, cpu_state.reg_L);
        write_word(ctx_ptr + 6, cpu_state.reg_P);
        
        /* Switch to new level */
        sim_printf("\n[INT] switching current level %d -> %d\n", cpu_state.curr_int_lvl, int_level);
        cpu_state.curr_int_lvl = int_level;
        
        /* Load new context */
        ind_word = read_word(ctx_ptr);
        cpu_state.PR = (ind_word >> 15) & 1;
        cpu_state.MA = (ind_word >> 14) & 1;
        cpu_state.MS = (ind_word >> 13) & 1;
        cpu_state.OV = (ind_word >> 12) & 1;
        cpu_state.C = (ind_word >> 11) & 1;
        cpu_state.reg_X = read_word(ctx_ptr + 1);
        cpu_state.reg_E = read_word(ctx_ptr + 2);
        cpu_state.reg_A = read_word(ctx_ptr + 3);
        cpu_state.reg_G = read_word(ctx_ptr + 4);
        cpu_state.reg_L = read_word(ctx_ptr + 5);
        cpu_state.reg_P = read_word(ctx_ptr + 6);
        sim_printf("\n[INT] program launched at level %d: P=%#05x L=%#05x (from CPT[%d]=%#05x)\n",
                 int_level, cpu_state.reg_P, cpu_state.reg_L, int_level, ctx_ptr);
        sim_printf("int-out\n");
    }
    
    /* Clear interrupt request */
    cpu_state.intrpt_mask &= ~(1u << int_level);
    sim_printf("\n[INT] cleared request bit for level=%d, remaining pending mask=%08X\n", int_level, cpu_state.intrpt_mask);
    
    return SCPE_OK;
}

/*
 * DIT - De-activate Interrupt (Section VII-12)
 * Returns from interrupt subroutine
 *
 • Acknowledgment of the high-speed interrupt is performed by the final DITR instruction of the interrupt
subroutine and includes the following operations:
- Indicators are saved in register 6 of the reserved block.
- R12 is cleared (return to block 0).
- High-speed level is de-activated (normal interrupts are enabled).
- Previous indicators saved in register 6 of block 0 a restored. 
 */
t_stat mitra_interrupt_return(t_bool high_speed) {
    sim_printf("\n[INT-RET] %s requested\n", cpu_state.high_speed ? "DITR (fast)" : "DIT (normal)");
    sim_printf("ditret-in\n");
    if (cpu_state.high_speed) {
        /* DITR - Return from high-speed interrupt */
        /* Save indicators in reserved block */
        uint16 ind_word = ((cpu_state.PR & 1) << 15) | ((cpu_state.MA & 1) << 14) |
                         ((cpu_state.MS & 1) << 13) | ((cpu_state.OV & 1) << 12) |
                         ((cpu_state.C & 1) << 11);
        cpu_state.reg_block[cpu_state.reg_12][6] = ind_word;
        
        /* Return to block 0 */
        cpu_state.SuspensionStack[susp_stack_ptr].J_reg = 0;
        cpu_state.reg_12 = 0;
        
        /* Restore indicators from block 0, register 6 */
        ind_word = cpu_state.reg_block[0][6];
        cpu_state.PR = (ind_word >> 15) & 1;
        cpu_state.MA = (ind_word >> 14) & 1;
        cpu_state.MS = (ind_word >> 13) & 1;
        cpu_state.OV = (ind_word >> 12) & 1;
        cpu_state.C = (ind_word >> 11) & 1;
        
        /* Restore shim registers from block 0 */
        cpu_state.reg_A = cpu_state.reg_block[0][3]; // A
        cpu_state.reg_E = cpu_state.reg_block[0][4]; // E
        cpu_state.reg_X = cpu_state.reg_block[0][5]; // X
        cpu_state.reg_L = cpu_state.reg_block[0][1]; // L
        cpu_state.reg_G = cpu_state.reg_block[0][2]; // G
        cpu_state.reg_P = cpu_state.reg_block[0][0]; // P
              
        sim_printf("\n[INT-RET] DITR complete, register block switched back to 0\n");
        sim_printf("ditret-fast-out\n");
        
    } else {
        /* DIT - Return from normal interrupt */
	uint16 ctx_ptr;
	t_stat lk = cpt_lookup(cpu_state.curr_int_lvl, &ctx_ptr);
	if (lk != SCPE_OK) {
	    sim_printf("\n[INT] ** cannot dispatch interrupt level=%d (cpt_lookup=%d) **\n", cpu_state.curr_int_lvl, lk);
	    return lk;
	}
        sim_printf("\n[INT-RET] leaving level=%d, saving its context to CPT[%d]=%#05x\n", cpu_state.curr_int_lvl, cpu_state.curr_int_lvl, ctx_ptr);
        
        /* Save current context */
        uint16 ind_word = ((cpu_state.PR & 1) << 15) | ((cpu_state.MA & 1) << 14) |
                         ((cpu_state.MS & 1) << 13) | ((cpu_state.OV & 1) << 12) |
                         ((cpu_state.C & 1) << 11);
        
        write_word(ctx_ptr, ind_word); // FIXME we should restore context, this looks suspiciously as saving context
        write_word(ctx_ptr + 1, cpu_state.reg_X);
        write_word(ctx_ptr + 2, cpu_state.reg_E);
        write_word(ctx_ptr + 3, cpu_state.reg_A);
        write_word(ctx_ptr + 4, cpu_state.reg_G);
        write_word(ctx_ptr + 5, cpu_state.reg_L);
        write_word(ctx_ptr + 6, cpu_state.reg_P);
        
        /* Find next highest pending interrupt */
        int next_lvl = -1;
        for (int i = 31; i >= 0; i--) {
            if (cpu_state.intrpt_mask & (1u << i)) {
                next_lvl = i;
                break;
            }
        }
        
        if (next_lvl >= 0) {
            /* Accept next interrupt */
            sim_printf("\n[INT-RET] another interrupt pending, resuming level=%d\n", next_lvl);
            cpu_state.curr_int_lvl = next_lvl;
            ctx_ptr = read_word(cpt_base + cpu_state.curr_int_lvl);
            
            ind_word = read_word(ctx_ptr);
            cpu_state.PR = (ind_word >> 15) & 1;
            cpu_state.MA = (ind_word >> 14) & 1;
            cpu_state.MS = (ind_word >> 13) & 1;
            cpu_state.OV = (ind_word >> 12) & 1;
            cpu_state.C = (ind_word >> 11) & 1;
            cpu_state.reg_X = read_word(ctx_ptr + 1);
            cpu_state.reg_E = read_word(ctx_ptr + 2);
            cpu_state.reg_A = read_word(ctx_ptr + 3);
            cpu_state.reg_G = read_word(ctx_ptr + 4);
            cpu_state.reg_L = read_word(ctx_ptr + 5);
            cpu_state.reg_P = read_word(ctx_ptr + 6);
            sim_printf("\n[INT-RET] program resumed at level %d: P=%#05x L=%#05x\n",
                     next_lvl, cpu_state.reg_P, cpu_state.reg_L);
        } else {
            /* Return to level 0 */
            sim_printf("\n[INT-RET] no more interrupts pending, returning to level 0\n");
            cpu_state.curr_int_lvl = 0;
        }
        sim_printf("ditret-out\n");
    }
    
    return SCPE_OK;
}


