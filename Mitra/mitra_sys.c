/* mitra_sys.c: CII Mitra 15/30 Simulator SCP Interface
 * adapted from sds_sys.c
 
 * Copyright (c) 2001-2020, Robert M Supnik
 * Copyright (c) 2026, Jean-Pierre Le Rouzic
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "mitra_defs.h"
#include "mitra_cpu.h"
#include "mitra_io.h"

#include <ctype.h>
#include <string.h>

#define FMTASC(x) ((x) < 040)? "<%03o>": "%c", (x)

/* ========== External Declarations ========== */
void io_init(void);

extern DEVICE cpu_dev;
extern DEVICE panel_dev;
extern DEVICE dri_dev;
extern DEVICE sagem_dev;
extern DEVICE asr_dev;
extern DEVICE ptr_dev;
extern DEVICE ptp_dev;
extern DEVICE cdr_dev;
extern DEVICE lp_dev;
extern DEVICE rtc_dev;

extern REG cpu_reg[];

/* CPU state is declared extern in mitra_cpu.h */
extern CPU_STATE cpu_state;
extern int susp_stack_ptr;

/* ========== SIMH Required Variables ========== */

/* sim_name: Simulator name string */
char sim_name[] = "Mitra 15/30";

/* sim_PC: Pointer to saved PC register descriptor
 * The PC is the first register in cpu_reg[] */
REG *sim_PC = &cpu_reg[0];  /* PC is the first register in cpu_reg[] (see mitra_cpu.c).
                                NOTE: this used to be set inside sim_init(), but SCP never
                                calls a function by that name, so it never ran and sim_PC
                                stayed NULL for the whole run -- any RUN/GO/STEP/CONTINUE
                                command dereferenced a NULL sim_PC and crashed. */

/* sim_emax: Number of words for examine (1 word at a time) */
int32 sim_emax = 1;

/* sim_devices: Array of pointers to simulated devices */
DEVICE *sim_devices[] = {
    &cpu_dev,
    &panel_dev,
    &rtc_dev,
    &dri_dev,
//    &sagem_dev,
    &ptr_dev,
    &ptp_dev,
    &asr_dev,
    &lp_dev,
    &cdr_dev,
    NULL
};

/* sim_stop_messages: Array of pointers to stop messages */
const char *sim_stop_messages[SCPE_BASE] = {
    "Unknown error",                    /* 0 */
    "IO device not ready",              /* 1 */
    "HALT instruction",                 /* 2 */
    "Breakpoint",                       /* 3 - STOP_IBKPT */
    "Invalid IO device",                /* 4 */
    "Invalid instruction",              /* 5 */
    "Invalid I/O operation",            /* 6 */
    "Nested indirects exceed limit",    /* 7 */
    "Nested EXU's exceed limit",        /* 8 */
    "Memory management trap",           /* 9 */
    "Trap during trap",                 /* 10 */
    "Trap instruction not BRM or BRU",  /* 11 */
    "RTC instruction not MIN or SKR",   /* 12 */
    "Interrupt vector zero",            /* 13 */
    "Runaway carriage control tape",    /* 14 */
    "Monitor-mode Breakpoint",          /* 15 */
    "Normal-mode Breakpoint",           /* 16 */
    "User-mode Breakpoint",             /* 17 */
    "Next expired"                      /* 18 */
};

/* ========== Memory Access Functions ========== */


/* ========== Binary Loader ========== */

/* Binary loader for Mitra 15 paper tape format
 * Format: 4 6-bit bytes per word (24 bits), but Mitra-15 is 16-bit.
 * The loader reads 6-bit groups and packs them into 16-bit words.
 * The first word loaded goes to address 0, then increments.
 */
t_stat sim_load(FILE *fileref, CONST char *cptr, CONST char *fnam, int flag) {
    int c;
    uint16 addr = 0;
    uint16 data = 0;
    int nibble_count = 0;
    t_stat r;
    
    sim_printf("\n[LOAD] Loading binary file: %s\n", fnam ? fnam : "(unnamed)");
    
    /* Paper tape format: 4 6-bit bytes per word */
    while ((c = fgetc(fileref)) != EOF) {
        if (c == 0) continue;  /* Null bytes are padding */
        
        data = (data << 6) | (c & 0x3F);
        nibble_count++;
        
        if (nibble_count == 4) {
            if (addr < MAX_MEM_WORDS) {
                M[addr] = data & 0xFFFF;
                sim_printf("[LOAD] addr=%04o data=%06o\n", addr, data & 0xFFFF);
                addr++;
            } else {
                sim_printf("[LOAD] Warning: address %04o exceeds memory limit\n", addr);
            }
            data = 0;
            nibble_count = 0;
        }
    }
    
    /* Set PC to start address from memory word 2 (address 2 in word addressing) */
    /* In Mitra-15, address 2 contains the entry point for the loaded program */
    if (addr > 0) {
        cpu_state.reg_P = M[2] & 0x7FFF;
        sim_printf("[LOAD] Entry point set to P=%04o (from M[2]=%06o)\n", 
               cpu_state.reg_P, M[2]);
    } else {
        sim_printf("[LOAD] Warning: No data loaded, P not set\n");
    }
    
    sim_printf("[LOAD] Load complete: %d words loaded\n", addr);
    return SCPE_OK;
}

