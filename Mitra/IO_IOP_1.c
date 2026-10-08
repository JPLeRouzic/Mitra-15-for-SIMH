/* mitra_sys.c: CII Mitra 15/30 Simulator SCP Interface
 * adapted from sds_sys.c
 * 
 * Copyright (c) 2026, Jean-Pierre Le Rouzic
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include "mitra_defs.h"
#include "mitra_cpu.h"
#include "mitra_io.h"

/* Channel data structures

   chan_dev     channel device descriptor
   chan_unit    channel unit descriptor
   chan_reg     channel register list
*/

UNIT chan_unit = { UDATA (NULL, 0, 0) };

REG chan_reg[] = {
    { BRDATA (UAR, chan_uar, 8, 6, NUM_CHAN) },
    { BRDATA (WCR, chan_wcr, 8, 15, NUM_CHAN) },
    { BRDATA (MAR, chan_mar, 8, 16, NUM_CHAN) },
    { BRDATA (DCR, chan_dcr, 8, 6, NUM_CHAN) },
    { BRDATA (WAR, chan_war, 8, 24, NUM_CHAN) },
    { BRDATA (CPW, chan_cpw, 8, 2, NUM_CHAN) },
    { BRDATA (CNT, chan_cnt, 8, 3, NUM_CHAN) },
    { BRDATA (MODE, chan_mode, 8, 12, NUM_CHAN) },
    { BRDATA (FLAG, chan_flag, 8, CHF_N_FLG, NUM_CHAN) },
    { NULL }
    };

MTAB chan_mod[] = {
    { MTAB_XTD | MTAB_VDV | MTAB_NMO, CHAN_W, "W", NULL,
      NULL, &chan_show_reg, NULL },
    { MTAB_XTD | MTAB_VDV | MTAB_NMO, CHAN_Y, "Y", NULL,
      NULL, &chan_show_reg, NULL },
    { MTAB_XTD | MTAB_VDV | MTAB_NMO, CHAN_C, "C", NULL,
      NULL, &chan_show_reg, NULL },
    { MTAB_XTD | MTAB_VDV | MTAB_NMO, CHAN_D, "D", NULL,
      NULL, &chan_show_reg, NULL },
    { MTAB_XTD | MTAB_VDV | MTAB_NMO, CHAN_E, "E", NULL,
      NULL, &chan_show_reg, NULL },
    { MTAB_XTD | MTAB_VDV | MTAB_NMO, CHAN_F, "F", NULL,
      NULL, &chan_show_reg, NULL },
    { MTAB_XTD | MTAB_VDV | MTAB_NMO, CHAN_G, "G", NULL,
      NULL, &chan_show_reg, NULL },
    { MTAB_XTD | MTAB_VDV | MTAB_NMO, CHAN_H, "H", NULL,
      NULL, &chan_show_reg, NULL }
    };

DEVICE chan_dev = {
    "CHAN", &chan_unit, chan_reg, chan_mod,
    1, 8, 8, 1, 8, 8,
    NULL, NULL, &chan_reset,
    NULL, NULL, NULL
    };

/* Channel read invokes the I/O device to get the next character and,
   if not end of record, assembles it into the word assembly register.
   If the interlace is on, the full word is stored in memory.
   The key difference points for the various terminal functions are

        end of record   comp: EOT interrupt
                        IORD, IOSD: EOR interrupt, disconnect
                        IORP, IOSP: EOR interrupt, interrecord
        interlace off:  comp: EOW interrupt
                        IORD, IORP: ignore
                        IOSD, IOSP: overrun error
        --wcr == 0:     comp: clear interlace
                        IORD, IORP, IOSP: ZWC interrupt
                        IOSD: ZWC interrupt, EOR interrupt, disconnect

   Note that the channel can be disconnected if CHF_EOR is set, but must
   not be if XFR_REQ is set */

