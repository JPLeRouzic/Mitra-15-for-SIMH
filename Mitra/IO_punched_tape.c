/* 
1. TAPE READER:

WD Advance reading with leader detection: 
	register A: 1 
	register E:8

WD Play advances without leader detection 
	register A: 3 
	register E:8

Stop 
	register A:2 
	register E:8

Status reading RD 
	register E:8 
	result in A 

        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
      | err |                 |     caractère lu      |
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

err: 
	00: no error 
	10: 
		power off 
		manual 
		absence of paper 
		paper break 
		(this list is exhaustive) 
	11: stops

---------------------------------------------------------
2. TAPE PUNCHER:

WD dialogues with the Program 

         0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
       +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 A     | S|                    |        données        |
       +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

S: 
	0 Stop. 
	1 Advance 

E 18

Status reading RD 
	register E: 18 
	result in A 

        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
      | err |                not used                 |
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

RD
	Result in A

err: 
	00 No error. 
	10 Error. 
	11 Stop.

Error: 
	Power off. 
	No paper. 
	the operator feeds paper simultaneously. 
	(exhaustive list)

Reader WD (E=8):
   A = 1 -> advance with leader detection
   A = 3 -> advance without leader detection
   A = 2 -> stop

Reader RD (E=8):
   Returns status in A:
     bits 0-1: err (00=no error, 10=offline/no tape/break, 11=stop)
     bits 8-15: last character read

Punch WD (E=18):
   A bit 15 (S): 0=stop, 1=advance
   A bits 0-7: data to punch

Punch RD (E=18):
   Returns status in A:
     bits 0-1: err (00=no error, 10=error, 11=stop)
     
All devices will follow the same integration pattern, they provide:
	 _wd and _rd handlers, 
	 a _poll function for asynchronous transfers, 
	 interrupt generation via int_req, 
	 and use the memory access helpers (read_byte_io, write_byte_io, read_word, write_word). 
	 The device state is stored in static structures, 
	 and attach/detach functions are provided for file‑based devices.
*/

/*
 * PAPER TAPE READER (PTR) AND PUNCH (PTP) – MITRA-15
 *
 * PTR WD (E=8):
 *   A = 1  – advance with leader detection
 *   A = 3  – advance without leader detection
 *   A = 2  – stop
 *
 * PTR RD (E=8):
 *   bits 0-1: err (00=no error, 10=offline/no tape/break, 11=stop)
 *   bits 8-15: last character read
 *
 * PTP WD (E=18):
 *   bit 15 (S): 0=stop, 1=advance
 *   bits 0-7: data to punch
 *
 * PTP RD (E=18):
 *   bits 0-1: err (00=ok, 10=error, 11=stop)
 */

#include "mitra_defs.h"
#include "mitra_cpu.h"
#include "mitra_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

extern uint32 intrpt_mask;  /* interrupt request bits */

/* Memory Access Functions (defined in mitra_cpu.h) */
extern t_value read_word(t_addr va);
extern void write_word(t_addr va, t_value val);
extern uint8 read_byte(t_addr va);
extern void write_byte(t_addr va, uint8 val);

/* ----- Reader state ----- */
typedef struct {
    FILE *image;
    uint32 pos;
    uint32 nzc;          /* non‑zero character seen (leader detection) */
    uint8  last_char;
    uint16 status;
    int    active;
    uint32 mem_addr;
    uint32 bytes_left;
    int    zio;
    int    mode;         /* 1=with leader, 3=without leader */
} PTR_DEV;

/* ----- Punch state ----- */
typedef struct {
    FILE *image;
    uint32 pos;
    uint16 status;
    int    active;
    uint32 mem_addr;
    uint32 bytes_left;
    int    zio;
    int    advance;      /* 0=stop, 1=advance */
} PTP_DEV;

static PTR_DEV ptr = {0};
static PTP_DEV ptp = {0};

/* ----- Reader functions ----- */
t_stat ptr_attach(UNIT *unit, const char *filename)
{
    if (ptr.image) 
    	fclose(ptr.image);
    	
    t_stat r;
    char *saved_filename = unit->filename;

    /* Let the standard SIMH helper open the file and set UNIT_ATT,
       fileref, filename, etc. */
    unit->filename = NULL;
    r = attach_unit(unit, filename);
    if (r != SCPE_OK) {
        unit->filename = saved_filename;
        return r;
    }

    ptr.pos = 0;
    ptr.nzc = 0;
    ptr.last_char = 0;
    ptr.status = 0;
    return SCPE_OK;
}

t_stat ptr_detach(UNIT *unit)
{
    ptr.image = NULL;
    ptr.active = 0;
    detach_unit(unit);
    return SCPE_OK;
}

