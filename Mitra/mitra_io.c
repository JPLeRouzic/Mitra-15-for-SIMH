/*
you must:

    Extend mitra_io.c to:

        Include these device headers.

        Call the appropriate _poll functions from io_poll_devices.

        Implement the io_rd/io_wd dispatch to call the new _wd/_rd functions based on E register.

        Provide the read_word/write_word functions for accessing memory‑mapped device registers (e.g., &39, &3B, etc.).  the memory access functions (read_word, write_word) are already present in mitra_cpu.c.

    Add the device attach/detach commands to the SIMH command parser, linking them to cdr_attach, ptr_attach, ptp_attach, dri_attach, sagem_attach, etc.

These improvements convert the original SIMH‑centric device code into a functional Mitra‑15 peripheral simulation that respects the documented RD/WD protocol, uses the existing memory access helpers, and integrates with the interrupt system.

All device files assume the following external symbols are provided by the main emulator (mitra_cpu.c and mitra_io.c):

    read_byte_io(uint32 addr, uint8 *val, int zio)

    write_byte_io(uint32 addr, uint8 val, int zio)

    read_word(uint16 addr) and write_word(uint16 addr, uint16 val) for memory‑mapped registers.

    int_req – global bitmask of pending interrupts.

    io_interrupt_dispatch(void) – called to process the interrupt.

    sim_tt_putc, sim_tt_inchar – terminal I/O (provided by SIMH framework).

Each device provides:

    _wd and _rd handlers for the io_wd/io_rd dispatcher.

    _poll function to be called regularly from io_poll_devices().

    _attach, _detach, _reset as needed.

The main emulator must call the poll functions for all active devices inside its instruction loop (or via a timer). The interrupt levels used (4,5,6,7,8,9,2,3) are examples; they can be adjusted to match the actual Mitra-15 interrupt assignments.
*/





