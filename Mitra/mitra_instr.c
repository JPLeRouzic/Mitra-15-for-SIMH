
#include "mitra_cpu.h"
#include "mitra_defs.h"
#include "mitra_io.h"

int get_highest_interrupt(void);

uint16 Mem_OP_Reg_To_Reg(t_value mem_value, uint16 target_address, uint16 inst);
uint16 Reg_OP_Mem_To_Mem(uint16 inst, uint16 address, uint32 mode);
void group_3_shift_PX(uint16 inst, uint32 mode);

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

t_stat io_rwd(uint16 inst, t_bool is_write);                 /* the public entry point */
uint16 case_instr_xDR(uint16 inst);

extern UNIT cpu_unit;
extern int susp_stack_ptr;

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
The Mitra-15 is fundamentally a 16-bit word-addressable machine. Data size is
chosen implicitly by the opcode (bits 4–7), not by any extra control bits in the
instruction. Examples: Byte: LBL (Load Byte Left into A) Word: LDA, STA, ADD,
SUB, AND, IOR, etc. Double word: DLD (Double Load): loads two words → E and A
*/

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
} shift_type_t;

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
    SRG_CHX = 0x1E  /* Compute Half X */
} srg_op_t;

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
t_stat one_inst(uint16 inst, uint16 pc, uint32 modeSIMH, uint16* trappc) {
    sim_printf(
        "one_inst: instruction: %#010x, PC: %#010x, mode: %#010x, trap: %#010x",
        inst, pc, modeSIMH, *trappc);

    uint8 opcode = (inst >> I_OPCODE_SHIFT) & 0x1F;
    uint16 disp = inst & I_DISP_MASK;

    *trappc = pc;

    sim_printf(
        "[INST] P=%#010x inst=%#010x (opcode=%#010x disp=%#010x hexcode=%04X) "
        "mode=%s blk=%#010x\n",
        pc, inst, opcode, disp, inst & 0xF000, cpu_state.MS ? "MASTER" : "SLAVE",
        cpu_state.SuspensionStack[susp_stack_ptr].J_reg);
    sim_printf(
        "  regs-before: A=%#010x E=%#010x X=%#010x C=%#010x O=%#010x L=%#010x "
        "G=%#010x\n",
        cpu_state.reg_A, cpu_state.reg_E, cpu_state.reg_X, cpu_state.C,
        cpu_state.OV, cpu_state.reg_L, cpu_state.reg_G);

    /* Check for privileged instruction in slave mode
     * Privileged opcodes are: 0x3A (STR), 0x3B (LDP), 0x3D (TES),
     *  0xF4 (SYS: STM, CLM, DIT, RD, WD), 0xEA (STR in PX)
     */
    uint16 mode = cpu_state.MS;
    if (cpu_state.MS == 0) { /* Slave mode */
        uint16 hexcode = inst & 0xF000;
        // The bug is that the code doesn't handle the RSV instruction's privileged check. RSV is at 0xF10C. 
        // The code should check hexcode == 0xF000 && opcode == 0x01 && (inst & 0x000F) == 0x0C 
        if ((hexcode == 0x3000 && 						// 3Axx, 3Bxx, 3Dxx
             (opcode == 0x0A || opcode == 0x0B || opcode == 0x0D)) ||
            (hexcode == 0xE000 && opcode == 0x0A) ||				// EAxx
            (hexcode == 0xF000 && opcode == 0x04) ||				// F4xx SYS instructions
            (inst == 0xEC06 || inst == 0xFC06) ||				// DITR at 0xEC06 or 0xFC06
            (hexcode == 0xF000 && opcode == 0x01 && ((inst & 0x000F) == 0x0C)))	// RSV is at 0xF10C
             {
            /* Check if it's a privileged SYS function */
            if (hexcode == 0xF000 && opcode == 0x04) { /* SYS */
//                if (disp == 0x01 || disp == 0x03 || disp == 0x08 || FIXME actually all 0xF4 SYS are privilegied
//                    disp == 0x0C || disp == 0x20) {
                    /* DIT, RD, WD, STM, CLM, DITR are privileged, so they trap in
                     * slave mode */
                    sim_printf(
                        "  ** privileged SYS function (disp=%#010x) attempted "
                        "in SLAVE mode -> TRAP_VM **\n",
                        disp);
                    return mitra_trap(TRAP_VM, pc);
//                }
            } else {
                sim_printf(
                    "  ** privileged instruction (opcode=%#010x) attempted in "
                    "SLAVE mode -> TRAP_VM **\n",
                    opcode);
                return mitra_trap(TRAP_VM, pc);
            }
        }
    }

    uint16 hexcode = inst & 0xF000;
    uint16 ret_code ; 

    /*
     * It's a first layer of dispatcher per addressing mode. Instructions are
     * encoded by blocks of 16 instructions except blocks 0xC000 and 0xD000 The
     * next layer will execute the addressing mode by finding the effective
     * address The third layer will attempt at executing most instructions
     * Special cases are handled separately
     */
    switch (hexcode) {
        case 0x0000:
            ret_code = group_1_DL(inst);
            break;
        case 0x1000:
            ret_code = group_2_DL(inst, mode);
            break;
        case 0x2000:
            ret_code = group_1_P(inst);
            break;
        case 0x3000:
            ret_code = group_3_DL(inst, mode);
            break;
        case 0x4000:
            ret_code = group_1_DG(inst);
            break;
        case 0x5000:
            ret_code = group_2_DG(inst, mode);
            break;
        case 0x6000:
            ret_code = group_1_IL(inst);
            break;
        case 0x7000:
            ret_code = group_2_IL(inst, mode);
            break;
        case 0x8000:
            ret_code = group_1_IGX(inst);
            break;
        case 0x9000:
            ret_code = group_2_IGX(inst, mode);
            break;
        case 0xA000:
            ret_code = group_1_ILX(inst);
            break;
        case 0xB000:
            ret_code = group_2_ILX(inst, mode);
            break;
        case 0xC000: {
            uint16 Crpcode = inst & 0x0800;
            switch (Crpcode) {
                case 0x0000:
                    ret_code = group_4_RP(inst);
                    break;
                case 0x0800:
                    ret_code = group_4_RM(inst);
                    break;
            }
            break;
        }
        case 0xD000: {
            uint16 Drpcode = inst & 0x0800;
            switch (Drpcode) {
                case 0x0000:
                    ret_code = group_5_IL(inst);
                    break;
                case 0x0800:
                    ret_code = group_5_IG(inst);
                    break;
            }
            break;
        }
        case 0xE000:
            ret_code = group_3_PX(inst, mode);
            break;
        case 0xF000:
            ret_code = group_3_P(inst, mode);
            break;
    }

    sim_printf(
        "  regs-after : P=%#010x A=%#010x E=%#010x X=%#010x C=%#010x O=%#010x "
        "L=%#010x G=%#010x\n",
        cpu_state.reg_P, cpu_state.reg_A, cpu_state.reg_E, cpu_state.reg_X,
        cpu_state.C, cpu_state.OV, cpu_state.reg_L, cpu_state.reg_G);

    /* Check for traps after instruction execution */
    if (cpu_state.trap_pending) {
        int cause = mitra_resolve_trap_cause(cpu_state.trp_req_bits);
        if (cause < 0) {
            /* Defensive: flag was set but no cause bit is actually present. */
            sim_printf(
                "  -> trap_pending set but trp_req_bits=0, clearing spurious "
                "trap\n");
            cpu_state.trap_pending = FALSE;
        } else {
            cpu_state.trap_cause = cause;
            sim_printf(
                "  -> trap pending (cause=%d %s) after execution, dispatching "
                "mitra_trap()\n",
                cause, mitra_trap_name(cause));
            return mitra_trap(cause, pc);
        }
    }
    return SCPE_OK;
}

