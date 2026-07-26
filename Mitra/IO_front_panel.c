/*
7- ACCES AU PUPITRE :

1) Lecture des clés dans A :
	E	&20
	RD

2) Ecriture de A sur voyants adresses :
	E	&20
	A	code à afficher
	WD

3) Ecriture de A sur voyants données :
	E	&10
	A	code à afficher
	WD

4) Extinction du voyant marche :
	Le calculateur marche
	Le bouton marche est éteint
	Les IT sont marquées
	Les déroutements sont ignorés (donc perdus)
	A	&60
	E	1
	WD

5) Arrêt du calculateur :
	Affichage sur calculateur (voyants donnés) des instructions qui suivent le WD
	A	&120
	E	1
	WD

6) RAZ système programmée :
	E	0
	WD	(coupure secteur)

7) Voyant marche allumé :
	Débloque les IT
	valide les déroutements
	calculateur toujours en arrêt
	A	&220
	E	1
	WD

All devices will follow the same integration pattern, they provide:
	 _wd and _rd handlers, 
	 a _poll function for asynchronous transfers, 
	 interrupt generation via int_req, 
	 and use the memory access helpers (read_byte_io, write_byte_io, read_word, write_word). 
	 The device state is stored in static structures, 
	 and attach/detach functions are provided for file‑based devices.

*/

/*
 * FRONT PANEL (Pupitre de programmation) – MITRA-15
 *
 * RD (E=0x20): read keys into A (not implemented – always returns 0)
 *
 * WD (E=0x20): write A to address lights
 * WD (E=0x10): write A to data lights
 *
 * WD (E=1):
 *   A = 0x060 – turn off run light, mark interrupts, ignore routing
 *   A = 0x120 – stop CPU (display following instructions on data lights)
 *   A = 0x220 – turn on run light, enable interrupts and routing (CPU still stopped)
 *
 * WD (E=0): system reset (power failure)
 */

#include "mitra_defs.h"
#include "mitra_cpu.h"
#include "mitra_io.h"
#include <stdio.h>
#include <stdbool.h>

/* External CPU control flags (defined in mitra_cpu.c) */
extern CPU_STATE cpu_state;
void panel_reset(void);
t_stat panel_reset_dev(DEVICE *dptr);

/* RD (E=0x20) – read keys */
t_stat panel_rd(uint16 e_reg, uint16 *result)
{
    if (e_reg != 0x20) return SCPE_IOERR;
    /* In a real emulator we would read the front panel keys.
       For now, always return 0. */
    *result = 0;
    return SCPE_OK;
}

/* WD handler */
t_stat panel_wd(uint16 e_reg, uint16 a_val)
{
    switch (e_reg) {
        case 0x20:   /* write address lights */
            cpu_state.panel_addr_lights = a_val;
            return SCPE_OK;
        case 0x10:   /* write data lights */
            cpu_state.panel_data_lights = a_val;
            return SCPE_OK;
        case 1:
            switch (a_val) {
                case 0x060:   /* turn off run light, mask interrupts, ignore routing */
                    cpu_state.cpu_running = 0;          /* stop CPU */
                    cpu_state.interrupts_enabled = 0;
                    cpu_state.routing_enabled = 0;
                    break;
                case 0x120:   /* stop CPU, show next instructions on data lights */
                    cpu_state.cpu_running = 0;
                    /* data lights would display the instruction at PC; handled elsewhere */
                    break;
                case 0x220:   /* turn on run light, enable interrupts/routing (CPU remains stopped) */
                    cpu_state.interrupts_enabled = 1;
                    cpu_state.routing_enabled = 1;
                    /* run light on – but CPU still stopped until a RUN command */
                    break;
                default:
                    return SCPE_IOERR;
            }
            return SCPE_OK;
            
        case 0:   /* system reset */
            /* Simulate power failure – reset entire system */
            cpu_state.cpu_running = 0;
            cpu_state.interrupts_enabled = 0;
            cpu_state.routing_enabled = 0;
            cpu_state.panel_addr_lights = 0;
            cpu_state.panel_data_lights = 0;
            /* Additional reset actions would be called from the main emulator */
            return SCPE_OK;
        default:
            return SCPE_IOERR;
    }
}

/* ========== SIMH STRUCTURES ========== */

/* Device reset routine */
t_stat panel_reset_dev(DEVICE *dptr)
{
    panel_reset();
    return SCPE_OK;
}

/* Unit definition */
UNIT panel_unit[] = {
    { UDATA(NULL, 0, 0) }
};

/* Register definitions */
REG panel_reg[] = {
    { ORDATA("ADDR", cpu_state.panel_addr_lights, 16) },
    { ORDATA("DATA", cpu_state.panel_data_lights, 16) },
    { FLDATA("RUN", cpu_state.cpu_running, 0) },
    { FLDATA("INTEN", cpu_state.interrupts_enabled, 0) },
    { FLDATA("ROUTING", cpu_state.routing_enabled, 0) },
    { NULL }
};

MTAB panel_mod[] = {
    { 0 }
};

/* Device definition */
DEVICE panel_dev = {
    "PANEL",            /* name */
    panel_unit,         /* units */
    panel_reg,          /* registers */
    panel_mod,          /* modifiers */
    1,                  /* numunits */
    8,                  /* aradix */
    16,                 /* awidth */
    1,                  /* aincr */
    8,                  /* dradix */
    16,                 /* dwidth */
    NULL,               /* examine */
    NULL,               /* deposit */
    &panel_reset_dev,   /* reset */
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
    0,                  /* brk_types */
    NULL               /* type_ctx */
//    NULL                /* unit_test */
};

/* Reset function */
void panel_reset(void)
{
    cpu_state.panel_addr_lights = 0;
    cpu_state.panel_data_lights = 0;
    /* FIXME Can we change CPU state here, are they instead handled by WD E=0 ? */
    cpu_state.cpu_running = 0;
    cpu_state.interrupts_enabled = 0;
    cpu_state.routing_enabled = 0;
    cpu_state.panel_addr_lights = 0;
    cpu_state.panel_data_lights = 0;
}


