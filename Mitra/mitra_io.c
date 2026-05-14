/* mitra_io.c – Mitra 15 I/O subsystem: Minibus, handlers, CSV M:1O/M:WAIT */
#include "mitra_io.h"
#include "mitra_defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* External references from mitra_cpu.c */
extern uint16 G;        /* general base register */
extern uint16 P;        /* program counter */
extern uint16 A, E, X, L;
extern uint8 C, OV, MS, MA, PR;
extern uint16 read_word(uint16 addr);   /* raw absolute word read */
extern void write_word(uint16 addr, uint16 val); /* raw absolute word write */
extern uint16 M[];      /* memory array */
extern uint16 RL1, RL2, RL4;  /* Relocation registers */
extern void set_dyn_map (void);

/* Forward declarations for memory access helpers */
static uint32 read_mem(uint32 addr, int zio);
static void write_mem(uint32 addr, uint32 val, int zio);
static uint8 read_byte(uint32 addr, int zio);
static void write_byte(uint32 addr, uint8 val, int zio);

/* Device callbacks (to be implemented by handlers) */
uint32 dev_read_byte(uint32 dev, int *eor);
uint32 dev_read_word(uint32 dev, int *eor);
void dev_write_byte(uint32 dev, uint8 val, int *eor);
void dev_write_word(uint32 dev, uint16 val, int *eor);
void dev_complete(uint32 dev, int ch);

/* dev_map maps device and channel numbers to a transfer flag masks */
uint32 dev_map[64][NUM_CHAN];

/* dev_dsp maps device and channel numbers to dispatch routines */
t_stat (*dev_dsp[64][NUM_CHAN])(uint32 fnc, uint32 dev, uint32 *dat) = { {NULL} };

/* dev3_dsp maps system device numbers to dispatch routines */
t_stat (*dev3_dsp[64])(uint32 fnc, uint32 dev, uint32 *dat) = { NULL };

t_stat chan_eor(int32 ch);
t_stat pot_ilc(uint32 num, uint32 *dat);
t_stat chan_read(int32 ch);
t_stat chan_write(int32 ch);


#define TST_XFR(d,c)    (xfr_req & dev_map[d][c])
#define SET_XFR(d,c)    xfr_req = xfr_req | dev_map[d][c]
#define CLR_XFR(d,c)    xfr_req = xfr_req & ~dev_map[d][c]
#define INV_DEV(d,c)    (dev_dsp[d][c] == NULL)
#define VLD_DEV(d,c)    (dev_dsp[d][c] != NULL)
#define TST_EOR(c)      (chan_flag[c] & CHF_EOR)
#define QAILCE(a)       (((a) >= POT_ILCY) && ((a) < (POT_ILCY + NUM_CHAN)))

/* Control Block offsets (bytes) */
#define CB_EVENT       0
#define CB_INDICATORS  1
#define CB_CMD         2
#define CB_OPLABEL     3
#define CB_ADDR_LO     4
#define CB_COUNT_LO    6
#define CB_ERRBR_LO    8
#define CB_EXTRA_LO   10
#define CB_TIMEOUT_LO 12
#define CB_INTLEV_LO  14

/* Event byte bits (from mitra_io.h) */
/* EV_ACTIVE = 0x01, EV_ERROR = 0x02, EV_PHYSERR = 0x04, EV_INITERR = 0x10 */
#define EV_USER       0x10

/* Indicators byte bits (byte 1) */
#define IND_U         0x01
#define IND_E         0x02
#define IND_S         0x04
#define IND_T         0x08
#define IND_I         0x10

uint8 chan_uar[NUM_CHAN];                               /* unit addr */
uint16 chan_wcr[NUM_CHAN];                              /* word count */
uint16 chan_mar[NUM_CHAN];                              /* mem addr */
uint8 chan_dcr[NUM_CHAN];                               /* data chain */
uint32 chan_war[NUM_CHAN];                              /* word assembly */
uint8 chan_cpw[NUM_CHAN];                               /* char per word */
uint8 chan_cnt[NUM_CHAN];                               /* char count */
uint16 chan_mode[NUM_CHAN];                             /* mode */
uint16 chan_flag[NUM_CHAN];                             /* flags */
static const char *chname[NUM_CHAN] = {
    "W", "Y", "C", "D", "E", "F", "G", "H"
};

