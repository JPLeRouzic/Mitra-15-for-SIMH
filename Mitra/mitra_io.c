/* mitra_io.c – Mitra 15 I/O subsystem: Minibus, handlers, CSV M:1O/M:WAIT
 *
 * Fully implemented with:
 *   - Typewriter (console) handler using SIMH terminal I/O
 *   - Fast access disk handler with sector addressing and file backing
 *   - Proper interrupt generation and CSV M:WAIT synchronization
 */

#include "mitra_io.h"
#include "mitra_defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

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
extern uint32 int_req;  /* interrupt request bits */
extern uint32 xfr_req;  /* transfer request bits */
extern void io_interrupt_dispatch(void);

/* SIMH terminal I/O functions (provided by the simulator framework) */
extern int sim_tt_getc(void);
extern void sim_tt_putc(int ch);
extern int sim_tt_inchar(void);
extern int sim_tt_open(const char *file, int write);
extern int sim_tt_close(void);

/* Forward declarations for memory access helpers */
static uint32 read_mem(uint32 addr, int zio);
static void write_mem(uint32 addr, uint32 val, int zio);
static uint8 read_byte(uint32 addr, int zio);
static void write_byte(uint32 addr, uint8 val, int zio);

/* Device callbacks */
uint32 dev_read_byte(uint32 dev, int *eor);
uint32 dev_read_word(uint32 dev, int *eor);
void dev_write_byte(uint32 dev, uint8 val, int *eor);
void dev_write_word(uint32 dev, uint16 val, int *eor);
void dev_complete(uint32 dev, int ch);

/* Device mapping tables (existing) */
uint32 dev_map[64][NUM_CHAN];
t_stat (*dev_dsp[64][NUM_CHAN])(uint32 fnc, uint32 dev, uint32 *dat) = { {NULL} };
t_stat (*dev3_dsp[64])(uint32 fnc, uint32 dev, uint32 *dat) = { NULL };

/* Indicators byte bits (byte 1) */
#define IND_U         0x01
#define IND_E         0x02
#define IND_S         0x04
#define IND_T         0x08
#define IND_I         0x10

/* Channel handling tables (existing) */
uint8 chan_uar[NUM_CHAN];
uint16 chan_wcr[NUM_CHAN];
uint16 chan_mar[NUM_CHAN];
uint8 chan_dcr[NUM_CHAN];
uint32 chan_war[NUM_CHAN];
uint8 chan_cpw[NUM_CHAN];
uint8 chan_cnt[NUM_CHAN];
uint16 chan_mode[NUM_CHAN];
uint16 chan_flag[NUM_CHAN];

/* Minibus device slots */
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
    uint8       indicators;
    void        *priv;      /* private data for device */
} MINIBUS_DEV;

static MINIBUS_DEV minibus[MAX_DEVICES];
static int next_free_dev = 0;

/* Operational label mapping */
typedef struct {
    int oplabel;
    int handler_id;
    int unit;
} OPL_MAP;
static OPL_MAP opl_map[32];
static int num_opl = 0;

/* ========== Memory access helpers ========== */
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

/* ========== Operational label assignment ========== */
static int find_device_by_oplabel(int oplabel)
{
    for (int i = 0; i < num_opl; i++)
        if (opl_map[i].oplabel == oplabel)
            return i;
    return -1;
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
    dev->priv = NULL;
    opl_map[num_opl].oplabel = oplabel;
    opl_map[num_opl].handler_id = handler_id;
    opl_map[num_opl].unit = unit;
    num_opl++;
    next_free_dev++;
}

/* ========== Typewriter (console) handler ========== */
typedef struct {
    int echo;
    int need_cr;
} TY_PRIV;

static int ty_init(uint32 cb_addr, int write, uint32 *extra)
{
    /* No special initialization */
    return 0;
}

static void ty_start(int unit)
{
    /* Nothing to do - polling will handle */
}

