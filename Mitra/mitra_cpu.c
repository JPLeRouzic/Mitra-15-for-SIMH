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
 */
 
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
#define I_DISP_MASK     0x00FF

#define GPRIME ((MS) ? G : 0)
#define VA_TO_PA(va) ((va) & 0x7FFF)

/* Interrupt vectors */
static const uint32 int_vec[32] = {
    0, 0, 0, 0,
    VEC_FORK, VEC_DRM, VEC_MUXCF, VEC_MUXCO,
    VEC_MUXT, VEC_MUXR, VEC_HEOR, VEC_HZWC,
    VEC_GEOR, VEC_GZWC, VEC_FEOR, VEC_FZWC,
    VEC_EEOR, VEC_EZWC, VEC_DEOR, VEC_DZWC,
    VEC_CEOR, VEC_CZWC, VEC_WEOR, VEC_YEOR,
    VEC_WZWC, VEC_YZWC, VEC_RTCP, VEC_RTCS,
    VEC_IPAR, VEC_CPAR, VEC_PWRF, VEC_PWRO
};

/* API mask for interrupt priority */
static const uint32 api_mask[32] = {
    0xFFFFFFFE, 0xFFFFFFFC, 0xFFFFFFF8, 0xFFFFFFF0,
    0xFFFFFFE0, 0xFFFFFFC0, 0xFFFFFF80, 0xFFFFFF00,
    0xFFFFFE00, 0xFFFFFC00, 0xFFFFF800, 0xFFFFF000,
    0xFFFFE000, 0xFFFFC000, 0xFFFF8000, 0xFFFF0000,
    0xFFFE0000, 0xFFFC0000, 0xFFF80000, 0xFFF00000,
    0xFFE00000, 0xFFC00000, 0xFF800000, 0xFF000000,
    0xFE000000, 0xFC000000, 0xF8000000, 0xF0000000,
    0xE0000000, 0xC0000000, 0x80000000, 0x00000000
};

/* ========== Type Definitions ========== */

typedef struct {
    uint32 typ;
    uint16 P;
    uint16 A;
    uint16 E;
    uint16 X;
    uint16 L;
    uint16 G;
    uint8 C;
    uint8 OV;
    uint8 MS;
    uint16 MA;
    uint16 PR;
    uint16 S;
    uint16 MREG;
    uint16 U;
    uint16 V, W;
    uint32 ea;
} InstHistory;

/* ========== Global State ========== */

uint32 xfr_req = 0;
uint16 cpu_mode;        /* 0=Normal/Slave, 1=Master */
uint16 RL1, RL2, RL4;
uint16 int_req;
uint16 int_lvl;
uint16 ion;
uint16 ion_defer;
uint32 int_reqhi = 0;
uint32 api_lvl = 0;
uint32 api_lvlhi = 0;
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

/* CPU registers */
uint16 A, E, X, L, G, P, S;
uint32 MREG;
uint16 V, W, U;
uint8 C, OV, MS;
uint16 MA, PR;

/* Memory - word addressable */
uint16 M[MAX_MEM_WORDS];
uint32 MEMsize = MEM_32K;

// uint16 TRAP_INVINS;
uint16 MM_INVINS;

/* Front panel / CPU control state */
int cpu_running = 0;           /* 1 = running, 0 = stopped */
int interrupts_enabled = 0;
int routing_enabled = 0;

/* Panel lights - exported so debugger can see them */
extern uint16 panel_addr_lights;
extern uint16 panel_data_lights;

/* ========== Function Prototypes ========== */