/* Mitra-15 I/O System 
1. General organization of I/O operations

The following describes how input/output (I/O) works under the control of the system monitor (the operating system core).

In Slave mode (modern user mode) the program uses CSV instruction which is a call to the Master mode (modern kernel mode)
In Master mode I/O is done with RD or WD instructions.

1.1. The 5 stages of an I/O operation

An I/O operation is divided into five steps:

	1.1.1. Buffer allocation (optional). A memory buffer may be reserved using a supervisor call.

	1.1.2. Transfer request. The program requests an I/O operation via a system call (M:1O or M:ZIO).

	1.1.3. Transfer initialization. The monitor prepares the transfer (device setup, parameters, etc.).

	1.1.4. Transfer execution. The data transfer happens independently of the user program.

	1.1.5. Transfer completion and validation. The system checks the result and reports status or errors.

1.2. Who performs each step?
Steps 1–2: executed by the user program
Step 3: executed by the monitor (control returns immediately after initialization)
Steps 4–5: executed asynchronously by the system

The user program may optionally wait for completion using M:WAIT (or M:ZWAT).

1.3. What happens if the device is busy?

Two behaviors are possible:

1.3.1. Queuing mode

The request is placed in a queue
Control returns immediately to the program
From the user’s perspective, the transfer appears to have started
The waiting time includes both queue delay and actual transfer time

1.3.2. Non-queuing mode

The system waits until the device is available
Control is returned only after initialization is actually done

2. I/O system architecture
The I/O system is organized into three abstraction levels:

2.1. Level 0 — Physical level
	Device-specific operations
	Direct interaction with hardware

2.2. Level 1 — I/O supervisor (core of the OS)

It handles:

	- Device allocation (busy/free)
	- Request queuing
	- Parameter setup
	- Transfer initiation
	- Error handling

This level is always resident in memory.

2.3. Level 2 and above — High-level services

These include:

	- File systems
	- Record formatting (blocking/unblocking)
	- Language-level I/O (e.g. FORTRAN)
	- Specialized libraries

They are typically implemented as subroutines linked to user programs.

3. Handlers (device-specific modules)

	Each device type is controlled by a handler, which is a specialized software module.

	A handler manages:

	Transfer initialization
	Transfer execution
	Error detection

Two parts of a handler
3.1. Handler 1 — Initialization

	Runs at the caller’s priority level.

	Responsibilities:

	Check device status before starting
	Send the read/write command
	Detect immediate errors
	Return control to caller

If an error is detected, the transfer is aborted and reported.

3.2. Handler 2 — Transfer control (interrupt-driven)

	Runs at interrupt level.

	Responsibilities:

	Continue or monitor the transfer after interrupts
	Possibly re-call Handler 1
	Detect errors during transfer
	Signal transfer completion

	At the end:

	It notifies the I/O supervisor
	It disables the corresponding interrupt

Two transfer modes depending on the device:

3.2.1) Block transfer

	Entire data block transferred in one operation

3.2.2) Element-by-element transfer

	Data transferred piece by piece
	Handler 2 may repeatedly call Handler 1
	Multiple devices
	Several devices of the same type can share the same handler
	Their interrupts are grouped
	If one interrupt is being handled, others wait
	Queues and device state
	Each device may have its own request queue
	Queues are managed using linked elements
	A busy flag indicates whether a device is in use

4. Transfer requests
How to request an I/O operation

A Control Block (CB) is used to describe the transfer:

	Input/output buffer address
	Parameters
	Status (filled after completion)

Typical call sequence:

	LEA CB
	CSV M:1O

For shared-memory I/O:

	CSV M:ZIO

When the call is made, the system associates an event with the request.
Control returns once the transfer is started or queued

5. Waiting for completion
Using M:WAIT

Purpose:

Wait until the transfer finishes

Behavior:

	If already finished → immediate return
	If still running:
	The task is suspended
	Lower-priority tasks can run
	The task resumes when the transfer ends

Return value:

	A ≥ 0: success
	A < 0: error (details in the control block)

Transfer validation modes
Standard mode (U = 0)
System interprets device status
Returns simplified error codes
Can branch to user-defined error handler
Fatal errors may stop the program
User mode (U = 1)
Raw status bits are returned
No interpretation by the system
Program must handle errors itself

6. Communication between supervisor and handlers
Initialization phase (M:1O)

The I/O supervisor:

Resets status flags in the control block
Checks if the device is busy
Sets up transfer parameters
Calls Handler 1

After return:

If no error → control returns to program
If error → status is stored and handled depending on mode
Transfer completion

When the device signals completion via interrupt:

Case 1 — Full block transfer

Handler 2:

Reads device status
Verifies correct execution
Calls M:1O2 (completion module)

M:1O2:

Handles errors
Resets internal parameters
Signals completion (activates event)
Resumes the waiting program
Case 2 — Incremental transfer

Handler 2:

Checks status after each step
If not finished → restarts via Handler 1
If finished → same as case 1

7. Important behavior note

Under the basic monitor:

Only one I/O request per device is allowed at a time
Additional requests will block until completion

Under more advanced monitors:

Multiple requests can be queued and processed sequentially

8. I/O hardware
A transfer is initiated by a program in RAM (handler)
- Status word
- General registers
- Transfer command
The transfer process is controlled by the firmware:

- Data transfer to memory

- Parity calculation

- End of sector detection
- End of transfer detection

- Head positioning control

The transfer report is provided in the status word accessible to the handler.
The hardwired logic section of the coupler consists of two MITRA 15 type boards, both of which have access to the MITRA minibus via connectors A, B, C, and D.

These two boards, DP15 and DP16, are connected to each other by wire-to-wire connections on connectors E and F. The first disk drive is connected to the coupler via connectors G and H of DP16.

The DP16 chip contains the logic corresponding to the disk drive interface.

The DP15 chip contains the coupler status word, the data receive and transmit buffers, and their parallel-to-serial and serial-to-parallel conversion.
The firmware is implemented on 7 pages of 32 micro-instructions each.

9. RD and WD instructions

    The key insight is that RD and WD instructions work with the E register to determine the operation mode (page 1 of the document mentions: "Le mode est déterminé par le contenu du registre E").
    RD/WD dispatch based on E register - The documentation clearly states "Le mode est déterminé par le contenu du registre E"

	    Fast Printer (Imprimante rapide) - E=3 for RD, various WD commands

	    SAGEM Disk - uses registers &3B-&3E with E=5 for commands

	    ASR33 Teletype - uses registers R9-R11 with E=1 for WD

	    Paper Tape Reader - E=8 for WD/RD

	    Paper Tape Punch - E=18

	    Card Reader - E=7 for WD, E=17 for RD

	    Console Panel (Pupitre) - E=&20, &10, etc.

	    DRI Disk - E=3, E=5, E=&15
	    

    Added register file access for the device registers (R9-R11 for ASR33, &3B-&3E for SAGEM, &39-&3E for DRI)

    Implemented status word formats per documentation:

        Fast printer status bits (0=prête, 1=prêt ligne, 2=fin papier, etc.)

        SAGEM ME register bits (11=adresse inexistante, 15=OK)

        DRI state word bits (3=erreur, 8=erreur disque, 9=non opérationnel, etc.)

9. Example with disk

	Register	Address		Fonction
	ADM			39			Transfert buffer address minus 2
	CM			3A			Number of words to transmit
	ADS			3C			Address of disk sector address (cylinder, head, sector)
        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
      |   cylinder 0-407         | T| sector 0-23  | D|
      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
			T = 0 -> lower head
			T = 1 -> upper head
			D = 0 -> movable disk
			D = 1 -> fixed disk
	ADR			3E			Set to 0
	PL			3D			longitudinal parity
	RT1			38			Working register 1
	RT2			3F			Working register 3
	WD						E = 0015, A : ADS configuration 
 */