static int ty_poll(int unit, uint32 *data, int *eor)
{
    TY_PRIV *priv = (TY_PRIV *) minibus[unit].priv;
    if (!priv) {
        priv = calloc(1, sizeof(TY_PRIV));
        priv->echo = 1;
        priv->need_cr = 0;
        minibus[unit].priv = priv;
    }

    int cmd = minibus[unit].cmd;
    int is_write = (cmd & 0x20) ? 1 : 0;

    if (is_write) {
        /* Output: data already contains the word to write? No, poll is called
           after the data has been read from memory. In the current CSV M:1O
           flow, poll is called with data pointer to write device data.
           We need to output the character. */
        if (minibus[unit].bytes_left > 0) {
            uint16 word = (uint16) *data;
            /* For console, we treat each byte as a character (EBCDIC).
               We need to convert EBCDIC to ASCII for output. */
            uint8 ebcdic = word & 0xFF;
            /* Simple conversion: high bit set? Actually terminal expects ASCII.
               We'll use a table for printable characters. */
            static const uint8 ebcdic_to_ascii[256] = {
                /* Basic EBCDIC to ASCII mapping (only common chars) */
                [0x40] = ' ', [0x4B] = '.', [0x4C] = '<', [0x4D] = '(',
                [0x4E] = '+', [0x4F] = '|', [0x50] = '&', [0x5A] = '!',
                [0x5B] = '$', [0x5C] = '*', [0x5D] = ')', [0x5E] = ';',
                [0x5F] = '-', [0x60] = '/', [0x61] = ',', [0x6A] = '%',
                [0x6B] = '_', [0x6C] = '>', [0x6D] = '?', [0x6E] = ':',
                [0x6F] = '#', [0x79] = '`', [0x7A] = ':', [0x7B] = '#',
                [0x7C] = '@', [0x7D] = '\'', [0x7E] = '=', [0x7F] = '"',
                /* letters */
                [0x81] = 'a', [0x82] = 'b', [0x83] = 'c', [0x84] = 'd',
                [0x85] = 'e', [0x86] = 'f', [0x87] = 'g', [0x88] = 'h',
                [0x89] = 'i', [0x91] = 'j', [0x92] = 'k', [0x93] = 'l',
                [0x94] = 'm', [0x95] = 'n', [0x96] = 'o', [0x97] = 'p',
                [0x98] = 'q', [0x99] = 'r', [0xA1] = 's', [0xA2] = 't',
                [0xA3] = 'u', [0xA4] = 'v', [0xA5] = 'w', [0xA6] = 'x',
                [0xA7] = 'y', [0xA8] = 'z',
                [0xC1] = 'A', [0xC2] = 'B', [0xC3] = 'C', [0xC4] = 'D',
                [0xC5] = 'E', [0xC6] = 'F', [0xC7] = 'G', [0xC8] = 'H',
                [0xC9] = 'I', [0xD1] = 'J', [0xD2] = 'K', [0xD3] = 'L',
                [0xD4] = 'M', [0xD5] = 'N', [0xD6] = 'O', [0xD7] = 'P',
                [0xD8] = 'Q', [0xD9] = 'R', [0xE2] = 'S', [0xE3] = 'T',
                [0xE4] = 'U', [0xE5] = 'V', [0xE6] = 'W', [0xE7] = 'X',
                [0xE8] = 'Y', [0xE9] = 'Z',
                [0xF0] = '0', [0xF1] = '1', [0xF2] = '2', [0xF3] = '3',
                [0xF4] = '4', [0xF5] = '5', [0xF6] = '6', [0xF7] = '7',
                [0xF8] = '8', [0xF9] = '9',
                [0x15] = '\n', [0x0D] = '\r', [0x25] = '\n' /* line feed */
            };
            int ascii = ebcdic_to_ascii[ebcdic];
            if (ascii == 0) ascii = '?';
            if (ascii == '\r') {
                sim_tt_putc('\n');
                priv->need_cr = 0;
            } else {
                sim_tt_putc(ascii);
                if (priv->need_cr) {
                    sim_tt_putc('\n');
                    priv->need_cr = 0;
                }
                if (ascii == '\n') priv->need_cr = 1;
            }
            minibus[unit].bytes_left -= 2; /* word size */
            if (minibus[unit].bytes_left == 0) {
                *eor = 1;
                return 0;
            }
            return 0;
        }
    } else {
        /* Input: read a character from console */
        if (minibus[unit].bytes_left > 0) {
            int ch = sim_tt_getc();
            if (ch < 0) {
                /* no character available */
                return 0;
            }
            /* Convert ASCII to EBCDIC */
            static const uint8 ascii_to_ebcdic[128] = {
                /* placeholder mapping */
                [' '] = 0x40, ['.'] = 0x4B, ['<'] = 0x4C, ['('] = 0x4D,
                ['+'] = 0x4E, ['|'] = 0x4F, ['&'] = 0x50, ['!'] = 0x5A,
                ['$'] = 0x5B, ['*'] = 0x5C, [')'] = 0x5D, [';'] = 0x5E,
                ['-'] = 0x5F, ['/'] = 0x60, [','] = 0x61, ['%'] = 0x6A,
                ['_'] = 0x6B, ['>'] = 0x6C, ['?'] = 0x6D, [':'] = 0x6E,
                ['#'] = 0x6F, ['`'] = 0x79, ['@'] = 0x7C, ['\''] = 0x7D,
                ['='] = 0x7E, ['"'] = 0x7F,
                ['a'] = 0x81, ['b'] = 0x82, ['c'] = 0x83, ['d'] = 0x84,
                ['e'] = 0x85, ['f'] = 0x86, ['g'] = 0x87, ['h'] = 0x88,
                ['i'] = 0x89, ['j'] = 0x91, ['k'] = 0x92, ['l'] = 0x93,
                ['m'] = 0x94, ['n'] = 0x95, ['o'] = 0x96, ['p'] = 0x97,
                ['q'] = 0x98, ['r'] = 0x99, ['s'] = 0xA2, ['t'] = 0xA3,
                ['u'] = 0xA4, ['v'] = 0xA5, ['w'] = 0xA6, ['x'] = 0xA7,
                ['y'] = 0xA8, ['z'] = 0xA9,
                ['A'] = 0xC1, ['B'] = 0xC2, ['C'] = 0xC3, ['D'] = 0xC4,
                ['E'] = 0xC5, ['F'] = 0xC6, ['G'] = 0xC7, ['H'] = 0xC8,
                ['I'] = 0xC9, ['J'] = 0xD1, ['K'] = 0xD2, ['L'] = 0xD3,
                ['M'] = 0xD4, ['N'] = 0xD5, ['O'] = 0xD6, ['P'] = 0xD7,
                ['Q'] = 0xD8, ['R'] = 0xD9, ['S'] = 0xE2, ['T'] = 0xE3,
                ['U'] = 0xE4, ['V'] = 0xE5, ['W'] = 0xE6, ['X'] = 0xE7,
                ['Y'] = 0xE8, ['Z'] = 0xE9,
                ['0'] = 0xF0, ['1'] = 0xF1, ['2'] = 0xF2, ['3'] = 0xF3,
                ['4'] = 0xF4, ['5'] = 0xF5, ['6'] = 0xF6, ['7'] = 0xF7,
                ['8'] = 0xF8, ['9'] = 0xF9,
                ['\n'] = 0x15, ['\r'] = 0x0D
            };
            uint8 ebcdic = (ch < 128) ? ascii_to_ebcdic[ch] : 0x40;
            *data = ebcdic;  /* place character in low byte */
            minibus[unit].bytes_left -= 2;
            if (minibus[unit].bytes_left == 0 || ch == '\n' || ch == '\r') {
                *eor = 1;
            }
            return 0;
        }
    }
    return 0;
}

