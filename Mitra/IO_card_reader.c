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

#include "mitra_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CDR_COLUMNS      80
#define CDR_BYTES_PER_CARD 120

extern uint32 int_req;               /* interrupt request bits */

/* Memory Access Functions (defined in mitra_cpu.c) */
extern uint16 read_word(uint16 addr);
extern void write_word(uint16 addr, uint16 val);
extern uint8 read_byte(uint16 va);
extern void write_byte(uint16 va, uint8 val);

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

static CDR_DEV cdr_dev = {0};

/* Forward declarations */
static void cdr_interrupt(void);
static int  cdr_read_card(void);
static void cdr_start_transfer(uint32 cmd, uint32 mem_addr, uint32 count, int zio);

/* Attach a card deck file */
t_stat cdr_attach(const char *filename)
{
    if (cdr_dev.image) fclose(cdr_dev.image);
    cdr_dev.image = fopen(filename, "rb");
    if (!cdr_dev.image) return SCPE_IOERR;

    fseek(cdr_dev.image, 0, SEEK_END);
    long size = ftell(cdr_dev.image);
    fseek(cdr_dev.image, 0, SEEK_SET);

    if (size % CDR_BYTES_PER_CARD != 0) {
        fclose(cdr_dev.image);
        cdr_dev.image = NULL;
        return SCPE_IOERR;
    }
    cdr_dev.hopper = size / CDR_BYTES_PER_CARD;
    cdr_dev.status = 0;
    return SCPE_OK;
}

void cdr_detach(void)
{
    if (cdr_dev.image) {
        fclose(cdr_dev.image);
        cdr_dev.image = NULL;
    }
    cdr_dev.hopper = 0;
    cdr_dev.active = 0;
}

/* Read one card from file into cdr_dev.buffer[]. Returns 1 on success, 0 on EOF/error. */
static int cdr_read_card(void)
{
    uint8 data[CDR_BYTES_PER_CARD];
    if (fread(data, 1, CDR_BYTES_PER_CARD, cdr_dev.image) != CDR_BYTES_PER_CARD)
        return 0;

    /* Pack two columns per three bytes as in original cr_readrec */
    for (int col = 0; col < CDR_COLUMNS; ) {
        uint8 c1 = data[col/2 * 3];
        uint8 c2 = data[col/2 * 3 + 1];
        uint8 c3 = data[col/2 * 3 + 2];
        cdr_dev.buffer[col++] = ((c1 << 4) | (c2 >> 4)) & 0xFFF;
        cdr_dev.buffer[col++] = (((c2 & 0x0F) << 8) | c3) & 0xFFF;
    }
    return 1;
}

/* Start a new transfer (called from WD) */
static void cdr_start_transfer(uint32 cmd, uint32 mem_addr, uint32 count, int zio)
{
    if (cdr_dev.active) return;          /* already busy */

    cdr_dev.mode = (cmd & 0x03) == 0x02 ? 2 : ((cmd & 0x03) == 0x01 ? 1 : 0);
    if (cdr_dev.mode == 2) {             /* idle command – do nothing */
        cdr_dev.status = 0x04;           /* stop */
        cdr_interrupt();
        return;
    }

    if (cdr_dev.hopper == 0) {
        cdr_dev.status = 0x40;           /* empty magazine */
        cdr_interrupt();
        return;
    }

    if (!cdr_read_card()) {
        cdr_dev.status = 0x80;           /* cell error (EOF) */
        cdr_interrupt();
        return;
    }

    cdr_dev.col = 0;
    cdr_dev.bptr = 0;
    cdr_dev.blnt = CDR_COLUMNS;
    cdr_dev.mem_addr = mem_addr;
    cdr_dev.bytes_left = count;
    cdr_dev.zio = zio;
    cdr_dev.active = 1;
    /* Transfer will be driven by cdr_poll() */
}

