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

/* ========================================================================
 * EBCDIC / code discrimination
 * ========================================================================
 * The Mitra-15 has no hardware- or assembler-enforced separation between
 * code and data: EBCDIC message tables sit directly in the instruction
 * stream. Because almost every EBCDIC letter/digit byte also happens to be
 * a syntactically valid Mitra-15 opcode byte, a purely local, per-word test
 * ("does this word look like a plausible instruction?" / "does this word
 * look like two plausible EBCDIC characters?") is ambiguous by itself and
 * cannot reliably tell the two apart word-by-word.
 *
 * The strategy implemented below is two-pass:
 *
 * It's not easy as EBCDIC valid characters are also valid Mitra-15 instructions.
As there is no separate areas by assembler design between code and data, I guess a possible mechanism would be to have two passes:
 * - the first one tries to guess what areas are EBCDIC strings,
 * - a second pass will assert if there are branch/jump/return or simply if PC for next instruction enters in one of the string areas detected in the first pass. If it is the case then the area consists of code, not string.
 * A special case is the entry point in the file, this two passes strategy might not work well here if we consider it might begin with a string, so this is a special case.
 * An open question is how large is the windows for detection of EBCDIC strings in the first pass, we can't assume a too small string because we will find many such strings, and we have the reverse problem with large strings.
 * In the second pass: Detecting if PC for next instruction enters in one of the string areas, works only if we are sure we come from a code area, otherwise as nearly every byte is a valid instruction, applying blindly this strategy would mean we detect nearly no strings.
 * Indeed we still don't know what is the code entry point in this dump, so there are lot of ambiguities to live with.
 *
 * Caveats (please read before trusting the output):
 *   - This is a heuristic, not a safe disassembler: Check the parameters and try several values.
 * ======================================================================== */

/* CP037 (EBCDIC) -> ASCII code point, 0 where there is no printable ASCII
 * equivalent (control codes, etc). */
static const unsigned char ebcdic_to_ascii[256] = {
      0,   1,   2,   3,   0,   9,   0, 127,   0,   0,   0,  11,  12,  13,  14,  15,
     16,  17,  18,  19,   0,   0,   8,   0,  24,  25,   0,   0,  28,  29,  30,  31,
      0,   0,   0,   0,   0,  10,  23,  27,   0,   0,   0,   0,   0,   5,   6,   7,
      0,   0,  22,   0,   0,   0,   0,   4,   0,   0,   0,   0,  20,  21,   0,  26,
     32,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  46,  60,  40,  43, 124,
     38,   0,   0,   0,   0,   0,   0,   0,   0,   0,  33,  36,  42,  41,  59,   0,
     45,  47,   0,   0,   0,   0,   0,   0,   0,   0,   0,  44,  37,  95,  62,  63,
      0,   0,   0,   0,   0,   0,   0,   0,   0,  96,  58,  35,  64,  39,  61,  34,
      0,  97,  98,  99, 100, 101, 102, 103, 104, 105,   0,   0,   0,   0,   0,   0,
      0, 106, 107, 108, 109, 110, 111, 112, 113, 114,   0,   0,   0,   0,   0,   0,
      0, 126, 115, 116, 117, 118, 119, 120, 121, 122,   0,   0,   0,   0,   0,   0,
     94,   0,   0,   0,   0,   0,   0,   0,   0,   0,  91,  93,   0,   0,   0,   0,
    123,  65,  66,  67,  68,  69,  70,  71,  72,  73,   0,   0,   0,   0,   0,   0,
    125,  74,  75,  76,  77,  78,  79,  80,  81,  82,   0,   0,   0,   0,   0,   0,
     92,   0,  83,  84,  85,  86,  87,  88,  89,  90,   0,   0,   0,   0,   0,   0,
     48,  49,  50,  51,  52,  53,  54,  55,  56,  57,   0,   0,   0,   0,   0,   0,
};

/* Set of punctuation EBCDIC bytes accepted as part of a "text". 
 * Deliberately narrow.
 * Extend if genuine strings use additional punctuation not listed here. */
static bool is_ebcdic_punct_ascii(unsigned char a) {
    switch (a) {
        case ' ': case '.': case ',': case ':': case ';': case '\'': case '"':
        case '!': case '?': case '-': case '/': case '*': case '+': case '=':
        case '(': case ')': case '@': case '&': case '_': case '%':
            return true;
        default:
            return false;
    }
}

/* A byte "looks like EBCDIC text" if it decodes to an uppercase/lowercase
 * letter, a digit, or one of the curated punctuation marks above. */
static bool is_ebcdic_text_byte(uint8_t b) {
    unsigned char a = ebcdic_to_ascii[b];
    if (a == 0) return false;
    if ((a >= 'A' && a <= 'Z') || (a >= 'a' && a <= 'z') || (a >= '0' && a <= '9'))
        return true;
    return is_ebcdic_punct_ascii(a);
}

/* Minimum run length (in words = character pairs) for Pass 1 to flag a
 * region as a text candidate. Tunable via -m/--min-text. Kept as a global
 * so both the finder and any diagnostics can see the active value. */
static int g_min_text_words = 2; /* 2 words = 4 EBCDIC characters, default */
static bool g_follow_branches = true; /* best-effort RP branch resolution */

/* 
* By default, ONLY a resolved branch/jump or an explicit entry point demotes a Pass-1 text candidate back to code.  * Start strict, and only relax this if you have evidence that a given string-looking area is genuinely misclassified. 
*/
static bool g_fallthrough_demotes = false;

/* When a text candidate is trusted enough to be reachable, only trust it within this many consecutive
 * ENTRY or resolved-BRANCH. The word immediately after an entry point (or a branch target) is very likely still code, but we should not trust a long, branch-free run of ordinary instructions with no
 * dead end because it might wander into the data). 
 * Tune with --fallthrough-trust-hops. 
 */
static int g_fallthrough_trust_hops = 1;

static uint16_t mem_word[MAX_MEM_WORDS];
static bool     mem_loaded[MAX_MEM_WORDS];   /* word was present in the input file */
static bool     mem_is_text[MAX_MEM_WORDS];  /* Pass 1 candidate, possibly demoted by Pass 2 */
static bool     mem_is_code[MAX_MEM_WORDS];  /* Pass 2: confirmed reachable as code */
static bool     mem_strong_code[MAX_MEM_WORDS]; /* reached via ENTRY or a resolved BRANCH, not just fallthrough */
static bool     mem_visited[MAX_MEM_WORDS];  /* Pass 2 worklist dedup */

#define MAX_ENTRY_POINTS 1024
static uint16_t g_entry_points[MAX_ENTRY_POINTS];
static int      g_n_entry_points = 0;

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

/* ========== Mitra-15 addressing model and system structures ==========
 *
 * ADDRESSES ARE BYTE ADDRESSES (16-bit words sit at even addresses). This is
 * what the manual uses everywhere (SRD of section n at G-4n, SPA stores
 * P+4, RTS resumes at saved P+2 ...) and it is also the first column of the
 * listing, so a label suffix IS the address column: L_0014 labels the line
 * 000014. Internally memory is indexed by word (index = address / 2).
 *
 * Instruction word: bits 15-13 mode, bit 12 "store family", bits 12-8
 * opcode, bits 7-0 D. D counts WORDS (so L reaches 256 words); it is scaled
 * by g_disp_scale = 2 into a byte offset (--disp-bytes uses D as bytes).
 *
 *   DL   Y = (L) + D                      local base
 *   DG   Y = (G) + D                      general base
 *   P    literal D (immediate)
 *   IL   Y = G' + mem[(L) + D]            indirect through a local pointer
 *   IGX  Y = G' + mem[(G) + D] + (X)      indirect, general-base pointer, indexed
 *   ILX  Y = G' + mem[(L) + D] + (X)      indirect, local pointer, indexed
 *   RP / RM   Y = P + D / P - D           relative branches
 *   0xD branches: indirect through (L)+D (IL) or (G)+D (IG)
 *   PX   Y = mem[(X)]                     system group
 * G' = G in normal (slave) mode, 0 in master mode (--abs-indirect).
 *
 * Program structure (manual, "CLS/RTS", "CSV/RSV"):
 *   CDS  common data section, addressed from G. Its first 16 words are the
 *        Task Working Block (TWB) reserved for monitor calls:
 *          word 0: caller P-G   word 1: caller L-G   word 2: indicators
 *          word 3: pointer to the system common area (ZC)
 *          words 4-7: N0..N3 (return addresses of up to 4 internal calls, SPA)
 *          words 8-15: T0..T7 monitor work area
 *        (words 4..15 are deduced from "3 saved words + ZC + N0-N3 + T0-T7 = 16").
 *   LDS  local data segment of a section, addressed from L. Its first two
 *        words receive the caller's P-G' and L-G' when the section is called
 *        by CLS; RTS restores them.
 *   LPS  executable segment (the section's code).
 *   PRT  Program Relocation Table, just below G: SRD n (2 words: L-G, P-G)
 *        at G-4n. CLS n loads L and P from it.
 *   PRTS supervisor's table: pointer at absolute byte address &C, number of
 *        modules at &E, SRD N (absolute L, P) at (&C)-4N. CSV N uses it;
 *        module 0 is reserved for traps.
 *   CPT  32 context pointers, pointer at absolute &A; DVT (32 words) just
 *        below it; each context (CTX) = 7 words: status X E A G L P.
 *   Calls:  CLS n / RTS            section call, return info in callee LDS
 *           CSV n / RSV            supervisor call, return info in caller TWB
 *           SPA y ; BRU target     internal subroutine call: y receives P+4-G'
 *                                  (return = branch indirect through y)
 *           DIT / DITR             end of an interrupt routine (scheduler)
 *
 * A static tool cannot know G, L and X: give G and L with -G / -L (byte
 * addresses). Without -G the PRT cannot be located.
 */

#include <stdarg.h>

#define BMASK 0xFFFF
#define IDX(a)   ((uint16_t)((((uint32_t)(a)) >> 1) & 0x7FFF))   /* byte address -> word index */
#define BADDR(i) ((uint16_t)(((uint32_t)(i) << 1) & 0xFFFF))      /* word index -> byte address */

static uint16_t g_G = 0;               /* assumed value of G (byte address) */
static uint16_t g_L = 0;               /* assumed value of L (byte address) */
static bool     g_G_given = false;
static bool     g_gprime_zero = false; /* master mode: G' = 0 */
static int      g_rel_base = 0;        /* P of a relative branch = this instruction + g_rel_base words (0: $+n) */
static int      g_disp_scale = 1;      /* bytes per unit of D in DL/DG/IL/IG... (word accesses are always even) */
static bool     g_ptr_follow = true;
static bool     g_structures = true;   /* detect PRT / PRTS / CPT / contexts */

static bool mem_call_slot[MAX_MEM_WORDS]; /* return-address cell stored by SPA (never a constant pointer) */
static bool mem_is_data[MAX_MEM_WORDS];   /* referenced as a variable, never executed */
static bool mem_struct[MAX_MEM_WORDS];    /* word of a recognised system structure */
static char g_name[MAX_MEM_WORDS][16];    /* fixed symbolic name (SEC2, CPT, MON3 ...) */
static const char *g_note[MAX_MEM_WORDS]; /* explanation printed beside a structure word */
static uint8_t g_role[MAX_MEM_WORDS];     /* routine classification of an entry word */
static bool    g_root[MAX_MEM_WORDS];     /* word is the entry of a section / routine */

/* SRD tables found by the structure discovery */
static bool     g_prt_valid[256];
static uint16_t g_prt_L[256], g_prt_P[256];   /* absolute byte addresses */
static bool     g_mon_valid[256];
static uint16_t g_mon_P[256];
static bool     g_modentry[MAX_MEM_WORDS];    /* SPA ; BRU ; RSV supervisor-module entry sequence */
static const char *g_prof[MAX_MEM_WORDS];     /* one-line profile of a routine entry */
static const char *g_csv_name[256];           /* CSV module names (annotation file) */
static const char *g_csv_desc[256];
static bool     g_ret_goff[512];              /* G-relative return cells written by SPA (symbolic G) */
static int      g_br_scale = 2;               /* branch D counts words */
typedef struct { uint16_t addr; char name[16]; char *text; int is_entry; } AnnName;
typedef struct { uint16_t start; char *text; bool done; } AnnRegion;
static char *g_annot_path = NULL;
static char *g_ann_header[32]; static int g_n_ann_header = 0;
typedef struct { uint16_t addr; int count; char name[16]; char *text; } AnnWords;
typedef struct { uint16_t start, end, lo, hi; } AnnVec;
static AnnWords g_ann_words[256]; static int g_n_ann_words = 0;
static AnnVec   g_ann_vecs[64];   static int g_n_ann_vecs = 0;
static AnnName   g_ann_names[1024]; static int g_n_ann_names = 0;
static AnnRegion g_ann_regions[128]; static int g_n_ann_regions = 0;


