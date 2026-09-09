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
 * The strategy implemented below (as sketched by the user) is two-pass:
 *
 *   Pass 1 (text candidate detection): scan the loaded image and mark
 *   maximal *word-aligned* runs of words whose both bytes decode to a
 *   restrictive, curated set of printable EBCDIC characters (letters,
 *   digits, space, and common monitor punctuation) as "text candidates",
 *   provided the run is at least MIN_TEXT_WORDS words long. Word alignment
 *   matters because Mitra-15 memory is word-addressable and the CPU always
 *   fetches whole words as instructions; a text run that isn't word-aligned
 *   internally consistent would desynchronize any subsequent code decode.
 *
 *   Pass 2 (control-flow assertion): starting from one or more known entry
 *   points, do a worklist-based control-flow trace over words NOT (yet)
 *   excluded as text, following (a) the deterministic "PC advances to the
 *   next word" fallthrough edge for every non-terminal instruction, and
 *   (b) statically resolvable branch targets (see resolve_branch_target()
 *   below for the significant caveat on what "resolvable" means here).
 *   Whenever this trace lands on a word that Pass 1 flagged as a text
 *   candidate, that word (and, transitively, everything reachable from it)
 *   is demoted back to code and decoding continues normally from there.
 *
 *   Entry point special case: because the trace is seeded directly from
 *   the caller-supplied entry point address(es) *before* Pass 1's
 *   candidate flag is consulted, an entry point that Pass 1 mistakenly
 *   flagged as text is transparently corrected the moment Pass 2 visits it
 *   -- no separate special-casing code is needed, but callers MUST make
 *   sure the real entry point(s) are supplied (see -e / --entry), since a
 *   missing entry point cannot be discovered this way.
 *
 * Caveats (please read before trusting the output):
 *   - Pass 2's branch-target resolution is best-effort and ONLY covers
 *     the RP ("relative to P", i.e. PC-relative) addressing form of
 *     unconditional/conditional branches, using an assumed 8-bit
 *     two's-complement word displacement relative to the address of the
 *     following instruction. This is a common minicomputer convention but
 *     has NOT been verified against the Mitra-15 microprogramming manual;
 *     please check it against Section... covering RP addressing and
 *     adjust resolve_branch_target() if the sign/scale differ. Indirect
 *     forms (RM, IL, IGX, ILX) depend on runtime register contents and are
 *     deliberately left unresolved -- fallthrough tracing is what recovers
 *     those cases.
 *   - This is a heuristic, not a proof: on real code that has a long
 *     literal run of alphanumeric-looking constants and is never reached
 *     by the traced control flow, it can still misclassify. Reachability
 *     from the entry points you supply is only as complete as those entry
 *     points are.
 * ======================================================================== */

/* CP037 (EBCDIC) -> ASCII code point, 0 where there is no printable ASCII
 * equivalent (control codes, etc). Generated from the standard cp037
 * mapping. */
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

/* Curated set of punctuation EBCDIC bytes accepted as part of a "text"
 * byte, on top of letters/digits/space. Deliberately narrow: the goal is
 * to keep Pass 1's per-byte test tight enough that long runs of code
 * rarely satisfy it by coincidence, since letters/digits/space alone are
 * already common enough in real instruction words. Extend if genuine
 * monitor strings use additional punctuation not listed here. */
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

/* IMPORTANT, learned empirically on real monitor code: because almost every
 * byte pattern is *syntactically* a valid Mitra-15 instruction (as noted by
 * the user), a "PC simply advances to the next word" fallthrough edge
 * essentially never hits a dead end -- it will happily walk straight
 * through a genuine EBCDIC message table too, since the table's bytes also
 * parse as "valid-looking" instructions almost all the time. So trusting
 * fallthrough alone as proof of code, with no cap, floods through real
 * strings just as easily as it does through real code.
 *
 * Resolved branch/jump targets are a much stronger signal: nothing walks
 * "accidentally" to a specific computed address the way it accidentally
 * falls through to address+1. So by default, ONLY a resolved branch/jump
 * edge (or an explicit --entry point) demotes a Pass-1 text candidate back
 * to code. Fallthrough edges are still traced (so reachability can keep
 * discovering further branches deeper in real code), but do not by
 * themselves overturn a text candidate unless --fallthrough-demotes is
 * passed. Recommended: start strict (the default), and only relax this if
 * you have evidence -- e.g. from the manual, or from -e-supplied entry
 * points known to be real code -- that a given string-looking area is
 * genuinely misclassified. */