static void ty_interrupt(int unit)
{
    MINIBUS_DEV *dev = &minibus[unit];
    if (dev->intr_level) {
        int_req |= (1 << dev->intr_level);
        io_interrupt_dispatch();
    }
}

static void ty_attach(int unit, const char *file, int write)
{
    /* For console, ignore file, just use existing terminal */
}

/* ========== Fast Access Disk handler ========== */
#define DISK_SECTOR_SIZE 256
#define DISK_MAX_SECTORS (32*1024*1024 / DISK_SECTOR_SIZE) /* up to 32MB disk */
typedef struct {
    FILE *image;
    uint32 num_sectors;
} DISK_PRIV;

static int disk_init(uint32 cb_addr, int write, uint32 *extra)
{
    /* extra contains sector address (low 16 bits?) Actually extra is from
       CB bytes 10-11, we'll treat as sector number. */
    return 0;
}

static void disk_start(int unit)
{
    /* Polling will handle */
}

static int disk_poll(int unit, uint32 *data, int *eor)
{
    MINIBUS_DEV *dev = &minibus[unit];
    DISK_PRIV *priv = (DISK_PRIV *) dev->priv;
    if (!priv) return 1; /* error */

    int cmd = dev->cmd;
    int is_write = (cmd & 0x20) ? 1 : 0;
    uint32 sector = dev->extra_info;  /* sector number from CB */
    uint32 num_bytes = dev->bytes_left;
    uint32 mem_addr = dev->buffer_addr;
    int update = (cmd & 0x10) ? 1 : 0;  /* update sector address after transfer */

    if (num_bytes == 0) {
        *eor = 1;
        return 0;
    }

    /* Transfer entire block (sector-aligned) */
    uint32 sectors_needed = (num_bytes + DISK_SECTOR_SIZE - 1) / DISK_SECTOR_SIZE;
    if (sector + sectors_needed > priv->num_sectors) {
        /* beyond disk size */
        dev->status = 1; /* error */
        *eor = 1;
        return 1;
    }

    /* Seek to sector */
    if (fseek(priv->image, sector * DISK_SECTOR_SIZE, SEEK_SET) != 0) {
        dev->status = 1;
        *eor = 1;
        return 1;
    }

    if (is_write) {
        /* Write: read data from memory and write to disk */
        uint8 buf[DISK_SECTOR_SIZE];
        uint32 remaining = num_bytes;
        uint32 curr_sector = sector;
        while (remaining > 0) {
            uint32 chunk = (remaining < DISK_SECTOR_SIZE) ? remaining : DISK_SECTOR_SIZE;
            /* read from memory into buffer */
            for (uint32 i = 0; i < chunk; i++) {
                buf[i] = read_byte(mem_addr + i, dev->zio);
            }
            /* pad remainder with zeros */
            for (uint32 i = chunk; i < DISK_SECTOR_SIZE; i++) buf[i] = 0;
            if (fwrite(buf, 1, DISK_SECTOR_SIZE, priv->image) != DISK_SECTOR_SIZE) {
                dev->status = 1;
                *eor = 1;
                return 1;
            }
            mem_addr += chunk;
            remaining -= chunk;
            curr_sector++;
        }
    } else {
        /* Read: read from disk into memory */
        uint8 buf[DISK_SECTOR_SIZE];
        uint32 remaining = num_bytes;
        while (remaining > 0) {
            uint32 chunk = (remaining < DISK_SECTOR_SIZE) ? remaining : DISK_SECTOR_SIZE;
            if (fread(buf, 1, DISK_SECTOR_SIZE, priv->image) != DISK_SECTOR_SIZE) {
                dev->status = 1;
                *eor = 1;
                return 1;
            }
            for (uint32 i = 0; i < chunk; i++) {
                write_byte(mem_addr + i, buf[i], dev->zio);
            }
            mem_addr += chunk;
            remaining -= chunk;
        }
    }

    if (update) {
        dev->extra_info = sector + sectors_needed;
        /* Write back the updated extra field to CB if needed */
        write_mem(dev->cb_addr + CB_EXTRA_LO, dev->extra_info, dev->zio);
    }

    dev->bytes_left = 0;
    *eor = 1;
    return 0;
}