extern uint16 M[MAXMEMSIZE];                            /* memory */
extern uint32 int_req;                                  /* int req */
extern uint32 xfr_req;                                  /* xfer req */
extern uint32 alert;                                    /* pin/pot alert */
extern uint16 X, EM2, EM3, ion, bpt;
extern uint8 OV;
extern uint32 cpu_mode;
extern int32 rtc_pie;
extern int32 stop_invins, stop_invdev, stop_inviop;
extern uint32 mon_usr_trap;
extern UNIT cpu_unit;

/* Device handler type */
typedef struct device_handler {
    const char *name;
    int   (*init)(uint32 cb_addr, int write, uint32 *extra);
    void  (*start)(int unit);
    int   (*poll)(int unit, uint32 *data, int *eor);
    void  (*interrupt)(int unit);
    void  (*attach)(int unit, const char *file, int write);
} DEVHANDLER;

/* Minibus device slot */
#define MAX_DEVICES 32
typedef struct {
    int         used;
    int         oplabel;
    int         handler_id;
    int         unit;
    uint32      cb_addr;
    int         zio;
    uint32      buffer_addr;
    uint32      bytes_left;
    uint32      extra_info;
    int         cmd;
    int         status;
    int         eor;
    int         waiting;
    uint32      wait_task;
    uint32      intr_level;
    uint32      timeout;
    int         active;
    uint8       indicators;            /* store byte 1 of CB */
} MINIBUS_DEV;

static MINIBUS_DEV minibus[MAX_DEVICES];
static int next_free_dev = 0;

/* Interrupt system */
#define INT_LEVELS 32
static uint32 intr_pending = 0;
static uint32 intr_mask = 0;
static uint32 intr_armed[INT_LEVELS];
static uint32 intr_context[INT_LEVELS];

/* Forward declarations of device handlers */
static int  ty_init(uint32 cb_addr, int write, uint32 *extra);
static void ty_start(int unit);
static int  ty_poll(int unit, uint32 *data, int *eor);
static void ty_interrupt(int unit);
static void ty_attach(int unit, const char *file, int write);

static int  ptr_init(uint32 cb_addr, int write, uint32 *extra);
static void ptr_start(int unit);
static int  ptr_poll(int unit, uint32 *data, int *eor);
static void ptr_interrupt(int unit);
static void ptr_attach(int unit, const char *file, int write);

static int  ptp_init(uint32 cb_addr, int write, uint32 *extra);
static void ptp_start(int unit);
static int  ptp_poll(int unit, uint32 *data, int *eor);
static void ptp_interrupt(int unit);
static void ptp_attach(int unit, const char *file, int write);

static int  lpr_init(uint32 cb_addr, int write, uint32 *extra);
static void lpr_start(int unit);
static int  lpr_poll(int unit, uint32 *data, int *eor);
static void lpr_interrupt(int unit);
static void lpr_attach(int unit, const char *file, int write);

void io_assign_oplabel(int oplabel, int handler_id, int unit);

/* Handler table */
static DEVHANDLER handlers[] = {
    { "typewriter",  ty_init,  ty_start,  ty_poll,  ty_interrupt,  ty_attach },
    { "ptr",         ptr_init, ptr_start, ptr_poll, ptr_interrupt, ptr_attach },
    { "ptp",         ptp_init, ptp_start, ptp_poll, ptp_interrupt, ptp_attach },
    { "lprinter",    lpr_init, lpr_start, lpr_poll, lpr_interrupt, lpr_attach },
    { NULL }
};
#define NUM_HANDLERS (sizeof(handlers)/sizeof(handlers[0]) - 1)

/* Operational label mapping */
typedef struct {
    int oplabel;
    int handler_id;
    int unit;
} OPL_MAP;
static OPL_MAP opl_map[32];
static int num_opl = 0;

/* ------------------------------------------------------------
   Memory access helpers (G‑relative or ZC‑relative)
   ------------------------------------------------------------ */
