
#include "mitra_defs.h"
#include "mitra_io.h"
#include "mitra_cpu.h"

int get_highest_interrupt(void);

uint16 group_1(uint16 target_address, uint16 inst);
uint16 group_2(uint16 inst, uint16 address, uint32 mode);

uint16 group_1_DL(uint16 inst);
uint16 group_2_DL(uint16 inst, uint32 mode);
uint16 group_1_P(uint16 inst);
uint16 group_3_DL(uint16 inst, uint32 mode);
uint16 group_1_DG(uint16 inst);
uint16 group_2_DG(uint16 inst, uint32 mode);
uint16 group_1_IL(uint16 inst);
uint16 group_2_IL(uint16 inst, uint32 mode);
uint16 group_1_IGX(uint16 inst);
uint16 group_2_IGX(uint16 inst, uint32 mode);
uint16 group_1_ILX(uint16 inst);
uint16 group_2_ILX(uint16 inst, uint32 mode);
uint16 group_4_RP(uint16 inst);
uint16 group_4_RM(uint16 inst);
uint16 group_5_IL(uint16 inst);
uint16 group_5_IG(uint16 inst);
uint16 group_3_PX(uint16 inst, uint32 mode);
uint16 group_3_P(uint16 inst, uint32 mode);

extern UNIT cpu_unit ;
extern int susp_stack_ptr;

/* ========== Effective Address Calculation ========== 
The manual defines three instruction classes and addressing modes:
0:	DL, P, DG, IL, IGX, ILX			load/arithmetic
0':	DL, DG, IL, IGX, ILX (same but no P)	store/complex
1:	P, PX, DL				shift, index, base and system operations
2:	RP, RM, IL, IG				conditional or unconditional branch instructions

-IG (Indirect General) is not directly listed with its own mode number in the code. It appears to be handled under mode 4 (DG) in some contexts or combined with other modes in ea_class2().
 *
 * The addressing mode is determined most of time by the opcode byte; bits 0-2 of
 * the instruction word alone do NOT suffice (they are overloaded across
 * classes).  The mapping below is derived directly from the per-opcode
 * (addressing-mode, instruction) table in the CII Mitra-15 reference manual.
 *
 * Addressing mode formulae (D = 8-bit displacement, G' = G in slave mode / 0
 * in master mode):
 *
 *   DL  – Direct Local          : Y = (L + D) & 0x7FFF
 *   P   – Parameter/Immediate   : Y = D            (no memory indirection)
 *   PX  – Parameter Indexed     : Y = D            (Class 1, same as P for EA)
 *   DG  – Direct General        : Y = (G + D) & 0x7FFF
 *   IL  – Indirect Local        : Y = (G' + mem[L + D]) & 0x7FFF
 *   IGX – Indirect General Idx  : Y = (G' + mem[G + D] + X) & 0x7FFF
 *   ILX – Indirect Local Idx    : Y = (G' + mem[L + D] + X) & 0x7FFF
 *   RP  – Relative Plus  (br)   : Y = (P + 2*D) & 0x7FFF
 *   RM  – Relative Minus (br)   : Y = (P - 2*D) & 0x7FFF
 *   IG  – Indirect General (br) : Y = (G' + mem[G + D]) & 0x7FFF
 *
 * Opcodes 34, 3E, 3F, E4, EE, EF, FE, FF are not implemented; this
 * function returns 0 for them (the caller is responsible for rejection).
 *
 Data width:
The Mitra-15 is fundamentally a 16-bit word-addressable machine. Data size is chosen implicitly by the opcode (bits 4–7), not by any extra control bits in the instruction. Examples:
	Byte: LBL (Load Byte Left into A)
	Word: LDA, STA, ADD, SUB, AND, IOR, etc.
	Double word: DLD (Double Load): loads two words → E and A
*/

/* ========== Condition Code Functions (per manual section II-6) ========== */

/* For LOAD instructions: C=1 if result=0, O=1 if result negative */
static void set_condition_codes_load(uint16 result) {
    cpu_state.C = (result == 0) ? 1 : 0;
    cpu_state.OV = (result & 0x8000) ? 1 : 0;
}

/* For COMPARE instructions: 
 * C=1 if A == operand (equality)
 * C=0 if A > operand
 * O=1 if A < operand
 */
static void set_condition_codes_compare(uint16 a, uint16 b, uint16 result) {
    (void) result;
    if (a == b) {
        cpu_state.C = 1;
        cpu_state.OV = 0;
    } else if (a < b) {
        cpu_state.C = 0;
        cpu_state.OV = 1;
    } else {
        cpu_state.C = 0;
        cpu_state.OV = 0;
    }
}

/* For ARITHMETIC instructions:
 * C = carry/borrow
 * O = overflow (operands same sign, result opposite sign)
 */
static void set_condition_codes_arithmetic(uint16 result, uint16 carry, uint16 overflow) {
    cpu_state.C = carry;
    cpu_state.OV = overflow;
}

/* For string operations and tests */
static void set_condition_codes_string(int equal, int less) {
    if (equal) {
        cpu_state.C = 1;
        cpu_state.OV = 0;
    } else if (less) {
        cpu_state.C = 0;
        cpu_state.OV = 1;
    } else {
        cpu_state.C = 0;
        cpu_state.OV = 0;
    }
}

/* ========== Arithmetic Functions ========== */
static uint16 add16(uint16 a, uint16 b, uint16 * carry, uint16 * overflow) {
    uint32 sum = (uint32) a + (uint32) b + * carry;
    uint16 result = sum & 0xFFFF;
    * carry = (sum >> 16) & 1;
    if (((a & 0x8000) == (b & 0x8000)) &&
        ((a & 0x8000) != (result & 0x8000))) {
        * overflow = 1;
    } else {
        * overflow = 0;
    }
    return result;
}

static uint16 sub16(uint16 a, uint16 b, uint16 * carry, uint16 * overflow) {
    uint32 diff = (uint32) a + (uint32)(~b & 0xFFFF) + * carry;
    uint16 result = diff & 0xFFFF;
    * carry = (diff >> 16) & 1;
    if (((a & 0x8000) != (b & 0x8000)) &&
        ((a & 0x8000) != (result & 0x8000))) {
        * overflow = 1;
    } else {
        * overflow = 0;
    }
    return result;
}

static void mul32(uint16 a, uint16 b, uint16 * high, uint16 * low) {
    uint32 product = (uint32)(int16_t) a * (uint32)(int16_t) b;
    * high = (product >> 16) & 0xFFFF;
    * low = product & 0xFFFF;
}

static int div32(uint16 high, uint16 low, uint16 divisor, uint16 * quot, uint16 * rem) {
    int32_t dividend = ((int32_t)(int16_t) high << 16) | (uint16_t) low;
    int16_t dvsr = (int16_t) divisor;
    if (dvsr == 0) return -1;
    * quot = (uint16_t)(dividend / dvsr);
    * rem = (uint16_t)(dividend % dvsr);
    return 0;
}

/* ========== Shift Operations (per manual section VII-7) ========== */

/* Shift Left Logical Single (SLLS) */
static uint16 shift_lls(uint16 val, int count) {
    uint16 result = val;
    int i;
    for (i = 0; i < count; i++) {
        cpu_state.C = (result & 0x8000) ? 1 : 0;
        result <<= 1;
    }
    return result;
}

/* Shift Right Logical Single (SRLS) */
static uint16 shift_rls(uint16 val, int count) {
    uint16 result = val;
    int i;
    for (i = 0; i < count; i++) {
        cpu_state.C = result & 1;
        result >>= 1;
    }
    return result;
}

/* Shift Right Arithmetic Single (SAS) - preserve sign bit */
static uint16 shift_sas(uint16 val, int count) {
    uint16 result = val;
    int i;
    for (i = 0; i < count; i++) {
        cpu_state.C = result & 1;
        uint16 sign = result & 0x8000;
        result >>= 1;
        if (sign) result |= 0x8000;
    }
    return result;
}

/* Shift Right Circular Single (SRCS) */
static uint16 shift_srcs(uint16 val, int count) {
    uint16 result = val;
    int i;
    for (i = 0; i < count; i++) {
        cpu_state.C = result & 1;
        result = (result >> 1) | ((result & 1) << 15);
    }
    return result;
}