static bool g_fallthrough_demotes = false;

/* When a text candidate IS trusted enough to be reachable via fallthrough
 * (see g_fallthrough_demotes), only trust it within this many consecutive
 * pure-fallthrough hops of the nearest ENTRY or resolved-BRANCH edge. This
 * matters right at a confirmed source: the word immediately after an entry
 * point (or a branch target) is very likely still code, so it's cheap to
 * trust one hop of fallthrough there -- but an unbounded chain of
 * fallthrough hops is exactly what flooded through the real message table
 * in testing (a long, branch-free run of ordinary instructions with no
 * dead end until it wanders into the data). Bounding the window keeps the
 * short "confirm the rest of this instruction's own body" case working
 * without reopening that flood. Tune with --fallthrough-trust-hops. */
static int g_fallthrough_trust_hops = 1;

/* Per-word memory + classification state, indexed by WORD address
 * (0 .. MAX_MEM_WORDS-1), matching the Mitra-15's word-addressable memory
 * (S register: 15 usable bits => exactly MAX_MEM_WORDS addressable words). */
static uint16_t mem_word[MAX_MEM_WORDS];
static bool     mem_loaded[MAX_MEM_WORDS];   /* word was present in the input file */
static bool     mem_is_text[MAX_MEM_WORDS];  /* Pass 1 candidate, possibly demoted by Pass 2 */
static bool     mem_is_code[MAX_MEM_WORDS];  /* Pass 2: confirmed reachable as code */
static bool     mem_visited[MAX_MEM_WORDS];  /* Pass 2 worklist dedup */

#define MAX_ENTRY_POINTS 64
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

/* ========== Disassembly Functions ========== */

