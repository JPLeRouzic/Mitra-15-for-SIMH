/* 
CONTROL DATA 9220 CARD READER

Channel Registers &1C to 28

Read WD E 7

         0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
       +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
  A    | err |                not used                 |
       +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

err:

	00 Binary read.
	10 EBCDIC read.
	11 Reader idle.

Read Status RD
	E: 17
	Result in A

	0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
	+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
	A | | Error |
	+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

Error:
	Empty Magazine &40
	Cell Errors &80
	Torn Cards, Jam Under Cells &C0
	Input Magazine Jam &A0
	Cards Read Wrong &10
	Transfer Failed &08
	Stop &04
	Not Operational &02
	EBCDIC Error &O1
 
Channel programming is handled by the Mitra-15 I/O system.
This file provides the device-specific logic for RD/WD commands.

WD (E=7):
   A bits 0-1: err
       00 -> binary read
       10 -> EBCDIC read
       11 -> reader idle

RD (E=17):
   Returns status word in A:
     bits: 0-15
       6 (0x40) -> empty magazine
       7 (0x80) -> cell errors
       8-9 (0xC0) -> torn cards, jam under cells
       5 (0xA0) -> input magazine jam
       4 (0x10) -> cards read wrong
       3 (0x08) -> transfer failed
       2 (0x04) -> stop
       1 (0x02) -> not operational
       0 (0x01) -> EBCDIC error
       
All devices will follow the same integration pattern, they provide:
	 _wd and _rd handlers, 
	 a _poll function for asynchronous transfers, 
	 interrupt generation via int_req, 
	 and use the memory access helpers (read_byte_io, write_byte_io, read_word, write_word). 
	 The device state is stored in static structures, 
	 and attach/detach functions are provided for file‑based devices.
*/

/*
 * CONTROL DATA 9220 CARD READER (MITRA-15)
 *
 * WD (E=7): A bits 0-1 = mode
 *   00 – binary read
 *   10 – EBCDIC read
 *   11 – reader idle
 *
 * RD (E=17): returns status word
 *   bit 6 (0x40) – empty magazine
 *   bit 7 (0x80) – cell errors
 *   bits 8-9 (0xC0) – torn cards / jam under cells
 *   bit 5 (0xA0) – input magazine jam
 *   bit 4 (0x10) – cards read wrong
 *   bit 3 (0x08) – transfer failed
 *   bit 2 (0x04) – stop
 *   bit 1 (0x02) – not operational
 *   bit 0 (0x01) – EBCDIC error
 *
 * Card image file: each card is 120 bytes (3 bytes per column × 80 columns).
 * The format matches the original cr_readrec packing:
 *   column 0: (byte0<<4)|(byte1>>4)
 *   column 1: ((byte1&0x0F)<<8)|byte2
 *   then next 3 bytes for columns 2&3, etc.
 */

#include "mitra_defs.h"
#include "mitra_cpu.h"
#include "mitra_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define CDR_COLUMNS      80
#define CDR_BYTES_PER_CARD 120

extern uint32 intrp_level;  /* interrupt request bits */

/* Memory Access Functions (defined in mitra_cpu.h) */
extern t_value read_word(t_addr va);
extern void write_word(t_addr va, t_value val);
extern uint8 read_byte(t_addr va);
extern void write_byte(t_addr va, uint8 val);

typedef struct {
    FILE *image;           /* card deck file */
    uint32 hopper;         /* cards left in hopper */
    uint32 stacker[3];     /* 0=normal, 1=alt1, 2=alt2 */
    uint32 stacker_sel;    /* selected stacker for current transfer */
    uint16 status;         /* last RD status */
    int    mode;           /* 0=binary, 1=ebcdic, 2=idle */
    int    active;         /* transfer in progress */
    uint32 buffer[CDR_COLUMNS]; /* 12‑bit column values */
    uint32 col;            /* current column (0-79) */
    uint32 bptr;           /* byte pointer for binary packing */
    uint32 blnt;           /* buffer length (0 or CDR_COLUMNS) */
    uint32 mem_addr;       /* current memory address */
    uint32 bytes_left;     /* bytes remaining */
    int    zio;            /* ZIO flag (shared memory) */
    uint32 cb_addr;        /* control block address for interrupt */
    int    waiting;        /* program waiting for completion */
} CDR_DEV;

static CDR_DEV cdr_state = {0};

/* Forward declarations */
static void cdr_interrupt(void);
static int  cdr_read_card(void);
static void cdr_start_transfer(uint32 cmd, uint32 mem_addr, uint32 count, int zio);

