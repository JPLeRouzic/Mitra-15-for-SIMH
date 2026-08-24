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
t_stat panel_rd(uint16 e_reg, uint16 *result);
t_stat panel_wd(uint16 e_reg, uint16 a_val);

/*
* A wrapper function to manage RD or WD instruction execution
* - matching dio_handler_t: t_stat xxx_dio(uint16 inst, t_bool is_write),
* - that reads cpu_state.reg_E/reg_A and 
* - calls the device's own _wd/_rd function, writing results back into cpu_state.reg_A for RD.
*/
t_stat panel_dio_handler(uint16 inst, t_bool is_write) {
    if(is_write) {
	return panel_wd(cpu_state.reg_E, cpu_state.reg_A);
	}
    else {
	 return panel_rd(cpu_state.reg_E, &cpu_state.reg_A);
	 }
}

/* RD (E=0x20) – read keys */
t_stat panel_rd(uint16 e_reg, uint16 *result)
{
    if (e_reg != 0x20) 
    	return SCPE_IOERR;
    uint8  fnc  = (uint8)(e_reg & 0xFF);          /* low 8 bits of E as function */
    switch (fnc) {
        case 0x00:                            /* sense switches → A */
            /* FIXME In a real machine the front-panel switches would be read.
               We expose a simple global for the moment. */
            cpu_state.reg_A = cpu_state.panel_data_lights;   /* or a dedicated SSW */
            break;

        case 0x10:                            /* read memory-fault register */
            cpu_state.reg_A = 0;                       /* placeholder */
            break;

        case 0x20:                            /* read address lights */
            cpu_state.reg_A = cpu_state.panel_addr_lights;
            break;

        case 0x40:                            /* read PSW2 / inhibit bits */
            cpu_state.reg_A = (cpu_state.MA ? 0x8000 : 0) |
                     (cpu_state.MS ? 0x4000 : 0) |
                     (cpu_state.PR ? 0x2000 : 0);
            break;

        default:
            return TRAP_II;
        }

    return SCPE_OK;
}

/* WD handler */
t_stat panel_wd(uint16 e_reg, uint16 a_val)
{
    switch (e_reg) {
        case 0:   /* system reset */
            /* Simulate power failure – reset entire system */
            cpu_state.cpu_running = 0;
            cpu_state.interrupts_enabled = 0;
            cpu_state.routing_enabled = 0;
            cpu_state.panel_addr_lights = 0;
            cpu_state.panel_data_lights = 0;
            /* Additional reset actions would be called from the main emulator */
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
            
        case 0x10:   /* write data lights */
            cpu_state.panel_data_lights = a_val;
            return SCPE_OK;
            
        case 0x20:   /* write address lights */
            cpu_state.panel_addr_lights = a_val;
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
t_stat panel_dio_handler(uint16 inst, t_bool is_write); // RD and WD wrapper

dib_t panel_dib = {
    0,                  // dva (not used for RD/WD, or set to a dummy channel/dev)
    NULL,               // disp (not used for RD/WD)
    0x15,               // dio: The "Mode" this device claims
    panel_dio_handler     // dio_disp: The handler function
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
    &panel_dib,         /* ctxt */
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


