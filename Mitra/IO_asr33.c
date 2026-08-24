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
         |	comparaison      |                       |
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

t_stat asr_dio_handler(uint16 inst, t_bool is_write); // RD and WD wrapper

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

extern uint32 intrpt_mask;  /* interrupt request bits */

/* Memory Access Functions (defined in mitra_cpu.h) */
extern t_value read_word(t_addr va);
extern void write_word(t_addr va, t_value val);
extern uint8 read_byte(t_addr va);
extern void write_byte(t_addr va, uint8 val);

t_stat asr33_rd(uint16 e_reg, uint16 *result);
t_stat asr33_wd(uint16 e_reg, uint16 val);


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

t_stat asr33_attach(UNIT *unit, const char *filename)
{
    t_stat r;
    char *saved_filename = unit->filename;

    /* Let the standard SIMH helper open the file and set UNIT_ATT,
       fileref, filename, etc. */
    unit->filename = NULL;
    r = attach_unit(unit, filename);
    if (r != SCPE_OK) {
        unit->filename = saved_filename;
        return r;
    }

    return SCPE_OK;
}

t_stat asr33_detach(UNIT *unit)
{
    asr_state.active = 0;
    return detach_unit(unit);
}

static void asr_interrupt(void)
{
    uint32 int_req = (1 << 3);   /* typical interrupt level for ASR33 */
    io_interrupt_dispatch(int_req, false);

}

/*
* A wrapper function to manage RD or WD instruction execution
* - matching dio_handler_t: t_stat xxx_dio(uint16 inst, t_bool is_write),
* - that reads cpu_state.reg_E/reg_A and 
* - calls the device's own _wd/_rd function, writing results back into cpu_state.reg_A for RD.
*/
t_stat asr_dio_handler(uint16 inst, t_bool is_write) {
    if(is_write) {
	return asr33_wd(cpu_state.reg_E, cpu_state.reg_A);
	}
    else {
	 return asr33_rd(cpu_state.reg_E, &cpu_state.reg_A);
	 }
}

/* WD handler (E=1) */
t_stat asr33_wd(uint16 e_reg, uint16 a_val)
{
    if (e_reg != 1) return SCPE_IOERR;
    uint8 cmd = a_val & 0xFF;


    /* Read R9 (z and count) and R11 compare char */
    uint16 r9  = read_word(ASR33_R9);
    uint32 mem_addr = r9 & 0x07FF;
    uint16 r10 = read_word(ASR33_R10);
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
            asr_state.bytes_left = r10 + 1;
            asr_state.status = 0x02;   /* écriture */
            break;
        case ASR_CMD_LEC_CLAV: /* Lecture clavier */
        case ASR_CMD_LEC_RUBAN: /* Lecture ruban */
            asr_state.mode = (cmd == ASR_CMD_LEC_CLAV) ? 1 : 2;
            asr_state.active = 1;
            asr_state.mem_addr = mem_addr;
            asr_state.bytes_left = r10 + 1;
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
    if (e_reg != 0x10) 
    	return SCPE_IOERR;
    	
    /* "état" occupies the top 3 bits (doc bits 0–2) and the last character occupies roughly the bottom byte (doc bits 7–15).
    Lecture d'état 
	E	&I0

        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
      |  état  |  |           |                       |
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

	Bits 0 à 2: 
		000	Repos
		001 Repos
		010 Ecriture
		011 Lecture
		111 Erreur

	Bits 7 à 15: dernier caractère ASCII transféré.
    */
    *result = ((asr_state.status & 0x07) << 13) | (asr_state.last_char && 0x0FF);  /* bits 7-15 = last char */
    asr_state.status = 0;   /* clear on read */
    return SCPE_OK;
}

/* ========== SIMH STRUCTURES ========== */

/* Unit service routine */
t_stat asr_svc(UNIT *uptr)
{
    return SCPE_OK; // FIXME
}

/* Device reset routine - must match t_stat (*)(DEVICE *) */
t_stat asr33_reset(DEVICE *dptr)
{
    asr_state.active = 0;
    asr_state.status = 0;
    asr_state.last_char = 0;
    return SCPE_OK;
}

/* Unit definition */
UNIT asr_unit = {
    UDATA(&asr_svc, UNIT_ATTABLE | UNIT_RO, 0)
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

dib_t asr_dib = {
    0,                  // dva (not used for RD/WD, or set to a dummy channel/dev)
    NULL,               // disp (not used for RD/WD)
    0x15,               // dio: The "Mode" this device claims
    asr_dio_handler     // dio_disp: The handler function
};

/* Device definition */
DEVICE asr_dev = {
    "ASR33",            /* name */
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
    &asr33_reset,         /* reset */
    NULL,               /* boot */
    &asr33_attach,      /* attach */
    &asr33_detach,      /* detach */
    &asr_dib,           /* ctxt */
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

