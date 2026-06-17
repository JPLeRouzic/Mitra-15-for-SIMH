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

#include "mitra_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DRI_SECTOR_SIZE      256
#define DRI_SECTORS_PER_TRACK 24
#define DRI_TRACKS_PER_CYL    2
#define DRI_CYLINDERS        203
#define DRI_WORDS_PER_SECTOR (DRI_SECTOR_SIZE / 2)   /* 128 */

extern uint32 int_req;               /* interrupt request bits */

/* Memory Access Functions (defined in mitra_cpu.c) */
extern uint16 read_word(uint16 addr);
extern void write_word(uint16 addr, uint16 val);
extern uint8 read_byte(uint16 va);
extern void write_byte(uint16 va, uint8 val);

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

static DRI_UNIT dri[2];      /* two units */
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
    int_req |= (1 << (7 + unit));   /* level 7 for unit0, 8 for unit1 */
    io_interrupt_dispatch();
}

/* Start a transfer on a given unit */
static void dri_start_transfer(int unit, uint32 mem_addr, uint32 byte_count, uint16 ads, int op, int zio)
{
    DRI_UNIT *d = &dri[unit];
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
    DRI_UNIT *d = &dri[unit];
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
t_stat dri_attach(int unit, const char *filename)
{
    if (unit < 0 || unit >= 2) return SCPE_IERR;
    if (dri[unit].image) fclose(dri[unit].image);
    dri[unit].image = fopen(filename, "rb+");
    if (!dri[unit].image) dri[unit].image = fopen(filename, "wb+");
    if (!dri[unit].image) return SCPE_IOERR;
    fseek(dri[unit].image, 0, SEEK_END);
    long size = ftell(dri[unit].image);
    dri[unit].total_sectors = size / DRI_SECTOR_SIZE;
    if (dri[unit].total_sectors == 0)
        dri[unit].total_sectors = DRI_CYLINDERS * DRI_TRACKS_PER_CYL * DRI_SECTORS_PER_TRACK;
    fseek(dri[unit].image, 0, SEEK_SET);
    dri[unit].status = 0;
    return SCPE_OK;
}

void dri_detach(int unit)
{
    if (dri[unit].image) fclose(dri[unit].image);
    dri[unit].image = NULL;
    dri[unit].active = 0;
}

/* WD handler */
t_stat dri_wd(uint16 e_reg, uint16 a_val)
{
    switch (e_reg) {
        case 3:   /* selection */
            if (a_val & 0x80) last_selected = 0;
            else if (a_val & 0x40) last_selected = 1;
            else return SCPE_IOERR;
            if (a_val & 0x80) dri[last_selected].operation = 1;   /* write */
            else if (!(a_val & 0x80) && !(a_val & 0xF0)) dri[last_selected].operation = 0; /* read */
            else if (a_val & 0xF0) dri[last_selected].operation = 2; /* compare */
            break;
        case 5:   /* rest */
            if (a_val == 0x80) {
                /* reset any pending error */
                dri[last_selected].status = 0;
            }
            break;
        case 0x15: /* start transfer */
            {
                uint16 adm = read_word(0x39);
                uint16 cm  = read_word(0x3A);
                uint16 ads = a_val;   /* supplied in A */
                uint32 mem_addr = adm + 2;
                uint32 byte_count = cm * 2;
                dri_start_transfer(last_selected, mem_addr, byte_count, ads,
                                   dri[last_selected].operation, 0);
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
    *result = dri[last_selected].status;
    dri[last_selected].status = 0;
    return SCPE_OK;
}

/* Reset all units */
void dri_reset(void)
{
    for (int i = 0; i < 2; i++) {
        dri[i].active = 0;
        dri[i].status = 0;
    }
}

/* Poll all units */
void dri_poll_devices(void)
{
    for (int i = 0; i < 2; i++)
        dri_poll(i);
}

