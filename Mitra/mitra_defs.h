/* mitra_defs.h: Mitra-15 simulator definitions

   Copyright (c) 2001-2020, Robert M. Supnik
   Copyright (c) 2026, Jean-Pierre Le Rouzic

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

   09-Nov-20    RMS     Added definitions for card reader/punch (Ken Rector)
   22-May-10    RMS     Added check for 64b definitions
   25-Apr-03    RMS     Revised for extended file support
*/

#ifndef SDS_DEFS_H_
#define SDS_DEFS_H_    0

#include "sim_defs.h"                                   /* simulator defns */

#if defined(USE_INT64) || defined(USE_ADDR64)
#error "Mitra 15 does not support 64b values!"
#endif

/* Minibus device slots */
#define MAX_DEVICES 32
typedef struct {
    int         used;
    int         oplabel;
    int         handler_id;
    int         unit;
    uint32      cb_addr;
    int         zio;
    uint32      buffer_addr;
    uint32      bytes_left;
    uint32      extra_info;
    int         cmd;
    int         status;
    int         eor;
    int         waiting;
    uint32      wait_task;
    uint32      intr_level;
    uint32      timeout;
    int         active;
    uint8       indicators;
    void        *priv;      /* private data for device */
} MINIBUS_DEV;

/* Simulator stop codes */
#define STOP_IONRDY     1                               /* I/O dev not ready */
#define STOP_HALT       2                               /* HALT */
#define STOP_IBKPT      3                               /* breakpoint */
#define STOP_INVDEV     4                               /* invalid dev */
#define STOP_INVINS     5                               /* invalid instr */
#define STOP_INVIOP     6                               /* invalid I/O op */
#define STOP_INDLIM     7                               /* indirect limit */
#define STOP_EXULIM     8                               /* EXU limit */
#define STOP_MMINT      9                               /* mm in intr */
#define STOP_MMTRP      10                              /* mm in trap */
#define STOP_TRPINS     11                              /* trap inst not BRM or BRU */
#define STOP_RTCINS     12                              /* rtc inst not MIN or SKR */
#define STOP_ILLVEC     13                              /* zero vector */
#define STOP_CCT        14                              /* runaway CCT */
#define STOP_MBKPT      15                              /* monitor-mode breakpoint */
#define STOP_NBKPT      16                              /* normal-mode breakpoint */
#define STOP_UBKPT      17                              /* user-mode breakpoint */
#define STOP_DBKPT      18                              /* step-over (dynamic) breakpoint */

/* Trap codes */
#define MM_PRVINS       -040                            /* privileged */
#define MM_NOACC        -041                            /* no access */
#define MM_WRITE        -043                            /* write protect */
#define MM_MONUSR       -044                            /* mon to user */

/* Conditional error returns */
#define CRETINS         return ((stop_invins)? STOP_INVINS: SCPE_OK)
#define CRETDEV         return ((stop_invdev)? STOP_INVDEV: SCPE_OK)
#define CRETIOP         return ((stop_inviop)? STOP_INVIOP: SCPE_OK)
#define CRETIOE(f,c)    return ((f)? c: SCPE_OK)

/* Architectural constants */

#define SIGN            040000000                       /* sign */
#define DMASK           0177777                         /* data mask */
#define EXPS            0400                            /* exp sign */
#define EXPMASK         0777                            /* exp mask */
#define SXT(x)          ((int32) (((x) & SIGN)? ((x) | ~DMASK): \
                        ((x) & DMASK)))
#define SXT_EXP(x)      ((int32) (((x) & EXPS)? ((x) | ~EXPMASK): \
                        ((x) & EXPMASK)))

/* CPU modes */

#define NML_MODE        0
#define MON_MODE        1
#define USR_MODE        2
#define BAD_MODE        3

/* Memory */

#define MAXMEMSIZE      (1 << 16)                       /* max memory size */
#define PAMASK          (MAXMEMSIZE - 1)                /* physical addr mask */
// #define MEMSIZE         (cpu_unit.capac)                /* actual memory size */
#define MEMSIZE         32768                /* actual memory size */
#define MEM_ADDR_OK(x)  (((uint32) (x)) < MEMSIZE)
#define ReadP(x)        M[x]
#define WriteP(x,y)     if (MEM_ADDR_OK (x)) M[x] = y

/* Virtual addressing */

