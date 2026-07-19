/* mitra_stubs.c
 * ---------------------------------------------------------------------
 * TEMPORARY placeholder definitions.
 *
 * These symbols are referenced (via `extern` in mitra_sys.c) by the REG[]
 * tables for devices whose real implementation isn't linked in yet:
 * card reader (IO_CDR.c), paper tape reader/puncher (IO_PTR.c/IO_PTP.c),
 * front panel (IO_panel.c), and line printer (IO_printer.c).
 *
 * This file exists ONLY to get you a linkable binary so you can keep
 * testing the rest of the simulator. None of these values are functionally
 * correct - the devices they belong to will not actually work correctly
 * until you replace this file's definitions with the real ones from your
 * IO_*.c sources (or finish writing those sources if they don't exist yet).
 *
 * As you implement each real module, DELETE the corresponding line(s)
 * below - the linker will then pick up the real definition instead, and
 * if you forget to delete one you'll get a "multiple definition" error,
 * which is your cue to remove it here.
 * --------------------------------------------------------------------- */

#include "sim_defs.h"

/* --- Card reader (IO_CDR.c) -------------------------------------------- */
uint32 lastcard   = 0;
uint32 carderr    = 0;
uint32 notready   = 0;
uint16 DAR        = 0;
uint16 LCR        = 0;
uint32 cdr_ebcdic = 0;
uint32 s2sel      = 0;
uint8  rbuf[80];        /* GUESS: 80-column card image. Replace with the
                            real size once IO_CDR.c defines it for real. */

/* --- Front panel (IO_panel.c) ------------------------------------------ */
uint32 panel_pie = 0;

/* --- Paper tape reader (IO_PTR.c) -------------------------------------- */
uint32 xfr_req      = 0;
uint32 ptr_sor       = 0;
uint32 ptr_stopioe   = 0;

t_stat ptr_boot(int32 unit, DEVICE *dptr)
{
    /* stub: real boot loader for the paper tape reader not implemented yet */
    return SCPE_OK;
}

/* --- Paper tape puncher (IO_PTP.c) ------------------------------------- */
uint32 ptp_ldr      = 0;
uint32 ptp_stopioe  = 0;

/* --- Line printer (IO_printer.c) --------------------------------------- */
uint32 READY = 0;
uint32 STOP  = 0;

t_stat printer_event(UNIT *uptr)
{
    /* stub: real printer service routine not implemented yet */
    return SCPE_OK;
}