enum { ROLE_RTS = 1, ROLE_RSV = 2, ROLE_DIT = 4, ROLE_DITR = 8, ROLE_CSV = 16, ROLE_CLS = 32 };

enum {
    RF_BR      = 0x0001,  /* branch target */
    RF_CALL    = 0x0002,  /* callee: target of a branch that follows SPA, or of CLS/CSV */
    RF_RD      = 0x0004,
    RF_WR      = 0x0008,
    RF_IDX     = 0x0010,  /* accessed X-indexed (table base) */
    RF_PTR     = 0x0020,  /* pointer cell */
    RF_ADDR    = 0x0040,  /* address taken (LEA) */
    RF_SECNUM  = 0x0080,  /* holds a CLS/CSV section number */
    RF_ENTRY   = 0x0100,  /* entry point */
    RF_OPND    = 0x0200,  /* operand of a system instruction */
    RF_DBL     = 0x0400,
    RF_BYTE    = 0x0800,
    RF_RETCELL = 0x1000   /* return-address cell written by SPA */
};
static uint16_t g_ref_s[MAX_MEM_WORDS];   /* references from traced code */
static uint16_t g_ref_w[MAX_MEM_WORDS];   /* references from untraced words only */
static uint16_t g_refcnt[MAX_MEM_WORDS];

enum { ACC_R = 1, ACC_W = 2, ACC_ADDR = 4, ACC_DBL = 8, ACC_BYTE = 16, ACC_OTHER = 32, ACC_RETCELL = 64 };

static const uint8_t g1_acc[16] = {
    ACC_R, ACC_R, ACC_R, ACC_R, ACC_ADDR, ACC_R, ACC_R, ACC_R,   /* LDA LDE LDX EOR LEA ADD SUB IOR */
    ACC_R, ACC_R, ACC_R, ACC_R, ACC_R,                           /* DIV AND CPS CMP MUL */
    ACC_R | ACC_BYTE, ACC_R | ACC_BYTE, ACC_R | ACC_BYTE         /* LBL LBR LBX */
};
static const uint8_t g2_acc[16] = {
    ACC_R | ACC_DBL, ACC_W, ACC_W, ACC_W,                        /* DLD STA STE STX */
    ACC_W | ACC_BYTE, ACC_W | ACC_BYTE, ACC_W | ACC_DBL, ACC_R | ACC_W, /* SBL SBR DST ADM */
    ACC_W | ACC_RETCELL, ACC_W,                                  /* SPA STS */
    ACC_R | ACC_DBL, ACC_R | ACC_DBL, ACC_R | ACC_DBL, ACC_R | ACC_DBL, /* FAD FSU FMU FDV */
    ACC_OTHER, ACC_OTHER                                         /* TRS MVS */
};

/* Comment templates; %s is the operand value expression, e.g. [V_0012]. */
static const char *g1_tmpl[16] = {
    "A <- %s", "E <- %s", "X <- %s", "A <- A xor %s",
    "A <- %s  (effective address, memory not read)",
    "A <- A + %s", "A <- A - %s", "A <- A or %s",
    "divide by %s", "A <- A and %s", NULL, "compare A with %s", "multiply by %s",
    "A <- left byte of %s", "A <- right byte of %s", NULL
};
static const char *g2_tmpl[16] = {
    "A,E <- %s (2 words)", "%s <- A", "%s <- E", "%s <- X",
    "left byte of %s <- A", "right byte of %s <- A", "%s <- A,E (2 words)", "add A to %s",
    "%s <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)", NULL,
    "floating add %s", "floating subtract %s", "floating multiply by %s", "floating divide by %s",
    NULL, NULL
};

typedef enum { AM_NONE, AM_DL, AM_IMM, AM_DG, AM_IL, AM_IGX, AM_ILX,
               AM_RP, AM_RM, AM_BIL, AM_BIG, AM_PX } AmKind;
typedef enum { CL_NOOPND, CL_MEM, CL_SYSOP, CL_BRANCH } InsnClass;

typedef struct {
    uint16_t    inst, hexcode;
    const char *opname;    /* NULL: not a valid instruction */
    const char *modename;
    const char *tmpl;
    InsnClass   cls;
    AmKind      am;
    uint8_t     acc;
    uint8_t     disp;
    uint16_t    print_disp;
    bool        is_uncond; /* BRU */
    bool        is_brx;    /* BRX: indexed jump into the table of BRU that follows */
    bool        is_cls;    /* CLS: call section */
    bool        is_csv;    /* CSV: call supervisor */
    bool        is_ret;    /* RTS RSV DIT DITR: no fall-through */
} Insn;

static AmKind am_for_hexcode(uint16_t hc) {
    switch (hc) {
        case 0x0000: case 0x1000: case 0x3000: return AM_DL;
        case 0x2000: case 0xF000:              return AM_IMM;
        case 0x4000: case 0x5000:              return AM_DG;
        case 0x6000: case 0x7000:              return AM_IL;
        case 0x8000: case 0x9000:              return AM_IGX;
        case 0xA000: case 0xB000:              return AM_ILX;
        case 0xE000:                           return AM_PX;
        default:                               return AM_NONE;
    }
}

static bool is_spa(const Insn *d) { return d->cls == CL_MEM && d->opname && !strcmp(d->opname, "SPA"); }

static bool decode_insn(uint16_t inst, Insn *d) {
    memset(d, 0, sizeof *d);
    d->inst     = inst;
    d->hexcode  = inst & 0xF000;
    d->disp     = (uint8_t)(inst & 0xFF);
    d->print_disp = d->disp;
    d->cls      = CL_NOOPND;
    d->am       = AM_NONE;
    int mode    = (inst >> 13) & 0x07;
    int opcode  = (inst >> 8) & 0x1F;
    uint16_t disp = d->disp;
    d->modename = mode_names[mode];

    switch (d->hexcode) {
        case 0x0000: case 0x2000: case 0x4000: case 0x6000: case 0x8000: case 0xA000:
            if (opcode < 16) {
                d->opname = group1_opnames[opcode];
                d->cls = CL_MEM;
                d->am  = am_for_hexcode(d->hexcode);
                d->acc = g1_acc[opcode];
                d->tmpl = g1_tmpl[opcode];
            }
            break;

        case 0x1000: case 0x5000: case 0x7000: case 0x9000: case 0xB000:
            d->opname = group2_opnames[opcode & 0x0F];
            d->cls = CL_MEM;
            d->am  = am_for_hexcode(d->hexcode);
            d->acc = g2_acc[opcode & 0x0F];
            d->tmpl = g2_tmpl[opcode & 0x0F];
            break;

        case 0x3000: case 0xE000: case 0xF000: {
            /* System group. Forms: DL (0x3xxx), PX (0xExxx), P (0xFxxx). */
            static const char *sys_names[] = {
                "SHR", "SRG", "ICX", "DCX", "CTL", "ICL", "DCL", "CSV",
                "CLS", "LDR", "STR", "LDP", "SHC", "TES", NULL, NULL
            };
            int sub = opcode & 0x0F;
            d->opname   = sys_names[sub];
            d->modename = (d->hexcode == 0x3000) ? "DL" : (d->hexcode == 0xE000) ? "PX" : "P";
            d->am       = am_for_hexcode(d->hexcode);
            if (!d->opname) break;

            if (sub == 0x0) {
                static const char *shr_names[] = {
                    "SLLS", "SRCS", "SAD", "SLCD", "SLCS", "SAS", "SRLS", "SRCD"
                };
                d->opname = shr_names[(disp >> 5) & 0x07];
                d->print_disp = disp & 0x1F;
            } else if (sub == 0xC) {
                switch ((disp >> 5) & 0x07) {
                    case 0: d->opname = "SLLD"; d->print_disp = disp & 0x1F; break;
                    case 2: d->opname = "PTY";  d->print_disp = disp & 0x1F; break;
                    case 4: d->opname = "SRLD"; d->print_disp = disp & 0x1F; break;
                    case 6: d->opname = "NLZ";  d->print_disp = disp & 0x1F; break;
                    case 1: d->opname = "DITR"; d->print_disp = 0; d->is_ret = true; break;
                    default: d->opname = NULL; break;
                }
            } else if (sub == 0x1) {
                static const char *srg_names[] = {
                    "RTS", "XAE", "XAX", "XEX", "XAA", "CCE", "RSV", "ACE",
                    "CCA", "AEE", "CNX", "AIE", "AAE", "LNE", "CNA", "CHX"
                };
                int srg_sub = disp >> 1;
                d->opname = (srg_sub < 16) ? srg_names[srg_sub] : NULL;
                d->print_disp = 0;
                d->is_ret = (srg_sub == 0 || srg_sub == 6);   /* RTS, RSV */
            } else if (sub == 0x4) {
                /* CLM/DIT/RD/WD/STM exist only as the P form F4xx, selected by the low byte */
                d->print_disp = 0;
                if (d->hexcode != 0xF000) { d->opname = NULL; break; }
                switch (disp) {
                    case 0x00: d->opname = "CLM"; break;
                    case 0x01: d->opname = "DIT"; d->is_ret = true; break;
                    case 0x02: d->opname = "RD";  break;
                    case 0x03: d->opname = "WD";  break;
                    case 0x08: d->opname = "STM"; break;
                    default:   d->opname = NULL;  break;
                }
            } else {
                d->cls = CL_SYSOP;
                d->is_csv = (sub == 0x7);
                d->is_cls = (sub == 0x8);
                d->acc = (sub == 0x9 || sub == 0xB) ? ACC_R :
                         (sub == 0xA) ? ACC_W : ACC_OTHER;
            }
            break;
        }

        case 0xC000: case 0xD000: {
            static const char *br_names[8] = {
                "BCT", "BRX", "BOT", "BCF", "BAN", "BAZ", "BOF", "BRU"
            };
            int op3 = opcode & 0x07;
            int bit11 = (inst >> 11) & 1;
            d->opname = br_names[op3];
            d->cls = CL_BRANCH;
            if (d->hexcode == 0xC000) {
                d->am = bit11 ? AM_RM : AM_RP;
                d->modename = bit11 ? "RM" : "RP";
            } else {
                d->am = bit11 ? AM_BIG : AM_BIL;
                d->modename = bit11 ? "IG" : "IL";
            }
            d->is_uncond = (op3 == 7);
            d->is_brx    = (op3 == 1);
            break;
        }
    }
    return d->opname != NULL;
}

/* True if the word before widx is a loaded SPA: the branch at widx is then
 * the call of a subroutine and widx+1 is where it returns (SPA stores P+4). */
static bool prev_is_spa(uint32_t widx) {
    Insn p;
    if (widx == 0 || !mem_loaded[widx - 1] || mem_is_text[widx - 1] || mem_struct[widx - 1]) return false;
    return decode_insn(mem_word[widx - 1], &p) && is_spa(&p);
}

/* BRX at word w: X selects one of the unconditional branches that follow it
 * (jump table). Returns how many consecutive BRU words follow. */
static int brx_table(uint32_t w, uint16_t *out, int max) {
    int n = 0;
    for (uint32_t j = w + 1; j < MAX_MEM_WORDS && n < max; j++) {
        Insn t;
        if (!mem_loaded[j] || mem_is_text[j] || mem_struct[j]) break;
        if (!decode_insn(mem_word[j], &t) || t.cls != CL_BRANCH || !t.is_uncond) break;
        if (out) out[n] = (uint16_t)j;
        n++;
    }
    return n;
}

/* ---- effective address ---- */

typedef enum { OK_NONE, OK_IMM, OK_MEM } OpKind;

typedef struct {
    OpKind   kind;
    bool     ea_known;
    uint16_t ea;          /* byte address (without X) */
    bool     indexed, indirect, via_x;
    uint16_t ptr;         /* byte address of the pointer cell */
    bool     ptr_static;
    uint16_t ptr_val;
    bool     ptr_is_slot; /* pointer cell is an SPA return cell */
    int      secno;       /* CLS / CSV immediate section number, -1 if none */
    bool     callee_known;/* section / monitor entry resolved through PRT / PRTS */
    uint16_t callee;      /* byte address of the entry (P) */
    uint16_t callee_l;    /* byte address of its LDS (CLS) */
    bool     gsym;        /* DG with G unknown: operand is G + goff */
    uint16_t goff;
    bool     ptr_gsym;    /* pointer cell is G + ptr_goff (IG / IGX with G unknown) */
    uint16_t ptr_goff;
    bool     tgt_gsym;    /* IL/ILX with G unknown and G' = G: target = G + ptr_val */
} Operand;

