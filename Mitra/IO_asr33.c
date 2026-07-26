/*
3. ASR 33 :
Registres

z:
	0 arrêt sur compte nul
	1 arrêt sur caractère de comparaison

           0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
         +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
  R9     | z|             compte d'octets                |
         +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
  R10    |             adresse des octets — 1            |
         +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
  R11    |  caractère de         |      données u        |
	     |		comparaison      |                       |
         +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

WD		E		1
		A		A0	Repos
				A2	écriture clavier
				A1	Lecture clavier (arrêt sur caractère de comparaison)
				A3	Lecture ruban (arrêt sur caractère de comparaison)
				A4	Stop
				A8	Suppression impression et perforation ruban

En lecture on recupère dans R9 le nombre de caractères non lus
(lorsque l'arrêt s'est fait sur caractère de comparaison).
Dans R11 (bits 7 à 15) on récupère le dernier caractère transféré.


Lecture d'état 
	E	&I0

        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
      |  état  |  |           |                        |
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

Bits 0 à 2: 
	000	Repos
	001 Repos
	010 Ecriture
	011 Lecture
	111 Erreur

Bits 7 à 15: dernier caractère ASCII transféré.

Complément d'informations:
	A	A9	Lecture sans impression
		AA	Perforations sans impression

All devices will follow the same integration pattern, they provide:
	 _wd and _rd handlers, 
	 a _poll function for asynchronous transfers, 
	 interrupt generation via int_req, 
	 and use the memory access helpers (read_byte_io, write_byte_io, read_word, write_word). 
	 The device state is stored in static structures, 
	 and attach/detach functions are provided for file‑based devices.
*/

/*
 * ASR33 TELETYPE (MITRA-15)
 *
 * Registers (memory‑mapped, absolute addresses):
 *   R9  (0x09) – bits: bit15 = z (0=stop on zero count, 1=stop on compare char),
 *                 bits 0-14 = byte count
 *   R10 (0x0A) – byte address -1
 *   R11 (0x0B) – bits 8-15 = compare character, bits 0-7 = data
 *
 * WD (E=1):
 *   A = 0x00 – repos
 *   A = 0x02 – écriture clavier (output to console)
 *   A = 0x01 – lecture clavier (input from keyboard, stop on compare char)
 *   A = 0x03 – lecture ruban (input from paper tape reader, stop on compare char)
 *   A = 0x04 – stop
 *   A = 0x08 – suppression impression et perforation
 *   A = 0x09 – lecture sans impression
 *   A = 0x0A – perforations sans impression
 *
 * RD (E=0x10, i.e. 16 decimal):
 *   bits 0-2: state (000=repos, 001=repos, 010=écriture, 011=lecture, 111=erreur)
 *   bits 7-15: last ASCII character transferred
 *
 * On read stop (compare character reached), R9 gets number of unread bytes,
 * R11 bits 7-15 get the last character.
 */

#include "mitra_defs.h"
#include "mitra_cpu.h"
#include "mitra_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* ASR33 - Corrected Register Addresses */
#define ASR33_R9   0x09   /* z bit (bit15) + byte count (bits 0-14) */
#define ASR33_R10  0x0A   /* byte address - 1 */
#define ASR33_R11  0x0B   /* compare char (bits 8-15) + data (bits 0-7) */

/* Command codes */
#define ASR_CMD_REPOS       0x00
#define ASR_CMD_LEC_CLAV    0x01
#define ASR_CMD_ECR_CLAV    0x02
#define ASR_CMD_LEC_RUBAN   0x03
#define ASR_CMD_STOP        0x04
#define ASR_CMD_SUPPR       0x08
#define ASR_CMD_LEC_SANS_IMP 0x09
#define ASR_CMD_PERF_SANS_IMP 0x0A

extern uint32 intrp_level;  /* interrupt request bits */

/* Memory Access Functions (defined in mitra_cpu.h) */
extern t_value read_word(t_addr va);
extern void write_word(t_addr va, t_value val);
extern uint8 read_byte(t_addr va);
extern void write_byte(t_addr va, uint8 val);

