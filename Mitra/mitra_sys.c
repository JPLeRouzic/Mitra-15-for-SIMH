/* mitra_sys.c: Mitra 15 simulator interface
 *
 * Based on MITRA 15 Reference Manual (CII, 1973)
 * Integrates CPU, RTC, and I/O peripherals (DRI, CDR, ASR33, Panel, PTR, PTP)
 */

#include "mitra_defs.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "mitra_io.h"

/* ========== External Declarations from mitra_cpu.c ========== */
extern uint16 M[MAXMEMSIZE];
extern uint16 P, A, E, X, L, G;
extern uint8  C, OV, MS, MA, PR;
extern uint16 cpu_mode;
extern uint32 int_req;               /* Corrected to uint32 to match mitra_io.c */

extern DEVICE cpu_dev;
extern DEVICE rtc_dev;               /* Real-time clock */

/* External I/O Device Functions (from IO_*.c files) */
extern t_stat dri_attach(int unit, const char *filename);
extern void   dri_detach(int unit);
extern void   dri_reset(void);

extern t_stat sagem_attach(int unit, const char *filename);
extern void   sagem_detach(int unit);
extern void   sagem_reset(void);

extern t_stat cdr_attach(const char *filename);
extern void   cdr_detach(void);
extern void   cdr_reset(void);

extern t_stat magtape_attach(const char *filename);
extern void   magtape_detach(void);
extern void   magtape_reset(void);

extern t_stat asr33_attach(const char *filename);
extern void   asr33_detach(void);
extern void   asr33_reset(void);

extern void   panel_reset(void);
extern void   ptr_reset(void);
extern void   ptp_reset(void);

/* SIMH wrapper prototypes */