static void compute_operand(uint32_t widx, const Insn *d, Operand *o) {
    memset(o, 0, sizeof *o);
    o->secno = -1;
    if (d->cls == CL_NOOPND) return;
    uint32_t D  = (uint32_t)d->disp * (uint32_t)g_disp_scale;   /* data operands: D in bytes */
    uint32_t Db = (uint32_t)d->disp * (uint32_t)g_br_scale;     /* branches: D in words */
    uint32_t gp = g_gprime_zero ? 0 : g_G;
    bool gsym = !g_G_given;      /* G unknown: G-relative operands stay symbolic (TWB_xx / G+xxxx) */
    uint32_t here = (uint32_t)widx * 2;
    o->kind = OK_MEM;

    switch (d->am) {
        case AM_IMM:
            o->kind = OK_IMM;
            if (d->is_cls || d->is_csv) {
                int n = d->disp;
                o->secno = n;
                if (d->is_cls && g_prt_valid[n])      { o->callee_known = true; o->callee = g_prt_P[n]; o->callee_l = g_prt_L[n]; }
                else if (d->is_csv && g_mon_valid[n]) { o->callee_known = true; o->callee = g_mon_P[n]; }
            }
            return;
        case AM_DL: o->ea_known = true; o->ea = (uint16_t)((g_L + D) & BMASK); return;
        case AM_DG:
            if (gsym) { o->gsym = true; o->goff = (uint16_t)D; return; }
            o->ea_known = true; o->ea = (uint16_t)((g_G + D) & BMASK); return;
        case AM_RP: o->ea_known = true; o->ea = (uint16_t)((here + g_rel_base * 2 + Db) & BMASK); return;
        case AM_RM: o->ea_known = true; o->ea = (uint16_t)((here + g_rel_base * 2 - Db) & BMASK); return;
        case AM_PX: o->via_x = true; return;
        case AM_IL:  case AM_BIL: o->indirect = true; o->ptr = (uint16_t)((g_L + D) & BMASK); break;
        case AM_ILX:              o->indirect = true; o->ptr = (uint16_t)((g_L + D) & BMASK); o->indexed = true; break;
        case AM_IGX:              o->indirect = true; o->indexed = true;
                                  if (gsym) { o->ptr_gsym = true; o->ptr_goff = (uint16_t)D; o->ptr_is_slot = g_ret_goff[D & 0x1FF]; return; }
                                  o->ptr = (uint16_t)((g_G + D) & BMASK); break;
        case AM_BIG:              o->indirect = true;
                                  if (gsym) { o->ptr_gsym = true; o->ptr_goff = (uint16_t)D; o->ptr_is_slot = g_ret_goff[D & 0x1FF]; return; }
                                  o->ptr = (uint16_t)((g_G + D) & BMASK); break;
        default: o->kind = OK_NONE; return;
    }

    uint16_t pi = IDX(o->ptr);
    o->ptr_is_slot = mem_call_slot[pi];
    if (g_ptr_follow && mem_loaded[pi] && !mem_is_text[pi] && !mem_call_slot[pi]) {
        o->ptr_static = true;
        o->ptr_val = mem_word[pi];
        if (!g_G_given && !g_gprime_zero) { o->tgt_gsym = true; return; }   /* address = G + value, G unknown */
        uint16_t t = (uint16_t)((gp + o->ptr_val) & BMASK);
        if (o->ptr_val != 0 && !(t & 1) && mem_loaded[IDX(t)]) { o->ea_known = true; o->ea = t; }
    }
}

/* ---- labels ---- */

static bool has_label(uint32_t i) { return (g_ref_s[i] | g_ref_w[i]) != 0 || g_name[i][0] != 0; }

/* Symbolic names of recognised structures win (SEC2, MON3, CPT, TWB_N0 ...).
 * Otherwise: E entry, S subroutine/callee, L branch target, T text, V data;
 * lower case = referenced only from code the flow trace never reached. */
static void label_name(uint16_t i, char *buf, size_t n) {
    if (g_name[i][0]) { snprintf(buf, n, "%s", g_name[i]); return; }
    uint16_t f = g_ref_s[i] | g_ref_w[i];
    char p;
    if      (f & RF_ENTRY) p = 'E';
    else if (f & RF_CALL)  p = 'S';
    else if (f & RF_BR)    p = 'L';
    else if (mem_loaded[i] && mem_is_text[i]) p = 'T';
    else                   p = 'V';
    if (!g_ref_s[i]) p = (char)tolower((unsigned char)p);
    snprintf(buf, n, "%c_%04X", p, BADDR(i));
}

/* Label for a BYTE address. */
static const char *lab(uint16_t a, char *buf, size_t n) {
    uint16_t i = IDX(a);
    if (has_label(i)) label_name(i, buf, n);
    else snprintf(buf, n, "$%04X", a);
    return buf;
}

static void cat(char *buf, size_t n, const char *fmt, ...) {
    size_t l = strlen(buf);
    if (l >= n - 1) return;
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf + l, n - l, fmt, ap);
    va_end(ap);
}

/* Name of a G-relative byte offset: the first 16 words of the CDS are the
 * Task Working Block. Layout (evidence: module entries use the 4 words at
 * G+18..G+1E as the SPA return cells, N0 = G+1E being the most used one):
 *   P, L, indicators, ZC pointer (G+6 = common-area address), T0..T7, N3..N0 */
static const char *gname(uint16_t off, char *buf, size_t n) {
    static const char *w[16] = { "TWB_P", "TWB_L", "TWB_IND", "TWB_ZC", "TWB_T0", "TWB_T1", "TWB_T2", "TWB_T3",
                                 "TWB_T4", "TWB_T5", "TWB_T6", "TWB_T7", "TWB_N3", "TWB_N2", "TWB_N1", "TWB_N0" };
    if (off < 0x20) snprintf(buf, n, "%s%s", w[off >> 1], (off & 1) ? "+1" : "");
    else            snprintf(buf, n, "G+%04X", off);
    return buf;
}

static const char *ptrname(const Operand *o, char *buf, size_t n) {
    if (o->ptr_gsym) return gname(o->ptr_goff, buf, n);
    return lab(o->ptr, buf, n);
}

static void csv_text(int num, char *buf, size_t sz) {
    buf[0] = 0;
    if (num < 0 || num > 255 || !g_csv_name[num]) return;
    snprintf(buf, sz, "%s%s%s", g_csv_name[num], (g_csv_desc[num] && g_csv_desc[num][0]) ? ": " : "",
             g_csv_desc[num] ? g_csv_desc[num] : "");
}

/* "-> routine S_153C: profile" for a target that is the entry of a routine */
static void prof_tag(uint16_t a, char *buf, size_t n) {
    char t[32];
    uint16_t i = IDX(a);
    if (g_prof[i]) cat(buf, n, "  [-> %s: %s]", lab(a, t, sizeof t), g_prof[i]);
}

static void operand_text(const Insn *d, const Operand *o, char *buf, size_t n) {
    char t[32];
    buf[0] = 0;
    if (d->cls == CL_NOOPND) {
        if (d->print_disp) snprintf(buf, n, "#%02X", d->print_disp);
        return;
    }
    if (d->is_brx)         { snprintf(buf, n, "$+X"); return; }
    if (o->kind == OK_IMM) { snprintf(buf, n, "=#%02X", d->disp); return; }
    if (o->via_x)          { snprintf(buf, n, "(X)"); return; }
    if (o->gsym)           { snprintf(buf, n, "%s", gname(o->goff, t, sizeof t)); return; }
    if (o->indirect) {
        snprintf(buf, n, "*%s%s", ptrname(o, t, sizeof t), o->indexed ? ",X" : "");
        return;
    }
    snprintf(buf, n, "%s", lab(o->ea, t, sizeof t));
}

static void value_expr(const Insn *d, const Operand *o, char *buf, size_t n) {
    char t[32], core[96];
    if (o->kind == OK_IMM) { snprintf(buf, n, "#%02X", d->disp); return; }
    if (o->via_x)          snprintf(core, sizeof core, "(X)");
    else if (o->gsym)      snprintf(core, sizeof core, "%s", gname(o->goff, t, sizeof t));
    else if (o->ea_known)  snprintf(core, sizeof core, "%s%s", lab(o->ea, t, sizeof t), o->indexed ? "+X" : "");
    else if (o->tgt_gsym)  snprintf(core, sizeof core, "G+%04X%s", o->ptr_val, o->indexed ? "+X" : "");
    else if (o->indirect)  snprintf(core, sizeof core, "*%s%s", ptrname(o, t, sizeof t), o->indexed ? "+X" : "");
    else                   core[0] = 0;
    if (d->acc & ACC_ADDR) snprintf(buf, n, "%s", core);
    else                   snprintf(buf, n, "[%s]", core);
}

static void indirect_note(const Operand *o, char *buf, size_t n) {
    char t[32];
    if (!o->indirect || o->ptr_is_slot) return;
    if (o->ptr_gsym)
        cat(buf, n, "  (pointer %s lives in the calling task's area: its contents are only known at run time)",
            ptrname(o, t, sizeof t));
    else if (o->tgt_gsym)
        cat(buf, n, "  (pointer %s = #%04X: address = G + #%04X, G unknown)", lab(o->ptr, t, sizeof t), o->ptr_val, o->ptr_val);
    else if (o->ptr_static && o->ea_known)
        cat(buf, n, "  (pointer %s = #%04X%s)", lab(o->ptr, t, sizeof t), o->ptr_val,
            o->indexed ? ", base only, X added at run time" : "");
    else if (o->ptr_static)
        cat(buf, n, "  (pointer %s = #%04X: zero or outside image, target not resolved)",
            lab(o->ptr, t, sizeof t), o->ptr_val);
    else
        cat(buf, n, "  (pointer %s not in image / not followed, target unknown at disassembly time)",
            lab(o->ptr, t, sizeof t));
}

static void build_comment(uint32_t widx, const Insn *d, const Operand *o, char *buf, size_t n) {
    char t[32], t2[32], ex[128], cs[200];
    buf[0] = 0;

    if (d->cls == CL_BRANCH && d->is_brx) {
        int nt = brx_table(widx, NULL, 64);
        cat(buf, n, "INDEXED JUMP (BRX): X selects one of the %d BRU entries that follow (jump / dispatch table)", nt);
        return;
    }
    if (d->cls == CL_BRANCH) {
        if (d->am == AM_RP) cat(buf, n, "%u word%s forward: ", d->disp, d->disp == 1 ? "" : "s");
        if (d->am == AM_RM) cat(buf, n, "%u word%s backward: ", d->disp, d->disp == 1 ? "" : "s");
        if (o->indirect && o->ptr_is_slot) {
            cat(buf, n, "RETURN from subroutine: branch through return cell %s (written by the SPA of the call)",
                ptrname(o, t, sizeof t));
            return;
        }
        if (prev_is_spa(widx)) {
            if (o->ea_known) {
                cat(buf, n, "CALL subroutine %s (SPA above saved the return address); returns to %s",
                    lab(o->ea, t, sizeof t), lab(BADDR(widx + 1), t2, sizeof t2));
                prof_tag(o->ea, buf, n);
            } else
                cat(buf, n, "CALL subroutine, target not known statically (SPA above saved the return address)");
        } else if (d->is_uncond) {
            if (o->ea_known) { cat(buf, n, "jump to %s", lab(o->ea, t, sizeof t)); prof_tag(o->ea, buf, n); }
            else if (o->tgt_gsym) cat(buf, n, "jump to G+#%04X (G unknown)", o->ptr_val);
            else             cat(buf, n, "jump, target not known statically");
        } else {
            if (o->ea_known) { cat(buf, n, "conditional branch (%s) to %s", d->opname, lab(o->ea, t, sizeof t)); prof_tag(o->ea, buf, n); }
            else             cat(buf, n, "conditional branch, target not known statically");
        }
        indirect_note(o, buf, n);
        return;
    }

    if (d->is_cls) {
        if (o->kind == OK_IMM) {
            cat(buf, n, "CALL SECTION #%d (CLS; caller P,L saved in the callee LDS; return = RTS)", o->secno);
            if (o->callee_known) {
                cat(buf, n, "  -> entry %s, LDS %s", lab(o->callee, t, sizeof t), lab(o->callee_l, t2, sizeof t2));
                prof_tag(o->callee, buf, n);
            } else
                cat(buf, n, "  (SRD not resolved: give -G, or PRT not in image)");
        } else {
            value_expr(d, o, ex, sizeof ex);
            cat(buf, n, "CALL SECTION, number taken from %s (CLS); return = RTS", ex);
        }
        indirect_note(o, buf, n);
        return;
    }
    if (d->is_csv) {
        if (o->kind == OK_IMM) {
            csv_text(o->secno, cs, sizeof cs);
            cat(buf, n, "SUPERVISOR CALL #%d%s", o->secno, o->secno == 0 ? " (module 0 = trap handler)" : "");
            if (cs[0]) cat(buf, n, " = %s", cs);
            cat(buf, n, " (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)");
            if (o->callee_known) { cat(buf, n, "  -> entry %s", lab(o->callee, t, sizeof t)); prof_tag(o->callee, buf, n); }
        } else {
            value_expr(d, o, ex, sizeof ex);
            cat(buf, n, "SUPERVISOR CALL, module number taken from %s (CSV); module ends with RSV", ex);
        }
        indirect_note(o, buf, n);
        return;
    }

    if (d->cls == CL_MEM || d->cls == CL_SYSOP) {
        value_expr(d, o, ex, sizeof ex);
        if (d->tmpl) cat(buf, n, d->tmpl, ex);
        else         cat(buf, n, "%s operand: %s", d->opname, ex);
        indirect_note(o, buf, n);
        return;
    }

    if (!strcmp(d->opname, "RTS")) cat(buf, n, "RETURN SECTION: restore L and P from the first 2 words of the current LDS, resume after the CLS");
    else if (!strcmp(d->opname, "RSV")) cat(buf, n, "RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV");
    else if (!strcmp(d->opname, "DIT")) cat(buf, n, "DE-ACTIVATE INTERRUPT: save outgoing context, dispatch the highest-priority waiting level (not a plain return)");
    else if (!strcmp(d->opname, "DITR")) cat(buf, n, "DE-ACTIVATE high-speed interrupt: return to interrupted task without block-0 context swap");
    else if (!strcmp(d->opname, "CLM") || !strcmp(d->opname, "RD") || !strcmp(d->opname, "WD") || !strcmp(d->opname, "STM"))
        cat(buf, n, "privileged control instruction (%s)", d->opname);
    else if (!strcmp(d->opname, "XAE")) cat(buf, n, "exchange A and E");
    else if (!strcmp(d->opname, "XAX")) cat(buf, n, "exchange A and X");
    else if (!strcmp(d->opname, "XEX")) cat(buf, n, "exchange E and X");
}