/* Poll routine – called from io_poll_devices() */
int cdr_poll(void)
{
    if (!cdr_dev.active) return 0;

    if (cdr_dev.blnt == 0) {
        /* No card in buffer – read next */
        if (!cdr_read_card()) {
            cdr_dev.active = 0;
            cdr_dev.status = 0x80;       /* cell error */
            cdr_interrupt();
            return 1;
        }
        cdr_dev.bptr = 0;
        cdr_dev.blnt = CDR_COLUMNS;
    }

    uint8 byte_out;
    if (cdr_dev.mode == 1) {             /* EBCDIC mode */
        uint16 row_bits = cdr_dev.buffer[cdr_dev.bptr++];
        /* Count holes in rows 1-7 (bits 1-7) */
        uint16 n = row_bits & 0x1FC;
        int bits = 0;
        while (n) { n &= n-1; bits++; }
        if (bits > 1) {
            byte_out = 0x00;
            cdr_dev.status |= 0x01;      /* EBCDIC error */
        } else {
            /* Simplified Hollerith → EBCDIC mapping.
               A real implementation would use a 4096‑entry table.
               Here we return the low 8 bits as a placeholder. */
            byte_out = (uint8)(row_bits & 0xFF);
        }
    } else {                         /* Binary mode – pack 12 bits into two bytes */
        switch (cdr_dev.col % 3) {
            case 0:
                byte_out = (cdr_dev.buffer[cdr_dev.bptr] >> 4) & 0xFF;
                break;
            case 1:
                byte_out = ((cdr_dev.buffer[cdr_dev.bptr] & 0x0F) << 4);
                cdr_dev.bptr++;
                byte_out |= ((cdr_dev.buffer[cdr_dev.bptr] & 0xF00) >> 8);
                break;
            case 2:
                byte_out = cdr_dev.buffer[cdr_dev.bptr++] & 0xFF;
                break;
        }
        cdr_dev.col++;
    }

    write_byte_io(cdr_dev.mem_addr, byte_out, cdr_dev.zio);
    cdr_dev.mem_addr++;
    cdr_dev.bytes_left--;

    /* End of card or requested length */
    if (cdr_dev.bytes_left == 0 || cdr_dev.bptr == cdr_dev.blnt) {
        cdr_dev.active = 0;
        cdr_dev.hopper--;
        cdr_dev.stacker[cdr_dev.stacker_sel]++;
        cdr_dev.status = 0;              /* no error */
        cdr_interrupt();
    }
    return 1;
}

/* Generate interrupt (typical level 4 for card reader) */
static void cdr_interrupt(void)
{
    int_req |= (1 << 4);
    io_interrupt_dispatch();
}

/* WD handler (E=7) */
t_stat cdr_wd(uint16 e_reg, uint16 a_val)
{
    if (e_reg != 7) return SCPE_IOERR;
    /* Channel registers: ADM at &1C (address -2), CM at &1D (word count) */
    uint16 adm = read_word(0x1C);
    uint16 cm  = read_word(0x1D);
    uint32 mem_addr = adm + 2;
    uint32 byte_count = cm * 2;
    cdr_start_transfer(a_val, mem_addr, byte_count, 0); /* ZIO not used for CR */
    return SCPE_OK;
}

/* RD handler (E=17) */
t_stat cdr_rd(uint16 e_reg, uint16 *result)
{
    if (e_reg != 17) return SCPE_IOERR;
    *result = cdr_dev.status;
    cdr_dev.status = 0;                  /* clear on read */
    return SCPE_OK;
}

/* Show status (for SHOW command) */
t_stat cdr_show(FILE *st, UNIT *uptr, int32 val, const void *desc)
{
    fprintf(st, "Card Reader: hopper=%u, stacker[normal]=%u, alt1=%u, alt2=%u\n",
            cdr_dev.hopper, cdr_dev.stacker[0], cdr_dev.stacker[1], cdr_dev.stacker[2]);
    return SCPE_OK;
}

/* Reset device */
void cdr_reset(void)
{
    cdr_dev.active = 0;
    cdr_dev.status = 0;
    /* do not detach file */
}
