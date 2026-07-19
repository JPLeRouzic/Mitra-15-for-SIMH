/* mitra_cpu.c: CII Mitra 15 CPU simulator for SIMH
 *
 * Based on MITRA 15 Reference Manual (CII, 1973)
 * 
 * The Mitra-15 is a 16-bit word-addressable computer with:
 * - 16-bit word size
 * - Byte-addressable (even addresses = word boundaries)
 * - Up to 32K words of memory, the bit 0 of program counter is always at 0.
 * - 86 instructions (Mitra-15/30)
 * - 32 interrupt levels
 * - Master/Slave modes with memory protection
 * - Interrupt, fast interrupt, suspension, and trap support

 */

#include <stdbool.h>
#include <math.h>
#include "mitra_defs.h"
#include "mitra_io.h"

/* ========== Constants and Definitions ========== */
#define PCQ_SIZE        64
#define PCQ_MASK        (PCQ_SIZE - 1)
#define PCQ_ENTRY       pcq[pcq_p = (pcq_p - 1) & PCQ_MASK] = pc

#define HIST_XCT        1
#define HIST_INT        2
#define HIST_TRP        3
#define HIST_MIN        64
#define HIST_MAX        65536
#define HIST_NOEA       0x40000000

/* Unit flags */
#define UNIT_MSIZE      (1 << 0)
#define UNIT_EXTINS     (1 << 1)
#define UNIT_FP         (1 << 2)
#define UNIT_MULDIV     (1 << 3)
#define UNIT_HSINT      (1 << 4)

/* Memory sizes (in words) */
#define MEM_4K          4096
#define MEM_8K          8192
#define MEM_16K         16384
#define MEM_32K         32768
#define MAX_MEM_WORDS   32768
#define MAX_MEM_BYTES   (MAX_MEM_WORDS * 2)

/* Condition code bits (per manual section II-6) */
#define CC_CARRY        0x0001    /* C = 1: zero result or equality */
#define CC_OVERFLOW     0x0002    /* O = 1: negative result or A < operand */
#define CC_ZERO         0x0004    /* Internal: result zero */
#define CC_NEG          0x0008    /* Internal: result negative */

/* Addressing modes - Class 0 (manual page 5-1, 5-2) */
#define AM_DL           0   /* Direct Local: Y = (L) + D */
#define AM_IL           1   /* Indirect Local: Y = G' + ((L) + D) */
#define AM_ILX          2   /* Indirect Local Indexed: Y = G' + ((L) + D) + (X) */
#define AM_DG           4   /* Direct General: Y = (G) + D */
#define AM_IGX          5   /* Indirect General Indexed: Y = G' + ((G) + D) + (X) */
#define AM_P            6   /* Parameter (immediate): operand = D */
#define AM_PX           7   /* Parameter Indexed (class 1 only) */

/* Addressing modes - Class 2 (manual page 5-3) */
#define AM_RP           6   /* Relative Plus: Y = (P) + 2D */
#define AM_RM           7   /* Relative Minus: Y = (P) - 2D */

/* Instruction format decoding */
#define I_MODE_MASK     0xE000
#define I_MODE_SHIFT    13
#define I_OPCODE_MASK   0x1F00
#define I_OPCODE_SHIFT  8
#define I_GROUP_SHIFT   9
#define I_DISP_MASK     0x00FF

/* Fixed GPRIME macro to correctly reference the G register in the current block */
#define GPRIME ((MS) ? reg_block[curr_bloc].G : 0)

#define VA_TO_PA(va) ((va) & 0x7FFF)

/* ========== Trap and Suspension Constants ========== */
/* Trap causes (Section II-8.3) */
#define TRAP_VM         0   /* Mode violation */
#define TRAP_PM         1   /* Memory protection violation */
#define TRAP_AI         2   /* Non-existing address */
#define TRAP_PA         3   /* Parity error */
#define TRAP_II         4   /* Invalid instruction */
#define TRAP_ES         5   /* I/O error */
#define TRAP_WD         6   /* Watchdog timer */

/* Suspension levels (Section II-8.2) */
#define SUSP_STACK_DEPTH    4
#define SUSP_INTERNAL_TRAP  0
#define SUSP_INTERNAL_IT    1
#define SUSP_INTERNAL_PANEL 2
#define SUSP_INTERNAL_PWR   3

/* Interrupt vectors */
static const uint32 int_vec[32] = {
    0, 0, 0, 0,
    VEC_FORK, VEC_DRM, VEC_MUXCF, VEC_MUXCO,
    VEC_MUXT, VEC_MUXR, VEC_HEOR, VEC_HZWC,
    VEC_GEOR, VEC_GZWC, VEC_FEOR, VEC_FZWC,
    VEC_EEOR, VEC_EZWC, VEC_DEOR, VEC_DZWC,
    VEC_CEOR, VEC_CZWC, VEC_WEOR, VEC_YEOR,
    VEC_WZWC, VEC_RTCP, VEC_RTCS, VEC_IPAR,
    VEC_CPAR, VEC_PWRF, VEC_PWRO, 0
};

/* ========== Type Definitions ========== */

/* 
 * CPU registers 
 * Each register has a unique address form 0 to 63 (or 127)
 * A high-speed interrupt causes an automatic switching of the register block. 
 * In the new block, the registers have then the same assignment as in block 0, but for other programs
 */
#define REG_BLOCS 8
uint8 curr_bloc = 0;
struct {
    uint16 A;
    uint16 E;
    uint16 X;
    uint8 C, OV;
    uint16 P;
    uint16 L;
    uint16 G;
    uint16 V;
    uint16 W;
}
reg_block[REG_BLOCS];

uint32 MREG;
uint16 S;
uint16 U;

/* Normal or "slave" mode */
// In normal mode, priviledged instructions cannot be executed and any attempt to execute such an instruction causes: A "mode violation" trap. 
// MS indicator is reset (MS = 0).

/* Priviledged or "master" mode */
// In master mode all instructions, whether priviledged or not, are executable. 
// MS indicator is set (MS = 1).
// The various OSes are examples of programs which must be executed in master mode. (See CSV and RSV instructions).
// It should be noted that addressing modes are different in master and slave modes (see Chapter V "Addressing modes") to provide absolute addressing capability in master mode.
uint16 MS;
uint16 PR; // Access to protected areos
// Interrupt mask
uint16 MA;

typedef struct {
    uint32 typ;
    uint16 P;
    uint16 A;
    uint16 E;
    uint16 X;
    uint16 L;
    uint16 G;
    uint16 S;
    uint8 C;
    uint8 OV;
    uint8 MS;
    uint16 MA;
    uint16 PR;
    uint16 MREG;
    uint16 U, V, W;
    uint32 ea;
} InstHistory;

/* ========== Global State ========== */
uint16 cpu_mode; /* 0=Normal/Slave, 1=Master */
uint16 RL1, RL2, RL4;

/* ========== Suspension System (Section II-8.2) ========== */
// The suspension system is able to interrupt the current micro-program at the end of every micro-instruction, and to launch a special micro-program. 
// The suspension request is either issued by a peripheral or internal to the CPU.
// On occurence of a suspension, the CPU status, i.e. the contents of U, J, T registers and of B, Tz, To, Ao indicators are transferred in a 
// The suspension micro-program is then executed.
// At the end of the suspension program, the initial context is restored from the values previously saved in the stack.
uint16 susp_req;		// suspension requests
uint16 susp_lvl;		// suspension trap level
uint32 susp_reqhi = 0; 		// Highest suspension request

/* Suspension stack - 4 levels deep */
typedef struct {
    uint16 U_reg;     /* Universal register */
    uint16 J_reg;     /* J register (block selector) */
    uint16 T_reg;     /* T register (micro-PC) */
    uint8  B_ind;     /* B indicator */
    uint8  Tz_ind;    /* Tz indicator */
    uint8  To_ind;    /* To indicator */
    uint8  Ao_ind;    /* Ao indicator */
    uint16 saved_bloc; /* Saved register block */
} SuspContext;

SuspContext susp_stack[SUSP_STACK_DEPTH];
int susp_stack_ptr = 0;

/* Suspension request bits (32 levels, 8 per stack level) */
uint32 susp_req_bits = 0;
uint16 susp_active_level = 0;
t_bool susp_pending = FALSE;

// Interrupts save context in an area pointed by an index to the CPT table.
// the CPT itself is pointed to by the contents of absolute address 10: M[10]
// There is a "high-speed" interrupt mechanism that uses register block switching instead of memory-based context saves.

/* Global interrupt state */
uint32 intrp_level = 0;  /* 32-bit bitmask of pending interrupts */
t_bool high_speed = FALSE;  /* TRUE if high-speed interrupt */

uint16 int_lvl = 0;           /* Current interrupt level */
uint32 int_reqhi = 0;         /* Highest pending interrupt level */

/* De-activation Word Table (DVT) - Section III-5 */
uint16 dvt_table[32];         /* 32 de-activation words */
uint16 cpt_base = 0;          /* Context Pointer Table base (M[10]) */

/* ========== Trap System (Section II-8.3) ========== */
uint16 trp_req_bits = 0;      /* Trap request bits */
t_bool trap_pending = FALSE;
uint16 trap_cause = 0;

// Traps
// The origin of a trap is an abnormal condition detected at the end of a micro-instruction.
// The trap processing microcode:
// - protects bytes 4 to 9 of the memory which contain L- and P-register values and the indicators status of the context of the instruction which initiated the trap;
// - signals the cause of the trap by settinga bit in memory word 2;
// - performs a call to supervisor section 0.
uint16 trp_req;			// trap requests
uint16 trp_lvl;			// current trap level
uint32 trp_reqhi = 0; 		// Highest trap request

t_bool dma_req;

/* Debug and history */
uint32 bpt;
uint32 alert;
uint32 em2_dyn, em3_dyn;
uint32 usr_map[8];
uint32 mon_map[8];
int32 ind_lim = 32;
int32 exu_lim = 32;
uint32 mon_usr_trap = 0;
uint32 EM2 = 2, EM3 = 3;
int32 cpu_genie = 0;
int32 cpu_astop = 0;
int32 stop_invins = 1;
int32 stop_invdev = 1;
int32 stop_inviop = 1;
uint16 pcq[PCQ_SIZE];
int32 pcq_p = 0;
REG *pcq_r = NULL;
int32 hst_p = 0;
int32 hst_lnt = 0;
uint32 hst_exclude = BAD_MODE;
InstHistory *hst = NULL;
int32 rtc_pie = 0;
int32 rtc_tps = 60;

/* Memory - word addressable */
uint16 M[MAX_MEM_WORDS];
uint32 MEMsize = MEM_32K;

uint16 MM_INVINS;

/* Front panel / CPU control state */
int cpu_running = 0; /* 1 = running, 0 = stopped */
int interrupts_enabled = 0;
int routing_enabled = 0;

/* Panel lights - exported so debugger can see them */
extern uint16 panel_addr_lights;
extern uint16 panel_data_lights;

extern void io_suspension_dispatch(uint16 susp_level);

/* ========== Function Prototypes ========== */
uint16 read_word(uint16 va);
void write_word(uint16 va, uint16 val);
uint8 read_byte(uint16 va);
void write_byte(uint16 va, uint8 val);
static uint16 add16(uint16 a, uint16 b, uint16 * carry, uint16 * overflow);
static uint16 sub16(uint16 a, uint16 b, uint16 * carry, uint16 * overflow);
static void set_condition_codes_load(uint16 result);
static void set_condition_codes_compare(uint16 a, uint16 b, uint16 result);
static void set_condition_codes_arithmetic(uint16 result, uint16 carry, uint16 overflow);
static void mul32(uint16 a, uint16 b, uint16 * high, uint16 * low);
static int div32(uint16 high, uint16 low, uint16 divisor, uint16 * quot, uint16 * rem);
static void double_to_mitra(double v, uint16 * A, uint16 * E);
static double mitra_to_double(uint16 A, uint16 E);

/* Enhanced trap and suspension functions */
t_stat mitra_trap(int trap, uint16 pc, uint16 *trappc);
t_stat mitra_suspension_request(uint16 susp_level);
t_stat mitra_suspension_process(void);
t_stat mitra_interrupt_accept(uint16 int_level, t_bool high_speed);
t_stat mitra_interrupt_return(t_bool high_speed);
void set_dyn_map(void);