/* fprint_sym: Print symbolic output (disassemble instruction) */
// t_stat fprint_sym(FILE *of, t_addr addr, t_value *val, UNIT *uptr, int32 sw) {
bool disassemble_inst(FILE* of, uint16_t val) {
    uint16 inst;
    int mode, opcode;
    uint16 disp;
    const char *opname = NULL;
    const char *modename = NULL;

    inst = val & 0xFFFF;

    mode = (inst >> 13) & 0x07;
    opcode = (inst >> 8) & 0x1F;
    disp = inst & 0x00FF;
    /* What actually gets printed after the mnemonic/mode. Normally same as
     * disp, but the SRG/STM-group/SHR/SHC families below consume disp as a
     * sub-opcode selector (and, for SHR/SHC, a shift count) rather than a
     * real address displacement, so they override this. */
    uint16 print_disp = disp;

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
        case 0x5000:
        case 0x7000:
        case 0x9000:
        case 0xB000:
            /*
            Group 2 instructions (store family): DLD, STA, STE, STX, SBL,
            SBR, DST, ADM, SPA, STS, FAD, FSU, FMU, FDV, TRS, MVS.
            opcode here is always 0x10 + suboffset (suboffset 0-15), because
            the store-family flag bit is bit 12 of the instruction; mask it
            off to recover the table index.
            */
            opname = group2_opnames[opcode & 0x0F];
            break;
        case 0x3000:
        case 0xE000:
        case 0xF000:
            /*
            System / register instructions: SHR, SRG, ICX, DCX, ICL, DCL,
            CSV, CLS, LDR, STR, LDP, SHC, TES (plus STM/CLM at suboffset 4,
            which the manual lists sharing a single P-only encoding). These
            use their own three addressing forms - DL (0x3xxx), PX (0xExxx)
            and P (0xFxxx) - which the generic mode_names[] table above does
            NOT correctly represent (it collides with "P"/"RM"), so it's
            overridden here.
            */
            {
                static const char *sys_names[] = {
                    "SHR", "SRG", "ICX", "DCX", "???", "ICL", "DCL", "CSV",
                    "CLS", "LDR", "STR", "LDP", "SHC", "TES", "???", "???"
                };
                int suboffset = opcode & 0x0F;
                opname = sys_names[suboffset];
                modename = (hexcode == 0x3000) ? "DL" :
                           (hexcode == 0xE000) ? "PX" : "P";

                if (suboffset == 0x0) {
                    /* SHR: low byte is shift-type (bits 8-10) + step count
                     * (bits 11-15), not a displacement (manual p.103/195). */
                    static const char *shr_names[] = {
                        "SLLS", "SRCS", "SAD", "SLCD", "SLCS", "SAS", "SRLS", "SRCD"
                    };
                    opname = shr_names[(disp >> 5) & 0x07];
                    print_disp = disp & 0x1F;
                } else if (suboffset == 0xC) {
                    /* SHC: same layout as SHR, but only 4 of the 8 shift
                     * types are assigned; type 1 is the unrelated DITR
                     * instruction (manual p.112/195). */
                    switch ((disp >> 5) & 0x07) {
                        case 0: opname = "SLLD"; print_disp = disp & 0x1F; break;
                        case 2: opname = "PTY";  print_disp = disp & 0x1F; break;
                        case 4: opname = "SRLD"; print_disp = disp & 0x1F; break;
                        case 6: opname = "NLZ";  print_disp = disp & 0x1F; break;
                        case 1: opname = "DITR"; print_disp = 0; break;
                        default: opname = NULL; break;
                    }
                } else if (suboffset == 0x1) {
                    /* SRG: low byte is 2 * sub-instruction number, e.g.
                     * RTS=F100, XAE=F102, ..., RSV=F10C, ... CHX=F11E
                     * (manual p.119/195, confirmed against Appendix B). */
                    static const char *srg_names[] = {
                        "RTS", "XAE", "XAX", "XEX", "XAA", "CCE", "RSV", "ACE",
                        "CCA", "AEE", "CNX", "AIE", "AAE", "LNE", "CNA", "CHX"
                    };
                    int srg_sub = disp >> 1;
                    opname = (srg_sub < 16) ? srg_names[srg_sub] : NULL;
                    print_disp = 0;
                } else if (suboffset == 0x4 && hexcode == 0xF000) {
                    /* Control instructions CLM/DIT/RD/WD/STM: only exist as
                     * the P-form F4xx (Appendix B shows 0x34/0xE4 unused),
                     * selected directly by the low byte (manual p.164-167). */
                    switch (disp) {
                        case 0x00: opname = "CLM"; break;
                        case 0x01: opname = "DIT"; break;
                        case 0x02: opname = "RD";  break;
                        case 0x03: opname = "WD";  break;
                        case 0x08: opname = "STM"; break;
                        default:   opname = NULL;  break;
                    }
                    print_disp = 0;
                } else if (suboffset == 0x4) {
                    /* 0x34 / 0xE4: unused per Appendix B's hex-order table. */
                    opname = NULL;
                }
            }
            break;
        case 0xC000:
        case 0xD000:
            /*
            Branch instructions. opcode & 0x07 recovers BCT..BRU regardless
            of addressing form (confirmed against the manual's branch pages).
            The addressing-mode label, however, was wrong: mode_names[mode]
            can't distinguish RP/RM (only differ in bit 11) nor 0xC/0xD
            (only differ in bit 12, outside the 3-bit "mode" field), so it
            always printed "RP". Fixed using the actual distinguishing bits.
            */
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
            {
                int rm_bit = (inst >> 11) & 1;
                modename = (hexcode == 0xC000) ? (rm_bit ? "RM" : "RP")
                                                : (rm_bit ? "IG" : "IL");
            }
            break;
    }
    
    /* Print instruction */
    if (opname) {
        if (print_disp) {
            fprintf(of, "%s\t%s\t#%03x", opname, modename, print_disp);
        } else {
            fprintf(of, "%s\t%s", opname, modename);
        }
    } else {
        fprintf(of, "??\t%04X", inst);
    }
    
    return true;
}

