/* 
* SAGEM Disk 

256 bytes per sector
12 sectors per track
Track Registers

AVS2:

	3B: AD
	3C: CM
	3D: AP
	3register register E: ME

AD: Address -2 of the first word to transfer.

CM: Word count.

AP
        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
      | 0|     ADp                  |    ADs    |  M0 |
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

ADp: Track address
ADs: Sector address
M0:

	00: Rest
	01: Write
	10: Read
	11: Compare

ME
        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
      | M0  |                   0                     |
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

Commands on coupler:

	register register E: 5
	register register A: AP

WD for rest state:

	register register E: 5
	register register A: 0

After a request, the value is retrieved from the interruption in the ME register.

        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
      |             Unused             |              |
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

	Bit 11: Requested sector address does not exist.
	Bit 12: CN initial word count zero.
	Bit 13: PL longitudinal parity error.
	Bit 14: VE error during comparison mode.
	Bit 15: I/O request successful.

If OK = 0, the cause of the error is specified by performing a status read.

	register register E: 3
	RD: Result in register register register A:

        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
      |             Unused                      | ER  |
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

ER:

	00: No error
	01: Area unavailable (address does not exist) or protected
	02: Rhythm error
	03: Disk not ready (powered off or not connected)

Registers:
   &3B AD  : memory address -2 (words)
   &3C CM  : word count
   &3D AP  : track/sector address + mode
        bits 1-7: track (0-127)
        bits 8-11: sector (0-15)
        bits 14-15: mode (00=rest, 01=write, 10=read, 11=compare)
   &3E ME  : mode/status (written by device on interrupt)

WD E=5 : write AP to register &3D, start operation
RD E=3 : read status (bits 0-1: ER: 00=ok, 01=bad address/protected, 02=rhythm error, 03=not ready)

ME register bits on interrupt:
   bit15: OK (1=success)
   bit14: VE compare error
   bit13: PL parity error
   bit12: CN zero word count
   bit11: invalid sector address
   
All devices will follow the same integration pattern, they provide:
	 _wd and _rd handlers, 
	 a _poll function for asynchronous transfers, 
	 interrupt generation via int_req, 
	 and use the memory access helpers (read_byte_io, write_byte_io, read_word, write_word). 
	 The device state is stored in static structures, 
	 and attach/detach functions are provided for file‑based devices.
*/

/*
 * SAGEM DISK (MITRA-15)
 *
 * Geometry: 256 bytes/sector, 12 sectors/track.
 * Registers (memory‑mapped):
 *   &3B AD  – memory address -2 (words)
 *   &3C CM  – word count
 *   &3D AP  – track/sector address + mode
 *   &3E ME  – mode/status (written by device on interrupt)
 *
 * AP bits:
 *   bits 1-7 (0-127) : track address (ADp)
 *   bits 8-11 (0-15) : sector address (ADs)
 *   bits 14-15       : mode (M0): 00=rest, 01=write, 10=read, 11=compare
 *
 * ME bits (returned in interrupt):
 *   bit15 – OK (success)
 *   bit14 – VE compare error
 *   bit13 – PL parity error
 *   bit12 – CN zero word count
 *   bit11 – invalid sector address
 *
 * Commands:
 *   WD E=5, A=AP   – start operation
 *   WD E=5, A=0    – rest
 *
 * Status RD (E=3):
 *   bits 0-1 (ER): 00=no error, 01=bad address/protected, 02=rhythm error, 03=not ready
 */

#include "mitra_defs.h"
#include "mitra_cpu.h"
#include "mitra_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define SAGEM_SECTOR_SIZE      256
#define SAGEM_SECTORS_PER_TRACK 12
#define SAGEM_TRACKS           128
#define SAGEM_WORDS_PER_SECTOR (SAGEM_SECTOR_SIZE / 2)
#define SAGEM_NUM_UNITS        2

extern uint32 intrpt_mask;  /* interrupt request bits */
UNIT sagem_unit[SAGEM_NUM_UNITS];

/* Declared in sim_disk.h; forward-declared here to avoid include-path issues */
extern t_stat sim_disk_set_fmt(UNIT *uptr, int32 val, const char *cptr, void *desc);
extern t_stat sim_disk_show_fmt(FILE *st, UNIT *uptr, int32 val, const void *desc);
extern t_stat sim_disk_set_capac(UNIT *uptr, int32 val, const char *cptr, void *desc);
extern t_stat sim_disk_show_capac(FILE *st, UNIT *uptr, int32 val, const void *desc);

