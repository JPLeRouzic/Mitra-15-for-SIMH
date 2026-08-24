/*
* 1. The address field and E content in RD or WD instructions is a Function Code, Not a Device Address
* It is a bitmask that tells the processor's internal I/O controller exactly which task to execute.
*
* The hardware decodes this address into a Mode and a Function Code to perform internal system-level operations, such as reading console sense switches, manipulating interrupt masks, or accessing low-memory processor registers.
*
* 2. The Initialization Routine (io_init)
* The dio_disp dispatch table is not updated dynamically while the CPU is executing instructions. Instead, it is rebuilt every time the simulator initializes or resets the I/O subsystem.
*
* Every I/O device in the simulator has a context structure called a Device Information Block (dib_t). For a device to respond to RD or WD instructions, its dib_t must define two specific fields:

    dio: The "Mode" or index (0 to DIO_N_MOD - 1) that this device claims.
    dio_disp: A pointer to the C function that will handle the RD/WD instructions for this specific mode.

Whenever the simulator starts or resets, the CPU calls the io_init() function (located in mitra_io.c). This function is responsible for wiring up the dispatch table. 

                *** RD ***
                bits 8 to 13 undefined
                bits 14, 14 = 10
                The opcode is 0xF402

                E register:

             0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
           +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
           |          |addr ext |        | mode| cont addr |
           +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

                Bits 3 to 6 and 12 to 15 are the I/O address
                Bits 10 and 11, are reading mode
                Result in A

                **** WD ***
                bits 8 to 13 undefined
                bits 14, 15 = 11
                The opcode is 0xF403

                E register:
                Bits 3 to 6 and 12 to 15 are the I/O address
                Bits 10 and 11, are writing mode
                

*/

#include "mitra_cpu.h"
#include "mitra_defs.h"
#include "mitra_io.h"

#include <stdio.h>
#include <string.h>



/* mitra_RD_WD.c: Mitra-15 Read Direct / Write Direct implementation

   ------------------------------------------------------------------
   Mitra-15 RD / WD model (from EFREI "Lecture / Écriture directe")

   Instruction format (16-bit word):
        bits 15-13 : addressing mode (ignored for RD/WD – always uses E)
        bits 12-8  : function field  (0xF4 for the RD/WD group)
        bits 7-0   : displacement    (ignored)

   Discrimination is done by the two low bits of the function field:
        10  →  RD   (Read Direct)   opcode family 0xF402
        11  →  WD   (Write Direct)  opcode family 0xF403

   All I/O parameters live in the E register:

        E bits 15-12 + 6-3  →  8-bit I/O address  (“mode”)
        E bits 11-10        →  sub-mode / transfer type
        A register          →  data for WD, result for RD

   The 8-bit mode is used as an index into the global dispatch table
   dio_disp[DIO_N_MOD].  Mode 0 is reserved for CPU-internal operations
   (panel, sense switches, system control).  All other modes are claimed
   by peripheral DIBs at reset time via io_init().
   ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

#ifndef DIO_N_MOD
#define DIO_N_MOD           256             /* 8-bit I/O address space */
#endif

/* Sub-mode field (E bits 11-10) */
#define DIO_SUBMODE_MASK    0x030
#define DIO_SUBMODE_SHIFT   4

/* I/O address extraction from E
   bits 3-6 and 12-15 form the 8-bit address
             0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
           +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
           |          |addr ext |        | mode| cont addr |
           +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 */
#define DIO_ADDR_HI(e)      (((e) >> 5) & 0xF0)   /* bits 15-12 → 7-4 */
#define DIO_ADDR_LO(e)      ((e) & 0x0F)   /* bits  6-3 → 3-0 */
#define DIO_GET_ADDR(e)     (DIO_ADDR_HI(e) | DIO_ADDR_LO(e))

/* Recognised opcodes (after the usual Mitra decode) */
#define OP_RD               0xF4            /* family; low 2 bits = 10 */
#define OP_WD               0xF4            /* family; low 2 bits = 11 */

/* ------------------------------------------------------------------ */
/* Dispatch table                                                     */
/* ------------------------------------------------------------------ */

/* Handler prototype: receives the full instruction word, the current
   value of A (for WD) and a pointer where RD must store its result.
   Returns 0 on success, non-zero trap code on error. */
typedef t_stat (*dio_handler_t)(uint16 inst, t_bool is_write);

extern dio_handler_t dio_disp[DIO_N_MOD];          /* global, filled by io_init */

/* ------------------------------------------------------------------ */
/* Forward declarations of the built-in mode-0 handler and a few
   convenience helpers that the device files will call.               */
/* ------------------------------------------------------------------ */

static t_stat io_rwd_m0(uint16 inst, t_bool is_write);
t_stat io_rwd(uint16 inst, t_bool is_write);                 /* the public entry point */

/* ------------------------------------------------------------------ */
/* Public entry point – called from the CPU instruction decoder       */
/* ------------------------------------------------------------------ */

/*
 * io_rwd - execute a Read-Direct or Write-Direct instruction
 *
 * The instruction word has already been fetched; the CPU has placed
 * the effective address (or the raw displacement) into the usual
 * places.  For Mitra-15 RD/WD the only interesting register is E.
 *
 * Returns:
 *   0          success
 *   TRAP_VM    illegal / non-existent device
 *   other      device-specific trap
 */
t_stat io_rwd(uint16 inst, t_bool is_write)
{
    uint16 e        = cpu_state.reg_E;
    uint8  mode     = (uint8)DIO_GET_ADDR(e);
    uint8  submode  = (uint8)((e & DIO_SUBMODE_MASK) >> DIO_SUBMODE_SHIFT);
    t_stat st;

    /* Debug trace (can be #ifdef'ed out for production) */
//    if (mode /* or a global debug flag */) {
        printf("[RD/WD] inst=%04X  E=%04X  mode=%02X  sub=%d  %s  A=%04X\n",
               inst, e, mode, submode,
               is_write ? "WD" : "RD",
               cpu_state.reg_A);
//    }

    /* Dispatch */
    if (dio_disp[mode] == NULL) {
        /* Non-existent device → invalid instruction trap */
        return mitra_trap(TRAP_ES, cpu_state.reg_P);
    }

    st = dio_disp[mode](inst, is_write);

    /* Optional: update condition codes from the result left in A */
    /* (Mitra manuals are silent; most real programs ignore CC here) */

    return st;
}