/* ========== Symbolic Decode (for disassembly) ========== */

/* Opcode names for Group 1 instructions (LDA, LDE, etc.) */
static const char *group1_opnames[] = {
    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX"
};

/* Opcode names for Group 2 instructions (DLD, STA, etc.) */
static const char *group2_opnames[] = {
    "DLD", "STA", "STE", "STX", "SBL", "SBR", "DST", "ADM",
    "SPA", "STS", "FAD", "FSU", "FMU", "FDV", "TRS", "MVS"
};

/* Opcode names for Group 3 instructions (DLD, STA, etc.) */
static const char *group3_opnames[] = {
    "SHR", "SRG", "ICX", "DCX", "", "ICL", "DCL", "CSV",
    "CLS", "LDR", "STR", "LDP", "SHC", "TES", "", ""
};

/* Opcode names for Group 4 instructions (DLD, STA, etc.) */
static const char *group4_opnames[] = {
    "BCT", "BRX", "BOT", "BCF", "BAN", "BAZ", "BOF", "BRU"
};

/* Addressing mode names */
static const char *mode_names[] = {
    "DL",  /* 0 */
    "P",   /* 1 */
    "DG",  /* 2 */
    "IL",  /* 3 */
    "IGX", /* 4 */
    "ILX", /* 5 */
    "RP",  /* 6 */
    "RM"   /* 7 */
};

/* ========== SCP Interface Functions ========== */

/* sim_reset: Reset the simulator */
t_stat sim_reset(void) {
    t_stat r;
    
    /* Reset CPU */
    r = cpu_reset(&cpu_dev);
    if (r != SCPE_OK) return r;
    
    /* Reset all devices */
    io_init();
    
    return SCPE_OK;
}

/* ========== Disassembly Functions ========== */

/* These are required by SCP for symbolic debugging */

/* parse_sym: Parse symbolic input (address or register name) */
t_stat parse_sym(CONST char *cptr, t_addr addr, UNIT *uptr, t_value *val, int32 sw) {
    char temp[256];
    const char *p = cptr;
    int i;
    
    /* Skip leading whitespace */
    while (isspace(*p)) p++;
    
    /* Check for register names */
    if (isalpha(*p)) {
        /* Copy token */
        i = 0;
        while (isalnum(*p) && i < sizeof(temp)-1) temp[i++] = *p++;
        temp[i] = '\0';
        
        /* Check if it's a register name */
        /* Register names: A, E, X, P, L, G, C, OV, etc. */
        if (strcasecmp(temp, "A") == 0) {
            *val = cpu_state.reg_A;
            return SCPE_OK;
        } else if (strcasecmp(temp, "E") == 0) {
            *val = cpu_state.reg_E;
            return SCPE_OK;
        } else if (strcasecmp(temp, "X") == 0) {
            *val = cpu_state.reg_X;
            return SCPE_OK;
        } else if (strcasecmp(temp, "P") == 0) {
            *val = cpu_state.reg_P;
            return SCPE_OK;
        } else if (strcasecmp(temp, "L") == 0) {
            *val = cpu_state.reg_L;
            return SCPE_OK;
        } else if (strcasecmp(temp, "G") == 0) {
            *val = cpu_state.reg_G;
            return SCPE_OK;
        } else if (strcasecmp(temp, "C") == 0) {
            *val = cpu_state.C;
            return SCPE_OK;
        } else if (strcasecmp(temp, "OV") == 0) {
            *val = cpu_state.OV;
            return SCPE_OK;
        } else if (strcasecmp(temp, "MS") == 0) {
            *val = cpu_state.MS;
            return SCPE_OK;
        } else if (strcasecmp(temp, "MA") == 0) {
            *val = cpu_state.MA;
            return SCPE_OK;
        } else if (strcasecmp(temp, "PR") == 0) {
            *val = cpu_state.PR;
            return SCPE_OK;
        }
        
        /* Unknown symbol */
        return SCPE_ARG;
    }
    
    /* Parse as numeric address */
    {
        t_stat r = SCPE_OK;
        *val = (t_value)get_uint(p, 16, 0xFFFF, &r);
        return r;
    }
}