/*
 * Find effective address for instructions in 0XXX form
 */
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
     *    Byte, word or double-word, located in the first 256 bytes of the local
     * segment. Y = (L) + D
     */
    uint16 disp = inst & I_DISP_MASK; /* 8-bit unsigned displacement / param */
    uint16 target_address;
    uint16 ret_code =0;

    target_address = (cpu_state.reg_L + disp) & 0x7FFF;
    t_value target_value = read_word(target_address);
    Mem_OP_Reg_To_Reg(target_value, target_address, inst);

    return ret_code;
}

/*
 * Find effective address for instructions in 1XXX form
 */
uint16 group_2_DL(uint16 inst, uint32 mode) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      | 0 0  0 | 1| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *
     *    "DLD", "STA", "STE", "STX", "SBL", "SBR", "DST", "ADM",
     *    "SPA", "STS", "FAD", "FSU", "FMU", "FDV", "TRS", "MVS",
     *
     *    Byte, word or double-word, located in the first 256 bytes of the local
     * segment. Y = (L) + D
     */
    uint8 opcode = (inst >> I_OPCODE_SHIFT) & 0x0FF;
    uint16 disp = inst & I_DISP_MASK;
    uint16 target_address;
    uint16 ret_code =0;

    target_address = (cpu_state.reg_L + disp) & 0x7FFF;
    if(opcode == 0x1A) {
            // FAD DL, Floating ADd (option)
    	    floating_inst(inst, mode, target_address);
    	    return ret_code;
    	}
    else if(opcode == 0x1F) {
            // MVS DL, Floating ADd (option)
    	    string_proc(inst, mode, target_address);
    	    return ret_code;
    	}

    /* else */
    Reg_OP_Mem_To_Mem(inst, target_address, mode);
    return ret_code;
}

