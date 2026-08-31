#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef unsigned char uint8;
typedef unsigned short int uint16;
typedef short int int16;
typedef unsigned int uint32;
typedef int int32;
typedef unsigned short int t_addr;
typedef unsigned short int t_value;
typedef void * UNIT;
typedef unsigned short int t_stat;

#define MAX_LINE 4096

#define MAX_MEM_WORDS   32768
/* Suspension levels (Section II-8.2) */
#define SUSP_STACK_DEPTH    4

#define t_bool   bool

typedef struct {
	    uint16 U_reg;     /* Universal register */
	    uint8 J_reg;     /* J register (bits 0-4 block selector: bits 5 to 7 register, MITRA 15S_15M/15 Manuel de microprogrammation) */
	    uint16 T_reg;     /* T register (micro-PC) */
	    uint8  B_ind;     /* B indicator */
	    uint8  Tz_ind;    /* Tz indicator */
	    uint8  To_ind;    /* To indicator */
	    uint8  Ao_ind;    /* Ao indicator */
	    uint16 saved_bloc; /* Saved register block */
	} SuspContext;

/* Memory - word addressable */
t_value M[MAX_MEM_WORDS]; // SIMH uses t_addr for addresses and t_value for values.

/* Single structure holds ALL CPU state - for REG table */
#define REG_BLOCS 8 // Number of register blocks
typedef struct {
/* 
 * CPU registers 
 * Each register has a unique address from 0 to 63 (or 127)
 * A high-speed interrupt causes an automatic switching of the register block. 
 * In the new block, the registers have then the same assignment as in block 0, but for other programs.
 * A complex semantic was tried (cpu_state.reg_block[cpu_state.curr_bloc].A) but it didn't compiled correctly by messing with with REG cpu_reg[] structure.
 * So now operation in the simulator occurs on a set of shim registers that are made pointing to the correct block.
 * V and W are used by micro-programs.
 */
 						
    uint16 reg_P, reg_L, reg_G, reg_A, reg_E, reg_X ;	// | => Alias of bloc 0 of 8 registers
    uint16 reg_V /* reg 6 */, reg_W;			// | reg_P is at index 0 and reg_W at index 7
    
 															// |
    uint16 reg_8; /* niveau de la tâche en cours */									// |
    uint16 reg_cnt_MAE,  reg_curr_MAE, reg_Work_MAE; /* mémoire de voie au télétype de service (ASR 33 ou MAE) */	// | => Alias of bloc 1 of 8 registers
    uint16 reg_12; /* adresse du bloc programme en cours d'utilisation */						// |
    uint16 reg_NC1, reg_NC2, reg_NC3;	/* not important for SIMH */							// |
    
    uint8 C, OV;
        
    uint16 reg_block[REG_BLOCS][8];
    
    uint8 J_reg;     /* J register (bits 0-4 block selector: bits 5 to 7 register, MITRA 15S_15M/15 Manuel de microprogrammation) */

    /* System registers */
    uint16 S ; // Memory address registers, bit 15 is always set to '0'
    uint16 M ; // Receives the transferred memory word
    uint16 U; //  not used by instruction set
    uint16 MREG;
   
    /*
    * Normal or "slave" mode, Priviledged or "master" mode
    * In normal mode (MS = 0), priviledged instructions cannot be executed and any attempt to execute such an instruction causes: A "mode violation" trap. 
    * MS indicator is reset.
    * In master mode (MS = 1) all instructions, whether priviledged or not, are executable. 
    * The various OSes are examples of programs which must be executed in master mode. (See CSV and RSV instructions).
    * It should be noted that addressing modes are different in master and slave modes (see Chapter V "Addressing modes") to provide absolute addressing capability.
    */
    uint8 MS; // Master/slave
    uint8 MA; // If interrupt mask is set to 1: all interrupt levels are masked
    uint8 PR; // Access to protected areas
    
    /* Interrupt/High speed Interrupt/Suspension/Trap state */
    uint32 intrpt_mask;  /* 32-bit bitmask of pending interrupts */
    int16 curr_int_lvl;     /* Current interrupt level not unit16 */
    t_bool high_speed;  /* TRUE if high-speed interrupt */
    int32 int_reqhi;         /* Highest pending interrupt level */
    
    /* Suspension request bits (32 levels, 8 per stack level) */
    uint32 susp_req_bits;
    uint16 susp_active_level;
    t_bool susp_pending;
    
    /* Trap state */
    uint16 trp_req_bits;      /* Trap request bits */
    t_bool trap_pending;
    uint16 trap_cause;
    uint16 trap_P;  /* Saved PC for trap */
    
    /* Front panel / CPU control state */
    uint8 cpu_mode; // The MITRA 15/20 can have an optional instruction set MC2, MITRA 15/30 may also have MC3 (Minibus/IOP)
    int cpu_running; /* 1 = running, 0 = stopped */
    int interrupts_enabled;
    int routing_enabled;
	uint16 panel_addr_lights;
	uint16 panel_data_lights;
    /* Suspension stack - 4 levels deep */
    SuspContext SuspensionStack[SUSP_STACK_DEPTH];
} CPU_STATE;

/* Global CPU state instance */
CPU_STATE cpu_state;

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