t_stat chan_read (int32 ch)
{
uint32 dat = 0;
uint32 dev = chan_uar[ch] & DEV_MASK;
uint32 tfnc = CHM_GETFNC (chan_mode[ch]);
t_stat r = SCPE_OK;

if ((dev != 0) && TST_XFR (dev, ch)) {                  /* ready to xfr? */
    if (INV_DEV (dev, ch))                              /* can't read? */
        CRETIOP;
    r = dev_dsp[dev][ch] (IO_READ, dev, &dat);          /* read data */
    if ((r != 0) || (chan_cnt[ch] > chan_cpw[ch]))      /* error or overrun? */
        chan_flag[ch] = chan_flag[ch] | CHF_ERR;
    else {                                              /* no, precess data */
        if (chan_flag[ch] & CHF_24B)                    /* 24B? */
            chan_war[ch] = dat;
        else if (chan_flag[ch] & CHF_12B)               /* 12B? */
            chan_war[ch] = ((chan_war[ch] << 12) | (dat & 07777)) & DMASK;
        else chan_war[ch] = ((chan_war[ch] << 6) | (dat & 077)) & DMASK;
        }
    if (chan_flag[ch] & CHF_SCAN)                       /* scanning? */
        chan_cnt[ch] = chan_cpw[ch];                    /* never full */
    else chan_cnt[ch] = chan_cnt[ch] + 1;               /* insert char */
    if (chan_cnt[ch] > chan_cpw[ch]) {                  /* full now? */
        if (chan_flag[ch] & CHF_ILCE) {                 /* interlace on? */
            chan_write_mem (ch);                        /* write to mem */
            if (chan_wcr[ch] == 0) {                    /* wc zero? */
                chan_flag[ch] = chan_flag[ch] & ~CHF_ILCE; /* clr interlace */
                if ((tfnc != CHM_COMP) && (chan_mode[ch] & CHM_ZC))
                    int_req = int_req | int_zc[ch];     /* zwc interrupt */
                if (tfnc == CHM_IOSD) {                 /* IOSD? also EOR */
                    if (chan_mode[ch] & CHM_ER)
                        int_req = int_req | int_er[ch];
                    dev_disc (ch, dev);                 /* disconnect */
                    }                                   /* end if IOSD */
                }                                       /* end if wcr == 0 */
            }                                           /* end if ilce on */
        else {                                          /* interlace off */
            if (TST_EOR (ch))                           /* eor? */
                return chan_eor (ch);
            if (tfnc == CHM_COMP) {                     /* C: EOW, intr */
                if (ion)
                    int_req = int_req | int_zc[ch];
                }
            else if (tfnc & CHM_SGNL)                   /* Sx: error */
                chan_flag[ch] = chan_flag[ch] | CHF_ERR;
            else chan_cnt[ch] = chan_cpw[ch];           /* Rx: ignore */
            }                                           /* end else ilce */
        }                                               /* end if full */
    }                                                   /* end if xfr */
if (TST_EOR (ch)) {                                     /* end record? */
    if (tfnc == CHM_COMP)                               /* C: fill war */
        chan_flush_war (ch);
    else if (chan_cnt[ch]) {                            /* RX, CX: fill? */
        chan_flush_war (ch);                            /* fill war */
        if (chan_flag[ch] & CHF_ILCE)                   /* ilce on? store */
            chan_write_mem (ch);
        }                                               /* end else if cnt */
    return chan_eor (ch);                               /* eot/eor int */
    }
return r;
}

void chan_write_mem (int32 ch)
{
WriteP (chan_mar[ch], chan_war[ch]);                    /* write to mem */
chan_mar[ch] = chan_mar_inc (ch);                       /* incr mar */
chan_wcr[ch] = (chan_wcr[ch] - 1) & 077777;             /* decr wcr */
chan_war[ch] = 0;                                       /* reset war */
chan_cnt[ch] = 0;                                       /* reset cnt */
return;
}

void chan_flush_war (int32 ch)
{
int32 i = (chan_cpw[ch] - chan_cnt[ch]) + 1;

if (i) {
    if (chan_flag[ch] & CHF_24B)
        chan_war[ch] = 0;
    else if (chan_flag[ch] & CHF_12B)
        chan_war[ch] = (chan_war[ch] << 12) & DMASK;
    else chan_war[ch] = (chan_war[ch] << (i * 6)) & DMASK;
    chan_cnt[ch] = chan_cpw[ch] + 1;
    }
return;
}