/*
 * Find effective address for instructions in 2XXX form
 */
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
    uint16 disp = inst & I_DISP_MASK;
    uint16 target_address;
    uint16 ret_code =0;

    target_address = (cpu_state.reg_P - 2);
    /* The operand IS the displacement field itself (zero-extended), not the
     * word at target_address -- target_address is only passed through for
     * instructions like LEA that need the instruction's own location. */
    t_value target_value = read_word(target_address);
    Mem_OP_Reg_To_Reg((t_value)disp, target_address, inst);

    return ret_code;
}

/*
 * Find effective address for instructions in 3XXX form
 */
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
     * Byte, word or double-word located in the first 256 bytes of the local
     * segment. Y = (L) + D
     */
    uint8 opcode = (inst >> I_OPCODE_SHIFT) & 0x0FF;
    uint16 disp = inst & I_DISP_MASK;
    uint16 data;
    uint16 target_address;
    uint16 ret_code =0;
    uint16 count;
    target_address = (cpu_state.reg_L + disp) & 0x7FFF;

    switch (opcode) {
        case 0x30:
        case 0x31:
            group_3_shift_DL(inst, mode);
            break;
        case 0x32:  // ICX DL, reg_X is incremented by the content of (reg_L
                    // added to displacement)
            cpu_state.reg_X =
                (cpu_state.reg_X + read_word(target_address)) & 0x7FFF;
            set_condition_codes_load(cpu_state.reg_X);
            break;
        case 0x33:  // DCX DL, reg_X is incremented by the content of (reg_L
                    // added to displacement)
            cpu_state.reg_X =
                (cpu_state.reg_X - read_word(target_address)) & 0x7FFF;
            set_condition_codes_load(cpu_state.reg_X);
            break;
        case 0x34:
            break;
        case 0x35:  // ICL DL, reg_L is incremented by the content of (reg_L
                    // added to displacement)
            cpu_state.reg_L =
                (cpu_state.reg_L + read_word(target_address)) & 0x7FFF;
            break;
        case 0x36:  // DCL DL, reg_X is decremented by the content of (reg_L
                    // added to displacement)
            cpu_state.reg_L =
                (cpu_state.reg_L - read_word(target_address)) & 0x7FFF;
            break;
        case 0x37:
            /* CSV */
            CSV_instr(target_address);
            break;
        case 0x38: 
            call_section(target_address);
            break;
        case 0x39:  // LDR DL
            case_instr_xDR(inst);
            break;

        case 0x3A:  // STR DL
            case_instr_xDR(inst);
            break;
            
        case 0x3B:
            /* LDP LoaD memory Protection */
            load_mem_protect(mode, target_address);
            break;
        case 0x3C:
            shift_instr(inst, mode, target_address);
            break;
        case 0x3D:
            test_and_set(mode, target_address);
            break;
        case 0x3E:
        case 0x3F:
            break;
    }
    return ret_code;
}