static uint32 read_mem(uint32 addr, int zio)
{
    if (zio) {
        uint32 zc_base = read_word(G + 6);
        return read_word(zc_base + addr);
    } else {
        return read_word(G + addr);
    }
}

static void write_mem(uint32 addr, uint32 val, int zio)
{
    if (zio) {
        uint32 zc_base = read_word(G + 6);
        write_word(zc_base + addr, val);
    } else {
        write_word(G + addr, val);
    }
}

static uint8 read_byte(uint32 addr, int zio)
{
    uint16 word = read_mem(addr & ~1, zio);
    return (addr & 1) ? (word >> 8) : (word & 0xFF);
}

static void write_byte(uint32 addr, uint8 val, int zio)
{
    uint16 word = read_mem(addr & ~1, zio);
    if (addr & 1)
        word = (word & 0x00FF) | (val << 8);
    else
        word = (word & 0xFF00) | val;
    write_mem(addr & ~1, word, zio);
}

/* ------------------------------------------------------------
   I/O Supervisor Calls
   ------------------------------------------------------------ */
static int find_device_by_oplabel(int oplabel)
{
    for (int i = 0; i < num_opl; i++)
        if (opl_map[i].oplabel == oplabel)
            return i;
    return -1;
}

t_stat io_csv_1o(uint32 cb_addr, int zio)
{
    uint8  event, indicators, cmd, oplabel;
    uint16 buffer_addr, byte_count;
    uint32 err_branch = 0, extra = 0, timeout = 0, intr_lvl = 0;
    int    opl_idx;
    MINIBUS_DEV *dev;

    /* Read fixed part of CB */
    event      = read_byte(cb_addr + CB_EVENT, zio);
    indicators = read_byte(cb_addr + CB_INDICATORS, zio);
    cmd        = read_byte(cb_addr + CB_CMD, zio);
    oplabel    = read_byte(cb_addr + CB_OPLABEL, zio);
    buffer_addr= read_mem(cb_addr + CB_ADDR_LO, zio);
    byte_count = read_mem(cb_addr + CB_COUNT_LO, zio);

    /* Optional fields */
    if (indicators & IND_E)
        err_branch = read_mem(cb_addr + CB_ERRBR_LO, zio);
    if (indicators & IND_S)
        extra = read_mem(cb_addr + CB_EXTRA_LO, zio);
    if (indicators & IND_T)
        timeout = read_mem(cb_addr + CB_TIMEOUT_LO, zio);
    if (indicators & IND_I)
        intr_lvl = read_mem(cb_addr + CB_INTLEV_LO, zio) & 0x1F;

    opl_idx = find_device_by_oplabel(oplabel);
    if (opl_idx < 0 || !minibus[opl_idx].used) {
        write_byte(cb_addr + CB_EVENT, EV_ERROR | EV_INITERR, zio);
        if (indicators & IND_E)
            P = err_branch;
        return SCPE_OK;
    }

    dev = &minibus[opl_idx];
    if (dev->active) {
        write_byte(cb_addr + CB_EVENT, EV_ERROR | EV_INITERR, zio);
        if (indicators & IND_E)
            P = err_branch;
        return SCPE_OK;
    }

    /* Fill transfer parameters */
    dev->cb_addr   = cb_addr;
    dev->zio       = zio;
    dev->buffer_addr = buffer_addr;
    dev->bytes_left  = byte_count;
    dev->extra_info  = extra;
    dev->cmd         = cmd;
    dev->timeout     = timeout;
    dev->intr_level  = intr_lvl;
    dev->active      = 1;
    dev->eor         = 0;
    dev->waiting     = 0;
    dev->status      = 0;
    dev->indicators  = indicators;

    DEVHANDLER *h = &handlers[dev->handler_id];
    if (h->init(cb_addr, (cmd & 0x20) ? 1 : 0, &extra)) {
        write_byte(cb_addr + CB_EVENT, EV_ERROR | EV_INITERR, zio);
        dev->active = 0;
        if (indicators & IND_E)
            P = err_branch;
        return SCPE_OK;
    }

    write_byte(cb_addr + CB_EVENT, EV_ACTIVE, zio);
    h->start(dev->unit);
    return SCPE_OK;
}