/* ========== Pass 1: text-candidate detection ========== */

/* Scan [lo, hi) and flag maximal word-aligned runs of >= g_min_text_words
 * words, where both bytes of every word in the run look like EBCDIC text,
 * as text candidates. This is purely local/lexical -- Pass 2 is what
 * asserts or overturns these candidates using control flow. */
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

/* ========== Pass 2: control-flow assertion ========== */

/* Best-effort branch target resolution. Only the RP (PC-relative) form is
 * resolved -- see the caveats block near the top of this file. Returns
 * true and fills *target (a word index) if resolution succeeded. */
static bool resolve_branch_target(uint32_t widx, uint16_t inst, uint16_t *target) {
    if (!g_follow_branches) return false;

    uint16_t hexcode = inst & 0xF000;
    if (hexcode != 0xC000) return false; /* only 0xC000 form has an RP/RM choice */
    int rm_bit = (inst >> 11) & 1;
    if (rm_bit != 0) return false; /* rm_bit=1 => RM, depends on a runtime base register */

    uint8_t disp = (uint8_t)(inst & 0x00FF);
    int8_t signed_disp = (int8_t)disp; /* ASSUMPTION: 8-bit two's complement word displacement */
    int32_t t = (int32_t)widx + 1 + signed_disp;
    if (t < 0 || t >= MAX_MEM_WORDS) return false;

    *target = (uint16_t)t;
    return true;
}

typedef enum { EDGE_ENTRY, EDGE_BRANCH, EDGE_FALLTHROUGH } EdgeKind;

/* Determine the set of statically-known control-flow successors of the
 * instruction at word index `widx` with raw value `inst`. Writes up to 2
 * word indices into out[] and their EdgeKind into kind[], and the count
 * into *n. An empty successor set means either a terminal instruction
 * (e.g. RTS) or an encoding this function doesn't recognize as valid -- in
 * both cases flow tracing stops here rather than guessing forward into
 * possibly-unrelated data. */
static void get_successors(uint32_t widx, uint16_t inst, uint16_t out[2], EdgeKind kind[2],
                            int self_hops, int hops_out[2], int *n) {
    *n = 0;
    /* self_hops is how many pure-fallthrough hops `widx` itself is from the
     * nearest ENTRY/BRANCH source. A BRANCH successor always resets to 0
     * (a freshly-resolved jump target is a strong source in its own
     * right); a FALLTHROUGH successor is self_hops + 1. */

    uint16_t hexcode = inst & 0xF000;
    uint16_t opcode  = (inst >> 8) & 0x1F;

    bool valid;
    switch (hexcode) {
        case 0x0000: case 0x2000: case 0x4000: case 0x6000: case 0x8000: case 0xA000:
            valid = (opcode < 16);
            break;
        case 0x1000: case 0x5000: case 0x7000: case 0x9000: case 0xB000:
            valid = true; /* group2_opnames has no blank entries */
            break;
        case 0x3000: case 0xE000: case 0xF000: {
            /* Mirrors sys_names[]/group3_opnames[] in disassemble_inst():
             * suboffsets 0x4, 0xE, 0xF are undefined. */
            static const bool sys_valid[16] = {
                true, true, true, true, false, true, true, true,
                true, true, true, true, true, true, false, false
            };
            valid = sys_valid[opcode & 0x0F];
            break;
        }
        case 0xC000: case 0xD000:
            valid = true; /* all 8 branch mnemonics are defined */
            break;
        default:
            valid = false;
    }

    if (!valid) return; /* dead end: don't propagate through unknown encodings */

    if (hexcode == 0xC000 || hexcode == 0xD000) {
        int op3 = opcode & 0x07;
        bool is_bru = (op3 == 7); /* unconditional branch: BRU */
        uint16_t target;
        bool have_target = resolve_branch_target(widx, inst, &target);

        if (is_bru) {
            /* Unconditional: control does not fall through. */
            if (have_target) { out[*n] = target; kind[*n] = EDGE_BRANCH; hops_out[*n] = 0; (*n)++; }
        } else {
            /* Conditional: both fallthrough and (if known) the target. */
            if (widx + 1 < MAX_MEM_WORDS) {
                out[*n] = (uint16_t)(widx + 1); kind[*n] = EDGE_FALLTHROUGH;
                hops_out[*n] = self_hops + 1; (*n)++;
            }
            if (have_target) { out[*n] = target; kind[*n] = EDGE_BRANCH; hops_out[*n] = 0; (*n)++; }
        }
        return;
    }

    if (hexcode == 0x3000 || hexcode == 0xE000 || hexcode == 0xF000) {
        int suboffset = opcode & 0x0F;
        if (suboffset == 0x1) {
            /* SRG group; srg_sub 0 is RTS ("return"), a genuine control
             * flow dead end from this tool's static point of view. */
            int srg_sub = (inst & 0xFF) >> 1;
            if (srg_sub == 0) return;
        }
    }

    /* Default: straight-line fallthrough to the next word. */
    if (widx + 1 < MAX_MEM_WORDS) {
        out[*n] = (uint16_t)(widx + 1); kind[*n] = EDGE_FALLTHROUGH;
        hops_out[*n] = self_hops + 1; (*n)++;
    }
}

