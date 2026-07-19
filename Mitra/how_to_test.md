# Some quick ways to test your Mitra-15 SIMH simulator:
## 1. Basic SIMH Smoke Test

check the correct syntax with:
sim> help set cpu

Create a minimal test script test_mitra.ini:
ini

; Test script for Mitra-15 simulator
; Minimal initialization

set cpu 32K
set cpu history=100
show cpu

; Load a simple test program (if you have one)
; load test.hexFor examine cpu, you need to specify registers individually:
text

sim> examine cpu A
sim> examine cpu E
sim> examine cpu X
sim> examine cpu P
sim> examine cpu L
sim> examine cpu G

Or to see multiple registers:
text

sim> examine cpu A E X P L G

For deposit with hex values:
text

sim> deposit 100 0x0A5A

Or use octal:
text

sim> deposit 100 012532

2. CPU Registers Names

The CPU registers available for examine are:

    A - Accumulator

    E - E register (double-word operations)

    X - Index register

    P - Program Counter

    L - Local base register

    G - Global base register

    S - S register (status)

    U - U register

    MS - Master/Slave mode flag

    MA - Interrupt mask

    PR - Protection flag

; Run for a few instructions
run 100

; Show registers
examine cpu

; Show memory at address 0
examine 0 16

; Quit
quit

Run it:
bash

./BIN/mitra test_mitra.ini

## 2. Interactive Commands Test

Start the simulator interactively and try these commands:
bash

./BIN/mitra

Then at the SIMH prompt:
text

sim> show cpu
sim> show dev
sim> show reg
sim> examine 0
sim> examine 10 8
sim> set cpu 16K
sim> show cpu
sim> quit

Expected outputs:

    show cpu should display CPU configuration without crashes

    examine should show memory contents (all zeros initially)

    No segmentation faults or assertion errors

## 3. Simple Memory Test

Create mem_test.ini:
ini

; Memory test
set cpu 32K

; Write some test values
deposit 0 1234
deposit 1 5678
deposit 2 ABCD
deposit 3 DEAD
deposit 4 BEEF

; Read them back
examine 0 5

; Test byte access
deposit 0 F0F0
deposit 1 0F0F
examine 0 2

quit

## 4. Test I/O Operations

Create io_test.ini:
ini

; I/O test
set cpu 32K

; Initialize ASR33 (console)
attach asr33 console
show asr33

; Initialize printer
attach printer print.out
show printer

; Initialize paper tape reader
attach ptr test.ptp
show ptr

; Run a few instructions
run 10

quit

## 5. Quick Program Load Test

Create a simple machine code program in hex format hello.hex:
text

; Simple loop test program (Mitra-15 machine code)
; Address: Instruction (opcode)
0000: 2000    ; LDA immediate 0 (if supported)
0001: 0000
0002: 3001    ; ADD immediate 1
0003: 0001
0004: C700    ; BRU unconditional branch to 0000
0005: 0000

Test script run_hello.ini:
ini

; Load and test a program
set cpu 32K

; Set G register (base)
deposit 5 0

; Load program at address 0
load hello.hex

; Set program counter to starting address
deposit 0 0

; Show initial state
examine cpu

; Run 20 instructions
run 20

; Show registers after run
examine cpu

; Show memory at program area
examine 0 10

quit

## 6. Use the SIMH do Command

Create quick_test.cmd:
cmd

; Quick sanity test
set cpu 32K
show cpu
deposit 0 1234
deposit 1 5678
deposit 2 9ABC
examine 0 3
run 1
show reg
quit

Run it:
bash

./BIN/mitra do quick_test.cmd

## 7. Monitor for Common Issues

Watch for these error messages:

    "Address out of range" → Memory size issue

    "Invalid instruction" → CPU decoding problem

    "I/O error" → Device emulation issue

    "Segmentation fault" → Memory corruption

    "Register sanity check" → Register macro problems

## 8. Minimal Function Test Program

If you have an assembler, create this simple test:
assembly

; Test program for Mitra-15
        ORG     0
START:  LDA     #0      ; Load A with 0
LOOP:   ADD     #1      ; A = A + 1
        CMP     #10     ; Compare with 10
        BCT     LOOP    ; Branch if carry (A < 10)
        HALT            ; Stop when A >= 10
        END     START

## 9. Expected Quick Test Results

A working simulator should:

    Start without crashes - No segfaults or assertion failures

    Respond to commands - show, examine, deposit work

    Execute instructions - At least simple operations

    Handle memory - Read/write to various addresses

    Process I/O - Basic device commands work

    Exit cleanly - quit command works

## 10. Debug Build Test

Compile with debug flags to catch issues early:
bash

make clean
make CFLAGS="-g -O0 -DDEBUG"

Then run with a debugger:
bash

gdb ./BIN/mitra
(gdb) run -h
(gdb) break sim_instr
(gdb) continue

Quick Checklist for Basic Functionality:

    Simulator starts (./BIN/mitra works)

    show cpu displays CPU state

    examine 0 shows memory content

    deposit 0 1234 writes to memory

    run 10 executes instructions

    quit exits cleanly

    No error messages on startup

    No segmentation faults

    show dev lists configured devices

    attach commands work for basic devices
