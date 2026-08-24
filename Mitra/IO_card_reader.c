/*
 * CONTROL DATA 9220 CARD READER (MITRA-15)
 *
 * Device-specific logic for RD / WD instructions.
 * Channel / DMA programming is the responsibility of the Mitra-15 I/O system;
 * this module only implements the card-reader behaviour.
 *
 * WD (E = 7)
 *   A bits 0-1 select mode:
 *     00 – binary read
 *     10 – EBCDIC read
 *     11 – reader idle / stop
 *   Before a non-idle WD the I/O system must have placed the target
 *   memory address in cdr_state.mem_addr and the byte count in
 *   cdr_state.bytes_left (and optionally the ZIO flag).
 *
 * RD (E = 17)
 *   Returns the current status word in *result and clears it.
 *   Status bits (same encoding as the original hardware):
 *     0x40  empty magazine
 *     0x80  cell errors
 *     0xC0  torn cards / jam under cells
 *     0xA0  input magazine jam
 *     0x10  cards read wrong
 *     0x08  transfer failed
 *     0x04  stop
 *     0x02  not operational
 *     0x01  EBCDIC error
 *
 * Card image file format (identical to sigma_cr.c / original cr_readrec):
 *   120-byte records, no headers.
 *   Two 12-bit columns are packed into three consecutive bytes:
 *     col 2k   = (b0 << 4) | (b1 >> 4)
 *     col 2k+1 = ((b1 & 0x0F) << 8) | b2
 *
 * Transfer is driven by cdr_poll(), which is called from the common
 * I/O poll loop.  Completion (or error) raises an interrupt.
 */

#include "mitra_defs.h"
#include "mitra_cpu.h"
#include "mitra_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define CDR_COLUMNS        80
#define CDR_BYTES_PER_CARD 120

/* Memory-access helpers (declared in mitra_cpu.h / mitra_io.h) */
extern t_value read_word(t_addr va);
extern void    write_word(t_addr va, t_value val);
extern uint8   read_byte(t_addr va);
extern void    write_byte(t_addr va, uint8 val);
extern void    write_byte_io(t_addr va, uint8 val, int zio);
extern void io_interrupt_dispatch(uint32 intr_lvl, t_bool hgh_spd);

t_stat cdr_rd(uint16 e_reg, uint16 *val);
t_stat cdr_wd(uint16 e_reg, uint16 result);

typedef struct {
    FILE   *image;              /* open card-deck file */
    uint32  hopper;             /* cards remaining in hopper */
    uint32  stacker[3];         /* 0 = normal, 1 = alt1, 2 = alt2 */
    uint32  stacker_sel;        /* selected stacker for current card */
    uint16  status;             /* last status word (cleared by RD) */
    int     mode;               /* 0 = binary, 1 = EBCDIC, 2 = idle */
    int     active;             /* non-zero while a transfer is in progress */
    uint32  buffer[CDR_COLUMNS];/* 12-bit column images */
    uint32  col;                /* column index used by binary packing */
    uint32  bptr;               /* next column to emit */
    uint32  blnt;               /* columns currently held in buffer */
    uint32  mem_addr;           /* next memory address to write */
    uint32  bytes_left;         /* remaining byte count */
    int     zio;                /* ZIO / shared-memory flag */
} CDR_DEV;

static CDR_DEV cdr_state = {0};

/* Forward declarations */
static void cdr_interrupt(void);
static int  cdr_read_card(void);
static void cdr_start_transfer(uint32 cmd);

/* ------------------------------------------------------------------ */
/* Attach / detach                                                    */
/* ------------------------------------------------------------------ */

t_stat cdr_attach(UNIT *uptr, const char *cptr)
{
    t_stat r;
    char *saved_filename = uptr->filename;

    /* Let the standard SIMH helper open the file and set UNIT_ATT,
       fileref, filename, etc. */
    uptr->filename = NULL;
    r = attach_unit(uptr, cptr);
    if (r != SCPE_OK) {
        uptr->filename = saved_filename;
        return r;
    }

    /* Card image must be an exact multiple of 120-byte records */
    long size = sim_fsize(uptr->fileref);
    if (size < 0 || (size % CDR_BYTES_PER_CARD) != 0) {
        detach_unit(uptr);
        return SCPE_IOERR;
    }

    cdr_state.image  = uptr->fileref;   /* keep a convenient alias */
    cdr_state.hopper = (uint32)(size / CDR_BYTES_PER_CARD);
    cdr_state.status = 0;
    cdr_state.active = 0;
    return SCPE_OK;
}

t_stat cdr_detach(UNIT *uptr)
{
    cdr_state.image  = NULL;
    cdr_state.hopper = 0;
    cdr_state.active = 0;
    return detach_unit(uptr);
}

