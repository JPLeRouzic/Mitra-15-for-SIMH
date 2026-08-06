/* sds_defs.h: SDS 940 simulator definitions

   Copyright (c) 2001-2020, Robert M. Supnik

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
#error "SDS 940 does not support 64b values!"
#endif

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

/* Trap types */
#define TRAP_INVINS     0
#define TRAP_PRVINS     1
#define TRAP_NXM        2
#define TRAP_PROT       3
#define TRAP_PARITY     4
#define TRAP_WDOG       5

#define MM_PRVINS       -040                            /* privileged */
#define MM_NOACC        -041                            /* no access */
#define MM_WRITE        -043                            /* write protect */
#define MM_MONUSR       -044                            /* mon to user */
#define MM_INVINS       -044                            /* privileged */

/* Conditional error returns */

#define CRETINS         return ((stop_invins)? STOP_INVINS: SCPE_OK)
#define CRETDEV         return ((stop_invdev)? STOP_INVDEV: SCPE_OK)
#define CRETIOP         return ((stop_inviop)? STOP_INVIOP: SCPE_OK)
#define CRETIOE(f,c)    return ((f)? c: SCPE_OK)

/* Architectural constants */

#define SIGN            040000000                       /* sign */
#define DMASK           0xFFFF                       /* data mask */
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

/* Channels */

#define NUM_CHAN        8                               /* max num chan */
#define CHAN_W          0                               /* TMCC */
#define CHAN_Y          1
#define CHAN_C          2
#define CHAN_D          3
#define CHAN_E          4                               /* DACC */
#define CHAN_F          5
#define CHAN_G          6
#define CHAN_H          7

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

/* Channel function prototypes
void chan_set_flag (int32 ch, uint32 fl);
void chan_set_ordy (int32 ch);
void chan_disc (int32 ch);
void chan_set_uar (int32 ch, uint32 dev);
t_stat set_chan (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat show_chan (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat chan_process (void);
t_bool chan_testact (void);
 */
/* Translation tables */
extern const int8 odd_par[64];


#endif
