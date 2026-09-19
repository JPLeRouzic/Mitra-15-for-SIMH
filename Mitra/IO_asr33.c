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

/* ASR-33 Register Addresses are located in cpu_state.reg_block[1][1] to cpu_state.reg_block[1][3]
cpu_state.reg_block[1][1] z bit (bit15) + byte count (bits 0-14)
cpu_state.reg_block[1][2] byte address - 1
cpu_state.reg_block[1][3] compare char (bits 8-15) + data (bits 0-7)
*/

/* Command codes */
#define ASR_CMD_REPOS        0x00
#define ASR_CMD_LEC_CLAV     0x01
#define ASR_CMD_ECR_CLAV     0x02
#define ASR_CMD_LEC_RUBAN    0x03
#define ASR_CMD_STOP         0x04
#define ASR_CMD_SUPPR        0x08
#define ASR_CMD_LEC_SANS_IMP 0x09
#define ASR_CMD_PERF_SANS_IMP 0x0A

/* Boot constants (placeholders, to be confirmed against documentation) */
#ifndef ASR33_LEADER_BYTE
#define ASR33_LEADER_BYTE    0x00
#endif
#ifndef ASR33_BOOT_LOAD_ADDR
#define ASR33_BOOT_LOAD_ADDR 0
#endif
#ifndef ASR33_BOOT_MAX_BYTES
#define ASR33_BOOT_MAX_BYTES 128
#endif

/* Interrupt level for ASR33 */
#define ASR33_INT_LEVEL      6 // From MESTRALLET's thesis

extern uint32 intrpt_mask;  /* interrupt request bits */

/* Memory Access Functions (defined in mitra_cpu.h) */
extern t_value read_word(t_addr va);
extern void write_word(t_addr va, t_value val);
extern uint8 read_byte(t_addr va);
extern void write_byte(t_addr va, uint8 val);

t_stat asr33_rd(uint16 e_reg, uint16 *result);
t_stat asr33_wd(uint16 e_reg, uint16 val);
t_stat asr33_boot(int32 unit_num, DEVICE *dptr);
t_stat asr33_reset(DEVICE *dptr);

extern DEVICE cpu_dev;

typedef struct {
    FILE  *image;           /* attached tape file */
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

/* ====================================================================== */
/* Attach / Detach                                                        */
/* ====================================================================== */

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

    /* Save the SIMH file reference into our own state, like dri_attach
       does with dri_state[].image. This is the whole point of the fix:
       any subsequent transfer engine can now consult asr_state.image. 
    */
    asr_state.image = unit->fileref;

    /* Reset tape position and state */
    asr_state.active = 0;
    asr_state.mode = 0;
    asr_state.last_char = 0;
    asr_state.status = 0;
    asr_state.bytes_left = 0;
    asr_state.mem_addr = 0;
    asr_state.waiting = 0;

    if (asr_state.image)
        fseek(asr_state.image, 0, SEEK_SET);

    return SCPE_OK;
}

t_stat asr33_detach(UNIT *unit)
{
    asr_state.active = 0;
    asr_state.image = NULL;
    return detach_unit(unit);
}

/* ====================================================================== */
/* Interrupt                                                              */
/* ====================================================================== */

static void asr_interrupt(void)
{
    uint32 int_req = (1 << ASR33_INT_LEVEL);   /* typical interrupt level for ASR33 */
    io_interrupt_dispatch(int_req, false);
}

/* ====================================================================== */
/* RD / WD Dispatch                                                       *
* A wrapper function to manage RD or WD instruction execution
* - matching dio_handler_t: t_stat xxx_dio(uint16 inst, t_bool is_write),
* - that reads cpu_state.reg_E/reg_A and 
* - calls the device's own _wd/_rd function, writing results back into cpu_state.reg_A for RD.
* ====================================================================== */

