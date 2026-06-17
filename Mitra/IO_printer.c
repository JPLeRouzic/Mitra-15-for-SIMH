/*
FAST PRINTER

Fast Registers:

	&20: Address Register
	&21: Byte Count
	register E: 3
	A(0-7): 40
	A(8-15): Jump Code

Jump Codes:

	0: No Line Spacing
	1: Single Line Spacing
	2: Double Line Spacing
	3: Triple Line Spacing
	20: Jump Channel 1 Top
	21: Jump Channel 2 Bottom
	22: Jump Channel 3 Bottom

The jump is performed after printing.

Without Format: Buffer Address, Byte Count
With Format: Buffer Address, Format Byte, Byte Count

Status Read

	register E: 3
	RD: Result in Register A

Status Word

        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
      |                       |        Not used       |
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

	Bit 7: L+3 Timeout Error 7, The printer_dev did not send a character request.
	Bit 6: +3 IT2, Printer not ready during a transfer.
	Bit 5: +3 IT1, Printer not ready at the start of a transfer.
	Bit 4: Paper fault, switch located under the feed rollers.
	Bit 3: Incident bit.
	Bit 2: Paper out, sensor located below the hammers - valid via channel 2.
	Bit 1: Line ready, UK printing.
	Bit 0: Printer ready, UK printing.

Channels (theoretical distribution)

	0: Top of page
	1: Bottom of page
	2 to 11: Customer only
	
All devices will follow the same integration pattern, they provide:
	 _wd and _rd handlers, 
	 a _poll function for asynchronous transfers, 
	 interrupt generation via int_req, 
	 and use the memory access helpers (read_byte_io, write_byte_io, read_word, write_word). 
	 The device state is stored in static structures, 
	 and attach/detach functions are provided for file‑based devices.
*/

/*
 * FAST PRINTER (Imprimante Rapide) – MITRA-15
 *
 * Registers:
 *   &20 – address register (buffer address)
 *   &21 – byte count
 *
 * WD (E=3):
 *   bits 0-7 (A low byte) = 0x40 (fixed)
 *   bits 8-15 = jump code:
 *       0  – no line spacing
 *       1  – single line spacing
 *       2  – double line spacing
 *       3  – triple line spacing
 *       20 – jump to channel 1 (top)
 *       21 – jump to channel 2 (bottom)
 *       22 – jump to channel 3 (bottom)
 *
 * Without format: buffer address (from &20), byte count (from &21)
 * With format:    buffer address, format byte, byte count
 *
 * RD (E=3): status word
 *   bit7 – L+3 timeout error
 *   bit6 – IT2 (printer_dev not ready during transfer)
 *   bit5 – IT1 (printer_dev not ready at start)
 *   bit4 – paper fault
 *   bit3 – incident bit
 *   bit2 – paper out
 *   bit1 – line ready
 *   bit0 – printer_dev ready
 */

#include "mitra_io.h"
#include <stdio.h>
#include <stdlib.h>

extern uint32 int_req;               /* interrupt request bits */

/* Memory Access Functions (defined in mitra_cpu.c) */
extern uint16 read_word(uint16 addr);
extern void write_word(uint16 addr, uint16 val);
extern uint8 read_byte(uint16 va);
extern void write_byte(uint16 va, uint8 val);

typedef struct {
    FILE *output;          /* file or terminal for printer_dev output */
    uint16 status;
    int    active;
    uint32 mem_addr;
    uint32 bytes_left;
    int    zio;
    uint8  format_byte;    /* if format mode, first byte after address */
    int    format_mode;    /* 1 = with format, 0 = without */
    int    jump_code;      /* saved jump code */
} LP_DEV;

static LP_DEV printer_dev = {0};

static void lp_interrupt(void)
{
    int_req |= (1 << 2);   /* typical interrupt level for printer_dev */
    io_interrupt_dispatch();
}