void set_dyn_map(void);
t_stat set_cc(void);
t_stat cpu_ex(t_value * vptr, t_addr addr, UNIT * uptr, int32 sw);
t_stat cpu_dep(t_value val, t_addr addr, UNIT * uptr, int32 sw);
t_stat cpu_reset(DEVICE * dptr);
t_bool cpu_is_pc_a_subroutine_call(t_addr ** ret_addrs);
t_stat cpu_set_size(UNIT * uptr, int32 val, CONST char * cptr, void * desc);
t_stat cpu_set_type(UNIT * uptr, int32 val, CONST char * cptr, void * desc);
void inst_hist(uint32 inst, uint32 pc, uint32 typ);
t_stat rtc_inst(uint32 inst);
t_stat rtc_svc(UNIT * uptr);
t_stat rtc_reset(DEVICE * dptr);
t_stat rtc_set_freq(UNIT * uptr, int32 val, CONST char * cptr, void * desc);
t_stat rtc_show_freq(FILE * st, UNIT * uptr, int32 val, CONST void * desc);
uint32 RelocC(int32 va, int32 sw);
static int get_highest_interrupt(void);
t_stat cpu_set_hist(UNIT * uptr, int32 val, CONST char * cptr, void * desc);
t_stat cpu_show_hist(FILE * st, UNIT * uptr, int32 val, CONST void * desc);
void panel_reset(void);

uint16 group_1(uint16 target_address, uint16 inst);
uint16 group_2(uint16 inst, uint16 address, uint32 mode);

uint16 group_1_DL(uint16 inst);
uint16 group_2_DL(uint16 inst, uint32 mode);
uint16 group_1_P(uint16 inst);
uint16 group_3_DL(uint16 inst, uint32 mode);
uint16 group_1_DG(uint16 inst);
uint16 group_2_DG(uint16 inst, uint32 mode);
uint16 group_1_IL(uint16 inst);
uint16 group_2_IL(uint16 inst, uint32 mode);
uint16 group_1_IGX(uint16 inst);
uint16 group_2_IGX(uint16 inst, uint32 mode);
uint16 group_1_ILX(uint16 inst);
uint16 group_2_ILX(uint16 inst, uint32 mode);
uint16 group_4_RP(uint16 inst);
uint16 group_4_RM(uint16 inst);
uint16 group_5_IL(uint16 inst);
uint16 group_5_IG(uint16 inst);
uint16 group_3_PX(uint16 inst, uint32 mode);
uint16 group_3_P(uint16 inst, uint32 mode);

/* ========== SIMH Device Tables ========== */
UNIT cpu_unit = {
    UDATA(NULL, UNIT_FIX + UNIT_BINK, MAX_MEM_WORDS)
};

/*
The SIMH macros work as follows:
*    FLDATA(name, var, reset) - For 1-bit boolean flags (uses a single bit in the register)
*    ORDATA(name, var, width) - For numeric values that can be any width (1-32 bits)
*    DRDATA(name, var, width) - For display-only numeric values
*/
REG cpu_reg[] = {
    { ORDATA(reg_block, reg_block, 16) },
    { FLDATA(MS, MS, 0) },
    { FLDATA(MA, MA, 0) },
    { FLDATA(PR, PR, 0) },
    { ORDATA(S, S, 15) },
    { ORDATA(MREG, MREG, 18) },
    { ORDATA(U, U, 16) },
    /* Use separate variables for interrupt state instead of struct member */
    { ORDATA(INT_REQ, intrp_level, 32) },
    { ORDATA(INT_LVL, int_lvl, 5) },
    { ORDATA(SUSP_REQ, susp_req_bits, 32) },
    { ORDATA(SUSP_LVL, susp_active_level, 5) },
    { ORDATA(TRP_REQ, trp_req_bits, 16) },
    { ORDATA(RL1, RL1, 16) },
    { ORDATA(RL2, RL2, 16) },
    { ORDATA(RL4, RL4, 16) },
    { ORDATA(CPU_MODE, cpu_mode, 2) },
    { DRDATA(INDLIM, ind_lim, 8), REG_NZ + PV_LEFT },
    { DRDATA(EXULIM, exu_lim, 8), REG_NZ + PV_LEFT },
    { ORDATA(WRU, sim_int_char, 8) },
    { ORDATA(PANEL_ADDR, panel_addr_lights, 16) },
    { ORDATA(PANEL_DATA, panel_data_lights, 16) },
    { FLDATA(CPU_RUNNING, cpu_running, 0) },
    { FLDATA(INT_ENABLED, interrupts_enabled, 0) },
    { FLDATA(ROUTING_ENABLED, routing_enabled, 0) },
    { NULL }
};

/* 
 * To integrate with the SIMH command parser, link these wrappers to your 
 * device's MTAB (modifier table) or register them as custom CLI commands.
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

UNIT rtc_unit = {
    UDATA(&rtc_svc, 0, 0),
    16000
};
REG rtc_reg[] = {
{ FLDATA(PIE, rtc_pie, 0) },
{ DRDATA(TIME, rtc_unit.wait, 24), REG_NZ + PV_LEFT },
{ DRDATA(TPS, rtc_tps, 8), PV_LEFT + REG_HRO },
{ NULL }
};
MTAB rtc_mod[] = {
{ MTAB_XTD|MTAB_VDV, 50, NULL, "50HZ", &rtc_set_freq, NULL, NULL },
{ MTAB_XTD|MTAB_VDV, 60, NULL, "60HZ", &rtc_set_freq, NULL, NULL },
{ MTAB_XTD|MTAB_VDV, 0, "FREQUENCY", NULL, NULL, &rtc_show_freq, NULL },
{ 0 }
};
DEVICE rtc_dev = {
"RTC", &rtc_unit, rtc_reg, rtc_mod,
1, 8, 8, 1, 8, 8,
NULL, NULL, &rtc_reset, NULL, NULL, NULL
};

/* ========== Memory Access Functions ========== */
uint16 read_word(uint16 va) {
    uint16 pa = VA_TO_PA(va);
    if (pa >= MEMsize) {
        /* Trigger address invalid trap (TRAP_AI) */
        trp_req_bits |= (1 << TRAP_AI);
        trap_pending = TRUE;
        return 0;
    }
    return M[pa];
}
void write_word(uint16 va, uint16 val) {
    uint16 pa = VA_TO_PA(va);
    if (pa >= MEMsize) {
        trp_req_bits |= (1 << TRAP_AI);
        trap_pending = TRUE;
        return;
    }
    /* Check memory protection */
    if (!PR && (M[pa] & 0x0001)) {  /* Protection bit set and PR=0 */
        trp_req_bits |= (1 << TRAP_PM);
        trap_pending = TRUE;
        return;
    }
    M[pa] = val;
}
uint8 read_byte(uint16 va) {
    uint16 word_addr = va >> 1;
    uint16 word = read_word(word_addr);
    return (va & 1) ? (word & 0xFF) : ((word >> 8) & 0xFF);
}
void write_byte(uint16 va, uint8 val) {
    uint16 word_addr = va >> 1;
    uint16 word = read_word(word_addr);
    if (va & 1)
        word = (word & 0xFF00) | val;
    else
        word = (word & 0x00FF) | (val << 8);
    write_word(word_addr, word);
}

/* ========== Effective Address Calculation ========== 
The manual defines three instruction classes and addressing modes:
0:	DL, P, DG, IL, IGX, ILX			load/arithmetic
0':	DL, DG, IL, IGX, ILX (same but no P)	store/complex
1:	P, PX, DL				shift, index, base and system operations
2:	RP, RM, IL, IG				conditional or unconditional branch instructions

-IG (Indirect General) is not directly listed with its own mode number in the code. It appears to be handled under mode 4 (DG) in some contexts or combined with other modes in ea_class2().
 *
 * The addressing mode is determined most of time by the opcode byte; bits 0-2 of
 * the instruction word alone do NOT suffice (they are overloaded across
 * classes).  The mapping below is derived directly from the per-opcode
 * (addressing-mode, instruction) table in the CII Mitra-15 reference manual.
 *
 * Addressing mode formulae (D = 8-bit displacement, G' = G in slave mode / 0
 * in master mode):
 *
 *   DL  – Direct Local          : Y = (L + D) & 0x7FFF
 *   P   – Parameter/Immediate   : Y = D            (no memory indirection)
 *   PX  – Parameter Indexed     : Y = D            (Class 1, same as P for EA)
 *   DG  – Direct General        : Y = (G + D) & 0x7FFF
 *   IL  – Indirect Local        : Y = (G' + mem[L + D]) & 0x7FFF
 *   IGX – Indirect General Idx  : Y = (G' + mem[G + D] + X) & 0x7FFF
 *   ILX – Indirect Local Idx    : Y = (G' + mem[L + D] + X) & 0x7FFF
 *   RP  – Relative Plus  (br)   : Y = (P + 2*D) & 0x7FFF
 *   RM  – Relative Minus (br)   : Y = (P - 2*D) & 0x7FFF
 *   IG  – Indirect General (br) : Y = (G' + mem[G + D]) & 0x7FFF
 *
 * Opcodes 34, 3E, 3F, E4, EE, EF, FE, FF are not implemented; this
 * function returns 0 for them (the caller is responsible for rejection).
 *
 Data width:
The Mitra-15 is fundamentally a 16-bit word-addressable machine. Data size is chosen implicitly by the opcode (bits 4–7), not by any extra control bits in the instruction. Examples:
	Byte: LBL (Load Byte Left into A)
	Word: LDA, STA, ADD, SUB, AND, IOR, etc.
	Double word: DLD (Double Load): loads two words → E and A
*/

/* ========== Condition Code Functions (per manual section II-6) ========== */

/* For LOAD instructions: C=1 if result=0, O=1 if result negative */
static void set_condition_codes_load(uint16 result) {
    reg_block[curr_bloc].C = (result == 0) ? 1 : 0;
    reg_block[curr_bloc].OV = (result & 0x8000) ? 1 : 0;
}

/* For COMPARE instructions: 
 * C=1 if A == operand (equality)
 * C=0 if A > operand
 * O=1 if A < operand
 */
static void set_condition_codes_compare(uint16 a, uint16 b, uint16 result) {
    (void) result;
    if (a == b) {
        reg_block[curr_bloc].C = 1;
        reg_block[curr_bloc].OV = 0;
    } else if (a < b) {
        reg_block[curr_bloc].C = 0;
        reg_block[curr_bloc].OV = 1;
    } else {
        reg_block[curr_bloc].C = 0;
        reg_block[curr_bloc].OV = 0;
    }
}

/* For ARITHMETIC instructions:
 * C = carry/borrow
 * O = overflow (operands same sign, result opposite sign)
 */
static void set_condition_codes_arithmetic(uint16 result, uint16 carry, uint16 overflow) {
    reg_block[curr_bloc].C = carry;
    reg_block[curr_bloc].OV = overflow;
}

/* For string operations and tests */
static void set_condition_codes_string(int equal, int less) {
    if (equal) {
        reg_block[curr_bloc].C = 1;
        reg_block[curr_bloc].OV = 0;
    } else if (less) {
        reg_block[curr_bloc].C = 0;
        reg_block[curr_bloc].OV = 1;
    } else {
        reg_block[curr_bloc].C = 0;
        reg_block[curr_bloc].OV = 0;
    }
}

/* ========== Arithmetic Functions ========== */
static uint16 add16(uint16 a, uint16 b, uint16 * carry, uint16 * overflow) {
    uint32 sum = (uint32) a + (uint32) b + * carry;
    uint16 result = sum & 0xFFFF;
    * carry = (sum >> 16) & 1;
    if (((a & 0x8000) == (b & 0x8000)) &&
        ((a & 0x8000) != (result & 0x8000))) {
        * overflow = 1;
    } else {
        * overflow = 0;
    }
    return result;
}

static uint16 sub16(uint16 a, uint16 b, uint16 * carry, uint16 * overflow) {
    uint32 diff = (uint32) a + (uint32)(~b & 0xFFFF) + * carry;
    uint16 result = diff & 0xFFFF;
    * carry = (diff >> 16) & 1;
    if (((a & 0x8000) != (b & 0x8000)) &&
        ((a & 0x8000) != (result & 0x8000))) {
        * overflow = 1;
    } else {
        * overflow = 0;
    }
    return result;
}

static void mul32(uint16 a, uint16 b, uint16 * high, uint16 * low) {
    uint32 product = (uint32)(int16_t) a * (uint32)(int16_t) b;
    * high = (product >> 16) & 0xFFFF;
    * low = product & 0xFFFF;
}