static uint16_t g_worklist[MAX_MEM_WORDS];
static EdgeKind g_worklist_edge[MAX_MEM_WORDS]; /* how each queued word was reached */
static int      g_worklist_hops[MAX_MEM_WORDS]; /* pure-fallthrough hops since nearest strong source */
static int g_worklist_count;

static void wl_push(uint16_t w, EdgeKind how, int hops) {
    if (!mem_loaded[w]) return;   /* nothing there to trace into */
    if (mem_visited[w]) return;   /* already queued/processed */
    mem_visited[w] = true;
    g_worklist[g_worklist_count] = w;
    g_worklist_edge[g_worklist_count] = how;
    g_worklist_hops[g_worklist_count] = hops;
    g_worklist_count++;
}

/* Diagnostics: regions demoted from text candidate back to code, reported
 * to stderr so the classification can be sanity-checked / calibrated. */
static void report_demotion(uint32_t widx, EdgeKind how) {
    static const char *names[] = { "entry point", "resolved branch/jump", "fallthrough" };
    fprintf(stderr,
        "[pass2] demoting word at byte addr %06X from text-candidate to code "
        "(reached via %s)\n", (unsigned)(widx * 2), names[how]);
}

/* Runs the worklist-based reachability trace from the configured entry
 * points, demoting any Pass-1 text candidate it actually reaches back to
 * code. See the caveats block near the top of the file.
 *
 * By default only EDGE_ENTRY and EDGE_BRANCH edges are trusted enough to
 * demote a text candidate; EDGE_FALLTHROUGH edges are still traced (so
 * reachability keeps discovering further branches) but do not by
 * themselves overturn a text candidate unless g_fallthrough_demotes is
 * set. See the comment on g_fallthrough_demotes for why. */