#define VA_SIZE         (1 << 14)                       /* virtual addr size */
#define VA_MASK         (VA_SIZE - 1)                   /* virtual addr mask */
#define VA_V_PN         11                              /* page number */
#define VA_M_PN         07
#define VA_GETPN(x)     (((x) >> VA_V_PN) & VA_M_PN)
#define VA_POFF         ((1 << VA_V_PN) - 1)            /* offset */
#define VA_USR          (I_USR)                         /* user flag in addr */
#define XVA_MASK        (VA_USR | VA_MASK)

/* Arithmetic */

#define TSTS(x)         ((x) & SIGN)
#define NEG(x)          (-((int32) (x)) & DMASK)
#define ABS(x)          (TSTS (x)? NEG(x): (x))

/* Memory map */

#define MAP_PROT        (040 << VA_V_PN)                /* protected */
#define MAP_PAGE        (037 << VA_V_PN)                /* phys page number */

/* Instruction format */

#define I_USR           (1 << 23)                       /* user */
#define I_IDX           (1 << 22)                       /* indexed */
#define I_POP           (1 << 21)                       /* programmed op */
#define I_V_TAG         21                              /* tag */
#define I_V_OP          15                              /* opcode */
#define I_M_OP          077
#define I_GETOP(x)      (((x) >> I_V_OP) & I_M_OP)
#define I_IND           (1 << 14)                       /* indirect */
#define I_V_SHFOP       11                              /* shift op */
#define I_M_SHFOP       07
#define I_GETSHFOP(x)   (((x) >> I_V_SHFOP) & I_M_SHFOP)
#define I_SHFMSK        0777                            /* shift count */
#define I_V_IOMD        12                              /* IO inst mode */
#define I_M_IOMD        03
#define I_GETIOMD(x)    (((x) >> I_V_IOMD) & I_M_IOMD)
#define I_V_SKCND       7                               /* SKS skip cond */
#define I_M_SKCND       037
#define I_GETSKCND(x)   (((x) >> I_V_SKCND) & I_M_SKCND)
#define I_EOB2          000400000                       /* chan# bit 2 */
#define I_SKB2          000040000                       /* skschan# bit 2 */
#define I_EOB1          020000000                       /* chan# bit 1 */
#define I_EOB0          000000100                       /* chan# bit 0 */
#define I_GETEOCH(x)    ((((x) & I_EOB2)? 4: 0) | \
                        (((x) & I_EOB1)? 2: 0) | \
                        (((x) & I_EOB0)? 1: 0))
#define I_SETEOCH(x)    ((((x) & 4)? I_EOB2: 0) | \
                        (((x) & 2)? I_EOB1: 0) | \
                        (((x) & 1)? I_EOB0: 0))
#define I_GETSKCH(x)    ((((x) & I_SKB2)? 4: 0) | \
                        (((x) & I_EOB1)? 2: 0) | \
                        (((x) & I_EOB0)? 1: 0))
#define I_SETSKCH(x)    ((((x) & 4)? I_SKB2: 0) | \
                        (((x) & 2)? I_EOB1: 0) | \
                        (((x) & 1)? I_EOB0: 0))

/* Globally visible flags */

#define UNIT_V_GENIE    (UNIT_V_UF + 0)
#define UNIT_GENIE      (1 << UNIT_V_GENIE)

/* Timers */

#define TMR_RTC         0                               /* clock */
#define TMR_MUX         1                               /* mux */

/* I/O routine functions */

#define IO_CONN         0                               /* connect */
#define IO_EOM1         1                               /* EOM mode 1 */
#define IO_DISC         2                               /* disconnect */
#define IO_READ         3                               /* read */
#define IO_WRITE        4                               /* write */
#define IO_WREOR        5                               /* write eor */
#define IO_SKS          6                               /* skip signal */

/* Dispatch template */

struct sdsdspt {
    uint32      num;                                    /* # entries */
    uint32      off;                                    /* offset from base */
    };

typedef struct sdsdspt DSPT;

/* Device information block */

struct sdsdib {
    int32       chan;                                   /* channel */
    int32       dev;                                    /* base dev no */
    int32       xfr;                                    /* xfer flag */
    DSPT        *tplt;                                  /* dispatch templates */
    t_stat      (*iop) (uint32 fnc, uint32 dev, uint32 *dat);
    };

typedef struct sdsdib DIB;

/* I/O control EOM */

#define CHC_REV         04000                           /* reverse */
#define CHC_NLDR        02000                           /* no leader */
#define CHC_BIN         01000                           /* binary */
#define CHC_V_CPW       7                               /* char/word */
#define CHC_M_CPW       03
#define CHC_GETCPW(x)   (((x) >> CHC_V_CPW) & CHC_M_CPW)

/* Buffer control (extended) EOM */

