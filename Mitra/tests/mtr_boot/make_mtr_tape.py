#!/usr/bin/env python3
"""Build a Mitra-15 ASR33 paper-tape image that boots the MTR monitor.

Tape layout (raw bytes, big-endian words):
  [1] primary boot, 128 bytes  -> loaded at 0x0000 by asr33_boot (unchanged code,
                                  3 constants patched + loader parameter cells)
  [2] loader, 68 bytes         -> read by the primary into 0x4000 (free hole)
  [3] MTR 0x0080..0x33CF       -> read by the loader
  [4] MTR 0x7C00..0x7FFF       -> read by the loader (optional, --no-high to skip)
  [5] MTR 0x0000..0x007F       -> read LAST (it overwrites the primary + parameters)
Then the loader either loads the monitor's CTX<n> (default n=6, ASR33 interrupt)
or jumps to --entry.

usage: make_mtr_tape.py mitra_boot.lst MTR_disasm_v7.asm out.tape [--ctx 6 | --entry XXXX] [--no-high] [--no-ctx0] [--no-arm] [--big-endian] [--leader 16]
"""
import re, sys
from parse_mtr import parse

LOADER_ADDR = 0x4000          # must be inside the free hole 0x33D0..0x7BFF
CELLS = 0x40                  # loader parameter cells (inside the 128-byte primary)

def words_to_bytes(words):
    out = bytearray()
    for w in words: out += bytes([(w >> 8) & 0xFF, w & 0xFF])
    return out

def primary_from_listing(path):
    m = bytearray(0x80)
    for l in open(path, encoding='utf-8', errors='replace'):
        r = re.match(r'^([0-9A-F]{4})\s+([0-9A-F]{4})(\s|$)', l)
        if r and int(r.group(1), 16) < 0x80:
            a, w = int(r.group(1), 16), int(r.group(2), 16)
            m[a] = w >> 8; m[a+1] = w & 0xFF
    return m

def put(m, a, w): m[a] = (w >> 8) & 0xFF; m[a+1] = w & 0xFF