uint16 read_word(uint16 va);
void write_word(uint16 va, uint16 val);
uint8 read_byte(uint16 va);
void write_byte(uint16 va, uint8 val);
static uint16 ea_class0(uint16 inst);
static uint16 ea_class2(uint16 inst);
static uint16 add16(uint16 a, uint16 b, uint16 *carry, uint16 *overflow);
static uint16 sub16(uint16 a, uint16 b, uint16 *carry, uint16 *overflow);
static void set_condition_codes_load(uint16 result);
static void set_condition_codes_compare(uint16 a, uint16 b, uint16 result);
static void set_condition_codes_arithmetic(uint16 result, uint16 carry, uint16 overflow);
static void mul32(uint16 a, uint16 b, uint16 *high, uint16 *low);
static int div32(uint16 high, uint16 low, uint16 divisor, uint16 *quot, uint16 *rem);
static void double_to_mitra(double v, uint16 *A, uint16 *E);
static double mitra_to_double(uint16 A, uint16 E);
t_stat mitra_trap(int trap, uint16 pc, uint16* trappc);
uint32 api_findreq(void);
void set_dyn_map(void);
t_stat set_cc(void);
t_stat cpu_ex(t_value *vptr, t_addr addr, UNIT *uptr, int32 sw);
t_stat cpu_dep(t_value val, t_addr addr, UNIT *uptr, int32 sw);
t_stat cpu_reset(DEVICE *dptr);
t_bool cpu_is_pc_a_subroutine_call(t_addr **ret_addrs);
t_stat cpu_set_size(UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat cpu_set_type(UNIT *uptr, int32 val, CONST char *cptr, void *desc);
void inst_hist(uint32 inst, uint32 pc, uint32 typ);
t_stat rtc_inst(uint32 inst);
t_stat rtc_svc(UNIT *uptr);
t_stat rtc_reset(DEVICE *dptr);
t_stat rtc_set_freq(UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat rtc_show_freq(FILE *st, UNIT *uptr, int32 val, CONST void *desc);
uint32 RelocC(int32 va, int32 sw);
static t_stat handle_interrupt(int lvl);
static int get_highest_interrupt(void);
static t_stat check_interrupts(void);
t_stat cpu_set_size(UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat cpu_set_hist(UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat cpu_show_hist(FILE *st, UNIT *uptr, int32 val, CONST void *desc);
void panel_reset(void);

/* ========== SIMH Device Tables ========== */

UNIT cpu_unit = {
    UDATA(NULL, UNIT_FIX + UNIT_BINK, MAX_MEM_WORDS)
};

REG cpu_reg[] = {
    { ORDATA(P, P, 16) },
    { ORDATA(A, A, 16) },
    { ORDATA(E, E, 16) },
    { ORDATA(X, X, 16) },
    { ORDATA(L, L, 16) },
    { ORDATA(G, G, 16) },
    { FLDATA(C, C, 0) },
    { FLDATA(OV, OV, 0) },
    { FLDATA(MS, MS, 0) },
    { FLDATA(MA, MA, 0) },
    { FLDATA(PR, PR, 0) },
    { ORDATA(S, S, 15) },
    { ORDATA(MREG, MREG, 18) },
    { ORDATA(U, U, 16) },
    { ORDATA(V, V, 16) },
    { ORDATA(W, W, 16) },
    { ORDATA(INT_REQ, int_req, 32) },
    { ORDATA(INT_LVL, int_lvl, 5) },
    { FLDATA(ION, ion, 0) },
    { FLDATA(ION_DEFER, ion_defer, 0) },
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

MTAB cpu_mod[] = {
    { UNIT_MSIZE, 4096, NULL, "4K", &cpu_set_size },
    { UNIT_MSIZE, 8192, NULL, "8K", &cpu_set_size },
    { UNIT_MSIZE, 16384, NULL, "16K", &cpu_set_size },
    { UNIT_MSIZE, 32768, NULL, "32K", &cpu_set_size },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO|MTAB_SHP, 0, "HISTORY", "HISTORY",
      &cpu_set_hist, &cpu_show_hist },
    { 0 }
};

DEVICE cpu_dev = {
    "CPU", &cpu_unit, cpu_reg, cpu_mod,
    1, 8, 16, 1, 8, 16,
    &cpu_ex, &cpu_dep, &cpu_reset,
    NULL, NULL, NULL, NULL, 0
};

UNIT rtc_unit = { UDATA(&rtc_svc, 0, 0), 16000 };

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

uint16 read_word(uint16 va)
{
    uint16 pa = VA_TO_PA(va);
    if (pa >= MEMsize) return 0;
    return M[pa];
}

void write_word(uint16 va, uint16 val)
{
    uint16 pa = VA_TO_PA(va);
    if (pa < MEMsize) M[pa] = val;
}

uint8 read_byte(uint16 va)
{
    uint16 word_addr = va >> 1;
    uint16 word = read_word(word_addr);
    return (va & 1) ? (word & 0xFF) : ((word >> 8) & 0xFF);
}

void write_byte(uint16 va, uint8 val)
{
    uint16 word_addr = va >> 1;
    uint16 word = read_word(word_addr);
    if (va & 1)
        word = (word & 0xFF00) | val;
    else
        word = (word & 0x00FF) | (val << 8);
    write_word(word_addr, word);
}

/* ========== Effective Address Calculation ========== */

static uint16 ea_class0(uint16 inst)
{
    uint16 mode = (inst >> I_MODE_SHIFT) & 0x07;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp, addr;
    
    switch (mode) {
        case AM_DL:
            addr = (L + disp) & 0x7FFF;
            break;
        case AM_IL:
            tmp = read_word(L + disp);
            addr = (GPRIME + tmp) & 0x7FFF;
            break;
        case AM_ILX:
            tmp = read_word(L + disp);
            addr = (GPRIME + tmp + X) & 0x7FFF;
            break;
        case AM_DG:
            addr = (G + disp) & 0x7FFF;
            break;
        case AM_IGX:
            tmp = read_word(G + disp);
            addr = (GPRIME + tmp + X) & 0x7FFF;
            break;
        case AM_P:
        case AM_PX:
            addr = disp;
            break;
        default:
            addr = disp;
            break;
    }
    return addr;
}

static uint16 ea_class2(uint16 inst)
{
    uint16 mode = (inst >> I_MODE_SHIFT) & 0x07;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp, addr;
    
    switch (mode) {
        case AM_RP:
            addr = (P + 2 * disp) & 0x7FFF;
            break;
        case AM_RM:
            addr = (P - 2 * disp) & 0x7FFF;
            break;
        case AM_IL:
            tmp = read_word(L + disp);
            addr = (GPRIME + tmp) & 0x7FFF;
            break;
        case AM_DG:
            addr = (GPRIME + G + disp) & 0x7FFF;
            break;
        default:
            addr = disp;
            break;
    }
    return addr;
}

/* ========== Effective Address Calculation ========== 
The manual defines three instruction classes and addressing modes:
0:	DL, P, DG, IL, IGX, ILX			load/arithmetic
0':	DL, DG, IL, IGX, ILX (same but no P)	store/complex
1:	P, PX, DL				shift, index, base and system operations
2:	RP, RM, IL, IG				conditional or unconditional branch instructions

Addressing mode LBL: Load Byte Left in A (Class 0)
DL	OX	000 0 1101	0
P	2X	001 0 1101	1
DG	4X	010 0 1101	2
IL	6X	011 0 1101	3
IGX	8X	100 0 1101	4
ILX	AX	101 0 1101	5

Addressing mode SBL: Store Byte left in A (Class 0')
DL	1X	000 1 0100	0
DG	5X	010 1 0100	2
IL	7X	011 1 0100	3
IGX	9X	100 1 0100	4
ILX	BX	101 1 0100	5

Addressing mode STR: STore Register	 (Class ?)
DL	3A	001 1 1010	1	X9, XA
DG	EA	111 0 1010	7
IL	FA	111 1 1010	7

Addressing mode ICX: InCrement X	 (Class 1)
DL	32	001 1 0010	1 	X1, X7, X8, XB, XD, 
PX	E2	111 0 0010	7	also uses bits 11 to 14 for instruction decoding 
P	F2	111 1 0010	7

Addressing mode ICX: InCrement X	 (Class 1)
DL	32	001 1 0010	1 	X2, X3, X4, X5, X9
PX	E2	111 0 0010	7	
P	F2	111 1 0010	7

Addressing mode SLLD: Shift Left, Logical, DoublE	 (Class 1)
PX	EC	111 0 1100	7	X0, XC
P	FC	111 1 1100	7	also uses bits 8 to 10 for instruction decoding 

Addressing mode XAE: eXchange contents of A and E	 (Class 1)
P	F1	111 1 0001	7	FX except F1

Addressing mode XAE: eXchange contents of A and E	 (Class 1)
P	F1	111 1 0001	7	F11C to F1FD
	also uses bits 11 to 14 for instruction decoding 

Addressing mode BRU: BRanch Unconditional	 (Class 2)
RP	C7	110 0 0111	6	C6 to DE
RM	CF	110 0 1111	6
IL	D7	110 1 0111	6
IG	DF	110 1 1111	6

Addressing Mode:	 			Used in Instruction Class	Notes
DL – Direct Local				Class 0	Y = (L) + D
IL – Indirect Local				Class 0	Y = G' + ((L) + D)
ILX – Indirect Local Indexed			Class 0	Y = G' + ((L) + D) + X
DG – Direct General				Class 0	Y = (G) + D
IG – Indirect General				Class 2 branch to Y=G'+((G}+D}
IGX – Indirect General Indexed			Class 0	Y = G' + ((G) + D) + X
P – Parameter (Immediate)			Class 0 / Class 1		Operand = D (no memory access)
PX – Parameter Indexed				Class 1 only			Usually for indexed parameter ops
RP – Relative Plus				Class 2 (branches)		branch Y = (P) + 2×D
RM – Relative Minus				Class 2 (branches)		branch Y = (P) – 2×D

-IG (Indirect General) is not directly listed with its own mode number in the code. It appears to be handled under mode 4 (DG) in some contexts or combined with other modes in ea_class2().
*/
/*
 * ea_calculate - Compute the effective address (or immediate parameter value)
 * for an instruction, based on the full 8-bit opcode byte (inst >> 8).
 *
 * The addressing mode is fully determined by the opcode byte; bits 0-2 of
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
 */
static uint16 ea_calculate(uint16 inst)
{
    uint16 op  = inst >> 8;          /* full 8-bit opcode byte              */
    uint16 d   = inst & I_DISP_MASK; /* 8-bit unsigned displacement / param */
    uint16 tmp;

    /* ------------------------------------------------------------------ */
    /* 00-1F : DL  –  Direct Local                                         */
    /* Covers Class 0 (00-0F load/arith) and Class 0' (10-1F store)       */
    /* ------------------------------------------------------------------ */
    if (op <= 0x1F)
        return (L + d) & 0x7FFF;

    /* ------------------------------------------------------------------ */
    /* 20-2F : P  –  Parameter (immediate)                                 */
    /* Same 16 instructions as 00-0F but operand IS the displacement.     */
    /* ------------------------------------------------------------------ */
    if (op <= 0x2F)
        return d;

    /* ------------------------------------------------------------------ */
    /* 30-3F : DL  –  Direct Local  (Class 1 shift/index/system group)    */
    /* 34, 3E, 3F are not implemented but still share DL addressing.      */
    /* ------------------------------------------------------------------ */
    if (op <= 0x3F)
        return (L + d) & 0x7FFF;

    /* ------------------------------------------------------------------ */
    /* 40-4F : DG  –  Direct General  (Class 0 load/arith, DG mode)       */
    /* 50-5F : DG  –  Direct General  (Class 0' store, DG mode)           */
    /* ------------------------------------------------------------------ */
    if (op <= 0x5F)
        return (G + d) & 0x7FFF;

    /* ------------------------------------------------------------------ */
    /* 60-6F : IL  –  Indirect Local  (Class 0 load/arith, IL mode)       */
    /* 70-7F : IL  –  Indirect Local  (Class 0' store, IL mode)           */
    /* ------------------------------------------------------------------ */
    if (op <= 0x7F) {
        tmp = read_word(L + d);
        return (GPRIME + tmp) & 0x7FFF;
    }

    /* ------------------------------------------------------------------ */
    /* 80-8F : IGX – Indirect General Indexed (Class 0 load/arith)        */
    /* 90-9F : IGX – Indirect General Indexed (Class 0' store)            */
    /* ------------------------------------------------------------------ */
    if (op <= 0x9F) {
        tmp = read_word(G + d);
        return (GPRIME + tmp + X) & 0x7FFF;
    }

    /* ------------------------------------------------------------------ */
    /* A0-AF : ILX – Indirect Local Indexed  (Class 0 load/arith)         */
    /* B0-BF : ILX – Indirect Local Indexed  (Class 0' store)             */
    /* ------------------------------------------------------------------ */
    if (op <= 0xBF) {
        tmp = read_word(L + d);
        return (GPRIME + tmp + X) & 0x7FFF;
    }

    /* ------------------------------------------------------------------ */
    /* C0-CF : Class 2 branches – RP (C0-C7) and RM (C8-CF)              */
    /* ------------------------------------------------------------------ */
    if (op <= 0xC7)                          /* RP: Relative Plus          */
        return (P + 2 * d) & 0x7FFF;
    if (op <= 0xCF)                          /* RM: Relative Minus         */
        return (P - 2 * d) & 0x7FFF;

    /* ------------------------------------------------------------------ */
    /* D0-DF : Class 2 branches – IL (D0-D7) and IG (D8-DF)              */
    /* ------------------------------------------------------------------ */
    if (op <= 0xD7) {                        /* IL: Indirect Local         */
        tmp = read_word(L + d);
        return (GPRIME + tmp) & 0x7FFF;
    }
    if (op <= 0xDF) {                        /* IG: Indirect General       */
        tmp = read_word(G + d);
        return (GPRIME + tmp) & 0x7FFF;
    }

    /* ------------------------------------------------------------------ */
    /* E0-EF : Class 1 system/index group (PX mode), with two exceptions: */
    /*   EA  → DG  (STR instruction, Direct General)                      */
    /*   E4, EE, EF → not implemented                                     */
    /* All remaining Ex opcodes use PX, which like P returns d directly.  */
    /* ------------------------------------------------------------------ */
    if (op == 0xEA)                          /* STR – DG mode              */
        return (G + d) & 0x7FFF;
    /* E4, EE, EF: not implemented – return 0, caller handles it */
    if (op == 0xE4 || op == 0xEE || op == 0xEF)
        return 0;
    if (op <= 0xEF)                          /* PX: parameter indexed      */
        return d;

    /* ------------------------------------------------------------------ */
    /* F0-FF : Class 1 system group (P mode), with two exceptions:        */
    /*   FA  → IL  (STR instruction, Indirect Local)                      */
    /*   FE, FF → not implemented                                          */
    /* All remaining Fx opcodes use P, which returns d directly.          */
    /* ------------------------------------------------------------------ */
    if (op == 0xFA) {                        /* STR – IL mode              */
        tmp = read_word(L + d);
        return (GPRIME + tmp) & 0x7FFF;
    }
    /* FE, FF: not implemented – return 0, caller handles it */
    if (op == 0xFE || op == 0xFF)
        return 0;
    /* F0-FD (except FA): P mode – operand is the displacement itself     */
    return d;
}

/* ========== Condition Code Functions (per manual section II-6) ========== */

/* For LOAD instructions: C=1 if result=0, O=1 if result negative */
static void set_condition_codes_load(uint16 result)
{
    C = (result == 0) ? 1 : 0;
    OV = (result & 0x8000) ? 1 : 0;
}

/* For COMPARE instructions: 
 * C=1 if A == operand (equality)
 * C=0 if A > operand
 * O=1 if A < operand
 */
static void set_condition_codes_compare(uint16 a, uint16 b, uint16 result)
{
    (void)result;  /* result is a - b, but we use direct comparison per manual */
    if (a == b) {
        C = 1;
        OV = 0;
    } else if (a < b) {
        C = 0;
        OV = 1;
    } else {  /* a > b */
        C = 0;
        OV = 0;
    }
}

/* For ARITHMETIC instructions:
 * C = carry/borrow
 * O = overflow (operands same sign, result opposite sign)
 */
static void set_condition_codes_arithmetic(uint16 result, uint16 carry, uint16 overflow)
{
    C = carry;
    OV = overflow;
}

/* For string operations and tests */
static void set_condition_codes_string(int equal, int less)
{
    if (equal) {
        C = 1;
        OV = 0;
    } else if (less) {
        C = 0;
        OV = 1;
    } else {
        C = 0;
        OV = 0;
    }
}

/* ========== Arithmetic Functions ========== */

static uint16 add16(uint16 a, uint16 b, uint16 *carry, uint16 *overflow)
{
    uint32 sum = (uint32)a + (uint32)b + *carry;
    uint16 result = sum & 0xFFFF;
    
    *carry = (sum >> 16) & 1;
    
    /* Overflow detection: operands same sign, result opposite sign */
    if (((a & 0x8000) == (b & 0x8000)) && 
        ((a & 0x8000) != (result & 0x8000))) {
        *overflow = 1;
    } else {
        *overflow = 0;
    }
    return result;
}

static uint16 sub16(uint16 a, uint16 b, uint16 *carry, uint16 *overflow)
{
    uint32 diff = (uint32)a + (uint32)(~b & 0xFFFF) + *carry;
    uint16 result = diff & 0xFFFF;
    
    *carry = (diff >> 16) & 1;
    
    /* Overflow detection for subtraction */
    if (((a & 0x8000) != (b & 0x8000)) && 
        ((a & 0x8000) != (result & 0x8000))) {
        *overflow = 1;
    } else {
        *overflow = 0;
    }
    return result;
}

static void mul32(uint16 a, uint16 b, uint16 *high, uint16 *low)
{
    uint32 product = (uint32)(int16_t)a * (uint32)(int16_t)b;
    *high = (product >> 16) & 0xFFFF;
    *low = product & 0xFFFF;
}

static int div32(uint16 high, uint16 low, uint16 divisor, uint16 *quot, uint16 *rem)
{
    int32_t dividend = ((int32_t)(int16_t)high << 16) | (uint16_t)low;
    int16_t dvsr = (int16_t)divisor;
    
    if (dvsr == 0) return -1;
    
    *quot = (uint16_t)(dividend / dvsr);
    *rem = (uint16_t)(dividend % dvsr);
    return 0;
}

/* ========== Shift Operations (per manual section VII-7) ========== */

/* Shift Left Logical Single (SLLS) */
static uint16 shift_lls(uint16 val, int count)
{
    uint16 result = val;
    int i;
    for (i = 0; i < count; i++) {
        C = (result & 0x8000) ? 1 : 0;
        result <<= 1;
    }
    return result;
}

/* Shift Right Logical Single (SRLS) */
static uint16 shift_rls(uint16 val, int count)
{
    uint16 result = val;
    int i;
    for (i = 0; i < count; i++) {
        C = result & 1;
        result >>= 1;
    }
    return result;
}

/* Shift Right Arithmetic Single (SAS) - preserve sign bit */
static uint16 shift_sas(uint16 val, int count)
{
    uint16 result = val;
    int i;
    for (i = 0; i < count; i++) {
        C = result & 1;
        uint16 sign = result & 0x8000;
        result >>= 1;
        if (sign) result |= 0x8000;
    }
    return result;
}

/* Shift Right Circular Single (SRCS) */
static uint16 shift_srcs(uint16 val, int count)
{
    uint16 result = val;
    int i;
    for (i = 0; i < count; i++) {
        C = result & 1;
        result = (result >> 1) | ((result & 1) << 15);
    }
    return result;
}

/* Shift Left Circular Single (SLCS) */
static uint16 shift_slcs(uint16 val, int count)
{
    uint16 result = val;
    int i;
    for (i = 0; i < count; i++) {
        C = (result & 0x8000) ? 1 : 0;
        result = (result << 1) | ((result & 0x8000) ? 1 : 0);
    }
    return result;
}

/* Shift Left Logical Double (SLLD) - shift (E,A) left */
static void shift_lld(uint16 *E_reg, uint16 *A_reg, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        C = (*E_reg & 0x8000) ? 1 : 0;
        *E_reg = (*E_reg << 1) | ((*A_reg & 0x8000) ? 1 : 0);
        *A_reg <<= 1;
    }
}

/* Shift Right Logical Double (SRLD) */
static void shift_rld(uint16 *E_reg, uint16 *A_reg, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        C = *A_reg & 1;
        *A_reg = (*A_reg >> 1) | ((*E_reg & 1) << 15);
        *E_reg >>= 1;
    }
}

/* Shift Right Arithmetic Double (SAD) */
static void shift_sad(uint16 *E_reg, uint16 *A_reg, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        C = *A_reg & 1;
        uint16 sign = *E_reg & 0x8000;
        *A_reg = (*A_reg >> 1) | ((*E_reg & 1) << 15);
        *E_reg = (*E_reg >> 1);
        if (sign) *E_reg |= 0x8000;
    }
}

/* Shift Left Circular Double (SLCD) */
static void shift_lcd(uint16 *E_reg, uint16 *A_reg, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        uint16 msb = (*E_reg & 0x8000) ? 1 : 0;
        C = msb;
        *E_reg = (*E_reg << 1) | ((*A_reg & 0x8000) ? 1 : 0);
        *A_reg = (*A_reg << 1) | msb;
    }
}