#define CHM_CE          04000                           /* compat/ext */
#define CHM_ER          02000                           /* end rec int */
#define CHM_ZC          01000                           /* zero wc int */
#define CHM_V_FNC       7                               /* term func */
#define CHM_M_FNC       03
#define CHM_GETFNC(x)   (((x) & CHM_CE)? (((x) >> CHM_V_FNC) & CHM_M_FNC): CHM_COMP)
#define  CHM_IORD       0                               /* record, disc */
#define  CHM_IOSD       1                               /* signal, disc */
#define  CHM_IORP       2                               /* record, proc */
#define  CHM_IOSP       3                               /* signal, proc */
#define  CHM_COMP       5                               /* compatible */
#define  CHM_SGNL       1                               /* signal bit */
#define  CHM_PROC       2                               /* proceed bit */
#define CHM_V_HMA       5                               /* hi mem addr */
#define CHM_M_HMA       03
#define CHM_GETHMA(x)   (((x) >> CHM_V_HMA) & CHM_M_HMA)
#define CHM_V_HWC       0                               /* hi word count */
#define CHM_M_HWC       037
#define CHM_GETHWC(x)   (((x) >> CHM_V_HWC) & CHM_M_HWC)

/* Channel flags word */

#define CHF_ERR         00001                           /* error */
#define CHF_IREC        00002                           /* interrecord */
#define CHF_ILCE        00004                           /* interlace */
#define CHF_DCHN        00010                           /* data chain */
#define CHF_EOR         00020                           /* end of record */
#define CHF_12B         00040                           /* 12 bit mode */
#define CHF_24B         00100                           /* 24 bit mode */
#define CHF_OWAK        00200                           /* output wake */
#define CHF_SCAN        00400                           /* scan */
#define CHF_TOP         01000                           /* TOP pending */
#define CHF_N_FLG       9                               /* <= 16 */

/* Interrupts and vectors (0 is reserved), highest bit is highest priority */

#define INT_V_PWRO      31                              /* power on */
#define INT_V_PWRF      30                              /* power off */
#define INT_V_CPAR      29                              /* CPU parity err */
#define INT_V_IPAR      28                              /* IO parity err */
#define INT_V_RTCS      27                              /* clock sync */
#define INT_V_RTCP      26                              /* clock pulse */
#define INT_V_YZWC      25                              /* chan Y zero wc */
#define INT_V_WZWC      24                              /* chan W zero wc */
#define INT_V_YEOR      23                              /* chan Y end rec */
#define INT_V_WEOR      22                              /* chan W end rec */
#define INT_V_CZWC      21                              /* chan C */
#define INT_V_CEOR      20
#define INT_V_DZWC      19                              /* chan D */
#define INT_V_DEOR      18
#define INT_V_EZWC      17                              /* chan E */
#define INT_V_EEOR      16
#define INT_V_FZWC      15                              /* chan F */
#define INT_V_FEOR      14
#define INT_V_GZWC      13                              /* chan G */
#define INT_V_GEOR      12
#define INT_V_HZWC      11                              /* chan H */
#define INT_V_HEOR      10
#define INT_V_MUXR      9                               /* mux receive */
#define INT_V_MUXT      8                               /* mux transmit */
#define INT_V_MUXCO     7                               /* SDS carrier on */
#define INT_V_MUXCF     6                               /* SDS carrier off */
#define INT_V_DRM       5                               /* Genie drum */
#define INT_V_FORK      4                               /* fork */

#define INT_PWRO        (1 << INT_V_PWRO)
#define INT_PWRF        (1 << INT_V_PWRF)
#define INT_CPAR        (1 << INT_V_CPAR)
#define INT_IPAR        (1 << INT_V_IPAR)
#define INT_RTCS        (1 << INT_V_RTCS)
#define INT_RTCP        (1 << INT_V_RTCP)
#define INT_YZWC        (1 << INT_V_YZWC)
#define INT_WZWC        (1 << INT_V_WZWC)
#define INT_YEOR        (1 << INT_V_YEOR)
#define INT_WEOR        (1 << INT_V_WEOR)
#define INT_CZWC        (1 << INT_V_CZWC)
#define INT_CEOR        (1 << INT_V_CEOR)
#define INT_DZWC        (1 << INT_V_DZWC)
#define INT_DEOR        (1 << INT_V_DEOR)
#define INT_EZWC        (1 << INT_V_EZWC)
#define INT_EEOR        (1 << INT_V_EEOR)
#define INT_FZWC        (1 << INT_V_FZWC)
#define INT_FEOR        (1 << INT_V_FEOR)
#define INT_GZWC        (1 << INT_V_GZWC)
#define INT_GEOR        (1 << INT_V_GEOR)
#define INT_HZWC        (1 << INT_V_HZWC)
#define INT_HEOR        (1 << INT_V_HEOR)
#define INT_MUXR        (1 << INT_V_MUXR)
#define INT_MUXT        (1 << INT_V_MUXT)
#define INT_MUXCO       (1 << INT_V_MUXCO)
#define INT_MUXCF       (1 << INT_V_MUXCF)
#define INT_DRM         (1 << INT_V_DRM)
#define INT_FORK        (1 << INT_V_FORK)