/* Simple output – converts EBCDIC to ASCII (basic mapping) */
static void lp_putc(uint8 ebcdic)
{
    static const uint8 ebcdic_to_ascii[256] = {
        /* Minimal mapping – add more as needed */
        [' '] = 0x20, [0x4B] = '.', [0x4C] = '<', [0x4D] = '(',
        [0x4E] = '+', [0x4F] = '|', [0x50] = '&', [0x5A] = '!',
        [0x5B] = '$', [0x5C] = '*', [0x5D] = ')', [0x5E] = ';',
        [0x5F] = '-', [0x60] = '/', [0x61] = ',', [0x6A] = '%',
        [0x6B] = '_', [0x6C] = '>', [0x6D] = '?', [0x6E] = ':',
        [0x6F] = '#', [0x79] = '`', [0x7A] = ':', [0x7B] = '#',
        [0x7C] = '@', [0x7D] = '\'', [0x7E] = '=', [0x7F] = '"',
        [0x81] = 'a', [0x82] = 'b', [0x83] = 'c', [0x84] = 'd',
        [0x85] = 'e', [0x86] = 'f', [0x87] = 'g', [0x88] = 'h',
        [0x89] = 'i', [0x91] = 'j', [0x92] = 'k', [0x93] = 'l',
        [0x94] = 'm', [0x95] = 'n', [0x96] = 'o', [0x97] = 'p',
        [0x98] = 'q', [0x99] = 'r', [0xA2] = 's', [0xA3] = 't',
        [0xA4] = 'u', [0xA5] = 'v', [0xA6] = 'w', [0xA7] = 'x',
        [0xA8] = 'y', [0xA9] = 'z', [0xC1] = 'A', [0xC2] = 'B',
        [0xC3] = 'C', [0xC4] = 'D', [0xC5] = 'E', [0xC6] = 'F',
        [0xC7] = 'G', [0xC8] = 'H', [0xC9] = 'I', [0xD1] = 'J',
        [0xD2] = 'K', [0xD3] = 'L', [0xD4] = 'M', [0xD5] = 'N',
        [0xD6] = 'O', [0xD7] = 'P', [0xD8] = 'Q', [0xD9] = 'R',
        [0xE2] = 'S', [0xE3] = 'T', [0xE4] = 'U', [0xE5] = 'V',
        [0xE6] = 'W', [0xE7] = 'X', [0xE8] = 'Y', [0xE9] = 'Z',
        [0xF0] = '0', [0xF1] = '1', [0xF2] = '2', [0xF3] = '3',
        [0xF4] = '4', [0xF5] = '5', [0xF6] = '6', [0xF7] = '7',
        [0xF8] = '8', [0xF9] = '9', [0x15] = '\n', [0x0D] = '\r'
    };
    int ascii = ebcdic_to_ascii[ebcdic];
    if (ascii == 0) ascii = '?';
    if (printer_dev.output) {
        fputc(ascii, printer_dev.output);
    } else {
//        sim_tt_putc(ascii);
	sim_tt_outcvt(ascii, TTUF_MODE_8B);
    }
}

static void lp_do_jump(int code)
{
    /* In a real printer_dev, this would move the paper.
       For emulation, we just simulate by printing an extra newline where needed. */
    switch (code) {
        case 1: lp_putc('\n'); break;
        case 2: lp_putc('\n'); lp_putc('\n'); break;
        case 3: lp_putc('\n'); lp_putc('\n'); lp_putc('\n'); break;
        /* Channel jumps: ignore for now */
        default: break;
    }
}

int printer_poll(void)
{
    if (!printer_dev.active) return 0;

    if (printer_dev.format_mode && printer_dev.bytes_left > 0) {
        /* First byte is format byte */
        uint8 fmt;
        if (read_byte_io(printer_dev.mem_addr, &fmt, printer_dev.zio) != SCPE_OK) {
            printer_dev.active = 0;
            printer_dev.status = 0x20;   /* IT1 error */
            lp_interrupt();
            return 1;
        }
        printer_dev.mem_addr++;
        printer_dev.bytes_left--;
        printer_dev.format_byte = fmt;
        /* Skip code is in the format byte – not implemented here */
    }

    while (printer_dev.active && printer_dev.bytes_left > 0) {
        uint8 ch;
        if (read_byte_io(printer_dev.mem_addr, &ch, printer_dev.zio) != SCPE_OK) {
            printer_dev.active = 0;
            printer_dev.status = 0x40;   /* IT2 error */
            lp_interrupt();
            return 1;
        }
        lp_putc(ch);
        printer_dev.mem_addr++;
        printer_dev.bytes_left--;
    }

    if (printer_dev.bytes_left == 0) {
        printer_dev.active = 0;
        printer_dev.status = 0x03;   /* ready + line ready */
        lp_do_jump(printer_dev.jump_code);
        lp_interrupt();
    }
    return 1;
}

/* WD handler (E=3) */
t_stat printer_wd(uint16 e_reg, uint16 a_val)
{
    if (e_reg != 3) return SCPE_IOERR;
    printer_dev.jump_code = (a_val >> 8) & 0xFF;
    printer_dev.format_mode = ((a_val & 0xFF) == 0x40) ? 1 : 0;   /* A low byte must be 0x40 */

    /* Read channel registers: &20 = address, &21 = byte count */
    uint16 addr_reg = read_word(0x20);
    uint16 count_reg = read_word(0x21);
    printer_dev.mem_addr = addr_reg;   /* address already absolute? Documentation says address register */
    printer_dev.bytes_left = count_reg;
    printer_dev.zio = 0;   /* no ZIO for printer_dev */
    printer_dev.active = 1;
    printer_dev.status = 0x03;   /* printer_dev ready, line ready initially */
    return SCPE_OK;
}

/* RD handler (E=3) */
t_stat printer_rd(uint16 e_reg, uint16 *result)
{
    if (e_reg != 3) return SCPE_IOERR;
    *result = printer_dev.status;
    printer_dev.status = 0;
    return SCPE_OK;
}

/* Attach printer_dev output to a file (or NULL for console) */
t_stat printer_attach(const char *filename)
{
    if (printer_dev.output) 
    	fclose(printer_dev.output);
    if (filename && filename[0]) {
        printer_dev.output = fopen(filename, "w");
        if (!printer_dev.output) return SCPE_IOERR;
    } else {
        printer_dev.output = NULL;   /* use console */
    }
    return SCPE_OK;
}

void printer_detach(void)
{
    if (printer_dev.output) fclose(printer_dev.output);
    printer_dev.output = NULL;
    printer_dev.active = 0;
}

void printer_reset(void)
{
    printer_dev.active = 0;
    printer_dev.status = 0;
    /* do not close file */
}