t_stat io_csv_wait(uint32 cb_addr, int zwat)
{
    MINIBUS_DEV *dev = NULL;
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (minibus[i].used && minibus[i].cb_addr == cb_addr && minibus[i].active) {
            dev = &minibus[i];
            break;
        }
    }
    if (!dev || !dev->active) {
        if (dev && dev->intr_level)
            intr_pending |= (1 << dev->intr_level);
        return SCPE_OK;
    }
    dev->waiting = 1;
    return SCPE_OK;
}

/* Poll active devices (called from sim_instr) */
void io_poll_devices(void)
{
    for (int i = 0; i < MAX_DEVICES; i++) {
        MINIBUS_DEV *dev = &minibus[i];
        if (!dev->used || !dev->active) continue;

        DEVHANDLER *h = &handlers[dev->handler_id];
        uint32 data;
        int eor = 0;
        int done = 0;
        uint8 event = EV_ACTIVE;

        if (h->poll(dev->unit, &data, &eor)) {
            event = EV_ERROR | EV_PHYSERR;
            dev->active = 0;
            done = 1;
        } else if (eor) {
            dev->eor = 1;
            if (dev->bytes_left == 0) {
                event = 0;
                dev->active = 0;
                done = 1;
            } else {
                /* Intermediate record – treat as done for simplicity */
                event = 0;
                dev->active = 0;
                done = 1;
            }
        } else if (dev->bytes_left > 0) {
            if (dev->cmd & 0x20) { /* write to device */
                data = read_mem(dev->buffer_addr, dev->zio);
                dev->buffer_addr += 2;
                dev->bytes_left -= 2;
            } else { /* read from device */
                write_mem(dev->buffer_addr, data, dev->zio);
                dev->buffer_addr += 2;
                dev->bytes_left -= 2;
            }
        }

        write_byte(dev->cb_addr + CB_EVENT, event, dev->zio);

        if (done && dev->waiting)
            dev->waiting = 0;
        if (done && dev->intr_level)
            intr_pending |= (1 << dev->intr_level);
    }
}

/* Interrupt dispatch */
void io_interrupt_dispatch(void)
{
    for (int level = 31; level >= 0; level--) {
        if ((intr_pending & (1 << level)) && !(intr_mask & (1 << level))) {
            intr_pending &= ~(1 << level);
            uint32 ctx_ptr = intr_context[level];
            P = read_word(ctx_ptr + 6);
            A = read_word(ctx_ptr + 3);
            E = read_word(ctx_ptr + 2);
            X = read_word(ctx_ptr + 1);
            L = read_word(ctx_ptr + 5);
            G = read_word(ctx_ptr + 4);
            uint32 ind = read_word(ctx_ptr);
            C = ind & 1;
            OV = (ind >> 1) & 1;
            MS = (ind >> 15) & 1;
            MA = (ind >> 13) & 1;
            PR = (ind >> 14) & 1;
            break;
        }
    }
}

t_stat io_dit(void)
{
    uint32 cur_lvl = 0; /* FIXME: obtain current level from CPU */
    uint32 ctx_ptr = intr_context[cur_lvl];
    write_word(ctx_ptr,     (C & 1) | ((OV & 1) << 1) | ((MA & 1) << 13) | ((PR & 1) << 14) | ((MS & 1) << 15));
    write_word(ctx_ptr + 1, X);
    write_word(ctx_ptr + 2, E);
    write_word(ctx_ptr + 3, A);
    write_word(ctx_ptr + 4, G);
    write_word(ctx_ptr + 5, L);
    write_word(ctx_ptr + 6, P);
    intr_armed[cur_lvl] = 0;
    io_interrupt_dispatch();
    return SCPE_OK;
}

t_stat io_ditr(void) { return io_dit(); }

t_stat io_rd(uint16 e_reg, uint16 *data_out) { *data_out = 0; return SCPE_OK; }
t_stat io_wd(uint16 e_reg, uint32 data) { return SCPE_OK; }