/* ========== Disassembly Functions ========== */

/* These are required by SCP for symbolic debugging */

/* parse_sym: Parse symbolic input (address or register name) */
t_stat parse_sym(const char *cptr, t_addr addr, UNIT *uptr, t_value *val, int32 sw) {
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
            return true;
        } else if (strcasecmp(temp, "E") == 0) {
            *val = cpu_state.reg_E;
            return true;
        } else if (strcasecmp(temp, "X") == 0) {
            *val = cpu_state.reg_X;
            return true;
        } else if (strcasecmp(temp, "P") == 0) {
            *val = cpu_state.reg_P;
            return true;
        } else if (strcasecmp(temp, "L") == 0) {
            *val = cpu_state.reg_L;
            return true;
        } else if (strcasecmp(temp, "G") == 0) {
            *val = cpu_state.reg_G;
            return true;
        } else if (strcasecmp(temp, "C") == 0) {
            *val = cpu_state.C;
            return true;
        } else if (strcasecmp(temp, "OV") == 0) {
            *val = cpu_state.OV;
            return true;
        } else if (strcasecmp(temp, "MS") == 0) {
            *val = cpu_state.MS;
            return true;
        } else if (strcasecmp(temp, "MA") == 0) {
            *val = cpu_state.MA;
            return true;
        } else if (strcasecmp(temp, "PR") == 0) {
            *val = cpu_state.PR;
            return true;
        }
        
        /* Unknown symbol */
        return false;
    }
    
    /* Parse as numeric address */
    {
//        t_stat r = true;
//        *val = (t_value)get_uint(p, 16, 0xFFFF, &r);
//        return r;
	return (uint16) p;
    }
}

/* fprint_sym: Print symbolic output (disassemble instruction) */
// t_stat fprint_sym(FILE *of, t_addr addr, t_value *val, UNIT *uptr, int32 sw) {
bool disassemble_inst(FILE* of, uint16_t val, t_addr addr) {
    uint16 inst;
    int mode, opcode;
    uint16 disp;
    const char *opname = NULL;
    const char *modename = NULL;
    
    /* If val is NULL, read from memory at addr */
    if (val == NULL) {
        if (addr >= MAX_MEM_WORDS) 
        	return false;
        inst = M[addr];
    } else {
        inst = val & 0xFFFF;
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
    
    return true;
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

/* Parse one hexdump line and disassemble instructions */
/* Parse one hexdump line (e.g., from `hd` or `hexdump -C`) */
int parse_hex_line(const char* line, FILE* out) {
    if (strlen(line) < 60) return 0;

    char addr_str[10];
    strncpy(addr_str, line, 8);
    addr_str[8] = '\0';
    uint32_t base_addr = (uint32_t)strtol(addr_str, NULL, 16);

    char hex_part[64];
    strncpy(hex_part, line + 9, 60);
    hex_part[60] = '\0';

    uint8_t bytes[16];
    int byte_count = 0;
    char* token = strtok(hex_part, " \t\r\n");
    
    while (token && byte_count < 16) {
        // Ensure token is exactly 2 hex digits to avoid parsing the ASCII column
        if (strlen(token) == 2 && isxdigit((unsigned char)token[0]) && isxdigit((unsigned char)token[1])) {
            bytes[byte_count++] = (uint8_t)strtol(token, NULL, 16);
        }
        token = strtok(NULL, " \t\r\n");
    }

    for (int i = 0; i + 1 < byte_count; i += 2) {
        uint16_t word = ((uint16_t)bytes[i] << 8) | bytes[i + 1];
        uint16_t addr = (uint16_t)(base_addr + i);
        fprintf(out, "%06X:\t%04X\t", addr, word);
        disassemble_inst(out, word, addr);
        fprintf(out, "\n");
    }

    return byte_count > 0 ? 1 : 0;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file.bin or file.hex>\n", argv[0]);
        return 1;
    }

    FILE* fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("Cannot open file");
        return 1;
    }

    char first_line[MAX_LINE];
    if (fgets(first_line, sizeof(first_line), fp)) {
        // Auto-detect: Does it look like a hexdump? (e.g., "00000000  00 00 ...")
        if (strlen(first_line) >= 60 && 
            isxdigit((unsigned char)first_line[0]) && 
            isxdigit((unsigned char)first_line[1]) &&
            isxdigit((unsigned char)first_line[7])) {
            
            // --- MODE 1: Text Hexdump ---
            rewind(fp);
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), fp)) {
                if (strlen(line) >= 60 && isxdigit((unsigned char)line[0])) {
                    parse_hex_line(line, stdout);
                }
            }
        } else {
            // --- MODE 2: Raw Binary File ---
            rewind(fp);
            uint8_t byte1, byte2;
            uint32_t addr = 0;
            
            // Read 16-bit words in Big-Endian format (standard for Mitra-15)
            while (fread(&byte1, 1, 1, fp) == 1 && fread(&byte2, 1, 1, fp) == 1) {
                uint16_t word = ((uint16_t)byte1 << 8) | byte2;
                printf("%06X:\t%04X\t", addr, word);
                disassemble_inst(stdout, word, (t_addr)addr);
                printf("\n");
                addr += 2;
            }
        }
    }

    fclose(fp);
    return 0;
    }