/* Interrupt vectors (from manual) */
#define VEC_FORK        0x08
#define VEC_DRM         0x0A
#define VEC_MUXCF       0x0C
#define VEC_MUXCO       0x0E
#define VEC_MUXT        0x10
#define VEC_MUXR        0x12
#define VEC_HEOR        0x14
#define VEC_HZWC        0x16
#define VEC_GEOR        0x18
#define VEC_GZWC        0x1A
#define VEC_FEOR        0x1C
#define VEC_FZWC        0x1E
#define VEC_EEOR        0x20
#define VEC_EZWC        0x22
#define VEC_DEOR        0x24
#define VEC_DZWC        0x26
#define VEC_CEOR        0x28
#define VEC_CZWC        0x2A
#define VEC_WEOR        0x2C
#define VEC_YEOR        0x2E
#define VEC_WZWC        0x30
#define VEC_YZWC        0x32
#define VEC_RTCP        0x34
#define VEC_RTCS        0x36
#define VEC_IPAR        0x38
#define VEC_CPAR        0x3A
#define VEC_PWRF        0x3C
#define VEC_PWRO        0x3E

/* Device constants */

#define DEV_MASK        077                             /* device mask */
#define DEV_TTI         001                             /* teletype */
#define DEV_PTR         004                             /* paper tape rdr */
#define DEV_CR          006                             /* card punch */
#define DEV_MT          010                             /* magtape */
#define DEV_RAD         026                             /* fixed head disk */
#define DEV_DSK         026                             /* moving head disk */
#define DEV_TTO         041                             /* teletype */
#define DEV_PTP         044                             /* paper tape punch */
#define DEV_CP          046                             /* card punch */
#define DEV_LPT         060                             /* line printer */
#define DEV_MTS         020                             /* MT scan/erase */
#define DEV_OUT         040                             /* output flag */
#define DEV3_GDRM       004                             /* Genie drum */
#define DEV3_GMUX       001                             /* Genie mux */
#define DEV3_SMUX       (DEV_MASK)                      /* standard mux */

#define LPT_WIDTH       132                             /* line print width */
#define CCT_LNT         132                             /* car ctrl length */

/* Transfer request flags for devices (0 is reserved) */

#define XFR_V_TTI       1                               /* console */
#define XFR_V_TTO       2
#define XFR_V_PTR       3                               /* paper tape */
#define XFR_V_PTP       4
#define XFR_V_LPT       5                               /* line printer */
#define XFR_V_RAD       6                               /* fixed hd disk */
#define XFR_V_DSK       7                               /* mving hd disk */
#define XFR_V_MT0       8                               /* magtape */
#define XFR_V_CR        9                               /* card reader  */
#define XFR_V_CP        10                              /* card punch   */

#define XFR_TTI         (1 << XFR_V_TTI)
#define XFR_TTO         (1 << XFR_V_TTO)
#define XFR_PTR         (1 << XFR_V_PTR)
#define XFR_PTP         (1 << XFR_V_PTP)
#define XFR_LPT         (1 << XFR_V_LPT)
#define XFR_RAD         (1 << XFR_V_RAD)
#define XFR_DSK         (1 << XFR_V_DSK)
#define XFR_MT0         (1 << XFR_V_MT0)
#define XFR_CR          (1 << XFR_V_CR)
#define XFR_CP          (1 << XFR_V_CP)

/* PIN/POT ordinals (0 is reserved) */

#define POT_ILCY        1                               /* interlace */
#define POT_DCRY        (POT_ILCY + NUM_CHAN)           /* data chain */
#define POT_ADRY        (POT_DCRY + NUM_CHAN)           /* address reg */
#define POT_RL1         (POT_ADRY + NUM_CHAN)           /* RL1 */
#define POT_RL2         (POT_RL1 + 1)                   /* RL2 */
#define POT_RL4         (POT_RL2 + 1)                   /* RL4 */
#define POT_RADS        (POT_RL4 + 1)                   /* fhd sector */
#define POT_RADA        (POT_RADS + 1)                  /* fhd addr */
#define POT_DSK         (POT_RADA + 1)                  /* mhd sec/addr */
#define POT_SYSI        (POT_DSK + 1)                   /* sys intr */
#define POT_MUX         (POT_SYSI + 1)                  /* multiplexor */

