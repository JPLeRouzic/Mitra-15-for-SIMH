/* mitra_cpu.c: CII Mitra 15 CPU simulator for SIMH
 *
 * Based on MITRA 15 Reference Manual (CII, 1973) and documents in Patrick Chour's website
 * 
 * MITRA 15 is built around a core memory the capacity which can be extended by 4K 16-bit words blocks. 
 * This core memory has four access ports for connecting up to four processing units or direct memory access controllers.
 * Each processing unit is a 16-bit word-addressable computer with:
 * - 16-bit word size
 * - Byte-addressable (even addresses = word boundaries)
 * - Up to 32K words of memory, the bit 0 of program counter is always at 0.
 * - 86 instructions (Mitra-15/30)
 * - 32 interrupt levels
 * - Master/Slave modes with memory protection
 * - Interrupt, fast interrupt, suspension, and trap support
 *
 * Inspired the SDS 940 CPU and other SDS sigma simulators

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
uint32 MEMsize = MEM_32K;

/* CPU state - declared extern in mitra_cpu.h; this is the one real, shared instance. */
CPU_STATE cpu_state;

uint32 xfr_req = 0;                                     /* xfr req */
uint32 ion = 0;                                         /* int enable */
uint32 ion_defer = 0;                                   /* int defer */
uint32 int_req = 0;                                     /* int requests */
// int32 int_reqhi = 0;                                   /* highest int request, not uint32 */
uint32 cpu_mode = NML_MODE;                             /* normal mode */
uint32 mon_usr_trap = 0;                                /* mon-user trap */
uint32 bpt;                                             /* breakpoint switches */
uint32 alert;                                           /* alert dispatch */
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
t_bool high_speed = FALSE;  /* TRUE if high-speed interrupt */

static BRKTYPTAB cpu_breakpoints[] = {
    BRKTYPE('E', "Execute Instruction"),
    { 0 }
};

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
t_stat one_inst (uint16 inst, uint16 pc, uint32 mode, uint16 *trappc);
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
//    UDATA(NULL, UNIT_FIX + UNIT_BINK, MAX_MEM_WORDS)
    UDATA(NULL, UNIT_FIX + UNIT_BINK + UNIT_MULDIV, MAX_MEM_WORDS) // Has optional divide
};