#include "mitra_io.h"
#include "mitra_defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========== External References from mitra_cpu.c ========== */
extern uint16 G;                     /* general base register */
extern uint16 P;                     /* program counter */
extern uint16 A, E, X, L;
extern uint8 C, OV, MS, MA, PR;
extern uint16 M[];                   /* memory array */
extern uint32 MEMsize;
extern uint32 int_req;               /* interrupt request bits */
extern uint32 xfr_req;               /* transfer request bits */
extern void io_interrupt_dispatch(void);

/* Memory Access Functions (defined in mitra_cpu.c) */
extern uint16 read_word(uint16 addr);
extern void write_word(uint16 addr, uint16 val);
extern uint8 read_byte(uint16 va);
extern void write_byte(uint16 va, uint8 val);

/* SIMH Terminal I/O Functions (provided by SIMH framework) */
extern int sim_poll_kbd(void);
extern void sim_tt_putc(int ch);
extern int sim_tt_inchar(void);
extern int sim_tt_open(const char *file, int write);
extern int sim_tt_close(void);

/* ========== Device Function Declarations ========== */
/* DRI Disk */
extern t_stat dri_wd(uint16 e_reg, uint16 a_val);
extern t_stat dri_rd(uint16 e_reg, uint16 *result);
extern void dri_poll_devices(void);
extern void dri_reset(void);
extern t_stat dri_attach(int unit, const char *filename);
extern void dri_detach(int unit);

/* SAGEM Disk */
extern t_stat sagem_wd(uint16 e_reg, uint16 a_val);
extern t_stat sagem_rd(uint16 e_reg, uint16 *result);
extern void sagem_poll_devices(void);
extern void sagem_reset(void);
extern t_stat sagem_attach(int unit, const char *filename);
extern void sagem_detach(int unit);

/* Line printer */
extern t_stat printer_wd(uint16 e_reg, uint16 a_val);
extern t_stat printer_rd(uint16 e_reg, uint16 *result);
extern int printer_poll(void);
extern void printer_reset(void);
extern t_stat printer_attach(const char *filename);
extern void printer_detach(void);

/* Card Reader */
extern t_stat cdr_wd(uint16 e_reg, uint16 a_val);
extern t_stat cdr_rd(uint16 e_reg, uint16 *result);
extern int cdr_poll(void);
extern void cdr_reset(void);
extern t_stat cdr_attach(const char *filename);
extern void cdr_detach(void);

/* ASR33 Teletype */
extern t_stat asr33_wd(uint16 e_reg, uint16 a_val);
extern t_stat asr33_rd(uint16 e_reg, uint16 *result);
extern int asr_poll(void);
extern void asr33_reset(void);

/* Front Panel */
extern t_stat panel_wd(uint16 e_reg, uint16 a_val);
extern t_stat panel_rd(uint16 e_reg, uint16 *result);
extern void panel_reset(void);

/* Paper Tape Reader / Punch */
extern t_stat ptr_wd(uint16 e_reg, uint16 a_val);
extern t_stat ptr_rd(uint16 e_reg, uint16 *result);
extern int ptr_poll(void);
extern void ptr_reset(void);

extern t_stat ptp_wd(uint16 e_reg, uint16 a_val);
extern t_stat ptp_rd(uint16 e_reg, uint16 *result);
extern int ptp_poll(void);
extern void ptp_reset(void);

/* ========== Memory Access Helpers for Devices ========== */
/* These wrap the CPU memory access to provide the signature expected 
   by the device files (including the zio shared-memory flag). */

t_stat read_byte_io(uint32 addr, uint8 *val, int zio) {
    /* ZIO handling can be expanded here if needed. For now, passthrough. */
    *val = read_byte((uint16)addr);
    return SCPE_OK;
}

void write_byte_io(uint32 addr, uint8 val, int zio) {
    write_byte((uint16)addr, val);
}

/* ========== I/O Polling ========== */
void io_poll_devices(void) {
    /* Poll all active devices to drive asynchronous transfers */
    dri_poll_devices();
    sagem_poll_devices();
    cdr_poll();
    asr_poll();
    printer_poll();
    ptr_poll();
    ptp_poll();
}

/* ========== RD / WD Dispatch ========== */
/*
 * RD: Copy the contents of general (or I/O channel) register nnnnnn into the accumulator (A).
 * The mode is determined by the contents of E-register.
 * 
 *		Example:
 *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
 *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *      |        |   ADC     |        |     |   AD      |
 *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *										EC
 *										LE
 *										AC
 * RD meaning depends on the addressed controller.
 * 	AD is controller's primary address
 * 	ADC is the secondary address
 *	EC Writing order
 *	LE Reading order
 *	AC Suspension acknowledge only available by microprogramming
*/