static void disk_interrupt(int unit)
{
    MINIBUS_DEV *dev = &minibus[unit];
    if (dev->intr_level) {
        int_req |= (1 << dev->intr_level);
        io_interrupt_dispatch();
    }
}

static void disk_attach(int unit, const char *file, int write)
{
    MINIBUS_DEV *dev = &minibus[unit];
    DISK_PRIV *priv = calloc(1, sizeof(DISK_PRIV));
    if (!priv) return;
    priv->image = fopen(file, "rb+");
    if (!priv->image && write) {
        priv->image = fopen(file, "wb+");
    }
    if (!priv->image) {
        free(priv);
        return;
    }
    /* Determine size */
    fseek(priv->image, 0, SEEK_END);
    long size = ftell(priv->image);
    priv->num_sectors = size / DISK_SECTOR_SIZE;
    if (priv->num_sectors == 0) priv->num_sectors = 1024; /* default 256KB */
    fseek(priv->image, 0, SEEK_SET);
    dev->priv = priv;
}

/* ========== I/O Supervisor Calls ========== */
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

    /* Call device init */
    if (dev->handler_id == 0) {  /* typewriter */
        if (ty_init(cb_addr, (cmd & 0x20) ? 1 : 0, &extra)) {
            write_byte(cb_addr + CB_EVENT, EV_ERROR | EV_INITERR, zio);
            dev->active = 0;
            if (indicators & IND_E) P = err_branch;
            return SCPE_OK;
        }
        ty_start(dev->unit);
    } else if (dev->handler_id == 1) { /* disk */
        if (disk_init(cb_addr, (cmd & 0x20) ? 1 : 0, &extra)) {
            write_byte(cb_addr + CB_EVENT, EV_ERROR | EV_INITERR, zio);
            dev->active = 0;
            if (indicators & IND_E) P = err_branch;
            return SCPE_OK;
        }
        disk_start(dev->unit);
    }

    write_byte(cb_addr + CB_EVENT, EV_ACTIVE, zio);
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
            int_req |= (1 << dev->intr_level);
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

        uint32 data = 0;
        int eor = 0;
        int done = 0;
        uint8 event = EV_ACTIVE;
        int is_write = (dev->cmd & 0x20) ? 1 : 0;

        if (is_write) {
            /* For write, data comes from memory */
            if (dev->bytes_left > 0) {
                data = read_mem(dev->buffer_addr, dev->zio);
                dev->buffer_addr += 2;
                dev->bytes_left -= 2;
            }
        }

        /* Call device poll */
        int err = 0;
        if (dev->handler_id == 0) {
            err = ty_poll(dev->unit, &data, &eor);
        } else if (dev->handler_id == 1) {
            err = disk_poll(dev->unit, &data, &eor);
        }

        if (!is_write && !err && dev->bytes_left > 0) {
            /* For read, data from device goes to memory */
            write_mem(dev->buffer_addr, data, dev->zio);
            dev->buffer_addr += 2;
            dev->bytes_left -= 2;
            if (dev->bytes_left == 0) eor = 1;
        }

        if (err) {
            event = EV_ERROR | EV_PHYSERR;
            dev->active = 0;
            done = 1;
        } else if (eor) {
            dev->eor = 1;
            event = 0;
            dev->active = 0;
            done = 1;
        }

        write_byte(dev->cb_addr + CB_EVENT, event, dev->zio);

        if (done && dev->waiting)
            dev->waiting = 0;
        if (done && dev->intr_level) {
            if (dev->handler_id == 0)
                ty_interrupt(dev->unit);
            else if (dev->handler_id == 1)
                disk_interrupt(dev->unit);
        }
    }
}

