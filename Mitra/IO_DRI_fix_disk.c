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
	 a _poll function for asynchronous transfers, (not used 
	 interrupt generation via int_req, 
	 and use the memory access helpers (read_byte_io, write_byte_io, read_word, write_word). 
	 The device state is stored in static structures, 
	 and attach/detach functions are provided for file‑based devices.

*/

/*
 * IO_DRI_fix_disk.c: DRI Disk Controller for CII Mitra-15
 * 
 * This module simulates the DRI disk controller (coupleur) which interacts 
 * with the CPU via memory-mapped general registers (&38 to &3F) and uses 
 * the Mitra-15 "suspension" mechanism to trigger the microprogram during 
 * the transfer phases.
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

/* ====================================================================== */
/* Geometry & Constants                                                   */
/* ====================================================================== */
#define DRI_SECTOR_SIZE         256
#define DRI_WORDS_PER_SECTOR    (DRI_SECTOR_SIZE / 2)   /* 128 words */
#define DRI_SECTORS_PER_TRACK   24
#define DRI_TRACKS_PER_CYL      2
#define DRI_CYLINDERS           203
#define DRI_TOTAL_TRACKS        (DRI_CYLINDERS * DRI_TRACKS_PER_CYL)
#define DRI_NUM_UNITS           2

/* Memory-mapped General Registers (CPU addresses) */
#define DRI_REG_RTI             0x38
#define DRI_REG_ADM             0x39    /* Buffer address - 2 */
#define DRI_REG_CM              0x3A    /* Word count */
#define DRI_REG_Q               0x3B    /* Microprogram work register */
#define DRI_REG_ADS             0x3C    /* Disk address (Cyl/Head/Sect/Drive) */
#define DRI_REG_PL              0x3D    /* Longitudinal parity */
#define DRI_REG_ADR             0x3E    /* Return sector address */
#define DRI_REG_RT2             0x3F

/* Status Word Bits (as per documentation)
         0     1    2   3    4     5    6    7    8   9    10   11   12 
       +----+----+----+----+----+----+----+----+----+----+----+----+----+
  A    | 01   02   03   E    M1   M2   RT  EPL  INC   OPE  ECA      VPE | 
       +----+----+----+----+----+----+----+----+----+----+----+----+----+
 */
#define DRI_STS_PHASE_MASK      0xE000  /* Bits 0-2: Phase (0-7) */
#define DRI_STS_E               0x1000  /* Bit 3: Error summary */
#define DRI_STS_MODE_MASK       0x0C00  /* Bits 4-5: Mode (01=Write, 10=Read, 11=Compare) */
#define DRI_STS_RT              0x0200  /* Bit 6: Transfer/Rhythm error */
#define DRI_STS_EPL             0x0100  /* Bit 7: Parity/Read error */
#define DRI_STS_INC             0x0080  /* Bit 12: Write protect violation */
#define DRI_STS_OPE             0x0040  /* Bit 9: Not operational */
#define DRI_STS_ECA             0x0020  /* Bit 8: Address comparison error */
#define DRI_STS_VPE             0x0008  /* Bit 12: Write protect violation */

#define DRI_STS_M1              0x0800  /* Bit 10: Hard error 1 */
#define DRI_STS_M2              0x0400  /* Bit 11: Hard error 2 */

/* Declared in sim_disk.h; forward-declared here to avoid include-path issues */
extern t_stat sim_disk_set_fmt(UNIT *uptr, int32 val, const char *cptr, void *desc);
extern t_stat sim_disk_show_fmt(FILE *st, UNIT *uptr, int32 val, const void *desc);
extern t_stat sim_disk_set_capac(UNIT *uptr, int32 val, const char *cptr, void *desc);
extern t_stat sim_disk_show_capac(FILE *st, UNIT *uptr, int32 val, const void *desc);

void dri_set_error(int unit, uint16 error_bits);
t_stat dri_rd(uint16 inst);
t_stat dri_wd(uint16 inst);

/* Memory Access Functions (defined in mitra_cpu.h) */
extern t_value read_word(t_addr va);
extern void write_word(t_addr va, t_value val);
extern uint8 read_byte(t_addr va);
extern void write_byte(t_addr va, uint8 val);

/* Suspension level assigned to DRI controller (must match io_suspension_dispatch) */
#define DRI_SUSP_LEVEL          0

/* ====================================================================== */
/* Controller State                                                       */
/* ====================================================================== */
typedef struct {
    FILE *image;
    uint32 total_sectors;
    uint16 status;         /* Current status word (includes phase in bits 0-2) */
    uint32 selected_unit;    /* 0 or 1 */
    uint32 operation;        /* 0=read, 1=write, 2=compare */
    bool    active;         /* Transfer in progress */
    uint32 mem_addr;
    uint32 bytes_left;
    int    zio;
    uint16 cur_ads;          /* current ADS (updated after each sector) */
    uint16 susp_level;
} DRI_UNIT;

/* ========== UNIT Definition ========== */
#define DRI_CAPACITY (DRI_CYLINDERS * DRI_TRACKS_PER_CYL * DRI_SECTORS_PER_TRACK * DRI_SECTOR_SIZE) /* bytes */

UNIT dri_unit[DRI_NUM_UNITS] = {
    { UDATA(NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_DISABLE, DRI_CAPACITY) },
    { UDATA(NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_DISABLE, DRI_CAPACITY) }
};

static DRI_UNIT dri_state[DRI_NUM_UNITS];      /* two units */
static uint32 last_selected = 0;   /* last unit selected by WD E=3 */

/* Convert ADS to logical sector number */
static uint32 ads_to_sector(uint16 ads)
{
    /* According to doc: bits 15-7 = cylinder, bit 6 = head, bits 5-1 = sector, bit 0 = drive */
    uint32 cylinder = (ads >> 7) & 0x1FF;   /* bits 1-9 */
    uint32 head     = (ads >> 6) & 1;       /* bit 10? Actually T = bit 6 (0‑based) */
    uint32 sector   = (ads >> 1) & 0x1F;    /* bits 2-6? */
    return ((cylinder * DRI_TRACKS_PER_CYL + head) * DRI_SECTORS_PER_TRACK + sector);
}

/* Convert logical sector number back to ADS format */
static uint16 sector_to_ads(uint32 sector, uint16 drive) {
    uint32 cyl = sector / (DRI_TRACKS_PER_CYL * DRI_SECTORS_PER_TRACK);
    uint32 head = (sector / DRI_SECTORS_PER_TRACK) % DRI_TRACKS_PER_CYL;
    uint32 sec = sector % DRI_SECTORS_PER_TRACK;
    return (cyl << 7) | (head << 6) | (sec << 1) | (drive & 1);
}

/* ====================================================================== */
/* Interrupt & Suspension Handling                                        */
/* ====================================================================== */
/* Generate a standard interrupt (IT) at the end of transfer */
static void dri_interrupt(int unit) {
    uint32 int_req = (1 << (7 + unit));   /* Level 7 for unit0, 8 for unit1 */
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
/* Derive unit index from the UNIT pointer that SIMH passes */
static int dri_unit_index(UNIT *uptr)
{
    int idx = (int)(uptr - ((UNIT *)dri_unit));
    if (idx < 0 || idx >= DRI_NUM_UNITS)
        return -1;
    return idx;
}

/* ====================================================================== */
/* Command Handlers (WD / RD)                                             */
/* ====================================================================== */
/*
* A wrapper function to manage RD or WD instruction execution
* - matching dio_handler_t: t_stat xxx_dio(uint16 inst, t_bool is_write),
* - that reads cpu_state.reg_E/reg_A and 
* - calls the device's own _wd/_rd function, writing results back into cpu_state.reg_A for RD.
*/
t_stat dri_dio_handler(uint16 inst, t_bool is_write) {
    if(is_write) {
	return dri_wd(inst);
	}
    else {
	 return dri_rd(inst);
	 }
}

/* WD handler */
t_stat dri_wd(uint16 inst)
{
    switch (cpu_state.reg_E) {
        /*
        the spec's unit/operation selection packs two different bytes of A — "left byte" (high byte) selects the unit (&80=unit0, &40=unit1) 
        and "right byte" (low byte) selects the operation (&80=write, &00=read, &F0=compare).
	*/
        case 3:   /* Selection */
            if (((cpu_state.reg_A >> 8) & 0xFF) & 0x80) {
                last_selected = 0;
                dri_state[0].operation = (cpu_state.reg_A & 0x0080) ? 1 : 0; /* write if bit7 set */
            } else if (((cpu_state.reg_A >> 8) & 0xFF) & 0x40) {
                last_selected = 1;
                dri_state[1].operation = (cpu_state.reg_A & 0x0080) ? 1 : 0;
            } else {
                return SCPE_IOERR;
            }
            /* Right byte controls operation */
            if ((cpu_state.reg_A & 0xFF) == 0x80) 
            	dri_state[last_selected].operation = 1;  /* write */
            else 
            if ((cpu_state.reg_A) == 0xF0) 
            	dri_state[last_selected].operation = 2; /* compare */
            else 
            	dri_state[last_selected].operation = 0;  /* read */
            break;
        case 5:   /* FIXME
        * Control / Reset 
        * §2.3.2.3, WD E=5, A=&2000, "Retour des têtes à zéro") and the write-protect 3-command sequence (§2.1.8) are not implemented; 
        * dri_wd()'s case 5 only recognizes A==0x80.
        */
            if (cpu_state.reg_A == 0x80) {
                /* Reset controller (RAZ) */
                dri_state[last_selected].status = 0;
                dri_state[last_selected].active = false;
            }
            break;
        case 0x15: /* Start transfer */
            {
                DRI_UNIT *d = &dri_state[last_selected];
                if (d->active) return SCPE_OK; /* Already busy */
                
                /* Store ADS in general register */
                write_word(DRI_REG_ADS, cpu_state.reg_A);
                
                /* Start the microprogram sequence */
                d->active = true;
                d->status = 0; /* Phase 0 */
                mitra_suspension_request(d->susp_level);
            }
            break;
        default:
            return SCPE_IOERR;
    }
    return SCPE_OK;
}

/* RD handler (status) */
t_stat dri_rd(uint16 inst)
{
    if (cpu_state.reg_E != 0x15) 
    	return SCPE_IOERR;
    	
    cpu_state.reg_A = dri_state[last_selected].status; // result in A
    dri_state[last_selected].status = 0;
    return SCPE_OK;
}

/* 
 * Suspension Handler: Simulates the controller's microprogram.
 * Called by the CPU's suspension mechanism when the controller requests it.
 */
void dri_suspension_handler(int unit) {
    DRI_UNIT *d = &dri_state[unit];
    if (!d->active) return;

    uint16 phase = d->status & DRI_STS_PHASE_MASK;
    uint16 cm    = read_word(DRI_REG_CM);
    uint16 ads   = read_word(DRI_REG_ADS);
    uint16 adm   = read_word(DRI_REG_ADM);
    uint32 sector = ads_to_sector(ads);

    switch (phase) { // FIXME
        case 0: /* Phase 0: Init / Rest */
            if (cm <= 0) {
                dri_set_error(unit, DRI_STS_EPL); /* CM <= 0 error */
                return;
            }
            d->status = (d->status & ~DRI_STS_PHASE_MASK) | 1;
            mitra_suspension_request(d->susp_level);
            break;

        case 1: /* 
        * Phase 1 (address check + bad-track indicator read, §2.1.5 / §3.3.5): entirely stubbed as "always successful." 
        * No comparison against the on-disk sector header, no ECA generation, and no branch to phase 7 on a FFFF bad-track indicator.
        */
            d->status = (d->status & ~DRI_STS_PHASE_MASK) | 2;
            mitra_suspension_request(d->susp_level);
            break;

        case 2: /* Phase 2: Init transfer (Setup Q register) */
            {
                uint16 q;
                if (cm < DRI_WORDS_PER_SECTOR) {
                    q = 0 - cm;             /* Negative word count */
                    cm = 0 - (DRI_WORDS_PER_SECTOR - cm); /* Negative zero padding count */
                } else if (cm == DRI_WORDS_PER_SECTOR) {
                    q = 0 - DRI_WORDS_PER_SECTOR;
                    cm = 0;
                } else {
                    q = 0 - DRI_WORDS_PER_SECTOR;
                    cm -= DRI_WORDS_PER_SECTOR;
                }
                write_word(DRI_REG_Q, q);
                write_word(DRI_REG_CM, cm);
            }
            d->status = (d->status & ~DRI_STS_PHASE_MASK) | 3;
            mitra_suspension_request(d->susp_level);
            break;

        case 3: /* Phase 3: Transfer P/S address (Simulated) */
            d->status = (d->status & ~DRI_STS_PHASE_MASK) | 4;
            mitra_suspension_request(d->susp_level);
            break;

        case 4: /* 
        * Phase 4: Transfer data (128 words) 
        * moves the whole 128-word sector at once via read_word/write_word rather than the word-by-word RT1-pipelined shuffle the manual describes.
        * never computes or writes PL (longitudinal parity, register &3D). That means EPL can never trigger from real data corruption, and "contrôle écriture"
        * (operation 2 / compare mode) isn't distinguished from a plain read at all: operation==1 is write, 
        * everything else is treated as a normal read into memory, so "compare" silently overwrites memory instead of just checking parity (§2.1.10).
        */
            {
                uint32 mem_addr = adm + 2; /* ADM is buffer address - 2 */
                uint32 offset = sector * DRI_SECTOR_SIZE;
                
                if (d->operation == 1) { /* Write */
                    uint8 buf[DRI_SECTOR_SIZE];
                    for (int i = 0; i < DRI_WORDS_PER_SECTOR; i++) {
                        uint16 w = read_word(mem_addr + i);
                        buf[i*2]     = (w >> 8) & 0xFF;
                        buf[i*2 + 1] = w & 0xFF;
                    }
                    fseek(d->image, offset, SEEK_SET);
                    fwrite(buf, 1, DRI_SECTOR_SIZE, d->image);
                } else { /* Read / Compare */
                    uint8 buf[DRI_SECTOR_SIZE];
                    fseek(d->image, offset, SEEK_SET);
                    fread(buf, 1, DRI_SECTOR_SIZE, d->image);
                    for (int i = 0; i < DRI_WORDS_PER_SECTOR; i++) {
                        uint16 w = (buf[i*2] << 8) | buf[i*2 + 1];
                        write_word(mem_addr + i, w);
                    }
                }
                
                /* Update ADM for next sector */
                write_word(DRI_REG_ADM, mem_addr + DRI_WORDS_PER_SECTOR - 2);
            }
            d->status = (d->status & ~DRI_STS_PHASE_MASK) | 6; /* Skip phase 5 for simplicity */
            mitra_suspension_request(d->susp_level);
            break;

        case 5: /* Phase 5: Write zeros (Only for write, skipped in this simplified model) */
            d->status = (d->status & ~DRI_STS_PHASE_MASK) | 6;
            mitra_suspension_request(d->susp_level);
            break;

        case 6: /* Phase 6: Parity / End of sector */
            cm = read_word(DRI_REG_CM);
            if (cm == 0) {
                /* Transfer complete */
                d->active = false;
                d->status = 0; /* Success, clear phase and errors */
                dri_interrupt(unit);
            } else {
                /* More sectors to transfer: Update ADS and loop back to Phase 0 */
                uint32 next_sector = sector + 1;
                uint16 new_ads = sector_to_ads(next_sector, ads & 1);
                write_word(DRI_REG_ADS, new_ads);
                d->status = (d->status & ~DRI_STS_PHASE_MASK) | 0;
                mitra_suspension_request(d->susp_level);
            }
            break;

        case 7: /* 
        * Phase 7: Bad track handling (Simulated) 
        * the manual says read the two copies of the reroute address, compare them, and if equal, resume in phase 0 at the new address; only mismatch is an error. 
        * The code unconditionally raises EPL — it never implements the redirection at all (currently unreachable anyway since phase 1 is stubbed).
        */
            dri_set_error(unit, DRI_STS_EPL);
            break;
    }
}

/* Set error bits in status word and generate IT */
void dri_set_error(int unit, uint16 error_bits) {
    dri_state[unit].status |= error_bits | DRI_STS_E;
    dri_state[unit].active = false;
    dri_interrupt(unit);
}

/* ====================================================================== */
/* SIMH Device Integration                                                */
/* ====================================================================== */
t_stat dri_reset(DEVICE *dptr) {
    for (int i = 0; i < DRI_NUM_UNITS; i++) {
        dri_state[i].active = false;
        dri_state[i].status = 0;
        dri_state[i].susp_level = DRI_SUSP_LEVEL;
    }
    return SCPE_OK;
}

/* Poll all units */
void dri_poll_devices(void)
{
    for (int i = 0; i < 2; i++)
        dri_poll(i);
}

t_stat dri_attach(UNIT *uptr, const char *cptr) {
    t_stat r;
    int unitnb = dri_unit_index(uptr);
    if (unitnb < 0) return SCPE_IERR;

    r = attach_unit(uptr, cptr);
    if (r != SCPE_OK) return r;

    dri_state[unitnb].image = uptr->fileref;
    long size = sim_fsize(uptr->fileref);
    dri_state[unitnb].total_sectors = (size > 0) ? (uint32)(size / DRI_SECTOR_SIZE) : 
                                      (DRI_CYLINDERS * DRI_TRACKS_PER_CYL * DRI_SECTORS_PER_TRACK);
    uptr->capac = dri_state[unitnb].total_sectors * DRI_SECTOR_SIZE;
    dri_state[unitnb].status = 0;
    dri_state[unitnb].active = false;
    return SCPE_OK;
}

t_stat dri_detach(UNIT *uptr) {
    int unitnb = dri_unit_index(uptr);
    if (unitnb < 0) return SCPE_IERR;
    dri_state[unitnb].image = NULL;
    dri_state[unitnb].active = false;
    return detach_unit(uptr);
}

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

/* ========== DEVICE Structure ========== 
* For a device to respond to RD or WD instructions, its dib_t must define two specific fields:
*
*    - dio: The "Mode" or index (0 to DIO_N_MOD - 1) that this device claims.
*    - dio_disp: A pointer to the C function that will handle the RD/WD instructions for this specific mode.
*
* When you add a new device (e.g., via the SIMH ATTACH or SET commands) that utilizes Direct I/O, it becomes part of the sim_devices list. 
* The next time io_init() runs (usually upon a system reset or boot), it will find the device's dib_t, read its dio mode, and insert 
* its specific handler function into the dio_disp table. 
* From that point on, any RD or WD instruction targeting that mode will be routed to the new device's code.
*/
t_stat dri_dio_handler(uint16 inst, t_bool is_write); // RD and WD wrapper

dib_t dri_dib = {
    0,                  // dva (not used for RD/WD, or set to a dummy channel/dev)
    NULL,               // disp (not used for RD/WD)
    0x15,               // dio: The "Mode" this device claims
    dri_dio_handler     // dio_disp: The handler function
};

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
    &dri_dib,        /* ctxt */
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