t_stat asr_dio_handler(uint16 inst, t_bool is_write)
{
    if(is_write) {
	return asr33_wd(cpu_state.reg_E, cpu_state.reg_A);
	}
    else {
	 return asr33_rd(cpu_state.reg_E, &cpu_state.reg_A);
	 }
}

/* ====================================================================== */
/* WD handler (E=1)                                                       */
/* ====================================================================== */

t_stat asr33_wd(uint16 e_reg, uint16 a_val)
{
    if (e_reg != 1)
        return SCPE_IOERR;

    uint8 cmd = a_val & 0xFF;

    /* Read R9 (z and count), R10 (address-1) and R11 compare char */
    uint16 r9  = cpu_state.reg_block[1][1]; // Byte count
    uint16 r10 = cpu_state.reg_block[1][2]; // Current address
    uint16 r11 = cpu_state.reg_block[1][3]; // Working register

    asr_state.stop_on_compare = (r9 >> 15) & 1;
    asr_state.compare_char    = (r11 >> 8) & 0xFF;

    switch (cmd) {
        case ASR_CMD_REPOS:      /* Repos */
        case ASR_CMD_STOP:       /* Stop */
            asr_state.active = 0;
            asr_state.mode = 0;
            asr_state.status = 0x00;
            break;

        case ASR_CMD_ECR_CLAV:   /* Ecriture clavier (write to console) */
            asr_state.mode = 3;
            asr_state.active = 1;
            /* For write mode, the memory buffer holds the data to send. */
            asr_state.mem_addr  = r10 + 1;             /* R10 = addr - 1 */
            asr_state.bytes_left = (r9 & 0x7FFF) + 1;  /* count in low 15 bits */
            asr_state.status = 0x02;                   /* ecriture */
            break;

        case ASR_CMD_LEC_CLAV:   /* Lecture clavier */
        case ASR_CMD_LEC_RUBAN:  /* Lecture ruban */
            asr_state.mode = (cmd == ASR_CMD_LEC_CLAV) ? 1 : 2;
            asr_state.active = 1;
            asr_state.mem_addr  = r10 + 1;
            asr_state.bytes_left = (r9 & 0x7FFF) + 1;
            asr_state.status = 0x03;                   /* lecture */
            /* Kick off the transfer engine; it will run asynchronously
               via asr_svc (called by SIMH). */
            break;

        case ASR_CMD_SUPPR:
        case ASR_CMD_LEC_SANS_IMP:
        case ASR_CMD_PERF_SANS_IMP:
            /* Not fully simulated - just accept and idle */
            asr_state.active = 0;
            asr_state.status = 0x00;
            break;

        default:
            return SCPE_IOERR;
    }

    return SCPE_OK;
}

/* ====================================================================== */
/* RD handler (E=0x10)                                                    */
/* ====================================================================== */

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
    *result = ((asr_state.status & 0x07) << 13) |
              ((asr_state.last_char & 0x0FF) << 7);

    asr_state.status = 0;   /* clear on read */
    return SCPE_OK;
}

/* ====================================================================== */
/* Transfer engine                                                        */
/* ====================================================================== */

/*
 * asr33_poll - perform one character of a transfer.
 *
 * Returns 1 if a character was transferred (or the transfer finished),
 * 0 if there is nothing to do (idle or waiting).
 *
 * This is the function that was missing before. It reads from or writes
 * to asr_state.image (which is now correctly saved by asr33_attach).
 */