/* Shift Right Circular Double (SRCD) */
static void shift_rcd(uint16 *E_reg, uint16 *A_reg, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        uint16 lsb = *A_reg & 1;
        C = lsb;
        *A_reg = (*A_reg >> 1) | ((*E_reg & 1) << 15);
        *E_reg = (*E_reg >> 1) | (lsb << 15);
    }
}

/* Normalize (NLZ) - shift left until bit 0 != bit 1 or max steps */
static int normalize(uint16 *E_reg, uint16 *A_reg, uint16 *X_reg, int max_steps)
{
    int steps = 0;
    uint32 double_word = ((uint32)*E_reg << 16) | *A_reg;
    
    while (steps < max_steps && steps < 31) {
        /* Check if bits 0 and 1 are different (normalized) */
        if (((double_word >> 31) & 1) != ((double_word >> 30) & 1))
            break;
        double_word <<= 1;
        steps++;
    }
    
    *E_reg = (double_word >> 16) & 0xFFFF;
    *A_reg = double_word & 0xFFFF;
    *X_reg -= steps;
    
    /* Set condition codes per manual */
    if (steps == 0) {
        C = 1;  /* Stop on zero count */
        OV = 0;
    } else if (steps == max_steps) {
        C = 0;
        OV = 1;  /* Stop on max */
    } else {
        C = 0;
        OV = 0;  /* Normalized */
    }
    return steps;
}

/* Compute parity (PTY) - count set bits shifted out */
static uint16 compute_parity(uint16 *A_reg, int count)
{
    uint16 result = *A_reg;
    uint16 parity_count = 0;
    int i;
    
    for (i = 0; i < count; i++) {
        if (result & 0x8000) parity_count++;
        result = (result << 1) | ((result & 0x8000) ? 1 : 0);
    }
    
    *A_reg = result;
    C = (result & 0x8000) ? 1 : 0;
    OV = 0;
    return parity_count;
}

/* ========== Floating Point (per manual section VII-9) ========== */
/* Mitra-15 uses base-16 exponent with characteristic +64 */

static double mitra_to_double(uint16 A, uint16 E)
{
    uint32 raw = ((uint32)A << 16) | E;
    int sign = (raw >> 31) & 1;
    int exp = (raw >> 24) & 0x7F;
    uint32 mant = raw & 0xFFFFFF;
    double m = mant / (double)(1 << 24);
    /* Exponent is base-16, characteristic is exp-64 */
    double val = m * pow(16.0, exp - 64);
    return sign ? -val : val;
}

static void double_to_mitra(double v, uint16 *A, uint16 *E)
{
    int sign = (v < 0);
    if (sign) v = -v;
    
    int exp;
    double m = frexp(v, &exp);
    /* Convert from base-2 exponent to base-16 */
    int exp16 = (exp - 1) / 4;
    double m16 = m * pow(2.0, 4 - (exp - 1 - exp16 * 4));
    
    uint32 mant = (uint32)(m16 * (1 << 24));
    uint32 raw = (sign << 31) | ((exp16 + 64) << 24) | (mant & 0xFFFFFF);
    *A = (raw >> 16) & 0xFFFF;
    *E = raw & 0xFFFF;
}

/* ========== Interrupt Handling ========== */

uint32 api_findreq(void)
{
    uint32 i, t;
    t = (int_req & ~1) & api_mask[api_lvlhi];
    for (i = 31; t && (i > 0); i--) {
        if ((t >> i) & 1) return i;
    }
    return 0;
}

/* ========== Trap Handling ========== */

t_stat mitra_trap(int trap, uint16 pc, uint16 *trappc)
{
    /* Store trap information in memory word 2 (per manual section II-8.3) */
    uint16 trap_word = read_word(2);
    trap_word |= (1 << trap);
    write_word(2, trap_word);
    
    /* Save registers in memory bytes 4-9 */
    write_word(4, L - GPRIME);
    write_word(6, P - GPRIME);
    write_word(8, ((C ? 1 : 0) | (OV ? 2 : 0) | (MS ? 4 : 0)));
    
    /* Call supervisor section 0 */
    uint16 svc_addr = read_word(12);  /* PRTS pointer */
    L = read_word(svc_addr + 2) + G;
    P = read_word(svc_addr) + G;
    MS = 1;
    PR = 1;
    
    *trappc = pc;
    return SCPE_OK;
}

/* ========== Interrupt Dispatch ========== */

void io_interrupt_dispatch(void)
{
    /* This is called when an interrupt occurs.
     * The actual interrupt handling is done in sim_instr().
     * This function just ensures the interrupt is processed. */
    /* Force interrupt processing in the main loop */
    /* The actual work is done in sim_instr() which checks int_req */
}

/* ========== SIMH Terminal Functions (wrappers) ========== */

int sim_tt_getc(void)
{
    /* Get character from SIMH console */
    return sim_poll_kbd();
}

void sim_tt_putc(int ch)
{
    /* Put character to SIMH console */
    sim_tt_putc(ch);
}

/* Note: sim_tt_inchar, sim_tt_open, sim_tt_close are provided by SIMH */

