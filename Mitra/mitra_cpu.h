#ifndef MITRA_CPU_H
#define MITRA_CPU_H

#include <stdio.h>
/* ========== Constants and Definitions ========== */
#define PCQ_SIZE        64
#define PCQ_MASK        (PCQ_SIZE - 1)
#define PCQ_ENTRY       pcq[pcq_p = (pcq_p - 1) & PCQ_MASK] = pc

#define HIST_XCT        1
#define HIST_INT        2
#define HIST_TRP        3
#define HIST_MIN        64
#define HIST_MAX        65536
#define HIST_NOEA       0x40000000

/* Unit flags */
#define UNIT_MSIZE      (1 << 0)
#define UNIT_EXTINS     (1 << 1)
#define UNIT_FP         (1 << 2)
#define UNIT_MULDIV     (1 << 3)
#define UNIT_HSINT      (1 << 4)

/* Memory sizes (in words) */
#define MEM_4K          4096
#define MEM_8K          8192
#define MEM_16K         16384
#define MEM_32K         32768
#define MAX_MEM_WORDS   32768
#define MAX_MEM_BYTES   (MAX_MEM_WORDS * 2)

/* Condition code bits (per manual section II-6) */
#define CC_CARRY        0x0001    /* C = 1: zero result or equality */
#define CC_OVERFLOW     0x0002    /* O = 1: negative result or A < operand */
#define CC_ZERO         0x0004    /* Internal: result zero */
#define CC_NEG          0x0008    /* Internal: result negative */

/* Addressing modes - Class 0 (manual page 5-1, 5-2) */
#define AM_DL           0   /* Direct Local: Y = (L) + D */
#define AM_IL           1   /* Indirect Local: Y = G' + ((L) + D) */
#define AM_ILX          2   /* Indirect Local Indexed: Y = G' + ((L) + D) + (X) */
#define AM_DG           4   /* Direct General: Y = (G) + D */
#define AM_IGX          5   /* Indirect General Indexed: Y = G' + ((G) + D) + (X) */
#define AM_P            6   /* Parameter (immediate): operand = D */
#define AM_PX           7   /* Parameter Indexed (class 1 only) */

/* Addressing modes - Class 2 (manual page 5-3) */
#define AM_RP           6   /* Relative Plus: Y = (P) + 2D */
#define AM_RM           7   /* Relative Minus: Y = (P) - 2D */

/* Instruction format decoding */
#define I_MODE_MASK     0xE000
#define I_MODE_SHIFT    13
#define I_OPCODE_MASK   0x1F00
#define I_OPCODE_SHIFT  8
#define I_GROUP_SHIFT   9
#define I_DISP_MASK     0x00FF

/* Fixed GPRIME macro to correctly reference the G register in the current block */
#define GPRIME ((cpu_state.MS) ? cpu_state.reg_G : 0)

#define VA_TO_PA(va) ((va) & 0x7FFF)

/* ========== Trap and Suspension Constants ========== */
/* Trap causes (Section II-8.3) */
#define TRAP_VM         0   /* Mode violation */
#define TRAP_PM         1   /* Memory protection violation */
#define TRAP_AI         2   /* Non-existing address */
#define TRAP_PA         3   /* Parity error */
#define TRAP_II         4   /* Invalid instruction */
#define TRAP_ES         5   /* I/O error */
#define TRAP_WD         6   /* Watchdog timer */

/* Suspension levels (Section II-8.2) */
#define SUSP_STACK_DEPTH    4
#define SUSP_INTERNAL_TRAP  0
#define SUSP_INTERNAL_IT    1
#define SUSP_INTERNAL_PANEL 2
#define SUSP_INTERNAL_PWR   3

