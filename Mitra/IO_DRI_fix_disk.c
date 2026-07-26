/* 
* DRI Disk 

256 bytes per sector.
24 sectors per track.
2 tracks per cylinder.
203 cylinders per unit.

Track Registers
	&39 ADM
	&3A NSNM or CM
	&3C ADS
	&3E ADR = 0
where:
	ADR is memory address -2 of the first word to transfer.
	NSNX (or CM) is number of words to transfer.
	ADS is disk sector track address.
	ADR is initialized to zero.

ADS
         0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
       +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 A     |  |     cylindre          | T|    secteur   |D |
       +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
          |      piste               |									 
          |--+--+--+--+--+--+--+--+--|

Track: The number of the track to be read or written.
Sector: The sector number - 1 modulo 24.
T: Head

	0: Lower head
	1: Upper head

D:

	0: Removable disk
	1: Fixed disk

Example:

	Track 1, Sector 4, ADS = &46
	Track 1, Sector 0, ADS = &68

After the transfer, the progress bar is stored in ADS;

Example:
	Write track 1 sector 23 given ADS = &6C
	End transfer rendered ADS = &6E

Track: number of the last track read or written.
Sector: number of the last sector read or written.

Example:
	Write next sector (from the previous example)
	Track 2 sector 0 gives ADS = &AE
	End transfer rendered ADS = &BU

Therefore, there is a discontinuity in the progression of ADS.

Command:

I1 There are 2 chained command WDs

I) Address selection WD E = 3
	Left byte of A A = &80 unit 0
	Left byte of A A = &40 unit 1

	Right byte of A A = &80 write
	Right byte of A A = &00 read
	Right byte of A A = &F0 compare write

2) Transfer WD
	E &15
	A ADS

Put the coupler to rest
	E &5
	A &80

Read the status word by RD
	Any A
	E = &15
	RD

Status word returned to A

         0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
       +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
  A    |        |  |     |                    |        |
       +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

	Bit 12: Disk memory protection violation.
	Bits 10, 11: Disk (Hard) malfunction.
	Bit 9: Disk not operational.
	Bit 8: Disk (Hard) malfunction.
	Bits 6, 7: Transfer error.
	Bit 3: Bit 3 summarizes errors.
		0: No error.
		1: Error.

Registers (memory‑mapped):
   &39 ADM   : memory address -2 (words)
   &3A NSNM  : number of words to transfer
   &3C ADS   : disk address (cylinder, head, sector, drive)
   &3E ADR   : always 0

WD E=3 : select unit (A left byte &80=unit0, &40=unit1) and operation (right byte &80=write, &00=read, &F0=compare)
WD E=5 : rest (A=&80)
WD E=&15: start transfer (A=ADS)

RD E=&15: read status word
   bit12: memory protection violation
   bits10‑11: hard malfunction
   bit9: not operational
   bit8: hard malfunction
   bits6‑7: transfer error
   bit3: error summary
   
All devices will follow the same integration pattern, they provide:
	 _wd and _rd handlers, 
	 a _poll function for asynchronous transfers, 
	 interrupt generation via int_req, 
	 and use the memory access helpers (read_byte_io, write_byte_io, read_word, write_word). 
	 The device state is stored in static structures, 
	 and attach/detach functions are provided for file‑based devices.

*/

/*
 * DRI DISK (MITRA-15)
 *
 * Geometry: 256 bytes/sector, 24 sectors/track, 2 tracks/cylinder, 203 cylinders.
 * Registers (memory‑mapped):
 *   &39 ADM – memory address -2 (words)
 *   &3A NSNM – number of words to transfer
 *   &3C ADS – disk address (cylinder, head, sector, drive)
 *   &3E ADR – always 0
 *
 * Commands:
 *   WD E=3 – selection: left byte &80=unit0, &40=unit1; right byte &80=write, &00=read, &F0=compare
 *   WD E=5, A=&80 – rest
 *   WD E=&15, A=ADS – start transfer
 *
 * Status RD (E=&15):
 *   bit12 – memory protection violation
 *   bits10‑11 – hard malfunction
 *   bit9 – not operational
 *   bit8 – hard malfunction
 *   bits6‑7 – transfer error
 *   bit3 – error summary (1=error)
 */