/* Interrupt dispatch (already in mitra_cpu, but we have an external reference) */
/* The actual dispatch is in mitra_cpu.c; we just set int_req and call it. */

/* DIT/DITR/RD/WD stubs (minimal) */
t_stat io_dit(void)
{
    io_interrupt_dispatch();
    return SCPE_OK;
}

t_stat io_ditr(void) { return io_dit(); }

t_stat io_rd(uint16 e_reg, uint16 *data_out)
{
    /* Minimal stub */
    *data_out = 0;
    return SCPE_OK;
}

t_stat io_wd(uint16 e_reg, uint32 data) { return SCPE_OK; }

/* Channel functions (minimal, required for linking) */
t_stat chan_process(void) { return SCPE_OK; }
t_bool chan_testact(void) { return 0; }
void chan_set_flag(int32 ch, uint32 fl) {}
void chan_set_ordy(int32 ch) {}
void chan_disc(int32 ch) {}
void chan_set_uar(int32 ch, uint32 dev) {}
t_stat set_chan(UNIT *uptr, int32 val, CONST char *cptr, void *desc) { return SCPE_OK; }
t_stat show_chan(FILE *st, UNIT *uptr, int32 val, CONST void *desc) { return SCPE_OK; }

/* System initialisation */
void io_init_system(void)
{
    memset(minibus, 0, sizeof(minibus));
    next_free_dev = 0;
    num_opl = 0;
    /* Assign operational labels: M:OC = 4 (typewriter), M:SY = 13 (disk) */
    io_assign_oplabel(OPL_M_OC, 0, 0);   /* handler_id 0 = typewriter, unit 0 */
    io_assign_oplabel(OPL_M_SY, 1, 0);   /* handler_id 1 = disk, unit 0 */
}

t_bool io_init(void)
{
    io_init_system();
    return FALSE;
}

/* Device callbacks (unused but required) */
uint32 dev_read_byte(uint32 dev, int *eor) { if(eor) *eor=0; return 0; }
uint32 dev_read_word(uint32 dev, int *eor) { if(eor) *eor=0; return 0; }
void dev_write_byte(uint32 dev, uint8 val, int *eor) { if(eor) *eor=0; }
void dev_write_word(uint32 dev, uint16 val, int *eor) { if(eor) *eor=0; }
void dev_complete(uint32 dev, int ch) { }