def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    lst, asm, out = args[:3]
    entry = 0x04D0
    if '--entry' in sys.argv: entry = int(sys.argv[sys.argv.index('--entry')+1], 16)
    high = '--no-high' not in sys.argv
    ctx_level = 6                              # default: start like a level-6 (ASR33) interrupt
    if '--ctx' in sys.argv: ctx_level = int(sys.argv[sys.argv.index('--ctx')+1])
    if '--entry' in sys.argv: ctx_level = None # raw jump instead
    leader = 16                                # blank (0x00) frames, skipped by asr33_boot
    if '--leader' in sys.argv: leader = int(sys.argv[sys.argv.index('--leader')+1])

    mem, _ = parse(asm)                       # word address -> word ; missing = 0
    img = bytearray(0x8000)
    for a, w in mem.items(): img[a] = w >> 8; img[a+1] = w & 0xFF

    blocks = [(0x0080, 0x33D0)]               # (first byte, end+1)
    if high: blocks.append((0x7C00, 0x8000))
    if '--no-ctx0' not in sys.argv:           # level-0 context
        cp0 = mem.get(mem.get(0x0A, 0), 0)    # CPT[0]
        cpt = mem.get(0x0A, 0); cp6 = mem.get(cpt + 12, 0)
        G6, L6 = mem.get(cp6 + 8, 0), mem.get(cp6 + 10, 0)      # level-6 (ASR33) G and L
        P0 = 0x050E                                              # idle loop BRU $
        if '--no-arm' not in sys.argv:
            # Stub run once at level 0: start a keyboard read exactly like the monitor's own
            # routine at 0x28F8-0x291C (R9=0x8000|len-1, R10=buf-1, R11=0x8D00 (CR), A=0xA1, E=1, V_0016<-E, WD)
            STUB = 0x4100
            r9  = 0x8000 | mem.get(L6 + 0x12, 0)      # V_0012 = max line length - 1
            r10 = (mem.get(L6 + 0x2C, 0) - 1) & 0xFFFF  # V_002C = input buffer address
            def v16(v, reg):
                c = []
                if v >> 8: c += [0x2000 | (v >> 8), 0xF008] + ([0x2500 | (v & 0xFF)] if v & 0xFF else [])
                else:      c += [0x2000 | v]
                return c + [0xFA00 | reg]
            stub = v16(r9, 9) + v16(r10, 10) + v16(0x8D00, 11) + [0x20A1, 0x2101, 0x1200 | 0x16, 0xF403, 0xC700]
            sb = words_to_bytes(stub)
            img[STUB:STUB+len(sb)] = sb
            blocks.append((STUB, STUB + len(sb)))
            P0 = STUB
            print("keyboard arm stub at %04X: R9=%04X R10=%04X R11=8D00 A=00A1 E=0001 V_0016<-1" % (STUB, r9, r10))
        ctx0 = [0x2000, 0, 0, 0, G6, L6, P0]                    # status(MS=1,MA=0) X E A G L P
        for i, w in enumerate(ctx0): img[cp0+2*i] = w >> 8; img[cp0+2*i+1] = w & 0xFF
        blocks.append((cp0, cp0 + 14))
        print("level-0 context written at %04X: G=%04X L=%04X P=%04X" % (cp0, G6, L6, P0))
    blocks.append((0x0000, 0x0080))           # LAST: clobbers primary

    # ---- loader code (each block = 10 words, same idiom as the primary) ----
    code = []
    for k in range(len(blocks)):
        c = CELLS + 4*k
        code += [0x4000 | c,        # LDA DG cnt-1 cell
                 0xFA09,            # STR R9
                 0x4000 | (c+2),    # LDA DG addr-1 cell
                 0xFA0A,            # STR R10
                 0x2101,            # LDE #1
                 0x2003,            # LDA #3   (tape read)
                 0xF403,            # WD
                 0xF909,            # LDR R9
                 0xC502,            # BAZ +2   (done)
                 0xCF02]            # BRU -2   (poll)
    def val16(v, reg):                       # build a 16-bit constant in A, store it in a register
        c = []
        if v >> 8: c += [0x2000 | (v >> 8), 0xF008] + ([0x2500 | (v & 0xFF)] if v & 0xFF else [])
        else:      c += [0x2000 | v]
        return c + [0xFA00 | reg]            # STR reg  (0=P 1=L 2=G 3=A 4=E 5=X)
    if ctx_level is None:                    # raw jump to --entry
        code += val16(entry, 0)
    else:                                    # emulate "interrupt level N accepted": load its CTX
        cpt = mem.get(0x0A, 0); cp = mem.get(cpt + 2*ctx_level, 0)
        st, X, E, A, G, L, P = [mem.get(cp + 2*i, 0) for i in range(7)]
        print("CTX%d @%04X: status=%04X X=%04X E=%04X A=%04X G=%04X L=%04X P=%04X" % (ctx_level, cp, st, X, E, A, G, L, P))
        code += val16(G, 2) + val16(L, 1) + val16(X, 5) + val16(E, 4)
        code += [0xF408 if (st >> 14) & 1 else 0xF400]       # MA bit of the saved status: STM / CLM
        code += val16(P, 0)                                   # last: P := INT routine (A is clobbered)
    loader = words_to_bytes(code)

    # ---- primary: patch the 3 constants and add the parameter cells ----
    p = primary_from_listing(lst)
    put(p, 0x0E, len(loader) - 1)
    put(p, 0x10, (LOADER_ADDR - 1) & 0xFFFF)
    put(p, 0x12, LOADER_ADDR)
    for k, (a, e) in enumerate(blocks):
        put(p, CELLS + 4*k,     (e - a - 1) & 0xFFFF)   # R9  = count-1
        put(p, CELLS + 4*k + 2, (a - 1) & 0xFFFF)       # R10 = addr-1

    # ---- byte order on the tape ----
    # The simulator stores byte N of the tape at memory byte address N.  With "even address = RIGHT (low) byte,
    # odd address = LEFT (high) byte" the tape carries the LOW byte of each word first (default, --low-first).
    # Use --big-endian for the old order (high byte first).
    low_first = '--big-endian' not in sys.argv
    def enc(b):                              # b is in "big-endian word" layout; returns tape order
        b = bytearray(b)
        if low_first:
            for i in range(0, len(b) - 1, 2): b[i], b[i+1] = b[i+1], b[i]
        return bytes(b)
    if low_first and p[1] == 0:              # asr33_boot skips leading 00 bytes as leader: first byte must be non-zero
        put(p, 0x00, 0x0302)                 # CPT[0] is only a dummy address beyond the first 128 words
        print("CPT[0] changed to 0302 (first tape byte must not be 00)")

    tape = bytearray(enc(p)) + enc(loader)
    for a, e in blocks: tape += enc(img[a:e])
    open(out, 'wb').write(bytes(leader) + bytes(tape))

    # ---- self-check: replay the load as the simulator does (byte N -> address N) ----
    ram = bytearray(0x8000)                  # in memory byte-address order
    ram[0:0x80] = tape[0:0x80]
    pos = 0x80
    ram[LOADER_ADDR:LOADER_ADDR+len(loader)] = tape[pos:pos+len(loader)]; pos += len(loader)
    for a, e in blocks:
        ram[a:e] = tape[pos:pos+(e-a)]; pos += e - a
    assert pos == len(tape)
    be = enc(ram)                            # back to big-endian layout for comparison (swap is its own inverse)
    for a, e in [(0, 0x33D0)] + [(a, e) for a, e in blocks if a >= 0x7C00 or a >= 0x33D0]:
        if a == 0 and low_first:             # CPT[0] was changed on purpose
            assert be[2:0x33D0] == img[2:0x33D0], hex(a)
        else:
            assert be[a:e] == img[a:e], hex(a)
    print("tape %s: %d bytes  (leader %d + primary 128 + loader %d + image %d)" %
          (out, leader+len(tape), leader, len(loader), len(tape)-128-len(loader)))
    print("primary patch: M[0E]=%04X M[10]=%04X M[12]=%04X ; loader at %04X ; entry %04X" %
          (len(loader)-1, LOADER_ADDR-1, LOADER_ADDR, LOADER_ADDR, entry))
    for i, w in enumerate(code): print("  %04X  %04X" % (LOADER_ADDR+2*i, w))

main()