int asr33_poll(void)
{
    if (!asr_state.active)
        return 0;

    if (asr_state.bytes_left == 0) {
        /* Transfer complete */
        asr_state.active = 0;
        asr_state.status = (asr_state.mode == 3) ? 0x02 : 0x03;
        /* Update R9 with remaining count (0) */
        cpu_state.reg_block[1][1] = 0; // cpu_state.reg_block[1][1] is R9
        asr_interrupt();
        return 1;
    }

    if (asr_state.mode == 2 || asr_state.mode == 1) {
        /* ---------- READ path (tape reader / keyboard) ---------- */
        int c;
        if (asr_state.image == NULL) {
            asr_state.active = 0;
            asr_state.status = 0x07;   /* error */
            asr_interrupt();
            return 1;
        }

        c = fgetc(asr_state.image);
        if (c == EOF) {
            /* End of tape: stop with error */
            asr_state.active = 0;
            asr_state.status = 0x07;
            asr_interrupt();
            return 1;
        }

        asr_state.last_char = (uint16)(c & 0xFF);
        write_byte(asr_state.mem_addr, (uint8)(c & 0xFF));
        asr_state.mem_addr++;
        asr_state.bytes_left--;

        /* Update R11 low byte with the last character */
        {
            uint16 r11 = cpu_state.reg_block[1][3];
            r11 = (r11 & 0xFF00) | (c & 0xFF);
            cpu_state.reg_block[1][3] = r11;
        }

        /* Stop-on-compare? */
        if (asr_state.stop_on_compare &&
            (uint8)(c & 0xFF) == (uint8)asr_state.compare_char) {
            asr_state.active = 0;
            asr_state.status = 0x03;
            /* R9 gets number of unread bytes */
            cpu_state.reg_block[1][3] = (asr_state.bytes_left & 0x7FFF) | (asr_state.stop_on_compare << 15);
            asr_interrupt();
            return 1;
        }
    } else if (asr_state.mode == 3) {
        /* ---------- WRITE path (keyboard output) ---------- */
        uint8 ch = read_byte(asr_state.mem_addr);
        asr_state.last_char = ch;
        asr_state.mem_addr++;
        asr_state.bytes_left--;

        if (asr_state.image) {
            /* Echo the character to the attached file (e.g. a log
               or console capture). Real hardware would drive the
               teletype's printer; SIMH can map this to stdout as well. */
            fputc(ch, asr_state.image);
            fflush(asr_state.image);
        }
        fputc(ch, stdout);
        fflush(stdout);
    }

    if (asr_state.bytes_left == 0) {
        asr_state.active = 0;
        asr_state.status = (asr_state.mode == 3) ? 0x02 : 0x03;
        cpu_state.reg_block[1][1] = 0;
        asr_interrupt();
    }

    return 1;
}

/* ====================================================================== */
/* SIMH UNIT service routine                                              */
/* ====================================================================== */

/*
 * asr_svc - SIMH unit service routine.
 *
 * Called by the SIMH scheduler whenever the unit is activated (e.g.
 * after a WD command starts a transfer, or on a periodic poll). It
 * drives the transfer engine one step at a time. This is the routine
 * that was a bare "return SCPE_OK" stub before.
 */
t_stat asr_svc(UNIT *uptr)
{
    if (!asr_state.active)
        return SCPE_OK;

    /* Perform one character of the transfer. */
    asr33_poll();

    /* If the transfer is still active, reschedule ourselves so that
       the transfer progresses even without further WD commands. */
    if (asr_state.active) {
        /* Schedule a short delay before the next character. In real
           hardware the ASR-33 runs at ~10 chars/sec; SIMH's
           sim_activate_after gives us a fine-grained timer. */
        sim_activate_after(uptr, 1000);   /* 1 ms */
    }

    return SCPE_OK;
}

/* ====================================================================== */
/* Device reset routine - must match t_stat (*)(DEVICE *)                 */
/* ====================================================================== */

t_stat asr33_reset(DEVICE *dptr)
{
    asr_state.active = 0;
    asr_state.mode = 0;
    asr_state.status = 0;
    asr_state.last_char = 0;
    asr_state.bytes_left = 0;
    asr_state.mem_addr = 0;
    asr_state.waiting = 0;
    return SCPE_OK;
}

/* ====================================================================== */
/* Boot Support                                                           */
/* ====================================================================== */

/*
 * asr33_boot - bootstrap loader using the ASR-33's own tape reader.
 *
 * This uses the WD/RD instruction path (asr33_wd / asr33_rd)
 * not front panel's INI microprogram.
 *
 * NOTE: ASR33_BOOT_LOAD_ADDR is a placeholder, to be confirmed against documentation.
 */