void set_dyn_map(void)
{
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

/* ========== Main Instruction Execution ========== */

/* Shift table for SHR instruction (manual page 7-38) */
typedef enum {
    SHIFT_SLLS = 0,   /* Shift Left Logical Single */
    SHIFT_SRCS = 1,   /* Shift Right Circular Single */
    SHIFT_SAD  = 2,   /* Shift Arithmetic Right Double */
    SHIFT_SLCD = 3,   /* Shift Left Circular Double */
    SHIFT_SLCS = 4,   /* Shift Left Circular Single */
    SHIFT_SAS  = 5,   /* Shift Arithmetic Right Single */
    SHIFT_SRLS = 6,   /* Shift Right Logical Single */
    SHIFT_SRCD = 7    /* Shift Right Circular Double */
} shift_type_t;

/* SRG operation codes (manual page 7-55) */
typedef enum {
    SRG_RTS = 0x00,   /* Return Section */
    SRG_XAE = 0x02,   /* Exchange A and E */
    SRG_XAX = 0x04,   /* Exchange A and X */
    SRG_XEX = 0x06,   /* Exchange E and X */
    SRG_XAA = 0x08,   /* Exchange bytes of A */
    SRG_CCE = 0x0A,   /* Complement E */
    SRG_RSV = 0x0C,   /* Return Supervisor */
    SRG_ACE = 0x0E,   /* Add Carry to E */
    SRG_CCA = 0x10,   /* Complement A */
    SRG_AEE = 0x12,   /* A XOR E */
    SRG_CNX = 0x14,   /* Copy Negative X */
    SRG_AIE = 0x16,   /* A OR E */
    SRG_AAE = 0x18,   /* A AND E */
    SRG_LNE = 0x1A,   /* Load -1 into E */
    SRG_CNA = 0x1C,   /* Copy Negative A */
    SRG_CHX = 0x1E    /* Compute Half X */
} srg_op_t;

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

Data width:
The Mitra-15 is fundamentally a 16-bit word-addressable machine. Data size is chosen implicitly by the opcode (bits 4–7), not by any extra control bits in the instruction. Examples:
	Byte: LBL (Load Byte Left into A)
	Word: LDA, STA, ADD, SUB, AND, IOR, etc.
	Double word: DLD (Double Load): loads two words → E and A
*
*        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*      | 0 0  0 | 0| x x  x  x |     displacement      |
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*	()
    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
*
*        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*      | 0 0  0 | 1| x x  x  x |     displacement      |
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*	()
    "DLD", "STA", "STE", "STX", "SBL", "SBR", "DST", "ADM",
    "SPA", "STS", "FAD", "FSU", "FMU", "FDV", "TRS", "MVS",
*
*        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*      | 0 0  1 | 0| x x  x  x |     displacement      |
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
      (P addressing mode) 
    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
*
*        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*      | 0 0  1 | 1| x x  x  x |     displacement      |
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    
    "SHR", "SRG", "ICX", "DCX", "",    "ICL", "DCL", "CSV",
    "CLS", "LDR", "STR", "LDP", "SHC", "TES", "",    "",
*
*        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*      | 0 1  0 | 0| x x  x  x |     displacement      |
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    (DG addressing mode) 
    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
*
*        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*      | 0 1  0 | 1| x x  x  x |     displacement      |
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    
    "DLD", "STA", "STE", "STX", "SBL", "SBR", "DST", "ADM",
    "SPA", "STS", "FAD", "FSU", "FMU", "FDV", "TRS", "MVS",
*
*        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*      | 0 1  1 | 0| x x  x  x |     displacement      |
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    (IL addressing mode) 
    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
*
*        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*      | 0 1  1 | 1| x x  x  x |     displacement      |
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    
    "DLD", "STA", "STE", "STX", "SBL", "SBR", "DST", "ADM",
    "SPA", "STS", "FAD", "FSU", "FMU", "FDV", "TRS", "MVS",
*
*        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*      |1  0  0 | 0| x x  x  x |     displacement      |
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    (IGX addressing mode)
    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
*
*        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*      |1  0  0 | 1| x x  x  x |     displacement      |
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    
    "DLD", "STA", "STE", "STX", "SBL", "SBR", "DST", "ADM",
    "SPA", "STS", "FAD", "FSU", "FMU", "FDV", "TRS", "MVS",
*
*        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*      |1  0  1 | 0| x x  x  x |     displacement      |
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    (ILX addressing mode)
    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
*
*        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*      |1  0  1 | 1| x x  x  x |     displacement      |
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    
    "DLD", "STA", "STE", "STX", "SBL", "SBR", "DST", "ADM",
    "SPA", "STS", "FAD", "FSU", "FMU", "FDV", "TRS", "MVS",
*
*        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*      |1  1  0 | 0| x x  x  x |     displacement      |
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    (Class 2 - RP addressing mode)
    "BCT", "BRX", "BOT", "BCF", "BAN", "BAZ", "BOF", "BRU",
    "BCT", "BRX", "BOT", "BCF", "BAN", "BAZ", "BOF", "BRU",
*
*        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*      |1  1  0 | 1| x x  x  x |     displacement      |
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    (Class 2 - RM/IL/IG addressing modes)
    "BCT", "BRX", "BOT", "BCF", "BAN", "BAZ", "BOF", "BRU",
    "BCT", "BRX", "BOT", "BCF", "BAN", "BAZ", "BOF", "BRU",
*
*        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*      |1  1  1 | 0| x x  x  x |     displacement      |
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    (Class 1 - PX addressing mode)
    "SHR", "SRG", "ICX", "DCX", "",    "ICL", "DCL", "CSV",
    "CLS", "LDR", "STR", "LDP", "SHC", "TES", "",    "",
*
*        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*      |1  1  1 | 1| x x  x  x |     displacement      |
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    (Class 1 and system - P mode)
    "SHR", "SRG", "ICX", "DCX", "SYS", "ICL", "DCL", "CSV",
    "CLS", "LDR", "STR", "LDP", "SHC", "TES", "",    ""
};
*/
t_stat one_inst(uint16 inst, uint16 pc, uint32 mode, uint16 *trappc)
{
    uint16 opcode = (inst >> I_OPCODE_SHIFT) & 0x1F;
    uint16 disp = inst & I_DISP_MASK;
    uint16 ea, data, data2, result;
    uint16 carry, overflow;
    int i, count;
    uint8 s_byte, d_byte;

    *trappc = pc;
    carry = 0;
    overflow = 0;

    /* Calculate effective address for most instructions */
    if (opcode != 0x37 && opcode != 0x38 && opcode != 0x39 && 
        opcode != 0x3A && opcode != 0x3B && opcode != 0x3D) {
        ea = ea_calculate(inst);
    } else {
        ea = disp;
    }

    switch (opcode) {
        /* ========== Class 0 Instructions (00-0F) ========== */
        case 0x00:  /* LDA - Load A */
            A = read_word(ea);
            set_condition_codes_load(A);
            break;
            
        case 0x01:  /* LDE - Load E */
            E = read_word(ea);
            set_condition_codes_load(E);
            break;
            
        case 0x02:  /* LDX - Load X */
            X = read_word(ea);
            set_condition_codes_load(X);
            break;
            
        case 0x03:  /* EOR - Exclusive OR */
            A ^= read_word(ea);
            set_condition_codes_load(A);
            break;
            
        case 0x04:  /* LEA - Load Effective Address */
            A = (ea - GPRIME) & 0x7FFF;
            set_condition_codes_load(A);
            break;
            
        case 0x05:  /* ADD - Add */
            carry = 0;
            result = add16(A, read_word(ea), &carry, &overflow);
            A = result;
            set_condition_codes_arithmetic(result, carry, overflow);
            break;
            
        case 0x06:  /* SUB - Subtract */
            carry = 0;
            result = sub16(A, read_word(ea), &carry, &overflow);
            A = result;
            set_condition_codes_arithmetic(result, carry, overflow);
            break;
            
        case 0x07:  /* IOR - Inclusive OR */
            A |= read_word(ea);
            set_condition_codes_load(A);
            break;
            
        case 0x08:  /* DIV - Divide (optional) */
            if (mode != 1) return MM_PRVINS;
            if (!(cpu_unit.flags & UNIT_MULDIV)) return MM_INVINS;
            data = read_word(ea);
            if (div32(E, A, data, &A, &E) != 0) {
                OV = 1;
            }
            set_condition_codes_load(A);
            break;
            
        case 0x09:  /* AND - Logical AND */
            A &= read_word(ea);
            set_condition_codes_load(A);
            break;
            
        case 0x0A:  /* CPS - Compare String (optional) */
            if (!(cpu_unit.flags & UNIT_EXTINS)) return MM_INVINS;
            for (i = 0; i < E; i++) {
                s_byte = read_byte(A + i);
                d_byte = read_byte(X + i);
                if (s_byte != d_byte) {
                    set_condition_codes_string(0, s_byte < d_byte);
                    break;
                }
            }
            if (i == E) set_condition_codes_string(1, 0);
            break;
            
        case 0x0B:  /* CMP - Compare */
            data = read_word(ea);
            sub16(A, data, &carry, &overflow);
            set_condition_codes_compare(A, data, 0);
            break;
            
        case 0x0C:  /* MUL - Multiply */
            if (!(cpu_unit.flags & UNIT_MULDIV)) return MM_INVINS;
            data = read_word(ea);
            mul32(A, data, &E, &A);
            set_condition_codes_load(E);
            break;
            
        case 0x0D:  /* LBL - Load Byte Left */
            data = read_word(ea);
            A = (A & 0x00FF) | (data & 0xFF00);
            set_condition_codes_load(A);
            break;
            
        case 0x0E:  /* LBR - Load Byte Right */
            data = read_word(ea);
            A = (data & 0x00FF) | ((data << 8) & 0xFF00);
            set_condition_codes_load(A);
            break;
            
        case 0x0F:  /* LBX - Load Byte Right into X */
            data = read_word(ea);
            X = (data & 0x00FF) | ((data << 8) & 0xFF00);
            set_condition_codes_load(X);
            break;
            
        /* ========== Store Instructions (10-1F) - Class 0' ========== */
        case 0x10:  /* DLD - Double Load */
            E = read_word(ea);
            A = read_word((ea + 2) & 0x7FFF);
            set_condition_codes_load(E);
            break;
            
        case 0x11:  /* STA - Store A */
            write_word(ea, A);
            break;
            
        case 0x12:  /* STE - Store E */
            write_word(ea, E);
            break;
            
        case 0x13:  /* STX - Store X */
            write_word(ea, X);
            break;
            
        case 0x14:  /* SBL - Store Byte Left */
            data = read_word(ea);
            write_word(ea, (data & 0x00FF) | (A & 0xFF00));
            break;
            
        case 0x15:  /* SBR - Store Byte Right */
            data = read_word(ea);
            write_word(ea, (data & 0xFF00) | ((A << 8) & 0x00FF));
            break;
            
        case 0x16:  /* DST - Double Store */
            write_word(ea, E);
            write_word((ea + 2) & 0x7FFF, A);
            break;
            
        case 0x17:  /* ADM - Add to Memory */
            data = read_word(ea);
            carry = 0;
            result = add16(data, A, &carry, &overflow);
            write_word(ea, result);
            A = result;
            set_condition_codes_arithmetic(result, carry, overflow);
            break;
            
        case 0x18:  /* SPA - Store Program Address */
            A = (pc + 4 + GPRIME) & 0x7FFF;
            write_word(ea, A);
            break;
            
        case 0x19:  /* STS - Store Selective */
            data = read_word(ea);
            result = (data & ~E) | (A & E);
            write_word(ea, result);
            set_condition_codes_load(result);
            break;
            
        case 0x1A:  /* FAD - Float Add (optional) */
            if (!(cpu_unit.flags & UNIT_FP)) return MM_INVINS;
            data = read_word(ea);
            data2 = read_word((ea + 2) & 0x7FFF);
            {
                double a = mitra_to_double(A, E);
                double b = mitra_to_double(data, data2);
                double r = a + b;
                double_to_mitra(r, &A, &E);
                set_condition_codes_load(A);
            }
            break;
            
        case 0x1B:  /* FSU - Float Subtract (optional) */
            if (!(cpu_unit.flags & UNIT_FP)) return MM_INVINS;
            data = read_word(ea);
            data2 = read_word((ea + 2) & 0x7FFF);
            {
                double a = mitra_to_double(A, E);
                double b = mitra_to_double(data, data2);
                double r = a - b;
                double_to_mitra(r, &A, &E);
                set_condition_codes_load(A);
            }
            break;
            
        case 0x1C:  /* FMU - Float Multiply (optional) */
            if (!(cpu_unit.flags & UNIT_FP)) return MM_INVINS;
            data = read_word(ea);
            data2 = read_word((ea + 2) & 0x7FFF);
            {
                double a = mitra_to_double(A, E);
                double b = mitra_to_double(data, data2);
                double r = a * b;
                double_to_mitra(r, &A, &E);
                set_condition_codes_load(A);
            }
            break;
            
        case 0x1D:  /* FDV - Float Divide (optional) */
            if (!(cpu_unit.flags & UNIT_FP)) return MM_INVINS;
            data = read_word(ea);
            data2 = read_word((ea + 2) & 0x7FFF);
            {
                double a = mitra_to_double(A, E);
                double b = mitra_to_double(data, data2);
                if (b == 0.0) return mitra_trap(TRAP_INVINS, pc, trappc);
                double r = a / b;
                double_to_mitra(r, &A, &E);
                set_condition_codes_load(A);
            }
            break;
            
        case 0x1E:  /* TRS - Translate String (optional) */
            if (!(cpu_unit.flags & UNIT_EXTINS)) return MM_INVINS;
            {
                uint16 table = ea;
                for (i = 0; i < E; i++) {
                    uint8 b = read_byte(A + i);
                    uint8 t = read_byte(table + (b & 0xFF));
                    write_byte(X + i, t);
                }
                E = 0xFFFF;
            }
            break;
            
        case 0x1F:  /* MVS - Move String (optional) */
            if (!(cpu_unit.flags & UNIT_EXTINS)) return MM_INVINS;
            {
                for (i = 0; i < E; i++) {
                    uint8 b = read_byte(A + i);
                    write_byte(X + i, b);
                }
                E = 0xFFFF;
            }
            break;
            
        /* ========== P Mode Instructions (20-2F) ========== */
        case 0x20: case 0x21: case 0x22: case 0x23: case 0x24:
        case 0x25: case 0x26: case 0x27: case 0x28: case 0x29:
        case 0x2A: case 0x2B: case 0x2C: case 0x2D: case 0x2E: case 0x2F:
            /* Same as 00-0F but with P mode addressing (already handled by ea) */
            switch (opcode & 0x0F) {
                case 0x00: A = read_word(ea); set_condition_codes_load(A); break;
                case 0x01: E = read_word(ea); set_condition_codes_load(E); break;
                case 0x02: X = read_word(ea); set_condition_codes_load(X); break;
                case 0x03: A ^= read_word(ea); set_condition_codes_load(A); break;
                case 0x04: A = (ea - GPRIME) & 0x7FFF; set_condition_codes_load(A); break;
                case 0x05:
                    carry = 0;
                    result = add16(A, read_word(ea), &carry, &overflow);
                    A = result;
                    set_condition_codes_arithmetic(result, carry, overflow);
                    break;
                case 0x06:
                    carry = 0;
                    result = sub16(A, read_word(ea), &carry, &overflow);
                    A = result;
                    set_condition_codes_arithmetic(result, carry, overflow);
                    break;
                case 0x07: A |= read_word(ea); set_condition_codes_load(A); break;
                case 0x08:
                    if (mode != 1) return MM_PRVINS;
                    data = read_word(ea);
                    if (div32(E, A, data, &A, &E) != 0) OV = 1;
                    set_condition_codes_load(A);
                    break;
                case 0x09: A &= read_word(ea); set_condition_codes_load(A); break;
                case 0x0A:
                    for (i = 0; i < E; i++) {
                        s_byte = read_byte(A + i);
                        d_byte = read_byte(X + i);
                        if (s_byte != d_byte) {
                            set_condition_codes_string(0, s_byte < d_byte);
                            break;
                        }
                    }
                    if (i == E) set_condition_codes_string(1, 0);
                    break;
                case 0x0B:
                    data = read_word(ea);
                    set_condition_codes_compare(A, data, 0);
                    break;
                case 0x0C:
                    data = read_word(ea);
                    mul32(A, data, &E, &A);
                    set_condition_codes_load(E);
                    break;
                case 0x0D:
                    data = read_word(ea);
                    A = (A & 0x00FF) | (data & 0xFF00);
                    set_condition_codes_load(A);
                    break;
                case 0x0E:
                    data = read_word(ea);
                    A = (data & 0x00FF) | ((data << 8) & 0xFF00);
                    set_condition_codes_load(A);
                    break;
                case 0x0F:
                    data = read_word(ea);
                    X = (data & 0x00FF) | ((data << 8) & 0xFF00);
                    set_condition_codes_load(X);
                    break;
            }
            break;
            
        /* ========== Shift and Index Instructions (30-3F) ========== */
        case 0x30:  /* SHR - Shift Register */
            count = (disp >> 3) & 0x1F;
            {
                shift_type_t type = (disp >> 0) & 0x07;
                switch (type) {
                    case SHIFT_SLLS:  /* Shift Left Logical Single */
                        A = shift_lls(A, count);
                        break;
                    case SHIFT_SRCS:  /* Shift Right Circular Single */
                        A = shift_srcs(A, count);
                        break;
                    case SHIFT_SAD:   /* Shift Arithmetic Right Double */
                        shift_sad(&E, &A, count);
                        break;
                    case SHIFT_SLCD:  /* Shift Left Circular Double */
                        shift_lcd(&E, &A, count);
                        break;
                    case SHIFT_SLCS:  /* Shift Left Circular Single */
                        A = shift_slcs(A, count);
                        break;
                    case SHIFT_SAS:   /* Shift Arithmetic Right Single */
                        A = shift_sas(A, count);
                        break;
                    case SHIFT_SRLS:  /* Shift Right Logical Single */
                        A = shift_rls(A, count);
                        break;
                    case SHIFT_SRCD:  /* Shift Right Circular Double */
                        shift_rcd(&E, &A, count);
                        break;
                }
                set_condition_codes_load(A);
            }
            break;
            
        case 0x31:  /* SRG - Set Register (inter-register operations) */
            {
                srg_op_t srg_op = (disp >> 1) & 0x1F;
                switch (srg_op) {
                    case SRG_RTS:  /* RTS - Return Section */
                        P = read_word(L + GPRIME);
                        L = read_word(L + 2 + GPRIME);
                        break;
                    case SRG_XAE:  /* XAE - Exchange A and E */
                        data = A; A = E; E = data;
                        break;
                    case SRG_XAX:  /* XAX - Exchange A and X */
                        data = A; A = X; X = data;
                        break;
                    case SRG_XEX:  /* XEX - Exchange E and X */
                        data = E; E = X; X = data;
                        break;
                    case SRG_XAA:  /* XAA - Exchange bytes of A */
                        A = ((A & 0xFF) << 8) | ((A >> 8) & 0xFF);
                        break;
                    case SRG_CCE:  /* CCE - Complement E */
                        E = ~E & 0xFFFF;
                        break;
                    case SRG_RSV:  /* RSV - Return Supervisor (privileged) */
                        if (mode != 1) return MM_PRVINS;
                        P = read_word(G);
                        L = read_word(G + 2);
                        {
                            uint16 saved_flags = read_word(G + 4);
                            C = (saved_flags >> 0) & 1;
                            OV = (saved_flags >> 1) & 1;
                            MS = 0;
                        }
                        break;
                    case SRG_ACE:  /* ACE - Add Carry to E */
                        E = (E + C) & 0xFFFF;
                        break;
                    case SRG_CCA:  /* CCA - Complement A */
                        A = ~A & 0xFFFF;
                        set_condition_codes_load(A);
                        break;
                    case SRG_AEE:  /* AEE - A XOR E */
                        A ^= E;
                        set_condition_codes_load(A);
                        break;
                    case SRG_CNX:  /* CNX - Copy Negative X */
                        X = (~X + 1) & 0xFFFF;
                        break;
                    case SRG_AIE:  /* AIE - A OR E */
                        A |= E;
                        set_condition_codes_load(A);
                        break;
                    case SRG_AAE:  /* AAE - A AND E */
                        A &= E;
                        set_condition_codes_load(A);
                        break;
                    case SRG_LNE:  /* LNE - Load -1 into E */
                        E = 0xFFFF;
                        break;
                    case SRG_CNA:  /* CNA - Copy Negative A */
                        A = (~A + 1) & 0xFFFF;
                        set_condition_codes_load(A);
                        break;
                    case SRG_CHX:  /* CHX - Compute Half X */
                        X = (X >> 1) | (X & 0x8000);
                        break;
                    default:
                        /* Reserved - no operation */
                        break;
                }
            }
            break;
            
        case 0x32:  /* ICX - Increment X (Class 1, P mode) */
            X = (X + disp) & 0x7FFF;
            set_condition_codes_load(X);
            break;
            
        case 0x33:  /* DCX - Decrement X */
            X = (X - disp) & 0x7FFF;
            set_condition_codes_load(X);
            break;
            
        case 0x34:  /* Reserved */
            break;
            
        case 0x35:  /* ICL - Increment L */
            L = (L + disp) & 0x7FFF;
            break;
            
        case 0x36:  /* DCL - Decrement L */
            L = (L - disp) & 0x7FFF;
            break;
            
        case 0x37:  /* CSV - Call Supervisor */
/*
 * In Slave mode (modern user mode) the I/O programs use CSV instruction which is a call to the Master mode (modern kernel mode)
*/
            {
                uint16 section = ea;
                /* Save context in TWB (first 3 words of CDS) */
                write_word(G, P - GPRIME);
                write_word(G + 2, L - GPRIME);
                write_word(G + 4, (C ? 1 : 0) | (OV ? 2 : 0) | (MS ? 4 : 0));
                /* Load new context from PRTS */
                L = read_word(12 - 4 * section) + G;
                P = read_word(12 - 4 * section + 2) + G;
                MS = 1;  /* Enter master mode */
                PR = 1;  /* Override protection */
            }
            break;
            
        case 0x38:  /* CLS - Call Section */
            if (mode == 0) return MM_PRVINS;
            {
                uint16 section = ea;
                /* Save return address in called section's LDS */
                write_word(L, P - GPRIME);
                write_word(L + 2, L - GPRIME);
                /* Load new section from PRT */
                L = read_word(G - 4 * section + 2) + G;
                P = read_word(G - 4 * section) + G;
            }
            break;
            
        case 0x39:  /* LDR - Load Register (Class 1) */
            {
                uint16 reg_num = read_word(ea) & 0x3F;
                /* Register access would need full register block implementation */
                /* For now, only registers 0-5 are implemented */
                if (reg_num == 0) A = read_word(ea);
                else if (reg_num == 1) E = read_word(ea);
                else if (reg_num == 2) X = read_word(ea);
                else if (reg_num == 3) L = read_word(ea);
                else if (reg_num == 4) G = read_word(ea);
                else if (reg_num == 5) P = read_word(ea);
                set_condition_codes_load(A);
            }
            break;
            
        case 0x3A:  /* STR - Store Register (privileged) */
            if (mode != 1) return MM_PRVINS;
            {
                uint16 reg_num = read_word(ea) & 0x3F;
                if (reg_num == 0) write_word(ea, A);
                else if (reg_num == 1) write_word(ea, E);
                else if (reg_num == 2) write_word(ea, X);
                else if (reg_num == 3) write_word(ea, L);
                else if (reg_num == 4) write_word(ea, G);
                else if (reg_num == 5) write_word(ea, P);
            }
            break;
            
        case 0x3B:  /* LDP - Load Protection (privileged) */
            if (mode != 1) return MM_PRVINS;
            PR = read_word(ea) & 1;
            break;
            
        case 0x3C:  /* SHC - Shift Special */
            count = (disp >> 3) & 0x1F;
            {
                uint8 shc_type = (disp >> 0) & 0x07;
                switch (shc_type) {
                    case 0:  /* SLLD - Shift Left Logical Double */
                        shift_lld(&E, &A, count);
                        break;
                    case 1:  /* DITR - Deactivate high-speed interrupt (privileged) */
                        if (mode != 1) return MM_PRVINS;
                        /* High-speed interrupt deactivation */
                        int_req &= ~(1u << int_lvl);
                        int_lvl = 0;
                        break;
                    case 2:  /* PTY - Compute parity */
                        E = compute_parity(&A, count);
                        break;
                    case 3:  /* Reserved / DITR variant */
                        if (mode != 1) return MM_PRVINS;
                        int_req &= ~(1u << int_lvl);
                        int_lvl = 0;
                        break;
                    case 4:  /* SRLD - Shift Right Logical Double */
                        shift_rld(&E, &A, count);
                        break;
                    case 5:  /* Reserved */
                        break;
                    case 6:  /* NLZ - Normalize */
                        normalize(&E, &A, &X, count);
                        break;
                    case 7:  /* Reserved */
                        break;
                }
                set_condition_codes_load(A);
            }
            break;
            
        case 0x3D:  /* TES - Test and Set (privileged) */
            if (mode != 1) return MM_PRVINS;
            A = read_word(ea);
            write_word(ea, 0);
            set_condition_codes_load(A);
            break;
            
        case 0x3E: case 0x3F:  /* Reserved */
            break;
            
        /* ========== DG Mode Instructions (40-4F) ========== */
        case 0x40: case 0x41: case 0x42: case 0x43: case 0x44:
        case 0x45: case 0x46: case 0x47: case 0x48: case 0x49:
        case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
            /* Same as 00-0F with DG addressing (already handled) */
            switch (opcode & 0x0F) {
                case 0x00: A = read_word(ea); set_condition_codes_load(A); break;
                case 0x01: E = read_word(ea); set_condition_codes_load(E); break;
                case 0x02: X = read_word(ea); set_condition_codes_load(X); break;
                case 0x03: A ^= read_word(ea); set_condition_codes_load(A); break;
                case 0x04: A = (ea - GPRIME) & 0x7FFF; set_condition_codes_load(A); break;
                case 0x05:
                    carry = 0;
                    result = add16(A, read_word(ea), &carry, &overflow);
                    A = result;
                    set_condition_codes_arithmetic(result, carry, overflow);
                    break;
                case 0x06:
                    carry = 0;
                    result = sub16(A, read_word(ea), &carry, &overflow);
                    A = result;
                    set_condition_codes_arithmetic(result, carry, overflow);
                    break;
                case 0x07: A |= read_word(ea); set_condition_codes_load(A); break;
                case 0x08:
                    if (mode != 1) return MM_PRVINS;
                    data = read_word(ea);
                    if (div32(E, A, data, &A, &E) != 0) OV = 1;
                    set_condition_codes_load(A);
                    break;
                case 0x09: A &= read_word(ea); set_condition_codes_load(A); break;
                case 0x0A:
                    for (i = 0; i < E; i++) {
                        s_byte = read_byte(A + i);
                        d_byte = read_byte(X + i);
                        if (s_byte != d_byte) {
                            set_condition_codes_string(0, s_byte < d_byte);
                            break;
                        }
                    }
                    if (i == E) set_condition_codes_string(1, 0);
                    break;
                case 0x0B:
                    data = read_word(ea);
                    set_condition_codes_compare(A, data, 0);
                    break;
                case 0x0C:
                    data = read_word(ea);
                    mul32(A, data, &E, &A);
                    set_condition_codes_load(E);
                    break;
                case 0x0D:
                    data = read_word(ea);
                    A = (A & 0x00FF) | (data & 0xFF00);
                    set_condition_codes_load(A);
                    break;
                case 0x0E:
                    data = read_word(ea);
                    A = (data & 0x00FF) | ((data << 8) & 0xFF00);
                    set_condition_codes_load(A);
                    break;
                case 0x0F:
                    data = read_word(ea);
                    X = (data & 0x00FF) | ((data << 8) & 0xFF00);
                    set_condition_codes_load(X);
                    break;
            }
            break;
            
        /* ========== DG Mode Store Instructions (50-5F) ========== */
        case 0x50:  /* DLD (DG mode) */
            E = read_word(ea);
            A = read_word((ea + 2) & 0x7FFF);
            set_condition_codes_load(E);
            break;
        case 0x51:  /* STA (DG mode) */
            write_word(ea, A);
            break;
        case 0x52:  /* STE (DG mode) */
            write_word(ea, E);
            break;
        case 0x53:  /* STX (DG mode) */
            write_word(ea, X);
            break;
        case 0x54:  /* SBL (DG mode) */
            data = read_word(ea);
            write_word(ea, (data & 0x00FF) | (A & 0xFF00));
            break;
        case 0x55:  /* SBR (DG mode) */
            data = read_word(ea);
            write_word(ea, (data & 0xFF00) | ((A << 8) & 0x00FF));
            break;
        case 0x56:  /* DST (DG mode) */
            write_word(ea, E);
            write_word((ea + 2) & 0x7FFF, A);
            break;
        case 0x57:  /* ADM (DG mode) */
            data = read_word(ea);
            carry = 0;
            result = add16(data, A, &carry, &overflow);
            write_word(ea, result);
            A = result;
            set_condition_codes_arithmetic(result, carry, overflow);
            break;
        case 0x58:  /* SPA (DG mode) */
            A = (pc + 4 + GPRIME) & 0x7FFF;
            write_word(ea, A);
            break;
        case 0x59:  /* STS (DG mode) */
            data = read_word(ea);
            result = (data & ~E) | (A & E);
            write_word(ea, result);
            set_condition_codes_load(result);
            break;
        case 0x5A:  /* FAD (DG mode) */
            if (!(cpu_unit.flags & UNIT_FP)) return MM_INVINS;
            data = read_word(ea);
            data2 = read_word((ea + 2) & 0x7FFF);
            {
                double a = mitra_to_double(A, E);
                double b = mitra_to_double(data, data2);
                double r = a + b;
                double_to_mitra(r, &A, &E);
                set_condition_codes_load(A);
            }
            break;
        case 0x5B:  /* FSU (DG mode) */
            if (!(cpu_unit.flags & UNIT_FP)) return MM_INVINS;
            data = read_word(ea);
            data2 = read_word((ea + 2) & 0x7FFF);
            {
                double a = mitra_to_double(A, E);
                double b = mitra_to_double(data, data2);
                double r = a - b;
                double_to_mitra(r, &A, &E);
                set_condition_codes_load(A);
            }
            break;
        case 0x5C:  /* FMU (DG mode) */
            if (!(cpu_unit.flags & UNIT_FP)) return MM_INVINS;
            data = read_word(ea);
            data2 = read_word((ea + 2) & 0x7FFF);
            {
                double a = mitra_to_double(A, E);
                double b = mitra_to_double(data, data2);
                double r = a * b;
                double_to_mitra(r, &A, &E);
                set_condition_codes_load(A);
            }
            break;
        case 0x5D:  /* FDV (DG mode) */
            if (!(cpu_unit.flags & UNIT_FP)) return MM_INVINS;
            data = read_word(ea);
            data2 = read_word((ea + 2) & 0x7FFF);
            {
                double a = mitra_to_double(A, E);
                double b = mitra_to_double(data, data2);
                if (b == 0.0) return mitra_trap(TRAP_INVINS, pc, trappc);
                double r = a / b;
                double_to_mitra(r, &A, &E);
                set_condition_codes_load(A);
            }
            break;
        case 0x5E:  /* TRS (DG mode) */
            if (!(cpu_unit.flags & UNIT_EXTINS)) return MM_INVINS;
            {
                uint16 table = ea;
                for (i = 0; i < E; i++) {
                    uint8 b = read_byte(A + i);
                    uint8 t = read_byte(table + (b & 0xFF));
                    write_byte(X + i, t);
                }
                E = 0xFFFF;
            }
            break;
        case 0x5F:  /* MVS (DG mode) */
            if (!(cpu_unit.flags & UNIT_EXTINS)) return MM_INVINS;
            {
                for (i = 0; i < E; i++) {
                    uint8 b = read_byte(A + i);
                    write_byte(X + i, b);
                }
                E = 0xFFFF;
            }
            break;
            
        /* ========== IL Mode Instructions (60-6F) ========== */
        case 0x60: case 0x61: case 0x62: case 0x63: case 0x64:
        case 0x65: case 0x66: case 0x67: case 0x68: case 0x69:
        case 0x6A: case 0x6B: case 0x6C: case 0x6D: case 0x6E: case 0x6F:
            /* Same as 00-0F with IL addressing */
            switch (opcode & 0x0F) {
                case 0x00: A = read_word(ea); set_condition_codes_load(A); break;
                case 0x01: E = read_word(ea); set_condition_codes_load(E); break;
                case 0x02: X = read_word(ea); set_condition_codes_load(X); break;
                case 0x03: A ^= read_word(ea); set_condition_codes_load(A); break;
                case 0x04: A = (ea - GPRIME) & 0x7FFF; set_condition_codes_load(A); break;
                case 0x05:
                    carry = 0;
                    result = add16(A, read_word(ea), &carry, &overflow);
                    A = result;
                    set_condition_codes_arithmetic(result, carry, overflow);
                    break;
                case 0x06:
                    carry = 0;
                    result = sub16(A, read_word(ea), &carry, &overflow);
                    A = result;
                    set_condition_codes_arithmetic(result, carry, overflow);
                    break;
                case 0x07: A |= read_word(ea); set_condition_codes_load(A); break;
                case 0x08:
                    if (mode != 1) return MM_PRVINS;
                    data = read_word(ea);
                    if (div32(E, A, data, &A, &E) != 0) OV = 1;
                    set_condition_codes_load(A);
                    break;
                case 0x09: A &= read_word(ea); set_condition_codes_load(A); break;
                case 0x0A:
                    for (i = 0; i < E; i++) {
                        s_byte = read_byte(A + i);
                        d_byte = read_byte(X + i);
                        if (s_byte != d_byte) {
                            set_condition_codes_string(0, s_byte < d_byte);
                            break;
                        }
                    }
                    if (i == E) set_condition_codes_string(1, 0);
                    break;
                case 0x0B:
                    data = read_word(ea);
                    set_condition_codes_compare(A, data, 0);
                    break;
                case 0x0C:
                    data = read_word(ea);
                    mul32(A, data, &E, &A);
                    set_condition_codes_load(E);
                    break;
                case 0x0D:
                    data = read_word(ea);
                    A = (A & 0x00FF) | (data & 0xFF00);
                    set_condition_codes_load(A);
                    break;
                case 0x0E:
                    data = read_word(ea);
                    A = (data & 0x00FF) | ((data << 8) & 0xFF00);
                    set_condition_codes_load(A);
                    break;
                case 0x0F:
                    data = read_word(ea);
                    X = (data & 0x00FF) | ((data << 8) & 0xFF00);
                    set_condition_codes_load(X);
                    break;
            }
            break;
            
        /* ========== IL Mode Store Instructions (70-7F) ========== */
        case 0x70:  /* DLD (IL mode) */
            E = read_word(ea);
            A = read_word((ea + 2) & 0x7FFF);
            set_condition_codes_load(E);
            break;
        case 0x71:  /* STA (IL mode) */
            write_word(ea, A);
            break;
        case 0x72:  /* STE (IL mode) */
            write_word(ea, E);
            break;
        case 0x73:  /* STX (IL mode) */
            write_word(ea, X);
            break;
        case 0x74:  /* SBL (IL mode) */
            data = read_word(ea);
            write_word(ea, (data & 0x00FF) | (A & 0xFF00));
            break;
        case 0x75:  /* SBR (IL mode) */
            data = read_word(ea);
            write_word(ea, (data & 0xFF00) | ((A << 8) & 0x00FF));
            break;
        case 0x76:  /* DST (IL mode) */
            write_word(ea, E);
            write_word((ea + 2) & 0x7FFF, A);
            break;
        case 0x77:  /* ADM (IL mode) */
            data = read_word(ea);
            carry = 0;
            result = add16(data, A, &carry, &overflow);
            write_word(ea, result);
            A = result;
            set_condition_codes_arithmetic(result, carry, overflow);
            break;
        case 0x78:  /* SPA (IL mode) */
            A = (pc + 4 + GPRIME) & 0x7FFF;
            write_word(ea, A);
            break;
        case 0x79:  /* STS (IL mode) */
            data = read_word(ea);
            result = (data & ~E) | (A & E);
            write_word(ea, result);
            set_condition_codes_load(result);
            break;
        case 0x7A:  /* FAD (IL mode) */
            if (!(cpu_unit.flags & UNIT_FP)) return MM_INVINS;
            data = read_word(ea);
            data2 = read_word((ea + 2) & 0x7FFF);
            {
                double a = mitra_to_double(A, E);
                double b = mitra_to_double(data, data2);
                double r = a + b;
                double_to_mitra(r, &A, &E);
                set_condition_codes_load(A);
            }
            break;
        case 0x7B:  /* FSU (IL mode) */
            if (!(cpu_unit.flags & UNIT_FP)) return MM_INVINS;
            data = read_word(ea);
            data2 = read_word((ea + 2) & 0x7FFF);
            {
                double a = mitra_to_double(A, E);
                double b = mitra_to_double(data, data2);
                double r = a - b;
                double_to_mitra(r, &A, &E);
                set_condition_codes_load(A);
            }
            break;
        case 0x7C:  /* FMU (IL mode) */
            if (!(cpu_unit.flags & UNIT_FP)) return MM_INVINS;
            data = read_word(ea);
            data2 = read_word((ea + 2) & 0x7FFF);
            {
                double a = mitra_to_double(A, E);
                double b = mitra_to_double(data, data2);
                double r = a * b;
                double_to_mitra(r, &A, &E);
                set_condition_codes_load(A);
            }
            break;
        case 0x7D:  /* FDV (IL mode) */
            if (!(cpu_unit.flags & UNIT_FP)) return MM_INVINS;
            data = read_word(ea);
            data2 = read_word((ea + 2) & 0x7FFF);
            {
                double a = mitra_to_double(A, E);
                double b = mitra_to_double(data, data2);
                if (b == 0.0) return mitra_trap(TRAP_INVINS, pc, trappc);
                double r = a / b;
                double_to_mitra(r, &A, &E);
                set_condition_codes_load(A);
            }
            break;
        case 0x7E:  /* TRS (IL mode) */
            if (!(cpu_unit.flags & UNIT_EXTINS)) return MM_INVINS;
            {
                uint16 table = ea;
                for (i = 0; i < E; i++) {
                    uint8 b = read_byte(A + i);
                    uint8 t = read_byte(table + (b & 0xFF));
                    write_byte(X + i, t);
                }
                E = 0xFFFF;
            }
            break;
        case 0x7F:  /* MVS (IL mode) */
            if (!(cpu_unit.flags & UNIT_EXTINS)) return MM_INVINS;
            {
                for (i = 0; i < E; i++) {
                    uint8 b = read_byte(A + i);
                    write_byte(X + i, b);
                }
                E = 0xFFFF;
            }
            break;
            
        /* ========== IGX Mode Instructions (80-8F, A0-AF, C0-CF, E0-EF handled above) ========== */
        /* These follow the same pattern - addressing already handled by ea_class0 with AM_IGX */
        /* For brevity, we'll use the same handlers as 00-0F */
        
        /* ========== Branch Instructions (Class 2) ========== */
        case 0xC0:  /* BCT - Branch on Carry True (RP mode) */
            if (C) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xC1:  /* BRX - Branch Indexed (RP mode) */
            P = (ea + X) & 0x7FFF;
            break;
        case 0xC2:  /* BOT - Branch on Overflow True (RP mode) */
            if (OV) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xC3:  /* BCF - Branch on Carry False (RP mode) */
            if (!C) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xC4:  /* BAN - Branch on A Negative (RP mode) */
            if (A & 0x8000) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xC5:  /* BAZ - Branch on A Zero (RP mode) */
            if (A == 0) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xC6:  /* BOF - Branch on Overflow False (RP mode) */
            if (!OV) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xC7:  /* BRU - Branch Unconditional (RP mode) */
            P = ea;
            break;
            
        /* Same for RM mode */
        case 0xC8:  /* BCT (RM mode) */
            if (C) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xC9:  /* BRX (RM mode) */
            P = (ea + X) & 0x7FFF;
            break;
        case 0xCA:  /* BOT (RM mode) */
            if (OV) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xCB:  /* BCF (RM mode) */
            if (!C) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xCC:  /* BAN (RM mode) */
            if (A & 0x8000) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xCD:  /* BAZ (RM mode) */
            if (A == 0) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xCE:  /* BOF (RM mode) */
            if (!OV) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xCF:  /* BRU (RM mode) */
            P = ea;
            break;
            
        /* IL mode branches */
        case 0xD0:  /* BCT (IL mode) */
            if (C) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xD1:  /* BRX (IL mode) */
            P = (ea + X) & 0x7FFF;
            break;
        case 0xD2:  /* BOT (IL mode) */
            if (OV) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xD3:  /* BCF (IL mode) */
            if (!C) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xD4:  /* BAN (IL mode) */
            if (A & 0x8000) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xD5:  /* BAZ (IL mode) */
            if (A == 0) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xD6:  /* BOF (IL mode) */
            if (!OV) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xD7:  /* BRU (IL mode) */
            P = ea;
            break;
            
        /* IG mode branches */
        case 0xD8:  /* BCT (IG mode) */
            if (C) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xD9:  /* BRX (IG mode) */
            P = (ea + X) & 0x7FFF;
            break;
        case 0xDA:  /* BOT (IG mode) */
            if (OV) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xDB:  /* BCF (IG mode) */
            if (!C) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xDC:  /* BAN (IG mode) */
            if (A & 0x8000) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xDD:  /* BAZ (IG mode) */
            if (A == 0) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xDE:  /* BOF (IG mode) */
            if (!OV) P = ea;
            else P = (P + 2) & 0x7FFF;
            break;
        case 0xDF:  /* BRU (IG mode) */
            P = ea;
            break;
            
        /* ========== PX Mode Instructions (E0-EF) ========== */
        case 0xE0:  /* SHR in PX mode */
            count = disp & 0x1F;
            A = shift_sas(A, count);
            set_condition_codes_load(A);
            break;
            
        case 0xE1:  /* SRG in PX mode */
            /* Handled same as 0x31 */
            {
                srg_op_t srg_op = (disp >> 1) & 0x1F;
                switch (srg_op) {
                    case SRG_RTS: P = read_word(L + GPRIME); L = read_word(L + 2 + GPRIME); break;
                    case SRG_XAE: data = A; A = E; E = data; break;
                    case SRG_XAX: data = A; A = X; X = data; break;
                    case SRG_XEX: data = E; E = X; X = data; break;
                    case SRG_XAA: A = ((A & 0xFF) << 8) | ((A >> 8) & 0xFF); break;
                    case SRG_CCE: E = ~E & 0xFFFF; break;
                    case SRG_RSV:
                        if (mode != 1) return MM_PRVINS;
                        P = read_word(G); L = read_word(G + 2);
                        {
                            uint16 saved_flags = read_word(G + 4);
                            C = (saved_flags >> 0) & 1;
                            OV = (saved_flags >> 1) & 1;
                            MS = 0;
                        }
                        break;
                    case SRG_ACE: E = (E + C) & 0xFFFF; break;
                    case SRG_CCA: A = ~A & 0xFFFF; set_condition_codes_load(A); break;
                    case SRG_AEE: A ^= E; set_condition_codes_load(A); break;
                    case SRG_CNX: X = (~X + 1) & 0xFFFF; break;
                    case SRG_AIE: A |= E; set_condition_codes_load(A); break;
                    case SRG_AAE: A &= E; set_condition_codes_load(A); break;
                    case SRG_LNE: E = 0xFFFF; break;
                    case SRG_CNA: A = (~A + 1) & 0xFFFF; set_condition_codes_load(A); break;
                    case SRG_CHX: X = (X >> 1) | (X & 0x8000); break;
                    default: break;
                }
            }
            break;
            
        case 0xE2:  /* ICX in PX mode */
            X = (X + 1) & 0x7FFF;
            set_condition_codes_load(X);
            break;
            
        case 0xE3:  /* DCX in PX mode */
            X = (X - 1) & 0x7FFF;
            set_condition_codes_load(X);
            break;
            
        case 0xE4:  /* Reserved */
            break;
            
        case 0xE5:  /* ICL in PX mode */
            L = (L + 1) & 0x7FFF;
            break;
            
        case 0xE6:  /* DCL in PX mode */
            L = (L - 1) & 0x7FFF;
            break;
            
        case 0xE7:  /* CSV in PX mode */
/*
 * In Slave mode (modern user mode) the program uses CSV instruction which is a call to the Master mode (modern kernel mode)
*/
            {
                uint16 section = ea;
                write_word(G, P - GPRIME);
                write_word(G + 2, L - GPRIME);
                write_word(G + 4, (C ? 1 : 0) | (OV ? 2 : 0) | (MS ? 4 : 0));
                L = read_word(12 - 4 * section) + G;
                P = read_word(12 - 4 * section + 2) + G;
                MS = 1;
                PR = 1;
            }
            break;
            
        case 0xE8:  /* CLS in PX mode */
            if (mode == 0) return MM_PRVINS;
            {
                uint16 section = ea;
                write_word(L, P - GPRIME);
                write_word(L + 2, L - GPRIME);
                L = read_word(G - 4 * section + 2) + G;
                P = read_word(G - 4 * section) + G;
            }
            break;
            
        case 0xE9:  /* LDR in PX mode */
            {
                uint16 reg_num = read_word(ea) & 0x3F;
                if (reg_num == 0) A = read_word(ea);
                else if (reg_num == 1) E = read_word(ea);
                else if (reg_num == 2) X = read_word(ea);
                else if (reg_num == 3) L = read_word(ea);
                else if (reg_num == 4) G = read_word(ea);
                else if (reg_num == 5) P = read_word(ea);
                set_condition_codes_load(A);
            }
            break;
            
        case 0xEA:  /* STR in PX mode (privileged) */
            if (mode != 1) return MM_PRVINS;
            {
                uint16 reg_num = read_word(ea) & 0x3F;
                if (reg_num == 0) write_word(ea, A);
                else if (reg_num == 1) write_word(ea, E);
                else if (reg_num == 2) write_word(ea, X);
                else if (reg_num == 3) write_word(ea, L);
                else if (reg_num == 4) write_word(ea, G);
                else if (reg_num == 5) write_word(ea, P);
            }
            break;
            
        case 0xEB:  /* LDP in PX mode (privileged) */
            if (mode != 1) return MM_PRVINS;
            PR = read_word(ea) & 1;
            break;
            
        case 0xEC:  /* SHC in PX mode */
            count = disp & 0x1F;
            {
                uint8 shc_type = (disp >> 5) & 0x07;
                switch (shc_type) {
                    case 0: shift_lld(&E, &A, count); break;
                    case 1:
                        if (mode != 1) return MM_PRVINS;
                        int_req &= ~(1u << int_lvl);
                        int_lvl = 0;
                        break;
                    case 2: E = compute_parity(&A, count); break;
                    case 3:
                        if (mode != 1) return MM_PRVINS;
                        int_req &= ~(1u << int_lvl);
                        int_lvl = 0;
                        break;
                    case 4: shift_rld(&E, &A, count); break;
                    case 5: break;
                    case 6: normalize(&E, &A, &X, count); break;
                    case 7: break;
                }
                set_condition_codes_load(A);
            }
            break;
            
        case 0xED:  /* TES in PX mode (privileged) */
            if (mode != 1) return MM_PRVINS;
            A = read_word(ea);
            write_word(ea, 0);
            set_condition_codes_load(A);
            break;
            
        case 0xEE: case 0xEF:  /* Reserved */
            break;
            
        /* ========== P Mode System Instructions (F0-FF) ========== */
        case 0xF0:  /* SHR (P mode) */
            count = disp & 0x1F;
            {
                shift_type_t type = (disp >> 0) & 0x07;
                switch (type) {
                    case SHIFT_SLLS: A = shift_lls(A, count); break;
                    case SHIFT_SRCS: A = shift_srcs(A, count); break;
                    case SHIFT_SAD: shift_sad(&E, &A, count); break;
                    case SHIFT_SLCD: shift_lcd(&E, &A, count); break;
                    case SHIFT_SLCS: A = shift_slcs(A, count); break;
                    case SHIFT_SAS: A = shift_sas(A, count); break;
                    case SHIFT_SRLS: A = shift_rls(A, count); break;
                    case SHIFT_SRCD: shift_rcd(&E, &A, count); break;
                }
                set_condition_codes_load(A);
            }
            break;
            
        case 0xF1:  /* SRG (P mode) */
            {
                srg_op_t srg_op = (disp >> 1) & 0x1F;
                switch (srg_op) {
                    case SRG_RTS: P = read_word(L + GPRIME); L = read_word(L + 2 + GPRIME); break;
                    case SRG_XAE: data = A; A = E; E = data; break;
                    case SRG_XAX: data = A; A = X; X = data; break;
                    case SRG_XEX: data = E; E = X; X = data; break;
                    case SRG_XAA: A = ((A & 0xFF) << 8) | ((A >> 8) & 0xFF); break;
                    case SRG_CCE: E = ~E & 0xFFFF; break;
                    case SRG_RSV:
                        if (mode != 1) return MM_PRVINS;
                        P = read_word(G); L = read_word(G + 2);
                        {
                            uint16 saved_flags = read_word(G + 4);
                            C = (saved_flags >> 0) & 1;
                            OV = (saved_flags >> 1) & 1;
                            MS = 0;
                        }
                        break;
                    case SRG_ACE: E = (E + C) & 0xFFFF; break;
                    case SRG_CCA: A = ~A & 0xFFFF; set_condition_codes_load(A); break;
                    case SRG_AEE: A ^= E; set_condition_codes_load(A); break;
                    case SRG_CNX: X = (~X + 1) & 0xFFFF; break;
                    case SRG_AIE: A |= E; set_condition_codes_load(A); break;
                    case SRG_AAE: A &= E; set_condition_codes_load(A); break;
                    case SRG_LNE: E = 0xFFFF; break;
                    case SRG_CNA: A = (~A + 1) & 0xFFFF; set_condition_codes_load(A); break;
                    case SRG_CHX: X = (X >> 1) | (X & 0x8000); break;
                    default: break;
                }
            }
            break;
            
        case 0xF2:  /* ICX (P mode) */
            X = (X + 1) & 0x7FFF;
            set_condition_codes_load(X);
            break;
            
        case 0xF3:  /* DCX (P mode) */
            X = (X - 1) & 0x7FFF;
            set_condition_codes_load(X);
            break;
            
        case 0xF4:  /* SYS family (privileged) */
            if (mode != 1) return MM_PRVINS;
			// Bits 14, 15 decode 4 intructions: STM, DIT, RD, WD
            switch (disp) {
                case 0x00:  /* STM - Set Interrupt Mask */
                    MA = 1;
                    break;
                case 0x01:  /* DIT - Deactivate Interrupt */
                    {
                        /* Save context at context pointer address */
                        uint16 ctx_ptr = int_vec[int_lvl];
                        write_word(ctx_ptr, ((C ? 1 : 0) | (OV ? 2 : 0) | (MS ? 4 : 0)));
                        write_word(ctx_ptr + 2, X);
                        write_word(ctx_ptr + 4, E);
                        write_word(ctx_ptr + 6, A);
                        write_word(ctx_ptr + 8, G);
                        write_word(ctx_ptr + 10, L);
                        write_word(ctx_ptr + 12, P);
                        /* Deactivate current level */
                        int_req &= ~(1u << int_lvl);
                        /* Find next highest priority level */
                        int_lvl = get_highest_interrupt();
                        /* Restore context */
                        ctx_ptr = int_vec[int_lvl];
                        {
                            uint16 saved_flags = read_word(ctx_ptr);
                            C = (saved_flags >> 0) & 1;
                            OV = (saved_flags >> 1) & 1;
                            MS = (saved_flags >> 2) & 1;
                        }
                        X = read_word(ctx_ptr + 2);
                        E = read_word(ctx_ptr + 4);
                        A = read_word(ctx_ptr + 6);
                        G = read_word(ctx_ptr + 8);
                        L = read_word(ctx_ptr + 10);
                        P = read_word(ctx_ptr + 12);
                    }
                    break;
                case 0x02:  /* RD - Read Direct */
/*
 * In Slave mode (modern user mode) the program uses CSV instruction which is a call to the Master mode (modern kernel mode)
 * In Master mode I/O is done with RD or WD instructions.

        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
      |          F4           |                 | 1  0|
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

	  The mode is determined by the contents of E-register.
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
      |   cylinder 0-407         | T| sector 0-23  | D|
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*/
                    return io_rd(E, &A);
                case 0x03:  /* WD - Write Direct */
/*
 * In Slave mode (modern user mode) the program uses CSV instruction which is a call to the Master mode (modern kernel mode)
 * In Master mode I/O is done with RD or WD instructions.
        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
      |          F4           |                 | 1  0|
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

	  The mode is determined by the contents of E-register.
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
      |   cylinder 0-407         | T| sector 0-23  | D|
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*/
                    return io_wd(E, A);
                case 0x08:  /* CLM - Clear Interrupt Mask */
					/* FIXME The reference manual is unclear here page B-4 says it exists but page VII-104 says disp is 2 bits*/
                    MA = 0;
                    break;
                default:
                    if (stop_invins) return STOP_INVINS;
                    break;
            }
            break;
            
        case 0xF5:  /* ICL (P mode) */
            L = (L + 1) & 0x7FFF;
            break;
            
        case 0xF6:  /* DCL (P mode) */
            L = (L - 1) & 0x7FFF;
            break;
            
        case 0xF7:  /* CSV (P mode) */
/*
 * In Slave mode (modern user mode) the program uses CSV instruction which is a call to the Master mode (modern kernel mode)
*/
            {
                uint16 section = ea;
                write_word(G, P - GPRIME);
                write_word(G + 2, L - GPRIME);
                write_word(G + 4, (C ? 1 : 0) | (OV ? 2 : 0) | (MS ? 4 : 0));
                L = read_word(12 - 4 * section) + G;
                P = read_word(12 - 4 * section + 2) + G;
                MS = 1;
                PR = 1;
            }
            break;
            
        case 0xF8:  /* CLS (P mode) */
            if (mode == 0) return MM_PRVINS;
            {
                uint16 section = ea;
                write_word(L, P - GPRIME);
                write_word(L + 2, L - GPRIME);
                L = read_word(G - 4 * section + 2) + G;
                P = read_word(G - 4 * section) + G;
            }
            break;
            
        case 0xF9:  /* LDR (P mode) */
            {
                uint16 reg_num = read_word(ea) & 0x3F;
                if (reg_num == 0) A = read_word(ea);
                else if (reg_num == 1) E = read_word(ea);
                else if (reg_num == 2) X = read_word(ea);
                else if (reg_num == 3) L = read_word(ea);
                else if (reg_num == 4) G = read_word(ea);
                else if (reg_num == 5) P = read_word(ea);
                set_condition_codes_load(A);
            }
            break;
            
        case 0xFA:  /* STR (P mode, privileged) */
            if (mode != 1) return MM_PRVINS;
            {
                uint16 reg_num = read_word(ea) & 0x3F;
                if (reg_num == 0) write_word(ea, A);
                else if (reg_num == 1) write_word(ea, E);
                else if (reg_num == 2) write_word(ea, X);
                else if (reg_num == 3) write_word(ea, L);
                else if (reg_num == 4) write_word(ea, G);
                else if (reg_num == 5) write_word(ea, P);
            }
            break;
            
        case 0xFB:  /* LDP (P mode, privileged) */
            if (mode != 1) return MM_PRVINS;
            PR = read_word(ea) & 1;
            break;
            
        case 0xFC:  /* SHC (P mode) */
            count = disp & 0x1F;
            {
                uint8 shc_type = (disp >> 0) & 0x07;
                switch (shc_type) {
                    case 0: shift_lld(&E, &A, count); break;
                    case 1:
                        if (mode != 1) return MM_PRVINS;
                        int_req &= ~(1u << int_lvl);
                        int_lvl = 0;
                        break;
                    case 2: E = compute_parity(&A, count); break;
                    case 3:
                        if (mode != 1) return MM_PRVINS;
                        int_req &= ~(1u << int_lvl);
                        int_lvl = 0;
                        break;
                    case 4: shift_rld(&E, &A, count); break;
                    case 5: break;
                    case 6: normalize(&E, &A, &X, count); break;
                    case 7: break;
                }
                set_condition_codes_load(A);
            }
            break;
            
        case 0xFD:  /* TES (P mode, privileged) */
            if (mode != 1) return MM_PRVINS;
            A = read_word(ea);
            write_word(ea, 0);
            set_condition_codes_load(A);
            break;
            
        case 0xFE: case 0xFF:  /* Reserved */
            break;
            
        default:
            if (stop_invins)
                return STOP_INVINS;
            break;
    }
    
    return SCPE_OK;
}

