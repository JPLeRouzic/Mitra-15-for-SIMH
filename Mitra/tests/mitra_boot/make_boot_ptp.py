#!/usr/bin/env python3
"""Build the Mitra-15 two-stage bootstrap paper tape (mitra_boot.ptp) from a listing.

usage: make_boot_ptp.py mitra_boot_v2.lst mitra_boot.ptp [--leader 16] [--big-endian] [--dump]

Listing lines (everything after ';' or not starting with an address is ignored):
    AAAA  HHHH   comment      one 16-bit word at byte address AAAA
    AAAA  "text" label        bytes in memory order, \\r \\n \\t \\xHH escapes allowed
    AAAA  =XY    label        two characters packed in one word (X = first character)

Byte order (default, as the simulator does today):  tape byte N is stored at memory byte address N,
an EVEN address is the RIGHT (low) byte of a word and an ODD address the LEFT (high) byte, so the
tape carries the LOW byte of every word first.  --big-endian gives the old order (high byte first).

Tape = <leader 00 bytes> + <primary: bytes 0x00..0x7F, read by asr33_boot> + <secondary: bytes 0x80..end,
read by the primary through WD with R9 = count-1, R10 = 0x7F>.
"""
import re, sys, codecs

def parse(path, low_first):
    mem = {}
    for ln, line in enumerate(open(path, encoding='utf-8'), 1):
        line = line.rstrip('\n').rstrip('\r')
        if not line.strip() or line.lstrip().startswith(';'):
            continue
        m = re.match(r'^([0-9A-Fa-f]{4})\s+(.*)$', line)
        if not m:
            continue
        a = int(m.group(1), 16); rest = m.group(2)
        if rest.startswith('"'):                                   # text, bytes in memory order
            j = 1
            while True:                                            # find the closing quote (not escaped)
                j = rest.index('"', j)
                if rest[j-1] != '\\': break
                j += 1
            text = codecs.decode(rest[1:j], 'unicode_escape')
            for i, ch in enumerate(text.encode('latin-1')): mem[a+i] = ch
        elif rest.startswith('='):                                 # two characters packed in a word
            x, y = rest[1], rest[2]
            w = (ord(y) << 8 | ord(x)) if low_first else (ord(x) << 8 | ord(y))
            put_word(mem, a, w, low_first)
        else:
            tok = rest.split()[0]
            if not re.fullmatch(r'[0-9A-Fa-f]{4}', tok):
                continue
            put_word(mem, a, int(tok, 16), low_first)
    return mem

def put_word(mem, a, w, low_first):
    assert a % 2 == 0, "word at odd address %04X" % a
    lo, hi = w & 0xFF, w >> 8
    if low_first: mem[a], mem[a+1] = lo, hi        # even address = right (low) byte
    else:         mem[a], mem[a+1] = hi, lo

def main():
    args = [x for x in sys.argv[1:] if not x.startswith('--')]
    lst, out = args[0], args[1]
    low_first = '--big-endian' not in sys.argv
    leader = 16
    if '--leader' in sys.argv: leader = int(sys.argv[sys.argv.index('--leader') + 1])

    mem = parse(lst, low_first)
    end = max(mem) + 1
    image = bytearray(mem.get(a, 0) for a in range(end))
    word = (lambda a: (image[a+1] << 8 | image[a]) if low_first else (image[a] << 8 | image[a+1]))

    r9, r10, entry = word(0x0E), word(0x10), word(0x12)
    sec_len = end - 0x80
    if r9 + 1 != sec_len:
        print("WARNING: R9+1 = %d but the secondary is %d bytes long (0x%X): fix M[0E] in the listing"
              % (r9 + 1, sec_len, sec_len))
    if image[0] == 0:
        sys.exit("ERROR: first tape byte is 00: asr33_boot would skip it as leader")
    open(out, 'wb').write(bytes(leader) + bytes(image))

    # ---- self-check: replay asr33_boot + the primary's WD read ----
    t = open(out, 'rb').read()
    i = 0
    while t[i] == 0: i += 1                                  # leader skipped by asr33_boot
    ram = bytearray(0x10000)
    ram[0:0x80] = t[i:i+0x80]                                # first 128 bytes -> addresses 0..0x7F
    n = word_at = None
    rw = lambda a: (ram[a+1] << 8 | ram[a]) if low_first else (ram[a] << 8 | ram[a+1])
    count, start = (rw(0x0E) & 0x7FFF) + 1, rw(0x10) + 1    # what the primary loads into R9/R10
    ram[start:start+count] = t[i+0x80:i+0x80+count]
    assert ram[:end] == image, "replayed memory differs from the listing"
    print("%s: %d bytes = leader %d + primary 128 + secondary %d" % (out, len(t), leader, sec_len))
    print("seen by the simulator:  CPT[0]=%04X  CPT[6]=%04X  M[0A]=%04X  R9=%04X  R10=%04X  entry=%04X  ctx6: status=%04X P=%04X"
          % (rw(0), rw(0xC), rw(0xA), rw(0xE), rw(0x10), rw(0x12), rw(rw(0xC)), rw(rw(0xC) + 12)))
    if '--dump' in sys.argv:
        for a in range(0, end, 16):
            print("%04X  %s" % (a, ' '.join('%02X' % b for b in image[a:a+16])))

main()