/* Interrupt vectors */
static const uint32 int_vec[32] = {
    0, 0, 0, 0,
    VEC_FORK, VEC_DRM, VEC_MUXCF, VEC_MUXCO,
    VEC_MUXT, VEC_MUXR, VEC_HEOR, VEC_HZWC,
    VEC_GEOR, VEC_GZWC, VEC_FEOR, VEC_FZWC,
    VEC_EEOR, VEC_EZWC, VEC_DEOR, VEC_DZWC,
    VEC_CEOR, VEC_CZWC, VEC_WEOR, VEC_YEOR,
    VEC_WZWC, VEC_RTCP, VEC_RTCS, VEC_IPAR,
    VEC_CPAR, VEC_PWRF, VEC_PWRO, 0
};

/* ========== Type Definitions ========== */
/* Memory - word addressable */
extern t_value M[MAX_MEM_WORDS]; // SIMH uses t_addr for addresses and t_value for values.

/* Single structure holds ALL CPU state - for REG table */
#define REG_BLOCS 8 // Number of register blocks
typedef struct {
/* 
 * CPU registers 
 * Each register has a unique address form 0 to 63 (or 127)
 * A high-speed interrupt causes an automatic switching of the register block. 
 * In the new block, the registers have then the same assignment as in block 0, but for other programs.
 * A complex semantic was tried (cpu_state.reg_block[cpu_state.curr_bloc].A) but it didn't compiled correctly with REG cpu_reg[] structure.
 * So now operation in the simulator occurs on a set of shim registers that are made pointing to the correct block.
 * V and W are used by micro-programs.
 */
    uint16 reg_A, reg_E, reg_X, reg_P, reg_L, reg_G, reg_V, reg_W;
    uint8 C, OV;
    uint16 reg_6, reg_8, reg_12;
        
    struct {
        uint16 A, E, X, P, L, G, V, W;
        uint8 C, OV;
    } reg_block[REG_BLOCS];
    
    uint8 curr_bloc; // A pointer to the currect bloc of eight registers A, E, X, P, L, G, V, W
    
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
    uint8 MA; // Interrupt mask
    uint8 PR; // Access to protected areas
    
    /* Interrupt/High speed Interrupt/Suspension/Trap state */
    uint32 intrpt_mask;  /* 32-bit bitmask of pending interrupts */
    uint16 int_lvl;     /* Current interrupt level */
    t_bool high_speed;  /* TRUE if high-speed interrupt */
    uint32 int_reqhi;         /* Highest pending interrupt level */
    
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
} CPU_STATE;

/* Global CPU state instance */
extern CPU_STATE cpu_state;

/* Suspension stack - 4 levels deep */
typedef struct {
    uint16 U_reg;     /* Universal register */
    uint16 J_reg;     /* J register (block selector) */
    uint16 T_reg;     /* T register (micro-PC) */
    uint8  B_ind;     /* B indicator */
    uint8  Tz_ind;    /* Tz indicator */
    uint8  To_ind;    /* To indicator */
    uint8  Ao_ind;    /* Ao indicator */
    uint16 saved_bloc; /* Saved register block */
} SuspContext;

/* ========== Diagnostic Trace Logging ==========
 * Added to help debug segfaults / bad-EA / interrupt-priority bugs.
 * Everything is written to a plain text file, line-buffered and
 * fflush()'d after every line so that the trace survives a crash
 * (including SIGSEGV) instead of being lost in a stdio buffer.
 *
 * Runtime toggles (deposit into the CPU device registers, e.g.
 * from the SIMH console: "d cpu LOG_ENABLE 0" or "d cpu LOG_MEM 0")
 *   mitra_log_enable - master on/off switch (default: on)
 *   mitra_log_inst   - per-instruction decode/register trace
 *   mitra_log_mem    - every read_word/write_word/read_byte/write_byte
 *   mitra_log_int    - interrupts, fast interrupts, suspensions, traps
 */
extern void mitra_log(const char *fmt, ...);
extern int32 mitra_log_enable;   /* master enable                         */
extern int32 mitra_log_inst;   /* instruction decode / register trace   */
extern int32 mitra_log_mem;   /* memory access trace (address, value)  */
extern int32 mitra_log_int;   /* interrupt/fast-int/suspension/trap    */
extern int32 mitra_log_io;   /* device I/O: RD/WD, attach/detach, poll */