#include "mitra_defs.h"
#include "mitra_cpu.h"
#include "mitra_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define DRI_SECTOR_SIZE      256
#define DRI_SECTORS_PER_TRACK 24
#define DRI_TRACKS_PER_CYL    2
#define DRI_CYLINDERS        203
#define DRI_WORDS_PER_SECTOR (DRI_SECTOR_SIZE / 2)   /* 128 */
#define DRI_NUM_UNITS        2

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

extern uint32 intrp_level;  /* interrupt request bits */

typedef struct {
    FILE *image;
    uint32 total_sectors;
    uint16 status;
    uint32 selected_unit;    /* 0 or 1 */
    uint32 operation;        /* 0=read, 1=write, 2=compare */
    int    active;
    uint32 mem_addr;
    uint32 bytes_left;
    int    zio;
    uint16 cur_ads;          /* current ADS (updated after each sector) */
} DRI_UNIT;

static DRI_UNIT dri_state[2];      /* two units */
static uint32 last_selected = 0;   /* last unit selected by WD E=3 */

/* Convert ADS to logical sector number */
static uint32 ads_to_sector(uint16 ads)
{
    uint32 cylinder = (ads >> 7) & 0x1FF;   /* bits 1-9 */
    uint32 head     = (ads >> 6) & 1;       /* bit 10? Actually T = bit 6 (0‑based) */
    uint32 sector   = (ads >> 1) & 0x1F;    /* bits 2-6? */
    /* According to doc: bits 15-7 = cylinder, bit 6 = head, bits 5-1 = sector, bit 0 = drive */
    cylinder = (ads >> 7) & 0x1FF;
    head     = (ads >> 6) & 1;
    sector   = (ads >> 1) & 0x1F;
    return ((cylinder * DRI_TRACKS_PER_CYL + head) * DRI_SECTORS_PER_TRACK + sector);
}

static void dri_interrupt(int unit)
{
    uint32 int_req = (1 << (7 + unit));   /* level 7 for unit0, 8 for unit1 */
    io_interrupt_dispatch(int_req, false);
}

/* Start a transfer on a given unit */
static void dri_start_transfer(int unit, uint32 mem_addr, uint32 byte_count, uint16 ads, int op, int zio)
{
    DRI_UNIT *d = &dri_state[unit];
    if (d->active) return;

    uint32 sector = ads_to_sector(ads);
    if (sector >= d->total_sectors) {
        d->status = 0x0200;   /* address error (bit9?) Actually use bit10-11 = hard error */
        d->status = 0x0400;   /* bit10 =1 */
        dri_interrupt(unit);
        return;
    }

    d->active = 1;
    d->mem_addr = mem_addr;
    d->bytes_left = byte_count;
    d->cur_ads = ads;
    d->operation = op;
    d->zio = zio;
    /* Transfer will be done in dri_poll() */
}