/* Shift Left Circular Single (SLCS) */
static uint16 shift_slcs(uint16 val, int count) {
    uint16 result = val;
    int i;
    for (i = 0; i < count; i++) {
        cpu_state.C = (result & 0x8000) ? 1 : 0;
        result = (result << 1) | ((result & 0x8000) ? 1 : 0);
    }
    return result;
}

/* Shift Left Logical Double (SLLD) - shift (E,A) left */
static void shift_lld(uint16 * E, uint16 * A, int count) {
    int i;
    for (i = 0; i < count; i++) {
        cpu_state.C = ( * E & 0x8000) ? 1 : 0;
        * E = ( * E << 1) | (( * A & 0x8000) ? 1 : 0);
        * A <<= 1;
    }
}

/* Shift Right Logical Double (SRLD) */
static void shift_rld(uint16 * E, uint16 * A, int count) {
    int i;
    for (i = 0; i < count; i++) {
        cpu_state.C = * A & 1;
        * A = ( * A >> 1) | (( * E & 1) << 15);
        * E >>= 1;
    }
}

/* Shift Right Arithmetic Double (SAD) */
static void shift_sad(uint16 * E, uint16 * A, int count) {
    int i;
    for (i = 0; i < count; i++) {
        cpu_state.C = * A & 1;
        uint16 sign = * E & 0x8000;
        * A = ( * A >> 1) | (( * E & 1) << 15);
        * E = ( * E >> 1);
        if (sign) * E |= 0x8000;
    }
}

/* Shift Left Circular Double (SLCD) */
static void shift_lcd(uint16 * E, uint16 * A, int count) {
    int i;
    for (i = 0; i < count; i++) {
        uint16 msb = ( * E & 0x8000) ? 1 : 0;
        cpu_state.C = msb;
        * E = ( * E << 1) | (( * A & 0x8000) ? 1 : 0);
        * A = ( * A << 1) | msb;
    }
}

/* Shift Right Circular Double (SRCD) */
static void shift_rcd(uint16 * E, uint16 * A, int count) {
    int i;
    for (i = 0; i < count; i++) {
        uint16 lsb = * A & 1;
        cpu_state.C = lsb;
        * A = ( * A >> 1) | (( * E & 1) << 15);
        * E = ( * E >> 1) | (lsb << 15);
    }
}

/* Normalize (NLZ) - shift left until bit 0 != bit 1 or max steps */
static int normalize(uint16 * E, uint16 * A, uint16 * X, int max_steps) {
    int steps = 0;
    uint32 double_word = ((uint32) * E << 16) | * A;
    while (steps < max_steps && steps < 31) {
        if (((double_word >> 31) & 1) != ((double_word >> 30) & 1))
            break;
        double_word <<= 1;
        steps++;
    }
    * E = (double_word >> 16) & 0xFFFF;
    * A = double_word & 0xFFFF;
    * X -= steps;

    if (steps == 0) {
        cpu_state.C = 0;
        cpu_state.OV = 1;
    } else if (steps == max_steps) {
        cpu_state.C = 1;
        cpu_state.OV = 0;
    } else {
        cpu_state.C = 0;
        cpu_state.OV = 0;
    }
    return steps;
}

/* Compute parity (PTY) - count set bits shifted out */
static uint16 compute_parity(uint16 * A, int count) {
    uint16 result = * A;
    uint16 parity_count = 0;
    int i;
    for (i = 0; i < count; i++) {
        if (result & 0x8000) parity_count++;
        result = (result << 1) | ((result & 0x8000) ? 1 : 0);
    }
    * A = result;
    cpu_state.C = (result & 0x8000) ? 1 : 0;
    cpu_state.OV = 0;
    return parity_count;
}

/* ========== Floating Point (per manual section VII-9) ========== */
/* Mitra-15 uses a base-16 exponent with a characteristic +64 */
static double mitra_to_double(uint16 A, uint16 E) {
    uint32 raw = ((uint32) A << 16) | E;
    int sign = (raw >> 31) & 1;
    int exp = (raw >> 24) & 0x7F;
    uint32 mant = raw & 0xFFFFFF;
    double m = mant / (double)(1 << 24);
    /* Exponent is base-16, characteristic is exp-64 */

    double val = m * pow(16.0, exp - 64);
    return sign ? -val : val;
}

static void double_to_mitra(double v, uint16 * A, uint16 * E) {
    int sign = (v < 0);
    if (sign) v = -v;
    int exp;
    double m = frexp(v, & exp);
    /* Convert from base-2 exponent to base-16 */
    int exp16 = (exp - 1) / 4;
    double m16 = m * pow(2.0, 4 - (exp - 1 - exp16 * 4));
    uint32 mant = (uint32)(m16 * (1 << 24));
    uint32 raw = (sign << 31) | ((exp16 + 64) << 24) | (mant & 0xFFFFFF);
    * A = (raw >> 16) & 0xFFFF;
    * E = raw & 0xFFFF;
}

/* ========== Main Instruction Execution ========== */

/* Shift table for SHR instruction (manual page 7-38) */
typedef enum {
    SHIFT_SLLS = 0,
        SHIFT_SRCS = 1,
        SHIFT_SAD = 2,
        SHIFT_SLCD = 3,
        SHIFT_SLCS = 4,
        SHIFT_SAS = 5,
        SHIFT_SRLS = 6,
        SHIFT_SRCD = 7
}
shift_type_t;

/* SRG operation codes (manual page 7-55) */
typedef enum {
    SRG_RTS = 0x00, /* Return Section */
        SRG_XAE = 0x02, /* Exchange A and E */
        SRG_XAX = 0x04, /* Exchange A and X */
        SRG_XEX = 0x06, /* Exchange E and X */
        SRG_XAA = 0x08, /* Exchange bytes of A */
        SRG_CCE = 0x0A, /* Complement E */
        SRG_RSV = 0x0C, /* Return Supervisor */
        SRG_ACE = 0x0E, /* Add Carry to E */
        SRG_CCA = 0x10, /* Complement A */
        SRG_AEE = 0x12, /* A XOR E */
        SRG_CNX = 0x14, /* Copy Negative X */
        SRG_AIE = 0x16, /* A OR E */
        SRG_AAE = 0x18, /* A AND E */
        SRG_LNE = 0x1A, /* Load -1 into E */
        SRG_CNA = 0x1C, /* Copy Negative A */
        SRG_CHX = 0x1E /* Compute Half X */
}
srg_op_t;