void group_3_shift_DL(uint16 inst, uint32 mode) {
    uint16 count;
    uint8 opcode = (inst >> I_OPCODE_SHIFT) & 0x0FF;
    uint16 disp = inst & I_DISP_MASK;
    t_addr target_address = (cpu_state.reg_L + disp) & 0x7FFF;
    switch (opcode) {
	    case 0x30: {
		/* SHR - Shift Register (DL mode: shift word read from memory) */
		/* Shift word from memory (16-bit); use low byte for parameters.
		 * Bit layout of shift parameter byte: bits[7:5]=type, bits[4:0]=count.
		 */
		uint8 shr_word = (uint8)(read_word(target_address) & 0xFF);
		uint16 count = shr_word & 0x1F;
		shift_type_t type = (shr_word >> 5) & 0x07;
		switch (type) {
		    case SHIFT_SLLS:
		        cpu_state.reg_A = shift_lls(cpu_state.reg_A, count);
		        break;
		    case SHIFT_SRCS:
		        cpu_state.reg_A = shift_srcs(cpu_state.reg_A, count);
		        break;
		    case SHIFT_SAD:
		        shift_sad(&cpu_state.reg_E, &cpu_state.reg_A, count);
		        break;
		    case SHIFT_SLCD:
		        shift_lcd(&cpu_state.reg_E, &cpu_state.reg_A, count);
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
		        shift_rcd(&cpu_state.reg_E, &cpu_state.reg_A, count);
		        break;
		}
		set_condition_codes_load(cpu_state.reg_A);
	    } break;

	    case 0x31: {
		set_register(inst, mode);
	    } break;
	}
}

/*
 * Find effective address for instructions in 4XXX form
 */
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
     * Byte, word or double-word located in the first 256 bytes of the common
     * segment. Y=(G) + D
     */
    uint16 disp = inst & I_DISP_MASK;
    uint16 target_address;
    uint16 ret_code =0;

    target_address = (cpu_state.reg_G + disp) & 0x7FFF;
    t_value target_value = read_word(target_address);
    Mem_OP_Reg_To_Reg(target_value, target_address, inst);

    return ret_code;
}

/*
 * Find effective address for instructions in 5XXX form
 */
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
     * Byte, word or double-word located in the first 256 bytes of the common
     * segment. Y=(G) + D
     */
    uint8 opcode = (inst >> I_OPCODE_SHIFT) & 0x0FF;
    uint16 disp = inst & I_DISP_MASK;
    uint16 target_address;
    uint16 ret_code =0;

    target_address = (cpu_state.reg_G + disp) & 0x7FFF;
    if(opcode == 0x5A) {
            // FAD DG, Floating ADd (option)
    	    floating_inst(inst, mode, target_address);
    	    return ret_code;
    	}
    else if(opcode == 0x5F) {
            // MVS DG, Floating ADd (option)
    	    string_proc(inst, mode, target_address);
    	    return ret_code;
    	}
    /* else */
    Reg_OP_Mem_To_Mem(inst, target_address, mode);
    return ret_code;
}

/*
 * Find effective address for instructions in 6XXX form
 */
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
     *  Byte, word or double-word located anywhere and pointed at through the
     * local segment.
     */
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    uint16 ret_code =0;

    tmp = read_word(cpu_state.reg_L + disp);
    target_address = (GPRIME + tmp) & 0x7FFF;
    t_value target_value = read_word(target_address);
    Mem_OP_Reg_To_Reg(target_value, target_address, inst);

    return ret_code;
}

/*
 * Find effective address for instructions in 7XXX form
 */
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
     *  Byte, word or double-word located anywhere and pointed at through the
     * local segment.
     */
    uint8 opcode = (inst >> I_OPCODE_SHIFT) & 0x0FF;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    tmp = read_word(cpu_state.reg_L + disp);
    uint16 target_address;
    uint16 ret_code =0;

    target_address = (GPRIME + tmp) & 0x7FFF;
    if(opcode == 0x7A) {
            // FAD IL, Floating ADd (option)
    	    floating_inst(inst, mode, target_address);
    	    return ret_code;
    	}
    else if(opcode == 0x7F) {
            // MVS IL, Floating ADd (option)
    	    string_proc(inst, mode, target_address);
    	    return ret_code;
    	}
    /* else */
    Reg_OP_Mem_To_Mem(inst, target_address, mode);
    return ret_code;
}