int dri_poll(int unit)
{
    DRI_UNIT *d = &dri_state[unit];
    if (!d->active) return 0;

    FILE *f = d->image;
    uint32 sector = ads_to_sector(d->cur_ads);
    uint32 offset = sector * DRI_SECTOR_SIZE;

    if (d->operation == 0) {   /* read */
        if (fseek(f, offset, SEEK_SET) != 0) {
            d->active = 0;
            d->status = 0x0C00;   /* bits10-11 = 11 (hard error) */
            dri_interrupt(unit);
            return 1;
        }
        uint32 bytes_to_do = (d->bytes_left < DRI_SECTOR_SIZE) ? d->bytes_left : DRI_SECTOR_SIZE;
        uint8 buf[DRI_SECTOR_SIZE];
        if (fread(buf, 1, bytes_to_do, f) != bytes_to_do) {
            d->active = 0;
            d->status = 0x0C00;
            dri_interrupt(unit);
            return 1;
        }
        for (uint32 i = 0; i < bytes_to_do; i++)
            write_byte_io(d->mem_addr + i, buf[i], d->zio);
        d->mem_addr += bytes_to_do;
        d->bytes_left -= bytes_to_do;
    } else if (d->operation == 1) {   /* write */
        if (fseek(f, offset, SEEK_SET) != 0) {
            d->active = 0;
            d->status = 0x0C00;
            dri_interrupt(unit);
            return 1;
        }
        uint32 bytes_to_do = (d->bytes_left < DRI_SECTOR_SIZE) ? d->bytes_left : DRI_SECTOR_SIZE;
        uint8 buf[DRI_SECTOR_SIZE];
        for (uint32 i = 0; i < bytes_to_do; i++)
            read_byte_io(d->mem_addr + i, &buf[i], d->zio);
        if (fwrite(buf, 1, bytes_to_do, f) != bytes_to_do) {
            d->active = 0;
            d->status = 0x0C00;
            dri_interrupt(unit);
            return 1;
        }
        d->mem_addr += bytes_to_do;
        d->bytes_left -= bytes_to_do;
    } else {   /* compare – not implemented, report error */
        d->active = 0;
        d->status = 0x1000;   /* memory protection violation */
        dri_interrupt(unit);
        return 1;
    }

    /* Update ADS to next sector */
    uint32 next_sector = sector + 1;
    uint32 new_cyl = next_sector / (DRI_TRACKS_PER_CYL * DRI_SECTORS_PER_TRACK);
    uint32 new_head = (next_sector / DRI_SECTORS_PER_TRACK) % DRI_TRACKS_PER_CYL;
    uint32 new_sec = next_sector % DRI_SECTORS_PER_TRACK;
    d->cur_ads = (new_cyl << 7) | (new_head << 6) | (new_sec << 1) | (d->cur_ads & 1);

    if (d->bytes_left == 0) {
        d->active = 0;
        d->status = 0;   /* success */
        dri_interrupt(unit);
    }
    return 1;
}

/* Attach a disk image file for a given unit */
t_stat dri_attach(UNIT *unit, const char *filename)
{
	uint8 unitnb = 1; // FIXME
	
    if (unitnb < 0 || unitnb >= 2) return SCPE_IERR;
    if (dri_state[unitnb].image) fclose(dri_state[unitnb].image);
    dri_state[unitnb].image = fopen(filename, "rb+");
    if (!dri_state[unitnb].image) dri_state[unitnb].image = fopen(filename, "wb+");
    if (!dri_state[unitnb].image) return SCPE_IOERR;
    fseek(dri_state[unitnb].image, 0, SEEK_END);
    long size = ftell(dri_state[unitnb].image);
    dri_state[unitnb].total_sectors = size / DRI_SECTOR_SIZE;
    if (dri_state[unitnb].total_sectors == 0)
        dri_state[unitnb].total_sectors = DRI_CYLINDERS * DRI_TRACKS_PER_CYL * DRI_SECTORS_PER_TRACK;
    fseek(dri_state[unitnb].image, 0, SEEK_SET);
    dri_state[unitnb].status = 0;
    return SCPE_OK;
}

t_stat dri_detach(UNIT *unit)
{
	uint8 unitnb = 1; // FIXME
	
    if (dri_state[unitnb].image) fclose(dri_state[unitnb].image);
    dri_state[unitnb].image = NULL;
    dri_state[unitnb].active = 0;
    return SCPE_OK;
}

