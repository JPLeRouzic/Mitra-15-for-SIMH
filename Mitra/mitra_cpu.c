/* mitra_cpu.c: Mitra 15 CPU simulator
 *
 * Based on MITRA 15 Reference Manual (CII, 1973)
 * 
 */

#include "mitra_defs.h"

#include "mitra_io.h"   /* header for I/O subsystem */

extern t_stat io_csv_1o(uint32 cb_addr, int zio);   /* M:1O / M:ZIO */
extern t_stat io_csv_wait(uint32 cb_addr, int zwat);/* M:WAIT / M:ZWAT */
extern void io_interrupt_dispatch(void);            /* call when interrupt pending */
extern int  io_check_ready(void);                   /* true if any device ready */


/* SIMH flags */
#define PCQ_SIZE        64                              /* must be 2**n */
#define PCQ_MASK        (PCQ_SIZE - 1)
#define PCQ_ENTRY       pcq[pcq_p = (pcq_p - 1) & PCQ_MASK] = pc
#define UNIT_V_MSIZE    (UNIT_V_GENIE + 1)              /* dummy mask */
#define UNIT_MSIZE      (1 << UNIT_V_MSIZE)

#define HIST_XCT        1                               /* instruction */
#define HIST_INT        2                               /* interrupt cycle */
#define HIST_TRP        3                               /* trap cycle */
#define HIST_MIN        64
#define HIST_MAX        65536
#define HIST_NOEA       0x40000000

/* Unit flags for cpu_unit */
// #define UNIT_MSIZE      (1 << 0)      /* Memory size modifier */
#define UNIT_EXTINS     (1 << 1)      /* Extended instruction set */
#define UNIT_FP         (1 << 2)      /* Floating point (OVF) */
#define UNIT_MULDIV     (1 << 3)      /* Hardware MUL/DIV */
#define UNIT_HSINT      (1 << 4)      /* High-speed interrupt */

/* Memory size values */
#define MEM_4K   4096
#define MEM_8K   8192
#define MEM_16K  16384
#define MEM_32K  32768

typedef struct {
    uint32              typ; // FIXME

    uint16 P ;          /* Program counter */
    uint16 A ;           /* Accumulator */
    uint16 E ;           /* Extension register */
    uint16 X ;           /* Index register */
    uint16 L ;           /* Local base */
    uint16 G ;           /* General base */
    
    /* Program indicators (page 11-5, 11-6) */
    uint8 C ;            /* Carry (bit 0) */
    uint8 OV ;          /* Overflow (bit 1) */
    uint8 MS ;          /* Master/Slave (1=Master) */
    uint16 MA;          /* Interrupt mask */
    uint16 PR;          /* Protection key */
    
    /* Microprocessor registers (page 11-4) */
    uint16 S ;           /* Address register */
    uint16 MREG ;     /* Memory data register */
    uint16 U ;           /* Accumulation register */
    uint16 V ;           /* Microprogram use */
    uint16 W ;           /* Microprogram use */
    
    uint32              ea; // FIXME
    } InstHistory;

uint32 xfr_req = 0;                                     /* xfr req */

/* System state */
uint16 cpu_mode;    /* 0=Normal, 1=Monitor, 2=User */
uint16 RL1, RL2, RL4;  /* Relocation registers */
uint16 int_req;     /* Interrupt requests */
uint16 int_lvl;     /* Current interrupt level */
uint16 ion;         /* Interrupt enable */
uint16 ion_defer;   /* Interrupt defer */

uint32 int_reqhi = 0;                                   /* highest int request */
uint32 api_lvl = 0;                                     /* api active */
uint32 api_lvlhi = 0;                                   /* highest api active */

t_bool chan_req;                                        /* chan request */


uint32 bpt;                                             /* breakpoint switches */
uint32 alert;                                           /* alert dispatch */
uint32 em2_dyn, em3_dyn;                                /* extensions, dynamic */
uint32 usr_map[8];                                      /* user map, dynamic */
uint32 mon_map[8];                                      /* mon map, dynamic */
int32 ind_lim = 32;                                     /* indirect limit */
int32 exu_lim = 32;                                     /* EXU limit */

uint16 cpu_mode = NML_MODE;                             /* normal mode */
uint32 mon_usr_trap = 0;                                /* mon-user trap */
uint32 EM2 = 2, EM3 = 3;                                /* extension registers */
// uint32 RL1, RL2, RL4;                                   /* relocation maps */