static int div32(uint16 high, uint16 low, uint16 divisor, uint16 * quot, uint16 * rem) {
    int32_t dividend = ((int32_t)(int16_t) high << 16) | (uint16_t) low;
    int16_t dvsr = (int16_t) divisor;
    if (dvsr == 0) return -1;
    * quot = (uint16_t)(dividend / dvsr);
    * rem = (uint16_t)(dividend % dvsr);
    return 0;
}

/* ========== Shift Operations (per manual section VII-7) ========== */

/* Shift Left Logical Single (SLLS) */
static uint16 shift_lls(uint16 val, int count) {
    uint16 result = val;
    int i;
    for (i = 0; i < count; i++) {
        reg_block[curr_bloc].C = (result & 0x8000) ? 1 : 0;
        result <<= 1;
    }
    return result;
}

/* Shift Right Logical Single (SRLS) */
static uint16 shift_rls(uint16 val, int count) {
    uint16 result = val;
    int i;
    for (i = 0; i < count; i++) {
        reg_block[curr_bloc].C = result & 1;
        result >>= 1;
    }
    return result;
}

/* Shift Right Arithmetic Single (SAS) - preserve sign bit */
static uint16 shift_sas(uint16 val, int count) {
    uint16 result = val;
    int i;
    for (i = 0; i < count; i++) {
        reg_block[curr_bloc].C = result & 1;
        uint16 sign = result & 0x8000;
        result >>= 1;
        if (sign) result |= 0x8000;
    }
    return result;
}

/* Shift Right Circular Single (SRCS) */
static uint16 shift_srcs(uint16 val, int count) {
    uint16 result = val;
    int i;
    for (i = 0; i < count; i++) {
        reg_block[curr_bloc].C = result & 1;
        result = (result >> 1) | ((result & 1) << 15);
    }
    return result;
}

/* Shift Left Circular Single (SLCS) */
static uint16 shift_slcs(uint16 val, int count) {
    uint16 result = val;
    int i;
    for (i = 0; i < count; i++) {
        reg_block[curr_bloc].C = (result & 0x8000) ? 1 : 0;
        result = (result << 1) | ((result & 0x8000) ? 1 : 0);
    }
    return result;
}

/* Shift Left Logical Double (SLLD) - shift (E,A) left */
static void shift_lld(uint16 * E, uint16 * A, int count) {
    int i;
    for (i = 0; i < count; i++) {
        reg_block[curr_bloc].C = ( * E & 0x8000) ? 1 : 0;
        * E = ( * E << 1) | (( * A & 0x8000) ? 1 : 0);
        * A <<= 1;
    }
}

/* Shift Right Logical Double (SRLD) */
static void shift_rld(uint16 * E, uint16 * A, int count) {
    int i;
    for (i = 0; i < count; i++) {
        reg_block[curr_bloc].C = * A & 1;
        * A = ( * A >> 1) | (( * E & 1) << 15);
        * E >>= 1;
    }
}

/* Shift Right Arithmetic Double (SAD) */
static void shift_sad(uint16 * E, uint16 * A, int count) {
    int i;
    for (i = 0; i < count; i++) {
        reg_block[curr_bloc].C = * A & 1;
        uint16 sign = * E & 0x8000;
        * A = ( * A >> 1) | (( * E & 1) << 15);
        * E = ( * E >> 1);
        if (sign) * E |= 0x8000;
    }
}

/* Shift Left Circular Double (SLCD) */
static void shift_lcd(uint16 * E, uint16 * A, int count) {
    int i;
    for (i = 0; i < count; i++) {
        uint16 msb = ( * E & 0x8000) ? 1 : 0;
        reg_block[curr_bloc].C = msb;
        * E = ( * E << 1) | (( * A & 0x8000) ? 1 : 0);
        * A = ( * A << 1) | msb;
    }
}

/* Shift Right Circular Double (SRCD) */
static void shift_rcd(uint16 * E, uint16 * A, int count) {
    int i;
    for (i = 0; i < count; i++) {
        uint16 lsb = * A & 1;
        reg_block[curr_bloc].C = lsb;
        * A = ( * A >> 1) | (( * E & 1) << 15);
        * E = ( * E >> 1) | (lsb << 15);
    }
}

/* Normalize (NLZ) - shift left until bit 0 != bit 1 or max steps */
static int normalize(uint16 * E, uint16 * A, uint16 * X, int max_steps) {
    int steps = 0;
    uint32 double_word = ((uint32) * E << 16) | * A;
    while (steps < max_steps && steps < 31) {
        if (((double_word >> 31) & 1) != ((double_word >> 30) & 1))
            break;
        double_word <<= 1;
        steps++;
    }
    * E = (double_word >> 16) & 0xFFFF;
    * A = double_word & 0xFFFF;
    * X -= steps;

    if (steps == 0) {
        reg_block[curr_bloc].C = 0;
        reg_block[curr_bloc].OV = 1;
    } else if (steps == max_steps) {
        reg_block[curr_bloc].C = 1;
        reg_block[curr_bloc].OV = 0;
    } else {
        reg_block[curr_bloc].C = 0;
        reg_block[curr_bloc].OV = 0;
    }
    return steps;
}

/* Compute parity (PTY) - count set bits shifted out */
static uint16 compute_parity(uint16 * A, int count) {
    uint16 result = * A;
    uint16 parity_count = 0;
    int i;
    for (i = 0; i < count; i++) {
        if (result & 0x8000) parity_count++;
        result = (result << 1) | ((result & 0x8000) ? 1 : 0);
    }
    * A = result;
    reg_block[curr_bloc].C = (result & 0x8000) ? 1 : 0;
    reg_block[curr_bloc].OV = 0;
    return parity_count;
}

/* ========== Floating Point (per manual section VII-9) ========== */
/* Mitra-15 uses a base-16 exponent with a characteristic +64 */
static double mitra_to_double(uint16 A, uint16 E) {
    uint32 raw = ((uint32) A << 16) | E;
    int sign = (raw >> 31) & 1;
    int exp = (raw >> 24) & 0x7F;
    uint32 mant = raw & 0xFFFFFF;
    double m = mant / (double)(1 << 24);
    /* Exponent is base-16, characteristic is exp-64 */

    double val = m * pow(16.0, exp - 64);
    return sign ? -val : val;
}