static void ptr_interrupt(void)
{
    uint32 int_req = (1 << 5);   /* typical interrupt level for reader */
    io_interrupt_dispatch(int_req, false);
}

int ptr_poll(void)
{
    if (!ptr.active) return 0;

    int c = fgetc(ptr.image);
    if (c == EOF) {
        ptr.active = 0;
        ptr.status = 0x02;   /* error code 10 binary: stop/error */
        ptr_interrupt();
        return 1;
    }
    ptr.pos++;
    if (c != 0) ptr.nzc = 1;

    if (ptr.mode == 1 && !ptr.nzc) {
        /* leader detection: skip zeros until first non‑zero */
        return 1;   /* continue polling, no data yet */
    }

    write_byte_io(ptr.mem_addr, (uint8)c, ptr.zio);
    ptr.mem_addr++;
    ptr.bytes_left--;
    ptr.last_char = (uint8)c;

    if (ptr.bytes_left == 0) {
        ptr.active = 0;
        ptr.status = 0;   /* no error */
        ptr_interrupt();
    }
    return 1;
}

t_stat ptr_wd(uint16 e_reg, uint16 a_val)
{
    if (e_reg != 8) return SCPE_IOERR;
    uint16 cmd = a_val & 0xFF;
    /* case 1: Advance with leader detection
    /* case 3: Advance without leader detection */
    if (cmd == 1 || cmd == 3) {
        if (!ptr.image) return SCPE_UNATT;
        if (ptr.active) return SCPE_OK;   /* busy */
        ptr.mode = cmd;
        ptr.active = 1;
        ptr.nzc = 0;  /* Reset leader detection */

        /* Channel registers: ADM at &1C, CM at &1D */
        uint16 adm = read_word(0x1C);
        uint16 cm  = read_word(0x1D);
        ptr.mem_addr = adm + 2;
        ptr.bytes_left = cm * 2;
        ptr.zio = 0;
    } else if (cmd == 2) { /* Stop */
        ptr.active = 0;
        ptr.status = 0x03;   /* stop */
    }
    return SCPE_OK;
}

t_stat ptr_rd(uint16 e_reg, uint16 *result)
{
    if (e_reg != 8) return SCPE_IOERR;
    *result = (ptr.status & 0x03) | (ptr.last_char << 8);
    ptr.status = 0;
    return SCPE_OK;
}

/* ----- Punch functions ----- */
t_stat ptp_attach(UNIT *unit, const char *filename)
{
    t_stat r;
    char *saved_filename = unit->filename;

    if (ptp.image) 
    	fclose(ptp.image);

    /* Let the standard SIMH helper open the file and set UNIT_ATT,
       fileref, filename, etc. */
    unit->filename = NULL;
    r = attach_unit(unit, filename);
    if (r != SCPE_OK) {
        unit->filename = saved_filename;
        return r;
    }

    ptp.pos = 0;
    ptp.status = 0;
    return SCPE_OK;
}

t_stat ptp_detach(UNIT *unit)
{
    ptp.image = NULL;
    ptp.active = 0;
    detach_unit(unit);
    return SCPE_OK;
}

static void ptp_interrupt(void)
{
    uint32 int_req = (1 << 6);   /* typical interrupt level for punch */
    io_interrupt_dispatch(int_req, false);
}

int ptp_poll(void)
{
    if (!ptp.active) return 0;

    uint8 data;
    if (read_byte_io(ptp.mem_addr, &data, ptp.zio) != SCPE_OK) {
        ptp.active = 0;
        ptp.status = 0x02;
        ptp_interrupt();
        return 1;
    }
    ptp.mem_addr++;
    ptp.bytes_left--;

    if (ptp.advance) {
        if (fputc(data, ptp.image) == EOF) {
            ptp.active = 0;
            ptp.status = 0x02;
            ptp_interrupt();
            return 1;
        }
        ptp.pos++;
    }

    if (ptp.bytes_left == 0) {
        ptp.active = 0;
        ptp.status = 0;
        ptp_interrupt();
    }
    return 1;
}

t_stat ptp_wd(uint16 e_reg, uint16 a_val)
{
    if (e_reg != 18) return SCPE_IOERR;
    if (!ptp.image) return SCPE_UNATT;
    
    ptp.advance = (a_val >> 15) & 1;  /* Bit 15 = S flag */
    if (ptp.advance) {
        if (ptp.active) return SCPE_OK;
        ptp.active = 1;
        uint16 adm = read_word(0x1C);
        uint16 cm  = read_word(0x1D);
        ptp.mem_addr = adm + 2;
        ptp.bytes_left = cm * 2;
        ptp.zio = 0;
        ptp.status = 0;   /* Clear status on start */
    } else {
        ptp.active = 0;
        ptp.status = 0x03;   /* stop */
    }
    return SCPE_OK;
}