static void flow_trace(bool verbose) {
    memset(mem_visited, 0, sizeof(mem_visited));
    memset(mem_is_code, 0, sizeof(mem_is_code));
    g_worklist_count = 0;

    for (int e = 0; e < g_n_entry_points; e++) wl_push(g_entry_points[e], EDGE_ENTRY, 0);

    for (int idx = 0; idx < g_worklist_count; idx++) {
        uint16_t w = g_worklist[idx];
        EdgeKind how  = g_worklist_edge[idx];
        int      hops = g_worklist_hops[idx];
        mem_is_code[w] = true;

        if (mem_is_text[w]) {
            bool trust = (how == EDGE_ENTRY) || (how == EDGE_BRANCH) ||
                         (how == EDGE_FALLTHROUGH &&
                          (g_fallthrough_demotes || hops <= g_fallthrough_trust_hops));
            if (trust) {
                /* Control flow actually reaches into what Pass 1 thought was
                 * a string -- it's code after all. This also transparently
                 * handles the "entry point begins with what looks like a
                 * string" special case: the entry point is pushed above
                 * unconditionally, before mem_is_text[] is ever consulted. */
                mem_is_text[w] = false;
                if (verbose) report_demotion(w, how);
            } else {
                /* Don't trust a distant, unbroken fallthrough chain to
                 * overturn a text candidate on its own (see
                 * g_fallthrough_trust_hops) -- but also don't keep tracing
                 * INTO the candidate as if it were code, since we've just
                 * concluded it probably isn't. Stop this path here; the
                 * text region will simply render as .EBCDIC and any real
                 * code after it needs to be reached some other way (a
                 * resolved branch, or its own -e entry point). */
                continue;
            }
        }

        uint16_t succ[2];
        EdgeKind eks[2];
        int hks[2];
        int n;
        get_successors(w, mem_word[w], succ, eks, hops, hks, &n);
        for (int k = 0; k < n; k++) wl_push(succ[k], eks[k], hks[k]);
    }
}

/* ========== Pass 3: rendering ========== */

/* Print one contiguous, still-classified-as-text run of words [start, end)
 * as an EBCDIC string directive rather than disassembling it. */
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

/* Emits the final listing: instructions for words classified as code
 * (including everything never flagged as text at all), and .EBCDIC
 * directives for contiguous runs still classified as text after Pass 2. */
static void render(FILE *of) {
    uint32_t w = 0;
    while (w < MAX_MEM_WORDS) {
        if (!mem_loaded[w]) { w++; continue; }

        if (mem_is_text[w]) {
            uint32_t start = w;
            while (w < MAX_MEM_WORDS && mem_loaded[w] && mem_is_text[w]) w++;
            print_text_region(of, start, w);
        } else {
            uint16_t val = mem_word[w];
            fprintf(of, "%06X:\t%04X\t", (unsigned)(w * 2), val);
            disassemble_inst(of, val);
            fprintf(of, "\n");
            w++;
        }
    }
}

/* Runs Pass 1 + Pass 2 over the whole loaded image. Call after loading and
 * before render(). */
static void analyze(bool verbose) {
    find_text_candidates(0, MAX_MEM_WORDS);
    if (verbose) {
        int cnt = 0;
        for (int i = 0; i < MAX_MEM_WORDS; i++) if (mem_is_text[i]) cnt++;
        fprintf(stderr, "[pass1] %d word(s) flagged as text candidates "
                        "(min run = %d words)\n", cnt, g_min_text_words);
    }
    flow_trace(verbose);
    if (verbose) {
        int cnt = 0;
        for (int i = 0; i < MAX_MEM_WORDS; i++) if (mem_is_text[i]) cnt++;
        fprintf(stderr, "[pass2] %d word(s) remain classified as text after "
                        "control-flow assertion\n", cnt);
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
            // BUGFIX: previously this case was silently ignored (the token
            // was just dropped without incrementing byte_count), which
            // desynchronized every following byte's position - all
            // addresses and instruction words after the bad token would
            // shift by one, with no indication anything had gone wrong.
            // A 2-character token that fails isxdigit() is almost always a
            // transcription typo in the source hexdump (e.g. letter 'O'
            // instead of digit '0', or 'l'/'I' instead of '1') rather than
            // something to legitimately skip. Substitute a placeholder
            // byte to keep alignment intact, but make the problem loud and
            // specific so it can't be missed or silently trusted.
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
    if (g_n_entry_points == 0) {
        /* Default entry point: address 0. Callers who know the real reset
         * vector / interrupt table / monitor entry addresses should pass
         * -e explicitly -- Pass 2 can only trace what it's told to start
         * from. */
        g_entry_points[g_n_entry_points++] = 0;
    }

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