static void double_to_mitra(double v, uint16 * A, uint16 * E) {
    int sign = (v < 0);
    if (sign) v = -v;
    int exp;
    double m = frexp(v, & exp);
    /* Convert from base-2 exponent to base-16 */
    int exp16 = (exp - 1) / 4;
    double m16 = m * pow(2.0, 4 - (exp - 1 - exp16 * 4));
    uint32 mant = (uint32)(m16 * (1 << 24));
    uint32 raw = (sign << 31) | ((exp16 + 64) << 24) | (mant & 0xFFFFFF);
    * A = (raw >> 16) & 0xFFFF;
    * E = raw & 0xFFFF;
}

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
 *       M[word_addr 4] = indicators (C, OV, MS packed)
 *  3. Calls supervisor section 0 via PRTS at absolute address 12 (decimal):
 *       PR and MS are forced to 1 (master mode, protection override)
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

    /* Step 1: Set trap cause bit in absolute memory word 1 (byte address 2) */
    trap_word = read_word(1);
    trap_word |= (0x8000u >> trap);
    write_word(1, trap_word);

    /* Step 2: 
     *Protect bytes 4-9 with faulting context
     * Save faulting context in words 2, 3, 4 
     * "bytes 4-9" = word addresses 2, 3, 4 (2 bytes per word) */
    write_word(2, (pc - GPRIME) & 0x7FFF);
    write_word(3, (reg_block[curr_bloc].L - GPRIME) & 0x7FFF);
    /* Indicators word: PR, MA, MS, OV, C */
    ind_word = ((PR & 1) << 15) | ((MA & 1) << 14) |
        ((MS & 1) << 13) | ((reg_block[curr_bloc].OV & 1) << 12) | ((reg_block[curr_bloc].C &
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
    if (prts_ptr >= MEMsize) {
        return SCPE_STOP;  /* Fatal: no PRTS */
    }
    
    sect0_Pbase = read_word(prts_ptr);
    sect0_Lbase = read_word(prts_ptr + 1);
    
    /* Force master mode with protection override */
    MS = 1;
    PR = 1;
    MA = 1;
    
    reg_block[curr_bloc].L = (sect0_Lbase + reg_block[curr_bloc].G) & 0x7FFF;
    reg_block[curr_bloc].P = (sect0_Pbase + reg_block[curr_bloc].G) & 0x7FFF;
    
    *trappc = pc;
    trap_pending = FALSE;
    trp_req_bits = 0;
    
    return SCPE_OK;
}

/* ========== Suspension System (Section II-8.2) ========== */
/*
 * Suspension system interrupts micro-program to handle urgent I/O.
 * Stack depth: 4 levels
 * Saves: U, J, T registers and B, Tz, To, Ao indicators
 */
t_stat mitra_suspension_request(uint16 susp_level) {
    if (susp_level >= 32) return SCPE_ARG;
    
    susp_req_bits |= (1u << susp_level);
    susp_pending = TRUE;
    
    return SCPE_OK;
}

t_stat mitra_suspension_process(void) {
    if (!susp_pending || susp_stack_ptr >= SUSP_STACK_DEPTH) {
        return SCPE_OK;
    }
    
    /* Find highest priority suspension request */
    int i;
    for (i = 31; i >= 0; i--) {
        if (susp_req_bits & (1u << i)) {
            susp_active_level = i;
            break;
        }
    }
    
    /* Save current micro-processor state to suspension stack */
    SuspContext *ctx = &susp_stack[susp_stack_ptr++];
    ctx->U_reg = U;
    ctx->J_reg = curr_bloc;  /* J register selects block */
    ctx->T_reg = 0;          /* T register (micro-PC) - simulated */
    ctx->B_ind = 0;          /* Micro-processor indicators */
    ctx->Tz_ind = 0;
    ctx->To_ind = 0;
    ctx->Ao_ind = 0;
    ctx->saved_bloc = curr_bloc;
    
    /* Clear the request bit */
    susp_req_bits &= ~(1u << susp_active_level);
    
    /* Execute suspension micro-program (device-specific handler) */
    /* This would call the appropriate device suspension handler */
    io_suspension_dispatch(susp_active_level);
    
    /* Restore micro-processor state */
    if (susp_stack_ptr > 0) {
        SuspContext *rctx = &susp_stack[--susp_stack_ptr];
        U = rctx->U_reg;
        curr_bloc = rctx->saved_bloc;
    }
    
    /* Check if more suspensions pending */
    if (susp_req_bits == 0) {
        susp_pending = FALSE;
    }
    
    return SCPE_OK;
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
    if (int_level >= 32) return SCPE_ARG;
    
    if (high_speed && (cpu_unit.flags & UNIT_HSINT)) {
        /* High-speed interrupt (5μs) */
        /* Save current indicators in block 0, register 6 */
        uint16 ind_word = ((PR & 1) << 15) | ((MA & 1) << 14) |
                         ((MS & 1) << 13) | ((reg_block[0].OV & 1) << 12) |
                         ((reg_block[0].C & 1) << 11);
        
        /* Store in reserved block (block 6 for high-speed) */
        reg_block[6].V = ind_word;  /* Use V register for indicator save */
        
        /* Switch to reserved block */
        curr_bloc = 6;
        
        /* Load indicators from reserved block */
        ind_word = reg_block[6].V;
        PR = (ind_word >> 15) & 1;
        MA = (ind_word >> 14) & 1;
        MS = (ind_word >> 13) & 1;
        reg_block[curr_bloc].OV = (ind_word >> 12) & 1;
        reg_block[curr_bloc].C = (ind_word >> 11) & 1;
        
    } else {
        /* Normal interrupt (30μs) */
        cpt_base = M[10];  /* CPT at absolute address 10 */
        if (cpt_base >= MEMsize) return SCPE_STOP;
        
        uint16 ctx_ptr = read_word(cpt_base + int_level);
        if (ctx_ptr >= MEMsize) return SCPE_STOP;
        
        /* Save current context */
        uint16 ind_word = ((PR & 1) << 15) | ((MA & 1) << 14) |
                         ((MS & 1) << 13) | ((reg_block[curr_bloc].OV & 1) << 12) |
                         ((reg_block[curr_bloc].C & 1) << 11);
        
        write_word(ctx_ptr, ind_word);
        write_word(ctx_ptr + 1, reg_block[curr_bloc].X);
        write_word(ctx_ptr + 2, reg_block[curr_bloc].E);
        write_word(ctx_ptr + 3, reg_block[curr_bloc].A);
        write_word(ctx_ptr + 4, reg_block[curr_bloc].G);
        write_word(ctx_ptr + 5, reg_block[curr_bloc].L);
        write_word(ctx_ptr + 6, reg_block[curr_bloc].P);
        
        /* Switch to new level */
        int_lvl = int_level;
        
        /* Load new context */
        ind_word = read_word(ctx_ptr);
        PR = (ind_word >> 15) & 1;
        MA = (ind_word >> 14) & 1;
        MS = (ind_word >> 13) & 1;
        reg_block[curr_bloc].OV = (ind_word >> 12) & 1;
        reg_block[curr_bloc].C = (ind_word >> 11) & 1;
        reg_block[curr_bloc].X = read_word(ctx_ptr + 1);
        reg_block[curr_bloc].E = read_word(ctx_ptr + 2);
        reg_block[curr_bloc].A = read_word(ctx_ptr + 3);
        reg_block[curr_bloc].G = read_word(ctx_ptr + 4);
        reg_block[curr_bloc].L = read_word(ctx_ptr + 5);
        reg_block[curr_bloc].P = read_word(ctx_ptr + 6);
    }
    
    /* Clear interrupt request */
    intrp_level &= ~(1u << int_level);
    
    return SCPE_OK;
}


/* ========== SIMH Terminal Functions (wrappers) ========== */
int sim_tt_getc(void) {
    /* Get character from SIMH console */
    return sim_poll_kbd();
}

/* Note: sim_tt_inchar, sim_tt_open, sim_tt_close are provided by SIMH */

void set_dyn_map(void) {
    em2_dyn = ((EM2 & 07) << 12) - 020000;
    em3_dyn = ((EM3 & 07) << 12) - 030000;
    usr_map[0] = (RL1 >> 7) & (MAP_PROT | MAP_PAGE);
    usr_map[1] = (RL1 >> 1) & (MAP_PROT | MAP_PAGE);
    usr_map[2] = (RL1 << 5) & (MAP_PROT | MAP_PAGE);
    usr_map[3] = (RL1 << 11) & (MAP_PROT | MAP_PAGE);
    usr_map[4] = (RL2 >> 7) & (MAP_PROT | MAP_PAGE);
    usr_map[5] = (RL2 >> 1) & (MAP_PROT | MAP_PAGE);
    usr_map[6] = (RL2 << 5) & (MAP_PROT | MAP_PAGE);
    usr_map[7] = (RL2 << 11) & (MAP_PROT | MAP_PAGE);
    mon_map[0] = (0 << VA_V_PN);
    mon_map[1] = (1 << VA_V_PN);
    mon_map[2] = (2 << VA_V_PN);
    mon_map[3] = (3 << VA_V_PN);
    mon_map[4] = ((EM2 & 07) << 12);
    mon_map[5] = ((EM2 & 07) << 12) + (1 << VA_V_PN);
    mon_map[6] = (RL4 << 5) & MAP_PAGE;
    mon_map[7] = (RL4 << 11) & MAP_PAGE;
    if (mon_map[6] == 0) mon_map[6] = MAP_PROT;
    if (mon_map[7] == 0) mon_map[7] = MAP_PROT;
}

/*
 * DIT - De-activate Interrupt (Section VII-12)
 * Returns from interrupt subroutine
 */
t_stat mitra_interrupt_return(t_bool high_speed) {
    if (high_speed) {
        /* DITR - Return from high-speed interrupt */
        /* Save indicators in reserved block */
        uint16 ind_word = ((PR & 1) << 15) | ((MA & 1) << 14) |
                         ((MS & 1) << 13) | ((reg_block[curr_bloc].OV & 1) << 12) |
                         ((reg_block[curr_bloc].C & 1) << 11);
        reg_block[curr_bloc].V = ind_word;
        
        /* Return to block 0 */
        curr_bloc = 0;
        
        /* Restore indicators from block 0, register 6 */
        ind_word = reg_block[6].V;
        PR = (ind_word >> 15) & 1;
        MA = (ind_word >> 14) & 1;
        MS = (ind_word >> 13) & 1;
        reg_block[curr_bloc].OV = (ind_word >> 12) & 1;
        reg_block[curr_bloc].C = (ind_word >> 11) & 1;
        
    } else {
        /* DIT - Return from normal interrupt */
        cpt_base = M[10];
        if (cpt_base >= MEMsize) return SCPE_STOP;
        
        uint16 ctx_ptr = read_word(cpt_base + int_lvl);
        if (ctx_ptr >= MEMsize) return SCPE_STOP;
        
        /* Save current context */
        uint16 ind_word = ((PR & 1) << 15) | ((MA & 1) << 14) |
                         ((MS & 1) << 13) | ((reg_block[curr_bloc].OV & 1) << 12) |
                         ((reg_block[curr_bloc].C & 1) << 11);
        
        write_word(ctx_ptr, ind_word);
        write_word(ctx_ptr + 1, reg_block[curr_bloc].X);
        write_word(ctx_ptr + 2, reg_block[curr_bloc].E);
        write_word(ctx_ptr + 3, reg_block[curr_bloc].A);
        write_word(ctx_ptr + 4, reg_block[curr_bloc].G);
        write_word(ctx_ptr + 5, reg_block[curr_bloc].L);
        write_word(ctx_ptr + 6, reg_block[curr_bloc].P);
        
        /* Find next highest pending interrupt */
        int next_lvl = -1;
        for (int i = 31; i >= 0; i--) {
            if (intrp_level & (1u << i)) {
                next_lvl = i;
                break;
            }
        }
        
        if (next_lvl >= 0) {
            /* Accept next interrupt */
            int_lvl = next_lvl;
            ctx_ptr = read_word(cpt_base + int_lvl);
            
            ind_word = read_word(ctx_ptr);
            PR = (ind_word >> 15) & 1;
            MA = (ind_word >> 14) & 1;
            MS = (ind_word >> 13) & 1;
            reg_block[curr_bloc].OV = (ind_word >> 12) & 1;
            reg_block[curr_bloc].C = (ind_word >> 11) & 1;
            reg_block[curr_bloc].X = read_word(ctx_ptr + 1);
            reg_block[curr_bloc].E = read_word(ctx_ptr + 2);
            reg_block[curr_bloc].A = read_word(ctx_ptr + 3);
            reg_block[curr_bloc].G = read_word(ctx_ptr + 4);
            reg_block[curr_bloc].L = read_word(ctx_ptr + 5);
            reg_block[curr_bloc].P = read_word(ctx_ptr + 6);
        } else {
            /* Return to level 0 */
            int_lvl = 0;
        }
    }
    
    return SCPE_OK;
}

/* Helper to get highest pending interrupt */
static int get_highest_interrupt(void) {
    int i;
    for (i = 31; i >= 0; i--) {
        if (intrp_level & (1u << i))
            return i;
    }
    return -1;
}

/* ========== Main Instruction Execution ========== */

/* Shift table for SHR instruction (manual page 7-38) */
typedef enum {
    SHIFT_SLLS = 0,
        SHIFT_SRCS = 1,
        SHIFT_SAD = 2,
        SHIFT_SLCD = 3,
        SHIFT_SLCS = 4,
        SHIFT_SAS = 5,
        SHIFT_SRLS = 6,
        SHIFT_SRCD = 7
}
shift_type_t;

/* SRG operation codes (manual page 7-55) */
typedef enum {
    SRG_RTS = 0x00, /* Return Section */
        SRG_XAE = 0x02, /* Exchange A and E */
        SRG_XAX = 0x04, /* Exchange A and X */
        SRG_XEX = 0x06, /* Exchange E and X */
        SRG_XAA = 0x08, /* Exchange bytes of A */
        SRG_CCE = 0x0A, /* Complement E */
        SRG_RSV = 0x0C, /* Return Supervisor */
        SRG_ACE = 0x0E, /* Add Carry to E */
        SRG_CCA = 0x10, /* Complement A */
        SRG_AEE = 0x12, /* A XOR E */
        SRG_CNX = 0x14, /* Copy Negative X */
        SRG_AIE = 0x16, /* A OR E */
        SRG_AAE = 0x18, /* A AND E */
        SRG_LNE = 0x1A, /* Load -1 into E */
        SRG_CNA = 0x1C, /* Copy Negative A */
        SRG_CHX = 0x1E /* Compute Half X */
}
srg_op_t;

/*
* Rough instruction binary layout
*
* Bits 0 to 3:	4 bits, addressing mode / class selector
* Bits 4 to 7:	4 bits, instruction opcode
* Bits 8 to 15:	8 bits, displacement

Bits 0-2 do not completely specify the address mode:
0	000:	DL, 
1	001:	P, DL, 
2	010:	DG, 
3	011:	IL,
4	100:	IGX, 
5	101:	ILX,
6	110:	RP, RM, IL, IG
7	111:	DG, IL, PX, P,
*/
t_stat one_inst(uint16 inst, uint16 pc, uint32 mode, uint16 * trappc) {
    uint16 opcode = (inst >> I_OPCODE_SHIFT) & 0x1F;
    uint16 disp = inst & I_DISP_MASK;
    uint16 ea, data, data2, result;
    uint16 carry, overflow;
    int i, count;
    uint8 s_byte, d_byte;
    * trappc = pc;
    carry = 0;
    overflow = 0;
    
    /* Check for privileged instruction in slave mode */
    /* Privileged opcodes: 0x3A (STR), 0x3B (LDP), 0x3D (TES), 
       0xF4 (SYS: STM, CLM, DIT, RD, WD), 0xEA (STR in PX) */
    if (mode == 0) {  /* Slave mode */
        uint16 hexcode = inst & 0xF000;
        if ((hexcode == 0x3000 && (opcode == 0x0A || opcode == 0x0B || opcode == 0x0D)) ||
            (hexcode == 0xE000 && opcode == 0x0A) ||
            (hexcode == 0xF000 && opcode == 0x04)) {  /* SYS instructions */
            /* Check if it's a privileged SYS function */
            if (opcode == 0x04) {  /* SYS */
                if (disp == 0x01 || disp == 0x03 || disp == 0x08 || disp == 0x0C || disp == 0x20) {
                    /* DIT, WD, STM, CLM, DITR are privileged */
                    return mitra_trap(TRAP_VM, pc, trappc);
                }
            } else {
                return mitra_trap(TRAP_VM, pc, trappc);
            }
        }
    }
    
    uint16 hexcode = inst & 0xF000;

    // We don't care of address mode here, it's: A first layer of dispatcher
    switch (hexcode) {
        case 0x0000:
            group_1_DL(inst);
            break;
        case 0x1000:
            group_2_DL(inst, mode);
            break;
        case 0x2000:
            group_1_P(inst);
            break;
        case 0x3000:
            group_3_DL(inst, mode);
            break;
        case 0x4000:
            group_1_DG(inst);
            break;
        case 0x5000:
            group_2_DG(inst, mode);
            break;
        case 0x6000:
            group_1_IL(inst);
            break;
        case 0x7000:
            group_2_IL(inst, mode);
            break;
        case 0x8000:
            group_1_IGX(inst);
            break;
        case 0x9000:
            group_2_IGX(inst, mode);
            break;
        case 0xA000:
            group_1_ILX(inst);
            break;
        case 0xB000:
            group_2_ILX(inst, mode);
            break;
        case 0xC000: {
            uint16 Crpcode = inst & 0x0800;
            switch (Crpcode) {
                case 0x00:
                    group_4_RP(inst);
                    break;
                case 0x08:
                    group_4_RM(inst);
                    break;
            }
            break;
        }
        case 0xD000: {
            uint16 Drpcode = inst & 0x0800;
            switch (Drpcode) {
                case 0x00:
                    group_5_IL(inst);
                    break;
                case 0x08:
                    group_5_IG(inst);
                    break;
            }
            break;
        }
        case 0xE000:
            group_3_PX(inst, mode);
            break;
        case 0xF000:
            group_3_P(inst, mode);
            break;
    }
    
    /* Check for traps after instruction execution */
    if (trap_pending) {
        return mitra_trap(trap_cause, pc, trappc);
    }
    
    return SCPE_OK;
}

uint16 group_1_DL(uint16 inst) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      | 0 0  0 | 0| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *	
     *    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
     *    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
     *
     *    Byte, word or double-word, located in the first 256 bytes of the local segment.
     *    Y = (L) + D
     */
    uint16 op = inst >> 8; /* full 8-bit opcode byte              */
    uint16 disp = inst & I_DISP_MASK; /* 8-bit unsigned displacement / param */
    uint16 tmp;
    uint16 target_address;
    target_address = (reg_block[curr_bloc].L + disp) & 0x7FFF;
    group_1(target_address, inst);
    return 0;
}