/* Channel write gets the next character and sends it to the I/O device.
   If this is the last character in an interlace operation, the end of
   record operation is invoked.
   The key difference points for the various terminal functions are

        end of record:  comp: EOT interrupt
                        IORD, IOSD: EOR interrupt, disconnect
                        IORP, IOSP: EOR interrupt, interrecord
        interlace off:  if not end of record, EOW interrupt
        --wcr == 0:     comp: EOT interrupt, disconnect
                        IORD, IORP: ignore
                        IOSD: ZWC interrupt, disconnect
                        IOSP: ZWC interrupt, interrecord
*/
t_stat chan_write (int32 ch)
{
uint32 dat = 0;
uint32 dev = chan_uar[ch] & DEV_MASK;
uint32 tfnc = CHM_GETFNC (chan_mode[ch]);
t_stat r = SCPE_OK;

if (dev && TST_XFR (dev, ch)) {                         /* ready to xfr? */
    if (INV_DEV (dev, ch))                              /* invalid dev? */
        CRETIOP;
    if (chan_cnt[ch] == 0) {                            /* buffer empty? */
        if (chan_flag[ch] & CHF_ILCE) {                 /* interlace on? */
            chan_war[ch] = ReadP (chan_mar[ch]);
            chan_mar[ch] = chan_mar_inc (ch);           /* incr mar */
            chan_wcr[ch] = (chan_wcr[ch] - 1) & 077777; /* decr mar */
            chan_cnt[ch] = chan_cpw[ch] + 1;            /* set cnt */
            }
        else {                                          /* ilce off */
             if (TST_EOR (dev))                         /* EOR? */
                return chan_eor (ch);
            chan_flag[ch] = chan_flag[ch] | CHF_ERR;    /* rate err */
            }                                           /* end else ilce */
        }                                               /* end if cnt */
    if (chan_cnt[ch] != 0) {                            /* if not underrun */
        chan_cnt[ch] = chan_cnt[ch] - 1;                /* decr cnt */
        if (chan_flag[ch] & CHF_24B)                    /* 24B? */
            dat = chan_war[ch];
        else if (chan_flag[ch] & CHF_12B) {             /* 12B? */
            dat = (chan_war[ch] >> 12) & 07777;         /* get halfword */
            chan_war[ch] = (chan_war[ch] << 12) & DMASK;/* remove from war */
            }
        else {                                          /* 6B */
            dat = (chan_war[ch] >> 18) & 077;           /* get char */
            chan_war[ch] = (chan_war[ch] << 6) & DMASK; /* remove from war */
            }
        }                                               /* end no underrun */
    r = dev_dsp[dev][ch] (IO_WRITE, dev, &dat);         /* write */
    if (r != 0)                                         /* error? */
        chan_flag[ch] = chan_flag[ch] | CHF_ERR;
    if (chan_cnt[ch] == 0) {                            /* buf empty? */
        if (chan_flag[ch] & CHF_ILCE) {                 /* ilce on? */
            if (chan_wcr[ch] == 0) {                    /* wc now 0? */
                chan_flag[ch] = chan_flag[ch] & ~CHF_ILCE; /* ilc off */
                if (tfnc == CHM_COMP) {                 /* compatible? */
                    if (ion)
                        int_req = int_req | int_zc[ch];
                    dev_disc (ch, dev);                 /* disconnnect */
                    }                                   /* end if comp */
                else {                                  /* extended */
                    if (chan_mode[ch] & CHM_ZC)         /* ZWC int */
                        int_req = int_req | int_zc[ch];
                    if (tfnc == CHM_IOSD) {             /* SD */
                        if (chan_mode[ch] & CHM_ER)     /* EOR int */
                            int_req = int_req | int_er[ch];
                        dev_disc (ch, dev);             /* disconnnect */
                        }                               /* end if SD */
                    else if (!(tfnc && CHM_SGNL) ||     /* IORx or IOSP TOP? */
                        (chan_flag[ch] & CHF_TOP))
                        dev_disc (ch, dev);             /* R: disconnect */
                    chan_flag[ch] = chan_flag[ch] & ~CHF_TOP;
                    }                                   /* end else comp */
                }                                       /* end if wcr */
            }                                           /* end if ilce */
        else if (chan_flag[ch] & CHF_TOP) {             /* off, TOP pending? */
            chan_flag[ch] = chan_flag[ch] & ~CHF_TOP;   /* clear TOP */
            dev_disc (ch, dev);                         /* disconnect */
            }
        else if (ion)                                   /* no TOP, EOW intr */
            int_req = int_req | int_zc[ch];
        }                                               /* end if cnt */
   }                                                    /* end if xfr */
if (TST_EOR (ch))                                       /* eor rcvd? */
    return chan_eor (ch);
return r;
}