static t_stat dri_sim_attach   (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
static t_stat dri_sim_detach   (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
static t_stat dri_sim_reset    (DEVICE *dptr);

static t_stat sagem_sim_attach (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
static t_stat sagem_sim_detach (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
static t_stat sagem_sim_reset  (DEVICE *dptr);

static t_stat cdr_sim_attach   (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
static t_stat cdr_sim_detach   (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
static t_stat cdr_sim_reset    (DEVICE *dptr);

static t_stat printer_sim_attach (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
static t_stat printer_sim_detach (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
static t_stat printer_sim_reset  (DEVICE *dptr);

static t_stat asr_sim_attach   (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
static t_stat asr_sim_detach   (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
static t_stat asr_sim_reset    (DEVICE *dptr);

static t_stat panel_sim_reset (DEVICE *dptr);
static t_stat ptr_sim_reset   (DEVICE *dptr);
static t_stat ptp_sim_reset   (DEVICE *dptr);

extern t_stat printer_attach (const char *filename);
extern void   printer_detach (void);
extern void   printer_reset (void);

/* ========== SIMH Core Data Structures ========== */
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

/* ========== SIMH Device Structures ========== */
/* Note: The DEVICE struct initialization assumes a standard SIMH layout. 
   Adjust the number of trailing NULLs if your mitra_defs.h DEVICE struct has more fields. */

/* DRI Disk (2 units) */
UNIT dri_unit[] = {
    {
        .action = NULL,
        .flags  = UNIT_FIX | UNIT_BINK,
        .capac  = 0
    }
};

MTAB dri_mod[] = {
    { MTAB_XTD|MTAB_VUN, 0, "ATTACH", "ATTACH", &dri_sim_attach, NULL, NULL, "Attach disk image" },
    { MTAB_XTD|MTAB_VUN, 0, "DETACH", "DETACH", &dri_sim_detach, NULL, NULL, "Detach disk image" },
    { 0 }
};

DEVICE dri_dev = {
    "DRI", dri_unit, NULL, dri_mod,
    2, 8, 16, 1, 8, 16,
    NULL, NULL, &dri_sim_reset, NULL, NULL, NULL, NULL, 0, 0
};

/* SAGEM Disk (2 units) */
UNIT sagem_unit[] = {
    {
        .action = NULL,
        .flags  = UNIT_FIX | UNIT_BINK,
        .capac  = 0
    }
};

MTAB sagem_mod[] = {
    { MTAB_XTD|MTAB_VUN, 0, "ATTACH", "ATTACH", &sagem_sim_attach, NULL, NULL, "Attach disk image" },
    { MTAB_XTD|MTAB_VUN, 0, "DETACH", "DETACH", &sagem_sim_detach, NULL, NULL, "Detach disk image" },
    { 0 }
};

DEVICE sagem_dev = {
    "SAGEM", sagem_unit, NULL, sagem_mod,
    2, 8, 16, 1, 8, 16,
    NULL, NULL, &sagem_sim_reset, NULL, NULL, NULL, NULL, 0, 0
};

/* Card Reader (1 unit) */
UNIT cdr_unit[] = {
    {
        .action = NULL,
        .flags  = UNIT_SEQ | UNIT_BINK,
        .capac  = 0
    }
};

MTAB cdr_mod[] = {
    { MTAB_XTD|MTAB_VUN, 0, "ATTACH", "ATTACH", &cdr_sim_attach, NULL, NULL, "Attach card deck" },
    { MTAB_XTD|MTAB_VUN, 0, "DETACH", "DETACH", &cdr_sim_detach, NULL, NULL, "Detach card deck" },
    { 0 }
};

DEVICE cdr_dev = {
    "CDR", cdr_unit, NULL, cdr_mod,
    1, 8, 16, 1, 8, 16,
    NULL, NULL, &cdr_sim_reset, NULL, NULL, NULL, NULL, 0, 0
};

/* ASR33 Teletype (Console) */
UNIT asr_unit[] = {
    {
        .action = NULL
    }
};

DEVICE asr_dev = {
    "ASR", asr_unit, NULL, NULL,
    1, 8, 16, 1, 8, 16,
    NULL, NULL, &asr_sim_reset, NULL, NULL, NULL, NULL, 0, 0
};

/* Line printer */
UNIT lp_unit[] = {
    {
        .action = NULL
    }
};

DEVICE printer_dev = {
    "LP", lp_unit, NULL, NULL,
    1, 8, 16, 1, 8, 16,
    NULL, NULL, &printer_sim_reset, NULL, NULL, NULL, NULL, 0, 0
};

/* Front Panel */
UNIT panel_unit[] = {
    {
        .action = NULL
    }
};

DEVICE panel_dev = {
    "PANEL", panel_unit, NULL, NULL,
    1, 8, 16, 1, 8, 16,
    NULL, NULL, &panel_sim_reset, NULL, NULL, NULL, NULL, 0, 0
};

/* Paper Tape Reader / Punch */
UNIT ptr_unit[] = {
    {
        .action = NULL,
        .flags = UNIT_SEQ | UNIT_BINK
    }
};

DEVICE ptr_dev = {
    "PTR", ptr_unit, NULL, NULL,
    1, 8, 16, 1, 8, 16,
    NULL, NULL, &ptr_sim_reset, NULL, NULL, NULL, NULL, 0, 0
};

UNIT ptp_unit[] = {
    {
        .action = NULL,
        .flags = UNIT_SEQ | UNIT_BINK
    }
};

DEVICE ptp_dev = {
    "PTP", ptp_unit, NULL, NULL,
    1, 8, 16, 1, 8, 16,
    NULL, NULL, &ptp_sim_reset, NULL, NULL, NULL, NULL, 0, 0
};

/* Device list */
DEVICE *sim_devices[] = {
    &cpu_dev,
    &rtc_dev,
    &dri_dev, // DRI fixed disk
    &sagem_dev, // SAGEM fixed disk
    &cdr_dev, // Card reader
    &asr_dev, // ASR33
    &panel_dev, // Mitra's front panel
    &printer_dev, // Line printer
//    &mag_tape_dev, // magnetic tape reader
    &ptr_dev, // Paper Tape reader
    &ptp_dev, // Paper Tape puncher
    NULL
};

/* ========== Instruction Opcode Table ========== */
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
    "SHR", "SRG", "ICX", "DCX", "",    "ICL", "DCL", "CSV",
    "CLS", "LDR", "STR", "LDP", "SHC", "TES", "",    "",
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
    "SHR", "SRG", "ICX", "DCX", "",    "ICL", "DCL", "CSV",
    "CLS", "LDR", "STR", "LDP", "SHC", "TES", "",    "",
    /* F0-FF (p mode - class 1 and system) */
    "SHR", "SRG", "ICX", "DCX", "SYS", "ICL", "DCL", "CSV",
    "CLS", "LDR", "STR", "LDP", "SHC", "TES", "",    ""
};

const int8 odd_par[64] = {
    0100, 0001, 0002, 0103, 0004, 0105, 0106, 0007,
    0010, 0111, 0112, 0013, 0114, 0015, 0016, 0117,
    0020, 0121, 0122, 0023, 0124, 0025, 0026, 0127,
    0130, 0031, 0032, 0133, 0034, 0135, 0136, 0037,
    0040, 0141, 0142, 0043, 0144, 0045, 0046, 0147,
    0150, 0051, 0052, 0153, 0154, 0055, 0156, 0057,
    0160, 0061, 0062, 0163, 0064, 0165, 0166, 0067,
    0070, 0171, 0172, 0073, 0174, 0075, 0176, 0077
};

/* Addressing mode strings */
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

/* ========== Disassembler / Assembler Helpers ========== */

static const char *get_mnemonic(uint16 inst) {
    uint16 opcode = (inst >> 8) & 0xFF;
    const char *mnemonic = opcode_table[opcode];
    return (mnemonic && mnemonic[0]) ? mnemonic : "???";
}

static const char *get_mode_string(uint16 inst) {
    uint16 mode = (inst >> 13) & 0x07;
    uint16 opcode = (inst >> 8) & 0xFF;
    
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

/* Symbolic disassembly */
t_stat fprint_sym(FILE *of, t_addr addr, t_value *val, UNIT *uptr, int32 sw)
{
    uint16 inst = val[0] & 0xFFFF;
    uint16 disp = inst & 0xFF;
    uint16 mode = (inst >> 13) & 0x07;
    uint16 opcode = (inst >> 8) & 0xFF;
    const char *mnemonic = get_mnemonic(inst);
    const char *mode_str = get_mode_string(inst);
    
    if (sw & SWMASK('M')) {
        /* CSV Instructions (0x37 and 0xF7) */
        if (opcode == 0x37 || opcode == 0xF7) {
            if (disp == M_10_SECTION)      fprintf(of, "CSV M:1O");
            else if (disp == M_ZIO_SECTION)  fprintf(of, "CSV M:ZIO");
            else if (disp == M_WAIT_SECTION) fprintf(of, "CSV M:WAIT");
            else if (disp == M_ZWAT_SECTION) fprintf(of, "CSV M:ZWAT");
            else                             fprintf(of, "CSV %03o", disp);
            return SCPE_OK;
        } 
        
        /* SYS Instructions (0xF4) */
        if (opcode == 0xF4) {
            switch (disp) {
                case 0x00: fprintf(of, "RTS"); break;
                case 0x01: fprintf(of, "DIT"); break;
                case 0x02: fprintf(of, "RD");  break;
                case 0x03: fprintf(of, "WD");  break;
                case 0x08: fprintf(of, "STM"); break;
                case 0x0C: fprintf(of, "CLM"); break;
                case 0x20: fprintf(of, "DITR"); break;
                case 0x40: fprintf(of, "RSV"); break;
                default:   fprintf(of, "SYS %03o", disp);
            }
            return SCPE_OK;
        }
        
        /* SRG family special mnemonics */
        if ((opcode == 0x31 || opcode == 0xE1 || opcode == 0xF1) && (mode == 6 || mode == 7)) {
            switch (disp & 0x1F) {
                case 0x02: mnemonic = "XAE"; break; case 0x04: mnemonic = "XAX"; break;
                case 0x06: mnemonic = "XEX"; break; case 0x08: mnemonic = "XAA"; break;
                case 0x0A: mnemonic = "CCE"; break; case 0x0E: mnemonic = "ACE"; break;
                case 0x10: mnemonic = "CCA"; break; case 0x12: mnemonic = "AEE"; break;
                case 0x14: mnemonic = "CNX"; break; case 0x16: mnemonic = "AIE"; break;
                case 0x18: mnemonic = "AAE"; break; case 0x1A: mnemonic = "LNE"; break;
                case 0x1C: mnemonic = "CNA"; break; case 0x1E: mnemonic = "CHX"; break;
            }
            fprintf(of, "%s", mnemonic);
            return SCPE_OK;
        }
        
        /* Class 2 relative branches */
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
    
    /* Default: print raw octal */
    fprintf(of, "%06o", inst);
    return SCPE_OK;
}

/* Symbolic input (Assembler) */
t_stat parse_sym(CONST char *cptr, t_addr addr, UNIT *uptr, t_value *val, int32 sw)
{
    char mnemonic[16];
    char arg[16];
    uint16 inst = 0;
    int i;
    
    while (isspace((unsigned char)*cptr)) cptr++;
    
    i = 0;
    while (*cptr && !isspace((unsigned char)*cptr) && *cptr != ',' && i < 15) {
        mnemonic[i++] = toupper((unsigned char)*cptr++);
    }
    mnemonic[i] = '\0';
    
    while (isspace((unsigned char)*cptr)) cptr++;
    
    i = 0;
    while (*cptr && !isspace((unsigned char)*cptr) && i < 15) {
        arg[i++] = *cptr++;
    }
    arg[i] = '\0';
    
    for (i = 0; i < 256; i++) {
        if (opcode_table[i] && strcmp(opcode_table[i], mnemonic) == 0) {
            uint16 opcode = i;
            uint16 mode = 0;
            uint16 disp = 0;
            
            /* Parse addressing mode prefix (using octal parsing) */
            if (arg[0] == '@') {
                if (arg[1] == '#') {
                    mode = 5;  /* IGX */
                    if (arg[2]) disp = (uint16)strtol(arg + 2, NULL, 8);
                } else if (arg[1] == ',') {
                    mode = 2;  /* ILX */
                    if (arg[2]) disp = (uint16)strtol(arg + 2, NULL, 8);
                } else {
                    mode = 1;  /* IL */
                    if (arg[1]) disp = (uint16)strtol(arg + 1, NULL, 8);
                }
            } else if (arg[0] == '#') {
                mode = 4;  /* DG */
                if (arg[1]) disp = (uint16)strtol(arg + 1, NULL, 8);
            } else if (arg[0] == '=') {
                mode = 6;  /* p */
                if (arg[1]) disp = (uint16)strtol(arg + 1, NULL, 8);
            } else if (arg[0] == '-') {
                mode = 7;  /* RM */
                if (arg[1]) disp = (uint16)strtol(arg + 1, NULL, 8);
            } else if (arg[0] == '+') {
                mode = 6;  /* RP */
                if (arg[1]) disp = (uint16)strtol(arg + 1, NULL, 8);
            } else if (isdigit((unsigned char)arg[0])) {
                mode = 0;  /* DL */
                disp = (uint16)strtol(arg, NULL, 8);
            }
            
            /* Special cases for SYS and CSV instructions */
            if (strcmp(mnemonic, "CSV") == 0) {
                if (strcmp(arg, "M:1O") == 0) disp = M_10_SECTION;
                else if (strcmp(arg, "M:ZIO") == 0) disp = M_ZIO_SECTION;
                else if (strcmp(arg, "M:WAIT") == 0) disp = M_WAIT_SECTION;
                else if (strcmp(arg, "M:ZWAT") == 0) disp = M_ZWAT_SECTION;
                else disp = (uint16)strtol(arg, NULL, 8);
                inst = (0x37 << 8) | (disp & 0xFF);
            } else if (strcmp(mnemonic, "RTS") == 0)  inst = 0xF400;
              else if (strcmp(mnemonic, "DIT") == 0)  inst = 0xF401;
              else if (strcmp(mnemonic, "RD") == 0)   inst = 0xF402;
              else if (strcmp(mnemonic, "WD") == 0)   inst = 0xF403;
              else if (strcmp(mnemonic, "STM") == 0)  inst = 0xF408;
              else if (strcmp(mnemonic, "CLM") == 0)  inst = 0xF40C;
              else if (strcmp(mnemonic, "DITR") == 0) inst = 0xF420;
              else if (strcmp(mnemonic, "RSV") == 0)  inst = 0xF440;
              else {
                /* Build standard instruction word */
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
            if (addr < MAXMEMSIZE) M[addr++] = data; /* Bounds checking */
            data = 0;
            nibble_count = 0;
        }
    }
    
    /* Set PC to start address (usually address 2 contains bootstrap) */
    if (MAXMEMSIZE > 2) P = M[2];
    
    return SCPE_OK;
}

/* ========== SIMH Device Integration Wrappers ========== */
/* These bridge the device functions from IO_*.c with SIMH's DEVICE framework */

/* DRI Disk */
static t_stat dri_sim_attach
(
    UNIT *uptr,
    int32 val,
    CONST char *cptr,
    void *desc
)
{
    int unit = (int)(uptr - dri_unit);
    return dri_attach(unit, cptr);
}

static t_stat dri_sim_detach
(
    UNIT *uptr,
    int32 val,
    CONST char *cptr,
    void *desc
)
{
    int unit = (int)(uptr - dri_unit);
    dri_detach(unit);
    return SCPE_OK;
}
static t_stat dri_sim_reset (DEVICE *dptr)
{
    dri_reset ();
    return SCPE_OK;
}

/* SAGEM Disk */
static t_stat sagem_sim_attach
(
    UNIT *uptr,
    int32 val,
    CONST char *cptr,
    void *desc
)
{
    int unit = (int)(uptr - sagem_unit);
    return sagem_attach(unit, cptr);
}

static t_stat sagem_sim_detach
(
    UNIT *uptr,
    int32 val,
    CONST char *cptr,
    void *desc
)
{
    int unit = (int)(uptr - sagem_unit);
    sagem_detach(unit);
    return SCPE_OK;
}

static t_stat sagem_sim_reset (DEVICE *dptr)
{
    sagem_reset ();
    return SCPE_OK;
}

/* Card Reader */
static t_stat cdr_sim_attach
(
    UNIT *uptr,
    int32 val,
    CONST char *cptr,
    void *desc
)
{
    return cdr_attach(cptr);
}

static t_stat cdr_sim_detach
(
    UNIT *uptr,
    int32 val,
    CONST char *cptr,
    void *desc
)
{
    cdr_detach();
    return SCPE_OK;
}

static t_stat cdr_sim_reset (DEVICE *dptr)
{
    cdr_reset ();
    return SCPE_OK;
}

/* Line printer */
static t_stat printer_sim_attach
(
    UNIT *uptr,
    int32 val,
    CONST char *cptr,
    void *desc
)
{
    return printer_attach(cptr);
}

static t_stat printer_sim_detach
(
    UNIT *uptr,
    int32 val,
    CONST char *cptr,
    void *desc
)
{
    printer_detach();
    return SCPE_OK;
}

static t_stat printer_sim_reset (DEVICE *dptr)
{
    printer_reset ();
    return SCPE_OK;
}

/* ASR33 Teletype */
static t_stat asr_sim_attach
(
    UNIT *uptr,
    int32 val,
    CONST char *cptr,
    void *desc
)
{
    return asr33_attach(cptr);
}

static t_stat asr_sim_detach
(
    UNIT *uptr,
    int32 val,
    CONST char *cptr,
    void *desc
)
{
    asr33_detach();
    return SCPE_OK;
}

static t_stat asr_sim_reset (DEVICE *dptr)
{
    asr33_reset ();
    return SCPE_OK;
}

/*  Front Panel, Paper Tape */
static t_stat panel_sim_reset (DEVICE *dptr)
{
    panel_reset ();
    return SCPE_OK;
}

static t_stat ptr_sim_reset (DEVICE *dptr)
{
    ptr_reset ();
    return SCPE_OK;
}

static t_stat ptp_sim_reset (DEVICE *dptr)
{
    ptp_reset ();
    return SCPE_OK;
}

/* System initialization */
t_stat mitra_sys_init(void)
{
    io_init_system();
    return SCPE_OK;
}

/* Memory clear */
void mitra_mem_clear(void)
{
    for (uint32 i = 0; i < MAXMEMSIZE; i++) /* Unified macro name */
        M[i] = 0;
}