static FILE *mitra_log_fp;
static const char *MITRA_LOG_FILENAME = "mitra_trace.log";

#define MLOG(...)      mitra_log(__VA_ARGS__)
#define MLOG_INST(...) do { if (mitra_log_enable && mitra_log_inst) mitra_log(__VA_ARGS__); } while (0)
#define MLOG_MEM(...)  do { if (mitra_log_enable && mitra_log_mem)  mitra_log(__VA_ARGS__); } while (0)
#define MLOG_INT(...)  do { if (mitra_log_enable && mitra_log_int)  mitra_log(__VA_ARGS__); } while (0)

typedef struct {
    uint32 typ;
    uint16 P;
    uint16 A;
    uint16 E;
    uint16 X;
    uint16 L;
    uint16 G;
    uint16 S;
    uint16 ir; // Instruction register, useful for history or debug
    uint8 C;
    uint8 OV;
    uint8 MS;
    uint16 MA;
    uint16 PR;
    uint16 MREG;
    uint16 U, V, W;
    uint32 ea;
} InstHistory;

/* ========== Function Prototypes ========== */
extern void io_suspension_dispatch(uint16 susp_level);

t_value read_word(t_addr va);
void write_word(t_addr va, t_value val);
uint8 read_byte(t_addr va);
void write_byte(t_addr va, uint8 val);
static uint16 add16(uint16 a, uint16 b, uint16 * carry, uint16 * overflow);
static uint16 sub16(uint16 a, uint16 b, uint16 * carry, uint16 * overflow);
static void set_condition_codes_load(uint16 result);
static void set_condition_codes_compare(uint16 a, uint16 b, uint16 result);
static void set_condition_codes_arithmetic(uint16 result, uint16 carry, uint16 overflow);
static void mul32(uint16 a, uint16 b, uint16 * high, uint16 * low);
static int div32(uint16 high, uint16 low, uint16 divisor, uint16 * quot, uint16 * rem);
static void double_to_mitra(double v, uint16 * A, uint16 * E);
static double mitra_to_double(uint16 A, uint16 E);

/* Enhanced trap and suspension functions */
t_stat mitra_trap(int trap, uint16 pc, uint16 *trappc);
t_stat mitra_suspension_request(uint16 susp_level);
t_stat mitra_suspension_process(void);
t_stat mitra_interrupt_accept(uint16 int_level, t_bool high_speed);
t_stat mitra_interrupt_return(t_bool high_speed);

t_stat set_cc(void);
t_stat cpu_ex(t_value * vptr, t_addr addr, UNIT * uptr, int32 sw);
t_stat cpu_dep(t_value val, t_addr addr, UNIT * uptr, int32 sw);
t_stat cpu_reset(DEVICE * dptr);
t_bool cpu_is_pc_a_subroutine_call(t_addr ** ret_addrs);
t_stat cpu_set_size(UNIT * uptr, int32 val, CONST char * cptr, void * desc);
t_stat cpu_set_type(UNIT * uptr, int32 val, CONST char * cptr, void * desc);
void inst_hist(uint32 inst, uint32 pc, uint32 typ);
t_stat rtc_inst(uint32 inst);
t_stat rtc_svc(UNIT * uptr);
t_stat rtc_reset(DEVICE * dptr);
t_stat rtc_set_freq(UNIT * uptr, int32 val, CONST char * cptr, void * desc);
t_stat rtc_show_freq(FILE * st, UNIT * uptr, int32 val, CONST void * desc);
uint32 RelocC(int32 va, int32 sw);
t_stat cpu_set_hist(UNIT * uptr, int32 val, CONST char * cptr, void * desc);
t_stat cpu_show_hist(FILE * st, UNIT * uptr, int32 val, CONST void * desc);
void panel_reset(void);
void mitra_log_regs(const char *label);

#endif