/*
* Rough instruction binary layout
*
* Bits 0 to 3:	4 bits, addressing mode / class selector
* Bits 4 to 7:	4 bits, instruction opcode
* Bits 8 to 15:	8 bits, displacement

Bits 0-2 do not completely specify the address mode:
0	000:	DL, 
1	001:	P, DL, 
2	010:	DG, 
3	011:	IL,
4	100:	IGX, 
5	101:	ILX,
6	110:	RP, RM, IL, IG
7	111:	DG, IL, PX, P,
*/
t_stat one_inst(uint16 inst, uint16 pc, uint32 mode, uint16 * trappc) {
// t_stat one_inst (uint32 inst, uint32 pc, uint32 mode, uint32 *trappc)

    uint16 opcode = (inst >> I_OPCODE_SHIFT) & 0x1F;
    uint16 disp = inst & I_DISP_MASK;
    uint16 ea, data, data2, result;
    uint16 carry, overflow;
    int i, count;
    uint8 s_byte, d_byte;
    * trappc = pc;
    carry = 0;
    overflow = 0;

    MLOG_INST("[INST] P=%05o inst=%06o (opcode=%02o disp=%03o hexcode=%04X) mode=%s blk=%d\n",
              pc, inst, opcode, disp, inst & 0xF000, mode ? "MASTER" : "SLAVE", cpu_state.SuspensionStack[susp_stack_ptr].J_reg);
    MLOG_INST("  regs-before: A=%06o E=%06o X=%06o C=%d O=%d L=%05o G=%05o\n",
              cpu_state.reg_A, cpu_state.reg_E, cpu_state.reg_X,
              cpu_state.C, cpu_state.OV,
              cpu_state.reg_L, cpu_state.reg_G);
    
    /* Check for privileged instruction in slave mode
     * Privileged opcodes are: 0x3A (STR), 0x3B (LDP), 0x3D (TES), 
     *  0xF4 (SYS: STM, CLM, DIT, RD, WD), 0xEA (STR in PX) 
    */
    if (mode == 0) {  /* Slave mode */
        uint16 hexcode = inst & 0xF000;
        if ((hexcode == 0x3000 && (opcode == 0x0A || opcode == 0x0B || opcode == 0x0D)) ||
            (hexcode == 0xE000 && opcode == 0x0A) ||
            (hexcode == 0xF000 && opcode == 0x04)) {  /* SYS instructions */
            /* Check if it's a privileged SYS function */
            if (opcode == 0x04) {  /* SYS */
                if (disp == 0x01 || disp == 0x03 || disp == 0x08 || disp == 0x0C || disp == 0x20) {
                    /* DIT, WD, STM, CLM, DITR are privileged */
                    MLOG_INST("  ** privileged SYS function (disp=%03o) attempted in SLAVE mode -> TRAP_VM **\n", disp);
                    return mitra_trap(TRAP_VM, pc, trappc);
                }
            } else {
                MLOG_INST("  ** privileged instruction (opcode=%02o) attempted in SLAVE mode -> TRAP_VM **\n", opcode);
                return mitra_trap(TRAP_VM, pc, trappc);
            }
        }
    }
    
    uint16 hexcode = inst & 0xF000;

    // We don't care of address mode here, it's: A first layer of dispatcher
    switch (hexcode) {
        case 0x0000:
            group_1_DL(inst);
            break;
        case 0x1000:
            group_2_DL(inst, mode);
            break;
        case 0x2000:
            group_1_P(inst);
            break;
        case 0x3000:
            group_3_DL(inst, mode);
            break;
        case 0x4000:
            group_1_DG(inst);
            break;
        case 0x5000:
            group_2_DG(inst, mode);
            break;
        case 0x6000:
            group_1_IL(inst);
            break;
        case 0x7000:
            group_2_IL(inst, mode);
            break;
        case 0x8000:
            group_1_IGX(inst);
            break;
        case 0x9000:
            group_2_IGX(inst, mode);
            break;
        case 0xA000:
            group_1_ILX(inst);
            break;
        case 0xB000:
            group_2_ILX(inst, mode);
            break;
        case 0xC000: {
            uint16 Crpcode = inst & 0x0800;
            switch (Crpcode) {
                case 0x00:
                    group_4_RP(inst);
                    break;
                case 0x08:
                    group_4_RM(inst);
                    break;
            }
            break;
        }
        case 0xD000: {
            uint16 Drpcode = inst & 0x0800;
            switch (Drpcode) {
                case 0x00:
                    group_5_IL(inst);
                    break;
                case 0x08:
                    group_5_IG(inst);
                    break;
            }
            break;
        }
        case 0xE000:
            group_3_PX(inst, mode);
            break;
        case 0xF000:
            group_3_P(inst, mode);
            break;
    }
    
    MLOG_INST("  regs-after : P=%05o A=%06o E=%06o X=%06o C=%d O=%d L=%05o G=%05o\n",
              cpu_state.reg_P, cpu_state.reg_A, cpu_state.reg_E,
              cpu_state.reg_X, cpu_state.C, cpu_state.OV,
              cpu_state.reg_L, cpu_state.reg_G);

    /* Check for traps after instruction execution */
    if (cpu_state.trap_pending) {
        MLOG_INST("  -> cpu_state.trap_pending set (cause=%d) after execution, dispatching mitra_trap()\n", cpu_state.trap_cause);
        return mitra_trap(cpu_state.trap_cause, pc, trappc);
    }
    
    return SCPE_OK;
}

uint16 group_1_DL(uint16 inst) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      | 0 0  0 | 0| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *	
     *    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
     *    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
     *
     *    Byte, word or double-word, located in the first 256 bytes of the local segment.
     *    Y = (L) + D
     */
    uint16 op = inst >> 8; /* full 8-bit opcode byte              */
    uint16 disp = inst & I_DISP_MASK; /* 8-bit unsigned displacement / param */
    uint16 tmp;
    uint16 target_address;
    target_address = (cpu_state.reg_L + disp) & 0x7FFF;
    group_1(target_address, inst);
    return 0;
}

uint16 group_2_DL(uint16 inst, uint32 mode) {
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    target_address = (cpu_state.reg_L + disp) & 0x7FFF;
    group_2(inst, target_address, mode);
    return 0;
}

uint16 group_1_P(uint16 inst) {
    /*
    *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
    *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    *      | 0 0  1 | 0| x x  x  x |     displacement      |
    *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    *      (P addressing mode) 
    *    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
    *    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
    *
    * A byte operand is specified in the displacement field of the instruction. 
    * This byte may be extended on the left by 8 leading zeroes, if required.
    *
    *   Y = (P) parameter or immediate
    *
    */
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    target_address = (cpu_state.reg_P - 2) & 0x0FF;
    group_1(target_address, inst);
    return 0;
}