/* Memory size */
#define MAX_MEM_WORDS 32768

/* Control Block offsets (bytes) */
#define CB_EVENT        0
#define CB_INDICATORS   1
#define CB_CMD          2
#define CB_OPLABEL      3
#define CB_ADDR_LO      4
#define CB_ADDR_HI      5
#define CB_COUNT_LO     6
#define CB_COUNT_HI     7
#define CB_ERRBR_LO     8
#define CB_ERRBR_HI     9
#define CB_EXTRA_LO     10
#define CB_EXTRA_HI     11
#define CB_TIMEOUT_LO   12
#define CB_TIMEOUT_HI   13
#define CB_INTLEV_LO    14
#define CB_INTLEV_HI    15

/* Event byte bits (CB byte 0) */
#define EV_ACTIVE      0x01   /* 1 = transfer in progress */
#define EV_ERROR       0x02   /* 1 = error/abnormal end */
#define EV_PHYSERR     0x04   /* physical error (bit 2) */
#define EV_LOGERR      0x08   /* logical error (bit 3) */
#define EV_INITERR     0x10   /* error during initialization */
#define EV_ENDERR      0x20   /* error after transfer end */
#define EV_STATUS      0xC0   /* status info present */
#define EV_EOF          0x40

/* Operational labels */
#define OPL_M_BI        1
#define OPL_M_BO        2
#define OPL_M_CI        3
#define OPL_M_OC        4
#define OPL_M_EI        5
#define OPL_M_EO        6
#define OPL_M_LO        7
#define OPL_M_LL        8
#define OPL_M_DO        9
#define OPL_M_SI        10
#define OPL_M_SL        11
#define OPL_M_UL        12
#define OPL_M_SY        13
#define OPL_M_EP        14
#define OPL_M_GI        15
#define OPL_M_GO        16

/* Trap types */
#define TRAP_INVINS     0
#define TRAP_PRVINS     1
#define TRAP_NXM        2
#define TRAP_PROT       3
#define TRAP_PARITY     4
#define TRAP_WDOG       5

/* SIMH integration 
extern int32 sim_interval;
extern int32 sim_brk_summ;
extern t_stat sim_process_event(void);
extern t_stat sim_activate(UNIT *uptr, int32 delay);
extern int32 sim_rtcn_calb(int32 freq, int32 base);
extern int32 sim_rtcn_init(int32 wait, int32 base);
extern int32 sim_poll_kbd(void);
extern void sim_putc(int ch);
extern t_bool get_yn(const char *prompt, t_bool def);
extern uint32 get_uint(const char *cptr, int32 base, uint32 max, t_stat *r);
extern uint32 sim_brk_test(t_addr addr, uint32 type);
*/

/* I/O functions */
t_stat io_csv_1o(uint32 cb_addr, int zio);
t_stat io_csv_wait(uint32 cb_addr, int zwat);
t_stat io_rd(uint16 e_reg, uint16 *data_out);
t_stat io_wd(uint16 e_reg, uint16 data);
t_stat io_dit(void);
t_stat io_ditr(void);
void io_poll_devices(void);
void io_init_system(void);
t_bool io_init(void);
void io_assign_oplabel(int oplabel, int handler_id, int unit);

/* Opcodes */

enum opcodes {
    LDA, LDE, LDX, EOR, LEA, ADD, SUB, IOR,
    DIV, AND, CPS, CMP, MUL, LBL, LBR, LBX,

    DLD, STA, STE, STX, SBL, SBR, DST, ADM,
    SPA, STS, FAD, FSU, FMU, FDV, TRS, MVS,

    SHR, SRG, ICX, DCX,   ICL, DCL, CSV,
    CLS, LDR, STR, LDP, SHC, TES,

    BCT, BRX, BOT, BCF, BAN, BAZ, BOF, BRU,
    };

/* Translation tables */
extern const int8 odd_par[64];

/*
 * I/O operations
 */
enum IOstatus {
  IO_REPLY,                             /* Device sent a reply */
  IO_REJECT,                            /* Device sent a reject */
  IO_INTERNALREJECT                     /* I/O rejected internally */
};


#endif