/* Device handler stubs (to be replaced by real implementations) */
static int ty_init(uint32 cb_addr, int write, uint32 *extra) { return 0; }
static void ty_start(int unit) { }
static int ty_poll(int unit, uint32 *data, int *eor) { return 0; }
static void ty_interrupt(int unit) { }
static void ty_attach(int unit, const char *file, int write) { }

static int ptr_init(uint32 cb_addr, int write, uint32 *extra) { return 0; }
static void ptr_start(int unit) { }
static int ptr_poll(int unit, uint32 *data, int *eor) { return 0; }
static void ptr_interrupt(int unit) { }
static void ptr_attach(int unit, const char *file, int write) { }

static int ptp_init(uint32 cb_addr, int write, uint32 *extra) { return 0; }
static void ptp_start(int unit) { }
static int ptp_poll(int unit, uint32 *data, int *eor) { return 0; }
static void ptp_interrupt(int unit) { }
static void ptp_attach(int unit, const char *file, int write) { }

static int lpr_init(uint32 cb_addr, int write, uint32 *extra) { return 0; }
static void lpr_start(int unit) { }
static int lpr_poll(int unit, uint32 *data, int *eor) { return 0; }
static void lpr_interrupt(int unit) { }
static void lpr_attach(int unit, const char *file, int write) { }

/* ---------------------------------------------------------------------- */
/* Channel handling (DMA)                                                 */
/* ---------------------------------------------------------------------- */
t_stat chan_process(void)
{
    int32 i, dev;
    t_stat r;
    for (i = 0; i < NUM_CHAN; i++) {
        dev = chan_uar[i] & DEV_MASK;
        if ((dev && TST_XFR(dev, i)) || TST_EOR(i)) {
            if (dev & DEV_OUT)
                r = chan_write(i);
            else
                r = chan_read(i);
            if (r) return r;
        }
    }
    return SCPE_OK;
}

t_bool chan_testact(void)
{
    int32 i, dev;
    for (i = 0; i < NUM_CHAN; i++) {
        dev = chan_uar[i] & DEV_MASK;
        if ((dev && TST_XFR(dev, i)) || TST_EOR(i))
            return 1;
    }
    return 0;
}

void chan_set_ordy(int32 ch)
{
    if (ch >= 0 && ch < NUM_CHAN) {
        int32 dev = chan_uar[ch] & DEV_MASK;
        if (chan_cnt[ch] || (chan_flag[ch] & CHF_ILCE))
            SET_XFR(dev, ch);
        else
            chan_flag[ch] |= CHF_OWAK;
    }
}

void chan_set_flag(int32 ch, uint32 fl)
{
    if (ch >= 0 && ch < NUM_CHAN)
        chan_flag[ch] |= fl;
}

void chan_set_uar(int32 ch, uint32 dev)
{
    if (ch >= 0 && ch < NUM_CHAN)
        chan_uar[ch] = dev & DEV_MASK;
}

void chan_disc(int32 ch)
{
    if (ch >= 0 && ch < NUM_CHAN)
        chan_uar[ch] = 0;
}

t_stat chan_reset(DEVICE *dptr)
{
    int32 i;
    xfr_req = 0;
    for (i = 0; i < NUM_CHAN; i++) {
        chan_uar[i] = 0;
        chan_wcr[i] = 0;
        chan_mar[i] = 0;
        chan_dcr[i] = 0;
        chan_war[i] = 0;
        chan_cpw[i] = 0;
        chan_cnt[i] = 0;
        chan_mode[i] = 0;
        chan_flag[i] = 0;
    }
    return SCPE_OK;
}