uint16 group_3_DL(uint16 inst, uint32 mode) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      | 0 0  1 | 1| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    
     *    "SHR", "SRG", "ICX", "DCX", "", "ICL", "DCL", "CSV",
     *    "CLS", "LDR", "STR", "LDP", "SHC", "TES", "",  "",
     *
     * Byte, word or double-word located in the first 256 bytes of the local segment.
     *  Y = (L) + D
     */
    uint16 opcode = (inst >> I_OPCODE_SHIFT);
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 data;
    uint16 target_address;
    uint16 count;
    target_address = (cpu_state.reg_L + disp) & 0x7FFF;

    switch (opcode) {
        case 0x30: {
            /* SHR - Shift Register (DL mode: shift word read from memory) */
            /* Shift word from memory (16-bit); use low byte for parameters.
             * Bit layout of shift parameter byte: bits[7:5]=type, bits[4:0]=count.
             * Verified: &23h=SRCS3, &E8h=SRCD8, &41h=SAD1. */
            uint8 shr_word = (uint8)(read_word(target_address) & 0xFF);
            shift_type_t type = (shr_word >> 5) & 0x07;
            count = shr_word & 0x1F;
            switch (type) {
                case SHIFT_SLLS:
                    cpu_state.reg_A = shift_lls(cpu_state.reg_A, count);
                    break;
                case SHIFT_SRCS:
                    cpu_state.reg_A = shift_srcs(cpu_state.reg_A, count);
                    break;
                case SHIFT_SAD:
                    shift_sad( & cpu_state.reg_E, & cpu_state.reg_A, count);
                    break;
                case SHIFT_SLCD:
                    shift_lcd( & cpu_state.reg_E, & cpu_state.reg_A, count);
                    break;
                case SHIFT_SLCS:
                    cpu_state.reg_A = shift_slcs(cpu_state.reg_A, count);
                    break;
                case SHIFT_SAS:
                    cpu_state.reg_A = shift_sas(cpu_state.reg_A, count);
                    break;
                case SHIFT_SRLS:
                    cpu_state.reg_A = shift_rls(cpu_state.reg_A, count);
                    break;
                case SHIFT_SRCD:
                    shift_rcd( & cpu_state.reg_E, & cpu_state.reg_A, count);
                    break;
            }
            set_condition_codes_load(cpu_state.reg_A);
        }
        break;
        case 0x31: {
            srg_op_t srg_op = disp & 0x1E;
            switch (srg_op) {
                case SRG_RTS: {
                    uint16 saved_P = read_word(cpu_state.reg_L) + GPRIME;
                    uint16 saved_L = read_word(cpu_state.reg_L + 2) + GPRIME;
                    cpu_state.reg_P = saved_P;
                    cpu_state.reg_L = saved_L;
                    break;
                }
                case SRG_XAE:
                    data = cpu_state.reg_A;
                    cpu_state.reg_A = cpu_state.reg_E;
                    cpu_state.reg_E = data;
                    break;
                case SRG_XAX:
                    data = cpu_state.reg_A;
                    cpu_state.reg_A = cpu_state.reg_X;
                    cpu_state.reg_X = data;
                    break;
                case SRG_XEX:
                    data = cpu_state.reg_E;
                    cpu_state.reg_E = cpu_state.reg_X;
                    cpu_state.reg_X = data;
                    break;
                case SRG_XAA:
                    cpu_state.reg_A = ((cpu_state.reg_A & 0xFF) << 8) | ((
                        cpu_state.reg_A >> 8) & 0xFF);
                    break;
                case SRG_CCE:
                    cpu_state.reg_E = ~cpu_state.reg_E & 0xFFFF;
                    break;
                case SRG_RSV:
                    if (mode != 1) return MM_PRVINS;
                    {
                        uint16 saved_flags = read_word(cpu_state.reg_G + 4);
                        cpu_state.C = (saved_flags >> 14) & 1;
                        cpu_state.OV = (saved_flags >> 13) & 1;
                        cpu_state.MS = 0;
                        cpu_state.MA = (saved_flags >> 12) & 1;
                        cpu_state.PR = (saved_flags >> 11) & 1;
                    }
                    cpu_state.reg_L = (cpu_state.reg_G + read_word(cpu_state.reg_G + 2)) & 0x7FFF;
                    cpu_state.reg_P = (cpu_state.reg_G + 2 + read_word(cpu_state.reg_G)) & 0x7FFF;
                    break;
                case SRG_ACE:
                    cpu_state.reg_E = (cpu_state.reg_E + cpu_state.C) &
                        0xFFFF;
                    break;
                case SRG_CCA:
                    cpu_state.reg_A = ~cpu_state.reg_A & 0xFFFF;
                    set_condition_codes_load(cpu_state.reg_A);
                    break;
                case SRG_AEE:
                    cpu_state.reg_A ^= cpu_state.reg_E;
                    set_condition_codes_load(cpu_state.reg_A);
                    break;
                case SRG_CNX:
                    cpu_state.reg_X = (~cpu_state.reg_X + 1) & 0xFFFF;
                    break;
                case SRG_AIE:
                    cpu_state.reg_A |= cpu_state.reg_E;
                    set_condition_codes_load(cpu_state.reg_A);
                    break;
                case SRG_AAE:
                    cpu_state.reg_A &= cpu_state.reg_E;
                    set_condition_codes_load(cpu_state.reg_A);
                    break;
                case SRG_LNE:
                    cpu_state.reg_E = 0xFFFF;
                    break;
                case SRG_CNA:
                    cpu_state.reg_A = (~cpu_state.reg_A + 1) & 0xFFFF;
                    set_condition_codes_load(cpu_state.reg_A);
                    break;
                case SRG_CHX:
                    cpu_state.reg_X = (cpu_state.reg_X >> 1) | (cpu_state.reg_X & 0x8000);
                    break;
                default:
                    break;
            }
        }
        break;
        case 0x32:
            cpu_state.reg_X = (cpu_state.reg_X + read_word(target_address)) &
                0x7FFF;
            set_condition_codes_load(cpu_state.reg_X);
            break;
        case 0x33:
            cpu_state.reg_X = (cpu_state.reg_X - read_word(target_address)) &
                0x7FFF;
            set_condition_codes_load(cpu_state.reg_X);
            break;
        case 0x34:
            break;
        case 0x35:
            cpu_state.reg_L = (cpu_state.reg_L + read_word(target_address)) &
                0x7FFF;
            break;
        case 0x36:
            cpu_state.reg_L = (cpu_state.reg_L - read_word(target_address)) &
                0x7FFF;
            break;
        case 0x37: {
            uint16 section = target_address;
            write_word(cpu_state.reg_G, cpu_state.reg_P - GPRIME);
            write_word(cpu_state.reg_G + 2, cpu_state.reg_L - GPRIME);
            write_word(cpu_state.reg_G + 4, (cpu_state.C ? 1 : 0) | (
                cpu_state.OV ? 2 : 0) | (cpu_state.MS ? 4 : 0));
            uint16 PRTS_addr = read_word(12);
            cpu_state.reg_L = ((PRTS_addr - (4 * section)) + cpu_state.reg_G) &
                0x7FFF;
            cpu_state.reg_P = ((PRTS_addr - (4 * section) + 2) + cpu_state.reg_G) & 0x7FFF;
            cpu_state.MS = 1;
            cpu_state.PR = 1;
        }
        break;
        case 0x38: {
            uint16 section = target_address;
            uint16 called_Lbase = read_word((cpu_state.reg_G - 4 * section + 2) &
            0x7FFF);
            uint16 called_Pbase = read_word((cpu_state.reg_G - 4 * section) & 0x7FFF);
            uint16 LDS = (called_Lbase + cpu_state.reg_G) & 0x7FFF;
            write_word(LDS, (cpu_state.reg_P - GPRIME) & 0x7FFF);
            write_word(LDS + 2, (cpu_state.reg_L - GPRIME) & 0x7FFF);
            cpu_state.reg_L = LDS;
            cpu_state.reg_P = (called_Pbase + cpu_state.reg_G) & 0x7FFF;
        }
        break;
        case 0x39: {
            uint16 reg_num = target_address & 0x3F;
            switch (reg_num & 0x07) {
                case 0:
                    cpu_state.reg_A = cpu_state.reg_A;
                    break;
                case 1:
                    cpu_state.reg_A = cpu_state.reg_E;
                    break;
                case 2:
                    cpu_state.reg_A = cpu_state.reg_P;
                    break;
                case 3:
                    cpu_state.reg_A = cpu_state.reg_X;
                    break;
                case 4:
                    cpu_state.reg_A = cpu_state.reg_L;
                    break;
                case 5:
                    cpu_state.reg_A = cpu_state.reg_G;
                    break;
                default:
                    break;
            }
            set_condition_codes_load(cpu_state.reg_A);
        }
        break;
        case 0x3A:
            if (mode != 1) return MM_PRVINS;
            {
                uint16 reg_num = target_address & 0x3F;
                switch (reg_num & 0x07) {
                    case 0:
                        cpu_state.reg_A = cpu_state.reg_A;
                        break;
                    case 1:
                        cpu_state.reg_A = cpu_state.reg_A;
                        break;
                    case 2:
                        cpu_state.reg_P = cpu_state.reg_A & 0x7FFF;
                        break;
                    case 3:
                        cpu_state.reg_X = cpu_state.reg_A;
                        break;
                    case 4:
                        cpu_state.reg_L = cpu_state.reg_A & 0x7FFF;
                        break;
                    case 5:
                        cpu_state.reg_G = cpu_state.reg_A;
                        break;
                    default:
                        break;
                }
            }
            break;
        case 0x3B:
            if (mode != 1)
                return MM_PRVINS;
            cpu_state.PR = read_word(target_address) & 1;
            break;
        case 0x3C: {
            uint8 shc_word = (uint8)(read_word(target_address) & 0xFF);
            uint8 shc_type = (shc_word >> 5) & 0x07;
            count = shc_word & 0x1F;
            switch (shc_type) {
                case 0:
                    shift_lld( & cpu_state.reg_E, & cpu_state.reg_A, count);
                    break;
                case 1:
                    if (mode != 1) return MM_PRVINS;
                    if (!(cpu_unit.flags & UNIT_HSINT))
                        return MM_INVINS;
                    cpu_state.intrpt_mask &= ~(1u << cpu_state.int_lvl);
                    break;
                case 2:
                    cpu_state.reg_E = compute_parity( & cpu_state.reg_A, count);
                    break;
                case 3:
                    if (mode != 1) return MM_PRVINS;
                    cpu_state.intrpt_mask &= ~(1u << cpu_state.int_lvl);
                    cpu_state.int_lvl = 0;
                    break;
                case 4:
                    shift_rld( & cpu_state.reg_E, & cpu_state.reg_A, count);
                    break;
                case 5:
                    break;
                case 6:
                    normalize( & cpu_state.reg_E, & cpu_state.reg_A, & cpu_state.reg_X, count);
                    break;
                case 7:
                    break;
            }
            set_condition_codes_load(cpu_state.reg_A);
        }
        break;
        case 0x3D:
            if (mode != 1) return MM_PRVINS;
            cpu_state.reg_A = read_word(target_address);
            write_word(target_address, 0);
            set_condition_codes_load(cpu_state.reg_A);
            break;
        case 0x3E:
        case 0x3F:
            break;
    }
    return 0;
}

uint16 group_1_DG(uint16 inst) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      | 0 1  0 | 0| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    (DG addressing mode) 
     *    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
     *    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
     *
     * Byte, word or double-word located in the first 256 bytes of the common segment.
     *  Y=(G) + D
     */
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    target_address = (cpu_state.reg_G + disp) & 0x7FFF;
    group_1(target_address, inst);
    return 0;
}

uint16 group_2_DG(uint16 inst, uint32 mode) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      | 0 1  0 | 1| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    
     *    "DLD", "STA", "STE", "STX", "SBL", "SBR", "DST", "ADM",
     *    "SPA", "STS", "FAD", "FSU", "FMU", "FDV", "TRS", "MVS",
     *
     * Byte, word or double-word located in the first 256 bytes of the common segment.
     *  Y=(G) + D
     */
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    target_address = (cpu_state.reg_G + disp) & 0x7FFF;
    group_2(inst, target_address, mode);
    return 0;
}