/* Memory Access Functions (defined in mitra_cpu.h) */
extern t_value read_word(t_addr va);
extern void write_word(t_addr va, t_value val);
extern uint8 read_byte(t_addr va);
extern void write_byte(t_addr va, uint8 val);

extern uint32 intrpt_mask;  /* interrupt request bits */

typedef struct {
    FILE *image;
    uint32 total_sectors;
    uint16 me;              /* ME register (set by device) */
    uint16 status;          /* RD status (ER bits) */
    int    active;
    uint32 mem_addr;
    uint32 bytes_left;
    int    zio;
    uint32 track;
    uint32 sector;
    int    mode;            /* 0=rest,1=write,2=read,3=compare */
} SAGEM_DEV;

static SAGEM_DEV sagem_state[SAGEM_NUM_UNITS] = {{0}};

static uint32 ts_to_sector(uint32 track, uint32 sector)
{
    return track * SAGEM_SECTORS_PER_TRACK + sector;
}

static void sagem_interrupt(void)
{
    uint32 int_req = (1 << 9);   /* typical interrupt level for SAGEM disk */
    io_interrupt_dispatch(int_req, false);
}

/* Start a transfer */
static void sagem_start_transfer(uint8 unitnb, uint32 mem_addr, uint32 byte_count, uint32 track, uint32 sector, int mode, int zio)
{
    if (sagem_state[unitnb].active) return;

    uint32 sec = ts_to_sector(track, sector);
    if (sec >= sagem_state[unitnb].total_sectors) {
        sagem_state[unitnb].me = 0x0800;   /* invalid sector address */
        sagem_state[unitnb].status = 0x01;
        sagem_interrupt();
        return;
    }
    if (byte_count == 0) {
        sagem_state[unitnb].me = 0x1000;   /* zero word count */
        sagem_state[unitnb].status = 0x02;
        sagem_interrupt();
        return;
    }

    sagem_state[unitnb].active = 1;
    sagem_state[unitnb].mem_addr = mem_addr;
    sagem_state[unitnb].bytes_left = byte_count;
    sagem_state[unitnb].mode = mode;
    sagem_state[unitnb].track = track;
    sagem_state[unitnb].sector = sector;
    sagem_state[unitnb].zio = zio;
}

/* FIXME unused
int sagem_poll(void)
{
    if (!sagem_state[unitnb].active) return 0;

    uint32 sector = ts_to_sector(sagem_state[unitnb].track, sagem_state[unitnb].sector);
    uint32 offset = sector * SAGEM_SECTOR_SIZE;
    FILE *f = sagem_state[unitnb].image;

    if (sagem_state[unitnb].mode == 2) {   * read *
        if (fseek(f, offset, SEEK_SET) != 0) {
            sagem_state[unitnb].active = 0;
            sagem_state[unitnb].me = 0x0400;   * parity error (simulated) *
            sagem_state[unitnb].status = 0x03;
            sagem_interrupt();
            return 1;
        }
        uint32 bytes_to_do = (sagem_state[unitnb].bytes_left < SAGEM_SECTOR_SIZE) ? sagem_state[unitnb].bytes_left : SAGEM_SECTOR_SIZE;
        uint8 buf[SAGEM_SECTOR_SIZE];
        if (fread(buf, 1, bytes_to_do, f) != bytes_to_do) {
            sagem_state[unitnb].active = 0;
            sagem_state[unitnb].me = 0x0400;
            sagem_state[unitnb].status = 0x03;
            sagem_interrupt();
            return 1;
        }
        for (uint32 i = 0; i < bytes_to_do; i++)
            write_byte_io(sagem_state[unitnb].mem_addr + i, buf[i], sagem_state[unitnb].zio);
        sagem_state[unitnb].mem_addr += bytes_to_do;
        sagem_state[unitnb].bytes_left -= bytes_to_do;
    } else if (sagem_state[unitnb].mode == 1) {   * write *
        if (fseek(f, offset, SEEK_SET) != 0) {
            sagem_state[unitnb].active = 0;
            sagem_state[unitnb].me = 0x0400;
            sagem_state[unitnb].status = 0x03;
            sagem_interrupt();
            return 1;
        }
        uint32 bytes_to_do = (sagem_state[unitnb].bytes_left < SAGEM_SECTOR_SIZE) ? sagem_state[unitnb].bytes_left : SAGEM_SECTOR_SIZE;
        uint8 buf[SAGEM_SECTOR_SIZE];
        for (uint32 i = 0; i < bytes_to_do; i++)
            read_byte_io(sagem_state[unitnb].mem_addr + i, &buf[i], sagem_state[unitnb].zio);
        if (fwrite(buf, 1, bytes_to_do, f) != bytes_to_do) {
            sagem_state[unitnb].active = 0;
            sagem_state[unitnb].me = 0x0400;
            sagem_state[unitnb].status = 0x03;
            sagem_interrupt();
            return 1;
        }
        sagem_state[unitnb].mem_addr += bytes_to_do;
        sagem_state[unitnb].bytes_left -= bytes_to_do;
    } else {
        * compare or rest – not implemented, treat as error *
        sagem_state[unitnb].active = 0;
        sagem_state[unitnb].me = 0x2000;   * compare error *
        sagem_state[unitnb].status = 0x02;
        sagem_interrupt();
        return 1;
    }

    * Advance to next sector if more data remains *
    if (sagem_state[unitnb].bytes_left > 0) {
        sagem_state[unitnb].sector++;
        if (sagem_state[unitnb].sector >= SAGEM_SECTORS_PER_TRACK) {
            sagem_state[unitnb].sector = 0;
            sagem_state[unitnb].track++;
        }
    }

    if (sagem_state[unitnb].bytes_left == 0) {
        sagem_state[unitnb].active = 0;
        sagem_state[unitnb].me = 0x8000;   * success *
        sagem_state[unitnb].status = 0;
        sagem_interrupt();
    }
    return 1;
} */