uint16 group_2_DL(uint16 inst, uint32 mode) {
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    target_address = (reg_block[curr_bloc].L + disp) & 0x7FFF;
    group_2(inst, target_address, mode);
    return 0;
}

uint16 group_1_P(uint16 inst) {
    /*
    *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
    *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    *      | 0 0  1 | 0| x x  x  x |     displacement      |
    *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    *      (P addressing mode) 
    *    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
    *    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
    *
    * A byte operand is specified in the displacement field of the instruction. 
    * This byte may be extended on the left by 8 leading zeroes, if required.
    *
    *   Y = (P) parameter or immediate
    *
    */
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    target_address = (reg_block[curr_bloc].P - 2) & 0x0FF;
    group_1(target_address, inst);
    return 0;
}

uint16 group_3_DL(uint16 inst, uint32 mode) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      | 0 0  1 | 1| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    
     *    "SHR", "SRG", "ICX", "DCX", "", "ICL", "DCL", "CSV",
     *    "CLS", "LDR", "STR", "LDP", "SHC", "TES", "",  "",
     *
     * Byte, word or double-word located in the first 256 bytes of the local segment.
     *  Y = (L) + D
     */
    uint16 opcode = (inst >> I_OPCODE_SHIFT);
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 data;
    uint16 target_address;
    uint16 count;
    target_address = (reg_block[curr_bloc].L + disp) & 0x7FFF;

    switch (opcode) {
        case 0x30: {
            /* SHR - Shift Register (DL mode: shift word read from memory) */
            /* Shift word from memory (16-bit); use low byte for parameters.
             * Bit layout of shift parameter byte: bits[7:5]=type, bits[4:0]=count.
             * Verified: &23h=SRCS3, &E8h=SRCD8, &41h=SAD1. */
            uint8 shr_word = (uint8)(read_word(target_address) & 0xFF);
            shift_type_t type = (shr_word >> 5) & 0x07;
            count = shr_word & 0x1F;
            switch (type) {
                case SHIFT_SLLS:
                    reg_block[curr_bloc].A = shift_lls(reg_block[curr_bloc].A, count);
                    break;
                case SHIFT_SRCS:
                    reg_block[curr_bloc].A = shift_srcs(reg_block[curr_bloc].A, count);
                    break;
                case SHIFT_SAD:
                    shift_sad( & reg_block[curr_bloc].E, & reg_block[curr_bloc].A, count);
                    break;
                case SHIFT_SLCD:
                    shift_lcd( & reg_block[curr_bloc].E, & reg_block[curr_bloc].A, count);
                    break;
                case SHIFT_SLCS:
                    reg_block[curr_bloc].A = shift_slcs(reg_block[curr_bloc].A, count);
                    break;
                case SHIFT_SAS:
                    reg_block[curr_bloc].A = shift_sas(reg_block[curr_bloc].A, count);
                    break;
                case SHIFT_SRLS:
                    reg_block[curr_bloc].A = shift_rls(reg_block[curr_bloc].A, count);
                    break;
                case SHIFT_SRCD:
                    shift_rcd( & reg_block[curr_bloc].E, & reg_block[curr_bloc].A, count);
                    break;
            }
            set_condition_codes_load(reg_block[curr_bloc].A);
        }
        break;
        case 0x31: {
            srg_op_t srg_op = disp & 0x1E;
            switch (srg_op) {
                case SRG_RTS: {
                    uint16 saved_P = read_word(reg_block[curr_bloc].L) + GPRIME;
                    uint16 saved_L = read_word(reg_block[curr_bloc].L + 2) + GPRIME;
                    reg_block[curr_bloc].P = saved_P;
                    reg_block[curr_bloc].L = saved_L;
                    break;
                }
                case SRG_XAE:
                    data = reg_block[curr_bloc].A;
                    reg_block[curr_bloc].A = reg_block[curr_bloc].E;
                    reg_block[curr_bloc].E = data;
                    break;
                case SRG_XAX:
                    data = reg_block[curr_bloc].A;
                    reg_block[curr_bloc].A = reg_block[curr_bloc].X;
                    reg_block[curr_bloc].X = data;
                    break;
                case SRG_XEX:
                    data = reg_block[curr_bloc].E;
                    reg_block[curr_bloc].E = reg_block[curr_bloc].X;
                    reg_block[curr_bloc].X = data;
                    break;
                case SRG_XAA:
                    reg_block[curr_bloc].A = ((reg_block[curr_bloc].A & 0xFF) << 8) | ((
                        reg_block[curr_bloc].A >> 8) & 0xFF);
                    break;
                case SRG_CCE:
                    reg_block[curr_bloc].E = ~reg_block[curr_bloc].E & 0xFFFF;
                    break;
                case SRG_RSV:
                    if (mode != 1) return MM_PRVINS;
                    {
                        uint16 saved_flags = read_word(reg_block[curr_bloc].G + 4);
                        reg_block[curr_bloc].C = (saved_flags >> 14) & 1;
                        reg_block[curr_bloc].OV = (saved_flags >> 13) & 1;
                        MS = 0;
                        MA = (saved_flags >> 12) & 1;
                        PR = (saved_flags >> 11) & 1;
                    }
                    reg_block[curr_bloc].L = (reg_block[curr_bloc].G + read_word(reg_block[
                        curr_bloc].G + 2)) & 0x7FFF;
                    reg_block[curr_bloc].P = (reg_block[curr_bloc].G + 2 + read_word(reg_block[
                        curr_bloc].G)) & 0x7FFF;
                    break;
                case SRG_ACE:
                    reg_block[curr_bloc].E = (reg_block[curr_bloc].E + reg_block[curr_bloc].C) &
                        0xFFFF;
                    break;
                case SRG_CCA:
                    reg_block[curr_bloc].A = ~reg_block[curr_bloc].A & 0xFFFF;
                    set_condition_codes_load(reg_block[curr_bloc].A);
                    break;
                case SRG_AEE:
                    reg_block[curr_bloc].A ^= reg_block[curr_bloc].E;
                    set_condition_codes_load(reg_block[curr_bloc].A);
                    break;
                case SRG_CNX:
                    reg_block[curr_bloc].X = (~reg_block[curr_bloc].X + 1) & 0xFFFF;
                    break;
                case SRG_AIE:
                    reg_block[curr_bloc].A |= reg_block[curr_bloc].E;
                    set_condition_codes_load(reg_block[curr_bloc].A);
                    break;
                case SRG_AAE:
                    reg_block[curr_bloc].A &= reg_block[curr_bloc].E;
                    set_condition_codes_load(reg_block[curr_bloc].A);
                    break;
                case SRG_LNE:
                    reg_block[curr_bloc].E = 0xFFFF;
                    break;
                case SRG_CNA:
                    reg_block[curr_bloc].A = (~reg_block[curr_bloc].A + 1) & 0xFFFF;
                    set_condition_codes_load(reg_block[curr_bloc].A);
                    break;
                case SRG_CHX:
                    reg_block[curr_bloc].X = (reg_block[curr_bloc].X >> 1) | (reg_block[
                        curr_bloc].X & 0x8000);
                    break;
                default:
                    break;
            }
        }
        break;
        case 0x32:
            reg_block[curr_bloc].X = (reg_block[curr_bloc].X + read_word(target_address)) &
                0x7FFF;
            set_condition_codes_load(reg_block[curr_bloc].X);
            break;
        case 0x33:
            reg_block[curr_bloc].X = (reg_block[curr_bloc].X - read_word(target_address)) &
                0x7FFF;
            set_condition_codes_load(reg_block[curr_bloc].X);
            break;
        case 0x34:
            break;
        case 0x35:
            reg_block[curr_bloc].L = (reg_block[curr_bloc].L + read_word(target_address)) &
                0x7FFF;
            break;
        case 0x36:
            reg_block[curr_bloc].L = (reg_block[curr_bloc].L - read_word(target_address)) &
                0x7FFF;
            break;
        case 0x37: {
            uint16 section = target_address;
            write_word(reg_block[curr_bloc].G, reg_block[curr_bloc].P - GPRIME);
            write_word(reg_block[curr_bloc].G + 2, reg_block[curr_bloc].L - GPRIME);
            write_word(reg_block[curr_bloc].G + 4, (reg_block[curr_bloc].C ? 1 : 0) | (
                reg_block[curr_bloc].OV ? 2 : 0) | (MS ? 4 : 0));
            uint16 PRTS_addr = read_word(12);
            reg_block[curr_bloc].L = ((PRTS_addr - (4 * section)) + reg_block[curr_bloc].G) &
                0x7FFF;
            reg_block[curr_bloc].P = ((PRTS_addr - (4 * section) + 2) + reg_block[curr_bloc]
                .G) & 0x7FFF;
            MS = 1;
            PR = 1;
        }
        break;
        case 0x38: {
            uint16 section = target_address;
            uint16 called_Lbase = read_word((reg_block[curr_bloc].G - 4 * section + 2) &
            0x7FFF);
            uint16 called_Pbase = read_word((reg_block[curr_bloc].G - 4 * section) & 0x7FFF);
            uint16 LDS = (called_Lbase + reg_block[curr_bloc].G) & 0x7FFF;
            write_word(LDS, (reg_block[curr_bloc].P - GPRIME) & 0x7FFF);
            write_word(LDS + 2, (reg_block[curr_bloc].L - GPRIME) & 0x7FFF);
            reg_block[curr_bloc].L = LDS;
            reg_block[curr_bloc].P = (called_Pbase + reg_block[curr_bloc].G) & 0x7FFF;
        }
        break;
        case 0x39: {
            uint16 reg_num = target_address & 0x3F;
            switch (reg_num & 0x07) {
                case 0:
                    reg_block[curr_bloc].A = reg_block[curr_bloc].A;
                    break;
                case 1:
                    reg_block[curr_bloc].A = reg_block[curr_bloc].E;
                    break;
                case 2:
                    reg_block[curr_bloc].A = reg_block[curr_bloc].P;
                    break;
                case 3:
                    reg_block[curr_bloc].A = reg_block[curr_bloc].X;
                    break;
                case 4:
                    reg_block[curr_bloc].A = reg_block[curr_bloc].L;
                    break;
                case 5:
                    reg_block[curr_bloc].A = reg_block[curr_bloc].G;
                    break;
                default:
                    break;
            }
            set_condition_codes_load(reg_block[curr_bloc].A);
        }
        break;
        case 0x3A:
            if (mode != 1) return MM_PRVINS;
            {
                uint16 reg_num = target_address & 0x3F;
                switch (reg_num & 0x07) {
                    case 0:
                        reg_block[curr_bloc].A = reg_block[curr_bloc].A;
                        break;
                    case 1:
                        reg_block[curr_bloc].A = reg_block[curr_bloc].A;
                        break;
                    case 2:
                        reg_block[curr_bloc].P = reg_block[curr_bloc].A & 0x7FFF;
                        break;
                    case 3:
                        reg_block[curr_bloc].X = reg_block[curr_bloc].A;
                        break;
                    case 4:
                        reg_block[curr_bloc].L = reg_block[curr_bloc].A & 0x7FFF;
                        break;
                    case 5:
                        reg_block[curr_bloc].G = reg_block[curr_bloc].A;
                        break;
                    default:
                        break;
                }
            }
            break;
        case 0x3B:
            if (mode != 1)
                return MM_PRVINS;
            PR = read_word(target_address) & 1;
            break;
        case 0x3C: {
            uint8 shc_word = (uint8)(read_word(target_address) & 0xFF);
            uint8 shc_type = (shc_word >> 5) & 0x07;
            count = shc_word & 0x1F;
            switch (shc_type) {
                case 0:
                    shift_lld( & reg_block[curr_bloc].E, & reg_block[curr_bloc].A, count);
                    break;
                case 1:
                    if (mode != 1) return MM_PRVINS;
                    if (!(cpu_unit.flags & UNIT_HSINT))
                        return MM_INVINS;
                    intrp_level &= ~(1u << int_lvl);
                    break;
                case 2:
                    reg_block[curr_bloc].E = compute_parity( & reg_block[curr_bloc].A, count);
                    break;
                case 3:
                    if (mode != 1) return MM_PRVINS;
                    intrp_level &= ~(1u << int_lvl);
                    int_lvl = 0;
                    break;
                case 4:
                    shift_rld( & reg_block[curr_bloc].E, & reg_block[curr_bloc].A, count);
                    break;
                case 5:
                    break;
                case 6:
                    normalize( & reg_block[curr_bloc].E, & reg_block[curr_bloc].A, & reg_block[
                        curr_bloc].X, count);
                    break;
                case 7:
                    break;
            }
            set_condition_codes_load(reg_block[curr_bloc].A);
        }
        break;
        case 0x3D:
            if (mode != 1) return MM_PRVINS;
            reg_block[curr_bloc].A = read_word(target_address);
            write_word(target_address, 0);
            set_condition_codes_load(reg_block[curr_bloc].A);
            break;
        case 0x3E:
        case 0x3F:
            break;
    }
    return 0;
}