uint16 group_1_IL(uint16 inst) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      | 0 1  1 | 0| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    (IL addressing mode) 
     *    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
     *    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
     *
     *  Y = (G' + mem[L + D]) & 0x7FFF
     *  Byte, word or double-word located anywhere and pointed at through the local segment.
     */
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    tmp = read_word(cpu_state.reg_L + disp);
    target_address = (GPRIME + tmp) & 0x7FFF;
    group_1(target_address, inst);
    return 0;
}

uint16 group_2_IL(uint16 inst, uint32 mode) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      | 0 1  1 | 1| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    
     *    "DLD", "STA", "STE", "STX", "SBL", "SBR", "DST", "ADM",
     *    "SPA", "STS", "FAD", "FSU", "FMU", "FDV", "TRS", "MVS",
     *
     *  Y = (G' + mem[L + D]) & 0x7FFF
     *  Byte, word or double-word located anywhere and pointed at through the local segment.
     */
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    tmp = read_word(cpu_state.reg_L + disp);
    uint16 target_address;
    target_address = (GPRIME + tmp) & 0x7FFF;
    group_2(inst, target_address, mode);
    return 0;
}

uint16 group_1_IGX(uint16 inst) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  0  0 | 0| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    (IGX addressing mode)
     *    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
     *    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
     *
     * Element of an array pointed at through the common segment.
     *  Y = (G) + ((G)+D) + (cpu_state.reg_X)
     */
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    tmp = read_word(cpu_state.reg_G + disp);
    target_address = (cpu_state.reg_G + tmp + cpu_state.reg_X) & 0x7FFF;
    group_1(target_address, inst);
    return 0;
}

uint16 group_2_IGX(uint16 inst, uint32 mode) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  0  0 | 1| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    
     *    "DLD", "STA", "STE", "STX", "SBL", "SBR", "DST", "ADM",
     *    "SPA", "STS", "FAD", "FSU", "FMU", "FDV", "TRS", "MVS",
     *
     * Element of an array pointed at through the common segment.
     *  Y = (G) + ((G)+D) + (cpu_state.reg_X)
     */
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    tmp = read_word(cpu_state.reg_G + disp);
    target_address = (cpu_state.reg_G + tmp + cpu_state.reg_X) & 0x7FFF;
    group_2(inst, target_address, mode);
    return 0;
}

uint16 group_1_ILX(uint16 inst) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  0  1 | 0| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    
     *    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
     *    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
     *
     * Element of cpu_state.reg_A byte, word or double-word array located anywhere and pointed at through the local segment.
     *  Y = G' + ((L)+D)+(cpu_state.reg_X)
     */
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    tmp = read_word(cpu_state.reg_L + disp);
    target_address = (GPRIME + tmp + cpu_state.reg_X) & 0x7FFF;
    group_1(target_address, inst);
    return 0;
}

uint16 group_2_ILX(uint16 inst, uint32 mode) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  0  1 | 1| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    
     *    "DLD", "STA", "STE", "STX", "SBL", "SBR", "DST", "ADM",
     *    "SPA", "STS", "FAD", "FSU", "FMU", "FDV", "TRS", "MVS",
     *
     * Element of a byte, word or double-word array located anywhere and pointed at through the local segment.
     *  Y = G' + ((L)+D)+(X)
     */
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    tmp = read_word(cpu_state.reg_L + disp);
    target_address = (GPRIME + tmp + cpu_state.reg_X) & 0x7FFF;
    group_2(inst, target_address, mode);
    return 0;
}

uint16 group_4_RP(uint16 inst) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  1  0 | 0| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    (Class 2 - RP addressing mode)
     *    "BCT", "BRX", "BOT", "BCF", "BAN", "BAZ", "BOF", "BRU",
     *
     * Y={P)+2D
     */
    uint16 opcode = (inst >> I_OPCODE_SHIFT);
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    target_address = cpu_state.reg_P + (disp << 1);
    switch (opcode) {
        case 0xC0:
            /* BCT - Branch on Carry True (RP mode) */
            if (cpu_state.C) cpu_state.reg_P = target_address;
            else cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xC1:
            /* BRX - Branch Indexed (RP mode) */
            cpu_state.reg_P = (target_address + cpu_state.reg_X) & 0x7FFF;
            break;
        case 0xC2:
            /* BOT - Branch on Overflow True (RP mode) */
            if (cpu_state.OV) cpu_state.reg_P = target_address;
            else cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xC3:
            /* BCF - Branch on Carry False (RP mode) */
            if (!cpu_state.C) cpu_state.reg_P = target_address;
            else cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xC4:
            /* BAN - Branch on cpu_state.reg_A Negative (RP mode) */
            if (cpu_state.reg_A & 0x8000)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xC5:
            /* BAZ - Branch on cpu_state.reg_A Zero (RP mode) */
            if (cpu_state.reg_A == 0)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xC6:
            /* BOF - Branch on Overflow False (RP mode) */
            if (!cpu_state.OV)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xC7:
            /* BRU - Branch Unconditional (RP mode) */
            cpu_state.reg_P = target_address;
            break;
    }
}

uint16 group_4_RM(uint16 inst) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  1  0 | 0| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    (Class 2 - RM addressing mode)
     *    "BCT", "BRX", "BOT", "BCF", "BAN", "BAZ", "BOF", "BRU",
     */
    uint16 opcode = (inst >> I_OPCODE_SHIFT);
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    target_address = cpu_state.reg_P - (disp << 1);
    switch (opcode) {
        case 0xC8:
            if (cpu_state.C) cpu_state.reg_P = target_address;
            else cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xC9:
            cpu_state.reg_P = (target_address + cpu_state.reg_X) & 0x7FFF;
            break;
        case 0xCA:
            if (cpu_state.OV) cpu_state.reg_P = target_address;
            else cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xCB:
            if (!cpu_state.C) cpu_state.reg_P = target_address;
            else cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xCC:
            if (cpu_state.reg_A & 0x8000)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xCD:
            if (cpu_state.reg_A == 0)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xCE:
            if (!cpu_state.OV) cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xCF:
            cpu_state.reg_P = target_address;
            break;
    }
    return 0;
}

uint16 group_5_IL(uint16 inst) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  1  0 | 1| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    (Class 2 - RM/IL/IG addressing modes)
     *    "BCT", "BRX", "BOT", "BCF", "BAN", "BAZ", "BOF", "BRU",
     *
     * Y = G' + ((L) + D)
     */
    uint16 opcode = (inst >> I_OPCODE_SHIFT);
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    tmp = (cpu_state.reg_L + disp) & 0x7FFF;
    target_address = GPRIME + tmp;
    switch (opcode) {
        case 0xD0:
            if (cpu_state.C)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xD1:
            cpu_state.reg_P = (target_address + cpu_state.reg_X) & 0x7FFF;
            break;
        case 0xD2:
            if (cpu_state.OV)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xD3:
            if (!cpu_state.C)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xD4:
            if (cpu_state.reg_A & 0x8000)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xD5:
            if (cpu_state.reg_A == 0)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xD6:
            if (!cpu_state.OV)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xD7:
            cpu_state.reg_P = target_address;
            break;
    }
    return 0;
}

uint16 group_5_IG(uint16 inst) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  1  0 | 1| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    (Class 2 - RM/IL/IG addressing modes)
     *    "BCT", "BRX", "BOT", "BCF", "BAN", "BAZ", "BOF", "BRU",
     *
     * Y = G'+((G)+D)
     */
    uint16 opcode = (inst >> I_OPCODE_SHIFT);
    uint16 op = inst >> 8;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    tmp = read_word(cpu_state.reg_G + disp);
    target_address = (GPRIME + tmp) & 0x7FFF;
    switch (opcode) {
        case 0xD8:
            if (cpu_state.C) cpu_state.reg_P = target_address;
            else cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xD9:
            cpu_state.reg_P = (target_address + cpu_state.reg_X) & 0x7FFF;
            break;
        case 0xDA:
            if (cpu_state.OV) cpu_state.reg_P = target_address;
            else cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xDB:
            if (!cpu_state.C) cpu_state.reg_P = target_address;
            else cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xDC:
            if (cpu_state.reg_A & 0x8000) cpu_state.reg_P = target_address;
            else cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xDD:
            if (cpu_state.reg_A == 0) cpu_state.reg_P = target_address;
            else cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xDE:
            if (!cpu_state.OV) cpu_state.reg_P = target_address;
            else cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xDF:
            cpu_state.reg_P = target_address;
            break;
    }
    return 0;
}