typedef struct {
    int    active;
    int    mode;            /* 0=idle, 1=read_keyboard, 2=read_tape, 3=write */
    int    stop_on_compare; /* z bit from R9 */
    uint16 compare_char;    /* from R11 high byte */
    uint32 mem_addr;
    uint32 bytes_left;
    int    waiting;         /* waiting for a character from terminal */
    uint16 last_char;       /* last transferred character (ASCII) */
    uint16 status;          /* last RD status */
} ASR33_DEV;

static ASR33_DEV asr_state = {0};

/*
 * The ASR33 is the system console; it has no image file to attach.
 * asr33_attach / asr33_detach are kept as no-ops so mitra_sys.c can
 * reference them without errors.
 */
t_stat asr33_attach(UNIT *unit, const char *filename)
{
    (void)filename;   /* console — nothing to attach */
    return SCPE_OK;
}

void asr33_detach(void)
{
    asr_state.active = 0;
}

static void asr_interrupt(void)
{
    uint32 int_req = (1 << 3);   /* typical interrupt level for ASR33 */
    io_interrupt_dispatch(int_req, false);

}

/* Write a character to the console */
static void asr_putc(uint8 ch)
{
    sim_putchar(ch);
}

/* Read a character from console (non‑blocking) – returns -1 if none */
static int asr_getc(void)
{
    int32 c = sim_poll_kbd();
    if (c < SCPE_KFLAG)
        return -1;
    return c & 0xFF;
}

/* Poll routine – called from main I/O loop */
int asr_poll(void)
{
    if (!asr_state.active) return 0;

    if (asr_state.mode == 3) {   /* output (write) */
        if (asr_state.bytes_left > 0) {
            uint8 ch;
            if (read_byte_io(asr_state.mem_addr, &ch, 0) != SCPE_OK) {
                asr_state.active = 0;
                asr_state.status = 0x07;   /* error */
                asr_interrupt();
                return 1;
            }
            asr_putc(ch);
            asr_state.mem_addr++;
            asr_state.bytes_left--;
            asr_state.last_char = ch;
        }
        if (asr_state.bytes_left == 0) {
            asr_state.active = 0;
            asr_state.status = 0x02;   /* écriture finished */
            asr_interrupt();
        }
        return 1;
    }

    if (asr_state.mode == 1 || asr_state.mode == 2) {   /* input from keyboard or paper tape */
        int ch = asr_getc();
        if (ch == -1) return 0;   /* no character yet */

        uint8 ascii = (uint8)ch;
        asr_state.last_char = ascii;
        write_byte_io(asr_state.mem_addr, ascii, 0);
        asr_state.mem_addr++;
        asr_state.bytes_left--;

        if (asr_state.bytes_left == 0 || (asr_state.stop_on_compare && ascii == asr_state.compare_char)) {
            /* Transfer finished or compare character reached */
            if (asr_state.stop_on_compare && ascii == asr_state.compare_char) {
                /* Update R9 with number of bytes not read */
                write_word(ASR33_R9, (asr_state.bytes_left & 0x7FFF) | (1 << 15)); /* z=1 */
                /* Update R11 high byte with last character */
                uint16 r11 = read_word(ASR33_R11);
                r11 = (r11 & 0x00FF) | (ascii << 8);
                write_word(ASR33_R11, r11);
            }
            asr_state.active = 0;
            asr_state.status = 0x03;   /* lecture finished */
            asr_interrupt();
        }
        return 1;
    }

    return 0;
}