t_stat ptp_rd(uint16 e_reg, uint16 *result)
{
    if (e_reg != 18) return SCPE_IOERR;
    *result = ptp.status & 0x03;
    ptp.status = 0;
    return SCPE_OK;
}

/* Reset functions */
void ptr_reset(void) { ptr.active = 0; ptr.status = 0; }
void ptp_reset(void) { ptp.active = 0; ptp.status = 0; }

/* Combined poll for both devices – called from main I/O loop */
void pt_poll_devices(void)
{
    ptr_poll();
    ptp_poll();
}

/* ========== SIMH STRUCTURES ========== */

/* PTR service routine */
t_stat ptr_svc(UNIT *uptr)
{
    return SCPE_OK;
}

/* PTR device reset routine */
t_stat ptr_reset_dev(DEVICE *dptr)
{
    ptr_reset();
    return SCPE_OK;
}

/* PTR device boot routine */
t_stat ptr_boot(int32 unit_num, DEVICE *dptr)
{
    return SCPE_OK;
}

UNIT ptr_unit = {
    UDATA (&ptr_svc, UNIT_SEQ+UNIT_ATTABLE+UNIT_ROABLE, 0),
           SERIAL_IN_WAIT
    };

REG ptr_reg[] = {
    { DRDATA("POS", ptr.pos, 18) },
    { DRDATA("LASTCHAR", ptr.last_char, 8) },
    { ORDATA("STATUS", ptr.status, 16) },
    { FLDATA("ACTIVE", ptr.active, 0) },
    { FLDATA("MODE", ptr.mode, 0) },
    { NULL }
    };

/* PTR modifier table */
MTAB ptr_mod[] = {
    { 0 }
    };

DEVICE ptr_dev = {
    "PTR",              /* name */
    &ptr_unit,          /* units */
    ptr_reg,            /* registers */
    ptr_mod,            /* modifiers */
    1,                  /* numunits */
    10,                 /* aradix */
    16,                 /* awidth */
    1,                  /* aincr */
    8,                  /* dradix */
    8,                  /* dwidth */
    NULL,               /* examine */
    NULL,               /* deposit */
    &ptr_reset_dev,     /* reset */
    &ptr_boot,          /* boot */
    &ptr_attach,        /* attach */
    &ptr_detach,        /* detach */
    NULL,               /* ctxt */
    DEV_DISABLE,        /* flags */
    0,                  /* dctrl */
    NULL,               /* debflags */
    NULL,               /* msize */
    NULL,               /* lname */
    NULL,               /* help */
    NULL,               /* attach_help */
    NULL,               /* help_ctxt */
    NULL,               /* description */
};

/* ===== PTP Structures ===== */

/* PTP service routine */
t_stat ptp_svc(UNIT *uptr)
{
    return SCPE_OK;
}

/* PTP reset routine */
t_stat ptp_reset_dev(DEVICE *dptr)
{
    ptp_reset();
    return SCPE_OK;
}

/* PTP unit definition */
UNIT ptp_unit = {
    UDATA(&ptp_svc, UNIT_SEQ | UNIT_ATTABLE, 0)
};

REG ptp_reg[] = {
    { DRDATA("POS", ptp.pos, 18) },
    { ORDATA("STATUS", ptp.status, 16) },
    { FLDATA("ACTIVE", ptp.active, 0) },
    { FLDATA("ADVANCE", ptp.advance, 0) },
    { NULL }
    };

MTAB ptp_mod[] = {
    { 0 }
    };

DEVICE ptp_dev = {
    "PTP",              /* name */
    &ptp_unit,          /* units */
    ptp_reg,            /* registers */
    ptp_mod,            /* modifiers */
    1,                  /* numunits */
    10,                 /* aradix */
    16,                 /* awidth */
    1,                  /* aincr */
    8,                  /* dradix */
    8,                  /* dwidth */
    NULL,               /* examine */
    NULL,               /* deposit */
    &ptp_reset_dev,     /* reset */
    NULL,               /* boot */
    &ptp_attach,        /* attach */
    &ptp_detach,        /* detach */
    NULL,               /* ctxt */
    DEV_DISABLE,        /* flags */
    0,                  /* dctrl */
    NULL,               /* debflags */
    NULL,               /* msize */
    NULL,               /* lname */
    NULL,               /* help */
    NULL,               /* attach_help */
    NULL,               /* help_ctxt */
    NULL,               /* description */
 };