/* Channel read: from device to memory */
t_stat chan_read(int32 ch)
{
    uint32 dev = chan_uar[ch] & DEV_MASK;
    uint32 data;
    int eor = 0;

    if (chan_cnt[ch] == 0) {
        if (chan_cpw[ch] == 1) {
            data = dev_read_byte(dev, &eor);
            chan_war[ch] = (chan_war[ch] & 0xFF00) | (data & 0xFF);
            chan_cnt[ch] = 1;
        } else {
            data = dev_read_word(dev, &eor);
            chan_war[ch] = data;
            chan_cnt[ch] = 2;
        }
    }

    if (chan_cnt[ch] > 0) {
        if (chan_cpw[ch] == 1) {
            uint8 byte = (chan_war[ch] >> ((chan_cnt[ch] == 1) ? 0 : 8)) & 0xFF;
            write_byte(chan_mar[ch], byte, 0);
            chan_mar[ch] += 1;
        } else {
            write_word(chan_mar[ch], (uint16)chan_war[ch]);
            chan_mar[ch] += 2;
        }
        chan_cnt[ch]--;
        chan_wcr[ch]--;
    }

    if (eor || chan_wcr[ch] == 0) {
        chan_flag[ch] |= CHF_EOR;
        CLR_XFR(dev, ch);
        if (chan_flag[ch] & CHF_ILCE)
            pot_ilc(ch, NULL);
        chan_eor(ch);
    }
    return SCPE_OK;
}

/* Channel write: from memory to device */
t_stat chan_write(int32 ch)
{
    uint32 dev = chan_uar[ch] & DEV_MASK;
    uint16 data;
    int eor = 0;

    if (chan_cnt[ch] == 0) {
        if (chan_cpw[ch] == 1) {
            data = read_byte(chan_mar[ch], 0);
            chan_war[ch] = (chan_war[ch] & 0xFF00) | data;
            chan_cnt[ch] = 1;
        } else {
            data = read_word(chan_mar[ch]);
            chan_war[ch] = data;
            chan_cnt[ch] = 2;
        }
        chan_mar[ch] += (chan_cpw[ch] == 1) ? 1 : 2;
    }

    if (chan_cnt[ch] > 0) {
        if (chan_cpw[ch] == 1) {
            uint8 byte = (chan_war[ch] >> ((chan_cnt[ch] == 1) ? 0 : 8)) & 0xFF;
            dev_write_byte(dev, byte, &eor);
        } else {
            dev_write_word(dev, (uint16)chan_war[ch], &eor);
        }
        chan_cnt[ch]--;
        chan_wcr[ch]--;
    }

    if (eor || chan_wcr[ch] == 0) {
        chan_flag[ch] |= CHF_EOR;
        CLR_XFR(dev, ch);
        if (chan_flag[ch] & CHF_ILCE)
            pot_ilc(ch, NULL);
        chan_eor(ch);
    }
    return SCPE_OK;
}

/* End of record handling */
t_stat chan_eor(int32 ch)
{
    if (chan_dcr[ch]) {
        uint16 dcr_addr = chan_dcr[ch];
        chan_mar[ch] = read_word(dcr_addr);
        chan_wcr[ch] = read_word(dcr_addr + 2);
        chan_dcr[ch] = read_word(dcr_addr + 4);
        chan_cnt[ch] = 0;
        SET_XFR(chan_uar[ch] & DEV_MASK, ch);
        return SCPE_OK;
    }
    dev_complete(chan_uar[ch] & DEV_MASK, ch);
    return SCPE_OK;
}

/* POT / PIN functions */
t_stat pot_ilc(uint32 num, uint32 *dat)
{
    if (num < 32) intr_pending &= ~(1 << num);
    return SCPE_OK;
}
t_stat pot_dcr(uint32 num, uint32 *dat) { return SCPE_OK; }
t_stat pin_adr(uint32 num, uint32 *dat) { if(dat) *dat=0; return SCPE_OK; }
t_stat pot_fork(uint32 num, uint32 *dat) { return SCPE_OK; }
/* POT routines for RL1, RL2, RL4 */

t_stat pot_RL1 (uint32 num, uint32 *dat)
{
RL1 = *dat;
set_dyn_map ();
return SCPE_OK;
}

t_stat pot_RL2 (uint32 num, uint32 *dat)
{
RL2 = *dat;
set_dyn_map ();
return SCPE_OK;
}

t_stat pot_RL4 (uint32 num, uint32 *dat)
{
RL4 = (*dat) & 03737;
set_dyn_map ();
return SCPE_OK;
}

