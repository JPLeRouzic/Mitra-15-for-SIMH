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

extern uint32 intrp_level;  /* interrupt request bits */

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
static void sagem_start_transfer(uint32 mem_addr, uint32 byte_count, uint32 track, uint32 sector, int mode, int zio)
{
    if (sagem_state[0].active) return;

    uint32 sec = ts_to_sector(track, sector);
    if (sec >= sagem_state[0].total_sectors) {
        sagem_state[0].me = 0x0800;   /* invalid sector address */
        sagem_state[0].status = 0x01;
        sagem_interrupt();
        return;
    }
    if (byte_count == 0) {
        sagem_state[0].me = 0x1000;   /* zero word count */
        sagem_state[0].status = 0x02;
        sagem_interrupt();
        return;
    }

    sagem_state[0].active = 1;
    sagem_state[0].mem_addr = mem_addr;
    sagem_state[0].bytes_left = byte_count;
    sagem_state[0].mode = mode;
    sagem_state[0].track = track;
    sagem_state[0].sector = sector;
    sagem_state[0].zio = zio;
}

int sagem_poll(void)
{
    if (!sagem_state[0].active) return 0;

    uint32 sector = ts_to_sector(sagem_state[0].track, sagem_state[0].sector);
    uint32 offset = sector * SAGEM_SECTOR_SIZE;
    FILE *f = sagem_state[0].image;

    if (sagem_state[0].mode == 2) {   /* read */
        if (fseek(f, offset, SEEK_SET) != 0) {
            sagem_state[0].active = 0;
            sagem_state[0].me = 0x0400;   /* parity error (simulated) */
            sagem_state[0].status = 0x03;
            sagem_interrupt();
            return 1;
        }
        uint32 bytes_to_do = (sagem_state[0].bytes_left < SAGEM_SECTOR_SIZE) ? sagem_state[0].bytes_left : SAGEM_SECTOR_SIZE;
        uint8 buf[SAGEM_SECTOR_SIZE];
        if (fread(buf, 1, bytes_to_do, f) != bytes_to_do) {
            sagem_state[0].active = 0;
            sagem_state[0].me = 0x0400;
            sagem_state[0].status = 0x03;
            sagem_interrupt();
            return 1;
        }
        for (uint32 i = 0; i < bytes_to_do; i++)
            write_byte_io(sagem_state[0].mem_addr + i, buf[i], sagem_state[0].zio);
        sagem_state[0].mem_addr += bytes_to_do;
        sagem_state[0].bytes_left -= bytes_to_do;
    } else if (sagem_state[0].mode == 1) {   /* write */
        if (fseek(f, offset, SEEK_SET) != 0) {
            sagem_state[0].active = 0;
            sagem_state[0].me = 0x0400;
            sagem_state[0].status = 0x03;
            sagem_interrupt();
            return 1;
        }
        uint32 bytes_to_do = (sagem_state[0].bytes_left < SAGEM_SECTOR_SIZE) ? sagem_state[0].bytes_left : SAGEM_SECTOR_SIZE;
        uint8 buf[SAGEM_SECTOR_SIZE];
        for (uint32 i = 0; i < bytes_to_do; i++)
            read_byte_io(sagem_state[0].mem_addr + i, &buf[i], sagem_state[0].zio);
        if (fwrite(buf, 1, bytes_to_do, f) != bytes_to_do) {
            sagem_state[0].active = 0;
            sagem_state[0].me = 0x0400;
            sagem_state[0].status = 0x03;
            sagem_interrupt();
            return 1;
        }
        sagem_state[0].mem_addr += bytes_to_do;
        sagem_state[0].bytes_left -= bytes_to_do;
    } else {
        /* compare or rest – not implemented, treat as error */
        sagem_state[0].active = 0;
        sagem_state[0].me = 0x2000;   /* compare error */
        sagem_state[0].status = 0x02;
        sagem_interrupt();
        return 1;
    }

    /* Advance to next sector if more data remains */
    if (sagem_state[0].bytes_left > 0) {
        sagem_state[0].sector++;
        if (sagem_state[0].sector >= SAGEM_SECTORS_PER_TRACK) {
            sagem_state[0].sector = 0;
            sagem_state[0].track++;
        }
    }

    if (sagem_state[0].bytes_left == 0) {
        sagem_state[0].active = 0;
        sagem_state[0].me = 0x8000;   /* success */
        sagem_state[0].status = 0;
        sagem_interrupt();
    }
    return 1;
}

/* Attach disk image */
t_stat sagem_attach(UNIT *unit, const char *filename)
{
    if (sagem_state[0].image) fclose(sagem_state[0].image);
    sagem_state[0].image = fopen(filename, "rb+");
    if (!sagem_state[0].image) sagem_state[0].image = fopen(filename, "wb+");
    if (!sagem_state[0].image) return SCPE_IOERR;
    fseek(sagem_state[0].image, 0, SEEK_END);
    long size = ftell(sagem_state[0].image);
    sagem_state[0].total_sectors = size / SAGEM_SECTOR_SIZE;
    if (sagem_state[0].total_sectors == 0)
        sagem_state[0].total_sectors = SAGEM_TRACKS * SAGEM_SECTORS_PER_TRACK;
    fseek(sagem_state[0].image, 0, SEEK_SET);
    sagem_state[0].me = 0;
    sagem_state[0].status = 0;
    return SCPE_OK;
}

t_stat sagem_detach(UNIT *unit)
{
    if (sagem_state[0].image) fclose(sagem_state[0].image);
    sagem_state[0].image = NULL;
    sagem_state[0].active = 0;
    return SCPE_OK;
}

/* WD handler (E=5) - Write AP to &3D */
t_stat sagem_wd(uint16 e_reg, uint16 a_val)
{
    if (e_reg != 5) return SCPE_IOERR;
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
        sagem_state[0].me = 0x1000;   /* CN zero word count (bit12) */
        sagem_state[0].status = 0x02;
        sagem_interrupt();
        return SCPE_OK;
    }
    
    sagem_start_transfer(mem_addr, byte_count, track, sector, mode, 0);
    return SCPE_OK;
}

/* RD handler (E=3) – returns status ER bits */
t_stat sagem_rd(uint16 e_reg, uint16 *result)
{
    if (e_reg != 3) return SCPE_IOERR;
    *result = sagem_state[0].status;
    sagem_state[0].status = 0;
    return SCPE_OK;
}

/* Read ME register (used by CPU after interrupt) */
uint16 sagem_get_me(void)
{
    return sagem_state[0].me;
}

/* Reset device */
t_stat sagem_reset(DEVICE *dptr)
{
    sagem_state[0].active = 0;
    sagem_state[0].me = 0;
    sagem_state[0].status = 0;
    return SCPE_OK;
}

/* Poll function */
void sagem_poll_devices(void)
{
    sagem_poll();
}

/* ========== UNIT Definition ========== */
UNIT sagem_unit[SAGEM_NUM_UNITS] = {
    { UDATA(NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_DISABLE, 0) },
    { UDATA(NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_DISABLE, 0) }
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