uint16 group_3_PX(uint16 inst, uint32 mode) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  1  1 | 0| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    (Class 1 - PX addressing mode)
     *    "SHR", "SRG", "ICX", "DCX", "",    "ICL", "DCL", "CSV",
     *
     * (Y) = D+(cpu_state.reg_X)
     *  Y = (P)
     */
    uint16 opcode = (inst >> I_OPCODE_SHIFT);
    uint16 op = inst >> 8;
    uint16 count;
    uint16 data;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    target_address = (disp) & 0x0FF;
    switch (opcode) {
        case 0xE0: {
            shift_type_t type = (disp >> 5) & 0x07;
            count = disp & 0x1F;
            switch (type) {
                case SHIFT_SLLS:
                    cpu_state.reg_A = shift_lls(cpu_state.reg_A, count);
                    break;
                case SHIFT_SRCS:
                    cpu_state.reg_A = shift_srcs(cpu_state.reg_A, count);
                    break;
                case SHIFT_SAD:
                    shift_sad( & cpu_state.reg_E, & cpu_state.reg_A, count);
                    break;
                case SHIFT_SLCD:
                    shift_lcd( & cpu_state.reg_E, & cpu_state.reg_A, count);
                    break;
                case SHIFT_SLCS:
                    cpu_state.reg_A = shift_slcs(cpu_state.reg_A, count);
                    break;
                case SHIFT_SAS:
                    cpu_state.reg_A = shift_sas(cpu_state.reg_A, count);
                    break;
                case SHIFT_SRLS:
                    cpu_state.reg_A = shift_rls(cpu_state.reg_A, count);
                    break;
                case SHIFT_SRCD:
                    shift_rcd( & cpu_state.reg_E, & cpu_state.reg_A, count);
                    break;
            }
            set_condition_codes_load(cpu_state.reg_A);
        }
        break;
        case 0xE1: {
            srg_op_t srg_op = disp & 0x1E;
            switch (srg_op) {
                case SRG_RTS:
                    uint16 saved_P = read_word(cpu_state.reg_L) + GPRIME;
                    uint16 saved_L = read_word(cpu_state.reg_L + 2) + GPRIME;
                    cpu_state.reg_P = saved_P;
                    cpu_state.reg_L = saved_L;
                    break;
                case SRG_XAE:
                    data = cpu_state.reg_A;
                    cpu_state.reg_A = cpu_state.reg_E;
                    cpu_state.reg_E = data;
                    break;
                case SRG_XAX:
                    data = cpu_state.reg_A;
                    cpu_state.reg_A = cpu_state.reg_X;
                    cpu_state.reg_X = data;
                    break;
                case SRG_XEX:
                    data = cpu_state.reg_E;
                    cpu_state.reg_E = cpu_state.reg_X;
                    cpu_state.reg_X = data;
                    break;
                case SRG_XAA:
                    cpu_state.reg_A = ((cpu_state.reg_A & 0xFF) << 8) | ((
                        cpu_state.reg_A >> 8) & 0xFF);
                    break;
                case SRG_CCE:
                    cpu_state.reg_A = ~cpu_state.reg_E & 0xFFFF;
                    break;
                case SRG_RSV:
                    if (mode != 1) return MM_PRVINS;
                    {
                        uint16 saved_flags = read_word(cpu_state.reg_G + 4);
                        cpu_state.C = (saved_flags >> 14) & 1;
                        cpu_state.OV = (saved_flags >> 13) & 1;
                        cpu_state.MA = (saved_flags >> 12) & 1;
                        cpu_state.PR = (saved_flags >> 11) & 1;
                        cpu_state.MS = 0;
                    }
                    cpu_state.reg_L = (cpu_state.reg_G + read_word(cpu_state.reg_G + 2)) & 0x7FFF;
                    cpu_state.reg_P = (cpu_state.reg_G + 2 + read_word(cpu_state.reg_G)) & 0x7FFF;
                    break;
                case SRG_ACE:
                    cpu_state.reg_A = (cpu_state.reg_E + cpu_state.C) &
                        0xFFFF;
                    break;
                case SRG_CCA:
                    cpu_state.reg_A = ~cpu_state.reg_A & 0xFFFF;
                    set_condition_codes_load(cpu_state.reg_A);
                    break;
                case SRG_AEE:
                    cpu_state.reg_A ^= cpu_state.reg_E;
                    set_condition_codes_load(cpu_state.reg_A);
                    break;
                case SRG_CNX:
                    cpu_state.reg_X = (~cpu_state.reg_X + 1) & 0xFFFF;
                    break;
                case SRG_AIE:
                    cpu_state.reg_A |= cpu_state.reg_E;
                    set_condition_codes_load(cpu_state.reg_A);
                    break;
                case SRG_AAE:
                    cpu_state.reg_A &= cpu_state.reg_E;
                    set_condition_codes_load(cpu_state.reg_A);
                    break;
                case SRG_LNE:
                    cpu_state.reg_A = 0xFFFF;
                    break;
                case SRG_CNA:
                    cpu_state.reg_A = (~cpu_state.reg_A + 1) & 0xFFFF;
                    set_condition_codes_load(cpu_state.reg_A);
                    break;
                case SRG_CHX:
                    cpu_state.reg_X = (cpu_state.reg_X >> 1) | (cpu_state.reg_X & 0x8000);
                    break;
                default:
                    break;
            }
        }
        break;
        case 0xE2:
            cpu_state.reg_X = (cpu_state.reg_X + target_address) & 0x7FFF;
            set_condition_codes_load(cpu_state.reg_X);
            break;
        case 0xE3:
            cpu_state.reg_X = (cpu_state.reg_X - target_address) & 0x7FFF;
            set_condition_codes_load(cpu_state.reg_X);
            break;
        case 0xE4:
            break;
        case 0xE5:
            cpu_state.reg_L = (cpu_state.reg_L + target_address) & 0x7FFF;
            break;
        case 0xE6:
            cpu_state.reg_L = (cpu_state.reg_L - target_address) & 0x7FFF;
            break;
        case 0xE7: {
            uint16 section = target_address;
            write_word(cpu_state.reg_G, cpu_state.reg_P - GPRIME);
            write_word(cpu_state.reg_G + 2, cpu_state.reg_L - GPRIME);
            write_word(cpu_state.reg_G + 4, (cpu_state.C ? 1 : 0) | (
                cpu_state.OV ? 2 : 0) | (cpu_state.MS ? 4 : 0));
            uint16 PRTS_addr = read_word(12);
            cpu_state.reg_L = ((PRTS_addr - (4 * section)) + cpu_state.reg_G) &
                0x7FFF;
            cpu_state.reg_P = ((PRTS_addr - (4 * section) + 2) + cpu_state.reg_G) & 0x7FFF;
            cpu_state.MS = 1;
            cpu_state.PR = 1;
        }
        break;
    }
    return 0;
}

        /* ========== P Mode System Instructions (F0-FF) ========== */
        // FIXME many bugs
