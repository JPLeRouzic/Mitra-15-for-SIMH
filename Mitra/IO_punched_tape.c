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

#include "mitra_io.h"
#include <stdio.h>
#include <stdlib.h>

extern uint32 int_req;               /* interrupt request bits */
extern uint16 read_word(uint16 addr);

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
t_stat ptr_attach(const char *filename)
{
    if (ptr.image) fclose(ptr.image);
    ptr.image = fopen(filename, "rb");
    if (!ptr.image) return SCPE_IOERR;
    ptr.pos = 0;
    ptr.nzc = 0;
    ptr.last_char = 0;
    ptr.status = 0;
    return SCPE_OK;
}

void ptr_detach(void)
{
    if (ptr.image) fclose(ptr.image);
    ptr.image = NULL;
    ptr.active = 0;
}

static void ptr_interrupt(void)
{
    int_req |= (1 << 5);   /* typical interrupt level for reader */
    io_interrupt_dispatch();
}

int ptr_poll(void)
{
    if (!ptr.active) return 0;

    int c = fgetc(ptr.image);
    if (c == EOF) {
        ptr.active = 0;
        ptr.status = 0x02;   /* stop/error */
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
        ptr.status = 0;
        ptr_interrupt();
    }
    return 1;
}

t_stat ptr_wd(uint16 e_reg, uint16 a_val)
{
    if (e_reg != 8) return SCPE_IOERR;
    uint16 cmd = a_val & 0xFF;
    if (cmd == 1 || cmd == 3) {
        if (!ptr.image) return SCPE_UNATT;
        if (ptr.active) return SCPE_OK;   /* busy */
        ptr.mode = cmd;
        ptr.active = 1;
        /* Channel registers: ADM at &1C, CM at &1D */
        uint16 adm = read_word(0x1C);
        uint16 cm  = read_word(0x1D);
        ptr.mem_addr = adm + 2;
        ptr.bytes_left = cm * 2;
        ptr.zio = 0;
    } else if (cmd == 2) {
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
t_stat ptp_attach(const char *filename)
{
    if (ptp.image) fclose(ptp.image);
    ptp.image = fopen(filename, "wb");
    if (!ptp.image) return SCPE_IOERR;
    ptp.pos = 0;
    ptp.status = 0;
    return SCPE_OK;
}

void ptp_detach(void)
{
    if (ptp.image) fclose(ptp.image);
    ptp.image = NULL;
    ptp.active = 0;
}

static void ptp_interrupt(void)
{
    int_req |= (1 << 6);   /* typical interrupt level for punch */
    io_interrupt_dispatch();
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
    ptp.advance = (a_val >> 15) & 1;
    if (ptp.advance) {
        if (ptp.active) return SCPE_OK;
        ptp.active = 1;
        uint16 adm = read_word(0x1C);
        uint16 cm  = read_word(0x1D);
        ptp.mem_addr = adm + 2;
        ptp.bytes_left = cm * 2;
        ptp.zio = 0;
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