/* WD handler (E=1) */
t_stat asr33_wd(uint16 e_reg, uint16 a_val)
{
    if (e_reg != 1) return SCPE_IOERR;
    uint8 cmd = a_val & 0xFF;

    /* Read channel registers: ADM at &1C, CM at &1D */
    /* FIXME it is not clear if
    	- Channel registers should be R9, R10, R11 (not &1C/&1D for ADM/CM)
	- Registers are at absolute addresses 0x1C/0x1D or 9, 10, 11 
    */
    uint16 adm = read_word(0x1C);
    uint16 cm  = read_word(0x1D);
    uint32 mem_addr = adm + 2;
    uint32 byte_count = cm * 2;

    /* Read R9 (z and count) and R11 compare char */
    uint16 r9  = read_word(ASR33_R9);
    uint16 r11 = read_word(ASR33_R11);
    asr_state.stop_on_compare = (r9 >> 15) & 1;
    asr_state.compare_char = (r11 >> 8) & 0xFF;

    /* FIXME
        - case ASR_CMD_REPOS:
        - case ASR_CMD_STOP:
        - case ASR_CMD_SUPPR:
        - case ASR_CMD_LEC_SANS_IMP:
        - case ASR_CMD_PERF_SANS_IMP:
    */
    switch (cmd) {
        case ASR_CMD_REPOS: /* Repos */
        case ASR_CMD_STOP:
            asr_state.active = 0;
            asr_state.status = 0x00;
            break;
        case ASR_CMD_ECR_CLAV: /* Écriture clavier */
            asr_state.mode = 3;   /* write */
            asr_state.active = 1;
            asr_state.mem_addr = mem_addr;
            asr_state.bytes_left = byte_count;
            asr_state.status = 0x02;   /* écriture */
            break;
        case ASR_CMD_LEC_CLAV: /* Lecture clavier */
        case ASR_CMD_LEC_RUBAN: /* Lecture ruban */
            asr_state.mode = (cmd == ASR_CMD_LEC_CLAV) ? 1 : 2;
            asr_state.active = 1;
            asr_state.mem_addr = mem_addr;
            asr_state.bytes_left = byte_count;
            asr_state.status = 0x03;   /* lecture */
            break;
        case ASR_CMD_SUPPR:
        case ASR_CMD_LEC_SANS_IMP:
        case ASR_CMD_PERF_SANS_IMP:
            /* Not fully simulated – just accept */
            asr_state.active = 0;
            asr_state.status = 0x00;
            break;
        default:
            return SCPE_IOERR;
    }
    return SCPE_OK;
}

/* RD handler (E=0x10) */
t_stat asr33_rd(uint16 e_reg, uint16 *result)
{
    if (e_reg != 0x10) return SCPE_IOERR;
    *result = (asr_state.status & 0x07) | (asr_state.last_char << 7);  /* bits 7-15 = last char */
    asr_state.status = 0;   /* clear on read */
    return SCPE_OK;
}

/* Reset device */
void asr33_reset(void)
{
    asr_state.active = 0;
    asr_state.status = 0;
    asr_state.last_char = 0;
}

/* ========== SIMH STRUCTURES ========== */

/* Unit service routine */
t_stat asr_svc(UNIT *uptr)
{
    return SCPE_OK; // FIXME
}

/* Device reset routine - must match t_stat (*)(DEVICE *) */
t_stat asr_reset(DEVICE *dptr)
{
    asr33_reset();
    return SCPE_OK;
}

/* Unit definition */
UNIT asr_unit = { 
    UDATA(&asr_svc, 0, 0) 
};

/* Register definitions - using asr_state variables */
REG asr_reg[] = {
    { ORDATA("MODE", asr_state.mode, 3) },
    { ORDATA("STATUS", asr_state.status, 16) },
    { ORDATA("LASTCHAR", asr_state.last_char, 8) },
    { FLDATA("ACTIVE", asr_state.active, 0) },
    { FLDATA("STOPCMP", asr_state.stop_on_compare, 0) },
    { NULL }
};

/* Modifier table - FIXME */
MTAB asr_mod[] = {
	
};

/* Device definition */
DEVICE asr_dev = {
    "TTI",              /* name */
    &asr_unit,          /* units */
    asr_reg,            /* registers */
    asr_mod,            /* modifiers */
    1,                  /* numunits */
    10,                 /* aradix */
    16,                 /* awidth */
    1,                  /* aincr */
    8,                  /* dradix */
    8,                  /* dwidth */
    NULL,               /* examine */
    NULL,               /* deposit */
    &asr_reset,         /* reset */
    NULL,               /* boot */
    NULL,               /* attach */
    NULL,               /* detach */
    NULL,               /* ctxt */
    0,                  /* flags */
    0,                  /* dctrl */
    NULL,               /* debflags */
    NULL,               /* msize */
    NULL,               /* lname */
    NULL,               /* help */
    NULL,               /* attach_help */
    NULL,               /* help_ctxt */
    NULL,               /* description */
};