/*
 * Find effective address for instructions in 8XXX form
 */
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
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    uint16 ret_code =0;

    tmp = read_word(cpu_state.reg_G + disp);
    target_address = (cpu_state.reg_G + tmp + cpu_state.reg_X) & 0x7FFF;
    t_value target_value = read_word(target_address);
    Mem_OP_Reg_To_Reg(target_value, target_address, inst);

    return ret_code;
}

/*
 * Find effective address for instructions in 9XXX form
 */
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
    uint8 opcode = (inst >> I_OPCODE_SHIFT) & 0x0FF;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    uint16 ret_code =0;

    tmp = read_word(cpu_state.reg_G + disp);
    target_address = (cpu_state.reg_G + tmp + cpu_state.reg_X) & 0x7FFF;
    if(opcode == 0x9A) {
            // FAD IGX, Floating ADd (option)
    	    floating_inst(inst, mode, target_address);
    	    return ret_code;
    	}
    else if(opcode == 0x9F) {
            // MVS IGX, Floating ADd (option)
    	    string_proc(inst, mode, target_address);
    	    return ret_code;
    	}
    /* else */
    Reg_OP_Mem_To_Mem(inst, target_address, mode);
    return ret_code;
}

/*
 * Find effective address for instructions in AXXX form
 */
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
     * Element of cpu_state.reg_A byte, word or double-word array located
     * anywhere and pointed at through the local segment. Y = G' +
     * ((L)+D)+(cpu_state.reg_X)
     */
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    uint16 ret_code =0;

    tmp = read_word(cpu_state.reg_L + disp);
    target_address = (GPRIME + tmp + cpu_state.reg_X) & 0x7FFF;
    t_value target_value = read_word(target_address);
    Mem_OP_Reg_To_Reg(target_value, target_address, inst);
    return ret_code;
}

/*
 * Find effective address for instructions in BXXX form
 */
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
     * Element of a byte, word or double-word array located anywhere and pointed
     * at through the local segment. Y = G' + ((L)+D)+(X)
     */
    uint8 opcode = (inst >> I_OPCODE_SHIFT) & 0x0FF;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    uint16 ret_code =0;

    tmp = read_word(cpu_state.reg_L + disp);
    target_address = (GPRIME + tmp + cpu_state.reg_X) & 0x7FFF;
    if(opcode == 0xBA) {
            // FAD ILX, Floating ADd (option)
    	    floating_inst(inst, mode, target_address);
    	    return ret_code;
    	}
    else if(opcode == 0xBF) {
            // MVS ILX, Floating ADd (option)
    	    string_proc(inst, mode, target_address);
    	    return ret_code;
    	}
    /* else */
    Reg_OP_Mem_To_Mem(inst, target_address, mode);
    return ret_code;
}

/*
 * Find effective address for instructions in EXXX form
 * Parameter, Indexed
 * Operand defined by value plus X-register contents.
 * (Y)==D+(X)
 * Y==(P)
 */
