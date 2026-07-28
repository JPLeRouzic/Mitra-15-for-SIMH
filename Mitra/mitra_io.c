/* sds_io.c: SDS 940 I/O simulator

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

   01-Nov-2020  RMS     Fixed overrun/underrun handling in single-word IO
   23-Oct-2020  RMS     TOP disconnects the channel rather than setting CHF_EOR
   19-Mar-2012  RMS     Fixed various declarations (Mark Pizzolato)
*/

#include "mitra_defs.h"
#include "mitra_cpu.h"
#include "mitra_io.h"

/* Data chain word */

#define CHD_INT         040                             /* int on chain */
#define CHD_PAGE        037                             /* new page # */

/* Interlace POT */

#define CHI_V_WC        14                              /* word count */
#define CHI_M_WC        01777
#define CHI_GETWC(x)    (((x) >> CHI_V_WC) & CHI_M_WC)
#define CHI_V_MA        0                               /* mem address */
#define CHI_M_MA        037777
#define CHI_GETMA(x)    (((x) >> CHI_V_MA) & CHI_M_MA)

/* System interrupt POT */

#define SYI_V_GRP       18                              /* group */
#define SYI_M_GRP       077
#define SYI_GETGRP(x)   (((x) >> SYI_V_GRP) & SYI_M_GRP)
#define SYI_DIS         (1 << 17)                       /* disarm if 0 */
#define SYI_ARM         (1 << 16)                       /* arm if 1 */
#define SYI_M_INT       0177777                         /* interrupt */

/* Pseudo-device number for EOM/SKS mode 3 */

#define I_GETDEV3(x)    ((((x) & 020046000) != 020046000)? ((x) & DEV_MASK): DEV_MASK)

#define TST_XFR(d,c)    (xfr_req & dev_map[d][c])
#define SET_XFR(d,c)    xfr_req = xfr_req | dev_map[d][c]
#define CLR_XFR(d,c)    xfr_req = xfr_req & ~dev_map[d][c]
#define INV_DEV(d,c)    (dev_dsp[d][c] == NULL)
#define VLD_DEV(d,c)    (dev_dsp[d][c] != NULL)
#define TST_EOR(c)      (chan_flag[c] & CHF_EOR)
#define QAILCE(a)       (((a) >= POT_ILCY) && ((a) < (POT_ILCY + NUM_CHAN)))

uint8 chan_uar[NUM_CHAN];                               /* unit addr */
uint16 chan_wcr[NUM_CHAN];                              /* word count */
uint16 chan_mar[NUM_CHAN];                              /* mem addr */
uint8 chan_dcr[NUM_CHAN];                               /* data chain */
uint32 chan_war[NUM_CHAN];                              /* word assembly */
uint8 chan_cpw[NUM_CHAN];                               /* char per word */
uint8 chan_cnt[NUM_CHAN];                               /* char count */
uint16 chan_mode[NUM_CHAN];                             /* mode */
uint16 chan_flag[NUM_CHAN];                             /* flags */
static const char *chname[NUM_CHAN] = {
    "W", "Y", "C", "D", "E", "F", "G", "H"
    };

extern t_value M[MAX_MEM_WORDS];                            /* memory */
extern uint32 int_req;                                  /* int req */
extern uint32 xfr_req;                                  /* xfer req */
extern uint32 alert;                                    /* pin/pot alert */
extern uint32 X, EM2, EM3, ion, bpt;
extern uint32 cpu_mode;
extern int32 rtc_pie;
extern int32 stop_invins, stop_invdev, stop_inviop;
extern uint32 mon_usr_trap;
extern UNIT cpu_unit;

t_stat chan_reset (DEVICE *dptr);
t_stat chan_read (int32 ch);
t_stat chan_write (int32 ch);
void chan_write_mem (int32 ch);
void chan_flush_war (int32 ch);
uint32 chan_mar_inc (int32 ch);
t_stat chan_eor (int32 ch);
t_stat pot_ilc (uint32 num, uint32 *dat);
t_stat pot_dcr (uint32 num, uint32 *dat);
t_stat pin_adr (uint32 num, uint32 *dat);
t_stat pot_fork (uint32 num, uint32 *dat);
t_stat dev_disc (uint32 ch, uint32 dev);
t_stat dev_wreor (uint32 ch, uint32 dev);
extern t_stat pin_dsk (uint32 num, uint32 *dat);
extern t_stat pot_dsk (uint32 num, uint32 *dat);
t_stat pin_mux (uint32 num, uint32 *dat);
t_stat pot_mux (uint32 num, uint32 *dat);

extern void dri_reset(void);
extern void sagem_reset(void);
extern void cdr_reset(void);
extern void asr33_reset(void);
extern void ptr_reset(void);
extern void ptp_reset(void);
extern void printer_reset(void);

t_stat chan_show_reg (FILE *st, UNIT *uptr, int32 val, CONST void *desc);

struct aldisp {
    t_stat      (*pin) (uint32 num, uint32 *dat);       /* altnum, *dat */
    t_stat      (*pot) (uint32 num, uint32 *dat);       /* altnum, *dat */
    };



/* ========== System Initialization ========== */
void io_init_system(void) {
    MLOG("[IO-INIT] resetting all devices: DRI, SAGEM, CDR, ASR33, PANEL, PTR, PTP, PRINTER\n");
    dri_reset();
    sagem_reset();
    cdr_reset();
    asr33_reset();
    panel_reset();
    ptr_reset();
    ptp_reset();
    printer_reset();
    MLOG("[IO-INIT] all devices reset\n");
}

t_bool io_init(void) {
    io_init_system();
    return FALSE;
}

void write_byte_io(uint32 addr, uint8 val, int zio) {
    MLOG("  [IO-MEM] write_byte_io addr=%05o zio=%d val=%03o\n", addr, zio, val);
    write_byte((uint16)addr, val);
}

t_stat read_byte_io(uint32 addr, uint8 *val, int zio) {
    *val = read_byte((uint16)addr);
    MLOG("  [IO-MEM] read_byte_io  addr=%05o zio=%d val=%03o\n", addr, zio, *val);
    return SCPE_OK;
}

/* Note: this signature (void, taking a pre-shifted bitmask + a high-speed flag) is
   inferred from how every device file calls it, e.g. io_interrupt_dispatch(int_req, false)
   with int_req already built as (1 << level). If mitra_io.h declares a different
   signature, adjust this definition to match it. */
void io_interrupt_dispatch(uint32 int_req, t_bool high_speed) {
    MLOG_INT("  [IO-INT] io_interrupt_dispatch int_req=%08x high_speed=%d\n", int_req, (int)high_speed);
    cpu_state.intrpt_mask |= int_req;
    if (high_speed) cpu_state.high_speed = TRUE;
}



