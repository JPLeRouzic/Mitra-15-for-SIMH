/*
 * IO_DRI_fix_disk.c: DRI Disk Controller for CII Mitra-15
 * 
 * This module simulates the DRI disk controller (coupleur) which interacts 
 * with the CPU via memory-mapped general registers (&38 to &3F) and uses 
 * the Mitra-15 "suspension" mechanism to trigger the microprogram during 
 * the transfer phases.
 *
 * Geometry: 256 bytes/sector (128 words), 24 sectors/track, 2 tracks/cylinder, 203 cylinders.
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

/* Status Word Bits (as per documentation) */
#define DRI_STS_PHASE_MASK      0x0007  /* Bits 0-2: Phase (0-7) */
#define DRI_STS_E               0x0008  /* Bit 3: Error summary */
#define DRI_STS_MODE_MASK       0x0030  /* Bits 4-5: Mode (01=Write, 10=Read, 11=Compare) */
#define DRI_STS_EPL             0x0040  /* Bit 6: Parity/Read error */
#define DRI_STS_RT              0x0080  /* Bit 7: Transfer/Rhythm error */
#define DRI_STS_ECA             0x0100  /* Bit 8: Address comparison error */
#define DRI_STS_OPE             0x0200  /* Bit 9: Not operational */
#define DRI_STS_M1              0x0400  /* Bit 10: Hard error 1 */
#define DRI_STS_M2              0x0800  /* Bit 11: Hard error 2 */
#define DRI_STS_VPE             0x1000  /* Bit 12: Write protect violation */

/* Suspension level assigned to DRI controller (must match io_suspension_dispatch) */
#define DRI_SUSP_LEVEL          0

/* ====================================================================== */
/* Controller State                                                       */
/* ====================================================================== */
typedef struct {
    FILE    *image;
    uint32  total_sectors;
    uint16  status;         /* Current status word (includes phase in bits 0-2) */
    uint32  operation;      /* 0=read, 1=write, 2=compare */
    bool    active;         /* Transfer in progress */
    uint32  susp_level;     /* Suspension level for this unit */
} DRI_UNIT;

static DRI_UNIT dri_state[DRI_NUM_UNITS];
static uint32 last_selected = 0;

/* Forward declarations */
static void dri_interrupt(int unit);
static void dri_set_error(int unit, uint16 error_bits);
static uint32 ads_to_sector(uint16 ads);
static uint16 sector_to_ads(uint32 sector, uint16 drive);

/* ====================================================================== */
/* Address Conversion                                                     */
/* ====================================================================== */
/* Convert ADS register to logical sector number */
static uint32 ads_to_sector(uint16 ads) {
    uint32 cylinder = (ads >> 7) & 0x1FF;
    uint32 head     = (ads >> 6) & 1;
    uint32 sector   = (ads >> 1) & 0x1F;
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

/* Set error bits in status word and generate IT */
static void dri_set_error(int unit, uint16 error_bits) {
    dri_state[unit].status |= error_bits | DRI_STS_E;
    dri_state[unit].active = false;
    dri_interrupt(unit);
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

    switch (phase) {
        case 0: /* Phase 0: Init / Rest */
            if (cm == 0) {
                dri_set_error(unit, DRI_STS_EPL); /* CM <= 0 error */
                return;
            }
            d->status = (d->status & ~DRI_STS_PHASE_MASK) | 1;
            mitra_suspension_request(d->susp_level);
            break;

        case 1: /* Phase 1: Address check (Simulated as successful) */
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

        case 4: /* Phase 4: Transfer data (128 words) */
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

        case 7: /* Phase 7: Bad track handling (Simulated) */
            dri_set_error(unit, DRI_STS_EPL);
            break;
    }
}

/* ====================================================================== */
/* Command Handlers (WD / RD)                                             */
/* ====================================================================== */
t_stat dri_wd(uint16 inst) {
    uint16 e = cpu_state.reg_E;
    uint16 a = cpu_state.reg_A;

    switch (e) {
        case 0x0003: /* Selection: Unit and Mode */
            {
                uint8 left = (a >> 8) & 0xFF;
                uint8 right = a & 0xFF;
                
                if (left & 0x80) last_selected = 0;
                else if (left & 0x40) last_selected = 1;
                else return SCPE_IOERR;

                if (right & 0x80) dri_state[last_selected].operation = 1;      /* Write */
                else if (right & 0xF0) dri_state[last_selected].operation = 2; /* Compare */
                else dri_state[last_selected].operation = 0;                   /* Read */
            }
            break;

        case 0x0005: /* Control / Reset */
            if (a == 0x0080) {
                /* Reset controller (RAZ) */
                dri_state[last_selected].status = 0;
                dri_state[last_selected].active = false;
            }
            break;

        case 0x0015: /* Start transfer */
            {
                DRI_UNIT *d = &dri_state[last_selected];
                if (d->active) return SCPE_OK; /* Already busy */
                
                /* Store ADS in general register */
                write_word(DRI_REG_ADS, a);
                
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

t_stat dri_rd(uint16 inst) {
    if (cpu_state.reg_E != 0x0015)
        return SCPE_IOERR;
    
    /* Return status word in A */
    cpu_state.reg_A = dri_state[last_selected].status;
    return SCPE_OK;
}

/* Wrapper for DIB */
t_stat dri_dio_handler(uint16 inst, t_bool is_write) {
    return is_write ? dri_wd(inst) : dri_rd(inst);
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

static int dri_unit_index(UNIT *uptr) {
    int idx = (int)(uptr - dri_unit);
    return (idx >= 0 && idx < DRI_NUM_UNITS) ? idx : -1;
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

/* UNIT Definition */
#define DRI_CAPACITY (DRI_CYLINDERS * DRI_TRACKS_PER_CYL * DRI_SECTORS_PER_TRACK * DRI_SECTOR_SIZE)
UNIT dri_unit[DRI_NUM_UNITS] = {
    { UDATA(NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_DISABLE, DRI_CAPACITY) },
    { UDATA(NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_DISABLE, DRI_CAPACITY) }
};

/* REGISTER Definitions */
REG dri_reg[] = {
    { ORDATA("STATUS0", dri_state[0].status, 16) },
    { ORDATA("STATUS1", dri_state[1].status, 16) },
    { NULL }
};

/* DIB (Device Information Block) */
dib_t dri_dib = {
    0,                  /* dva */
    NULL,               /* disp */
    0x15,               /* dio: Mode claimed by this device */
    dri_dio_handler     /* dio_disp: Handler function */
};

/* DEVICE Structure */
DEVICE dri_dev = {
    "DRI",              /* name */
    dri_unit,           /* units */
    dri_reg,            /* registers */
    NULL,               /* modifiers */
    DRI_NUM_UNITS,      /* numunits */
    8,                  /* aradix */
    16,                 /* awidth */
    1,                  /* aincr */
    8,                  /* dradix */
    16,                 /* dwidth */
    NULL,               /* examine */
    NULL,               /* deposit */
    &dri_reset,         /* reset */
    NULL,               /* boot */
    &dri_attach,        /* attach */
    &dri_detach,        /* detach */
    &dri_dib,           /* ctxt (DIB) */
    DEV_DISK | DEV_DISABLE, /* flags */
    0,                  /* dctrl */
    NULL,               /* debflags */
    NULL,               /* msize */
    NULL,               /* lname */
    NULL,               /* help */
    NULL,               /* attach_help */
    NULL,               /* help_ctxt */
    NULL                /* description */
};