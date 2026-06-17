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

#include "mitra_io.h"
#include <stdio.h>

/* External CPU control flags (defined in mitra_cpu.c) */
extern int cpu_running;      /* 1 = running, 0 = stopped */
extern int interrupts_enabled;
extern int routing_enabled;

uint16 panel_addr_lights;
uint16 panel_data_lights;

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
            panel_addr_lights = a_val;
            return SCPE_OK;
        case 0x10:   /* write data lights */
            panel_data_lights = a_val;
            return SCPE_OK;
        case 1:
            switch (a_val) {
                case 0x060:   /* turn off run light, mask interrupts, ignore routing */
                    cpu_running = 0;          /* stop CPU */
                    interrupts_enabled = 0;
                    routing_enabled = 0;
                    break;
                case 0x120:   /* stop CPU, show next instructions on data lights */
                    cpu_running = 0;
                    /* data lights would display the instruction at PC; handled elsewhere */
                    break;
                case 0x220:   /* turn on run light, enable interrupts/routing (CPU remains stopped) */
                    interrupts_enabled = 1;
                    routing_enabled = 1;
                    /* run light on – but CPU still stopped until a RUN command */
                    break;
                default:
                    return SCPE_IOERR;
            }
            return SCPE_OK;
            
        case 0:   /* system reset */
            /* Simulate power failure – reset entire system */
            cpu_running = 0;
            interrupts_enabled = 0;
            routing_enabled = 0;
            panel_addr_lights = 0;
            panel_data_lights = 0;
            /* Additional reset actions would be called from the main emulator */
            return SCPE_OK;
        default:
            return SCPE_IOERR;
    }
}

/* Reset function for the panel device */
void panel_reset(void)
{
    panel_addr_lights = 0;
    panel_data_lights = 0;
    /* Do not change CPU state here – that's handled by WD E=0 */
}
