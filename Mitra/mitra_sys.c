/* mitra_sys.c: Mitra 15 simulator interface
 *
 * Based on MITRA 15 Reference Manual (CII, 1973)
 */

#include "mitra_defs.h"
#include <ctype.h>

/* External device declarations */
extern DEVICE cpu_dev;
extern DEVICE ptr_dev;   /* Paper tape reader */
extern DEVICE ptp_dev;   /* Paper tape punch */
extern DEVICE tti_dev;   /* Typewriter input */
extern DEVICE tto_dev;   /* Typewriter output */
extern DEVICE lpt_dev;   /* Line printer */
extern DEVICE rtc_dev;   /* Real-time clock */

/* Memory */
extern uint16 M[MAXMEMSIZE];
extern uint16 P, A, E, X, L, G, C, OV, MS, MA, PR;
extern uint16 cpu_mode;
extern uint16 int_req;

/* SCP data structures */
char sim_name[] = "Mitra 15";
REG *sim_PC = NULL;
int32 sim_emax = 1;

/* Stop messages (manual section 11-8.3) */
const char *sim_stop_messages[] = {
    "Unknown error",
    "I/O device not ready",
    "HALT instruction",
    "Breakpoint",
    "Invalid I/O device",
    "Invalid instruction",
    "Invalid I/O operation",
    "Indirect addressing limit exceeded",
    "EXU loop limit exceeded",
    "Memory management trap during interrupt",
    "Memory management trap during trap",
    "Trap instruction invalid",
    "RTC instruction not valid",
    "Interrupt vector zero",
    NULL
};

/* Device list */
DEVICE *sim_devices[] = {
    &cpu_dev,
    &ptr_dev,
    &ptp_dev,
    &tti_dev,
    &tto_dev,
    &lpt_dev,
    &rtc_dev,
    NULL
};

/* Instruction opcode table (86 instructions from manual) */
static const char *opcode_table[256] = {
    /* 00-0F */
    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
    /* 10-1F */
    "DLD", "STA", "STE", "STX", "SBL", "SBR", "DST", "ADM",
    "SPA", "STS", "FAD", "FSU", "FMU", "FDV", "TRS", "MVS",
    /* 20-2F (p mode - same as 00-0F) */
    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
    /* 30-3F */
    "SHR", "SRG", "ICX", "DCX", "",   "ICL", "DCL", "CSV",
    "CLS", "LDR", "STR", "LDP", "SHC", "TES", "",   "",
    /* 40-4F (DG mode - same as 00-0F) */
    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
    /* 50-5F */
    "DLD", "STA", "STE", "STX", "SBL", "SBR", "DST", "ADM",
    "SPA", "STS", "FAD", "FSU", "FMU", "FDV", "TRS", "MVS",
    /* 60-6F (IL mode - same as 00-0F) */
    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
    /* 70-7F */
    "DLD", "STA", "STE", "STX", "SBL", "SBR", "DST", "ADM",
    "SPA", "STS", "FAD", "FSU", "FMU", "FDV", "TRS", "MVS",
    /* 80-8F (IGX mode - same as 00-0F) */
    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
    /* 90-9F */
    "DLD", "STA", "STE", "STX", "SBL", "SBR", "DST", "ADM",
    "SPA", "STS", "FAD", "FSU", "FMU", "FDV", "TRS", "MVS",
    /* A0-AF (ILX mode - same as 00-0F) */
    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
    /* B0-BF */
    "DLD", "STA", "STE", "STX", "SBL", "SBR", "DST", "ADM",
    "SPA", "STS", "FAD", "FSU", "FMU", "FDV", "TRS", "MVS",
    /* C0-CF (Class 2 - RP mode) */
    "BCT", "BRX", "BOT", "BCF", "BAN", "BAZ", "BOF", "BRU",
    "BCT", "BRX", "BOT", "BCF", "BAN", "BAZ", "BOF", "BRU",
    /* D0-DF (Class 2 - RM/IL/IG modes) */
    "BCT", "BRX", "BOT", "BCF", "BAN", "BAZ", "BOF", "BRU",
    "BCT", "BRX", "BOT", "BCF", "BAN", "BAZ", "BOF", "BRU",
    /* E0-EF (PX mode - class 1) */
    "SHR", "SRG", "ICX", "DCX", "",   "ICL", "DCL", "CSV",
    "CLS", "LDR", "STR", "LDP", "SHC", "TES", "",   "",
    /* F0-FF (p mode - class 1 and system) */
    "SHR", "SRG", "ICX", "DCX", "SYS", "ICL", "DCL", "CSV",
    "CLS", "LDR", "STR", "LDP", "SHC", "TES", "",   ""
};