/* WD handler */
t_stat dri_wd(uint16 e_reg, uint16 a_val)
{
    switch (e_reg) {
        case 3:   /* Selection */
            if (a_val & 0x80) {
                last_selected = 0;
                dri_state[0].operation = (a_val & 0x0080) ? 1 : 0; /* write if bit7 set */
            } else if (a_val & 0x40) {
                last_selected = 1;
                dri_state[1].operation = (a_val & 0x0080) ? 1 : 0;
            } else {
                return SCPE_IOERR;
            }
            /* Right byte controls operation */
            if (a_val & 0x80) dri_state[last_selected].operation = 1;  /* write */
            else if (a_val & 0xF0) dri_state[last_selected].operation = 2; /* compare */
            else dri_state[last_selected].operation = 0;  /* read */
            break;
        case 5:   /* Rest */
            if (a_val == 0x80) {
                dri_state[last_selected].status = 0;
                dri_state[last_selected].active = 0;
            }
            break;
        case 0x15: /* Start transfer */
            {
                uint16 adm = read_word(0x39);
                uint16 cm  = read_word(0x3A);
                uint32 mem_addr = adm + 2;
                uint32 byte_count = cm * 2;
                dri_start_transfer(last_selected, mem_addr, byte_count, a_val,
                                   dri_state[last_selected].operation, 0);
            }
            break;
        default:
            return SCPE_IOERR;
    }
    return SCPE_OK;
}

/* RD handler (status) */
t_stat dri_rd(uint16 e_reg, uint16 *result)
{
    if (e_reg != 0x15) return SCPE_IOERR;
    *result = dri_state[last_selected].status;
    dri_state[last_selected].status = 0;
    return SCPE_OK;
}

/* Reset all units */
t_stat dri_reset(DEVICE *dptr)
{
    for (int i = 0; i < 2; i++) {
        dri_state[i].active = 0;
        dri_state[i].status = 0;
    }
    return SCPE_OK;
}

/* Poll all units */
void dri_poll_devices(void)
{
    for (int i = 0; i < 2; i++)
        dri_poll(i);
}

/* ========== UNIT Definition ========== */
UNIT dri_unit[DRI_NUM_UNITS] = {
    { UDATA(NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_DISABLE, 0) },
    { UDATA(NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_DISABLE, 0) }
};

/* ========== MTAB for SET/SHOW ========== */
MTAB dri_mod[] = {
    { MTAB_XTD | MTAB_VDV, 0, "FORMAT", "FORMAT", 
      &sim_disk_set_fmt, &sim_disk_show_fmt, NULL, "Disk format" },
    { MTAB_XTD | MTAB_VUN, 0, "CAPACITY", "CAPACITY",
      &sim_disk_set_capac, &sim_disk_show_capac, NULL, "Disk capacity" },
    { 0 }
};

/* ========== REGISTER Definitions ========== */
REG dri_reg[DRI_NUM_UNITS * 2 + 1] = {
    /* Unit 0 state - DRI_UNIT has no "sector" field, use cur_ads (current disk address) */
    { ORDATA("STATUS0", dri_state[0].status, 16) },
    { ORDATA("ADS0", dri_state[0].cur_ads, 16) },
    /* Unit 1 state */
    { ORDATA("STATUS1", dri_state[1].status, 16) },
    { ORDATA("ADS1", dri_state[1].cur_ads, 16) },
    { NULL }
};

/* ========== DEVICE Structure ========== */
DEVICE dri_dev = {
    "DRI",           /* name */
    dri_unit,        /* units */
    dri_reg,         /* registers */
    dri_mod,         /* modifiers */
    DRI_NUM_UNITS,   /* numunits */
    8,               /* aradix */
    16,              /* awidth */
    1,               /* aincr */
    8,               /* dradix */
    16,              /* dwidth */
    NULL,            /* examine (dri_ex was never implemented) */
    NULL,            /* deposit (dri_dep was never implemented) */
    &dri_reset,      /* reset */
    NULL,            /* boot */
    &dri_attach,     /* attach */
    &dri_detach,     /* detach */
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