uint16 group_1_DG(uint16 inst) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      | 0 1  0 | 0| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    (DG addressing mode) 
     *    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
     *    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
     *
     * Byte, word or double-word located in the first 256 bytes of the common segment.
     *  Y=(G) + D
     */
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    target_address = (reg_block[curr_bloc].G + disp) & 0x7FFF;
    group_1(target_address, inst);
    return 0;
}

uint16 group_2_DG(uint16 inst, uint32 mode) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      | 0 1  0 | 1| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    
     *    "DLD", "STA", "STE", "STX", "SBL", "SBR", "DST", "ADM",
     *    "SPA", "STS", "FAD", "FSU", "FMU", "FDV", "TRS", "MVS",
     *
     * Byte, word or double-word located in the first 256 bytes of the common segment.
     *  Y=(G) + D
     */
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    target_address = (reg_block[curr_bloc].G + disp) & 0x7FFF;
    group_2(inst, target_address, mode);
    return 0;
}

uint16 group_1_IL(uint16 inst) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      | 0 1  1 | 0| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    (IL addressing mode) 
     *    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
     *    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
     *
     *  Y = (G' + mem[L + D]) & 0x7FFF
     *  Byte, word or double-word located anywhere and pointed at through the local segment.
     */
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    tmp = read_word(reg_block[curr_bloc].L + disp);
    target_address = (GPRIME + tmp) & 0x7FFF;
    group_1(target_address, inst);
    return 0;
}

uint16 group_2_IL(uint16 inst, uint32 mode) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      | 0 1  1 | 1| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    
     *    "DLD", "STA", "STE", "STX", "SBL", "SBR", "DST", "ADM",
     *    "SPA", "STS", "FAD", "FSU", "FMU", "FDV", "TRS", "MVS",
     *
     *  Y = (G' + mem[L + D]) & 0x7FFF
     *  Byte, word or double-word located anywhere and pointed at through the local segment.
     */
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    tmp = read_word(reg_block[curr_bloc].L + disp);
    uint16 target_address;
    target_address = (GPRIME + tmp) & 0x7FFF;
    group_2(inst, target_address, mode);
    return 0;
}

uint16 group_1_IGX(uint16 inst) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  0  0 | 0| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    (IGX addressing mode)
     *    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
     *    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
     *
     * Element of an array pointed at through the common segment.
     *  Y = (G) + ((G)+D) + (reg_block[curr_bloc].X)
     */
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    tmp = read_word(reg_block[curr_bloc].G + disp);
    target_address = (reg_block[curr_bloc].G + tmp + reg_block[curr_bloc].X) & 0x7FFF;
    group_1(target_address, inst);
    return 0;
}

uint16 group_2_IGX(uint16 inst, uint32 mode) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  0  0 | 1| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    
     *    "DLD", "STA", "STE", "STX", "SBL", "SBR", "DST", "ADM",
     *    "SPA", "STS", "FAD", "FSU", "FMU", "FDV", "TRS", "MVS",
     *
     * Element of an array pointed at through the common segment.
     *  Y = (G) + ((G)+D) + (reg_block[curr_bloc].X)
     */
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    tmp = read_word(reg_block[curr_bloc].G + disp);
    target_address = (reg_block[curr_bloc].G + tmp + reg_block[curr_bloc].X) & 0x7FFF;
    group_2(inst, target_address, mode);
    return 0;
}

uint16 group_1_ILX(uint16 inst) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  0  1 | 0| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    
     *    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
     *    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
     *
     * Element of reg_block[curr_bloc].A byte, word or double-word array located anywhere and pointed at through the local segment.
     *  Y = G' + ((L)+D)+(reg_block[curr_bloc].X)
     */
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    tmp = read_word(reg_block[curr_bloc].L + disp);
    target_address = (GPRIME + tmp + reg_block[curr_bloc].X) & 0x7FFF;
    group_1(target_address, inst);
    return 0;
}

uint16 group_2_ILX(uint16 inst, uint32 mode) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  0  1 | 1| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    
     *    "DLD", "STA", "STE", "STX", "SBL", "SBR", "DST", "ADM",
     *    "SPA", "STS", "FAD", "FSU", "FMU", "FDV", "TRS", "MVS",
     *
     * Element of a byte, word or double-word array located anywhere and pointed at through the local segment.
     *  Y = G' + ((L)+D)+(X)
     */
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    tmp = read_word(reg_block[curr_bloc].L + disp);
    target_address = (GPRIME + tmp + reg_block[curr_bloc].X) & 0x7FFF;
    group_2(inst, target_address, mode);
    return 0;
}

uint16 group_4_RP(uint16 inst) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  1  0 | 0| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    (Class 2 - RP addressing mode)
     *    "BCT", "BRX", "BOT", "BCF", "BAN", "BAZ", "BOF", "BRU",
     *
     * Y={P)+2D
     */
    uint16 opcode = (inst >> I_OPCODE_SHIFT);
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    target_address = reg_block[curr_bloc].P + (disp << 1);
    switch (opcode) {
        case 0xC0:
            /* BCT - Branch on Carry True (RP mode) */
            if (reg_block[curr_bloc].C) reg_block[curr_bloc].P = target_address;
            else reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xC1:
            /* BRX - Branch Indexed (RP mode) */
            reg_block[curr_bloc].P = (target_address + reg_block[curr_bloc].X) & 0x7FFF;
            break;
        case 0xC2:
            /* BOT - Branch on Overflow True (RP mode) */
            if (reg_block[curr_bloc].OV) reg_block[curr_bloc].P = target_address;
            else reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xC3:
            /* BCF - Branch on Carry False (RP mode) */
            if (!reg_block[curr_bloc].C) reg_block[curr_bloc].P = target_address;
            else reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xC4:
            /* BAN - Branch on reg_block[curr_bloc].A Negative (RP mode) */
            if (reg_block[curr_bloc].A & 0x8000)
                reg_block[curr_bloc].P = target_address;
            else
                reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xC5:
            /* BAZ - Branch on reg_block[curr_bloc].A Zero (RP mode) */
            if (reg_block[curr_bloc].A == 0)
                reg_block[curr_bloc].P = target_address;
            else
                reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xC6:
            /* BOF - Branch on Overflow False (RP mode) */
            if (!reg_block[curr_bloc].OV)
                reg_block[curr_bloc].P = target_address;
            else
                reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xC7:
            /* BRU - Branch Unconditional (RP mode) */
            reg_block[curr_bloc].P = target_address;
            break;
    }
}

uint16 group_4_RM(uint16 inst) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  1  0 | 0| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    (Class 2 - RM addressing mode)
     *    "BCT", "BRX", "BOT", "BCF", "BAN", "BAZ", "BOF", "BRU",
     */
    uint16 opcode = (inst >> I_OPCODE_SHIFT);
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    target_address = reg_block[curr_bloc].P - (disp << 1);
    switch (opcode) {
        case 0xC8:
            if (reg_block[curr_bloc].C) reg_block[curr_bloc].P = target_address;
            else reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xC9:
            reg_block[curr_bloc].P = (target_address + reg_block[curr_bloc].X) & 0x7FFF;
            break;
        case 0xCA:
            if (reg_block[curr_bloc].OV) reg_block[curr_bloc].P = target_address;
            else reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xCB:
            if (!reg_block[curr_bloc].C) reg_block[curr_bloc].P = target_address;
            else reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xCC:
            if (reg_block[curr_bloc].A & 0x8000)
                reg_block[curr_bloc].P = target_address;
            else
                reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xCD:
            if (reg_block[curr_bloc].A == 0)
                reg_block[curr_bloc].P = target_address;
            else
                reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xCE:
            if (!reg_block[curr_bloc].OV) reg_block[curr_bloc].P = target_address;
            else
                reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xCF:
            reg_block[curr_bloc].P = target_address;
            break;
    }
    return 0;
}

uint16 group_5_IL(uint16 inst) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  1  0 | 1| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    (Class 2 - RM/IL/IG addressing modes)
     *    "BCT", "BRX", "BOT", "BCF", "BAN", "BAZ", "BOF", "BRU",
     *
     * Y = G' + ((L) + D)
     */
    uint16 opcode = (inst >> I_OPCODE_SHIFT);
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    tmp = (reg_block[curr_bloc].L + disp) & 0x7FFF;
    target_address = GPRIME + tmp;
    switch (opcode) {
        case 0xD0:
            if (reg_block[curr_bloc].C)
                reg_block[curr_bloc].P = target_address;
            else
                reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xD1:
            reg_block[curr_bloc].P = (target_address + reg_block[curr_bloc].X) & 0x7FFF;
            break;
        case 0xD2:
            if (reg_block[curr_bloc].OV)
                reg_block[curr_bloc].P = target_address;
            else
                reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xD3:
            if (!reg_block[curr_bloc].C)
                reg_block[curr_bloc].P = target_address;
            else
                reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xD4:
            if (reg_block[curr_bloc].A & 0x8000)
                reg_block[curr_bloc].P = target_address;
            else
                reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xD5:
            if (reg_block[curr_bloc].A == 0)
                reg_block[curr_bloc].P = target_address;
            else
                reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xD6:
            if (!reg_block[curr_bloc].OV)
                reg_block[curr_bloc].P = target_address;
            else
                reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xD7:
            reg_block[curr_bloc].P = target_address;
            break;
    }
    return 0;
}

uint16 group_5_IG(uint16 inst) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  1  0 | 1| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    (Class 2 - RM/IL/IG addressing modes)
     *    "BCT", "BRX", "BOT", "BCF", "BAN", "BAZ", "BOF", "BRU",
     *
     * Y = G'+((G)+D)
     */
    uint16 opcode = (inst >> I_OPCODE_SHIFT);
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    tmp = read_word(reg_block[curr_bloc].G + disp);
    target_address = (GPRIME + tmp) & 0x7FFF;
    switch (opcode) {
        case 0xD8:
            if (reg_block[curr_bloc].C) reg_block[curr_bloc].P = target_address;
            else reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xD9:
            reg_block[curr_bloc].P = (target_address + reg_block[curr_bloc].X) & 0x7FFF;
            break;
        case 0xDA:
            if (reg_block[curr_bloc].OV) reg_block[curr_bloc].P = target_address;
            else reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xDB:
            if (!reg_block[curr_bloc].C) reg_block[curr_bloc].P = target_address;
            else reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xDC:
            if (reg_block[curr_bloc].A & 0x8000) reg_block[curr_bloc].P = target_address;
            else reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xDD:
            if (reg_block[curr_bloc].A == 0) reg_block[curr_bloc].P = target_address;
            else reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xDE:
            if (!reg_block[curr_bloc].OV) reg_block[curr_bloc].P = target_address;
            else reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            break;
        case 0xDF:
            reg_block[curr_bloc].P = target_address;
            break;
    }
    return 0;
}