uint16 group_3_PX(uint16 inst, uint32 mode) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  1  1 | 0| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    (Class 1 - PX addressing mode)
     *    "SHR", "SRG", "ICX", "DCX", "",    "ICL", "DCL", "CSV",
     *
     * (Y) = D+(reg_X)
     *  Y = (P)
     */
    uint8 opcode = (inst >> I_OPCODE_SHIFT) & 0x0FF;
    uint16 count;
    uint16 data;
    uint16 value;
    uint16 disp = inst & I_DISP_MASK;
    uint16 target_address;
    uint16 ret_code =0;

    target_address = (disp) & 0x0FF;
    switch (opcode) {
        case 0xE0:
        case 0xE1:
            group_3_shift_PX(inst, mode);
            break;

        /*
         * (Y) = D+(reg_X)
         *  Y = (P)
         */
        case 0xE2:  // ICX PX, reg_X register contents is incremented by the
                    // content of memory pointed by reg_X
            value = read_word(cpu_state.reg_X);
            cpu_state.reg_X = (cpu_state.reg_X + value) & 0x7FFF;
            set_condition_codes_load(cpu_state.reg_X);
            break;
        case 0xE3:  // DCX PX, reg_X register contents is decremented by the
                    // content of memory pointed by reg_X
            value = read_word(cpu_state.reg_X);
            cpu_state.reg_X = (cpu_state.reg_X - value) & 0x7FFF;
            set_condition_codes_load(cpu_state.reg_X);
            break;
        case 0xE4:
            break;
        case 0xE5:  // ICL PX, reg_L register contents is incremented by the
                    // content of memory pointed by reg_X
            value = read_word(cpu_state.reg_X);
            cpu_state.reg_L = (cpu_state.reg_L + value) & 0x7FFF;
            break;
        case 0xE6:  // DCL PX, reg_L register contents is decremented by the
                    // content of memory pointed by reg_X
            value = read_word(cpu_state.reg_X);
            cpu_state.reg_L = (cpu_state.reg_L - value) & 0x7FFF;
            break;
        case 0xE7:
            /* CSV */
            CSV_instr(target_address);
            break;

        case 0xE8: 
            call_section(target_address);
            break;

        case 0xE9:  // LDR PX
            case_instr_xDR(inst);
            break;

        case 0xEB:
            /* LDP LoaD memory Protection */
            load_mem_protect(mode, target_address);
            break;

        case 0xEC:  // Shift special
            shift_instr(inst, mode, target_address);
            break;
    }
    return ret_code;
}

void group_3_Shift_P(uint16 inst, uint32 mode) {
    uint8 opcode = (inst >> I_OPCODE_SHIFT) & 0x0FF;
    uint16 disp = inst & I_DISP_MASK;
    uint8 count = disp & 0x1F;

    srg_op_t srg_op = disp & 0xE0;
    switch (opcode) {
        case 0xF0:
            switch (srg_op) {
                case SHIFT_SLLS:
                    cpu_state.reg_A = shift_lls(cpu_state.reg_A, count);
                    break;
                case SHIFT_SRCS:
                    cpu_state.reg_A = shift_srcs(cpu_state.reg_A, count);
                    break;
                case SHIFT_SAD:
                    shift_sad(&cpu_state.reg_E, &cpu_state.reg_A, count);
                    break;
                case SHIFT_SLCD:
                    shift_lcd(&cpu_state.reg_E, &cpu_state.reg_A, count);
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
                    shift_rcd(&cpu_state.reg_E, &cpu_state.reg_A, count);
                    break;
            }
            set_condition_codes_load(cpu_state.reg_A);
            break;

        case 0xF1:
            set_register(inst, mode);
            break;
    }
}

void group_3_shift_PX(uint16 inst, uint32 mode) {
    uint8 opcode = (inst >> I_OPCODE_SHIFT) & 0x0FF;
    uint16 disp = inst & I_DISP_MASK;
    srg_op_t srg_op = disp & 0x1E;

    switch (opcode) {
	    case 0xE0: {
		shift_type_t type = (disp >> 5) & 0x07;
		uint8 count = disp & 0x1F;
		switch (type) {
		    case SHIFT_SLLS:
		        cpu_state.reg_A = shift_lls(cpu_state.reg_A, count);
		        break;
		    case SHIFT_SRCS:
		        cpu_state.reg_A = shift_srcs(cpu_state.reg_A, count);
		        break;
		    case SHIFT_SAD:
		        shift_sad(&cpu_state.reg_E, &cpu_state.reg_A, count);
		        break;
		    case SHIFT_SLCD:
		        shift_lcd(&cpu_state.reg_E, &cpu_state.reg_A, count);
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
		        shift_rcd(&cpu_state.reg_E, &cpu_state.reg_A, count);
		        break;
		}
		set_condition_codes_load(cpu_state.reg_A);
	    } 
	    break;
	    case 0xE1: {
		set_register(inst, mode);
	    } 
	    break;
	}
}

/* ========== P Mode System Instructions (F0-FF) ========== */
// FIXME many bugs
/*
 * Find effective address for instructions in FXXX form
 */
