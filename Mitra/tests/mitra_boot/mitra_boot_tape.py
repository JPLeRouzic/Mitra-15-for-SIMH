#!/usr/bin/env python3
"""
mitra_boot_tape.py - builds a Mitra-15 ASR-33 paper-tape boot image.

    python3 mitra_boot_tape.py            -> writes mitra_boot.ptp + mitra_boot.lst
    python3 mitra_boot_tape.py --selftest -> also runs the mini-emulator check

TAPE LAYOUT (matches asr33_boot() in IO_asr33.c)
    [16 x 0x00 leader]   skipped by asr33_boot
    [128 bytes primary]  loaded at 0x0000 by asr33_boot, started via the
                         level-6 context stored inside those 128 bytes
    [N bytes secondary]  read by the primary through WD (E=1, A=3) into 0x0080

Instruction word = [mode/opcode byte][8-bit displacement]
    P   (immediate)   LDA 20  LDE 21  ADD 25
    DG  (G+D, G=0)    LDA 40  AND 49  CMP 4B  STA 51
    RP/RM branches    C0/C8 BCT   C3/CB BCF   C5/CD BAZ   C7/CF BRU
                      target = instr_addr +/- 2*disp
    IG  branch        DF  BRU  Y = mem[G+D]   (G = 0 so absolute)
    LDR/STR P         F9 / FA  + register number (R9=09, R10=0A, R11=0B)
    SYS               F402 RD, F403 WD, F408 STM
"""
import sys

# ---------------------------------------------------------------- tunables
E_WD       = 0x0001      # E for WD to the ASR33  (asr33_wd requires E == 1)
CMD_TAPE   = 0x03        # A for "lecture ruban"
CMD_PRINT  = 0x02        # A for "ecriture clavier" (print)
CMD_KBD    = 0x01        # A for "lecture clavier"
SEC_BASE   = 0x0080      # where the secondary boot lands (right after primary)
SEC_ENTRY  = 0x0100      # first code byte of the secondary (data = 0x80..0xFF)
LEADER     = 16

# ---------------------------------------------------------------- assembler
class Asm:
    def __init__(self, origin):
        self.org = origin
        self.items = []          # (addr, kind, payload, comment)
        self.labels = {}
        self.pc = origin

    def label(self, name):
        self.labels[name] = self.pc

    def word(self, v, comment=""):
        self.items.append((self.pc, "w", v & 0xFFFF, comment)); self.pc += 2

    def ins(self, hi, lo=0, comment=""):
        self.word((hi << 8) | (lo & 0xFF), comment)

    def br(self, op_rp, label, comment=""):
        """relative branch; op_rp is the RP opcode (C0..C7), RM = op_rp+8"""
        self.items.append((self.pc, "br", (op_rp, label), comment)); self.pc += 2

    def text(self, s, comment=""):
        b = s.encode("ascii")
        assert len(b) % 2 == 0
        self.items.append((self.pc, "t", b, comment)); self.pc += len(b)

    def org_to(self, addr):
        assert addr >= self.pc, "overflow: %#x > %#x" % (self.pc, addr)
        self.pc = addr

    def link(self):
        mem = {}
        lst = []
        for addr, kind, p, c in self.items:
            if kind == "w":
                mem[addr] = p >> 8; mem[addr + 1] = p & 0xFF
                lst.append("%04X  %04X        %s" % (addr, p, c))
            elif kind == "t":
                for i, ch in enumerate(p):
                    mem[addr + i] = ch
                lst.append("%04X  %-12s  %s" % (addr, repr(p.decode())[:12], c))
            else:
                op, lab = p
                tgt = self.labels[lab]
                d = tgt - addr
                assert d % 2 == 0
                if d >= 0:
                    hi, disp = op, d // 2
                else:
                    hi, disp = op + 8, (-d) // 2
                assert 0 <= disp <= 255, "branch out of range to " + lab
                w = (hi << 8) | disp
                mem[addr] = w >> 8; mem[addr + 1] = w & 0xFF
                lst.append("%04X  %04X        %s" % (addr, w, c or ("-> " + lab)))
        return mem, lst