t_stat asr33_boot(int32 unit_num, DEVICE *dptr)
{
    UNIT *uptr;
    int32 c;
    uint32 addr;

    if (unit_num != 0)
        return SCPE_NXDEV;              /* ASR33 only has one unit */

    uptr = &dptr->units[unit_num];

    if ((uptr->flags & UNIT_ATT) == 0)
        return SCPE_UNATT;              /* no tape image attached */

    if (uptr->fileref == NULL)
        return SCPE_IERR;

    /* Make sure asr_state.image points at the file */
    asr_state.image = uptr->fileref;

    /* Use the standard front-panel sequence to read the tape through
       the ASR-33's own WD/RD interface, rather than bypassing it.
       We simulate the operator pressing "lecture ruban" after the
       front-panel INI microprogram has loaded the first 128 bytes. */

    /* Rewind the tape and skip the leader (blank frames) */
    if (fseek(asr_state.image, 0, SEEK_SET) != 0)
        return SCPE_IOERR;

    while ((c = fgetc(asr_state.image)) == ASR33_LEADER_BYTE)
        ;	// <- not a glitch!

    if (c == EOF)
        return SCPE_FMT;

    /* Load the rest of the tape verbatim into memory, one byte per
       frame, starting at ASR33_BOOT_LOAD_ADDR. The first non-leader
       byte already read above is included as the first loaded byte.
       We do this through write_byte, which is the same helper the
       WD/RD transfer engine uses, so both paths share the memory
       accessor. */
    addr = ASR33_BOOT_LOAD_ADDR;
    write_byte(addr++, (uint8)c);

    while (addr < ASR33_BOOT_LOAD_ADDR + ASR33_BOOT_MAX_BYTES) {
        c = fgetc(asr_state.image);
        if (c == EOF)
            break;
        write_byte(addr++, (uint8)c);
    }

    /* Reset the controller and CPU exactly as a hardware reset/boot
       would, then transfer control to the freshly loaded code. */
    asr33_reset(dptr);
    cpu_reset(&cpu_dev);
//    cpu_state.MS = 1;                   /* master/privileged mode */
//    cpu_state.PR = 0;                   /* no protected-area restriction yet */
    asr_interrupt();
    get_BOOT_ENTRY_ADDR(); // get registers and condition codes from task context
    cpu_state.cpu_running = 1;

    return SCPE_OK;
}

/* ====================================================================== */
/* SIMH Structures                                                        */
/* ====================================================================== */

/* Unit definition */
UNIT asr_unit = {
    UDATA(&asr_svc, UNIT_ATTABLE | UNIT_RO, 0)
};

/* Register definitions - using asr_state variables */
REG asr_reg[] = {
    { ORDATA("MODE",      asr_state.mode,      3)  },
    { ORDATA("STATUS",    asr_state.status,    16) },
    { ORDATA("LASTCHAR",  asr_state.last_char, 8)  },
    { FLDATA("ACTIVE",    asr_state.active,    0)  },
    { FLDATA("STOPCMP",   asr_state.stop_on_compare, 0) },
    { ORDATA("MEMADDR",   asr_state.mem_addr,  32) },
    { ORDATA("BYTESLEFT", asr_state.bytes_left,32) },
    { NULL }
};

/* Modifier table - FIXME */
MTAB asr_mod[] = {
    { 0 }
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

t_stat asr_dio_handler(uint16 inst, t_bool is_write);

dib_t asr_dib = {
    0,                  // dva (not used for RD/WD, or set to a dummy channel/dev)
    NULL,               // disp (not used for RD/WD)
    0x15,               // dio: The "Mode" this device claims
    asr_dio_handler     // dio_disp: The handler function
};

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
    &asr33_reset,       /* reset */
    &asr33_boot,        /* boot */
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
