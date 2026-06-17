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

#include "mitra_io.h"
#include <stdio.h>
#include <stdlib.h>

#define SAGEM_SECTOR_SIZE      256
#define SAGEM_SECTORS_PER_TRACK 12
#define SAGEM_TRACKS           128
#define SAGEM_WORDS_PER_SECTOR (SAGEM_SECTOR_SIZE / 2)

extern uint32 int_req;               /* interrupt request bits */
extern uint16 read_word(uint16 addr);
extern uint16 read_word(uint16 addr);
extern void write_word(uint16 addr, uint16 val);
extern uint8 read_byte(uint16 va);
extern void write_byte(uint16 va, uint8 val);

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

static SAGEM_DEV sagem = {0};

static uint32 ts_to_sector(uint32 track, uint32 sector)
{
    return track * SAGEM_SECTORS_PER_TRACK + sector;
}

static void sagem_interrupt(void)
{
    int_req |= (1 << 9);   /* typical interrupt level for SAGEM disk */
    io_interrupt_dispatch();
}

/* Start a transfer */
static void sagem_start_transfer(uint32 mem_addr, uint32 byte_count, uint32 track, uint32 sector, int mode, int zio)
{
    if (sagem.active) return;

    uint32 sec = ts_to_sector(track, sector);
    if (sec >= sagem.total_sectors) {
        sagem.me = 0x0800;   /* invalid sector address */
        sagem.status = 0x01;
        sagem_interrupt();
        return;
    }
    if (byte_count == 0) {
        sagem.me = 0x1000;   /* zero word count */
        sagem.status = 0x02;
        sagem_interrupt();
        return;
    }

    sagem.active = 1;
    sagem.mem_addr = mem_addr;
    sagem.bytes_left = byte_count;
    sagem.mode = mode;
    sagem.track = track;
    sagem.sector = sector;
    sagem.zio = zio;
}

int sagem_poll(void)
{
    if (!sagem.active) return 0;

    uint32 sector = ts_to_sector(sagem.track, sagem.sector);
    uint32 offset = sector * SAGEM_SECTOR_SIZE;
    FILE *f = sagem.image;

    if (sagem.mode == 2) {   /* read */
        if (fseek(f, offset, SEEK_SET) != 0) {
            sagem.active = 0;
            sagem.me = 0x0400;   /* parity error (simulated) */
            sagem.status = 0x03;
            sagem_interrupt();
            return 1;
        }
        uint32 bytes_to_do = (sagem.bytes_left < SAGEM_SECTOR_SIZE) ? sagem.bytes_left : SAGEM_SECTOR_SIZE;
        uint8 buf[SAGEM_SECTOR_SIZE];
        if (fread(buf, 1, bytes_to_do, f) != bytes_to_do) {
            sagem.active = 0;
            sagem.me = 0x0400;
            sagem.status = 0x03;
            sagem_interrupt();
            return 1;
        }
        for (uint32 i = 0; i < bytes_to_do; i++)
            write_byte_io(sagem.mem_addr + i, buf[i], sagem.zio);
        sagem.mem_addr += bytes_to_do;
        sagem.bytes_left -= bytes_to_do;
    } else if (sagem.mode == 1) {   /* write */
        if (fseek(f, offset, SEEK_SET) != 0) {
            sagem.active = 0;
            sagem.me = 0x0400;
            sagem.status = 0x03;
            sagem_interrupt();
            return 1;
        }
        uint32 bytes_to_do = (sagem.bytes_left < SAGEM_SECTOR_SIZE) ? sagem.bytes_left : SAGEM_SECTOR_SIZE;
        uint8 buf[SAGEM_SECTOR_SIZE];
        for (uint32 i = 0; i < bytes_to_do; i++)
            read_byte_io(sagem.mem_addr + i, &buf[i], sagem.zio);
        if (fwrite(buf, 1, bytes_to_do, f) != bytes_to_do) {
            sagem.active = 0;
            sagem.me = 0x0400;
            sagem.status = 0x03;
            sagem_interrupt();
            return 1;
        }
        sagem.mem_addr += bytes_to_do;
        sagem.bytes_left -= bytes_to_do;
    } else {
        /* compare or rest – not implemented, treat as error */
        sagem.active = 0;
        sagem.me = 0x2000;   /* compare error */
        sagem.status = 0x02;
        sagem_interrupt();
        return 1;
    }

    /* Advance to next sector if more data remains */
    if (sagem.bytes_left > 0) {
        sagem.sector++;
        if (sagem.sector >= SAGEM_SECTORS_PER_TRACK) {
            sagem.sector = 0;
            sagem.track++;
        }
    }

    if (sagem.bytes_left == 0) {
        sagem.active = 0;
        sagem.me = 0x8000;   /* success */
        sagem.status = 0;
        sagem_interrupt();
    }
    return 1;
}

/* Attach disk image */
t_stat sagem_attach(const char *filename)
{
    if (sagem.image) fclose(sagem.image);
    sagem.image = fopen(filename, "rb+");
    if (!sagem.image) sagem.image = fopen(filename, "wb+");
    if (!sagem.image) return SCPE_IOERR;
    fseek(sagem.image, 0, SEEK_END);
    long size = ftell(sagem.image);
    sagem.total_sectors = size / SAGEM_SECTOR_SIZE;
    if (sagem.total_sectors == 0)
        sagem.total_sectors = SAGEM_TRACKS * SAGEM_SECTORS_PER_TRACK;
    fseek(sagem.image, 0, SEEK_SET);
    sagem.me = 0;
    sagem.status = 0;
    return SCPE_OK;
}

void sagem_detach(void)
{
    if (sagem.image) fclose(sagem.image);
    sagem.image = NULL;
    sagem.active = 0;
}

/* WD handler (E=5) */
t_stat sagem_wd(uint16 e_reg, uint16 a_val)
{
    if (e_reg != 5) return SCPE_IOERR;
    /* Write AP to register &3D (emulated) */
    write_word(0x3D, a_val);
    uint32 track  = (a_val >> 8) & 0x7F;
    uint32 sector = (a_val >> 4) & 0x0F;
    uint32 mode   = a_val & 0x03;

    if (mode == 0) {   /* rest – do nothing */
        return SCPE_OK;
    }

    uint16 ad = read_word(0x3B);
    uint16 cm = read_word(0x3C);
    uint32 mem_addr = ad + 2;
    uint32 byte_count = cm * 2;
    sagem_start_transfer(mem_addr, byte_count, track, sector, mode, 0);
    return SCPE_OK;
}

/* RD handler (E=3) – returns status ER bits */
t_stat sagem_rd(uint16 e_reg, uint16 *result)
{
    if (e_reg != 3) return SCPE_IOERR;
    *result = sagem.status;
    sagem.status = 0;
    return SCPE_OK;
}

/* Read ME register (used by CPU after interrupt) */
uint16 sagem_get_me(void)
{
    return sagem.me;
}

/* Reset device */
void sagem_reset(void)
{
    sagem.active = 0;
    sagem.me = 0;
    sagem.status = 0;
}

/* Poll function */
void sagem_poll_devices(void)
{
    sagem_poll();
}