/* fprint_sym: Print symbolic output (disassemble instruction) */
t_stat fprint_sym(FILE *of, t_addr addr, t_value *val, UNIT *uptr, int32 sw) {
    uint16 inst;
    int mode, opcode;
    uint16 disp;
    const char *opname = NULL;
    const char *modename = NULL;
    
    /* If val is NULL, read from memory at addr */
    if (val == NULL) {
        if (addr >= MAX_MEM_WORDS) return SCPE_NXM;
        inst = M[addr];
    } else {
        inst = *val & 0xFFFF;
    }
    
    mode = (inst >> 13) & 0x07;
    opcode = (inst >> 8) & 0x1F;
    disp = inst & 0x00FF;
    
    /* Addressing mode names */
    static const char *mode_names[] = {
        "DL", "P", "DG", "IL", "IGX", "ILX", "RP", "RM"
    };
    modename = (mode < 8) ? mode_names[mode] : "??";
    
    /* Determine instruction group and get opcode name */
    uint16 hexcode = inst & 0xF000;
    
    /*
	hexcode	mode	formula
	0x0	DL	Y = (L) + D
	0x1	DL (store family)	Y = (L) + D
	0x2	P	immediate (=n)
	0x3	DL (system group)	ICX, DCX, ICL, DCL, LDR, STR, TES, SHR...
	0x4	DG	Y = (G) + D
	0x5	DG (store family)	Y = (G) + D
	0x6	IL	Y = G′ + mem[(L)+D]
	0x7	IL (store family)	Y = G′ + mem[(L)+D]
	0x8	IGX	Y = (G) + mem[(G)+D] + (X)
	0x9	IGX (store family)	Y = (G) + mem[(G)+D] + (X)
	0xA	ILX	Y = G′ + mem[(L)+D] + (X)
	0xB	ILX (store family)	Y = G′ + mem[(L)+D] + (X)
	0xC	RP / RM	branches (bit 11 picks the sub-form)
	0xD	D-IL / D-IG	indirect branch forms
	0xE	PX	system group, Y = mem[(X)]
	0xF	RM	system group (STM/CLM/DIT/RD/WD/SHR/SHC/LDR/STR...)
    */
    
    switch (hexcode) {
        case 0x0000:
        case 0x2000:
        case 0x4000:
        case 0x6000:
        case 0x8000:
        case 0xA000:
            /* 
            Group 1 instructions 
                opcode = (inst >> 8) & 0x1F;
            */
            if (opcode < 16) {
                static const char *g1_names[] = {
                    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
                    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX"
                };
                opname = g1_names[opcode];
            }
            break;
        case 0x1000:
        case 0x3000:
        case 0x5000:
        case 0x7000:
        case 0x9000:
        case 0xB000:
        case 0xE000:
        case 0xF000:
            /* 
            Group 2 instructions 
            opcode = (inst >> 8) & 0x1F;
            */
            if (opcode < 16) {
                static const char *g2_names[] = {
                    "DLD", "STA", "STE", "STX", "SBL", "SBR", "DST", "ADM",
                    "SPA", "STS", "FAD", "FSU", "FMU", "FDV", "TRS", "MVS"
                };
                opname = g2_names[opcode];
            } else {
                /* System instructions */
                static const char *sys_names[] = {
                    "SHR", "SRG", "ICX", "DCX", "SYS", "ICL", "DCL", "CSV",
                    "CLS", "LDR", "STR", "LDP", "SHC", "TES", "???", "???"
                };
                if ((opcode >= 0x10 && opcode <= 0x1F) && opcode != 0x14) {
                    opname = sys_names[opcode - 0x10];
                    }
                else {
                    // It's an intruction in the SYS group
                    switch(disp) {
                        case 0x00:
                            opname = "CLM";
                            break;
                        case 0x01:
                            opname = "DIT";
                            break;
                        case 0x02:
                            opname = "RD";
                            break;
                        case 0x03:
                            opname = "WD";
                            break;
                        case 0x08:
                            opname = "STM";
                            break;
                    }
                }
            }
            break;
        case 0xC000:
        case 0xD000:
            /* Branch instructions */
            switch (opcode & 0x07) {
                case 0: opname = "BCT"; break;
                case 1: opname = "BRX"; break;
                case 2: opname = "BOT"; break;
                case 3: opname = "BCF"; break;
                case 4: opname = "BAN"; break;
                case 5: opname = "BAZ"; break;
                case 6: opname = "BOF"; break;
                case 7: opname = "BRU"; break;
            }
            break;
    }
    
    /* Print instruction */
    if (opname) {
        if (disp) {
            fprintf(of, "%s\t%s\t#%03x", opname, modename, disp);
        } else {
            fprintf(of, "%s\t%s", opname, modename);
        }
    } else {
        fprintf(of, "??\t%04X", inst);
    }
    
    return SCPE_OK;
}