/* Helper to get highest pending interrupt */
static int get_highest_interrupt(void)
{
    int i;
    for (i = 31; i >= 0; i--) {
        if (int_req & (1u << i)) return i;
    }
    return 0;
}

/* ========== SIMH Interface Functions ========== */

t_stat sim_instr(void)
{
    uint16 inst, save_P, trap_P;
    t_stat reason = 0;

    xfr_req = xfr_req & ~1;
    int_req = int_req & ~1;
    api_lvl = api_lvl & ~1;
    set_dyn_map();
    
    io_poll_devices(); // also for checking front panel
    
    int_reqhi = api_findreq();

    while (reason == 0) {
        if (cpu_astop) {
            cpu_astop = 0;
            return SCPE_STOP;
        }

        if (sim_interval <= 0) {
            if ((reason = sim_process_event()))
                break;
            int_reqhi = api_findreq();
        }

        sim_interval--;

        /* Check for interrupts */
        if (ion && !ion_defer && int_reqhi && int_reqhi > int_lvl) {
            uint16 pa = int_vec[int_reqhi];
            if (pa == 0) {
                reason = STOP_ILLVEC;
                break;
            }
            
            /* Save context for current level */
            uint16 ctx_ptr = int_vec[int_lvl];
            write_word(ctx_ptr, ((C ? 1 : 0) | (OV ? 2 : 0) | (MS ? 4 : 0)));
            write_word(ctx_ptr + 2, X);
            write_word(ctx_ptr + 4, E);
            write_word(ctx_ptr + 6, A);
            write_word(ctx_ptr + 8, G);
            write_word(ctx_ptr + 10, L);
            write_word(ctx_ptr + 12, P);
            
            /* Switch to new level */
            int_lvl = int_reqhi;
            ctx_ptr = int_vec[int_lvl];
            {
                uint16 saved_flags = read_word(ctx_ptr);
                C = (saved_flags >> 0) & 1;
                OV = (saved_flags >> 1) & 1;
                MS = (saved_flags >> 2) & 1;
            }
            X = read_word(ctx_ptr + 2);
            E = read_word(ctx_ptr + 4);
            A = read_word(ctx_ptr + 6);
            G = read_word(ctx_ptr + 8);
            L = read_word(ctx_ptr + 10);
            P = read_word(ctx_ptr + 12);
            
            /* Acknowledge interrupt */
            if (pa != VEC_RTCP && rtc_pie) {
                int_req |= INT_RTCP;
            }
            int_reqhi = api_findreq();
        } else {
            /* Normal instruction fetch */
            if (sim_brk_summ) {
                static uint32 bmask[] = {SWMASK('E') | SWMASK('N'),
                                         SWMASK('E') | SWMASK('M'),
                                         SWMASK('E') | SWMASK('U')};
                uint32 btyp = sim_brk_test(P, bmask[cpu_mode]);
                if (btyp) {
                    if (btyp & SWMASK('E'))
                        reason = STOP_IBKPT;
                    else if (btyp & BRK_TYP_DYN_STEPOVER)
                        reason = STOP_DBKPT;
                    else switch (btyp) {
                        case SWMASK('M'): reason = STOP_MBKPT; break;
                        case SWMASK('N'): reason = STOP_NBKPT; break;
                        case SWMASK('U'): reason = STOP_UBKPT; break;
                    }
                    sim_interval++;
                    break;
                }
            }
            
            trap_P = save_P = P;
            inst = read_word(P);
            P = (P + 2) & 0x7FFF;  /* Instructions are word-aligned */
            
            if (inst != 0) {
                ion_defer = 0;
                reason = one_inst(inst, save_P, cpu_mode, &trap_P);
                if (reason > 0 && reason != STOP_HALT) {
                    P = save_P;
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

t_stat rtc_svc(UNIT *uptr)
{
    if (rtc_pie)
        int_req |= INT_RTCP;
    rtc_unit.wait = sim_rtcn_calb(rtc_tps, TMR_RTC);
    sim_activate(&rtc_unit, rtc_unit.wait);
    return SCPE_OK;
}

t_stat rtc_reset(DEVICE *dptr)
{
    rtc_pie = 0;
    rtc_unit.wait = sim_rtcn_init(rtc_unit.wait, TMR_RTC);
    sim_activate(&rtc_unit, rtc_unit.wait);
    return SCPE_OK;
}

t_stat rtc_set_freq(UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
    if (cptr)
        return SCPE_ARG;
    if (val != 50 && val != 60)
        return SCPE_IERR;
    rtc_tps = val;
    return SCPE_OK;
}

t_stat rtc_show_freq(FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
    fprintf(st, (rtc_tps == 50) ? "50Hz" : "60Hz");
    return SCPE_OK;
}

/* ========== CPU Reset and Management ========== */

t_stat cpu_reset(DEVICE *dptr)
{
    A = E = X = L = G = P = S = 0;
    MREG = V = W = U = 0;
    C = OV = MS = 0;
    MA = PR = 0;
    cpu_mode = 0;
    ion = 0;
    ion_defer = 0;
    int_req = 0;
    int_lvl = 0;
    
	/* Front panel reset */
    cpu_running = 0;
    interrupts_enabled = 0;
    routing_enabled = 0;
    panel_addr_lights = 0;
    panel_data_lights = 0;

    panel_reset();   /* call the panel reset too */
    
    return SCPE_OK;
}

t_stat cpu_set_size(UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
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

t_stat cpu_ex(t_value *vptr, t_addr addr, UNIT *uptr, int32 sw)
{
    uint32 pa = addr & 0x7FFF;
    if (pa >= MEMsize)
        return SCPE_NXM;
    if (vptr != NULL)
        *vptr = M[pa] & DMASK;
    return SCPE_OK;
}

t_stat cpu_dep(t_value val, t_addr addr, UNIT *uptr, int32 sw)
{
    uint32 pa = addr & 0x7FFF;
    if (pa >= MEMsize)
        return SCPE_NXM;
    M[pa] = val & DMASK;
    return SCPE_OK;
}

/* ========== History Functions ========== */

void inst_hist(uint32 c, uint32 pc, uint32 tp)
{
    if (cpu_mode == hst_exclude)
        return;
    hst_p = (hst_p + 1);
    if (hst_p >= hst_lnt)
        hst_p = 0;
    hst[hst_p].typ = tp | (OV << 4) | (cpu_mode << 5);
    hst[hst_p].P = pc;
    hst[hst_p].A = A;
    hst[hst_p].E = E;
    hst[hst_p].X = X;
    hst[hst_p].L = L;
    hst[hst_p].G = G;
    hst[hst_p].S = S;
    hst[hst_p].U = U;
    hst[hst_p].V = V;
    hst[hst_p].W = W;
    hst[hst_p].MREG = MREG;
    hst[hst_p].ea = HIST_NOEA;
}

t_stat cpu_set_hist(UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
    int32 i, lnt;
    t_stat r;
    
    if (cptr == NULL) {
        for (i = 0; i < hst_lnt; i++)
            hst[i].typ = 0;
        hst_p = 0;
        return SCPE_OK;
    }
    
    lnt = (int32)get_uint(cptr, 10, HIST_MAX, &r);
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
        hst = (InstHistory *)calloc(lnt, sizeof(InstHistory));
        if (hst == NULL)
            return SCPE_MEM;
        hst_lnt = lnt;
    }
    
    return SCPE_OK;
}

t_stat cpu_show_hist(FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
    int32 k, lnt;
    CONST char *cptr = (CONST char *)desc;
    t_stat r;
    InstHistory *h;
    static const char *cyc[] = {"   ", "   ", "INT", "TRP"};
    static const char *modes = "NMU?";
    
    if (hst_lnt == 0)
        return SCPE_NOFNC;
    
    if (cptr) {
        lnt = (int32)get_uint(cptr, 10, hst_lnt, &r);
        if (r != SCPE_OK || lnt == 0)
            return SCPE_ARG;
    } else {
        lnt = hst_lnt;
    }
    
    fprintf(st, "CYC PC    MD OV A        E        X        EA\n\n");
    for (k = 0; k < lnt; k++) {
        h = &hst[(++hst_p) % hst_lnt];
        if (h->typ) {
            fprintf(st, "%s %05o %c  %o  %06o %06o %06o\n",
                cyc[h->typ & 3], h->P, modes[(h->typ >> 5) & 3],
                (h->typ >> 4) & 1, h->A, h->E, h->X);
        }
    }
    return SCPE_OK;
}