def image(mem, lo, hi):
    return bytes(mem.get(a, 0) for a in range(lo, hi))

# ---------------------------------------------------------------- SECONDARY
def build_secondary():
    a = Asm(SEC_BASE)
    # ---------- data area 0x80..0xFF (DG-addressable) ----------
    D = {}
    def data_text(name, s):
        D[name] = a.pc; a.text(s, name)
    def data_word(name, v):
        D[name] = a.pc; a.word(v, name)
    data_text("MSG1", "Secondary boot listening\r\n")     # 26 bytes
    data_text("MSG2", "Dummy subroutine running\r\n")     # 26 bytes
    data_word("W_RE", 0x5245)     # 'RE'
    data_word("W_PE", 0x5045)     # 'PE'
    data_word("W_AT", 0x4154)     # 'AT'
    data_word("W_GO", 0x474F)     # 'GO'
    data_word("CRL",  0x0D00)     # CR in left byte (also R11: compare char)
    data_word("MCASE", 0xDFDF)    # upper-case mask (accept lower case too)
    data_word("MASKL", 0xFF00)
    data_word("MASKR", 0x00FF)
    data_word("CRR",  0x000D)     # CR in right byte
    data_word("R9KEY", 0x801F)    # z=1 (stop on compare), count-1 = 31
    data_word("RET1", 0)          # return address of PRT
    data_word("RET2", 0)          # return address of DUMMY
    data_word("DFLAG", 0)         # set by dummy subroutine
    a.org_to(0xE0)
    D["BUF"] = a.pc               # 32-byte command buffer 0xE0..0xFF
    BUF = D["BUF"]
    a.pc = SEC_ENTRY

    # ---------- helper emitters ----------
    def LDAi(v, c=""): a.ins(0x20, v, c or "LDA #%#x" % v)
    def LDEi(v, c=""): a.ins(0x21, v, c or "LDE #%#x" % v)
    def ADDi(v, c=""): a.ins(0x25, v, c or "ADD #%#x" % v)
    def LDA(n, c=""):  a.ins(0x40, D[n], c or "LDA %s" % n)
    def LDAb(i, c=""): a.ins(0x40, BUF + 2 * i, c or "LDA BUF[%d]" % i)
    def AND(n, c=""):  a.ins(0x49, D[n], c or "AND %s" % n)
    def CMP(n, c=""):  a.ins(0x4B, D[n], c or "CMP %s" % n)
    def STA(n, c=""):  a.ins(0x51, D[n], c or "STA %s" % n)
    def STAb(i, c=""): a.ins(0x51, BUF + 2 * i, c or "STA BUF[%d]" % i)
    def STR(r, c=""):  a.ins(0xFA, r, c or "STR R%d" % r)
    def LDR(r, c=""):  a.ins(0xF9, r, c or "LDR R%d" % r)
    def WD(c="WD"):    a.word(0xF403, c)
    def call(target, ret, c):
        LDR(0, "LDR R0 : A = P (address of next instr)")
        ADDi(6, "ADD #6 : A = address after the BRU")
        STA(ret, "STA %s : save return address" % ret)
        a.br(0xC7, target, "BRU %s  (%s)" % (target, c))
    def ret(slot):
        a.ins(0xDF, D[slot], "BRU (%s) : return" % slot)

    # ---------- code ----------
    a.label("START")
    a.word(0xF408, "STM : mask interrupts (no CPT needed for ASR33 irq)")
    LDAi(25, "A = count-1 (26 bytes)")
    STR(9)
    LDAi(D["MSG1"] - 1, "A = address-1")
    STR(10)
    call("PRT", "RET1", "print greeting")

    a.label("LISTEN")
    LDAi(0, "A = 0")
    for i in range(16):
        STAb(i, "clear command buffer word %d" % i)
    LDA("R9KEY");  STR(9, "R9  = z=1, count-1=31")
    LDAi(BUF - 1, "A = BUF-1"); STR(10, "R10 = BUF-1")
    LDA("CRL");    STR(11, "R11 = compare char CR (high byte)")
    LDEi(E_WD);    LDAi(CMD_KBD, "A = lecture clavier"); WD("WD : start keyboard read")
    a.label("WAITCR")
    LDR(11, "R11 low byte = last char transferred")
    AND("MASKR")
    CMP("CRR", "CMP #CR")
    a.br(0xC0, "GOTLINE", "BCT GOTLINE")
    a.br(0xC7, "WAITCR", "BRU WAITCR")

    a.label("GOTLINE")
    # --- REPEAT ---
    for i, w in enumerate(["W_RE", "W_PE", "W_AT"]):
        LDAb(i); AND("MCASE"); CMP(w); a.br(0xC3, "TRYGO", "BCF TRYGO")
    LDAb(3); AND("MASKL"); CMP("CRL"); a.br(0xC3, "TRYGO", "BCF TRYGO")
    # REPEAT recognised
    LDAi(25, "A = count-1"); STR(9)
    LDAi(D["MSG1"] - 1); STR(10)
    call("PRT", "RET1", "re-send greeting")
    a.br(0xC7, "LISTEN", "BRU LISTEN")

    a.label("TRYGO")
    LDAb(0); AND("MCASE"); CMP("W_GO"); a.br(0xC3, "LISTEN", "BCF LISTEN (unknown)")
    LDAb(1); AND("MASKL"); CMP("CRL"); a.br(0xC3, "LISTEN", "BCF LISTEN (unknown)")
    call("DUMMY", "RET2", "launch dummy subroutine")
    a.br(0xC7, "LISTEN", "BRU LISTEN")

    # ---------- subroutine PRT : R9/R10 already loaded by caller ----------
    a.label("PRT")
    LDEi(E_WD); LDAi(CMD_PRINT, "A = ecriture clavier"); WD("WD : start print")
    a.label("PRTW")
    LDR(9, "R9 -> 0 when the transfer is complete")
    a.br(0xC5, "PRTX", "BAZ PRTX")
    a.br(0xC7, "PRTW", "BRU PRTW")
    a.label("PRTX")
    ret("RET1")

    # ---------- the dummy subroutine ----------
    a.label("DUMMY")
    LDAi(0x55); STA("DFLAG", "STA DFLAG : visible side effect (M[DFLAG]=0x0055)")
    LDAi(25, "A = count-1"); STR(9)
    LDAi(D["MSG2"] - 1); STR(10)
    call("PRT", "RET1", "print MSG2")
    LDAi(0xAA); STA("DFLAG", "DFLAG = 0x00AA : completed")
    ret("RET2")

    mem, lst = a.link()
    end = a.pc
    if end % 2: end += 1
    return mem, lst, SEC_BASE, end, a.labels, D