const int8 odd_par[64] = {
    0100, 0001, 0002, 0103, 0004, 0105, 0106, 0007,
    0010, 0111, 0112, 0013, 0114, 0015, 0016, 0117,
    0020, 0121, 0122, 0023, 0124, 0025, 0026, 0127,
    0130, 0031, 0032, 0133, 0034, 0135, 0136, 0037,
    0040, 0141, 0142, 0043, 0144, 0045, 0046, 0147,
    0150, 0051, 0052, 0153, 0054, 0155, 0156, 0057,
    0160, 0061, 0062, 0163, 0064, 0165, 0166, 0067,
    0070, 0171, 0172, 0073, 0174, 0075, 0076, 0177
    };

/* Addressing mode strings (manual page 5-1 to 5-3) */
static const char *mode_strings[] = {
    "",      /* AM_DL */
    "@",     /* AM_IL */
    "@,X",   /* AM_ILX */
    "",      
    "#",     /* AM_DG */
    "@#X",   /* AM_IGX */
    "=",     /* AM_P */
    "=,X"    /* AM_PX (class 1 only) */
};

/* Class 2 addressing mode strings (manual page 5-3) */
static const char *mode2_strings[] = {
    "", "", "", "", "", "",
    "",      /* AM_RP - relative plus */
    ""       /* AM_RM - relative minus */
};

/* Get opcode mnemonic */
static const char *get_mnemonic(uint16 inst)
{
    uint16 opcode = (inst >> 8) & 0xFF;
    const char *mnemonic = opcode_table[opcode];
    return mnemonic ? mnemonic : "???";
}

/* Get addressing mode string */
static const char *get_mode_string(uint16 inst)
{
    uint16 mode = (inst >> 13) & 0x07;
    uint16 opcode = (inst >> 8) & 0xFF;
    
    /* Class 2 instructions use different mode interpretation */
    if (opcode >= 0xC0 && opcode <= 0xDF) {
        if (mode == 6) return "";   /* RP - just displacement */
        if (mode == 7) return "-";  /* RM - minus sign before displacement */
        if (mode == 1) return "@";
        if (mode == 4) return "@#";
        return "";
    }
    
    if (mode < 8 && mode_strings[mode])
        return mode_strings[mode];
    return "";
}

/* Symbolic disassembly (manual section V) */
t_stat fprint_sym(FILE *of, t_addr addr, t_value *val, UNIT *uptr, int32 sw)
{
    uint16 inst = val[0] & 0xFFFF;
    uint16 disp = inst & 0xFF;
    uint16 mode = (inst >> 13) & 0x07;
    uint16 opcode = (inst >> 8) & 0xFF;
    const char *mnemonic = get_mnemonic(inst);
    const char *mode_str = get_mode_string(inst);
    
    if (sw & SWMASK('M')) {
        /* Check for SYS instructions (F4 opcode with special disp) */
        if (opcode == 0xF4) {
            switch (disp) {
                case 0x00: mnemonic = "RTS"; break;
                case 0x01: mnemonic = "DIT"; break;
                case 0x02: mnemonic = "RD"; break;
                case 0x03: mnemonic = "WD"; break;
                case 0x08: mnemonic = "STM"; break;
                case 0x0C: mnemonic = "CLM"; break;
                case 0x20: mnemonic = "DITR"; break;
                case 0x40: mnemonic = "RSV"; break;
                default: break;
            }
            fprintf(of, "%s", mnemonic);
            return SCPE_OK;
        }
        
        /* Check for SRG family special mnemonics (manual page 7-56) */
        if ((opcode == 0x31 || opcode == 0xE1 || opcode == 0xF1) && 
            (mode == 6 || mode == 7)) {
            switch (disp & 0x1F) {
                case 0x02: mnemonic = "XAE"; break;
                case 0x04: mnemonic = "XAX"; break;
                case 0x06: mnemonic = "XEX"; break;
                case 0x08: mnemonic = "XAA"; break;
                case 0x0A: mnemonic = "CCE"; break;
                case 0x0E: mnemonic = "ACE"; break;
                case 0x10: mnemonic = "CCA"; break;
                case 0x12: mnemonic = "AEE"; break;
                case 0x14: mnemonic = "CNX"; break;
                case 0x16: mnemonic = "AIE"; break;
                case 0x18: mnemonic = "AAE"; break;
                case 0x1A: mnemonic = "LNE"; break;
                case 0x1C: mnemonic = "CNA"; break;
                case 0x1E: mnemonic = "CHX"; break;
                default: break;
            }
            fprintf(of, "%s", mnemonic);
            return SCPE_OK;
        }
        
        /* Class 2 relative branches - show displacement as label offset */
        if (opcode >= 0xC0 && opcode <= 0xDF) {
            int16 offset = disp;
            if (mode == 7) offset = -disp;
            fprintf(of, "%s %+d", mnemonic, offset);
            return SCPE_OK;
        }
        
        /* Normal instruction with addressing mode */
        if (mode_str && mode_str[0]) {
            fprintf(of, "%s %s%03o", mnemonic, mode_str, disp);
        } else {
            fprintf(of, "%s %03o", mnemonic, disp);
        }
        return SCPE_OK;
    }
    
    /* Default: print raw hex */
    fprintf(of, "%04o", inst);
    return SCPE_OK;
}