static void print_insn(FILE *of, uint32_t w) {
    uint16_t val = mem_word[w];
    Insn d;
    if (!decode_insn(val, &d)) {
        fprintf(of, "%06X:\t%04X\t??\t%04X", (unsigned)(w * 2), val, val);
        return;
    }
    Operand o;
    compute_operand(w, &d, &o);
    char opnd[64], cmt[900];
    operand_text(&d, &o, opnd, sizeof opnd);
    build_comment(w, &d, &o, cmt, sizeof cmt);
    if (g_note[w] && !mem_struct[w]) cat(cmt, sizeof cmt, "%s| %s", cmt[0] ? "  " : "", g_note[w]);
    fprintf(of, "%06X:\t%04X\t%-4s\t%-3s\t%s", (unsigned)(w * 2), val, d.opname, d.modename, opnd);
    if (cmt[0]) fprintf(of, "\t; %s", cmt);
}

/* ========== Pass 1: text-candidate detection ========== */

/* Scan and flag maximal word-aligned runs of >= g_min_text_words
 * words, where both bytes of every word in the run look like EBCDIC text,
 * as text candidates. 
 * Pass 2 is what asserts or overturns these candidates data areas using control flow. */
static void find_text_candidates(uint16_t lo, uint32_t hi) {
    uint32_t i = lo;
    while (i < hi) {
        if (!mem_loaded[i]) { i++; continue; }

        uint32_t start = i;
        while (i < hi && mem_loaded[i]) {
            uint16_t w = mem_word[i];
            uint8_t hib = (uint8_t)(w >> 8);
            uint8_t lob = (uint8_t)(w & 0xFF);
            if (is_ebcdic_text_byte(hib) && is_ebcdic_text_byte(lob)) {
                i++;
            } else {
                break;
            }
        }

        uint32_t run_len = i - start;
        if (run_len >= (uint32_t)g_min_text_words) {
            for (uint32_t k = start; k < i; k++) mem_is_text[k] = true;
        }
        if (i == start) i++; /* current word failed the test; move past it */
    }
}

/* ========== Pass 2a: system-structure discovery ==========
 *
 * The manual fixes several memory structures. When the image contains them
 * they are recognised, named, and their code entry points become roots of
 * the flow trace (so sections, supervisor modules and interrupt routines are
 * reached even though nothing in the code branches to them directly):
 *
 *   PRT   (needs -G)  SRD n at G-4n          -> SEC<n> (code), LDS<n> (data)
 *   PRTS  pointer at absolute &C, count at &E -> MON<N> supervisor modules
 *   CPT   pointer at absolute &A              -> contexts CTX<k>, routines INT<k>
 *   DVT   32 words just below the CPT
 * System structures need the image to start at absolute address 0 (a monitor
 * dump); a user program image normally only contains its PRT (give -G).
 */

static const char *g_summary[96];
static int g_nsummary = 0;

static void summary(const char *fmt, ...) {
    if (g_nsummary >= 96) return;
    char b[300];
    va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    g_summary[g_nsummary++] = strdup(b);
}

/* Mark a loaded word as part of a structure: not text, never decoded as code. */
static void struct_word(uint32_t i, const char *name, const char *fmt, ...) {
    if (i >= MAX_MEM_WORDS || !mem_loaded[i]) return;
    mem_struct[i] = true;
    mem_is_text[i] = false;
    if (name && !g_name[i][0]) snprintf(g_name[i], sizeof g_name[i], "%s", name);
    if (fmt && !g_note[i]) {
        char b[300];
        va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
        g_note[i] = strdup(b);
    }
}

static bool addr_ok(uint32_t a) { return !(a & 1) && a <= 0xFFFE && mem_loaded[IDX(a)]; }

static void add_entry(uint16_t a, const char *name) {
    if (!addr_ok(a)) return;
    uint16_t i = IDX(a);
    if (name && !g_name[i][0]) snprintf(g_name[i], sizeof g_name[i], "%s", name);
    g_root[i] = true;
    for (int e = 0; e < g_n_entry_points; e++) if (g_entry_points[e] == i) return;
    if (g_n_entry_points < MAX_ENTRY_POINTS) g_entry_points[g_n_entry_points++] = i;
}

static void discover_prt(void) {
    if (!g_G_given) return;
    int found = 0;
    for (int n = 1; n <= 255; n++) {
        int32_t a = (int32_t)g_G - 4 * n;
        if (a < 0) break;
        uint16_t i = IDX(a);
        if (!mem_loaded[i] || !mem_loaded[i + 1]) break;
        uint16_t loff = mem_word[i], poff = mem_word[i + 1];
        uint16_t labs = (uint16_t)((g_G + loff) & BMASK), pabs = (uint16_t)((g_G + poff) & BMASK);
        bool lok = (loff == 0) || addr_ok(labs);
        if (poff == 0 || !addr_ok(pabs) || !lok) break;       /* end of the table */
        char nm[16];
        snprintf(nm, sizeof nm, "PRT%d", n);
        struct_word(i, nm, "SRD %d, word 1: L-G = #%04X (LDS%d at #%04X)", n, loff, n, labs);
        struct_word(i + 1u, NULL, "SRD %d, word 2: P-G = #%04X (entry SEC%d at #%04X)", n, poff, n, pabs);
        g_prt_valid[n] = true; g_prt_L[n] = labs; g_prt_P[n] = pabs;
        snprintf(nm, sizeof nm, "SEC%d", n);
        add_entry(pabs, nm);
        if (loff != 0) {
            snprintf(nm, sizeof nm, "LDS%d", n);
            uint16_t li = IDX(labs);
            if (!g_name[li][0]) snprintf(g_name[li], sizeof g_name[li], "%s", nm);
            struct_word(li, NULL, "LDS%d word 0: caller's P-G' saved here by CLS (RTS resumes at this +2)", n);
            struct_word(li + 1u, NULL, "LDS%d word 1: caller's L-G' saved here by CLS", n);
        }
        found++;
    }
    if (found)
        summary("PRT: %d section SRD(s) at G-4n (G=#%04X): SEC1..SEC%d are LPS entries, LDS1..LDS%d their local data; "
                "called with CLS n, left with RTS", found, g_G, found, found);
}

static void discover_system(void) {
    if (!mem_loaded[5] || !mem_loaded[6] || !mem_loaded[7]) return;
    uint16_t cpt = mem_word[5], prts = mem_word[6], cnt = mem_word[7];
    char nm[16];

    /* ---- PRTS: supervisor modules ---- */
    if (addr_ok(prts) && cnt >= 1 && cnt <= 255) {
        int found = 0;
        for (int n = 0; n < cnt; n++) {
            int32_t a = (int32_t)prts - 4 * n;
            if (a < 0) break;
            uint16_t i = IDX(a);
            if (!mem_loaded[i] || !mem_loaded[i + 1]) break;
            uint16_t labs = mem_word[i], pabs = mem_word[i + 1];
            if (!addr_ok(pabs)) break;
            snprintf(nm, sizeof nm, "PRTS%d", n);
            struct_word(i, nm, "supervisor SRD %d, word 1: absolute L = #%04X", n, labs);
            struct_word(i + 1u, NULL, "supervisor SRD %d, word 2: absolute P = #%04X (monitor module %d entry)", n, pabs, n);
            g_mon_valid[n] = true; g_mon_P[n] = pabs;
            if (n == 0) snprintf(nm, sizeof nm, "MON0_TRAP"); else snprintf(nm, sizeof nm, "MON%d", n);
            add_entry(pabs, nm);
            found++;
        }
        if (found) {
            struct_word(6, "PRTS_PTR", "absolute &C: pointer to the supervisor relocation table PRTS (SRD N at this-4N)");
            struct_word(7, "PRTS_COUNT", "absolute &E: number of supervisor modules (CSV is checked against it, else mode-violation trap)");
            summary("PRTS: pointer at &C = #%04X, %d module(s) found: MON0_TRAP (CSV 0, trap handler), MON1.. = CSV n services; "
                    "modules run in master mode and end with RSV", prts, found);
        }
    }

    /* ---- CPT / contexts / DVT ---- */
    if (!addr_ok(cpt) || IDX(cpt) + 32u > MAX_MEM_WORDS) return;
    uint16_t ci = IDX(cpt);
    for (int k = 0; k < 32; k++) if (!mem_loaded[ci + k]) return;
    int valid = 0;
    for (int k = 0; k < 32; k++) {
        uint16_t p = mem_word[ci + k];
        if (!p || (p & 1)) continue;
        uint16_t pi = IDX(p);
        bool ok = pi + 7u <= MAX_MEM_WORDS;
        for (int j = 0; ok && j < 7; j++) if (!mem_loaded[pi + j]) ok = false;
        if (ok) valid++;
    }
    if (!valid) return;

    struct_word(5, "CPT_PTR", "absolute &A: pointer to the Context Pointer Table (CPT)");
    static const char *ctxf[7] = { "status", "X", "E", "A", "G", "L", "P" };
    for (int k = 0; k < 32; k++) {
        uint16_t p = mem_word[ci + k];
        if (k == 0) struct_word(ci, "CPT", "CPT[0]: context pointer of interrupt level 0");
        else        struct_word(ci + k, NULL, "CPT[%d]: pointer to the context (CTX) of interrupt level %d", k, k);
        if (!p || (p & 1)) continue;
        uint16_t pi = IDX(p);
        bool ok = pi + 7u <= MAX_MEM_WORDS;
        for (int j = 0; ok && j < 7; j++) if (!mem_loaded[pi + j]) ok = false;
        if (!ok) continue;
        if (!mem_struct[pi]) {
            snprintf(nm, sizeof nm, "CTX%d", k);
            for (int j = 0; j < 7; j++)
                struct_word(pi + j, j == 0 ? nm : NULL, "CTX level %d: saved %s%s", k, ctxf[j],
                            j == 6 ? " (entry of the task / interrupt routine of this level)" : "");
            snprintf(nm, sizeof nm, "INT%d", k);
            add_entry(mem_word[pi + 6], nm);
        }
    }
    /* DVT: 32 words just below the CPT (the DIT description speaks of an OVT of
     * 31 configurations in descending order from the CPT address: same area) */
    int32_t dv = (int32_t)cpt - 64;
    if (dv >= 0) {
        uint16_t di = IDX(dv);
        bool ok = true;
        for (int k = 0; k < 32; k++) if (!mem_loaded[di + k]) ok = false;
        if (ok)
            for (int k = 0; k < 32; k++)
                struct_word(di + k, k == 0 ? "DVT" : NULL,
                            "DVT/OVT entry %d: interrupt configuration word (trigger/enable/arm, group, level)", k);
    }
    summary("CPT: pointer at &A = #%04X, %d context(s) found: CTX<level> (7 words: status X E A G L P), "
            "their P fields are the interrupt routines INT<level> (end with DIT/DITR); DVT = 32 words below the CPT",
            cpt, valid);
}