/* Attach a card deck file */
t_stat cdr_attach(UNIT *unit, const char *filename)
{
    if (cdr_state.image) fclose(cdr_state.image);
    cdr_state.image = fopen(filename, "rb");
    if (!cdr_state.image) return SCPE_IOERR;

    fseek(cdr_state.image, 0, SEEK_END);
    long size = ftell(cdr_state.image);
    fseek(cdr_state.image, 0, SEEK_SET);

    if (size % CDR_BYTES_PER_CARD != 0) {
        fclose(cdr_state.image);
        cdr_state.image = NULL;
        return SCPE_IOERR;
    }
    cdr_state.hopper = size / CDR_BYTES_PER_CARD;
    cdr_state.status = 0;
    return SCPE_OK;
}

void cdr_detach(void)
{
    if (cdr_state.image) {
        fclose(cdr_state.image);
        cdr_state.image = NULL;
    }
    cdr_state.hopper = 0;
    cdr_state.active = 0;
}

/* Read one card from file into cdr_state.buffer[]. Returns 1 on success, 0 on EOF/error. */
static int cdr_read_card(void)
{
    uint8 data[CDR_BYTES_PER_CARD];
    if (fread(data, 1, CDR_BYTES_PER_CARD, cdr_state.image) != CDR_BYTES_PER_CARD)
        return 0;

    /* Pack two columns per three bytes as in original cr_readrec */
    for (int col = 0; col < CDR_COLUMNS; ) {
        uint8 c1 = data[col/2 * 3];
        uint8 c2 = data[col/2 * 3 + 1];
        uint8 c3 = data[col/2 * 3 + 2];
        cdr_state.buffer[col++] = ((c1 << 4) | (c2 >> 4)) & 0xFFF;
        cdr_state.buffer[col++] = (((c2 & 0x0F) << 8) | c3) & 0xFFF;
    }
    return 1;
}

/* Start a new transfer (called from WD) */
static void cdr_start_transfer(uint32 cmd, uint32 mem_addr, uint32 count, int zio)
{
    if (cdr_state.active) return;          /* already busy */

    cdr_state.mode = (cmd & 0x03) == 0x02 ? 2 : ((cmd & 0x03) == 0x01 ? 1 : 0);
    if (cdr_state.mode == 2) {             /* idle command – do nothing */
        cdr_state.status = 0x04;           /* stop */
        cdr_interrupt();
        return;
    }

    if (cdr_state.hopper == 0) {
        cdr_state.status = 0x40;           /* empty magazine */
        cdr_interrupt();
        return;
    }

    if (!cdr_read_card()) {
        cdr_state.status = 0x80;           /* cell error (EOF) */
        cdr_interrupt();
        return;
    }

    cdr_state.col = 0;
    cdr_state.bptr = 0;
    cdr_state.blnt = CDR_COLUMNS;
    cdr_state.mem_addr = mem_addr;
    cdr_state.bytes_left = count;
    cdr_state.zio = zio;
    cdr_state.active = 1;
    /* Transfer will be driven by cdr_poll() */
}