/* Attach disk image */
/* Derive unit index from the UNIT pointer that SIMH passes */
static int sagem_unit_index(UNIT *uptr)
{
    int idx = (int)(uptr - sagem_unit);
    if (idx < 0 || idx >= SAGEM_NUM_UNITS)
        return -1;
    return idx;
}

/* Attach disk image */
t_stat sagem_attach(UNIT *uptr, const char *cptr)
{
    t_stat r;
    int unitnb = sagem_unit_index(uptr);
    char *saved_filename;

    if (unitnb < 0)
        return SCPE_IERR;

    saved_filename = uptr->filename;
    uptr->filename = NULL;
    r = attach_unit(uptr, cptr);
    if (r != SCPE_OK) {
        uptr->filename = saved_filename;
        return r;
    }

    /* Now that the standard helper has opened the file, remember it */
    sagem_state[unitnb].image = uptr->fileref;

    long size = sim_fsize(uptr->fileref);
    if (size < 0)
        size = 0;
    sagem_state[unitnb].total_sectors = (uint32)(size / SAGEM_SECTOR_SIZE);
    if (sagem_state[unitnb].total_sectors == 0)
        sagem_state[unitnb].total_sectors = SAGEM_TRACKS * SAGEM_SECTORS_PER_TRACK;
    uptr->capac = sagem_state[unitnb].total_sectors * SAGEM_SECTOR_SIZE;

    sagem_state[unitnb].me     = 0;
    sagem_state[unitnb].status = 0;
    sagem_state[unitnb].active = 0;
    return SCPE_OK;
}

t_stat sagem_detach(UNIT *uptr)
{
    int unitnb = sagem_unit_index(uptr);
    if (unitnb < 0)
        return SCPE_IERR;

    sagem_state[unitnb].image  = NULL;
    sagem_state[unitnb].active = 0;
    return detach_unit(uptr);
}

/* WD handler (E=5) - Write AP to &3D */
t_stat sagem_wd(uint16 e_reg, uint16 a_val)
{
    uint8 unitnb = 0; // FIXME
	
    if (unitnb < 0 || unitnb >= 2) 
    	return SCPE_IERR;
    	
    if (e_reg != 5) 
    	return SCPE_IOERR;
    	
    /* Write AP to register &3D (emulated) */
    write_word(0x3D, a_val);
    
    /* Decode AP: bits 1-7 = track, bits 8-11 = sector, bits 14-15 = mode */
    uint32 track  = (a_val >> 8) & 0x7F;
    uint32 sector = (a_val >> 4) & 0x0F;
    uint32 mode   = a_val & 0x03;

    if (mode == 0) {   /* rest – do nothing */
        return SCPE_OK;
    }

    /* Read AD (&3B) and CM (&3C) */
    uint16 ad = read_word(0x3B);
    uint16 cm = read_word(0x3C);
    uint32 mem_addr = ad + 2;
    uint32 byte_count = cm * 2;
    
    if (cm == 0) {
        sagem_state[unitnb].me = 0x1000;   /* CN zero word count (bit12) */
        sagem_state[unitnb].status = 0x02;
        sagem_interrupt();
        return SCPE_OK;
    }
    
    sagem_start_transfer(unitnb, mem_addr, byte_count, track, sector, mode, 0);
    return SCPE_OK;
}