/* ========== Pass 2b: control-flow assertion ========== */

typedef enum { EDGE_ENTRY, EDGE_BRANCH, EDGE_FALLTHROUGH } EdgeKind;

/* Successors of the instruction at word index `widx`.
 *   branch           both outcomes (unconditional: target only); a branch that
 *                    follows an SPA is a call: its fall-through is the return point
 *   CLS / CSV        the instruction after it (the callee comes back there) and,
 *                    if the SRD is resolvable, the section / module entry
 *   RTS RSV DIT DITR dead end
 */
static void get_successors(uint32_t widx, uint16_t inst, uint16_t out[2], EdgeKind kind[2],
                           int self_hops, int hops_out[2], int *n) {
    *n = 0;
    Insn d;
    if (!decode_insn(inst, &d)) return;

    if (d.cls == CL_BRANCH) {
        Operand o;
        compute_operand(widx, &d, &o);
        bool have_target = g_follow_branches && o.ea_known && !d.is_brx;
        bool call = prev_is_spa(widx);
        if ((!d.is_uncond || call) && widx + 1 < MAX_MEM_WORDS) {
            out[*n] = (uint16_t)(widx + 1); kind[*n] = EDGE_FALLTHROUGH;
            hops_out[*n] = self_hops + 1; (*n)++;
        }
        if (have_target) { out[*n] = IDX(o.ea); kind[*n] = EDGE_BRANCH; hops_out[*n] = 0; (*n)++; }
        return;
    }
    if (d.is_cls || d.is_csv) {
        Operand o;
        compute_operand(widx, &d, &o);
        if (widx + 1 < MAX_MEM_WORDS) {
            out[*n] = (uint16_t)(widx + 1); kind[*n] = EDGE_FALLTHROUGH;
            hops_out[*n] = self_hops + 1; (*n)++;
        }
        if (g_follow_branches && o.callee_known) {
            out[*n] = IDX(o.callee); kind[*n] = EDGE_BRANCH; hops_out[*n] = 0; (*n)++;
        }
        return;
    }
    if (d.is_ret) return;

    if (widx + 1 < MAX_MEM_WORDS) {
        out[*n] = (uint16_t)(widx + 1); kind[*n] = EDGE_FALLTHROUGH;
        hops_out[*n] = self_hops + 1; (*n)++;
    }
}

static uint16_t g_worklist[MAX_MEM_WORDS];
static EdgeKind g_worklist_edge[MAX_MEM_WORDS];
static int      g_worklist_hops[MAX_MEM_WORDS];
static int g_worklist_count;

static void wl_push(uint16_t w, EdgeKind how, int hops) {
    if (!mem_loaded[w]) return;
    if (mem_visited[w]) return;
    mem_visited[w] = true;
    g_worklist[g_worklist_count] = w;
    g_worklist_edge[g_worklist_count] = how;
    g_worklist_hops[g_worklist_count] = hops;
    g_worklist_count++;
}

static void report_demotion(uint32_t widx, EdgeKind how) {
    static const char *names[] = { "entry point", "resolved branch/jump", "fallthrough" };
    fprintf(stderr,
        "[pass2] demoting word at byte addr %06X from text-candidate to code "
        "(reached via %s)\n", (unsigned)(widx * 2), names[how]);
}

/* A traced SPA writes a return address into its operand cell: that cell is
 * run-time data, never a constant pointer to follow. */
static void mark_ret_cell(uint16_t a) {
    uint16_t i = IDX(a);
    mem_call_slot[i] = true;
    mem_is_text[i] = false;
}

static int count_call_slots(void) {
    int c = 0;
    for (int i = 0; i < MAX_MEM_WORDS; i++) if (mem_call_slot[i]) c++;
    for (int i = 0; i < 512; i++) if (g_ret_goff[i]) c++;
    return c;
}

/*
 * If there is a branch to a word, then the target area consists of code, not string.
 * By default only EDGE_ENTRY and EDGE_BRANCH edges are trusted enough to
 * demote a text candidate (see g_fallthrough_demotes / hops options).
 */
static void flow_trace_once(bool verbose) {
    memset(mem_visited, 0, sizeof(mem_visited));
    memset(mem_is_code, 0, sizeof(mem_is_code));
    memset(mem_strong_code, 0, sizeof(mem_strong_code));
    g_worklist_count = 0;

    for (int e = 0; e < g_n_entry_points; e++) wl_push(g_entry_points[e], EDGE_ENTRY, 0);

    for (int idx = 0; idx < g_worklist_count; idx++) {
        uint16_t w = g_worklist[idx];
        EdgeKind how  = g_worklist_edge[idx];
        int      hops = g_worklist_hops[idx];

        if (mem_call_slot[w] || mem_struct[w]) continue;   /* data */

        mem_is_code[w] = true;
        if (how == EDGE_ENTRY || how == EDGE_BRANCH) mem_strong_code[w] = true;

        if (mem_is_text[w]) {
            bool trust = (how == EDGE_ENTRY) || (how == EDGE_BRANCH) ||
                         (how == EDGE_FALLTHROUGH &&
                          (g_fallthrough_demotes || hops <= g_fallthrough_trust_hops));
            if (trust) {
                mem_is_text[w] = false;
                if (verbose) report_demotion(w, how);
            } else {
                continue;
            }
        }

        Insn d;
        if (decode_insn(mem_word[w], &d) && is_spa(&d)) {
            Operand o;
            compute_operand(w, &d, &o);
            if (o.ea_known) mark_ret_cell(o.ea);
            else if (o.gsym) g_ret_goff[o.goff & 0x1FF] = true;
        }

        uint16_t succ[2];
        EdgeKind eks[2];
        int hks[2];
        int n;
        get_successors(w, mem_word[w], succ, eks, hops, hks, &n);
        for (int k = 0; k < n; k++) wl_push(succ[k], eks[k], hks[k]);
        if (decode_insn(mem_word[w], &d) && d.is_brx) {
            uint16_t tb[64];
            int nt = brx_table(w, tb, 64);
            for (int k = 0; k < nt; k++) wl_push(tb[k], EDGE_BRANCH, 0);
        }
    }
}

/* New SPA return cells change which pointer cells may be followed, so
 * iterate until the set is stable. */
static void flow_trace(bool verbose) {
    for (int it = 0; it < 8; it++) {
        int before = count_call_slots();
        flow_trace_once(verbose);
        if (count_call_slots() == before) break;
    }
}

/* ========== Pass 3: reference collection (labels) ========== */

static bool g_seen_csv = false;

static void add_ref(uint16_t a, uint16_t flags, bool strong) {
    uint16_t i = IDX(a);
    if (strong) g_ref_s[i] |= flags; else g_ref_w[i] |= flags;
    if (g_refcnt[i] < 0xFFFF) g_refcnt[i]++;
}

static void note_operand_refs(uint32_t w, const Insn *d, const Operand *o, bool strong) {
    if (d->is_csv && strong) g_seen_csv = true;

    if (d->cls == CL_BRANCH && d->is_brx) {
        uint16_t tb[64];
        int nt = brx_table(w, tb, 64);
        for (int k = 0; k < nt; k++) add_ref(BADDR(tb[k]), RF_BR, strong);
        return;
    }
    if (d->cls == CL_BRANCH) {
        if (o->indirect && !o->ptr_gsym) add_ref(o->ptr, RF_PTR | RF_RD | (o->ptr_is_slot ? RF_RETCELL : 0), strong);
        if (o->ea_known) add_ref(o->ea, prev_is_spa(w) ? RF_CALL : RF_BR, strong);
        if (prev_is_spa(w)) add_ref(BADDR(w + 1), RF_BR, strong);   /* where the callee returns (SPA stored P+4) */
        return;
    }
    if (d->is_cls || d->is_csv) {
        if (o->callee_known) add_ref(o->callee, RF_CALL, strong);
    }
    if (o->kind != OK_MEM) return;
    if (o->indirect && !o->ptr_gsym) add_ref(o->ptr, RF_PTR | RF_RD, strong);
    if (!o->ea_known) return;

    uint16_t f = 0;
    if (d->acc & ACC_R)       f |= RF_RD;
    if (d->acc & ACC_W)       f |= RF_WR;
    if (d->acc & ACC_ADDR)    f |= RF_ADDR;
    if (d->acc & ACC_BYTE)    f |= RF_BYTE;
    if (d->acc & ACC_RETCELL) f |= RF_RETCELL;
    if (d->is_cls || d->is_csv) f |= RF_SECNUM | RF_RD;
    if (o->indexed)           f |= RF_IDX;
    if (!f)                   f |= RF_OPND;
    add_ref(o->ea, f, strong);
    if (d->acc & ACC_DBL) add_ref((uint16_t)(o->ea + 2), f | RF_DBL, strong);
}

static void gather_refs(void) {
    memset(g_ref_s, 0, sizeof g_ref_s);
    memset(g_ref_w, 0, sizeof g_ref_w);
    memset(g_refcnt, 0, sizeof g_refcnt);

    for (int e = 0; e < g_n_entry_points; e++) add_ref(BADDR(g_entry_points[e]), RF_ENTRY, true);

    for (uint32_t w = 0; w < MAX_MEM_WORDS; w++) {
        if (!mem_loaded[w] || mem_is_text[w] || mem_call_slot[w] || mem_is_data[w] || mem_struct[w]) continue;
        Insn d;
        if (!decode_insn(mem_word[w], &d)) continue;
        Operand o;
        compute_operand(w, &d, &o);
        note_operand_refs(w, &d, &o, mem_is_code[w]);
    }
}

/* The first 16 words of the CDS are the Task Working Block when the program
 * makes supervisor calls (CSV). */
static void apply_twb(void) {
    if (!g_G_given || !g_seen_csv) return;
    static const char *nm[16] = { "TWB_P", "TWB_L", "TWB_IND", "TWB_ZC", "TWB_T0", "TWB_T1", "TWB_T2", "TWB_T3",
                                  "TWB_T4", "TWB_T5", "TWB_T6", "TWB_T7", "TWB_N3", "TWB_N2", "TWB_N1", "TWB_N0" };
    static const char *ds[16] = {
        "caller's P-G saved by CSV (RSV resumes after the CSV from here)",
        "caller's L-G saved by CSV",
        "caller's program indicators (C, O ...) saved by CSV; a module may edit them before RSV to return C/O",
        "pointer to the common area (G+6) / system common area ZC, set by the loader",
        "T0 monitor work word", "T1 monitor work word", "T2 monitor work word", "T3 monitor work word",
        "T4 monitor work word", "T5 monitor work word", "T6 monitor work word", "T7 monitor work word",
        "N3: return-address cell of a 4th nested internal call (SPA)", "N2: return-address cell of a 3rd nested internal call (SPA)",
        "N1: return-address cell of a 2nd nested internal call (SPA)", "N0: return-address cell of the first internal call (SPA); module entries use it" };
    uint32_t base = IDX(g_G);
    int marked = 0;
    for (int k = 0; k < 16; k++) {
        uint32_t i = base + k;
        if (i >= MAX_MEM_WORDS || !mem_loaded[i] || mem_is_code[i]) continue;
        struct_word(i, nm[k], "Task Working Block word %d: %s", k, ds[k]);
        marked++;
    }
    if (marked)
        summary("TWB: first 16 words of the CDS at G=#%04X (monitor-call work block: saved P, L, indicators, ZC, N0-N3, T0-T7) "
                "- the program makes CSV calls", g_G);
}

static void collect_refs(void) {
    memset(mem_is_data, 0, sizeof mem_is_data);
    g_seen_csv = false;
    /* an untraced 0000 word is far more likely reserved/padding data than "LDA DL #0" */
    for (uint32_t w = 0; w < MAX_MEM_WORDS; w++)
        if (mem_loaded[w] && mem_word[w] == 0 && !mem_is_code[w] && !mem_is_text[w]) mem_is_data[w] = true;

    /* Round 1: references made by traced code decide which words are data. */
    gather_refs();

    const uint16_t data_flags = RF_RD | RF_WR | RF_PTR | RF_DBL | RF_BYTE | RF_OPND | RF_RETCELL | RF_SECNUM;
    const uint16_t code_flags = RF_BR | RF_CALL | RF_ENTRY;
    for (uint32_t w = 0; w < MAX_MEM_WORDS; w++) {
        if (!mem_loaded[w] || mem_is_code[w] || mem_is_text[w]) continue;
        uint16_t s = g_ref_s[w];
        if ((s & data_flags) && !(s & code_flags)) mem_is_data[w] = true;
    }

    apply_twb();

    /* Round 2: data words no longer generate references as if they were instructions. */
    gather_refs();
}

/* ========== Pass 3b: routine classification ========== */