# ---------------------------------------------------------------- PRIMARY
def build_primary(sec_len):
    a = Asm(0)
    # --- 0x00..0x21 : CPT + level-6 context (see notes in the answer) ---
    a.word(0x0300, "CPT[0]   (first byte must be non-zero: asr33_boot skips 00s)")
    a.word(0x0000); a.word(0x0000)
    a.word(0x0014, "CPT[6]   (index = level, as cpt_lookup() computes today)")
    a.word(0x0000)
    a.word(0x0000, "M[10]    = CPT base = 0")
    a.word(0x0014, "CPT[6]   (index = 2*level, in case cpt_lookup is fixed)")
    D_R9, D_R10, D_ENT = a.pc, a.pc + 2, a.pc + 4
    a.word(sec_len - 1, "R9  value : bytes to read - 1")
    a.word(SEC_BASE - 1, "R10 value : load address - 1")
    a.word(SEC_ENTRY, "secondary entry point")
    assert a.pc == 0x14
    a.word(0x2000, "ctx6.indicators")
    a.word(0x0000, "ctx6.X"); a.word(0x0000, "ctx6.E"); a.word(0x0000, "ctx6.A")
    a.word(0x0000, "ctx6.G"); a.word(0x0000, "ctx6.L")
    a.word(0x0022, "ctx6.P  = entry of primary code")
    assert a.pc == 0x22
    # --- code ---
    a.word(0xF408, "STM : mask interrupts en tête du code primaire (à 0x0022) pour masquer les interruptions avant le WD. Cela empêche l'interruption de niveau 6 de redémarrer le primaire.")
    a.ins(0x40, D_R9);  a.ins(0xFA, 9,  "LDA/STR R9  = count-1")
    a.ins(0x40, D_R10); a.ins(0xFA, 10, "LDA/STR R10 = addr-1")
    a.ins(0x21, E_WD,   "LDE #1")
    a.ins(0x20, CMD_TAPE, "LDA #3 (lecture ruban)")
    a.word(0xF403, "WD : start tape read")
    a.label("W")
    a.ins(0xF9, 9, "LDR R9 -> 0 when done")
    a.br(0xC5, "GO", "BAZ GO")
    a.br(0xC7, "W", "BRU W")
    a.label("GO")
    a.ins(0xDF, D_ENT, "BRU (SEC_ENTRY)")
    assert a.pc <= 128
    mem, lst = a.link()
    return image(mem, 0, 128), lst