/* RD handler (E=3) – returns status ER bits */
t_stat sagem_rd(uint16 e_reg, uint16 *result)
{
    if (e_reg != 3) 
    	return SCPE_IOERR;
    	
    uint8 unitnb = 0; // FIXME
	
    if (unitnb < 0 || unitnb >= 2) 
    	return SCPE_IERR;
    *result = sagem_state[unitnb].status;
    sagem_state[unitnb].status = 0;
    return SCPE_OK;
}

/* Read ME register (used by CPU after interrupt) */
uint16 sagem_get_me(void)
{
    uint8 unitnb = 0; // FIXME
	
    if (unitnb < 0 || unitnb >= 2) 
    	return SCPE_IERR;
    return sagem_state[unitnb].me;
}

/* Reset device */
t_stat sagem_reset(DEVICE *dptr)
{
    uint8 unitnb = 0; // FIXME
	
    if (unitnb < 0 || unitnb >= 2) 
    	return SCPE_IERR;
    sagem_state[unitnb].active = 0;
    sagem_state[unitnb].me = 0;
    sagem_state[unitnb].status = 0;
    return SCPE_OK;
}

/* ========== UNIT Definition ========== */
#define SAGEM_CAPACITY ( SAGEM_TRACKS * SAGEM_SECTORS_PER_TRACK * SAGEM_SECTOR_SIZE) /* bytes */

UNIT sagem_unit[SAGEM_NUM_UNITS] = {
    { UDATA(NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_DISABLE, SAGEM_CAPACITY) },
    { UDATA(NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_DISABLE, SAGEM_CAPACITY) }
};

/* ========== MTAB for SET/SHOW ========== */
MTAB sagem_mod[] = {
    { MTAB_XTD | MTAB_VDV, 0, "FORMAT", "FORMAT", 
      &sim_disk_set_fmt, &sim_disk_show_fmt, NULL, "Disk format" },
    { MTAB_XTD | MTAB_VUN, 0, "CAPACITY", "CAPACITY",
      &sim_disk_set_capac, &sim_disk_show_capac, NULL, "Disk capacity" },
    { 0 }
};

/* ========== REGISTER Definitions ========== */
REG sagem_reg[SAGEM_NUM_UNITS * 2 + 1] = {
    /* Unit 0 state */
    { ORDATA("STATUS0", sagem_state[0].status, 16) },
    { ORDATA("SECTOR0", sagem_state[0].sector, 32) },
    /* Unit 1 state */
    { ORDATA("STATUS1", sagem_state[1].status, 16) },
    { ORDATA("SECTOR1", sagem_state[1].sector, 32) },
    { NULL }
};

/* ========== DEVICE Structure ========== */
DEVICE sagem_dev = {
    "SAGEM",         /* name (was "DRI", which duplicated the DRI device's name) */
    sagem_unit,        /* units */
    sagem_reg,         /* registers */
    sagem_mod,         /* modifiers */
    SAGEM_NUM_UNITS, /* numunits */
    8,               /* aradix */
    16,              /* awidth */
    1,               /* aincr */
    8,               /* dradix */
    16,              /* dwidth */
    NULL,            /* examine (sagem_ex was never implemented) */
    NULL,            /* deposit (sagem_dep was never implemented) */
    &sagem_reset,      /* reset */
    NULL,            /* boot */
    &sagem_attach,     /* attach */
    &sagem_detach,     /* detach */
    NULL,            /* ctxt */
    DEV_DISK | DEV_DISABLE,  /* flags */
    0,               /* dctrl */
    NULL,            /* debflags */
    NULL,            /* msize */
    NULL,            /* lname */
    NULL,            /* help */
    NULL,            /* attach_help */
    NULL,            /* help_ctxt */
    NULL             /* description */
};