/* Poll routine – called from io_poll_devices() */
int cdr_poll(void)
{
    if (!cdr_state.active) return 0;

    if (cdr_state.blnt == 0) {
        /* No card in buffer – read next */
        if (!cdr_read_card()) {
            cdr_state.active = 0;
            cdr_state.status = 0x80;       /* cell error */
            cdr_interrupt();
            return 1;
        }
        cdr_state.bptr = 0;
        cdr_state.blnt = CDR_COLUMNS;
    }

    uint8 byte_out;
    if (cdr_state.mode == 1) {             /* EBCDIC mode */
        uint16 row_bits = cdr_state.buffer[cdr_state.bptr++];
        /* Count holes in rows 1-7 (bits 1-7) */
        uint16 n = row_bits & 0x1FC;
        int bits = 0;
        while (n) { n &= n-1; bits++; }
        if (bits > 1) {
            byte_out = 0x00;
            cdr_state.status |= 0x01;      /* EBCDIC error */
        } else {
            /* Simplified Hollerith → EBCDIC mapping.
               A real implementation would use a 4096‑entry table.
               Here we return the low 8 bits as a placeholder. */
            byte_out = (uint8)(row_bits & 0xFF);
        }
    } else {                         /* Binary mode – pack 12 bits into two bytes */
        switch (cdr_state.col % 3) {
            case 0:
                byte_out = (cdr_state.buffer[cdr_state.bptr] >> 4) & 0xFF;
                break;
            case 1:
                byte_out = ((cdr_state.buffer[cdr_state.bptr] & 0x0F) << 4);
                cdr_state.bptr++;
                byte_out |= ((cdr_state.buffer[cdr_state.bptr] & 0xF00) >> 8);
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

    /* End of card or requested length */
    if (cdr_state.bytes_left == 0 || cdr_state.bptr == cdr_state.blnt) {
        cdr_state.active = 0;
        cdr_state.hopper--;
        cdr_state.stacker[cdr_state.stacker_sel]++;
        cdr_state.status = 0;              /* no error */
        cdr_interrupt();
    }
    return 1;
}

/* Generate interrupt (typical level 4 for card reader) */
static void cdr_interrupt(void)
{
    uint32 int_req = (1 << 4);
    io_interrupt_dispatch(int_req, false);
}

/* WD handler (E=7) */
t_stat cdr_wd(uint16 e_reg, uint16 a_val)
{
    if (e_reg != 7) return SCPE_IOERR;
    
    /* Mode from A bits 0-1: 00=binary, 10=EBCDIC, 11=idle 
       Channel registers: ADM at &1C (address -2), CM at &1D (word count) 
    */
    uint16 adm = read_word(0x1C);
    uint16 cm  = read_word(0x1D);
    uint32 mem_addr = adm + 2;
    uint32 byte_count = cm * 2;  /* words → bytes */
    cdr_start_transfer(a_val, mem_addr, byte_count, 0); /* ZIO not used for CR */
    
    cdr_state.zio = 0;
    cdr_state.active = 1;

    return SCPE_OK;
}

/* RD handler (E=17) */
t_stat cdr_rd(uint16 e_reg, uint16 *result)
{
    if (e_reg != 17) return SCPE_IOERR;
    *result = cdr_state.status;
    cdr_state.status = 0;                  /* clear on read */
    return SCPE_OK;
}

/* Show status (for SHOW command) */
t_stat cdr_show(FILE *st, UNIT *uptr, int32 val, const void *desc)
{
    fprintf(st, "Card Reader: hopper=%u, stacker[normal]=%u, alt1=%u, alt2=%u\n",
            cdr_state.hopper, cdr_state.stacker[0], cdr_state.stacker[1], cdr_state.stacker[2]);
    return SCPE_OK;
}

/* Reset device */
void cdr_reset(void)
{
    cdr_state.active = 0;
    cdr_state.status = 0;
    /* do not detach file */
}

/* ========== SIMH STRUCTURES ========== */

/* Unit service routine */
t_stat cr_svc(UNIT *uptr)
{
    return SCPE_OK;
}

/* Device reset routine */
t_stat cr_reset(DEVICE *dptr)
{
    cdr_reset();
    return SCPE_OK;
}

/* Device attach routine */
t_stat cr_attach(UNIT *uptr, const char *cptr)
{
    return cdr_attach(uptr, cptr);
}

/* Device detach routine */
t_stat cr_detach(UNIT *uptr)
{
    cdr_detach();
    return SCPE_OK;
}

/* Device boot routine */
t_stat cr_boot(int32 unit_num, DEVICE *dptr)
{
    /* Bootstrap loader would go here */
    return SCPE_OK;
}

/* Show capacity routine */
t_stat cr_show_cap(FILE *st, UNIT *uptr, int32 val, const void *desc)
{
    return cdr_show(st, uptr, val, desc);
}

/* Set channel routine */
t_stat set_chan(UNIT *uptr, int32 val, const char *cptr, void *desc)
{
    return SCPE_OK;
}

/* Show channel routine */
t_stat show_chan(FILE *st, UNIT *uptr, int32 val, const void *desc)
{
    fprintf(st, "Channel not implemented\n");
    return SCPE_OK;
}

/* Unit definition */
UNIT cr_unit = {
    UDATA(&cr_svc, UNIT_ATTABLE | UNIT_RO, 0)
};

/* Register definitions */
REG cr_reg[] = {
    { DRDATA("HOPPER", cdr_state.hopper, 18) },
    { DRDATA("STACKER0", cdr_state.stacker[0], 18) },
    { DRDATA("STACKER1", cdr_state.stacker[1], 18) },
    { DRDATA("STACKER2", cdr_state.stacker[2], 18) },
    { ORDATA("STATUS", cdr_state.status, 16) },
    { FLDATA("ACTIVE", cdr_state.active, 0) },
    { NULL }
};

/* Modifier table */
MTAB cr_mod[] = {
    {MTAB_XTD | MTAB_VDV, 0, "CHANNEL", "CHANNEL",
        &set_chan, &show_chan, NULL, "Device Channel"},
    { MTAB_XTD|MTAB_VDV, 0, "CAPACITY", NULL,
        NULL, &cr_show_cap, NULL, "Card Input Status" },
    {0}
};

/* Device definition */
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
    NULL,               /* ctxt */
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