/*
* A wrapper function to manage RD or WD instruction execution
* - matching dio_handler_t: t_stat xxx_dio(uint16 inst, t_bool is_write),
* - that reads cpu_state.reg_E/reg_A and 
* - calls the device's own _wd/_rd function, writing results back into cpu_state.reg_A for RD.
*/
t_stat cdr_dio_handler(uint16 inst, t_bool is_write) {
    if(is_write) {
	return cdr_wd(cpu_state.reg_E, cpu_state.reg_A);
	}
    else {
	 return cdr_rd(cpu_state.reg_E, &cpu_state.reg_A);
	 }
}

/* ------------------------------------------------------------------ */
/* Card image reading (identical packing to sigma_cr.c)               */
/* ------------------------------------------------------------------ */

static int cdr_read_card(void)
{
    uint8 data[CDR_BYTES_PER_CARD];
    if (fread(data, 1, CDR_BYTES_PER_CARD, cdr_state.image) != CDR_BYTES_PER_CARD)
        return 0;

    /* Pack two 12-bit columns from every three bytes */
    for (int col = 0; col < CDR_COLUMNS; ) {
        uint8 c1 = data[col / 2 * 3];
        uint8 c2 = data[col / 2 * 3 + 1];
        uint8 c3 = data[col / 2 * 3 + 2];
        cdr_state.buffer[col++] = ((c1 << 4) | (c2 >> 4)) & 0xFFF;
        cdr_state.buffer[col++] = (((c2 & 0x0F) << 8) | c3) & 0xFFF;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Start a transfer (called from WD after mode decode)                */
/* ------------------------------------------------------------------ */

static void cdr_start_transfer(uint32 cmd)
{
    if (cdr_state.active)
        return;                                 /* already busy */

    /* Mode bits 0-1 of the WD data word 
        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
      |     |                                         |
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    */
    uint16 mode_selector = ((cmd & 0xC000) >> 14) && 0x03;
    cdr_state.mode = ((mode_selector) == 0x02) ? 2 :
                     ((mode_selector) == 0x01) ? 1 : 0;

    if (cdr_state.mode == 2) {                  /* idle / stop */
        cdr_state.status = 0x04;
        cdr_interrupt();
        return;
    }

    if (cdr_state.hopper == 0) {
        cdr_state.status = 0x40;                /* empty magazine */
        cdr_interrupt();
        return;
    }

    if (!cdr_read_card()) {
        cdr_state.status = 0x80;                /* cell error / unexpected EOF */
        cdr_interrupt();
        return;
    }

    /* mem_addr / bytes_left / zio must already have been supplied by
       the Mitra-15 I/O system before the WD that starts the transfer. */
    cdr_state.col  = 0;
    cdr_state.bptr = 0;
    cdr_state.blnt = CDR_COLUMNS;
    cdr_state.active = 1;
    /* Transfer continues under cdr_poll() */
}

/* ------------------------------------------------------------------ */
/* Poll – called from the common I/O device poll loop                 */
/* ------------------------------------------------------------------ */

int cdr_poll(void)
{
    if (!cdr_state.active)
        return 0;

    if (cdr_state.blnt == 0) {
        /* Need another card */
        if (!cdr_read_card()) {
            cdr_state.active = 0;
            cdr_state.status = 0x80;
            cdr_interrupt();
            return 1;
        }
        cdr_state.bptr = 0;
        cdr_state.blnt = CDR_COLUMNS;
    }

    uint8 byte_out;

    if (cdr_state.mode == 1) {                  /* EBCDIC (automatic) mode */
        uint16 row_bits = cdr_state.buffer[cdr_state.bptr++];
        /* Count punches in rows 1-7 (bits 1-7 of the 12-bit column) */
        uint16 n = row_bits & 0x1FC;
        int bits = 0;
        while (n) {
            n &= n - 1;
            bits++;
        }
        if (bits > 1) {
            byte_out = 0x00;
            cdr_state.status |= 0x01;           /* EBCDIC / data error */
        } else {
            /* Placeholder – a full implementation uses a 4096-entry
               Hollerith-to-EBCDIC table (see sigma_cr.c). */
            byte_out = (uint8)(row_bits & 0xFF);
        }
    } else {                                    /* binary mode – 3 bytes / 2 columns */
        switch (cdr_state.col % 3) {
        case 0:
            byte_out = (cdr_state.buffer[cdr_state.bptr] >> 4) & 0xFF;
            break;
        case 1:
            byte_out  = (cdr_state.buffer[cdr_state.bptr] & 0x0F) << 4;
            cdr_state.bptr++;
            byte_out |= (cdr_state.buffer[cdr_state.bptr] & 0xF00) >> 8;
            break;
        case 2:
            byte_out = cdr_state.buffer[cdr_state.bptr++] & 0xFF;
            break;
        }
        cdr_state.col++;
    }

    write_byte_io(cdr_state.mem_addr, byte_out, cdr_state.zio);
    cdr_state.mem_addr++;
    cdr_state.bytes_left--;

    /* End of requested length or end of current card */
    if (cdr_state.bytes_left == 0 || cdr_state.bptr == cdr_state.blnt) {
        cdr_state.active = 0;
        cdr_state.hopper--;
        cdr_state.stacker[cdr_state.stacker_sel]++;
        /* Preserve any EBCDIC error that occurred; otherwise clear */
        if ((cdr_state.status & 0x01) == 0)
            cdr_state.status = 0;
        cdr_interrupt();
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Interrupt (typical Mitra card-reader level = 4)                    */
/* ------------------------------------------------------------------ */

static void cdr_interrupt(void)
{
    uint32 int_req = (1u << 4);
    io_interrupt_dispatch(int_req, false);
}

/* ------------------------------------------------------------------ */
/* WD handler (E = 7)                                                 */
/* ------------------------------------------------------------------ */

t_stat cdr_wd(uint16 e_reg, uint16 a_val)
{
    if (e_reg != 7)
        return SCPE_IOERR;

    /* The Mitra-15 I/O system is responsible for having already
       written the transfer address into cdr_state.mem_addr and the
       byte count into cdr_state.bytes_left (and optionally the ZIO
       flag).  This handler only decodes the mode bits and starts
       (or stops) the reader. */
    cdr_start_transfer(a_val);
    return SCPE_OK;
}

/* ------------------------------------------------------------------ */
/* RD handler (E = 17)                                                */
/* ------------------------------------------------------------------ */

t_stat cdr_rd(uint16 e_reg, uint16 *result)
{
    if (e_reg != 17)
        return SCPE_IOERR;

    *result = cdr_state.status;
    cdr_state.status = 0;                       /* clear on read */
    return SCPE_OK;
}

/* ------------------------------------------------------------------ */
/* SHOW / RESET helpers                                               */
/* ------------------------------------------------------------------ */

t_stat cdr_show(FILE *st, UNIT *uptr, int32 val, const void *desc)
{
    fprintf(st, "Card Reader: hopper=%u, stacker[normal]=%u, alt1=%u, alt2=%u\n",
            cdr_state.hopper,
            cdr_state.stacker[0],
            cdr_state.stacker[1],
            cdr_state.stacker[2]);
    return SCPE_OK;
}

void cdr_reset(void)
{
    cdr_state.active = 0;
    cdr_state.status = 0;
    /* file remains attached */
}

/* ------------------------------------------------------------------ */
/* SIMH scaffolding                                                   */
/* ------------------------------------------------------------------ */

t_stat cr_svc(UNIT *uptr)
{
    return SCPE_OK;
}

t_stat cr_reset(DEVICE *dptr)
{
    cdr_reset();
    return SCPE_OK;
}

t_stat cr_attach(UNIT *uptr, const char *cptr)
{
    return cdr_attach(uptr, cptr);
}

t_stat cr_detach(UNIT *uptr)
{
    return cdr_detach(uptr);
}

t_stat cr_boot(int32 unit_num, DEVICE *dptr)
{
    return SCPE_OK;
}

t_stat cr_show_cap(FILE *st, UNIT *uptr, int32 val, const void *desc)
{
    return cdr_show(st, uptr, val, desc);
}

UNIT cr_unit = {
    UDATA(&cr_svc, UNIT_ATTABLE | UNIT_RO, 0)
};

REG cr_reg[] = {
    { DRDATA("HOPPER",   cdr_state.hopper,     18) },
    { DRDATA("STACKER0", cdr_state.stacker[0], 18) },
    { DRDATA("STACKER1", cdr_state.stacker[1], 18) },
    { DRDATA("STACKER2", cdr_state.stacker[2], 18) },
    { ORDATA("STATUS",   cdr_state.status,     16) },
    { FLDATA("ACTIVE",   cdr_state.active,      0) },
    { NULL }
};

MTAB cr_mod[] = {
    { MTAB_XTD | MTAB_VDV, 0, "CAPACITY", NULL,
      NULL, &cr_show_cap, NULL, "Card hopper / stacker status" },
    { 0 }
};

DEVICE cdr_dev = {
    "CR",               /* name */
    &cr_unit,           /* units */
    cr_reg,             /* registers */
    cr_mod,             /* modifiers */
    1,                  /* numunits */
    10,                 /* aradix */
    16,                 /* awidth */
    1,                  /* aincr */
    8,                  /* dradix */
    8,                  /* dwidth */
    NULL,               /* examine */
    NULL,               /* deposit */
    &cr_reset,          /* reset */
    &cr_boot,           /* boot */
    &cr_attach,         /* attach */
    &cr_detach,         /* detach */
    &cdr_dio_handler,               /* ctxt */
    DEV_DISABLE,        /* flags */
    0,                  /* dctrl */
    NULL,               /* debflags */
    NULL,               /* msize */
    NULL,               /* lname */
    NULL,               /* help */
    NULL,               /* attach_help */
    NULL,               /* help_ctxt */
    NULL                /* description */
};
