# Mitra-15-for-SIMH

## A Mitra-15 Simulator for SIMH

This is a preliminary version. For now, my code compiles, but it is not a Mitra-15 simulator in the strict sense; rather, it is a tentative effort toward a fully functional simulator. Many thanks to Pascal Chour for the very useful documentation available on his website: https://www.pascalchour.fr/ressources/cii/mitra15.htm

The Mitra-15 is a microcomputer that features interesting concepts, such as the ability to program input/output peripherals using microcode.
https://en.wikipedia.org/wiki/Mitra_15

The Mitra-15 was a 16-bit minicomputer developed by **CII (Compagnie Internationale pour l'Informatique)** in the early 1970s. It was widely used in industrial automation, scientific computing, education, and military applications.
I never programmed on Mitra-15, but I learned CS on a CII 10070 and later I worked at France Telecom which had telephone exchanges piloted with the Mitra family.

Although it was once widespread in France, very little software and documentation has survived. This project contributes to the preservation of this important part of computing history. 

This Mitra-15 simulator is intended to eventually include:
- Complete instruction decoder
- SIMH console support
- Memory management
- Interrupt, fast interrupt, and trap systems
- Various I/O devices, but I lack documentation for many of those peripherals:
IO_coupleurs_asynchrones.c
IO_fast_channel_multiplexed.c
IO_printer.c
IO_analogic_interface.c
IO_coupleurs_synchrones.c
IO_front_panel.c
IO_punched_tape.c
IO_DRI_fix_disk.c
IO_IOP_1.c and IO_IOP_2.c
IO_sagem_fix_disk.c
IO_card_reader.c
IO_fast_channel_1_ADM.c
IO_fast_channel_2_ADM.c
IO_magn_tape.c
- System instructions
- Extensive comments describing the original hardware behavior

---

## Requirements

- GCC or Clang
- Make
- Linux (tested)
- A recent version of openSIMH

Within the SIMH monitor, you can then load memory, examine registers, and execute Mitra-15 programs.

---

## Documentation

Please look at the doc folder. 
The implementation is primarily based on the original Mitra-15 technical documentation, including:

- Processor reference manual
- Programming manual
- Hardware documentation
- Original instruction set descriptions

The simulator attempts to reproduce the documented behavior as closely as possible, though significant unknowns remain. I am proceeding on the assumption that the engineers who designed the Mitra-15 were familiar with Scientific Data Systems computers; after all, the machine the Mitra-15 was intended to replace within CII's lineup was the CII 10070—which was essentially an SDS Sigma 7 produced under license. ---

## Repository Structure

```
Mitra/
mitra_cpu.c Processor implementation

mitra_defs.h Processor definitions

mitra_sys.c SIMH integration

mitra_io.c I/O peripherals

IO_xxx.c Specific peripheral implementations

---

## Acknowledgments

Many thanks to:

- The SIMH developers for creating and maintaining the simulator

- The historians and collectors who preserve CII documentation

- Everyone who has contributed to keeping the history of French computing alive
