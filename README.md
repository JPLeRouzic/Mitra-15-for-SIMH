# Mitra-15-for-SIMH
<img width="1100" height="676" alt="image" src="https://github.com/user-attachments/assets/6f6851af-2893-42f9-b2e9-495054bdb784" />
https://ajovomultja.hu/mitra-15

## A Mitra-15 Simulator for SIMH

This is a preliminary version. For now, my code compiles, but it is still not a true Mitra-15 simulator; rather, it is a tentative effort. Many thanks to Pascal Chour for the very useful documentation available on his website: https://www.pascalchour.fr/ressources/cii/mitra15.htm

As my C is rusty (pun is intended) I started with an existing SDS 940 simulator for SIMH and slowly modified it towards Mitra-15's characteristics.

The Mitra-15 is a microcomputer that features interesting concepts, such as the ability to program input/output peripherals using microcode, instead of using an external mechanism such as DMA.
https://en.wikipedia.org/wiki/Mitra_15

The Mitra-15 was a 16-bit minicomputer developed by **CII (Compagnie Internationale pour l'Informatique)** in the early 1970s. It was widely used in industrial automation, scientific computing, education, and military applications.
I never programmed on Mitra-15, but I learned CS on a CII 10070 and later I worked at France Telecom which had telephone exchanges piloted with the Mitra family.

Although it was once widespread in France, very little software and documentation has survived. This project contributes to the preservation of this important part of computing history. 

This Mitra-15 simulator is intended to eventually include:
```
- System and optional instructions
- Extensive comments describing the original hardware behavior
- SIMH console support
- Memory management
- Interrupt, fast interrupt, suspensions and trap systems
* printer
* analogic interface
* punched_tape
* DRI fixed disk
* sagem fixed disk
* card reader
* magnetic tape reader
- Various I/O devices, but I lack documentation for many of those peripherals:
* asynchronous channels 
* synchronous channels 
* fast channel multiplexed
* fast channel ADM (no idea what it is)
* IOPs (if it's not the same as asynchronous channels).
```
To this day I did:
- search for and analyzed the few remaining documents (see /doc folder).
- create a working SIMH environment for SIMH (deposit/examine/run/break/etc)
- create a complete instruction decoder for each instruction and addressing mode
- create code for common Mitra and CII devices
- create a convincing (not tested) code for RD and WD instructions for simple communication with devices such as ASR33 or line printer.
- create a convincing (not tested) code for DRI disks (UK's Data Recording Instrument) that uses the suspension system, the CII invention that aims at a similar goal as modern DMA.
- create a test program.
- test the branch instructions in RP (immediate in modern parlance) addressing mode, so at least this part should be correct.
- I have also set out to reconstruct CII's MTR (Real-Time Monitor) from a hexadecimal dump found in a PDF file; however, as the OCR output is abysmal, I have to visually verify every single byte. This will likely be the only piece of original CII code against which my simulator is tested, as everything else will have to be inferred from the documentation. It is therefore crucial that this test be successful.
- It's not useful but I have also created a rudimentary assembler (not tested).

On the long term I have plans to port RSX280 (RSX-11 clone) to the Mitra. Z80's BC and DE are similar to L and G registers, IX is an index register as Mitra's X, A and HL resemble to A and E. Main problems: Mitra has less registers, less addressing modes than the Z80, and it lacks a concept of a stack, which makes managing reentrancy complicated.

## Requirements

- GCC or Clang
- Make
- Linux (tested)
- A recent version of openSIMH

Within the SIMH monitor, you can then load memory, examine registers, and execute Mitra-15 programs.

## Acknowledgments

Many thanks to:

- The SIMH developers for creating and maintaining the simulator

- Everyone who has contributed to keeping the history of French computing alive but mainly Pascal Chour
https://www.pascalchour.fr/ressources/cii/mitra15.htm