uint16 group_3_PX(uint16 inst, uint32 mode) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  1  1 | 0| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    (Class 1 - PX addressing mode)
     *    "SHR", "SRG", "ICX", "DCX", "",    "ICL", "DCL", "CSV",
     *
     * (Y) = D+(reg_block[curr_bloc].X)
     *  Y = (P)
     */
    uint16 opcode = (inst >> I_OPCODE_SHIFT);
    uint16 op = inst >> 8;
    uint16 count;
    uint16 data;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    target_address = (disp) & 0x0FF;
    switch (opcode) {
        case 0xE0: {
            shift_type_t type = (disp >> 5) & 0x07;
            count = disp & 0x1F;
            switch (type) {
                case SHIFT_SLLS:
                    reg_block[curr_bloc].A = shift_lls(reg_block[curr_bloc].A, count);
                    break;
                case SHIFT_SRCS:
                    reg_block[curr_bloc].A = shift_srcs(reg_block[curr_bloc].A, count);
                    break;
                case SHIFT_SAD:
                    shift_sad( & reg_block[curr_bloc].E, & reg_block[curr_bloc].A, count);
                    break;
                case SHIFT_SLCD:
                    shift_lcd( & reg_block[curr_bloc].E, & reg_block[curr_bloc].A, count);
                    break;
                case SHIFT_SLCS:
                    reg_block[curr_bloc].A = shift_slcs(reg_block[curr_bloc].A, count);
                    break;
                case SHIFT_SAS:
                    reg_block[curr_bloc].A = shift_sas(reg_block[curr_bloc].A, count);
                    break;
                case SHIFT_SRLS:
                    reg_block[curr_bloc].A = shift_rls(reg_block[curr_bloc].A, count);
                    break;
                case SHIFT_SRCD:
                    shift_rcd( & reg_block[curr_bloc].E, & reg_block[curr_bloc].A, count);
                    break;
            }
            set_condition_codes_load(reg_block[curr_bloc].A);
        }
        break;
        case 0xE1: {
            srg_op_t srg_op = disp & 0x1E;
            switch (srg_op) {
                case SRG_RTS:
                    uint16 saved_P = read_word(reg_block[curr_bloc].L) + GPRIME;
                    uint16 saved_L = read_word(reg_block[curr_bloc].L + 2) + GPRIME;
                    reg_block[curr_bloc].P = saved_P;
                    reg_block[curr_bloc].L = saved_L;
                    break;
                case SRG_XAE:
                    data = reg_block[curr_bloc].A;
                    reg_block[curr_bloc].A = reg_block[curr_bloc].E;
                    reg_block[curr_bloc].E = data;
                    break;
                case SRG_XAX:
                    data = reg_block[curr_bloc].A;
                    reg_block[curr_bloc].A = reg_block[curr_bloc].X;
                    reg_block[curr_bloc].X = data;
                    break;
                case SRG_XEX:
                    data = reg_block[curr_bloc].E;
                    reg_block[curr_bloc].E = reg_block[curr_bloc].X;
                    reg_block[curr_bloc].X = data;
                    break;
                case SRG_XAA:
                    reg_block[curr_bloc].A = ((reg_block[curr_bloc].A & 0xFF) << 8) | ((
                        reg_block[curr_bloc].A >> 8) & 0xFF);
                    break;
                case SRG_CCE:
                    reg_block[curr_bloc].A = ~reg_block[curr_bloc].E & 0xFFFF;
                    break;
                case SRG_RSV:
                    if (mode != 1) return MM_PRVINS;
                    {
                        uint16 saved_flags = read_word(reg_block[curr_bloc].G + 4);
                        reg_block[curr_bloc].C = (saved_flags >> 14) & 1;
                        reg_block[curr_bloc].OV = (saved_flags >> 13) & 1;
                        MA = (saved_flags >> 12) & 1;
                        PR = (saved_flags >> 11) & 1;
                        MS = 0;
                    }
                    reg_block[curr_bloc].L = (reg_block[curr_bloc].G + read_word(reg_block[
                        curr_bloc].G + 2)) & 0x7FFF;
                    reg_block[curr_bloc].P = (reg_block[curr_bloc].G + 2 + read_word(reg_block[
                        curr_bloc].G)) & 0x7FFF;
                    break;
                case SRG_ACE:
                    reg_block[curr_bloc].A = (reg_block[curr_bloc].E + reg_block[curr_bloc].C) &
                        0xFFFF;
                    break;
                case SRG_CCA:
                    reg_block[curr_bloc].A = ~reg_block[curr_bloc].A & 0xFFFF;
                    set_condition_codes_load(reg_block[curr_bloc].A);
                    break;
                case SRG_AEE:
                    reg_block[curr_bloc].A ^= reg_block[curr_bloc].E;
                    set_condition_codes_load(reg_block[curr_bloc].A);
                    break;
                case SRG_CNX:
                    reg_block[curr_bloc].X = (~reg_block[curr_bloc].X + 1) & 0xFFFF;
                    break;
                case SRG_AIE:
                    reg_block[curr_bloc].A |= reg_block[curr_bloc].E;
                    set_condition_codes_load(reg_block[curr_bloc].A);
                    break;
                case SRG_AAE:
                    reg_block[curr_bloc].A &= reg_block[curr_bloc].E;
                    set_condition_codes_load(reg_block[curr_bloc].A);
                    break;
                case SRG_LNE:
                    reg_block[curr_bloc].A = 0xFFFF;
                    break;
                case SRG_CNA:
                    reg_block[curr_bloc].A = (~reg_block[curr_bloc].A + 1) & 0xFFFF;
                    set_condition_codes_load(reg_block[curr_bloc].A);
                    break;
                case SRG_CHX:
                    reg_block[curr_bloc].X = (reg_block[curr_bloc].X >> 1) | (reg_block[
                        curr_bloc].X & 0x8000);
                    break;
                default:
                    break;
            }
        }
        break;
        case 0xE2:
            reg_block[curr_bloc].X = (reg_block[curr_bloc].X + target_address) & 0x7FFF;
            set_condition_codes_load(reg_block[curr_bloc].X);
            break;
        case 0xE3:
            reg_block[curr_bloc].X = (reg_block[curr_bloc].X - target_address) & 0x7FFF;
            set_condition_codes_load(reg_block[curr_bloc].X);
            break;
        case 0xE4:
            break;
        case 0xE5:
            reg_block[curr_bloc].L = (reg_block[curr_bloc].L + target_address) & 0x7FFF;
            break;
        case 0xE6:
            reg_block[curr_bloc].L = (reg_block[curr_bloc].L - target_address) & 0x7FFF;
            break;
        case 0xE7: {
            uint16 section = target_address;
            write_word(reg_block[curr_bloc].G, reg_block[curr_bloc].P - GPRIME);
            write_word(reg_block[curr_bloc].G + 2, reg_block[curr_bloc].L - GPRIME);
            write_word(reg_block[curr_bloc].G + 4, (reg_block[curr_bloc].C ? 1 : 0) | (
                reg_block[curr_bloc].OV ? 2 : 0) | (MS ? 4 : 0));
            uint16 PRTS_addr = read_word(12);
            reg_block[curr_bloc].L = ((PRTS_addr - (4 * section)) + reg_block[curr_bloc].G) &
                0x7FFF;
            reg_block[curr_bloc].P = ((PRTS_addr - (4 * section) + 2) + reg_block[curr_bloc]
                .G) & 0x7FFF;
            MS = 1;
            PR = 1;
        }
        break;
    }
    return 0;
}

uint16 group_3_P(uint16 inst, uint32 mode) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  1  1 | 0| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    (Class 1 - P addressing mode)
     *    "CLS", "LDR", "STR", "LDP", "SHC", "TES", "",    "",
     */
    uint16 opcode = (inst >> I_OPCODE_SHIFT);
    uint16 op = inst >> 8;
    uint16 count;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    target_address = disp;
    switch (opcode) {
        case 0xE8: {
            uint16 section = target_address;
            uint16 called_Lbase = read_word((reg_block[curr_bloc].G - 4 * section + 2) &
            0x7FFF);
            uint16 called_Pbase = read_word((reg_block[curr_bloc].G - 4 * section) & 0x7FFF);
            uint16 LDS = (called_Lbase + reg_block[curr_bloc].G) & 0x7FFF;
            write_word(LDS, (reg_block[curr_bloc].P - GPRIME) & 0x7FFF);
            write_word(LDS + 2, (reg_block[curr_bloc].L - GPRIME) & 0x7FFF);
            reg_block[curr_bloc].L = LDS;
            reg_block[curr_bloc].P = (called_Pbase + reg_block[curr_bloc].G) & 0x7FFF;
        }
        break;
        case 0xE9: {
            uint16 reg_num = target_address & 0x3F;
            switch (reg_num & 0x07) {
                case 0:
                    reg_block[curr_bloc].A = reg_block[curr_bloc].A;
                    break;
                case 1:
                    reg_block[curr_bloc].A = reg_block[curr_bloc].E;
                    break;
                case 2:
                    reg_block[curr_bloc].A = reg_block[curr_bloc].P;
                    break;
                case 3:
                    reg_block[curr_bloc].A = reg_block[curr_bloc].X;
                    break;
                case 4:
                    reg_block[curr_bloc].A = reg_block[curr_bloc].L;
                    break;
                case 5:
                    reg_block[curr_bloc].A = reg_block[curr_bloc].G;
                    break;
                default:
                    break;
            }
            set_condition_codes_load(reg_block[curr_bloc].A);
        }
        break;
        case 0xEA:
            if (mode != 1) return MM_PRVINS;
            {
                uint16 reg_num = target_address & 0x3F;
                switch (reg_num & 0x07) {
                    case 0:
                        reg_block[curr_bloc].A = reg_block[curr_bloc].A;
                        break;
                    case 1:
                        reg_block[curr_bloc].A = reg_block[curr_bloc].A;
                        break;
                    case 2:
                        reg_block[curr_bloc].P = reg_block[curr_bloc].A & 0x7FFF;
                        break;
                    case 3:
                        reg_block[curr_bloc].X = reg_block[curr_bloc].A;
                        break;
                    case 4:
                        reg_block[curr_bloc].L = reg_block[curr_bloc].A & 0x7FFF;
                        break;
                    case 5:
                        reg_block[curr_bloc].G = reg_block[curr_bloc].A;
                        break;
                    default:
                        break;
                }
            }
            break;
        case 0xEB:
            if (mode != 1) return MM_PRVINS;
            PR = read_word(target_address) & 1;
            break;
        case 0xEC:
            count = disp & 0x1F;
            {
                uint8 shc_type = (disp >> 5) & 0x07;
                switch (shc_type) {
                    case 0:
                        shift_lld( & reg_block[curr_bloc].E, & reg_block[curr_bloc].A, count);
                        break;
                    case 1:
                        if (mode != 1) return MM_PRVINS;
                        intrp_level &= ~(1u << int_lvl);
                        int_lvl = 0;
                        break;
                    case 2:
                        reg_block[curr_bloc].A = compute_parity( & reg_block[curr_bloc].A,
                            count);
                        break;
                    case 3:
                        if (mode != 1) return MM_PRVINS;
                        intrp_level &= ~(1u << int_lvl);
                        int_lvl = 0;
                        break;
                    case 4:
                        shift_rld( & reg_block[curr_bloc].E, & reg_block[curr_bloc].A, count);
                        break;
                    case 5:
                        break;
                    case 6:
                        normalize( & reg_block[curr_bloc].E, & reg_block[curr_bloc].A, &
                            reg_block[curr_bloc].X, count);
                        break;
                    case 7:
                        break;
                }
                set_condition_codes_load(reg_block[curr_bloc].A);
            }
            break;
        case 0xED:
            if (mode != 1) return MM_PRVINS;
            reg_block[curr_bloc].A = read_word(target_address);
            write_word(target_address, 0);
            set_condition_codes_load(reg_block[curr_bloc].A);
            break;
        case 0xEE:
        case 0xEF:
            break;
    }
    return 0;
}