/* Explore everything reachable from an entry without entering callees and
 * build a one-line profile: what the routine does, judged from its code. */
static void scan_routine(uint16_t root) {
    static bool seen[MAX_MEM_WORDS];
    static uint16_t stack[MAX_MEM_WORDS];
    memset(seen, 0, sizeof seen);
    int sp = 0;
    uint8_t role = 0;
    uint16_t start = root;
    if (g_modentry[root]) {          /* SPA N; BRU body; RSV : profile the body */
        Insn b; Operand ob;
        if (root + 1u < MAX_MEM_WORDS && decode_insn(mem_word[root + 1], &b) && b.cls == CL_BRANCH) {
            compute_operand(root + 1u, &b, &ob);
            if (ob.ea_known) start = IDX(ob.ea);
        }
        role |= ROLE_RSV;
    }
    int ninsn = 0, nbyte = 0, nfloat = 0, nloop = 0, npriv = 0, nshift = 0, nmuldiv = 0, nstr = 0, ncmp = 0;
    int nret = 0, nstore_tw = 0;
    uint16_t callee[4]; int ncallee = 0, ncallee_all = 0;
    int csvn[4]; int ncsv = 0, ncsv_all = 0;
    stack[sp++] = start; seen[start] = true;
    while (sp) {
        uint16_t w = stack[--sp];
        if (!mem_loaded[w] || mem_is_text[w] || mem_struct[w] || mem_call_slot[w]) continue;
        Insn d;
        if (!decode_insn(mem_word[w], &d)) continue;
        Operand o;
        compute_operand(w, &d, &o);
        ninsn++;
        const char *op = d.opname;
        if (!strcmp(op, "RTS"))  role |= ROLE_RTS;
        if (!strcmp(op, "RSV"))  role |= ROLE_RSV;
        if (!strcmp(op, "DIT"))  role |= ROLE_DIT;
        if (!strcmp(op, "DITR")) role |= ROLE_DITR;
        if (d.is_csv) {
            role |= ROLE_CSV;
            ncsv_all++;
            if (o.kind == OK_IMM) {
                bool dup = false;
                for (int k = 0; k < ncsv; k++) if (csvn[k] == o.secno) dup = true;
                if (!dup && ncsv < 4) csvn[ncsv++] = o.secno;
            }
        }
        if (d.is_cls) role |= ROLE_CLS;
        if (!strcmp(op, "LBL") || !strcmp(op, "LBR") || !strcmp(op, "LBX") || !strcmp(op, "SBL") || !strcmp(op, "SBR")) nbyte++;
        if (!strcmp(op, "MVS") || !strcmp(op, "TRS")) nstr++;
        if (!strcmp(op, "FAD") || !strcmp(op, "FSU") || !strcmp(op, "FMU") || !strcmp(op, "FDV")) nfloat++;
        if (!strcmp(op, "MUL") || !strcmp(op, "DIV")) nmuldiv++;
        if (!strcmp(op, "CLM") || !strcmp(op, "RD") || !strcmp(op, "WD") || !strcmp(op, "STM")) npriv++;
        if (d.cls == CL_NOOPND && d.opname[0] == 'S' && (strstr(op, "SL") || strstr(op, "SR") || !strcmp(op, "SAD") || !strcmp(op, "SAS"))) nshift++;
        if (!strcmp(op, "CMP") || !strcmp(op, "CPS")) ncmp++;
        if (d.cls == CL_BRANCH && d.am == AM_RM) nloop++;
        if (d.cls == CL_BRANCH && o.indirect && o.ptr_is_slot) nret++;
        if (o.gsym && (d.acc & ACC_W) && o.goff < 0x20) nstore_tw++;

        if (d.is_brx) {
            uint16_t tb[64];
            int nt = brx_table(w, tb, 64);
            for (int q = 0; q < nt; q++) if (!seen[tb[q]]) { seen[tb[q]] = true; stack[sp++] = tb[q]; }
        }
        bool callsite = d.is_cls || d.is_csv || (d.cls == CL_BRANCH && prev_is_spa(w));
        uint16_t succ[2]; EdgeKind k[2]; int h[2], n;
        get_successors(w, mem_word[w], succ, k, 0, h, &n);
        for (int j = 0; j < n; j++) {
            if (callsite && k[j] == EDGE_BRANCH) {
                if (d.cls == CL_BRANCH) {
                    ncallee_all++;
                    bool dup = false;
                    for (int q = 0; q < ncallee; q++) if (callee[q] == succ[j]) dup = true;
                    if (!dup && ncallee < 4) callee[ncallee++] = succ[j];
                }
                continue;   /* do not enter the callee */
            }
            if (!seen[succ[j]]) { seen[succ[j]] = true; stack[sp++] = succ[j]; }
        }
    }
    g_role[root] = role;

    char b[500]; b[0] = 0;
    cat(b, sizeof b, "%d instruction%s", ninsn, ninsn == 1 ? "" : "s");
    if (nstr)   cat(b, sizeof b, ", block move / translate string (MVS/TRS)");
    if (nbyte)  cat(b, sizeof b, ", byte/character handling (%d byte ops)", nbyte);
    if (nfloat) cat(b, sizeof b, ", floating point");
    if (nmuldiv)cat(b, sizeof b, ", multiply/divide");
    if (nshift > 2) cat(b, sizeof b, ", shifts/bit manipulation");
    if (ncmp)   cat(b, sizeof b, ", comparisons");
    if (nloop)  cat(b, sizeof b, ", %d loop%s", nloop, nloop == 1 ? "" : "s");
    if (npriv)  cat(b, sizeof b, ", privileged I/O or mask ops (CLM/RD/WD/STM)");
    if (nstore_tw) cat(b, sizeof b, ", stores into the caller's TWB");
    if (ncsv) {
        cat(b, sizeof b, ", supervisor calls:");
        for (int q = 0; q < ncsv; q++) {
            cat(b, sizeof b, " #%d%s%s", csvn[q], g_csv_name[csvn[q]] ? " " : "", g_csv_name[csvn[q]] ? g_csv_name[csvn[q]] : "");
        }
        if (ncsv_all > ncsv) cat(b, sizeof b, " ...");
    } else if (ncsv_all) cat(b, sizeof b, ", supervisor call(s) with a module number from memory");
    if (ncallee) {
        char t[32];
        cat(b, sizeof b, ", calls");
        for (int q = 0; q < ncallee; q++) cat(b, sizeof b, " %s", lab(BADDR(callee[q]), t, sizeof t));
        if (ncallee_all > ncallee) cat(b, sizeof b, " ...");
    }
    if (role & (ROLE_DIT | ROLE_DITR)) cat(b, sizeof b, ", ends with DIT/DITR");
    else if (role & ROLE_RSV) cat(b, sizeof b, ", returns to its CSV caller with RSV");
    else if (role & ROLE_RTS) cat(b, sizeof b, ", returns with RTS");
    else if (nret) cat(b, sizeof b, ", returns through its return cell");
    if (g_prof[root]) free((void *)g_prof[root]);
    g_prof[root] = strdup(b);
}

static void classify_roots(void) {
    memset(g_role, 0, sizeof g_role);
    for (int e = 0; e < g_n_entry_points; e++) g_root[g_entry_points[e]] = true;
    for (uint32_t i = 0; i < MAX_MEM_WORDS; i++)
        if (g_ref_s[i] & (RF_CALL | RF_ENTRY)) g_root[i] = true;
    for (uint32_t i = 0; i < MAX_MEM_WORDS; i++)
        if (g_root[i] && mem_loaded[i] && !mem_is_text[i] && !mem_struct[i]) scan_routine((uint16_t)i);
}

/* ========== Pass 4: rendering ========== */

static void describe_label(uint16_t i, char *buf, size_t n) {
    uint16_t f = g_ref_s[i] | g_ref_w[i];
    buf[0] = 0;
    if (g_name[i][0] && mem_struct[i]) cat(buf, n, "system structure; ");
    if (f & RF_ENTRY)   cat(buf, n, "entry point; ");
    if (f & RF_CALL)    cat(buf, n, "callee (section / supervisor module / SPA subroutine); ");
    if (f & RF_BR)      cat(buf, n, "branch target; ");
    if (mem_loaded[i] && mem_is_text[i]) cat(buf, n, "EBCDIC text; ");
    if (f & RF_RETCELL) cat(buf, n, "return-address cell (written by SPA, read by the return branch); ");
    if (f & RF_SECNUM)  cat(buf, n, "holds a section / module number; ");
    if (f & RF_PTR) {
        uint16_t tv = mem_word[i];
        if (mem_loaded[i] && !mem_is_text[i] && !mem_call_slot[i] && !(tv & 1) && tv && mem_loaded[IDX(tv)] &&
            (g_ref_s[IDX(tv)] & (RF_CALL | RF_ENTRY))) {
            char tn[32];
            label_name(IDX(tv), tn, sizeof tn);
            cat(buf, n, "VECTOR: holds the entry address of %s; ", tn);
        } else
            cat(buf, n, "pointer cell; ");
    }
    if (f & RF_RD)      cat(buf, n, "read; ");
    if (f & RF_WR)      cat(buf, n, "written; ");
    if (f & RF_IDX)     cat(buf, n, "indexed access (table/array base); ");
    if (f & RF_ADDR)    cat(buf, n, "address taken; ");
    if (f & RF_DBL)     cat(buf, n, "double-word; ");
    if (f & RF_BYTE)    cat(buf, n, "byte access; ");
    if ((f & RF_OPND) && !(f & (RF_RD | RF_WR))) cat(buf, n, "operand of system instruction; ");
    if (g_refcnt[i]) cat(buf, n, "%u ref%s", g_refcnt[i], g_refcnt[i] == 1 ? "" : "s");
    else if (buf[0]) buf[strlen(buf) - 2] = 0;   /* drop trailing "; " */
    if (f && !g_ref_s[i]) cat(buf, n, "; only from untraced words");
}

static void print_banner(FILE *of, uint16_t i) {
    if (!g_root[i] || !(g_ref_s[i] & (RF_CALL | RF_ENTRY)) ) return;
    char nm[32], t[32];
    label_name(i, nm, sizeof nm);
    uint8_t r = g_role[i];
    const char *kind;
    char custom[300];
    if (g_modentry[i]) {
        Insn a; Operand o;
        decode_insn(mem_word[i], &a);
        compute_operand(i, &a, &o);
        snprintf(custom, sizeof custom,
                 "SUPERVISOR MODULE ENTRY: SPA saves the return address in %s, BRU runs the module body as a subroutine, "
                 "RSV (reached when the body returns) goes back to the CSV caller",
                 o.gsym ? gname(o.goff, t, sizeof t) : "its return cell");
        kind = custom;
    } else if (r & (ROLE_DIT | ROLE_DITR))
        kind = "INTERRUPT ROUTINE (ends with DIT/DITR: leaves through the interrupt scheduler)";
    else if (r & ROLE_RSV)
        kind = "SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV";
    else if (r & ROLE_RTS)
        kind = "SECTION (LPS) entered by CLS, leaves with RTS";
    else if (g_ref_s[i] & RF_CALL)
        kind = (g_name[i][0] ? "section / module entry (no RTS/RSV/DIT seen on its paths)"
                             : "SUBROUTINE called with SPA + branch; returns by branch through its return cell");
    else
        kind = "program entry point";
    fprintf(of, "\n; ===== %s: %s =====\n", nm, kind);
    if (g_prof[i]) fprintf(of, "; routine profile: %s\n", g_prof[i]);
}

static void print_label_line(FILE *of, uint32_t i) {
    if (!has_label(i)) return;
    char nm[32], ds[400];
    label_name((uint16_t)i, nm, sizeof nm);
    describe_label((uint16_t)i, ds, sizeof ds);
    fprintf(of, "%s:\t\t\t\t; %s\n", nm, ds);
}

static void print_text_region(FILE *of, uint32_t start, uint32_t end) {
    fprintf(of, "%06X:\t.EBCDIC\t\"", (unsigned)(start * 2));
    for (uint32_t w = start; w < end; w++) {
        uint16_t val = mem_word[w];
        uint8_t bytes2[2] = { (uint8_t)(val >> 8), (uint8_t)(val & 0xFF) };
        for (int b = 0; b < 2; b++) {
            unsigned char a = ebcdic_to_ascii[bytes2[b]];
            if (a >= 0x20 && a < 0x7F && a != '"' && a != '\\') {
                fputc(a, of);
            } else if (a == '"' || a == '\\') {
                fputc('\\', of); fputc(a, of);
            } else {
                fprintf(of, "\\x%02X", bytes2[b]);
            }
        }
    }
    fprintf(of, "\"\t; %u word%s, raw:", (unsigned)(end - start), (end - start) == 1 ? "" : "s");
    for (uint32_t w = start; w < end; w++) fprintf(of, " %04X", mem_word[w]);
    fprintf(of, "\n");
}