uint16 group_3_P(uint16 inst, uint32 mode) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  1  1 | 1| x x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    (Class 1 - P addressing mode)
     *    "CLS", "LDR", "STR", "LDP", "SHC", "TES", "",    "",
     */
    uint8 opcode = (inst >> I_OPCODE_SHIFT) & 0x0FF;

    uint16 disp = inst & I_DISP_MASK;
    uint16 count = disp & 0x1F;
    uint16 target_address = disp;
    uint16 ret_code =0;

    uint16 data;

    if (opcode == 0xF4) {
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

            case 0x01: /* DIT - Deactivate Interrupt */
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

        }
    }

    switch (opcode) {
        case 0xF0:
        case 0xF1:
            group_3_Shift_P(inst, mode);
            break;
        case 0xF2:  // ICX P, reg_X register contents is incremented by disp
            cpu_state.reg_X = (cpu_state.reg_X + target_address) & 0x7FFF;
            set_condition_codes_load(cpu_state.reg_X);
            break;
        case 0xF3:  // DCX P, reg_X register contents is decremented by disp
            cpu_state.reg_X = (cpu_state.reg_X - target_address) & 0x7FFF;
            set_condition_codes_load(cpu_state.reg_X);
            break;
        case 0xF4:
            break;
        case 0xF5:  // ICL P, reg_L register contents is incremented by disp
            cpu_state.reg_L = (cpu_state.reg_L + target_address) & 0x7FFF;
            break;
        case 0xF6:  // DCL P, reg_L register contents is decremented by disp
            cpu_state.reg_L = (cpu_state.reg_L - target_address) & 0x7FFF;
            break;
        case 0xF7:
            /* CSV */
            CSV_instr(target_address);
            break;

        case 0xF8: 
            call_section(target_address);
            break;

        case 0xF9:  // LDR P
            case_instr_xDR(inst);
            break;

        case 0xFA:  // STR P
            case_instr_xDR(inst);
            break;

        case 0xFB:
            /* LDP LoaD memory Protection */
            load_mem_protect(mode, target_address);
            break;
        case 0xFC:
            shift_instr(inst, mode, target_address);
            break;
        case 0xFD:
            test_and_set(mode, target_address);
            break;
        case 0xFE:
        case 0xFF:
            break;
    }
    return ret_code;
}

/*
 * Find effective address for instructions in CXXX form (bit 4 == 0)
 */
uint16 group_4_RP(uint16 inst) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  1  0 | 0| 0 x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    (Class 2 - RP addressing mode)
     *    "BCT", "BRX", "BOT", "BCF", "BAN", "BAZ", "BOF", "BRU",
     *
     *	  Relative Plus: Y = (reg_P) + (2 * disp)
     */
    uint8 opcode = (inst >> I_OPCODE_SHIFT) & 0x0FF;
    uint16 disp = inst & I_DISP_MASK;
    uint16 target_address;
    uint16 ret_code =0;

    target_address = cpu_state.reg_P + (disp << 1); // (reg_P) + (2 * disp),'2 *' is << 1, not << 2
    switch (opcode) {
        case 0xC0:
            /* 
            * BCT - Branch on Carry True (RP mode) 
            * If Carry indicator is set (1), Y calculated address in loaded into P-register and execution continues at Y -address.
            */
            if (cpu_state.C)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF; // If Carry indicator is reset (O), execution proceeds in sequence.
            break;
        case 0xC1:
            /* BRX
            * Y-address is loaded into P-register and execution proceeds at Y-address.
            * (P) + (2 * disp) + (2 * (X)) -> (P)
	    */
            cpu_state.reg_P = (target_address + (cpu_state.reg_X << 1)) & 0x7FFF;
            break;
        case 0xC2:
            /* BOT - Branch on Overflow True (RP mode) */
            if (cpu_state.OV)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xC3:
            /* BCF - Branch on Carry False (RP mode) */
            if (!cpu_state.C)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
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
            /* 
            * BRU - Branch Unconditional (RP mode) 
            */
            cpu_state.reg_P = target_address; // (reg_P) + (2 * disp)
            break;
    }
}

/*
 * Find effective address for instructions in CXXX form (bit 4 == 1)
 */