/* Get addressing mode from instruction */
static int get_mode(uint16 inst) {
    /* Mode is encoded in bits 13-15 (0-7) */
    return (inst >> 13) & 0x07;
}

/* Get opcode from instruction */
static int get_opcode(uint16 inst) {
    return (inst >> 8) & 0x1F;
}

/* Decode instruction and print to file */
void fprint_inst(FILE *of, uint16 inst, uint16 addr) {
    int mode = get_mode(inst);
    int opcode = get_opcode(inst);
    uint16 disp = inst & 0x00FF;
    uint16 hexcode = inst & 0xF000;
    const char *opname = NULL;
    const char *modename = (mode < 8) ? mode_names[mode] : "??";
    
    /* Determine instruction group and get opcode name */
    switch (hexcode) {
        case 0x0000:
        case 0x4000:
        case 0x6000:
        case 0x8000:
        case 0xA000:
        case 0x2000:
            /* Group 1 instructions */
            if (opcode < 16) opname = group1_opnames[opcode];
            break;
        case 0x1000:
        case 0x5000:
        case 0x7000:
        case 0x9000:
        case 0xB000:
        case 0x3000:
        case 0xE000:
        case 0xF000:
            /* Group 2 or system instructions */
            if (opcode < 16) opname = group2_opnames[opcode];
            break;
        case 0xC000:
        case 0xD000:
            /* Branch instructions */
            switch (opcode & 0x07) {
                case 0: opname = "BCT"; break;
                case 1: opname = "BRX"; break;
                case 2: opname = "BOT"; break;
                case 3: opname = "BCF"; break;
                case 4: opname = "BAN"; break;
                case 5: opname = "BAZ"; break;
                case 6: opname = "BOF"; break;
                case 7: opname = "BRU"; break;
            }
            break;
    }
    
    if (opname) {
        if (disp) {
            fprintf(of, "%s\t%s\t#%03x", opname, modename, disp);
        } else {
            fprintf(of, "%s\t%s", opname, modename);
        }
    } else {
        fprintf(of, "??\t%04X", inst);
    }
}

/* ========== Symbol Table (for debug) ========== */

/* Simple symbol table support */
typedef struct {
    const char *name;
    uint16 addr;
} Symbol;

static Symbol symbol_table[] = {
    /* Add symbols here as needed */
    { NULL, 0 }
};

/* Look up symbol by name */
static uint16 lookup_symbol(const char *name) {
    int i;
    for (i = 0; symbol_table[i].name != NULL; i++) {
        if (strcmp(symbol_table[i].name, name) == 0)
            return symbol_table[i].addr;
    }
    return 0xFFFF;
}

/* rtc_svc: Called by RTC device when timer expires */
t_stat rtc_svc(UNIT *uptr) {
    /* RTC interrupt handling */
    /* This is called from the RTC device when it triggers */
    return SCPE_OK;
}

/* ========== SIMH Boot Support ========== */

/* Boot from device */
t_stat sim_boot(DEVICE *dptr) {
    /* Default boot: load from paper tape reader */
    /* This would be overridden by specific device boots */
    return SCPE_NOFNC;
}

t_stat sim_shutdown(void) {
    return SCPE_OK;
}


/* ========== Help Support ========== */

/* Show help for simulator */
t_stat sim_help(FILE *of, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr) {
    fprintf(of, "Mitra 15/30 Simulator\n\n");
    fprintf(of, "Commands:\n");
    fprintf(of, "  LOAD <file>    - Load binary paper tape format\n");
    fprintf(of, "  BOOT           - Boot from default device\n");
    fprintf(of, "  GO             - Start execution\n");
    fprintf(of, "  EXAMINE/DEPOSIT - Examine/deposit memory or registers\n");
    return SCPE_OK;
}

/* ========== Additional SCP Functions ========== */

/* These may be called by SCP */

t_bool sim_is_switches_enabled(void) {
    return FALSE;
}

t_stat sim_interval_service(void) {
    return SCPE_OK;
}


/* ========== Simulator Initialization ========== */

/* This is called when the simulator starts */
t_stat sim_init(void) {
    /* Initialize memory to zero */
//    memset(M, 0, sizeof(M));
    
    /* Initialize all devices */
    cpu_reset(&cpu_dev);
    rtc_reset(&rtc_dev);
    io_init();

    return SCPE_OK;
}

/* ========== Main Entry Point ========== */

/* The main function is provided by SIMH */
/* This file just provides the interface */
