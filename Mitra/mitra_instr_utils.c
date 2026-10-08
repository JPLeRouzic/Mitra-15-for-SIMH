/* mitra_sys.c: CII Mitra 15/30 Simulator SCP Interface
 * adapted from sds_sys.c
 * 
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
#include "mitra_cpu.h"
#include "mitra_defs.h"
#include "mitra_io.h"
#include <stdint.h>

int get_highest_interrupt(void);

uint16 Mem_OP_Reg_To_Reg(t_value mem_value, t_addr target_address, uint16 inst);
uint16 Reg_OP_Mem_To_Mem(uint16 inst, t_addr address, uint32 mode);
t_stat io_rwd(uint16 inst, t_bool is_write);                 /* the public entry point */

extern UNIT cpu_unit;
extern int susp_stack_ptr;

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
} shift_type_t;

/* SRG operation codes (manual page 7-55) 
* This is the value of disp shifted one bit to the right
*/
typedef enum {
    SRG_RTS = 0x00, /* Return Section */
    SRG_XAE = 0x01, /* Exchange A and E */
    SRG_XAX = 0x02, /* Exchange A and X */
    SRG_XEX = 0x03, /* Exchange E and X */
    SRG_XAA = 0x04, /* Exchange bytes of A */
    SRG_CCE = 0x05, /* Complement E */
    SRG_RSV = 0x06, /* Return Supervisor */
    SRG_ACE = 0x07, /* Add Carry to E */
    SRG_CCA = 0x08, /* Complement A */
    SRG_AEE = 0x09, /* A XOR E */
    SRG_CNX = 0x0A, /* Copy Negative X */
    SRG_AIE = 0x0B, /* A OR E */
    SRG_AAE = 0x0C, /* A AND E */
    SRG_LNE = 0x0D, /* Load -1 into E */
    SRG_CNA = 0x0E, /* Copy Negative A */
    SRG_CHX = 0x0F  /* Compute Half X */
} srg_op_t;

/* ========== Effective Address Calculation ==========
The manual defines three instruction classes and addressing modes:
0:	DL, P, DG, IL, IGX, ILX			load/arithmetic
0':	DL, DG, IL, IGX, ILX (same but no P)	store/complex
1:	P, PX, DL				shift, index, base and system
operations 2:	RP, RM, IL, IG				conditional or
unconditional branch instructions

-IG (Indirect General) is not directly listed with its own mode number in the
code. It appears to be handled under mode 4 (DG) in some contexts or combined
with other modes in ea_class2().
 *
 * The addressing mode is determined most of time by the opcode byte; bits 0-2
of
 * the instruction word alone do NOT suffice (they are overloaded across
 * classes).  The mapping below is derived directly from the per-opcode
 * (addressing-mode, instruction) table in the CII Mitra-15 reference manual.
 *
 * Addressing mode formulae (D = 8-bit displacement, G' = G in slave mode / 0
 * in master mode):
 *
 *   DL  – Direct Local          : Y = (L + D) & 0x0FFFF
 *   P   – Parameter/Immediate   : Y = D            (no memory indirection)
 *   PX  – Parameter Indexed     : Y = D            (Class 1, same as P for EA)
 *   DG  – Direct General        : Y = (G + D) & 0x0FFFF
 *   IL  – Indirect Local        : Y = (G' + mem[L + D]) & 0x0FFFF
 *   IGX – Indirect General Idx  : Y = (G' + mem[G + D] + X) & 0x0FFFF
 *   ILX – Indirect Local Idx    : Y = (G' + mem[L + D] + X) & 0x0FFFF
 *   RP  – Relative Plus  (br)   : Y = (P + 2*D) & 0x0FFFF
 *   RM  – Relative Minus (br)   : Y = (P - 2*D) & 0x0FFFF
 *   IG  – Indirect General (br) : Y = (G' + mem[G + D]) & 0x0FFFF
 *
 * Opcodes 34, 3E, 3F, E4, EE, EF, FE, FF are not implemented; this
 * function returns 0 for them (the caller is responsible for rejection).
 *
 Data width:
The Mitra-15 is fundamentally a 16-bit word-addressable machine. Data size is
chosen implicitly by the opcode (bits 4–7), not by any extra control bits in the
instruction. Examples: Byte: LBL (Load Byte Left into A) Word: LDA, STA, ADD,
SUB, AND, IOR, etc. Double word: DLD (Double Load): loads two words → E and A
*/

/* ========== Condition Code Functions (per manual section II-6) ========== */

/* For LOAD instructions: 
 *   C | OV | test
 *   0 |  0 | result > 0
 *   0 |  1 | result < 0
 *   1 |  0 | result = 0
 */
void set_condition_codes_load(uint16 result) {
    cpu_state.C = (result == 0) ? 1 : 0;
    cpu_state.OV = (result & 0x8000) ? 1 : 0;
}

/* CMP - Compare (manual p.98)
 *   C | OV | test
 *   0 |  0 | (A) > Y2
 *   0 |  1 | (A) < Y2
 *   1 |  0 | (A) = Y2
 * Comparison is algebraic (signed 16-bit), not unsigned.
 */
static void set_condition_codes_compare(uint16 acc, uint16 mem, uint16 result) {
    (void)result;
    int16_t sacc = (int16_t)acc;
    int16_t smem = (int16_t)mem;

    if (sacc == smem) {
        cpu_state.C  = 1;
        cpu_state.OV = 0;
    } else if (sacc < smem) {
        cpu_state.C  = 0;
        cpu_state.OV = 1;
    } else {
        cpu_state.C  = 0;
        cpu_state.OV = 0;
    }
}

/* Set condition codes for float results (manual p.130, p.131, p.132, p.133)
 *
 *   C | OV |
 *   0 |  0 | Result != 0, no overflow
 *   0 |  1 | Overflow
 *   1 |  0 | Result = 0
 *   1 |  1 | Underflow
 *
 * C and OV cannot be derived from (E,A) alone: an unrepresentable
 * result and a legitimate zero both produce some form of zero or
 * garbage in E:A. The caller must pass the status returned by
 * double_to_mitra().
 */
void set_condition_codes_float(fp_status_t status)
{
    sim_printf("\n [set_condition_codes_float] fp_status_t: %#05x", status);

    switch (status) {
    case MITRA_FP_OVERFLOW:
        cpu_state.C  = 0;
        cpu_state.OV = 1;
        break;
        
    case MITRA_FP_ZERO:
        cpu_state.C  = 1;
        cpu_state.OV = 0;
        break;
        
    case MITRA_FP_UNDERFLOW:
        cpu_state.C  = 1;
        cpu_state.OV = 1;
        break;
        
    case MITRA_FP_OK:
    
    default:
        cpu_state.C  = 0;
        cpu_state.OV = 0;
        break;
    }
}

/* For ARITHMETIC instructions:
 * C = carry/borrow
 * O = overflow (operands same sign, result opposite sign)
 */
