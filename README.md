# Mitra-15-for-SIMH
An attempt at creating a branch for Mitra-15 in SIMH. Works in progress.

This is a very early attempt, unfortunately I didn't started with the "official" SIMH project.
At the moment I can compile my code, but its not really a Mitra-15 simulator but something in between the SDS 940 and Mitra-15.
I choosed to modify the SDS 940 code because:
- Mitra engineers have some knowledge of the SDS Sigma 3 series because CII in 1974-1976 merged with Honeywell-Bull.
At that time the SDS Sigma 3 was 9 years old and the CII 10070 computer was a rebadged Sigma 7.
Scientific Data Systems (SDS) was sold to Xerox in 1969. In 1975, Xerox sold its computer business to Honeywell, Inc. which continued support for the Sigma line for a time.

The SDS 940 and Mitra-15 are both 16 bit computers and bit 0 is on the left which is unusual on US computers.
Their memory system and peripherals are unfortunately very different.

Mitra-15 is a microprogramed computer, and has interesting ideas about multi-cpu and flexibility of IO devices.