/*
* The SIMH macros are used in dialog with user, they work as follows:
*    FLDATA(name, var, reset) - For 1-bit boolean flags (uses a single bit in the register)
*    ORDATA(name, var, width) - For numeric values that can be any width (1-32 bits)
*    DRDATA(name, var, width) - For display-only numeric values
*/
REG cpu_reg[] = {
    { HRDATA(P, cpu_state.reg_P, 16) }, // The order is important. HRDATA (not ORDATA): P must
                                         // be entered/displayed in hex to match cpu_dev's aradix=16.
                                         // ORDATA defaults to radix 8, so e.g. "RUN 100" was being
                                         // parsed as octal 100 = 0x40 instead of hex 0x100.
    { HRDATA(L, cpu_state.reg_L, 16) },
    { HRDATA(G, cpu_state.reg_G, 16) },
    { HRDATA(A, cpu_state.reg_A, 16) },
    { HRDATA(E, cpu_state.reg_E, 16) },
    { HRDATA(X, cpu_state.reg_X, 16) },
    { HRDATA(V, cpu_state.reg_V, 16) },
    { HRDATA(W, cpu_state.reg_W, 16) },
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
    { ORDATA(INT_LVL, cpu_state.curr_int_lvl, 5) },
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
	"CPU", 			// device name
	&cpu_unit, 		// pointer to array of UNIT structures
	cpu_reg, 		// pointer to array of REG structures
	cpu_mod,		// pointer to array of MTAB structures
	1, 		// number of units in this device
	16, 		// radix for input and display of device addresses (hexadecimal)
	16, 		// # of bits of address field
	2, 		// increment between device addresses, Mitra-15 is byte-addressable with even addresses = word boundaries
	16, 		// radix for input and display of device data (hexadecimal)
	16, 		// width in bits of device data
	&cpu_ex, 		// address to examine
	&cpu_dep, 		// address to deposit
	&cpu_reset,		// address of reset routine
	NULL, 			// address of bootstrap routine
	NULL, 			// address of attach routine
	NULL, 			// address of detach routine
	NULL, 			// address of VM-specific device context table
	0,			// device flags
	0,			// debug control flags
//	NULL,			// pointer to array of DEBTAB structures
	NULL,			// address of memory size change routine
	NULL,			// pointer to logical name string
	NULL,			// address of help routine
	NULL,			// address of displaying help
	NULL,			// address of output of the "SHOW FEATURES"
	cpu_breakpoints,	// Breakpoint types supported by the device
	NULL,			// Device type encoded in the flags field
	0			// Device unit test routine
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
    uint16 inst, save_P;
    t_stat reason = 0;
    cpu_state.intrpt_mask = cpu_state.intrpt_mask & ~1;
    while (reason == 0) {
    sim_printf("\ncpu_state.reg_P: %#010x\n", cpu_state.reg_P);
        if (cpu_astop) {
            cpu_astop = 0;
            return SCPE_STOP;
        }
        if (sim_interval <= 0) {
            if ((reason = sim_process_event()))
                break;
        }
        
        /* ----- BREAKPOINT TEST ----- */
        if (sim_brk_summ &&
            sim_brk_test(cpu_state.reg_P, SWMASK('E'))) {
            reason = STOP_IBKPT;          /* or SCPE_STOP */
            break;
        }
        
        sim_interval--;

        /* Check for traps.
         * Traps that became pending asynchronously (e.g. a future watchdog)
	 * between the previous instruction's completion and this fetch.
	 * Traps raised synchronously by instruction execution itself are
	 * already dispatched from inside one_inst(); this exists only for
	 * sources not tied to a specific instruction. */
	if (cpu_state.trap_pending) {
	    int cause = mitra_resolve_trap_cause(cpu_state.trp_req_bits);
	    if (cause >= 0) {
		cpu_state.trap_cause = cause;
		reason = mitra_trap(cause, cpu_state.reg_P);
		if (reason != SCPE_OK)
		    break;
		continue;   /* re-check breakpoints/suspensions/interrupts before fetching */
	    }
	    cpu_state.trap_pending = FALSE;  /* defensive: clear spurious flag */
	}
        /* 
         * Check for suspensions
         * Suspensions are called interrupts in modern parlance as they are unconditionaly processed at the end of the current instruction.
         * In contrast Mitra's interrupts are managed by the code, not the hardware. 
         * The manual (section II-8.2) describes a hardware suspension system distinct from interrupts, operating at the micro-program level with a 4-deep stack.
         * Suspensions are used to couple peripherals requiring "urgent or frequent transfers" with 300 μs maximum response time. 
        */
        mitra_suspension_process();
        
        /* 
        * Check for interrupts FIXME
        *
        * This is incorrect, not all instructions are interruptable
        */
        cpu_state.int_reqhi = get_highest_interrupt();
        
        if ((cpu_state.MA == 0) && (cpu_state.int_reqhi >= 0) && (cpu_state.int_reqhi > cpu_state.curr_int_lvl)) {
// sim_printf("sim_instr(void) 17\n");
            uint16 pa;
	    reason = cpt_lookup((uint16)cpu_state.int_reqhi, &pa);
	    if (reason != SCPE_OK)
	        break;
            /* Accept interrupt
             * Save context of currently-running level (per DIT manual section)
             * CPT is a 32-word table at absolute address M[10].
             * CPT[i] = word address of the context save area for level i.
             * Save area layout: word 0=Indicators, 1=X, 2=E, 3=A, 4=G, 5=L, 6=P 
             */
// sim_printf("sim_instr(void) 20\n");
            reason = mitra_interrupt_accept(cpu_state.int_reqhi, high_speed);
// sim_printf("sim_instr(void) 11\n");
            if (reason != SCPE_OK) break;
            
/*            if (pa != VEC_RTCP && rtc_pie) {
                cpu_state.intrpt_mask |= INT_RTCP;
            } FIXME probably a remnant of SDS940*/
        } else {
            /* Normal instruction fetch */
            if (sim_brk_summ) {
            	// We encountered a breakpoint
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
            // Normal case, instruction execution 
            cpu_state.trap_P = save_P = cpu_state.reg_P;
            inst = read_word(cpu_state.reg_P);
            cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            if (inst != 0) {
                sim_printf("\n--- sim_instr: fetched inst=%#010x at P=%#010x ---\n", inst, save_P);
                
                // Print the instruction  mnemonic
                fprint_sym(stdout, save_P, (unsigned int *) &inst, NULL, 0);
                sim_printf("\n");
                
                reason = one_inst(inst, save_P, cpu_state.cpu_mode, & cpu_state.trap_P);
                sim_printf("\n--- sim_instr: one_inst() returned reason=%#04x ---", reason);
                if (reason > 0 && reason != STOP_HALT) {
                    sim_printf("\n    P restored to %#010x (was advanced to %#010x) due to reason=%#010x",
                              save_P, cpu_state.reg_P, reason);
                    cpu_state.reg_P = save_P;
                }
                if (reason == STOP_IONRDY)
                    reason = 0;
            }
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
            sim_printf("[INT] priority scan: highest pending=%d (mask=%08X)\n", i, cpu_state.intrpt_mask);
            return i;
        }
    }
    return -1;   /* nothing pending */
}

/* ========== Memory Access Functions ========== */
t_value read_word(t_addr va) {
    uint16 pa = VA_TO_PA(va);
    if (pa >= MAX_MEM_WORDS) {
        /* Trigger address invalid trap (TRAP_AI) */
        sim_printf("\n    [MEM] trap in read_word  va=%#010x pa=%#010x  ** OUT OF RANGE ** (MAX_MEM_WORDS=%d) -> TRAP_AI queued",
                 va, pa, MAX_MEM_WORDS);
        cpu_state.trp_req_bits |= (1 << TRAP_AI);
        cpu_state.trap_pending = TRUE;
        return 0;
    }
    sim_printf("\n[MEM] read_word  pa=%#010x, value: %#010x\n", pa, M[pa]);
    return M[pa];
}
void write_word(t_addr va, t_value val) {
    t_addr pa = VA_TO_PA(va);
sim_printf("\n    Entering write_word()  va=%#010x pa=%d val=%#010x", va, pa, val);
    if (pa >= MAX_MEM_WORDS) {
        sim_printf("\n    [MEM] write_word va=%#010x pa=%d val=%#010x ** OUT OF RANGE ** (MAX_MEM_WORDS=%d) -> TRAP_AI queued",
                 va, pa, val, MAX_MEM_WORDS);
        cpu_state.trp_req_bits |= (1 << TRAP_AI);
        cpu_state.trap_pending = TRUE;
        return;
    }
    /* Check memory protection */
    if (!cpu_state.PR && (M[pa] & 0x0001)) {  /* Protection bit set and cpu_state.PR=0 */
        sim_printf("\n    [MEM] write_word va=%#010x pa=%#010x val=%#010x ** PROTECTED ** (cpu_state.PR=0, prot bit set) -> TRAP_PM queued",
                 va, pa, val);
        cpu_state.trp_req_bits |= (1 << TRAP_PM);
        cpu_state.trap_pending = TRUE;
        return;
    }
    sim_printf("\n[MEM] write_word pa=%#010x val=%#010x (was %#010x)\n", pa, val, M[pa]);
    M[pa] = val;
}
uint8 read_byte(t_addr va) {
    uint16 word_addr = va >> 1;
    uint16 word = read_word(word_addr);
    uint8 b = (va & 1) ? (word & 0xFF) : ((word >> 8) & 0xFF);
    sim_printf("\n    [MEM] read_byte  va=%#010x (word %#010x, %#010x byte) -> %#010x",
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
    sim_printf("\n    [MEM] write_byte va=%#010x (word %#010x, %#010x byte) val=%#010x",
             va, word_addr, (va & 1) ? "low" : "high", val);
    write_word(word_addr, word);
}

/* ========== CPU Reset and Management ========== */
t_stat cpu_reset(DEVICE * dptr) {
    cpu_state.reg_A = cpu_state.reg_E = cpu_state.reg_X = cpu_state.reg_L = cpu_state.reg_G = cpu_state.reg_P = cpu_state.S = 0;
    cpu_state.SuspensionStack[susp_stack_ptr].J_reg = 0;
    cpu_state.MREG = cpu_state.reg_V = cpu_state.reg_W = cpu_state.U = 0;
    cpu_state.C = cpu_state.OV = cpu_state.MS = 0;
    cpu_state.MS = 1; // We boot in master mode
    cpu_state.MA = cpu_state.PR = 0;
    cpu_state.cpu_mode = 0;
    cpu_state.intrpt_mask = 0;
    cpu_state.curr_int_lvl = 0;
    cpu_state.susp_req_bits = 0;
    susp_stack_ptr = 0;
    cpu_state.trp_req_bits = 0;
    cpu_state.trap_pending = FALSE;
    cpu_state.cpu_running = 0;
    cpu_state.interrupts_enabled = 0;
    cpu_state.panel_addr_lights = 0;
    cpu_state.panel_data_lights = 0;
//    uint16 ctx_table = M[10]; // FIXME how to assign an address in Mitra's simulated space to ctx_table?
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
    
    sim_brk_types = SWMASK('E') | SWMASK('R') | SWMASK('W');;   /* we support execution breakpoints */
    sim_brk_dflt  = SWMASK('E');   /* default type for BREAK */

    return SCPE_OK;
}

/* Memory examine */
t_stat cpu_ex (t_value *vptr, t_addr addr, UNIT *uptr, int32 sw)
{
uint32 pa;

pa = addr;
if (pa > MAX_MEM_WORDS)
    return SCPE_REL;
if (pa >= MAX_MEM_WORDS)
    return SCPE_NXM;
if (vptr != NULL) {
    *vptr = M[pa] & DMASK;
    }
// sim_printf("\nAddress at: %#010x contains: %#010x\n", pa, M[pa] & DMASK);
return SCPE_OK;
}

/* Memory deposit */
t_stat cpu_dep(t_value val, t_addr addr, UNIT * uptr, int32 sw) {
    uint32 pa = addr & 0x7FFF;
    sim_printf("\nDeposit: %#010x to: %#010x", val & DMASK, pa);
    if (pa >= MAX_MEM_WORDS)
        return SCPE_NXM;
    M[pa] = val & DMASK;
//    sim_printf("\nMemory now contains: %#010x\n", M[pa]);
    return SCPE_OK;
}

/* Set memory size */

t_stat cpu_set_size(UNIT * uptr, int32 val, CONST char * cptr, void * desc) {
    int32 mc = 0;
    uint32 i;
    if (val <= 0 || val > MAX_MEM_WORDS || (val & 037777) != 0)
        return SCPE_ARG;
    for (i = val; i < MEMsize; i++)
        mc = mc | M[i];
    if (mc != 0 && !get_yn("Really truncate memory [N]?", FALSE))
        return SCPE_OK;
    MEMsize = val;
    for (i = MEMsize; i < MAX_MEM_WORDS; i++)
        M[i] = 0;
    return SCPE_OK;
}

/* The real time clock runs continuously; therefore, it only has
   a unit service routine and a reset routine.  The service routine
   sets an interrupt that invokes the clock counter.  The clock counter
   is a "one instruction interrupt", and only MIN/SKR are valid.


t_stat rtc_svc (UNIT *uptr)
{
if (rtc_pie)                                            /* set pulse intr *
    int_req = int_req | INT_RTCP;
rtc_unit.wait = sim_rtcn_calb (rtc_tps, TMR_RTC);       /* calibrate *
sim_activate (&rtc_unit, rtc_unit.wait);                /* reactivate *
return SCPE_OK;
} */

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
            fprintf(st, "%s %d %c  %o  %d %d %d\n",
                cyc[h->typ & 3], h->P, modes[(h->typ >> 5) & 3],
                (h->typ >> 4) & 1, h->A, h->E, h->X);
        }
    }
    return SCPE_OK;
}