uint16 group_4_RM(uint16 inst) {
    /*
     *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *      |1  1  0 | 0| 1 x  x  x |     displacement      |
     *      +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
     *    (Class 2 - RM addressing mode)
     *    "BCT", "BRX", "BOT", "BCF", "BAN", "BAZ", "BOF", "BRU",
     *
     *	  Relative Minus: Y = (reg_P) - (2 * disp)
     */
    uint8 opcode = (inst >> I_OPCODE_SHIFT) & 0x0FF;
    uint16 disp = inst & I_DISP_MASK;
    uint16 target_address;
    uint16 ret_code =0;

    target_address = cpu_state.reg_P - (disp << 1);
    switch (opcode) {
        case 0xC8:
            /* 
            * BCT - Branch on Carry True (RM mode) 
            * If Carry indicator is set (1), Y calculated address in loaded into P-register and execution continues at Y -address.
            * 
            */
            if (cpu_state.C)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF; // If Carry indicator is reset (O), execution proceeds in sequence.
            break;
        case 0xC9:
            /* BRX
            * Y-address is loaded into P-register and execution proceeds at Y-address.
            * (P) - (2 * disp) - (2 * (X)) -> (P)
	    */
            cpu_state.reg_P = (target_address - (cpu_state.reg_X << 2)) & 0x7FFF;
            break;
        case 0xCA:
            if (cpu_state.OV)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xCB:
            /* BCF RM mode */
            if (!cpu_state.C)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
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
            if (!cpu_state.OV)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xCF:
            /* 
            * BRU - Branch Unconditional (RM mode) 
            */
            cpu_state.reg_P = target_address;
            break;
    }
    return ret_code;
}

/*
 * Find effective address for instructions in DXXX form (bit 4 == 0)
 * Indirect Local: Y = G' + ((L) + D)
 */
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
    uint8 opcode = (inst >> I_OPCODE_SHIFT) & 0x0FF;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    uint16 ret_code =0;

    tmp = (cpu_state.reg_L + disp) & 0x7FFF;
    target_address = GPRIME + tmp;
    
    switch (opcode) {
        case 0xD0:
            /* 
            * BCT - Branch on Carry True (IL mode)
            */
            if (cpu_state.C)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xD1:
            /* 
            * BRX 
            * In indirect addressing, the index is used for a pre-indexing executed prior to indirect addressing.
            * IL: (disp + (L) + (X)) + G' -> (P)
            */
            t_addr ind_pointer = read_word(cpu_state.reg_L + cpu_state.reg_X + disp);
            cpu_state.reg_P = (GPRIME + ind_pointer) & 0x7FFF;
            break;
        case 0xD2:
            if (cpu_state.OV)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xD3:
            /* BCF IL mode */
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
            /* 
            * BRU - Branch Unconditional (IL mode) 
            */
            cpu_state.reg_P = target_address;
            break;
    }
    return ret_code;
}

/*
 * Find effective address for instructions in DXXX form (bit 4 == 1)
 */
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
    uint8 opcode = (inst >> I_OPCODE_SHIFT) & 0x0FF;
    uint16 disp = inst & I_DISP_MASK;
    uint16 tmp;
    uint16 target_address;
    uint16 ret_code =0;

    tmp = read_word(cpu_state.reg_G + disp);
    target_address = (GPRIME + tmp) & 0x7FFF;
    switch (opcode) {
        case 0xD8:
            /* 
            * BCT - Branch on Carry True (IL mode)
            */
            if (cpu_state.C)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xD9:
            /* 
            * BRX 
            * In indirect addressing, the index is used for a pre-indexing executed prior to indirect addressing.
            * IG: (disp + (G) + (X)) + G' -> (P)
            */
            t_addr ind_pointer = read_word(cpu_state.reg_G + cpu_state.reg_X + disp);
            cpu_state.reg_P = (GPRIME + ind_pointer) & 0x7FFF;
            break;
        case 0xDA:
            if (cpu_state.OV)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xDB:
            /* BCF IG mode */
            if (!cpu_state.C)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xDC:
            if (cpu_state.reg_A & 0x8000)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xDD:
            if (cpu_state.reg_A == 0)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xDE:
            if (!cpu_state.OV)
                cpu_state.reg_P = target_address;
            else
                cpu_state.reg_P = (cpu_state.reg_P + 2) & 0x7FFF;
            break;
        case 0xDF:
            /* 
            * BRU - Branch Unconditional (IG mode) 
            */
            cpu_state.reg_P = target_address;
            break;
    }
    return ret_code;
}