/* Device callback stubs (to be implemented by real handlers) */
uint32 dev_read_byte(uint32 dev, int *eor) { if(eor) *eor=0; return 0; }
uint32 dev_read_word(uint32 dev, int *eor) { if(eor) *eor=0; return 0; }
void dev_write_byte(uint32 dev, uint8 val, int *eor) { if(eor) *eor=0; }
void dev_write_word(uint32 dev, uint16 val, int *eor) { if(eor) *eor=0; }
void dev_complete(uint32 dev, int ch) { }

/* ---------------------------------------------------------------------- */
/* System initialisation and device mapping                              */
/* ---------------------------------------------------------------------- */
void io_init_system(void)
{
    memset(minibus, 0, sizeof(minibus));
    next_free_dev = 0;
    num_opl = 0;
    intr_pending = 0;
    intr_mask = 0;
    io_assign_oplabel(OPL_M_OC, 0, 0);
    io_assign_oplabel(OPL_M_SI, 1, 0);
    io_assign_oplabel(OPL_M_LO, 2, 0);
}

void io_assign_oplabel(int oplabel, int handler_id, int unit)
{
    if (num_opl >= MAX_DEVICES) return;
    if (find_device_by_oplabel(oplabel) >= 0) return;
    MINIBUS_DEV *dev = &minibus[next_free_dev];
    dev->used = 1;
    dev->oplabel = oplabel;
    dev->handler_id = handler_id;
    dev->unit = unit;
    dev->active = 0;
    opl_map[num_opl].oplabel = oplabel;
    opl_map[num_opl].handler_id = handler_id;
    opl_map[num_opl].unit = unit;
    num_opl++;
    next_free_dev++;
}

t_stat set_chan(UNIT *uptr, int32 val, CONST char *sptr, void *desc)
{
    DEVICE *dptr;
    DIB *dibp;
    int32 i;
    if (sptr == NULL) return SCPE_ARG;
    if (uptr == NULL) return SCPE_IERR;
    dptr = find_dev_from_unit(uptr);
    if (dptr == NULL) return SCPE_IERR;
    dibp = (DIB *) dptr->ctxt;
    if (dibp == NULL) return SCPE_IERR;
    for (i = 0; i < NUM_CHAN; i++) {
        if (strcmp(sptr, chname[i]) == 0) {
            if (val && !(val & (1 << i))) return SCPE_ARG;
            dibp->chan = i;
            return SCPE_OK;
        }
    }
    return SCPE_ARG;
}

t_stat show_chan(FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
    DEVICE *dptr;
    DIB *dibp;
    if (uptr == NULL) return SCPE_IERR;
    dptr = find_dev_from_unit(uptr);
    if (dptr == NULL) return SCPE_IERR;
    dibp = (DIB *) dptr->ctxt;
    if (dibp == NULL) return SCPE_IERR;
    fprintf(st, "channel=%s", chname[dibp->chan]);
    return SCPE_OK;
}

t_bool io_init(void)
{
    DEVICE *dptr;
    DIB *dibp;
    DSPT *tplp;
    int32 ch;
    uint32 i, j, dev, doff;

    for (i = 0; i < NUM_CHAN; i++)
        for (j = 0; j < (DEV_MASK + 1); j++) {
            dev_dsp[j][i] = NULL;
            dev_map[j][i] = 0;
        }

    for (i = 0; (dptr = sim_devices[i]); i++) {
        dibp = (DIB *) dptr->ctxt;
        if ((dibp == NULL) || (dptr->flags & DEV_DIS)) continue;
        ch = dibp->chan;
        dev = dibp->dev;
        if (ch < 0)
            dev3_dsp[dev] = dibp->iop;
        else {
            if (dibp->tplt == NULL) return TRUE;
            for (tplp = dibp->tplt; tplp->num; tplp++) {
                for (j = 0; j < tplp->num; j++) {
                    doff = dev + tplp->off + j;
                    if (dev_map[doff][ch]) {
                        sim_printf("Device number conflict, chan = %s, devno = %02o\n",
                                   chname[ch], doff);
                        return TRUE;
                    }
                    dev_map[doff][ch] = dibp->xfr;
                    dev_dsp[doff][ch] = dibp->iop;
                }
            }
        }
    }
    return FALSE;
}