static void set_condition_codes_arithmetic(uint16 result, uint16 carry,
                                           uint16 overflow) {
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
static uint16 add16(uint16 a, uint16 b, uint16* carry, uint16* overflow) {
    uint32 sum = (uint32)a + (uint32)b + *carry;
    uint16 result = sum & 0xFFFF;
    *carry = (sum >> 16) & 1;
    if (((a & 0x8000) == (b & 0x8000)) && ((a & 0x8000) != (result & 0x8000))) {
        *overflow = 1;
    } else {
        *overflow = 0;
    }
    return result;
}

// A - Y => A
/* 
* carry must be seeded to 1 for a standalone op
* If a genuine double-word SUB ever gets implemented, that's when we would need borrow propagation across two 16-bit halves
static uint16 sub16(uint16 a, uint16 b, uint16* carry, uint16* overflow) {
    uint32 diff = (uint32)a + (uint32)(~b & 0xFFFF) + *carry;
    uint16 result = diff & 0xFFFF;
    *carry = (diff >> 16) & 1;
    if (((a & 0x8000) != (b & 0x8000)) && ((a & 0x8000) != (result & 0x8000))) {
        *overflow = 1;
    } else {
        *overflow = 0;
    }
    return result;
} */
static uint16 sub16(uint16 a, uint16 b, uint16* carry, uint16* overflow) {
    uint16 result = (uint16)(a - b);   /* unsigned wraparound does the modular
                                           arithmetic for you — no ~b+1 needed */

    *carry    = (a < b) ? 1 : 0;       /* 1 = borrow occurred */
    *overflow = (((a ^ b) & 0x8000) && ((a ^ result) & 0x8000)) ? 1 : 0;

    return result;
}

/* mul32(cpu_state.reg_A, mem_value, &cpu_state.reg_E, &cpu_state.reg_A);
static void mul32(uint16 a, uint16 b, uint16* high, uint16* low) {
    uint32 product = (uint32)(int16_t)a * (uint32)(int16_t)b;
    sim_printf("\nproduct: %#010x", product);
    *high = (product >> 16) & 0xFFFF;
    *low = product & 0xFFFF;
} */
static void mul32(uint16 a, uint16 b, uint16* high, uint16* low) {
    uint32 product = (uint32)a * (uint32)b;
    sim_printf("\nproduct: %#010x", product);
    *high = (uint16)(product >> 16);
    *low  = (uint16)product;
}

static int div32(uint16 high, uint16 low, uint16 divisor, uint16* quot,
                 uint16* rem) {
    int32_t dividend = ((int32_t)(int16_t)high << 16) | (uint16_t)low;
    int16_t dvsr = (int16_t)divisor;
    // test for division by zero
    if (dvsr == 0) 
		return -1;
    *quot = (uint16_t)(dividend / dvsr);
	// test for overflow
	if (*quot < INT16_MIN || *quot > INT16_MAX)
		return -2;
    *rem = (uint16_t)(dividend % dvsr);
    return 0;
}

/* ========== Shift Operations (per manual section VII-7) ========== */

/* Shift Left Logical Single (SLLS) */
uint16 shift_lls(uint16 val, int count) {
    uint16 result = val;
    int i;
    for (i = 0; i < count; i++) {
        cpu_state.C = (result & 0x8000) ? 1 : 0;
        result <<= 1;
    }
    return result;
}

/* Shift Right Logical Single (SRLS) */
uint16 shift_rls(uint16 val, int count) {
    uint16 result = val;
    int i;
    for (i = 0; i < count; i++) {
        cpu_state.C = result & 1;
        result >>= 1;
    }
    return result;
}

/* Shift Right Arithmetic Single (SAS) - preserve sign bit */
uint16 shift_sas(uint16 val, int count) {
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
uint16 shift_srcs(uint16 val, int count) {
    uint16 result = val;
    int i;
    for (i = 0; i < count; i++) {
		uint16 lsb = result & 1;

		cpu_state.C = lsb;
		result = (result >> 1) | (lsb << 15);
    }
    return result;
}

/* Shift Left Circular Single (SLCS) */
uint16 shift_slcs(uint16 val, int count) {
    uint16 result = val;
    int i;
    for (i = 0; i < count; i++) {
		uint16 msb = (result >> 15) & 1;

		cpu_state.C = msb;
		result = (result << 1) | msb;
		}
    return result;
}

/* Shift Left Logical Double (SLLD) - shift (E,A) left */
void shift_lld(uint16* E, uint16* A, int count) {
    int i;
    for (i = 0; i < count; i++) {
        cpu_state.C = (*E & 0x8000) ? 1 : 0;
        *E = (*E << 1) | ((*A & 0x8000) ? 1 : 0);
        *A <<= 1;
    }
}

/* Shift Right Logical Double (SRLD) */
void shift_rld(uint16* E, uint16* A, int count) {
    int i;
    for (i = 0; i < count; i++) {
        cpu_state.C = *A & 1;
        *A = (*A >> 1) | ((*E & 1) << 15);
        *E >>= 1;
    }
}

/* Shift Right Arithmetic Double (SAD) */
void shift_sad(uint16* E, uint16* A, int count) {
    int i;
    for (i = 0; i < count; i++) {
        cpu_state.C = *A & 1;
        uint16 sign = *E & 0x8000;
        *A = (*A >> 1) | ((*E & 1) << 15);
        *E = (*E >> 1);
        if (sign) *E |= 0x8000;
    }
}

/* Shift Left Circular Double (SLCD) */
void shift_lcd(uint16* E, uint16* A, int count) {
    int i;
    for (i = 0; i < count; i++) {
        uint16 msb = (*E & 0x8000) ? 1 : 0;
        cpu_state.C = msb;
        *E = (*E << 1) | ((*A & 0x8000) ? 1 : 0);
        *A = (*A << 1) | msb;
    }
}

/* Shift Right Circular Double (SRCD) */
void shift_rcd(uint16* E, uint16* A, int count) {
    int i;
    for (i = 0; i < count; i++) {
        uint16 lsb = *A & 1;
        cpu_state.C = lsb;
        *A = (*A >> 1) | ((*E & 1) << 15);
        *E = (*E >> 1) | (lsb << 15);
    }
}

/*
 * NLZ, double length NormaliZe (option)
 * The contents of E, A extended register is shifted to the left until:
 *    - bit 0 is different from bit 1 or
 *    - up to a maximum of n positions.
 * X-register is decremented by the actual number of shift steps.
 */
static int normalize(uint16* E, uint16* A, uint16* X, int max_steps) {
    int steps = 0;
    uint32 double_word = ((uint32)*E << 16) | *A;
    while (steps < max_steps && steps < 31) {
        if (((double_word >> 31) & 1) != ((double_word >> 30) & 1)) break;
        double_word <<= 1;
        steps++;
    }
    *E = (double_word >> 16) & 0xFFFF;
    *A = double_word & 0xFFFF;
    *X -= steps;

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

/* Compute parity (PTY) - count set bits shifted out 
 * 
 *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
 *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *      | x  x  x  x| 1 1  0  0 | 0  1  0|    count     |
 *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *
 * Function: 
 * (A) = shifted (A)
 * (E) = number of set bits shifted out of (A)
 * A register contents shifted circularly in positions to the left. 
 * When the instruction is over, E register contains the number of set bits which have been shifted out of A.
 */
static uint16 compute_parity(uint16* A, int count) {
	uint16 result = *A;
	uint16 parity_count = 0;
	uint16 shifted_out = 0;
	
	for (int i = 0; i < count; i++) {
        /* Bit shifted out from the left */
        shifted_out = (result & 0x8000) >> 15;
		result = result << 1;
        /* As it is a circular shift left shifted out must be reintroduced at right */
        result = result | shifted_out;

        /* Count set bits */
        if (shifted_out)
            parity_count++;
    }

    *A = result;

    /* C = last bit shifted out */
    cpu_state.C = shifted_out;

    /* OV = parity of number of 1 bits shifted out */
    cpu_state.OV = parity_count & 1;

    return parity_count;
}

/* ========== Floating Point (per manual section VII-9) ========== 
 Mitra-15 uses a base-16 exponent with a characteristic +64 
 the exponent is a power of 16, not a power of 2
   ex: 1000=0.244140625×16^3
 The 64 is a bias, a kind of sign for the characteristic. It lets the 7-bit unsigned field represent negative and positive exponents.
┌────┬─────────┬───────────────────────┐
│ S  │  C (7)  │       M (24)          │
└────┴─────────┴───────────────────────┘

          E                  A
┌────────────────────┬─────────────────────┐
│ 16 most significant│ 16 least significant│
└────────────────────┴─────────────────────┘
*/

/*
 * Convert a Mitra-15 value (E,A) to double.
 *
 * E = most significant 16 bits
 * A = least significant 16 bits
 */
static double mitra_to_double(uint16_t A, uint16_t E)
{
    uint32_t raw = ((uint32_t)E << 16) | A;

    if (raw == 0)
        return 0.0;

    /*
     * Negative Mitra numbers are stored as the
     * two's complement of the complete 32-bit value.
     *
     * Convert back to the positive magnitude first.
     */
    bool negative = (raw & 0x80000000U) != 0;

    if (negative)
        raw = (~raw) + 1;

    /* Extract characteristic and mantissa */
    int C = (raw >> 24) & 0x7F;
    uint32_t mant = raw & 0xFFFFFFU;

    /*
     * M = mantissa / 2^24
     */
    double M = mant / 16777216.0;     /* 2^24 */

    /*
     * N = M * 16^(C-64)
     */
    double value = M * pow(16.0, C - 64);

    return negative ? -value : value;
}

/*
 * Convert a double to Mitra-15.
 *
 * Returns false if the value is outside the
 * representable Mitra-15 range.
 */
static fp_status_t double_to_mitra(double v, uint16_t *A, uint16_t *E)
{
    if (v == 0.0) {
        *A = 0; *E = 0;
        return MITRA_FP_ZERO;
    }

    bool negative = v < 0.0;
    double x = fabs(v);
    sim_printf("\n[double_to_mitra] x = %g", x);    

    int e = (int)floor(log(x) / log(16.0)) + 1;
    
    sim_printf("\n[double_to_mitra] e = %d", e);    

    /* Characteristic must fit in the 7-bit biased field.
     * Normalized mantissa is in [1/16, 1), so the exponent e
     * must satisfy 1 <= e+64 <= 126 (0 and 127 are reserved). */
    if (e + 64 <= 0) {
        *A = 0; *E = 0;
        return MITRA_FP_UNDERFLOW;
    }
    if (e + 64 >= 127) {
        *A = 0; *E = 0;
        return MITRA_FP_OVERFLOW;
    }

    double M = x / pow(16.0, e);
    uint64_t mant = (uint64_t)llround(M * 16777216.0);
    if (mant >= 16777216ULL) {
        mant = 1048576ULL;
        e++;
        if (e + 64 >= 127) {
            *A = 0; *E = 0;
            return MITRA_FP_OVERFLOW;
        }
    }

    int C = e + 64;
    uint32_t raw = ((uint32_t)C << 24) | ((uint32_t)mant & 0xFFFFFFU);
    if (negative) raw = (~raw) + 1;

    *E = (uint16_t)(raw >> 16);
    *A = (uint16_t)(raw & 0xFFFF);
    return MITRA_FP_OK;
}

uint16 F4MasterInst(uint16 mode, uint16 inst) {
    uint16 ret_code =0;
        // Bits 12, 14, 15 decode 5 intructions: STM, CLM, DIT, RD, WD
        uint16 MasterInst = inst & 0x000F;
        if (mode != 1) // These four instructions are privilegied
            	return MM_PRVINS;
        switch (MasterInst) {
            case 0x00: 
            /* 
            * CLM - clear Interrupt Mask
            * As a consequence, a II interrupt levels are masked. 
            */
                cpu_state.MA = 0;
                break;

            case 0x01: /* DIT - Deactivate normal Interrupt */
                return mitra_interrupt_return(FALSE);
                break;

            /*
             * RD et WD sont des instructions synchrones : le processeur attend
             * la réponse du périphérique. On fait souvent du polling (boucle de
             * RD) pour attendre un résultat ou un statut. Ces instructions ne déclenchent
             * pas elles-mêmes une interruption pour livrer un résultat différé.
             *
             * Les interruptions restent un mécanisme séparé, utilisé en
             * parallèle pour les événements asynchrones (fin d’opération
             * longue, signal externe, etc.). C’est le mode d’E/S le plus simple
             * et le plus direct des mini-ordinateurs des années 1960-70, par
             * opposition aux transferts par canal (IOP) qui sont eux
             * asynchrones et pilotés par interruptions + chaînage de commandes.
             *
             * La documentation ne mentionne pas de changement de contenu des registres ou des codes conditions
             */
            case 0x02:
                /*** RD
                bits 8 to 13 undefined
                bits 14, 14 = 10
                The opcode is 0xF402

                E register:
                Bits 3 to 6 and 12 to 15 are the I/O address
                Bits 10 and 11, are reading mode
                ***/
                ret_code = io_rwd(inst, false);
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
                ret_code = io_rwd(inst, true);
                break;
                
            case 0x08: /* STM - Set Interrupt Mask */
                cpu_state.MA = 1;
                break;

			default:
				sim_printf("\n[group_3_P] 0xF4 unknown MasterInst %#04x", MasterInst);
				return TRAP_II;
				
				}
    return ret_code;
    }

// LDR or STR
uint16 case_instr_xDR(uint16 inst) {
    uint8 opcode = (inst >> I_OPCODE_SHIFT) & 0x0FF; // opcodes all over 8 bits
            
    sim_printf("\n[case_instr_xDR] opcode: %#010x\n", opcode);
    
    uint16 disp = inst & 0x00FF;
    t_addr target_address;
    t_value target_value;
    uint8 reg_num = inst & 0x0007;   // 64 registers in 8 blocks of 8 registers
    uint8 reg_block;                 // block number, 0-7

    switch (opcode) {
        case 0x39: {  // LDR DL, register number located in the first 256 bytes
                      // of the local segment.
            // This instruction is not privileged
            target_address = (cpu_state.reg_L + disp) & 0x0FFFF;
            target_value = read_word(target_address);
            reg_block = (target_value >> 3) & 0x0007;
            reg_num = target_value & 0x0007;

			instr_LDR(reg_block, reg_num);
        } break;

        case 0x3A: {  // STR DL, register number located in the first 256 bytes
                      // of the local segment.
            // This instruction is only executable in master mode
            if (!cpu_state.MS) 
            	return MM_PRVINS;
            target_address = (cpu_state.reg_L + disp) & 0x0FFFF;
            target_value = read_word(target_address);
            reg_block = (target_value >> 3) & 0x0007;
            reg_num = target_value & 0x0007;

            instr_STR(reg_block, reg_num);
        } break;

        case 0xE9: {  // LDR PX, register number defined by displacement value
                      // plus X-register
            // This instruction is not privileged
            target_value = (disp + cpu_state.reg_X) & 0x0FFFF;
            reg_block = (target_value >> 3) & 0x0007;
            reg_num = target_value & 0x0007;
 
			instr_LDR(reg_block, reg_num);
        } break;
 
        case 0xEA: {  // STR PX, register number defined by displacement value
                      // plus X-register
            // This instruction is only executable in master mode
            if (!cpu_state.MS) 
            	return MM_PRVINS;
            target_value = (disp + cpu_state.reg_X) & 0x0FFFF;
            reg_block = (target_value >> 3) & 0x0007;
            reg_num = target_value & 0x0007;
 
            instr_STR(reg_block, reg_num);
        } break;
 
        case 0xF9: {  // LDR P, register number defined by displacement value
            // This instruction is not privileged
            target_value = disp;
            reg_block = (target_value >> 3) & 0x0007;
            reg_num = target_value & 0x0007;
            
			instr_LDR(reg_block, reg_num);
        } break;
 
        case 0xFA: {  // STR P, register number defined by displacement value
            // This instruction is only executable in master mode
            if (!cpu_state.MS) 
            	return MM_PRVINS;
            target_value = disp;
            reg_block = (target_value >> 3) & 0x0007;
            reg_num = target_value & 0x0007;
 
            instr_STR(reg_block, reg_num);
        } break;
        
	default:
	    sim_printf("\n[case_instr_xDR] unknown opcode %#04x, inst %#06x\n",
		       opcode, inst);
	    return TRAP_II;
        
    }    
    return 0;
}

void instr_LDR(uint16 reg_block, uint16 reg_num) {
			if(reg_block == 0) {
				switch (reg_num) {
					case 0: cpu_state.reg_A = cpu_state.reg_P; break; 
					case 1: cpu_state.reg_A = cpu_state.reg_L; break; 
					case 2: cpu_state.reg_A = cpu_state.reg_G; break; 
					case 3: break; 
					case 4: cpu_state.reg_A = cpu_state.reg_E; break; 
					case 5: cpu_state.reg_A = cpu_state.reg_X; break; 
					case 6: cpu_state.reg_A = cpu_state.reg_V; break; 
					case 7: cpu_state.reg_A = cpu_state.reg_W; break; 
					}
				}
			else if (reg_block == 1) {
				switch (reg_num) {
					case 0: cpu_state.reg_A = cpu_state.reg_8; break;
					case 1: cpu_state.reg_A = cpu_state.reg_cnt_MAE; break;
					case 2: cpu_state.reg_A = cpu_state.reg_curr_MAE; break;
					case 3: cpu_state.reg_A = cpu_state.reg_Work_MAE; break;
					case 4: cpu_state.reg_A = cpu_state.reg_12; break;
					case 5: cpu_state.reg_A = cpu_state.reg_NC1; break;
					case 6: cpu_state.reg_A = cpu_state.reg_NC2; break;
					case 7: cpu_state.reg_A = cpu_state.reg_NC3; break;
					}
			}
			else {
				cpu_state.reg_A = cpu_state.reg_block[reg_block][reg_num];
			}
    	    set_condition_codes_load(cpu_state.reg_A);
}

void instr_STR(uint16 reg_block, uint16 reg_num) {
			if(reg_block == 0) {
				switch (reg_num) {
					case 0: cpu_state.reg_P = cpu_state.reg_A; break;
					case 1: cpu_state.reg_L = cpu_state.reg_A; break; 
					case 2: cpu_state.reg_G = cpu_state.reg_A; break; 
					case 3: break; // A = A
					case 4: cpu_state.reg_E = cpu_state.reg_A; break; 
					case 5: cpu_state.reg_X = cpu_state.reg_A; break; 
					case 6: cpu_state.reg_V = cpu_state.reg_A; break; 
					case 7: cpu_state.reg_W = cpu_state.reg_A; break; 
					}
				}
			else if (reg_block == 1) {
				switch (reg_num) {
					case 0: cpu_state.reg_8 = cpu_state.reg_8; break;
					case 1: cpu_state.reg_cnt_MAE = cpu_state.reg_cnt_MAE; break;
					case 2: cpu_state.reg_curr_MAE = cpu_state.reg_curr_MAE; break;
					case 3: cpu_state.reg_Work_MAE = cpu_state.reg_Work_MAE; break;
					case 4: cpu_state.reg_12 = cpu_state.reg_12; break;
					case 5: cpu_state.reg_NC1 = cpu_state.reg_NC1; break;
					case 6: cpu_state.reg_NC2 = cpu_state.reg_NC2; break;
					case 7: cpu_state.reg_NC3 = cpu_state.reg_NC3; break;
					}
			}
			else {
				cpu_state.reg_block[reg_block][reg_num] = cpu_state.reg_A;
			}
    	    set_condition_codes_load(cpu_state.reg_A);
}

/* SHC, Special shift
 *
 *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
 *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *      |    opcode | 1  1  0  0|  type  |   count      |
 *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *
 */
uint16 shift_instr(uint16 inst, uint32 mode, t_addr target_address) {
    uint8 opcode = (inst >> I_OPCODE_SHIFT) & 0x0FF;
            sim_printf("\n[SHC, Special shift] opcode: %#010x\n", opcode);
    uint16 disp = inst & I_DISP_MASK;
    uint8 count = disp & 0x1F;

    switch (opcode) {
        case 0x3C: { // SHC DL
            uint8 shc_word = (uint8)(read_word(target_address) & 0xFF);
            uint8 shc_type = (shc_word >> 5) & 0x07;
            sim_printf("\nshc_type: %#010x\n", shc_type);
            count = shc_word & 0x1F;
            switch (shc_type) {
                case 0: // SLLD
                    shift_lld(&cpu_state.reg_E, &cpu_state.reg_A, count);
                    break;
                case 1: // ???
                    break;
                case 2:  // PTY, ParitY check in A
                    cpu_state.reg_E = compute_parity(&cpu_state.reg_A, count);
                    // no break as it would execute set_condition_codes_load()
                    return 0;

                case 3: // ???
                    if (mode != 1) return MM_PRVINS;
                    cpu_state.intrpt_mask &= ~(1u << cpu_state.curr_int_lvl);
                    cpu_state.curr_int_lvl = 0;
                    break;
                case 4:  // SRLD
                    shift_rld(&cpu_state.reg_E, &cpu_state.reg_A, count);
                    break;
                case 5: // ???
                    break;
                case 6: // NLZ
                    normalize(&cpu_state.reg_E, &cpu_state.reg_A, &cpu_state.reg_X, count);
                    // no break as it would execute set_condition_codes_load()
                    return 0;

                case 7: // ???
                    break;
                    
                default:
					sim_printf("\n[shift_instr] case 0x3C unknown opcode %#04x, inst %#06x\n",
						   opcode, inst);
					return TRAP_II;
                    
            }
            set_condition_codes_load(cpu_state.reg_A);
        }
        break;

        case 0xEC: { // SHC PX
	    uint8 param = target_address & 0xFF;
	    uint8 shc_type = (param >> 5) & 0x07;
	    count = param & 0x1F;   // reassign, count was set from disp before the switch
            switch (shc_type) {
                case 0:
                    shift_lld(&cpu_state.reg_E, &cpu_state.reg_A, count);
                    break;
                case 1: // DITR PX, deactivate high-speed interrupt 
                    if (mode != 1) 
						return MM_PRVINS;
//                    cpu_state.intrpt_mask &= ~(1u << cpu_state.curr_int_lvl);
//                    cpu_state.curr_int_lvl = 0;
					mitra_interrupt_return(FALSE);
                    break;
                case 2:  // PTY, ParitY check in A
                    cpu_state.reg_E = compute_parity(&cpu_state.reg_A, count);
                    // no break as it would execute set_condition_codes_load()
                    return 0;
                case 3:
                    if (mode != 1) return MM_PRVINS;
                    cpu_state.intrpt_mask &= ~(1u << cpu_state.curr_int_lvl);
                    cpu_state.curr_int_lvl = 0;
                    break;
                case 4:  // SRLD
                    shift_rld(&cpu_state.reg_E, &cpu_state.reg_A, count);
                    break;
                case 5:
                    break;
                case 6:
                    /* NLZ, double length NormaliZe (option)
                    The contents of E, A extended register is shifted to the
                    left until bit 0 is different from bit 1 or up to a maximum
                    of n positions. X-register is decremented by the actual
                    number of shift steps.
                    */
                    normalize(&cpu_state.reg_E, &cpu_state.reg_A, &cpu_state.reg_X, count);
                    // no break as it would execute set_condition_codes_load()
                    return 0;

                case 7:
                    break;
                    
                default:
					sim_printf("\n[shift_instr] case 0xEC unknown opcode %#04x, inst %#06x\n",
						   opcode, inst);
					return TRAP_II;
                    
            }
            set_condition_codes_load(cpu_state.reg_A); // reg_A
        }
        break;

        case 0xFC: { // SHC P
            uint8 shc_type = (disp >> 5) & 0x07;
            switch (shc_type) {
                case 0:
                    shift_lld(&cpu_state.reg_E, &cpu_state.reg_A, count);
                    break;
                case 1: // DITR P, deactivate high-speed interrupt
                    if (mode != 1) 
						return MM_PRVINS;
//                    cpu_state.intrpt_mask &= ~(1u << cpu_state.curr_int_lvl);
//                    cpu_state.curr_int_lvl = 0;
					mitra_interrupt_return(FALSE);
                    break;
                    
                case 2:  // PTY, ParitY check in A
                    cpu_state.reg_E = compute_parity(&cpu_state.reg_A, count);
                    // no break as it would execute set_condition_codes_load()
                    return 0;
                    
                case 3:
                    if (mode != 1) return MM_PRVINS;
                    cpu_state.intrpt_mask &= ~(1u << cpu_state.curr_int_lvl);
                    cpu_state.curr_int_lvl = 0;
                    break;
                case 4:  // SRLD
                    shift_rld(&cpu_state.reg_E, &cpu_state.reg_A, count);
                    break;
                case 5:
                    break;
                case 6:
                    /* NLZ, double length NormaliZe (option)
                    The contents of E, A extended register is shifted to the
                    left until bit 0 is different from bit 1 or up to a maximum
                    of n positions. X-register is decremented by the actual
                    number of shift steps.
                    */
                    normalize(&cpu_state.reg_E, &cpu_state.reg_A, &cpu_state.reg_X, count);
                    // no break as it would execute set_condition_codes_load()
                    return 0;

                case 7:
                    break;
                    
                default:
					sim_printf("\n[shift_instr] case 0xFC unknown opcode %#04x, inst %#06x\n",
						   opcode, inst);
					return TRAP_II;

            }
            set_condition_codes_load(cpu_state.reg_A);
        }
        break;
        
	default:
	    sim_printf("\n[shift_instr] unknown opcode %#04x, inst %#06x\n",
		       opcode, inst);
	    return TRAP_II;
        
    }
    return 0;
}

/*
* SRG instruction group
* 
*        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*      | x  x  x  x| 0 0  0  1 |        |    type   |  |
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*/
uint16 set_register(uint16 inst, uint32 mode) {
    uint8 opcode = (inst >> I_OPCODE_SHIFT) & 0x0FF;
            sim_printf("\n[set_register] opcode: %#010x\n", opcode);
    uint16 disp = inst & I_DISP_MASK;
    uint16 data;
    
    switch (opcode) {
        case 0x31: { // DL mode type is computed from target word in memory
	    uint16 srg_word = read_word((cpu_state.reg_L + disp) & 0x0FFFF);
	    srg_op_t type = (srg_word & 0x1E) >> 1;
            switch (type) {
                case SRG_XAE:  // XAE, eXchange contents of A and E
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
                    cpu_state.reg_A = ((cpu_state.reg_A & 0xFF) << 8) |
                                      ((cpu_state.reg_A >> 8) & 0xFF);
                    break;
                case SRG_CCE:
                    cpu_state.reg_E = ~cpu_state.reg_E & 0xFFFF;
                    break;
                case SRG_ACE:
                    cpu_state.reg_E = (cpu_state.reg_E + cpu_state.C) & 0xFFFF;
                    /*
                     * Manual p. VII-59 says ACE modifies E and the C/O indicators..
                     */
                    set_condition_codes_load(cpu_state.reg_A);
                    break;
                case SRG_CCA:
                    cpu_state.reg_A = ~cpu_state.reg_A & 0xFFFF;
                    /*
                     * Manual p. VII-60 lists only A as modified for CCA; unlike AEE/AIE/AAE/CNA, it does not list condition codes as modified.
                     */
                    // set_condition_codes_load(cpu_state.reg_A);
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
                    /*
                    * X-register contents shifted one position to the right. Sign bit (bit 0) restored. 
                    * As a result, X-register contents is divided by two.
		    */
                    cpu_state.reg_X =
                        (cpu_state.reg_X >> 1) | (cpu_state.reg_X & 0x8000);
                    break;
                default:
                    break;
            }
        }
        break;

        case 0xE1: { 
            // In PX case, type is computed from word at displacement field + X
            srg_op_t type = ((disp + cpu_state.reg_X) & 0x1E) >> 1;
            switch (type) {
                case SRG_XAE:  // XAE, eXchange contents of A and E
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
                    cpu_state.reg_A = ((cpu_state.reg_A & 0xFF) << 8) |
                                      ((cpu_state.reg_A >> 8) & 0xFF);
                    break;
                case SRG_CCE:
                    cpu_state.reg_E = ~cpu_state.reg_E & 0xFFFF;
                    break;
                case SRG_ACE:
                    cpu_state.reg_E = (cpu_state.reg_E + cpu_state.C) & 0xFFFF;
                    /*
                     * Manual p. VII-59 says ACE modifies E and the C/O indicators..
                     */
                    set_condition_codes_load(cpu_state.reg_A);
                    break;
                case SRG_CCA:
                    cpu_state.reg_A = ~cpu_state.reg_A & 0xFFFF;
                    /*
                     * Manual p. VII-60 lists only A as modified for CCA; unlike AEE/AIE/AAE/CNA, it does not list condition codes as modified.
                     */
                    // set_condition_codes_load(cpu_state.reg_A);
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
                    /*
                    * X-register contents shifted one position to the right. Sign bit (bit 0) restored. 
                    * As a result, X-register contents is divided by two.
		    */
                    cpu_state.reg_X =
                        (cpu_state.reg_X >> 1) | (cpu_state.reg_X & 0x8000);
                    break;
                default:
                    break;
            }
        }
        break;

        case 0xF1: {
            // In P case, type is computed from displacement field
    	    srg_op_t type = (disp & 0x1E) >> 1;
            switch (type) {
                case SRG_RTS:
                /*
                * RTS (there is no address mode)
                * The RTS placed in a section called by a CLS provides the return to the calling section by restoring the 
                * L and P-registers to the corresponding values contained in the first two words of the called section's LDS.
                *
                * ((L)) + G' + 2 -> (P)
                * ((L) + 2) + G' -> (L)
                * 
                * Because the simulator pre-increments P, RTS is one of the cases where we must not compensate by 
                * subtracting 2 as the saved P was already written by CLS as the address of the CLS instruction.
                */
                    uint16 saved_P = (read_word(cpu_state.reg_L) + ((GPRIME) + 2)) & 0x0FFFF ; 	// ((L)) + G' + 2
                    uint16 saved_L = (read_word(cpu_state.reg_L + 2) + GPRIME) & 0x0FFFF; 		// L = ((L) + 2) + G' 
                    cpu_state.reg_P = saved_P;
                    cpu_state.reg_L = saved_L; 
                    break;
                case SRG_XAE:  // XAE, eXchange contents of A and E
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
                    cpu_state.reg_A = ((cpu_state.reg_A & 0xFF) << 8) |
                                      ((cpu_state.reg_A >> 8) & 0xFF);
                    break;
                case SRG_CCE:
                    cpu_state.reg_E = ~cpu_state.reg_E & 0xFFFF;
                    break;
                case SRG_RSV:
					/* * RSV * * The RSV at the end of an OS section 
					(Master mode) that was called by a CSV provides the 
					return to the calling * section by restoring in L- 
					and P-registers the corresponding values contained 
					in the first two words of the * CDS of the program 
					to which the calling section belongs. * It also 
					restores the initial status of the indicators. * * 
					((G) + 4) -> Indicators * (G) + ((G) + 2) -> (L) * 
					(G) + 2 + ((G)) -> (P)
					*/
                    if (mode != 1) 
                    	return MM_PRVINS;
                    
                        // ((G) + 4) -> Indicators
                        uint16 saved_flags = read_word(cpu_state.reg_G + 4);
                        load_indicators_from_block(saved_flags);
                    
                    cpu_state.reg_L = // (G) + ((G) + 2)
                        (cpu_state.reg_G + read_word(cpu_state.reg_G + 2)) &
                        0x7FFF;
                    cpu_state.reg_P = // (G) + 2 + ((G))
                        (cpu_state.reg_G + 2 + read_word(cpu_state.reg_G)) &
                        0x7FFF;
					sim_printf("\n[RSV_instr] P = %#05x", cpu_state.reg_P);
                    break;
                case SRG_ACE:
                    cpu_state.reg_E = (cpu_state.reg_E + cpu_state.C) & 0xFFFF;
                    /*
                     * Manual p. VII-59 says ACE modifies E and the C/O indicators..
                     */
                    set_condition_codes_load(cpu_state.reg_A);
                    break;
                case SRG_CCA:
                    cpu_state.reg_A = ~cpu_state.reg_A & 0xFFFF;
                    /*
                     * Manual p. VII-60 lists only A as modified for CCA; unlike AEE/AIE/AAE/CNA, it does not list condition codes as modified.
                     */
                    // set_condition_codes_load(cpu_state.reg_A);
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
					/* * X-register contents shifted one position to 
					the right. Sign bit (bit 0) restored. * As a 
					result, X-register contents is divided by two.
		    */
                    cpu_state.reg_X =
                        (cpu_state.reg_X >> 1) | (cpu_state.reg_X & 0x8000);
                    break;
                default:
						sim_printf("\n[set_register] unknown opcode %#04x, inst %#06x\n", opcode, inst);
						return TRAP_II;
            } // switch (type) 
        } // case 0xF1:
        break;

	default:
	    sim_printf("\n[set_register] unknown opcode %#04x, inst %#06x\n", opcode, inst);
	    return TRAP_II;
            
        }
    return 0;
}

/*
 * Load something and operate on it
 *
 *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
 *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *      |  address  |   opcode  |     displacement      |
 *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *
 *    "LDA", "LDE", "LDX", "EOR", "LEA", "ADD", "SUB", "IOR",
 *    "DIV", "AND", "CPS", "CMP", "MUL", "LBL", "LBR", "LBX",
 *
 */
uint16 Mem_OP_Reg_To_Reg(t_value mem_value, t_addr target_address, uint16 inst) {
    uint8 opcode = (inst >> I_OPCODE_SHIFT) & 0x0F; // opcode on bits 4 to 7
            sim_printf("\n[Mem_OP_Reg_To_Reg] opcode: %#05x mem_value: %#05x\n", opcode, mem_value);
    uint8 s_byte, d_byte;
    uint16 i, data;
    uint16 carry, overflow;
    switch (opcode) {
        case 0x00:  // "LDA"
            cpu_state.reg_A = mem_value;
            set_condition_codes_load(cpu_state.reg_A);
            break;

        case 0x01:  // "LDE"
            cpu_state.reg_E = mem_value;
            set_condition_codes_load(cpu_state.reg_E);
            break;

        case 0x02:  // "LDX"
            cpu_state.reg_X = mem_value;
            set_condition_codes_load(cpu_state.reg_X);
            break;

        case 0x03:  // EOR - Exclusive OR
            cpu_state.reg_A ^= mem_value;
            set_condition_codes_load(cpu_state.reg_A);
            break;

        case 0x04:  // "LEA" Load Effective address into A
            Complex_Mem_OP_Reg_To_Reg(opcode, inst, target_address, mem_value);
            break;

        case 0x05:  // "ADD"
            carry = 0;
            cpu_state.reg_A =
                add16(cpu_state.reg_A, mem_value, &carry, &overflow);
            set_condition_codes_arithmetic(cpu_state.reg_A, carry, overflow);
            break;

        case 0x06:  // "SUB" A - M => A
            carry = 0;
            cpu_state.reg_A =
                sub16(cpu_state.reg_A, mem_value, &carry, &overflow);
            set_condition_codes_arithmetic(cpu_state.reg_A, carry, overflow);
            break;

        case 0x07:  // IOR - Inclusive OR
            cpu_state.reg_A = cpu_state.reg_A | mem_value;
            set_condition_codes_load(cpu_state.reg_A);
            break;

        case 0x08:
            /* DIV - Divide (optional) */
            if (!(cpu_unit.flags & UNIT_MULDIV)) return MM_INVINS;
            if (div32(cpu_state.reg_E, cpu_state.reg_A, mem_value,
                      &cpu_state.reg_A, &cpu_state.reg_E) != 0) {
                cpu_state.OV = 1;
            } else {
				set_condition_codes_load(cpu_state.reg_A);
			}
            break;

        case 0x09:
            /* AND - Logical AND */
            cpu_state.reg_A &= mem_value;
            set_condition_codes_load(cpu_state.reg_A);
            break;

        case 0x0A:  // CPS
            Complex_Mem_OP_Reg_To_Reg(opcode, inst, target_address, mem_value);
            break;

        case 0x0B:
            /* CMP - Compare 
            	C	|OV	|	
            	0	|0	|	(A)> Y2
				0	|1	|	(A)< Y2
				1	|0	|	(A)= Y2
            */
//    sim_printf("\n[CMP] carry = %#05x \n", cpu_state.C);
//    sim_printf("\n[CMP] overflow = %#010x \n", cpu_state.OV);
            set_condition_codes_compare(cpu_state.reg_A, mem_value, 0);
//    sim_printf("\n[CMP after codes_compare] carry = %#05x \n", cpu_state.C);
//    sim_printf("\n[CMP] overflow = %#05x \n", cpu_state.OV);
            break;

        case 0x0C:
            /* MUL - Multiply */
            if (!(cpu_unit.flags & UNIT_MULDIV))
            	return MM_INVINS;
            mul32(cpu_state.reg_A, mem_value, &cpu_state.reg_E,
                  &cpu_state.reg_A);
            set_condition_codes_load(cpu_state.reg_E);
            break;

        case 0x0D:
            /* LBL - Load Byte Left
             * Operand is loaded in leftmost byte of A-register
             * Rightmost byte of A register is unaffected.
             */
//    sim_printf("\n[LBL] right: %#05x\n", cpu_state.reg_A & 0x00FF);
//    sim_printf("\n[LBL] mem_value: %#05x\n", mem_value);
//    sim_printf("\n[LBL] mem_value & 0xFF00: %#05x\n", mem_value & 0xFF00);
            cpu_state.reg_A =
                (cpu_state.reg_A & 0x00FF) |  // Rightmost byte of A register is unaffected
                ((mem_value << 8) & 0xFF00);  // Memory's left byte is loaded in A's left byte
            set_condition_codes_load(cpu_state.reg_A);
//    sim_printf("\n[Load Byte Left] A: %#05x\n", cpu_state.reg_A);
            break;

        case 0x0E:
            /*
             * LBR - Load Byte Right
             * Leftmost byte of A-register cleared.
             */
            cpu_state.reg_A =
                ((mem_value) & 0x00FF);  // Load memory's right byte into A,
                                         // clear A's left byte
            set_condition_codes_load(cpu_state.reg_A);
            break;

        case 0x0F:
            /*
             * LBX - Load Byte Right into X
             * Memory's right byte is loaded in X-register's rightmost byte.
             * Left byte of X-register is cleared.
             */
            cpu_state.reg_X =
                (mem_value & 0x00FF);  // Memory's right byte is loaded and left
                                       // byte of X-register is cleared.
            set_condition_codes_load(cpu_state.reg_X);
            break;
            
	default:
	    sim_printf("\n[Mem_OP_Reg_To_Reg] unknown opcode %#04x, inst %#06x\n",
		       opcode, inst);
	    return TRAP_II;
    
    }
    return 0;
}

// Complex cases
uint16 Complex_Mem_OP_Reg_To_Reg(uint8 opcode, uint16 inst, t_addr target_address, t_value mem_value) {
    uint16 disp = inst & I_DISP_MASK;
            sim_printf("\n[Complex_Mem_OP_Reg_To_Reg] opcode: %#010x\n", opcode);
    switch (opcode) {
        case 0x04:  // "LEA"
            cpu_state.reg_A = (target_address - cpu_state.reg_G) & 0x0FFFF;
            set_condition_codes_load(cpu_state.reg_A);
            break;

        case 0x0A:
            /* CPS - Compare String (optional)
                * A byte in main memory is compared sequentially with each byte
               of a string starting at an address defined relative to the G-base
               by the contents of register A, and having a length specified in
               register E.
                    - If the reference byte is found in the string, its address
               relative to the G-base is held in register A, and register E
               contains the length of the remaining part of the string.
                    - If the reference byte is not found in the string, register
               A contains the starting address relative to the G-base, and the
               final content of register E is zero.
                */
            if (!(cpu_unit.flags & UNIT_EXTINS)) 
            	return MM_INVINS;
            uint16 s_byte = read_byte(target_address); // Only one byte is compared to each byte of the string
            uint16 i = 0;
            for (; i < cpu_state.reg_E; i++) {
                uint16 d_byte = read_byte(cpu_state.reg_G + cpu_state.reg_A + i);
                if (s_byte != d_byte) {
                    set_condition_codes_string(0, s_byte < d_byte);
                    break;
                }
            }
            if (i == cpu_state.reg_E) 
            	set_condition_codes_string(1, 0);
            break;

	default:
	    sim_printf("\n[Complex_Mem_OP_Reg_To_Reg] unknown opcode %#04x, inst %#06x\n",
		       opcode, inst);
	    return TRAP_II;
                
    }
    return 0;
}

/*
 * All address modes with memory operation on register and is stored in memory
 *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
 *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *      | y y  y | y| x x  x  x |     displacement      |
 *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *
 *    "DLD", "STA", "STE", "STX", "SBL", "SBR", "DST", "ADM",
 *    "SPA", "STS", "FAD", "FSU", "FMU", "FDV", "TRS", "MVS",
 *
 */
uint16 Reg_OP_Mem_To_Mem(uint16 inst, t_addr target_address, uint32 mode) {
    uint8 opcode = (inst >> I_OPCODE_SHIFT) & 0x0F; // opcodes on bits 4 to 7
            sim_printf("\n[Reg_OP_Mem_To_Mem] opcode: %#010x\n", opcode);
    uint16 data, data2, result;
    uint16 carry, overflow;
    int i;
    uint16* trappc = &cpu_state.reg_P;
    switch (opcode) {
        case 0x01:
            /* STA - Store A */
            write_word(target_address, cpu_state.reg_A);
            break;

        case 0x02:
            /* STE - Store E */
            write_word(target_address, cpu_state.reg_E);
            break;

        case 0x03:
            /* STX - Store X */
            write_word(target_address, cpu_state.reg_X);
            break;

        case 0x04:
            /* SBL - Store Byte Left
             * Store A register's left byte in memory's left byte
             */
            data = read_word(target_address);
            write_word(target_address,    // Store result in memory
                       (data & 0x00FF) |  // Keep memory's right byte
                           (cpu_state.reg_A &
                            0xFF00));  // Select A register's left byte
            break;

        case 0x05:
            /*
             * SBR - Store Byte Right
             * Rightmost byte of register A is stored in memory
             */
            data = read_word(target_address);
            write_word(
                target_address,
                (data & 0xFF00) |  // Left byte of memory is unchanged
                    ((cpu_state.reg_A) &
                     0x00FF));  // Rightmost byte of register A is selected
            break;

        case 0x06:
            /* DST - Double Store */
            write_word(target_address, cpu_state.reg_E);
            write_word((target_address + 2) & 0x0FFFF, cpu_state.reg_A);
            break;

        case 0x07:
            /* ADM - Add to Memory */
            data = read_word(target_address);
            carry = 0;
            result = add16(data, cpu_state.reg_A, &carry, &overflow);
            write_word(target_address, result);
            cpu_state.reg_A = result;
            set_condition_codes_arithmetic(result, carry, overflow);
            break;

        case 0x08:
            /* SPA - Store Program Address */
            cpu_state.reg_A = (cpu_state.reg_P + 4 - 2 + GPRIME) & 0x0FFFF; // minus 2 because we preincrement P before simulating the instruction
            write_word(target_address, cpu_state.reg_A);
            break;

        case 0x09:
            /* STS - Store Selective */
            data = read_word(target_address);
            result =
                (data & ~cpu_state.reg_E) | (cpu_state.reg_A & cpu_state.reg_E);
            write_word(target_address, result);
            set_condition_codes_load(result);
            break;

        case 0x0E:
            /* 
            * TRS - Translate String (optional) 
            * A string beginning at an address defined with respect to G-base by the contents of A-register and 
            * 	whose length is specified in E-register 
            * 	is translated byte per byte through the translation table by TRS instruction.
            * 
            * Starting from Y calculated address, the origin string is overwritten byte per byte by the result string.
            * Translation table creation is obviously the user's responsibility.
            */
            
            uint16 table = target_address; // a 256 consecutive byte translation table starting at Y-calculated address
    sim_printf("\n[TRS 1] table: %#05x", table);
    sim_printf("\n[TRS 2] E: %#05x", cpu_state.reg_E);
            for (i = 0; i < cpu_state.reg_E; i++) {
                    // ((A) + (G) + i) = (Y + ((A) + (G) +i)e)
                    // b = ((A) + (G) +i)
                    uint8 b = read_byte(cpu_state.reg_A + cpu_state.reg_G + i);
    sim_printf("\n[TRS 3] b: %#05x", b);
                    uint8 t = read_byte(table + (b & 0xFF));
    sim_printf("\n[TRS 4] t: %#05x", t);
                    // ((A) + (G) + i) = t
    sim_printf("\n[TRS 5] target: %#05x", cpu_state.reg_A + cpu_state.reg_G + i);
                    write_byte(cpu_state.reg_A + cpu_state.reg_G + i, t);
                }
            cpu_state.reg_E = 0;
            
            break;

        case 0x0F:
            /* 
            * MVS DG - Move String (optional) 
            * A byte string beginning at an address defined with respect to G-base by the contents of A-register and
            * 	whose length (in bytes) is specified in E-register, 
            *	is stored in core memory starting from Y-address.
            * When the transfer is over, E-register contents is -1 and A-register contents is unmodified.
            */
            if (!(cpu_unit.flags & UNIT_EXTINS)) 
            	return MM_INVINS;
            {
                for (i = 0; i < cpu_state.reg_E; i++) {
                    uint8 b = read_byte(cpu_state.reg_G + cpu_state.reg_A + i);
                    write_byte(target_address + i, b);
                }
                cpu_state.reg_E = 0xFFFF;
            }
            break;
            
	default:
	    sim_printf("\n[Reg_OP_Mem_To_Mem] unknown opcode %#04x, inst %#06x\n",
		       opcode, inst);
	    return TRAP_II;
                
    }
    return 0;
}

/*
* The content in floating format of the extended accumulator is added to the floating operand contained in memory
* The result is stored in E, A.
* Most significant word in E.
*/
uint16 floating_inst(uint16 inst, uint32 mode, t_addr target_address) {
    uint8 opcode = ((inst & 0x0F00) >> I_OPCODE_SHIFT);
            sim_printf("\n[floating_inst] opcode: %#010x\n", opcode);
    uint16 data, data2;
    
    switch(opcode) {
        case 0x0A:
	    {
            /* FAD - Float Add (optional)
            * Contents in floating format of E, A extended accumulator added with floating operand contained in Y2 -address double-word; 
            * Result in E, A.
            */
            if (!(cpu_unit.flags & UNIT_FP)) 
            	return MM_INVINS;
	    uint16_t op_A = read_word((target_address + 2) & 0x0FFFF);   /* low half */
	    uint16_t op_E = read_word(target_address);                  /* high half */ 
            double a = mitra_to_double(cpu_state.reg_A, cpu_state.reg_E);
    sim_printf("\n[floating_inst] a: %g\n", a);
            double b = mitra_to_double(op_A, op_E);
    sim_printf("\n[floating_inst] b: %g\n", b);
            double r = a + b;
    sim_printf("\n[floating_inst] r: %g\n", r);
            fp_status_t st = double_to_mitra(r, &cpu_state.reg_A, &cpu_state.reg_E);
    sim_printf("\n[floating_inst] E: %#05x\n", cpu_state.reg_E);
    sim_printf("\n[floating_inst] A: %#05x\n", cpu_state.reg_A);
            set_condition_codes_float(st);
            }
            break;

        case 0x0B:
	    {
            /* FSU - Float Subtract (optional) */
            if (!(cpu_unit.flags & UNIT_FP)) 
            	return MM_INVINS;
	    uint16_t op_A = read_word((target_address + 2) & 0x0FFFF);   /* low half */
	    uint16_t op_E = read_word(target_address);                  /* high half */
            double a = mitra_to_double(cpu_state.reg_A, cpu_state.reg_E);
    sim_printf("\n[floating_inst] a: %g\n", a);
            double b = mitra_to_double(op_A, op_E);
    sim_printf("\n[floating_inst] b: %g\n", b);
            double r = a - b;
    sim_printf("\n[floating_inst] r: %g\n", r);
            fp_status_t st = double_to_mitra(r, &cpu_state.reg_A, &cpu_state.reg_E);
    sim_printf("\n[floating_inst] E: %#05x\n", cpu_state.reg_E);
    sim_printf("\n[floating_inst] A: %#05x\n", cpu_state.reg_A);
            set_condition_codes_float(st);
            }
            break;

        case 0x0C:
            {
            /* FMU - Float Multiply (optional) */
	    if (!(cpu_unit.flags & UNIT_FP)) return MM_INVINS;
	    uint16_t op_A = read_word((target_address + 2) & 0x0FFFF);   /* low half */
	    uint16_t op_E = read_word(target_address);                  /* high half */
	    double a = mitra_to_double(cpu_state.reg_A, cpu_state.reg_E);
	    double b = mitra_to_double(op_A, op_E);
	    double r = a * b;
	    fp_status_t st = double_to_mitra(r, &cpu_state.reg_A, &cpu_state.reg_E);
    	    sim_printf("\n[floating_inst] E: %#05x\n", cpu_state.reg_E);
    	    sim_printf("\n[floating_inst] A: %#05x\n", cpu_state.reg_A);
	    set_condition_codes_float(st);
	} 
	break;

        case 0x0D:
            {
            /* FDV - Float Divide (optional) */
	    if (!(cpu_unit.flags & UNIT_FP)) return TRAP_II;
	    uint16_t op_A = read_word((target_address + 2) & 0x0FFFF);
	    uint16_t op_E = read_word(target_address);
	    double a = mitra_to_double(cpu_state.reg_A, cpu_state.reg_E);
	    double b = mitra_to_double(op_A, op_E);
	    if (b == 0.0) return TRAP_II;
	    double r = a / b;
	    fp_status_t st = double_to_mitra(r, &cpu_state.reg_A, &cpu_state.reg_E);
    	    sim_printf("\n[floating_inst] E: %#05x\n", cpu_state.reg_E);
    	    sim_printf("\n[floating_inst] A: %#05x\n", cpu_state.reg_A);
	    set_condition_codes_float(st);
	} 
	break;
      
	default:
	    sim_printf("\n[floating_inst] unknown opcode %#04x, inst %#06x\n",
		       opcode, inst);
	    return TRAP_II;
          
      }
      return 0;
}

/*
* MVS Move String
*
*        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*      |x  x  x | 1 1  1  1  1 |     displacement      |
*      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*
* A byte string beginning at an address defined with respect to G-base by 
*    - the contents of A-register and
*    - whose length (in bytes) is specified in E-register, 
* is stored in core memory starting from V-address.
*
* For alpha varying on a byte basis from (E) - 1 to 0,
* ((G) + (A) + alpha) -> (Y + alpha)
*
* When the transfer is over, E-register contents is -1 and A-register contents is unmodified.
*/
uint16 string_proc(uint16 inst, uint32 mode, t_addr target_address) {
    uint8 opcode = ((inst & 0x1F00) >> I_OPCODE_SHIFT);
            sim_printf("\n[string_proc] opcode: %#010x\n", opcode);
    
    switch(opcode) {
        case 0x1F:  // DL
        case 0x5F:  // DG
        case 0x7F:  // IL
        case 0x9F:  // IGX
        case 0xAF:  // ILX
        /* MVS - MVS moves a string from (G)+(A) to Y (optional) */
//    	sim_printf("\n[string_proc] flags = %#05x", cpu_unit.flags);
//    	sim_printf("\n[string_proc] UNIT_EXTINS = %#05x", UNIT_EXTINS);
//        if (!(cpu_unit.flags & UNIT_EXTINS)) FIXME breaks here
//            	return MM_INVINS;
        for (int alpha = (cpu_state.reg_E - 1); alpha >= 0; alpha--) {
                    uint8 b_moved = read_byte(cpu_state.reg_G + cpu_state.reg_A + alpha);
                    write_byte(target_address + alpha, b_moved);
                }
        cpu_state.reg_E = -1;
        break;
    
	default:
	    sim_printf("\n[string_proc] unknown opcode %#04x, inst %#06x\n",
		       opcode, inst);
	    return TRAP_II;
        
    }
    return 0;
}

void call_section(t_addr target_address) {
            /* CLS Call section
            * As displacement is limited to 256 bytes, a program is made up of a number of sections individually characterized by a local base L and a program base P.
            * A CLS is basically a branch instruction providing connection between a given section ("calling") and another section ("called") of the same program,
            * while ensuring permanent communications between the two sections as well as an easy return means.
            *
            * Involved elements
		- Program's PRT
		- First two words of the calling section's LDS
		- Contents of CLS calculated address which contains the called section number.
	    * Operation of the CLS
		- L and P base values, relative to G, of the calling section are stored in the first two words of the called section's LDS.
		- With the section number, L and P base values of the called section are fetched and stored in the corresponding registers.
		- A branch is made at the called section.
	    *
	    * Communication with the calling section
	    * Three methods are available for transferring the parameters between calling and called sections:
		1) Through the Common Data Section (CDS)
		2) In indirect local (IL) or indirect local indexed (ILX) addressing modes, via the second word of the LDS.
		3) Via A, E, X registers and/or C or O indicators.
            *
            * Function:
            * (P) - G' -> (((G) - 4Y) + (G))
            * (L) - G' -> (((G) - 4Y) + (G) +2)
            * (G) + ((G) - 4Y) -> (L)
            * (G) + ((G) - 4Y + 2) -> (P)
            */ 
            uint16 section = target_address;
	    sim_printf("\n[call_section] target_address=%u section=%u P=%#06x G=%#06x GPRIME=%#06x\n",
            target_address, section, cpu_state.reg_P, cpu_state.reg_G, GPRIME);            
            
            uint16 called_Lbase = read_word((cpu_state.reg_G - 4 * section) & 0x0FFFF);		// Lbase = ((G) - 4Y)
            uint16 called_Pbase = read_word((cpu_state.reg_G - 4 * section + 2) & 0x0FFFF);	// ((G) - 4Y + 2)
                
            uint16 LDS = (called_Lbase + cpu_state.reg_G) & 0x0FFFF;		// LDS = (G) + ((G) - 4Y) = (G) + Lbase
            
            write_word(LDS, (cpu_state.reg_P - 2 - GPRIME) & 0x0FFFF);		// minus 2 because we preincrement P before simulating the instruction
            write_word(LDS + 2, (cpu_state.reg_L - GPRIME) & 0x0FFFF);
            
            cpu_state.reg_L = LDS; 						// L = LDS = (G) + ((G) - 4Y)
            cpu_state.reg_P = (called_Pbase + cpu_state.reg_G) & 0x0FFFF;	// P = (G) + ((G) - 4Y + 2) = (G) + Pbase
        }

void CSV_instr(t_addr target_address) {
            /* CSV DL
            * Motivation:
	    * Any Mitra-15 program, including any OS, is made up of a number of sections individually characterized by a local base L and a program base P. 
	    * For an OS, the local base L and a program base P values are entered in the PRTS (Supervisor's PRT).
	    * The PRTS maybe located anywhere in the memory and it is pointed at through absolute address 12 (decimal). 
	    * An OS operates in Master mode and overrides memory protection.
	    *
	    * Operation:
	    * - L- and P-base values, relative to G of the current section and its indicators are stored in the first three words of the calling program's CDS.
	    * - L- and P-base values are fetched with the section number .
	    * - MA and PR indicators are forced to 1
	    * - A branch is made at the called section.
	    *
	    * Purpose of the CSV:
	    * A CSV is basically a branch instruction providing connection between a user program section and a supervisor section.
	    * The CSV instruction ensures the re-entrance of the Supervisor section and an easy return to the user program.            
	    *             
	    * (P) - (G) -> ((G))
	    * (L) - (G) -> ((G) + 2)
	    *
	    * Indicators -> ((G) + 4)
	    * 1 -> PR
	    * 1 -> MS
	    *
	    * ((12) - 4 * target_address) -> (L)
	    * ((12) - 4 * target_address + 2) -> (P)
	    *             
	    * &C = adresse absolue 12 (0x0C) : pointeur vers la PRTS
	    * &E = adresse absolue 14 (0x0E) : nombre max de sections superviseurs
            */
            
	    /* ---- Contrôle de borne ---- */
	    uint16 max_sections = read_word(0x0E);
	    if (target_address >= max_sections) {
	        sim_printf("\n[CSV] section %u >= max_sections %u -> TRAP_VM\n", target_address, max_sections);
	        mitra_trap(TRAP_VM, cpu_state.reg_P);
	        return;
	    }

	    /*  ---- Sauvegarde de P, L et des indicateurs dans la CDS ---- */
            write_word(cpu_state.reg_G, (cpu_state.reg_P - cpu_state.reg_G - 2) & 0x0FFFF); 	// (P) - (G) minus 2 because we preincrement P before simulating the instruction            
            write_word(cpu_state.reg_G + 2, (cpu_state.reg_L - cpu_state.reg_G) & 0x0FFFF);	// (L) - (G)
            
            // Indicators -> ((G) + 4)
	    /* Indicateurs sur les bits 14..10 — format identique à RSV */
	    write_word(cpu_state.reg_G + 4, set_indicators());

	    /* ---- Forçage des indicateurs ---- */
            cpu_state.MS = 1;
            cpu_state.PR = 1;

	    /* ---- Initialisation de L et P depuis le contenu du SRDN ---- */
	    uint16 PRTS_addr = read_word(0x0C);                 /* (&C) is address 12  in decimal*/
	    uint16 srdn_addr = (PRTS_addr - 4 * target_address) & 0x0FFFF;

	    cpu_state.reg_L = read_word(srdn_addr);              /* ((&C) - 4N)     */
	    cpu_state.reg_P = read_word(srdn_addr + 2);          /* ((&C) - 4N + 2) */
	    sim_printf("\n[CSV_instr] P = %#05x", cpu_state.reg_P);
}

uint16 test_and_set(uint32 mode, t_addr target_address) {
            /* 
            * TES 
            * This instruction tests and clears a memory location without being interrupted. Initial value loaded in A.
            * Test result loaded in the indicators. 
            * This instruction is used when several processors work in a common memory area, for example 
            * to enable a processor to perform an "occupation test" on a table in memory.
            * I guess it is used as this:
            *    - The content of the occupation table is saved in A
            *    - If A is zero it means, another Mitra-15 locked the memory (Mitra-15's memory could be accessed by up to 4 Mitra-15 CPU).
            *
            * P:
            *	- (disp)2 -> A
            *	- 0 -> (disp)2
            *
            * PX:
            *	- (disp + (X))2 -> A
            *	- 0 -> (disp + (X))2
            *
            * DL:
            *	- (disp + (L))2 -> A
            *	- 0 -> (disp + (L))2
            */
//            if (mode != 1) 
//            	return MM_PRVINS;
            // temporarily mask all interrupts
            uint16 intmask = cpu_state.intrpt_mask;
            cpu_state.intrpt_mask = 0xFFFF;
            
            cpu_state.reg_A = read_word(target_address);
            
            // Manual p. VII-102 says TES clears the tested word and its protection bit.
            write_word((target_address & 0xFFFE), 0);           
            MP[VA_TO_PA((target_address & 0xFFFE))] = false;
            
            /* Weird condition codes setup: Manual VII-l02
            *   C | OV | test
			*   0 |  1 | target_address > 0
			*   0 |  0 | target_address < 0
			*   1 |  0 | target_address = 0
			*/
			cpu_state.C = (target_address == 0) ? 1 : 0;
			cpu_state.OV = (target_address > 0) ? 1 : 0;
            
            // restore interrupt mask
            cpu_state.intrpt_mask = intmask;
            
            return 0;
}

/*
* LDP
*
* This feature provides full protection to any memory area against unwanted attemps to alter its contents.
* The protection its assigned on a dynamic basis (LDP instruction): A protection "lock" is associated with every memory word. 
* Besides, an indicator of the program status (PR) acts as a "key" : 
* - when this indicator is set to I, the program is able to gain access to all memory locations; 
* - otherwise the program can only gain access to unprotected areas whose lock value is O.
* 
* Function:
For X varying 0 and (E)-1 {
	N15 -> bp((A) + 2X)
	}
Upon execution:
	E = -1
	A = Address of the first non-processed word
LDP loads the protection bit into a word string.
The starting address is in A and the string length is specified in E.

Each protection bit is loaded with the value of bit 15 in the calculated operand (contents of calculated address). 
The string is protected when this bit is set (1), otherwise, there is no protection.

Modified elements:
- Registers: E and A
- Memory locations: (Y2) to (Y2 + (E) - 1)

*/
uint16 load_mem_protect(uint32 mode, t_addr target_address) {
	if (mode != 1) 
            	return MM_PRVINS;
    int i = 0;
	for (; i < cpu_state.reg_E; i++) {
            // Load protection lock bit for this address
            MP[target_address] = read_word(cpu_state.reg_A) & 0x0001;
            cpu_state.reg_A += 2;
            target_address += 2;
            }
    // reg_A points already to next address
    cpu_state.reg_E = -1;
    return 0;
}
