/* mitra_cpu.c: CII Mitra 15 CPU simulator for SIMH
 *
 * Based on MITRA 15 Reference Manual (CII, 1973) and documents in Patrick Chour's website
 * 
 * The Mitra-15 is a 16-bit word-addressable computer with:
 * - 16-bit word size
 * - Byte-addressable (even addresses = word boundaries)
 * - Up to 32K words of memory, the bit 0 of program counter is always at 0.
 * - 86 instructions (Mitra-15/30)
 * - 32 interrupt levels
 * - Master/Slave modes with memory protection
 * - Interrupt, fast interrupt, suspension, and trap support
 *
 * based on on SDS 940 CPU simulator

   Copyright (c) 2001-2017, Robert M. Supnik
   Copyright (c) 2026, Jean-Pierre Le Rouzic contact@padiracinnovation.org

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   ROBERT M SUPNIK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of Robert M Supnik shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.
*/


#include "mitra_defs.h"
#include "mitra_cpu.h"
#include "mitra_io.h"

#define PCQ_SIZE        64                              /* must be 2**n */
#define PCQ_MASK        (PCQ_SIZE - 1)
#define PCQ_ENTRY       pcq[pcq_p = (pcq_p - 1) & PCQ_MASK] = pc

#define HIST_XCT        1                               /* instruction */
#define HIST_INT        2                               /* interrupt cycle */
#define HIST_TRP        3                               /* trap cycle */
#define HIST_MIN        64
#define HIST_MAX        65536
#define HIST_NOEA       0x40000000

/* Main memory - declared extern in mitra_cpu.h; this is the one real instance */
t_value M[MAX_MEM_WORDS];

/* CPU state - declared extern in mitra_cpu.h; this is the one real, shared instance. */
CPU_STATE cpu_state;

/* Diagnostic trace-logging toggles - declared extern in mitra_cpu.h; defined once here. */
int32 mitra_log_enable = 1;
int32 mitra_log_inst = 0;
int32 mitra_log_mem = 0;
int32 mitra_log_int = 0;
int32 mitra_log_io = 0;

uint32 xfr_req = 0;                                     /* xfr req */
uint32 ion = 0;                                         /* int enable */
uint32 ion_defer = 0;                                   /* int defer */
uint32 int_req = 0;                                     /* int requests */
uint32 int_reqhi = 0;                                   /* highest int request */
uint32 api_lvl = 0;                                     /* api active */
uint32 api_lvlhi = 0;                                   /* highest api active */
t_bool chan_req;                                        /* chan request */
uint32 cpu_mode = NML_MODE;                             /* normal mode */
uint32 mon_usr_trap = 0;                                /* mon-user trap */
uint32 EM2 = 2, EM3 = 3;                                /* extension registers */
uint32 RL1, RL2, RL4;                                   /* relocation maps */
uint32 bpt;                                             /* breakpoint switches */
uint32 alert;                                           /* alert dispatch */
uint32 em2_dyn, em3_dyn;                                /* extensions, dynamic */
uint32 usr_map[8];                                      /* user map, dynamic */
uint32 mon_map[8];                                      /* mon map, dynamic */
int32 ind_lim = 32;                                     /* indirect limit */
int32 exu_lim = 32;                                     /* EXU limit */
int32 cpu_astop = 0;                                    /* address stop */
int32 stop_invins = 1;                                  /* stop inv inst */
int32 stop_invdev = 1;                                  /* stop inv dev */
int32 stop_inviop = 1;                                  /* stop inv io op */
uint16 pcq[PCQ_SIZE] = { 0 };                           /* PC queue */
int32 pcq_p = 0;                                        /* PC queue ptr */
REG *pcq_r = NULL;                                      /* PC queue reg ptr */
int32 hst_p = 0;                                        /* history pointer */
int32 hst_lnt = 0;                                      /* history length */
uint32 hst_exclude = BAD_MODE;                          /* cpu_mode excluded from history */
InstHistory *hst = NULL;                                /* instruction history */
int32 rtc_pie = 0;                                      /* rtc pulse ie */
int32 rtc_tps = 60;                                     /* rtc ticks/sec */