uint16 group_3_P(uint16 inst, uint32 mode) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  1  1 | 0| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    (Class 1 - P addressing mode)
     *    "CLS", "LDR", "STR", "LDP", "SHC", "TES", "",    "",
     */
    uint16 opcode = (inst >> I_OPCODE_SHIFT);
    uint16 op = inst >> 8;
    uint16 count;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    target_address = disp;
    uint16 data;
    
    if(opcode == 0xF4) {
	    // Bits 14, 15 decode 4 intructions: STM, DIT, RD, WD
	    uint16 MasterInst = inst & 0x0003;
	    switch (MasterInst) {
		case 0x00:  /* STM - Set Interrupt Mask */
		        cpu_state.MA = 1;
		        break;
		            
		case 0x01:  /* DIT - Deactivate Interrupt */
                    {
                /* Save context at context pointer address */
                uint16 ctx_ptr = int_vec[cpu_state.int_lvl];
                write_word(ctx_ptr, ((cpu_state.C ? 1 : 0) | (cpu_state.OV ? 2 : 0) | (cpu_state.MS ? 4 : 0)));
                write_word(ctx_ptr + 2, cpu_state.reg_X);
                write_word(ctx_ptr + 4, cpu_state.reg_E);
                write_word(ctx_ptr + 6, cpu_state.reg_A);
                write_word(ctx_ptr + 8, cpu_state.reg_G);
                write_word(ctx_ptr + 10, cpu_state.reg_L);
                write_word(ctx_ptr + 12, cpu_state.reg_P);
                /* Deactivate current level */
                cpu_state.intrpt_mask &= ~(1u << cpu_state.int_lvl);
                /* Find next highest priority level */
                cpu_state.int_lvl = get_highest_interrupt();
                /* Restore context */
                ctx_ptr = int_vec[cpu_state.int_lvl];
                {
                    uint16 saved_flags = read_word(ctx_ptr);
                    cpu_state.C = (saved_flags >> 0) & 1;
                    cpu_state.OV = (saved_flags >> 1) & 1;
                    cpu_state.MS = (saved_flags >> 2) & 1;
                }
                cpu_state.reg_X = read_word(ctx_ptr + 2);
                cpu_state.reg_E = read_word(ctx_ptr + 4);
                cpu_state.reg_A = read_word(ctx_ptr + 6);
                cpu_state.reg_G = read_word(ctx_ptr + 8);
                cpu_state.reg_L = read_word(ctx_ptr + 10);
                cpu_state.reg_P = read_word(ctx_ptr + 12);
                    }
                    break;
                    
        case 0x02:
		/*** RD 
		bits 8 to 13 undefined
		bits 14, 14 = 10
		The opcode is 0xF402

		E register:
		Bits 3 to 6 and 12 to 15 are the I/O address
		Bits 10 and 11, are reading mode
		***/
		break;
	
    	case 0x03:
		/**** WD 
		bits 8 to 13 undefined
		bits 14, 15 = 11
		The opcode is 0xF403

		E register:
		Bits 3 to 6 and 12 to 15 are the I/O address
		Bits 10 and 11, are writing mode
		***/
		break;
		}
	}
    
    shift_type_t type = (disp >> 5) & 0x07;
    count = disp & 0x1F;
    
    switch (opcode) {
        case 0xF0: {
                case SHIFT_SLLS:
                    cpu_state.reg_A = shift_lls(cpu_state.reg_A, count);
                    break;
                case SHIFT_SRCS:
                    cpu_state.reg_A = shift_srcs(cpu_state.reg_A, count);
                    break;
                case SHIFT_SAD:
                    shift_sad( & cpu_state.reg_E, & cpu_state.reg_A, count);
                    break;
                case SHIFT_SLCD:
                    shift_lcd( & cpu_state.reg_E, & cpu_state.reg_A, count);
                    break;
                case SHIFT_SLCS:
                    cpu_state.reg_A = shift_slcs(cpu_state.reg_A, count);
                    break;
                case SHIFT_SAS:
                    cpu_state.reg_A = shift_sas(cpu_state.reg_A, count);
                    break;
                case SHIFT_SRLS:
                    cpu_state.reg_A = shift_rls(cpu_state.reg_A, count);
                    break;
                case SHIFT_SRCD:
                    shift_rcd( & cpu_state.reg_E, & cpu_state.reg_A, count);
                    break;
            }
            set_condition_codes_load(cpu_state.reg_A);
        break;
        
        case 0xF1: {
            srg_op_t srg_op = disp & 0x1E;
            switch (srg_op) {
                case SRG_RTS:
                    uint16 saved_P = read_word(cpu_state.reg_L) + GPRIME;
                    uint16 saved_L = read_word(cpu_state.reg_L + 2) + GPRIME;
                    cpu_state.reg_P = saved_P;
                    cpu_state.reg_L = saved_L;
                    break;
                case SRG_XAE:
                    data = cpu_state.reg_A;
                    cpu_state.reg_A = cpu_state.reg_E;
                    cpu_state.reg_E = data;
                    break;
                case SRG_XAX:
                    data = cpu_state.reg_A;
                    cpu_state.reg_A = cpu_state.reg_X;
                    cpu_state.reg_X = data;
                    break;
                case SRG_XEX:
                    data = cpu_state.reg_E;
                    cpu_state.reg_E = cpu_state.reg_X;
                    cpu_state.reg_X = data;
                    break;
                case SRG_XAA:
                    cpu_state.reg_A = ((cpu_state.reg_A & 0xFF) << 8) | ((
                cpu_state.reg_A >> 8) & 0xFF);
                    break;
                case SRG_CCE:
                    cpu_state.reg_A = ~cpu_state.reg_E & 0xFFFF;
                    break;
                case SRG_RSV:
                    if (mode != 1) return MM_PRVINS;
                    {
                uint16 saved_flags = read_word(cpu_state.reg_G + 4);
                cpu_state.C = (saved_flags >> 14) & 1;
                cpu_state.OV = (saved_flags >> 13) & 1;
                cpu_state.MA = (saved_flags >> 12) & 1;
                cpu_state.PR = (saved_flags >> 11) & 1;
                cpu_state.MS = 0;
                    }
                    cpu_state.reg_L = (cpu_state.reg_G + read_word(cpu_state.reg_G + 2)) & 0x7FFF;
                    cpu_state.reg_P = (cpu_state.reg_G + 2 + read_word(cpu_state.reg_G)) & 0x7FFF;
                    break;
                case SRG_ACE:
                    cpu_state.reg_A = (cpu_state.reg_E + cpu_state.C) &
                0xFFFF;
                    break;
                case SRG_CCA:
                    cpu_state.reg_A = ~cpu_state.reg_A & 0xFFFF;
                    set_condition_codes_load(cpu_state.reg_A);
                    break;
                case SRG_AEE:
                    cpu_state.reg_A ^= cpu_state.reg_E;
                    set_condition_codes_load(cpu_state.reg_A);
                    break;
                case SRG_CNX:
                    cpu_state.reg_X = (~cpu_state.reg_X + 1) & 0xFFFF;
                    break;
                case SRG_AIE:
                    cpu_state.reg_A |= cpu_state.reg_E;
                    set_condition_codes_load(cpu_state.reg_A);
                    break;
                case SRG_AAE:
                    cpu_state.reg_A &= cpu_state.reg_E;
                    set_condition_codes_load(cpu_state.reg_A);
                    break;
                case SRG_LNE:
                    cpu_state.reg_A = 0xFFFF;
                    break;
                case SRG_CNA:
                    cpu_state.reg_A = (~cpu_state.reg_A + 1) & 0xFFFF;
                    set_condition_codes_load(cpu_state.reg_A);
                    break;
                case SRG_CHX:
                    cpu_state.reg_X = (cpu_state.reg_X >> 1) | (cpu_state.reg_X & 0x8000);
                    break;
                default:
                    break;
            }
        }
        break;
        case 0xF2:
            cpu_state.reg_X = (cpu_state.reg_X + target_address) & 0x7FFF;
            set_condition_codes_load(cpu_state.reg_X);
            break;
        case 0xF3:
            cpu_state.reg_X = (cpu_state.reg_X - target_address) & 0x7FFF;
            set_condition_codes_load(cpu_state.reg_X);
            break;
        case 0xF4:
            break;
        case 0xF5:
            cpu_state.reg_L = (cpu_state.reg_L + target_address) & 0x7FFF;
            break;
        case 0xF6:
            cpu_state.reg_L = (cpu_state.reg_L - target_address) & 0x7FFF;
            break;
        case 0xF7: {
            uint16 section = target_address;
            write_word(cpu_state.reg_G, cpu_state.reg_P - GPRIME);
            write_word(cpu_state.reg_G + 2, cpu_state.reg_L - GPRIME);
            write_word(cpu_state.reg_G + 4, (cpu_state.C ? 1 : 0) | (
                cpu_state.OV ? 2 : 0) | (cpu_state.MS ? 4 : 0));
            uint16 PRTS_addr = read_word(12);
            cpu_state.reg_L = ((PRTS_addr - (4 * section)) + cpu_state.reg_G) &
                0x7FFF;
            cpu_state.reg_P = ((PRTS_addr - (4 * section) + 2) + cpu_state.reg_G) & 0x7FFF;
            cpu_state.MS = 1;
            cpu_state.PR = 1;
        }
        break;
        
        case 0xF8: {
            uint16 section = target_address;
            uint16 called_Lbase = read_word((cpu_state.reg_G - 4 * section + 2) &
            0x7FFF);
            uint16 called_Pbase = read_word((cpu_state.reg_G - 4 * section) & 0x7FFF);
            uint16 LDS = (called_Lbase + cpu_state.reg_G) & 0x7FFF;
            write_word(LDS, (cpu_state.reg_P - GPRIME) & 0x7FFF);
            write_word(LDS + 2, (cpu_state.reg_L - GPRIME) & 0x7FFF);
            cpu_state.reg_L = LDS;
            cpu_state.reg_P = (called_Pbase + cpu_state.reg_G) & 0x7FFF;
        }
        break;
        case 0xF9: {
            uint16 reg_num = target_address & 0x3F;
            switch (reg_num & 0x07) {
                case 0:
                    cpu_state.reg_A = cpu_state.reg_A;
                    break;
                case 1:
                    cpu_state.reg_A = cpu_state.reg_E;
                    break;
                case 2:
                    cpu_state.reg_A = cpu_state.reg_P;
                    break;
                case 3:
                    cpu_state.reg_A = cpu_state.reg_X;
                    break;
                case 4:
                    cpu_state.reg_A = cpu_state.reg_L;
                    break;
                case 5:
                    cpu_state.reg_A = cpu_state.reg_G;
                    break;
                default:
                    break;
            }
            set_condition_codes_load(cpu_state.reg_A);
        }
        break;
        case 0xFA:
            if (mode != 1) return MM_PRVINS;
            {
                uint16 reg_num = target_address & 0x3F;
                switch (reg_num & 0x07) {
                    case 0:
                        cpu_state.reg_A = cpu_state.reg_A;
                        break;
                    case 1:
                        cpu_state.reg_A = cpu_state.reg_A;
                        break;
                    case 2:
                        cpu_state.reg_P = cpu_state.reg_A & 0x7FFF;
                        break;
                    case 3:
                        cpu_state.reg_X = cpu_state.reg_A;
                        break;
                    case 4:
                        cpu_state.reg_L = cpu_state.reg_A & 0x7FFF;
                        break;
                    case 5:
                        cpu_state.reg_G = cpu_state.reg_A;
                        break;
                    default:
                        break;
                }
            }
            break;
        case 0xFB:
            if (mode != 1) return MM_PRVINS;
            cpu_state.PR = read_word(target_address) & 1;
            break;
        case 0xFC:
            count = disp & 0x1F;
            {
                uint8 shc_type = (disp >> 5) & 0x07;
                switch (shc_type) {
                    case 0:
                        shift_lld( & cpu_state.reg_E, & cpu_state.reg_A, count);
                        break;
                    case 1:
                        if (mode != 1) return MM_PRVINS;
                        cpu_state.intrpt_mask &= ~(1u << cpu_state.int_lvl);
                        cpu_state.int_lvl = 0;
                        break;
                    case 2:
                        cpu_state.reg_A = compute_parity( & cpu_state.reg_A,
                            count);
                        break;
                    case 3:
                        if (mode != 1) return MM_PRVINS;
                        cpu_state.intrpt_mask &= ~(1u << cpu_state.int_lvl);
                        cpu_state.int_lvl = 0;
                        break;
                    case 4:
                        shift_rld( & cpu_state.reg_E, & cpu_state.reg_A, count);
                        break;
                    case 5:
                        break;
                    case 6:
                        normalize( & cpu_state.reg_E, & cpu_state.reg_A, &
                            cpu_state.reg_X, count);
                        break;
                    case 7:
                        break;
                }
                set_condition_codes_load(cpu_state.reg_A);
            }
            break;
        case 0xFD:
            if (mode != 1) return MM_PRVINS;
            cpu_state.reg_A = read_word(target_address);
            write_word(target_address, 0);
            set_condition_codes_load(cpu_state.reg_A);
            break;
        case 0xFE:
        case 0xFF:
            break;
    }
    return 0;
}

