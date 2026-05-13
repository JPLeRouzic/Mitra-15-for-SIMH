/* mitra_io.h – interface for Mitra 15 I/O emulation */
#ifndef MITRA_IO_H
#define MITRA_IO_H

#include "mitra_defs.h"

/* I/O supervisor section numbers (defined by the monitor) */
#define M_10_SECTION    10   /* M:1O  – normal I/O request */
#define M_ZIO_SECTION   11   /* M:ZIO – I/O relative to ZC */
#define M_WAIT_SECTION  12   /* M:WAIT  – wait for I/O completion */
#define M_ZWAT_SECTION  13   /* M:ZWAT – wait for ZC I/O */

/* Control block (CB) layout – see manual page VIII‑6 */
#define CB_EVENT        0    /* byte 0 */
#define CB_INDICATORS   1    /* byte 1 */
#define CB_CMD          2    /* byte 2 – function code */
#define CB_OPLABEL      3    /* byte 3 – operational label */
#define CB_ADDR_LO      4    /* byte 4 (word) – buffer address (low) */
#define CB_ADDR_HI      5    /* high byte */
#define CB_COUNT_LO     6    /* byte count (low) */
#define CB_COUNT_HI     7    /* high byte */
#define CB_ERRBR_LO     8    /* error branch address (optional) */
#define CB_ERRBR_HI     9
#define CB_EXTRA_LO    10    /* extra info (e.g. disk sector) */
#define CB_EXTRA_HI    11
#define CB_TIMEOUT_LO  12    /* time‑out value (optional) */
#define CB_TIMEOUT_HI  13
#define CB_INTLEV_LO   14    /* interrupt level to trigger after transfer */
#define CB_INTLEV_HI   15

/* Event byte bits (CB byte 0) */
#define EV_ACTIVE      0x01   /* 1 = transfer in progress */
#define EV_ERROR       0x02   /* 1 = error/abnormal end */
#define EV_PHYSERR     0x04   /* physical error (bit 2) */
#define EV_LOGERR      0x08   /* logical error (bit 3) */
#define EV_INITERR     0x10   /* error during initialization */
#define EV_ENDERR      0x20   /* error after transfer end */
#define EV_STATUS      0xC0   /* status info present */

/* Command byte (CB byte 2) – see manual page VIII‑7 */
#define CMD_READ_FWD   0x00
#define CMD_READ_BWD   0x10
#define CMD_WRITE      0x20
#define CMD_WRITE_EOD  0x30
#define CMD_FORMAT     0x40
#define CMD_SKIP_FWD   0x50
#define CMD_SKIP_FILE  0x60
#define CMD_SKIP_BWD   0x70

/* Standard operational labels (manual page VIII‑11) */
#define OPL_M_BI       1   /* binary input */
#define OPL_M_BO       2   /* binary output */
#define OPL_M_CI       3   /* command input */
#define OPL_M_OC       4   /* operator console */
#define OPL_M_EI       5   /* element input */
#define OPL_M_EO       6   /* element output */
#define OPL_M_LO       7   /* listing output */
#define OPL_M_LL       8   /* listing log */
#define OPL_M_DO       9   /* diagnostic output */
#define OPL_M_SI      10   /* source input */
#define OPL_M_SL      11   /* system library */
#define OPL_M_UL      12   /* user library */
#define OPL_M_SY      13   /* system */
#define OPL_M_EP      14   /* executable programs */
#define OPL_M_GI      15   /* go input */
#define OPL_M_GO      16   /* go output */

/* External functions */
t_stat io_csv_1o(uint32 cb_addr, int zio);
t_stat io_csv_wait(uint32 cb_addr, int zwat);
t_stat io_rd(uint32 e_reg, uint16 *data_out);
t_stat io_wd(uint32 e_reg, uint32 data);
t_stat io_dit(void);
t_stat io_ditr(void);
void   io_interrupt_dispatch(void);
int    io_check_ready(void);
void   io_dev_attach(int oplabel, const char *file, int write);

#endif