static void print_data_word(FILE *of, uint32_t w) {
    fprintf(of, "%06X:\t%04X\t.WORD\t#%04X\t; ", (unsigned)(w * 2), mem_word[w], mem_word[w]);
    if (g_note[w])
        fprintf(of, "%s\n", g_note[w]);
    else if (mem_call_slot[w])
        fprintf(of, "return-address cell (SPA stores P+4-G' here at call time; the value in the dump is just leftover)\n");
    else if (mem_word[w] == 0 && !(g_ref_s[w] & (RF_RD | RF_WR | RF_PTR | RF_DBL | RF_BYTE | RF_OPND | RF_SECNUM)))
        fprintf(of, "zero word (padding, reserved area or unused data; never reached as code)\n");
    else
        fprintf(of, "data word (used as a variable by traced code, never executed)\n");
}

static void print_header(FILE *of) {
    fprintf(of,
        "; ---------------------------------------------------------------------\n"
        "; Mitra-15 disassembly with symbolic labels\n"
        "; ALL ADDRESSES ARE BYTE ADDRESSES: a label suffix equals the address column of the listing.\n"
        "; Prefixes: E_ entry  S_ SPA-called subroutine  L_ branch target  V_ variable  T_ text\n"
        ";           lower case (l_ v_ ...) = referenced only from code the flow trace did not reach\n"
        "; Named structures: SEC<n>/LDS<n>/PRT<n> (program sections), MON<n>/PRTS<n> (supervisor),\n"
        ";           INT<k>/CTX<k>/CPT/DVT (interrupts), TWB_* (monitor-call work block)\n"
        "; Assumed run-time values (static analysis cannot know them): G=#%04X%s  L=#%04X\n"
        ";   indirect base G' = %s;  data D = %s;  branch D = words, P = address of the %s instruction ($+n)\n"
        ";   indirect pointers %s\n"
        ";   X is unknown: indexed accesses are labelled by their base (label+X)\n",
        g_G, g_G_given ? "" : " (not given: G-relative operands shown as TWB_xx / G+xxxx; PRT not located)", g_L,
        g_gprime_zero ? "0 (master mode, absolute)" : "G",
        g_disp_scale == 1 ? "bytes" : "words",
        g_rel_base ? "next" : "current",
        g_ptr_follow ? "are followed when the cell in the image is non-zero and points inside the image"
                     : "are never followed");
    if (g_annot_path) fprintf(of, ";   service names / region banners / labels forced from annotation file %s\n", g_annot_path);
    for (int i = 0; i < g_n_ann_header; i++) fprintf(of, "; %s\n", g_ann_header[i]);
    for (int i = 0; i < g_nsummary; i++) fprintf(of, "; * %s\n", g_summary[i]);
    {
        bool any = false;
        for (uint32_t i = 0; i < MAX_MEM_WORDS; i++) {
            if (!g_modentry[i]) continue;
            if (!any) { fprintf(of, "; MODULE INDEX (entry sequences found, by address):\n"); any = true; }
            char nm[32];
            label_name((uint16_t)i, nm, sizeof nm);
            fprintf(of, ";   %s @%04X: %s\n", nm, BADDR(i), g_prof[i] ? g_prof[i] : "");
        }
    }
    fprintf(of, "; ---------------------------------------------------------------------\n");
}

static void render(FILE *of) {
    print_header(of);
    uint32_t w = 0;
    while (w < MAX_MEM_WORDS) {
        if (!mem_loaded[w]) { w++; continue; }

        for (int r = 0; r < g_n_ann_regions; r++)
            if (!g_ann_regions[r].done && w >= g_ann_regions[r].start) {
                g_ann_regions[r].done = true;
                fprintf(of, "\n; ##########################################################################\n"
                            "; %s\n"
                            "; ##########################################################################\n", g_ann_regions[r].text);
            }
        print_banner(of, (uint16_t)w);
        print_label_line(of, w);

        if (mem_is_text[w]) {
            uint32_t start = w++;
            while (w < MAX_MEM_WORDS && mem_loaded[w] && mem_is_text[w] && !has_label(w)) w++;
            print_text_region(of, start, w);
        } else if (mem_struct[w] || mem_call_slot[w] || mem_is_data[w]) {
            /* collapse runs of plain zero padding (no label, no note, no reference) */
            bool plain_zero = !mem_struct[w] && !mem_call_slot[w] && mem_word[w] == 0 &&
                              !(g_ref_s[w] & (RF_RD | RF_WR | RF_PTR | RF_DBL | RF_BYTE | RF_OPND | RF_SECNUM));
            if (plain_zero) {
                uint32_t start = w++;
                while (w < MAX_MEM_WORDS && mem_loaded[w] && !has_label(w) && !mem_struct[w] && !mem_call_slot[w] &&
                       mem_is_data[w] && mem_word[w] == 0 && !g_root[w]) w++;
                if (w - start == 1) print_data_word(of, start);
                else fprintf(of, "%06X:\t0000\t.ZERO\t%u\t; %u zero words (padding, reserved area or unused data; never reached as code)\n",
                             (unsigned)(start * 2), (unsigned)(w - start), (unsigned)(w - start));
            } else {
                print_data_word(of, w);
                w++;
            }
        } else {
            print_insn(of, w);
            fprintf(of, "\n");
            w++;
        }
    }

    bool any = false;
    for (uint32_t i = 0; i < MAX_MEM_WORDS; i++) {
        if (!has_label(i) || mem_loaded[i]) continue;
        char nm[32];
        if (!any) { fprintf(of, "\n; --- addresses referenced but outside the loaded image ---\n"); any = true; }
        label_name((uint16_t)i, nm, sizeof nm);
        fprintf(of, "%s\tEQU\t#%04X\t; %u ref%s\n", nm, BADDR(i), g_refcnt[i], g_refcnt[i] == 1 ? "" : "s");
    }
}

/* ========== Annotation file (--annot FILE) ==========
 * Lines (# starts a comment), numbers in hex unless noted:
 *   header <text>                      line printed in the listing header
 *   words <addr> <count> <NAME|-> <note>  force count words to be data (name on the first)
 *   vectors <start> <end> <lo> <hi>    words in [start,end] holding an even address in [lo,hi] are
 *                                      routine addresses: data words AND code entry points
 *   csv <n> <NAME> <description>       supervisor-call module number (decimal or 0x..)
 *   region <start> <end> <text>        banner printed before the first word >= start
 *   name <addr> <NAME> <note>          force the label name of a word (+ comment)
 *   entry <addr> <NAME|-> <note>       extra code entry point (+ name, comment)
 *   note <addr> <text>                 comment appended to a word
 */
static char *trim_dup(const char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *r = strdup(s);
    size_t l = strlen(r);
    while (l && (r[l - 1] == '\n' || r[l - 1] == '\r' || r[l - 1] == ' ')) r[--l] = 0;
    return r;
}

static void load_annotations(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot open annotation file %s\n", path); return; }
    char line[600];
    while (fgets(line, sizeof line, f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == 0) continue;
        char nm[40] = "", text[500] = "";
        unsigned a = 0, b = 0; int n = 0;
        if (!strncmp(p, "csv ", 4)) {
            if (sscanf(p + 4, "%i %39s %499[^\n]", &n, nm, text) >= 2 && n >= 0 && n < 256) {
                g_csv_name[n] = strdup(nm);
                g_csv_desc[n] = trim_dup(text);
            }
        } else if (!strncmp(p, "header ", 7)) {
            if (g_n_ann_header < 32) g_ann_header[g_n_ann_header++] = trim_dup(p + 7);
        } else if (!strncmp(p, "words ", 6)) {
            unsigned c = 1;
            if (sscanf(p + 6, "%x %u %39s %499[^\n]", &a, &c, nm, text) >= 3 && g_n_ann_words < 256) {
                AnnWords *aw = &g_ann_words[g_n_ann_words++];
                aw->addr = (uint16_t)a; aw->count = (int)c;
                if (strcmp(nm, "-")) snprintf(aw->name, sizeof aw->name, "%.15s", nm); else aw->name[0] = 0;
                aw->text = trim_dup(text);
            }
        } else if (!strncmp(p, "vectors ", 8)) {
            unsigned c = 0, d2 = 0;
            if (sscanf(p + 8, "%x %x %x %x", &a, &b, &c, &d2) == 4 && g_n_ann_vecs < 64) {
                g_ann_vecs[g_n_ann_vecs].start = (uint16_t)a; g_ann_vecs[g_n_ann_vecs].end = (uint16_t)b;
                g_ann_vecs[g_n_ann_vecs].lo = (uint16_t)c;    g_ann_vecs[g_n_ann_vecs].hi = (uint16_t)d2;
                g_n_ann_vecs++;
            }
        } else if (!strncmp(p, "region ", 7)) {
            if (sscanf(p + 7, "%x %x %499[^\n]", &a, &b, text) >= 2 && g_n_ann_regions < 128) {
                g_ann_regions[g_n_ann_regions].start = IDX(a);
                g_ann_regions[g_n_ann_regions].text = trim_dup(text);
                g_ann_regions[g_n_ann_regions].done = false;
                g_n_ann_regions++;
            }
        } else if (!strncmp(p, "name ", 5) || !strncmp(p, "entry ", 6)) {
            bool ent = (p[0] == 'e');
            if (sscanf(p + (ent ? 6 : 5), "%x %39s %499[^\n]", &a, nm, text) >= 2 && g_n_ann_names < 1024) {
                AnnName *an = &g_ann_names[g_n_ann_names++];
                an->addr = (uint16_t)a;
                if (strcmp(nm, "-")) snprintf(an->name, sizeof an->name, "%.15s", nm); else an->name[0] = 0;
                an->text = trim_dup(text);
                an->is_entry = ent;
            }
        } else if (!strncmp(p, "note ", 5)) {
            if (sscanf(p + 5, "%x %499[^\n]", &a, text) >= 1 && g_n_ann_names < 1024) {
                AnnName *an = &g_ann_names[g_n_ann_names++];
                an->addr = (uint16_t)a; an->name[0] = 0; an->text = trim_dup(text); an->is_entry = 0;
            }
        }
    }
    fclose(f);
}

static void apply_annotations(void) {
    for (int k = 0; k < g_n_ann_words; k++) {
        AnnWords *aw = &g_ann_words[k];
        for (int j = 0; j < aw->count; j++) {
            uint32_t i = IDX(aw->addr) + (uint32_t)j;
            if (i >= MAX_MEM_WORDS || !mem_loaded[i]) continue;
            struct_word(i, (j == 0 && aw->name[0]) ? aw->name : NULL, NULL);
            if (aw->text && aw->text[0] && !g_note[i]) g_note[i] = aw->text;
        }
    }
    int nvec = 0;
    for (int k = 0; k < g_n_ann_vecs; k++) {
        AnnVec *v = &g_ann_vecs[k];
        for (uint32_t a = v->start; a <= v->end; a += 2) {
            uint16_t i = IDX(a);
            if (!mem_loaded[i] || mem_struct[i]) continue;
            uint16_t val = mem_word[i];
            if ((val & 1) || val < v->lo || val > v->hi || !addr_ok(val)) continue;
            add_entry(val, NULL);
            struct_word(i, NULL, "routine address #%04X (vector / dispatch table entry)", val);
            nvec++;
        }
    }
    if (nvec) summary("Vector / dispatch tables: %d routine address(es) taken as code entry points (annotation 'vectors')", nvec);
    for (int k = 0; k < g_n_ann_names; k++) {
        AnnName *an = &g_ann_names[k];
        uint16_t i = IDX(an->addr);
        if (an->is_entry) add_entry(an->addr, NULL);
        if (!mem_loaded[i]) continue;
        if (an->name[0]) snprintf(g_name[i], sizeof g_name[i], "%s", an->name);
        if (an->text && an->text[0]) g_note[i] = an->text;
    }
}

/* A supervisor-module entry is the sequence  SPA <TWB cell> ; BRU body ; RSV  :
 * the module body is called as a subroutine and RSV is what runs when it
 * returns. Every such sequence is the entry of a CSV-callable module. */