uint16 group_1(uint16 target_address, uint16 inst) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |                                   |  opcode   |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     */
    uint16 opcode = (inst >> I_GROUP_SHIFT) & 0x1F;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 carry, overflow;
    switch (opcode) {
        case 0x00:
            cpu_state.reg_A = read_word(target_address);
            set_condition_codes_load(cpu_state.reg_A);
            break;
        case 0x01:
            cpu_state.reg_E = read_word(target_address);
            set_condition_codes_load(cpu_state.reg_E);
            break;
        case 0x02:
            cpu_state.reg_X = read_word(target_address);
            set_condition_codes_load(cpu_state.reg_X);
            break;
        case 0x03:
            cpu_state.reg_A ^= read_word(target_address);
            set_condition_codes_load(cpu_state.reg_A);
            break;
        case 0x04:
            cpu_state.reg_A = (target_address - GPRIME) & 0x7FFF;
            set_condition_codes_load(cpu_state.reg_A);
            break;
        case 0x05:
            carry = 0;
            cpu_state.reg_A = add16(cpu_state.reg_A, read_word(target_address), &
                carry, & overflow);
            set_condition_codes_arithmetic(cpu_state.reg_A, carry, overflow);
            break;
        case 0x06:
            carry = 0;
            cpu_state.reg_A = sub16(cpu_state.reg_A, read_word(target_address), &
                carry, & overflow);
            set_condition_codes_arithmetic(cpu_state.reg_A, carry, overflow);
            break;
        case 0x07:
            cpu_state.reg_A = cpu_state.reg_A | read_word(target_address);
            set_condition_codes_load(cpu_state.reg_A);
            break;
    }
    return 0;
}

uint16 group_2(uint16 inst, uint16 target_address, uint32 mode) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |                                   |  opcode   |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     */
    uint16 opcode = (inst >> I_GROUP_SHIFT) & 0x1F;
    uint16 disp = inst & I_DISP_MASK;
    uint16 data;
    uint16 tmp;
    uint16 carry, overflow;
    uint8 s_byte, d_byte;
    int i;
    switch (opcode) {
        case 0x08:
            if (mode != 1)
                return MM_PRVINS;
            if (!(cpu_unit.flags & UNIT_MULDIV))
                return MM_INVINS;
            data = read_word(target_address);
            if (div32(cpu_state.reg_E, cpu_state.reg_A, data, & cpu_state.reg_A, & cpu_state.reg_E) != 0) {
                cpu_state.OV = 1;
            }
            set_condition_codes_load(cpu_state.reg_A);
            break;
        case 0x09:
            cpu_state.reg_A = cpu_state.reg_A & read_word(target_address);
            set_condition_codes_load(cpu_state.reg_A);
            break;
        case 0x0A:
            if (!(cpu_unit.flags & UNIT_EXTINS))
                return MM_INVINS;
            s_byte = read_byte(disp);
            for (i = 0; i < cpu_state.reg_E; i++) {
                d_byte = read_byte(cpu_state.reg_G + cpu_state.reg_A + i);
                if (s_byte == d_byte) {
                    cpu_state.reg_A = cpu_state.reg_G + cpu_state.reg_A +
                    i;
                    cpu_state.reg_E = 0;
                    set_condition_codes_string(0, s_byte < d_byte);
                    break;
                }
            }
            if (i == cpu_state.reg_E) {
                cpu_state.reg_A = cpu_state.reg_G + cpu_state.reg_A;
                cpu_state.reg_E = 0;
                set_condition_codes_string(1, 0);
            }
            break;
        case 0x0B:
            data = read_word(target_address);
            sub16(cpu_state.reg_A, data, & carry, & overflow);
            set_condition_codes_compare(cpu_state.reg_A, data, 0);
            break;
        case 0x0C:
            if (!(cpu_unit.flags & UNIT_MULDIV))
                return MM_INVINS;
            data = read_word(target_address);
            mul32(cpu_state.reg_A, data, & cpu_state.reg_E, & cpu_state.reg_A);
            set_condition_codes_load(cpu_state.reg_E);
            break;
        case 0x0D:
            data = read_word(target_address);
            cpu_state.reg_A = (cpu_state.reg_A & 0x00FF) | (data & 0xFF00);
            set_condition_codes_load(cpu_state.reg_A);
            break;
        case 0x0E:
            data = read_word(target_address);
            cpu_state.reg_A = data & 0x00FF;
            set_condition_codes_load(cpu_state.reg_A);
            break;
        case 0x0F:
            data = read_word(target_address);
            cpu_state.reg_X = data & 0x00FF;
            set_condition_codes_load(cpu_state.reg_X);
            break;
    }
    return 0;
}