t_stat cpu_ex (t_value *vptr, t_addr addr, UNIT *uptr, int32 sw);
t_stat cpu_dep (t_value val, t_addr addr, UNIT *uptr, int32 sw);
t_stat cpu_reset (DEVICE *dptr);
t_bool cpu_is_pc_a_subroutine_call (t_addr **ret_addrs);
t_stat cpu_set_size (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat cpu_set_hist (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat cpu_show_hist (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat EaSh (uint32 wd, uint32 *va);
t_stat Read (uint32 va, uint32 *dat);
t_stat Write (uint32 va, uint32 dat);
uint32 api_findreq (void);
void api_dismiss (void);
t_stat one_inst (uint32 inst, uint32 pc, uint32 mode, uint16 *trappc);
void inst_hist (uint32 inst, uint32 pc, uint32 typ);
t_stat rtc_inst (uint32 inst);
t_stat rtc_svc (UNIT *uptr);
t_stat rtc_reset (DEVICE *dptr);
t_stat rtc_set_freq (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat rtc_show_freq (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
int get_highest_interrupt(void);

extern t_bool io_init (void);
extern t_stat op_wyim (uint32 inst, uint32 *dat);
extern t_stat op_miwy (uint32 inst, uint32 dat);
extern t_stat op_pin (uint32 *dat);
extern t_stat op_pot (uint32 dat);
extern t_stat op_eomd (uint32 inst);
extern t_stat op_sks (uint32 inst, uint32 *skp);

/* CPU data structures

   cpu_dev      CPU device descriptor
   cpu_unit     CPU unit descriptor
   cpu_reg      CPU register list
   cpu_mod      CPU modifiers list
*/

UNIT cpu_unit = {
    UDATA(NULL, UNIT_FIX + UNIT_BINK, MAX_MEM_WORDS)
};

/*
* The SIMH macros are used in dialog with user, they work as follows:
*    FLDATA(name, var, reset) - For 1-bit boolean flags (uses a single bit in the register)
*    ORDATA(name, var, width) - For numeric values that can be any width (1-32 bits)
*    DRDATA(name, var, width) - For display-only numeric values
*/
REG cpu_reg[] = {
    { ORDATA(P, cpu_state.reg_P, 16) }, // The order is important
    { ORDATA(L, cpu_state.reg_L, 16) },
    { ORDATA(G, cpu_state.reg_G, 16) },
    { ORDATA(A, cpu_state.reg_A, 16) },
    { ORDATA(E, cpu_state.reg_E, 16) },
    { ORDATA(X, cpu_state.reg_X, 16) },
    { ORDATA(V, cpu_state.reg_V, 16) },
    { ORDATA(W, cpu_state.reg_W, 16) },
    { ORDATA(reg_8, cpu_state.reg_8, 16) },
    { FLDATA(C, cpu_state.C, 0) },
    { FLDATA(OV, cpu_state.OV, 0) },
    { FLDATA(MS, cpu_state.MS, 0) },
    { FLDATA(MA, cpu_state.MA, 0) },
    { FLDATA(PR, cpu_state.PR, 0) },
    { ORDATA(MREG, cpu_state.MREG, 18) },
    { ORDATA(S, cpu_state.S, 16) }, // FIXME or 15?
    { ORDATA(U, cpu_state.U, 16) },
    /* Use separate variables for interrupt state instead of struct member */
    { ORDATA(INT_REQ, cpu_state.intrpt_mask, 32) },
    { ORDATA(INT_LVL, cpu_state.int_lvl, 5) },
    { ORDATA(SUSP_REQ, cpu_state.susp_req_bits, 32) },
    { ORDATA(SUSP_LVL, cpu_state.susp_active_level, 5) },
    { ORDATA(TRP_REQ, cpu_state.trp_req_bits, 16) },
    { ORDATA(CPU_MODE, cpu_state.cpu_mode, 2) },
    { DRDATA(INDLIM, ind_lim, 8), REG_NZ + PV_LEFT },
    { DRDATA(EXULIM, exu_lim, 8), REG_NZ + PV_LEFT },
    { ORDATA(WRU, sim_int_char, 8) },
    { ORDATA(PANEL_ADDR, cpu_state.panel_addr_lights, 16) },
    { ORDATA(PANEL_DATA, cpu_state.panel_data_lights, 16) },
    { FLDATA(CPU_RUNNING, cpu_state.cpu_running, 0) },
    { FLDATA(INT_ENABLED, cpu_state.interrupts_enabled, 0) },
//    { FLDATA(ROUTING_ENABLED, routing_enabled, 0) },
    /* Diagnostic trace logging toggles: e.g. "d cpu LOG_MEM 0" to silence memory trace */
    { FLDATA(LOG_ENABLE, mitra_log_enable, 0) },
    { FLDATA(LOG_INST, mitra_log_inst, 0) },
    { FLDATA(LOG_MEM, mitra_log_mem, 0) },
    { FLDATA(LOG_INT, mitra_log_int, 0) },
    { FLDATA(LOG_IO, mitra_log_io, 0) },
    { NULL }
};

/* 
 * Link these wrappers to your device's MTAB (modifier table) to integrate with the SIMH command parser.
 * Example MTAB entry:
 *   { MTAB_XTD | MTAB_VDV, 0, "ATTACH", "ATTACH", &io_cmd_attach, NULL, NULL, "Attach disk image" }
 */
MTAB cpu_mod[] = {
	{ UNIT_MSIZE, 4096, NULL,  "4K ",  &cpu_set_size }, // Set size of memory
	{ UNIT_MSIZE, 8192, NULL,  "8K ",  &cpu_set_size },
	{ UNIT_MSIZE, 16384, NULL,  "16K ",  &cpu_set_size },
	{ UNIT_MSIZE, 32768, NULL,  "32K ",  &cpu_set_size },
	{ MTAB_XTD|MTAB_VDV|MTAB_NMO|MTAB_SHP, 0,  "HISTORY ",  "HISTORY ", &cpu_set_hist,  &cpu_show_hist },
	{ 0 }
};

DEVICE cpu_dev = {
	"CPU", &cpu_unit, cpu_reg, cpu_mod,
	1, 8, 16, 1, 8, 16,
	&cpu_ex, &cpu_dep, &cpu_reset,
	NULL, NULL, NULL, NULL, 0
	};

/* Clock data structures

   rtc_dev      RTC device descriptor
   rtc_unit     RTC unit
   rtc_reg      RTC register list
*/

UNIT rtc_unit = { UDATA (&rtc_svc, 0, 0), 16000 };

REG rtc_reg[] = {
    { FLDATA (PIE, rtc_pie, 0) },
    { DRDATA (TIME, rtc_unit.wait, 24), REG_NZ + PV_LEFT },
    { DRDATA (TPS, rtc_tps, 8), PV_LEFT + REG_HRO },
    { NULL }
    };

MTAB rtc_mod[] = {
    { MTAB_XTD|MTAB_VDV, 50, NULL, "50HZ",
      &rtc_set_freq, NULL, NULL },
    { MTAB_XTD|MTAB_VDV, 60, NULL, "60HZ",
      &rtc_set_freq, NULL, NULL },
    { MTAB_XTD|MTAB_VDV, 0, "FREQUENCY", NULL,
      NULL, &rtc_show_freq, NULL },
    { 0 }
    };

DEVICE rtc_dev = {
    "RTC", &rtc_unit, rtc_reg, rtc_mod,
    1, 8, 8, 1, 8, 8,
    NULL, NULL, &rtc_reset,
    NULL, NULL, NULL
    };

/* ========== Suspension System (Section II-8.2) ========== */
// The suspension system is able to interrupt the current micro-program at the end of every micro-instruction, and to launch a special micro-program. 
// The suspension request is either issued by a peripheral or internal to the CPU.
// On occurence of a suspension, the CPU status, i.e. the contents of U, J, T registers and of B, Tz, To, Ao indicators are transferred in a 
// The suspension micro-program is then executed.
// At the end of the suspension program, the initial context is restored from the values previously saved in the stack.
uint16 susp_req;		// suspension requests
uint16 susp_lvl;		// suspension trap level
uint32 susp_reqhi = 0; 		// Highest suspension request

SuspContext susp_stack[SUSP_STACK_DEPTH];
int susp_stack_ptr = 0;

/* De-activation Word Table (DVT) - Section III-5 */
uint16 dvt_table[32];         /* 32 de-activation words */
t_addr cpt_base = 0;          /* Context Pointer Table base (M[10]) */

// Traps
// The origin of a trap is an abnormal condition detected at the end of a micro-instruction.
// The trap processing microcode:
// - protects bytes 4 to 9 of the memory which contain L- and P-register values and the indicators status of the context of the instruction which initiated the trap;
// - signals the cause of the trap by settinga bit in memory word 2;
// - performs a call to supervisor section 0.
uint16 trp_req;			// trap requests
uint16 trp_lvl;			// current trap level
uint32 trp_reqhi = 0; 		// Highest trap request

/* ========== SIMH Interface Functions ========== */
t_stat sim_instr(void) {
    uint16 inst, save_P, trap_P;
    t_stat reason = 0;
    cpu_state.intrpt_mask = cpu_state.intrpt_mask & ~1;
    while (reason == 0) {
        if (cpu_astop) {
            cpu_astop = 0;
            return SCPE_STOP;
        }
        if (sim_interval <= 0) {
            if ((reason = sim_process_event()))
                break;
        }
        sim_interval--;

        /* Check for traps.
         * Traps are not implemented in the current simulator FIXME
         */
         int trap = -1; // Dummy code to be fixed late
         if(trap >= 0) { // FIXME
		uint16 * trappc = 0;
		mitra_trap(trap, cpu_state.reg_P, trappc);
             }
        /* 
         * Check for suspensions
         * Suspensions are called interrupts in modern parlance as they are unconditionaly processed at the end of the current instruction.
         * In contrast Mitra's interrupts are managed by the code, not the hardware. 
         * The manual (section II-8.2) describes a hardware suspension system distinct from interrupts, operating at the micro-program level with a 4-deep stack.
         * Suspensions are used to couple peripherals requiring "urgent or frequent transfers" with 300 μs maximum response time. 
        */
        mitra_suspension_process();
        
            /* Normal instruction fetch */
            if (sim_brk_summ) {
                static uint32 bmask[] = {
                    SWMASK('E') | SWMASK('N'),
                    SWMASK('E') | SWMASK('M'),
                    SWMASK('E') | SWMASK('U')
                };
                uint32 btyp = sim_brk_test(cpu_state.reg_P, bmask[cpu_state.cpu_mode]);
                if (btyp) {
                    if (btyp & SWMASK('E'))
                        reason = STOP_IBKPT;
                    else if (btyp & BRK_TYP_DYN_STEPOVER)
                        reason = STOP_DBKPT;
                    else switch (btyp) {
                        case SWMASK('M'):
                            reason = STOP_MBKPT;
                            break;
                        case SWMASK('N'):
                            reason = STOP_NBKPT;
                            break;
                        case SWMASK('U'):
                            reason = STOP_UBKPT;
                            break;
                    }
                    sim_interval++;
                    break;
                }
            }
            cpu_state.trap_P = save_P = cpu_state.reg_P;
            inst = read_word(cpu_state.reg_P);
            cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            if (inst != 0) {
                MLOG_INST("--- sim_instr: fetched inst=%06o at P=%05o, calling one_inst() ---\n", inst, save_P);
                reason = one_inst(inst, save_P, cpu_state.cpu_mode, & cpu_state.trap_P);
                MLOG_INST("--- sim_instr: one_inst() returned reason=%d (cpu_state.trap_P=%05o) ---\n", reason, cpu_state.trap_P);
                if (reason > 0 && reason != STOP_HALT) {
                    MLOG_INST("    P restored to %05o (was advanced to %05o) due to reason=%d\n",
                              save_P, cpu_state.reg_P, reason);
                    cpu_state.reg_P = save_P;
                }
                if (reason == STOP_IONRDY)
                    reason = 0;
            }
    }
    if (pcq_r)
        pcq_r->qptr = pcq_p;
    return reason;
}

/* Helper to get highest pending interrupt */
int get_highest_interrupt(void) {
    int i;
    for (i = 31; i >= 0; i--) {
        if (cpu_state.intrpt_mask & (1u << i)) {
            MLOG_INT("[INT] priority scan: highest pending=%d (mask=%08X)\n", i, cpu_state.intrpt_mask);
            return i;
        }
    }
    return -1;
}

/* ========== Memory Access Functions ========== */
t_value read_word(t_addr va) {
    uint16 pa = VA_TO_PA(va);
    if (pa >= MAX_MEM_WORDS) {
        /* Trigger address invalid trap (TRAP_AI) */
        MLOG_MEM("    [MEM] read_word  va=%05o pa=%05o  ** OUT OF RANGE ** (MAX_MEM_WORDS=%05o) -> TRAP_AI queued\n",
                 va, pa, MAX_MEM_WORDS);
        cpu_state.trp_req_bits |= (1 << TRAP_AI);
        cpu_state.trap_pending = TRUE;
        return 0;
    }
    MLOG_MEM("    [MEM] read_word  va=%05o pa=%05o -> %06o\n", va, pa, M[pa]);
    return M[pa];
}
void write_word(t_addr va, t_value val) {
    t_addr pa = VA_TO_PA(va);
    if (pa >= MAX_MEM_WORDS) {
        MLOG_MEM("    [MEM] write_word va=%05o pa=%05o val=%06o ** OUT OF RANGE ** (MAX_MEM_WORDS=%05o) -> TRAP_AI queued\n",
                 va, pa, val, MAX_MEM_WORDS);
        cpu_state.trp_req_bits |= (1 << TRAP_AI);
        cpu_state.trap_pending = TRUE;
        return;
    }
    /* Check memory protection */
    if (!cpu_state.PR && (M[pa] & 0x0001)) {  /* Protection bit set and cpu_state.PR=0 */
        MLOG_MEM("    [MEM] write_word va=%05o pa=%05o val=%06o ** PROTECTED ** (cpu_state.PR=0, prot bit set) -> TRAP_PM queued\n",
                 va, pa, val);
        cpu_state.trp_req_bits |= (1 << TRAP_PM);
        cpu_state.trap_pending = TRUE;
        return;
    }
    MLOG_MEM("    [MEM] write_word va=%05o pa=%05o val=%06o (was %06o)\n", va, pa, val, M[pa]);
    M[pa] = val;
}
uint8 read_byte(t_addr va) {
    uint16 word_addr = va >> 1;
    uint16 word = read_word(word_addr);
    uint8 b = (va & 1) ? (word & 0xFF) : ((word >> 8) & 0xFF);
    MLOG_MEM("    [MEM] read_byte  va=%05o (word %05o, %s byte) -> %03o\n",
             va, word_addr, (va & 1) ? "low" : "high", b);
    return b;
}
void write_byte(t_addr va, uint8 val) {
    uint16 word_addr = va >> 1;
    uint16 word = read_word(word_addr);
    if (va & 1)
        word = (word & 0xFF00) | val;
    else
        word = (word & 0x00FF) | (val << 8);
    MLOG_MEM("    [MEM] write_byte va=%05o (word %05o, %s byte) val=%03o\n",
             va, word_addr, (va & 1) ? "low" : "high", val);
    write_word(word_addr, word);
}

/* ========== CPU Reset and Management ========== */
t_stat cpu_reset(DEVICE * dptr) {
    cpu_state.reg_A = cpu_state.reg_E = cpu_state.reg_X = cpu_state.reg_L = cpu_state.reg_G = cpu_state.reg_P = cpu_state.S = 0;
    cpu_state.curr_bloc = 0;
    cpu_state.MREG = cpu_state.reg_V = cpu_state.reg_W = cpu_state.U = 0;
    cpu_state.C = cpu_state.OV = cpu_state.MS = 0;
    cpu_state.MA = cpu_state.PR = 0;
    cpu_state.cpu_mode = 0;
    cpu_state.intrpt_mask = 0;
    cpu_state.int_lvl = 0;
    cpu_state.susp_req_bits = 0;
    susp_stack_ptr = 0;
    cpu_state.trp_req_bits = 0;
    cpu_state.trap_pending = FALSE;
    cpu_state.cpu_running = 0;
    cpu_state.interrupts_enabled = 0;
    cpu_state.panel_addr_lights = 0;
    cpu_state.panel_data_lights = 0;
    panel_reset();
    /* At cpu_reset(), the simulator does not need to initialize its own CPT in memory. 
    Every interrupt has an associated pointer indicating a memory area in which the context may be saved on occurence of an interrupt at this level. 
    The memory area for saving the context at a given level is actually reserved only if a program is connected to this level. 
    Generally, this area is located immediately after the program storage area.
    This area contains the register and indicator values at the last interrupt time (either if a higher level interrupt has been accepted, or if the level has been de-activated). 
    If the program is never interrupted, the saving area contains the initial program contents (at the time its execution is started).
    For example, it could allocate 32 context areas (7 words each) and fill the CPT at M[10] with their addresses. 
    This would allow the interrupt system to be tested independently of a full OS.
    */

    return SCPE_OK;
}

/* For Next command, determine if should use breakpoints
   to step over a subroutine branch or POP or SYSPOP.  Return
   TRUE if so with a list of addresses where dynamic (temporary)
   breakpoints should be set.
*/
typedef enum Next_Case {      /*    Next            Next Atomic         Next Forward */
    Next_BadOp  = 0,          /*    FALSE           FALSE               FALSE        */
    Next_Branch,              /*    FALSE           EA                  FALSE        */
    Next_BRM,                 /*    P+1,P+2,P+3     EA+1,P+1,P+2,P+3    P+1,P+2,P+3  */
    Next_BRX,                 /*    FALSE           EA,P+1              P+1          */
    Next_Simple,              /*    FALSE           P+1                 P+1          */
    Next_POP,                 /*    P+1,P+2         100+OP,P+1,P+2      P+1,P+2      */
    Next_Skip,                /*    P+1,P+2         P+1,P+2             P+1,P+2      */
    Next_EXU                  /*      ??              ??                  ??         */
} Next_Case;

Next_Case Op_Cases[64] = {
 Next_BadOp,    Next_Branch,    Next_Simple,    Next_BadOp,     /*  HLT BRU EOM ...  */
 Next_BadOp,    Next_BadOp,     Next_Simple,    Next_BadOp,     /*  ... ... EOD ...  */
 Next_Simple,   Next_Branch,    Next_Simple,    Next_Simple,    /*  MIY BRI MIW POT  */
 Next_Simple,   Next_BadOp,     Next_Simple,    Next_Simple,    /*  ETR ... MRG EOR  */
 Next_Simple,   Next_BadOp,     Next_Simple,    Next_EXU,       /*  NOP ... ROV EXU  */
 Next_BadOp,    Next_BadOp,     Next_BadOp,     Next_BadOp,     /*  ... ... ... ...  */
 Next_Simple,   Next_BadOp,     Next_Simple,    Next_Simple,    /*  YIM ... WIM PIN  */
 Next_BadOp,    Next_Simple,    Next_Simple,    Next_Simple,    /*  ... STA STB STX  */
 Next_Skip,     Next_BRX,       Next_BadOp,     Next_BRM,       /*  SKS BRX ... BRM  */
 Next_BadOp,    Next_BadOp,     Next_Simple,    Next_BadOp,     /*  ... ... RCH ...  */
 Next_Skip,     Next_Branch,    Next_Skip,      Next_Skip,      /*  SKE BRR SKB SKN  */
 Next_Simple,   Next_Simple,    Next_Simple,    Next_Simple,    /*  SUB ADD SUC ADC  */
 Next_Skip,     Next_Simple,    Next_Simple,    Next_Simple,    /*  SKR MIN XMA ADM  */
 Next_Simple,   Next_Simple,    Next_Simple,    Next_Simple,    /*  MUL DIV RSH LSH  */
 Next_Skip,     Next_Simple,    Next_Skip,      Next_Skip,      /*  SKM LDX SKA SKG  */
 Next_Skip,     Next_Simple,    Next_Simple,    Next_Simple };  /*  SKD LDB LDA EAX  */

/* Memory examine */

t_stat cpu_ex (t_value *vptr, t_addr addr, UNIT *uptr, int32 sw)
{
uint32 pa;

pa = addr;
if (pa > MAXMEMSIZE)
    return SCPE_REL;
if (pa >= MEMSIZE)
    return SCPE_NXM;
if (vptr != NULL)
    *vptr = M[pa] & DMASK;
return SCPE_OK;
}

/* Memory deposit */

t_stat cpu_dep (t_value val, t_addr addr, UNIT *uptr, int32 sw)
{
uint32 pa;

pa = addr;
if (pa > MAXMEMSIZE)
    return SCPE_REL;
if (pa >= MEMSIZE)
    return SCPE_NXM;
M[pa] = val & DMASK;
return SCPE_OK;
}

/* Set memory size */

t_stat cpu_set_size (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
int32 mc = 0;
uint32 i;

if ((val <= 0) || (val > MAXMEMSIZE) || ((val & 037777) != 0))
    return SCPE_ARG;
for (i = val; i < MEMSIZE; i++)
    mc = mc | M[i];
if ((mc != 0) && (!get_yn ("Really truncate memory [N]?", FALSE)))
    return SCPE_OK;
MEMSIZE = val;
for (i = MEMSIZE; i < MAXMEMSIZE; i++)
    M[i] = 0;
return SCPE_OK;
}

/* The real time clock runs continuously; therefore, it only has
   a unit service routine and a reset routine.  The service routine
   sets an interrupt that invokes the clock counter.  The clock counter
   is a "one instruction interrupt", and only MIN/SKR are valid.
*/

t_stat rtc_svc (UNIT *uptr)
{
if (rtc_pie)                                            /* set pulse intr */
    int_req = int_req | INT_RTCP;
rtc_unit.wait = sim_rtcn_calb (rtc_tps, TMR_RTC);       /* calibrate */
sim_activate (&rtc_unit, rtc_unit.wait);                /* reactivate */
return SCPE_OK;
}

/* Clock interrupt instruction */

t_stat rtc_inst (uint32 inst)
{
return SCPE_OK;
}

/* Clock reset */

t_stat rtc_reset (DEVICE *dptr)
{
rtc_pie = 0;                                            /* disable pulse */
rtc_unit.wait = sim_rtcn_init (rtc_unit.wait, TMR_RTC); /* initialize clock calibration */
sim_activate (&rtc_unit, rtc_unit.wait);                /* activate unit */
return SCPE_OK;
}

/* Set frequency */

t_stat rtc_set_freq (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
if (cptr)
    return SCPE_ARG;
if ((val != 50) && (val != 60))
    return SCPE_IERR;
rtc_tps = val;
return SCPE_OK;
}

/* Show frequency */

t_stat rtc_show_freq (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
fprintf (st, (rtc_tps == 50)? "50Hz": "60Hz");
return SCPE_OK;
}

/* ========== History Functions ========== */
void inst_hist(uint32 c, uint32 pc, uint32 tp) {
    if (cpu_state.cpu_mode == hst_exclude)
        return;
    hst_p = (hst_p + 1);
    if (hst_p >= hst_lnt)
        hst_p = 0;
    hst[hst_p].typ = tp | (cpu_state.OV << 4) | (cpu_state.cpu_mode << 5);
    hst[hst_p].P = pc;
    hst[hst_p].A = cpu_state.reg_A;
    hst[hst_p].E = cpu_state.reg_E;
    hst[hst_p].X = cpu_state.reg_X;
    hst[hst_p].L = cpu_state.reg_L;
    hst[hst_p].G = cpu_state.reg_G;
    hst[hst_p].S = cpu_state.S;
    hst[hst_p].U = cpu_state.U;
    hst[hst_p].V = cpu_state.reg_V;
    hst[hst_p].W = cpu_state.reg_W;
    hst[hst_p].MREG = cpu_state.MREG;
    hst[hst_p].ea = HIST_NOEA;
}

/* Set history */

t_stat cpu_set_hist (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
int32 i, lnt;
t_stat r;

if (cptr == NULL) {
    for (i = 0; i < hst_lnt; i++)
        hst[i].typ = 0;
    hst_p = 0;
    return SCPE_OK;
    }
lnt = (int32) get_uint (cptr, 10, HIST_MAX, &r);
if ((r != SCPE_OK) || (lnt && (lnt < HIST_MIN)))
    return SCPE_ARG;
hst_p = 0;
if (sim_switches & SWMASK('M'))
    hst_exclude = MON_MODE;
else if (sim_switches & SWMASK('N'))
    hst_exclude = NML_MODE;
else if (sim_switches & SWMASK('U'))
    hst_exclude = USR_MODE;
else
    hst_exclude = BAD_MODE;
if (hst_lnt) {
    free (hst);
    hst_lnt = 0;
    hst = NULL;
    }
if (lnt) {
    hst = (InstHistory *) calloc (lnt, sizeof (InstHistory));
    if (hst == NULL)
        return SCPE_MEM;
    hst_lnt = lnt;
    }
return SCPE_OK;
}

/* Show history */

t_stat cpu_show_hist(FILE * st, UNIT * uptr, int32 val, CONST void * desc) {
    int32 k, lnt;
    CONST char * cptr = (CONST char * ) desc;
    t_stat r;
    InstHistory * h;
    static
    const char * cyc[] = {
        "   ",
        "   ",
        "INT",
        "TRP"
    };
    static
    const char * modes = "NMU?";
    if (hst_lnt == 0)
        return SCPE_NOFNC;
    if (cptr) {
        lnt = (int32) get_uint(cptr, 10, hst_lnt, & r);
        if (r != SCPE_OK || lnt == 0)
            return SCPE_ARG;
    } else {
        lnt = hst_lnt;
    }
    fprintf(st, "CYC PC    MD OV A        E        X        EA\n\n");
    uint16 hst_p_loc = hst_p;
    for (k = 0; k < lnt; k++) {
        h = & hst[(++hst_p_loc) % hst_lnt];
        if (h->typ) {
            fprintf(st, "%s %05o %c  %o  %06o %06o %06o\n",
                cyc[h->typ & 3], h->P, modes[(h->typ >> 5) & 3],
                (h->typ >> 4) & 1, h->A, h->E, h->X);
        }
    }
    return SCPE_OK;
}

static void mitra_log_close(void) {
    if (mitra_log_fp != NULL) {
        fprintf(mitra_log_fp, "===== MITRA-15 trace log closed =====\n");
        fclose(mitra_log_fp);
        mitra_log_fp = NULL;
    }
}

static void mitra_log_init(void) {
    if (mitra_log_fp == NULL) {
        mitra_log_fp = fopen(MITRA_LOG_FILENAME, "a");
        if (mitra_log_fp != NULL) {
            time_t now = time(NULL);
            /* Line-buffered: each '\n' is flushed to disk immediately. */
            setvbuf(mitra_log_fp, NULL, _IOLBF, 0);
            fprintf(mitra_log_fp, "\n===== MITRA-15 trace log opened %s", ctime(&now));
            atexit(mitra_log_close);
        }
    }
}

/* Low-level formatted write; always safe to call.
 * Not static: shared with mitra_io.c (and other modules) via extern
 * declarations, so CPU and device-level traces interleave in one file. */
void mitra_log(const char *fmt, ...) {
    va_list ap;
    if (!mitra_log_enable)
        return;
    mitra_log_init();
    if (mitra_log_fp == NULL)
        return;
    va_start(ap, fmt);
    vfprintf(mitra_log_fp, fmt, ap);
    va_end(ap);
    fflush(mitra_log_fp);  /* belt-and-braces: guarantee it hits disk before a segfault can lose it */
}

/* Dumps the full visible CPU context (used by interrupt/trap/suspension logging).
 * Not static: also called from mitra_io.c. */
void mitra_log_regs(const char *label) {
    if (!(mitra_log_enable && mitra_log_int))
        return;
    mitra_log("    [%-9s] blk=%d P=%05o L=%05o G=%05o A=%06o E=%06o X=%06o C=%d O=%d cpu_state.MS=%d cpu_state.PR=%d cpu_state.MA=%d\n",
        label, cpu_state.curr_bloc,
        cpu_state.reg_P, cpu_state.reg_L, cpu_state.reg_G,
        cpu_state.reg_A, cpu_state.reg_E, cpu_state.reg_X,
        cpu_state.C, cpu_state.OV, cpu_state.MS, cpu_state.PR, cpu_state.MA);
}