uint16 group_1(uint16 target_address, uint16 inst) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |                                   |  opcode   |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     */
    uint16 opcode = (inst >> I_GROUP_SHIFT) & 0x1F;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 carry, overflow;
    switch (opcode) {
        case 0x00:
            reg_block[curr_bloc].A = read_word(target_address);
            set_condition_codes_load(reg_block[curr_bloc].A);
            break;
        case 0x01:
            reg_block[curr_bloc].E = read_word(target_address);
            set_condition_codes_load(reg_block[curr_bloc].E);
            break;
        case 0x02:
            reg_block[curr_bloc].X = read_word(target_address);
            set_condition_codes_load(reg_block[curr_bloc].X);
            break;
        case 0x03:
            reg_block[curr_bloc].A ^= read_word(target_address);
            set_condition_codes_load(reg_block[curr_bloc].A);
            break;
        case 0x04:
            reg_block[curr_bloc].A = (target_address - GPRIME) & 0x7FFF;
            set_condition_codes_load(reg_block[curr_bloc].A);
            break;
        case 0x05:
            carry = 0;
            reg_block[curr_bloc].A = add16(reg_block[curr_bloc].A, read_word(target_address), &
                carry, & overflow);
            set_condition_codes_arithmetic(reg_block[curr_bloc].A, carry, overflow);
            break;
        case 0x06:
            carry = 0;
            reg_block[curr_bloc].A = sub16(reg_block[curr_bloc].A, read_word(target_address), &
                carry, & overflow);
            set_condition_codes_arithmetic(reg_block[curr_bloc].A, carry, overflow);
            break;
        case 0x07:
            reg_block[curr_bloc].A = reg_block[curr_bloc].A | read_word(target_address);
            set_condition_codes_load(reg_block[curr_bloc].A);
            break;
    }
    return 0;
}

uint16 group_2(uint16 inst, uint16 target_address, uint32 mode) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |                                   |  opcode   |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     */
    uint16 opcode = (inst >> I_GROUP_SHIFT) & 0x1F;
    uint16 disp = inst & I_DISP_MASK;
    uint16 data;
    uint16 tmp;
    uint16 carry, overflow;
    uint8 s_byte, d_byte;
    int i;
    switch (opcode) {
        case 0x08:
            if (mode != 1)
                return MM_PRVINS;
            if (!(cpu_unit.flags & UNIT_MULDIV))
                return MM_INVINS;
            data = read_word(target_address);
            if (div32(reg_block[curr_bloc].E, reg_block[curr_bloc].A, data, & reg_block[
                    curr_bloc].A, & reg_block[curr_bloc].E) != 0) {
                reg_block[curr_bloc].OV = 1;
            }
            set_condition_codes_load(reg_block[curr_bloc].A);
            break;
        case 0x09:
            reg_block[curr_bloc].A = reg_block[curr_bloc].A & read_word(target_address);
            set_condition_codes_load(reg_block[curr_bloc].A);
            break;
        case 0x0A:
            if (!(cpu_unit.flags & UNIT_EXTINS))
                return MM_INVINS;
            s_byte = read_byte(disp);
            for (i = 0; i < reg_block[curr_bloc].E; i++) {
                d_byte = read_byte(reg_block[curr_bloc].G + reg_block[curr_bloc].A + i);
                if (s_byte == d_byte) {
                    reg_block[curr_bloc].A = reg_block[curr_bloc].G + reg_block[curr_bloc].A +
                    i;
                    reg_block[curr_bloc].E = 0;
                    set_condition_codes_string(0, s_byte < d_byte);
                    break;
                }
            }
            if (i == reg_block[curr_bloc].E) {
                reg_block[curr_bloc].A = reg_block[curr_bloc].G + reg_block[curr_bloc].A;
                reg_block[curr_bloc].E = 0;
                set_condition_codes_string(1, 0);
            }
            break;
        case 0x0B:
            data = read_word(target_address);
            sub16(reg_block[curr_bloc].A, data, & carry, & overflow);
            set_condition_codes_compare(reg_block[curr_bloc].A, data, 0);
            break;
        case 0x0C:
            if (!(cpu_unit.flags & UNIT_MULDIV))
                return MM_INVINS;
            data = read_word(target_address);
            mul32(reg_block[curr_bloc].A, data, & reg_block[curr_bloc].E, & reg_block[curr_bloc]
                .A);
            set_condition_codes_load(reg_block[curr_bloc].E);
            break;
        case 0x0D:
            data = read_word(target_address);
            reg_block[curr_bloc].A = (reg_block[curr_bloc].A & 0x00FF) | (data & 0xFF00);
            set_condition_codes_load(reg_block[curr_bloc].A);
            break;
        case 0x0E:
            data = read_word(target_address);
            reg_block[curr_bloc].A = data & 0x00FF;
            set_condition_codes_load(reg_block[curr_bloc].A);
            break;
        case 0x0F:
            data = read_word(target_address);
            reg_block[curr_bloc].X = data & 0x00FF;
            set_condition_codes_load(reg_block[curr_bloc].X);
            break;
    }
    return 0;
}

/* ========== SIMH Interface Functions ========== */
t_stat sim_instr(void) {
    uint16 inst, save_P, trap_P;
    t_stat reason = 0;
    intrp_level = intrp_level & ~1;
    set_dyn_map();
    io_poll_devices(); // also for checking front panel
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

        /* Check for traps
         * Traps are not implemented in the current simulator FIXME
         * The manual (section II-8.2) describes a hardware suspension system distinct from interrupts, operating at the micro-program level with a 4-deep stack.
         * Suspensions are used to couple peripherals requiring "urgent or frequent transfers" with 300 μs maximum response time. 
         * Currently the emulator has no suspension machinery whatsoever. 
         * For DMA-style peripherals relying on suspensions, this means those transfer paths are non-functional.
         */

        /* Check for interrupts */
        int_reqhi = get_highest_interrupt();
        
        if ((MA == 0) && (int_reqhi >= 0) && (int_reqhi > int_lvl)) {
            uint16 pa = int_vec[int_reqhi];
            if (pa == 0) {
                reason = STOP_ILLVEC;
                break;
            }
            /* Save context of currently-running level (per DIT manual section)
             * CPT is a 32-word table at absolute address M[10].
             * CPT[i] = word address of the context save area for level i.
             * Save area layout: word 0=Indicators, 1=X, 2=E, 3=A, 4=G, 5=L, 6=P */
            if (high_speed == false) {
		    /* Accept interrupt */
		    reason = mitra_interrupt_accept(int_reqhi, high_speed);
		    if (reason != SCPE_OK) break;
		    
		    if (pa != VEC_RTCP && rtc_pie) {
		        intrp_level |= INT_RTCP;
		    }
            } else {
                /* High speed interrupt processing
                 * Acceptance of the high-speed interrupt includes the following operations:
                 *	- Normal interrupts are placed in waiting status until acknowledgment of the high-speed interrupt.
                 *	- Current indicators are saved in register 6 of block O.
                 *	- R12 is loaded with the number of the block which is reserved for high-speed interrupt processing.
                 *	- Indicators are loaded with the contents of register 6 in the reserved block.
                 */
                curr_bloc = intrp_level;
            }

            /* Accept interrupt */
            reason = mitra_interrupt_accept(int_reqhi, high_speed);
            if (reason != SCPE_OK) break;
            
            if (pa != VEC_RTCP && rtc_pie) {
                intrp_level |= INT_RTCP;
            }
        } else {
            /* Normal instruction fetch */
            if (sim_brk_summ) {
                static uint32 bmask[] = {
                    SWMASK('E') | SWMASK('N'),
                    SWMASK('E') | SWMASK('M'),
                    SWMASK('E') | SWMASK('U')
                };
                uint32 btyp = sim_brk_test(reg_block[curr_bloc].P, bmask[cpu_mode]);
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
            trap_P = save_P = reg_block[curr_bloc].P;
            inst = read_word(reg_block[curr_bloc].P);
            reg_block[curr_bloc].P = (reg_block[curr_bloc].P + 2) & 0x7FFF;
            if (inst != 0) {
                reason = one_inst(inst, save_P, cpu_mode, & trap_P);
                if (reason > 0 && reason != STOP_HALT) {
                    reg_block[curr_bloc].P = save_P;
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

/* ========== RTC Functions ========== */
t_stat rtc_svc(UNIT * uptr) {
    if (rtc_pie)
        intrp_level |= INT_RTCP;
    rtc_unit.wait = sim_rtcn_calb(rtc_tps, TMR_RTC);
    sim_activate( & rtc_unit, rtc_unit.wait);
    return SCPE_OK;
}

t_stat rtc_reset(DEVICE * dptr) {
    rtc_pie = 0;
    rtc_unit.wait = sim_rtcn_init(rtc_unit.wait, TMR_RTC);
    sim_activate( & rtc_unit, rtc_unit.wait);
    return SCPE_OK;
}

t_stat rtc_set_freq(UNIT * uptr, int32 val, CONST char * cptr, void * desc) {
    if (cptr)
        return SCPE_ARG;
    if (val != 50 && val != 60)
        return SCPE_IERR;
    rtc_tps = val;
    return SCPE_OK;
}

t_stat rtc_show_freq(FILE * st, UNIT * uptr, int32 val, CONST void * desc) {
    fprintf(st, (rtc_tps == 50) ? "50Hz" : "60Hz");
    return SCPE_OK;
}

/* ========== CPU Reset and Management ========== */
t_stat cpu_reset(DEVICE * dptr) {
    reg_block[curr_bloc].A = reg_block[curr_bloc].E = reg_block[curr_bloc].X = reg_block[
        curr_bloc].L = reg_block[curr_bloc].G = reg_block[curr_bloc].P = S = 0;
    curr_bloc = 0;
    MREG = reg_block[curr_bloc].V = reg_block[curr_bloc].W = U = 0;
    reg_block[curr_bloc].C = reg_block[curr_bloc].OV = MS = 0;
    MA = PR = 0;
    cpu_mode = 0;
    intrp_level = 0;
    int_lvl = 0;
    susp_req_bits = 0;
    susp_stack_ptr = 0;
    trp_req_bits = 0;
    trap_pending = FALSE;
    cpu_running = 0;
    interrupts_enabled = 0;
    routing_enabled = 0;
    panel_addr_lights = 0;
    panel_data_lights = 0;
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

t_stat cpu_ex(t_value * vptr, t_addr addr, UNIT * uptr, int32 sw) {
    uint32 pa = addr & 0x7FFF;
    if (pa >= MEMsize)
        return SCPE_NXM;
    if (vptr != NULL)
        *
        vptr = M[pa] & DMASK;
    return SCPE_OK;
}

t_stat cpu_dep(t_value val, t_addr addr, UNIT * uptr, int32 sw) {
    uint32 pa = addr & 0x7FFF;
    if (pa >= MEMsize)
        return SCPE_NXM;
    M[pa] = val & DMASK;
    return SCPE_OK;
}

/* ========== History Functions ========== */
void inst_hist(uint32 c, uint32 pc, uint32 tp) {
    if (cpu_mode == hst_exclude)
        return;
    hst_p = (hst_p + 1);
    if (hst_p >= hst_lnt)
        hst_p = 0;
    hst[hst_p].typ = tp | (reg_block[curr_bloc].OV << 4) | (cpu_mode << 5);
    hst[hst_p].P = pc;
    hst[hst_p].A = reg_block[curr_bloc].A;
    hst[hst_p].E = reg_block[curr_bloc].E;
    hst[hst_p].X = reg_block[curr_bloc].X;
    hst[hst_p].L = reg_block[curr_bloc].L;
    hst[hst_p].G = reg_block[curr_bloc].G;
    hst[hst_p].S = S;
    hst[hst_p].U = U;
    hst[hst_p].V = reg_block[curr_bloc].V;
    hst[hst_p].W = reg_block[curr_bloc].W;
    hst[hst_p].MREG = MREG;
    hst[hst_p].ea = HIST_NOEA;
}

t_stat cpu_set_hist(UNIT * uptr, int32 val, CONST char * cptr, void * desc) {
    int32 i, lnt;
    t_stat r;
    if (cptr == NULL) {
        for (i = 0; i < hst_lnt; i++)
            hst[i].typ = 0;
        hst_p = 0;
        return SCPE_OK;
    }
    lnt = (int32) get_uint(cptr, 10, HIST_MAX, & r);
    if (r != SCPE_OK || (lnt && lnt < HIST_MIN))
        return SCPE_ARG;
    hst_p = 0;
    if (sim_switches & SWMASK('M'))
        hst_exclude = 1;
    else if (sim_switches & SWMASK('N'))
        hst_exclude = 0;
    else if (sim_switches & SWMASK('U'))
        hst_exclude = 2;
    else
        hst_exclude = BAD_MODE;
    if (hst_lnt) {
        free(hst);
        hst_lnt = 0;
        hst = NULL;
    }
    if (lnt) {
        hst = (InstHistory * ) calloc(lnt, sizeof(InstHistory));
        if (hst == NULL)
            return SCPE_MEM;
        hst_lnt = lnt;
    }
    return SCPE_OK;
}

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