/* Symbolic input */
t_stat parse_sym(CONST char *cptr, t_addr addr, UNIT *uptr, t_value *val, int32 sw)
{
    char mnemonic[16];
    char arg[16];
    uint16 inst = 0;
    int i;
    
    /* Skip whitespace */
    while (isspace(*cptr)) cptr++;
    
    /* Parse mnemonic */
    i = 0;
    while (*cptr && !isspace(*cptr) && *cptr != ',' && i < 15) {
        mnemonic[i++] = toupper(*cptr++);
    }
    mnemonic[i] = '\0';
    
    /* Skip whitespace after mnemonic */
    while (isspace(*cptr)) cptr++;
    
    /* Parse argument */
    i = 0;
    while (*cptr && !isspace(*cptr) && i < 15) {
        arg[i++] = *cptr++;
    }
    arg[i] = '\0';
    
    /* Look up mnemonic in opcode table */
    for (i = 0; i < 256; i++) {
        if (opcode_table[i] && strcmp(opcode_table[i], mnemonic) == 0) {
            uint16 opcode = i;
            uint16 mode = 0;
            uint16 disp = 0;
            
            /* Parse addressing mode prefix */
            if (arg[0] == '@') {
                if (arg[1] == '#') {
                    mode = 5;  /* IGX */
                    if (arg[2]) disp = atoi(arg + 2);
                } else if (arg[1] == ',') {
                    mode = 2;  /* ILX */
                    if (arg[2]) disp = atoi(arg + 2);
                } else {
                    mode = 1;  /* IL */
                    if (arg[1]) disp = atoi(arg + 1);
                }
            } else if (arg[0] == '#') {
                mode = 4;  /* DG */
                if (arg[1]) disp = atoi(arg + 1);
            } else if (arg[0] == '=') {
                mode = 6;  /* p */
                if (arg[1]) disp = atoi(arg + 1);
            } else if (arg[0] == '-') {
                mode = 7;  /* RM */
                if (arg[1]) disp = atoi(arg + 1);
            } else if (arg[0] == '+') {
                mode = 6;  /* RP */
                if (arg[1]) disp = atoi(arg + 1);
            } else if (isdigit(arg[0]) || arg[0] == '-') {
                mode = 0;  /* DL */
                disp = atoi(arg);
            }
            
            /* Special case for SYS instructions */
            if (strcmp(mnemonic, "RTS") == 0) {
                inst = 0xF400;
            } else if (strcmp(mnemonic, "DIT") == 0) {
                inst = 0xF401;
            } else if (strcmp(mnemonic, "RD") == 0) {
                inst = 0xF402;
            } else if (strcmp(mnemonic, "WD") == 0) {
                inst = 0xF403;
            } else if (strcmp(mnemonic, "STM") == 0) {
                inst = 0xF408;
            } else if (strcmp(mnemonic, "CLM") == 0) {
                inst = 0xF40C;
            } else if (strcmp(mnemonic, "RSV") == 0) {
                inst = 0xF440;
            } else {
                /* Build instruction word */
                inst = (mode << 13) | (opcode << 8) | (disp & 0xFF);
            }
            
            *val = inst;
            return SCPE_OK;
        }
    }
    
    return SCPE_ARG;
}

/* Binary loader for Mitra 15 paper tape format */
t_stat sim_load(FILE *fileref, CONST char *cptr, CONST char *fnam, int flag)
{
    int c;
    uint16 addr = 0;
    uint16 data = 0;
    int nibble_count = 0;
    
    /* Paper tape format: 4 6-bit bytes per word */
    while ((c = fgetc(fileref)) != EOF) {
        if (c == 0) continue;  /* Null bytes are padding */
        
        data = (data << 6) | (c & 0x3F);
        nibble_count++;
        
        if (nibble_count == 4) {
            M[addr++] = data;
            data = 0;
            nibble_count = 0;
        }
    }
    
    /* Set PC to start address (usually address 2 contains bootstrap) */
    P = M[2];
    
    return SCPE_OK;
}

/* CPU reset */
t_stat cpu_reset(DEVICE *dptr)
{
    A = E = X = L = G = P = 0;
    C = OV = MS = MA = PR = 0;
    cpu_mode = 0;
    int_req = 0;
    return SCPE_OK;
}