t_stat io_rd(uint16 e_reg, uint16 *result) {
    /* RD: Copy contents of I/O register into accumulator A.
       The mode is determined by the contents of E-register. */
    
    switch (e_reg) {
        case 0x08:  /* Paper Tape Reader status */
            return ptr_rd(e_reg, result);

        case 0x10:  /* ASR33 Teletype status (E=&10 = octal 20 = decimal 16) */
            return asr33_rd(e_reg, result);

        case 0x11:  /* Card Reader status (E=17 decimal = 0x11) */
            return cdr_rd(e_reg, result);

        case 0x12:  /* Paper Tape Punch status (E=18 decimal = 0x12) */
            return ptp_rd(e_reg, result);

        case 0x15:  /* DRI Disk status (E=&15) */
            return dri_rd(e_reg, result);

        case 0x20:  /* Front Panel read keys (E=&20) */
            return panel_rd(e_reg, result);

        default:
            /* Unknown device or unhandled RD */
            return SCPE_IOERR;
    }
}

/*
 * WD: Copy the contents of the accumulator (A) general into register nnnnnn.
 * The write mode is determined by the contents of E-register, see RD above.
 *
 *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
 *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *      |        |   ADC     |        |     |   AD      |
 *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *										EC
 *										LE
 *										AC
 * WD meaning depends on the addressed controller.
*/
t_stat io_wd(uint16 e_reg, uint16 a_val) {
    /* WD: Copy accumulator A into I/O register.
       The mode is determined by the contents of E-register. */
    
    switch (e_reg) {
        case 0x00:  /* Front Panel RAZ (E=0) */
            return panel_wd(e_reg, a_val);

        case 0x01:  /* ASR33 Teletype command OR Front Panel (A=0x60, 0x120, 0x220) */
            /* Try Front Panel first; if it returns SCPE_IOERR, fall back to ASR33 */
            if (panel_wd(e_reg, a_val) == SCPE_OK) return SCPE_OK;
            return asr33_wd(e_reg, a_val);

        case 0x03:  /* DRI Disk selection (E=3) */
            return dri_wd(e_reg, a_val);

        case 0x05:  /* SAGEM Disk OR DRI Disk rest (E=5, A=&80) */
            if (a_val == 0x80) {
                return dri_wd(e_reg, a_val);
            }
            /* return sagem_wd(e_reg, a_val); */
            return SCPE_OK;

        case 0x07:  /* Card Reader command (E=7) */
            return cdr_wd(e_reg, a_val);

        case 0x08:  /* Paper Tape Reader command (E=8) */
            return ptr_wd(e_reg, a_val);

        case 0x10:  /* Front Panel write data lights (E=&10) */
            return panel_wd(e_reg, a_val);

        case 0x12:  /* Paper Tape Punch command (E=18 decimal = 0x12) */
            return ptp_wd(e_reg, a_val);

        case 0x15:  /* DRI Disk start transfer (E=&15) */
            return dri_wd(e_reg, a_val);

        case 0x20:  /* Front Panel write address lights (E=&20) */
            return panel_wd(e_reg, a_val);

        default:
            return SCPE_IOERR;
    }
}

/* ========== Device Initialization and Reset ========== */
void io_init_system(void) {
    dri_reset();
    cdr_reset();
    asr33_reset();
    panel_reset();
    ptr_reset();
    ptp_reset();
}

t_bool io_init(void) {
    io_init_system();
    return FALSE;
}

/* ========== SIMH Command Integration (Attach/Detach) ========== */
/* 
 * To integrate with the SIMH command parser, link these wrappers to your 
 * device's MTAB (modifier table) or register them as custom CLI commands.
 * Example MTAB entry:
 *   { MTAB_XTD | MTAB_VDV, 0, "ATTACH", "ATTACH", &io_cmd_attach, NULL, NULL, "Attach disk image" }
 */

t_stat io_attach_dri(int unit, const char *filename) {
    return dri_attach(unit, filename);
}

t_stat io_detach_dri(int unit) {
    dri_detach(unit);
    return SCPE_OK;
}

t_stat io_attach_cdr(const char *filename) {
    return cdr_attach(filename);
}

t_stat io_detach_cdr(void) {
    cdr_detach();
    return SCPE_OK;
}

t_stat io_attach_ptr(const char *filename) {
    /* Implement or return SCPE_NOATT if not supported */
    return SCPE_NOATT; 
}

t_stat io_detach_ptr(void) {
    return SCPE_OK;
}

t_stat io_attach_ptp(const char *filename) {
    /* Implement or return SCPE_NOATT if not supported */
    return SCPE_NOATT;
}

t_stat io_detach_ptp(void) {
    return SCPE_OK;
}