int32 cpu_genie = 0;                                    /* Genie flag */
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
t_stat cpu_set_type (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat cpu_set_hist (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat cpu_show_hist (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat Ea (uint32 wd, uint16 *va);
t_stat EaSh (uint32 wd, uint16 *va);
t_stat Read (uint16 *va, uint16 *dat);
t_stat Write (uint16 va, uint16 dat);
void set_dyn_map (void);
uint32 api_findreq (void);
void api_dismiss (void);
uint32 Add24 (uint32 s1, uint32 s2, uint32 cin);
uint32 AddM24 (uint32 s1, uint32 s2);
void Mul48 (uint32 mplc, uint32 mplr);
void Div48 (uint32 dvdh, uint32 dvdl, uint32 dvr);
void RotR48 (uint32 sc);
void ShfR48 (uint32 sc, uint32 sgn);
t_stat one_inst (uint16 inst, uint16 pc, uint32 mode, uint16 *trappc);
void inst_hist (uint32 inst, uint32 pc, uint32 typ);
t_stat rtc_inst (uint32 inst);
t_stat rtc_svc (UNIT *uptr);
t_stat rtc_reset (DEVICE *dptr);
t_stat rtc_set_freq (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat rtc_show_freq (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
uint32 RelocC (int32 va, int32 sw);

extern uint32 mon_usr_trap;
/* CPU data structures

   cpu_dev      CPU device descriptor
   cpu_unit     CPU unit descriptor
   cpu_reg      CPU register list
   cpu_mod      CPU modifiers list

	Memory is 4K to 32K words
*/

UNIT cpu_unit = { 
    UDATA(NULL, UNIT_FIX + UNIT_BINK, MAXMEMSIZE)  /* Max 32K words */
};

/* CPU Registers (manual page 1-2, 11-4) */
uint16 A;   /* Accumulator */
uint16 E;   /* Extended A register (used with A for double precision) */
uint16 X;   /* Index register */
uint16 L;   /* Local base register */
uint16 G;   /* General base register */
uint16 P;   /* Program counter */
uint16 S;   /* Address register (15-bit, used by microprogram) */
uint32 MREG; /* Memory data register (18-bit with parity/protection) */
uint16 V;   /* Used by microprograms */
uint16 W;   /* Used by microprograms */
uint16 U;   /* Microprocessor accumulation register */

/* Program Indicators (manual page 11-5) */
uint8 C;   /* Carry indicator (bit 0) / also test result */
uint8 OV;  /* Overflow indicator (bit 1) */
uint8 MS;  /* Master/Slave mode (1=Master, 0=Slave) */
uint16 MA;  /* Interrupt mask indicator */
uint16 PR;  /* Memory protection key */


/* CPU Register List
Right Justified Octal Register Data
#define ORDATA(nm,loc,wd) \
    _RegCheck(#nm,&(loc),8,wd,0,1,NULL,NULL,0,0,sizeof((loc)),ORDATA)
*/
REG cpu_reg[] = {
    /* User registers (page 1-2, 11-4) */
    { ORDATA(P, P, 16) },           /* Program counter */
    { ORDATA(A, A, 16) },           /* Accumulator */
    { ORDATA(E, E, 16) },           /* Extension register */
    { ORDATA(X, X, 16) },           /* Index register */
    { ORDATA(L, L, 16) },           /* Local base */
    { ORDATA(G, G, 16) },           /* General base */
    
    /* Program indicators (page 11-5, 11-6) */
    { FLDATA(C, C, 0) },            /* Carry (bit 0) */
    { FLDATA(OV, OV, 0) },          /* Overflow (bit 1) */
    { FLDATA(MS, MS, 0) },          /* Master/Slave (1=Master) */
    { FLDATA(MA, MA, 0) },          /* Interrupt mask */
    { FLDATA(PR, PR, 0) },          /* Protection key */
    
    /* Microprocessor registers (page 11-4) */
    { ORDATA(S, S, 15) },           /* Address register */
    { ORDATA(MREG, MREG, 18) },     /* Memory data register */
    { ORDATA(U, U, 16) },           /* Accumulation register */
    { ORDATA(V, V, 16) },           /* Microprogram use */
    { ORDATA(W, W, 16) },           /* Microprogram use */
    
    /* Interrupt system (page 11-6, 11-7) */
    { ORDATA(INT_REQ, int_req, 32) },   /* Interrupt requests (32 levels) */
    { ORDATA(INT_LVL, int_lvl, 5) },    /* Current interrupt level (0-31) */
    { FLDATA(ION, ion, 0) },            /* Interrupt enable */
    { FLDATA(ION_DEFER, ion_defer, 0) }, /* Interrupt defer */
//    { ORDATA(INT_MASK, int_mask, 32) },  /* Interrupt mask */ FIXME
    
    /* Memory management (page 11-10) */
    { ORDATA(RL1, RL1, 16) },       /* Relocation register 1 */
    { ORDATA(RL2, RL2, 16) },       /* Relocation register 2 */
    { ORDATA(RL4, RL4, 16) },       /* Relocation register 4 */
    
    /* System state */
    { ORDATA(CPU_MODE, cpu_mode, 2) },  /* 0=Normal, 1=Monitor, 2=User */
    
    /* Debugging */
    { DRDATA(INDLIM, ind_lim, 8), REG_NZ + PV_LEFT },  /* Indirect limit */
    { DRDATA(EXULIM, exu_lim, 8), REG_NZ + PV_LEFT },  /* EXU limit */
    
    /* Console */
    { ORDATA(WRU, sim_int_char, 8) },
    
    { NULL }
};

/*
CPU Modifiers List (cpu_mod)

Based on manual options (page 1-3, 1-4):
Option	Manual Reference	Description
Memory size	page 1-1	4K to 32K words
Instruction set	page 1-2	Basic (77) or Extended (86)
Floating point	page 1-3, 1-4	OVF option
MUL/DIV	page 1-3, 1-4	Hardware multiply/divide
Interrupt levels	page 11-6	1 to 32 levels
*/
MTAB cpu_mod[] = {
    /* Memory size options (page 1-1) */
//  FIXME   { UNIT_MSIZE, 4096, NULL, "4K", &cpu_set_size },
    { UNIT_MSIZE, 8192, NULL, "8K", &cpu_set_size },
    { UNIT_MSIZE, 16384, NULL, "16K", &cpu_set_size },
    { UNIT_MSIZE, 32768, NULL, "32K", &cpu_set_size },
    
    /* Instruction set options (page 1-2) */
//  FIXME    { UNIT_EXTINS, 0, "basic instructions", "BASIC", &cpu_set_inset },
//  FIXME    { UNIT_EXTINS, UNIT_EXTINS, "extended instructions", "EXTENDED", &cpu_set_inset },
    
    /* Floating point option (page 1-3, 1-4, 7-64) */
//  FIXME    { UNIT_FP, 0, "no floating point", "NOFP", &cpu_set_fp },
//  FIXME    { UNIT_FP, UNIT_FP, "floating point (OVF)", "FP", &cpu_set_fp },
    
    /* Hardware MUL/DIV option (page 1-3, 1-4) */
//  FIXME    { UNIT_MULDIV, 0, "software MUL/DIV", "SWMD", &cpu_set_muldiv },
//  FIXME    { UNIT_MULDIV, UNIT_MULDIV, "hardware MUL/DIV", "HWMD", &cpu_set_muldiv },
    
    /* High-speed interrupt option (page 11-7) */
//  FIXME    { UNIT_HSINT, 0, "standard interrupts", "STDINT", &cpu_set_hsint },
//  FIXME    { UNIT_HSINT, UNIT_HSINT, "high-speed interrupt", "HSINT", &cpu_set_hsint },
    
    /* History (debugging) */
    { MTAB_XTD|MTAB_VDV|MTAB_NMO|MTAB_SHP, 0, "HISTORY", "HISTORY",
      &cpu_set_hist, &cpu_show_hist },
    
    { 0 }
};

/*
* The Mitra-15 CPU has:
*
*    16-bit word size (from page 1-1)
*
*    Byte-addressable (even addresses = word boundaries)
*
*    Up to 32K words of memory (page 1-1)
*
*    86 instructions (page 1-2)
*/
DEVICE cpu_dev = {
    "CPU",           /* Device name */
    &cpu_unit,       /* Unit descriptor */
    cpu_reg,         /* Register list */
    cpu_mod,         /* Modifier list */
    1,               /* Number of units */
    8, 16,           /* Address radix (octal), data radix (hex) */
    1, 8, 16,        /* Address size (16-bit), data size (16-bit) */
    &cpu_ex,         /* Examine routine */
    &cpu_dep,        /* Deposit routine */
    &cpu_reset,      /* Reset routine */
    NULL, NULL, NULL, /* Boot, attach, detach */
    NULL, 0          /* Context, flags */
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

/* Interrupt tables */

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

/* static const uint32 int_vec[32] = {
    0, 0, 0, 0,
    VEC_FORK, VEC_DRM,  VEC_MUXCF,VEC_MUXCO,
    VEC_MUXT, VEC_MUXR, VEC_HEOR, VEC_HZWC,
    VEC_GEOR, VEC_GZWC, VEC_FEOR, VEC_FZWC,
    VEC_EEOR, VEC_EZWC, VEC_DEOR, VEC_DZWC,
    VEC_CEOR, VEC_CZWC, VEC_WEOR, VEC_YEOR,
    VEC_WZWC, VEC_YZWC, VEC_RTCP, VEC_RTCS,
    VEC_IPAR, VEC_CPAR, VEC_PWRF, VEC_PWRO
    }; */

extern t_bool io_init (void);
extern t_stat op_wyim (uint32 inst, uint32 *dat);
extern t_stat op_miwy (uint32 inst, uint32 dat);
extern t_stat op_pin (uint32 *dat);
extern t_stat op_pot (uint32 dat);
extern t_stat op_eomd (uint32 inst);
extern t_stat op_sks (uint32 inst, uint32 *skp);

/* Memory */
uint16 M[MAXMEMSIZE];

/* Instruction format decoding (manual page 1-2) */
#define I_MODE_MASK     0xE000  /* bits 0-2 (15-13) */
#define I_MODE_SHIFT    13
#define I_OPCODE_MASK   0x1F00  /* bits 3-7 */
#define I_OPCODE_SHIFT  8
#define I_DISP_MASK     0x00FF  /* bits 8-15 */

/* Special format for some instructions (4-4-8 format) */
#define I_MODE4_MASK    0xF000  /* bits 0-3 */
#define I_MODE4_SHIFT   12
#define I_OPCODE4_MASK  0x0F00  /* bits 4-7 */
#define I_OPCODE4_SHIFT 8

/* Addressing Modes - Class 0 and 0' (manual page 5-1, 5-2) */
#define AM_DL   0  /* Direct Local: Y = (L) + D */
#define AM_IL   1  /* Indirect Local: Y = G' + ((L) + D) */
#define AM_ILX  2  /* Indirect Local Indexed: Y = G' + ((L) + D) + (X) */
#define AM_DG   4  /* Direct General: Y = (G) + D */
#define AM_IGX  5  /* Indirect General Indexed: Y = G' + ((G) + D) + (X) */
#define AM_P    6  /* Parameter (immediate): operand = D */
#define AM_PX   7  /* Parameter Indexed (class 1 only) */

/* Addressing Modes - Class 2 (manual page 5-3) */
#define AM_RP   6  /* Relative Plus: Y = (P) + 2D */
#define AM_RM   7  /* Relative Minus: Y = (P) - 2D */

/* G' in slave mode is 0, in master mode is G (manual page 11-10) */
#define GPRIME  ((MS) ? G : 0)

/* Condition code bits (manual page 11-5) */
#define CARRY_BIT   0x0001
#define OVERFLOW_BIT 0x0002
#define ZERO_BIT    0x0004
#define NEG_BIT     0x0008

/* Function prototypes */
static uint16 ea_class0(uint16 inst);
static uint16 ea_class2(uint16 inst);
uint16 read_word(uint16 addr);
void write_word(uint16 addr, uint16 val);
static uint16 add16(uint16 a, uint16 b, uint16 *carry, uint16 *overflow);
static uint16 sub16(uint16 a, uint16 b, uint16 *carry, uint16 *overflow);
static void set_condition_codes(uint16 result);
static void mul32(uint16 a, uint16 b, uint16 *high, uint16 *low);
static int div32(uint16 high, uint16 low, uint16 divisor, uint16 *quot, uint16 *rem);

/* ========== Memory Access with Protection (manual page 11-10) ========== */

uint16 read_word(uint16 addr)
{
    uint16 word_addr = addr >> 1;  /* byte address to word address */
    if (word_addr >= MEMSIZE) {
        return 0;  /* would trap */
    }
    return M[word_addr];
}

void write_word(uint16 addr, uint16 val)
{
    uint16 word_addr = addr >> 1;
    if (word_addr < MEMSIZE) {
        M[word_addr] = val;
    }
}

/* ========== Arithmetic (manual page 11-5) ========== */

static uint16 add16(uint16 a, uint16 b, uint16 *carry, uint16 *overflow)
{
    uint32 sum = a + b + *carry;
    uint16 result = sum & 0xFFFF;
    
    *carry = (sum >> 16) & 1;
    
    /* Overflow: operands same sign, result opposite sign */
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
    /* Subtraction: A - B = A + (~B) + 1 */
    uint16 borrow = *carry;
    uint32 diff = a + (~b & 0xFFFF) + borrow;
    uint16 result = diff & 0xFFFF;
    
    *carry = (diff >> 16) & 1;
    
    if (((a & 0x8000) != (b & 0x8000)) && 
        ((a & 0x8000) != (result & 0x8000))) {
        *overflow = 1;
    } else {
        *overflow = 0;
    }
    return result;
}

static void set_condition_codes(uint16 result)
{
    /* C and OV are set by arithmetic, preserved otherwise */
    if (result == 0) {
        /* Zero indicator is C=1 (manual page 11-5) */
        /* For non-arithmetic, C=1 means zero */
    }
}

/* 32-bit multiplication for MUL instruction (manual page 7-29) */
static void mul32(uint16 a, uint16 b, uint16 *high, uint16 *low)
{
    uint32 product = (uint32)a * (uint32)b;
    *high = (product >> 16) & 0xFFFF;
    *low = product & 0xFFFF;
}

/* 32-bit division for DIV instruction (manual page 7-30) */
static int div32(uint16 high, uint16 low, uint16 divisor, uint16 *quot, uint16 *rem)
{
    uint32 dividend = ((uint32)high << 16) | low;
    if (divisor == 0) return -1;  /* Division by zero */
    *quot = dividend / divisor;
    *rem = dividend % divisor;
    return 0;
}

/* ========== Effective Address Calculation - Class 0 (manual page 5-1) ========== */

static uint16 ea_class0(uint16 inst)
{
    uint16 mode = (inst >> I_MODE_SHIFT) & 0x07;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp, addr;
    
    switch (mode) {
        case AM_DL:   /* Direct Local */
            addr = (L + disp) & 0xFFFF;
            break;
            
        case AM_IL:   /* Indirect Local */
            tmp = read_word((L + disp) & 0xFFFF);
            addr = (GPRIME + tmp) & 0xFFFF;
            break;
            
        case AM_ILX:  /* Indirect Local Indexed */
            tmp = read_word((L + disp) & 0xFFFF);
            addr = (GPRIME + tmp + X) & 0xFFFF;
            break;
            
        case AM_DG:   /* Direct General */
            addr = (G + disp) & 0xFFFF;
            break;
            
        case AM_IGX:  /* Indirect General Indexed */
            tmp = read_word((G + disp) & 0xFFFF);
            addr = (GPRIME + tmp + X) & 0xFFFF;
            break;
            
        default:      /* Parameter modes */
            addr = disp;
            break;
    }
    return addr;
}

/* ========== Effective Address Calculation - Class 2 (manual page 5-3) ========== */

static uint16 ea_class2(uint16 inst)
{
    uint16 mode = (inst >> I_MODE_SHIFT) & 0x07;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp, addr;
    
    switch (mode) {
        case AM_RP:   /* Relative Plus */
            addr = (P + 2 * disp) & 0xFFFF;
            break;
            
        case AM_RM:   /* Relative Minus */
            addr = (P - 2 * disp) & 0xFFFF;
            break;
            
        case AM_IL:   /* Indirect Local (class 2) */
            tmp = read_word((L + disp) & 0xFFFF);
            addr = (GPRIME + tmp) & 0xFFFF;
            break;
            
        case AM_DG:   /* Direct General (class 2) */
            addr = (GPRIME + (G + disp)) & 0xFFFF;
            break;
            
        default:
            addr = disp;
            break;
    }
    return addr;
}

/* ========== Main Instruction Execution ========== */

static const uint32 int_vec[32] = {
    0, 0, 0, 0,
    VEC_FORK, VEC_DRM,  VEC_MUXCF,VEC_MUXCO,
    VEC_MUXT, VEC_MUXR, VEC_HEOR, VEC_HZWC,
    VEC_GEOR, VEC_GZWC, VEC_FEOR, VEC_FZWC,
    VEC_EEOR, VEC_EZWC, VEC_DEOR, VEC_DZWC,
    VEC_CEOR, VEC_CZWC, VEC_WEOR, VEC_YEOR,
    VEC_WZWC, VEC_YZWC, VEC_RTCP, VEC_RTCS,
    VEC_IPAR, VEC_CPAR, VEC_PWRF, VEC_PWRO
    };

t_stat sim_instr (void)
{
uint16 inst, tinst, pa, save_P, save_mode, trap_P, tmp;
t_stat reason, tr;

/* Restore register state */

if (io_init ())                                         /* init IO; conflict? */
    return SCPE_STOP;
reason = 0;
xfr_req = xfr_req & ~1;                                 /* <0> reserved */
int_req = int_req & ~1;                                 /* <0> reserved */
api_lvl = api_lvl & ~1;                                 /* <0> reserved */
set_dyn_map ();                                         /* set up mapping */
int_reqhi = api_findreq ();                             /* recalc int req */
chan_req = chan_testact ();                             /* recalc chan act */

/* Main instruction fetch/decode loop */

while (reason == 0) {                                   /* loop until halted */

    if (cpu_astop) {                                    /* debug stop? */
        cpu_astop = 0;
        return SCPE_STOP;
        }

    if (sim_interval <= 0) {                            /* event queue? */
        if ((reason = sim_process_event ()))            /* process */
            break;
        int_reqhi = api_findreq ();                     /* recalc int req */
        chan_req = chan_testact ();                     /* recalc chan act */
        }

    if (chan_req) {                                     /* channel request? */
        if ((reason = chan_process ()))                 /* process */
            break;
        int_reqhi = api_findreq ();                     /* recalc int req */
        chan_req = chan_testact ();                     /* recalc chan act */
        }

    sim_interval = sim_interval - 1;                    /* count down */
    if (ion && !ion_defer && int_reqhi) {               /* int request? */
        pa = int_vec[int_reqhi];                        /* get vector */
        if (pa == 0) {                                  /* bad value? */
            reason = STOP_ILLVEC;
            break;
            }
        tinst = ReadP (pa);                             /* get inst */
        save_mode = cpu_mode;                           /* save mode */
        cpu_mode = MON_MODE;                            /* switch to mon */
        if (hst_lnt)                                    /* record inst */
            inst_hist (tinst, P, HIST_INT);
        if (pa != VEC_RTCP) {                           /* normal intr? */
            tr = one_inst (tinst, P, save_mode, &tmp);  /* exec intr inst */
            Read (&P, (uint16 *) &C);
            if (tr) {                                   /* stop code? */
                cpu_mode = save_mode;                   /* restore mode */
                reason = (tr > 0)? tr: STOP_MMINT;
                break;
                }
            api_lvl = api_lvl | (1u << int_reqhi);      /* set level active */
            api_lvlhi = int_reqhi;                      /* elevate api */
            }
        else {                                          /* clock intr */
            tr = rtc_inst (tinst);                      /* exec RTC inst */
            cpu_mode = save_mode;                       /* restore mode */
            if (tr) {                                   /* stop code? */
                reason = (tr > 0)? tr: STOP_MMINT;
                break;
                }
            int_req = int_req & ~INT_RTCP;              /* clr clkp intr */
            }
        int_reqhi = api_findreq ();                     /* recalc int req */
        }
    else {                                              /* normal instr */
        if (sim_brk_summ) {
            static uint32 bmask[] = {SWMASK ('E') | SWMASK ('N'),
                                     SWMASK ('E') | SWMASK ('M'),
                                     SWMASK ('E') | SWMASK ('U')};
            uint32 btyp;

            btyp = sim_brk_test (P, bmask[cpu_mode]); 
            if (btyp) {
                if (btyp & SWMASK ('E'))                /* unqualified breakpoint? */
                    reason = STOP_IBKPT;                /* stop simulation */
                else if (btyp & BRK_TYP_DYN_STEPOVER)   /* stepover breakpoint? */
                    reason = STOP_DBKPT;                /* stop simulation */
                else switch (btyp) {                    /* qualified breakpoint */
                    case SWMASK ('M'):                  /* monitor mode */
                        reason = STOP_MBKPT;            /* stop simulation */
                        break;
                    case SWMASK ('N'):                  /* normal (SDS 930) mode */
                        reason = STOP_NBKPT;            /* stop simulation */
                        break;
                    case SWMASK ('U'):                  /* user mode */
                        reason = STOP_UBKPT;            /* stop simulation */
                        break;
                    }
                sim_interval++;                         /* don't count non-executed instruction */
                break;
                }
            }
        trap_P = save_P = P;                            /* set backups for fetch */
        reason = Read (&P, &inst);                       /* get instr */
        P = (P + 1) & VA_MASK;                          /* incr PC */
        if (reason == SCPE_OK) {                        /* fetch ok? */
            ion_defer = 0;                              /* clear ion */
            if (hst_lnt)
                inst_hist (C, save_P, HIST_XCT);
            reason = one_inst (C, save_P, cpu_mode, &trap_P); /* exec inst */
            Read (&P, (uint16 *) &C);
            if (reason > 0) {                           /* stop code? */
                if (reason != STOP_HALT) {
                    P = save_P;
                    Read (&P, (uint16 *) &C);
                }
                if (reason == STOP_IONRDY)
                    reason = 0;
                }
            }                                           /* end if r == 0 */
        if (reason < 0) {                               /* mm (fet or ex)? */
            int8 op;
            pa = -reason;                               /* get vector */
            if (reason == MM_MONUSR)                    /* record P of user-mode */
                save_P = P;                             /*  transition point     */
            tinst = ReadP (pa);                         /* get inst */
            op = I_GETOP (tinst);
            if (op != BRM && op != BRU) {               /* not BRM or BRU? */
                reason = STOP_TRPINS;                   /* fatal err */
                break;
                }
            save_mode = cpu_mode;                       /* save mode */
            cpu_mode = MON_MODE;                        /* switch to mon */
            mon_usr_trap = 0;
            if (hst_lnt)
                inst_hist (tinst, save_P, HIST_TRP);
            
            /* Use previously recorded trap address if memory acccess trap.
               Will differ from save_P if trapped instruction was a branch.
               See page 17 of 940 reference manual for additional info.
            */
            tr = one_inst (tinst, (reason == MM_NOACC)?
                  trap_P: save_P, save_mode, &tmp);     /* trap address */
            Read (&P, (uint16 *) &C);
            if (tr) {                                   /* stop code? */
                cpu_mode = save_mode;                   /* restore mode */
                P = save_P;                             /* restore PC */
                reason = (tr > 0)? tr: STOP_MMTRP;
                break;
                }
            reason = 0;                                 /* defang */
            }                                           /* end if reason */
        }                                               /* end else int */
    }                                                   /* end while */

/* Simulation halted */

pcq_r->qptr = pcq_p;                                    /* update pc q ptr */
return reason;
}

t_stat one_inst (uint16 inst, uint16 pc, uint32 mode, uint16 *trappc)
{
    uint16 opcode = (inst >> I_OPCODE_SHIFT) & 0x1F;
    uint16 addr_mode = (inst >> I_MODE_SHIFT) & 0x07;
    uint16 disp = inst & I_DISP_MASK;
    uint16 ea, data, result;
    uint16 carry, overflow;
    uint16 old_c, old_ov;
    
    *trappc = pc;
    old_c = C;
    old_ov = OV;
    
    /* ========== CLASS 0 INSTRUCTIONS ========== */
    
    switch (opcode) {
        /* Load and Store (pages 7-6 to 7-25) */
        case 0x00:  /* LDA - Load A (7-13) */
            ea = ea_class0(inst);
            A = read_word(ea);
            goto set_cc;
            
        case 0x01:  /* LDE - Load E (7-15) */
            ea = ea_class0(inst);
            E = read_word(ea);
            goto set_cc;
            
        case 0x02:  /* LDX - Load X (7-17) */
            ea = ea_class0(inst);
            X = read_word(ea);
            goto set_cc;
            
        case 0x03:  /* EOR - Exclusive OR (7-32) */
            ea = ea_class0(inst);
            A ^= read_word(ea);
            goto set_cc;
            
        case 0x04:  /* LEA - Load Effective Address (7-21) */
            ea = ea_class0(inst);
            /* In master mode, subtract G; in slave mode, G' = 0 */
            A = (ea - GPRIME) & 0xFFFF;
            goto set_cc;
            
        case 0x05:  /* ADD - Add (7-26) */
            ea = ea_class0(inst);
            carry = 0;
            A = add16(A, read_word(ea), &carry, &overflow);
            C = carry ? CARRY_BIT : 0;
            OV = overflow ? OVERFLOW_BIT : 0;
            goto set_cc;
            
        case 0x06:  /* SUB - Subtract (7-28) */
            ea = ea_class0(inst);
            carry = 0;
            A = sub16(A, read_word(ea), &carry, &overflow);
            C = carry ? CARRY_BIT : 0;
            OV = overflow ? OVERFLOW_BIT : 0;
            goto set_cc;
            
        case 0x07:  /* IOR - Inclusive OR (7-31) */
            ea = ea_class0(inst);
            A |= read_word(ea);
            goto set_cc;
            
        case 0x08:  /* DIV - Divide (7-30) (optional) */
            if (mode == 2) return MM_PRVINS;
            ea = ea_class0(inst);
            data = read_word(ea);
            if (div32(E, A, data, &A, &E) != 0) {
                OV = OVERFLOW_BIT;
            }
            goto set_cc;
            
        case 0x09:  /* AND - Logical AND (7-33) */
            ea = ea_class0(inst);
            A &= read_word(ea);
            goto set_cc;
            
        case 0x0A:  /* CPS - Compare String (7-71) (optional) */
            /* Complex string operation */
            break;
            
        case 0x0B:  /* CMP - Compare (7-34) */
            ea = ea_class0(inst);
            data = read_word(ea);
            sub16(A, data, &carry, &overflow);
            C = carry ? CARRY_BIT : 0;
            OV = overflow ? OVERFLOW_BIT : 0;
            /* C=1 if A == data, C=0 if A > data, OV=1 if A < data */
            break;
            
        case 0x0C:  /* MUL - Multiply (7-29) */
            ea = ea_class0(inst);
            data = read_word(ea);
            mul32(A, data, &E, &A);
            goto set_cc;
            
        case 0x0D:  /* LBL - Load Byte Left (7-8) */
            ea = ea_class0(inst);
            data = read_word(ea);
            A = (A & 0x00FF) | (data & 0xFF00);
            goto set_cc;
            
        case 0x0E:  /* LBR - Load Byte Right (7-10) */
            ea = ea_class0(inst);
            data = read_word(ea);
            A = (data & 0x00FF) | ((data << 8) & 0xFF00);
            goto set_cc;
            
        case 0x0F:  /* LBX - Load Byte Right into X (7-12) */
            ea = ea_class0(inst);
            data = read_word(ea);
            X = (data & 0x00FF) | ((data << 8) & 0xFF00);
            X &= 0xFFFF;
            goto set_cc;
            
        case 0x10:  /* DLD - Double Load (7-24) */
            ea = ea_class0(inst);
            E = read_word(ea);
            A = read_word((ea + 2) & 0xFFFF);
            goto set_cc;
            
        case 0x11:  /* STA - Store A (7-14) */
            ea = ea_class0(inst);
            write_word(ea, A);
            break;
            
        case 0x12:  /* STE - Store E (7-16) */
            ea = ea_class0(inst);
            write_word(ea, E);
            break;
            
        case 0x13:  /* STX - Store X (7-18) */
            ea = ea_class0(inst);
            write_word(ea, X);
            break;
            
        case 0x14:  /* SBL - Store Byte Left (7-9) */
            ea = ea_class0(inst);
            data = read_word(ea);
            write_word(ea, (data & 0x00FF) | (A & 0xFF00));
            break;
            
        case 0x15:  /* SBR - Store Byte Right (7-11) */
            ea = ea_class0(inst);
            data = read_word(ea);
            write_word(ea, (data & 0xFF00) | ((A << 8) & 0x00FF));
            break;
            
        case 0x16:  /* DST - Double Store (7-25) */
            ea = ea_class0(inst);
            write_word(ea, E);
            write_word((ea + 2) & 0xFFFF, A);
            break;
            
        case 0x17:  /* ADM - Add to Memory (7-27) */
            ea = ea_class0(inst);
            data = read_word(ea);
            carry = 0;
            result = add16(data, A, &carry, &overflow);
            write_word(ea, result);
            A = result;
            C = carry ? CARRY_BIT : 0;
            OV = overflow ? OVERFLOW_BIT : 0;
            goto set_cc;
            
        case 0x18:  /* SPA - Store Program Address (7-22) */
            ea = ea_class0(inst);
            A = (pc + GPRIME) & 0xFFFF;
            write_word(ea, A);
            break;
            
        case 0x19:  /* STS - Store Selective (7-23) */
            ea = ea_class0(inst);
            data = read_word(ea);
            result = (data & ~E) | (A & E);
            write_word(ea, result);
            goto set_cc;
            
        /* ========== CLASS 1 INSTRUCTIONS ========== */
        
        case 0x1A:  /* FAD - Float Add (optional) */
        case 0x1B:  /* FSU - Float Subtract (optional) */
        case 0x1C:  /* FMU - Float Multiply (optional) */
        case 0x1D:  /* FDV - Float Divide (optional) */
            break;
            
        case 0x1E:  /* TRS - Translate String (optional) */
        case 0x1F:  /* MVS - Move String (optional) */
            break;
            
        /* ========== SHIFT INSTRUCTIONS (page 7-38) ========== */
        
        case 0x30:  /* SHR - Shift Register (basic) */
        case 0xE0:  /* SHR in PX mode */
        case 0xF0:  /* SHR in p mode */
            /* Shift count and type in displacement */
            break;
            
        case 0x31:  /* SRG - Set Register (basic) */
        case 0xE1:  /* SRG in PX mode */
        case 0xF1:  /* SRG in p mode */
            /* Inter-register operations (XAE, XAX, XEX, etc.) */
            switch (disp & 0x1F) {
                case 0x02:  /* XAE - Exchange A and E */
                    data = A; A = E; E = data; break;
                case 0x04:  /* XAX - Exchange A and X */
                    data = A; A = X; X = data; break;
                case 0x06:  /* XEX - Exchange E and X */
                    data = E; E = X; X = data; break;
                case 0x08:  /* XAA - Exchange left/right bytes of A */
                    A = ((A & 0xFF) << 8) | ((A >> 8) & 0xFF);
                    break;
                case 0x0A:  /* CCE - Complement E */
                    E = ~E & 0xFFFF;
                    break;
                case 0x0E:  /* ACE - Add Carry to E */
                    E = (E + (C ? 1 : 0)) & 0xFFFF;
                    break;
                case 0x10:  /* CCA - Complement A */
                    A = ~A & 0xFFFF;
                    break;
                case 0x12:  /* AEE - A XOR E */
                    A ^= E;
                    break;
                case 0x14:  /* CNX - Complement X (2's complement) */
                    X = (~X + 1) & 0xFFFF;
                    break;
                case 0x16:  /* AIE - A OR E */
                    A |= E;
                    break;
                case 0x18:  /* AAE - A AND E */
                    A &= E;
                    break;
                case 0x1A:  /* LNE - Load Negative 1 into E */
                    E = 0xFFFF;
                    break;
                case 0x1C:  /* CNA - Complement A (2's complement) */
                    A = (~A + 1) & 0xFFFF;
                    break;
                case 0x1E:  /* CHX - Compute Half X (divide by 2) */
                    X = (X >> 1) | (X & 0x8000);
                    break;
            }
            goto set_cc;
            
        case 0x32:  /* ICX - Increment X (7-35) */
            X = (X + disp) & 0xFFFF;
            goto set_cc;
            
        case 0x33:  /* DCX - Decrement X (7-36) */
            X = (X - disp) & 0xFFFF;
            goto set_cc;
            
        case 0x35:  /* ICL - Increment L (7-37) */
            L = (L + disp) & 0xFFFF;
            break;
            
        case 0x36:  /* DCL - Decrement L (7-37) */
            L = (L - disp) & 0xFFFF;
            break;
            
/*        case 0x37:  /* CSV - Call Supervisor (7-94) *
            if (mode == 0) return MM_PRVINS;
            /* Store context in TWB *
            write_word(G, P - GPRIME);
            write_word(G + 2, L - GPRIME);
            write_word(G + 4, (MS << 15) | (PR << 14) | (MA << 13) | (OV << 1) | C);
            MS = 1;
            PR = 1;
            /* Load called section context from PRT at addr 12 *
            ea = disp;
            G = read_word(12) & 0xFFFF;
            L = read_word(G - 4 * ea + 2) + G;
            P = read_word(G - 4 * ea) + G;
            return SCPE_OK;
*/
//->
    case 0x37:  /* CSV – Call Supervisor (7-94) */
        /* Check if this is a known I/O supervisor section */
        if (disp == M_10_SECTION) {          /* M:1O section number = ? */
            /* A-register contains CB address relative to G */
            uint32 cb_addr = (MS ? G + A : A) & 0xFFFF;
            return io_csv_1o(cb_addr, 0);
        } else if (disp == M_ZIO_SECTION) {  /* M:ZIO section number */
            uint32 cb_addr = (MS ? G + A : A) & 0xFFFF;
            return io_csv_1o(cb_addr, 1);
        } else if (disp == M_WAIT_SECTION) { /* M:WAIT */
            uint32 cb_addr = (MS ? G + A : A) & 0xFFFF;
            return io_csv_wait(cb_addr, 0);
        } else if (disp == M_ZWAT_SECTION) { /* M:ZWAT */
            uint32 cb_addr = (MS ? G + A : A) & 0xFFFF;
            return io_csv_wait(cb_addr, 1);
        }
//<-
            
        case 0x38:  /* CLS - Call Section (7-90) */
            if (mode == 0) return MM_PRVINS;
            ea = disp;
            /* Store return address in called section's LDS */
            write_word(L, P - GPRIME);
            write_word(L + 2, L - GPRIME);
            /* Load called section context from PRT */
            L = read_word(G - 4 * ea + 2) + G;
            P = read_word(G - 4 * ea) + G;
            return SCPE_OK;
            
        case 0x39:  /* LDR - Load Register (7-19) */
            ea = disp;
            A = read_word(ea);
            goto set_cc;
            
        case 0x3A:  /* STR - Store Register (7-20) (privileged) */
            if (mode != 1) return MM_PRVINS;
            ea = disp;
            write_word(ea, A);
            break;
            
        case 0x3B:  /* LDP - Load Protection (7-105) (optional, privileged) */
            if (mode != 1) return MM_PRVINS;
            ea = disp;
            PR = read_word(ea) & 1;
            break;
            
        case 0x3C:  /* SHC - Shift Special (7-49) */
        case 0xEC: case 0xFC:
            break;
            
        case 0x3D:  /* TES - Test and Set (7-102) (privileged) */
            if (mode != 1) return MM_PRVINS;
            ea = disp;
            A = read_word(ea);
            write_word(ea, 0);
            goto set_cc;
            
        /* SYS instructions (STM, CLM, RD, WD, DIT) */
/*
        case 0xF4:  /* SYS family *
            switch (disp) {
                case 0x00:  /* RTS - Return Section (7-93) *
                    P = read_word(L + 2) + GPRIME;
                    L = read_word(L) + GPRIME;
                    return SCPE_OK;
                case 0x01:  /* DIT - Deactivate Interrupt (7-97) (privileged) */
                    if (mode != 1) return MM_PRVINS;
                    /* Complex context switching *
                    break;
                case 0x02:  /* RD - Read Direct (7-104) (privileged) *
                    break;
                case 0x03:  /* WD - Write Direct (7-104) (privileged) *
                    break;
                case 0x08:  /* STM - Set Interrupt Mask (7-103) (privileged) *
                    if (mode != 1) return MM_PRVINS;
                    MA = 1;
                    break;
                case 0x0C:  /* CLM - Clear Interrupt Mask (7-103) (privileged) *
                    if (mode != 1) return MM_PRVINS;
                    MA = 0;
                    break;
                case 0x20:  /* DITR - Deactivate High-Speed Interrupt (7-100) (optional) *
                    break;
                case 0x40:  /* RSV - Return Supervisor (7-96) (privileged) *
                    if (mode != 1) return MM_PRVINS;
                    C = read_word(G + 4) & 1;
                    OV = (read_word(G + 4) >> 1) & 1;
                    MA = (read_word(G + 4) >> 13) & 1;
                    PR = (read_word(G + 4) >> 14) & 1;
                    MS = (read_word(G + 4) >> 15) & 1;
                    L = read_word(G + 2) + G;
                    P = read_word(G) + G;
                    return SCPE_OK;
            } 
*/
//->
   case 0xF4:  /* SYS family – includes RD, WD, DIT, DITR, etc. */
        switch (disp) {
		    case 0x02: /* RD – Read Direct */
		        if (mode != 1) return MM_PRVINS;
		        /* E-register defines read mode and controller address */
		        return io_rd(E, &A);   /* result in A */
		    case 0x03: /* WD – Write Direct */
		        if (mode != 1) return MM_PRVINS;
		        return io_wd(E, A);
		    case 0x01: /* DIT – Deactivate Interrupt */
		        if (mode != 1) return MM_PRVINS;
		        return io_dit();
		    case 0x20: /* DITR – High‑speed DIT */
		        if (mode != 1) return MM_PRVINS;
		        return io_ditr();
	//<-
		        break;
		       
		    default:
                            /* Class 2 Branch Instructions (opcodes 0xC0-0xDF) */
                            if (opcode >= 0x18 && opcode <= 0x1F) {
                                /* Class 2 instructions handled by opcode range */
                            }
		        break;
		 	}
 }   /* end of switch(opcode) */
    return SCPE_OK;
//->    
/* Check for pending interrupts */
if (int_req && !ion_defer && (int_req & ~MA)) {
    io_interrupt_dispatch();
    /* The dispatch will load context and set P, etc. */
}
//<-
    
set_cc:
    /* Set condition codes based on A (manual page 7-8, 7-13, etc.) */
    if (A == 0) {
        C = CARRY_BIT;
        OV = 0;
    } else if (A & 0x8000) {
        C = 0;
        OV = OVERFLOW_BIT;
    } else {
        C = 0;
        OV = 0;
    }
    P = (pc + 2) & 0xFFFF;
    return SCPE_OK;
}

uint32 AddM24 (uint32 s1, uint32 s2)
{
uint32 t = s1 + s2;                                     /* add */
if (((s1 ^ ~s2) & (s1 ^ t)) & SIGN)                     /* overflow */
    OV = 1;
return t & DMASK;
}

/* Read word from virtual address */

t_stat Read (uint16 *va, uint16 *dat)
{
    if ((uint) va >= MEMSIZE) 
		{
		return SCPE_NXM;
		}
    *dat = M[(uint) va];
    return SCPE_OK;
}

/* Write word to virtual address */

t_stat Write (uint16 va, uint16 dat)
{
    if (va >= MEMSIZE) 
		{
		return SCPE_NXM;
		}
    M[va] = dat & DMASK;
    return SCPE_OK;
}

void set_dyn_map (void)
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
if (mon_map[6] == 0)
    mon_map[6] = MAP_PROT;
if (mon_map[7] == 0)
    mon_map[7] = MAP_PROT;
return;
}

/* Recalculate api requests */

uint32 api_findreq (void)
{
uint32 i, t;

t = (int_req & ~1) & api_mask[api_lvlhi];               /* unmasked int */
for (i = 31; t && (i > 0); i--) {                       /* find highest */
    if ((t >> i) & 1)
        return i;
    }
return 0;                                               /* none */
}

/* FIXME Relocate addr for console access */

uint32 RelocC (int32 va, int32 sw)
{
return va;                                               
}

/* Memory examine */

t_stat cpu_ex (t_value *vptr, t_addr addr, UNIT *uptr, int32 sw)
{
uint32 pa;

pa = RelocC (addr, sw);
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

pa = RelocC (addr, sw);
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
// FIXME MEMSIZE = val;
for (i = MEMSIZE; i < MAXMEMSIZE; i++)
    M[i] = 0;
return SCPE_OK;
}

/* Set system type (1 = Genie, 0 = standard) */

t_stat cpu_set_type (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
extern t_stat drm_reset (DEVICE *dptr);
extern DEVICE drm_dev, mux_dev, muxl_dev;
extern UNIT drm_unit, mux_unit;
extern DIB mux_dib;

if ((cpu_unit.flags & UNIT_GENIE) == (uint32) val)
    return SCPE_OK;
if ((drm_unit.flags & UNIT_ATT) ||                      /* attached? */
    (mux_unit.flags & UNIT_ATT))                        /* can't do it */
    return SCPE_NOFNC;
if (val) {                                              /* Genie? */
    drm_dev.flags = drm_dev.flags & ~DEV_DIS;           /* enb drum */
    mux_dev.flags = mux_dev.flags & ~DEV_DIS;           /* enb mux */
    muxl_dev.flags = muxl_dev.flags & ~DEV_DIS;
    mux_dib.dev = DEV3_GMUX;                            /* Genie mux */
    }
else {
    drm_dev.flags = drm_dev.flags | DEV_DIS;            /* dsb drum */
    mux_dib.dev = DEV3_SMUX;                            /* std mux */
    return drm_reset (&drm_dev);
    }
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
uint16 op, dat, val, va;
t_stat r;

op = I_GETOP (inst);                                    /* get opcode */
if (op == MIN)                                          /* incr */
    val = 1;
else if (op == SKR)                                     /* decr */
    val = DMASK;
else return STOP_RTCINS;                                /* can't do it */
if ((r = Ea (inst, &va)))                               /* decode eff addr */
    return r;
if ((r = Read (&va, &dat)))                              /* get operand */
    return r;
dat = AddM24 (dat, val);                                /* mem +/- 1 */
if ((r = Write (va, dat)))                              /* rewrite */
    return r;
if ((op == MIN && dat == 0) || (dat & SIGN))            /* set clk sync int */
    int_req = int_req | INT_RTCS;
return SCPE_OK;
}

// FIXME
t_stat Ea (uint32 inst, uint16 *va)
{
    uint16 opcode = (inst >> I_OPCODE_SHIFT) & 0x1F;
    uint16 mode = (inst >> I_MODE_SHIFT) & 0x07;
    uint16 disp = inst & I_DISP_MASK;
    uint32 addr;

    /* Class 0 instructions use ea_class0, Class 2 use ea_class2 */
    /* The original Mitra uses addressing mode bits to distinguish */
    if (mode & 0x04) {
        /* Class 2 style (modes 4-7) – Relative or special */
        addr = ea_class2(inst);
    } else {
        /* Class 0 style (modes 0-3) */
        addr = ea_class0(inst);
    }
    *va = addr;
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


/* Record history */

void inst_hist (uint32 c, uint32 pc, uint32 tp)
{
if (cpu_mode == hst_exclude)
    return;
hst_p = (hst_p + 1);                                    /* next entry */
if (hst_p >= hst_lnt)
    hst_p = 0;
hst[hst_p].typ = tp | (OV << 4) | (cpu_mode << 5);

// High level registers
hst[hst_p].P = pc;
hst[hst_p].A = A;
hst[hst_p].E = E;
hst[hst_p].X = X;
hst[hst_p].L = L;
hst[hst_p].G = G;
// Microprocessor registers (page 11-4)
hst[hst_p].S = S;
hst[hst_p].U = U;
hst[hst_p].V = V;
hst[hst_p].W = W;
hst[hst_p].MREG = MREG;

hst[hst_p].ea = HIST_NOEA;
return;
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

t_stat cpu_show_hist (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
int32 ov, k, di, lnt;
CONST char *cptr = (CONST char *) desc;
t_stat r;
InstHistory *h;
static const char *cyc[] = { "   ", "   ", "INT", "TRP" };
static const char *modes = "NMU?";

if (hst_lnt == 0)                                       /* enabled? */
    return SCPE_NOFNC;
if (cptr) {
    lnt = (int32) get_uint (cptr, 10, hst_lnt, &r);
    if ((r != SCPE_OK) || (lnt == 0))
        return SCPE_ARG;
    }
else lnt = hst_lnt;
di = hst_p - lnt;                                       /* work forward */
if (di < 0)
    di = di + hst_lnt;
fprintf (st, "CYC PC    MD OV A        B        X        EA      C\n\n");
for (k = 0; k < lnt; k++) {                             /* print specified */
    h = &hst[(++di) % hst_lnt];                         /* entry pointer */
    if (h->typ) {                                       /* instruction? */
        ov = (h->typ >> 4) & 1;                         /* overflow */
        fprintf (st, "%s %05o %c  %o  %08o %08o %08o ", cyc[h->typ & 3],
            h->P, modes[(h->typ >> 5) & 3], ov, h->A, h->E, h->X, h->L, h->G);
        if (h->ea & HIST_NOEA)
            fprintf (st, "      ");
// FIXME        else fprintf (st, "%05o ", h->ea);
// FIXME        sim_eval[0] = h->c;
// FIXME        if ((fprint_sym (st, h->P, sim_eval, &cpu_unit, SWMASK ('M'))) > 0)
// FIXME            fprintf (st, "(undefined) %08o", h->c);
        fputc ('\n', st);                               /* end line */
        }                                               /* end else instruction */
    }                                                   /* end for */
return SCPE_OK;
}