static int find_module_entries(void) {
    int cnt = 0;
    for (uint32_t w = 0; w + 2 < MAX_MEM_WORDS; w++) {
        if (!mem_loaded[w] || !mem_loaded[w + 1] || !mem_loaded[w + 2]) continue;
        if (mem_is_text[w] || mem_is_text[w + 1] || mem_is_text[w + 2]) continue;
        Insn a, b, c;
        if (!decode_insn(mem_word[w], &a) || !is_spa(&a)) continue;
        if (!decode_insn(mem_word[w + 1], &b) || b.cls != CL_BRANCH || !b.is_uncond) continue;
        if (!decode_insn(mem_word[w + 2], &c) || strcmp(c.opname, "RSV")) continue;
        char nm[16];
        snprintf(nm, sizeof nm, "MOD_%04X", BADDR(w));
        add_entry(BADDR(w), nm);
        g_modentry[w] = true;
        cnt++;
    }
    if (cnt)
        summary("Supervisor module entries: %d sequence(s) 'SPA cell ; BRU body ; RSV' found (labelled MOD_xxxx): "
                "each is the entry of a module called with CSV", cnt);
    return cnt;
}

static void analyze(bool verbose) {
    find_text_candidates(0, MAX_MEM_WORDS);
    if (verbose) {
        int cnt = 0;
        for (int i = 0; i < MAX_MEM_WORDS; i++) if (mem_is_text[i]) cnt++;
        fprintf(stderr, "[pass1] %d word(s) flagged as text candidates (min run = %d words)\n",
                cnt, g_min_text_words);
    }
    if (g_annot_path) load_annotations(g_annot_path);
    if (g_structures) {
        discover_prt();
        discover_system();
        if (verbose) {
            int s = 0, e = 0;
            for (int i = 0; i < MAX_MEM_WORDS; i++) if (mem_struct[i]) s++;
            for (int i = 0; i < g_n_entry_points; i++) if (g_name[g_entry_points[i]][0]) e++;
            fprintf(stderr, "[struct] %d structure word(s), %d named entry point(s) from PRT/PRTS/CPT\n", s, e);
        }
    }
    find_module_entries();
    apply_annotations();
    if (g_n_entry_points == 0) add_entry(0, NULL);   /* nothing given or discovered: start at 0 */

    flow_trace(verbose);
    if (verbose) {
        int cnt = 0;
        for (int i = 0; i < MAX_MEM_WORDS; i++) if (mem_is_text[i]) cnt++;
        fprintf(stderr, "[pass2] %d word(s) remain classified as text after control-flow assertion\n", cnt);
        fprintf(stderr, "[pass2] %d SPA return cell(s) found\n", count_call_slots());
    }
    collect_refs();
    classify_roots();
    if (verbose) {
        int lab_s = 0, lab_w = 0;
        for (int i = 0; i < MAX_MEM_WORDS; i++) {
            if (g_ref_s[i]) lab_s++; else if (g_ref_w[i]) lab_w++;
        }
        fprintf(stderr, "[pass3] %d label(s) from traced code, %d weak label(s) from untraced words\n", lab_s, lab_w);
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

/* Store one loaded word into the memory arrays, given its BYTE address
 * (as displayed in listings); converts to the word index internally. */
static void store_word(uint32_t byte_addr, uint16_t val) {
    uint32_t widx = byte_addr / 2;
    if (widx >= MAX_MEM_WORDS) return; /* out of the addressable 15-bit word space */
    mem_word[widx] = val;
    mem_loaded[widx] = true;
}

/* Parse one hexdump line and load its words into memory */
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
        } else if (strlen(token) == 2) {
            // A 2-character token that fails isxdigit() is almost always a
            // transcription typo in the source hexdump (e.g. letter 'O'
            // instead of digit '0', or 'l'/'I' instead of '1') rather than
            // something to legitimately skip.             
            // All addresses and instruction words after that bad token would
            // incorrectly shift the flow by one, with no indication anything had gone wrong.
            // So we substitute a placeholder byte to keep alignment intact, but make the problem loud
            // so it can't be missed or silently trusted.
            fprintf(stderr,
                "WARNING: malformed byte token '%s' at approx. address "
                "%06X (byte #%d on this line) - not valid hex, substituting "
                "00 and continuing. This is very likely a transcription "
                "typo in the source (e.g. 'O' vs '0', 'l'/'I' vs '1') - "
                "please verify against the original.\n",
                token, base_addr + byte_count, byte_count);
            bytes[byte_count++] = 0x00;
        }
        token = strtok(NULL, " \t\r\n");
    }

    for (int i = 0; i + 1 < byte_count; i += 2) {
        uint16_t word = ((uint16_t)bytes[i] << 8) | bytes[i + 1];
        uint32_t addr = base_addr + i;
        store_word(addr, word);
    }

    (void)out;
    return byte_count > 0 ? 1 : 0;
}

static void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [options] <file.bin or file.hex>\n"
        "\n"
        "Two-pass EBCDIC-text vs. code discrimination options:\n"
        "  -e, --entry ADDR      Known code entry point, as a BYTE address in hex\n"
        "                        (e.g. -e 0). May be given multiple times (interrupt\n"
        "                        vectors, monitor command table targets, etc).\n"
        "                        Default: -e 0 if none given.\n"
        "  -m, --min-text N      Minimum run length in WORDS (=2 EBCDIC chars) for\n"
        "                        Pass 1 to flag a region as a text candidate.\n"
        "                        Default: 2 (i.e. 4 characters).\n"
        "      --no-branch-follow\n"
        "                        Disable best-effort RP-relative branch target\n"
        "                        resolution in Pass 2; rely on fallthrough only.\n"
        "      --fallthrough-demotes\n"
        "                        Also let a bare 'PC advances to next word' edge\n"
        "                        demote a text candidate to code (off by default:\n"
        "                        on real ROM data it tends to flood straight\n"
        "                        through genuine string tables, since nearly\n"
        "                        every byte pattern is syntactically a valid\n"
        "                        instruction here).\n"
        "      --fallthrough-trust-hops N\n"
        "                        How many consecutive pure-fallthrough hops from\n"
        "                        a confirmed entry point/branch target are still\n"
        "                        trusted to demote a text candidate. Default: 1\n"
        "                        (only the word immediately after a confirmed\n"
        "                        source). 0 disables fallthrough demotion\n"
        "                        entirely; a large value approximates\n"
        "                        --fallthrough-demotes.\n"
        "Address decoding / labels (ALL ADDRESSES ARE BYTE ADDRESSES, = listing column):\n"
        "  -G, --base-g ADDR     Value of the general base G (hex byte address). Needed to\n"
        "                        locate the PRT (SRD n at G-4n). Without it G-relative operands stay symbolic.\n"
        "  -L, --base-l ADDR     Assumed value of the local base L (hex byte address). Default 0.\n"
        "      --abs-indirect    Master mode: indirect addresses are absolute (G' = 0).\n"
        "      --rel-from-next   RP/RM branches are relative to the FOLLOWING instruction instead of the\n"
        "                        branch itself (default: BRU $+2 skips one instruction).\n"
        "      --disp-words      DL/DG/IL/IG displacement D counts words (default: bytes; word\n"
        "                        accesses then have even D, byte accesses may have odd D).\n"
        "                        Branch D always counts words.\n"
        "      --annot FILE      Annotation file: csv <n> <NAME> <text> / region <a> <b> <text> /\n"
        "                        name <a> <NAME> <text> / entry <a> <NAME> <text> / note <a> <text>.\n"
        "      (without -G, G-relative operands are printed symbolically as TWB_xx / G+xxxx)\n"
        "      --no-ptr-follow   Do not resolve indirect addresses through pointer cells\n"
        "                        read from the image.\n"
        "      --no-structures   Do not look for PRT / PRTS / CPT / contexts.\n"
        "System structures found (see header of the listing): sections SEC<n>/LDS<n> (PRT, needs -G),\n"
        "supervisor modules MON<n> (PRTS at absolute &C), interrupt routines INT<k> (CPT at &A).\n"
        "  -r, --raw             Force raw binary mode (skip hexdump auto-detect).\n"
        "  -x, --hex             Force hexdump text mode (skip auto-detect).\n"
        "  -v, --verbose         Print pass1/pass2 diagnostics to stderr.\n"
        "\n",
        prog);
}

int main(int argc, char* argv[]) {
    const char *path = NULL;
    int force_mode = 0; /* 0 = auto, 1 = raw, 2 = hex */
    bool verbose = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--entry") == 0) {
            if (i + 1 >= argc) { print_usage(argv[0]); return 1; }
            i++;
            unsigned long byte_addr = strtoul(argv[i], NULL, 16);
            if (g_n_entry_points < MAX_ENTRY_POINTS) {
                /* Entry points are given as byte addresses (matching the
                 * listing's address column) but stored/traced as word
                 * indices internally. */
                g_entry_points[g_n_entry_points++] = (uint16_t)(byte_addr / 2);
            }
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--min-text") == 0) {
            if (i + 1 >= argc) { print_usage(argv[0]); return 1; }
            g_min_text_words = atoi(argv[++i]);
            if (g_min_text_words < 1) g_min_text_words = 1;
        } else if (strcmp(argv[i], "--no-branch-follow") == 0) {
            g_follow_branches = false;
        } else if (strcmp(argv[i], "--fallthrough-demotes") == 0) {
            g_fallthrough_demotes = true;
        } else if (strcmp(argv[i], "--fallthrough-trust-hops") == 0) {
            if (i + 1 >= argc) { print_usage(argv[0]); return 1; }
            g_fallthrough_trust_hops = atoi(argv[++i]);
            if (g_fallthrough_trust_hops < 0) g_fallthrough_trust_hops = 0;
        } else if (strcmp(argv[i], "-G") == 0 || strcmp(argv[i], "--base-g") == 0) {
            if (i + 1 >= argc) { print_usage(argv[0]); return 1; }
            g_G = (uint16_t)(strtoul(argv[++i], NULL, 16) & 0xFFFE);
            g_G_given = true;
        } else if (strcmp(argv[i], "-L") == 0 || strcmp(argv[i], "--base-l") == 0) {
            if (i + 1 >= argc) { print_usage(argv[0]); return 1; }
            g_L = (uint16_t)(strtoul(argv[++i], NULL, 16) & 0xFFFE);
        } else if (strcmp(argv[i], "--abs-indirect") == 0) {
            g_gprime_zero = true;
        } else if (strcmp(argv[i], "--rel-from-current") == 0) {
            g_rel_base = 0;
        } else if (strcmp(argv[i], "--disp-bytes") == 0) {
            g_disp_scale = 1;
        } else if (strcmp(argv[i], "--disp-words") == 0) {
            g_disp_scale = 2;
        } else if (strcmp(argv[i], "--rel-from-next") == 0) {
            g_rel_base = 1;
        } else if (strcmp(argv[i], "--annot") == 0) {
            if (i + 1 >= argc) { print_usage(argv[0]); return 1; }
            g_annot_path = argv[++i];
        } else if (strcmp(argv[i], "--no-ptr-follow") == 0) {
            g_ptr_follow = false;
        } else if (strcmp(argv[i], "--no-structures") == 0) {
            g_structures = false;
        } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--raw") == 0) {
            force_mode = 1;
        } else if (strcmp(argv[i], "-x") == 0 || strcmp(argv[i], "--hex") == 0) {
            force_mode = 2;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (argv[i][0] == '-' && strlen(argv[i]) > 1) {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            path = argv[i];
        }
    }

    if (!path) {
        print_usage(argv[0]);
        return 1;
    }
    /* no -e given: analyze() starts at address 0 unless PRT/PRTS/CPT entries are discovered */

    FILE* fp = fopen(path, "rb");
    if (!fp) {
        perror("Cannot open file");
        return 1;
    }

    char first_line[MAX_LINE];
    long start_pos = ftell(fp);
    bool got_first_line = fgets(first_line, sizeof(first_line), fp) != NULL;

    bool hex_mode;
    if (force_mode == 1) {
        hex_mode = false;
    } else if (force_mode == 2) {
        hex_mode = true;
    } else {
        // Auto-detect: Does it look like a hexdump? (e.g., "00000000  00 00 ...")
        hex_mode = got_first_line && strlen(first_line) >= 60 &&
                   isxdigit((unsigned char)first_line[0]) &&
                   isxdigit((unsigned char)first_line[1]) &&
                   isxdigit((unsigned char)first_line[7]);
    }

    if (hex_mode) {
        // --- MODE 1: Text Hexdump ---
        fseek(fp, start_pos, SEEK_SET);
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), fp)) {
            if (strlen(line) >= 60 && isxdigit((unsigned char)line[0])) {
                parse_hex_line(line, stdout);
            }
        }
    } else {
        // --- MODE 2: Raw Binary File ---
        fseek(fp, start_pos, SEEK_SET);
        uint8_t byte1, byte2;
        uint32_t addr = 0;

        // Read 16-bit words in Big-Endian format (standard for Mitra-15)
        while (fread(&byte1, 1, 1, fp) == 1 && fread(&byte2, 1, 1, fp) == 1) {
            uint16_t word = ((uint16_t)byte1 << 8) | byte2;
            store_word(addr, word);
            addr += 2;
        }
    }

    fclose(fp);

    analyze(verbose);
    render(stdout);

    return 0;
}