# ---------------------------------------------------------------- build
def build():
    smem, slst, lo, hi, labels, D = build_secondary()
    sec = image(smem, lo, hi)
    prim, plst = build_primary(len(sec))
    tape = bytes(LEADER) + prim + sec
    listing = ["; ---- PRIMARY (0x0000..0x007F, loaded by asr33_boot) ----"] + plst + \
              ["", "; ---- SECONDARY (loaded at %#06x, %d bytes) ----" % (lo, len(sec))] + slst
    return tape, prim, sec, listing, labels, D

# ---------------------------------------------------------------- mini emulator
# Follows the semantics of the uploaded sources (NOT the simulator itself):
#  * STR/LDR on R9-R11 act on a real register file (needs the instr_STR/LDR fix)
#  * ASR33 engine moves one char per ASR_PERIOD instructions
class Emu:
    ASR_PERIOD = 40
    def __init__(self, tape, typed):
        self.M = bytearray(65536)
        self.R = {9: 0, 10: 0, 11: 0}
        self.A = self.E = self.X = 0
        self.C = self.OV = 0
        self.out = []
        self.tape = tape; self.tp = 0
        self.typed = typed              # list of (instr_threshold, bytes)
        self.kbd = bytearray()
        self.eng = None; self.steps = 0
        # asr33_boot(): skip leader, load 128 bytes at 0
        while tape[self.tp] == 0: self.tp += 1
        self.M[0:128] = tape[self.tp:self.tp + 128]; self.tp += 128
        base = self.w(10)
        ctx = self.w(base + 6)
        self.P = self.w(ctx + 12)
    def w(self, a): return (self.M[a] << 8) | self.M[a + 1]
    def sw(self, a, v): self.M[a] = (v >> 8) & 255; self.M[a + 1] = v & 255
    def ld(self, v): self.C = int(v == 0); self.OV = int(bool(v & 0x8000))
    def engine(self):
        e = self.eng
        if not e: return
        mode, addr, left = e["mode"], e["addr"], e["left"]
        if mode == 3:
            self.out.append(chr(self.M[addr])); e["addr"] += 1; e["left"] -= 1
        elif mode == 2:
            self.M[addr] = self.tape[self.tp]; self.tp += 1
            self.R[11] = (self.R[11] & 0xFF00) | self.M[addr]
            e["addr"] += 1; e["left"] -= 1
        elif mode == 1:
            if not self.kbd: return
            ch = self.kbd.pop(0); self.M[addr] = ch
            self.R[11] = (self.R[11] & 0xFF00) | ch
            e["addr"] += 1; e["left"] -= 1
            if e["z"] and ch == e["cmp"]:
                self.R[9] = (e["left"] & 0x7FFF) | 0x8000; self.eng = None; return
        if e["left"] == 0:
            self.R[9] = 0; self.eng = None
    def run(self, nmax):
        for _ in range(nmax):
            self.steps += 1
            for thr, data in list(self.typed):
                if self.steps >= thr:
                    self.kbd += data; self.typed.remove((thr, data))
            if self.steps % self.ASR_PERIOD == 0: self.engine()
            P = self.P; inst = self.w(P); self.P = (P + 2) & 0xFFFF
            hi, lo = inst >> 8, inst & 255
            if inst == 0: continue
            if hi == 0x20: self.A = lo; self.ld(self.A)
            elif hi == 0x21: self.E = lo; self.ld(self.E)
            elif hi == 0x25: self.A = (self.A + lo) & 0xFFFF
            elif hi == 0x40: self.A = self.w(lo); self.ld(self.A)
            elif hi == 0x49: self.A &= self.w(lo); self.ld(self.A)
            elif hi == 0x4B:
                m = self.w(lo); sa = self.A - 65536 if self.A & 0x8000 else self.A
                sm = m - 65536 if m & 0x8000 else m
                self.C, self.OV = (1, 0) if sa == sm else ((0, 1) if sa < sm else (0, 0))
            elif hi == 0x51: self.sw(lo, self.A)
            elif hi in (0xC0, 0xC3, 0xC5, 0xC7, 0xC8, 0xCB, 0xCD, 0xCF):
                k = hi & 7
                ok = {0: self.C, 3: not self.C, 5: self.A == 0, 7: True}[k]
                if ok:
                    d = 2 * lo
                    self.P = (P + d if hi < 0xC8 else P - d) & 0xFFFF
            elif hi == 0xDF: self.P = self.w(lo)
            elif hi == 0xF9:
                self.A = {0: self.P}.get(lo, self.R.get(lo)); self.ld(self.A)
            elif hi == 0xFA: self.R[lo] = self.A; self.ld(self.A)
            elif inst == 0xF408: pass
            elif inst == 0xF403:
                assert self.E == 1, "E must be 1"
                r9, r10 = self.R[9], self.R[10]
                self.eng = dict(mode={1: 1, 2: 3, 3: 2}[self.A], addr=r10 + 1,
                                left=(r9 & 0x7FFF) + 1, z=r9 >> 15,
                                cmp=(self.R[11] >> 8) & 255)
            else:
                raise RuntimeError("unimplemented %04X at %04X" % (inst, P))

def selftest(tape, D):
    emu = Emu(tape, [(150000, b"REPEAT\r"), (300000, b"go\r"),
                     (450000, b"BOGUS\r"), (600000, b"Repeat\r")])
    emu.run(900000)
    text = "".join(emu.out)
    print("---- emulated TTY output ----"); print(text.replace("\r", ""), end="")
    assert text.count("Secondary boot listening") == 3, "greeting count"
    assert text.count("Dummy subroutine running") == 1, "dummy subroutine"
    assert emu.w(D["DFLAG"]) == 0x00AA, "dummy flag"
    print("---- self-test OK (greeting x3, dummy x1, DFLAG=%#06x) ----" % emu.w(D["DFLAG"]))

if __name__ == "__main__":
    tape, prim, sec, listing, labels, D = build()
    open("mitra_boot.ptp", "wb").write(tape)
    open("mitra_boot.lst", "w").write("\n".join(listing) + "\n")
    print("mitra_boot.ptp : %d bytes (leader %d, primary %d, secondary %d)" %
          (len(tape), LEADER, len(prim), len(sec)))
    if "--selftest" in sys.argv:
        selftest(tape, D)
