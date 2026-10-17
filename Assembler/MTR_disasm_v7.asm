; ---------------------------------------------------------------------
; Mitra-15 MTR (Real Time Monitor) Disassembly with symbolic labels
; 
; Prefixes: E_ entry  S_ SPA-called subroutine  L_ branch target  V_ variable  T_ text
;           lower case (l_ v_ ...) = referenced only from code the flow trace did not reach
; Named structures: SEC<n>/LDS<n>/PRT<n> (program sections), MON<n>/PRTS<n> (supervisor),
;           INT<k>/CTX<k>/CPT/DVT (interrupts), TWB_* (monitor-call work block)
; ---------------------------------------------------------------------
; STRUCTURE OF THE RESIDENT MTR
; The resident kernel is always loaded starting at absolute address 0 of the RAM.
; It comprises:
; a) System data (CDS/TWB), b) Common subroutines, c) Handlers, d) Immediate tasks
; Memory Zones: Foreground, Background, Common (accessed at G+6).
; ---------------------------------------------------------------------
; STRUCTURE OF THE RESIDENT MTR
; The resident kernel is always loaded starting at absolute address 0 of the RAM.
; It comprises:
; a) System data: Communication addresses, Save area, System table addresses, etc.
; b) Common subroutines: Modules accessible via CSV calls, or strictly internal.
; c) Handlers: Physical control of peripheral transfers (e.g., MAE/ASR33).
; d) Immediate tasks: Console, Clock, Power failure, User immediate tasks.
;
; Memory Zones:
; - Foreground zone: End of resident kernel to beginning of background zone.
; - Background zone: End of foreground to beginning of common zone (Level zero).
; - Common zone: Address accessed at G+6. Used for communication between programs.
;
; Assumed run-time values (static analysis cannot know them): 
; G=#0000 (not given: G-relative operands shown as TWB_xx / G+xxxx; PRT not located)  
; L=#0000
;   indirect base G' = G;  data D = bytes;  branch D = words, P = address of the current instruction ($+n)
;   indirect pointers are followed when the cell in the image is non-zero and points inside the image
;   X is unknown: indexed accesses are labelled by their base (label+X)
; * PRTS: pointer at &C = #3002, 56 module(s) found: MON0_TRAP (CSV 0, trap handler), M_EXIT.. = CSV n services; modules run in master mode and end with RSV
; * CPT: pointer at &A = #0380, 31 context(s) found: CTX<level> (7 words: status X E A G L P), their P fields are the interrupt routines INT<level> (end with DIT/DITR); DVT = 32 words below the CPT
; * Supervisor module entries: 27 sequence(s) 'SPA cell ; BRU body ; RSV' found (labelled MOD_xxxx): each is the entry of a module called with CSV
; 
; MODULE INDEX (entry sequences found, by address):
;   MOD_0514 @0514: shifts/bit manipulation 8 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_056E S_06C2 S_06C4 S_07EA ..., returns to its CSV caller with RSV
;   MOD_0792 @0792: shifts/bit manipulation 6 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_06C4 S_07EA S_078C, returns to its CSV caller with RSV
;   M_WAIT @083E: 2 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, ends with DIT/DITR
;   M_HXBN @0890: comparisons, 1 loop, stores into the caller's TWB, returns to its CSV caller with RSV
;   M_BNHX @08D2: comparisons, 1 loop, stores into the caller's TWB, returns to its CSV caller with RSV
;   M_DCBN @08F0: multiply/divide 1 loop, stores into the caller's TWB, returns to its CSV caller with RSV
;   M_ASEB @0962: 1 loop, stores into the caller's TWB, returns to its CSV caller with RSV
;   M_EBAS @0980: comparisons, 2 loops, stores into the caller's TWB, returns to its CSV caller with RSV
;   MOD_0FBC @0FBC: stores into the caller's TWB, returns to its CSV caller with RSV
;   MOD_12B0 @12B0: stores into the caller's TWB, returns to its CSV caller with RSV
;   MOD_13E4 @13E4: stores into the caller's TWB, returns to its CSV caller with RSV
;   M_MOVE @144E: 1 loop, stores into the caller's TWB, returns to its CSV caller with RSV
;   M_EDIT @1464: 4 loops, stores into the caller's TWB, returns to its CSV caller with RSV
;   M_ERED @14EA: 3 loops, stores into the caller's TWB, returns to its CSV caller with RSV
;   M_KILL @153A: shifts/bit manipulation privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, returns to its CSV caller with RSV
;   M_CMPA @163C: comparisons, 1 loop, stores into the caller's TWB, returns to its CSV caller with RSV
;   M_DUMP @165E: 6block move / translate string (MVS/TRS) 4 loops, stores into the caller's TWB, returns to its CSV caller with RSV
;   MOD_16FC @16FC: stores into the caller's TWB, returns to its CSV caller with RSV
;   M_IT @1702: shifts/bit manipulation privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, returns to its CSV caller with RSV
;   M_CMPS @179A: comparisons, 1 loop, stores into the caller's TWB, returns to its CSV caller with RSV
;   M_ABIO @17C6: comparisons, 7 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, supervisor calls: #3, returns to its CSV caller with RSV
;   M_BIB @19E2: comparisons, 3 loops, stores into the caller's TWB, returns to its CSV caller with RSV
;   MOD_1A68 @1A68: stores into the caller's TWB, returns to its CSV caller with RSV
;   MOD_1A70 @1A70: stores into the caller's TWB, returns to its CSV caller with RSV
;   MON49 @1B80: shifts/bit manipulation returns to its CSV caller with RSV
;   MOD_1BF8 @1BF8: comparisons, 1 loop, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, returns to its CSV caller with RSV
;   MOD_1C68 @1C68: stores into the caller's TWB, returns to its CSV caller with RSV
; ---------------------------------------------------------------------
;
TWB_P    EQU  #00
TWB_L    EQU  #02
TWB_IND  EQU  #04
TWB_ZC   EQU  #06        ; [DUMP] ZC base, relative to G'
TWB_T0   EQU  #08
TWB_T1   EQU  #0A
TWB_T2   EQU  #0C
TWB_T3   EQU  #0E
TWB_T4   EQU  #10
TWB_T5   EQU  #12
TWB_T6   EQU  #14
TWB_T7   EQU  #16
TWB_N3   EQU  #18
TWB_N2   EQU  #1A
TWB_N1   EQU  #1C
TWB_N0   EQU  #1E

; ===== Start of MTR (as inferred from the dump)  =====
; routine profile: 3 instructions
Unknow_1:				; entry point; return-address cell (written by SPA, read by the return branch); holds a section / module number; pointer cell; read; written; address taken; byte access; 84 refs
000000:	0000	LDA 	DL 	Unknow_1	; A <- [Unknow_1]
V_0002:				; pointer cell; read; written; address taken; double-word; byte access; 40 refs
000002:	0000	LDA 	DL 	Unknow_1	; A <- [Unknow_1]
V_0004:				; pointer cell; read; written; address taken; double-word; 41 refs
000004:	0000	LDA 	DL 	Unknow_1	; A <- [Unknow_1]
V_0006:				; return-address cell (written by SPA, read by the return branch); pointer cell; read; written; double-word; byte access; 36 refs
000006:	0000	.WORD	#0000	; return-address cell (SPA stores P+4-G' here at call time; the value in the dump is just leftover)
V_0008:				; pointer cell; read; written; byte access; 47 refs
000008:	0000	.WORD	#0000	; data word (used as a variable)
; ============================================
CPT_PTR:				; system structure
; ============================================
; pointer cell; read; written; double-word; byte access; 75 refs
00000A:	0380	.WORD	#0380	; absolute &A: pointer to the Context Pointer Table (CPT)
; ============================================; system structure
; 
; ==============================================
PRTS_PTR:				; system structure
; ============================================
; return-address cell (written by SPA, read by the return branch); pointer cell; read; written; address taken; double-word; byte access; 39 refs
00000C:	3002	.WORD	#3002	; absolute &C: pointer to the supervisor relocation table PRTS (SRD N at this-4N)
; ============================================; system structure
; 
; ==============================================
PRTS_COUNT:				; system structure
; ============================================
; pointer cell; read; written; address taken; double-word; byte access; 50 refs
00000E:	0038	.WORD	#0038	; absolute &E: number of supervisor modules (CSV is checked against it, else mode-violation trap)
V_0010:				; pointer cell; read; written; address taken; double-word; byte access; 43 refs
000010:	0000	.WORD	#0000	; data word (used as a variable)
V_0012:				; pointer cell; read; written; address taken; double-word; byte access; 41 refs
000012:	225A	.WORD	#225A	; data word (used as a variable)
V_0014:				; return-address cell (written by SPA, read by the return branch); pointer cell; read; written; address taken; double-word; byte access; 49 refs
000014:	2264	.WORD	#2264	; return-address cell (SPA stores P+4-G' here at call time; the value in the dump is just leftover)
V_0016:				; pointer cell; read; written; double-word; 31 refs
000016:	2270	.WORD	#2270	; data word (used as a variable)
V_0018:				; return-address cell (written by SPA, read by the return branch); pointer cell; read; written; byte access; 42 refs
000018:	2294	.WORD	#2294	; data word (used as a variable)
V_001A:				; pointer cell; read; written; byte access; 21 refs
00001A:	22A6	.WORD	#22A6	; data word (used as a variable)
V_001C:				; pointer cell; read; written; double-word; 19 refs
00001C:	2214	.WORD	#2214	; data word (used as a variable)
V_001E:				; pointer cell; read; written; double-word; byte access; 19 refs
00001E:	2224	.WORD	#2224	; data word (used as a variable)
V_0020:				; pointer cell; read; written; double-word; byte access; 29 refs
000020:	0000	.WORD	#0000	; data word (used as a variable)
V_0022:				; pointer cell; read; written; double-word; 31 refs
000022:	3006	.WORD	#3006	; data word (used as a variable)
V_0024:				; pointer cell; read; written; 24 refs
000024:	7CD6	.WORD	#7CD6	; data word (used as a variable)
V_0026:				; pointer cell; read; written; byte access; 21 refs
000026:	7FFF	.WORD	#7FFF	; data word (used as a variable)
V_0028:				; pointer cell; read; written; double-word; byte access; 12 refs
000028:	1E7E	.WORD	#1E7E	; data word (used as a variable)
V_002A:				; VECTOR: holds the entry address of S_0AB8; read; 5 refs
00002A:	0AB8	.WORD	#0AB8	; data word (used as a variable)
V_002C:				; holds a section / module number; VECTOR: holds the entry address of S_0986; read; written; 14 refs
00002C:	0986	.WORD	#0986	; data word (used as a variable)
V_002E:				; VECTOR: holds the entry address of S_0968; read; written; byte access; 9 refs
00002E:	0968	.WORD	#0968	; data word (used as a variable)
V_0030:				; pointer cell; read; written; 43 refs
000030:	0000	.WORD	#0000	; data word (used as a variable)
V_0032:				; return-address cell (written by SPA, read by the return branch); pointer cell; read; written; address taken; 17 refs
000032:	0001	.WORD	#0001	; return-address cell (SPA stores P+4-G' here at call time; the value in the dump is just leftover)
V_0034:				; return-address cell (written by SPA, read by the return branch); pointer cell; read; written; byte access; 20 refs
000034:	0002	.WORD	#0002	; return-address cell (SPA stores P+4-G' here at call time; the value in the dump is just leftover)
V_0036:				; pointer cell; read; written; address taken; double-word; byte access; 18 refs
000036:	0003	.WORD	#0003	; data word (used as a variable)
V_0038:				; pointer cell; read; written; 10 refs
000038:	0004	.WORD	#0004	; data word (used as a variable)
V_003A:				; pointer cell; read; written; double-word; 10 refs
00003A:	0006	.WORD	#0006	; data word (used as a variable)
V_003C:				; read; written; address taken; double-word; 8 refs
00003C:	7FFF	.WORD	#7FFF	; data word (used as a variable)
V_003E:				; read; written; byte access; 4 refs
00003E:	0000	.WORD	#0000	; data word (used as a variable)
v_0040:				; pointer cell; read; written; 6 refs; only from untraced words
000040:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
v_0042:				; read; address taken; byte access; 6 refs; only from untraced words
000042:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
V_0044:				; read; written; 9 refs
000044:	0000	.WORD	#0000	; data word (used as a variable)
V_0046:				; pointer cell; read; written; double-word; 10 refs
000046:	0000	.WORD	#0000	; data word (used as a variable)
V_0048:				; read; written; byte access; 6 refs
000048:	0000	.WORD	#0000	; data word (used as a variable)
V_004A:				; read; written; byte access; 4 refs
00004A:	0000	.WORD	#0000	; data word (used as a variable)
V_004C:				; pointer cell; read; written; byte access; 4 refs
00004C:	0000	.WORD	#0000	; data word (used as a variable)
V_004E:				; return-address cell (written by SPA, read by the return branch); pointer cell; read; written; 8 refs
00004E:	0000	.WORD	#0000	; data word (used as a variable)
V_0050:				; VECTOR: holds the entry address of S_051A; read; address taken; 17 refs
000050:	051A	.WORD	#051A	; data word (used as a variable)
V_0052:				; VECTOR: holds the entry address of S_0844; read; written; address taken; 16 refs
000052:	0844	.WORD	#0844	; data word (used as a variable)
V_0054:				; VECTOR: holds the entry address of S_0896; read; 4 refs
000054:	0896	.WORD	#0896	; data word (used as a variable)
V_0056:				; VECTOR: holds the entry address of S_08D8; read; 5 refs
000056:	08D8	.WORD	#08D8	; data word (used as a variable)
v_0058:				; VECTOR: holds the entry address of S_08F6; read; byte access; 8 refs; only from untraced words
000058:	08F6	DIV 	DL 	V_00F6	; divide by [V_00F6]
V_005A:				; VECTOR: holds the entry address of S_1642; read; double-word; byte access; 18 refs
00005A:	1642	.WORD	#1642	; data word (used as a variable)
V_005C:				; VECTOR: holds the entry address of S_146A; read; 5 refs
00005C:	146A	.WORD	#146A	; data word (used as a variable)
V_005E:				; VECTOR: holds the entry address of S_14F0; read; address taken; 9 refs
00005E:	14F0	.WORD	#14F0	; data word (used as a variable)
v_0060:				; pointer cell; read; written; 10 refs; only from untraced words
000060:	03C2	EOR 	DL 	v_00C2	; A <- A xor [v_00C2]
l_0062:				; branch target; pointer cell; read; 8 refs; only from untraced words
000062:	0222	LDX 	DL 	V_0022	; X <- [V_0022]
l_0064:				; branch target; written; 2 refs; only from untraced words
000064:	22C8	LDX 	P  	=#C8	; X <- #C8
L_0066:				; branch target; read; written; double-word; 11 refs
000066:	7FFC	.WORD	#7FFC	; data word (used as a variable)
v_0068:				; address taken; 1 ref; only from untraced words
000068:	22C8	LDX 	P  	=#C8	; X <- #C8
V_006A:				; read; written; 5 refs
00006A:	0108	.WORD	#0108	; data word (used as a variable)
v_006C:				; pointer cell; read; written; byte access; 8 refs; only from untraced words
00006C:	03C6	EOR 	DL 	V_00C6	; A <- A xor [V_00C6]
v_006E:				; pointer cell; read; written; 11 refs; only from untraced words
00006E:	33B4	DCX 	DL 	V_00B4	; DCX operand: [V_00B4]
v_0070:				; pointer cell; read; written; byte access; 5 refs; only from untraced words
000070:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
v_0072:				; read; written; address taken; double-word; 6 refs; only from untraced words
000072:	0778	IOR 	DL 	v_0078	; A <- A or [v_0078]
v_0074:				; read; 1 ref; only from untraced words
000074:	077E	IOR 	DL 	V_007E	; A <- A or [V_007E]
v_0076:				; pointer cell; read; address taken; double-word; 5 refs; only from untraced words
000076:	0314	EOR 	DL 	V_0014	; A <- A xor [V_0014]
v_0078:				; read; double-word; 6 refs; only from untraced words
000078:	0314	EOR 	DL 	V_0014	; A <- A xor [V_0014]
V_007A:				; pointer cell; read; address taken; double-word; 9 refs
00007A:	22C8	.WORD	#22C8	; data word (used as a variable)
V_007C:				; pointer cell; read; address taken; 4 refs
00007C:	1B1E	.WORD	#1B1E	; data word (used as a variable)
V_007E:				; read; written; 5 refs
00007E:	0000	.WORD	#0000	; data word (used as a variable)
V_0080:				; pointer cell; read; written; byte access; 10 refs
000080:	226E	.WORD	#226E	; data word (used as a variable)
V_0082:				; read; written; address taken; byte access; 6 refs
000082:	226E	.WORD	#226E	; data word (used as a variable)
v_0084:				; read; written; byte access; 4 refs; only from untraced words
000084:	226E	LDX 	P  	=#6E	; X <- #6E
v_0086:				; read; address taken; 3 refs; only from untraced words
000086:	0000	.ZERO	2	; 2 zero words (padding, reserved area or unused data; never reached as code)
v_008A:				; read; byte access; 1 ref; only from untraced words
00008A:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
v_008C:				; read; 2 refs; only from untraced words
00008C:	0144	LDE 	DL 	V_0044	; E <- [V_0044]
v_008E:				; read; 2 refs; only from untraced words
00008E:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
v_0090:				; pointer cell; read; address taken; byte access; 4 refs; only from untraced words
000090:	018C	LDE 	DL 	v_008C	; E <- [v_008C]
V_0092:				; pointer cell; read; written; 3 refs
000092:	019C	.WORD	#019C	; data word (used as a variable)
V_0094:				; pointer cell; read; byte access; 6 refs
000094:	00DC	.WORD	#00DC	; data word (used as a variable)
V_0096:				; pointer cell; read; 7 refs
000096:	00E4	.WORD	#00E4	; data word (used as a variable)
V_0098:				; pointer cell; read; 10 refs
000098:	2232	.WORD	#2232	; data word (used as a variable)
V_009A:				; pointer cell; read; 6 refs
00009A:	2246	.WORD	#2246	; data word (used as a variable)
V_009C:				; pointer cell; read; written; 33 refs
00009C:	0234	.WORD	#0234	; data word (used as a variable)
V_009E:				; pointer cell; read; 3 refs
00009E:	016A	.WORD	#016A	; data word (used as a variable)
V_00A0:				; pointer cell; read; 22 refs
0000A0:	153C	.WORD	#153C	; data word (used as a variable)
v_00A2:				; pointer cell; read; address taken; 7 refs; only from untraced words
0000A2:	13EA	STX 	DL 	v_00EA	; [v_00EA] <- X
v_00A4:				; pointer cell; read; double-word; 4 refs; only from untraced words
0000A4:	1418	SBL 	DL 	V_0018	; left byte of [V_0018] <- A
v_00A6:				; pointer cell; read; 1 ref; only from untraced words
0000A6:	1434	SBL 	DL 	V_0034	; left byte of [V_0034] <- A
0000A8:	1664	DST 	DL 	l_0064	; [l_0064] <- A,E (2 words)
V_00AA:				; VECTOR: holds the entry address of S_17CC; read; byte access; 4 refs
0000AA:	17CC	.WORD	#17CC	; data word (used as a variable)
V_00AC:				; VECTOR: holds the entry address of S_1454; read; byte access; 3 refs
0000AC:	1454	.WORD	#1454	; data word (used as a variable)
V_00AE:				; VECTOR: holds the entry address of S_1B86; read; 2 refs
0000AE:	1B86	.WORD	#1B86	; data word (used as a variable)
V_00B0:				; pointer cell; read; address taken; 2 refs
0000B0:	04E2	.WORD	#04E2	; data word (used as a variable)
V_00B2:				; pointer cell; read; 4 refs
0000B2:	050A	.WORD	#050A	; data word (used as a variable)
V_00B4:				; pointer cell; read; 5 refs
0000B4:	04E8	.WORD	#04E8	; data word (used as a variable)
L_00B6:				; branch target; pointer cell; read; 2 refs
0000B6:	051C	.WORD	#051C	; data word (used as a variable)
v_00B8:				; pointer cell; read; 3 refs; only from untraced words
0000B8:	1A76	FAD 	DL 	v_0076	; floating add [v_0076]
V_00BA:				; pointer cell; read; 1 ref
0000BA:	0422	.WORD	#0422	; data word (used as a variable)
v_00BC:				; pointer cell; read; byte access; 5 refs; only from untraced words
0000BC:	03E4	EOR 	DL 	v_00E4	; A <- A xor [v_00E4]
v_00BE:				; pointer cell; read; 3 refs; only from untraced words
0000BE:	0402	LEA 	DL 	V_0002	; A <- V_0002  (effective address, memory not read)
v_00C0:				; pointer cell; read; double-word; 8 refs; only from untraced words
0000C0:	0D90	LBL 	DL 	v_0090	; A <- left byte of [v_0090]
v_00C2:				; holds a section / module number; pointer cell; read; byte access; 11 refs; only from untraced words
0000C2:	0D94	LBL 	DL 	V_0094	; A <- left byte of [V_0094]
v_00C4:				; pointer cell; read; 5 refs; only from untraced words
0000C4:	0BB8	CMP 	DL 	v_00B8	; compare A with [v_00B8]
V_00C6:				; pointer cell; read; address taken; 3 refs
0000C6:	0176	.WORD	#0176	; data word (used as a variable)
V_00C8:				; pointer cell; read; 2 refs
0000C8:	0008	.WORD	#0008	; data word (used as a variable)
v_00CA:				; read; written; 3 refs; only from untraced words
0000CA:	FFFA	??	FFFA
V_00CC:				; read; address taken; double-word; 3 refs
0000CC:	6667	.WORD	#6667	; data word (used as a variable)
V_00CE:				; VECTOR: holds the entry address of S_1708; read; 1 ref
0000CE:	1708	.WORD	#1708	; data word (used as a variable)
v_00D0:				; VECTOR: holds the entry address of S_19E8; read; written; byte access; 3 refs; only from untraced words
0000D0:	19E8	STS 	DL 	V_00E8	; STS operand: [V_00E8]
V_00D2:				; holds a section / module number; pointer cell; read; 5 refs
0000D2:	0326	.WORD	#0326	; data word (used as a variable)
V_00D4:				; read; written; byte access; 36 refs
0000D4:	0000	.WORD	#0000	; data word (used as a variable)
V_00D6:				; pointer cell; read; written; 8 refs
0000D6:	0000	.WORD	#0000	; data word (used as a variable)
V_00D8:				; pointer cell; read; 2 refs
0000D8:	030E	.WORD	#030E	; data word (used as a variable)
V_00DA:				; pointer cell; read; 3 refs
0000DA:	0312	.WORD	#0312	; data word (used as a variable)
V_00DC:				; read; written; address taken; byte access; 5 refs
0000DC:	0600	.WORD	#0600	; data word (used as a variable)
V_00DE:				; read; written; byte access; 6 refs
0000DE:	0000	.WORD	#0000	; data word (used as a variable)
v_00E0:				; pointer cell; read; 1 ref; only from untraced words
0000E0:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
v_00E2:				; read; 2 refs; only from untraced words
0000E2:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
v_00E4:				; read; 1 ref; only from untraced words
0000E4:	0011	LDA 	DL 	V_0010	; A <- [V_0010]
V_00E6:				; read; 6 refs
0000E6:	02DE	.WORD	#02DE	; data word (used as a variable)
V_00E8:				; read; written; byte access; 11 refs
0000E8:	02CE	.WORD	#02CE	; data word (used as a variable)
v_00EA:				; written; address taken; 2 refs; only from untraced words
0000EA:	000A	LDA 	DL 	CPT_PTR	; A <- [CPT_PTR]
v_00EC:				; pointer cell; read; byte access; 4 refs; only from untraced words
0000EC:	23C0	EOR 	P  	=#C0	; A <- A xor #C0
V_00EE:				; pointer cell; read; 9 refs
0000EE:	02FE	.WORD	#02FE	; data word (used as a variable)
v_00F0:				; pointer cell; read; 1 ref; only from untraced words
0000F0:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
v_00F2:				; operand of system instruction; 2 refs; only from untraced words
0000F2:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
v_00F4:				; read; written; byte access; 4 refs; only from untraced words
0000F4:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
V_00F6:				; read; 3 refs
0000F6:	02EE	.WORD	#02EE	; data word (used as a variable)
0000F8:	0222	LDX 	DL 	V_0022	; X <- [V_0022]
v_00FA:				; read; 2 refs; only from untraced words
0000FA:	22CA	LDX 	P  	=#CA	; X <- #CA
v_00FC:				; read; byte access; 1 ref; only from untraced words
0000FC:	22CA	LDX 	P  	=#CA	; X <- #CA
v_00FE:				; pointer cell; read; written; byte access; 13 refs; only from untraced words
0000FE:	22CA	LDX 	P  	=#CA	; X <- #CA
v_0100:				; read; double-word; 1 ref; only from untraced words
000100:	22CA	LDX 	P  	=#CA	; X <- #CA
000102:	22CA	LDX 	P  	=#CA	; X <- #CA
000104:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
000106:	0314	EOR 	DL 	V_0014	; A <- A xor [V_0014]
000108:	FEF8	??	FEF8
00010A:	020C	LDX 	DL 	PRTS_PTR	; X <- [PRTS_PTR]
00010C:	020C	LDX 	DL 	PRTS_PTR	; X <- [PRTS_PTR]
00010E:	0001	LDA 	DL 	Unknow_1	; A <- [Unknow_1]
000110:	0000	.ZERO	4	; 4 zero words (padding, reserved area or unused data; never reached as code)
000118:	FFFF	??	FFFF
00011A:	7FFF	MVS 	IL 	*v_00FE	; MVS operand: [G+22CA]  (pointer v_00FE = #22CA: address = G + #22CA, G unknown)
00011C:	6000	LDA 	IL 	*Unknow_1	; A <- [G+0000]  (pointer Unknow_1 = #0000: address = G + #0000, G unknown)
00011E:	8000	LDA 	IGX	*TWB_P,X	; A <- [*TWB_P+X]  (pointer TWB_P lives in the calling task's area: its contents are only known at run time)
000120:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
000122:	0001	LDA 	DL 	Unknow_1	; A <- [Unknow_1]
000124:	0002	LDA 	DL 	V_0002	; A <- [V_0002]
000126:	0003	LDA 	DL 	V_0002	; A <- [V_0002]
000128:	0004	LDA 	DL 	V_0004	; A <- [V_0004]
00012A:	0005	LDA 	DL 	V_0004	; A <- [V_0004]
00012C:	0006	LDA 	DL 	V_0006	; A <- [V_0006]
00012E:	4000	LDA 	DG 	TWB_P	; A <- [TWB_P]
000130:	0380	EOR 	DL 	V_0080	; A <- A xor [V_0080]
000132:	013C	LDE 	DL 	V_003C	; E <- [V_003C]
000134:	0140	LDE 	DL 	v_0040	; E <- [v_0040]
000136:	0144	LDE 	DL 	V_0044	; E <- [V_0044]
000138:	014A	LDE 	DL 	V_004A	; E <- [V_004A]
00013A:	1BFE	FSU 	DL 	v_00FE	; floating subtract [v_00FE]
00013C:	A5C5	ADD 	ILX	*v_00C4,X	; A <- A + [G+0BB8+X]  (pointer v_00C4 = #0BB8: address = G + #0BB8, G unknown)
00013E:	CF44	BRU 	RM 	L_00B6	; 68 words backward: jump to L_00B6
000140:	.EBCDIC	"%EOD"	; 2 words, raw: 6CC5 D6C4
000144:	6101	LDE 	IL 	*Unknow_1	; E <- [G+0000]  (pointer Unknow_1 = #0000: address = G + #0000, G unknown)
000146:	6081	LDA 	IL 	*V_0080	; A <- [G+226E]  (pointer V_0080 = #226E: address = G + #226E, G unknown)
000148:	6041	LDA 	IL 	*v_0040	; A <- [G+0000]  (pointer v_0040 = #0000: address = G + #0000, G unknown)
00014A:	0000	.ZERO	2	; 2 zero words (padding, reserved area or unused data; never reached as code)
00014E:	000A	LDA 	DL 	CPT_PTR	; A <- [CPT_PTR]
000150:	0000	.ZERO	13	; 13 zero words (padding, reserved area or unused data; never reached as code)
00016A:	0080	LDA 	DL 	V_0080	; A <- [V_0080]
00016C:	8004	LDA 	IGX	*TWB_IND,X	; A <- [*TWB_IND+X]  (pointer TWB_IND lives in the calling task's area: its contents are only known at run time)
00016E:	0028	LDA 	DL 	V_0028	; A <- [V_0028]
000170:	0006	LDA 	DL 	V_0006	; A <- [V_0006]
000172:	.EBCDIC	"%%DVDRIOLDACECERRTABA0A1MX0123456789ABCDEF"	; 21 words, raw: 6C6C C4E5 C4D9 C9D6 D3C4 C1C3 C5C3 C5D9 D9E3 C1C2 C1F0 C1F1 D4E7 F0F1 F2F3 F4F5 F6F7 F8F9 C1C2 C3C4 C5C6
00019C:	0001	LDA 	DL 	Unknow_1	; A <- [Unknow_1]
00019E:	0203	LDX 	DL 	V_0002	; X <- [V_0002]
0001A0:	372D	CSV 	DL 	V_002C	; SUPERVISOR CALL, module number taken from [V_002C] (CSV); module ends with RSV
0001A2:	2E2F	LBR 	P  	=#2F	; A <- right byte of #2F
0001A4:	1605	DST 	DL 	V_0004	; [V_0004] <- A,E (2 words)
0001A6:	150B	SBR 	DL 	CPT_PTR	; right byte of [CPT_PTR] <- A
0001A8:	0C0D	MUL 	DL 	PRTS_PTR	; multiply by [PRTS_PTR]
0001AA:	0E0F	LBR 	DL 	PRTS_COUNT	; A <- right byte of [PRTS_COUNT]
0001AC:	1011	DLD 	DL 	V_0010	; A,E <- [V_0010] (2 words)
0001AE:	1213	STE 	DL 	V_0012	; [V_0012] <- E
0001B0:	3C3D	DITR	DL 		; DE-ACTIVATE high-speed interrupt: return to interrupted task without block-0 context swap
0001B2:	3226	ICX 	DL 	V_0026	; ICX operand: [V_0026]
0001B4:	1819	SPA 	DL 	V_0018	; [V_0018] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0001B6:	3F27	??	3F27
0001B8:	1C1D	FMU 	DL 	V_001C	; floating multiply by [V_001C]
0001BA:	1E1F	TRS 	DL 	V_001E	; TRS operand: [V_001E]
0001BC:	404F	LDA 	DG 	G+004F	; A <- [G+004F]
0001BE:	7F7B	MVS 	IL 	*V_007A	; MVS operand: [G+22C8]  (pointer V_007A = #22C8: address = G + #22C8, G unknown)
0001C0:	5B6C	FSU 	DG 	G+006C	; floating subtract [G+006C]
0001C2:	.EBCDIC	"&'()*+,-./0123456789:;"	; 11 words, raw: 507D 4D5D 5C4E 6B60 4B61 F0F1 F2F3 F4F5 F6F7 F8F9 7A5E
0001D8:	4C7E	MUL 	DG 	G+007E	; multiply by [G+007E]
0001DA:	6E6F	LBR 	IL 	*v_006E	; A <- right byte of [G+33B4]  (pointer v_006E = #33B4: address = G + #33B4, G unknown)
0001DC:	.EBCDIC	"@ABCDEFGHIJKLMNOPQRSTUVWXY"	; 13 words, raw: 7CC1 C2C3 C4C5 C6C7 C8C9 D1D2 D3D4 D5D6 D7D8 D9E2 E3E4 E5E6 E7E8
0001F6:	E94A	LDR 	PX 	(X)	; LDR operand: [(X)]
0001F8:	E05A	SAD 	PX 	#1A
0001FA:	5F6D	MVS 	DG 	G+006D	; MVS operand: [G+006D]
0001FC:	CACB	BOT 	RM 	L_0066	; 203 words backward: conditional branch (BOT) to L_0066
0001FE:	CCCD	BAN 	RM 	l_0064	; 205 words backward: conditional branch (BAN) to l_0064
000200:	CECF	BOF 	RM 	l_0062	; 207 words backward: conditional branch (BOF) to l_0062
000202:	.EBCDIC	"fghijklmnopqrstuvwxy"	; 10 words, raw: 8687 8889 9192 9394 9596 9798 99A2 A3A4 A5A6 A7A8
000216:	A9C0	AND 	ILX	*v_00C0,X	; A <- A and [G+0D90+X]  (pointer v_00C0 = #0D90: address = G + #0D90, G unknown)
000218:	6AD0	CPS 	IL 	*v_00D0	; CPS operand: [G+19E8]  (pointer v_00D0 = #19E8: address = G + #19E8, G unknown)
00021A:	A107	LDE 	ILX	*V_0006,X	; E <- [*V_0006+X]
00021C:	0A0A	CPS 	DL 	CPT_PTR	; CPS operand: [CPT_PTR]
00021E:	0402	LEA 	DL 	V_0002	; A <- V_0002  (effective address, memory not read)
000220:	0806	DIV 	DL 	V_0006	; divide by [V_0006]
000222:	4B00	CMP 	DG 	TWB_P	; compare A with [TWB_P]
000224:	2000	LDA 	P  	=#00	; A <- #00
000226:	012B	LDE 	DL 	V_002A	; E <- [V_002A]
000228:	0513	ADD 	DL 	V_0012	; A <- A + [V_0012]
00022A:	08FB	DIV 	DL 	v_00FA	; divide by [v_00FA]
00022C:	0CE3	MUL 	DL 	v_00E2	; multiply by [v_00E2]
00022E:	10CB	DLD 	DL 	v_00CA	; A,E <- [v_00CA] (2 words)
000230:	4AFF	CPS 	DG 	G+00FF	; CPS operand: [G+00FF]
000232:	0014	LDA 	DL 	V_0014	; A <- [V_0014]
000234:	0010	LDA 	DL 	V_0010	; A <- [V_0010]
000236:	000E	LDA 	DL 	PRTS_COUNT	; A <- [PRTS_COUNT]
000238:	0004	LDA 	DL 	V_0004	; A <- [V_0004]
00023A:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
00023C:	0008	LDA 	DL 	V_0008	; A <- [V_0008]
00023E:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
000240:	0002	LDA 	DL 	V_0002	; A <- [V_0002]
000242:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
000244:	000A	LDA 	DL 	CPT_PTR	; A <- [CPT_PTR]
000246:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
000248:	000C	LDA 	DL 	PRTS_PTR	; A <- [PRTS_PTR]
00024A:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
00024C:	000E	LDA 	DL 	PRTS_COUNT	; A <- [PRTS_COUNT]
00024E:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
000250:	0010	LDA 	DL 	V_0010	; A <- [V_0010]
000252:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
000254:	0012	LDA 	DL 	V_0012	; A <- [V_0012]
000256:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
000258:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
00025A:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
00025C:	0016	LDA 	DL 	V_0016	; A <- [V_0016]
00025E:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
000260:	0018	LDA 	DL 	V_0018	; A <- [V_0018]
000262:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
000264:	001A	LDA 	DL 	V_001A	; A <- [V_001A]
000266:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
000268:	001C	LDA 	DL 	V_001C	; A <- [V_001C]
00026A:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
00026C:	001E	LDA 	DL 	V_001E	; A <- [V_001E]
00026E:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
000270:	0020	LDA 	DL 	V_0020	; A <- [V_0020]
000272:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
000274:	0022	LDA 	DL 	V_0022	; A <- [V_0022]
000276:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
000278:	0024	LDA 	DL 	V_0024	; A <- [V_0024]
00027A:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
00027C:	0026	LDA 	DL 	V_0026	; A <- [V_0026]
00027E:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
000280:	0028	LDA 	DL 	V_0028	; A <- [V_0028]
000282:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
000284:	002A	LDA 	DL 	V_002A	; A <- [V_002A]
000286:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
000288:	0006	LDA 	DL 	V_0006	; A <- [V_0006]
00028A:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
00028C:	156C	SBR 	DL 	v_006C	; right byte of [v_006C] <- A
00028E:	0000	.ZERO	9	; 9 zero words (padding, reserved area or unused data; never reached as code)
0002C0:	0000	.ZERO	7	; 7 zero words (padding, reserved area or unused data; never reached as code)
; ============================================; system structure
; 
; ==============================================
CTX1:				; system structure
; ============================================

0002CE:	0014	.WORD	#0014	; CTX level 1: saved status
0002D0:	0000	.WORD	#0000	; CTX level 1: saved X
0002D2:	0000	.WORD	#0000	; CTX level 1: saved E
0002D4:	0000	.WORD	#0000	; CTX level 1: saved A
0002D6:	028E	.WORD	#028E	; CTX level 1: saved G
0002D8:	0000	.WORD	#0000	; CTX level 1: saved L
0002DA:	0510	.WORD	#0510	; CTX level 1: saved P (entry of the task / interrupt routine of this level)
0002DC:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
0002DE:	0015	LDA 	DL 	V_0014	; A <- [V_0014]
0002E0:	0000	.ZERO	3	; 3 zero words (padding, reserved area or unused data; never reached as code)
0002E6:	028E	LDX 	DL 	v_008E	; X <- [v_008E]
0002E8:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
0002EA:	050E	ADD 	DL 	PRTS_COUNT	; A <- A + [PRTS_COUNT]
0002EC:	0000	.ZERO	17	; 17 zero words (padding, reserved area or unused data; never reached as code)
00030E:	02DE	LDX 	DL 	V_00DE	; X <- [V_00DE]
000310:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
000312:	FF00	??	FF00
000314:	FCEC	??	FCEC
000316:	FDF4	TES 	P  	=#F4	; TES operand: #F4
000318:	0000	.ZERO	12	; 12 zero words (padding, reserved area or unused data; never reached as code)
; ============================================
; DVT:				; system structure
; ============================================

000340:	E004	.WORD	#E004	; DVT/OVT entry 0: interrupt configuration word (trigger/enable/arm, group, level)
000342:	E004	.WORD	#E004	; DVT/OVT entry 1: interrupt configuration word (trigger/enable/arm, group, level)
000344:	E004	.WORD	#E004	; DVT/OVT entry 2: interrupt configuration word (trigger/enable/arm, group, level)
000346:	E004	.WORD	#E004	; DVT/OVT entry 3: interrupt configuration word (trigger/enable/arm, group, level)
000348:	E004	.WORD	#E004	; DVT/OVT entry 4: interrupt configuration word (trigger/enable/arm, group, level)
00034A:	E004	.WORD	#E004	; DVT/OVT entry 5: interrupt configuration word (trigger/enable/arm, group, level)
00034C:	6005	.WORD	#6005	; DVT/OVT entry 6: interrupt configuration word (trigger/enable/arm, group, level)
00034E:	E004	.WORD	#E004	; DVT/OVT entry 7: interrupt configuration word (trigger/enable/arm, group, level)
000350:	E004	.WORD	#E004	; DVT/OVT entry 8: interrupt configuration word (trigger/enable/arm, group, level)
000352:	E004	.WORD	#E004	; DVT/OVT entry 9: interrupt configuration word (trigger/enable/arm, group, level)
000354:	E004	.WORD	#E004	; DVT/OVT entry 10: interrupt configuration word (trigger/enable/arm, group, level)
000356:	E004	.WORD	#E004	; DVT/OVT entry 11: interrupt configuration word (trigger/enable/arm, group, level)
000358:	E004	.WORD	#E004	; DVT/OVT entry 12: interrupt configuration word (trigger/enable/arm, group, level)
00035A:	E004	.WORD	#E004	; DVT/OVT entry 13: interrupt configuration word (trigger/enable/arm, group, level)
00035C:	E004	.WORD	#E004	; DVT/OVT entry 14: interrupt configuration word (trigger/enable/arm, group, level)
00035E:	E004	.WORD	#E004	; DVT/OVT entry 15: interrupt configuration word (trigger/enable/arm, group, level)
000360:	E004	.WORD	#E004	; DVT/OVT entry 16: interrupt configuration word (trigger/enable/arm, group, level)
000362:	E004	.WORD	#E004	; DVT/OVT entry 17: interrupt configuration word (trigger/enable/arm, group, level)
000364:	6040	.WORD	#6040	; DVT/OVT entry 18: interrupt configuration word (trigger/enable/arm, group, level)
000366:	6080	.WORD	#6080	; DVT/OVT entry 19: interrupt configuration word (trigger/enable/arm, group, level)
000368:	E004	.WORD	#E004	; DVT/OVT entry 20: interrupt configuration word (trigger/enable/arm, group, level)
00036A:	E004	.WORD	#E004	; DVT/OVT entry 21: interrupt configuration word (trigger/enable/arm, group, level)
00036C:	E004	.WORD	#E004	; DVT/OVT entry 22: interrupt configuration word (trigger/enable/arm, group, level)
00036E:	E004	.WORD	#E004	; DVT/OVT entry 23: interrupt configuration word (trigger/enable/arm, group, level)
000370:	E004	.WORD	#E004	; DVT/OVT entry 24: interrupt configuration word (trigger/enable/arm, group, level)
000372:	6200	.WORD	#6200	; DVT/OVT entry 25: interrupt configuration word (trigger/enable/arm, group, level)
000374:	6008	.WORD	#6008	; DVT/OVT entry 26: interrupt configuration word (trigger/enable/arm, group, level)
000376:	6004	.WORD	#6004	; DVT/OVT entry 27: interrupt configuration word (trigger/enable/arm, group, level)
000378:	E004	.WORD	#E004	; DVT/OVT entry 28: interrupt configuration word (trigger/enable/arm, group, level)
00037A:	E004	.WORD	#E004	; DVT/OVT entry 29: interrupt configuration word (trigger/enable/arm, group, level)
00037C:	E004	.WORD	#E004	; DVT/OVT entry 30: interrupt configuration word (trigger/enable/arm, group, level)
00037E:	E004	.WORD	#E004	; DVT/OVT entry 31: interrupt configuration word (trigger/enable/arm, group, level)
; ============================================
; CPT:	; system structure
; ============================================
000380:	38C2	.WORD	#38C2	; CPT[0]: context pointer of interrupt level 0
000382:	02CE	.WORD	#02CE	; CPT[1]: pointer to the context (CTX) of interrupt level 1
000384:	02CE	.WORD	#02CE	; CPT[2]: pointer to the context (CTX) of interrupt level 2
000386:	02CE	.WORD	#02CE	; CPT[3]: pointer to the context (CTX) of interrupt level 3
000388:	02CE	.WORD	#02CE	; CPT[4]: pointer to the context (CTX) of interrupt level 4
00038A:	2628	.WORD	#2628	; CPT[5]: pointer to the context (CTX) of interrupt level 5
00038C:	29C2	.WORD	#29C2	; CPT[6]: pointer to the context (CTX) of interrupt level 6
00038E:	337C	.WORD	#337C	; CPT[7]: pointer to the context (CTX) of interrupt level 7
000390:	02CE	.WORD	#02CE	; CPT[8]: pointer to the context (CTX) of interrupt level 8
000392:	02CE	.WORD	#02CE	; CPT[9]: pointer to the context (CTX) of interrupt level 9
000394:	02CE	.WORD	#02CE	; CPT[10]: pointer to the context (CTX) of interrupt level 10
000396:	02CE	.WORD	#02CE	; CPT[11]: pointer to the context (CTX) of interrupt level 11
000398:	02CE	.WORD	#02CE	; CPT[12]: pointer to the context (CTX) of interrupt level 12
00039A:	2C6E	.WORD	#2C6E	; CPT[13]: pointer to the context (CTX) of interrupt level 13
00039C:	2B68	.WORD	#2B68	; CPT[14]: pointer to the context (CTX) of interrupt level 14
00039E:	02CE	.WORD	#02CE	; CPT[15]: pointer to the context (CTX) of interrupt level 15
0003A0:	02CE	.WORD	#02CE	; CPT[16]: pointer to the context (CTX) of interrupt level 16
0003A2:	02CE	.WORD	#02CE	; CPT[17]: pointer to the context (CTX) of interrupt level 17
0003A4:	02CE	.WORD	#02CE	; CPT[18]: pointer to the context (CTX) of interrupt level 18
0003A6:	02CE	.WORD	#02CE	; CPT[19]: pointer to the context (CTX) of interrupt level 19
0003A8:	02CE	.WORD	#02CE	; CPT[20]: pointer to the context (CTX) of interrupt level 20
0003AA:	02CE	.WORD	#02CE	; CPT[21]: pointer to the context (CTX) of interrupt level 21
0003AC:	02CE	.WORD	#02CE	; CPT[22]: pointer to the context (CTX) of interrupt level 22
0003AE:	02CE	.WORD	#02CE	; CPT[23]: pointer to the context (CTX) of interrupt level 23
0003B0:	02CE	.WORD	#02CE	; CPT[24]: pointer to the context (CTX) of interrupt level 24
0003B2:	02CE	.WORD	#02CE	; CPT[25]: pointer to the context (CTX) of interrupt level 25
0003B4:	2F16	.WORD	#2F16	; CPT[26]: pointer to the context (CTX) of interrupt level 26
0003B6:	02CE	.WORD	#02CE	; CPT[27]: pointer to the context (CTX) of interrupt level 27
0003B8:	02CE	.WORD	#02CE	; CPT[28]: pointer to the context (CTX) of interrupt level 28
0003BA:	02CE	.WORD	#02CE	; CPT[29]: pointer to the context (CTX) of interrupt level 29
0003BC:	26B0	.WORD	#26B0	; CPT[30]: pointer to the context (CTX) of interrupt level 30
0003BE:	2670	.WORD	#2670	; CPT[31]: pointer to the context (CTX) of interrupt level 31
0003C0:	1E6E	TRS 	DL 	v_006E	; TRS operand: [v_006E]
0003C2:	226F	LDX 	P  	=#6F	; X <- #6F
0003C4:	04B1	LEA 	DL 	V_00B0	; A <- V_00B0  (effective address, memory not read)
0003C6:	0442	LEA 	DL 	v_0042	; A <- v_0042  (effective address, memory not read)
0003C8:	045F	LEA 	DL 	V_005E	; A <- V_005E  (effective address, memory not read)
0003CA:	0469	LEA 	DL 	v_0068	; A <- v_0068  (effective address, memory not read)
0003CC:	0472	LEA 	DL 	v_0072	; A <- v_0072  (effective address, memory not read)
0003CE:	0472	LEA 	DL 	v_0072	; A <- v_0072  (effective address, memory not read)
0003D0:	0472	LEA 	DL 	v_0072	; A <- v_0072  (effective address, memory not read)
0003D2:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
0003D4:	0477	LEA 	DL 	v_0076	; A <- v_0076  (effective address, memory not read)
0003D6:	047A	LEA 	DL 	V_007A	; A <- V_007A  (effective address, memory not read)
0003D8:	0472	LEA 	DL 	v_0072	; A <- v_0072  (effective address, memory not read)
0003DA:	0486	LEA 	DL 	v_0086	; A <- v_0086  (effective address, memory not read)
0003DC:	04A2	LEA 	DL 	v_00A2	; A <- v_00A2  (effective address, memory not read)
0003DE:	04C6	LEA 	DL 	V_00C6	; A <- V_00C6  (effective address, memory not read)
0003E0:	04CD	LEA 	DL 	V_00CC	; A <- V_00CC  (effective address, memory not read)
0003E2:	047D	LEA 	DL 	V_007C	; A <- V_007C  (effective address, memory not read)
0003E4:	0491	LEA 	DL 	v_0090	; A <- v_0090  (effective address, memory not read)
0003E6:	0BC2	CMP 	DL 	v_00C2	; compare A with [v_00C2]
0003E8:	0C04	MUL 	DL 	V_0004	; multiply by [V_0004]
0003EA:	0C46	MUL 	DL 	V_0046	; multiply by [V_0046]
0003EC:	0E58	LBR 	DL 	v_0058	; A <- right byte of [v_0058]
0003EE:	0E58	LBR 	DL 	v_0058	; A <- right byte of [v_0058]
0003F0:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
0003F2:	0C46	MUL 	DL 	V_0046	; multiply by [V_0046]
0003F4:	0C54	MUL 	DL 	V_0054	; multiply by [V_0054]
0003F6:	0E58	LBR 	DL 	v_0058	; A <- right byte of [v_0058]
0003F8:	0C60	MUL 	DL 	v_0060	; multiply by [v_0060]
0003FA:	0E18	LBR 	DL 	V_0018	; A <- right byte of [V_0018]
0003FC:	0E0A	LBR 	DL 	CPT_PTR	; A <- right byte of [CPT_PTR]
0003FE:	0E10	LBR 	DL 	V_0010	; A <- right byte of [V_0010]
000400:	0E70	LBR 	DL 	v_0070	; A <- right byte of [v_0070]
000402:	0DAC	LBL 	DL 	V_00AC	; A <- left byte of [V_00AC]
000404:	0EBC	LBR 	DL 	v_00BC	; A <- right byte of [v_00BC]
000406:	0EEC	LBR 	DL 	v_00EC	; A <- right byte of [v_00EC]
000408:	0EFC	LBR 	DL 	v_00FC	; A <- right byte of [v_00FC]
00040A:	0FAA	LBX 	DL 	V_00AA	; LBX operand: [V_00AA]
00040C:	0FAA	LBX 	DL 	V_00AA	; LBX operand: [V_00AA]
00040E:	0EE8	LBR 	DL 	V_00E8	; A <- right byte of [V_00E8]
000410:	0F2E	LBX 	DL 	V_002E	; LBX operand: [V_002E]
000412:	0F42	LBX 	DL 	v_0042	; LBX operand: [v_0042]
000414:	0F48	LBX 	DL 	V_0048	; LBX operand: [V_0048]
000416:	0F5A	LBX 	DL 	V_005A	; LBX operand: [V_005A]
000418:	0FAC	LBX 	DL 	V_00AC	; LBX operand: [V_00AC]
00041A:	0F36	LBX 	DL 	V_0036	; LBX operand: [V_0036]
00041C:	0F3E	LBX 	DL 	V_003E	; LBX operand: [V_003E]
00041E:	0F4A	LBX 	DL 	V_004A	; LBX operand: [V_004A]
000420:	0F8A	LBX 	DL 	v_008A	; LBX operand: [v_008A]
000422:	0BB4	CMP 	DL 	V_00B4	; compare A with [V_00B4]
000424:	0FC2	LBX 	DL 	v_00C2	; LBX operand: [v_00C2]
000426:	10BE	DLD 	DL 	v_00BE	; A,E <- [v_00BE] (2 words)
000428:	0FC2	LBX 	DL 	v_00C2	; LBX operand: [v_00C2]
00042A:	1208	STE 	DL 	V_0008	; [V_0008] <- E
00042C:	122C	STE 	DL 	V_002C	; [V_002C] <- E
00042E:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
000430:	114A	STA 	DL 	V_004A	; [V_004A] <- A
000432:	116A	STA 	DL 	V_006A	; [V_006A] <- A
000434:	1192	STA 	DL 	V_0092	; [V_0092] <- A
000436:	13CA	STX 	DL 	v_00CA	; [v_00CA] <- X
000438:	1238	STE 	DL 	V_0038	; [V_0038] <- E
00043A:	129C	STE 	DL 	V_009C	; [V_009C] <- E
00043C:	129C	STE 	DL 	V_009C	; [V_009C] <- E
00043E:	1340	STX 	DL 	v_0040	; [v_0040] <- X
000440:	12FE	STE 	DL 	v_00FE	; [v_00FE] <- E
000442:	00D3	LDA 	DL 	V_00D2	; A <- [V_00D2]
000444:	.EBCDIC	"RCXY"	; 2 words, raw: D9C3 E7E8
000448:	5CFF	FMU 	DG 	G+00FF	; floating multiply by [G+00FF]
00044A:	01C1	LDE 	DL 	v_00C0	; E <- [v_00C0]
00044C:	.EBCDIC	"BFODIASDEITACDUM"	; 8 words, raw: C2C6 D6C4 C9C1 E2C4 C5C9 E3C1 C3C4 E4D4
00045C:	D600	BOF 	IL 	*Unknow_1	; conditional branch, target not known statically  (pointer Unknow_1 = #0000: address = G + #0000, G unknown)
00045E:	FF00	??	FF00
000460:	.EBCDIC	"FASS@BGE"	; 4 words, raw: C6C1 E2E2 7CC2 C7C5
000468:	FF00	??	FF00
00046A:	.EBCDIC	"LPAEXS"	; 3 words, raw: D3D7 C1C5 E7E2
000470:	D5FF	BAZ 	IL 	*v_00FE	; conditional branch, target not known statically  (pointer v_00FE = #22CA: address = G + #22CA, G unknown)
000472:	00FF	LDA 	DL 	v_00FE	; A <- [v_00FE]
000474:	00D7	LDA 	DL 	V_00D6	; A <- [V_00D6]
000476:	FF00	??	FF00
000478:	D5FF	BAZ 	IL 	*v_00FE	; conditional branch, target not known statically  (pointer v_00FE = #22CA: address = G + #22CA, G unknown)
00047A:	007C	LDA 	DL 	V_007C	; A <- [V_007C]
00047C:	FF00	??	FF00
00047E:	.EBCDIC	"RMA@LO"	; 3 words, raw: D9D4 C17C D3D6
000484:	C7FF	BRU 	RP 	l_0682	; 255 words forward: jump to l_0682
000486:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
000488:	.EBCDIC	"MTTDEEBA"	; 4 words, raw: D4E3 E3C4 C5C5 C2C1
000490:	FF00	??	FF00
000492:	.EBCDIC	"APS@&012345678"	; 7 words, raw: C1D7 E27C 50F0 F1F2 F3F4 F5F6 F7F8
0004A0:	F9FF	LDR 	P  	=#FF	; LDR operand: #FF
0004A2:	00D4	LDA 	DL 	V_00D4	; A <- [V_00D4]
0004A4:	E4D6	??	E4D6
0004A6:	D5FF	BAZ 	IL 	*v_00FE	; conditional branch, target not known statically  (pointer v_00FE = #22CA: address = G + #22CA, G unknown)
0004A8:	F080	SLCS	P  	
0004AA:	B070	DLD 	ILX	*v_0070,X	; A,E <- [G+0000+X] (2 words)  (pointer v_0070 = #0000: address = G + #0000, G unknown)
0004AC:	5060	DLD 	DG 	G+0060	; A,E <- [G+0060] (2 words)
0004AE:	4030	LDA 	DG 	G+0030	; A <- [G+0030]
0004B0:	A001	LDA 	ILX	*Unknow_1,X	; A <- [G+0000+X]  (pointer Unknow_1 = #0000: address = G + #0000, G unknown)
0004B2:	.EBCDIC	"TPRERSRBFSFBSPEORO"	; 9 words, raw: E3D7 D9C5 D9E2 D9C2 C6E2 C6C2 E2D7 C5D6 D9D6
0004C4:	00FF	LDA 	DL 	v_00FE	; A <- [v_00FE]
0004C6:	00C1	LDA 	DL 	v_00C0	; A <- [v_00C0]
0004C8:	.EBCDIC	"DVIT"	; 2 words, raw: C4E5 C9E3
0004CC:	D900	BRX 	IG 	$+X	; INDEXED JUMP (BRX): X selects one of the 0 BRU entries that follow (jump / dispatch table)
0004CE:	D5FF	BAZ 	IL 	*v_00FE	; conditional branch, target not known statically  (pointer v_00FE = #22CA: address = G + #22CA, G unknown)

; ===== M_EXIT: INTERRUPT ROUTINE (ends with DIT/DITR: leaves through the interrupt scheduler) =====
; routine profile: privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, ends with DIT/DITR
; =====================================================================
; M:EXIT 4D0 - Terminate task / return to monitor
;
; TASK TERMINATION
; * Normal termination:
; A task terminates normally via a CSV M:EXIT monitor call. The system then deactivates the calling task's level, preserving its current context to ensure the task can be restarted if the level is reactivated later.
; Consequently, the CSV M:EXIT call should normally be followed by a branch to a task re-initialization sequence.
;
; * Abnormal termination:
; The user can employ the %ABORT command to terminate the task associated with the level specified in the command.
; If an exception occurs within an immediate program, the entire system is re-initialized, as the faulty immediate program has likely disrupted the system as a whole.
;
; End of program.
; - If the program is at level zero, control is returned to the monitor, which waits for a console interrupt (level 0 wait loop: BRU $). Any ongoing or pending I/O operations are aborted.
; - If the program is not at level zero, its level is deactivated, allowing any lower level to be processed. Ongoing or pending I/O operations at the level in question are not aborted.
; =====================================================================
M_EXIT:				; entry point; callee (section / supervisor module / SPA subroutine); 2 refs
0004D0:	F908	LDR 	P  	=#08	; LDR operand: #08
0004D2:	C003	BCT 	RP 	L_04D8	; 3 words forward: conditional branch (BCT) to L_04D8
0004D4:	F401	DIT 	P  		; DE-ACTIVATE INTERRUPT: save outgoing context, dispatch the highest-priority waiting level (not a plain return)
0004D6:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
L_04D8:				; branch target; 1 ref
0004D8:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0004DA:	D7AA	BRU 	IL 	*V_00AA	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_00AA = #17CC: address = G + #17CC, G unknown)
L_04DC:				; branch target; 1 ref
0004DC:	F408	STM 	P  		; privileged control instruction (STM)
0004DE:	0ED4	LBR 	DL 	V_00D4	; A <- right byte of [V_00D4]
0004E0:	C504	BAZ 	RP 	L_04E8	; 4 words forward: conditional branch (BAZ) to L_04E8
0004E2:	14DE	SBL 	DL 	V_00DE	; left byte of [V_00DE] <- A
0004E4:	14D5	SBL 	DL 	V_00D4	; left byte of [V_00D4] <- A
0004E6:	C701	BRU 	RP 	L_04E8	; 1 word forward: jump to L_04E8
L_04E8:				; branch target; 2 refs
0004E8:	02D6	LDX 	DL 	V_00D6	; X <- [V_00D6]
0004EA:	AEDA	LBR 	ILX	*V_00DA,X	; A <- right byte of [G+0312+X]  (pointer V_00DA = #0312: address = G + #0312, G unknown)
0004EC:	15D4	SBR 	DL 	V_00D4	; right byte of [V_00D4] <- A
0004EE:	00D6	LDA 	DL 	V_00D6	; A <- [V_00D6]
0004F0:	C503	BAZ 	RP 	L_04F6	; 3 words forward: conditional branch (BAZ) to L_04F6
0004F2:	2601	SUB 	P  	=#01	; A <- A - #01
0004F4:	11D6	STA 	DL 	V_00D6	; [V_00D6] <- A
L_04F6:				; branch target; 1 ref
0004F6:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
0004F8:	A2D8	LDX 	ILX	*V_00D8,X	; X <- [G+030E+X]  (pointer V_00D8 = #030E: address = G + #030E, G unknown)
0004FA:	600A	LDA 	IL 	*CPT_PTR	; A <- [G+0380]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
l_04FC:				; branch target; 1 ref; only from untraced words
0004FC:	730A	STX 	IL 	*CPT_PTR	; [G+0380] <- X  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
0004FE:	2240	LDX 	P  	=#40	; X <- #40
000500:	B10A	STA 	ILX	*CPT_PTR,X	; [G+0380+X] <- A  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
000502:	2040	LDA 	P  	=#40	; A <- #40
000504:	FA08	STR 	P  	=#08	; STR operand: #08
000506:	F401	DIT 	P  		; DE-ACTIVATE INTERRUPT: save outgoing context, dispatch the highest-priority waiting level (not a plain return)
000508:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
00050A:	00F6	LDA 	DL 	V_00F6	; A <- [V_00F6]
00050C:	CF08	BRU 	RM 	l_04FC	; 8 words backward: jump to l_04FC
l_050E:				; branch target; 1 ref; only from untraced words
00050E:	C700	BRU 	RP 	l_050E	; 0 words forward: jump to l_050E

; ===== INT1: INTERRUPT ROUTINE (ends with DIT/DITR: leaves through the interrupt scheduler) =====
; routine profile: 1 instruction, ends with DIT/DITR
INT1:				; entry point; branch target; 2 refs
000510:	F401	DIT 	P  		; DE-ACTIVATE INTERRUPT: save outgoing context, dispatch the highest-priority waiting level (not a plain return)
000512:	CF01	BRU 	RM 	INT1	; 1 word backward: jump to INT1  [-> INT1: 1 instruction, ends with DIT/DITR]

; ===== MOD_0514: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N1, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: shifts/bit manipulation 8 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_056E S_06C2 S_06C4 S_07EA ..., returns to its CSV caller with RSV
MOD_0514:				; entry point; 1 ref
000514:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000516:	C702	BRU 	RP 	S_051A	; 2 words forward: CALL subroutine S_051A (SPA above saved the return address); returns to L_0518  [-> S_051A: shifts/bit manipulation 8 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_056E S_06C2 S_06C4 S_07EA ..., returns through its return cell]
L_0518:				; branch target; 1 ref
000518:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== S_051A: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: shifts/bit manipulation 8 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_056E S_06C2 S_06C4 S_07EA ..., returns through its return cell
S_051A:				; callee (section / supervisor module / SPA subroutine); 1 ref
00051A:	2100	LDE 	P  	=#00	; E <- #00
00051C:	5210	STE 	DG 	TWB_T4	; [TWB_T4] <- E
00051E:	5114	STA 	DG 	TWB_T6	; [TWB_T6] <- A
000520:	20A0	LDA 	P  	=#A0	; A <- #A0
000522:	2200	LDX 	P  	=#00	; X <- #00
000524:	4110	LDE 	DG 	TWB_T4	; E <- [TWB_T4]
000526:	C602	BOF 	RP 	L_052A	; 2 words forward: conditional branch (BOF) to L_052A
000528:	4206	LDX 	DG 	TWB_ZC	; X <- [TWB_ZC]
L_052A:				; branch target; 1 ref
00052A:	9514	SBR 	IGX	*TWB_T6,X	; right byte of [*TWB_T6+X] <- A  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
00052C:	4014	LDA 	DG 	TWB_T6	; A <- [TWB_T6]
00052E:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
000530:	F908	LDR 	P  	=#08	; LDR operand: #08
000532:	F0C1	SRLS	P  	#01
000534:	510A	STA 	DG 	TWB_T1	; [TWB_T1] <- A
000536:	F108	XAA 	P  	
000538:	4710	IOR 	DG 	TWB_T4	; A <- A or [TWB_T4]
00053A:	5410	SBL 	DG 	TWB_T4	; left byte of [TWB_T4] <- A
00053C:	F902	LDR 	P  	=#02	; LDR operand: #02
00053E:	4210	LDX 	DG 	TWB_T4	; X <- [TWB_T4]
000540:	C602	BOF 	RP 	L_0544	; 2 words forward: conditional branch (BOF) to L_0544
000542:	4506	ADD 	DG 	TWB_ZC	; A <- A + [TWB_ZC]
L_0544:				; branch target; 1 ref
000544:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000546:	C714	BRU 	RP 	S_056E	; 20 words forward: CALL subroutine S_056E (SPA above saved the return address); returns to L_0548  [-> S_056E: shifts/bit manipulation 8 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_06C4 S_07EA S_078C S_06C2, returns through its return cell]
L_0548:				; branch target; 1 ref
000548:	4E0A	LBR 	DG 	TWB_T1	; A <- right byte of [TWB_T1]
00054A:	F0C3	SRLS	P  	#03
00054C:	C523	BAZ 	RP 	L_0592	; 35 words forward: conditional branch (BAZ) to L_0592
00054E:	410C	LDE 	DG 	TWB_T2	; E <- [TWB_T2]
000550:	4208	LDX 	DG 	TWB_T0	; X <- [TWB_T0]
000552:	AE34	LBR 	ILX	*V_0034,X	; A <- right byte of [*V_0034+X]
000554:	2960	AND 	P  	=#60	; A <- A and #60
000556:	F102	XAE 	P  		; exchange A and E
000558:	C303	BCF 	RP 	L_055E	; 3 words forward: conditional branch (BCF) to L_055E
00055A:	450E	ADD 	DG 	TWB_T3	; A <- A + [TWB_T3]
00055C:	C022	BCT 	RP 	L_05A0	; 34 words forward: conditional branch (BCT) to L_05A0
L_055E:				; branch target; 1 ref
00055E:	0126	LDE 	DL 	V_0026	; E <- [V_0026]
000560:	F102	XAE 	P  		; exchange A and E
000562:	4208	LDX 	DG 	TWB_T0	; X <- [TWB_T0]
000564:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000566:	D75A	BRU 	IL 	*V_005A	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005A = #1642: address = G + #1642, G unknown)
L_0568:				; branch target; 1 ref
000568:	5308	STX 	DG 	TWB_T0	; [TWB_T0] <- X
00056A:	C41B	BAN 	RP 	L_05A0	; 27 words forward: conditional branch (BAN) to L_05A0
00056C:	C750	BRU 	RP 	L_060C	; 80 words forward: jump to L_060C

; ===== S_056E: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: shifts/bit manipulation 8 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_06C4 S_07EA S_078C S_06C2, returns through its return cell
S_056E:				; callee (section / supervisor module / SPA subroutine); branch target; 2 refs
00056E:	5708	ADM 	DG 	TWB_T0	; add A to [TWB_T0]
000570:	4208	LDX 	DG 	TWB_T0	; X <- [TWB_T0]
000572:	A138	LDE 	ILX	*V_0038,X	; E <- [G+0004+X]  (pointer V_0038 = #0004: address = G + #0004, G unknown)
000574:	520C	STE 	DG 	TWB_T2	; [TWB_T2] <- E
000576:	4614	SUB 	DG 	TWB_T6	; A <- A - [TWB_T6]
000578:	570C	ADM 	DG 	TWB_T2	; add A to [TWB_T2]
00057A:	A03A	LDA 	ILX	*V_003A,X	; A <- [G+0006+X]  (pointer V_003A = #0006: address = G + #0006, G unknown)
00057C:	510E	STA 	DG 	TWB_T3	; [TWB_T3] <- A
00057E:	AE30	LBR 	ILX	*V_0030,X	; A <- right byte of [G+0000+X]  (pointer V_0030 = #0000: address = G + #0000, G unknown)
000580:	2780	IOR 	P  	=#80	; A <- A or #80
000582:	B530	SBR 	ILX	*V_0030,X	; right byte of [G+0000+X] <- A  (pointer V_0030 = #0000: address = G + #0000, G unknown)
000584:	AD36	LBL 	ILX	*V_0036,X	; A <- left byte of [G+0003+X]  (pointer V_0036 = #0003: address = G + #0003, G unknown)
000586:	C42B	BAN 	RP 	L_05DC	; 43 words forward: conditional branch (BAN) to L_05DC
000588:	AE36	LBR 	ILX	*V_0036,X	; A <- right byte of [G+0003+X]  (pointer V_0036 = #0003: address = G + #0003, G unknown)
00058A:	C503	BAZ 	RP 	L_0590	; 3 words forward: conditional branch (BAZ) to L_0590
00058C:	6B16	CMP 	IL 	*V_0016	; compare A with [G+2270]  (pointer V_0016 = #2270: address = G + #2270, G unknown)
00058E:	C20B	BOT 	RP 	L_05A4	; 11 words forward: conditional branch (BOT) to L_05A4
L_0590:				; branch target; 3 refs
000590:	20B9	LDA 	P  	=#B9	; A <- #B9
L_0592:				; branch target; 2 refs
000592:	FA05	STR 	P  	=#05	; STR operand: #05
000594:	4E10	LBR 	DG 	TWB_T4	; A <- right byte of [TWB_T4]
000596:	510E	STA 	DG 	TWB_T3	; [TWB_T3] <- A
000598:	F905	LDR 	P  	=#05	; LDR operand: #05
00059A:	5810	SPA 	DG 	TWB_T4	; [TWB_T4] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
00059C:	C793	BRU 	RP 	S_06C2	; 147 words forward: CALL subroutine S_06C2 (SPA above saved the return address); returns to L_059E  [-> S_06C2: block move / translate string (MVS/TRS), shifts/bit manipulation 8 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_07EA S_078C S_06C4 ..., returns through its return cell]
L_059E:				; branch target; 1 ref
00059E:	C78B	BRU 	RP 	L_06B4	; 139 words forward: jump to L_06B4
L_05A0:				; branch target; 2 refs
0005A0:	20B7	LDA 	P  	=#B7	; A <- #B7
0005A2:	CF08	BRU 	RM 	L_0592	; 8 words backward: jump to L_0592
L_05A4:				; branch target; 1 ref
0005A4:	F104	XAX 	P  		; exchange A and X
0005A6:	AE18	LBR 	ILX	*V_0018,X	; A <- right byte of [G+2294+X]  (pointer V_0018 = #2294: address = G + #2294, G unknown)
0005A8:	5112	STA 	DG 	TWB_T5	; [TWB_T5] <- A
0005AA:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
0005AC:	A11A	LDE 	ILX	*V_001A,X	; E <- [G+22A6+X]  (pointer V_001A = #22A6: address = G + #22A6, G unknown)
0005AE:	520A	STE 	DG 	TWB_T1	; [TWB_T1] <- E
0005B0:	2000	LDA 	P  	=#00	; A <- #00
0005B2:	F065	SLCD	P  	#05
0005B4:	2B02	CMP 	P  	=#02	; compare A with #02
0005B6:	C202	BOT 	RP 	L_05BA	; 2 words forward: conditional branch (BOT) to L_05BA
0005B8:	2601	SUB 	P  	=#01	; A <- A - #01
L_05BA:				; branch target; 1 ref
0005BA:	F102	XAE 	P  		; exchange A and E
0005BC:	4208	LDX 	DG 	TWB_T0	; X <- [TWB_T0]
0005BE:	AE34	LBR 	ILX	*V_0034,X	; A <- right byte of [*V_0034+X]
0005C0:	2B30	CMP 	P  	=#30	; compare A with #30
0005C2:	C208	BOT 	RP 	L_05D2	; 8 words forward: conditional branch (BOT) to L_05D2
0005C4:	2380	EOR 	P  	=#80	; A <- A xor #80
0005C6:	29C0	AND 	P  	=#C0	; A <- A and #C0
0005C8:	C308	BCF 	RP 	L_05D8	; 8 words forward: conditional branch (BCF) to L_05D8
0005CA:	4012	LDA 	DG 	TWB_T5	; A <- [TWB_T5]
0005CC:	2910	AND 	P  	=#10	; A <- A and #10
0005CE:	CD1F	BAZ 	RM 	L_0590	; 31 words backward: conditional branch (BAZ) to L_0590
0005D0:	C704	BRU 	RP 	L_05D8	; 4 words forward: jump to L_05D8
L_05D2:				; branch target; 1 ref
0005D2:	4012	LDA 	DG 	TWB_T5	; A <- [TWB_T5]
0005D4:	2920	AND 	P  	=#20	; A <- A and #20
0005D6:	CD23	BAZ 	RM 	L_0590	; 35 words backward: conditional branch (BAZ) to L_0590
L_05D8:				; branch target; 2 refs
0005D8:	5212	STE 	DG 	TWB_T5	; [TWB_T5] <- E
0005DA:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)
L_05DC:				; branch target; 1 ref
0005DC:	F001	SLLS	P  	#01
0005DE:	C403	BAN 	RP 	L_05E4	; 3 words forward: conditional branch (BAN) to L_05E4
0005E0:	6E7A	LBR 	IL 	*V_007A	; A <- right byte of [G+22C8]  (pointer V_007A = #22C8: address = G + #22C8, G unknown)
0005E2:	C705	BRU 	RP 	L_05EC	; 5 words forward: jump to L_05EC
L_05E4:				; branch target; 1 ref
0005E4:	F902	LDR 	P  	=#02	; LDR operand: #02
0005E6:	4516	ADD 	DG 	TWB_T7	; A <- A + [TWB_T7]
0005E8:	510C	STA 	DG 	TWB_T2	; [TWB_T2] <- A
0005EA:	607A	LDA 	IL 	*V_007A	; A <- [G+22C8]  (pointer V_007A = #22C8: address = G + #22C8, G unknown)
L_05EC:				; branch target; 1 ref
0005EC:	AD36	LBL 	ILX	*V_0036,X	; A <- left byte of [G+0003+X]  (pointer V_0036 = #0003: address = G + #0003, G unknown)
0005EE:	F002	SLLS	P  	#02
0005F0:	F0C2	SRLS	P  	#02
0005F2:	510A	STA 	DG 	TWB_T1	; [TWB_T1] <- A
0005F4:	C731	BRU 	RP 	L_0656	; 49 words forward: jump to L_0656
L_05F6:				; branch target; 1 ref
0005F6:	6214	LDX 	IL 	*V_0014	; X <- [*V_0014]
0005F8:	2910	AND 	P  	=#10	; A <- A and #10
0005FA:	C33E	BCF 	RP 	L_0676	; 62 words forward: conditional branch (BCF) to L_0676
0005FC:	F301	DCX 	P  	=#01	; DCX operand: #01
0005FE:	C73C	BRU 	RP 	L_0676	; 60 words forward: jump to L_0676
L_0600:				; branch target; 1 ref
000600:	2120	LDE 	P  	=#20	; E <- #20
000602:	4E10	LBR 	DG 	TWB_T4	; A <- right byte of [TWB_T4]
000604:	297F	AND 	P  	=#7F	; A <- A and #7F
000606:	2DFF	LBL 	P  	=#FF	; A <- left byte of #FF
000608:	F403	WD  	P  		; privileged control instruction (WD)
00060A:	F400	CLM 	P  		; privileged control instruction (CLM)
L_060C:				; branch target; 1 ref
00060C:	F408	STM 	P  		; privileged control instruction (STM)
00060E:	709C	DLD 	IL 	*V_009C	; A,E <- [G+0234] (2 words)  (pointer V_009C = #0234: address = G + #0234, G unknown)
000610:	F112	AEE 	P  	
000612:	CD09	BAZ 	RM 	L_0600	; 9 words backward: conditional branch (BAZ) to L_0600
000614:	629C	LDX 	IL 	*V_009C	; X <- [G+0234]  (pointer V_009C = #0234: address = G + #0234, G unknown)
000616:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
000618:	A09C	LDA 	ILX	*V_009C,X	; A <- [G+0234+X]  (pointer V_009C = #0234: address = G + #0234, G unknown)
00061A:	29FF	AND 	P  	=#FF	; A <- A and #FF
00061C:	719C	STA 	IL 	*V_009C	; [G+0234] <- A  (pointer V_009C = #0234: address = G + #0234, G unknown)
00061E:	4E10	LBR 	DG 	TWB_T4	; A <- right byte of [TWB_T4]
000620:	F108	XAA 	P  	
000622:	4114	LDE 	DG 	TWB_T6	; E <- [TWB_T6]
000624:	F102	XAE 	P  		; exchange A and E
000626:	B69C	DST 	ILX	*V_009C,X	; [G+0234+X] <- A,E (2 words)  (pointer V_009C = #0234: address = G + #0234, G unknown)
000628:	4112	LDE 	DG 	TWB_T5	; E <- [TWB_T5]
00062A:	F106	XEX 	P  		; exchange E and X
00062C:	F0E1	SRCD	P  	#01
00062E:	AE98	LBR 	ILX	*V_0098,X	; A <- right byte of [G+2232+X]  (pointer V_0098 = #2232: address = G + #2232, G unknown)
000630:	C50A	BAZ 	RP 	L_0644	; 10 words forward: conditional branch (BAZ) to L_0644
000632:	AE9A	LBR 	ILX	*V_009A,X	; A <- right byte of [G+2246+X]  (pointer V_009A = #2246: address = G + #2246, G unknown)
000634:	F104	XAX 	P  		; exchange A and X
000636:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
000638:	A09C	LDA 	ILX	*V_009C,X	; A <- [G+0234+X]  (pointer V_009C = #0234: address = G + #0234, G unknown)
00063A:	F116	AIE 	P  	
00063C:	B19C	STA 	ILX	*V_009C,X	; [G+0234+X] <- A  (pointer V_009C = #0234: address = G + #0234, G unknown)
00063E:	4212	LDX 	DG 	TWB_T5	; X <- [TWB_T5]
000640:	F102	XAE 	P  		; exchange A and E
000642:	C703	BRU 	RP 	L_0648	; 3 words forward: jump to L_0648
L_0644:				; branch target; 1 ref
000644:	F102	XAE 	P  		; exchange A and E
000646:	B598	SBR 	ILX	*V_0098,X	; right byte of [G+2232+X] <- A  (pointer V_0098 = #2232: address = G + #2232, G unknown)
L_0648:				; branch target; 1 ref
000648:	B59A	SBR 	ILX	*V_009A,X	; right byte of [G+2246+X] <- A  (pointer V_009A = #2246: address = G + #2246, G unknown)
00064A:	AD12	LBL 	ILX	*V_0012,X	; A <- left byte of [G+225A+X]  (pointer V_0012 = #225A: address = G + #225A, G unknown)
00064C:	C424	BAN 	RP 	L_0694	; 36 words forward: conditional branch (BAN) to L_0694
00064E:	4E10	LBR 	DG 	TWB_T4	; A <- right byte of [TWB_T4]
000650:	2780	IOR 	P  	=#80	; A <- A or #80
000652:	B512	SBR 	ILX	*V_0012,X	; right byte of [G+225A+X] <- A  (pointer V_0012 = #225A: address = G + #225A, G unknown)
L_0654:				; branch target; 1 ref
000654:	F400	CLM 	P  		; privileged control instruction (CLM)
L_0656:				; branch target; 1 ref
000656:	400A	LDA 	DG 	TWB_T1	; A <- [TWB_T1]
000658:	F0C1	SRLS	P  	#01
00065A:	2903	AND 	P  	=#03	; A <- A and #03
00065C:	C30A	BCF 	RP 	L_0670	; 10 words forward: conditional branch (BCF) to L_0670
00065E:	400A	LDA 	DG 	TWB_T1	; A <- [TWB_T1]
000660:	2930	AND 	P  	=#30	; A <- A and #30
000662:	CB36	BCF 	RM 	L_05F6	; 54 words backward: conditional branch (BCF) to L_05F6
000664:	4E0A	LBR 	DG 	TWB_T1	; A <- right byte of [TWB_T1]
000666:	F0C3	SRLS	P  	#03
000668:	2B02	CMP 	P  	=#02	; compare A with #02
00066A:	C202	BOT 	RP 	L_066E	; 2 words forward: conditional branch (BOT) to L_066E
00066C:	2601	SUB 	P  	=#01	; A <- A - #01
L_066E:				; branch target; 1 ref
00066E:	C703	BRU 	RP 	L_0674	; 3 words forward: jump to L_0674
L_0670:				; branch target; 1 ref
000670:	6514	ADD 	IL 	*V_0014	; A <- A + [*V_0014]
000672:	2605	SUB 	P  	=#05	; A <- A - #05
L_0674:				; branch target; 1 ref
000674:	FA05	STR 	P  	=#05	; STR operand: #05
L_0676:				; branch target; 2 refs
000676:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
000678:	A014	LDA 	ILX	*V_0014,X	; A <- [*V_0014+X]
00067A:	FA01	STR 	P  	=#01	; STR operand: #01
00067C:	4208	LDX 	DG 	TWB_T0	; X <- [TWB_T0]
00067E:	500A	DLD 	DG 	TWB_T1	; A,E <- [TWB_T1] (2 words)
000680:	1806	SPA 	DL 	V_0006	; [V_0006] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
l_0682:				; branch target; 1 ref; only from untraced words
000682:	D700	BRU 	IL 	*Unknow_1	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer Unknow_1 = #0000: address = G + #0000, G unknown)
L_0684:				; branch target; 1 ref
000684:	F400	CLM 	P  		; privileged control instruction (CLM)
000686:	3604	DCL 	DL 	V_0004	; DCL operand: [V_0004]
000688:	C506	BAZ 	RP 	L_0694	; 6 words forward: conditional branch (BAZ) to L_0694
00068A:	FA04	STR 	P  	=#04	; STR operand: #04
00068C:	2000	LDA 	P  	=#00	; A <- #00
00068E:	550F	SBR 	DG 	TWB_T3+1	; right byte of [TWB_T3+1] <- A
000690:	F904	LDR 	P  	=#04	; LDR operand: #04
000692:	C717	BRU 	RP 	L_06C0	; 23 words forward: jump to L_06C0
L_0694:				; branch target; 2 refs
000694:	4208	LDX 	DG 	TWB_T0	; X <- [TWB_T0]
000696:	A430	LEA 	ILX	*V_0030,X	; A <- G+0000+X  (effective address, memory not read)  (pointer V_0030 = #0000: address = G + #0000, G unknown)
000698:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
00069A:	2200	LDX 	P  	=#00	; X <- #00
00069C:	F408	STM 	P  		; privileged control instruction (STM)
00069E:	8D08	LBL 	IGX	*TWB_T0,X	; A <- left byte of [*TWB_T0+X]
0006A0:	C608	BOF 	RP 	L_06B0	; 8 words forward: conditional branch (BOF) to L_06B0
0006A2:	8E08	LBR 	IGX	*TWB_T0,X	; A <- right byte of [*TWB_T0+X]
0006A4:	2BA0	CMP 	P  	=#A0	; compare A with #A0
0006A6:	C003	BCT 	RP 	L_06AC	; 3 words forward: conditional branch (BCT) to L_06AC
0006A8:	2780	IOR 	P  	=#80	; A <- A or #80
0006AA:	C702	BRU 	RP 	L_06AE	; 2 words forward: jump to L_06AE
L_06AC:				; branch target; 1 ref
0006AC:	2E80	LBR 	P  	=#80	; A <- right byte of #80
L_06AE:				; branch target; 1 ref
0006AE:	9508	SBR 	IGX	*TWB_T0,X	; right byte of [*TWB_T0+X] <- A
L_06B0:				; branch target; 1 ref
0006B0:	F400	CLM 	P  		; privileged control instruction (CLM)
0006B2:	F400	CLM 	P  		; privileged control instruction (CLM)
L_06B4:				; branch target; 1 ref
0006B4:	4014	LDA 	DG 	TWB_T6	; A <- [TWB_T6]
0006B6:	F0C1	SRLS	P  	#01
0006B8:	F001	SLLS	P  	#01
0006BA:	DF1C	BRU 	IG 	*TWB_N1	; RETURN from subroutine: branch through return cell TWB_N1 (written by the SPA of the call)
L_06BC:				; branch target; 1 ref
0006BC:	CF34	BRU 	RM 	L_0654	; 52 words backward: jump to L_0654
L_06BE:				; branch target; 1 ref
0006BE:	CFA8	BRU 	RM 	S_056E	; 168 words backward: jump to S_056E  [-> S_056E: shifts/bit manipulation 8 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_06C4 S_07EA S_078C S_06C2, returns through its return cell]
L_06C0:				; branch target; 1 ref
0006C0:	C777	BRU 	RP 	L_07AE	; 119 words forward: jump to L_07AE

; ===== S_06C2: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: block move / translate string (MVS/TRS), shifts/bit manipulation 8 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_07EA S_078C S_06C4 ..., returns through its return cell
S_06C2:				; callee (section / supervisor module / SPA subroutine); 1 ref
0006C2:	C713	BRU 	RP 	L_06E8	; 19 words forward: jump to L_06E8

; ===== S_06C4: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: 19block move / translate string (MVS/TRS), shifts/bit manipulation 8 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_07EA S_078C S_06C4 ..., returns through its return cell
S_06C4:				; callee (section / supervisor module / SPA subroutine); 2 refs
0006C4:	510C	STA 	DG 	TWB_T2	; [TWB_T2] <- A
0006C6:	400A	LDA 	DG 	TWB_T1	; A <- [TWB_T1]
0006C8:	2930	AND 	P  	=#30	; A <- A and #30
0006CA:	C005	BCT 	RP 	L_06D4	; 5 words forward: conditional branch (BCT) to L_06D4
0006CC:	400C	LDA 	DG 	TWB_T2	; A <- [TWB_T2]
0006CE:	F021	SRCS	P  	#01
0006D0:	510C	STA 	DG 	TWB_T2	; [TWB_T2] <- A
0006D2:	C747	BRU 	RP 	L_0760	; 71 words forward: jump to L_0760
L_06D4:				; branch target; 1 ref
0006D4:	4E0A	LBR 	DG 	TWB_T1	; A <- right byte of [TWB_T1]
0006D6:	F0C3	SRLS	P  	#03
0006D8:	2B02	CMP 	P  	=#02	; compare A with #02
0006DA:	C202	BOT 	RP 	L_06DE	; 2 words forward: conditional branch (BOT) to L_06DE
0006DC:	2601	SUB 	P  	=#01	; A <- A - #01
L_06DE:				; branch target; 1 ref
0006DE:	5112	STA 	DG 	TWB_T5	; [TWB_T5] <- A
0006E0:	FA05	STR 	P  	=#05	; STR operand: #05
0006E2:	AE12	LBR 	ILX	*V_0012,X	; A <- right byte of [G+225A+X]  (pointer V_0012 = #225A: address = G + #225A, G unknown)
0006E4:	550F	SBR 	DG 	TWB_T3+1	; right byte of [TWB_T3+1] <- A
0006E6:	400C	LDA 	DG 	TWB_T2	; A <- [TWB_T2]
L_06E8:				; branch target; 1 ref
0006E8:	F021	SRCS	P  	#01
0006EA:	510C	STA 	DG 	TWB_T2	; [TWB_T2] <- A
0006EC:	4208	LDX 	DG 	TWB_T0	; X <- [TWB_T0]
0006EE:	AE32	LBR 	ILX	*V_0032,X	; A <- right byte of [*V_0032+X]
0006F0:	2902	AND 	P  	=#02	; A <- A and #02
0006F2:	C337	BCF 	RP 	L_0760	; 55 words forward: conditional branch (BCF) to L_0760
0006F4:	400C	LDA 	DG 	TWB_T2	; A <- [TWB_T2]
0006F6:	297F	AND 	P  	=#7F	; A <- A and #7F
0006F8:	2B7B	CMP 	P  	=#7B	; compare A with #7B
0006FA:	C227	BOT 	RP 	L_0748	; 39 words forward: conditional branch (BOT) to L_0748
0006FC:	401A	LDA 	DG 	TWB_N2	; A <- [TWB_N2]
0006FE:	C322	BCF 	RP 	L_0742	; 34 words forward: conditional branch (BCF) to L_0742
000700:	4012	LDA 	DG 	TWB_T5	; A <- [TWB_T5]
000702:	511A	STA 	DG 	TWB_N2	; [TWB_N2] <- A
000704:	210C	LDE 	P  	=#0C	; E <- #0C
000706:	649E	LEA 	IL 	*V_009E	; A <- G+016A  (effective address, memory not read)  (pointer V_009E = #016A: address = G + #016A, G unknown)
000708:	5F28	MVS 	DG 	G+0028	; MVS operand: [G+0028]
00070A:	4E0A	LBR 	DG 	TWB_T1	; A <- right byte of [TWB_T1]
00070C:	F0C3	SRLS	P  	#03
00070E:	FA05	STR 	P  	=#05	; STR operand: #05
000710:	E202	ICX 	PX 	(X)	; ICX operand: [(X)]
000712:	A01C	LDA 	ILX	*V_001C,X	; A <- [G+2214+X]  (pointer V_001C = #2214: address = G + #2214, G unknown)
000714:	5132	STA 	DG 	G+0032	; [G+0032] <- A
000716:	400A	LDA 	DG 	TWB_T1	; A <- [TWB_T1]
000718:	F0C7	SRLS	P  	#07
00071A:	290F	AND 	P  	=#0F	; A <- A and #0F
00071C:	27F0	IOR 	P  	=#F0	; A <- A or #F0
00071E:	5535	SBR 	DG 	G+0035	; right byte of [G+0035] <- A
000720:	400A	LDA 	DG 	TWB_T1	; A <- [TWB_T1]
000722:	2906	AND 	P  	=#06	; A <- A and #06
000724:	F0C1	SRLS	P  	#01
000726:	27F0	IOR 	P  	=#F0	; A <- A or #F0
000728:	5534	SBR 	DG 	G+0034	; right byte of [G+0034] <- A
00072A:	2030	LDA 	P  	=#30	; A <- #30
00072C:	512C	STA 	DG 	G+002C	; [G+002C] <- A
00072E:	401C	LDA 	DG 	TWB_N1	; A <- [TWB_N1]
000730:	5118	STA 	DG 	TWB_N3	; [TWB_N3] <- A
000732:	4428	LEA 	DG 	G+0028	; A <- G+0028  (effective address, memory not read)
000734:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000736:	D750	BRU 	IL 	*V_0050	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0050 = #051A: address = G + #051A, G unknown)
L_0738:				; branch target; 1 ref
000738:	F408	STM 	P  		; privileged control instruction (STM)
00073A:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
00073C:	D752	BRU 	IL 	*V_0052	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0052 = #0844: address = G + #0844, G unknown)
L_073E:				; branch target; 1 ref
00073E:	4018	LDA 	DG 	TWB_N3	; A <- [TWB_N3]
000740:	511C	STA 	DG 	TWB_N1	; [TWB_N1] <- A
L_0742:				; branch target; 1 ref
000742:	401A	LDA 	DG 	TWB_N2	; A <- [TWB_N2]
000744:	5112	STA 	DG 	TWB_T5	; [TWB_T5] <- A
000746:	C778	BRU 	RP 	L_0836	; 120 words forward: jump to L_0836
L_0748:				; branch target; 1 ref
000748:	AD32	LBL 	ILX	*V_0032,X	; A <- left byte of [*V_0032+X]
00074A:	C40B	BAN 	RP 	L_0760	; 11 words forward: conditional branch (BAN) to L_0760
00074C:	400C	LDA 	DG 	TWB_T2	; A <- [TWB_T2]
00074E:	C609	BOF 	RP 	L_0760	; 9 words forward: conditional branch (BOF) to L_0760
000750:	F0C4	SRLS	P  	#04
000752:	250D	ADD 	P  	=#0D	; A <- A + #0D
000754:	4208	LDX 	DG 	TWB_T0	; X <- [TWB_T0]
000756:	4D0F	LBL 	DG 	TWB_T3+1	; A <- left byte of [TWB_T3+1]
000758:	093C	AND 	DL 	V_003C	; A <- A and [V_003C]
00075A:	F108	XAA 	P  	
00075C:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
00075E:	D7A0	BRU 	IL 	*V_00A0	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_00A0 = #153C: address = G + #153C, G unknown)
L_0760:				; branch target; 5 refs
000760:	4008	LDA 	DG 	TWB_T0	; A <- [TWB_T0]
000762:	410C	LDE 	DG 	TWB_T2	; E <- [TWB_T2]
000764:	356A	ICL 	DL 	V_006A	; ICL operand: [V_006A]
000766:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000768:	D732	BRU 	IL 	*V_0032	; RETURN from subroutine: branch through return cell V_0032 (written by the SPA of the call)
L_076A:				; branch target; 1 ref
00076A:	3500	ICL 	DL 	Unknow_1	; ICL operand: [Unknow_1]
00076C:	4208	LDX 	DG 	TWB_T0	; X <- [TWB_T0]
00076E:	AD36	LBL 	ILX	*V_0036,X	; A <- left byte of [G+0003+X]  (pointer V_0036 = #0003: address = G + #0003, G unknown)
000770:	5118	STA 	DG 	TWB_N3	; [TWB_N3] <- A
000772:	F400	CLM 	P  		; privileged control instruction (CLM)
000774:	C436	BAN 	RP 	L_07E0	; 54 words forward: conditional branch (BAN) to L_07E0
000776:	DF10	BRU 	IG 	*TWB_T4	; RETURN from subroutine: branch through return cell TWB_T4 (written by the SPA of the call)
000778:	5810	SPA 	DG 	TWB_T4	; [TWB_T4] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
00077A:	CF5B	BRU 	RM 	S_06C4	; 91 words backward: CALL subroutine S_06C4 (SPA above saved the return address); returns to l_077C  [-> S_06C4: 19block move / translate string (MVS/TRS), shifts/bit manipulation 8 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_07EA S_078C S_06C4 ..., returns through its return cell]
l_077C:				; branch target; 1 ref; only from untraced words
00077C:	C75D	BRU 	RP 	L_0836	; 93 words forward: jump to L_0836
00077E:	4E0A	LBR 	DG 	TWB_T1	; A <- right byte of [TWB_T1]
000780:	F0C3	SRLS	P  	#03
000782:	2B02	CMP 	P  	=#02	; compare A with #02
000784:	C202	BOT 	RP 	l_0788	; 2 words forward: conditional branch (BOT) to l_0788
000786:	2601	SUB 	P  	=#01	; A <- A - #01
l_0788:				; branch target; 1 ref; only from untraced words
000788:	5112	STA 	DG 	TWB_T5	; [TWB_T5] <- A
00078A:	C714	BRU 	RP 	L_07B2	; 20 words forward: jump to L_07B2

; ===== S_078C: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: shifts/bit manipulation 10 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_06C4 S_07EA S_078C S_06C2, returns through its return cell
S_078C:				; callee (section / supervisor module / SPA subroutine); 1 ref
00078C:	CF67	BRU 	RM 	L_06BE	; 103 words backward: jump to L_06BE
L_078E:				; branch target; 1 ref
00078E:	CF69	BRU 	RM 	L_06BC	; 105 words backward: jump to L_06BC

; ===== MON3: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_0798, returns to its CSV caller with RSV
MON3:				; entry point; callee (section / supervisor module / SPA subroutine); 7 refs
000790:	F400	CLM 	P  		; privileged control instruction (CLM)

; ===== MOD_0792: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N1, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: shifts/bit manipulation 6 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_06C4 S_07EA S_078C, returns to its CSV caller with RSV
MOD_0792:				; entry point; 1 ref
000792:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000794:	C702	BRU 	RP 	S_0798	; 2 words forward: CALL subroutine S_0798 (SPA above saved the return address); returns to L_0796  [-> S_0798: shifts/bit manipulation 6 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_06C4 S_07EA S_078C, returns through its return cell]
L_0796:				; branch target; 1 ref
000796:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== S_0798: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: shifts/bit manipulation 6 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_06C4 S_07EA S_078C, returns through its return cell
S_0798:				; callee (section / supervisor module / SPA subroutine); 1 ref
000798:	FA04	STR 	P  	=#04	; STR operand: #04
00079A:	4208	LDX 	DG 	TWB_T0	; X <- [TWB_T0]
00079C:	AE30	LBR 	ILX	*V_0030,X	; A <- right byte of [G+0000+X]  (pointer V_0030 = #0000: address = G + #0000, G unknown)
00079E:	550E	SBR 	DG 	TWB_T3	; right byte of [TWB_T3] <- A
0007A0:	F904	LDR 	P  	=#04	; LDR operand: #04
0007A2:	29E0	AND 	P  	=#E0	; A <- A and #E0
0007A4:	2BE0	CMP 	P  	=#E0	; compare A with #E0
0007A6:	C003	BCT 	RP 	L_07AC	; 3 words forward: conditional branch (BCT) to L_07AC
0007A8:	2000	LDA 	P  	=#00	; A <- #00
0007AA:	511A	STA 	DG 	TWB_N2	; [TWB_N2] <- A
L_07AC:				; branch target; 1 ref
0007AC:	F102	XAE 	P  		; exchange A and E
L_07AE:				; branch target; 1 ref
0007AE:	5810	SPA 	DG 	TWB_T4	; [TWB_T4] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0007B0:	CF76	BRU 	RM 	S_06C4	; 118 words backward: CALL subroutine S_06C4 (SPA above saved the return address); returns to L_07B2  [-> S_06C4: 19block move / translate string (MVS/TRS), shifts/bit manipulation 8 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_07EA S_078C S_06C4 ..., returns through its return cell]
L_07B2:				; branch target; 2 refs
0007B2:	F408	STM 	P  		; privileged control instruction (STM)
0007B4:	4212	LDX 	DG 	TWB_T5	; X <- [TWB_T5]
0007B6:	AF98	LBX 	ILX	*V_0098,X	; LBX operand: [G+2232+X]  (pointer V_0098 = #2232: address = G + #2232, G unknown)
0007B8:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
0007BA:	A09C	LDA 	ILX	*V_009C,X	; A <- [G+0234+X]  (pointer V_009C = #0234: address = G + #0234, G unknown)
0007BC:	540F	SBL 	DG 	TWB_T3+1	; left byte of [TWB_T3+1] <- A
0007BE:	29FF	AND 	P  	=#FF	; A <- A and #FF
0007C0:	4112	LDE 	DG 	TWB_T5	; E <- [TWB_T5]
0007C2:	F106	XEX 	P  		; exchange E and X
0007C4:	B598	SBR 	ILX	*V_0098,X	; right byte of [G+2232+X] <- A  (pointer V_0098 = #2232: address = G + #2232, G unknown)
0007C6:	F106	XEX 	P  		; exchange E and X
0007C8:	A19C	LDE 	ILX	*V_009C,X	; E <- [G+0234+X]  (pointer V_009C = #0234: address = G + #0234, G unknown)
0007CA:	F112	AEE 	P  	
0007CC:	B19C	STA 	ILX	*V_009C,X	; [G+0234+X] <- A  (pointer V_009C = #0234: address = G + #0234, G unknown)
0007CE:	709C	DLD 	IL 	*V_009C	; A,E <- [G+0234] (2 words)  (pointer V_009C = #0234: address = G + #0234, G unknown)
0007D0:	F11E	CHX 	P  	
0007D2:	F104	XAX 	P  		; exchange A and X
0007D4:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
0007D6:	B19C	STA 	ILX	*V_009C,X	; [G+0234+X] <- A  (pointer V_009C = #0234: address = G + #0234, G unknown)
0007D8:	F104	XAX 	P  		; exchange A and X
0007DA:	709C	DLD 	IL 	*V_009C	; A,E <- [G+0234] (2 words)  (pointer V_009C = #0234: address = G + #0234, G unknown)
0007DC:	F905	LDR 	P  	=#05	; LDR operand: #05
0007DE:	769C	DST 	IL 	*V_009C	; [G+0234] <- A,E (2 words)  (pointer V_009C = #0234: address = G + #0234, G unknown)
L_07E0:				; branch target; 1 ref
0007E0:	4018	LDA 	DG 	TWB_N3	; A <- [TWB_N3]
0007E2:	C225	BOT 	RP 	L_082C	; 37 words forward: conditional branch (BOT) to L_082C
L_07E4:				; branch target; 1 ref
0007E4:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0007E6:	C702	BRU 	RP 	S_07EA	; 2 words forward: CALL subroutine S_07EA (SPA above saved the return address); returns to L_07E8  [-> S_07EA: privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, returns through its return cell]
L_07E8:				; branch target; 1 ref
0007E8:	C71C	BRU 	RP 	L_0820	; 28 words forward: jump to L_0820

; ===== S_07EA: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, returns through its return cell
S_07EA:				; callee (section / supervisor module / SPA subroutine); 2 refs
0007EA:	F408	STM 	P  		; privileged control instruction (STM)
0007EC:	4212	LDX 	DG 	TWB_T5	; X <- [TWB_T5]
0007EE:	AE98	LBR 	ILX	*V_0098,X	; A <- right byte of [G+2232+X]  (pointer V_0098 = #2232: address = G + #2232, G unknown)
0007F0:	C51C	BAZ 	RP 	L_0828	; 28 words forward: conditional branch (BAZ) to L_0828
0007F2:	FA05	STR 	P  	=#05	; STR operand: #05
0007F4:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
0007F6:	B09C	DLD 	ILX	*V_009C,X	; A,E <- [G+0234+X] (2 words)  (pointer V_009C = #0234: address = G + #0234, G unknown)
0007F8:	5114	STA 	DG 	TWB_T6	; [TWB_T6] <- A
0007FA:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
0007FC:	F904	LDR 	P  	=#04	; LDR operand: #04
0007FE:	5410	SBL 	DG 	TWB_T4	; left byte of [TWB_T4] <- A
000800:	F108	XAA 	P  	
000802:	297F	AND 	P  	=#7F	; A <- A and #7F
000804:	510A	STA 	DG 	TWB_T1	; [TWB_T1] <- A
000806:	2780	IOR 	P  	=#80	; A <- A or #80
000808:	4212	LDX 	DG 	TWB_T5	; X <- [TWB_T5]
00080A:	B512	SBR 	ILX	*V_0012,X	; right byte of [G+225A+X] <- A  (pointer V_0012 = #225A: address = G + #225A, G unknown)
00080C:	F400	CLM 	P  		; privileged control instruction (CLM)
00080E:	420A	LDX 	DG 	TWB_T1	; X <- [TWB_T1]
000810:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
000812:	A00A	LDA 	ILX	*CPT_PTR,X	; A <- [G+0380+X]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
000814:	F104	XAX 	P  		; exchange A and X
000816:	A0C8	LDA 	ILX	*V_00C8,X	; A <- [G+0008+X]  (pointer V_00C8 = #0008: address = G + #0008, G unknown)
000818:	4110	LDE 	DG 	TWB_T4	; E <- [TWB_T4]
00081A:	C602	BOF 	RP 	L_081E	; 2 words forward: conditional branch (BOF) to L_081E
00081C:	0024	LDA 	DL 	V_0024	; A <- [V_0024]
L_081E:				; branch target; 1 ref
00081E:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)
L_0820:				; branch target; 1 ref
000820:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000822:	CF4B	BRU 	RM 	S_078C	; 75 words backward: CALL subroutine S_078C (SPA above saved the return address); returns to L_0824  [-> S_078C: shifts/bit manipulation 10 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_06C4 S_07EA S_078C S_06C2, returns through its return cell]
L_0824:				; branch target; 1 ref
000824:	4212	LDX 	DG 	TWB_T5	; X <- [TWB_T5]
000826:	CF4C	BRU 	RM 	L_078E	; 76 words backward: jump to L_078E
L_0828:				; branch target; 1 ref
000828:	B512	SBR 	ILX	*V_0012,X	; right byte of [G+225A+X] <- A  (pointer V_0012 = #225A: address = G + #225A, G unknown)
00082A:	F400	CLM 	P  		; privileged control instruction (CLM)
L_082C:				; branch target; 1 ref
00082C:	4014	LDA 	DG 	TWB_T6	; A <- [TWB_T6]
00082E:	F0C1	SRLS	P  	#01
000830:	F001	SLLS	P  	#01
000832:	F400	CLM 	P  		; privileged control instruction (CLM)
000834:	DF1C	BRU 	IG 	*TWB_N1	; RETURN from subroutine: branch through return cell TWB_N1 (written by the SPA of the call)
L_0836:				; branch target; 2 refs
000836:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000838:	CF27	BRU 	RM 	S_07EA	; 39 words backward: CALL subroutine S_07EA (SPA above saved the return address); returns to L_083A  [-> S_07EA: privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, returns through its return cell]
L_083A:				; branch target; 1 ref
00083A:	5708	ADM 	DG 	TWB_T0	; add A to [TWB_T0]
00083C:	CF2C	BRU 	RM 	L_07E4	; 44 words backward: jump to L_07E4

; ===== M_WAIT: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N1, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: 2 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, ends with DIT/DITR
; =====================================================================
; M:WAIT 083E - Wait for event completion
; From the user's perspective, the transfer begins as soon as the M:I0 module acknowledges the I/O call. It starts by setting the event bit in byte 0 of the relevant CB to 1, thereby disabling the event.
; The transfer concludes once the event bit is reset to zero; the monitor then enables the event.
; After the transfer has been initiated—or queued, if the coupler is busy—the user can call the M:WAIT module.
; The role of M:WAIT is to:
; - Disable the caller's interrupt (if the caller is not at level zero) to allow programs assigned to an interrupt level lower than the caller's to execute. The caller will be reactivated upon actual completion of the transfer.
; - Perform a wait loop for event activation (waiting for transfer completion) if the calling program is at level zero.
; - Branch to the "error recovery address" if one is specified in the relevant CB.
; =====================================================================

M_WAIT:				; entry point; callee (section / supervisor module / SPA subroutine); 6 refs
00083E:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000840:	C702	BRU 	RP 	S_0844	; 2 words forward: CALL subroutine S_0844 (SPA above saved the return address); returns to L_0842  [-> S_0844: 2 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, ends with DIT/DITR]
L_0842:				; branch target; 1 ref
000842:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== S_0844: INTERRUPT ROUTINE (ends with DIT/DITR: leaves through the interrupt scheduler) =====
; routine profile: 2 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, ends with DIT/DITR
S_0844:				; callee (section / supervisor module / SPA subroutine); 1 ref
000844:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
L_0846:				; branch target; 2 refs
000846:	F400	CLM 	P  		; privileged control instruction (CLM)
000848:	2200	LDX 	P  	=#00	; X <- #00
L_084A:				; branch target; 1 ref
00084A:	8008	LDA 	IGX	*TWB_T0,X	; A <- [*TWB_T0+X]
00084C:	C616	BOF 	RP 	L_0878	; 22 words forward: conditional branch (BOF) to L_0878
00084E:	F908	LDR 	P  	=#08	; LDR operand: #08
000850:	F0C1	SRLS	P  	#01
000852:	CD04	BAZ 	RM 	L_084A	; 4 words backward: conditional branch (BAZ) to L_084A
000854:	F102	XAE 	P  		; exchange A and E
000856:	F408	STM 	P  		; privileged control instruction (STM)
000858:	8008	LDA 	IGX	*TWB_T0,X	; A <- [*TWB_T0+X]
00085A:	CE0A	BOF 	RM 	L_0846	; 10 words backward: conditional branch (BOF) to L_0846
00085C:	F102	XAE 	P  		; exchange A and E
00085E:	2780	IOR 	P  	=#80	; A <- A or #80
000860:	9508	SBR 	IGX	*TWB_T0,X	; right byte of [*TWB_T0+X] <- A
000862:	F908	LDR 	P  	=#08	; LDR operand: #08
000864:	F11C	CNA 	P  	
000866:	FA05	STR 	P  	=#05	; STR operand: #05
000868:	A00A	LDA 	ILX	*CPT_PTR,X	; A <- [G+0380+X]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
00086A:	F108	XAA 	P  	
00086C:	2340	EOR 	P  	=#40	; A <- A xor #40
00086E:	F108	XAA 	P  	
000870:	2131	LDE 	P  	=#31	; E <- #31
000872:	F403	WD  	P  		; privileged control instruction (WD)
000874:	F401	DIT 	P  		; DE-ACTIVATE INTERRUPT: save outgoing context, dispatch the highest-priority waiting level (not a plain return)
000876:	CF18	BRU 	RM 	L_0846	; 24 words backward: jump to L_0846
L_0878:				; branch target; 1 ref
000878:	F108	XAA 	P  	
00087A:	2940	AND 	P  	=#40	; A <- A and #40
00087C:	C509	BAZ 	RP 	L_088E	; 9 words forward: conditional branch (BAZ) to L_088E
00087E:	8008	LDA 	IGX	*TWB_T0,X	; A <- [*TWB_T0+X]
000880:	2940	AND 	P  	=#40	; A <- A and #40
000882:	C505	BAZ 	RP 	L_088C	; 5 words forward: conditional branch (BAZ) to L_088C
000884:	2208	LDX 	P  	=#08	; X <- #08
000886:	8008	LDA 	IGX	*TWB_T0,X	; A <- [*TWB_T0+X]
000888:	2602	SUB 	P  	=#02	; A <- A - #02
00088A:	5100	STA 	DG 	TWB_P	; [TWB_P] <- A
L_088C:				; branch target; 1 ref
00088C:	2DFF	LBL 	P  	=#FF	; A <- left byte of #FF
L_088E:				; branch target; 1 ref
00088E:	DF1C	BRU 	IG 	*TWB_N1	; RETURN from subroutine: branch through return cell TWB_N1 (written by the SPA of the call)

; ===== M_HXBN: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N0, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: comparisons, 1 loop, stores into the caller's TWB, returns to its CSV caller with RSV
; =====================================================================
; M:HXBN 0890 - Hex string -> binary
; Calling convention: A = pointer to EBCDIC hex string, result in E
;
; Call sequence:
; LEA CHAINE
; CSV M•HXBN
; At the time of CSV execution, register A must contain the address—relative to G—of the first byte of the hexadecimal string to be converted.
;
; Function:
; Convert a hexadecimal number into binary (as a 16-bit word); the number is represented by an EBCDIC character string terminated by a delimiter—a character other than a hexadecimal digit (0–9 and A–F).
; =====================================================================
M_HXBN:				; entry point; 1 ref
000890:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000892:	C702	BRU 	RP 	S_0896	; 2 words forward: CALL subroutine S_0896 (SPA above saved the return address); returns to L_0894  [-> S_0896: comparisons, 1 loop, stores into the caller's TWB, returns through its return cell]
L_0894:				; branch target; 1 ref
000894:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== S_0896: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: comparisons, 1 loop, stores into the caller's TWB, returns through its return cell
S_0896:				; callee (section / supervisor module / SPA subroutine); 1 ref
000896:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
000898:	2200	LDX 	P  	=#00	; X <- #00
00089A:	530A	STX 	DG 	TWB_T1	; [TWB_T1] <- X
L_089C:				; branch target; 1 ref
00089C:	8E08	LBR 	IGX	*TWB_T0,X	; A <- right byte of [*TWB_T0+X]
00089E:	2BFA	CMP 	P  	=#FA	; compare A with #FA
0008A0:	C614	BOF 	RP 	L_08C8	; 20 words forward: conditional branch (BOF) to L_08C8
0008A2:	2BC1	CMP 	P  	=#C1	; compare A with #C1
0008A4:	C212	BOT 	RP 	L_08C8	; 18 words forward: conditional branch (BOT) to L_08C8
0008A6:	2BEF	CMP 	P  	=#EF	; compare A with #EF
0008A8:	C604	BOF 	RP 	L_08B0	; 4 words forward: conditional branch (BOF) to L_08B0
0008AA:	2BC7	CMP 	P  	=#C7	; compare A with #C7
0008AC:	C60E	BOF 	RP 	L_08C8	; 14 words forward: conditional branch (BOF) to L_08C8
0008AE:	2539	ADD 	P  	=#39	; A <- A + #39
L_08B0:				; branch target; 1 ref
0008B0:	290F	AND 	P  	=#0F	; A <- A and #0F
0008B2:	410A	LDE 	DG 	TWB_T1	; E <- [TWB_T1]
0008B4:	F00C	SLLS	P  	#0C
0008B6:	F064	SLCD	P  	#04
0008B8:	520A	STE 	DG 	TWB_T1	; [TWB_T1] <- E
0008BA:	C502	BAZ 	RP 	L_08BE	; 2 words forward: conditional branch (BAZ) to L_08BE
0008BC:	C703	BRU 	RP 	L_08C2	; 3 words forward: jump to L_08C2
L_08BE:				; branch target; 1 ref
0008BE:	F201	ICX 	P  	=#01	; ICX operand: #01
0008C0:	CF12	BRU 	RM 	L_089C	; 18 words backward: jump to L_089C
L_08C2:				; branch target; 1 ref
0008C2:	F905	LDR 	P  	=#05	; LDR operand: #05
0008C4:	F114	CNX 	P  	
0008C6:	C702	BRU 	RP 	L_08CA	; 2 words forward: jump to L_08CA
L_08C8:				; branch target; 3 refs
0008C8:	F905	LDR 	P  	=#05	; LDR operand: #05
L_08CA:				; branch target; 1 ref
0008CA:	5708	ADM 	DG 	TWB_T0	; add A to [TWB_T0]
0008CC:	F104	XAX 	P  		; exchange A and X
0008CE:	410A	LDE 	DG 	TWB_T1	; E <- [TWB_T1]
0008D0:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)

; ===== M_BNHX: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N0, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: comparisons, 1 loop, stores into the caller's TWB, returns to its CSV caller with RSV
; =====================================================================
; M:BNHX 08D2 - Binary -> Hex string
;
; * Call sequence
; LDE NUMBER
; LEA STRING
; CSV M:BNHX
;
; At the time of the CSV execution, register A must contain the address (relative to G) of the first byte of the result string, and register E must contain the number to be converted.
; * Function:
; Convert a 16-bit binary number (&0000 to &FFFF) into a string of hexadecimal digits in the form of four EBCDIC characters.
; =====================================================================
M_BNHX:				; entry point; callee (section / supervisor module / SPA subroutine); 2 refs
0008D2:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0008D4:	C702	BRU 	RP 	S_08D8	; 2 words forward: CALL subroutine S_08D8 (SPA above saved the return address); returns to L_08D6  [-> S_08D8: comparisons, 1 loop, stores into the caller's TWB, returns through its return cell]
L_08D6:				; branch target; 1 ref
0008D6:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== S_08D8: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: comparisons, 1 loop, stores into the caller's TWB, returns through its return cell
S_08D8:				; callee (section / supervisor module / SPA subroutine); 1 ref
0008D8:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
0008DA:	2203	LDX 	P  	=#03	; X <- #03
L_08DC:				; branch target; 1 ref
0008DC:	F0E4	SRCD	P  	#04
0008DE:	F0CC	SRLS	P  	#0C
0008E0:	25F0	ADD 	P  	=#F0	; A <- A + #F0
0008E2:	2BFA	CMP 	P  	=#FA	; compare A with #FA
0008E4:	C202	BOT 	RP 	L_08E8	; 2 words forward: conditional branch (BOT) to L_08E8
0008E6:	2639	SUB 	P  	=#39	; A <- A - #39
L_08E8:				; branch target; 1 ref
0008E8:	9508	SBR 	IGX	*TWB_T0,X	; right byte of [*TWB_T0+X] <- A
0008EA:	F301	DCX 	P  	=#01	; DCX operand: #01
0008EC:	C808	BCT 	RM 	L_08DC	; 8 words backward: conditional branch (BCT) to L_08DC
0008EE:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)

; ===== M_DCBN: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N0, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: multiply/divide 1 loop, stores into the caller's TWB, returns to its CSV caller with RSV
; =====================================================================
; M:DCBN 08F0 - Decimal string -> binary
;
; Decimal-to-Binary Conversion
; Call sequence:
; LEA CHAINE
; CSV M:DCBN
; At the time of the CSV execution, register A must contain the G-relative address of the first byte of the decimal string to be converted.
; Function: 16-bit binary conversion of a decimal number represented by an EBCDIC character string terminated by a delimiter (a character other than a decimal digit, 0–9).

; Output elements:
; - E and T1: conversion result (16-bit word).
; - X and T0: G-relative address of the string-terminating delimiter.
; A >= 0: Number of characters in the string (if no overflow occurred).
; A < 0: If overflow occurred.
; TWB memory locations used: T0 to N0.
; =====================================================================
M_DCBN:				; entry point; 1 ref
0008F0:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0008F2:	C702	BRU 	RP 	S_08F6	; 2 words forward: CALL subroutine S_08F6 (SPA above saved the return address); returns to L_08F4  [-> S_08F6: multiply/divide 1 loop, stores into the caller's TWB, returns through its return cell]
L_08F4:				; branch target; 1 ref
0008F4:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== S_08F6: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: multiply/divide 1 loop, stores into the caller's TWB, returns through its return cell
S_08F6:				; callee (section / supervisor module / SPA subroutine); 1 ref
0008F6:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
0008F8:	2200	LDX 	P  	=#00	; X <- #00
0008FA:	530A	STX 	DG 	TWB_T1	; [TWB_T1] <- X
L_08FC:				; branch target; 1 ref
0008FC:	8E08	LBR 	IGX	*TWB_T0,X	; A <- right byte of [*TWB_T0+X]
0008FE:	2BFA	CMP 	P  	=#FA	; compare A with #FA
000900:	C612	BOF 	RP 	L_0924	; 18 words forward: conditional branch (BOF) to L_0924
000902:	2BF0	CMP 	P  	=#F0	; compare A with #F0
000904:	C210	BOT 	RP 	L_0924	; 16 words forward: conditional branch (BOT) to L_0924
000906:	290F	AND 	P  	=#0F	; A <- A and #0F
000908:	510C	STA 	DG 	TWB_T2	; [TWB_T2] <- A
00090A:	400A	LDA 	DG 	TWB_T1	; A <- [TWB_T1]
00090C:	2C0A	MUL 	P  	=#0A	; multiply by #0A
00090E:	450C	ADD 	DG 	TWB_T2	; A <- A + [TWB_T2]
000910:	510A	STA 	DG 	TWB_T1	; [TWB_T1] <- A
000912:	F10E	ACE 	P  	
000914:	F102	XAE 	P  		; exchange A and E
000916:	C502	BAZ 	RP 	L_091A	; 2 words forward: conditional branch (BAZ) to L_091A
000918:	C703	BRU 	RP 	L_091E	; 3 words forward: jump to L_091E
L_091A:				; branch target; 1 ref
00091A:	F201	ICX 	P  	=#01	; ICX operand: #01
00091C:	CF10	BRU 	RM 	L_08FC	; 16 words backward: jump to L_08FC
L_091E:				; branch target; 1 ref
00091E:	F905	LDR 	P  	=#05	; LDR operand: #05
000920:	F114	CNX 	P  	
000922:	C702	BRU 	RP 	L_0926	; 2 words forward: jump to L_0926
L_0924:				; branch target; 2 refs
000924:	F905	LDR 	P  	=#05	; LDR operand: #05
L_0926:				; branch target; 1 ref
000926:	5708	ADM 	DG 	TWB_T0	; add A to [TWB_T0]
000928:	F104	XAX 	P  		; exchange A and X
00092A:	410A	LDE 	DG 	TWB_T1	; E <- [TWB_T1]
00092C:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)

; ===== M_BNDC: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: multiply/divide, 1 loop, stores into the caller's TWB, returns to its CSV caller with RSV
; =====================================================================
; M:BNDC 092E - Binary -> Decimal string
; Uses reciprocal multiplication by 0x6667 (BNDC_MAGIC) for division by 10
;
; * Call sequence:
; LDE NUMBER
; LEA STRING
; CSV M:BNDC
; At the time of CSV execution, register A must contain the address relative to G of the first byte of the result string, and register E must contain the number to be converted.

; * Function:
; Convert a 15-bit positive binary number (&.0;:n•-..Ç7FFF) into a decimal string in the form of five EBCDIC characters. These characters are right-aligned with leading zeros removed (replaced with spaces).
; =====================================================================
M_BNDC:				; entry point; 1 ref
00092E:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
000930:	2200	LDX 	P  	=#00	; X <- #00
000932:	2E40	LBR 	P  	=#40	; A <- right byte of #40
000934:	2D40	LBL 	P  	=#40	; A <- left byte of #40
000936:	9508	SBR 	IGX	*TWB_T0,X	; right byte of [*TWB_T0+X] <- A
000938:	F201	ICX 	P  	=#01	; ICX operand: #01
00093A:	9108	STA 	IGX	*TWB_T0,X	; [*TWB_T0+X] <- A
00093C:	F202	ICX 	P  	=#02	; ICX operand: #02
00093E:	9108	STA 	IGX	*TWB_T0,X	; [*TWB_T0+X] <- A
000940:	2204	LDX 	P  	=#04	; X <- #04
000942:	F102	XAE 	P  		; exchange A and E
L_0944:				; branch target; 1 ref
000944:	0CCC	MUL 	DL 	V_00CC	; multiply by [V_00CC]
000946:	F042	SAD 	P  	#02
000948:	520A	STE 	DG 	TWB_T1	; [TWB_T1] <- E
00094A:	F0C1	SRLS	P  	#01
00094C:	2C14	MUL 	P  	=#14	; multiply by #14
00094E:	25FF	ADD 	P  	=#FF	; A <- A + #FF
000950:	F10E	ACE 	P  	
000952:	20F0	LDA 	P  	=#F0	; A <- #F0
000954:	F116	AIE 	P  	
000956:	9508	SBR 	IGX	*TWB_T0,X	; right byte of [*TWB_T0+X] <- A
000958:	400A	LDA 	DG 	TWB_T1	; A <- [TWB_T1]
00095A:	C503	BAZ 	RP 	L_0960	; 3 words forward: conditional branch (BAZ) to L_0960
00095C:	F301	DCX 	P  	=#01	; DCX operand: #01
00095E:	C80D	BCT 	RM 	L_0944	; 13 words backward: conditional branch (BCT) to L_0944
L_0960:				; branch target; 1 ref
000960:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== M_ASEB: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N0, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: 1 loop, stores into the caller's TWB, returns to its CSV caller with RSV
; =====================================================================
; M:ASEB 0962 - ASCII -> EBCDIC (in-place conversion)
;
; * Call sequence:
; LDX Number (of characters in the string)
; LEA STRING
; CSV M:ASEB
; At the time of the CSV execution, register A must contain the G-relative address of the first byte of the string to be converted, and register X must contain the number of characters in the string.
; 
; * Function:
; Conversion of a string encoded in ASCII (7-bit ISO) into a string encoded in EBCDIC (8-bit ISO). The converted characters replace the original characters on a one-for-one basis. The value of the zero bit of the ASCII characters is ignored.
; =====================================================================
M_ASEB:				; entry point; callee (section / supervisor module / SPA subroutine); 3 refs
000962:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000964:	C702	BRU 	RP 	S_0968	; 2 words forward: CALL subroutine S_0968 (SPA above saved the return address); returns to L_0966  [-> S_0968: 1 loop, stores into the caller's TWB, returns through its return cell]
L_0966:				; branch target; 1 ref
000966:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== S_0968: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: 1 loop, stores into the caller's TWB, returns through its return cell
S_0968:				; callee (section / supervisor module / SPA subroutine); 1 ref
000968:	510C	STA 	DG 	TWB_T2	; [TWB_T2] <- A
L_096A:				; branch target; 1 ref
00096A:	F301	DCX 	P  	=#01	; DCX operand: #01
00096C:	C309	BCF 	RP 	L_097E	; 9 words forward: conditional branch (BCF) to L_097E
00096E:	530E	STX 	DG 	TWB_T3	; [TWB_T3] <- X
000970:	8E0C	LBR 	IGX	*TWB_T2,X	; A <- right byte of [*TWB_T2+X]  (pointer TWB_T2 lives in the calling task's area: its contents are only known at run time)
000972:	297F	AND 	P  	=#7F	; A <- A and #7F
000974:	F104	XAX 	P  		; exchange A and X
000976:	AE92	LBR 	ILX	*V_0092,X	; A <- right byte of [G+019C+X]  (pointer V_0092 = #019C: address = G + #019C, G unknown)
000978:	420E	LDX 	DG 	TWB_T3	; X <- [TWB_T3]
00097A:	950C	SBR 	IGX	*TWB_T2,X	; right byte of [*TWB_T2+X] <- A  (pointer TWB_T2 lives in the calling task's area: its contents are only known at run time)
00097C:	CF09	BRU 	RM 	L_096A	; 9 words backward: jump to L_096A
L_097E:				; branch target; 1 ref
00097E:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)

; ===== M_EBAS: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N0, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: comparisons, 2 loops, stores into the caller's TWB, returns to its CSV caller with RSV
; =====================================================================
; M:EBAS 0980 - EBCDIC -> ASCII (two-path: fast table or slow bit-loop)
;
; * Call sequence:
; LDX Number (of characters in the string)
; LEA STRING
; CSV M:EBAS
; At the time of the CSV execution, register A must contain the address (relative to G) of the first byte of the string to be converted, and register X must contain the number of characters in the string.
;
; * Function:
; Conversion of a string encoded in EBCDIC (8-bit) into a string encoded in ASCII (7-bit). The converted characters replace the original characters one-for-one. The resulting ASCII characters have even parity. Any EBCDIC character that has no ASCII equivalent is replaced by a space.
; =====================================================================
M_EBAS:				; entry point; 1 ref
000980:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000982:	C702	BRU 	RP 	S_0986	; 2 words forward: CALL subroutine S_0986 (SPA above saved the return address); returns to L_0984  [-> S_0986: comparisons, 2 loops, stores into the caller's TWB, returns through its return cell]
L_0984:				; branch target; 1 ref
000984:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== S_0986: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: comparisons, 2 loops, stores into the caller's TWB, returns through its return cell
S_0986:				; callee (section / supervisor module / SPA subroutine); 1 ref
000986:	510C	STA 	DG 	TWB_T2	; [TWB_T2] <- A
L_0988:				; branch target; 1 ref
000988:	F301	DCX 	P  	=#01	; DCX operand: #01
00098A:	DB1E	BCF 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)
00098C:	5310	STX 	DG 	TWB_T4	; [TWB_T4] <- X
00098E:	8E0C	LBR 	IGX	*TWB_T2,X	; A <- right byte of [*TWB_T2+X]  (pointer TWB_T2 lives in the calling task's area: its contents are only known at run time)
000990:	550E	SBR 	DG 	TWB_T3	; right byte of [TWB_T3] <- A
000992:	6492	LEA 	IL 	*V_0092	; A <- G+019C  (effective address, memory not read)  (pointer V_0092 = #019C: address = G + #019C, G unknown)
000994:	2180	LDE 	P  	=#80	; E <- #80
000996:	4A0E	CPS 	DG 	TWB_T3	; CPS operand: [TWB_T3]
000998:	C603	BOF 	RP 	L_099E	; 3 words forward: conditional branch (BOF) to L_099E
00099A:	20A0	LDA 	P  	=#A0	; A <- #A0
00099C:	C70C	BRU 	RP 	L_09B4	; 12 words forward: jump to L_09B4
L_099E:				; branch target; 1 ref
00099E:	F904	LDR 	P  	=#04	; LDR operand: #04
0009A0:	F11C	CNA 	P  	
0009A2:	257F	ADD 	P  	=#7F	; A <- A + #7F
0009A4:	510E	STA 	DG 	TWB_T3	; [TWB_T3] <- A
0009A6:	2206	LDX 	P  	=#06	; X <- #06
0009A8:	2100	LDE 	P  	=#00	; E <- #00
L_09AA:				; branch target; 1 ref
0009AA:	F021	SRCS	P  	#01
0009AC:	F10E	ACE 	P  	
0009AE:	F301	DCX 	P  	=#01	; DCX operand: #01
0009B0:	C803	BCT 	RM 	L_09AA	; 3 words backward: conditional branch (BCT) to L_09AA
0009B2:	F0E1	SRCD	P  	#01
L_09B4:				; branch target; 1 ref
0009B4:	4210	LDX 	DG 	TWB_T4	; X <- [TWB_T4]
0009B6:	940C	SBL 	IGX	*TWB_T2,X	; left byte of [*TWB_T2+X] <- A  (pointer TWB_T2 lives in the calling task's area: its contents are only known at run time)
0009B8:	CF18	BRU 	RM 	L_0988	; 24 words backward: jump to L_0988
0009BA:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)

; ===== M_LOAD: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: 19block move / translate string (MVS/TRS), multiply/divide, shifts/bit manipulation 6 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_0AB8 ..., returns to its CSV caller with RSV
; =====================================================================
; M:LOAD 09BC - Load an IMT module from disk/tape
;
; The loader loads the program's IMT—including the 16-byte end block (containing the context)—and, after loading, leaves the address of the common area relative to G at location G+6.
;
; * Call sequence:
; - Read header (first record)
; - Set up parameters in the TWB
; - CSV M:LOAD
;
; * Function:
; - Loads an IMT module (non-disk format)—either a program (without overlays) or data—into memory. The user is responsible for reading and parsing the IMT header, as some header elements are used to define the input parameters for M:LOAD.
- Outputs the load map and, if requested, the PRT of the loaded program to M:OC (see % LOAD command).
;
; It outputs the following information to M:OC (teleprinter):
; AAAAAA Program name (from the IMT header)
; PRT (eight words per line if the S option is present)
; xxxx YYYY xxxx YYYY
; 	*
; 	*
; 	*
; 7ZZZ (a) ZZZZ (b) ZZZZ (c)
; (a) Start address of the loaded program (absolute)
; (b) Value of base G (absolute)
; (c) First free address after the program (absolute)
;
; A program is associated to an interrupt level
;
; A program in memory consists of:
; - Its PRT,
; - A Common Data Segment (CDS),
; - Sections (LDS, ..., LPS),
; - A 16-byte program end area containing the context (14 bytes).
;
; Header information to be used
; Address 0: Start
; ...
; Address 8: N == 1 for program, N == 3 for data
; 
; Address 10: LPR: Program length, including PRT and context
; Address 12: LBL: Record length (120)
;
; The M:LOAD module uses a 64-byte extended TWB as a work area and as an input/output interface.
; The TWB elements to be initialized before the call are:
; G+20 T8 
;	CB=0 Event byte
;	1NDI=0 Non-disk IMT format
; G+22 T9  
;	Order=0 Read
;	Label = Operational label (normally 1 for M:BI)
; G+2A T13	Set to 0  
; G+2C T14  ADTAMP = Read buffer address relative to caller's G
; G+2E T15  LBL = 120 Record length (from header)
; G+30 T16
;	Set to 0
;	SORPRT:
;	 - bit 7 = 0 => No PRT output
;	 - bit 6 = 0 => Unprotected program
;	 - bit 5 = 0 => No layout output
; G+32 T17  N (from header)
; 1 => Program
; 3 => Data
; G+38 T20  ADPR = Program load address relative to caller's G
; G+3A T21  LPR = Program length in bytes, including PRT and context (from header).
; 
; Output parameters:
; The TWB elements provided as output are:
; ADPR = Absolute load address
; GPR = G-base of the loaded program (absolute) (for N = 1); length of the loaded IMT for N = 3 (data block)
; FIN PR = First free address following the loaded module (absolute)
; A DCTX = Address of the loaded program's context, relative to the caller's G-base.
; 
; Accumulator: A < 0 indicates an error during loading.
; No validity check is performed on the load location.
; After a program module is loaded, the user connects it to a level by calling the M:CNEC module.
; Standard interrupt handling initiates the program via the conventional context-switching mechanism.
; A program cannot load another program and connect it to the same level as itself.
; =====================================================================
M_LOAD:				; entry point; callee (section / supervisor module / SPA subroutine); 2 refs
0009BC:	4038	LDA 	DG 	G+0038	; A <- [G+0038]
0009BE:	F0C1	SRLS	P  	#01
0009C0:	F001	SLLS	P  	#01
0009C2:	5138	STA 	DG 	G+0038	; [G+0038] <- A
0009C4:	4E32	LBR 	DG 	G+0032	; A <- right byte of [G+0032]
0009C6:	2B03	CMP 	P  	=#03	; compare A with #03
0009C8:	C304	BCF 	RP 	L_09D0	; 4 words forward: conditional branch (BCF) to L_09D0
0009CA:	2010	LDA 	P  	=#10	; A <- #10
0009CC:	F11C	CNA 	P  	
0009CE:	573A	ADM 	DG 	G+003A	; add A to [G+003A]
L_09D0:				; branch target; 1 ref
0009D0:	4E21	LBR 	DG 	G+0021	; A <- right byte of [G+0021]
0009D2:	2920	AND 	P  	=#20	; A <- A and #20
0009D4:	C511	BAZ 	RP 	L_09F6	; 17 words forward: conditional branch (BAZ) to L_09F6
0009D6:	5038	DLD 	DG 	G+0038	; A,E <- [G+0038] (2 words)
0009D8:	5624	DST 	DG 	G+0024	; [G+0024] <- A,E (2 words)
0009DA:	523C	STE 	DG 	G+003C	; [G+003C] <- E
0009DC:	573C	ADM 	DG 	G+003C	; add A to [G+003C]
0009DE:	4420	LEA 	DG 	G+0020	; A <- G+0020  (effective address, memory not read)
0009E0:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0009E2:	D750	BRU 	IL 	*V_0050	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0050 = #051A: address = G + #051A, G unknown)
L_09E4:				; branch target; 1 ref
0009E4:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0009E6:	D752	BRU 	IL 	*V_0052	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0052 = #0844: address = G + #0844, G unknown)
L_09E8:				; branch target; 1 ref
0009E8:	C402	BAN 	RP 	L_09EC	; 2 words forward: conditional branch (BAN) to L_09EC
0009EA:	C71C	BRU 	RP 	L_0A22	; 28 words forward: jump to L_0A22
L_09EC:				; branch target; 1 ref
0009EC:	2D21	LBL 	P  	=#21	; A <- left byte of #21
0009EE:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0009F0:	D75E	BRU 	IL 	*V_005E	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005E = #14F0: address = G + #14F0, G unknown)
L_09F2:				; branch target; 1 ref
0009F2:	2DFF	LBL 	P  	=#FF	; A <- left byte of #FF
L_09F4:				; branch target; 2 refs
0009F4:	C761	BRU 	RP 	L_0AB6	; 97 words forward: jump to L_0AB6
L_09F6:				; branch target; 1 ref
0009F6:	502C	DLD 	DG 	G+002C	; A,E <- [G+002C] (2 words)
0009F8:	5624	DST 	DG 	G+0024	; [G+0024] <- A,E (2 words)
0009FA:	4038	LDA 	DG 	G+0038	; A <- [G+0038]
0009FC:	513C	STA 	DG 	G+003C	; [G+003C] <- A
0009FE:	2000	LDA 	P  	=#00	; A <- #00
000A00:	5533	SBR 	DG 	G+0033	; right byte of [G+0033] <- A
L_0A02:				; branch target; 1 ref
000A02:	5818	SPA 	DG 	TWB_N3	; [TWB_N3] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000A04:	C75A	BRU 	RP 	S_0AB8	; 90 words forward: CALL subroutine S_0AB8 (SPA above saved the return address); returns to L_0A06  [-> S_0AB8: comparisons, 4 loops, stores into the caller's TWB, returns through its return cell]
L_0A06:				; branch target; 1 ref
000A06:	CC09	BAN 	RM 	L_09F4	; 9 words backward: conditional branch (BAN) to L_09F4
000A08:	423C	LDX 	DG 	G+003C	; X <- [G+003C]
000A0A:	5308	STX 	DG 	TWB_T0	; [TWB_T0] <- X
000A0C:	0E03	LBR 	DL 	V_0002	; A <- right byte of [V_0002]
000A0E:	2604	SUB 	P  	=#04	; A <- A - #04
000A10:	FA04	STR 	P  	=#04	; STR operand: #04
000A12:	573C	ADM 	DG 	G+003C	; add A to [G+003C]
000A14:	0404	LEA 	DL 	V_0004	; A <- V_0004  (effective address, memory not read)
000A16:	2200	LDX 	P  	=#00	; X <- #00
000A18:	9F08	MVS 	IGX	*TWB_T0,X	; MVS operand: [*TWB_T0+X]
000A1A:	2000	LDA 	P  	=#00	; A <- #00
000A1C:	FA01	STR 	P  	=#01	; STR operand: #01
000A1E:	8D2C	LBL 	IGX	*G+002C,X	; A <- left byte of [*G+002C+X]  (pointer G+002C lives in the calling task's area: its contents are only known at run time)
000A20:	CC0F	BAN 	RM 	L_0A02	; 15 words backward: conditional branch (BAN) to L_0A02
L_0A22:				; branch target; 1 ref
000A22:	4F32	LBX 	DG 	G+0032	; LBX operand: [G+0032]
000A24:	C100	BRX 	RP 	$+X	; INDEXED JUMP (BRX): X selects one of the 3 BRU entries that follow (jump / dispatch table)
L_0A26:				; branch target; 1 ref
000A26:	C703	BRU 	RP 	L_0A2C	; 3 words forward: jump to L_0A2C
L_0A28:				; branch target; 1 ref
000A28:	C717	BRU 	RP 	L_0A56	; 23 words forward: jump to L_0A56
L_0A2A:				; branch target; 1 ref
000A2A:	C78D	BRU 	RP 	L_0B44	; 141 words forward: jump to L_0B44
L_0A2C:				; branch target; 1 ref
000A2C:	4E30	LBR 	DG 	G+0030	; A <- right byte of [G+0030]
000A2E:	2C06	MUL 	P  	=#06	; multiply by #06
000A30:	F11C	CNA 	P  	
000A32:	C502	BAZ 	RP 	L_0A36	; 2 words forward: conditional branch (BAZ) to L_0A36
000A34:	2602	SUB 	P  	=#02	; A <- A - #02
L_0A36:				; branch target; 1 ref
000A36:	2610	SUB 	P  	=#10	; A <- A - #10
000A38:	453C	ADD 	DG 	G+003C	; A <- A + [G+003C]
000A3A:	513E	STA 	DG 	G+003E	; [G+003E] <- A
000A3C:	F902	LDR 	P  	=#02	; LDR operand: #02
000A3E:	453E	ADD 	DG 	G+003E	; A <- A + [G+003E]
000A40:	5128	STA 	DG 	G+0028	; [G+0028] <- A
000A42:	FA01	STR 	P  	=#01	; STR operand: #01
000A44:	0E0E	LBR 	DL 	PRTS_COUNT	; A <- right byte of [PRTS_COUNT]
000A46:	F002	SLLS	P  	#02
000A48:	4538	ADD 	DG 	G+0038	; A <- A + [G+0038]
000A4A:	1108	STA 	DL 	V_0008	; [V_0008] <- A
000A4C:	F902	LDR 	P  	=#02	; LDR operand: #02
000A4E:	1708	ADM 	DL 	V_0008	; add A to [V_0008]
000A50:	513A	STA 	DG 	G+003A	; [G+003A] <- A
000A52:	0002	LDA 	DL 	V_0002	; A <- [V_0002]
000A54:	C708	BRU 	RP 	L_0A64	; 8 words forward: jump to L_0A64
L_0A56:				; branch target; 1 ref
000A56:	F908	LDR 	P  	=#08	; LDR operand: #08
000A58:	FA05	STR 	P  	=#05	; STR operand: #05
000A5A:	A00A	LDA 	ILX	*CPT_PTR,X	; A <- [G+0380+X]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
000A5C:	5128	STA 	DG 	G+0028	; [G+0028] <- A
000A5E:	4226	LDX 	DG 	G+0026	; X <- [G+0026]
000A60:	F302	DCX 	P  	=#02	; DCX operand: #02
000A62:	8038	LDA 	IGX	*G+0038,X	; A <- [*G+0038+X]  (pointer G+0038 lives in the calling task's area: its contents are only known at run time)
L_0A64:				; branch target; 1 ref
000A64:	C558	BAZ 	RP 	L_0B14	; 88 words forward: conditional branch (BAZ) to L_0B14
000A66:	5134	STA 	DG 	G+0034	; [G+0034] <- A
000A68:	2E00	LBR 	P  	=#00	; A <- right byte of #00
000A6A:	5533	SBR 	DG 	G+0033	; right byte of [G+0033] <- A
000A6C:	502C	DLD 	DG 	G+002C	; A,E <- [G+002C] (2 words)
000A6E:	5624	DST 	DG 	G+0024	; [G+0024] <- A,E (2 words)
000A70:	4E21	LBR 	DG 	G+0021	; A <- right byte of [G+0021]
000A72:	2920	AND 	P  	=#20	; A <- A and #20
000A74:	C502	BAZ 	RP 	L_0A78	; 2 words forward: conditional branch (BAZ) to L_0A78
000A76:	2094	LDA 	P  	=#94	; A <- #94
L_0A78:				; branch target; 1 ref
000A78:	256C	ADD 	P  	=#6C	; A <- A + #6C
000A7A:	5136	STA 	DG 	G+0036	; [G+0036] <- A
L_0A7C:				; branch target; 1 ref
000A7C:	2000	LDA 	P  	=#00	; A <- #00
000A7E:	FA01	STR 	P  	=#01	; STR operand: #01
000A80:	5818	SPA 	DG 	TWB_N3	; [TWB_N3] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000A82:	C71B	BRU 	RP 	S_0AB8	; 27 words forward: CALL subroutine S_0AB8 (SPA above saved the return address); returns to L_0A84  [-> S_0AB8: comparisons, 4 loops, stores into the caller's TWB, returns through its return cell]
L_0A84:				; branch target; 1 ref
000A84:	CC48	BAN 	RM 	L_09F4	; 72 words backward: conditional branch (BAN) to L_09F4
000A86:	4028	LDA 	DG 	G+0028	; A <- [G+0028]
000A88:	FA01	STR 	P  	=#01	; STR operand: #01
000A8A:	2200	LDX 	P  	=#00	; X <- #00
000A8C:	4E21	LBR 	DG 	G+0021	; A <- right byte of [G+0021]
000A8E:	2920	AND 	P  	=#20	; A <- A and #20
000A90:	C302	BCF 	RP 	L_0A94	; 2 words forward: conditional branch (BCF) to L_0A94
000A92:	2204	LDX 	P  	=#04	; X <- #04
L_0A94:				; branch target; 2 refs
000A94:	8024	LDA 	IGX	*G+0024,X	; A <- [*G+0024+X]  (pointer G+0024 lives in the calling task's area: its contents are only known at run time)
000A96:	0508	ADD 	DL 	V_0008	; A <- A + [V_0008]
000A98:	F408	STM 	P  		; privileged control instruction (STM)
000A9A:	110A	STA 	DL 	CPT_PTR	; [CPT_PTR] <- A
000A9C:	600A	LDA 	IL 	*CPT_PTR	; A <- [G+0380]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
000A9E:	050B	ADD 	DL 	CPT_PTR	; A <- A + [CPT_PTR]
000AA0:	710A	STA 	IL 	*CPT_PTR	; [G+0380] <- A  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
000AA2:	F400	CLM 	P  		; privileged control instruction (CLM)
000AA4:	F202	ICX 	P  	=#02	; ICX operand: #02
000AA6:	F11A	LNE 	P  	
000AA8:	F904	LDR 	P  	=#04	; LDR operand: #04
000AAA:	5734	ADM 	DG 	G+0034	; add A to [G+0034]
000AAC:	C534	BAZ 	RP 	L_0B14	; 52 words forward: conditional branch (BAZ) to L_0B14
000AAE:	F905	LDR 	P  	=#05	; LDR operand: #05
000AB0:	4636	SUB 	DG 	G+0036	; A <- A - [G+0036]
000AB2:	CD1B	BAZ 	RM 	L_0A7C	; 27 words backward: conditional branch (BAZ) to L_0A7C
000AB4:	CF10	BRU 	RM 	L_0A94	; 16 words backward: jump to L_0A94
L_0AB6:				; branch target; 1 ref
000AB6:	C776	BRU 	RP 	L_0BA2	; 118 words forward: jump to L_0BA2

; ===== S_0AB8: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: comparisons, 4 loops, stores into the caller's TWB, returns through its return cell
S_0AB8:				; callee (section / supervisor module / SPA subroutine); 2 refs
000AB8:	4420	LEA 	DG 	G+0020	; A <- G+0020  (effective address, memory not read)
000ABA:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000ABC:	D750	BRU 	IL 	*V_0050	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0050 = #051A: address = G + #051A, G unknown)
L_0ABE:				; branch target; 1 ref
000ABE:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000AC0:	D752	BRU 	IL 	*V_0052	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0052 = #0844: address = G + #0844, G unknown)
L_0AC2:				; branch target; 1 ref
000AC2:	C424	BAN 	RP 	L_0B0A	; 36 words forward: conditional branch (BAN) to L_0B0A
L_0AC4:				; branch target; 1 ref
000AC4:	F902	LDR 	P  	=#02	; LDR operand: #02
000AC6:	4524	ADD 	DG 	G+0024	; A <- A + [G+0024]
000AC8:	FA01	STR 	P  	=#01	; STR operand: #01
000ACA:	4E21	LBR 	DG 	G+0021	; A <- right byte of [G+0021]
000ACC:	2920	AND 	P  	=#20	; A <- A and #20
000ACE:	C502	BAZ 	RP 	L_0AD2	; 2 words forward: conditional branch (BAZ) to L_0AD2
000AD0:	DF18	BRU 	IG 	*TWB_N3	; RETURN from subroutine: branch through return cell TWB_N3 (written by the SPA of the call)
L_0AD2:				; branch target; 1 ref
000AD2:	226C	LDX 	P  	=#6C	; X <- #6C
000AD4:	2000	LDA 	P  	=#00	; A <- #00
000AD6:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
L_0AD8:				; branch target; 1 ref
000AD8:	F301	DCX 	P  	=#01	; DCX operand: #01
000ADA:	C304	BCF 	RP 	L_0AE2	; 4 words forward: conditional branch (BCF) to L_0AE2
000ADC:	8E24	LBR 	IGX	*G+0024,X	; A <- right byte of [*G+0024+X]  (pointer G+0024 lives in the calling task's area: its contents are only known at run time)
000ADE:	5708	ADM 	DG 	TWB_T0	; add A to [TWB_T0]
000AE0:	CF04	BRU 	RM 	L_0AD8	; 4 words backward: jump to L_0AD8
L_0AE2:				; branch target; 1 ref
000AE2:	2D00	LBL 	P  	=#00	; A <- left byte of #00
000AE4:	C509	BAZ 	RP 	L_0AF6	; 9 words forward: conditional branch (BAZ) to L_0AF6
000AE6:	2122	LDE 	P  	=#22	; E <- #22
L_0AE8:				; branch target; 2 refs
000AE8:	2000	LDA 	P  	=#00	; A <- #00
000AEA:	FA01	STR 	P  	=#01	; STR operand: #01
000AEC:	F102	XAE 	P  		; exchange A and E
000AEE:	F108	XAA 	P  	
000AF0:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000AF2:	D75E	BRU 	IL 	*V_005E	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005E = #14F0: address = G + #14F0, G unknown)
L_0AF4:				; branch target; 1 ref
000AF4:	DF18	BRU 	IG 	*TWB_N3	; RETURN from subroutine: branch through return cell TWB_N3 (written by the SPA of the call)
L_0AF6:				; branch target; 1 ref
000AF6:	0E01	LBR 	DL 	Unknow_1	; A <- right byte of [Unknow_1]
000AF8:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
000AFA:	4E33	LBR 	DG 	G+0033	; A <- right byte of [G+0033]
000AFC:	4B08	CMP 	DG 	TWB_T0	; compare A with [TWB_T0]
000AFE:	C304	BCF 	RP 	L_0B06	; 4 words forward: conditional branch (BCF) to L_0B06
000B00:	2501	ADD 	P  	=#01	; A <- A + #01
000B02:	5533	SBR 	DG 	G+0033	; right byte of [G+0033] <- A
000B04:	DF18	BRU 	IG 	*TWB_N3	; RETURN from subroutine: branch through return cell TWB_N3 (written by the SPA of the call)
L_0B06:				; branch target; 1 ref
000B06:	2123	LDE 	P  	=#23	; E <- #23
000B08:	CF10	BRU 	RM 	L_0AE8	; 16 words backward: jump to L_0AE8
L_0B0A:				; branch target; 1 ref
000B0A:	4E20	LBR 	DG 	G+0020	; A <- right byte of [G+0020]
000B0C:	2B66	CMP 	P  	=#66	; compare A with #66
000B0E:	C825	BCT 	RM 	L_0AC4	; 37 words backward: conditional branch (BCT) to L_0AC4
000B10:	2121	LDE 	P  	=#21	; E <- #21
000B12:	CF15	BRU 	RM 	L_0AE8	; 21 words backward: jump to L_0AE8
L_0B14:				; branch target; 2 refs
000B14:	4E32	LBR 	DG 	G+0032	; A <- right byte of [G+0032]
000B16:	2B02	CMP 	P  	=#02	; compare A with #02
000B18:	C043	BCT 	RP 	L_0B9E	; 67 words forward: conditional branch (BCT) to L_0B9E
000B1A:	0004	LDA 	DL 	V_0004	; A <- [V_0004]
000B1C:	F002	SLLS	P  	#02
000B1E:	F11C	CNA 	P  	
000B20:	0508	ADD 	DL 	V_0008	; A <- A + [V_0008]
000B22:	1104	STA 	DL 	V_0004	; [V_0004] <- A
000B24:	2202	LDX 	P  	=#02	; X <- #02
L_0B26:				; branch target; 1 ref
000B26:	A004	LDA 	ILX	*V_0004,X	; A <- [G+0000+X]  (pointer V_0004 = #0000: address = G + #0000, G unknown)
000B28:	0508	ADD 	DL 	V_0008	; A <- A + [V_0008]
000B2A:	F102	XAE 	P  		; exchange A and E
000B2C:	F302	DCX 	P  	=#02	; DCX operand: #02
000B2E:	C804	BCT 	RM 	L_0B26	; 4 words backward: conditional branch (BCT) to L_0B26
000B30:	160A	DST 	DL 	CPT_PTR	; [CPT_PTR] <- A,E (2 words)
000B32:	2000	LDA 	P  	=#00	; A <- #00
000B34:	2100	LDE 	P  	=#00	; E <- #00
000B36:	1604	DST 	DL 	V_0004	; [V_0004] <- A,E (2 words)
000B38:	4E30	LBR 	DG 	G+0030	; A <- right byte of [G+0030]
000B3A:	C505	BAZ 	RP 	L_0B44	; 5 words forward: conditional branch (BAZ) to L_0B44
000B3C:	0410	LEA 	DL 	V_0010	; A <- V_0010  (effective address, memory not read)
000B3E:	0510	ADD 	DL 	V_0010	; A <- A + [V_0010]
000B40:	513C	STA 	DG 	G+003C	; [G+003C] <- A
000B42:	C702	BRU 	RP 	L_0B46	; 2 words forward: jump to L_0B46
L_0B44:				; branch target; 2 refs
000B44:	403C	LDA 	DG 	G+003C	; A <- [G+003C]
L_0B46:				; branch target; 1 ref
000B46:	4638	SUB 	DG 	G+0038	; A <- A - [G+0038]
000B48:	F0C1	SRLS	P  	#01
000B4A:	FA04	STR 	P  	=#04	; STR operand: #04
000B4C:	F902	LDR 	P  	=#02	; LDR operand: #02
000B4E:	4538	ADD 	DG 	G+0038	; A <- A + [G+0038]
000B50:	402C	LDA 	DG 	G+002C	; A <- [G+002C]
000B52:	5124	STA 	DG 	G+0024	; [G+0024] <- A
000B54:	0E0E	LBR 	DL 	PRTS_COUNT	; A <- right byte of [PRTS_COUNT]
000B56:	F002	SLLS	P  	#02
000B58:	F102	XAE 	P  		; exchange A and E
000B5A:	2000	LDA 	P  	=#00	; A <- #00
000B5C:	FA01	STR 	P  	=#01	; STR operand: #01
000B5E:	4E31	LBR 	DG 	G+0031	; A <- right byte of [G+0031]
000B60:	5536	SBR 	DG 	G+0036	; right byte of [G+0036] <- A
000B62:	4E32	LBR 	DG 	G+0032	; A <- right byte of [G+0032]
000B64:	5534	SBR 	DG 	G+0034	; right byte of [G+0034] <- A
000B66:	2B03	CMP 	P  	=#03	; compare A with #03
000B68:	C008	BCT 	RP 	L_0B78	; 8 words forward: conditional branch (BCT) to L_0B78
000B6A:	4E31	LBR 	DG 	G+0031	; A <- right byte of [G+0031]
000B6C:	2901	AND 	P  	=#01	; A <- A and #01
000B6E:	C505	BAZ 	RP 	L_0B78	; 5 words forward: conditional branch (BAZ) to L_0B78
000B70:	4224	LDX 	DG 	G+0024	; X <- [G+0024]
000B72:	4038	LDA 	DG 	G+0038	; A <- [G+0038]
000B74:	5818	SPA 	DG 	TWB_N3	; [TWB_N3] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000B76:	D75C	BRU 	IL 	*V_005C	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005C = #146A: address = G + #146A, G unknown)
L_0B78:				; branch target; 3 refs
000B78:	F902	LDR 	P  	=#02	; LDR operand: #02
000B7A:	5738	ADM 	DG 	G+0038	; add A to [G+0038]
000B7C:	F902	LDR 	P  	=#02	; LDR operand: #02
000B7E:	573C	ADM 	DG 	G+003C	; add A to [G+003C]
000B80:	4E34	LBR 	DG 	G+0034	; A <- right byte of [G+0034]
000B82:	2B02	CMP 	P  	=#02	; compare A with #02
000B84:	C605	BOF 	RP 	L_0B8E	; 5 words forward: conditional branch (BOF) to L_0B8E
000B86:	0024	LDA 	DL 	V_0024	; A <- [V_0024]
000B88:	463A	SUB 	DG 	G+003A	; A <- A - [G+003A]
000B8A:	423A	LDX 	DG 	G+003A	; X <- [G+003A]
000B8C:	B13A	STA 	ILX	*V_003A,X	; [G+0006+X] <- A  (pointer V_003A = #0006: address = G + #0006, G unknown)
L_0B8E:				; branch target; 1 ref
000B8E:	4E36	LBR 	DG 	G+0036	; A <- right byte of [G+0036]
000B90:	2904	AND 	P  	=#04	; A <- A and #04
000B92:	C006	BCT 	RP 	L_0B9E	; 6 words forward: conditional branch (BCT) to L_0B9E
000B94:	4224	LDX 	DG 	G+0024	; X <- [G+0024]
000B96:	4438	LEA 	DG 	G+0038	; A <- G+0038  (effective address, memory not read)
000B98:	2106	LDE 	P  	=#06	; E <- #06
000B9A:	5818	SPA 	DG 	TWB_N3	; [TWB_N3] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000B9C:	D75C	BRU 	IL 	*V_005C	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005C = #146A: address = G + #146A, G unknown)
L_0B9E:				; branch target; 3 refs
000B9E:	2000	LDA 	P  	=#00	; A <- #00
000BA0:	FA01	STR 	P  	=#01	; STR operand: #01
L_0BA2:				; branch target; 1 ref
000BA2:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== M_ANAC: section / module entry (no RTS/RSV/DIT seen on its paths) =====
; routine profile: stores into the caller's TWB, returns through its return cell
; =====================================================================
; M:ANAC is a multi-function analysis routine. It walks the command line
; character by character, identifying delimiters and dispatching to the
; appropriate command handler. It uses a state machine with computed
; jumps and case ladders for token recognition.
;
; The commands recognized by MTR are as follows:
; % LOAD Load an IMT
; % RUN Start the last loaded program
; % ABORT Abort a program
; % FOREGROUND Move the foreground limit
; % DISPLAY Edit system elements
; % DUMP Edit memory
; % ASSIGN Assign operational labels
; % DEVICE Specific action on a peripheral
; % TIME Initialize date
; % X Abort background with post-mortem dump
; % Y Abort background without post-mortem dump
; % MODIFY Modify memory
; % IT Arm and/or enable interrupt
; % ACTIVATE Trigger an interrupt
; % PM Protection/unprotection
;
; The first two letters of a command are used to identify it (except for the LOAD, RUN, X, and Y commands, where only the first character is used for this purpose).
;
; TWB Slots used for parsing state (relative to G'):
;   TWB_T5 (&12) = CMD_PTR   : cursor into CMD_BUF while scanning args
;   TWB_T6 (&14) = CMD_BUF   : pointer to current character being scanned
;   TWB_N1 (&1C) = STATE_1   : scratch / state variable
;   TWB_N0 (&1E) = RET_ADDR  : return address for sub-scans
;   TWB_T8 (&20) = ARG_ACC   : argument accumulator / scratch
;   TWB_T9 (&22) = ARG_SCR   : argument scratch
;   TWB_T10(&24) = BUF_BASE  : buffer base address
;   TWB_T11(&26) = BUF_LEN   : buffer length / count
;   TWB_T14(&2C) = TOK_TYPE  : token type / state flags
;   TWB_T15(&2E) = TOK_VAL   : token value / scratch
; =====================================================================
M_ANAC:				; entry point; callee (section / supervisor module / SPA subroutine); 6 refs
000BA4:	5112	STA 	DG 	TWB_T5	; [TWB_T5] <- A
000BA6:	2000	LDA 	P  	=#00	; A <- #00
000BA8:	5114	STA 	DG 	TWB_T6	; [TWB_T6] <- A
000BAA:	530E	STX 	DG 	TWB_T3	; [TWB_T3] <- X
000BAC:	E300	DCX 	PX 	(X)	; DCX operand: [(X)]
000BAE:	A0BA	LDA 	ILX	*V_00BA,X	; A <- [G+0422+X]  (pointer V_00BA = #0422: address = G + #0422, G unknown)
000BB0:	511E	STA 	DG 	TWB_N0	; [TWB_N0] <- A
000BB2:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)
000BB4:	6430	LEA 	IL 	*V_0030	; A <- G+0000  (effective address, memory not read)  (pointer V_0030 = #0000: address = G + #0000, G unknown)
000BB6:	656C	ADD 	IL 	*v_006C	; A <- A + [G+03C6]  (pointer v_006C = #03C6: address = G + #03C6, G unknown)
000BB8:	F102	XAE 	P  		; exchange A and E
000BBA:	8E12	LBR 	IGX	*TWB_T5,X	; A <- right byte of [*TWB_T5+X]  (pointer TWB_T5 lives in the calling task's area: its contents are only known at run time)
000BBC:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000BBE:	D7A2	BRU 	IL 	*v_00A2	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer v_00A2 = #13EA: address = G + #13EA, G unknown)
l_0BC0:				; branch target; 1 ref; only from untraced words
000BC0:	C7B5	BRU 	RP 	l_0D2A	; 181 words forward: jump to l_0D2A
; Handler for %R - Run?
000BC2:	4210	LDX 	DG 	TWB_T4	; X <- [TWB_T4]
000BC4:	C100	BRX 	RP 	$+X	; INDEXED JUMP (BRX): X selects one of the 8 BRU entries that follow (jump / dispatch table)
l_0BC6:				; branch target; 1 ref; only from untraced words
000BC6:	C710	BRU 	RP 	l_0BE6	; 16 words forward: jump to l_0BE6
l_0BC8:				; branch target; 1 ref; only from untraced words
000BC8:	C70F	BRU 	RP 	l_0BE6	; 15 words forward: jump to l_0BE6
l_0BCA:				; branch target; 1 ref; only from untraced words
000BCA:	C70E	BRU 	RP 	l_0BE6	; 14 words forward: jump to l_0BE6
l_0BCC:				; branch target; 1 ref; only from untraced words
000BCC:	C70D	BRU 	RP 	l_0BE6	; 13 words forward: jump to l_0BE6
l_0BCE:				; branch target; 1 ref; only from untraced words
000BCE:	C711	BRU 	RP 	l_0BF0	; 17 words forward: jump to l_0BF0
l_0BD0:				; branch target; 1 ref; only from untraced words
000BD0:	C703	BRU 	RP 	l_0BD6	; 3 words forward: jump to l_0BD6
l_0BD2:				; branch target; 1 ref; only from untraced words
000BD2:	C704	BRU 	RP 	l_0BDA	; 4 words forward: jump to l_0BDA
l_0BD4:				; branch target; 1 ref; only from untraced words
000BD4:	C712	BRU 	RP 	l_0BF8	; 18 words forward: jump to l_0BF8
l_0BD6:				; branch target; 1 ref; only from untraced words
000BD6:	2001	LDA 	P  	=#01	; A <- #01
000BD8:	C702	BRU 	RP 	l_0BDC	; 2 words forward: jump to l_0BDC
l_0BDA:				; branch target; 1 ref; only from untraced words
000BDA:	200F	LDA 	P  	=#0F	; A <- #0F
l_0BDC:				; branch target; 1 ref; only from untraced words
000BDC:	552D	SBR 	DG 	G+002D	; right byte of [G+002D] <- A
000BDE:	4E2C	LBR 	DG 	G+002C	; A <- right byte of [G+002C]
000BE0:	29FE	AND 	P  	=#FE	; A <- A and #FE
000BE2:	552C	SBR 	DG 	G+002C	; right byte of [G+002C] <- A
000BE4:	C709	BRU 	RP 	l_0BF6	; 9 words forward: jump to l_0BF6
l_0BE6:				; branch target; 4 refs; only from untraced words
000BE6:	2080	LDA 	P  	=#80	; A <- #80
000BE8:	E001	SLLS	PX 	#01
000BEA:	472C	IOR 	DG 	G+002C	; A <- A or [G+002C]
000BEC:	542C	SBL 	DG 	G+002C	; left byte of [G+002C] <- A
000BEE:	C704	BRU 	RP 	l_0BF6	; 4 words forward: jump to l_0BF6
l_0BF0:				; branch target; 1 ref; only from untraced words
000BF0:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000BF2:	C7B7	BRU 	RP 	s_0D60	; 183 words forward: CALL subroutine s_0D60 (SPA above saved the return address); returns to l_0BF4
l_0BF4:				; branch target; 1 ref; only from untraced words
000BF4:	512E	STA 	DG 	G+002E	; [G+002E] <- A
l_0BF6:				; branch target; 2 refs; only from untraced words
000BF6:	C71F	BRU 	RP 	l_0C34	; 31 words forward: jump to l_0C34
l_0BF8:				; branch target; 1 ref; only from untraced words
000BF8:	4E2C	LBR 	DG 	G+002C	; A <- right byte of [G+002C]
000BFA:	2701	IOR 	P  	=#01	; A <- A or #01
000BFC:	552C	SBR 	DG 	G+002C	; right byte of [G+002C] <- A
000BFE:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000C00:	.EBCDIC	"G:GF"	; 2 words, raw: C77A C7C6
; Handler for %C?
000C04:	4210	LDX 	DG 	TWB_T4	; X <- [TWB_T4]
000C06:	F306	DCX 	P  	=#06	; DCX operand: #06
000C08:	C304	BCF 	RP 	l_0C10	; 4 words forward: conditional branch (BCF) to l_0C10
000C0A:	C101	BRX 	RP 	$+X	; INDEXED JUMP (BRX): X selects one of the 2 BRU entries that follow (jump / dispatch table)
l_0C0C:				; branch target; 1 ref; only from untraced words
000C0C:	C715	BRU 	RP 	l_0C36	; 21 words forward: jump to l_0C36
l_0C0E:				; branch target; 1 ref; only from untraced words
000C0E:	C70D	BRU 	RP 	l_0C28	; 13 words forward: jump to l_0C28
l_0C10:				; branch target; 1 ref; only from untraced words
000C10:	4210	LDX 	DG 	TWB_T4	; X <- [TWB_T4]
000C12:	2080	LDA 	P  	=#80	; A <- #80
000C14:	E000	SLLS	PX 	
000C16:	F108	XAA 	P  	
000C18:	4723	IOR 	DG 	G+0023	; A <- A or [G+0023]
000C1A:	5523	SBR 	DG 	G+0023	; right byte of [G+0023] <- A
000C1C:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000C1E:	C7A1	BRU 	RP 	s_0D60	; 161 words forward: CALL subroutine s_0D60 (SPA above saved the return address); returns to l_0C20
l_0C20:				; branch target; 1 ref; only from untraced words
000C20:	4210	LDX 	DG 	TWB_T4	; X <- [TWB_T4]
000C22:	E224	ICX 	PX 	(X)	; ICX operand: [(X)]
000C24:	9114	STA 	IGX	*TWB_T6,X	; [*TWB_T6+X] <- A  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
000C26:	C7B4	BRU 	RP 	l_0D8E	; 180 words forward: jump to l_0D8E
l_0C28:				; branch target; 1 ref; only from untraced words
000C28:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000C2A:	C79B	BRU 	RP 	s_0D60	; 155 words forward: CALL subroutine s_0D60 (SPA above saved the return address); returns to l_0C2C
l_0C2C:				; branch target; 1 ref; only from untraced words
000C2C:	2B20	CMP 	P  	=#20	; compare A with #20
000C2E:	C6B7	BOF 	RP 	l_0D9C	; 183 words forward: conditional branch (BOF) to l_0D9C
000C30:	C4B6	BAN 	RP 	l_0D9C	; 182 words forward: conditional branch (BAN) to l_0D9C
000C32:	5520	SBR 	DG 	G+0020	; right byte of [G+0020] <- A
l_0C34:				; branch target; 1 ref; only from untraced words
000C34:	C7AD	BRU 	RP 	l_0D8E	; 173 words forward: jump to l_0D8E
l_0C36:				; branch target; 2 refs; only from untraced words
000C36:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000C38:	C794	BRU 	RP 	s_0D60	; 148 words forward: CALL subroutine s_0D60 (SPA above saved the return address); returns to l_0C3A
l_0C3A:				; branch target; 1 ref; only from untraced words
000C3A:	2BFF	CMP 	P  	=#FF	; compare A with #FF
000C3C:	C6B0	BOF 	RP 	l_0D9C	; 176 words forward: conditional branch (BOF) to l_0D9C
000C3E:	C5AF	BAZ 	RP 	l_0D9C	; 175 words forward: conditional branch (BAZ) to l_0D9C
000C40:	C4AE	BAN 	RP 	l_0D9C	; 174 words forward: conditional branch (BAN) to l_0D9C
000C42:	5522	SBR 	DG 	G+0022	; right byte of [G+0022] <- A
000C44:	C7A5	BRU 	RP 	l_0D8E	; 165 words forward: jump to l_0D8E
; Handler for %X - Abort with post-mortem dump?
l_0C46:				; branch target; 2 refs; only from untraced words
000C46:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000C48:	C78C	BRU 	RP 	s_0D60	; 140 words forward: CALL subroutine s_0D60 (SPA above saved the return address); returns to l_0C4A
l_0C4A:				; branch target; 1 ref; only from untraced words
000C4A:	C4A9	BAN 	RP 	l_0D9C	; 169 words forward: conditional branch (BAN) to l_0D9C
000C4C:	2B20	CMP 	P  	=#20	; compare A with #20
000C4E:	C6A7	BOF 	RP 	l_0D9C	; 167 words forward: conditional branch (BOF) to l_0D9C
000C50:	5120	STA 	DG 	G+0020	; [G+0020] <- A
000C52:	C79E	BRU 	RP 	l_0D8E	; 158 words forward: jump to l_0D8E
; Handler for %Y - Abort without dump?
000C54:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000C56:	C785	BRU 	RP 	s_0D60	; 133 words forward: CALL subroutine s_0D60 (SPA above saved the return address); returns to l_0C58
l_0C58:				; branch target; 1 ref; only from untraced words
000C58:	5120	STA 	DG 	G+0020	; [G+0020] <- A
000C5A:	C79A	BRU 	RP 	l_0D8E	; 154 words forward: jump to l_0D8E
000C5C:	CF0B	BRU 	RM 	l_0C46	; 11 words backward: jump to l_0C46
000C5E:	C756	BRU 	RP 	l_0D0A	; 86 words forward: jump to l_0D0A
; Handler for %B - Background limit?
000C60:	4210	LDX 	DG 	TWB_T4	; X <- [TWB_T4]
000C62:	F307	DCX 	P  	=#07	; DCX operand: #07
000C64:	C30C	BCF 	RP 	l_0C7C	; 12 words forward: conditional branch (BCF) to l_0C7C
l_0C66:				; branch target; 1 ref; only from untraced words
000C66:	4E21	LBR 	DG 	G+0021	; A <- right byte of [G+0021]
000C68:	C101	BRX 	RP 	$+X	; INDEXED JUMP (BRX): X selects one of the 2 BRU entries that follow (jump / dispatch table)
l_0C6A:				; branch target; 1 ref; only from untraced words
000C6A:	C706	BRU 	RP 	l_0C76	; 6 words forward: jump to l_0C76
l_0C6C:				; branch target; 1 ref; only from untraced words
000C6C:	C703	BRU 	RP 	l_0C72	; 3 words forward: jump to l_0C72
000C6E:	2710	IOR 	P  	=#10	; A <- A or #10
000C70:	C704	BRU 	RP 	l_0C78	; 4 words forward: jump to l_0C78
l_0C72:				; branch target; 1 ref; only from untraced words
000C72:	29EF	AND 	P  	=#EF	; A <- A and #EF
000C74:	C702	BRU 	RP 	l_0C78	; 2 words forward: jump to l_0C78
l_0C76:				; branch target; 1 ref; only from untraced words
000C76:	2720	IOR 	P  	=#20	; A <- A or #20
l_0C78:				; branch target; 2 refs; only from untraced words
000C78:	5521	SBR 	DG 	G+0021	; right byte of [G+0021] <- A
000C7A:	C78A	BRU 	RP 	l_0D8E	; 138 words forward: jump to l_0D8E
l_0C7C:				; branch target; 1 ref; only from untraced words
000C7C:	4212	LDX 	DG 	TWB_T5	; X <- [TWB_T5]
000C7E:	F201	ICX 	P  	=#01	; ICX operand: #01
000C80:	8E14	LBR 	IGX	*TWB_T6,X	; A <- right byte of [*TWB_T6+X]  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
000C82:	2B7A	CMP 	P  	=#7A	; compare A with #7A
000C84:	C388	BCF 	RP 	l_0D94	; 136 words forward: conditional branch (BCF) to l_0D94
000C86:	F201	ICX 	P  	=#01	; ICX operand: #01
000C88:	5312	STX 	DG 	TWB_T5	; [TWB_T5] <- X
000C8A:	4210	LDX 	DG 	TWB_T4	; X <- [TWB_T4]
000C8C:	C100	BRX 	RP 	$+X	; INDEXED JUMP (BRX): X selects one of the 6 BRU entries that follow (jump / dispatch table)
l_0C8E:				; branch target; 1 ref; only from untraced words
000C8E:	C706	BRU 	RP 	l_0C9A	; 6 words forward: jump to l_0C9A
l_0C90:				; branch target; 1 ref; only from untraced words
000C90:	C721	BRU 	RP 	l_0CD2	; 33 words forward: jump to l_0CD2
l_0C92:				; branch target; 1 ref; only from untraced words
000C92:	C71A	BRU 	RP 	l_0CC6	; 26 words forward: jump to l_0CC6
l_0C94:				; branch target; 1 ref; only from untraced words
000C94:	C71F	BRU 	RP 	l_0CD2	; 31 words forward: jump to l_0CD2
l_0C96:				; branch target; 1 ref; only from untraced words
000C96:	C71E	BRU 	RP 	l_0CD2	; 30 words forward: jump to l_0CD2
l_0C98:				; branch target; 1 ref; only from untraced words
000C98:	C71D	BRU 	RP 	l_0CD2	; 29 words forward: jump to l_0CD2
l_0C9A:				; branch target; 2 refs; only from untraced words
000C9A:	E304	DCX 	PX 	(X)	; DCX operand: [(X)]
000C9C:	6430	LEA 	IL 	*V_0030	; A <- G+0000  (effective address, memory not read)  (pointer V_0030 = #0000: address = G + #0000, G unknown)
000C9E:	A56C	ADD 	ILX	*v_006C,X	; A <- A + [G+03C6+X]  (pointer v_006C = #03C6: address = G + #03C6, G unknown)
000CA0:	F102	XAE 	P  		; exchange A and E
000CA2:	2201	LDX 	P  	=#01	; X <- #01
000CA4:	8E12	LBR 	IGX	*TWB_T5,X	; A <- right byte of [*TWB_T5+X]  (pointer TWB_T5 lives in the calling task's area: its contents are only known at run time)
000CA6:	2200	LDX 	P  	=#00	; X <- #00
000CA8:	8D12	LBL 	IGX	*TWB_T5,X	; A <- left byte of [*TWB_T5+X]  (pointer TWB_T5 lives in the calling task's area: its contents are only known at run time)
000CAA:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000CAC:	D7A2	BRU 	IL 	*v_00A2	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer v_00A2 = #13EA: address = G + #13EA, G unknown)
l_0CAE:				; branch target; 1 ref; only from untraced words
000CAE:	C473	BAN 	RP 	l_0D94	; 115 words forward: conditional branch (BAN) to l_0D94
000CB0:	F11E	CHX 	P  	
000CB2:	F301	DCX 	P  	=#01	; DCX operand: #01
000CB4:	F905	LDR 	P  	=#05	; LDR operand: #05
000CB6:	5520	SBR 	DG 	G+0020	; right byte of [G+0020] <- A
000CB8:	AE18	LBR 	ILX	*V_0018,X	; A <- right byte of [G+2294+X]  (pointer V_0018 = #2294: address = G + #2294, G unknown)
000CBA:	F0C6	SRLS	P  	#06
000CBC:	2B03	CMP 	P  	=#03	; compare A with #03
000CBE:	C302	BCF 	RP 	l_0CC2	; 2 words forward: conditional branch (BCF) to l_0CC2
000CC0:	2001	LDA 	P  	=#01	; A <- #01
l_0CC2:				; branch target; 1 ref; only from untraced words
000CC2:	FA05	STR 	P  	=#05	; STR operand: #05
000CC4:	CF2F	BRU 	RM 	l_0C66	; 47 words backward: jump to l_0C66
l_0CC6:				; branch target; 1 ref; only from untraced words
000CC6:	2201	LDX 	P  	=#01	; X <- #01
000CC8:	8E12	LBR 	IGX	*TWB_T5,X	; A <- right byte of [*TWB_T5+X]  (pointer TWB_T5 lives in the calling task's area: its contents are only known at run time)
000CCA:	2200	LDX 	P  	=#00	; X <- #00
000CCC:	8D12	LBL 	IGX	*TWB_T5,X	; A <- left byte of [*TWB_T5+X]  (pointer TWB_T5 lives in the calling task's area: its contents are only known at run time)
000CCE:	5122	STA 	DG 	G+0022	; [G+0022] <- A
000CD0:	C75F	BRU 	RP 	l_0D8E	; 95 words forward: jump to l_0D8E
l_0CD2:				; branch target; 5 refs; only from untraced words
000CD2:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000CD4:	C749	BRU 	RP 	s_0D66	; 73 words forward: CALL subroutine s_0D66 (SPA above saved the return address); returns to l_0CD6
l_0CD6:				; branch target; 1 ref; only from untraced words
000CD6:	2BFF	CMP 	P  	=#FF	; compare A with #FF
000CD8:	C002	BCT 	RP 	l_0CDC	; 2 words forward: conditional branch (BCT) to l_0CDC
000CDA:	C663	BOF 	RP 	l_0DA0	; 99 words forward: conditional branch (BOF) to l_0DA0
l_0CDC:				; branch target; 1 ref; only from untraced words
000CDC:	4210	LDX 	DG 	TWB_T4	; X <- [TWB_T4]
000CDE:	F220	ICX 	P  	=#20	; ICX operand: #20
000CE0:	F102	XAE 	P  		; exchange A and E
000CE2:	4010	LDA 	DG 	TWB_T4	; A <- [TWB_T4]
000CE4:	2B02	CMP 	P  	=#02	; compare A with #02
000CE6:	F102	XAE 	P  		; exchange A and E
000CE8:	C003	BCT 	RP 	l_0CEE	; 3 words forward: conditional branch (BCT) to l_0CEE
000CEA:	9514	SBR 	IGX	*TWB_T6,X	; right byte of [*TWB_T6+X] <- A  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
000CEC:	C751	BRU 	RP 	l_0D8E	; 81 words forward: jump to l_0D8E
l_0CEE:				; branch target; 1 ref; only from untraced words
000CEE:	2510	ADD 	P  	=#10	; A <- A + #10
000CF0:	5520	SBR 	DG 	G+0020	; right byte of [G+0020] <- A
000CF2:	C74E	BRU 	RP 	l_0D8E	; 78 words forward: jump to l_0D8E
000CF4:	204B	LDA 	P  	=#4B	; A <- #4B
000CF6:	4212	LDX 	DG 	TWB_T5	; X <- [TWB_T5]
000CF8:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000CFA:	D7A4	BRU 	IL 	*v_00A4	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer v_00A4 = #1418: address = G + #1418, G unknown)
l_0CFC:				; branch target; 1 ref; only from untraced words
000CFC:	F301	DCX 	P  	=#01	; DCX operand: #01
000CFE:	8E14	LBR 	IGX	*TWB_T6,X	; A <- right byte of [*TWB_T6+X]  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
000D00:	2B4B	CMP 	P  	=#4B	; compare A with #4B
000D02:	C349	BCF 	RP 	l_0D94	; 73 words forward: conditional branch (BCF) to l_0D94
l_0D04:				; branch target; 1 ref; only from untraced words
000D04:	F201	ICX 	P  	=#01	; ICX operand: #01
l_0D06:				; branch target; 1 ref; only from untraced words
000D06:	5312	STX 	DG 	TWB_T5	; [TWB_T5] <- X
000D08:	4436	LEA 	DG 	G+0036	; A <- G+0036  (effective address, memory not read)
l_0D0A:				; branch target; 1 ref; only from untraced words
000D0A:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
000D0C:	01CA	LDE 	DL 	v_00CA	; E <- [v_00CA]
l_0D0E:				; branch target; 1 ref; only from untraced words
000D0E:	8E14	LBR 	IGX	*TWB_T6,X	; A <- right byte of [*TWB_T6+X]  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
000D10:	F201	ICX 	P  	=#01	; ICX operand: #01
000D12:	2B7A	CMP 	P  	=#7A	; compare A with #7A
000D14:	C003	BCT 	RP 	l_0D1A	; 3 words forward: conditional branch (BCT) to l_0D1A
000D16:	2BC1	CMP 	P  	=#C1	; compare A with #C1
000D18:	C207	BOT 	RP 	l_0D26	; 7 words forward: conditional branch (BOT) to l_0D26
l_0D1A:				; branch target; 1 ref; only from untraced words
000D1A:	F106	XEX 	P  		; exchange E and X
000D1C:	9508	SBR 	IGX	*TWB_T0,X	; right byte of [*TWB_T0+X] <- A
000D1E:	F201	ICX 	P  	=#01	; ICX operand: #01
000D20:	F106	XEX 	P  		; exchange E and X
000D22:	C003	BCT 	RP 	l_0D28	; 3 words forward: conditional branch (BCT) to l_0D28
000D24:	CF0B	BRU 	RM 	l_0D0E	; 11 words backward: jump to l_0D0E
l_0D26:				; branch target; 1 ref; only from untraced words
000D26:	F301	DCX 	P  	=#01	; DCX operand: #01
l_0D28:				; branch target; 1 ref; only from untraced words
000D28:	DF1C	BRU 	IG 	*TWB_N1	; RETURN from subroutine: branch through return cell TWB_N1 (written by the SPA of the call)
l_0D2A:				; branch target; 1 ref; only from untraced words
000D2A:	410E	LDE 	DG 	TWB_T3	; E <- [TWB_T3]
000D2C:	C009	BCT 	RP 	l_0D3E	; 9 words forward: conditional branch (BCT) to l_0D3E
000D2E:	C131	BRX 	RP 	$+X	; INDEXED JUMP (BRX): X selects one of the 0 BRU entries that follow (jump / dispatch table)
000D30:	5310	STX 	DG 	TWB_T4	; [TWB_T4] <- X
000D32:	420E	LDX 	DG 	TWB_T3	; X <- [TWB_T3]
000D34:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
000D36:	A0BC	LDA 	ILX	*v_00BC,X	; A <- [G+03E4+X]  (pointer v_00BC = #03E4: address = G + #03E4, G unknown)
000D38:	511E	STA 	DG 	TWB_N0	; [TWB_N0] <- A
000D3A:	2001	LDA 	P  	=#01	; A <- #01
000D3C:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)
l_0D3E:				; branch target; 1 ref; only from untraced words
000D3E:	C402	BAN 	RP 	l_0D42	; 2 words forward: conditional branch (BAN) to l_0D42
000D40:	C70E	BRU 	RP 	l_0D5C	; 14 words forward: jump to l_0D5C
l_0D42:				; branch target; 1 ref; only from untraced words
000D42:	6430	LEA 	IL 	*V_0030	; A <- G+0000  (effective address, memory not read)  (pointer V_0030 = #0000: address = G + #0000, G unknown)
000D44:	656C	ADD 	IL 	*v_006C	; A <- A + [G+03C6]  (pointer v_006C = #03C6: address = G + #03C6, G unknown)
000D46:	250B	ADD 	P  	=#0B	; A <- A + #0B
000D48:	F102	XAE 	P  		; exchange A and E
000D4A:	2201	LDX 	P  	=#01	; X <- #01
000D4C:	8E12	LBR 	IGX	*TWB_T5,X	; A <- right byte of [*TWB_T5+X]  (pointer TWB_T5 lives in the calling task's area: its contents are only known at run time)
000D4E:	2200	LDX 	P  	=#00	; X <- #00
000D50:	8D12	LBL 	IGX	*TWB_T5,X	; A <- left byte of [*TWB_T5+X]  (pointer TWB_T5 lives in the calling task's area: its contents are only known at run time)
000D52:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000D54:	D7A2	BRU 	IL 	*v_00A2	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer v_00A2 = #13EA: address = G + #13EA, G unknown)
l_0D56:				; branch target; 1 ref; only from untraced words
000D56:	C41D	BAN 	RP 	l_0D90	; 29 words forward: conditional branch (BAN) to l_0D90
000D58:	F11E	CHX 	P  	
000D5A:	F206	ICX 	P  	=#06	; ICX operand: #06
l_0D5C:				; branch target; 1 ref; only from untraced words
000D5C:	C7A9	BRU 	RP 	l_0EAE	; 169 words forward: jump to l_0EAE
l_0D5E:				; branch target; 1 ref; only from untraced words
000D5E:	CF8C	BRU 	RM 	l_0C46	; 140 words backward: jump to l_0C46
s_0D60:				; callee (section / supervisor module / SPA subroutine); 9 refs; only from untraced words
000D60:	4012	LDA 	DG 	TWB_T5	; A <- [TWB_T5]
000D62:	2501	ADD 	P  	=#01	; A <- A + #01
000D64:	5112	STA 	DG 	TWB_T5	; [TWB_T5] <- A
s_0D66:				; callee (section / supervisor module / SPA subroutine); 3 refs; only from untraced words
000D66:	4012	LDA 	DG 	TWB_T5	; A <- [TWB_T5]
000D68:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000D6A:	D7A6	BRU 	IL 	*v_00A6	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer v_00A6 = #1434: address = G + #1434, G unknown)
l_0D6C:				; branch target; 1 ref; only from untraced words
000D6C:	C416	BAN 	RP 	l_0D98	; 22 words forward: conditional branch (BAN) to l_0D98
000D6E:	8E14	LBR 	IGX	*TWB_T6,X	; A <- right byte of [*TWB_T6+X]  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
000D70:	2B61	CMP 	P  	=#61	; compare A with #61
000D72:	C00B	BCT 	RP 	l_0D88	; 11 words forward: conditional branch (BCT) to l_0D88
000D74:	2B7A	CMP 	P  	=#7A	; compare A with #7A
000D76:	C009	BCT 	RP 	l_0D88	; 9 words forward: conditional branch (BCT) to l_0D88
000D78:	2B6B	CMP 	P  	=#6B	; compare A with #6B
000D7A:	C007	BCT 	RP 	l_0D88	; 7 words forward: conditional branch (BCT) to l_0D88
000D7C:	2B5E	CMP 	P  	=#5E	; compare A with #5E
000D7E:	C005	BCT 	RP 	l_0D88	; 5 words forward: conditional branch (BCT) to l_0D88
000D80:	2B40	CMP 	P  	=#40	; compare A with #40
000D82:	C003	BCT 	RP 	l_0D88	; 3 words forward: conditional branch (BCT) to l_0D88
000D84:	2B0D	CMP 	P  	=#0D	; compare A with #0D
000D86:	C307	BCF 	RP 	l_0D94	; 7 words forward: conditional branch (BCF) to l_0D94
l_0D88:				; branch target; 5 refs; only from untraced words
000D88:	F102	XAE 	P  		; exchange A and E
000D8A:	5312	STX 	DG 	TWB_T5	; [TWB_T5] <- X
000D8C:	DF1A	BRU 	IG 	*TWB_N2	; RETURN from subroutine: branch through return cell TWB_N2 (written by the SPA of the call)
l_0D8E:				; branch target; 11 refs; only from untraced words
000D8E:	C7A2	BRU 	RP 	l_0ED2	; 162 words forward: jump to l_0ED2
; =====================================================================
; DIGIT CASE LADDER (0D90 - 0DAA)
; =====================================================================
; If the character is not a delimiter, it checks if it's a digit.
; This is a dense "case ladder" using the LBL instruction to load 
; the expected digit code into A, then jumping to the handler.
; Codes 031-039 correspond to EBCDIC digits '1'-'9'.
; =====================================================================
l_0D90:				; branch target; 1 ref; only from untraced words
000D90:	2D31	LBL 	P  	=#31	; A <- left byte of #31
000D92:	C794	BRU 	RP 	l_0EBA	; 148 words forward: jump to l_0EBA
l_0D94:				; branch target; 8 refs; only from untraced words
000D94:	2D32	LBL 	P  	=#32	; A <- left byte of #32
000D96:	C792	BRU 	RP 	l_0EBA	; 146 words forward: jump to l_0EBA
l_0D98:				; branch target; 1 ref; only from untraced words
000D98:	2D35	LBL 	P  	=#35	; A <- left byte of #35
000D9A:	C790	BRU 	RP 	l_0EBA	; 144 words forward: jump to l_0EBA
l_0D9C:				; branch target; 8 refs; only from untraced words
000D9C:	2D36	LBL 	P  	=#36	; A <- left byte of #36
000D9E:	C78E	BRU 	RP 	l_0EBA	; 142 words forward: jump to l_0EBA
l_0DA0:				; branch target; 1 ref; only from untraced words
000DA0:	2D37	LBL 	P  	=#37	; A <- left byte of #37
000DA2:	C78C	BRU 	RP 	l_0EBA	; 140 words forward: jump to l_0EBA
000DA4:	2D38	LBL 	P  	=#38	; A <- left byte of #38
000DA6:	C78A	BRU 	RP 	l_0EBA	; 138 words forward: jump to l_0EBA
l_0DA8:				; branch target; 1 ref; only from untraced words
000DA8:	2D39	LBL 	P  	=#39	; A <- left byte of #39
000DAA:	C788	BRU 	RP 	l_0EBA	; 136 words forward: jump to l_0EBA
000DAC:	4210	LDX 	DG 	TWB_T4	; X <- [TWB_T4]
000DAE:	F304	DCX 	P  	=#04	; DCX operand: #04
000DB0:	C005	BCT 	RP 	l_0DBA	; 5 words forward: conditional branch (BCT) to l_0DBA
000DB2:	C104	BRX 	RP 	$+X	; INDEXED JUMP (BRX): X selects one of the 3 BRU entries that follow (jump / dispatch table)
l_0DB4:				; branch target; 1 ref; only from untraced words
000DB4:	C719	BRU 	RP 	l_0DE6	; 25 words forward: jump to l_0DE6
l_0DB6:				; branch target; 1 ref; only from untraced words
000DB6:	C71A	BRU 	RP 	l_0DEA	; 26 words forward: jump to l_0DEA
l_0DB8:				; branch target; 1 ref; only from untraced words
000DB8:	CFC1	BRU 	RM 	l_0C36	; 193 words backward: jump to l_0C36
l_0DBA:				; branch target; 1 ref; only from untraced words
000DBA:	F301	DCX 	P  	=#01	; DCX operand: #01
000DBC:	C019	BCT 	RP 	l_0DEE	; 25 words forward: conditional branch (BCT) to l_0DEE
000DBE:	4028	LDA 	DG 	G+0028	; A <- [G+0028]
000DC0:	5126	STA 	DG 	G+0026	; [G+0026] <- A
000DC2:	2504	ADD 	P  	=#04	; A <- A + #04
000DC4:	5128	STA 	DG 	G+0028	; [G+0028] <- A
000DC6:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000DC8:	CF34	BRU 	RM 	s_0D60	; 52 words backward: CALL subroutine s_0D60 (SPA above saved the return address); returns to l_0DCA
l_0DCA:				; branch target; 1 ref; only from untraced words
000DCA:	F102	XAE 	P  		; exchange A and E
000DCC:	8E14	LBR 	IGX	*TWB_T6,X	; A <- right byte of [*TWB_T6+X]  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
000DCE:	2B7A	CMP 	P  	=#7A	; compare A with #7A
000DD0:	CB1E	BCF 	RM 	l_0D94	; 30 words backward: conditional branch (BCF) to l_0D94
000DD2:	5520	SBR 	DG 	G+0020	; right byte of [G+0020] <- A
000DD4:	2000	LDA 	P  	=#00	; A <- #00
000DD6:	F102	XAE 	P  		; exchange A and E
000DD8:	4226	LDX 	DG 	G+0026	; X <- [G+0026]
000DDA:	9614	DST 	IGX	*TWB_T6,X	; [*TWB_T6+X] <- A,E (2 words)  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
000DDC:	F204	ICX 	P  	=#04	; ICX operand: #04
000DDE:	2D80	LBL 	P  	=#80	; A <- left byte of #80
000DE0:	9114	STA 	IGX	*TWB_T6,X	; [*TWB_T6+X] <- A  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
000DE2:	207A	LDA 	P  	=#7A	; A <- #7A
000DE4:	C778	BRU 	RP 	l_0ED4	; 120 words forward: jump to l_0ED4
l_0DE6:				; branch target; 1 ref; only from untraced words
000DE6:	5521	SBR 	DG 	G+0021	; right byte of [G+0021] <- A
000DE8:	C775	BRU 	RP 	l_0ED2	; 117 words forward: jump to l_0ED2
l_0DEA:				; branch target; 1 ref; only from untraced words
000DEA:	5523	SBR 	DG 	G+0023	; right byte of [G+0023] <- A
000DEC:	C773	BRU 	RP 	l_0ED2	; 115 words forward: jump to l_0ED2
l_0DEE:				; branch target; 1 ref; only from untraced words
000DEE:	4E20	LBR 	DG 	G+0020	; A <- right byte of [G+0020]
000DF0:	CD2E	BAZ 	RM 	l_0D94	; 46 words backward: conditional branch (BAZ) to l_0D94
000DF2:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000DF4:	CF47	BRU 	RM 	s_0D66	; 71 words backward: CALL subroutine s_0D66 (SPA above saved the return address); returns to l_0DF6
l_0DF6:				; branch target; 1 ref; only from untraced words
000DF6:	4228	LDX 	DG 	G+0028	; X <- [G+0028]
000DF8:	F102	XAE 	P  		; exchange A and E
000DFA:	2D80	LBL 	P  	=#80	; A <- left byte of #80
000DFC:	9614	DST 	IGX	*TWB_T6,X	; [*TWB_T6+X] <- A,E (2 words)  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
000DFE:	F202	ICX 	P  	=#02	; ICX operand: #02
000E00:	5328	STX 	DG 	G+0028	; [G+0028] <- X
000E02:	4226	LDX 	DG 	G+0026	; X <- [G+0026]
000E04:	2001	LDA 	P  	=#01	; A <- #01
000E06:	9714	ADM 	IGX	*TWB_T6,X	; add A to [*TWB_T6+X]  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
000E08:	C765	BRU 	RP 	l_0ED2	; 101 words forward: jump to l_0ED2
; Handler for %DU - Dump memory?
000E0A:	4210	LDX 	DG 	TWB_T4	; X <- [TWB_T4]
000E0C:	F307	DCX 	P  	=#07	; DCX operand: #07
000E0E:	C302	BCF 	RP 	l_0E12	; 2 words forward: conditional branch (BCF) to l_0E12
000E10:	CF59	BRU 	RM 	l_0D5E	; 89 words backward: jump to l_0D5E
l_0E12:				; branch target; 1 ref; only from untraced words
000E12:	F206	ICX 	P  	=#06	; ICX operand: #06
000E14:	5322	STX 	DG 	G+0022	; [G+0022] <- X
000E16:	C75E	BRU 	RP 	l_0ED2	; 94 words forward: jump to l_0ED2
; Handler for %AC - Activate interrupt?
000E18:	4212	LDX 	DG 	TWB_T5	; X <- [TWB_T5]
000E1A:	F201	ICX 	P  	=#01	; ICX operand: #01
000E1C:	8E14	LBR 	IGX	*TWB_T6,X	; A <- right byte of [*TWB_T6+X]  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
000E1E:	2B7A	CMP 	P  	=#7A	; compare A with #7A
000E20:	CB46	BCF 	RM 	l_0D94	; 70 words backward: conditional branch (BCF) to l_0D94
000E22:	F201	ICX 	P  	=#01	; ICX operand: #01
000E24:	5312	STX 	DG 	TWB_T5	; [TWB_T5] <- X
000E26:	4210	LDX 	DG 	TWB_T4	; X <- [TWB_T4]
000E28:	C100	BRX 	RP 	$+X	; INDEXED JUMP (BRX): X selects one of the 3 BRU entries that follow (jump / dispatch table)
l_0E2A:				; branch target; 1 ref; only from untraced words
000E2A:	CFC8	BRU 	RM 	l_0C9A	; 200 words backward: jump to l_0C9A
l_0E2C:				; branch target; 1 ref; only from untraced words
000E2C:	CFAD	BRU 	RM 	l_0CD2	; 173 words backward: jump to l_0CD2
l_0E2E:				; branch target; 1 ref; only from untraced words
000E2E:	C706	BRU 	RP 	l_0E3A	; 6 words forward: jump to l_0E3A
000E30:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000E32:	CF66	BRU 	RM 	s_0D66	; 102 words backward: CALL subroutine s_0D66 (SPA above saved the return address); returns to l_0E34
l_0E34:				; branch target; 1 ref; only from untraced words
000E34:	CC4C	BAN 	RM 	l_0D9C	; 76 words backward: conditional branch (BAN) to l_0D9C
000E36:	5122	STA 	DG 	G+0022	; [G+0022] <- A
000E38:	C74D	BRU 	RP 	l_0ED2	; 77 words forward: jump to l_0ED2
l_0E3A:				; branch target; 1 ref; only from untraced words
000E3A:	E302	DCX 	PX 	(X)	; DCX operand: [(X)]
000E3C:	6430	LEA 	IL 	*V_0030	; A <- G+0000  (effective address, memory not read)  (pointer V_0030 = #0000: address = G + #0000, G unknown)
000E3E:	A56C	ADD 	ILX	*v_006C,X	; A <- A + [G+03C6+X]  (pointer v_006C = #03C6: address = G + #03C6, G unknown)
000E40:	F102	XAE 	P  		; exchange A and E
000E42:	2201	LDX 	P  	=#01	; X <- #01
000E44:	8E12	LBR 	IGX	*TWB_T5,X	; A <- right byte of [*TWB_T5+X]  (pointer TWB_T5 lives in the calling task's area: its contents are only known at run time)
000E46:	2200	LDX 	P  	=#00	; X <- #00
000E48:	8D12	LBL 	IGX	*TWB_T5,X	; A <- left byte of [*TWB_T5+X]  (pointer TWB_T5 lives in the calling task's area: its contents are only known at run time)
000E4A:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000E4C:	D7A2	BRU 	IL 	*v_00A2	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer v_00A2 = #13EA: address = G + #13EA, G unknown)
l_0E4E:				; branch target; 1 ref; only from untraced words
000E4E:	CC5D	BAN 	RM 	l_0D94	; 93 words backward: conditional branch (BAN) to l_0D94
000E50:	F11E	CHX 	P  	
000E52:	F905	LDR 	P  	=#05	; LDR operand: #05
000E54:	5521	SBR 	DG 	G+0021	; right byte of [G+0021] <- A
000E56:	C73E	BRU 	RP 	l_0ED2	; 62 words forward: jump to l_0ED2
; Handler for %FO Foreground / %DI Display?
000E58:	C73D	BRU 	RP 	l_0ED2	; 61 words forward: jump to l_0ED2
000E5A:	4210	LDX 	DG 	TWB_T4	; X <- [TWB_T4]
000E5C:	C100	BRX 	RP 	$+X	; INDEXED JUMP (BRX): X selects one of the 1 BRU entries that follow (jump / dispatch table)
l_0E5E:				; branch target; 1 ref; only from untraced words
000E5E:	C705	BRU 	RP 	l_0E68	; 5 words forward: jump to l_0E68
000E60:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000E62:	CF81	BRU 	RM 	s_0D60	; 129 words backward: CALL subroutine s_0D60 (SPA above saved the return address); returns to l_0E64
l_0E64:				; branch target; 1 ref; only from untraced words
000E64:	5122	STA 	DG 	G+0022	; [G+0022] <- A
000E66:	CF6C	BRU 	RM 	l_0D8E	; 108 words backward: jump to l_0D8E
l_0E68:				; branch target; 1 ref; only from untraced words
000E68:	5320	STX 	DG 	G+0020	; [G+0020] <- X
000E6A:	CF6E	BRU 	RM 	l_0D8E	; 110 words backward: jump to l_0D8E
000E6C:	CFB3	BRU 	RM 	l_0D06	; 179 words backward: jump to l_0D06
s_0E6E:				; callee (section / supervisor module / SPA subroutine); 1 ref; only from untraced words
000E6E:	CFB5	BRU 	RM 	l_0D04	; 181 words backward: jump to l_0D04
; Handler for %IT - Arm/Enable interrupt?
000E70:	4210	LDX 	DG 	TWB_T4	; X <- [TWB_T4]
000E72:	C100	BRX 	RP 	$+X	; INDEXED JUMP (BRX): X selects one of the 7 BRU entries that follow (jump / dispatch table)
l_0E74:				; branch target; 1 ref; only from untraced words
000E74:	C707	BRU 	RP 	l_0E82	; 7 words forward: jump to l_0E82
l_0E76:				; branch target; 1 ref; only from untraced words
000E76:	C706	BRU 	RP 	l_0E82	; 6 words forward: jump to l_0E82
l_0E78:				; branch target; 1 ref; only from untraced words
000E78:	C705	BRU 	RP 	l_0E82	; 5 words forward: jump to l_0E82
l_0E7A:				; branch target; 1 ref; only from untraced words
000E7A:	C707	BRU 	RP 	l_0E88	; 7 words forward: jump to l_0E88
l_0E7C:				; branch target; 1 ref; only from untraced words
000E7C:	C715	BRU 	RP 	l_0EA6	; 21 words forward: jump to l_0EA6
l_0E7E:				; branch target; 1 ref; only from untraced words
000E7E:	C714	BRU 	RP 	l_0EA6	; 20 words forward: jump to l_0EA6
l_0E80:				; branch target; 1 ref; only from untraced words
000E80:	C713	BRU 	RP 	l_0EA6	; 19 words forward: jump to l_0EA6
l_0E82:				; branch target; 3 refs; only from untraced words
000E82:	2080	LDA 	P  	=#80	; A <- #80
000E84:	952C	SBR 	IGX	*G+002C,X	; right byte of [*G+002C+X] <- A  (pointer G+002C lives in the calling task's area: its contents are only known at run time)
000E86:	C726	BRU 	RP 	l_0ED2	; 38 words forward: jump to l_0ED2
l_0E88:				; branch target; 1 ref; only from untraced words
000E88:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000E8A:	CF95	BRU 	RM 	s_0D60	; 149 words backward: CALL subroutine s_0D60 (SPA above saved the return address); returns to l_0E8C
l_0E8C:				; branch target; 1 ref; only from untraced words
000E8C:	4128	LDE 	DG 	G+0028	; E <- [G+0028]
000E8E:	C003	BCT 	RP 	l_0E94	; 3 words forward: conditional branch (BCT) to l_0E94
000E90:	5126	STA 	DG 	G+0026	; [G+0026] <- A
000E92:	C702	BRU 	RP 	l_0E96	; 2 words forward: jump to l_0E96
l_0E94:				; branch target; 1 ref; only from untraced words
000E94:	5124	STA 	DG 	G+0024	; [G+0024] <- A
l_0E96:				; branch target; 1 ref; only from untraced words
000E96:	2101	LDE 	P  	=#01	; E <- #01
000E98:	5228	STE 	DG 	G+0028	; [G+0028] <- E
000E9A:	0126	LDE 	DL 	V_0026	; E <- [V_0026]
000E9C:	F102	XAE 	P  		; exchange A and E
000E9E:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000EA0:	D75A	BRU 	IL 	*V_005A	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005A = #1642: address = G + #1642, G unknown)
l_0EA2:				; branch target; 1 ref; only from untraced words
000EA2:	CC7D	BAN 	RM 	l_0DA8	; 125 words backward: conditional branch (BAN) to l_0DA8
000EA4:	C717	BRU 	RP 	l_0ED2	; 23 words forward: jump to l_0ED2
l_0EA6:				; branch target; 3 refs; only from untraced words
000EA6:	F305	DCX 	P  	=#05	; DCX operand: #05
000EA8:	F104	XAX 	P  		; exchange A and X
000EAA:	5520	SBR 	DG 	G+0020	; right byte of [G+0020] <- A
000EAC:	C713	BRU 	RP 	l_0ED2	; 19 words forward: jump to l_0ED2
l_0EAE:				; branch target; 1 ref; only from untraced words
000EAE:	530E	STX 	DG 	TWB_T3	; [TWB_T3] <- X
000EB0:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
000EB2:	A0BE	LDA 	ILX	*v_00BE,X	; A <- [G+0402+X]  (pointer v_00BE = #0402: address = G + #0402, G unknown)
000EB4:	511E	STA 	DG 	TWB_N0	; [TWB_N0] <- A
000EB6:	2001	LDA 	P  	=#01	; A <- #01
000EB8:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)
l_0EBA:				; branch target; 7 refs; only from untraced words
000EBA:	C77D	BRU 	RP 	l_0FB4	; 125 words forward: jump to l_0FB4
; Handler for %E?
000EBC:	2E0F	LBR 	P  	=#0F	; A <- right byte of #0F
000EBE:	2D20	LBL 	P  	=#20	; A <- left byte of #20
000EC0:	512C	STA 	DG 	G+002C	; [G+002C] <- A
000EC2:	2E40	LBR 	P  	=#40	; A <- right byte of #40
000EC4:	2D40	LBL 	P  	=#40	; A <- left byte of #40
000EC6:	2100	LDE 	P  	=#00	; E <- #00
000EC8:	562E	DST 	DG 	G+002E	; [G+002E] <- A,E (2 words)
000ECA:	FA04	STR 	P  	=#04	; STR operand: #04
000ECC:	5632	DST 	DG 	G+0032	; [G+0032] <- A,E (2 words)
l_0ECE:				; branch target; 9 refs; only from untraced words
000ECE:	2061	LDA 	P  	=#61	; A <- #61
000ED0:	C702	BRU 	RP 	l_0ED4	; 2 words forward: jump to l_0ED4
l_0ED2:				; branch target; 11 refs; only from untraced words
000ED2:	206B	LDA 	P  	=#6B	; A <- #6B
l_0ED4:				; branch target; 2 refs; only from untraced words
000ED4:	4212	LDX 	DG 	TWB_T5	; X <- [TWB_T5]
000ED6:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000ED8:	D7A4	BRU 	IL 	*v_00A4	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer v_00A4 = #1418: address = G + #1418, G unknown)
l_0EDA:				; branch target; 1 ref; only from untraced words
000EDA:	5312	STX 	DG 	TWB_T5	; [TWB_T5] <- X
000EDC:	6430	LEA 	IL 	*V_0030	; A <- G+0000  (effective address, memory not read)  (pointer V_0030 = #0000: address = G + #0000, G unknown)
000EDE:	420E	LDX 	DG 	TWB_T3	; X <- [TWB_T3]
000EE0:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
000EE2:	A56C	ADD 	ILX	*v_006C,X	; A <- A + [G+03C6+X]  (pointer v_006C = #03C6: address = G + #03C6, G unknown)
000EE4:	2200	LDX 	P  	=#00	; X <- #00
000EE6:	D7C4	BRU 	IL 	*v_00C4	; jump to G+#0BB8 (G unknown)  (pointer v_00C4 = #0BB8: address = G + #0BB8, G unknown)
; Handler for %EO ?
000EE8:	2000	LDA 	P  	=#00	; A <- #00
000EEA:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
; Handler for %F ?
000EEC:	4420	LEA 	DG 	G+0020	; A <- G+0020  (effective address, memory not read)
000EEE:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
000EF0:	220F	LDX 	P  	=#0F	; X <- #0F
000EF2:	2000	LDA 	P  	=#00	; A <- #00
l_0EF4:				; branch target; 1 ref; only from untraced words
000EF4:	9508	SBR 	IGX	*TWB_T0,X	; right byte of [*TWB_T0+X] <- A
000EF6:	F301	DCX 	P  	=#01	; DCX operand: #01
000EF8:	C802	BCT 	RM 	l_0EF4	; 2 words backward: conditional branch (BCT) to l_0EF4
000EFA:	CF16	BRU 	RM 	l_0ECE	; 22 words backward: jump to l_0ECE
; Handler for %FC ?
000EFC:	2E0E	LBR 	P  	=#0E	; A <- right byte of #0E
000EFE:	2D21	LBL 	P  	=#21	; A <- left byte of #21
000F00:	512C	STA 	DG 	G+002C	; [G+002C] <- A
000F02:	2040	LDA 	P  	=#40	; A <- #40
000F04:	2D40	LBL 	P  	=#40	; A <- left byte of #40
000F06:	2100	LDE 	P  	=#00	; E <- #00
000F08:	562E	DST 	DG 	G+002E	; [G+002E] <- A,E (2 words)
000F0A:	FA04	STR 	P  	=#04	; STR operand: #04
000F0C:	5632	DST 	DG 	G+0032	; [G+0032] <- A,E (2 words)
000F0E:	2061	LDA 	P  	=#61	; A <- #61
000F10:	4212	LDX 	DG 	TWB_T5	; X <- [TWB_T5]
000F12:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000F14:	D7A4	BRU 	IL 	*v_00A4	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer v_00A4 = #1418: address = G + #1418, G unknown)
l_0F16:				; branch target; 1 ref; only from untraced words
000F16:	F301	DCX 	P  	=#01	; DCX operand: #01
000F18:	8E14	LBR 	IGX	*TWB_T6,X	; A <- right byte of [*TWB_T6+X]  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
000F1A:	2B61	CMP 	P  	=#61	; compare A with #61
000F1C:	D3C2	BCF 	IL 	*v_00C2	; conditional branch, target not known statically  (pointer v_00C2 = #0D94: address = G + #0D94, G unknown)
000F1E:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000F20:	CF59	BRU 	RM 	s_0E6E	; 89 words backward: CALL subroutine s_0E6E (SPA above saved the return address); returns to l_0F22
l_0F22:				; branch target; 1 ref; only from untraced words
000F22:	8E14	LBR 	IGX	*TWB_T6,X	; A <- right byte of [*TWB_T6+X]  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
000F24:	2B61	CMP 	P  	=#61	; compare A with #61
000F26:	D3C2	BCF 	IL 	*v_00C2	; conditional branch, target not known statically  (pointer v_00C2 = #0D94: address = G + #0D94, G unknown)
000F28:	F201	ICX 	P  	=#01	; ICX operand: #01
000F2A:	2000	LDA 	P  	=#00	; A <- #00
000F2C:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
; Handler for %RO ?
000F2E:	2000	LDA 	P  	=#00	; A <- #00
000F30:	5120	STA 	DG 	G+0020	; [G+0020] <- A
000F32:	5122	STA 	DG 	G+0022	; [G+0022] <- A
000F34:	CF33	BRU 	RM 	l_0ECE	; 51 words backward: jump to l_0ECE
000F36:	2004	LDA 	P  	=#04	; A <- #04
l_0F38:				; branch target; 1 ref; only from untraced words
000F38:	2100	LDE 	P  	=#00	; E <- #00
000F3A:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
000F3C:	CF37	BRU 	RM 	l_0ECE	; 55 words backward: jump to l_0ECE
000F3E:	2006	LDA 	P  	=#06	; A <- #06
000F40:	CF04	BRU 	RM 	l_0F38	; 4 words backward: jump to l_0F38
000F42:	0024	LDA 	DL 	V_0024	; A <- [V_0024]
000F44:	0622	SUB 	DL 	V_0022	; A <- A - [V_0022]
000F46:	5120	STA 	DG 	G+0020	; [G+0020] <- A
000F48:	CF3D	BRU 	RM 	l_0ECE	; 61 words backward: jump to l_0ECE
000F4A:	2100	LDE 	P  	=#00	; E <- #00
000F4C:	2000	LDA 	P  	=#00	; A <- #00
000F4E:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
000F50:	5624	DST 	DG 	G+0024	; [G+0024] <- A,E (2 words)
000F52:	5128	STA 	DG 	G+0028	; [G+0028] <- A
000F54:	4420	LEA 	DG 	G+0020	; A <- G+0020  (effective address, memory not read)
000F56:	512C	STA 	DG 	G+002C	; [G+002C] <- A
000F58:	CF45	BRU 	RM 	l_0ECE	; 69 words backward: jump to l_0ECE
000F5A:	2004	LDA 	P  	=#04	; A <- #04
000F5C:	C702	BRU 	RP 	l_0F60	; 2 words forward: jump to l_0F60
000F5E:	2000	LDA 	P  	=#00	; A <- #00
l_0F60:				; branch target; 1 ref; only from untraced words
000F60:	5120	STA 	DG 	G+0020	; [G+0020] <- A
000F62:	2000	LDA 	P  	=#00	; A <- #00
000F64:	2100	LDE 	P  	=#00	; E <- #00
000F66:	5622	DST 	DG 	G+0022	; [G+0022] <- A,E (2 words)
000F68:	5126	STA 	DG 	G+0026	; [G+0026] <- A
000F6A:	4428	LEA 	DG 	G+0028	; A <- G+0028  (effective address, memory not read)
l_0F6C:				; branch target; 1 ref; only from untraced words
000F6C:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
000F6E:	2206	LDX 	P  	=#06	; X <- #06
000F70:	2E40	LBR 	P  	=#40	; A <- right byte of #40
l_0F72:				; branch target; 1 ref; only from untraced words
000F72:	F301	DCX 	P  	=#01	; DCX operand: #01
000F74:	CB53	BCF 	RM 	l_0ECE	; 83 words backward: conditional branch (BCF) to l_0ECE
000F76:	9508	SBR 	IGX	*TWB_T0,X	; right byte of [*TWB_T0+X] <- A
000F78:	CF03	BRU 	RM 	l_0F72	; 3 words backward: jump to l_0F72
000F7A:	2000	LDA 	P  	=#00	; A <- #00
000F7C:	2100	LDE 	P  	=#00	; E <- #00
000F7E:	5626	DST 	DG 	G+0026	; [G+0026] <- A,E (2 words)
000F80:	562A	DST 	DG 	G+002A	; [G+002A] <- A,E (2 words)
000F82:	512E	STA 	DG 	G+002E	; [G+002E] <- A
000F84:	4420	LEA 	DG 	G+0020	; A <- G+0020  (effective address, memory not read)
000F86:	CF0D	BRU 	RM 	l_0F6C	; 13 words backward: jump to l_0F6C
000F88:	C7DD	BRU 	RP 	l_1142	; 221 words forward: jump to l_1142
000F8A:	2000	LDA 	P  	=#00	; A <- #00
000F8C:	2100	LDE 	P  	=#00	; E <- #00
000F8E:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
000F90:	4206	LDX 	DG 	TWB_ZC	; X <- [TWB_ZC]
000F92:	F250	ICX 	P  	=#50	; ICX operand: #50
000F94:	F908	LDR 	P  	=#08	; LDR operand: #08
000F96:	C303	BCF 	RP 	l_0F9C	; 3 words forward: conditional branch (BCF) to l_0F9C
000F98:	F2FF	ICX 	P  	=#FF	; ICX operand: #FF
000F9A:	F287	ICX 	P  	=#87	; ICX operand: #87
l_0F9C:				; branch target; 1 ref; only from untraced words
000F9C:	5324	STX 	DG 	G+0024	; [G+0024] <- X
000F9E:	5326	STX 	DG 	G+0026	; [G+0026] <- X
000FA0:	5328	STX 	DG 	G+0028	; [G+0028] <- X
000FA2:	2000	LDA 	P  	=#00	; A <- #00
000FA4:	2D80	LBL 	P  	=#80	; A <- left byte of #80
000FA6:	9114	STA 	IGX	*TWB_T6,X	; [*TWB_T6+X] <- A  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
000FA8:	CF6D	BRU 	RM 	l_0ECE	; 109 words backward: jump to l_0ECE
; Handler for %FB / %SP ?
000FAA:	CF6E	BRU 	RM 	l_0ECE	; 110 words backward: jump to l_0ECE
000FAC:	2100	LDE 	P  	=#00	; E <- #00
000FAE:	2001	LDA 	P  	=#01	; A <- #01
000FB0:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
000FB2:	CF72	BRU 	RM 	l_0ECE	; 114 words backward: jump to l_0ECE
; =====================================================================
; MAIN COMMAND DISPATCH & ARGUMENT PARSING (0FB6 - 1148)
; =====================================================================
; This block is reached after the command mnemonic has been identified.
; It parses the arguments following the '/' delimiter and dispatches
; to the appropriate supervisor module based on the command letter.
;
; The parser uses a state machine (TOK_TYPE at MAE_parsing_state) to track:
;   - Bit 0: Continuation / special syntax flag (e.g., '@' for address)
;   - Bits 4-7: Argument type / delimiter state
;
; TWB Slots used for argument parsing:
;   TWB_T10(&24) = CMD_PTR   : cursor into CMD_BUF
;   TWB_T11(&26) = CMD_BUF   : pointer to current character
;   TWB_T14(&2C) = TOK_TYPE  : token type / state flags
;   TWB_T15(&2E) = TOK_VAL   : token value / scratch
;   TWB_T8 (&20) = ARG_ACC   : argument accumulator
;   TWB_T9 (&22) = ARG_SCR   : argument scratch
;   v_0040(&40) = ARG_BASE   : base address for argument
;   v_0048(&48) = ARG_PTR    : pointer to argument buffer
;   v_0050(&50) = ARG_LEN    : argument length / count
;   MAE_indirect_dispatch(&54) = PARSE_RT   : return address for sub-parsers
;   MAE_parsing_state(&58) = TOK_TYPE   : token type / state flags
;   v_005C(&5C) = TOK_VAL    : token value
;   v_0060(&60) = DISP_TAB   : dispatch table base
;   v_006C(&6C) = ARG_IDX    : argument index
; =====================================================================

; --- Entry Point from Command Letter Case Ladder ---
; After the initial mnemonic is recognized by the case ladder, control transfers to MAE_state_machine, which acts as a sophisticated state machine for parsing arguments (like hex literals, addresses, or device labels) and dispatching to the specific supervisor modules (M_LOAD, M_DUMP, M_EDIT, etc.).
MAE_state_machine:				; branch target; 12 refs
l_0FB4:				; branch target; 12 refs; only from untraced words
000FB4:	FA04	STR 	P  	=#04	; STR operand: #04
000FB6:	2000	LDA 	P  	=#00	; A <- #00
000FB8:	FA01	STR 	P  	=#01	; STR operand: #01
000FBA:	F904	LDR 	P  	=#04	; LDR operand: #04

; ===== MOD_0FBC: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N2, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: stores into the caller's TWB, returns to its CSV caller with RSV
MOD_0FBC:				; entry point; 1 ref
000FBC:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000FBE:	D75E	BRU 	IL 	*V_005E	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005E = #14F0: address = G + #14F0, G unknown)
L_0FC0:				; branch target; 1 ref
000FC0:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
000FC2:	4E2C	LBR 	DG 	G+002C	; A <- right byte of [G+002C]
000FC4:	2901	AND 	P  	=#01	; A <- A and #01
000FC6:	C508	BAZ 	RP 	l_0FD6	; 8 words forward: conditional branch (BAZ) to l_0FD6
000FC8:	415C	LDE 	DG 	G+005C	; E <- [G+005C]
000FCA:	4430	LEA 	DG 	G+0030	; A <- G+0030  (effective address, memory not read)
000FCC:	2F0E	LBX 	P  	=#0E	; LBX operand: #0E
000FCE:	5818	SPA 	DG 	TWB_N3	; [TWB_N3] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000FD0:	D7D0	BRU 	IL 	*v_00D0	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer v_00D0 = #19E8: address = G + #19E8, G unknown)
l_0FD2:				; branch target; 1 ref; only from untraced words
000FD2:	C4AC	BAN 	RP 	l_112A	; 172 words forward: conditional branch (BAN) to l_112A
000FD4:	C717	BRU 	RP 	l_1002	; 23 words forward: jump to l_1002
l_0FD6:				; branch target; 1 ref; only from untraced words
000FD6:	2100	LDE 	P  	=#00	; E <- #00
000FD8:	522A	STE 	DG 	G+002A	; [G+002A] <- E
000FDA:	2200	LDX 	P  	=#00	; X <- #00
000FDC:	5233	STE 	DG 	G+0033	; [G+0033] <- E
000FDE:	2180	LDE 	P  	=#80	; E <- #80
000FE0:	4E2D	LBR 	DG 	G+002D	; A <- right byte of [G+002D]
000FE2:	2B01	CMP 	P  	=#01	; compare A with #01
000FE4:	C004	BCT 	RP 	l_0FEC	; 4 words forward: conditional branch (BCT) to l_0FEC
000FE6:	21A0	LDE 	P  	=#A0	; E <- #A0
000FE8:	2D10	LBL 	P  	=#10	; A <- left byte of #10
000FEA:	2288	LDX 	P  	=#88	; X <- #88
l_0FEC:				; branch target; 1 ref; only from untraced words
000FEC:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
000FEE:	405C	LDA 	DG 	G+005C	; A <- [G+005C]
000FF0:	5124	STA 	DG 	G+0024	; [G+0024] <- A
000FF2:	F104	XAX 	P  		; exchange A and X
000FF4:	2578	ADD 	P  	=#78	; A <- A + #78
000FF6:	5126	STA 	DG 	G+0026	; [G+0026] <- A
000FF8:	5818	SPA 	DG 	TWB_N3	; [TWB_N3] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
000FFA:	D72A	BRU 	IL 	*V_002A	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_002A = #0AB8: address = G + #0AB8, G unknown)
l_0FFC:				; branch target; 1 ref; only from untraced words
000FFC:	C44E	BAN 	RP 	l_1098	; 78 words forward: conditional branch (BAN) to l_1098
000FFE:	425C	LDX 	DG 	G+005C	; X <- [G+005C]
001000:	F204	ICX 	P  	=#04	; ICX operand: #04
l_1002:				; branch target; 1 ref; only from untraced words
001002:	5336	STX 	DG 	G+0036	; [G+0036] <- X
001004:	2208	LDX 	P  	=#08	; X <- #08
001006:	8E36	LBR 	IGX	*G+0036,X	; A <- right byte of [*G+0036+X]  (pointer G+0036 lives in the calling task's area: its contents are only known at run time)
001008:	2B01	CMP 	P  	=#01	; compare A with #01
00100A:	C003	BCT 	RP 	l_1010	; 3 words forward: conditional branch (BCT) to l_1010
00100C:	2B03	CMP 	P  	=#03	; compare A with #03
00100E:	C38C	BCF 	RP 	l_1126	; 140 words forward: conditional branch (BCF) to l_1126
l_1010:				; branch target; 1 ref; only from untraced words
001010:	5540	SBR 	DG 	G+0040	; right byte of [G+0040] <- A
001012:	2000	LDA 	P  	=#00	; A <- #00
001014:	FA01	STR 	P  	=#01	; STR operand: #01
001016:	2204	LDX 	P  	=#04	; X <- #04
001018:	4036	LDA 	DG 	G+0036	; A <- [G+0036]
00101A:	.EBCDIC	"j; ;"	; 2 words, raw: 915E 405E
00101E:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001020:	D750	BRU 	IL 	*V_0050	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0050 = #051A: address = G + #051A, G unknown)
l_1022:				; branch target; 1 ref; only from untraced words
001022:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001024:	D752	BRU 	IL 	*V_0052	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0052 = #0844: address = G + #0844, G unknown)
l_1026:				; branch target; 1 ref; only from untraced words
001026:	C47C	BAN 	RP 	l_111E	; 124 words forward: conditional branch (BAN) to l_111E
001028:	4E2C	LBR 	DG 	G+002C	; A <- right byte of [G+002C]
00102A:	2902	AND 	P  	=#02	; A <- A and #02
00102C:	5541	SBR 	DG 	G+0041	; right byte of [G+0041] <- A
00102E:	016E	LDE 	DL 	v_006E	; E <- [v_006E]
001030:	0224	LDX 	DL 	V_0024	; X <- [V_0024]
001032:	0224	LDX 	DL 	V_0024	; X <- [V_0024]
001034:	C503	BAZ 	RP 	l_103A	; 3 words forward: conditional branch (BAZ) to l_103A
001036:	0122	LDE 	DL 	V_0022	; E <- [V_0022]
001038:	026E	LDX 	DL 	v_006E	; X <- [v_006E]
l_103A:				; branch target; 1 ref; only from untraced words
00103A:	520A	STE 	DG 	TWB_T1	; [TWB_T1] <- E
00103C:	530C	STX 	DG 	TWB_T2	; [TWB_T2] <- X
00103E:	5342	STX 	DG 	G+0042	; [G+0042] <- X
001040:	4E2C	LBR 	DG 	G+002C	; A <- right byte of [G+002C]
001042:	2904	AND 	P  	=#04	; A <- A and #04
001044:	C502	BAZ 	RP 	l_1048	; 2 words forward: conditional branch (BAZ) to l_1048
001046:	C704	BRU 	RP 	l_104E	; 4 words forward: jump to l_104E
l_1048:				; branch target; 1 ref; only from untraced words
001048:	400A	LDA 	DG 	TWB_T1	; A <- [TWB_T1]
00104A:	572E	ADM 	DG 	G+002E	; add A to [G+002E]
00104C:	C06B	BCT 	RP 	l_1122	; 107 words forward: conditional branch (BCT) to l_1122
l_104E:				; branch target; 1 ref; only from untraced words
00104E:	410A	LDE 	DG 	TWB_T1	; E <- [TWB_T1]
001050:	402E	LDA 	DG 	G+002E	; A <- [G+002E]
001052:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001054:	D75A	BRU 	IL 	*V_005A	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005A = #1642: address = G + #1642, G unknown)
l_1056:				; branch target; 1 ref; only from untraced words
001056:	C466	BAN 	RP 	l_1122	; 102 words forward: conditional branch (BAN) to l_1122
001058:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
00105A:	C703	BRU 	RP 	s_1060	; 3 words forward: CALL subroutine s_1060 (SPA above saved the return address); returns to l_105C
l_105C:				; branch target; 1 ref; only from untraced words
00105C:	C41F	BAN 	RP 	l_109A	; 31 words forward: conditional branch (BAN) to l_109A
00105E:	C714	BRU 	RP 	l_1086	; 20 words forward: jump to l_1086
s_1060:				; callee (section / supervisor module / SPA subroutine); 2 refs; only from untraced words
001060:	220A	LDX 	P  	=#0A	; X <- #0A
001062:	9036	DLD 	IGX	*G+0036,X	; A,E <- [*G+0036+X] (2 words)  (pointer G+0036 lives in the calling task's area: its contents are only known at run time)
001064:	523A	STE 	DG 	G+003A	; [G+003A] <- E
001066:	512A	STA 	DG 	G+002A	; [G+002A] <- A
001068:	2206	LDX 	P  	=#06	; X <- #06
00106A:	9036	DLD 	IGX	*G+0036,X	; A,E <- [*G+0036+X] (2 words)  (pointer G+0036 lives in the calling task's area: its contents are only known at run time)
00106C:	5630	DST 	DG 	G+0030	; [G+0030] <- A,E (2 words)
00106E:	2000	LDA 	P  	=#00	; A <- #00
001070:	4D31	LBL 	DG 	G+0031	; A <- left byte of [G+0031]
001072:	4F30	LBX 	DG 	G+0030	; LBX operand: [G+0030]
001074:	C302	BCF 	RP 	l_1078	; 2 words forward: conditional branch (BCF) to l_1078
001076:	403A	LDA 	DG 	G+003A	; A <- [G+003A]
l_1078:				; branch target; 1 ref; only from untraced words
001078:	452E	ADD 	DG 	G+002E	; A <- A + [G+002E]
00107A:	C054	BCT 	RP 	l_1122	; 84 words forward: conditional branch (BCT) to l_1122
00107C:	410C	LDE 	DG 	TWB_T2	; E <- [TWB_T2]
00107E:	F102	XAE 	P  		; exchange A and E
001080:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001082:	D75A	BRU 	IL 	*V_005A	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005A = #1642: address = G + #1642, G unknown)
l_1084:				; branch target; 1 ref; only from untraced words
001084:	DF1C	BRU 	IG 	*TWB_N1	; RETURN from subroutine: branch through return cell TWB_N1 (written by the SPA of the call)
l_1086:				; branch target; 2 refs; only from untraced words
001086:	4E2C	LBR 	DG 	G+002C	; A <- right byte of [G+002C]
001088:	F0C3	SRLS	P  	#03
00108A:	5531	SBR 	DG 	G+0031	; right byte of [G+0031] <- A
00108C:	6430	LEA 	IL 	*V_0030	; A <- G+0000  (effective address, memory not read)  (pointer V_0030 = #0000: address = G + #0000, G unknown)
00108E:	452E	ADD 	DG 	G+002E	; A <- A + [G+002E]
001090:	5138	STA 	DG 	G+0038	; [G+0038] <- A
001092:	5024	DLD 	DG 	G+0024	; A,E <- [G+0024] (2 words)
001094:	562C	DST 	DG 	G+002C	; [G+002C] <- A,E (2 words)
001096:	2000	LDA 	P  	=#00	; A <- #00
l_1098:				; branch target; 1 ref; only from untraced words
001098:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
l_109A:				; branch target; 1 ref; only from untraced words
00109A:	2208	LDX 	P  	=#08	; X <- #08
00109C:	8E36	LBR 	IGX	*G+0036,X	; A <- right byte of [*G+0036+X]  (pointer G+0036 lives in the calling task's area: its contents are only known at run time)
00109E:	2B03	CMP 	P  	=#03	; compare A with #03
0010A0:	C341	BCF 	RP 	l_1122	; 65 words forward: conditional branch (BCF) to l_1122
0010A2:	4E2C	LBR 	DG 	G+002C	; A <- right byte of [G+002C]
0010A4:	2904	AND 	P  	=#04	; A <- A and #04
0010A6:	C53E	BAZ 	RP 	l_1122	; 62 words forward: conditional branch (BAZ) to l_1122
0010A8:	0166	LDE 	DL 	L_0066	; E <- [L_0066]
0010AA:	402E	LDA 	DG 	G+002E	; A <- [G+002E]
0010AC:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0010AE:	D75A	BRU 	IL 	*V_005A	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005A = #1642: address = G + #1642, G unknown)
l_10B0:				; branch target; 1 ref; only from untraced words
0010B0:	C439	BAN 	RP 	l_1122	; 57 words forward: conditional branch (BAN) to l_1122
0010B2:	0026	LDA 	DL 	V_0026	; A <- [V_0026]
0010B4:	510C	STA 	DG 	TWB_T2	; [TWB_T2] <- A
0010B6:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0010B8:	CF2C	BRU 	RM 	s_1060	; 44 words backward: CALL subroutine s_1060 (SPA above saved the return address); returns to l_10BA
l_10BA:				; branch target; 1 ref; only from untraced words
0010BA:	C434	BAN 	RP 	l_1122	; 52 words forward: conditional branch (BAN) to l_1122
0010BC:	CF1B	BRU 	RM 	l_1086	; 27 words backward: jump to l_1086
0010BE:	F902	LDR 	P  	=#02	; LDR operand: #02
0010C0:	4558	ADD 	DG 	G+0058	; A <- A + [G+0058]
0010C2:	FA01	STR 	P  	=#01	; STR operand: #01
0010C4:	0008	LDA 	DL 	V_0008	; A <- [V_0008]
0010C6:	5726	ADM 	DG 	G+0026	; add A to [G+0026]
0010C8:	C03F	BCT 	RP 	l_1146	; 63 words forward: conditional branch (BCT) to l_1146
0010CA:	0008	LDA 	DL 	V_0008	; A <- [V_0008]
0010CC:	5728	ADM 	DG 	G+0028	; add A to [G+0028]
0010CE:	C03C	BCT 	RP 	l_1146	; 60 words forward: conditional branch (BCT) to l_1146
0010D0:	4E22	LBR 	DG 	G+0022	; A <- right byte of [G+0022]
0010D2:	C513	BAZ 	RP 	l_10F8	; 19 words forward: conditional branch (BAZ) to l_10F8
0010D4:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
0010D6:	0E0E	LBR 	DL 	PRTS_COUNT	; A <- right byte of [PRTS_COUNT]
0010D8:	4608	SUB 	DG 	TWB_T0	; A <- A - [TWB_T0]
0010DA:	C334	BCF 	RP 	l_1142	; 52 words forward: conditional branch (BCF) to l_1142
0010DC:	4008	LDA 	DG 	TWB_T0	; A <- [TWB_T0]
0010DE:	F002	SLLS	P  	#02
0010E0:	F11C	CNA 	P  	
0010E2:	0508	ADD 	DL 	V_0008	; A <- A + [V_0008]
0010E4:	110C	STA 	DL 	PRTS_PTR	; [PRTS_PTR] <- A
0010E6:	600C	LDA 	IL 	*PRTS_PTR	; A <- [*PRTS_PTR]
0010E8:	4526	ADD 	DG 	G+0026	; A <- A + [G+0026]
0010EA:	C02E	BCT 	RP 	l_1146	; 46 words forward: conditional branch (BCT) to l_1146
0010EC:	110A	STA 	DL 	CPT_PTR	; [CPT_PTR] <- A
0010EE:	2202	LDX 	P  	=#02	; X <- #02
0010F0:	A00C	LDA 	ILX	*PRTS_PTR,X	; A <- [*PRTS_PTR+X]
0010F2:	4528	ADD 	DG 	G+0028	; A <- A + [G+0028]
0010F4:	C029	BCT 	RP 	l_1146	; 41 words forward: conditional branch (BCT) to l_1146
0010F6:	C70A	BRU 	RP 	l_110A	; 10 words forward: jump to l_110A
l_10F8:				; branch target; 1 ref; only from untraced words
0010F8:	4E23	LBR 	DG 	G+0023	; A <- right byte of [G+0023]
0010FA:	2901	AND 	P  	=#01	; A <- A and #01
0010FC:	C503	BAZ 	RP 	l_1102	; 3 words forward: conditional branch (BAZ) to l_1102
0010FE:	4026	LDA 	DG 	G+0026	; A <- [G+0026]
001100:	110A	STA 	DL 	CPT_PTR	; [CPT_PTR] <- A
l_1102:				; branch target; 1 ref; only from untraced words
001102:	4E23	LBR 	DG 	G+0023	; A <- right byte of [G+0023]
001104:	2902	AND 	P  	=#02	; A <- A and #02
001106:	C503	BAZ 	RP 	l_110C	; 3 words forward: conditional branch (BAZ) to l_110C
001108:	4028	LDA 	DG 	G+0028	; A <- [G+0028]
l_110A:				; branch target; 1 ref; only from untraced words
00110A:	110C	STA 	DL 	PRTS_PTR	; [PRTS_PTR] <- A
l_110C:				; branch target; 1 ref; only from untraced words
00110C:	2000	LDA 	P  	=#00	; A <- #00
00110E:	FA01	STR 	P  	=#01	; STR operand: #01
001110:	F908	LDR 	P  	=#08	; LDR operand: #08
001112:	C505	BAZ 	RP 	l_111C	; 5 words forward: conditional branch (BAZ) to l_111C
001114:	2000	LDA 	P  	=#00	; A <- #00
001116:	4F20	LBX 	DG 	G+0020	; LBX operand: [G+0020]
001118:	C302	BCF 	RP 	l_111C	; 2 words forward: conditional branch (BCF) to l_111C
00111A:	15D4	SBR 	DL 	V_00D4	; right byte of [V_00D4] <- A
l_111C:				; branch target; 2 refs; only from untraced words
00111C:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
; =====================================================================
; COMMAND LETTER CASE LADDER (111E - 1148)
; =====================================================================
; Once a full command mnemonic is scanned, this ladder dispatches to 
; the specific command handler based on the first letter.
; Codes 041-047 correspond to EBCDIC letters 'A'-'G'.
; All paths jump backward to MAE_state_machine, which is the main command dispatch.
; =====================================================================
l_111E:				; branch target; 1 ref; only from untraced words
00111E:	2D21	LBL 	P  	=#21	; A <- left byte of #21
001120:	CFB6	BRU 	RM 	l_0FB4	; 182 words backward: jump to l_0FB4
l_1122:				; branch target; 7 refs; only from untraced words
001122:	2D25	LBL 	P  	=#25	; A <- left byte of #25
001124:	CFB8	BRU 	RM 	l_0FB4	; 184 words backward: jump to l_0FB4
l_1126:				; branch target; 1 ref; only from untraced words
001126:	2D26	LBL 	P  	=#26	; A <- left byte of #26
001128:	CFBA	BRU 	RM 	l_0FB4	; 186 words backward: jump to l_0FB4
l_112A:				; branch target; 1 ref; only from untraced words
00112A:	2D27	LBL 	P  	=#27	; A <- left byte of #27
00112C:	CFBC	BRU 	RM 	l_0FB4	; 188 words backward: jump to l_0FB4
l_112E:				; branch target; 4 refs; only from untraced words
00112E:	2D41	LBL 	P  	=#41	; A <- left byte of #41
001130:	CFBE	BRU 	RM 	l_0FB4	; 190 words backward: jump to l_0FB4
l_1132:				; branch target; 3 refs; only from untraced words
001132:	2D42	LBL 	P  	=#42	; A <- left byte of #42
001134:	CFC0	BRU 	RM 	l_0FB4	; 192 words backward: jump to l_0FB4
l_1136:				; branch target; 3 refs; only from untraced words
001136:	2D43	LBL 	P  	=#43	; A <- left byte of #43
001138:	CFC2	BRU 	RM 	l_0FB4	; 194 words backward: jump to l_0FB4
l_113A:				; branch target; 1 ref; only from untraced words
00113A:	2D44	LBL 	P  	=#44	; A <- left byte of #44
00113C:	CFC4	BRU 	RM 	l_0FB4	; 196 words backward: jump to l_0FB4
00113E:	2D45	LBL 	P  	=#45	; A <- left byte of #45
001140:	CFC6	BRU 	RM 	l_0FB4	; 198 words backward: jump to l_0FB4
l_1142:				; branch target; 2 refs; only from untraced words
001142:	2D46	LBL 	P  	=#46	; A <- left byte of #46
001144:	CFC8	BRU 	RM 	l_0FB4	; 200 words backward: jump to l_0FB4
l_1146:				; branch target; 4 refs; only from untraced words
001146:	2D47	LBL 	P  	=#47	; A <- left byte of #47
001148:	CFCA	BRU 	RM 	l_0FB4	; 202 words backward: jump to l_0FB4
00114A:	4020	LDA 	DG 	G+0020	; A <- [G+0020]
00114C:	5720	ADM 	DG 	G+0020	; add A to [G+0020]
00114E:	F908	LDR 	P  	=#08	; LDR operand: #08
001150:	4B20	CMP 	DG 	G+0020	; compare A with [G+0020]
001152:	C812	BCT 	RM 	l_112E	; 18 words backward: conditional branch (BCT) to l_112E
001154:	4220	LDX 	DG 	G+0020	; X <- [G+0020]
001156:	A00A	LDA 	ILX	*CPT_PTR,X	; A <- [G+0380+X]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
001158:	0BE8	CMP 	DL 	V_00E8	; compare A with [V_00E8]
00115A:	C814	BCT 	RM 	l_1132	; 20 words backward: conditional branch (BCT) to l_1132
00115C:	0BE6	CMP 	DL 	V_00E6	; compare A with [V_00E6]
00115E:	C816	BCT 	RM 	l_1132	; 22 words backward: conditional branch (BCT) to l_1132
001160:	F11E	CHX 	P  	
001162:	F905	LDR 	P  	=#05	; LDR operand: #05
001164:	C302	BCF 	RP 	l_1168	; 2 words forward: conditional branch (BCF) to l_1168
001166:	15DD	SBR 	DL 	V_00DC	; right byte of [V_00DC] <- A
l_1168:				; branch target; 1 ref; only from untraced words
001168:	C765	BRU 	RP 	l_1232	; 101 words forward: jump to l_1232
00116A:	0022	LDA 	DL 	V_0022	; A <- [V_0022]
00116C:	4520	ADD 	DG 	G+0020	; A <- A + [G+0020]
00116E:	C820	BCT 	RM 	l_112E	; 32 words backward: conditional branch (BCT) to l_112E
001170:	510A	STA 	DG 	TWB_T1	; [TWB_T1] <- A
001172:	0124	LDE 	DL 	V_0024	; E <- [V_0024]
001174:	F102	XAE 	P  		; exchange A and E
001176:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001178:	D75A	BRU 	IL 	*V_005A	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005A = #1642: address = G + #1642, G unknown)
l_117A:				; branch target; 1 ref; only from untraced words
00117A:	CC26	BAN 	RM 	l_112E	; 38 words backward: conditional branch (BAN) to l_112E
00117C:	400A	LDA 	DG 	TWB_T1	; A <- [TWB_T1]
00117E:	016E	LDE 	DL 	v_006E	; E <- [v_006E]
001180:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001182:	D75A	BRU 	IL 	*V_005A	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005A = #1642: address = G + #1642, G unknown)
l_1184:				; branch target; 1 ref; only from untraced words
001184:	C404	BAN 	RP 	l_118C	; 4 words forward: conditional branch (BAN) to l_118C
001186:	600A	LDA 	IL 	*CPT_PTR	; A <- [G+0380]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
001188:	0BE6	CMP 	DL 	V_00E6	; compare A with [V_00E6]
00118A:	CB2C	BCF 	RM 	l_1132	; 44 words backward: conditional branch (BCF) to l_1132
l_118C:				; branch target; 1 ref; only from untraced words
00118C:	400A	LDA 	DG 	TWB_T1	; A <- [TWB_T1]
00118E:	116E	STA 	DL 	v_006E	; [v_006E] <- A
001190:	C73A	BRU 	RP 	l_1204	; 58 words forward: jump to l_1204
001192:	2200	LDX 	P  	=#00	; X <- #00
001194:	5328	STX 	DG 	G+0028	; [G+0028] <- X
l_1196:				; branch target; 1 ref; only from untraced words
001196:	4128	LDE 	DG 	G+0028	; E <- [G+0028]
001198:	2200	LDX 	P  	=#00	; X <- #00
00119A:	925C	STE 	IGX	*G+005C,X	; [*G+005C+X] <- E  (pointer G+005C lives in the calling task's area: its contents are only known at run time)
00119C:	F106	XEX 	P  		; exchange E and X
00119E:	A00A	LDA 	ILX	*CPT_PTR,X	; A <- [G+0380+X]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
0011A0:	0BE8	CMP 	DL 	V_00E8	; compare A with [V_00E8]
0011A2:	C01B	BCT 	RP 	l_11D8	; 27 words forward: conditional branch (BCT) to l_11D8
0011A4:	0BE6	CMP 	DL 	V_00E6	; compare A with [V_00E6]
0011A6:	C019	BCT 	RP 	l_11D8	; 25 words forward: conditional branch (BCT) to l_11D8
0011A8:	FA01	STR 	P  	=#01	; STR operand: #01
0011AA:	0E0E	LBR 	DL 	PRTS_COUNT	; A <- right byte of [PRTS_COUNT]
0011AC:	F002	SLLS	P  	#02
0011AE:	F11C	CNA 	P  	
0011B0:	0508	ADD 	DL 	V_0008	; A <- A + [V_0008]
0011B2:	F102	XAE 	P  		; exchange A and E
0011B4:	0008	LDA 	DL 	V_0008	; A <- [V_0008]
0011B6:	2202	LDX 	P  	=#02	; X <- #02
0011B8:	965C	DST 	IGX	*G+005C,X	; [*G+005C+X] <- A,E (2 words)  (pointer G+005C lives in the calling task's area: its contents are only known at run time)
0011BA:	F901	LDR 	P  	=#01	; LDR operand: #01
0011BC:	2510	ADD 	P  	=#10	; A <- A + #10
0011BE:	0F0F	LBX 	DL 	PRTS_COUNT	; LBX operand: [PRTS_COUNT]
0011C0:	C002	BCT 	RP 	l_11C4	; 2 words forward: conditional branch (BCT) to l_11C4
0011C2:	0510	ADD 	DL 	V_0010	; A <- A + [V_0010]
l_11C4:				; branch target; 1 ref; only from untraced words
0011C4:	2206	LDX 	P  	=#06	; X <- #06
0011C6:	915C	STA 	IGX	*G+005C,X	; [*G+005C+X] <- A  (pointer G+005C lives in the calling task's area: its contents are only known at run time)
0011C8:	2000	LDA 	P  	=#00	; A <- #00
0011CA:	FA01	STR 	P  	=#01	; STR operand: #01
0011CC:	405C	LDA 	DG 	G+005C	; A <- [G+005C]
0011CE:	2108	LDE 	P  	=#08	; E <- #08
0011D0:	425C	LDX 	DG 	G+005C	; X <- [G+005C]
0011D2:	F20A	ICX 	P  	=#0A	; ICX operand: #0A
0011D4:	5818	SPA 	DG 	TWB_N3	; [TWB_N3] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0011D6:	D75C	BRU 	IL 	*V_005C	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005C = #146A: address = G + #146A, G unknown)
l_11D8:				; branch target; 3 refs; only from untraced words
0011D8:	2002	LDA 	P  	=#02	; A <- #02
0011DA:	5728	ADM 	DG 	G+0028	; add A to [G+0028]
0011DC:	2B40	CMP 	P  	=#40	; compare A with #40
0011DE:	CA24	BOT 	RM 	l_1196	; 36 words backward: conditional branch (BOT) to l_1196
0011E0:	2200	LDX 	P  	=#00	; X <- #00
0011E2:	010C	LDE 	DL 	PRTS_PTR	; E <- [PRTS_PTR]
0011E4:	000E	LDA 	DL 	PRTS_COUNT	; A <- [PRTS_COUNT]
0011E6:	965C	DST 	IGX	*G+005C,X	; [*G+005C+X] <- A,E (2 words)  (pointer G+005C lives in the calling task's area: its contents are only known at run time)
0011E8:	F204	ICX 	P  	=#04	; ICX operand: #04
0011EA:	010A	LDE 	DL 	CPT_PTR	; E <- [CPT_PTR]
0011EC:	0022	LDA 	DL 	V_0022	; A <- [V_0022]
0011EE:	965C	DST 	IGX	*G+005C,X	; [*G+005C+X] <- A,E (2 words)  (pointer G+005C lives in the calling task's area: its contents are only known at run time)
0011F0:	F204	ICX 	P  	=#04	; ICX operand: #04
0011F2:	016E	LDE 	DL 	v_006E	; E <- [v_006E]
0011F4:	0024	LDA 	DL 	V_0024	; A <- [V_0024]
0011F6:	.EBCDIC	"o* *"	; 2 words, raw: 965C 405C
0011FA:	210C	LDE 	P  	=#0C	; E <- #0C
0011FC:	425C	LDX 	DG 	G+005C	; X <- [G+005C]
0011FE:	F20C	ICX 	P  	=#0C	; ICX operand: #0C
001200:	5818	SPA 	DG 	TWB_N3	; [TWB_N3] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001202:	D75C	BRU 	IL 	*V_005C	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005C = #146A: address = G + #146A, G unknown)
l_1204:				; branch target; 2 refs; only from untraced words
001204:	2000	LDA 	P  	=#00	; A <- #00
001206:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
001208:	2000	LDA 	P  	=#00	; A <- #00
00120A:	0DD4	LBL 	DL 	V_00D4	; A <- left byte of [V_00D4]
00120C:	CC6B	BAN 	RM 	l_1136	; 107 words backward: conditional branch (BAN) to l_1136
00120E:	C502	BAZ 	RP 	l_1212	; 2 words forward: conditional branch (BAZ) to l_1212
001210:	C70E	BRU 	RP 	l_122C	; 14 words forward: jump to l_122C
l_1212:				; branch target; 1 ref; only from untraced words
001212:	2110	LDE 	P  	=#10	; E <- #10
001214:	6430	LEA 	IL 	*V_0030	; A <- G+0000  (effective address, memory not read)  (pointer V_0030 = #0000: address = G + #0000, G unknown)
001216:	05EC	ADD 	DL 	v_00EC	; A <- A + [v_00EC]
001218:	7FEE	MVS 	IL 	*V_00EE	; MVS operand: [G+02FE]  (pointer V_00EE = #02FE: address = G + #02FE, G unknown)
00121A:	2202	LDX 	P  	=#02	; X <- #02
00121C:	016E	LDE 	DL 	v_006E	; E <- [v_006E]
00121E:	0024	LDA 	DL 	V_0024	; A <- [V_0024]
001220:	F102	XAE 	P  		; exchange A and E
001222:	B6EE	DST 	ILX	*V_00EE,X	; [G+02FE+X] <- A,E (2 words)  (pointer V_00EE = #02FE: address = G + #02FE, G unknown)
001224:	2001	LDA 	P  	=#01	; A <- #01
001226:	15DE	SBR 	DL 	V_00DE	; right byte of [V_00DE] <- A
001228:	15D5	SBR 	DL 	V_00D4	; right byte of [V_00D4] <- A
00122A:	C703	BRU 	RP 	l_1230	; 3 words forward: jump to l_1230
l_122C:				; branch target; 1 ref; only from untraced words
00122C:	0DD4	LBL 	DL 	V_00D4	; A <- left byte of [V_00D4]
00122E:	CC7C	BAN 	RM 	l_1136	; 124 words backward: conditional branch (BAN) to l_1136
l_1230:				; branch target; 1 ref; only from untraced words
001230:	2000	LDA 	P  	=#00	; A <- #00
l_1232:				; branch target; 1 ref; only from untraced words
001232:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001234:	D7A0	BRU 	IL 	*V_00A0	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_00A0 = #153C: address = G + #153C, G unknown)
l_1236:				; branch target; 1 ref; only from untraced words
001236:	C783	BRU 	RP 	l_133C	; 131 words forward: jump to l_133C
001238:	4020	LDA 	DG 	G+0020	; A <- [G+0020]
00123A:	D5C2	BAZ 	IL 	*v_00C2	; conditional branch, target not known statically  (pointer v_00C2 = #0D94: address = G + #0D94, G unknown)
00123C:	5433	SBL 	DG 	G+0033	; left byte of [G+0033] <- A
00123E:	2080	LDA 	P  	=#80	; A <- #80
001240:	5130	STA 	DG 	G+0030	; [G+0030] <- A
001242:	E302	DCX 	PX 	(X)	; DCX operand: [(X)]
001244:	4E21	LBR 	DG 	G+0021	; A <- right byte of [G+0021]
001246:	F11C	CNA 	P  	
001248:	A56C	ADD 	ILX	*v_006C,X	; A <- A + [G+03C6+X]  (pointer v_006C = #03C6: address = G + #03C6, G unknown)
00124A:	FA05	STR 	P  	=#05	; STR operand: #05
00124C:	AE30	LBR 	ILX	*V_0030,X	; A <- right byte of [G+0000+X]  (pointer V_0030 = #0000: address = G + #0000, G unknown)
00124E:	5532	SBR 	DG 	G+0032	; right byte of [G+0032] <- A
001250:	4022	LDA 	DG 	G+0022	; A <- [G+0022]
001252:	5136	STA 	DG 	G+0036	; [G+0036] <- A
001254:	4E32	LBR 	DG 	G+0032	; A <- right byte of [G+0032]
001256:	2BB0	CMP 	P  	=#B0	; compare A with #B0
001258:	C304	BCF 	RP 	l_1260	; 4 words forward: conditional branch (BCF) to l_1260
00125A:	2078	LDA 	P  	=#78	; A <- #78
00125C:	5136	STA 	DG 	G+0036	; [G+0036] <- A
00125E:	C715	BRU 	RP 	l_1288	; 21 words forward: jump to l_1288
l_1260:				; branch target; 1 ref; only from untraced words
001260:	2B80	CMP 	P  	=#80	; compare A with #80
001262:	C313	BCF 	RP 	l_1288	; 19 words forward: conditional branch (BCF) to l_1288
001264:	4E33	LBR 	DG 	G+0033	; A <- right byte of [G+0033]
001266:	FA05	STR 	P  	=#05	; STR operand: #05
001268:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
00126A:	A11A	LDE 	ILX	*V_001A,X	; E <- [G+22A6+X]  (pointer V_001A = #22A6: address = G + #22A6, G unknown)
00126C:	F102	XAE 	P  		; exchange A and E
00126E:	2901	AND 	P  	=#01	; A <- A and #01
001270:	356A	ICL 	DL 	V_006A	; ICL operand: [V_006A]
001272:	C504	BAZ 	RP 	l_127A	; 4 words forward: conditional branch (BAZ) to l_127A
001274:	702C	DLD 	IL 	*V_002C	; A,E <- [G+0986] (2 words)  (pointer V_002C = #0986: address = G + #0986, G unknown)
001276:	2250	LDX 	P  	=#50	; X <- #50
001278:	C703	BRU 	RP 	l_127E	; 3 words forward: jump to l_127E
l_127A:				; branch target; 1 ref; only from untraced words
00127A:	702A	DLD 	IL 	*V_002A	; A,E <- [G+0AB8] (2 words)  (pointer V_002A = #0AB8: address = G + #0AB8, G unknown)
00127C:	2278	LDX 	P  	=#78	; X <- #78
l_127E:				; branch target; 1 ref; only from untraced words
00127E:	3500	ICL 	DL 	Unknow_1	; ICL operand: [Unknow_1]
001280:	5336	STX 	DG 	G+0036	; [G+0036] <- X
001282:	425C	LDX 	DG 	G+005C	; X <- [G+005C]
001284:	5334	STX 	DG 	G+0034	; [G+0034] <- X
001286:	9614	DST 	IGX	*TWB_T6,X	; [*TWB_T6+X] <- A,E (2 words)  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
l_1288:				; branch target; 2 refs; only from untraced words
001288:	2030	LDA 	P  	=#30	; A <- #30
00128A:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
00128C:	D750	BRU 	IL 	*V_0050	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0050 = #051A: address = G + #051A, G unknown)
l_128E:				; branch target; 1 ref; only from untraced words
00128E:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001290:	D752	BRU 	IL 	*V_0052	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0052 = #0844: address = G + #0844, G unknown)
l_1292:				; branch target; 1 ref; only from untraced words
001292:	4E30	LBR 	DG 	G+0030	; A <- right byte of [G+0030]
001294:	2B64	CMP 	P  	=#64	; compare A with #64
001296:	C053	BCT 	RP 	l_133C	; 83 words forward: conditional branch (BCT) to l_133C
001298:	C552	BAZ 	RP 	l_133C	; 82 words forward: conditional branch (BAZ) to l_133C
00129A:	CFB6	BRU 	RM 	l_112E	; 182 words backward: jump to l_112E
00129C:	4220	LDX 	DG 	G+0020	; X <- [G+0020]
00129E:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
0012A0:	F114	CNX 	P  	
0012A2:	A00A	LDA 	ILX	*CPT_PTR,X	; A <- [G+0380+X]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
0012A4:	29FF	AND 	P  	=#FF	; A <- A and #FF
0012A6:	2B04	CMP 	P  	=#04	; compare A with #04
0012A8:	C8B7	BCT 	RM 	l_113A	; 183 words backward: conditional branch (BCT) to l_113A
0012AA:	4020	LDA 	DG 	G+0020	; A <- [G+0020]
0012AC:	CDBB	BAZ 	RM 	l_1136	; 187 words backward: conditional branch (BAZ) to l_1136
0012AE:	4222	LDX 	DG 	G+0022	; X <- [G+0022]

; ===== MOD_12B0: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N0, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: stores into the caller's TWB, returns to its CSV caller with RSV
MOD_12B0:				; entry point; 1 ref
0012B0:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0012B2:	D7CE	BRU 	IL 	*V_00CE	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_00CE = #1708: address = G + #1708, G unknown)
L_12B4:				; branch target; 1 ref
0012B4:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
s_12B6:				; callee (section / supervisor module / SPA subroutine); branch target; 2 refs; only from untraced words
0012B6:	4E21	LBR 	DG 	G+0021	; A <- right byte of [G+0021]
0012B8:	C504	BAZ 	RP 	l_12C0	; 4 words forward: conditional branch (BAZ) to l_12C0
0012BA:	4022	LDA 	DG 	G+0022	; A <- [G+0022]
0012BC:	C51F	BAZ 	RP 	l_12FA	; 31 words forward: conditional branch (BAZ) to l_12FA
0012BE:	D7C2	BRU 	IL 	*v_00C2	; jump to G+#0D94 (G unknown)  (pointer v_00C2 = #0D94: address = G + #0D94, G unknown)
l_12C0:				; branch target; 1 ref; only from untraced words
0012C0:	4E54	LBR 	DG 	G+0054	; A <- right byte of [G+0054]
0012C2:	C586	BAZ 	RP 	l_13CE	; 134 words forward: conditional branch (BAZ) to l_13CE
0012C4:	F902	LDR 	P  	=#02	; LDR operand: #02
0012C6:	4558	ADD 	DG 	G+0058	; A <- A + [G+0058]
0012C8:	FA01	STR 	P  	=#01	; STR operand: #01
0012CA:	4E22	LBR 	DG 	G+0022	; A <- right byte of [G+0022]
0012CC:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
0012CE:	0E0E	LBR 	DL 	PRTS_COUNT	; A <- right byte of [PRTS_COUNT]
0012D0:	4608	SUB 	DG 	TWB_T0	; A <- A - [TWB_T0]
0012D2:	C484	BAN 	RP 	l_13DA	; 132 words forward: conditional branch (BAN) to l_13DA
0012D4:	4008	LDA 	DG 	TWB_T0	; A <- [TWB_T0]
0012D6:	F002	SLLS	P  	#02
0012D8:	F11C	CNA 	P  	
0012DA:	FA05	STR 	P  	=#05	; STR operand: #05
0012DC:	4E23	LBR 	DG 	G+0023	; A <- right byte of [G+0023]
0012DE:	C502	BAZ 	RP 	l_12E2	; 2 words forward: conditional branch (BAZ) to l_12E2
0012E0:	F202	ICX 	P  	=#02	; ICX operand: #02
l_12E2:				; branch target; 1 ref; only from untraced words
0012E2:	3208	ICX 	DL 	V_0008	; ICX operand: [V_0008]
0012E4:	0108	LDE 	DL 	V_0008	; E <- [V_0008]
0012E6:	2000	LDA 	P  	=#00	; A <- #00
0012E8:	FA01	STR 	P  	=#01	; STR operand: #01
0012EA:	4E22	LBR 	DG 	G+0022	; A <- right byte of [G+0022]
0012EC:	C504	BAZ 	RP 	l_12F4	; 4 words forward: conditional branch (BAZ) to l_12F4
0012EE:	F102	XAE 	P  		; exchange A and E
0012F0:	A530	ADD 	ILX	*V_0030,X	; A <- A + [G+0000+X]  (pointer V_0030 = #0000: address = G + #0000, G unknown)
0012F2:	C704	BRU 	RP 	l_12FA	; 4 words forward: jump to l_12FA
l_12F4:				; branch target; 1 ref; only from untraced words
0012F4:	4E23	LBR 	DG 	G+0023	; A <- right byte of [G+0023]
0012F6:	D3C2	BCF 	IL 	*v_00C2	; conditional branch, target not known statically  (pointer v_00C2 = #0D94: address = G + #0D94, G unknown)
0012F8:	F102	XAE 	P  		; exchange A and E
l_12FA:				; branch target; 2 refs; only from untraced words
0012FA:	510E	STA 	DG 	TWB_T3	; [TWB_T3] <- A
0012FC:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)
0012FE:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001300:	CF25	BRU 	RM 	s_12B6	; 37 words backward: CALL subroutine s_12B6 (SPA above saved the return address); returns to l_1302
l_1302:				; branch target; 1 ref; only from untraced words
001302:	2200	LDX 	P  	=#00	; X <- #00
l_1304:				; branch target; 1 ref; only from untraced words
001304:	8024	LDA 	IGX	*G+0024,X	; A <- [*G+0024+X]  (pointer G+0024 lives in the calling task's area: its contents are only known at run time)
001306:	C41B	BAN 	RP 	l_133C	; 27 words forward: conditional branch (BAN) to l_133C
001308:	9024	DLD 	IGX	*G+0024,X	; A,E <- [*G+0024+X] (2 words)  (pointer G+0024 lives in the calling task's area: its contents are only known at run time)
00130A:	520C	STE 	DG 	TWB_T2	; [TWB_T2] <- E
00130C:	450E	ADD 	DG 	TWB_T3	; A <- A + [TWB_T3]
00130E:	C062	BCT 	RP 	l_13D2	; 98 words forward: conditional branch (BCT) to l_13D2
001310:	510A	STA 	DG 	TWB_T1	; [TWB_T1] <- A
001312:	F204	ICX 	P  	=#04	; ICX operand: #04
001314:	5316	STX 	DG 	TWB_T7	; [TWB_T7] <- X
l_1316:				; branch target; 1 ref; only from untraced words
001316:	4216	LDX 	DG 	TWB_T7	; X <- [TWB_T7]
001318:	400C	LDA 	DG 	TWB_T2	; A <- [TWB_T2]
00131A:	CD0B	BAZ 	RM 	l_1304	; 11 words backward: conditional branch (BAZ) to l_1304
00131C:	0026	LDA 	DL 	V_0026	; A <- [V_0026]
00131E:	410A	LDE 	DG 	TWB_T1	; E <- [TWB_T1]
001320:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001322:	D75A	BRU 	IL 	*V_005A	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005A = #1642: address = G + #1642, G unknown)
l_1324:				; branch target; 1 ref; only from untraced words
001324:	C457	BAN 	RP 	l_13D2	; 87 words forward: conditional branch (BAN) to l_13D2
001326:	8024	LDA 	IGX	*G+0024,X	; A <- [*G+0024+X]  (pointer G+0024 lives in the calling task's area: its contents are only known at run time)
001328:	F202	ICX 	P  	=#02	; ICX operand: #02
00132A:	5316	STX 	DG 	TWB_T7	; [TWB_T7] <- X
00132C:	420A	LDX 	DG 	TWB_T1	; X <- [TWB_T1]
00132E:	B130	STA 	ILX	*V_0030,X	; [G+0000+X] <- A  (pointer V_0030 = #0000: address = G + #0000, G unknown)
001330:	F202	ICX 	P  	=#02	; ICX operand: #02
001332:	530A	STX 	DG 	TWB_T1	; [TWB_T1] <- X
001334:	400C	LDA 	DG 	TWB_T2	; A <- [TWB_T2]
001336:	2601	SUB 	P  	=#01	; A <- A - #01
001338:	510C	STA 	DG 	TWB_T2	; [TWB_T2] <- A
00133A:	CF12	BRU 	RM 	l_1316	; 18 words backward: jump to l_1316
l_133C:				; branch target; 6 refs; only from untraced words
00133C:	2000	LDA 	P  	=#00	; A <- #00
00133E:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
001340:	4D22	LBL 	DG 	G+0022	; A <- left byte of [G+0022]
001342:	C419	BAN 	RP 	l_1374	; 25 words forward: conditional branch (BAN) to l_1374
001344:	2208	LDX 	P  	=#08	; X <- #08
001346:	8058	LDA 	IGX	*G+0058,X	; A <- [*G+0058+X]  (pointer G+0058 lives in the calling task's area: its contents are only known at run time)
001348:	512A	STA 	DG 	G+002A	; [G+002A] <- A
00134A:	4028	LDA 	DG 	G+0028	; A <- [G+0028]
00134C:	C517	BAZ 	RP 	l_137A	; 23 words forward: conditional branch (BAZ) to l_137A
00134E:	4D23	LBL 	DG 	G+0023	; A <- left byte of [G+0023]
001350:	C41F	BAN 	RP 	l_138E	; 31 words forward: conditional branch (BAN) to l_138E
001352:	4E54	LBR 	DG 	G+0054	; A <- right byte of [G+0054]
001354:	C543	BAZ 	RP 	l_13DA	; 67 words forward: conditional branch (BAZ) to l_13DA
001356:	402A	LDA 	DG 	G+002A	; A <- [G+002A]
001358:	5724	ADM 	DG 	G+0024	; add A to [G+0024]
00135A:	0126	LDE 	DL 	V_0026	; E <- [V_0026]
00135C:	F102	XAE 	P  		; exchange A and E
00135E:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001360:	D75A	BRU 	IL 	*V_005A	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005A = #1642: address = G + #1642, G unknown)
l_1362:				; branch target; 1 ref; only from untraced words
001362:	C434	BAN 	RP 	l_13CA	; 52 words forward: conditional branch (BAN) to l_13CA
001364:	402A	LDA 	DG 	G+002A	; A <- [G+002A]
001366:	5726	ADM 	DG 	G+0026	; add A to [G+0026]
001368:	0126	LDE 	DL 	V_0026	; E <- [V_0026]
00136A:	F102	XAE 	P  		; exchange A and E
00136C:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
00136E:	D75A	BRU 	IL 	*V_005A	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005A = #1642: address = G + #1642, G unknown)
l_1370:				; branch target; 1 ref; only from untraced words
001370:	C42D	BAN 	RP 	l_13CA	; 45 words forward: conditional branch (BAN) to l_13CA
001372:	C70E	BRU 	RP 	l_138E	; 14 words forward: jump to l_138E
l_1374:				; branch target; 1 ref; only from untraced words
001374:	0026	LDA 	DL 	V_0026	; A <- [V_0026]
001376:	5126	STA 	DG 	G+0026	; [G+0026] <- A
001378:	C70B	BRU 	RP 	l_138E	; 11 words forward: jump to l_138E
l_137A:				; branch target; 1 ref; only from untraced words
00137A:	0EDD	LBR 	DL 	V_00DC	; A <- right byte of [V_00DC]
00137C:	C503	BAZ 	RP 	l_1382	; 3 words forward: conditional branch (BAZ) to l_1382
00137E:	01F4	LDE 	DL 	v_00F4	; E <- [v_00F4]
001380:	C702	BRU 	RP 	l_1384	; 2 words forward: jump to l_1384
l_1382:				; branch target; 1 ref; only from untraced words
001382:	0122	LDE 	DL 	V_0022	; E <- [V_0022]
l_1384:				; branch target; 1 ref; only from untraced words
001384:	0026	LDA 	DL 	V_0026	; A <- [V_0026]
001386:	5624	DST 	DG 	G+0024	; [G+0024] <- A,E (2 words)
001388:	C703	BRU 	RP 	l_138E	; 3 words forward: jump to l_138E
00138A:	2040	LDA 	P  	=#40	; A <- #40
00138C:	5720	ADM 	DG 	G+0020	; add A to [G+0020]
l_138E:				; branch target; 4 refs; only from untraced words
00138E:	F908	LDR 	P  	=#08	; LDR operand: #08
001390:	CD2A	BAZ 	RM 	l_133C	; 42 words backward: conditional branch (BAZ) to l_133C
001392:	0EDE	LBR 	DL 	V_00DE	; A <- right byte of [V_00DE]
001394:	C321	BCF 	RP 	l_13D6	; 33 words forward: conditional branch (BCF) to l_13D6
001396:	6430	LEA 	IL 	*V_0030	; A <- G+0000  (effective address, memory not read)  (pointer V_0030 = #0000: address = G + #0000, G unknown)
001398:	2110	LDE 	P  	=#10	; E <- #10
00139A:	05EC	ADD 	DL 	v_00EC	; A <- A + [v_00EC]
00139C:	7FEE	MVS 	IL 	*V_00EE	; MVS operand: [G+02FE]  (pointer V_00EE = #02FE: address = G + #02FE, G unknown)
00139E:	2202	LDX 	P  	=#02	; X <- #02
0013A0:	5024	DLD 	DG 	G+0024	; A,E <- [G+0024] (2 words)
0013A2:	F102	XAE 	P  		; exchange A and E
0013A4:	B6EE	DST 	ILX	*V_00EE,X	; [G+02FE+X] <- A,E (2 words)  (pointer V_00EE = #02FE: address = G + #02FE, G unknown)
0013A6:	2206	LDX 	P  	=#06	; X <- #06
0013A8:	4020	LDA 	DG 	G+0020	; A <- [G+0020]
0013AA:	B1EE	STA 	ILX	*V_00EE,X	; [G+02FE+X] <- A  (pointer V_00EE = #02FE: address = G + #02FE, G unknown)
0013AC:	2001	LDA 	P  	=#01	; A <- #01
0013AE:	15DE	SBR 	DL 	V_00DE	; right byte of [V_00DE] <- A
0013B0:	17D6	ADM 	DL 	V_00D6	; add A to [V_00D6]
0013B2:	02D6	LDX 	DL 	V_00D6	; X <- [V_00D6]
0013B4:	0ED4	LBR 	DL 	V_00D4	; A <- right byte of [V_00D4]
0013B6:	B5DA	SBR 	ILX	*V_00DA,X	; right byte of [G+0312+X] <- A  (pointer V_00DA = #0312: address = G + #0312, G unknown)
0013B8:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
0013BA:	600A	LDA 	IL 	*CPT_PTR	; A <- [G+0380]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
0013BC:	B1D8	STA 	ILX	*V_00D8,X	; [G+030E+X] <- A  (pointer V_00D8 = #030E: address = G + #030E, G unknown)
0013BE:	00EE	LDA 	DL 	V_00EE	; A <- [V_00EE]
0013C0:	710A	STA 	IL 	*CPT_PTR	; [G+0380] <- A  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
0013C2:	2001	LDA 	P  	=#01	; A <- #01
0013C4:	15D4	SBR 	DL 	V_00D4	; right byte of [V_00D4] <- A
0013C6:	CF45	BRU 	RM 	l_133C	; 69 words backward: jump to l_133C
0013C8:	CF89	BRU 	RM 	s_12B6	; 137 words backward: jump to s_12B6
l_13CA:				; branch target; 2 refs; only from untraced words
0013CA:	2D41	LBL 	P  	=#41	; A <- left byte of #41
0013CC:	C708	BRU 	RP 	l_13DC	; 8 words forward: jump to l_13DC
l_13CE:				; branch target; 1 ref; only from untraced words
0013CE:	2D42	LBL 	P  	=#42	; A <- left byte of #42
0013D0:	C706	BRU 	RP 	l_13DC	; 6 words forward: jump to l_13DC
l_13D2:				; branch target; 2 refs; only from untraced words
0013D2:	2D43	LBL 	P  	=#43	; A <- left byte of #43
0013D4:	C704	BRU 	RP 	l_13DC	; 4 words forward: jump to l_13DC
l_13D6:				; branch target; 1 ref; only from untraced words
0013D6:	2D44	LBL 	P  	=#44	; A <- left byte of #44
0013D8:	C702	BRU 	RP 	l_13DC	; 2 words forward: jump to l_13DC
l_13DA:				; branch target; 2 refs; only from untraced words
0013DA:	2D46	LBL 	P  	=#46	; A <- left byte of #46
l_13DC:				; branch target; 4 refs; only from untraced words
0013DC:	FA04	STR 	P  	=#04	; STR operand: #04
0013DE:	2000	LDA 	P  	=#00	; A <- #00
0013E0:	FA01	STR 	P  	=#01	; STR operand: #01
0013E2:	F904	LDR 	P  	=#04	; LDR operand: #04

; ===== MOD_13E4: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N2, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: stores into the caller's TWB, returns to its CSV caller with RSV
MOD_13E4:				; entry point; 1 ref
0013E4:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0013E6:	D75E	BRU 	IL 	*V_005E	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005E = #14F0: address = G + #14F0, G unknown)
L_13E8:				; branch target; 1 ref
0013E8:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
0013EA:	5208	STE 	DG 	TWB_T0	; [TWB_T0] <- E
0013EC:	510C	STA 	DG 	TWB_T2	; [TWB_T2] <- A
0013EE:	2200	LDX 	P  	=#00	; X <- #00
0013F0:	8E08	LBR 	IGX	*TWB_T0,X	; A <- right byte of [*TWB_T0+X]
0013F2:	FA04	STR 	P  	=#04	; STR operand: #04
l_13F4:				; branch target; 1 ref; only from untraced words
0013F4:	F201	ICX 	P  	=#01	; ICX operand: #01
0013F6:	8E08	LBR 	IGX	*TWB_T0,X	; A <- right byte of [*TWB_T0+X]
0013F8:	F102	XAE 	P  		; exchange A and E
0013FA:	C506	BAZ 	RP 	l_1406	; 6 words forward: conditional branch (BAZ) to l_1406
0013FC:	F102	XAE 	P  		; exchange A and E
0013FE:	F201	ICX 	P  	=#01	; ICX operand: #01
001400:	8D08	LBL 	IGX	*TWB_T0,X	; A <- left byte of [*TWB_T0+X]
001402:	F108	XAA 	P  	
001404:	C702	BRU 	RP 	l_1408	; 2 words forward: jump to l_1408
l_1406:				; branch target; 1 ref; only from untraced words
001406:	F102	XAE 	P  		; exchange A and E
l_1408:				; branch target; 1 ref; only from untraced words
001408:	4B0C	CMP 	DG 	TWB_T2	; compare A with [TWB_T2]
00140A:	C005	BCT 	RP 	l_1414	; 5 words forward: conditional branch (BCT) to l_1414
00140C:	2BFF	CMP 	P  	=#FF	; compare A with #FF
00140E:	CB0D	BCF 	RM 	l_13F4	; 13 words backward: conditional branch (BCF) to l_13F4
001410:	2DFF	LBL 	P  	=#FF	; A <- left byte of #FF
001412:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)
l_1414:				; branch target; 1 ref; only from untraced words
001414:	2D00	LBL 	P  	=#00	; A <- left byte of #00
001416:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)
001418:	510C	STA 	DG 	TWB_T2	; [TWB_T2] <- A
l_141A:				; branch target; 1 ref; only from untraced words
00141A:	8E14	LBR 	IGX	*TWB_T6,X	; A <- right byte of [*TWB_T6+X]  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
00141C:	F201	ICX 	P  	=#01	; ICX operand: #01
00141E:	4B0C	CMP 	DG 	TWB_T2	; compare A with [TWB_T2]
001420:	D81E	BCT 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)
001422:	2B6B	CMP 	P  	=#6B	; compare A with #6B
001424:	D81E	BCT 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)
001426:	2B5E	CMP 	P  	=#5E	; compare A with #5E
001428:	C005	BCT 	RP 	l_1432	; 5 words forward: conditional branch (BCT) to l_1432
00142A:	2B0D	CMP 	P  	=#0D	; compare A with #0D
00142C:	C003	BCT 	RP 	l_1432	; 3 words forward: conditional branch (BCT) to l_1432
00142E:	2B40	CMP 	P  	=#40	; compare A with #40
001430:	CB0B	BCF 	RM 	l_141A	; 11 words backward: conditional branch (BCF) to l_141A
l_1432:				; branch target; 2 refs; only from untraced words
001432:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
001434:	FA05	STR 	P  	=#05	; STR operand: #05
001436:	8E14	LBR 	IGX	*TWB_T6,X	; A <- right byte of [*TWB_T6+X]  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
001438:	2B50	CMP 	P  	=#50	; compare A with #50
00143A:	C005	BCT 	RP 	l_1444	; 5 words forward: conditional branch (BCT) to l_1444
00143C:	F905	LDR 	P  	=#05	; LDR operand: #05
00143E:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001440:	D758	BRU 	IL 	*v_0058	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer v_0058 = #08F6: address = G + #08F6, G unknown)
l_1442:				; branch target; 1 ref; only from untraced words
001442:	C705	BRU 	RP 	l_144C	; 5 words forward: jump to l_144C
l_1444:				; branch target; 1 ref; only from untraced words
001444:	F201	ICX 	P  	=#01	; ICX operand: #01
001446:	F905	LDR 	P  	=#05	; LDR operand: #05
001448:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
00144A:	D754	BRU 	IL 	*V_0054	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0054 = #0896: address = G + #0896, G unknown)
l_144C:				; branch target; 2 refs; only from untraced words
00144C:	DF1C	BRU 	IG 	*TWB_N1	; RETURN from subroutine: branch through return cell TWB_N1 (written by the SPA of the call)

; ===== M_MOVE: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N0, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: 1 loop, stores into the caller's TWB, returns to its CSV caller with RSV
; =====================================================================
; M:MOVE MOVING A STRING OF BYTES
* Calling sequence:
LEA CHAIN2
XAX
LEA ​​CHAIN1
lDE NUMBER
CSV M:MOVE
At the time of CSV execution, register A must contain the G-relative address of the source string, register X must contain the G-relative address of the destination string, and register E must contain the number of bytes to transfer.

* Function:
Moving a string of bytes in memory. The module operates using descending addresses, byte by byte.

* Output information:
G-relative address of the source string.
G-relative address of the destination string.
• TWB memory locations used: T0 to N0.
; =====================================================================
M_MOVE:				; entry point; callee (section / supervisor module / SPA subroutine); 2 refs
00144E:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001450:	C702	BRU 	RP 	S_1454	; 2 words forward: CALL subroutine S_1454 (SPA above saved the return address); returns to L_1452  [-> S_1454: 1 loop, stores into the caller's TWB, returns through its return cell]
L_1452:				; branch target; 1 ref
001452:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== S_1454: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: 1 loop, stores into the caller's TWB, returns through its return cell
S_1454:				; callee (section / supervisor module / SPA subroutine); 1 ref
001454:	F106	XEX 	P  		; exchange E and X
001456:	5608	DST 	DG 	TWB_T0	; [TWB_T0] <- A,E (2 words)
L_1458:				; branch target; 1 ref
001458:	F301	DCX 	P  	=#01	; DCX operand: #01
00145A:	C304	BCF 	RP 	L_1462	; 4 words forward: conditional branch (BCF) to L_1462
00145C:	8E0A	LBR 	IGX	*TWB_T1,X	; A <- right byte of [*TWB_T1+X]  (pointer TWB_T1 lives in the calling task's area: its contents are only known at run time)
00145E:	9508	SBR 	IGX	*TWB_T0,X	; right byte of [*TWB_T0+X] <- A
001460:	CF04	BRU 	RM 	L_1458	; 4 words backward: jump to L_1458
L_1462:				; branch target; 1 ref
001462:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)

; ===== M_EDIT: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N3, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: 4 loops, stores into the caller's TWB, returns to its CSV caller with RSV
; =====================================================================
; M:EDIT EDITING A MEMORY AREA
* Calling sequence:
LEA BUFF
XAX
LEA ​​ZONE
LDE NUMBER
CSV M:EDIT
At the time of CSV execution, register A must contain the address (relative to G) of the memory area to be edited, register E must contain the number of memory bytes to be edited, and register X must contain the address (relative to G) of the user buffer required for the editing operation. This buffer must be at least ((5 * n) + 1) bytes in size—modulo 41—if the intention is to edit *n* memory bytes per line.

* Function:
Displays a memory area in hexadecimal format. This display is performed word by word, with eight words per line, on the M:OC (teletypewriter).
An Input/Output error causes the module to abort.
; =====================================================================
M_EDIT:				; entry point; callee (section / supervisor module / SPA subroutine); 3 refs
001464:	5818	SPA 	DG 	TWB_N3	; [TWB_N3] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001466:	C702	BRU 	RP 	S_146A	; 2 words forward: CALL subroutine S_146A (SPA above saved the return address); returns to L_1468  [-> S_146A: 4 loops, stores into the caller's TWB, returns through its return cell]
L_1468:				; branch target; 1 ref
001468:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== S_146A: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: 4 loops, stores into the caller's TWB, returns through its return cell
S_146A:				; callee (section / supervisor module / SPA subroutine); 1 ref
00146A:	562E	DST 	DG 	G+002E	; [G+002E] <- A,E (2 words)
00146C:	5324	STX 	DG 	G+0024	; [G+0024] <- X
00146E:	2180	LDE 	P  	=#80	; E <- #80
001470:	2D80	LBL 	P  	=#80	; A <- left byte of #80
001472:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
001474:	2100	LDE 	P  	=#00	; E <- #00
001476:	522C	STE 	DG 	G+002C	; [G+002C] <- E
001478:	2E40	LBR 	P  	=#40	; A <- right byte of #40
00147A:	2D40	LBL 	P  	=#40	; A <- left byte of #40
00147C:	2200	LDX 	P  	=#00	; X <- #00
00147E:	9124	STA 	IGX	*G+0024,X	; [*G+0024+X] <- A  (pointer G+0024 lives in the calling task's area: its contents are only known at run time)
L_1480:				; branch target; 1 ref
001480:	2102	LDE 	P  	=#02	; E <- #02
001482:	2008	LDA 	P  	=#08	; A <- #08
L_1484:				; branch target; 1 ref
001484:	5632	DST 	DG 	G+0032	; [G+0032] <- A,E (2 words)
001486:	422C	LDX 	DG 	G+002C	; X <- [G+002C]
001488:	8130	LDE 	IGX	*G+0030,X	; E <- [*G+0030+X]  (pointer G+0030 lives in the calling task's area: its contents are only known at run time)
00148A:	F202	ICX 	P  	=#02	; ICX operand: #02
00148C:	532C	STX 	DG 	G+002C	; [G+002C] <- X
00148E:	4232	LDX 	DG 	G+0032	; X <- [G+0032]
001490:	8424	LEA 	IGX	*G+0024,X	; A <- *G+0024+X  (effective address, memory not read)  (pointer G+0024 lives in the calling task's area: its contents are only known at run time)
001492:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001494:	D756	BRU 	IL 	*V_0056	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0056 = #08D8: address = G + #08D8, G unknown)
L_1496:				; branch target; 1 ref
001496:	4232	LDX 	DG 	G+0032	; X <- [G+0032]
001498:	F204	ICX 	P  	=#04	; ICX operand: #04
00149A:	2E40	LBR 	P  	=#40	; A <- right byte of #40
00149C:	9524	SBR 	IGX	*G+0024,X	; right byte of [*G+0024+X] <- A  (pointer G+0024 lives in the calling task's area: its contents are only known at run time)
00149E:	F201	ICX 	P  	=#01	; ICX operand: #01
0014A0:	402C	LDA 	DG 	G+002C	; A <- [G+002C]
0014A2:	462E	SUB 	DG 	G+002E	; A <- A - [G+002E]
0014A4:	C006	BCT 	RP 	L_14B0	; 6 words forward: conditional branch (BCT) to L_14B0
0014A6:	4034	LDA 	DG 	G+0034	; A <- [G+0034]
0014A8:	2601	SUB 	P  	=#01	; A <- A - #01
0014AA:	C503	BAZ 	RP 	L_14B0	; 3 words forward: conditional branch (BAZ) to L_14B0
0014AC:	F106	XEX 	P  		; exchange E and X
0014AE:	CF15	BRU 	RM 	L_1484	; 21 words backward: jump to L_1484
L_14B0:				; branch target; 2 refs
0014B0:	5326	STX 	DG 	G+0026	; [G+0026] <- X
0014B2:	F908	LDR 	P  	=#08	; LDR operand: #08
0014B4:	C503	BAZ 	RP 	L_14BA	; 3 words forward: conditional branch (BAZ) to L_14BA
L_14B6:				; branch target; 2 refs
0014B6:	2004	LDA 	P  	=#04	; A <- #04
0014B8:	C706	BRU 	RP 	L_14C4	; 6 words forward: jump to L_14C4
L_14BA:				; branch target; 1 ref
0014BA:	0EDD	LBR 	DL 	V_00DC	; A <- right byte of [V_00DC]
0014BC:	CD03	BAZ 	RM 	L_14B6	; 3 words backward: conditional branch (BAZ) to L_14B6
0014BE:	0EDE	LBR 	DL 	V_00DE	; A <- right byte of [V_00DE]
0014C0:	CB05	BCF 	RM 	L_14B6	; 5 words backward: conditional branch (BCF) to L_14B6
0014C2:	2008	LDA 	P  	=#08	; A <- #08
L_14C4:				; branch target; 1 ref
0014C4:	5523	SBR 	DG 	G+0023	; right byte of [G+0023] <- A
0014C6:	4420	LEA 	DG 	G+0020	; A <- G+0020  (effective address, memory not read)
0014C8:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0014CA:	D750	BRU 	IL 	*V_0050	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0050 = #051A: address = G + #051A, G unknown)
L_14CC:				; branch target; 1 ref
0014CC:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0014CE:	D752	BRU 	IL 	*V_0052	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0052 = #0844: address = G + #0844, G unknown)
L_14D0:				; branch target; 1 ref
0014D0:	C406	BAN 	RP 	L_14DC	; 6 words forward: conditional branch (BAN) to L_14DC
0014D2:	402C	LDA 	DG 	G+002C	; A <- [G+002C]
0014D4:	462E	SUB 	DG 	G+002E	; A <- A - [G+002E]
0014D6:	CB2B	BCF 	RM 	L_1480	; 43 words backward: conditional branch (BCF) to L_1480
0014D8:	2000	LDA 	P  	=#00	; A <- #00
0014DA:	DF18	BRU 	IG 	*TWB_N3	; RETURN from subroutine: branch through return cell TWB_N3 (written by the SPA of the call)
L_14DC:				; branch target; 1 ref
0014DC:	4E20	LBR 	DG 	G+0020	; A <- right byte of [G+0020]
0014DE:	F0C4	SRLS	P  	#04
0014E0:	250D	ADD 	P  	=#0D	; A <- A + #0D
0014E2:	F108	XAA 	P  	
0014E4:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0014E6:	D75E	BRU 	IL 	*V_005E	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005E = #14F0: address = G + #14F0, G unknown)
L_14E8:				; branch target; 1 ref
0014E8:	DF18	BRU 	IG 	*TWB_N3	; RETURN from subroutine: branch through return cell TWB_N3 (written by the SPA of the call)

; ===== M_ERED: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N2, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: 3 loops, stores into the caller's TWB, returns to its CSV caller with RSV
; =====================================================================
; M:ERED Edition message d'erreur sur MAE (ASR 33)
; =====================================================================
M_ERED:				; entry point; callee (section / supervisor module / SPA subroutine); 4 refs
0014EA:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0014EC:	C702	BRU 	RP 	S_14F0	; 2 words forward: CALL subroutine S_14F0 (SPA above saved the return address); returns to L_14EE  [-> S_14F0: 3 loops, stores into the caller's TWB, returns through its return cell]
L_14EE:				; branch target; 1 ref
0014EE:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== S_14F0: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: 3 loops, stores into the caller's TWB, returns through its return cell
S_14F0:				; callee (section / supervisor module / SPA subroutine); 1 ref
0014F0:	512E	STA 	DG 	G+002E	; [G+002E] <- A
0014F2:	4E2E	LBR 	DG 	G+002E	; A <- right byte of [G+002E]
0014F4:	C51D	BAZ 	RP 	L_152E	; 29 words forward: conditional branch (BAZ) to L_152E
0014F6:	210A	LDE 	P  	=#0A	; E <- #0A
0014F8:	649E	LEA 	IL 	*V_009E	; A <- G+016A  (effective address, memory not read)  (pointer V_009E = #016A: address = G + #016A, G unknown)
0014FA:	F104	XAX 	P  		; exchange A and X
0014FC:	4420	LEA 	DG 	G+0020	; A <- G+0020  (effective address, memory not read)
0014FE:	F104	XAX 	P  		; exchange A and X
001500:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001502:	D7AC	BRU 	IL 	*V_00AC	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_00AC = #1454: address = G + #1454, G unknown)
L_1504:				; branch target; 1 ref
001504:	4E2E	LBR 	DG 	G+002E	; A <- right byte of [G+002E]
001506:	290F	AND 	P  	=#0F	; A <- A and #0F
001508:	F102	XAE 	P  		; exchange A and E
00150A:	442A	LEA 	DG 	G+002A	; A <- G+002A  (effective address, memory not read)
00150C:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
00150E:	D756	BRU 	IL 	*V_0056	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0056 = #08D8: address = G + #08D8, G unknown)
L_1510:				; branch target; 1 ref
001510:	4E2E	LBR 	DG 	G+002E	; A <- right byte of [G+002E]
001512:	F0C3	SRLS	P  	#03
001514:	F104	XAX 	P  		; exchange A and X
001516:	A0C6	LDA 	ILX	*V_00C6,X	; A <- [G+0176+X]  (pointer V_00C6 = #0176: address = G + #0176, G unknown)
001518:	512A	STA 	DG 	G+002A	; [G+002A] <- A
00151A:	F908	LDR 	P  	=#08	; LDR operand: #08
00151C:	C50B	BAZ 	RP 	L_1532	; 11 words forward: conditional branch (BAZ) to L_1532
L_151E:				; branch target; 2 refs
00151E:	2E04	LBR 	P  	=#04	; A <- right byte of #04
L_1520:				; branch target; 1 ref
001520:	5523	SBR 	DG 	G+0023	; right byte of [G+0023] <- A
001522:	4420	LEA 	DG 	G+0020	; A <- G+0020  (effective address, memory not read)
001524:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001526:	D750	BRU 	IL 	*V_0050	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0050 = #051A: address = G + #051A, G unknown)
L_1528:				; branch target; 1 ref
001528:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
00152A:	D752	BRU 	IL 	*V_0052	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0052 = #0844: address = G + #0844, G unknown)
L_152C:				; branch target; 1 ref
00152C:	CC07	BAN 	RM 	L_151E	; 7 words backward: conditional branch (BAN) to L_151E
L_152E:				; branch target; 1 ref
00152E:	2DFF	LBL 	P  	=#FF	; A <- left byte of #FF
001530:	DF1A	BRU 	IG 	*TWB_N2	; RETURN from subroutine: branch through return cell TWB_N2 (written by the SPA of the call)
L_1532:				; branch target; 1 ref
001532:	0EDD	LBR 	DL 	V_00DC	; A <- right byte of [V_00DC]
001534:	CD0B	BAZ 	RM 	L_151E	; 11 words backward: conditional branch (BAZ) to L_151E
001536:	2E08	LBR 	P  	=#08	; A <- right byte of #08
001538:	CF0C	BRU 	RM 	L_1520	; 12 words backward: jump to L_1520

; ===== M_KILL: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N0, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: shifts/bit manipulation privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, returns to its CSV caller with RSV
; =====================================================================
; M:KILL Terminating a Task
; 
; * Calling sequence:
; LDA NIV
; CSV M : KILL
; At the time the CSV is executed, register A must contain the level of the task to be terminated.
; 
; * Function:
; — This module forces the task—whose level is specified in register A—to terminate itself.
; — This termination includes cleaning up system tables regarding input/output operations, time delays, and any resources previously held by the terminated task.
; 
; * Output values:
; A, E, X: insignificant
; 
; TWB memory locations used:
; T0 through T19 (a TWB extended to 56 bytes).
; =====================================================================
M_KILL:				; entry point; callee (section / supervisor module / SPA subroutine); 3 refs
00153A:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
00153C:	C705	BRU 	RP 	S_1546	; 5 words forward: CALL subroutine S_1546 (SPA above saved the return address); returns to L_153E  [-> S_1546: shifts/bit manipulation privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, returns through its return cell]
L_153E:				; branch target; 1 ref
00153E:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
001540:	4002	LDA 	DG 	TWB_L	; A <- [TWB_L]
001542:	F70F	CSV 	P  	=#0F	; SUPERVISOR CALL #15 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_ERED  [-> M_ERED: 3 loops, stores into the caller's TWB, returns to its CSV caller with RSV]
001544:	F712	CSV 	P  	=#12	; SUPERVISOR CALL #18 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_ABRT  [-> M_ABRT: shifts/bit manipulation 1 loop, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, ends with DIT/DITR]

; ===== S_1546: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: shifts/bit manipulation privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, returns through its return cell
S_1546:				; callee (section / supervisor module / SPA subroutine); 1 ref
001546:	5116	STA 	DG 	TWB_T7	; [TWB_T7] <- A
001548:	4E17	LBR 	DG 	TWB_T7+1	; A <- right byte of [TWB_T7+1]
00154A:	F001	SLLS	P  	#01
00154C:	F104	XAX 	P  		; exchange A and X
00154E:	A000	LDA 	ILX	*Unknow_1,X	; A <- [G+0000+X]  (pointer Unknow_1 = #0000: address = G + #0000, G unknown)
001550:	0BE8	CMP 	DL 	V_00E8	; compare A with [V_00E8]
001552:	D81E	BCT 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)
001554:	0BE6	CMP 	DL 	V_00E6	; compare A with [V_00E6]
001556:	D81E	BCT 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)
001558:	F908	LDR 	P  	=#08	; LDR operand: #08
00155A:	F0C1	SRLS	P  	#01
00155C:	4D16	LBL 	DG 	TWB_T7	; A <- left byte of [TWB_T7]
00155E:	4B16	CMP 	DG 	TWB_T7	; compare A with [TWB_T7]
001560:	C019	BCT 	RP 	L_1592	; 25 words forward: conditional branch (BCT) to L_1592
001562:	F408	STM 	P  		; privileged control instruction (STM)
001564:	4E17	LBR 	DG 	TWB_T7+1	; A <- right byte of [TWB_T7+1]
001566:	F001	SLLS	P  	#01
001568:	F104	XAX 	P  		; exchange A and X
00156A:	A20A	LDX 	ILX	*CPT_PTR,X	; X <- [G+0380+X]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
00156C:	F206	ICX 	P  	=#06	; ICX operand: #06
00156E:	4016	LDA 	DG 	TWB_T7	; A <- [TWB_T7]
001570:	B130	STA 	ILX	*V_0030,X	; [G+0000+X] <- A  (pointer V_0030 = #0000: address = G + #0000, G unknown)
001572:	2006	LDA 	P  	=#06	; A <- #06
001574:	05A0	ADD 	DL 	V_00A0	; A <- A + [V_00A0]
001576:	F206	ICX 	P  	=#06	; ICX operand: #06
001578:	B130	STA 	ILX	*V_0030,X	; [G+0000+X] <- A  (pointer V_0030 = #0000: address = G + #0000, G unknown)
00157A:	4E17	LBR 	DG 	TWB_T7+1	; A <- right byte of [TWB_T7+1]
00157C:	F001	SLLS	P  	#01
00157E:	C508	BAZ 	RP 	L_158E	; 8 words forward: conditional branch (BAZ) to L_158E
001580:	F11C	CNA 	P  	
001582:	F104	XAX 	P  		; exchange A and X
001584:	20FF	LDA 	P  	=#FF	; A <- #FF
001586:	2D1F	LBL 	P  	=#1F	; A <- left byte of #1F
001588:	A90A	AND 	ILX	*CPT_PTR,X	; A <- A and [G+0380+X]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
00158A:	2121	LDE 	P  	=#21	; E <- #21
00158C:	F403	WD  	P  		; privileged control instruction (WD)
L_158E:				; branch target; 1 ref
00158E:	F400	CLM 	P  		; privileged control instruction (CLM)
001590:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)
L_1592:				; branch target; 1 ref
001592:	4016	LDA 	DG 	TWB_T7	; A <- [TWB_T7]
001594:	5102	STA 	DG 	TWB_L	; [TWB_L] <- A
001596:	2202	LDX 	P  	=#02	; X <- #02
001598:	A4A0	LEA 	ILX	*V_00A0,X	; A <- G+153C+X  (effective address, memory not read)  (pointer V_00A0 = #153C: address = G + #153C, G unknown)
00159A:	5100	STA 	DG 	TWB_P	; [TWB_P] <- A
00159C:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)

; ===== M_KEY: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: privileged I/O or mask ops (CLM/RD/WD/STM), returns to its CSV caller with RSV
; =====================================================================
; M:KEY
;
; Write the contents of register A to the console's "data" indicators (LDX = 1) or to the console's "address" indicators (LDX = 2), or read the console switches to store them in register A (LDX = 0).
; =====================================================================
M_KEY:				; entry point; 1 ref
00159E:	F104	XAX 	P  		; exchange A and X
0015A0:	C40C	BAN 	RP 	L_15B8	; 12 words forward: conditional branch (BAN) to L_15B8
0015A2:	F104	XAX 	P  		; exchange A and X
0015A4:	F303	DCX 	P  	=#03	; DCX operand: #03
0015A6:	C009	BCT 	RP 	L_15B8	; 9 words forward: conditional branch (BCT) to L_15B8
0015A8:	2120	LDE 	P  	=#20	; E <- #20
0015AA:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
0015AC:	C107	BRX 	RP 	$+X	; INDEXED JUMP (BRX): X selects one of the 0 BRU entries that follow (jump / dispatch table)
0015AE:	F402	RD  	P  		; privileged control instruction (RD)
0015B0:	C704	BRU 	RP 	L_15B8	; 4 words forward: jump to L_15B8
0015B2:	2110	LDE 	P  	=#10	; E <- #10
0015B4:	C701	BRU 	RP 	l_15B6	; 1 word forward: jump to l_15B6
l_15B6:				; branch target; 1 ref; only from untraced words
0015B6:	F403	WD  	P  		; privileged control instruction (WD)
L_15B8:				; branch target; 3 refs
0015B8:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== M_ABRT: INTERRUPT ROUTINE (ends with DIT/DITR: leaves through the interrupt scheduler) =====
; routine profile: shifts/bit manipulation 1 loop, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, ends with DIT/DITR
; =====================================================================
; M:ABRT ABNORMAL TASK TERMINATION
; * Call Sequence:
; CSV M:ABRT
; 
; * Function:
; Abandoning the program calling M:ABRT by connecting to a wait context:
; - DIT BRU $-1 if the level is not zero.
; - BRU $ if the level is zero.
;
; TASK TERMINATION
; * Normal termination:
; A task terminates normally via a CSV M:EXIT monitor call. The system then deactivates the calling task's level, preserving its current context to ensure the task can be restarted if the level is reactivated later.
; Consequently, the CSV M:EXIT call should normally be followed by a branch to a task re-initialization sequence.
;
; * Abnormal termination:
; The user can employ the %ABORT command to terminate the task associated with the level specified in the command.
; If an exception occurs within an immediate program, the entire system is re-initialized, as the faulty immediate program has likely disrupted the system as a whole.
; =====================================================================
M_ABRT:				; entry point; callee (section / supervisor module / SPA subroutine); 8 refs
0015BA:	F908	LDR 	P  	=#08	; LDR operand: #08
0015BC:	F0C1	SRLS	P  	#01
0015BE:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0015C0:	D7AA	BRU 	IL 	*V_00AA	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_00AA = #17CC: address = G + #17CC, G unknown)
L_15C2:				; branch target; 1 ref
0015C2:	2108	LDE 	P  	=#08	; E <- #08
0015C4:	F908	LDR 	P  	=#08	; LDR operand: #08
0015C6:	F0C1	SRLS	P  	#01
0015C8:	2B10	CMP 	P  	=#10	; compare A with #10
0015CA:	C203	BOT 	RP 	L_15D0	; 3 words forward: conditional branch (BOT) to L_15D0
0015CC:	2610	SUB 	P  	=#10	; A <- A - #10
0015CE:	2109	LDE 	P  	=#09	; E <- #09
L_15D0:				; branch target; 1 ref
0015D0:	F024	SRCS	P  	#04
0015D2:	F116	AIE 	P  	
0015D4:	F024	SRCS	P  	#04
0015D6:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0015D8:	D75E	BRU 	IL 	*V_005E	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005E = #14F0: address = G + #14F0, G unknown)
L_15DA:				; branch target; 1 ref
0015DA:	F908	LDR 	P  	=#08	; LDR operand: #08
0015DC:	FA05	STR 	P  	=#05	; STR operand: #05
0015DE:	A00A	LDA 	ILX	*CPT_PTR,X	; A <- [G+0380+X]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
0015E0:	FA01	STR 	P  	=#01	; STR operand: #01
0015E2:	0E0E	LBR 	DL 	PRTS_COUNT	; A <- right byte of [PRTS_COUNT]
0015E4:	F002	SLLS	P  	#02
0015E6:	F11C	CNA 	P  	
0015E8:	0508	ADD 	DL 	V_0008	; A <- A + [V_0008]
0015EA:	F102	XAE 	P  		; exchange A and E
0015EC:	0008	LDA 	DL 	V_0008	; A <- [V_0008]
0015EE:	5636	DST 	DG 	G+0036	; [G+0036] <- A,E (2 words)
0015F0:	F901	LDR 	P  	=#01	; LDR operand: #01
0015F2:	2510	ADD 	P  	=#10	; A <- A + #10
0015F4:	0F0F	LBX 	DL 	PRTS_COUNT	; LBX operand: [PRTS_COUNT]
0015F6:	C002	BCT 	RP 	L_15FA	; 2 words forward: conditional branch (BCT) to L_15FA
0015F8:	0510	ADD 	DL 	V_0010	; A <- A + [V_0010]
L_15FA:				; branch target; 1 ref
0015FA:	513A	STA 	DG 	G+003A	; [G+003A] <- A
0015FC:	2000	LDA 	P  	=#00	; A <- #00
0015FE:	FA01	STR 	P  	=#01	; STR operand: #01
001600:	2106	LDE 	P  	=#06	; E <- #06
001602:	4436	LEA 	DG 	G+0036	; A <- G+0036  (effective address, memory not read)
001604:	FA05	STR 	P  	=#05	; STR operand: #05
001606:	F206	ICX 	P  	=#06	; ICX operand: #06
001608:	5818	SPA 	DG 	TWB_N3	; [TWB_N3] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
00160A:	D75C	BRU 	IL 	*V_005C	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005C = #146A: address = G + #146A, G unknown)
L_160C:				; branch target; 1 ref
00160C:	F908	LDR 	P  	=#08	; LDR operand: #08
00160E:	C50C	BAZ 	RP 	L_1626	; 12 words forward: conditional branch (BAZ) to L_1626
001610:	F104	XAX 	P  		; exchange A and X
001612:	01E8	LDE 	DL 	V_00E8	; E <- [V_00E8]
001614:	A00A	LDA 	ILX	*CPT_PTR,X	; A <- [G+0380+X]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
001616:	F408	STM 	P  		; privileged control instruction (STM)
001618:	B20A	STE 	ILX	*CPT_PTR,X	; [G+0380+X] <- E  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
00161A:	2240	LDX 	P  	=#40	; X <- #40
00161C:	B10A	STA 	ILX	*CPT_PTR,X	; [G+0380+X] <- A  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
00161E:	F104	XAX 	P  		; exchange A and X
001620:	FA08	STR 	P  	=#08	; STR operand: #08
001622:	F401	DIT 	P  		; DE-ACTIVATE INTERRUPT: save outgoing context, dispatch the highest-priority waiting level (not a plain return)
001624:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
L_1626:				; branch target; 1 ref
001626:	F408	STM 	P  		; privileged control instruction (STM)
001628:	0ED4	LBR 	DL 	V_00D4	; A <- right byte of [V_00D4]
00162A:	C506	BAZ 	RP 	L_1636	; 6 words forward: conditional branch (BAZ) to L_1636
00162C:	D7B0	BRU 	IL 	*V_00B0	; jump to G+#04E2 (G unknown)  (pointer V_00B0 = #04E2: address = G + #04E2, G unknown)
L_162E:				; branch target; 1 ref
00162E:	2001	LDA 	P  	=#01	; A <- #01
001630:	15D4	SBR 	DL 	V_00D4	; right byte of [V_00D4] <- A
001632:	02EE	LDX 	DL 	V_00EE	; X <- [V_00EE]
001634:	D7B2	BRU 	IL 	*V_00B2	; jump to G+#050A (G unknown)  (pointer V_00B2 = #050A: address = G + #050A, G unknown)
L_1636:				; branch target; 1 ref
001636:	0ED5	LBR 	DL 	V_00D4	; A <- right byte of [V_00D4]
001638:	CB05	BCF 	RM 	L_162E	; 5 words backward: conditional branch (BCF) to L_162E
00163A:	D7B4	BRU 	IL 	*V_00B4	; jump to G+#04E8 (G unknown)  (pointer V_00B4 = #04E8: address = G + #04E8, G unknown)

; ===== M_CMPA: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N0, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: comparisons, 1 loop, stores into the caller's TWB, returns to its CSV caller with RSV
; =====================================================================
; M:CMPA ARITHMETIC COMPARISON OF TWO 16-BIT WORDS
; * Calling sequence:
; LDA FIRST
; LDE SECOND
; CSV M:CMPA
; At the time of the CSV call, register A must contain the first term of the comparison, and register E the second term.
; • Function
; 16-bit arithmetic comparison of the two supplied terms. The CMPA function differs from the CMP instruction in that CMP treats the two terms as signed numbers (15-bit mantissa and one sign bit), whereas CMPA treats them as unsigned numbers (16-bit mantissa). This module is useful when comparing addresses where at least one may exceed 7FFF (hexadecimal).
;
; Output elements
; A < 0 First term < second term
; A = 0 First term = second term
; A > 0 First term > second term
; =====================================================================
M_CMPA:				; entry point; callee (section / supervisor module / SPA subroutine); 3 refs
00163C:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
00163E:	C702	BRU 	RP 	S_1642	; 2 words forward: CALL subroutine S_1642 (SPA above saved the return address); returns to L_1640  [-> S_1642: comparisons, 1 loop, stores into the caller's TWB, returns through its return cell]
L_1640:				; branch target; 1 ref
001640:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== S_1642: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: comparisons, 1 loop, stores into the caller's TWB, returns through its return cell
S_1642:				; callee (section / supervisor module / SPA subroutine); 1 ref
001642:	5208	STE 	DG 	TWB_T0	; [TWB_T0] <- E
001644:	4B08	CMP 	DG 	TWB_T0	; compare A with [TWB_T0]
001646:	C00A	BCT 	RP 	L_165A	; 10 words forward: conditional branch (BCT) to L_165A
001648:	C604	BOF 	RP 	L_1650	; 4 words forward: conditional branch (BOF) to L_1650
00164A:	C404	BAN 	RP 	L_1652	; 4 words forward: conditional branch (BAN) to L_1652
L_164C:				; branch target; 1 ref
00164C:	2DFF	LBL 	P  	=#FF	; A <- left byte of #FF
00164E:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)
L_1650:				; branch target; 1 ref
001650:	C403	BAN 	RP 	L_1656	; 3 words forward: conditional branch (BAN) to L_1656
L_1652:				; branch target; 1 ref
001652:	4008	LDA 	DG 	TWB_T0	; A <- [TWB_T0]
001654:	CC04	BAN 	RM 	L_164C	; 4 words backward: conditional branch (BAN) to L_164C
L_1656:				; branch target; 1 ref
001656:	2001	LDA 	P  	=#01	; A <- #01
001658:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)
L_165A:				; branch target; 1 ref
00165A:	2000	LDA 	P  	=#00	; A <- #00
00165C:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)

; ===== M_DUMP: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N2, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: 6block move / translate string (MVS/TRS) 4 loops, stores into the caller's TWB, returns to its CSV caller with RSV
; =====================================================================
; M:DUMP DUMPING A MEMORY AREA
; * Calling sequence:
; LEA CB
; LDE START
; LDX END
; CSV M:DUMP
; When the CSV is executed, register A must contain the address of the control block relative to G, register E must contain the address of the start of the memory area to be dumped relative to G, and register X must contain the address of the end of the memory area to be dumped relative to G.
; 
; * Function:
; Prints the specified memory area to the device associated with the operational label specified in the CB, at a rate of eight or sixteen words per line, ensuring that a whole number of lines is output.
; The number of words printed per line is determined at system generation. By standard convention, only monitors that include the printer handler will use the 16-word-per-line format.
; 
; * User control block
; CB	DATA,1 0
; 	DATA,1 0
; 	DATA,1 &80
; 	DATA,1 M:xx (Operational label used. If using
; 		MITRAS 1, the binary number itself must be specified).
; 	DATA # BUFFER Address of the user buffer
; 	DATA 54 Minimum size of the buffer provided by the user
; =====================================================================
M_DUMP:				; entry point; callee (section / supervisor module / SPA subroutine); 2 refs
00165E:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001660:	C702	BRU 	RP 	S_1664	; 2 words forward: CALL subroutine S_1664 (SPA above saved the return address); returns to L_1662  [-> S_1664: 6block move / translate string (MVS/TRS) 4 loops, stores into the caller's TWB, returns through its return cell]
L_1662:				; branch target; 1 ref
001662:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== S_1664: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: 6block move / translate string (MVS/TRS) 4 loops, stores into the caller's TWB, returns through its return cell
S_1664:				; callee (section / supervisor module / SPA subroutine); 1 ref
001664:	5112	STA 	DG 	TWB_T5	; [TWB_T5] <- A
001666:	5218	STE 	DG 	TWB_N3	; [TWB_N3] <- E
001668:	5306	STX 	DG 	TWB_ZC	; [TWB_ZC] <- X
00166A:	F902	LDR 	P  	=#02	; LDR operand: #02
00166C:	4518	ADD 	DG 	TWB_N3	; A <- A + [TWB_N3]
00166E:	F0C5	SRLS	P  	#05
001670:	F005	SLLS	P  	#05
001672:	5118	STA 	DG 	TWB_N3	; [TWB_N3] <- A
001674:	F902	LDR 	P  	=#02	; LDR operand: #02
001676:	5706	ADM 	DG 	TWB_ZC	; add A to [TWB_ZC]
001678:	4118	LDE 	DG 	TWB_N3	; E <- [TWB_N3]
00167A:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
00167C:	D75A	BRU 	IL 	*V_005A	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005A = #1642: address = G + #1642, G unknown)
L_167E:				; branch target; 1 ref
00167E:	C435	BAN 	RP 	L_16E8	; 53 words forward: conditional branch (BAN) to L_16E8
001680:	4006	LDA 	DG 	TWB_ZC	; A <- [TWB_ZC]
001682:	4618	SUB 	DG 	TWB_N3	; A <- A - [TWB_N3]
L_1684:				; branch target; 1 ref
001684:	5106	STA 	DG 	TWB_ZC	; [TWB_ZC] <- A
L_1686:				; branch target; 1 ref
001686:	4118	LDE 	DG 	TWB_N3	; E <- [TWB_N3]
001688:	2204	LDX 	P  	=#04	; X <- #04
00168A:	8012	LDA 	IGX	*TWB_T5,X	; A <- [*TWB_T5+X]  (pointer TWB_T5 lives in the calling task's area: its contents are only known at run time)
00168C:	510C	STA 	DG 	TWB_T2	; [TWB_T2] <- A
00168E:	2265	LDX 	P  	=#65	; X <- #65
001690:	2040	LDA 	P  	=#40	; A <- #40
L_1692:				; branch target; 1 ref
001692:	950C	SBR 	IGX	*TWB_T2,X	; right byte of [*TWB_T2+X] <- A  (pointer TWB_T2 lives in the calling task's area: its contents are only known at run time)
001694:	F301	DCX 	P  	=#01	; DCX operand: #01
001696:	C802	BCT 	RM 	L_1692	; 2 words backward: conditional branch (BCT) to L_1692
001698:	2000	LDA 	P  	=#00	; A <- #00
L_169A:				; branch target; 1 ref
00169A:	5114	STA 	DG 	TWB_T6	; [TWB_T6] <- A
00169C:	200E	LDA 	P  	=#0E	; A <- #0E
00169E:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0016A0:	D756	BRU 	IL 	*V_0056	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0056 = #08D8: address = G + #08D8, G unknown)
L_16A2:				; branch target; 1 ref
0016A2:	4214	LDX 	DG 	TWB_T6	; X <- [TWB_T6]
0016A4:	200E	LDA 	P  	=#0E	; A <- #0E
0016A6:	2104	LDE 	P  	=#04	; E <- #04
0016A8:	9F0C	MVS 	IGX	*TWB_T2,X	; MVS operand: [*TWB_T2+X]  (pointer TWB_T2 lives in the calling task's area: its contents are only known at run time)
0016AA:	4014	LDA 	DG 	TWB_T6	; A <- [TWB_T6]
0016AC:	C517	BAZ 	RP 	L_16DA	; 23 words forward: conditional branch (BAZ) to L_16DA
0016AE:	2B62	CMP 	P  	=#62	; compare A with #62
0016B0:	C316	BCF 	RP 	L_16DC	; 22 words forward: conditional branch (BCF) to L_16DC
0016B2:	4012	LDA 	DG 	TWB_T5	; A <- [TWB_T5]
0016B4:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0016B6:	D750	BRU 	IL 	*V_0050	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0050 = #051A: address = G + #051A, G unknown)
L_16B8:				; branch target; 1 ref
0016B8:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0016BA:	D752	BRU 	IL 	*V_0052	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0052 = #0844: address = G + #0844, G unknown)
L_16BC:				; branch target; 1 ref
0016BC:	4108	LDE 	DG 	TWB_T0	; E <- [TWB_T0]
0016BE:	5212	STE 	DG 	TWB_T5	; [TWB_T5] <- E
0016C0:	C409	BAN 	RP 	L_16D2	; 9 words forward: conditional branch (BAN) to L_16D2
0016C2:	4006	LDA 	DG 	TWB_ZC	; A <- [TWB_ZC]
0016C4:	2620	SUB 	P  	=#20	; A <- A - #20
0016C6:	5106	STA 	DG 	TWB_ZC	; [TWB_ZC] <- A
0016C8:	C821	BCT 	RM 	L_1686	; 33 words backward: conditional branch (BCT) to L_1686
0016CA:	6424	LEA 	IL 	*V_0024	; A <- G+7CD6  (effective address, memory not read)  (pointer V_0024 = #7CD6: address = G + #7CD6, G unknown)
0016CC:	5106	STA 	DG 	TWB_ZC	; [TWB_ZC] <- A
0016CE:	2000	LDA 	P  	=#00	; A <- #00
0016D0:	DF1A	BRU 	IG 	*TWB_N2	; RETURN from subroutine: branch through return cell TWB_N2 (written by the SPA of the call)
L_16D2:				; branch target; 1 ref
0016D2:	6424	LEA 	IL 	*V_0024	; A <- G+7CD6  (effective address, memory not read)  (pointer V_0024 = #7CD6: address = G + #7CD6, G unknown)
0016D4:	5106	STA 	DG 	TWB_ZC	; [TWB_ZC] <- A
0016D6:	2D80	LBL 	P  	=#80	; A <- left byte of #80
0016D8:	DF1A	BRU 	IG 	*TWB_N2	; RETURN from subroutine: branch through return cell TWB_N2 (written by the SPA of the call)
L_16DA:				; branch target; 1 ref
0016DA:	2002	LDA 	P  	=#02	; A <- #02
L_16DC:				; branch target; 1 ref
0016DC:	2506	ADD 	P  	=#06	; A <- A + #06
0016DE:	4218	LDX 	DG 	TWB_N3	; X <- [TWB_N3]
0016E0:	A130	LDE 	ILX	*V_0030,X	; E <- [G+0000+X]  (pointer V_0030 = #0000: address = G + #0000, G unknown)
0016E2:	F202	ICX 	P  	=#02	; ICX operand: #02
0016E4:	5318	STX 	DG 	TWB_N3	; [TWB_N3] <- X
0016E6:	CF26	BRU 	RM 	L_169A	; 38 words backward: jump to L_169A
L_16E8:				; branch target; 1 ref
0016E8:	2000	LDA 	P  	=#00	; A <- #00
0016EA:	CF33	BRU 	RM 	L_1684	; 51 words backward: jump to L_1684

; ===== MON21: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, supervisor calls: #18 ..., returns to its CSV caller with RSV
MON21:				; entry point; 1 ref
0016EC:	F712	CSV 	P  	=#12	; SUPERVISOR CALL #18 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_ABRT  [-> M_ABRT: shifts/bit manipulation 1 loop, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, ends with DIT/DITR]

; ===== MON22: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, supervisor calls: #18 ..., returns to its CSV caller with RSV
MON22:				; entry point; 1 ref
0016EE:	F712	CSV 	P  	=#12	; SUPERVISOR CALL #18 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_ABRT  [-> M_ABRT: shifts/bit manipulation 1 loop, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, ends with DIT/DITR]

; ===== MON23: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, supervisor calls: #18, returns to its CSV caller with RSV
MON23:				; entry point; 1 ref
0016F0:	F712	CSV 	P  	=#12	; SUPERVISOR CALL #18 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_ABRT  [-> M_ABRT: shifts/bit manipulation 1 loop, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, ends with DIT/DITR]

; ===== M_TYPE: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, returns to its CSV caller with RSV
; =====================================================================
; M:TYPE
; =====================================================================
M_TYPE:				; entry point; 1 ref
0016F2:	521A	STE 	DG 	TWB_N2	; [TWB_N2] <- E
0016F4:	F408	STM 	P  		; privileged control instruction (STM)
0016F6:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0016F8:	D750	BRU 	IL 	*V_0050	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0050 = #051A: address = G + #051A, G unknown)
L_16FA:				; branch target; 1 ref
0016FA:	401A	LDA 	DG 	TWB_N2	; A <- [TWB_N2]

; ===== MOD_16FC: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N1, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: stores into the caller's TWB, returns to its CSV caller with RSV
MOD_16FC:				; entry point; 1 ref
0016FC:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0016FE:	D750	BRU 	IL 	*V_0050	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0050 = #051A: address = G + #051A, G unknown)
L_1700:				; branch target; 1 ref
001700:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== M_IT: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N0, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: shifts/bit manipulation privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, returns to its CSV caller with RSV
; =====================================================================
; M:IT INTERRUPT SYSTEM OPERATION REQUEST
; * Call sequence:
; LDA = ID
; LDX = CODE
; CSV M:IT
; 
; When the CSV instruction is executed, register A must contain the identifier of the interrupt to be processed—this identifier representing either its level or its configuration, depending on the value of bit 0 of A (0 = level)—and register X must contain the code specifying the operation. The notes regarding the %IT and %AC commands also apply to the use of the M:IT module.
; 
; X (code) details
; 0 Arm
; 1 Disarm
; 2 Enable
; 3 Disable
; 4 Arm and enable
; 5 Disarm and disable
; 6 Trigger
; 7 Acknowledge
; =====================================================================
M_IT:				; entry point; 1 ref
001702:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001704:	C702	BRU 	RP 	S_1708	; 2 words forward: CALL subroutine S_1708 (SPA above saved the return address); returns to L_1706  [-> S_1708: shifts/bit manipulation privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, returns through its return cell]
L_1706:				; branch target; 1 ref
001706:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== S_1708: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: shifts/bit manipulation privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, returns through its return cell
S_1708:				; callee (section / supervisor module / SPA subroutine); 1 ref
001708:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
00170A:	530A	STX 	DG 	TWB_T1	; [TWB_T1] <- X
00170C:	C40B	BAN 	RP 	L_1722	; 11 words forward: conditional branch (BAN) to L_1722
00170E:	C51D	BAZ 	RP 	L_1748	; 29 words forward: conditional branch (BAZ) to L_1748
001710:	2B20	CMP 	P  	=#20	; compare A with #20
001712:	C61B	BOF 	RP 	L_1748	; 27 words forward: conditional branch (BOF) to L_1748
001714:	4208	LDX 	DG 	TWB_T0	; X <- [TWB_T0]
001716:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
001718:	F114	CNX 	P  	
00171A:	A00A	LDA 	ILX	*CPT_PTR,X	; A <- [G+0380+X]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
00171C:	F003	SLLS	P  	#03
00171E:	F0C3	SRLS	P  	#03
001720:	C702	BRU 	RP 	L_1724	; 2 words forward: jump to L_1724
L_1722:				; branch target; 1 ref
001722:	F110	CCA 	P  	
L_1724:				; branch target; 1 ref
001724:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
001726:	2001	LDA 	P  	=#01	; A <- #01
001728:	490A	AND 	DG 	TWB_T1	; A <- A and [TWB_T1]
00172A:	2121	LDE 	P  	=#21	; E <- #21
00172C:	C502	BAZ 	RP 	L_1730	; 2 words forward: conditional branch (BAZ) to L_1730
00172E:	2131	LDE 	P  	=#31	; E <- #31
L_1730:				; branch target; 1 ref
001730:	400A	LDA 	DG 	TWB_T1	; A <- [TWB_T1]
001732:	C40B	BAN 	RP 	L_1748	; 11 words forward: conditional branch (BAN) to L_1748
001734:	2B08	CMP 	P  	=#08	; compare A with #08
001736:	C609	BOF 	RP 	L_1748	; 9 words forward: conditional branch (BOF) to L_1748
001738:	2502	ADD 	P  	=#02	; A <- A + #02
00173A:	F110	CCA 	P  	
00173C:	F0C1	SRLS	P  	#01
00173E:	F00D	SLLS	P  	#0D
001740:	4708	IOR 	DG 	TWB_T0	; A <- A or [TWB_T0]
001742:	F403	WD  	P  		; privileged control instruction (WD)
001744:	2D00	LBL 	P  	=#00	; A <- left byte of #00
001746:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)
L_1748:				; branch target; 4 refs
001748:	2DFF	LBL 	P  	=#FF	; A <- left byte of #FF
00174A:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)

; ===== M_FLAG: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: comparisons, stores into the caller's TWB, supervisor calls: #16, returns to its CSV caller with RSV
; =====================================================================
; M:FLAG
; =====================================================================
M_FLAG:				; entry point; 1 ref
00174C:	F104	XAX 	P  		; exchange A and X
00174E:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
001750:	C405	BAN 	RP 	L_175A	; 5 words forward: conditional branch (BAN) to L_175A
001752:	6E94	LBR 	IL 	*V_0094	; A <- right byte of [G+00DC]  (pointer V_0094 = #00DC: address = G + #00DC, G unknown)
001754:	4B08	CMP 	DG 	TWB_T0	; compare A with [TWB_T0]
001756:	C002	BCT 	RP 	L_175A	; 2 words forward: conditional branch (BCT) to L_175A
001758:	C606	BOF 	RP 	L_1764	; 6 words forward: conditional branch (BOF) to L_1764
L_175A:				; branch target; 2 refs
00175A:	F908	LDR 	P  	=#08	; LDR operand: #08
00175C:	F0C1	SRLS	P  	#01
00175E:	2D53	LBL 	P  	=#53	; A <- left byte of #53
001760:	F710	CSV 	P  	=#10	; SUPERVISOR CALL #16 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_KILL  [-> M_KILL: shifts/bit manipulation privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, returns to its CSV caller with RSV]
001762:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
L_1764:				; branch target; 1 ref
001764:	4008	LDA 	DG 	TWB_T0	; A <- [TWB_T0]
001766:	F104	XAX 	P  		; exchange A and X
001768:	F201	ICX 	P  	=#01	; ICX operand: #01
00176A:	C403	BAN 	RP 	L_1770	; 3 words forward: conditional branch (BAN) to L_1770
00176C:	AE94	LBR 	ILX	*V_0094,X	; A <- right byte of [G+00DC+X]  (pointer V_0094 = #00DC: address = G + #00DC, G unknown)
00176E:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
L_1770:				; branch target; 1 ref
001770:	B594	SBR 	ILX	*V_0094,X	; right byte of [G+00DC+X] <- A  (pointer V_0094 = #00DC: address = G + #00DC, G unknown)
001772:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== M_ADRS: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: comparisons, supervisor calls: #16, returns to its CSV caller with RSV
; =====================================================================
; M:ADRS
; =====================================================================
M_ADRS:				; entry point; callee (section / supervisor module / SPA subroutine); 4 refs
001774:	C407	BAN 	RP 	L_1782	; 7 words forward: conditional branch (BAN) to L_1782
001776:	6B96	CMP 	IL 	*V_0096	; compare A with [G+00E4]  (pointer V_0096 = #00E4: address = G + #00E4, G unknown)
001778:	C60C	BOF 	RP 	L_1790	; 12 words forward: conditional branch (BOF) to L_1790
00177A:	FA05	STR 	P  	=#05	; STR operand: #05
00177C:	E202	ICX 	PX 	(X)	; ICX operand: [(X)]
00177E:	A096	LDA 	ILX	*V_0096,X	; A <- [G+00E4+X]  (pointer V_0096 = #00E4: address = G + #00E4, G unknown)
001780:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
L_1782:				; branch target; 1 ref
001782:	F905	LDR 	P  	=#05	; LDR operand: #05
001784:	C406	BAN 	RP 	L_1790	; 6 words forward: conditional branch (BAN) to L_1790
001786:	6B96	CMP 	IL 	*V_0096	; compare A with [G+00E4]  (pointer V_0096 = #00E4: address = G + #00E4, G unknown)
001788:	C604	BOF 	RP 	L_1790	; 4 words forward: conditional branch (BOF) to L_1790
00178A:	E202	ICX 	PX 	(X)	; ICX operand: [(X)]
00178C:	B296	STE 	ILX	*V_0096,X	; [G+00E4+X] <- E  (pointer V_0096 = #00E4: address = G + #00E4, G unknown)
00178E:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
L_1790:				; branch target; 3 refs
001790:	F908	LDR 	P  	=#08	; LDR operand: #08
001792:	F0C1	SRLS	P  	#01
001794:	2D53	LBL 	P  	=#53	; A <- left byte of #53
001796:	F710	CSV 	P  	=#10	; SUPERVISOR CALL #16 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_KILL  [-> M_KILL: shifts/bit manipulation privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, returns to its CSV caller with RSV]
001798:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== M_CMPS: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N0, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: comparisons, 1 loop, stores into the caller's TWB, returns to its CSV caller with RSV
; =====================================================================
; M:CMPS COMPARISON OF TWO BYTE STRINGS
; * Calling sequence:
; LEA CHAIN2
; XAE
; LEA CHAIN1
; LDX =LENGTH
; CSV M:CMPS
; At the time of the CSV call, register A must contain the G-relative address of the first byte of string 1, register E must contain the G-relative address of the first byte of string 2, and register X must contain the number of bytes to compare.
; 
; * Function:
; The comparison is performed by treating each of the two strings as a number, with the first byte of the string containing the most significant bits.
; 
; * Output results:
; A < 0: string 1 < string 2
; A = 0: string 1 = string 2
; A > 0: string 1 > string 2
; • Work memory locations used: T0 to N0
; =====================================================================
M_CMPS:				; entry point; 1 ref
00179A:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
00179C:	C702	BRU 	RP 	S_17A0	; 2 words forward: CALL subroutine S_17A0 (SPA above saved the return address); returns to L_179E  [-> S_17A0: comparisons, 1 loop, stores into the caller's TWB, returns through its return cell]
L_179E:				; branch target; 1 ref
00179E:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== S_17A0: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: comparisons, 1 loop, stores into the caller's TWB, returns through its return cell
S_17A0:				; callee (section / supervisor module / SPA subroutine); 1 ref
0017A0:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
0017A2:	520A	STE 	DG 	TWB_T1	; [TWB_T1] <- E
0017A4:	530C	STX 	DG 	TWB_T2	; [TWB_T2] <- X
0017A6:	2200	LDX 	P  	=#00	; X <- #00
L_17A8:				; branch target; 1 ref
0017A8:	8E0A	LBR 	IGX	*TWB_T1,X	; A <- right byte of [*TWB_T1+X]  (pointer TWB_T1 lives in the calling task's area: its contents are only known at run time)
0017AA:	510E	STA 	DG 	TWB_T3	; [TWB_T3] <- A
0017AC:	8E08	LBR 	IGX	*TWB_T0,X	; A <- right byte of [*TWB_T0+X]
0017AE:	4B0E	CMP 	DG 	TWB_T3	; compare A with [TWB_T3]
0017B0:	C005	BCT 	RP 	L_17BA	; 5 words forward: conditional branch (BCT) to L_17BA
0017B2:	C202	BOT 	RP 	L_17B6	; 2 words forward: conditional branch (BOT) to L_17B6
0017B4:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)
L_17B6:				; branch target; 1 ref
0017B6:	2D80	LBL 	P  	=#80	; A <- left byte of #80
0017B8:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)
L_17BA:				; branch target; 1 ref
0017BA:	F201	ICX 	P  	=#01	; ICX operand: #01
0017BC:	F905	LDR 	P  	=#05	; LDR operand: #05
0017BE:	4B0C	CMP 	DG 	TWB_T2	; compare A with [TWB_T2]
0017C0:	CB0C	BCF 	RM 	L_17A8	; 12 words backward: conditional branch (BCF) to L_17A8
0017C2:	2000	LDA 	P  	=#00	; A <- #00
0017C4:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)

; ===== M_ABIO: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N2, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: comparisons, 7 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, supervisor calls: #3, returns to its CSV caller with RSV
; =====================================================================
; M:ABIO Réinitialisation des E/S
; =====================================================================
M_ABIO:				; entry point; 1 ref
0017C6:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0017C8:	C702	BRU 	RP 	S_17CC	; 2 words forward: CALL subroutine S_17CC (SPA above saved the return address); returns to L_17CA  [-> S_17CC: comparisons, 7 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, supervisor calls: #3, returns through its return cell]
L_17CA:				; branch target; 1 ref
0017CA:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== S_17CC: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: comparisons, 7 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, supervisor calls: #3, returns through its return cell
S_17CC:				; callee (section / supervisor module / SPA subroutine); 1 ref
0017CC:	550E	SBR 	DG 	TWB_T3	; right byte of [TWB_T3] <- A
0017CE:	6424	LEA 	IL 	*V_0024	; A <- G+7CD6  (effective address, memory not read)  (pointer V_0024 = #7CD6: address = G + #7CD6, G unknown)
0017D0:	5106	STA 	DG 	TWB_ZC	; [TWB_ZC] <- A
0017D2:	6E12	LBR 	IL 	*V_0012	; A <- right byte of [G+225A]  (pointer V_0012 = #225A: address = G + #225A, G unknown)
0017D4:	2501	ADD 	P  	=#01	; A <- A + #01
0017D6:	510A	STA 	DG 	TWB_T1	; [TWB_T1] <- A
0017D8:	F408	STM 	P  		; privileged control instruction (STM)
0017DA:	709C	DLD 	IL 	*V_009C	; A,E <- [G+0234] (2 words)  (pointer V_009C = #0234: address = G + #0234, G unknown)
0017DC:	5514	SBR 	DG 	TWB_T6	; right byte of [TWB_T6] <- A
L_17DE:				; branch target; 2 refs
0017DE:	420A	LDX 	DG 	TWB_T1	; X <- [TWB_T1]
0017E0:	F301	DCX 	P  	=#01	; DCX operand: #01
0017E2:	C336	BCF 	RP 	L_184E	; 54 words forward: conditional branch (BCF) to L_184E
0017E4:	530A	STX 	DG 	TWB_T1	; [TWB_T1] <- X
0017E6:	AE98	LBR 	ILX	*V_0098,X	; A <- right byte of [G+2232+X]  (pointer V_0098 = #2232: address = G + #2232, G unknown)
0017E8:	CD05	BAZ 	RM 	L_17DE	; 5 words backward: conditional branch (BAZ) to L_17DE
0017EA:	510C	STA 	DG 	TWB_T2	; [TWB_T2] <- A
0017EC:	5415	SBL 	DG 	TWB_T6+1	; left byte of [TWB_T6+1] <- A
0017EE:	AE9A	LBR 	ILX	*V_009A,X	; A <- right byte of [G+2246+X]  (pointer V_009A = #2246: address = G + #2246, G unknown)
0017F0:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
0017F2:	420C	LDX 	DG 	TWB_T2	; X <- [TWB_T2]
L_17F4:				; branch target; 1 ref
0017F4:	5312	STX 	DG 	TWB_T5	; [TWB_T5] <- X
0017F6:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
0017F8:	F905	LDR 	P  	=#05	; LDR operand: #05
0017FA:	C524	BAZ 	RP 	L_1842	; 36 words forward: conditional branch (BAZ) to L_1842
0017FC:	A09C	LDA 	ILX	*V_009C,X	; A <- [G+0234+X]  (pointer V_009C = #0234: address = G + #0234, G unknown)
0017FE:	550F	SBR 	DG 	TWB_T3+1	; right byte of [TWB_T3+1] <- A
001800:	093C	AND 	DL 	V_003C	; A <- A and [V_003C]
001802:	4F15	LBX 	DG 	TWB_T6+1	; LBX operand: [TWB_T6+1]
001804:	C303	BCF 	RP 	L_180A	; 3 words forward: conditional branch (BCF) to L_180A
001806:	5515	SBR 	DG 	TWB_T6+1	; right byte of [TWB_T6+1] <- A
001808:	C718	BRU 	RP 	L_1838	; 24 words forward: jump to L_1838
L_180A:				; branch target; 1 ref
00180A:	4B0E	CMP 	DG 	TWB_T3	; compare A with [TWB_T3]
00180C:	C316	BCF 	RP 	L_1838	; 22 words forward: conditional branch (BCF) to L_1838
00180E:	4F14	LBX 	DG 	TWB_T6	; LBX operand: [TWB_T6]
001810:	E201	ICX 	PX 	(X)	; ICX operand: [(X)]
001812:	4012	LDA 	DG 	TWB_T5	; A <- [TWB_T5]
001814:	B59C	SBR 	ILX	*V_009C,X	; right byte of [G+0234+X] <- A  (pointer V_009C = #0234: address = G + #0234, G unknown)
001816:	5514	SBR 	DG 	TWB_T6	; right byte of [TWB_T6] <- A
001818:	4012	LDA 	DG 	TWB_T5	; A <- [TWB_T5]
00181A:	4B0C	CMP 	DG 	TWB_T2	; compare A with [TWB_T2]
00181C:	C304	BCF 	RP 	L_1824	; 4 words forward: conditional branch (BCF) to L_1824
00181E:	4E0F	LBR 	DG 	TWB_T3+1	; A <- right byte of [TWB_T3+1]
001820:	510C	STA 	DG 	TWB_T2	; [TWB_T2] <- A
001822:	C70E	BRU 	RP 	L_183E	; 14 words forward: jump to L_183E
L_1824:				; branch target; 1 ref
001824:	4210	LDX 	DG 	TWB_T4	; X <- [TWB_T4]
001826:	A09C	LDA 	ILX	*V_009C,X	; A <- [G+0234+X]  (pointer V_009C = #0234: address = G + #0234, G unknown)
001828:	4E0F	LBR 	DG 	TWB_T3+1	; A <- right byte of [TWB_T3+1]
00182A:	B19C	STA 	ILX	*V_009C,X	; [G+0234+X] <- A  (pointer V_009C = #0234: address = G + #0234, G unknown)
00182C:	4012	LDA 	DG 	TWB_T5	; A <- [TWB_T5]
00182E:	4B08	CMP 	DG 	TWB_T0	; compare A with [TWB_T0]
001830:	C307	BCF 	RP 	L_183E	; 7 words forward: conditional branch (BCF) to L_183E
001832:	F11E	CHX 	P  	
001834:	5308	STX 	DG 	TWB_T0	; [TWB_T0] <- X
001836:	C704	BRU 	RP 	L_183E	; 4 words forward: jump to L_183E
L_1838:				; branch target; 2 refs
001838:	4212	LDX 	DG 	TWB_T5	; X <- [TWB_T5]
00183A:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
00183C:	5310	STX 	DG 	TWB_T4	; [TWB_T4] <- X
L_183E:				; branch target; 3 refs
00183E:	4F0F	LBX 	DG 	TWB_T3+1	; LBX operand: [TWB_T3+1]
001840:	CF26	BRU 	RM 	L_17F4	; 38 words backward: jump to L_17F4
L_1842:				; branch target; 1 ref
001842:	420A	LDX 	DG 	TWB_T1	; X <- [TWB_T1]
001844:	400C	LDA 	DG 	TWB_T2	; A <- [TWB_T2]
001846:	B598	SBR 	ILX	*V_0098,X	; right byte of [G+2232+X] <- A  (pointer V_0098 = #2232: address = G + #2232, G unknown)
001848:	4008	LDA 	DG 	TWB_T0	; A <- [TWB_T0]
00184A:	B59A	SBR 	ILX	*V_009A,X	; right byte of [G+2246+X] <- A  (pointer V_009A = #2246: address = G + #2246, G unknown)
00184C:	CF37	BRU 	RM 	L_17DE	; 55 words backward: jump to L_17DE
L_184E:				; branch target; 1 ref
00184E:	619C	LDE 	IL 	*V_009C	; E <- [G+0234]  (pointer V_009C = #0234: address = G + #0234, G unknown)
001850:	4E14	LBR 	DG 	TWB_T6	; A <- right byte of [TWB_T6]
001852:	769C	DST 	IL 	*V_009C	; [G+0234] <- A,E (2 words)  (pointer V_009C = #0234: address = G + #0234, G unknown)
001854:	6E12	LBR 	IL 	*V_0012	; A <- right byte of [G+225A]  (pointer V_0012 = #225A: address = G + #225A, G unknown)
001856:	2501	ADD 	P  	=#01	; A <- A + #01
001858:	510A	STA 	DG 	TWB_T1	; [TWB_T1] <- A
L_185A:				; branch target; 4 refs
00185A:	420A	LDX 	DG 	TWB_T1	; X <- [TWB_T1]
00185C:	F301	DCX 	P  	=#01	; DCX operand: #01
00185E:	C355	BCF 	RP 	L_1908	; 85 words forward: conditional branch (BCF) to L_1908
001860:	530A	STX 	DG 	TWB_T1	; [TWB_T1] <- X
001862:	F408	STM 	P  		; privileged control instruction (STM)
001864:	420A	LDX 	DG 	TWB_T1	; X <- [TWB_T1]
001866:	AF98	LBX 	ILX	*V_0098,X	; LBX operand: [G+2232+X]  (pointer V_0098 = #2232: address = G + #2232, G unknown)
001868:	C807	BCT 	RM 	L_185A	; 7 words backward: conditional branch (BCT) to L_185A
00186A:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
00186C:	B09C	DLD 	ILX	*V_009C,X	; A,E <- [G+0234+X] (2 words)  (pointer V_009C = #0234: address = G + #0234, G unknown)
00186E:	F102	XAE 	P  		; exchange A and E
001870:	5612	DST 	DG 	TWB_T5	; [TWB_T5] <- A,E (2 words)
001872:	093C	AND 	DL 	V_003C	; A <- A and [V_003C]
001874:	550F	SBR 	DG 	TWB_T3+1	; right byte of [TWB_T3+1] <- A
001876:	4B0E	CMP 	DG 	TWB_T3	; compare A with [TWB_T3]
001878:	CB0F	BCF 	RM 	L_185A	; 15 words backward: conditional branch (BCF) to L_185A
00187A:	F400	CLM 	P  		; privileged control instruction (CLM)
00187C:	F902	LDR 	P  	=#02	; LDR operand: #02
00187E:	4114	LDE 	DG 	TWB_T6	; E <- [TWB_T6]
001880:	C602	BOF 	RP 	L_1884	; 2 words forward: conditional branch (BOF) to L_1884
001882:	0024	LDA 	DL 	V_0024	; A <- [V_0024]
L_1884:				; branch target; 1 ref
001884:	5712	ADM 	DG 	TWB_T5	; add A to [TWB_T5]
001886:	4212	LDX 	DG 	TWB_T5	; X <- [TWB_T5]
001888:	AE36	LBR 	ILX	*V_0036,X	; A <- right byte of [G+0003+X]  (pointer V_0036 = #0003: address = G + #0003, G unknown)
00188A:	FA05	STR 	P  	=#05	; STR operand: #05
00188C:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
00188E:	F908	LDR 	P  	=#08	; LDR operand: #08
001890:	A11A	LDE 	ILX	*V_001A,X	; E <- [G+22A6+X]  (pointer V_001A = #22A6: address = G + #22A6, G unknown)
001892:	5220	STE 	DG 	G+0020	; [G+0020] <- E
001894:	420A	LDX 	DG 	TWB_T1	; X <- [TWB_T1]
001896:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
001898:	A014	LDA 	ILX	*V_0014,X	; A <- [*V_0014+X]
00189A:	5122	STA 	DG 	G+0022	; [G+0022] <- A
00189C:	4020	LDA 	DG 	G+0020	; A <- [G+0020]
00189E:	2906	AND 	P  	=#06	; A <- A and #06
0018A0:	C50E	BAZ 	RP 	L_18BC	; 14 words forward: conditional branch (BAZ) to L_18BC
0018A2:	5122	STA 	DG 	G+0022	; [G+0022] <- A
0018A4:	4E20	LBR 	DG 	G+0020	; A <- right byte of [G+0020]
0018A6:	F0C3	SRLS	P  	#03
0018A8:	FA05	STR 	P  	=#05	; STR operand: #05
0018AA:	E203	ICX 	PX 	(X)	; ICX operand: [(X)]
0018AC:	AF1E	LBX 	ILX	*V_001E,X	; LBX operand: [G+2224+X]  (pointer V_001E = #2224: address = G + #2224, G unknown)
0018AE:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
0018B0:	A014	LDA 	ILX	*V_0014,X	; A <- [*V_0014+X]
0018B2:	5722	ADM 	DG 	G+0022	; add A to [G+0022]
0018B4:	4222	LDX 	DG 	G+0022	; X <- [G+0022]
0018B6:	F21E	ICX 	P  	=#1E	; ICX operand: #1E
0018B8:	A030	LDA 	ILX	*V_0030,X	; A <- [G+0000+X]  (pointer V_0030 = #0000: address = G + #0000, G unknown)
0018BA:	C702	BRU 	RP 	L_18BE	; 2 words forward: jump to L_18BE
L_18BC:				; branch target; 1 ref
0018BC:	4022	LDA 	DG 	G+0022	; A <- [G+0022]
L_18BE:				; branch target; 1 ref
0018BE:	FA01	STR 	P  	=#01	; STR operand: #01
0018C0:	2000	LDA 	P  	=#00	; A <- #00
0018C2:	0116	LDE 	DL 	V_0016	; E <- [V_0016]
0018C4:	FA01	STR 	P  	=#01	; STR operand: #01
0018C6:	C313	BCF 	RP 	L_18EC	; 19 words forward: conditional branch (BCF) to L_18EC
0018C8:	4120	LDE 	DG 	G+0020	; E <- [G+0020]
0018CA:	400A	LDA 	DG 	TWB_T1	; A <- [TWB_T1]
0018CC:	520A	STE 	DG 	TWB_T1	; [TWB_T1] <- E
0018CE:	410E	LDE 	DG 	TWB_T3	; E <- [TWB_T3]
0018D0:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
0018D2:	401A	LDA 	DG 	TWB_N2	; A <- [TWB_N2]
0018D4:	5124	STA 	DG 	G+0024	; [G+0024] <- A
0018D6:	4012	LDA 	DG 	TWB_T5	; A <- [TWB_T5]
0018D8:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
0018DA:	2000	LDA 	P  	=#00	; A <- #00
0018DC:	F703	CSV 	P  	=#03	; SUPERVISOR CALL #3 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry MON3  [-> MON3: privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_0798, returns to its CSV caller with RSV]
0018DE:	4024	LDA 	DG 	G+0024	; A <- [G+0024]
0018E0:	511A	STA 	DG 	TWB_N2	; [TWB_N2] <- A
0018E2:	4020	LDA 	DG 	G+0020	; A <- [G+0020]
0018E4:	510E	STA 	DG 	TWB_T3	; [TWB_T3] <- A
0018E6:	4022	LDA 	DG 	G+0022	; A <- [G+0022]
0018E8:	510A	STA 	DG 	TWB_T1	; [TWB_T1] <- A
0018EA:	CF48	BRU 	RM 	L_185A	; 72 words backward: jump to L_185A
L_18EC:				; branch target; 1 ref
0018EC:	4020	LDA 	DG 	G+0020	; A <- [G+0020]
0018EE:	4112	LDE 	DG 	TWB_T5	; E <- [TWB_T5]
0018F0:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0018F2:	D7AE	BRU 	IL 	*V_00AE	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_00AE = #1B86: address = G + #1B86, G unknown)
L_18F4:				; branch target; 1 ref
0018F4:	4212	LDX 	DG 	TWB_T5	; X <- [TWB_T5]
0018F6:	F201	ICX 	P  	=#01	; ICX operand: #01
0018F8:	2E02	LBR 	P  	=#02	; A <- right byte of #02
0018FA:	A730	IOR 	ILX	*V_0030,X	; A <- A or [G+0000+X]  (pointer V_0030 = #0000: address = G + #0000, G unknown)
0018FC:	B530	SBR 	ILX	*V_0030,X	; right byte of [G+0000+X] <- A  (pointer V_0030 = #0000: address = G + #0000, G unknown)
0018FE:	6430	LEA 	IL 	*V_0030	; A <- G+0000  (effective address, memory not read)  (pointer V_0030 = #0000: address = G + #0000, G unknown)
001900:	4512	ADD 	DG 	TWB_T5	; A <- A + [TWB_T5]
001902:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001904:	D752	BRU 	IL 	*V_0052	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0052 = #0844: address = G + #0844, G unknown)
L_1906:				; branch target; 1 ref
001906:	CF56	BRU 	RM 	L_185A	; 86 words backward: jump to L_185A
L_1908:				; branch target; 1 ref
001908:	F400	CLM 	P  		; privileged control instruction (CLM)
00190A:	DF1A	BRU 	IG 	*TWB_N2	; RETURN from subroutine: branch through return cell TWB_N2 (written by the SPA of the call)

; ===== M_CNEC: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: stores into the caller's TWB, returns to its CSV caller with RSV
; =====================================================================
; M:CNEC CONNECTING A CONTEXT TO A LEVEL
; * Call sequence:
; LDA ADCTX ; Context address relative to the caller's G-base
; LDX =R ; Interrupt level
; CSV M:CNEC
; 
; At the time of the CSV call, register A must contain the address (relative to the calling program's G-base) of the context to be connected, and register X must contain the interrupt level to which the context is to be connected.
; 
; * Function:
; Connects a context to a level so that it is activated upon the arrival of an interrupt at that level. The M:CNEC module is normally used after the M:LOAD module.
; 
; * TWB memory locations used: TO to NO
; =====================================================================
M_CNEC:				; entry point; 1 ref
00190C:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
00190E:	F902	LDR 	P  	=#02	; LDR operand: #02
001910:	5708	ADM 	DG 	TWB_T0	; add A to [TWB_T0]
001912:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
001914:	B10A	STA 	ILX	*CPT_PTR,X	; [G+0380+X] <- A  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
001916:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== M_OVLD: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: comparisons, 1 loop, stores into the caller's TWB, returns to its CSV caller with RSV
; =====================================================================
; M:OVLD
; =====================================================================
M_OVLD:				; entry point; 1 ref
001918:	2120	LDE 	P  	=#20	; E <- #20
00191A:	200E	LDA 	P  	=#0E	; A <- #0E
00191C:	2D10	LBL 	P  	=#10	; A <- left byte of #10
00191E:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
001920:	2002	LDA 	P  	=#02	; A <- #02
001922:	5532	SBR 	DG 	G+0032	; right byte of [G+0032] <- A
001924:	2000	LDA 	P  	=#00	; A <- #00
001926:	2D01	LBL 	P  	=#01	; A <- left byte of #01
001928:	512E	STA 	DG 	G+002E	; [G+002E] <- A
00192A:	F908	LDR 	P  	=#08	; LDR operand: #08
00192C:	FA05	STR 	P  	=#05	; STR operand: #05
00192E:	A00A	LDA 	ILX	*CPT_PTR,X	; A <- [G+0380+X]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
001930:	250E	ADD 	P  	=#0E	; A <- A + #0E
001932:	510A	STA 	DG 	TWB_T1	; [TWB_T1] <- A
001934:	6430	LEA 	IL 	*V_0030	; A <- G+0000  (effective address, memory not read)  (pointer V_0030 = #0000: address = G + #0000, G unknown)
001936:	570A	ADM 	DG 	TWB_T1	; add A to [TWB_T1]
001938:	F902	LDR 	P  	=#02	; LDR operand: #02
00193A:	4502	ADD 	DG 	TWB_L	; A <- A + [TWB_L]
00193C:	FA01	STR 	P  	=#01	; STR operand: #01
00193E:	511C	STA 	DG 	TWB_N1	; [TWB_N1] <- A
001940:	4004	LDA 	DG 	TWB_IND	; A <- [TWB_IND]
001942:	2904	AND 	P  	=#04	; A <- A and #04
001944:	C503	BAZ 	RP 	L_194A	; 3 words forward: conditional branch (BAZ) to L_194A
001946:	6100	LDE 	IL 	*Unknow_1	; E <- [G+0000]  (pointer Unknow_1 = #0000: address = G + #0000, G unknown)
001948:	C705	BRU 	RP 	L_1952	; 5 words forward: jump to L_1952
L_194A:				; branch target; 1 ref
00194A:	0000	LDA 	DL 	Unknow_1	; A <- [Unknow_1]
00194C:	5114	STA 	DG 	TWB_T6	; [TWB_T6] <- A
00194E:	2200	LDX 	P  	=#00	; X <- #00
001950:	8114	LDE 	IGX	*TWB_T6,X	; E <- [*TWB_T6+X]  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
L_1952:				; branch target; 1 ref
001952:	5208	STE 	DG 	TWB_T0	; [TWB_T0] <- E
001954:	4E08	LBR 	DG 	TWB_T0	; A <- right byte of [TWB_T0]
001956:	2BF8	CMP 	P  	=#F8	; compare A with #F8
001958:	F102	XAE 	P  		; exchange A and E
00195A:	C002	BCT 	RP 	L_195E	; 2 words forward: conditional branch (BCT) to L_195E
00195C:	050A	ADD 	DL 	CPT_PTR	; A <- A + [CPT_PTR]
L_195E:				; branch target; 1 ref
00195E:	0610	SUB 	DL 	V_0010	; A <- A - [V_0010]
001960:	5508	SBR 	DG 	TWB_T0	; right byte of [TWB_T0] <- A
001962:	1012	DLD 	DL 	V_0012	; A,E <- [V_0012] (2 words)
001964:	5614	DST 	DG 	TWB_T6	; [TWB_T6] <- A,E (2 words)
001966:	0000	LDA 	DL 	Unknow_1	; A <- [Unknow_1]
001968:	2602	SUB 	P  	=#02	; A <- A - #02
00196A:	1100	STA 	DL 	Unknow_1	; [Unknow_1] <- A
00196C:	0016	LDA 	DL 	V_0016	; A <- [V_0016]
00196E:	512C	STA 	DG 	G+002C	; [G+002C] <- A
001970:	4F08	LBX 	DG 	TWB_T0	; LBX operand: [TWB_T0]
001972:	8E16	LBR 	IGX	*TWB_T7,X	; A <- right byte of [*TWB_T7+X]  (pointer TWB_T7 lives in the calling task's area: its contents are only known at run time)
001974:	F001	SLLS	P  	#01
001976:	510C	STA 	DG 	TWB_T2	; [TWB_T2] <- A
001978:	FA05	STR 	P  	=#05	; STR operand: #05
00197A:	900A	DLD 	IGX	*TWB_T1,X	; A,E <- [*TWB_T1+X] (2 words)  (pointer TWB_T1 lives in the calling task's area: its contents are only known at run time)
00197C:	5638	DST 	DG 	G+0038	; [G+0038] <- A,E (2 words)
00197E:	4538	ADD 	DG 	G+0038	; A <- A + [G+0038]
001980:	513C	STA 	DG 	G+003C	; [G+003C] <- A
001982:	F302	DCX 	P  	=#02	; DCX operand: #02
001984:	800A	LDA 	IGX	*TWB_T1,X	; A <- [*TWB_T1+X]  (pointer TWB_T1 lives in the calling task's area: its contents are only known at run time)
001986:	512A	STA 	DG 	G+002A	; [G+002A] <- A
001988:	2201	LDX 	P  	=#01	; X <- #01
L_198A:				; branch target; 1 ref
00198A:	2000	LDA 	P  	=#00	; A <- #00
00198C:	FA01	STR 	P  	=#01	; STR operand: #01
00198E:	5310	STX 	DG 	TWB_T4	; [TWB_T4] <- X
001990:	8F16	LBX 	IGX	*TWB_T7,X	; LBX operand: [*TWB_T7+X]  (pointer TWB_T7 lives in the calling task's area: its contents are only known at run time)
001992:	C027	BCT 	RP 	L_19E0	; 39 words forward: conditional branch (BCT) to L_19E0
001994:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
001996:	530E	STX 	DG 	TWB_T3	; [TWB_T3] <- X
001998:	900A	DLD 	IGX	*TWB_T1,X	; A,E <- [*TWB_T1+X] (2 words)  (pointer TWB_T1 lives in the calling task's area: its contents are only known at run time)
00199A:	850A	ADD 	IGX	*TWB_T1,X	; A <- A + [*TWB_T1+X]  (pointer TWB_T1 lives in the calling task's area: its contents are only known at run time)
00199C:	4138	LDE 	DG 	G+0038	; E <- [G+0038]
00199E:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0019A0:	D75A	BRU 	IL 	*V_005A	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005A = #1642: address = G + #1642, G unknown)
L_19A2:				; branch target; 1 ref
0019A2:	C41C	BAN 	RP 	L_19DA	; 28 words forward: conditional branch (BAN) to L_19DA
0019A4:	C51B	BAZ 	RP 	L_19DA	; 27 words forward: conditional branch (BAZ) to L_19DA
0019A6:	800A	LDA 	IGX	*TWB_T1,X	; A <- [*TWB_T1+X]  (pointer TWB_T1 lives in the calling task's area: its contents are only known at run time)
0019A8:	413C	LDE 	DG 	G+003C	; E <- [G+003C]
0019AA:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0019AC:	D75A	BRU 	IL 	*V_005A	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005A = #1642: address = G + #1642, G unknown)
L_19AE:				; branch target; 1 ref
0019AE:	C402	BAN 	RP 	L_19B2	; 2 words forward: conditional branch (BAN) to L_19B2
0019B0:	C715	BRU 	RP 	L_19DA	; 21 words forward: jump to L_19DA
L_19B2:				; branch target; 1 ref
0019B2:	401C	LDA 	DG 	TWB_N1	; A <- [TWB_N1]
0019B4:	FA01	STR 	P  	=#01	; STR operand: #01
0019B6:	4010	LDA 	DG 	TWB_T4	; A <- [TWB_T4]
0019B8:	0510	ADD 	DL 	V_0010	; A <- A + [V_0010]
0019BA:	F002	SLLS	P  	#02
0019BC:	F11C	CNA 	P  	
0019BE:	5112	STA 	DG 	TWB_T5	; [TWB_T5] <- A
0019C0:	F902	LDR 	P  	=#02	; LDR operand: #02
0019C2:	5712	ADM 	DG 	TWB_T5	; add A to [TWB_T5]
0019C4:	400E	LDA 	DG 	TWB_T3	; A <- [TWB_T3]
0019C6:	4B0C	CMP 	DG 	TWB_T2	; compare A with [TWB_T2]
0019C8:	C306	BCF 	RP 	L_19D4	; 6 words forward: conditional branch (BCF) to L_19D4
0019CA:	4210	LDX 	DG 	TWB_T4	; X <- [TWB_T4]
0019CC:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
0019CE:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
0019D0:	9014	DLD 	IGX	*TWB_T6,X	; A,E <- [*TWB_T6+X] (2 words)  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
0019D2:	C702	BRU 	RP 	L_19D6	; 2 words forward: jump to L_19D6
L_19D4:				; branch target; 1 ref
0019D4:	1004	DLD 	DL 	V_0004	; A,E <- [V_0004] (2 words)
L_19D6:				; branch target; 1 ref
0019D6:	4212	LDX 	DG 	TWB_T5	; X <- [TWB_T5]
0019D8:	B608	DST 	ILX	*V_0008,X	; [G+0000+X] <- A,E (2 words)  (pointer V_0008 = #0000: address = G + #0000, G unknown)
L_19DA:				; branch target; 3 refs
0019DA:	4210	LDX 	DG 	TWB_T4	; X <- [TWB_T4]
0019DC:	F201	ICX 	P  	=#01	; ICX operand: #01
0019DE:	CF2A	BRU 	RM 	L_198A	; 42 words backward: jump to L_198A
L_19E0:				; branch target; 1 ref
0019E0:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== M_BIB: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N3, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: comparisons, 3 loops, stores into the caller's TWB, returns to its CSV caller with RSV
; =====================================================================
; M:BIB
; =====================================================================
M_BIB:				; entry point; 1 ref
0019E2:	5818	SPA 	DG 	TWB_N3	; [TWB_N3] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0019E4:	C702	BRU 	RP 	S_19E8	; 2 words forward: CALL subroutine S_19E8 (SPA above saved the return address); returns to L_19E6  [-> S_19E8: comparisons, 3 loops, stores into the caller's TWB, returns through its return cell]
L_19E6:				; branch target; 1 ref
0019E6:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== S_19E8: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: comparisons, 3 loops, stores into the caller's TWB, returns through its return cell
S_19E8:				; callee (section / supervisor module / SPA subroutine); 1 ref
0019E8:	5136	STA 	DG 	G+0036	; [G+0036] <- A
0019EA:	5224	STE 	DG 	G+0024	; [G+0024] <- E
0019EC:	21A0	LDE 	P  	=#A0	; E <- #A0
0019EE:	F905	LDR 	P  	=#05	; LDR operand: #05
0019F0:	2D10	LBL 	P  	=#10	; A <- left byte of #10
0019F2:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
0019F4:	2000	LDA 	P  	=#00	; A <- #00
0019F6:	2101	LDE 	P  	=#01	; E <- #01
0019F8:	5628	DST 	DG 	G+0028	; [G+0028] <- A,E (2 words)
0019FA:	2D01	LBL 	P  	=#01	; A <- left byte of #01
0019FC:	5126	STA 	DG 	G+0026	; [G+0026] <- A
L_19FE:				; branch target; 1 ref
0019FE:	4420	LEA 	DG 	G+0020	; A <- G+0020  (effective address, memory not read)
001A00:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001A02:	D750	BRU 	IL 	*V_0050	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0050 = #051A: address = G + #051A, G unknown)
L_1A04:				; branch target; 1 ref
001A04:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001A06:	D752	BRU 	IL 	*V_0052	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0052 = #0844: address = G + #0844, G unknown)
L_1A08:				; branch target; 1 ref
001A08:	C425	BAN 	RP 	L_1A52	; 37 words forward: conditional branch (BAN) to L_1A52
001A0A:	4228	LDX 	DG 	G+0028	; X <- [G+0028]
001A0C:	F301	DCX 	P  	=#01	; DCX operand: #01
001A0E:	C305	BCF 	RP 	L_1A18	; 5 words forward: conditional branch (BCF) to L_1A18
001A10:	5328	STX 	DG 	G+0028	; [G+0028] <- X
001A12:	2201	LDX 	P  	=#01	; X <- #01
001A14:	8E24	LBR 	IGX	*G+0024,X	; A <- right byte of [*G+0024+X]  (pointer G+0024 lives in the calling task's area: its contents are only known at run time)
001A16:	5138	STA 	DG 	G+0038	; [G+0038] <- A
L_1A18:				; branch target; 1 ref
001A18:	4024	LDA 	DG 	G+0024	; A <- [G+0024]
001A1A:	2504	ADD 	P  	=#04	; A <- A + #04
001A1C:	510A	STA 	DG 	TWB_T1	; [TWB_T1] <- A
001A1E:	2211	LDX 	P  	=#11	; X <- #11
L_1A20:				; branch target; 1 ref
001A20:	5308	STX 	DG 	TWB_T0	; [TWB_T0] <- X
001A22:	4238	LDX 	DG 	G+0038	; X <- [G+0038]
001A24:	F301	DCX 	P  	=#01	; DCX operand: #01
001A26:	F106	XEX 	P  		; exchange E and X
001A28:	C003	BCT 	RP 	L_1A2E	; 3 words forward: conditional branch (BCT) to L_1A2E
001A2A:	2DFF	LBL 	P  	=#FF	; A <- left byte of #FF
001A2C:	C719	BRU 	RP 	L_1A5E	; 25 words forward: jump to L_1A5E
L_1A2E:				; branch target; 1 ref
001A2E:	2204	LDX 	P  	=#04	; X <- #04
L_1A30:				; branch target; 1 ref
001A30:	800A	LDA 	IGX	*TWB_T1,X	; A <- [*TWB_T1+X]  (pointer TWB_T1 lives in the calling task's area: its contents are only known at run time)
001A32:	C50A	BAZ 	RP 	L_1A46	; 10 words forward: conditional branch (BAZ) to L_1A46
001A34:	8B36	CMP 	IGX	*G+0036,X	; compare A with [*G+0036+X]  (pointer G+0036 lives in the calling task's area: its contents are only known at run time)
001A36:	C307	BCF 	RP 	L_1A44	; 7 words forward: conditional branch (BCF) to L_1A44
001A38:	F302	DCX 	P  	=#02	; DCX operand: #02
001A3A:	C805	BCT 	RM 	L_1A30	; 5 words backward: conditional branch (BCT) to L_1A30
001A3C:	F20C	ICX 	P  	=#0C	; ICX operand: #0C
001A3E:	900A	DLD 	IGX	*TWB_T1,X	; A,E <- [*TWB_T1+X] (2 words)  (pointer TWB_T1 lives in the calling task's area: its contents are only known at run time)
001A40:	420A	LDX 	DG 	TWB_T1	; X <- [TWB_T1]
001A42:	DF18	BRU 	IG 	*TWB_N3	; RETURN from subroutine: branch through return cell TWB_N3 (written by the SPA of the call)
L_1A44:				; branch target; 1 ref
001A44:	5238	STE 	DG 	G+0038	; [G+0038] <- E
L_1A46:				; branch target; 1 ref
001A46:	200E	LDA 	P  	=#0E	; A <- #0E
001A48:	570A	ADM 	DG 	TWB_T1	; add A to [TWB_T1]
001A4A:	4208	LDX 	DG 	TWB_T0	; X <- [TWB_T0]
001A4C:	F301	DCX 	P  	=#01	; DCX operand: #01
001A4E:	C817	BCT 	RM 	L_1A20	; 23 words backward: conditional branch (BCT) to L_1A20
001A50:	CF29	BRU 	RM 	L_19FE	; 41 words backward: jump to L_19FE
L_1A52:				; branch target; 1 ref
001A52:	4E20	LBR 	DG 	G+0020	; A <- right byte of [G+0020]
001A54:	F0C4	SRLS	P  	#04
001A56:	250D	ADD 	P  	=#0D	; A <- A + #0D
001A58:	F108	XAA 	P  	
001A5A:	581A	SPA 	DG 	TWB_N2	; [TWB_N2] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001A5C:	D75E	BRU 	IL 	*V_005E	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_005E = #14F0: address = G + #14F0, G unknown)
L_1A5E:				; branch target; 2 refs
001A5E:	DF18	BRU 	IG 	*TWB_N3	; RETURN from subroutine: branch through return cell TWB_N3 (written by the SPA of the call)

; ===== M_ZIO: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: stores into the caller's TWB, returns to its CSV caller with RSV
; =====================================================================
; M:ZIO REQUEST FOR ENTRY/EXIT IN COMMON AREA
;
; Call sequence:
; LDA ACBZC (Address of I/O control block Ci)
; CSV M:ZIO
; 
; At the time of CSV execution, register A must contain the address relative to the start of the common area of ​​the I/O control block.
; 
; * Function:
; Execution of I/O requests (see I/O organization under MTR control).
; 
; * CB (Control Block) description:
; The CB used, located in the common area, is identical to the CB used by M:I0, except that the user buffer address must be expressed relative to the start of the common area (rather than relative to G). The branch address for user-monitored errors (if specified) remains relative to G.
; 
; * Output information:
; Identical to the information provided by M:I0, except that A contains the CB address relative to the common area.
; 
; * Memory locations used: T0 to N0
; * Modules called: M:I0
:
; An I/O request is not addressed directly to a handler, as this would make I/O programming permanently dependent on a specific physical environment.
; To overcome this constraint, a system of "operational labels" was adopted, allowing programs to operate within a logical environment.
; An I/O request is addressed to an operational label. The mapping between the operational label and the handler is managed by the M:I0 module; this mapping is established when the monitor is generated and can be modified using the % ASSIGN command.
; A Control Block (CB) is associated with the call to the supervisor (CSV) for a transfer request. This control block contains user-defined parameters. Upon completion of the transfer, it also holds status information returned by the system (see the CB description in the M:I0 section).
; At the time of the call, register A must contain the address of the CB. For M:I0, this address is relative to the calling program's G-base.
; The monitor associates a dynamic event—defined by the CB address—with the transfer request; this corresponds to bit 0 of byte 0 of the CB:
; - Call acknowledgment: event deactivated (bit 0 of byte 0 of the CB set to 1)
; - Transfer completion: event activated (bit 0 of byte 0 of the CB set to 0)
; - Requests are queued (one queue per coupler).
; The monitor returns control to the user program after queuing the request.
; The actual initiation of the transfer takes place as the relevant coupler becomes available.
; The user can then call the M:WAIT module (to wait for event activation, where the event is the completion of the transfer).
; =====================================================================
M_ZIO:				; entry point; 1 ref
001A60:	F102	XAE 	P  		; exchange A and E
001A62:	2000	LDA 	P  	=#00	; A <- #00
001A64:	2D80	LBL 	P  	=#80	; A <- left byte of #80
001A66:	F102	XAE 	P  		; exchange A and E

; ===== MOD_1A68: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N1, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: stores into the caller's TWB, returns to its CSV caller with RSV
MOD_1A68:				; entry point; 1 ref
001A68:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001A6A:	D7B6	BRU 	IL 	*L_00B6	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer L_00B6 = #051C: address = G + #051C, G unknown)
L_1A6C:				; branch target; 1 ref
001A6C:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== M_ZWAT: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: stores into the caller's TWB, returns to its CSV caller with RSV
; =====================================================================
; M:ZWAT WAIT FOR COMPLETION OF TRANSFER IN COMMON AREA
; *Call sequence:
; LDA ACBZC ; (Command block address)
; CSV M:ZWAT
; 
; At the time of the CSV execution, register A must contain the relative address of the start of the common area of ​​the I/O command block to be monitored.
; 
; *Function:
; Identical to that of the M:WAIT module.
; 
; *Output data:
; Identical to the data returned by M:WAIT.
; 
; *TWB memory locations used: TO to NO
; *Modules called: M:WAIT
; =====================================================================
M_ZWAT:				; entry point; 1 ref
001A6E:	4506	ADD 	DG 	TWB_ZC	; A <- A + [TWB_ZC]

; ===== MOD_1A70: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N1, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: stores into the caller's TWB, returns to its CSV caller with RSV
MOD_1A70:				; entry point; 1 ref
001A70:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001A72:	D752	BRU 	IL 	*V_0052	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0052 = #0844: address = G + #0844, G unknown)
L_1A74:				; branch target; 1 ref
001A74:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
001A76:	4506	ADD 	DG 	TWB_ZC	; A <- A + [TWB_ZC]
001A78:	D752	BRU 	IL 	*V_0052	; jump to G+#0844 (G unknown)  (pointer V_0052 = #0844: address = G + #0844, G unknown)

; ===== M_XI: program entry point =====
; routine profile: 1 instruction
; =====================================================================
; M:XI
; =====================================================================
M_XI:				; entry point; branch target; 2 refs
001A7A:	C700	BRU 	RP 	M_XI	; 0 words forward: jump to M_XI  [-> M_XI: 1 instruction]

; ===== M_ASGN: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: shifts/bit manipulation 9 loops, stores into the caller's TWB, returns to its CSV caller with RSV
; =====================================================================
; M:ASGN ASSIGNATION DYNAMIQUE
; 
; LEA CTB (Table de contrôle)
; CSV M:ASGN
; 
; Au moment de l'exécution du CSV, le registre A doit contenir l'adresse relative à G d'une table de contrôle. La table de contrôle doit avoir une adresse paire et contenir les informations suivantes :
; CTB	OL	MODE
; CTB + 2	NOM,
; CTB + 4	Zéro	D
; CTB + 6	UEM	MODE
; 
; OL: Numéro de l'étiquette opérationnelle à assigner-
; NOM: : Nom du périphérique en EBCDIC (2 caractères)
; D : Numéro du périphérique
; Si le coupleur est monopériphérique, mettre zéro.
; UEM: Numéro de l'unité de traitement (0 si unité centrale)
; 
; * Fonction:
; Traitement identique à celui d'une commande %ASSIGN. L'étiquette opérationnelle
; M:OC n'est pas assignable.
; .
; Eléments communiqués en sortie dans X
; 0 = Assignation correcte
; 1= Tableau CTB erroné
; 2 = Assignation interdite
; 
; 
; * Modules appelés: Néant
; =====================================================================
M_ASGN:				; entry point; callee (section / supervisor module / SPA subroutine); 2 refs
001A7C:	5114	STA 	DG 	TWB_T6	; [TWB_T6] <- A
001A7E:	2200	LDX 	P  	=#00	; X <- #00
001A80:	8E14	LBR 	IGX	*TWB_T6,X	; A <- right byte of [*TWB_T6+X]  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
001A82:	C309	BCF 	RP 	L_1A94	; 9 words forward: conditional branch (BCF) to L_1A94
001A84:	C703	BRU 	RP 	L_1A8A	; 3 words forward: jump to L_1A8A
L_1A86:				; branch target; 1 ref
001A86:	2200	LDX 	P  	=#00	; X <- #00
001A88:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
L_1A8A:				; branch target; 3 refs
001A8A:	2201	LDX 	P  	=#01	; X <- #01
001A8C:	C702	BRU 	RP 	L_1A90	; 2 words forward: jump to L_1A90
L_1A8E:				; branch target; 6 refs
001A8E:	2202	LDX 	P  	=#02	; X <- #02
L_1A90:				; branch target; 1 ref
001A90:	2D81	LBL 	P  	=#81	; A <- left byte of #81
001A92:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
L_1A94:				; branch target; 1 ref
001A94:	2206	LDX 	P  	=#06	; X <- #06
001A96:	8E14	LBR 	IGX	*TWB_T6,X	; A <- right byte of [*TWB_T6+X]  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
001A98:	5116	STA 	DG 	TWB_T7	; [TWB_T7] <- A
001A9A:	2202	LDX 	P  	=#02	; X <- #02
001A9C:	8114	LDE 	IGX	*TWB_T6,X	; E <- [*TWB_T6+X]  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
001A9E:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001AA0:	D77C	BRU 	IL 	*V_007C	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_007C = #1B1E: address = G + #1B1E, G unknown)
L_1AA2:				; branch target; 1 ref
001AA2:	CC0C	BAN 	RM 	L_1A8A	; 12 words backward: conditional branch (BAN) to L_1A8A
001AA4:	5312	STX 	DG 	TWB_T5	; [TWB_T5] <- X
001AA6:	F202	ICX 	P  	=#02	; ICX operand: #02
001AA8:	AE1E	LBR 	ILX	*V_001E,X	; A <- right byte of [G+2224+X]  (pointer V_001E = #2224: address = G + #2224, G unknown)
001AAA:	510A	STA 	DG 	TWB_T1	; [TWB_T1] <- A
001AAC:	2200	LDX 	P  	=#00	; X <- #00
001AAE:	8E14	LBR 	IGX	*TWB_T6,X	; A <- right byte of [*TWB_T6+X]  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
001AB0:	2B04	CMP 	P  	=#04	; compare A with #04
001AB2:	C812	BCT 	RM 	L_1A8E	; 18 words backward: conditional branch (BCT) to L_1A8E
001AB4:	2B0B	CMP 	P  	=#0B	; compare A with #0B
001AB6:	C203	BOT 	RP 	L_1ABC	; 3 words forward: conditional branch (BOT) to L_1ABC
001AB8:	2B11	CMP 	P  	=#11	; compare A with #11
001ABA:	CA16	BOT 	RM 	L_1A8E	; 22 words backward: conditional branch (BOT) to L_1A8E
L_1ABC:				; branch target; 1 ref
001ABC:	6B16	CMP 	IL 	*V_0016	; compare A with [G+2270]  (pointer V_0016 = #2270: address = G + #2270, G unknown)
001ABE:	CE18	BOF 	RM 	L_1A8E	; 24 words backward: conditional branch (BOF) to L_1A8E
001AC0:	510C	STA 	DG 	TWB_T2	; [TWB_T2] <- A
001AC2:	FA05	STR 	P  	=#05	; STR operand: #05
001AC4:	AE18	LBR 	ILX	*V_0018,X	; A <- right byte of [G+2294+X]  (pointer V_0018 = #2294: address = G + #2294, G unknown)
001AC6:	29F0	AND 	P  	=#F0	; A <- A and #F0
001AC8:	490A	AND 	DG 	TWB_T1	; A <- A and [TWB_T1]
001ACA:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
001ACC:	2B40	CMP 	P  	=#40	; compare A with #40
001ACE:	CA20	BOT 	RM 	L_1A8E	; 32 words backward: conditional branch (BOT) to L_1A8E
001AD0:	2930	AND 	P  	=#30	; A <- A and #30
001AD2:	CD22	BAZ 	RM 	L_1A8E	; 34 words backward: conditional branch (BAZ) to L_1A8E
001AD4:	5110	STA 	DG 	TWB_T4	; [TWB_T4] <- A
001AD6:	2201	LDX 	P  	=#01	; X <- #01
001AD8:	8E14	LBR 	IGX	*TWB_T6,X	; A <- right byte of [*TWB_T6+X]  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
001ADA:	510A	STA 	DG 	TWB_T1	; [TWB_T1] <- A
001ADC:	2910	AND 	P  	=#10	; A <- A and #10
001ADE:	C502	BAZ 	RP 	L_1AE2	; 2 words forward: conditional branch (BAZ) to L_1AE2
001AE0:	2040	LDA 	P  	=#40	; A <- #40
L_1AE2:				; branch target; 1 ref
001AE2:	2540	ADD 	P  	=#40	; A <- A + #40
001AE4:	4908	AND 	DG 	TWB_T0	; A <- A and [TWB_T0]
001AE6:	CD2C	BAZ 	RM 	L_1A8E	; 44 words backward: conditional branch (BAZ) to L_1A8E
001AE8:	2205	LDX 	P  	=#05	; X <- #05
001AEA:	8E14	LBR 	IGX	*TWB_T6,X	; A <- right byte of [*TWB_T6+X]  (pointer TWB_T6 lives in the calling task's area: its contents are only known at run time)
001AEC:	510E	STA 	DG 	TWB_T3	; [TWB_T3] <- A
001AEE:	4012	LDA 	DG 	TWB_T5	; A <- [TWB_T5]
001AF0:	F003	SLLS	P  	#03
001AF2:	450E	ADD 	DG 	TWB_T3	; A <- A + [TWB_T3]
001AF4:	F006	SLLS	P  	#06
001AF6:	4516	ADD 	DG 	TWB_T7	; A <- A + [TWB_T7]
001AF8:	F001	SLLS	P  	#01
001AFA:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
001AFC:	400A	LDA 	DG 	TWB_T1	; A <- [TWB_T1]
001AFE:	2910	AND 	P  	=#10	; A <- A and #10
001B00:	C503	BAZ 	RP 	L_1B06	; 3 words forward: conditional branch (BAZ) to L_1B06
001B02:	2001	LDA 	P  	=#01	; A <- #01
001B04:	5708	ADM 	DG 	TWB_T0	; add A to [TWB_T0]
L_1B06:				; branch target; 1 ref
001B06:	420C	LDX 	DG 	TWB_T2	; X <- [TWB_T2]
001B08:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
001B0A:	400A	LDA 	DG 	TWB_T1	; A <- [TWB_T1]
001B0C:	4108	LDE 	DG 	TWB_T0	; E <- [TWB_T0]
001B0E:	2904	AND 	P  	=#04	; A <- A and #04
001B10:	C506	BAZ 	RP 	L_1B1C	; 6 words forward: conditional branch (BAZ) to L_1B1C
001B12:	400A	LDA 	DG 	TWB_T1	; A <- [TWB_T1]
001B14:	2920	AND 	P  	=#20	; A <- A and #20
001B16:	C501	BAZ 	RP 	L_1B18	; 1 word forward: conditional branch (BAZ) to L_1B18
L_1B18:				; branch target; 1 ref
001B18:	B21A	STE 	ILX	*V_001A,X	; [G+22A6+X] <- E  (pointer V_001A = #22A6: address = G + #22A6, G unknown)
001B1A:	CF4A	BRU 	RM 	L_1A86	; 74 words backward: jump to L_1A86
L_1B1C:				; branch target; 1 ref
001B1C:	CF49	BRU 	RM 	L_1A8A	; 73 words backward: jump to L_1A8A
001B1E:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
001B20:	2000	LDA 	P  	=#00	; A <- #00
001B22:	510A	STA 	DG 	TWB_T1	; [TWB_T1] <- A
001B24:	510C	STA 	DG 	TWB_T2	; [TWB_T2] <- A
l_1B26:				; branch target; 1 ref; only from untraced words
001B26:	420C	LDX 	DG 	TWB_T2	; X <- [TWB_T2]
001B28:	A01C	LDA 	ILX	*V_001C,X	; A <- [G+2214+X]  (pointer V_001C = #2214: address = G + #2214, G unknown)
001B2A:	C417	BAN 	RP 	l_1B58	; 23 words forward: conditional branch (BAN) to l_1B58
001B2C:	F001	SLLS	P  	#01
001B2E:	2502	ADD 	P  	=#02	; A <- A + #02
001B30:	570C	ADM 	DG 	TWB_T2	; add A to [TWB_T2]
001B32:	4208	LDX 	DG 	TWB_T0	; X <- [TWB_T0]
001B34:	F301	DCX 	P  	=#01	; DCX operand: #01
001B36:	C304	BCF 	RP 	l_1B3E	; 4 words forward: conditional branch (BCF) to l_1B3E
001B38:	5308	STX 	DG 	TWB_T0	; [TWB_T0] <- X
001B3A:	510A	STA 	DG 	TWB_T1	; [TWB_T1] <- A
001B3C:	CF0B	BRU 	RM 	l_1B26	; 11 words backward: jump to l_1B26
l_1B3E:				; branch target; 1 ref; only from untraced words
001B3E:	2202	LDX 	P  	=#02	; X <- #02
001B40:	420C	LDX 	DG 	TWB_T2	; X <- [TWB_T2]
l_1B42:				; branch target; 1 ref; only from untraced words
001B42:	F302	DCX 	P  	=#02	; DCX operand: #02
001B44:	F905	LDR 	P  	=#05	; LDR operand: #05
001B46:	4B0A	CMP 	DG 	TWB_T1	; compare A with [TWB_T1]
001B48:	C008	BCT 	RP 	l_1B58	; 8 words forward: conditional branch (BCT) to l_1B58
001B4A:	F904	LDR 	P  	=#04	; LDR operand: #04
001B4C:	AB1C	CMP 	ILX	*V_001C,X	; compare A with [G+2214+X]  (pointer V_001C = #2214: address = G + #2214, G unknown)
001B4E:	C002	BCT 	RP 	l_1B52	; 2 words forward: conditional branch (BCT) to l_1B52
001B50:	CF07	BRU 	RM 	l_1B42	; 7 words backward: jump to l_1B42
l_1B52:				; branch target; 1 ref; only from untraced words
001B52:	2000	LDA 	P  	=#00	; A <- #00
001B54:	F302	DCX 	P  	=#02	; DCX operand: #02
001B56:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)
l_1B58:				; branch target; 2 refs; only from untraced words
001B58:	2D80	LBL 	P  	=#80	; A <- left byte of #80
001B5A:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)

; ===== MON37: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: stores into the caller's TWB, returns to its CSV caller with RSV
MON37:				; entry point; 1 ref
001B5C:	F002	SLLS	P  	#02
001B5E:	5104	STA 	DG 	TWB_IND	; [TWB_IND] <- A
001B60:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
001B62:	0020	LDA 	DL 	V_0020	; A <- [V_0020]
001B64:	000D	LDA 	DL 	PRTS_PTR	; A <- [PRTS_PTR]
001B66:	0020	LDA 	DL 	V_0020	; A <- [V_0020]
001B68:	0500	ADD 	DL 	Unknow_1	; A <- A + [Unknow_1]
001B6A:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
001B6C:	0050	LDA 	DL 	V_0050	; A <- [V_0050]
001B6E:	EB5C	LDP 	PX 	(X)	; LDP operand: [(X)]
001B70:	338C	DCX 	DL 	v_008C	; DCX operand: [v_008C]

; ===== MON38: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: stores into the caller's TWB, returns to its CSV caller with RSV
MON38:				; entry point; callee (section / supervisor module / SPA subroutine); 2 refs
001B72:	000C	LDA 	DL 	PRTS_PTR	; A <- [PRTS_PTR]
001B74:	5102	STA 	DG 	TWB_L	; [TWB_L] <- A
001B76:	0400	LEA 	DL 	Unknow_1	; A <- Unknow_1  (effective address, memory not read)
001B78:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== MON39: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: returns to its CSV caller with RSV
MON39:				; entry point; 1 ref
001B7A:	2000	LDA 	P  	=#00	; A <- #00
001B7C:	2200	LDX 	P  	=#00	; X <- #00

; ===== MON40: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: 1 instruction, returns to its CSV caller with RSV
MON40:				; entry point; 1 ref
001B7E:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== MON49: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N0, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: shifts/bit manipulation returns to its CSV caller with RSV
MON49:				; entry point; 1 ref
001B80:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001B82:	C702	BRU 	RP 	S_1B86	; 2 words forward: CALL subroutine S_1B86 (SPA above saved the return address); returns to L_1B84  [-> S_1B86: shifts/bit manipulation returns through its return cell]
L_1B84:				; branch target; 1 ref
001B84:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== S_1B86: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: shifts/bit manipulation returns through its return cell
S_1B86:				; callee (section / supervisor module / SPA subroutine); 1 ref
001B86:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
001B88:	4022	LDA 	DG 	G+0022	; A <- [G+0022]
001B8A:	2920	AND 	P  	=#20	; A <- A and #20
001B8C:	C30B	BCF 	RP 	L_1BA2	; 11 words forward: conditional branch (BCF) to L_1BA2
001B8E:	4022	LDA 	DG 	G+0022	; A <- [G+0022]
001B90:	F0C1	SRLS	P  	#01
001B92:	2903	AND 	P  	=#03	; A <- A and #03
001B94:	C311	BCF 	RP 	L_1BB6	; 17 words forward: conditional branch (BCF) to L_1BB6
001B96:	4E22	LBR 	DG 	G+0022	; A <- right byte of [G+0022]
001B98:	F0C3	SRLS	P  	#03
001B9A:	2B02	CMP 	P  	=#02	; compare A with #02
001B9C:	C202	BOT 	RP 	L_1BA0	; 2 words forward: conditional branch (BOT) to L_1BA0
001B9E:	2601	SUB 	P  	=#01	; A <- A - #01
L_1BA0:				; branch target; 1 ref
001BA0:	C70D	BRU 	RP 	L_1BBA	; 13 words forward: jump to L_1BBA
L_1BA2:				; branch target; 1 ref
001BA2:	4022	LDA 	DG 	G+0022	; A <- [G+0022]
001BA4:	F0C1	SRLS	P  	#01
001BA6:	2903	AND 	P  	=#03	; A <- A and #03
001BA8:	C307	BCF 	RP 	L_1BB6	; 7 words forward: conditional branch (BCF) to L_1BB6
001BAA:	6214	LDX 	IL 	*V_0014	; X <- [*V_0014]
001BAC:	4022	LDA 	DG 	G+0022	; A <- [G+0022]
001BAE:	2910	AND 	P  	=#10	; A <- A and #10
001BB0:	C312	BCF 	RP 	L_1BD4	; 18 words forward: conditional branch (BCF) to L_1BD4
001BB2:	F301	DCX 	P  	=#01	; DCX operand: #01
001BB4:	C710	BRU 	RP 	L_1BD4	; 16 words forward: jump to L_1BD4
L_1BB6:				; branch target; 2 refs
001BB6:	6514	ADD 	IL 	*V_0014	; A <- A + [*V_0014]
001BB8:	2605	SUB 	P  	=#05	; A <- A - #05
L_1BBA:				; branch target; 1 ref
001BBA:	F001	SLLS	P  	#01
001BBC:	5124	STA 	DG 	G+0024	; [G+0024] <- A
001BBE:	4022	LDA 	DG 	G+0022	; A <- [G+0022]
001BC0:	2920	AND 	P  	=#20	; A <- A and #20
001BC2:	C30B	BCF 	RP 	L_1BD8	; 11 words forward: conditional branch (BCF) to L_1BD8
001BC4:	4E22	LBR 	DG 	G+0022	; A <- right byte of [G+0022]
001BC6:	F0C3	SRLS	P  	#03
001BC8:	FA05	STR 	P  	=#05	; STR operand: #05
001BCA:	E202	ICX 	PX 	(X)	; ICX operand: [(X)]
001BCC:	AE1E	LBR 	ILX	*V_001E,X	; A <- right byte of [G+2224+X]  (pointer V_001E = #2224: address = G + #2224, G unknown)
001BCE:	2901	AND 	P  	=#01	; A <- A and #01
001BD0:	C50E	BAZ 	RP 	L_1BEC	; 14 words forward: conditional branch (BAZ) to L_1BEC
001BD2:	C703	BRU 	RP 	L_1BD8	; 3 words forward: jump to L_1BD8
L_1BD4:				; branch target; 2 refs
001BD4:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
001BD6:	5324	STX 	DG 	G+0024	; [G+0024] <- X
L_1BD8:				; branch target; 2 refs
001BD8:	4224	LDX 	DG 	G+0024	; X <- [G+0024]
001BDA:	A014	LDA 	ILX	*V_0014,X	; A <- [*V_0014+X]
001BDC:	FA01	STR 	P  	=#01	; STR operand: #01
001BDE:	5020	DLD 	DG 	G+0020	; A,E <- [G+0020] (2 words)
001BE0:	1834	SPA 	DL 	V_0034	; [V_0034] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001BE2:	D732	BRU 	IL 	*V_0032	; RETURN from subroutine: branch through return cell V_0032 (written by the SPA of the call)
L_1BE4:				; branch target; 1 ref
001BE4:	2000	LDA 	P  	=#00	; A <- #00
001BE6:	FA01	STR 	P  	=#01	; STR operand: #01
001BE8:	2001	LDA 	P  	=#01	; A <- #01
001BEA:	C702	BRU 	RP 	L_1BEE	; 2 words forward: jump to L_1BEE
L_1BEC:				; branch target; 1 ref
001BEC:	2000	LDA 	P  	=#00	; A <- #00
L_1BEE:				; branch target; 1 ref
001BEE:	4224	LDX 	DG 	G+0024	; X <- [G+0024]
001BF0:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)

; ===== MON50: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: stores into the caller's TWB, calls S_1C00, returns to its CSV caller with RSV
MON50:				; entry point; 1 ref
001BF2:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
001BF4:	F902	LDR 	P  	=#02	; LDR operand: #02
001BF6:	5708	ADM 	DG 	TWB_T0	; add A to [TWB_T0]

; ===== MOD_1BF8: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N0, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: comparisons, 1 loop, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, returns to its CSV caller with RSV
MOD_1BF8:				; entry point; 1 ref
001BF8:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001BFA:	C703	BRU 	RP 	S_1C00	; 3 words forward: CALL subroutine S_1C00 (SPA above saved the return address); returns to L_1BFC  [-> S_1C00: comparisons, 1 loop, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, returns through its return cell]
L_1BFC:				; branch target; 1 ref
001BFC:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
001BFE:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A

; ===== S_1C00: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: comparisons, 1 loop, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, returns through its return cell
S_1C00:				; callee (section / supervisor module / SPA subroutine); 1 ref
001C00:	520C	STE 	DG 	TWB_T2	; [TWB_T2] <- E
001C02:	4208	LDX 	DG 	TWB_T0	; X <- [TWB_T0]
001C04:	F408	STM 	P  		; privileged control instruction (STM)
001C06:	AD18	LBL 	ILX	*V_0018,X	; A <- left byte of [G+2294+X]  (pointer V_0018 = #2294: address = G + #2294, G unknown)
001C08:	C62A	BOF 	RP 	L_1C5C	; 42 words forward: conditional branch (BOF) to L_1C5C
001C0A:	AE18	LBR 	ILX	*V_0018,X	; A <- right byte of [G+2294+X]  (pointer V_0018 = #2294: address = G + #2294, G unknown)
001C0C:	297F	AND 	P  	=#7F	; A <- A and #7F
001C0E:	F001	SLLS	P  	#01
001C10:	C512	BAZ 	RP 	L_1C34	; 18 words forward: conditional branch (BAZ) to L_1C34
001C12:	2B41	CMP 	P  	=#41	; compare A with #41
001C14:	C625	BOF 	RP 	L_1C5E	; 37 words forward: conditional branch (BOF) to L_1C5E
001C16:	F400	CLM 	P  		; privileged control instruction (CLM)
001C18:	F11C	CNA 	P  	
001C1A:	F104	XAX 	P  		; exchange A and X
001C1C:	A028	LDA 	ILX	*V_0028,X	; A <- [G+1E7E+X]  (pointer V_0028 = #1E7E: address = G + #1E7E, G unknown)
001C1E:	0326	EOR 	DL 	V_0026	; A <- A xor [V_0026]
001C20:	2121	LDE 	P  	=#21	; E <- #21
001C22:	F408	STM 	P  		; privileged control instruction (STM)
001C24:	F403	WD  	P  		; privileged control instruction (WD)
001C26:	4208	LDX 	DG 	TWB_T0	; X <- [TWB_T0]
001C28:	AE18	LBR 	ILX	*V_0018,X	; A <- right byte of [G+2294+X]  (pointer V_0018 = #2294: address = G + #2294, G unknown)
001C2A:	297F	AND 	P  	=#7F	; A <- A and #7F
001C2C:	F104	XAX 	P  		; exchange A and X
001C2E:	AE30	LBR 	ILX	*V_0030,X	; A <- right byte of [G+0000+X]  (pointer V_0030 = #0000: address = G + #0000, G unknown)
001C30:	2501	ADD 	P  	=#01	; A <- A + #01
001C32:	B530	SBR 	ILX	*V_0030,X	; right byte of [G+0000+X] <- A  (pointer V_0030 = #0000: address = G + #0000, G unknown)
L_1C34:				; branch target; 1 ref
001C34:	4208	LDX 	DG 	TWB_T0	; X <- [TWB_T0]
001C36:	400C	LDA 	DG 	TWB_T2	; A <- [TWB_T2]
001C38:	B518	SBR 	ILX	*V_0018,X	; right byte of [G+2294+X] <- A  (pointer V_0018 = #2294: address = G + #2294, G unknown)
001C3A:	AE1A	LBR 	ILX	*V_001A,X	; A <- right byte of [G+22A6+X]  (pointer V_001A = #22A6: address = G + #22A6, G unknown)
001C3C:	2908	AND 	P  	=#08	; A <- A and #08
001C3E:	C50F	BAZ 	RP 	L_1C5C	; 15 words forward: conditional branch (BAZ) to L_1C5C
001C40:	AE1E	LBR 	ILX	*V_001E,X	; A <- right byte of [G+2224+X]  (pointer V_001E = #2224: address = G + #2224, G unknown)
001C42:	C002	BCT 	RP 	L_1C46	; 2 words forward: conditional branch (BCT) to L_1C46
001C44:	F206	ICX 	P  	=#06	; ICX operand: #06
L_1C46:				; branch target; 1 ref
001C46:	A024	LDA 	ILX	*V_0024,X	; A <- [G+7CD6+X]  (pointer V_0024 = #7CD6: address = G + #7CD6, G unknown)
001C48:	C50B	BAZ 	RP 	L_1C5E	; 11 words forward: conditional branch (BAZ) to L_1C5E
001C4A:	2B1E	CMP 	P  	=#1E	; compare A with #1E
001C4C:	C609	BOF 	RP 	L_1C5E	; 9 words forward: conditional branch (BOF) to L_1C5E
001C4E:	FA05	STR 	P  	=#05	; STR operand: #05
001C50:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
001C52:	F114	CNX 	P  	
001C54:	A028	LDA 	ILX	*V_0028,X	; A <- [G+1E7E+X]  (pointer V_0028 = #1E7E: address = G + #1E7E, G unknown)
001C56:	2121	LDE 	P  	=#21	; E <- #21
001C58:	F403	WD  	P  		; privileged control instruction (WD)
001C5A:	2000	LDA 	P  	=#00	; A <- #00
L_1C5C:				; branch target; 3 refs
001C5C:	DF1E	BRU 	IG 	*TWB_N0	; RETURN from subroutine: branch through return cell TWB_N0 (written by the SPA of the call)
L_1C5E:				; branch target; 3 refs
001C5E:	2DFF	LBL 	P  	=#FF	; A <- left byte of #FF
001C60:	CF02	BRU 	RM 	L_1C5C	; 2 words backward: jump to L_1C5C

; ===== MON51: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: stores into the caller's TWB, returns to its CSV caller with RSV
MON51:				; entry point; 1 ref
001C62:	3500	ICL 	DL 	Unknow_1	; ICL operand: [Unknow_1]
001C64:	0524	ADD 	DL 	V_0024	; A <- A + [V_0024]
001C66:	356A	ICL 	DL 	V_006A	; ICL operand: [V_006A]

; ===== MOD_1C68: SUPERVISOR MODULE ENTRY: SPA saves the return address in TWB_N0, BRU runs the module body as a subroutine, RSV (reached when the body returns) goes back to the CSV caller =====
; routine profile: stores into the caller's TWB, returns to its CSV caller with RSV
MOD_1C68:				; entry point; 1 ref
001C68:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001C6A:	D732	BRU 	IL 	*V_0032	; RETURN from subroutine: branch through return cell V_0032 (written by the SPA of the call)
L_1C6C:				; branch target; 1 ref
001C6C:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== MON52: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: 1comparisons, stores into the caller's TWB, supervisor calls: #18, returns to its CSV caller with RSV
MON52:				; entry point; 1 ref
001C6E:	F712	CSV 	P  	=#12	; SUPERVISOR CALL #18 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_ABRT  [-> M_ABRT: shifts/bit manipulation 1 loop, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, ends with DIT/DITR]

; ===== MON53: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: 1comparisons, stores into the caller's TWB, returns to its CSV caller with RSV
MON53:				; entry point; 1 ref
001C70:	C50F	BAZ 	RP 	L_1C8E	; 15 words forward: conditional branch (BAZ) to L_1C8E
001C72:	C40E	BAN 	RP 	L_1C8E	; 14 words forward: conditional branch (BAN) to L_1C8E
001C74:	6B16	CMP 	IL 	*V_0016	; compare A with [G+2270]  (pointer V_0016 = #2270: address = G + #2270, G unknown)
001C76:	C60C	BOF 	RP 	L_1C8E	; 12 words forward: conditional branch (BOF) to L_1C8E
001C78:	FA05	STR 	P  	=#05	; STR operand: #05
001C7A:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
001C7C:	A11A	LDE 	ILX	*V_001A,X	; E <- [G+22A6+X]  (pointer V_001A = #22A6: address = G + #22A6, G unknown)
001C7E:	5208	STE 	DG 	TWB_T0	; [TWB_T0] <- E
001C80:	4E08	LBR 	DG 	TWB_T0	; A <- right byte of [TWB_T0]
001C82:	F0C3	SRLS	P  	#03
001C84:	FA05	STR 	P  	=#05	; STR operand: #05
001C86:	E202	ICX 	PX 	(X)	; ICX operand: [(X)]
001C88:	A01C	LDA 	ILX	*V_001C,X	; A <- [G+2214+X]  (pointer V_001C = #2214: address = G + #2214, G unknown)
001C8A:	2200	LDX 	P  	=#00	; X <- #00
001C8C:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
L_1C8E:				; branch target; 3 refs
001C8E:	2201	LDX 	P  	=#01	; X <- #01
001C90:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV

; ===== MON54: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: 1 instruction, returns to its CSV caller with RSV
MON54:				; entry point; 1 ref
001C92:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
001C94:	1E3A	TRS 	DL 	V_003A	; TRS operand: [V_003A]
001C96:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
001C98:	9004	DLD 	IGX	*TWB_IND,X	; A,E <- [*TWB_IND+X] (2 words)  (pointer TWB_IND lives in the calling task's area: its contents are only known at run time)
001C9A:	1CA2	FMU 	DL 	v_00A2	; floating multiply by [v_00A2]
001C9C:	0017	LDA 	DL 	V_0016	; A <- [V_0016]
001C9E:	.EBCDIC	"DRCS"	; 2 words, raw: C4D9 C3E2
001CA2:	0D15	LBL 	DL 	V_0014	; A <- left byte of [V_0014]
001CA4:	.EBCDIC	"%%      "	; 4 words, raw: 6C6C 4040 4040 4040
001CAC:	0D15	LBL 	DL 	V_0014	; A <- left byte of [V_0014]
001CAE:	.EBCDIC	"SYS READ"	; 4 words, raw: E2E8 E240 D9C5 C1C4
001CB6:	E80D	CLS 	PX 	(X)	; CALL SECTION, number taken from [(X)] (CLS); return = RTS
001CB8:	1500	SBR 	DL 	Unknow_1	; right byte of [Unknow_1] <- A
001CBA:	.EBCDIC	"    "	; 2 words, raw: 4040 4040

; ===== MON55: INTERRUPT ROUTINE (ends with DIT/DITR: leaves through the interrupt scheduler) =====
; routine profile: shifts/bit manipulation 11 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, supervisor calls: #38 #6 #2 #0 ..., ends with DIT/DITR
MON55:				; entry point; 1 ref
001CBE:	F902	LDR 	P  	=#02	; LDR operand: #02
001CC0:	C079	BCT 	RP 	L_1DB2	; 121 words forward: conditional branch (BCT) to L_1DB2
001CC2:	F408	STM 	P  		; privileged control instruction (STM)
001CC4:	521A	STE 	DG 	TWB_N2	; [TWB_N2] <- E
001CC6:	F904	LDR 	P  	=#04	; LDR operand: #04
001CC8:	C005	BCT 	RP 	L_1CD2	; 5 words forward: conditional branch (BCT) to L_1CD2
001CCA:	1026	DLD 	DL 	V_0026	; A,E <- [V_0026] (2 words)
001CCC:	1614	DST 	DL 	V_0014	; [V_0014] <- A,E (2 words)
001CCE:	000C	LDA 	DL 	PRTS_PTR	; A <- [PRTS_PTR]
001CD0:	C70C	BRU 	RP 	L_1CE8	; 12 words forward: jump to L_1CE8
L_1CD2:				; branch target; 1 ref
001CD2:	2402	LEA 	P  	=#02	; A <- #02  (effective address, memory not read)
001CD4:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
001CD6:	F908	LDR 	P  	=#08	; LDR operand: #08
001CD8:	F0C1	SRLS	P  	#01
001CDA:	2201	LDX 	P  	=#01	; X <- #01
001CDC:	8D08	LBL 	IGX	*TWB_T0,X	; A <- left byte of [*TWB_T0+X]
001CDE:	F108	XAA 	P  	
001CE0:	FA04	STR 	P  	=#04	; STR operand: #04
001CE2:	0414	LEA 	DL 	V_0014	; A <- V_0014  (effective address, memory not read)
001CE4:	F706	CSV 	P  	=#06	; SUPERVISOR CALL #6 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_BNHX  [-> M_BNHX: comparisons, 1 loop, stores into the caller's TWB, returns to its CSV caller with RSV]
001CE6:	000A	LDA 	DL 	CPT_PTR	; A <- [CPT_PTR]
L_1CE8:				; branch target; 1 ref
001CE8:	1112	STA 	DL 	V_0012	; [V_0012] <- A
001CEA:	040E	LEA 	DL 	PRTS_COUNT	; A <- PRTS_COUNT  (effective address, memory not read)
001CEC:	1106	STA 	DL 	V_0006	; [V_0006] <- A
001CEE:	F901	LDR 	P  	=#01	; LDR operand: #01
001CF0:	5118	STA 	DG 	TWB_N3	; [TWB_N3] <- A
001CF2:	4618	SUB 	DG 	TWB_N3	; A <- A - [TWB_N3]
001CF4:	FA01	STR 	P  	=#01	; STR operand: #01
001CF6:	01E6	LDE 	DL 	V_00E6	; E <- [V_00E6]
001CF8:	720A	STE 	IL 	*CPT_PTR	; [G+0380] <- E  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
001CFA:	2240	LDX 	P  	=#40	; X <- #40
001CFC:	00F6	LDA 	DL 	V_00F6	; A <- [V_00F6]
001CFE:	B10A	STA 	ILX	*CPT_PTR,X	; [G+0380+X] <- A  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
001D00:	2200	LDX 	P  	=#00	; X <- #00
001D02:	5314	STX 	DG 	TWB_T6	; [TWB_T6] <- X
L_1D04:				; branch target; 2 refs
001D04:	4214	LDX 	DG 	TWB_T6	; X <- [TWB_T6]
001D06:	F202	ICX 	P  	=#02	; ICX operand: #02
001D08:	F905	LDR 	P  	=#05	; LDR operand: #05
001D0A:	2B3C	CMP 	P  	=#3C	; compare A with #3C
001D0C:	C01A	BCT 	RP 	L_1D40	; 26 words forward: conditional branch (BCT) to L_1D40
001D0E:	5314	STX 	DG 	TWB_T6	; [TWB_T6] <- X
001D10:	A00A	LDA 	ILX	*CPT_PTR,X	; A <- [G+0380+X]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
001D12:	0BE8	CMP 	DL 	V_00E8	; compare A with [V_00E8]
001D14:	C808	BCT 	RM 	L_1D04	; 8 words backward: conditional branch (BCT) to L_1D04
001D16:	510A	STA 	DG 	TWB_T1	; [TWB_T1] <- A
001D18:	420A	LDX 	DG 	TWB_T1	; X <- [TWB_T1]
001D1A:	F208	ICX 	P  	=#08	; ICX operand: #08
001D1C:	A230	LDX 	ILX	*V_0030,X	; X <- [G+0000+X]  (pointer V_0030 = #0000: address = G + #0000, G unknown)
001D1E:	530E	STX 	DG 	TWB_T3	; [TWB_T3] <- X
001D20:	F304	DCX 	P  	=#04	; DCX operand: #04
001D22:	A030	LDA 	ILX	*V_0030,X	; A <- [G+0000+X]  (pointer V_0030 = #0000: address = G + #0000, G unknown)
001D24:	450E	ADD 	DG 	TWB_T3	; A <- A + [TWB_T3]
001D26:	FA04	STR 	P  	=#04	; STR operand: #04
001D28:	F202	ICX 	P  	=#02	; ICX operand: #02
001D2A:	A030	LDA 	ILX	*V_0030,X	; A <- [G+0000+X]  (pointer V_0030 = #0000: address = G + #0000, G unknown)
001D2C:	450E	ADD 	DG 	TWB_T3	; A <- A + [TWB_T3]
001D2E:	420A	LDX 	DG 	TWB_T1	; X <- [TWB_T1]
001D30:	F20A	ICX 	P  	=#0A	; ICX operand: #0A
001D32:	B630	DST 	ILX	*V_0030,X	; [G+0000+X] <- A,E (2 words)  (pointer V_0030 = #0000: address = G + #0000, G unknown)
001D34:	4214	LDX 	DG 	TWB_T6	; X <- [TWB_T6]
001D36:	F114	CNX 	P  	
001D38:	A00A	LDA 	ILX	*CPT_PTR,X	; A <- [G+0380+X]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
001D3A:	2131	LDE 	P  	=#31	; E <- #31
001D3C:	F403	WD  	P  		; privileged control instruction (WD)
001D3E:	CF1D	BRU 	RM 	L_1D04	; 29 words backward: jump to L_1D04
L_1D40:				; branch target; 1 ref
001D40:	6F12	LBX 	IL 	*V_0012	; LBX operand: [G+225A]  (pointer V_0012 = #225A: address = G + #225A, G unknown)
L_1D42:				; branch target; 1 ref
001D42:	F905	LDR 	P  	=#05	; LDR operand: #05
001D44:	C004	BCT 	RP 	L_1D4C	; 4 words forward: conditional branch (BCT) to L_1D4C
001D46:	B412	SBL 	ILX	*V_0012,X	; left byte of [G+225A+X] <- A  (pointer V_0012 = #225A: address = G + #225A, G unknown)
001D48:	F301	DCX 	P  	=#01	; DCX operand: #01
001D4A:	CF04	BRU 	RM 	L_1D42	; 4 words backward: jump to L_1D42
L_1D4C:				; branch target; 1 ref
001D4C:	E302	DCX 	PX 	(X)	; DCX operand: [(X)]
001D4E:	A09C	LDA 	ILX	*V_009C,X	; A <- [G+0234+X]  (pointer V_009C = #0234: address = G + #0234, G unknown)
001D50:	F001	SLLS	P  	#01
001D52:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
001D54:	FA05	STR 	P  	=#05	; STR operand: #05
001D56:	E204	ICX 	PX 	(X)	; ICX operand: [(X)]
001D58:	2000	LDA 	P  	=#00	; A <- #00
001D5A:	2100	LDE 	P  	=#00	; E <- #00
001D5C:	B69C	DST 	ILX	*V_009C,X	; [G+0234+X] <- A,E (2 words)  (pointer V_009C = #0234: address = G + #0234, G unknown)
001D5E:	F304	DCX 	P  	=#04	; DCX operand: #04
001D60:	4008	LDA 	DG 	TWB_T0	; A <- [TWB_T0]
001D62:	2502	ADD 	P  	=#02	; A <- A + #02
L_1D64:				; branch target; 1 ref
001D64:	2100	LDE 	P  	=#00	; E <- #00
001D66:	F102	XAE 	P  		; exchange A and E
001D68:	B69C	DST 	ILX	*V_009C,X	; [G+0234+X] <- A,E (2 words)  (pointer V_009C = #0234: address = G + #0234, G unknown)
001D6A:	F102	XAE 	P  		; exchange A and E
001D6C:	2602	SUB 	P  	=#02	; A <- A - #02
001D6E:	F304	DCX 	P  	=#04	; DCX operand: #04
001D70:	C806	BCT 	RM 	L_1D64	; 6 words backward: conditional branch (BCT) to L_1D64
001D72:	2202	LDX 	P  	=#02	; X <- #02
001D74:	4008	LDA 	DG 	TWB_T0	; A <- [TWB_T0]
001D76:	2502	ADD 	P  	=#02	; A <- A + #02
001D78:	B19C	STA 	ILX	*V_009C,X	; [G+0234+X] <- A  (pointer V_009C = #0234: address = G + #0234, G unknown)
001D7A:	2000	LDA 	P  	=#00	; A <- #00
001D7C:	6F12	LBX 	IL 	*V_0012	; LBX operand: [G+225A]  (pointer V_0012 = #225A: address = G + #225A, G unknown)
L_1D7E:				; branch target; 1 ref
001D7E:	B598	SBR 	ILX	*V_0098,X	; right byte of [G+2232+X] <- A  (pointer V_0098 = #2232: address = G + #2232, G unknown)
001D80:	F301	DCX 	P  	=#01	; DCX operand: #01
001D82:	C802	BCT 	RM 	L_1D7E	; 2 words backward: conditional branch (BCT) to L_1D7E
001D84:	6F12	LBX 	IL 	*V_0012	; LBX operand: [G+225A]  (pointer V_0012 = #225A: address = G + #225A, G unknown)
L_1D86:				; branch target; 1 ref
001D86:	B59A	SBR 	ILX	*V_009A,X	; right byte of [G+2246+X] <- A  (pointer V_009A = #2246: address = G + #2246, G unknown)
001D88:	F301	DCX 	P  	=#01	; DCX operand: #01
001D8A:	C802	BCT 	RM 	L_1D86	; 2 words backward: conditional branch (BCT) to L_1D86
001D8C:	6214	LDX 	IL 	*V_0014	; X <- [*V_0014]
L_1D8E:				; branch target; 1 ref
001D8E:	F301	DCX 	P  	=#01	; DCX operand: #01
001D90:	C309	BCF 	RP 	L_1DA2	; 9 words forward: conditional branch (BCF) to L_1DA2
001D92:	5308	STX 	DG 	TWB_T0	; [TWB_T0] <- X
001D94:	E202	ICX 	PX 	(X)	; ICX operand: [(X)]
001D96:	A214	LDX 	ILX	*V_0014,X	; X <- [*V_0014+X]
001D98:	C003	BCT 	RP 	L_1D9E	; 3 words forward: conditional branch (BCT) to L_1D9E
001D9A:	F216	ICX 	P  	=#16	; ICX operand: #16
001D9C:	B130	STA 	ILX	*V_0030,X	; [G+0000+X] <- A  (pointer V_0030 = #0000: address = G + #0000, G unknown)
L_1D9E:				; branch target; 1 ref
001D9E:	4208	LDX 	DG 	TWB_T0	; X <- [TWB_T0]
001DA0:	CF09	BRU 	RM 	L_1D8E	; 9 words backward: jump to L_1D8E
L_1DA2:				; branch target; 1 ref
001DA2:	2000	LDA 	P  	=#00	; A <- #00
001DA4:	1102	STA 	DL 	V_0002	; [V_0002] <- A
001DA6:	6F94	LBX 	IL 	*V_0094	; LBX operand: [G+00DC]  (pointer V_0094 = #00DC: address = G + #00DC, G unknown)
L_1DA8:				; branch target; 1 ref
001DA8:	F905	LDR 	P  	=#05	; LDR operand: #05
001DAA:	C005	BCT 	RP 	L_1DB4	; 5 words forward: conditional branch (BCT) to L_1DB4
001DAC:	B494	SBL 	ILX	*V_0094,X	; left byte of [G+00DC+X] <- A  (pointer V_0094 = #00DC: address = G + #00DC, G unknown)
001DAE:	F301	DCX 	P  	=#01	; DCX operand: #01
001DB0:	CF04	BRU 	RM 	L_1DA8	; 4 words backward: jump to L_1DA8
L_1DB2:				; branch target; 1 ref
001DB2:	C754	BRU 	RP 	L_1E5A	; 84 words forward: jump to L_1E5A
L_1DB4:				; branch target; 1 ref
001DB4:	11D6	STA 	DL 	V_00D6	; [V_00D6] <- A
001DB6:	2DFF	LBL 	P  	=#FF	; A <- left byte of #FF
001DB8:	11D4	STA 	DL 	V_00D4	; [V_00D4] <- A
001DBA:	4018	LDA 	DG 	TWB_N3	; A <- [TWB_N3]
001DBC:	FA01	STR 	P  	=#01	; STR operand: #01
001DBE:	401A	LDA 	DG 	TWB_N2	; A <- [TWB_N2]
001DC0:	C50A	BAZ 	RP 	L_1DD4	; 10 words forward: conditional branch (BAZ) to L_1DD4
001DC2:	0402	LEA 	DL 	V_0002	; A <- V_0002  (effective address, memory not read)
001DC4:	F702	CSV 	P  	=#02	; SUPERVISOR CALL #2 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry MON2  [-> MON2: block move / translate string (MVS/TRS) 2 loops, stores into the caller's TWB, returns to its CSV caller with RSV]
001DC6:	2101	LDE 	P  	=#01	; E <- #01
001DC8:	2E20	LBR 	P  	=#20	; A <- right byte of #20
001DCA:	2D02	LBL 	P  	=#02	; A <- left byte of #02
001DCC:	F403	WD  	P  		; privileged control instruction (WD)
001DCE:	2000	LDA 	P  	=#00	; A <- #00
001DD0:	FA01	STR 	P  	=#01	; STR operand: #01
001DD2:	C736	BRU 	RP 	L_1E3E	; 54 words forward: jump to L_1E3E
L_1DD4:				; branch target; 1 ref
001DD4:	2121	LDE 	P  	=#21	; E <- #21
001DD6:	2008	LDA 	P  	=#08	; A <- #08
001DD8:	F403	WD  	P  		; privileged control instruction (WD)
001DDA:	2101	LDE 	P  	=#01	; E <- #01
001DDC:	20A4	LDA 	P  	=#A4	; A <- #A4
001DDE:	F403	WD  	P  		; privileged control instruction (WD)
001DE0:	2131	LDE 	P  	=#31	; E <- #31
001DE2:	2008	LDA 	P  	=#08	; A <- #08
001DE4:	F403	WD  	P  		; privileged control instruction (WD)
001DE6:	0402	LEA 	DL 	V_0002	; A <- V_0002  (effective address, memory not read)
001DE8:	F702	CSV 	P  	=#02	; SUPERVISOR CALL #2 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry MON2  [-> MON2: block move / translate string (MVS/TRS) 2 loops, stores into the caller's TWB, returns to its CSV caller with RSV]
L_1DEA:				; branch target; 1 ref
001DEA:	F909	LDR 	P  	=#09	; LDR operand: #09
001DEC:	F001	SLLS	P  	#01
001DEE:	C502	BAZ 	RP 	L_1DF2	; 2 words forward: conditional branch (BAZ) to L_1DF2
001DF0:	CF03	BRU 	RM 	L_1DEA	; 3 words backward: jump to L_1DEA
L_1DF2:				; branch target; 1 ref
001DF2:	2200	LDX 	P  	=#00	; X <- #00
L_1DF4:				; branch target; 1 ref
001DF4:	F201	ICX 	P  	=#01	; ICX operand: #01
001DF6:	F905	LDR 	P  	=#05	; LDR operand: #05
001DF8:	CE02	BOF 	RM 	L_1DF4	; 2 words backward: conditional branch (BOF) to L_1DF4
001DFA:	2040	LDA 	P  	=#40	; A <- #40
001DFC:	FA08	STR 	P  	=#08	; STR operand: #08
001DFE:	2000	LDA 	P  	=#00	; A <- #00
001E00:	FA01	STR 	P  	=#01	; STR operand: #01
001E02:	6D12	LBL 	IL 	*V_0012	; A <- left byte of [G+225A]  (pointer V_0012 = #225A: address = G + #225A, G unknown)
001E04:	7112	STA 	IL 	*V_0012	; [G+225A] <- A  (pointer V_0012 = #225A: address = G + #225A, G unknown)
001E06:	E302	DCX 	PX 	(X)	; DCX operand: [(X)]
001E08:	A09C	LDA 	ILX	*V_009C,X	; A <- [G+0234+X]  (pointer V_009C = #0234: address = G + #0234, G unknown)
001E0A:	F001	SLLS	P  	#01
001E0C:	2502	ADD 	P  	=#02	; A <- A + #02
001E0E:	2102	LDE 	P  	=#02	; E <- #02
001E10:	769C	DST 	IL 	*V_009C	; [G+0234] <- A,E (2 words)  (pointer V_009C = #0234: address = G + #0234, G unknown)
001E12:	2204	LDX 	P  	=#04	; X <- #04
001E14:	2104	LDE 	P  	=#04	; E <- #04
001E16:	2000	LDA 	P  	=#00	; A <- #00
001E18:	B69C	DST 	ILX	*V_009C,X	; [G+0234+X] <- A,E (2 words)  (pointer V_009C = #0234: address = G + #0234, G unknown)
001E1A:	2201	LDX 	P  	=#01	; X <- #01
001E1C:	B598	SBR 	ILX	*V_0098,X	; right byte of [G+2232+X] <- A  (pointer V_0098 = #2232: address = G + #2232, G unknown)
001E1E:	B59A	SBR 	ILX	*V_009A,X	; right byte of [G+2246+X] <- A  (pointer V_009A = #2246: address = G + #2246, G unknown)
001E20:	640C	LEA 	IL 	*PRTS_PTR	; A <- *PRTS_PTR  (effective address, memory not read)
001E22:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
001E24:	700C	DLD 	IL 	*PRTS_PTR	; A,E <- [*PRTS_PTR] (2 words)
001E26:	560A	DST 	DG 	TWB_T1	; [TWB_T1] <- A,E (2 words)
001E28:	4018	LDA 	DG 	TWB_N3	; A <- [TWB_N3]
001E2A:	FA01	STR 	P  	=#01	; STR operand: #01
001E2C:	0100	LDE 	DL 	Unknow_1	; E <- [Unknow_1]
001E2E:	2000	LDA 	P  	=#00	; A <- #00
001E30:	FA01	STR 	P  	=#01	; STR operand: #01
001E32:	F102	XAE 	P  		; exchange A and E
001E34:	760C	DST 	IL 	*PRTS_PTR	; [*PRTS_PTR] <- A,E (2 words)
001E36:	F403	WD  	P  		; privileged control instruction (WD)
001E38:	F700	CSV 	P  	=#00	; SUPERVISOR CALL #0 (module 0 = trap handler) (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry MON0_TRAP  [-> MON0_TRAP: shifts/bit manipulation 5 loops, privileged I/O or mask ops (CLM/RD/WD/STM), supervisor calls: #14 #15 #18 ..., calls S_1F98]
001E3A:	500A	DLD 	DG 	TWB_T1	; A,E <- [TWB_T1] (2 words)
001E3C:	9608	DST 	IGX	*TWB_T0,X	; [*TWB_T0+X] <- A,E (2 words)
L_1E3E:				; branch target; 2 refs
001E3E:	2006	LDA 	P  	=#06	; A <- #06
001E40:	2D30	LBL 	P  	=#30	; A <- left byte of #30
001E42:	FA02	STR 	P  	=#02	; STR operand: #02
001E44:	F726	CSV 	P  	=#26	; SUPERVISOR CALL #38 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry MON38  [-> MON38: stores into the caller's TWB, returns to its CSV caller with RSV]
001E46:	2000	LDA 	P  	=#00	; A <- #00
001E48:	210C	LDE 	P  	=#0C	; E <- #0C
001E4A:	F403	WD  	P  		; privileged control instruction (WD)
001E4C:	5808	SPA 	DG 	TWB_T0	; [TWB_T0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001E4E:	D70E	BRU 	IL 	*PRTS_COUNT	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer PRTS_COUNT = #0038: address = G + #0038, G unknown)
L_1E50:				; branch target; 1 ref
001E50:	C701	BRU 	RP 	L_1E52	; 1 word forward: jump to L_1E52
L_1E52:				; branch target; 1 ref
001E52:	2040	LDA 	P  	=#40	; A <- #40
001E54:	FA08	STR 	P  	=#08	; STR operand: #08
001E56:	F401	DIT 	P  		; DE-ACTIVATE INTERRUPT: save outgoing context, dispatch the highest-priority waiting level (not a plain return)
l_1E58:				; branch target; 1 ref; only from untraced words
001E58:	C700	BRU 	RP 	l_1E58	; 0 words forward: jump to l_1E58
L_1E5A:				; branch target; 1 ref
001E5A:	FA01	STR 	P  	=#01	; STR operand: #01
001E5C:	0024	LDA 	DL 	V_0024	; A <- [V_0024]
001E5E:	05D2	ADD 	DL 	V_00D2	; A <- A + [V_00D2]
001E60:	1166	STA 	DL 	L_0066	; [L_0066] <- A
001E62:	01E6	LDE 	DL 	V_00E6	; E <- [V_00E6]
001E64:	600A	LDA 	IL 	*CPT_PTR	; A <- [G+0380]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
001E66:	2240	LDX 	P  	=#40	; X <- #40
001E68:	B10A	STA 	ILX	*CPT_PTR,X	; [G+0380+X] <- A  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
001E6A:	720A	STE 	IL 	*CPT_PTR	; [G+0380] <- E  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
001E6C:	CF17	BRU 	RM 	L_1E3E	; 23 words backward: jump to L_1E3E
001E6E:	0014	LDA 	DL 	V_0014	; A <- [V_0014]
001E70:	0002	LDA 	DL 	V_0002	; A <- [V_0002]
001E72:	FFFF	??	FFFF
001E74:	0040	LDA 	DL 	v_0040	; A <- [v_0040]
001E76:	3006	SLLS	DL 	#06
001E78:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
001E7A:	1E58	TRS 	DL 	v_0058	; TRS operand: [v_0058]
001E7C:	3800	CLS 	DL 	Unknow_1	; CALL SECTION, number taken from [Unknow_1] (CLS); return = RTS
001E7E:	FFFF	??	FFFF
001E80:	1E7E	TRS 	DL 	V_007E	; TRS operand: [V_007E]
001E82:	0002	LDA 	DL 	V_0002	; A <- [V_0002]
001E84:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
001E86:	0003	LDA 	DL 	V_0002	; A <- [V_0002]
001E88:	0008	LDA 	DL 	V_0008	; A <- [V_0008]
001E8A:	1F00	MVS 	DL 	Unknow_1	; MVS operand: [Unknow_1]
001E8C:	4000	LDA 	DG 	TWB_P	; A <- [TWB_P]
001E8E:	8000	LDA 	IGX	*TWB_P,X	; A <- [*TWB_P+X]  (pointer TWB_P lives in the calling task's area: its contents are only known at run time)
001E90:	FFFE	??	FFFE
001E92:	FFFF	??	FFFF
001E94:	7FFF	MVS 	IL 	*v_00FE	; MVS operand: [G+22CA]  (pointer v_00FE = #22CA: address = G + #22CA, G unknown)
001E96:	FF80	??	FF80
001E98:	F000	SLLS	P  	
001E9A:	FFF0	??	FFF0
001E9C:	211C	LDE 	P  	=#1C	; E <- #1C
001E9E:	20F6	LDA 	P  	=#F6	; A <- #F6
001EA0:	1FFE	MVS 	DL 	v_00FE	; MVS operand: [v_00FE]
001EA2:	2088	LDA 	P  	=#88	; A <- #88
001EA4:	2094	LDA 	P  	=#94	; A <- #94
001EA6:	2154	LDE 	P  	=#54	; E <- #54
001EA8:	1F00	MVS 	DL 	Unknow_1	; MVS operand: [Unknow_1]
001EAA:	1F00	MVS 	DL 	Unknow_1	; MVS operand: [Unknow_1]
001EAC:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
001EAE:	1FC8	MVS 	DL 	V_00C8	; MVS operand: [V_00C8]
001EB0:	0000	.ZERO	12	; 12 zero words (padding, reserved area or unused data; never reached as code)

; ===== MON0_TRAP: section / module entry (no RTS/RSV/DIT seen on its paths) =====
; routine profile: shifts/bit manipulation 5 loops, privileged I/O or mask ops (CLM/RD/WD/STM), supervisor calls: #14 #15 #18 ..., calls S_1F98
; =====================================================================
; M:TRAP FIXME source unknown
M:TRAP TYPE 0
"Program" trap at level zero
The M:TRAP Type 0 module triggers the printing of the following trap message on the teleprinter (M:OC):
▪ Status word for the instruction that caused the trap
• Absolute P
• Absolute L
• Indicators regarding the last monitor call
• P - G
• L - G
• %%DRxx
xx = 01 if trap caused by a coupler
xx = 02 if program trap.
; =====================================================================
MON0_TRAP:				; entry point; callee (section / supervisor module / SPA subroutine); 2 refs
001EC8:	F408	STM 	P  		; privileged control instruction (STM)
001ECA:	1644	DST 	DL 	V_0044	; [V_0044] <- A,E (2 words)
001ECC:	2119	LDE 	P  	=#19	; E <- #19
001ECE:	F403	WD  	P  		; privileged control instruction (WD)
001ED0:	133A	STX 	DL 	V_003A	; [V_003A] <- X
001ED2:	002E	LDA 	DL 	V_002E	; A <- [V_002E]
001ED4:	C316	BCF 	RP 	L_1F00	; 22 words forward: conditional branch (BCF) to L_1F00
001ED6:	3602	DCL 	DL 	V_0002	; DCL operand: [V_0002]
001ED8:	F902	LDR 	P  	=#02	; LDR operand: #02
001EDA:	1704	ADM 	DL 	V_0004	; add A to [V_0004]
001EDC:	F102	XAE 	P  		; exchange A and E
001EDE:	F902	LDR 	P  	=#02	; LDR operand: #02
001EE0:	1706	ADM 	DL 	V_0006	; add A to [V_0006]
001EE2:	0208	LDX 	DL 	V_0008	; X <- [V_0008]
001EE4:	3528	ICL 	DL 	V_0028	; ICL operand: [V_0028]
001EE6:	1338	STX 	DL 	V_0038	; [V_0038] <- X
001EE8:	1634	DST 	DL 	V_0034	; [V_0034] <- A,E (2 words)
001EEA:	6004	LDA 	IL 	*V_0004	; A <- [G+0000]  (pointer V_0004 = #0000: address = G + #0000, G unknown)
001EEC:	C00A	BCT 	RP 	L_1F00	; 10 words forward: conditional branch (BCT) to L_1F00
001EEE:	2B40	CMP 	P  	=#40	; compare A with #40
001EF0:	C303	BCF 	RP 	L_1EF6	; 3 words forward: conditional branch (BCF) to L_1EF6
001EF2:	2001	LDA 	P  	=#01	; A <- #01
001EF4:	7704	ADM 	IL 	*V_0004	; add A to [G+0000]  (pointer V_0004 = #0000: address = G + #0000, G unknown)
L_1EF6:				; branch target; 1 ref
001EF6:	2901	AND 	P  	=#01	; A <- A and #01
001EF8:	C02B	BCT 	RP 	L_1F4E	; 43 words forward: conditional branch (BCT) to L_1F4E
001EFA:	6E08	LBR 	IL 	*V_0008	; A <- right byte of [G+0000]  (pointer V_0008 = #0000: address = G + #0000, G unknown)
001EFC:	2906	AND 	P  	=#06	; A <- A and #06
001EFE:	C32D	BCF 	RP 	L_1F58	; 45 words forward: conditional branch (BCF) to L_1F58
L_1F00:				; branch target; 5 refs
001F00:	F908	LDR 	P  	=#08	; LDR operand: #08
001F02:	C326	BCF 	RP 	L_1F4E	; 38 words forward: conditional branch (BCF) to L_1F4E
001F04:	2001	LDA 	P  	=#01	; A <- #01
001F06:	1700	ADM 	DL 	Unknow_1	; add A to [Unknow_1]
001F08:	C50D	BAZ 	RP 	L_1F22	; 13 words forward: conditional branch (BAZ) to L_1F22
l_1F0A:				; branch target; 1 ref; only from untraced words
001F0A:	2120	LDE 	P  	=#20	; E <- #20
001F0C:	20FF	LDA 	P  	=#FF	; A <- #FF
001F0E:	F403	WD  	P  		; privileged control instruction (WD)
001F10:	2101	LDE 	P  	=#01	; E <- #01
001F12:	2060	LDA 	P  	=#60	; A <- #60
001F14:	F403	WD  	P  		; privileged control instruction (WD)
001F16:	2110	LDE 	P  	=#10	; E <- #10
001F18:	6004	LDA 	IL 	*V_0004	; A <- [G+0000]  (pointer V_0004 = #0000: address = G + #0000, G unknown)
001F1A:	F403	WD  	P  		; privileged control instruction (WD)
L_1F1C:				; branch target; 1 ref
001F1C:	C700	BRU 	RP 	L_1F1C	; 0 words forward: jump to L_1F1C
001F1E:	F400	CLM 	P  		; privileged control instruction (CLM)
001F20:	CF0B	BRU 	RM 	l_1F0A	; 11 words backward: jump to l_1F0A
L_1F22:				; branch target; 1 ref
001F22:	6004	LDA 	IL 	*V_0004	; A <- [G+0000]  (pointer V_0004 = #0000: address = G + #0000, G unknown)
001F24:	1132	STA 	DL 	V_0032	; [V_0032] <- A
001F26:	5000	DLD 	DG 	TWB_P	; A,E <- [TWB_P] (2 words)
001F28:	163A	DST 	DL 	V_003A	; [V_003A] <- A,E (2 words)
001F2A:	4004	LDA 	DG 	TWB_IND	; A <- [TWB_IND]
001F2C:	113E	STA 	DL 	V_003E	; [V_003E] <- A
001F2E:	2236	LDX 	P  	=#36	; X <- #36
001F30:	0432	LEA 	DL 	V_0032	; A <- V_0032  (effective address, memory not read)
001F32:	210E	LDE 	P  	=#0E	; E <- #0E
001F34:	F70E	CSV 	P  	=#0E	; SUPERVISOR CALL #14 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_EDIT  [-> M_EDIT: 4 loops, stores into the caller's TWB, returns to its CSV caller with RSV]
001F36:	6004	LDA 	IL 	*V_0004	; A <- [G+0000]  (pointer V_0004 = #0000: address = G + #0000, G unknown)
001F38:	2901	AND 	P  	=#01	; A <- A and #01
001F3A:	2501	ADD 	P  	=#01	; A <- A + #01
001F3C:	F108	XAA 	P  	
001F3E:	F70F	CSV 	P  	=#0F	; SUPERVISOR CALL #15 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_ERED  [-> M_ERED: 3 loops, stores into the caller's TWB, returns to its CSV caller with RSV]
001F40:	2000	LDA 	P  	=#00	; A <- #00
001F42:	7104	STA 	IL 	*V_0004	; [G+0000] <- A  (pointer V_0004 = #0000: address = G + #0000, G unknown)
001F44:	112E	STA 	DL 	V_002E	; [V_002E] <- A
001F46:	E301	DCX 	PX 	(X)	; DCX operand: [(X)]
001F48:	1300	STX 	DL 	Unknow_1	; [Unknow_1] <- X
001F4A:	F400	CLM 	P  		; privileged control instruction (CLM)
001F4C:	F712	CSV 	P  	=#12	; SUPERVISOR CALL #18 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_ABRT  [-> M_ABRT: shifts/bit manipulation 1 loop, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, ends with DIT/DITR]
L_1F4E:				; branch target; 2 refs
001F4E:	3602	DCL 	DL 	V_0002	; DCL operand: [V_0002]
001F50:	020E	LDX 	DL 	PRTS_COUNT	; X <- [PRTS_COUNT]
001F52:	F301	DCX 	P  	=#01	; DCX operand: #01
001F54:	2100	LDE 	P  	=#00	; E <- #00
001F56:	E700	CSV 	PX 	(X)	; SUPERVISOR CALL, module number taken from [(X)] (CSV); module ends with RSV
L_1F58:				; branch target; 1 ref
001F58:	112E	STA 	DL 	V_002E	; [V_002E] <- A
001F5A:	2904	AND 	P  	=#04	; A <- A and #04
001F5C:	C003	BCT 	RP 	L_1F62	; 3 words forward: conditional branch (BCT) to L_1F62
001F5E:	6E34	LBR 	IL 	*V_0034	; A <- right byte of [*V_0034]
001F60:	291F	AND 	P  	=#1F	; A <- A and #1F
L_1F62:				; branch target; 1 ref
001F62:	6E34	LBR 	IL 	*V_0034	; A <- right byte of [*V_0034]
001F64:	291F	AND 	P  	=#1F	; A <- A and #1F
001F66:	2B1A	CMP 	P  	=#1A	; compare A with #1A
001F68:	CA34	BOT 	RM 	L_1F00	; 52 words backward: conditional branch (BOT) to L_1F00
001F6A:	2000	LDA 	P  	=#00	; A <- #00
001F6C:	7508	SBR 	IL 	*V_0008	; right byte of [G+0000] <- A  (pointer V_0008 = #0000: address = G + #0000, G unknown)
001F6E:	112E	STA 	DL 	V_002E	; [V_002E] <- A
001F70:	1034	DLD 	DL 	V_0034	; A,E <- [V_0034] (2 words)
001F72:	F102	XAE 	P  		; exchange A and E
001F74:	5634	DST 	DG 	G+0034	; [G+0034] <- A,E (2 words)
001F76:	1832	SPA 	DL 	V_0032	; [V_0032] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
001F78:	C710	BRU 	RP 	S_1F98	; 16 words forward: CALL subroutine S_1F98 (SPA above saved the return address); returns to L_1F7A  [-> S_1F98: comparisons, 6 loops, returns through its return cell]
L_1F7A:				; branch target; 1 ref
001F7A:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
001F7C:	1044	DLD 	DL 	V_0044	; A,E <- [V_0044] (2 words)
001F7E:	5624	DST 	DG 	G+0024	; [G+0024] <- A,E (2 words)
001F80:	1038	DLD 	DL 	V_0038	; A,E <- [V_0038] (2 words)
001F82:	5630	DST 	DG 	G+0030	; [G+0030] <- A,E (2 words)
001F84:	6E34	LBR 	IL 	*V_0034	; A <- right byte of [*V_0034]
001F86:	F400	CLM 	P  		; privileged control instruction (CLM)
001F88:	291F	AND 	P  	=#1F	; A <- A and #1F
001F8A:	261A	SUB 	P  	=#1A	; A <- A - #1A
001F8C:	F104	XAX 	P  		; exchange A and X
001F8E:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
001F90:	D122	BRX 	IL 	$+X	; INDEXED JUMP (BRX): X selects one of the 3 BRU entries that follow (jump / dispatch table)
L_1F92:				; branch target; 1 ref
001F92:	CF49	BRU 	RM 	L_1F00	; 73 words backward: jump to L_1F00
L_1F94:				; branch target; 1 ref
001F94:	CF4A	BRU 	RM 	L_1F00	; 74 words backward: jump to L_1F00
L_1F96:				; branch target; 1 ref
001F96:	C734	BRU 	RP 	L_1FFE	; 52 words forward: jump to L_1FFE

; ===== S_1F98: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: comparisons, 6 loops, returns through its return cell
S_1F98:				; callee (section / supervisor module / SPA subroutine); 1 ref
001F98:	6034	LDA 	IL 	*V_0034	; A <- [*V_0034]
001F9A:	7130	STA 	IL 	*V_0030	; [G+0000] <- A  (pointer V_0030 = #0000: address = G + #0000, G unknown)
001F9C:	6E34	LBR 	IL 	*V_0034	; A <- right byte of [*V_0034]
001F9E:	29E0	AND 	P  	=#E0	; A <- A and #E0
001FA0:	2B60	CMP 	P  	=#60	; compare A with #60
001FA2:	C021	BCT 	RP 	L_1FE4	; 33 words forward: conditional branch (BCT) to L_1FE4
001FA4:	2BA0	CMP 	P  	=#A0	; compare A with #A0
001FA6:	C01F	BCT 	RP 	L_1FE4	; 31 words forward: conditional branch (BCT) to L_1FE4
L_1FA8:				; branch target; 1 ref
001FA8:	023A	LDX 	DL 	V_003A	; X <- [V_003A]
L_1FAA:				; branch target; 2 refs
001FAA:	1348	STX 	DL 	V_0048	; [V_0048] <- X
001FAC:	6E34	LBR 	IL 	*V_0034	; A <- right byte of [*V_0034]
001FAE:	291F	AND 	P  	=#1F	; A <- A and #1F
001FB0:	2B08	CMP 	P  	=#08	; compare A with #08
001FB2:	C013	BCT 	RP 	L_1FD8	; 19 words forward: conditional branch (BCT) to L_1FD8
001FB4:	2B0A	CMP 	P  	=#0A	; compare A with #0A
001FB6:	C013	BCT 	RP 	L_1FDC	; 19 words forward: conditional branch (BCT) to L_1FDC
001FB8:	2B1E	CMP 	P  	=#1E	; compare A with #1E
001FBA:	C613	BOF 	RP 	L_1FE0	; 19 words forward: conditional branch (BOF) to L_1FE0
001FBC:	2D10	LBL 	P  	=#10	; A <- left byte of #10
L_1FBE:				; branch target; 3 refs
001FBE:	010C	LDE 	DL 	PRTS_PTR	; E <- [PRTS_PTR]
001FC0:	7930	STS 	IL 	*V_0030	; STS operand: [G+0000]  (pointer V_0030 = #0000: address = G + #0000, G unknown)
001FC2:	0248	LDX 	DL 	V_0048	; X <- [V_0048]
001FC4:	0036	LDA 	DL 	V_0036	; A <- [V_0036]
001FC6:	FA01	STR 	P  	=#01	; STR operand: #01
001FC8:	0000	LDA 	DL 	Unknow_1	; A <- [Unknow_1]
001FCA:	FA05	STR 	P  	=#05	; STR operand: #05
001FCC:	2000	LDA 	P  	=#00	; A <- #00
001FCE:	FA01	STR 	P  	=#01	; STR operand: #01
001FD0:	3528	ICL 	DL 	V_0028	; ICL operand: [V_0028]
001FD2:	1348	STX 	DL 	V_0048	; [V_0048] <- X
001FD4:	0048	LDA 	DL 	V_0048	; A <- [V_0048]
001FD6:	D732	BRU 	IL 	*V_0032	; RETURN from subroutine: branch through return cell V_0032 (written by the SPA of the call)
L_1FD8:				; branch target; 1 ref
001FD8:	2D00	LBL 	P  	=#00	; A <- left byte of #00
001FDA:	CF0E	BRU 	RM 	L_1FBE	; 14 words backward: jump to L_1FBE
L_1FDC:				; branch target; 1 ref
001FDC:	2D0E	LBL 	P  	=#0E	; A <- left byte of #0E
001FDE:	CF10	BRU 	RM 	L_1FBE	; 16 words backward: jump to L_1FBE
L_1FE0:				; branch target; 1 ref
001FE0:	2D04	LBL 	P  	=#04	; A <- left byte of #04
001FE2:	CF12	BRU 	RM 	L_1FBE	; 18 words backward: jump to L_1FBE
L_1FE4:				; branch target; 2 refs
001FE4:	600A	LDA 	IL 	*CPT_PTR	; A <- [G+0380]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
001FE6:	2904	AND 	P  	=#04	; A <- A and #04
001FE8:	CB20	BCF 	RM 	L_1FA8	; 32 words backward: conditional branch (BCF) to L_1FA8
001FEA:	F902	LDR 	P  	=#02	; LDR operand: #02
001FEC:	FA05	STR 	P  	=#05	; STR operand: #05
001FEE:	6E34	LBR 	IL 	*V_0034	; A <- right byte of [*V_0034]
001FF0:	2940	AND 	P  	=#40	; A <- A and #40
001FF2:	C303	BCF 	RP 	L_1FF8	; 3 words forward: conditional branch (BCF) to L_1FF8
001FF4:	323A	ICX 	DL 	V_003A	; ICX operand: [V_003A]
001FF6:	CF26	BRU 	RM 	L_1FAA	; 38 words backward: jump to L_1FAA
L_1FF8:				; branch target; 1 ref
001FF8:	000E	LDA 	DL 	PRTS_COUNT	; A <- [PRTS_COUNT]
001FFA:	7730	ADM 	IL 	*V_0030	; add A to [G+0000]  (pointer V_0030 = #0000: address = G + #0000, G unknown)
001FFC:	CF29	BRU 	RM 	L_1FAA	; 41 words backward: jump to L_1FAA
L_1FFE:				; branch target; 2 refs
001FFE:	4020	LDA 	DG 	G+0020	; A <- [G+0020]
002000:	C602	BOF 	RP 	L_2004	; 2 words forward: conditional branch (BOF) to L_2004
002002:	F110	CCA 	P  	
L_2004:				; branch target; 1 ref
002004:	5429	SBL 	DG 	G+0029	; left byte of [G+0029] <- A
002006:	4024	LDA 	DG 	G+0024	; A <- [G+0024]
002008:	C602	BOF 	RP 	L_200C	; 2 words forward: conditional branch (BOF) to L_200C
00200A:	F110	CCA 	P  	
L_200C:				; branch target; 1 ref
00200C:	5428	SBL 	DG 	G+0028	; left byte of [G+0028] <- A
00200E:	4E29	LBR 	DG 	G+0029	; A <- right byte of [G+0029]
002010:	512A	STA 	DG 	G+002A	; [G+002A] <- A
002012:	4E28	LBR 	DG 	G+0028	; A <- right byte of [G+0028]
002014:	462A	SUB 	DG 	G+002A	; A <- A - [G+002A]
002016:	512A	STA 	DG 	G+002A	; [G+002A] <- A
002018:	C434	BAN 	RP 	L_2080	; 52 words forward: conditional branch (BAN) to L_2080
00201A:	5020	DLD 	DG 	G+0020	; A,E <- [G+0020] (2 words)
00201C:	562C	DST 	DG 	G+002C	; [G+002C] <- A,E (2 words)
00201E:	5024	DLD 	DG 	G+0024	; A,E <- [G+0024] (2 words)
002020:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
002022:	4E28	LBR 	DG 	G+0028	; A <- right byte of [G+0028]
L_2024:				; branch target; 1 ref
002024:	2501	ADD 	P  	=#01	; A <- A + #01
002026:	5128	STA 	DG 	G+0028	; [G+0028] <- A
002028:	402A	LDA 	DG 	G+002A	; A <- [G+002A]
00202A:	C602	BOF 	RP 	L_202E	; 2 words forward: conditional branch (BOF) to L_202E
00202C:	F11C	CNA 	P  	
L_202E:				; branch target; 1 ref
00202E:	F002	SLLS	P  	#02
002030:	5126	STA 	DG 	G+0026	; [G+0026] <- A
002032:	2200	LDX 	P  	=#00	; X <- #00
002034:	2B1C	CMP 	P  	=#1C	; compare A with #1C
002036:	D61E	BOF 	IL 	*V_001E	; conditional branch, target not known statically  (pointer V_001E = #2224: address = G + #2224, G unknown)
002038:	2D00	LBL 	P  	=#00	; A <- left byte of #00
00203A:	4220	LDX 	DG 	G+0020	; X <- [G+0020]
00203C:	C602	BOF 	RP 	L_2040	; 2 words forward: conditional branch (BOF) to L_2040
00203E:	2DFF	LBL 	P  	=#FF	; A <- left byte of #FF
L_2040:				; branch target; 1 ref
002040:	5420	SBL 	DG 	G+0020	; left byte of [G+0020] <- A
002042:	5020	DLD 	DG 	G+0020	; A,E <- [G+0020] (2 words)
002044:	F064	SLCD	P  	#04
002046:	091C	AND 	DL 	V_001C	; A <- A and [V_001C]
002048:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
00204A:	2D00	LBL 	P  	=#00	; A <- left byte of #00
00204C:	422C	LDX 	DG 	G+002C	; X <- [G+002C]
00204E:	C602	BOF 	RP 	L_2052	; 2 words forward: conditional branch (BOF) to L_2052
002050:	2DFF	LBL 	P  	=#FF	; A <- left byte of #FF
L_2052:				; branch target; 1 ref
002052:	542C	SBL 	DG 	G+002C	; left byte of [G+002C] <- A
002054:	502C	DLD 	DG 	G+002C	; A,E <- [G+002C] (2 words)
002056:	F064	SLCD	P  	#04
002058:	091C	AND 	DL 	V_001C	; A <- A and [V_001C]
00205A:	4226	LDX 	DG 	G+0026	; X <- [G+0026]
00205C:	E040	SAD 	PX 	
00205E:	4522	ADD 	DG 	G+0022	; A <- A + [G+0022]
002060:	F10E	ACE 	P  	
002062:	F102	XAE 	P  		; exchange A and E
002064:	4520	ADD 	DG 	G+0020	; A <- A + [G+0020]
002066:	2201	LDX 	P  	=#01	; X <- #01
002068:	C40A	BAN 	RP 	L_207C	; 10 words forward: conditional branch (BAN) to L_207C
L_206A:				; branch target; 1 ref
00206A:	532A	STX 	DG 	G+002A	; [G+002A] <- X
00206C:	F102	XAE 	P  		; exchange A and E
00206E:	422A	LDX 	DG 	G+002A	; X <- [G+002A]
002070:	C304	BCF 	RP 	L_2078	; 4 words forward: conditional branch (BCF) to L_2078
002072:	F10A	CCE 	P  	
002074:	F11C	CNA 	P  	
002076:	F10E	ACE 	P  	
L_2078:				; branch target; 1 ref
002078:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
00207A:	D720	BRU 	IL 	*V_0020	; jump to G+#0000 (G unknown)  (pointer V_0020 = #0000: address = G + #0000, G unknown)
L_207C:				; branch target; 1 ref
00207C:	2200	LDX 	P  	=#00	; X <- #00
00207E:	CF0A	BRU 	RM 	L_206A	; 10 words backward: jump to L_206A
L_2080:				; branch target; 1 ref
002080:	5024	DLD 	DG 	G+0024	; A,E <- [G+0024] (2 words)
002082:	562C	DST 	DG 	G+002C	; [G+002C] <- A,E (2 words)
002084:	4E29	LBR 	DG 	G+0029	; A <- right byte of [G+0029]
002086:	CF31	BRU 	RM 	L_2024	; 49 words backward: jump to L_2024
002088:	5020	DLD 	DG 	G+0020	; A,E <- [G+0020] (2 words)
00208A:	F10A	CCE 	P  	
00208C:	F11C	CNA 	P  	
00208E:	F10E	ACE 	P  	
002090:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
002092:	CF4A	BRU 	RM 	L_1FFE	; 74 words backward: jump to L_1FFE
002094:	2201	LDX 	P  	=#01	; X <- #01
002096:	5024	DLD 	DG 	G+0024	; A,E <- [G+0024] (2 words)
002098:	C605	BOF 	RP 	l_20A2	; 5 words forward: conditional branch (BOF) to l_20A2
00209A:	F301	DCX 	P  	=#01	; DCX operand: #01
00209C:	F10A	CCE 	P  	
00209E:	F11C	CNA 	P  	
0020A0:	F10E	ACE 	P  	
l_20A2:				; branch target; 1 ref; only from untraced words
0020A2:	F102	XAE 	P  		; exchange A and E
0020A4:	5428	SBL 	DG 	G+0028	; left byte of [G+0028] <- A
0020A6:	2D00	LBL 	P  	=#00	; A <- left byte of #00
0020A8:	F066	SLCD	P  	#06
0020AA:	F001	SLLS	P  	#01
0020AC:	F0E1	SRCD	P  	#01
0020AE:	5624	DST 	DG 	G+0024	; [G+0024] <- A,E (2 words)
0020B0:	5020	DLD 	DG 	G+0020	; A,E <- [G+0020] (2 words)
0020B2:	C605	BOF 	RP 	l_20BC	; 5 words forward: conditional branch (BOF) to l_20BC
0020B4:	F301	DCX 	P  	=#01	; DCX operand: #01
0020B6:	F10A	CCE 	P  	
0020B8:	F11C	CNA 	P  	
0020BA:	F10E	ACE 	P  	
l_20BC:				; branch target; 1 ref; only from untraced words
0020BC:	F102	XAE 	P  		; exchange A and E
0020BE:	542B	SBL 	DG 	G+002B	; left byte of [G+002B] <- A
0020C0:	2D00	LBL 	P  	=#00	; A <- left byte of #00
0020C2:	542A	SBL 	DG 	G+002A	; left byte of [G+002A] <- A
0020C4:	F066	SLCD	P  	#06
0020C6:	F001	SLLS	P  	#01
0020C8:	F0E1	SRCD	P  	#01
0020CA:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
0020CC:	4E28	LBR 	DG 	G+0028	; A <- right byte of [G+0028]
0020CE:	452A	ADD 	DG 	G+002A	; A <- A + [G+002A]
0020D0:	263F	SUB 	P  	=#3F	; A <- A - #3F
0020D2:	5128	STA 	DG 	G+0028	; [G+0028] <- A
0020D4:	532A	STX 	DG 	G+002A	; [G+002A] <- X
0020D6:	4024	LDA 	DG 	G+0024	; A <- [G+0024]
0020D8:	4C22	MUL 	DG 	G+0022	; multiply by [G+0022]
0020DA:	562C	DST 	DG 	G+002C	; [G+002C] <- A,E (2 words)
0020DC:	4026	LDA 	DG 	G+0026	; A <- [G+0026]
0020DE:	4C20	MUL 	DG 	G+0020	; multiply by [G+0020]
0020E0:	452E	ADD 	DG 	G+002E	; A <- A + [G+002E]
0020E2:	F10E	ACE 	P  	
0020E4:	F102	XAE 	P  		; exchange A and E
0020E6:	452C	ADD 	DG 	G+002C	; A <- A + [G+002C]
0020E8:	F061	SLCD	P  	#01
0020EA:	512C	STA 	DG 	G+002C	; [G+002C] <- A
0020EC:	4026	LDA 	DG 	G+0026	; A <- [G+0026]
0020EE:	4C22	MUL 	DG 	G+0022	; multiply by [G+0022]
0020F0:	452C	ADD 	DG 	G+002C	; A <- A + [G+002C]
0020F2:	F10E	ACE 	P  	
0020F4:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
0020F6:	C52A	BAZ 	RP 	l_214A	; 42 words forward: conditional branch (BAZ) to l_214A
l_20F8:				; branch target; 1 ref; only from untraced words
0020F8:	4028	LDA 	DG 	G+0028	; A <- [G+0028]
0020FA:	C423	BAN 	RP 	l_2140	; 35 words forward: conditional branch (BAN) to l_2140
l_20FC:				; branch target; 1 ref; only from untraced words
0020FC:	001A	LDA 	DL 	V_001A	; A <- [V_001A]
0020FE:	4920	AND 	DG 	G+0020	; A <- A and [G+0020]
002100:	C51A	BAZ 	RP 	l_2134	; 26 words forward: conditional branch (BAZ) to l_2134
002102:	5020	DLD 	DG 	G+0020	; A,E <- [G+0020] (2 words)
002104:	F0E8	SRCD	P  	#08
002106:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
002108:	2200	LDX 	P  	=#00	; X <- #00
00210A:	4D29	LBL 	DG 	G+0029	; A <- left byte of [G+0029]
00210C:	C604	BOF 	RP 	l_2114	; 4 words forward: conditional branch (BOF) to l_2114
00210E:	2201	LDX 	P  	=#01	; X <- #01
002110:	0018	LDA 	DL 	V_0018	; A <- [V_0018]
002112:	5728	ADM 	DG 	G+0028	; add A to [G+0028]
l_2114:				; branch target; 1 ref; only from untraced words
002114:	4D29	LBL 	DG 	G+0029	; A <- left byte of [G+0029]
002116:	5420	SBL 	DG 	G+0020	; left byte of [G+0020] <- A
002118:	402A	LDA 	DG 	G+002A	; A <- [G+002A]
00211A:	C507	BAZ 	RP 	l_2128	; 7 words forward: conditional branch (BAZ) to l_2128
l_211C:				; branch target; 2 refs; only from untraced words
00211C:	5328	STX 	DG 	G+0028	; [G+0028] <- X
00211E:	4E31	LBR 	DG 	G+0031	; A <- right byte of [G+0031]
002120:	29FC	AND 	P  	=#FC	; A <- A and #FC
002122:	4528	ADD 	DG 	G+0028	; A <- A + [G+0028]
002124:	5531	SBR 	DG 	G+0031	; right byte of [G+0031] <- A
002126:	C763	BRU 	RP 	l_21EC	; 99 words forward: jump to l_21EC
l_2128:				; branch target; 1 ref; only from untraced words
002128:	5020	DLD 	DG 	G+0020	; A,E <- [G+0020] (2 words)
00212A:	F10A	CCE 	P  	
00212C:	F11C	CNA 	P  	
00212E:	F10E	ACE 	P  	
002130:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
002132:	CF0B	BRU 	RM 	l_211C	; 11 words backward: jump to l_211C
l_2134:				; branch target; 1 ref; only from untraced words
002134:	5020	DLD 	DG 	G+0020	; A,E <- [G+0020] (2 words)
002136:	F064	SLCD	P  	#04
002138:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
00213A:	0014	LDA 	DL 	V_0014	; A <- [V_0014]
00213C:	5728	ADM 	DG 	G+0028	; add A to [G+0028]
00213E:	C821	BCT 	RM 	l_20FC	; 33 words backward: conditional branch (BCT) to l_20FC
l_2140:				; branch target; 1 ref; only from untraced words
002140:	2203	LDX 	P  	=#03	; X <- #03
l_2142:				; branch target; 1 ref; only from untraced words
002142:	2000	LDA 	P  	=#00	; A <- #00
002144:	5120	STA 	DG 	G+0020	; [G+0020] <- A
002146:	5122	STA 	DG 	G+0022	; [G+0022] <- A
002148:	CF16	BRU 	RM 	l_211C	; 22 words backward: jump to l_211C
l_214A:				; branch target; 1 ref; only from untraced words
00214A:	4020	LDA 	DG 	G+0020	; A <- [G+0020]
00214C:	C502	BAZ 	RP 	l_2150	; 2 words forward: conditional branch (BAZ) to l_2150
00214E:	CF2B	BRU 	RM 	l_20F8	; 43 words backward: jump to l_20F8
l_2150:				; branch target; 1 ref; only from untraced words
002150:	2202	LDX 	P  	=#02	; X <- #02
002152:	CF08	BRU 	RM 	l_2142	; 8 words backward: jump to l_2142
002154:	2201	LDX 	P  	=#01	; X <- #01
002156:	5020	DLD 	DG 	G+0020	; A,E <- [G+0020] (2 words)
002158:	C040	BCT 	RP 	l_21D8	; 64 words forward: conditional branch (BCT) to l_21D8
l_215A:				; branch target; 1 ref; only from untraced words
00215A:	C605	BOF 	RP 	l_2164	; 5 words forward: conditional branch (BOF) to l_2164
00215C:	F301	DCX 	P  	=#01	; DCX operand: #01
00215E:	F10A	CCE 	P  	
002160:	F11C	CNA 	P  	
002162:	F10E	ACE 	P  	
l_2164:				; branch target; 1 ref; only from untraced words
002164:	F102	XAE 	P  		; exchange A and E
002166:	5429	SBL 	DG 	G+0029	; left byte of [G+0029] <- A
002168:	2D00	LBL 	P  	=#00	; A <- left byte of #00
00216A:	5428	SBL 	DG 	G+0028	; left byte of [G+0028] <- A
00216C:	532A	STX 	DG 	G+002A	; [G+002A] <- X
00216E:	F102	XAE 	P  		; exchange A and E
002170:	2207	LDX 	P  	=#07	; X <- #07
002172:	FCCA	NLZ 	P  	#0A
002174:	F114	CNX 	P  	
002176:	532C	STX 	DG 	G+002C	; [G+002C] <- X
002178:	422A	LDX 	DG 	G+002A	; X <- [G+002A]
00217A:	F102	XAE 	P  		; exchange A and E
00217C:	F001	SLLS	P  	#01
00217E:	F0E1	SRCD	P  	#01
002180:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
002182:	5024	DLD 	DG 	G+0024	; A,E <- [G+0024] (2 words)
002184:	C02E	BCT 	RP 	l_21E0	; 46 words forward: conditional branch (BCT) to l_21E0
l_2186:				; branch target; 1 ref; only from untraced words
002186:	C605	BOF 	RP 	l_2190	; 5 words forward: conditional branch (BOF) to l_2190
002188:	F301	DCX 	P  	=#01	; DCX operand: #01
00218A:	F10A	CCE 	P  	
00218C:	F11C	CNA 	P  	
00218E:	F10E	ACE 	P  	
l_2190:				; branch target; 1 ref; only from untraced words
002190:	F102	XAE 	P  		; exchange A and E
002192:	542E	SBL 	DG 	G+002E	; left byte of [G+002E] <- A
002194:	532A	STX 	DG 	G+002A	; [G+002A] <- X
002196:	2D00	LBL 	P  	=#00	; A <- left byte of #00
002198:	422C	LDX 	DG 	G+002C	; X <- [G+002C]
00219A:	E060	SLCD	PX 	
00219C:	5124	STA 	DG 	G+0024	; [G+0024] <- A
00219E:	5226	STE 	DG 	G+0026	; [G+0026] <- E
0021A0:	4E2E	LBR 	DG 	G+002E	; A <- right byte of [G+002E]
0021A2:	4628	SUB 	DG 	G+0028	; A <- A - [G+0028]
0021A4:	2541	ADD 	P  	=#41	; A <- A + #41
0021A6:	5128	STA 	DG 	G+0028	; [G+0028] <- A
0021A8:	5024	DLD 	DG 	G+0024	; A,E <- [G+0024] (2 words)
0021AA:	4822	DIV 	DG 	G+0022	; divide by [G+0022]
0021AC:	5624	DST 	DG 	G+0024	; [G+0024] <- A,E (2 words)
0021AE:	F11C	CNA 	P  	
0021B0:	4C20	MUL 	DG 	G+0020	; multiply by [G+0020]
0021B2:	562C	DST 	DG 	G+002C	; [G+002C] <- A,E (2 words)
0021B4:	4124	LDE 	DG 	G+0024	; E <- [G+0024]
0021B6:	2000	LDA 	P  	=#00	; A <- #00
0021B8:	F0E1	SRCD	P  	#01
0021BA:	452E	ADD 	DG 	G+002E	; A <- A + [G+002E]
0021BC:	F10E	ACE 	P  	
0021BE:	F102	XAE 	P  		; exchange A and E
0021C0:	452C	ADD 	DG 	G+002C	; A <- A + [G+002C]
0021C2:	F102	XAE 	P  		; exchange A and E
0021C4:	4822	DIV 	DG 	G+0022	; divide by [G+0022]
0021C6:	4126	LDE 	DG 	G+0026	; E <- [G+0026]
0021C8:	F001	SLLS	P  	#01
0021CA:	C304	BCF 	RP 	l_21D2	; 4 words forward: conditional branch (BCF) to l_21D2
0021CC:	F102	XAE 	P  		; exchange A and E
0021CE:	2601	SUB 	P  	=#01	; A <- A - #01
0021D0:	F102	XAE 	P  		; exchange A and E
l_21D2:				; branch target; 1 ref; only from untraced words
0021D2:	F063	SLCD	P  	#03
0021D4:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
0021D6:	D720	BRU 	IL 	*V_0020	; jump to G+#0000 (G unknown)  (pointer V_0020 = #0000: address = G + #0000, G unknown)
l_21D8:				; branch target; 1 ref; only from untraced words
0021D8:	C502	BAZ 	RP 	l_21DC	; 2 words forward: conditional branch (BAZ) to l_21DC
0021DA:	CF40	BRU 	RM 	l_215A	; 64 words backward: jump to l_215A
l_21DC:				; branch target; 1 ref; only from untraced words
0021DC:	2201	LDX 	P  	=#01	; X <- #01
0021DE:	C704	BRU 	RP 	l_21E6	; 4 words forward: jump to l_21E6
l_21E0:				; branch target; 1 ref; only from untraced words
0021E0:	C502	BAZ 	RP 	l_21E4	; 2 words forward: conditional branch (BAZ) to l_21E4
0021E2:	CF2E	BRU 	RM 	l_2186	; 46 words backward: jump to l_2186
l_21E4:				; branch target; 1 ref; only from untraced words
0021E4:	2202	LDX 	P  	=#02	; X <- #02
l_21E6:				; branch target; 1 ref; only from untraced words
0021E6:	5024	DLD 	DG 	G+0024	; A,E <- [G+0024] (2 words)
0021E8:	5620	DST 	DG 	G+0020	; [G+0020] <- A,E (2 words)
0021EA:	D71E	BRU 	IL 	*V_001E	; jump to G+#2224 (G unknown)  (pointer V_001E = #2224: address = G + #2224, G unknown)
l_21EC:				; branch target; 1 ref; only from untraced words
0021EC:	3602	DCL 	DL 	V_0002	; DCL operand: [V_0002]
0021EE:	F908	LDR 	P  	=#08	; LDR operand: #08
0021F0:	F104	XAX 	P  		; exchange A and X
0021F2:	F408	STM 	P  		; privileged control instruction (STM)
0021F4:	A20A	LDX 	ILX	*CPT_PTR,X	; X <- [G+0380+X]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
0021F6:	5030	DLD 	DG 	G+0030	; A,E <- [G+0030] (2 words)
0021F8:	B630	DST 	ILX	*V_0030,X	; [G+0000+X] <- A,E (2 words)  (pointer V_0030 = #0000: address = G + #0000, G unknown)
0021FA:	5020	DLD 	DG 	G+0020	; A,E <- [G+0020] (2 words)
0021FC:	B638	DST 	ILX	*V_0038,X	; [G+0004+X] <- A,E (2 words)  (pointer V_0038 = #0004: address = G + #0004, G unknown)
0021FE:	F20A	ICX 	P  	=#0A	; ICX operand: #0A
002200:	5034	DLD 	DG 	G+0034	; A,E <- [G+0034] (2 words)
002202:	2502	ADD 	P  	=#02	; A <- A + #02
002204:	B630	DST 	ILX	*V_0030,X	; [G+0000+X] <- A,E (2 words)  (pointer V_0030 = #0000: address = G + #0000, G unknown)
002206:	2240	LDX 	P  	=#40	; X <- #40
002208:	F902	LDR 	P  	=#02	; LDR operand: #02
00220A:	2520	ADD 	P  	=#20	; A <- A + #20
00220C:	B10A	STA 	ILX	*CPT_PTR,X	; [G+0380+X] <- A  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
00220E:	2040	LDA 	P  	=#40	; A <- #40
002210:	FA08	STR 	P  	=#08	; STR operand: #08
002212:	F401	DIT 	P  		; DE-ACTIVATE INTERRUPT: save outgoing context, dispatch the highest-priority waiting level (not a plain return)
002214:	0006	LDA 	DL 	V_0006	; A <- [V_0006]
002216:	.EBCDIC	"NOTYPTPRPPDM"	; 6 words, raw: D5D6 E3E8 D7E3 D7D9 D7D7 C4D4
002222:	FFFF	??	FFFF
002224:	0006	LDA 	DL 	V_0006	; A <- [V_0006]
002226:	F000	SLLS	P  	
002228:	B000	DLD 	ILX	*Unknow_1,X	; A,E <- [G+0000+X] (2 words)  (pointer Unknow_1 = #0000: address = G + #0000, G unknown)
00222A:	F000	SLLS	P  	
00222C:	E000	SLLS	PX 	
00222E:	D000	BCT 	IL 	*Unknow_1	; conditional branch, target not known statically  (pointer Unknow_1 = #0000: address = G + #0000, G unknown)
002230:	F000	SLLS	P  	
002232:	0000	.ZERO	10	; 10 zero words (padding, reserved area or unused data; never reached as code)
002246:	001C	LDA 	DL 	V_001C	; A <- [V_001C]
002248:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
00224A:	1620	DST 	DL 	V_0020	; [V_0020] <- A,E (2 words)
00224C:	0000	.ZERO	7	; 7 zero words (padding, reserved area or unused data; never reached as code)
00225A:	0400	LEA 	DL 	Unknow_1	; A <- Unknow_1  (effective address, memory not read)
00225C:	0000	.ZERO	4	; 4 zero words (padding, reserved area or unused data; never reached as code)
002264:	0004	LDA 	DL 	V_0004	; A <- [V_0004]
002266:	26E8	SUB 	P  	=#E8	; A <- A - #E8
002268:	29FA	AND 	P  	=#FA	; A <- A and #FA
00226A:	2B90	CMP 	P  	=#90	; compare A with #90
00226C:	2CA6	MUL 	P  	=#A6	; multiply by #A6
00226E:	3026	SRCS	DL 	#06
002270:	0011	LDA 	DL 	V_0010	; A <- [V_0010]
002272:	.EBCDIC	"BIBOCIOCEIEOLOLLDOSISLULSYEPGIGO"	; 16 words, raw: C2C9 C2D6 C3C9 D6C3 C5C9 C5D6 D3D6 D3D3 C4D6 E2C9 E2D3 E4D3 E2E8 C5D7 C7C9 C7D6
002292:	00FF	LDA 	DL 	v_00FE	; A <- [v_00FE]
002294:	0060	LDA 	DL 	v_0060	; A <- [v_0060]
002296:	50A0	DLD 	DG 	G+00A0	; A,E <- [G+00A0] (2 words)
002298:	B0E0	DLD 	ILX	*v_00E0,X	; A,E <- [G+0000+X] (2 words)  (pointer v_00E0 = #0000: address = G + #0000, G unknown)
00229A:	D090	BCT 	IL 	*v_0090	; conditional branch, target not known statically  (pointer v_0090 = #018C: address = G + #018C, G unknown)
00229C:	9090	DLD 	IGX	*G+0090,X	; A,E <- [*G+0090+X] (2 words)  (pointer G+0090 lives in the calling task's area: its contents are only known at run time)
00229E:	A0F0	LDA 	ILX	*v_00F0,X	; A <- [G+0000+X]  (pointer v_00F0 = #0000: address = G + #0000, G unknown)
0022A0:	F0F0	SRCD	P  	#10
0022A2:	F0E0	SRCD	P  	
0022A4:	D000	BCT 	IL 	*Unknow_1	; conditional branch, target not known statically  (pointer Unknow_1 = #0000: address = G + #0000, G unknown)
0022A6:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
0022A8:	1800	SPA 	DL 	Unknow_1	; [Unknow_1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0022AA:	2000	LDA 	P  	=#00	; A <- #00
0022AC:	0801	DIV 	DL 	Unknow_1	; divide by [Unknow_1]
0022AE:	8001	LDA 	IGX	*TWB_P+1,X	; A <- [*TWB_P+1+X]  (pointer TWB_P+1 lives in the calling task's area: its contents are only known at run time)
0022B0:	2801	DIV 	P  	=#01	; divide by #01
0022B2:	2001	LDA 	P  	=#01	; A <- #01
0022B4:	3001	SLLS	DL 	#01
0022B6:	3001	SLLS	DL 	#01
0022B8:	0801	DIV 	DL 	Unknow_1	; divide by [Unknow_1]
0022BA:	1801	SPA 	DL 	Unknow_1	; [Unknow_1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0022BC:	2800	DIV 	P  	=#00	; divide by #00
0022BE:	2800	DIV 	P  	=#00	; divide by #00
0022C0:	2800	DIV 	P  	=#00	; divide by #00
0022C2:	2800	DIV 	P  	=#00	; divide by #00
0022C4:	2800	DIV 	P  	=#00	; divide by #00
0022C6:	2800	DIV 	P  	=#00	; divide by #00
0022C8:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
0022CA:	00EE	LDA 	DL 	V_00EE	; A <- [V_00EE]
0022CC:	0102	LDE 	DL 	V_0002	; E <- [V_0002]
0022CE:	02E2	LDX 	DL 	v_00E2	; X <- [v_00E2]
0022D0:	15F4	SBR 	DL 	v_00F4	; right byte of [v_00F4] <- A
0022D2:	0004	LDA 	DL 	V_0004	; A <- [V_0004]
0022D4:	5A08	FAD 	DG 	TWB_T0	; floating add [TWB_T0]
0022D6:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
0022D8:	0801	DIV 	DL 	Unknow_1	; divide by [Unknow_1]
0022DA:	0450	LEA 	DL 	V_0050	; A <- V_0050  (effective address, memory not read)
0022DC:	0002	LDA 	DL 	V_0002	; A <- [V_0002]
0022DE:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
0022E0:	38C2	CLS 	DL 	v_00C2	; CALL SECTION, number taken from [v_00C2] (CLS); return = RTS
0022E2:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
0022E4:	26E8	SUB 	P  	=#E8	; A <- A - #E8
0022E6:	0B9E	CMP 	DL 	V_009E	; compare A with [V_009E]
0022E8:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
0022EA:	14D0	SBL 	DL 	v_00D0	; left byte of [v_00D0] <- A
0022EC:	10BE	DLD 	DL 	v_00BE	; A,E <- [v_00BE] (2 words)
0022EE:	0000	.ZERO	3	; 3 zero words (padding, reserved area or unused data; never reached as code)
0022F4:	33BC	DCX 	DL 	v_00BC	; DCX operand: [v_00BC]
0022F6:	33BC	DCX 	DL 	v_00BC	; DCX operand: [v_00BC]
0022F8:	0000	.ZERO	3	; 3 zero words (padding, reserved area or unused data; never reached as code)
0022FE:	0038	LDA 	DL 	V_0038	; A <- [V_0038]
002300:	000C	LDA 	DL 	PRTS_PTR	; A <- [PRTS_PTR]
002302:	0006	LDA 	DL 	V_0006	; A <- [V_0006]
002304:	0400	LEA 	DL 	Unknow_1	; A <- Unknow_1  (effective address, memory not read)
002306:	33B4	DCX 	DL 	V_00B4	; DCX operand: [V_00B4]
002308:	33BC	DCX 	DL 	v_00BC	; DCX operand: [v_00BC]
00230A:	38D2	CLS 	DL 	V_00D2	; CALL SECTION, number taken from [V_00D2] (CLS); return = RTS
00230C:	15F4	SBR 	DL 	v_00F4	; right byte of [v_00F4] <- A
00230E:	0100	LDE 	DL 	Unknow_1	; E <- [Unknow_1]
002310:	7CD6	FMU 	IL 	*V_00D6	; floating multiply by [G+0000]  (pointer V_00D6 = #0000: address = G + #0000, G unknown)
002312:	0000	.ZERO	8	; 8 zero words (padding, reserved area or unused data; never reached as code)
002322:	0100	LDE 	DL 	Unknow_1	; E <- [Unknow_1]
002324:	0100	LDE 	DL 	Unknow_1	; E <- [Unknow_1]
002326:	15F4	SBR 	DL 	v_00F4	; right byte of [v_00F4] <- A
002328:	5A10	FAD 	DG 	TWB_T4	; floating add [TWB_T4]
00232A:	5A58	FAD 	DG 	G+0058	; floating add [G+0058]
00232C:	0078	LDA 	DL 	v_0078	; A <- [v_0078]
00232E:	00EE	LDA 	DL 	V_00EE	; A <- [V_00EE]
002330:	0150	LDE 	DL 	V_0050	; E <- [V_0050]
002332:	0326	EOR 	DL 	V_0026	; A <- A xor [V_0026]
002334:	0080	LDA 	DL 	V_0080	; A <- [V_0080]
002336:	8004	LDA 	IGX	*TWB_IND,X	; A <- [*TWB_IND+X]  (pointer TWB_IND lives in the calling task's area: its contents are only known at run time)
002338:	0076	LDA 	DL 	v_0076	; A <- [v_0076]
00233A:	0001	LDA 	DL 	Unknow_1	; A <- [Unknow_1]
00233C:	0080	LDA 	DL 	V_0080	; A <- [V_0080]
00233E:	0004	LDA 	DL 	V_0004	; A <- [V_0004]
002340:	5A08	FAD 	DG 	TWB_T0	; floating add [TWB_T0]
002342:	0050	LDA 	DL 	V_0050	; A <- [V_0050]
002344:	6C00	MUL 	IL 	*Unknow_1	; multiply by [G+0000]  (pointer Unknow_1 = #0000: address = G + #0000, G unknown)
002346:	0080	LDA 	DL 	V_0080	; A <- [V_0080]
002348:	8004	LDA 	IGX	*TWB_IND,X	; A <- [*TWB_IND+X]  (pointer TWB_IND lives in the calling task's area: its contents are only known at run time)
00234A:	5AF6	FAD 	DG 	G+00F6	; floating add [G+00F6]
00234C:	0006	LDA 	DL 	V_0006	; A <- [V_0006]
00234E:	006E	LDA 	DL 	v_006E	; A <- [v_006E]
002350:	000A	LDA 	DL 	CPT_PTR	; A <- [CPT_PTR]
002352:	8007	LDA 	IGX	*TWB_ZC+1,X	; A <- [*TWB_ZC+1+X]  (pointer TWB_ZC+1 lives in the calling task's area: its contents are only known at run time)
002354:	0000	.ZERO	14	; 14 zero words (padding, reserved area or unused data; never reached as code)
0023B0:	0000	.ZERO	3	; 3 zero words (padding, reserved area or unused data; never reached as code)
0023B6:	8007	LDA 	IGX	*TWB_ZC+1,X	; A <- [*TWB_ZC+1+X]  (pointer TWB_ZC+1 lives in the calling task's area: its contents are only known at run time)
0023B8:	59D2	STS 	DG 	G+00D2	; STS operand: [G+00D2]
0023BA:	0066	LDA 	DL 	L_0066	; A <- [L_0066]
0023BC:	0000	.ZERO	2	; 2 zero words (padding, reserved area or unused data; never reached as code)
0023C0:	0014	LDA 	DL 	V_0014	; A <- [V_0014]
0023C2:	0000	.ZERO	3	; 3 zero words (padding, reserved area or unused data; never reached as code)
0023C8:	2354	EOR 	P  	=#54	; A <- A xor #54
0023CA:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
0023CC:	246C	LEA 	P  	=#6C	; A <- #6C  (effective address, memory not read)
0023CE:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
0023D0:	F901	LDR 	P  	=#01	; LDR operand: #01
0023D2:	2504	ADD 	P  	=#04	; A <- A + #04
0023D4:	FA04	STR 	P  	=#04	; STR operand: #04
0023D6:	2D80	LBL 	P  	=#80	; A <- left byte of #80
0023D8:	2203	LDX 	P  	=#03	; X <- #03
0023DA:	F71B	CSV 	P  	=#1B	; SUPERVISOR CALL #27 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_ADRS  [-> M_ADRS: comparisons, supervisor calls: #16, returns to its CSV caller with RSV]
0023DC:	2000	LDA 	P  	=#00	; A <- #00
0023DE:	FA01	STR 	P  	=#01	; STR operand: #01
0023E0:	5154	STA 	DG 	G+0054	; [G+0054] <- A
0023E2:	6424	LEA 	IL 	*V_0024	; A <- G+7CD6  (effective address, memory not read)  (pointer V_0024 = #7CD6: address = G + #7CD6, G unknown)
0023E4:	5172	STA 	DG 	G+0072	; [G+0072] <- A
0023E6:	2550	ADD 	P  	=#50	; A <- A + #50
0023E8:	515C	STA 	DG 	G+005C	; [G+005C] <- A
L_23EA:				; branch target; 3 refs
0023EA:	2250	LDX 	P  	=#50	; X <- #50
0023EC:	2000	LDA 	P  	=#00	; A <- #00
0023EE:	2100	LDE 	P  	=#00	; E <- #00
L_23F0:				; branch target; 1 ref
0023F0:	F304	DCX 	P  	=#04	; DCX operand: #04
0023F2:	C303	BCF 	RP 	L_23F8	; 3 words forward: conditional branch (BCF) to L_23F8
0023F4:	9672	DST 	IGX	*G+0072,X	; [*G+0072+X] <- A,E (2 words)  (pointer G+0072 lives in the calling task's area: its contents are only known at run time)
0023F6:	CF03	BRU 	RM 	L_23F0	; 3 words backward: jump to L_23F0
L_23F8:				; branch target; 1 ref
0023F8:	FA01	STR 	P  	=#01	; STR operand: #01
0023FA:	5556	SBR 	DG 	G+0056	; right byte of [G+0056] <- A
0023FC:	5557	SBR 	DG 	G+0057	; right byte of [G+0057] <- A
0023FE:	4466	LEA 	DG 	G+0066	; A <- G+0066  (effective address, memory not read)
002400:	F702	CSV 	P  	=#02	; SUPERVISOR CALL #2 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry MON2  [-> MON2: block move / translate string (MVS/TRS) 2 loops, stores into the caller's TWB, returns to its CSV caller with RSV]
002402:	C718	BRU 	RP 	L_2432	; 24 words forward: jump to L_2432
002404:	F704	CSV 	P  	=#04	; SUPERVISOR CALL #4 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_WAIT  [-> M_WAIT: 2 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, ends with DIT/DITR]
002406:	4082	LDA 	DG 	G+0082	; A <- [G+0082]
002408:	2202	LDX 	P  	=#02	; X <- #02
00240A:	9180	STA 	IGX	*G+0080,X	; [*G+0080+X] <- A  (pointer G+0080 lives in the calling task's area: its contents are only known at run time)
00240C:	446E	LEA 	DG 	G+006E	; A <- G+006E  (effective address, memory not read)
00240E:	F702	CSV 	P  	=#02	; SUPERVISOR CALL #2 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry MON2  [-> MON2: block move / translate string (MVS/TRS) 2 loops, stores into the caller's TWB, returns to its CSV caller with RSV]
002410:	F704	CSV 	P  	=#04	; SUPERVISOR CALL #4 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_WAIT  [-> M_WAIT: 2 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, ends with DIT/DITR]
002412:	4084	LDA 	DG 	G+0084	; A <- [G+0084]
002414:	2202	LDX 	P  	=#02	; X <- #02
002416:	9180	STA 	IGX	*G+0080,X	; [*G+0080+X] <- A  (pointer G+0080 lives in the calling task's area: its contents are only known at run time)
002418:	446E	LEA 	DG 	G+006E	; A <- G+006E  (effective address, memory not read)
00241A:	F702	CSV 	P  	=#02	; SUPERVISOR CALL #2 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry MON2  [-> MON2: block move / translate string (MVS/TRS) 2 loops, stores into the caller's TWB, returns to its CSV caller with RSV]
00241C:	F704	CSV 	P  	=#04	; SUPERVISOR CALL #4 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_WAIT  [-> M_WAIT: 2 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, ends with DIT/DITR]
00241E:	2E1E	LBR 	P  	=#1E	; A <- right byte of #1E
002420:	5575	SBR 	DG 	G+0075	; right byte of [G+0075] <- A
002422:	2E04	LBR 	P  	=#04	; A <- right byte of #04
002424:	5571	SBR 	DG 	G+0071	; right byte of [G+0071] <- A
002426:	446E	LEA 	DG 	G+006E	; A <- G+006E  (effective address, memory not read)
002428:	F702	CSV 	P  	=#02	; SUPERVISOR CALL #2 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry MON2  [-> MON2: block move / translate string (MVS/TRS) 2 loops, stores into the caller's TWB, returns to its CSV caller with RSV]
00242A:	F704	CSV 	P  	=#04	; SUPERVISOR CALL #4 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_WAIT  [-> M_WAIT: 2 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, ends with DIT/DITR]
00242C:	2E50	LBR 	P  	=#50	; A <- right byte of #50
00242E:	5575	SBR 	DG 	G+0075	; right byte of [G+0075] <- A
002430:	C704	BRU 	RP 	l_2438	; 4 words forward: jump to l_2438
L_2432:				; branch target; 1 ref
002432:	446E	LEA 	DG 	G+006E	; A <- G+006E  (effective address, memory not read)
002434:	F702	CSV 	P  	=#02	; SUPERVISOR CALL #2 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry MON2  [-> MON2: block move / translate string (MVS/TRS) 2 loops, stores into the caller's TWB, returns to its CSV caller with RSV]
002436:	F704	CSV 	P  	=#04	; SUPERVISOR CALL #4 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_WAIT  [-> M_WAIT: 2 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, ends with DIT/DITR]
l_2438:				; branch target; 1 ref; only from untraced words
002438:	4E6E	LBR 	DG 	G+006E	; A <- right byte of [G+006E]
00243A:	2B44	CMP 	P  	=#44	; compare A with #44
00243C:	C829	BCT 	RM 	L_23EA	; 41 words backward: conditional branch (BCT) to L_23EA
00243E:	2200	LDX 	P  	=#00	; X <- #00
002440:	8E72	LBR 	IGX	*G+0072,X	; A <- right byte of [*G+0072+X]  (pointer G+0072 lives in the calling task's area: its contents are only known at run time)
002442:	2B0D	CMP 	P  	=#0D	; compare A with #0D
002444:	C04F	BCT 	RP 	L_24E2	; 79 words forward: conditional branch (BCT) to L_24E2
L_2446:				; branch target; 1 ref
002446:	F905	LDR 	P  	=#05	; LDR operand: #05
002448:	2B50	CMP 	P  	=#50	; compare A with #50
00244A:	C63A	BOF 	RP 	L_24BE	; 58 words forward: conditional branch (BOF) to L_24BE
00244C:	8E72	LBR 	IGX	*G+0072,X	; A <- right byte of [*G+0072+X]  (pointer G+0072 lives in the calling task's area: its contents are only known at run time)
00244E:	F201	ICX 	P  	=#01	; ICX operand: #01
002450:	2B5E	CMP 	P  	=#5E	; compare A with #5E
002452:	C004	BCT 	RP 	L_245A	; 4 words forward: conditional branch (BCT) to L_245A
002454:	2B0D	CMP 	P  	=#0D	; compare A with #0D
002456:	CB08	BCF 	RM 	L_2446	; 8 words backward: conditional branch (BCF) to L_2446
002458:	C702	BRU 	RP 	L_245C	; 2 words forward: jump to L_245C
L_245A:				; branch target; 1 ref
00245A:	5557	SBR 	DG 	G+0057	; right byte of [G+0057] <- A
L_245C:				; branch target; 1 ref
00245C:	4072	LDA 	DG 	G+0072	; A <- [G+0072]
00245E:	2200	LDX 	P  	=#00	; X <- #00
002460:	F70C	CSV 	P  	=#0C	; SUPERVISOR CALL #12 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_ANAC  [-> M_ANAC: stores into the caller's TWB, returns through its return cell]
002462:	C440	BAN 	RP 	L_24E2	; 64 words forward: conditional branch (BAN) to L_24E2
002464:	535A	STX 	DG 	G+005A	; [G+005A] <- X
002466:	420E	LDX 	DG 	TWB_T3	; X <- [TWB_T3]
002468:	400E	LDA 	DG 	TWB_T3	; A <- [TWB_T3]
00246A:	C740	BRU 	RP 	L_24EA	; 64 words forward: jump to L_24EA
00246C:	5610	DST 	DG 	TWB_T4	; [TWB_T4] <- A,E (2 words)
00246E:	530C	STX 	DG 	TWB_T2	; [TWB_T2] <- X
002470:	2000	LDA 	P  	=#00	; A <- #00
002472:	5160	STA 	DG 	G+0060	; [G+0060] <- A
002474:	400C	LDA 	DG 	TWB_T2	; A <- [TWB_T2]
002476:	4110	LDE 	DG 	TWB_T4	; E <- [TWB_T4]
002478:	F713	CSV 	P  	=#13	; SUPERVISOR CALL #19 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_CMPA  [-> M_CMPA: comparisons, 1 loop, stores into the caller's TWB, returns to its CSV caller with RSV]
00247A:	C405	BAN 	RP 	l_2484	; 5 words forward: conditional branch (BAN) to l_2484
00247C:	400C	LDA 	DG 	TWB_T2	; A <- [TWB_T2]
00247E:	4610	SUB 	DG 	TWB_T4	; A <- A - [TWB_T4]
002480:	2502	ADD 	P  	=#02	; A <- A + #02
002482:	C702	BRU 	RP 	l_2486	; 2 words forward: jump to l_2486
l_2484:				; branch target; 1 ref; only from untraced words
002484:	2002	LDA 	P  	=#02	; A <- #02
l_2486:				; branch target; 1 ref; only from untraced words
002486:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
002488:	6430	LEA 	IL 	*V_0030	; A <- G+0000  (effective address, memory not read)  (pointer V_0030 = #0000: address = G + #0000, G unknown)
00248A:	510E	STA 	DG 	TWB_T3	; [TWB_T3] <- A
00248C:	5710	ADM 	DG 	TWB_T4	; add A to [TWB_T4]
00248E:	400E	LDA 	DG 	TWB_T3	; A <- [TWB_T3]
002490:	570C	ADM 	DG 	TWB_T2	; add A to [TWB_T2]
002492:	4E12	LBR 	DG 	TWB_T5	; A <- right byte of [TWB_T5]
002494:	C508	BAZ 	RP 	l_24A4	; 8 words forward: conditional branch (BAZ) to l_24A4
002496:	4436	LEA 	DG 	G+0036	; A <- G+0036  (effective address, memory not read)
002498:	FA05	STR 	P  	=#05	; STR operand: #05
00249A:	4108	LDE 	DG 	TWB_T0	; E <- [TWB_T0]
00249C:	4010	LDA 	DG 	TWB_T4	; A <- [TWB_T4]
00249E:	F70E	CSV 	P  	=#0E	; SUPERVISOR CALL #14 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_EDIT  [-> M_EDIT: 4 loops, stores into the caller's TWB, returns to its CSV caller with RSV]
0024A0:	C408	BAN 	RP 	l_24B0	; 8 words forward: conditional branch (BAN) to l_24B0
0024A2:	C706	BRU 	RP 	l_24AE	; 6 words forward: jump to l_24AE
l_24A4:				; branch target; 1 ref; only from untraced words
0024A4:	4460	LEA 	DG 	G+0060	; A <- G+0060  (effective address, memory not read)
0024A6:	4110	LDE 	DG 	TWB_T4	; E <- [TWB_T4]
0024A8:	420C	LDX 	DG 	TWB_T2	; X <- [TWB_T2]
0024AA:	F714	CSV 	P  	=#14	; SUPERVISOR CALL #20 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_DUMP  [-> M_DUMP: 6block move / translate string (MVS/TRS) 4 loops, stores into the caller's TWB, returns to its CSV caller with RSV]
0024AC:	C402	BAN 	RP 	l_24B0	; 2 words forward: conditional branch (BAN) to l_24B0
l_24AE:				; branch target; 1 ref; only from untraced words
0024AE:	F701	CSV 	P  	=#01	; SUPERVISOR CALL #1 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_EXIT  [-> M_EXIT: privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, ends with DIT/DITR]
l_24B0:				; branch target; 2 refs; only from untraced words
0024B0:	F712	CSV 	P  	=#12	; SUPERVISOR CALL #18 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_ABRT  [-> M_ABRT: shifts/bit manipulation 1 loop, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, ends with DIT/DITR]
L_24B2:				; branch target; 2 refs
0024B2:	2D24	LBL 	P  	=#24	; A <- left byte of #24
0024B4:	C715	BRU 	RP 	L_24DE	; 21 words forward: jump to L_24DE
L_24B6:				; branch target; 4 refs
0024B6:	2D25	LBL 	P  	=#25	; A <- left byte of #25
0024B8:	C713	BRU 	RP 	L_24DE	; 19 words forward: jump to L_24DE
l_24BA:				; branch target; 1 ref; only from untraced words
0024BA:	2D31	LBL 	P  	=#31	; A <- left byte of #31
0024BC:	C711	BRU 	RP 	L_24DE	; 17 words forward: jump to L_24DE
L_24BE:				; branch target; 2 refs
0024BE:	2D32	LBL 	P  	=#32	; A <- left byte of #32
0024C0:	C70F	BRU 	RP 	L_24DE	; 15 words forward: jump to L_24DE
l_24C2:				; branch target; 1 ref; only from untraced words
0024C2:	2D41	LBL 	P  	=#41	; A <- left byte of #41
0024C4:	C70D	BRU 	RP 	L_24DE	; 13 words forward: jump to L_24DE
L_24C6:				; branch target; 4 refs
0024C6:	2D42	LBL 	P  	=#42	; A <- left byte of #42
0024C8:	C70B	BRU 	RP 	L_24DE	; 11 words forward: jump to L_24DE
L_24CA:				; branch target; 3 refs
0024CA:	2D43	LBL 	P  	=#43	; A <- left byte of #43
0024CC:	C709	BRU 	RP 	L_24DE	; 9 words forward: jump to L_24DE
L_24CE:				; branch target; 2 refs
0024CE:	2D44	LBL 	P  	=#44	; A <- left byte of #44
0024D0:	C707	BRU 	RP 	L_24DE	; 7 words forward: jump to L_24DE
L_24D2:				; branch target; 2 refs
0024D2:	2D45	LBL 	P  	=#45	; A <- left byte of #45
0024D4:	C705	BRU 	RP 	L_24DE	; 5 words forward: jump to L_24DE
0024D6:	2D46	LBL 	P  	=#46	; A <- left byte of #46
0024D8:	C703	BRU 	RP 	L_24DE	; 3 words forward: jump to L_24DE
0024DA:	2D47	LBL 	P  	=#47	; A <- left byte of #47
0024DC:	C701	BRU 	RP 	L_24DE	; 1 word forward: jump to L_24DE
L_24DE:				; branch target; 11 refs
0024DE:	F70F	CSV 	P  	=#0F	; SUPERVISOR CALL #15 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_ERED  [-> M_ERED: 3 loops, stores into the caller's TWB, returns to its CSV caller with RSV]
0024E0:	C701	BRU 	RP 	L_24E2	; 1 word forward: jump to L_24E2
L_24E2:				; branch target; 10 refs
0024E2:	4E57	LBR 	DG 	G+0057	; A <- right byte of [G+0057]
0024E4:	CB7D	BCF 	RM 	L_23EA	; 125 words backward: conditional branch (BCF) to L_23EA
0024E6:	F401	DIT 	P  		; DE-ACTIVATE INTERRUPT: save outgoing context, dispatch the highest-priority waiting level (not a plain return)

; ===== INT5: INTERRUPT ROUTINE (ends with DIT/DITR: leaves through the interrupt scheduler) =====
; routine profile: block move / translate string (MVS/TRS) 21 loops, stores into the caller's TWB, supervisor calls: #2 #4 #15 #12 ..., ends with DIT/DITR
INT5:				; entry point; 1 ref
0024E8:	CF7F	BRU 	RM 	L_23EA	; 127 words backward: jump to L_23EA
L_24EA:				; branch target; 1 ref
0024EA:	C100	BRX 	RP 	$+X	; INDEXED JUMP (BRX): X selects one of the 2 BRU entries that follow (jump / dispatch table)
L_24EC:				; branch target; 1 ref
0024EC:	C70F	BRU 	RP 	L_250A	; 15 words forward: jump to L_250A
L_24EE:				; branch target; 1 ref
0024EE:	C745	BRU 	RP 	L_2578	; 69 words forward: jump to L_2578
0024F0:	.EBCDIC	"GaGoGn"	; 3 words, raw: C781 C796 C795
0024F6:	CF22	BRU 	RM 	L_24B2	; 34 words backward: jump to L_24B2
0024F8:	.EBCDIC	"GlGkGjGdGb"	; 5 words, raw: C793 C792 C791 C784 C782
002502:	C78E	BRU 	RP 	l_261E	; 142 words forward: jump to l_261E
002504:	C78D	BRU 	RP 	l_261E	; 141 words forward: jump to l_261E
002506:	C779	BRU 	RP 	l_25F8	; 121 words forward: jump to l_25F8
002508:	C78B	BRU 	RP 	l_261E	; 139 words forward: jump to l_261E
L_250A:				; branch target; 2 refs
00250A:	4E2C	LBR 	DG 	G+002C	; A <- right byte of [G+002C]
00250C:	2902	AND 	P  	=#02	; A <- A and #02
00250E:	C305	BCF 	RP 	L_2518	; 5 words forward: conditional branch (BCF) to L_2518
002510:	2000	LDA 	P  	=#00	; A <- #00
002512:	F71B	CSV 	P  	=#1B	; SUPERVISOR CALL #27 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_ADRS  [-> M_ADRS: comparisons, supervisor calls: #16, returns to its CSV caller with RSV]
002514:	6B0A	CMP 	IL 	*CPT_PTR	; compare A with [G+0380]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
002516:	CB32	BCF 	RM 	L_24B2	; 50 words backward: conditional branch (BCF) to L_24B2
L_2518:				; branch target; 1 ref
002518:	2201	LDX 	P  	=#01	; X <- #01
00251A:	F70C	CSV 	P  	=#0C	; SUPERVISOR CALL #12 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_ANAC  [-> M_ANAC: stores into the caller's TWB, returns through its return cell]
00251C:	CC1D	BAN 	RM 	L_24E2	; 29 words backward: conditional branch (BAN) to L_24E2
00251E:	F70B	CSV 	P  	=#0B	; SUPERVISOR CALL #11 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_LOAD  [-> M_LOAD: 19block move / translate string (MVS/TRS), multiply/divide, shifts/bit manipulation 6 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_0AB8 ..., returns to its CSV caller with RSV]
002520:	CC1F	BAN 	RM 	L_24E2	; 31 words backward: conditional branch (BAN) to L_24E2
002522:	4E40	LBR 	DG 	G+0040	; A <- right byte of [G+0040]
002524:	2B01	CMP 	P  	=#01	; compare A with #01
002526:	C312	BCF 	RP 	L_254A	; 18 words forward: conditional branch (BCF) to L_254A
002528:	220F	LDX 	P  	=#0F	; X <- #0F
00252A:	8E3E	LBR 	IGX	*G+003E,X	; A <- right byte of [*G+003E+X]  (pointer G+003E lives in the calling task's area: its contents are only known at run time)
00252C:	C50F	BAZ 	RP 	L_254A	; 15 words forward: conditional branch (BAZ) to L_254A
00252E:	2210	LDX 	P  	=#10	; X <- #10
002530:	803E	LDA 	IGX	*G+003E,X	; A <- [*G+003E+X]  (pointer G+003E lives in the calling task's area: its contents are only known at run time)
002532:	453E	ADD 	DG 	G+003E	; A <- A + [G+003E]
002534:	C83F	BCT 	RM 	L_24B6	; 63 words backward: conditional branch (BCT) to L_24B6
002536:	2510	ADD 	P  	=#10	; A <- A + #10
002538:	C841	BCT 	RM 	L_24B6	; 65 words backward: conditional branch (BCT) to L_24B6
00253A:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
00253C:	F902	LDR 	P  	=#02	; LDR operand: #02
00253E:	5708	ADM 	DG 	TWB_T0	; add A to [TWB_T0]
002540:	C845	BCT 	RM 	L_24B6	; 69 words backward: conditional branch (BCT) to L_24B6
002542:	4042	LDA 	DG 	G+0042	; A <- [G+0042]
002544:	4108	LDE 	DG 	TWB_T0	; E <- [TWB_T0]
002546:	F713	CSV 	P  	=#13	; SUPERVISOR CALL #19 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_CMPA  [-> M_CMPA: comparisons, 1 loop, stores into the caller's TWB, returns to its CSV caller with RSV]
002548:	CC49	BAN 	RM 	L_24B6	; 73 words backward: conditional branch (BAN) to L_24B6
L_254A:				; branch target; 2 refs
00254A:	4E40	LBR 	DG 	G+0040	; A <- right byte of [G+0040]
00254C:	2B01	CMP 	P  	=#01	; compare A with #01
00254E:	C306	BCF 	RP 	L_255A	; 6 words forward: conditional branch (BCF) to L_255A
002550:	4E41	LBR 	DG 	G+0041	; A <- right byte of [G+0041]
002552:	2D81	LBL 	P  	=#81	; A <- left byte of #81
002554:	5154	STA 	DG 	G+0054	; [G+0054] <- A
002556:	403E	LDA 	DG 	G+003E	; A <- [G+003E]
002558:	5158	STA 	DG 	G+0058	; [G+0058] <- A
L_255A:				; branch target; 1 ref
00255A:	4E56	LBR 	DG 	G+0056	; A <- right byte of [G+0056]
00255C:	CD3D	BAZ 	RM 	L_24E2	; 61 words backward: conditional branch (BAZ) to L_24E2
00255E:	4420	LEA 	DG 	G+0020	; A <- G+0020  (effective address, memory not read)
002560:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
002562:	2000	LDA 	P  	=#00	; A <- #00
002564:	220E	LDX 	P  	=#0E	; X <- #0E
L_2566:				; branch target; 1 ref
002566:	9108	STA 	IGX	*TWB_T0,X	; [*TWB_T0+X] <- A
002568:	F302	DCX 	P  	=#02	; DCX operand: #02
00256A:	C802	BCT 	RM 	L_2566	; 2 words backward: conditional branch (BCT) to L_2566
00256C:	C706	BRU 	RP 	L_2578	; 6 words forward: jump to L_2578
l_256E:				; branch target; 1 ref; only from untraced words
00256E:	CF5A	BRU 	RM 	l_24BA	; 90 words backward: jump to l_24BA
002570:	CF59	BRU 	RM 	L_24BE	; 89 words backward: jump to L_24BE
002572:	CF54	BRU 	RM 	L_24CA	; 84 words backward: jump to L_24CA
002574:	CF53	BRU 	RM 	L_24CE	; 83 words backward: jump to L_24CE
002576:	CF52	BRU 	RM 	L_24D2	; 82 words backward: jump to L_24D2
L_2578:				; branch target; 2 refs
002578:	4D54	LBL 	DG 	G+0054	; A <- left byte of [G+0054]
00257A:	C402	BAN 	RP 	L_257E	; 2 words forward: conditional branch (BAN) to L_257E
00257C:	CF5B	BRU 	RM 	L_24C6	; 91 words backward: jump to L_24C6
L_257E:				; branch target; 1 ref
00257E:	CD5C	BAZ 	RM 	L_24C6	; 92 words backward: conditional branch (BAZ) to L_24C6
002580:	4E20	LBR 	DG 	G+0020	; A <- right byte of [G+0020]
002582:	C504	BAZ 	RP 	L_258A	; 4 words forward: conditional branch (BAZ) to L_258A
002584:	4E55	LBR 	DG 	G+0055	; A <- right byte of [G+0055]
002586:	CD5E	BAZ 	RM 	L_24CA	; 94 words backward: conditional branch (BAZ) to L_24CA
002588:	C704	BRU 	RP 	L_2590	; 4 words forward: jump to L_2590
L_258A:				; branch target; 1 ref
00258A:	4E55	LBR 	DG 	G+0055	; A <- right byte of [G+0055]
00258C:	C502	BAZ 	RP 	L_2590	; 2 words forward: conditional branch (BAZ) to L_2590
00258E:	CF60	BRU 	RM 	L_24CE	; 96 words backward: jump to L_24CE
L_2590:				; branch target; 2 refs
002590:	4F20	LBX 	DG 	G+0020	; LBX operand: [G+0020]
002592:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
002594:	2000	LDA 	P  	=#00	; A <- #00
002596:	FA01	STR 	P  	=#01	; STR operand: #01
002598:	F905	LDR 	P  	=#05	; LDR operand: #05
00259A:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
00259C:	C502	BAZ 	RP 	L_25A0	; 2 words forward: conditional branch (BAZ) to L_25A0
00259E:	2001	LDA 	P  	=#01	; A <- #01
L_25A0:				; branch target; 1 ref
0025A0:	F71B	CSV 	P  	=#1B	; SUPERVISOR CALL #27 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_ADRS  [-> M_ADRS: comparisons, supervisor calls: #16, returns to its CSV caller with RSV]
0025A2:	4208	LDX 	DG 	TWB_T0	; X <- [TWB_T0]
0025A4:	AB0A	CMP 	ILX	*CPT_PTR,X	; compare A with [G+0380+X]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
0025A6:	CB6A	BCF 	RM 	L_24D2	; 106 words backward: conditional branch (BCF) to L_24D2
0025A8:	F902	LDR 	P  	=#02	; LDR operand: #02
0025AA:	4558	ADD 	DG 	G+0058	; A <- A + [G+0058]
0025AC:	FA01	STR 	P  	=#01	; STR operand: #01
0025AE:	2202	LDX 	P  	=#02	; X <- #02
0025B0:	F70C	CSV 	P  	=#0C	; SUPERVISOR CALL #12 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_ANAC  [-> M_ANAC: stores into the caller's TWB, returns through its return cell]
0025B2:	CC68	BAN 	RM 	L_24E2	; 104 words backward: conditional branch (BAN) to L_24E2
0025B4:	4064	LDA 	DG 	G+0064	; A <- [G+0064]
0025B6:	4F20	LBX 	DG 	G+0020	; LBX operand: [G+0020]
0025B8:	C302	BCF 	RP 	L_25BC	; 2 words forward: conditional branch (BCF) to L_25BC
0025BA:	4062	LDA 	DG 	G+0062	; A <- [G+0062]
L_25BC:				; branch target; 1 ref
0025BC:	1102	STA 	DL 	V_0002	; [V_0002] <- A
0025BE:	F908	LDR 	P  	=#08	; LDR operand: #08
0025C0:	F0C1	SRLS	P  	#01
0025C2:	4D20	LBL 	DG 	G+0020	; A <- left byte of [G+0020]
0025C4:	F108	XAA 	P  	
0025C6:	4F56	LBX 	DG 	G+0056	; LBX operand: [G+0056]
0025C8:	F106	XEX 	P  		; exchange E and X
0025CA:	1604	DST 	DL 	V_0004	; [V_0004] <- A,E (2 words)
0025CC:	2000	LDA 	P  	=#00	; A <- #00
0025CE:	FA01	STR 	P  	=#01	; STR operand: #01
0025D0:	4E56	LBR 	DG 	G+0056	; A <- right byte of [G+0056]
0025D2:	C505	BAZ 	RP 	L_25DC	; 5 words forward: conditional branch (BAZ) to L_25DC
0025D4:	2150	LDE 	P  	=#50	; E <- #50
0025D6:	405A	LDA 	DG 	G+005A	; A <- [G+005A]
0025D8:	4262	LDX 	DG 	G+0062	; X <- [G+0062]
0025DA:	BF24	MVS 	ILX	*V_0024,X	; MVS operand: [G+7CD6+X]  (pointer V_0024 = #7CD6: address = G + #7CD6, G unknown)
L_25DC:				; branch target; 1 ref
0025DC:	4F20	LBX 	DG 	G+0020	; LBX operand: [G+0020]
0025DE:	E200	ICX 	PX 	(X)	; ICX operand: [(X)]
0025E0:	F902	LDR 	P  	=#02	; LDR operand: #02
0025E2:	4558	ADD 	DG 	G+0058	; A <- A + [G+0058]
0025E4:	B10A	STA 	ILX	*CPT_PTR,X	; [G+0380+X] <- A  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
0025E6:	F902	LDR 	P  	=#02	; LDR operand: #02
0025E8:	4560	ADD 	DG 	G+0060	; A <- A + [G+0060]
0025EA:	FA01	STR 	P  	=#01	; STR operand: #01
0025EC:	2001	LDA 	P  	=#01	; A <- #01
0025EE:	5554	SBR 	DG 	G+0054	; right byte of [G+0054] <- A
l_25F0:				; branch target; 1 ref; only from untraced words
0025F0:	CF87	BRU 	RM 	L_24E2	; 135 words backward: jump to L_24E2
0025F2:	2001	LDA 	P  	=#01	; A <- #01
0025F4:	5556	SBR 	DG 	G+0056	; right byte of [G+0056] <- A
0025F6:	CF76	BRU 	RM 	L_250A	; 118 words backward: jump to L_250A
l_25F8:				; branch target; 1 ref; only from untraced words
0025F8:	4E20	LBR 	DG 	G+0020	; A <- right byte of [G+0020]
0025FA:	2B02	CMP 	P  	=#02	; compare A with #02
0025FC:	C89B	BCT 	RM 	L_24C6	; 155 words backward: conditional branch (BCT) to L_24C6
0025FE:	420E	LDX 	DG 	TWB_T3	; X <- [TWB_T3]
002600:	F70C	CSV 	P  	=#0C	; SUPERVISOR CALL #12 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_ANAC  [-> M_ANAC: stores into the caller's TWB, returns through its return cell]
002602:	CF90	BRU 	RM 	L_24E2	; 144 words backward: jump to L_24E2
002604:	C70E	BRU 	RP 	l_2620	; 14 words forward: jump to l_2620
002606:	210E	LDE 	P  	=#0E	; E <- #0E
002608:	405C	LDA 	DG 	G+005C	; A <- [G+005C]
00260A:	F104	XAX 	P  		; exchange A and X
00260C:	2020	LDA 	P  	=#20	; A <- #20
00260E:	F70D	CSV 	P  	=#0D	; SUPERVISOR CALL #13 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_MOVE  [-> M_MOVE: 1 loop, stores into the caller's TWB, returns to its CSV caller with RSV]
002610:	405C	LDA 	DG 	G+005C	; A <- [G+005C]
002612:	F724	CSV 	P  	=#24	; SUPERVISOR CALL #36 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_ASGN  [-> M_ASGN: shifts/bit manipulation 9 loops, stores into the caller's TWB, returns to its CSV caller with RSV]
002614:	C101	BRX 	RP 	$+X	; INDEXED JUMP (BRX): X selects one of the 5 BRU entries that follow (jump / dispatch table)
l_2616:				; branch target; 1 ref; only from untraced words
002616:	CF9A	BRU 	RM 	L_24E2	; 154 words backward: jump to L_24E2
l_2618:				; branch target; 1 ref; only from untraced words
002618:	CFAB	BRU 	RM 	l_24C2	; 171 words backward: jump to l_24C2
l_261A:				; branch target; 1 ref; only from untraced words
00261A:	CFAA	BRU 	RM 	L_24C6	; 170 words backward: jump to L_24C6
l_261C:				; branch target; 1 ref; only from untraced words
00261C:	CFA9	BRU 	RM 	L_24CA	; 169 words backward: jump to L_24CA
l_261E:				; branch target; 4 refs; only from untraced words
00261E:	C701	BRU 	RP 	l_2620	; 1 word forward: jump to l_2620
l_2620:				; branch target; 2 refs; only from untraced words
002620:	420E	LDX 	DG 	TWB_T3	; X <- [TWB_T3]
002622:	F70C	CSV 	P  	=#0C	; SUPERVISOR CALL #12 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_ANAC  [-> M_ANAC: stores into the caller's TWB, returns through its return cell]
002624:	CF1A	BRU 	RM 	l_25F0	; 26 words backward: jump to l_25F0
002626:	CF5C	BRU 	RM 	l_256E	; 92 words backward: jump to l_256E; system structure
; 
; ==============================================
CTX5:				; system structure
; ============================================

002628:	0006	.WORD	#0006	; CTX level 5: saved status
00262A:	0000	.WORD	#0000	; CTX level 5: saved X
00262C:	FFFF	.WORD	#FFFF	; CTX level 5: saved E
00262E:	0000	.WORD	#0000	; CTX level 5: saved A
002630:	22CE	.WORD	#22CE	; CTX level 5: saved G
002632:	23BC	.WORD	#23BC	; CTX level 5: saved L
002634:	24E8	.WORD	#24E8	; CTX level 5: saved P (entry of the task / interrupt routine of this level)
002636:	0100	LDE 	DL 	Unknow_1	; E <- [Unknow_1]
002638:	0008	LDA 	DL 	V_0008	; A <- [V_0008]
00263A:	000A	LDA 	DL 	CPT_PTR	; A <- [CPT_PTR]
00263C:	0000	.ZERO	3	; 3 zero words (padding, reserved area or unused data; never reached as code)
002642:	569A	DST 	DG 	G+009A	; [G+009A] <- A,E (2 words)
002644:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)

; ===== INT31: program entry point =====
; routine profile: privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB
INT31:				; entry point; 1 ref
002646:	F901	LDR 	P  	=#01	; LDR operand: #01
002648:	5102	STA 	DG 	TWB_L	; [TWB_L] <- A
00264A:	4602	SUB 	DG 	TWB_L	; A <- A - [TWB_L]
00264C:	FA01	STR 	P  	=#01	; STR operand: #01
00264E:	223C	LDX 	P  	=#3C	; X <- #3C
002650:	A20A	LDX 	ILX	*CPT_PTR,X	; X <- [G+0380+X]  (pointer CPT_PTR = #0380: address = G + #0380, G unknown)
002652:	4002	LDA 	DG 	TWB_L	; A <- [TWB_L]
002654:	FA01	STR 	P  	=#01	; STR operand: #01
002656:	5300	STX 	DG 	TWB_P	; [TWB_P] <- X
002658:	F208	ICX 	P  	=#08	; ICX operand: #08
00265A:	A200	LDX 	ILX	*Unknow_1,X	; X <- [G+0000+X]  (pointer Unknow_1 = #0000: address = G + #0000, G unknown)
00265C:	5304	STX 	DG 	TWB_IND	; [TWB_IND] <- X
00265E:	F302	DCX 	P  	=#02	; DCX operand: #02
002660:	2100	LDE 	P  	=#00	; E <- #00
002662:	A000	LDA 	ILX	*Unknow_1,X	; A <- [G+0000+X]  (pointer Unknow_1 = #0000: address = G + #0000, G unknown)
002664:	4504	ADD 	DG 	TWB_IND	; A <- A + [TWB_IND]
002666:	4200	LDX 	DG 	TWB_P	; X <- [TWB_P]
002668:	F20A	ICX 	P  	=#0A	; ICX operand: #0A
00266A:	B600	DST 	ILX	*Unknow_1,X	; [G+0000+X] <- A,E (2 words)  (pointer Unknow_1 = #0000: address = G + #0000, G unknown)
00266C:	2100	LDE 	P  	=#00	; E <- #00
00266E:	F403	WD  	P  		; privileged control instruction (WD); system structure
; 
; ==============================================
CTX31:				; system structure
; ============================================

002670:	0014	.WORD	#0014	; CTX level 31: saved status
002672:	0000	.WORD	#0000	; CTX level 31: saved X
002674:	0000	.WORD	#0000	; CTX level 31: saved E
002676:	0000	.WORD	#0000	; CTX level 31: saved A
002678:	263C	.WORD	#263C	; CTX level 31: saved G
00267A:	2644	.WORD	#2644	; CTX level 31: saved L
00267C:	2646	.WORD	#2646	; CTX level 31: saved P (entry of the task / interrupt routine of this level)
00267E:	0100	LDE 	DL 	Unknow_1	; E <- [Unknow_1]
002680:	0020	LDA 	DL 	V_0020	; A <- [V_0020]
002682:	0020	LDA 	DL 	V_0020	; A <- [V_0020]
002684:	0000	.ZERO	2	; 2 zero words (padding, reserved area or unused data; never reached as code)
l_2688:				; branch target; 1 ref; only from untraced words
002688:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
00268A:	5652	DST 	DG 	G+0052	; [G+0052] <- A,E (2 words)
00268C:	0000	.ZERO	12	; 12 zero words (padding, reserved area or unused data; never reached as code)

; ===== INT30: program entry point =====
; routine profile: supervisor call(s) with a module number from memory
INT30:				; entry point; 1 ref
0026A4:	2000	LDA 	P  	=#00	; A <- #00
0026A6:	FA0C	STR 	P  	=#0C	; STR operand: #0C
0026A8:	020E	LDX 	DL 	PRTS_COUNT	; X <- [PRTS_COUNT]
0026AA:	F301	DCX 	P  	=#01	; DCX operand: #01
0026AC:	2102	LDE 	P  	=#02	; E <- #02
0026AE:	E700	CSV 	PX 	(X)	; SUPERVISOR CALL, module number taken from [(X)] (CSV); module ends with RSV; system structure
; 
; ==============================================
CTX30:				; system structure
; ============================================

0026B0:	0014	.WORD	#0014	; CTX level 30: saved status
0026B2:	0000	.WORD	#0000	; CTX level 30: saved X
0026B4:	0000	.WORD	#0000	; CTX level 30: saved E
0026B6:	0000	.WORD	#0000	; CTX level 30: saved A
0026B8:	2684	.WORD	#2684	; CTX level 30: saved G
0026BA:	26A4	.WORD	#26A4	; CTX level 30: saved L
0026BC:	26A4	.WORD	#26A4	; CTX level 30: saved P (entry of the task / interrupt routine of this level)
0026BE:	0100	LDE 	DL 	Unknow_1	; E <- [Unknow_1]
0026C0:	0020	LDA 	DL 	V_0020	; A <- [V_0020]
0026C2:	018E	LDE 	DL 	v_008E	; E <- [v_008E]
0026C4:	0020	LDA 	DL 	V_0020	; A <- [V_0020]
0026C6:	0090	LDA 	DL 	v_0090	; A <- [v_0090]
0026C8:	00B4	LDA 	DL 	V_00B4	; A <- [V_00B4]
0026CA:	0020	LDA 	DL 	V_0020	; A <- [V_0020]
0026CC:	0016	LDA 	DL 	V_0016	; A <- [V_0016]
0026CE:	560E	DST 	DG 	TWB_T3	; [TWB_T3] <- A,E (2 words)
0026D0:	33F2	DCX 	DL 	v_00F2	; DCX operand: [v_00F2]
0026D2:	0801	DIV 	DL 	Unknow_1	; divide by [Unknow_1]
0026D4:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
0026D6:	8000	LDA 	IGX	*TWB_P,X	; A <- [*TWB_P+X]  (pointer TWB_P lives in the calling task's area: its contents are only known at run time)
0026D8:	07B2	IOR 	DL 	V_00B2	; A <- A or [V_00B2]
0026DA:	0001	LDA 	DL 	Unknow_1	; A <- [Unknow_1]
0026DC:	006E	LDA 	DL 	v_006E	; A <- [v_006E]
0026DE:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
0026E0:	0400	LEA 	DL 	Unknow_1	; A <- Unknow_1  (effective address, memory not read)
0026E2:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
0026E4:	0796	IOR 	DL 	V_0096	; A <- A or [V_0096]
0026E6:	07E8	IOR 	DL 	V_00E8	; A <- A or [V_00E8]
0026E8:	2856	DIV 	P  	=#56	; divide by #56
0026EA:	28DA	DIV 	P  	=#DA	; divide by #DA
0026EC:	26E8	SUB 	P  	=#E8	; A <- A - #E8
0026EE:	0684	SUB 	DL 	v_0084	; A <- A - [v_0084]
0026F0:	0000	.ZERO	5	; 5 zero words (padding, reserved area or unused data; never reached as code)
0026FA:	000D	LDA 	DL 	PRTS_PTR	; A <- [PRTS_PTR]
0026FC:	0001	LDA 	DL 	Unknow_1	; A <- [Unknow_1]
0026FE:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
002700:	0029	LDA 	DL 	V_0028	; A <- [V_0028]
002702:	0006	LDA 	DL 	V_0006	; A <- [V_0006]
002704:	33F2	DCX 	DL 	v_00F2	; DCX operand: [v_00F2]
002706:	0801	DIV 	DL 	Unknow_1	; divide by [Unknow_1]
002708:	349E	??	349E
00270A:	34AB	??	34AB
00270C:	270E	IOR 	P  	=#0E	; A <- A or #0E
00270E:	A5C5	ADD 	ILX	*v_00C4,X	; A <- A + [G+0BB8+X]  (pointer v_00C4 = #0BB8: address = G + #0BB8, G unknown)
002710:	CF44	BRU 	RM 	l_2688	; 68 words backward: jump to l_2688
002712:	6D0D	LBL 	IL 	*PRTS_PTR	; A <- left byte of [*PRTS_PTR]
002714:	271E	IOR 	P  	=#1E	; A <- A or #1E
002716:	271E	IOR 	P  	=#1E	; A <- A or #1E
002718:	0D0A	LBL 	DL 	CPT_PTR	; A <- left byte of [CPT_PTR]
00271A:	0D0D	LBL 	DL 	PRTS_PTR	; A <- left byte of [PRTS_PTR]
00271C:	0D0A	LBL 	DL 	CPT_PTR	; A <- left byte of [CPT_PTR]
00271E:	4D56	LBL 	DG 	G+0056	; A <- left byte of [G+0056]
002720:	AFC0	LBX 	ILX	*v_00C0,X	; LBX operand: [G+0D90+X]  (pointer v_00C0 = #0D90: address = G + #0D90, G unknown)
002722:	30AC	SAS 	DL 	#0C
002724:	C0A6	BCT 	RP 	l_2870	; 166 words forward: conditional branch (BCT) to l_2870
002726:	3333	DCX 	DL 	V_0032	; DCX operand: [V_0032]
002728:	4236	LDX 	DG 	G+0036	; X <- [G+0036]
00272A:	8DB8	LBL 	IGX	*G+00B8,X	; A <- left byte of [*G+00B8+X]  (pointer G+00B8 lives in the calling task's area: its contents are only known at run time)
00272C:	44B2	LEA 	DG 	G+00B2	; A <- G+00B2  (effective address, memory not read)
00272E:	A000	LDA 	ILX	*Unknow_1,X	; A <- [G+0000+X]  (pointer Unknow_1 = #0000: address = G + #0000, G unknown)
002730:	0000	.ZERO	2	; 2 zero words (padding, reserved area or unused data; never reached as code)
002734:	2738	IOR 	P  	=#38	; A <- A or #38
002736:	2748	IOR 	P  	=#48	; A <- A or #48
002738:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
00273A:	FF01	??	FF01
00273C:	0101	LDE 	DL 	Unknow_1	; E <- [Unknow_1]
00273E:	0101	LDE 	DL 	Unknow_1	; E <- [Unknow_1]
002740:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
002742:	0100	LDE 	DL 	Unknow_1	; E <- [Unknow_1]
002744:	FF02	??	FF02
002746:	FF02	??	FF02
002748:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
00274A:	FF02	??	FF02
00274C:	0202	LDX 	DL 	V_0002	; X <- [V_0002]
00274E:	0202	LDX 	DL 	V_0002	; X <- [V_0002]
002750:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
002752:	0202	LDX 	DL 	V_0002	; X <- [V_0002]
002754:	FF02	??	FF02
002756:	FF02	??	FF02
L_2758:				; branch target; 1 ref
002758:	0016	LDA 	DL 	V_0016	; A <- [V_0016]
00275A:	C512	BAZ 	RP 	L_277E	; 18 words forward: conditional branch (BAZ) to L_277E
00275C:	2000	LDA 	P  	=#00	; A <- #00
00275E:	1116	STA 	DL 	V_0016	; [V_0016] <- A
002760:	2110	LDE 	P  	=#10	; E <- #10
002762:	F402	RD  	P  		; privileged control instruction (RD)
002764:	C403	BAN 	RP 	L_276A	; 3 words forward: conditional branch (BAN) to L_276A
002766:	F001	SLLS	P  	#01
002768:	C411	BAN 	RP 	L_278A	; 17 words forward: conditional branch (BAN) to L_278A
L_276A:				; branch target; 1 ref
00276A:	22DD	LDX 	P  	=#DD	; X <- #DD
00276C:	C702	BRU 	RP 	L_2770	; 2 words forward: jump to L_2770
L_276E:				; branch target; 4 refs
00276E:	2200	LDX 	P  	=#00	; X <- #00
L_2770:				; branch target; 3 refs
002770:	101C	DLD 	DL 	V_001C	; A,E <- [V_001C] (2 words)
002772:	5608	DST 	DG 	TWB_T0	; [TWB_T0] <- A,E (2 words)
002774:	2101	LDE 	P  	=#01	; E <- #01
002776:	20A0	LDA 	P  	=#A0	; A <- #A0
002778:	F403	WD  	P  		; privileged control instruction (WD)
00277A:	F905	LDR 	P  	=#05	; LDR operand: #05
00277C:	F703	CSV 	P  	=#03	; SUPERVISOR CALL #3 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry MON3  [-> MON3: privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_0798, returns to its CSV caller with RSV]
L_277E:				; branch target; 2 refs
00277E:	F401	DIT 	P  		; DE-ACTIVATE INTERRUPT: save outgoing context, dispatch the highest-priority waiting level (not a plain return)

; ===== INT6: INTERRUPT ROUTINE (ends with DIT/DITR: leaves through the interrupt scheduler) =====
; routine profile: 12comparisons, 17 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, supervisor calls: #3 #9, ends with DIT/DITR
INT6:				; entry point; 1 ref
002780:	CF14	BRU 	RM 	L_2758	; 20 words backward: jump to L_2758
L_2782:				; branch target; 5 refs
002782:	1806	SPA 	DL 	V_0006	; [V_0006] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
002784:	D702	BRU 	IL 	*V_0002	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0002 = #0000: address = G + #0000, G unknown)
L_2786:				; branch target; 1 ref
002786:	F400	CLM 	P  		; privileged control instruction (CLM)
002788:	CF05	BRU 	RM 	L_277E	; 5 words backward: jump to L_277E
L_278A:				; branch target; 1 ref
00278A:	F909	LDR 	P  	=#09	; LDR operand: #09
00278C:	29FF	AND 	P  	=#FF	; A <- A and #FF
00278E:	F11C	CNA 	P  	
002790:	1712	ADM 	DL 	V_0012	; add A to [V_0012]
002792:	0E0C	LBR 	DL 	PRTS_PTR	; A <- right byte of [PRTS_PTR]
002794:	CB13	BCF 	RM 	L_276E	; 19 words backward: conditional branch (BCF) to L_276E
002796:	0E08	LBR 	DL 	V_0008	; A <- right byte of [V_0008]
002798:	C559	BAZ 	RP 	L_284A	; 89 words forward: conditional branch (BAZ) to L_284A
00279A:	0E09	LBR 	DL 	V_0008	; A <- right byte of [V_0008]
00279C:	C505	BAZ 	RP 	L_27A6	; 5 words forward: conditional branch (BAZ) to L_27A6
00279E:	0118	LDE 	DL 	V_0018	; E <- [V_0018]
0027A0:	CB0F	BCF 	RM 	L_2782	; 15 words backward: conditional branch (BCF) to L_2782
0027A2:	150C	SBR 	DL 	PRTS_PTR	; right byte of [PRTS_PTR] <- A
0027A4:	CF11	BRU 	RM 	L_2782	; 17 words backward: jump to L_2782
L_27A6:				; branch target; 1 ref
0027A6:	2200	LDX 	P  	=#00	; X <- #00
0027A8:	0E0A	LBR 	DL 	CPT_PTR	; A <- right byte of [CPT_PTR]
0027AA:	C30D	BCF 	RP 	L_27C4	; 13 words forward: conditional branch (BCF) to L_27C4
L_27AC:				; branch target; 1 ref
0027AC:	AE2C	LBR 	ILX	*V_002C,X	; A <- right byte of [G+0986+X]  (pointer V_002C = #0986: address = G + #0986, G unknown)
0027AE:	C505	BAZ 	RP 	L_27B8	; 5 words forward: conditional branch (BAZ) to L_27B8
0027B0:	2B14	CMP 	P  	=#14	; compare A with #14
0027B2:	C308	BCF 	RP 	L_27C2	; 8 words forward: conditional branch (BCF) to L_27C2
0027B4:	F201	ICX 	P  	=#01	; ICX operand: #01
0027B6:	C706	BRU 	RP 	L_27C2	; 6 words forward: jump to L_27C2
L_27B8:				; branch target; 1 ref
0027B8:	F201	ICX 	P  	=#01	; ICX operand: #01
0027BA:	F905	LDR 	P  	=#05	; LDR operand: #05
0027BC:	0B12	CMP 	DL 	V_0012	; compare A with [V_0012]
0027BE:	C81E	BCT 	RM 	L_2782	; 30 words backward: conditional branch (BCT) to L_2782
0027C0:	CF0A	BRU 	RM 	L_27AC	; 10 words backward: jump to L_27AC
L_27C2:				; branch target; 2 refs
0027C2:	150A	SBR 	DL 	CPT_PTR	; right byte of [CPT_PTR] <- A
L_27C4:				; branch target; 2 refs
0027C4:	2000	LDA 	P  	=#00	; A <- #00
0027C6:	510A	STA 	DG 	TWB_T1	; [TWB_T1] <- A
0027C8:	F905	LDR 	P  	=#05	; LDR operand: #05
0027CA:	0B12	CMP 	DL 	V_0012	; compare A with [V_0012]
0027CC:	C011	BCT 	RP 	L_27EE	; 17 words forward: conditional branch (BCT) to L_27EE
L_27CE:				; branch target; 1 ref
0027CE:	AE2C	LBR 	ILX	*V_002C,X	; A <- right byte of [G+0986+X]  (pointer V_002C = #0986: address = G + #0986, G unknown)
0027D0:	0114	LDE 	DL 	V_0014	; E <- [V_0014]
0027D2:	C005	BCT 	RP 	L_27DC	; 5 words forward: conditional branch (BCT) to L_27DC
0027D4:	2B0A	CMP 	P  	=#0A	; compare A with #0A
0027D6:	C008	BCT 	RP 	L_27E6	; 8 words forward: conditional branch (BCT) to L_27E6
0027D8:	2BFF	CMP 	P  	=#FF	; compare A with #FF
0027DA:	C006	BCT 	RP 	L_27E6	; 6 words forward: conditional branch (BCT) to L_27E6
L_27DC:				; branch target; 1 ref
0027DC:	7522	SBR 	IL 	*V_0022	; right byte of [G+3006] <- A  (pointer V_0022 = #3006: address = G + #3006, G unknown)
0027DE:	2001	LDA 	P  	=#01	; A <- #01
0027E0:	1722	ADM 	DL 	V_0022	; add A to [V_0022]
0027E2:	2001	LDA 	P  	=#01	; A <- #01
0027E4:	570A	ADM 	DG 	TWB_T1	; add A to [TWB_T1]
L_27E6:				; branch target; 2 refs
0027E6:	F201	ICX 	P  	=#01	; ICX operand: #01
0027E8:	F905	LDR 	P  	=#05	; LDR operand: #05
0027EA:	0B12	CMP 	DL 	V_0012	; compare A with [V_0012]
0027EC:	CB0F	BCF 	RM 	L_27CE	; 15 words backward: conditional branch (BCF) to L_27CE
L_27EE:				; branch target; 1 ref
0027EE:	0018	LDA 	DL 	V_0018	; A <- [V_0018]
0027F0:	460A	SUB 	DG 	TWB_T1	; A <- A - [TWB_T1]
0027F2:	1118	STA 	DL 	V_0018	; [V_0018] <- A
0027F4:	0114	LDE 	DL 	V_0014	; E <- [V_0014]
0027F6:	C006	BCT 	RP 	L_2802	; 6 words forward: conditional branch (BCT) to L_2802
0027F8:	F90B	LDR 	P  	=#0B	; LDR operand: #0B
0027FA:	29FF	AND 	P  	=#FF	; A <- A and #FF
0027FC:	2B8D	CMP 	P  	=#8D	; compare A with #8D
0027FE:	C007	BCT 	RP 	L_280C	; 7 words forward: conditional branch (BCT) to L_280C
002800:	0018	LDA 	DL 	V_0018	; A <- [V_0018]
L_2802:				; branch target; 1 ref
002802:	C505	BAZ 	RP 	L_280C	; 5 words forward: conditional branch (BAZ) to L_280C
002804:	CF41	BRU 	RM 	L_2782	; 65 words backward: jump to L_2782
002806:	0E10	LBR 	DL 	V_0010	; A <- right byte of [V_0010]
002808:	2901	AND 	P  	=#01	; A <- A and #01
00280A:	CB4E	BCF 	RM 	L_276E	; 78 words backward: conditional branch (BCF) to L_276E
L_280C:				; branch target; 2 refs
00280C:	140D	SBL 	DL 	PRTS_PTR	; left byte of [PRTS_PTR] <- A
00280E:	2204	LDX 	P  	=#04	; X <- #04
L_2810:				; branch target; 1 ref
002810:	F302	DCX 	P  	=#02	; DCX operand: #02
002812:	C305	BCF 	RP 	L_281C	; 5 words forward: conditional branch (BCF) to L_281C
002814:	A020	LDA 	ILX	*V_0020,X	; A <- [G+0000+X]  (pointer V_0020 = #0000: address = G + #0000, G unknown)
002816:	AB24	CMP 	ILX	*V_0024,X	; compare A with [G+7CD6+X]  (pointer V_0024 = #7CD6: address = G + #7CD6, G unknown)
002818:	C804	BCT 	RM 	L_2810	; 4 words backward: conditional branch (BCT) to L_2810
00281A:	C702	BRU 	RP 	L_281E	; 2 words forward: jump to L_281E
L_281C:				; branch target; 1 ref
00281C:	150D	SBR 	DL 	PRTS_PTR	; right byte of [PRTS_PTR] <- A
L_281E:				; branch target; 1 ref
00281E:	0014	LDA 	DL 	V_0014	; A <- [V_0014]
002820:	C011	BCT 	RP 	L_2842	; 17 words forward: conditional branch (BCT) to L_2842
002822:	0022	LDA 	DL 	V_0022	; A <- [V_0022]
002824:	0620	SUB 	DL 	V_0020	; A <- A - [V_0020]
002826:	5110	STA 	DG 	TWB_T4	; [TWB_T4] <- A
002828:	FA05	STR 	P  	=#05	; STR operand: #05
00282A:	6420	LEA 	IL 	*V_0020	; A <- G+0000  (effective address, memory not read)  (pointer V_0020 = #0000: address = G + #0000, G unknown)
00282C:	F709	CSV 	P  	=#09	; SUPERVISOR CALL #9 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_ASEB  [-> M_ASEB: 1 loop, stores into the caller's TWB, returns to its CSV caller with RSV]
00282E:	4010	LDA 	DG 	TWB_T4	; A <- [TWB_T4]
002830:	2B02	CMP 	P  	=#02	; compare A with #02
002832:	C208	BOT 	RP 	L_2842	; 8 words forward: conditional branch (BOT) to L_2842
002834:	AE22	LBR 	ILX	*V_0022,X	; A <- right byte of [G+3006+X]  (pointer V_0022 = #3006: address = G + #3006, G unknown)
002836:	F301	DCX 	P  	=#01	; DCX operand: #01
002838:	AD22	LBL 	ILX	*V_0022,X	; A <- left byte of [G+3006+X]  (pointer V_0022 = #3006: address = G + #3006, G unknown)
L_283A:				; branch target; 1 ref
00283A:	0B2A	CMP 	DL 	V_002A	; compare A with [V_002A]
00283C:	C303	BCF 	RP 	L_2842	; 3 words forward: conditional branch (BCF) to L_2842
00283E:	2288	LDX 	P  	=#88	; X <- #88
002840:	CF68	BRU 	RM 	L_2770	; 104 words backward: jump to L_2770
L_2842:				; branch target; 3 refs
002842:	0E0D	LBR 	DL 	PRTS_PTR	; A <- right byte of [PRTS_PTR]
002844:	C86B	BCT 	RM 	L_276E	; 107 words backward: conditional branch (BCT) to L_276E
002846:	2282	LDX 	P  	=#82	; X <- #82
002848:	CF6C	BRU 	RM 	L_2770	; 108 words backward: jump to L_2770
L_284A:				; branch target; 1 ref
00284A:	2200	LDX 	P  	=#00	; X <- #00
00284C:	CE09	BOF 	RM 	L_283A	; 9 words backward: conditional branch (BOF) to L_283A
00284E:	CD45	BAZ 	RM 	L_27C4	; 69 words backward: conditional branch (BAZ) to L_27C4
002850:	0118	LDE 	DL 	V_0018	; E <- [V_0018]
002852:	CB68	BCF 	RM 	L_2782	; 104 words backward: conditional branch (BCF) to L_2782
002854:	CF73	BRU 	RM 	L_276E	; 115 words backward: jump to L_276E
002856:	161E	DST 	DL 	V_001E	; [V_001E] <- A,E (2 words)
002858:	1122	STA 	DL 	V_0022	; [V_0022] <- A
00285A:	131C	STX 	DL 	V_001C	; [V_001C] <- X
00285C:	2000	LDA 	P  	=#00	; A <- #00
00285E:	2100	LDE 	P  	=#00	; E <- #00
002860:	1608	DST 	DL 	V_0008	; [V_0008] <- A,E (2 words)
002862:	160C	DST 	DL 	PRTS_PTR	; [PRTS_PTR] <- A,E (2 words)
002864:	2202	LDX 	P  	=#02	; X <- #02
002866:	AE1C	LBR 	ILX	*V_001C,X	; A <- right byte of [G+2214+X]  (pointer V_001C = #2214: address = G + #2214, G unknown)
002868:	1510	SBR 	DL 	V_0010	; right byte of [V_0010] <- A
00286A:	F0C4	SRLS	P  	#04
00286C:	F104	XAX 	P  		; exchange A and X
00286E:	0E1E	LBR 	DL 	V_001E	; A <- right byte of [V_001E]
l_2870:				; branch target; 1 ref; only from untraced words
002870:	2910	AND 	P  	=#10	; A <- A and #10
002872:	C504	BAZ 	RP 	l_287A	; 4 words forward: conditional branch (BAZ) to l_287A
002874:	1508	SBR 	DL 	V_0008	; right byte of [V_0008] <- A
002876:	AE4C	LBR 	ILX	*V_004C,X	; A <- right byte of [G+0000+X]  (pointer V_004C = #0000: address = G + #0000, G unknown)
002878:	C702	BRU 	RP 	l_287C	; 2 words forward: jump to l_287C
l_287A:				; branch target; 1 ref; only from untraced words
00287A:	AE4E	LBR 	ILX	*V_004E,X	; A <- right byte of [G+0000+X]  (pointer V_004E = #0000: address = G + #0000, G unknown)
l_287C:				; branch target; 1 ref; only from untraced words
00287C:	2B01	CMP 	P  	=#01	; compare A with #01
00287E:	C009	BCT 	RP 	l_2890	; 9 words forward: conditional branch (BCT) to l_2890
002880:	F108	XAA 	P  	
002882:	C509	BAZ 	RP 	l_2894	; 9 words forward: conditional branch (BAZ) to l_2894
002884:	C404	BAN 	RP 	l_288C	; 4 words forward: conditional branch (BAN) to l_288C
002886:	2E02	LBR 	P  	=#02	; A <- right byte of #02
002888:	F108	XAA 	P  	
00288A:	C71E	BRU 	RP 	l_28C6	; 30 words forward: jump to l_28C6
l_288C:				; branch target; 1 ref; only from untraced words
00288C:	2EBB	LBR 	P  	=#BB	; A <- right byte of #BB
00288E:	C71C	BRU 	RP 	l_28C6	; 28 words forward: jump to l_28C6
l_2890:				; branch target; 1 ref; only from untraced words
002890:	2EA2	LBR 	P  	=#A2	; A <- right byte of #A2
002892:	C71A	BRU 	RP 	l_28C6	; 26 words forward: jump to l_28C6
l_2894:				; branch target; 1 ref; only from untraced words
002894:	001E	LDA 	DL 	V_001E	; A <- [V_001E]
002896:	2901	AND 	P  	=#01	; A <- A and #01
002898:	1114	STA 	DL 	V_0014	; [V_0014] <- A
00289A:	002C	LDA 	DL 	V_002C	; A <- [V_002C]
00289C:	112E	STA 	DL 	V_002E	; [V_002E] <- A
00289E:	400E	LDA 	DG 	TWB_T3	; A <- [TWB_T3]
0028A0:	C303	BCF 	RP 	l_28A6	; 3 words forward: conditional branch (BCF) to l_28A6
0028A2:	2EBD	LBR 	P  	=#BD	; A <- right byte of #BD
0028A4:	C711	BRU 	RP 	l_28C6	; 17 words forward: jump to l_28C6
l_28A6:				; branch target; 1 ref; only from untraced words
0028A6:	1118	STA 	DL 	V_0018	; [V_0018] <- A
0028A8:	0E10	LBR 	DL 	V_0010	; A <- right byte of [V_0010]
0028AA:	29F0	AND 	P  	=#F0	; A <- A and #F0
0028AC:	2B80	CMP 	P  	=#80	; compare A with #80
0028AE:	C20D	BOT 	RP 	l_28C8	; 13 words forward: conditional branch (BOT) to l_28C8
0028B0:	C009	BCT 	RP 	l_28C2	; 9 words forward: conditional branch (BCT) to l_28C2
0028B2:	2B90	CMP 	P  	=#90	; compare A with #90
0028B4:	C306	BCF 	RP 	l_28C0	; 6 words forward: conditional branch (BCF) to l_28C0
0028B6:	0E08	LBR 	DL 	V_0008	; A <- right byte of [V_0008]
0028B8:	C305	BCF 	RP 	l_28C2	; 5 words forward: conditional branch (BCF) to l_28C2
0028BA:	2001	LDA 	P  	=#01	; A <- #01
0028BC:	150B	SBR 	DL 	CPT_PTR	; right byte of [CPT_PTR] <- A
0028BE:	C702	BRU 	RP 	l_28C2	; 2 words forward: jump to l_28C2
l_28C0:				; branch target; 1 ref; only from untraced words
0028C0:	150E	SBR 	DL 	PRTS_COUNT	; right byte of [PRTS_COUNT] <- A
l_28C2:				; branch target; 3 refs; only from untraced words
0028C2:	1509	SBR 	DL 	V_0008	; right byte of [V_0008] <- A
0028C4:	C70B	BRU 	RP 	l_28DA	; 11 words forward: jump to l_28DA
l_28C6:				; branch target; 4 refs; only from untraced words
0028C6:	D706	BRU 	IL 	*V_0006	; RETURN from subroutine: branch through return cell V_0006 (written by the SPA of the call)
l_28C8:				; branch target; 1 ref; only from untraced words
0028C8:	0E08	LBR 	DL 	V_0008	; A <- right byte of [V_0008]
0028CA:	C508	BAZ 	RP 	l_28DA	; 8 words forward: conditional branch (BAZ) to l_28DA
0028CC:	0E10	LBR 	DL 	V_0010	; A <- right byte of [V_0010]
0028CE:	290F	AND 	P  	=#0F	; A <- A and #0F
0028D0:	2B01	CMP 	P  	=#01	; compare A with #01
0028D2:	C304	BCF 	RP 	l_28DA	; 4 words forward: conditional branch (BCF) to l_28DA
0028D4:	150A	SBR 	DL 	CPT_PTR	; right byte of [CPT_PTR] <- A
0028D6:	2000	LDA 	P  	=#00	; A <- #00
0028D8:	1114	STA 	DL 	V_0014	; [V_0014] <- A
l_28DA:				; branch target; 3 refs; only from untraced words
0028DA:	0E0C	LBR 	DL 	PRTS_PTR	; A <- right byte of [PRTS_PTR]
0028DC:	C508	BAZ 	RP 	l_28EC	; 8 words forward: conditional branch (BAZ) to l_28EC
0028DE:	1408	SBL 	DL 	V_0008	; left byte of [V_0008] <- A
0028E0:	2E14	LBR 	P  	=#14	; A <- right byte of #14
0028E2:	1536	SBR 	DL 	V_0036	; right byte of [V_0036] <- A
0028E4:	2001	LDA 	P  	=#01	; A <- #01
0028E6:	FA09	STR 	P  	=#09	; STR operand: #09
0028E8:	002C	LDA 	DL 	V_002C	; A <- [V_002C]
0028EA:	C765	BRU 	RP 	l_29B4	; 101 words forward: jump to l_29B4
l_28EC:				; branch target; 1 ref; only from untraced words
0028EC:	0E09	LBR 	DL 	V_0008	; A <- right byte of [V_0008]
0028EE:	C31A	BCF 	RP 	l_2922	; 26 words forward: conditional branch (BCF) to l_2922
0028F0:	0018	LDA 	DL 	V_0018	; A <- [V_0018]
0028F2:	2B16	CMP 	P  	=#16	; compare A with #16
0028F4:	C202	BOT 	RP 	l_28F8	; 2 words forward: conditional branch (BOT) to l_28F8
0028F6:	2016	LDA 	P  	=#16	; A <- #16
l_28F8:				; branch target; 1 ref; only from untraced words
0028F8:	1112	STA 	DL 	V_0012	; [V_0012] <- A
0028FA:	0114	LDE 	DL 	V_0014	; E <- [V_0014]
0028FC:	C002	BCT 	RP 	l_2900	; 2 words forward: conditional branch (BCT) to l_2900
0028FE:	2D80	LBL 	P  	=#80	; A <- left byte of #80
l_2900:				; branch target; 1 ref; only from untraced words
002900:	0712	IOR 	DL 	V_0012	; A <- A or [V_0012]
002902:	FA09	STR 	P  	=#09	; STR operand: #09
002904:	2D8D	LBL 	P  	=#8D	; A <- left byte of #8D
002906:	FA0B	STR 	P  	=#0B	; STR operand: #0B
002908:	002C	LDA 	DL 	V_002C	; A <- [V_002C]
00290A:	2601	SUB 	P  	=#01	; A <- A - #01
00290C:	FA0A	STR 	P  	=#0A	; STR operand: #0A
00290E:	20A1	LDA 	P  	=#A1	; A <- #A1
002910:	0108	LDE 	DL 	V_0008	; E <- [V_0008]
002912:	C002	BCT 	RP 	l_2916	; 2 words forward: conditional branch (BCT) to l_2916
002914:	2502	ADD 	P  	=#02	; A <- A + #02
l_2916:				; branch target; 2 refs; only from untraced words
002916:	2101	LDE 	P  	=#01	; E <- #01
002918:	F408	STM 	P  		; privileged control instruction (STM)
00291A:	1216	STE 	DL 	V_0016	; [V_0016] <- E
00291C:	F403	WD  	P  		; privileged control instruction (WD)
00291E:	2000	LDA 	P  	=#00	; A <- #00
002920:	D706	BRU 	IL 	*V_0006	; RETURN from subroutine: branch through return cell V_0006 (written by the SPA of the call)
l_2922:				; branch target; 1 ref; only from untraced words
002922:	2100	LDE 	P  	=#00	; E <- #00
002924:	022C	LDX 	DL 	V_002C	; X <- [V_002C]
002926:	0E08	LBR 	DL 	V_0008	; A <- right byte of [V_0008]
002928:	C31D	BCF 	RP 	l_2962	; 29 words forward: conditional branch (BCF) to l_2962
00292A:	0E0F	LBR 	DL 	PRTS_COUNT	; A <- right byte of [PRTS_COUNT]
00292C:	C31B	BCF 	RP 	l_2962	; 27 words forward: conditional branch (BCF) to l_2962
00292E:	0E0B	LBR 	DL 	CPT_PTR	; A <- right byte of [CPT_PTR]
002930:	C50E	BAZ 	RP 	l_294C	; 14 words forward: conditional branch (BAZ) to l_294C
002932:	6E22	LBR 	IL 	*V_0022	; A <- right byte of [G+3006]  (pointer V_0022 = #3006: address = G + #3006, G unknown)
002934:	5516	SBR 	DG 	TWB_T7	; right byte of [TWB_T7] <- A
002936:	2001	LDA 	P  	=#01	; A <- #01
002938:	1722	ADM 	DL 	V_0022	; add A to [V_0022]
00293A:	0018	LDA 	DL 	V_0018	; A <- [V_0018]
00293C:	2601	SUB 	P  	=#01	; A <- A - #01
00293E:	1118	STA 	DL 	V_0018	; [V_0018] <- A
002940:	4E16	LBR 	DG 	TWB_T7	; A <- right byte of [TWB_T7]
002942:	C50E	BAZ 	RP 	l_295E	; 14 words forward: conditional branch (BAZ) to l_295E
002944:	2BE0	CMP 	P  	=#E0	; compare A with #E0
002946:	C008	BCT 	RP 	l_2956	; 8 words forward: conditional branch (BCT) to l_2956
002948:	2B60	CMP 	P  	=#60	; compare A with #60
00294A:	C006	BCT 	RP 	l_2956	; 6 words forward: conditional branch (BCT) to l_2956
l_294C:				; branch target; 1 ref; only from untraced words
00294C:	2102	LDE 	P  	=#02	; E <- #02
00294E:	F302	DCX 	P  	=#02	; DCX operand: #02
002950:	0030	LDA 	DL 	V_0030	; A <- [V_0030]
002952:	1134	STA 	DL 	V_0034	; [V_0034] <- A
002954:	C705	BRU 	RP 	l_295E	; 5 words forward: jump to l_295E
l_2956:				; branch target; 2 refs; only from untraced words
002956:	2102	LDE 	P  	=#02	; E <- #02
002958:	F302	DCX 	P  	=#02	; DCX operand: #02
00295A:	0032	LDA 	DL 	V_0032	; A <- [V_0032]
00295C:	1134	STA 	DL 	V_0034	; [V_0034] <- A
l_295E:				; branch target; 2 refs; only from untraced words
00295E:	2E01	LBR 	P  	=#01	; A <- right byte of #01
002960:	150F	SBR 	DL 	PRTS_COUNT	; right byte of [PRTS_COUNT] <- A
l_2962:				; branch target; 2 refs; only from untraced words
002962:	1212	STE 	DL 	V_0012	; [V_0012] <- E
002964:	132E	STX 	DL 	V_002E	; [V_002E] <- X
002966:	2116	LDE 	P  	=#16	; E <- #16
002968:	0018	LDA 	DL 	V_0018	; A <- [V_0018]
00296A:	2B16	CMP 	P  	=#16	; compare A with #16
00296C:	C602	BOF 	RP 	l_2970	; 2 words forward: conditional branch (BOF) to l_2970
00296E:	0118	LDE 	DL 	V_0018	; E <- [V_0018]
l_2970:				; branch target; 1 ref; only from untraced words
002970:	121A	STE 	DL 	V_001A	; [V_001A] <- E
002972:	0E0E	LBR 	DL 	PRTS_COUNT	; A <- right byte of [PRTS_COUNT]
002974:	C506	BAZ 	RP 	l_2980	; 6 words forward: conditional branch (BAZ) to l_2980
002976:	F106	XEX 	P  		; exchange E and X
l_2978:				; branch target; 1 ref; only from untraced words
002978:	F301	DCX 	P  	=#01	; DCX operand: #01
00297A:	C309	BCF 	RP 	l_298C	; 9 words forward: conditional branch (BCF) to l_298C
00297C:	B42C	SBL 	ILX	*V_002C,X	; left byte of [G+0986+X] <- A  (pointer V_002C = #0986: address = G + #0986, G unknown)
00297E:	CF03	BRU 	RM 	l_2978	; 3 words backward: jump to l_2978
l_2980:				; branch target; 1 ref; only from untraced words
002980:	F106	XEX 	P  		; exchange E and X
l_2982:				; branch target; 1 ref; only from untraced words
002982:	F301	DCX 	P  	=#01	; DCX operand: #01
002984:	C304	BCF 	RP 	l_298C	; 4 words forward: conditional branch (BCF) to l_298C
002986:	AE22	LBR 	ILX	*V_0022,X	; A <- right byte of [G+3006+X]  (pointer V_0022 = #3006: address = G + #3006, G unknown)
002988:	B52C	SBR 	ILX	*V_002C,X	; right byte of [G+0986+X] <- A  (pointer V_002C = #0986: address = G + #0986, G unknown)
00298A:	CF04	BRU 	RM 	l_2982	; 4 words backward: jump to l_2982
l_298C:				; branch target; 2 refs; only from untraced words
00298C:	001A	LDA 	DL 	V_001A	; A <- [V_001A]
00298E:	1722	ADM 	DL 	V_0022	; add A to [V_0022]
002990:	001A	LDA 	DL 	V_001A	; A <- [V_001A]
l_2992:				; branch target; 1 ref; only from untraced words
002992:	1712	ADM 	DL 	V_0012	; add A to [V_0012]
002994:	FA09	STR 	P  	=#09	; STR operand: #09
002996:	0018	LDA 	DL 	V_0018	; A <- [V_0018]
002998:	061A	SUB 	DL 	V_001A	; A <- A - [V_001A]
00299A:	1118	STA 	DL 	V_0018	; [V_0018] <- A
00299C:	0014	LDA 	DL 	V_0014	; A <- [V_0014]
00299E:	C00A	BCT 	RP 	l_29B2	; 10 words forward: conditional branch (BCT) to l_29B2
0029A0:	F901	LDR 	P  	=#01	; LDR operand: #01
0029A2:	5116	STA 	DG 	TWB_T7	; [TWB_T7] <- A
0029A4:	0436	LEA 	DL 	V_0036	; A <- V_0036  (effective address, memory not read)
0029A6:	021A	LDX 	DL 	V_001A	; X <- [V_001A]
0029A8:	3604	DCL 	DL 	V_0004	; DCL operand: [V_0004]
0029AA:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0029AC:	D72C	BRU 	IL 	*V_002C	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_002C = #0986: address = G + #0986, G unknown)
l_29AE:				; branch target; 1 ref; only from untraced words
0029AE:	4016	LDA 	DG 	TWB_T7	; A <- [TWB_T7]
0029B0:	FA01	STR 	P  	=#01	; STR operand: #01
l_29B2:				; branch target; 1 ref; only from untraced words
0029B2:	002E	LDA 	DL 	V_002E	; A <- [V_002E]
l_29B4:				; branch target; 1 ref; only from untraced words
0029B4:	2601	SUB 	P  	=#01	; A <- A - #01
0029B6:	FA0A	STR 	P  	=#0A	; STR operand: #0A
0029B8:	20A2	LDA 	P  	=#A2	; A <- #A2
0029BA:	0F08	LBX 	DL 	V_0008	; LBX operand: [V_0008]
0029BC:	C002	BCT 	RP 	l_29C0	; 2 words forward: conditional branch (BCT) to l_29C0
0029BE:	2508	ADD 	P  	=#08	; A <- A + #08
l_29C0:				; branch target; 1 ref; only from untraced words
0029C0:	CF55	BRU 	RM 	l_2916	; 85 words backward: jump to l_2916; system structure
; 
; ==============================================
CTX6:				; system structure
; ============================================

0029C2:	0016	.WORD	#0016	; CTX level 6: saved status
0029C4:	0001	.WORD	#0001	; CTX level 6: saved X
0029C6:	001E	.WORD	#001E	; CTX level 6: saved E
0029C8:	006E	.WORD	#006E	; CTX level 6: saved A
0029CA:	26C8	.WORD	#26C8	; CTX level 6: saved G
0029CC:	26E8	.WORD	#26E8	; CTX level 6: saved L
0029CE:	2780	.WORD	#2780	; CTX level 6: saved P (entry of the task / interrupt routine of this level)
0029D0:	0200	LDX 	DL 	Unknow_1	; X <- [Unknow_1]
0029D2:	0020	LDA 	DL 	V_0020	; A <- [V_0020]
0029D4:	0118	LDE 	DL 	V_0018	; E <- [V_0018]
0029D6:	0020	LDA 	DL 	V_0020	; A <- [V_0020]
0029D8:	005A	LDA 	DL 	V_005A	; A <- [V_005A]
0029DA:	0000	.ZERO	3	; 3 zero words (padding, reserved area or unused data; never reached as code)
0029E0:	52FC	STE 	DG 	G+00FC	; [G+00FC] <- E
0029E2:	0000	.ZERO	12	; 12 zero words (padding, reserved area or unused data; never reached as code)
0029FA:	2AF2	CPS 	P  	=#F2	; CPS operand: #F2
0029FC:	2B3A	CMP 	P  	=#3A	; compare A with #3A
0029FE:	29FA	AND 	P  	=#FA	; A <- A and #FA
002A00:	0000	.ZERO	10	; 10 zero words (padding, reserved area or unused data; never reached as code)
002A14:	.EBCDIC	"%EODvE"	; 3 words, raw: 6CC5 D6C4 A5C5
002A1A:	CF44	BRU 	RM 	l_2992	; 68 words backward: jump to l_2992
002A1C:	2A14	CPS 	P  	=#14	; CPS operand: #14
002A1E:	2A16	CPS 	P  	=#16	; CPS operand: #16
002A20:	2A22	CPS 	P  	=#22	; CPS operand: #22
002A22:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
002A24:	FF01	??	FF01
002A26:	0101	LDE 	DL 	Unknow_1	; E <- [Unknow_1]
002A28:	0101	LDE 	DL 	Unknow_1	; E <- [Unknow_1]
002A2A:	FFFF	??	FFFF
002A2C:	FFFF	??	FFFF
002A2E:	FF02	??	FF02
002A30:	FF02	??	FF02
002A32:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
l_2A34:				; branch target; 1 ref; only from untraced words
002A34:	0016	LDA 	DL 	V_0016	; A <- [V_0016]
002A36:	C551	BAZ 	RP 	l_2AD8	; 81 words forward: conditional branch (BAZ) to l_2AD8
002A38:	2000	LDA 	P  	=#00	; A <- #00
002A3A:	1116	STA 	DL 	V_0016	; [V_0016] <- A
002A3C:	2108	LDE 	P  	=#08	; E <- #08
002A3E:	F402	RD  	P  		; privileged control instruction (RD)
002A40:	1513	SBR 	DL 	V_0012	; right byte of [V_0012] <- A
002A42:	F001	SLLS	P  	#01
002A44:	C303	BCF 	RP 	l_2A4A	; 3 words forward: conditional branch (BCF) to l_2A4A
002A46:	C405	BAN 	RP 	l_2A50	; 5 words forward: conditional branch (BAN) to l_2A50
002A48:	C706	BRU 	RP 	l_2A54	; 6 words forward: jump to l_2A54
l_2A4A:				; branch target; 1 ref; only from untraced words
002A4A:	F001	SLLS	P  	#01
002A4C:	C306	BCF 	RP 	l_2A58	; 6 words forward: conditional branch (BCF) to l_2A58
002A4E:	C745	BRU 	RP 	l_2AD8	; 69 words forward: jump to l_2AD8
l_2A50:				; branch target; 1 ref; only from untraced words
002A50:	229D	LDX 	P  	=#9D	; X <- #9D
002A52:	C73E	BRU 	RP 	l_2ACE	; 62 words forward: jump to l_2ACE
l_2A54:				; branch target; 1 ref; only from untraced words
002A54:	22FB	LDX 	P  	=#FB	; X <- #FB
002A56:	C73C	BRU 	RP 	l_2ACE	; 60 words forward: jump to l_2ACE
l_2A58:				; branch target; 1 ref; only from untraced words
002A58:	2200	LDX 	P  	=#00	; X <- #00
002A5A:	0E15	LBR 	DL 	V_0014	; A <- right byte of [V_0014]
002A5C:	C309	BCF 	RP 	l_2A6E	; 9 words forward: conditional branch (BCF) to l_2A6E
002A5E:	0E18	LBR 	DL 	V_0018	; A <- right byte of [V_0018]
002A60:	C307	BCF 	RP 	l_2A6E	; 7 words forward: conditional branch (BCF) to l_2A6E
002A62:	0E13	LBR 	DL 	V_0012	; A <- right byte of [V_0012]
002A64:	2B14	CMP 	P  	=#14	; compare A with #14
002A66:	C304	BCF 	RP 	l_2A6E	; 4 words forward: conditional branch (BCF) to l_2A6E
002A68:	2E01	LBR 	P  	=#01	; A <- right byte of #01
002A6A:	1518	SBR 	DL 	V_0018	; right byte of [V_0018] <- A
002A6C:	C73D	BRU 	RP 	l_2AE6	; 61 words forward: jump to l_2AE6
l_2A6E:				; branch target; 3 refs; only from untraced words
002A6E:	2E01	LBR 	P  	=#01	; A <- right byte of #01
002A70:	1518	SBR 	DL 	V_0018	; right byte of [V_0018] <- A
002A72:	090A	AND 	DL 	CPT_PTR	; A <- A and [CPT_PTR]
002A74:	FA05	STR 	P  	=#05	; STR operand: #05
002A76:	C50B	BAZ 	RP 	l_2A8C	; 11 words forward: conditional branch (BAZ) to l_2A8C
002A78:	0413	LEA 	DL 	V_0012	; A <- V_0012  (effective address, memory not read)
002A7A:	F709	CSV 	P  	=#09	; SUPERVISOR CALL #9 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry M_ASEB  [-> M_ASEB: 1 loop, stores into the caller's TWB, returns to its CSV caller with RSV]
002A7C:	0E15	LBR 	DL 	V_0014	; A <- right byte of [V_0014]
002A7E:	C306	BCF 	RP 	l_2A8A	; 6 words forward: conditional branch (BCF) to l_2A8A
002A80:	0E13	LBR 	DL 	V_0012	; A <- right byte of [V_0012]
002A82:	2B15	CMP 	P  	=#15	; compare A with #15
002A84:	C031	BCT 	RP 	l_2AE6	; 49 words forward: conditional branch (BCT) to l_2AE6
002A86:	2B07	CMP 	P  	=#07	; compare A with #07
002A88:	C02F	BCT 	RP 	l_2AE6	; 47 words forward: conditional branch (BCT) to l_2AE6
l_2A8A:				; branch target; 1 ref; only from untraced words
002A8A:	2201	LDX 	P  	=#01	; X <- #01
l_2A8C:				; branch target; 1 ref; only from untraced words
002A8C:	2001	LDA 	P  	=#01	; A <- #01
002A8E:	170E	ADM 	DL 	PRTS_COUNT	; add A to [PRTS_COUNT]
002A90:	0E13	LBR 	DL 	V_0012	; A <- right byte of [V_0012]
002A92:	750E	SBR 	IL 	*PRTS_COUNT	; right byte of [G+0038] <- A  (pointer PRTS_COUNT = #0038: address = G + #0038, G unknown)
002A94:	C101	BRX 	RP 	$+X	; INDEXED JUMP (BRX): X selects one of the 1 BRU entries that follow (jump / dispatch table)
l_2A96:				; branch target; 1 ref; only from untraced words
002A96:	C723	BRU 	RP 	l_2ADC	; 35 words forward: jump to l_2ADC
002A98:	2B0D	CMP 	P  	=#0D	; compare A with #0D
002A9A:	C322	BCF 	RP 	l_2ADE	; 34 words forward: conditional branch (BCF) to l_2ADE
002A9C:	F106	XEX 	P  		; exchange E and X
002A9E:	020E	LDX 	DL 	PRTS_COUNT	; X <- [PRTS_COUNT]
002AA0:	F301	DCX 	P  	=#01	; DCX operand: #01
002AA2:	AE38	LBR 	ILX	*V_0038,X	; A <- right byte of [G+0004+X]  (pointer V_0038 = #0004: address = G + #0004, G unknown)
002AA4:	F106	XEX 	P  		; exchange E and X
002AA6:	2B6D	CMP 	P  	=#6D	; compare A with #6D
002AA8:	C303	BCF 	RP 	l_2AAE	; 3 words forward: conditional branch (BCF) to l_2AAE
002AAA:	2288	LDX 	P  	=#88	; X <- #88
002AAC:	C711	BRU 	RP 	l_2ACE	; 17 words forward: jump to l_2ACE
l_2AAE:				; branch target; 2 refs; only from untraced words
002AAE:	0E15	LBR 	DL 	V_0014	; A <- right byte of [V_0014]
002AB0:	C30E	BCF 	RP 	l_2ACC	; 14 words forward: conditional branch (BCF) to l_2ACC
002AB2:	F201	ICX 	P  	=#01	; ICX operand: #01
002AB4:	F106	XEX 	P  		; exchange E and X
002AB6:	2204	LDX 	P  	=#04	; X <- #04
l_2AB8:				; branch target; 1 ref; only from untraced words
002AB8:	F301	DCX 	P  	=#01	; DCX operand: #01
002ABA:	C31A	BCF 	RP 	l_2AEE	; 26 words forward: conditional branch (BCF) to l_2AEE
002ABC:	AE10	LBR 	ILX	*V_0010,X	; A <- right byte of [G+0000+X]  (pointer V_0010 = #0000: address = G + #0000, G unknown)
002ABE:	F301	DCX 	P  	=#01	; DCX operand: #01
002AC0:	AD10	LBL 	ILX	*V_0010,X	; A <- left byte of [G+0000+X]  (pointer V_0010 = #0000: address = G + #0000, G unknown)
002AC2:	F106	XEX 	P  		; exchange E and X
002AC4:	A322	EOR 	ILX	*V_0022,X	; A <- A xor [G+3006+X]  (pointer V_0022 = #3006: address = G + #3006, G unknown)
002AC6:	F302	DCX 	P  	=#02	; DCX operand: #02
002AC8:	F106	XEX 	P  		; exchange E and X
002ACA:	CD09	BAZ 	RM 	l_2AB8	; 9 words backward: conditional branch (BAZ) to l_2AB8
l_2ACC:				; branch target; 1 ref; only from untraced words
002ACC:	2200	LDX 	P  	=#00	; X <- #00
l_2ACE:				; branch target; 4 refs; only from untraced words
002ACE:	000A	LDA 	DL 	CPT_PTR	; A <- [CPT_PTR]
002AD0:	0108	LDE 	DL 	V_0008	; E <- [V_0008]
002AD2:	5608	DST 	DG 	TWB_T0	; [TWB_T0] <- A,E (2 words)
002AD4:	F104	XAX 	P  		; exchange A and X
002AD6:	F703	CSV 	P  	=#03	; SUPERVISOR CALL #3 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry MON3  [-> MON3: privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_0798, returns to its CSV caller with RSV]
l_2AD8:				; branch target; 3 refs; only from untraced words
002AD8:	F401	DIT 	P  		; DE-ACTIVATE INTERRUPT: save outgoing context, dispatch the highest-priority waiting level (not a plain return)
002ADA:	CF53	BRU 	RM 	l_2A34	; 83 words backward: jump to l_2A34
l_2ADC:				; branch target; 1 ref; only from untraced words
002ADC:	2205	LDX 	P  	=#05	; X <- #05
l_2ADE:				; branch target; 1 ref; only from untraced words
002ADE:	000C	LDA 	DL 	PRTS_PTR	; A <- [PRTS_PTR]
002AE0:	2601	SUB 	P  	=#01	; A <- A - #01
002AE2:	110C	STA 	DL 	PRTS_PTR	; [PRTS_PTR] <- A
002AE4:	CD1B	BAZ 	RM 	l_2AAE	; 27 words backward: conditional branch (BAZ) to l_2AAE
l_2AE6:				; branch target; 3 refs; only from untraced words
002AE6:	1806	SPA 	DL 	V_0006	; [V_0006] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
002AE8:	D702	BRU 	IL 	*V_0002	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0002 = #0000: address = G + #0000, G unknown)
l_2AEA:				; branch target; 1 ref; only from untraced words
002AEA:	F400	CLM 	P  		; privileged control instruction (CLM)
002AEC:	CF0A	BRU 	RM 	l_2AD8	; 10 words backward: jump to l_2AD8
l_2AEE:				; branch target; 1 ref; only from untraced words
002AEE:	2282	LDX 	P  	=#82	; X <- #82
002AF0:	CF11	BRU 	RM 	l_2ACE	; 17 words backward: jump to l_2ACE
002AF2:	120A	STE 	DL 	CPT_PTR	; [CPT_PTR] <- E
002AF4:	1110	STA 	DL 	V_0010	; [V_0010] <- A
002AF6:	1308	STX 	DL 	V_0008	; [V_0008] <- X
002AF8:	2202	LDX 	P  	=#02	; X <- #02
002AFA:	AE08	LBR 	ILX	*V_0008,X	; A <- right byte of [G+0000+X]  (pointer V_0008 = #0000: address = G + #0000, G unknown)
002AFC:	1514	SBR 	DL 	V_0014	; right byte of [V_0014] <- A
002AFE:	F0C4	SRLS	P  	#04
002B00:	F104	XAX 	P  		; exchange A and X
002B02:	AE26	LBR 	ILX	*V_0026,X	; A <- right byte of [G+7FFF+X]  (pointer V_0026 = #7FFF: address = G + #7FFF, G unknown)
002B04:	2B01	CMP 	P  	=#01	; compare A with #01
002B06:	C009	BCT 	RP 	l_2B18	; 9 words forward: conditional branch (BCT) to l_2B18
002B08:	F108	XAA 	P  	
002B0A:	C509	BAZ 	RP 	l_2B1C	; 9 words forward: conditional branch (BAZ) to l_2B1C
002B0C:	C404	BAN 	RP 	l_2B14	; 4 words forward: conditional branch (BAN) to l_2B14
002B0E:	2E02	LBR 	P  	=#02	; A <- right byte of #02
002B10:	F108	XAA 	P  	
002B12:	C720	BRU 	RP 	l_2B52	; 32 words forward: jump to l_2B52
l_2B14:				; branch target; 1 ref; only from untraced words
002B14:	2EBB	LBR 	P  	=#BB	; A <- right byte of #BB
002B16:	C71E	BRU 	RP 	l_2B52	; 30 words forward: jump to l_2B52
l_2B18:				; branch target; 1 ref; only from untraced words
002B18:	2EA2	LBR 	P  	=#A2	; A <- right byte of #A2
002B1A:	C71C	BRU 	RP 	l_2B52	; 28 words forward: jump to l_2B52
l_2B1C:				; branch target; 1 ref; only from untraced words
002B1C:	2E00	LBR 	P  	=#00	; A <- right byte of #00
002B1E:	1518	SBR 	DL 	V_0018	; right byte of [V_0018] <- A
002B20:	0010	LDA 	DL 	V_0010	; A <- [V_0010]
002B22:	2601	SUB 	P  	=#01	; A <- A - #01
002B24:	410E	LDE 	DG 	TWB_T3	; E <- [TWB_T3]
002B26:	C017	BCT 	RP 	l_2B54	; 23 words forward: conditional branch (BCT) to l_2B54
002B28:	160C	DST 	DL 	PRTS_PTR	; [PRTS_PTR] <- A,E (2 words)
002B2A:	0E14	LBR 	DL 	V_0014	; A <- right byte of [V_0014]
002B2C:	290F	AND 	P  	=#0F	; A <- A and #0F
002B2E:	2B01	CMP 	P  	=#01	; compare A with #01
002B30:	C003	BCT 	RP 	l_2B36	; 3 words forward: conditional branch (BCT) to l_2B36
002B32:	2000	LDA 	P  	=#00	; A <- #00
002B34:	C702	BRU 	RP 	l_2B38	; 2 words forward: jump to l_2B38
l_2B36:				; branch target; 1 ref; only from untraced words
002B36:	2001	LDA 	P  	=#01	; A <- #01
l_2B38:				; branch target; 1 ref; only from untraced words
002B38:	1515	SBR 	DL 	V_0014	; right byte of [V_0014] <- A
002B3A:	0E15	LBR 	DL 	V_0014	; A <- right byte of [V_0014]
002B3C:	C305	BCF 	RP 	l_2B46	; 5 words forward: conditional branch (BCF) to l_2B46
002B3E:	0E18	LBR 	DL 	V_0018	; A <- right byte of [V_0018]
002B40:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
002B42:	2001	LDA 	P  	=#01	; A <- #01
002B44:	C702	BRU 	RP 	l_2B48	; 2 words forward: jump to l_2B48
l_2B46:				; branch target; 1 ref; only from untraced words
002B46:	2003	LDA 	P  	=#03	; A <- #03
l_2B48:				; branch target; 1 ref; only from untraced words
002B48:	2108	LDE 	P  	=#08	; E <- #08
002B4A:	F408	STM 	P  		; privileged control instruction (STM)
002B4C:	1216	STE 	DL 	V_0016	; [V_0016] <- E
002B4E:	F403	WD  	P  		; privileged control instruction (WD)
002B50:	2000	LDA 	P  	=#00	; A <- #00
l_2B52:				; branch target; 4 refs; only from untraced words
002B52:	D706	BRU 	IL 	*V_0006	; RETURN from subroutine: branch through return cell V_0006 (written by the SPA of the call)
l_2B54:				; branch target; 1 ref; only from untraced words
002B54:	2EBD	LBR 	P  	=#BD	; A <- right byte of #BD
002B56:	CF02	BRU 	RM 	l_2B52	; 2 words backward: jump to l_2B52
002B58:	0014	LDA 	DL 	V_0014	; A <- [V_0014]
002B5A:	0000	.ZERO	3	; 3 zero words (padding, reserved area or unused data; never reached as code)
002B60:	29DA	AND 	P  	=#DA	; A <- A and #DA
002B62:	29FA	AND 	P  	=#FA	; A <- A and #FA
002B64:	2A34	CPS 	P  	=#34	; CPS operand: #34
002B66:	0200	LDX 	DL 	Unknow_1	; X <- [Unknow_1]; system structure
; 
; ==============================================
CTX14:				; system structure
; ============================================

002B68:	0020	.WORD	#0020	; CTX level 14: saved status
002B6A:	008C	.WORD	#008C	; CTX level 14: saved X
002B6C:	0020	.WORD	#0020	; CTX level 14: saved E
002B6E:	004A	.WORD	#004A	; CTX level 14: saved A
002B70:	0000	.WORD	#0000	; CTX level 14: saved G
002B72:	0000	.WORD	#0000	; CTX level 14: saved L
002B74:	0000	.WORD	#0000	; CTX level 14: saved P (entry of the task / interrupt routine of this level)
002B76:	5166	STA 	DG 	G+0066	; [G+0066] <- A
002B78:	0000	.ZERO	12	; 12 zero words (padding, reserved area or unused data; never reached as code)
002B90:	2BFC	CMP 	P  	=#FC	; compare A with #FC
002B92:	2C30	MUL 	P  	=#30	; multiply by #30
002B94:	2B90	CMP 	P  	=#90	; compare A with #90
002B96:	0000	.ZERO	9	; 9 zero words (padding, reserved area or unused data; never reached as code)
002BA8:	2BAA	CMP 	P  	=#AA	; compare A with #AA
002BAA:	FFFF	??	FFFF
002BAC:	FF01	??	FF01
002BAE:	0101	LDE 	DL 	Unknow_1	; E <- [Unknow_1]
002BB0:	0101	LDE 	DL 	Unknow_1	; E <- [Unknow_1]
002BB2:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
002BB4:	0100	LDE 	DL 	Unknow_1	; E <- [Unknow_1]
002BB6:	FF02	??	FF02
002BB8:	FF02	??	FF02

; ===== INT13: INTERRUPT ROUTINE (ends with DIT/DITR: leaves through the interrupt scheduler) =====
; routine profile: privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, supervisor calls: #3, ends with DIT/DITR
INT13:				; entry point; branch target; 2 refs
002BBA:	0016	LDA 	DL 	V_0016	; A <- [V_0016]
002BBC:	C51E	BAZ 	RP 	L_2BF8	; 30 words forward: conditional branch (BAZ) to L_2BF8
002BBE:	2000	LDA 	P  	=#00	; A <- #00
002BC0:	1116	STA 	DL 	V_0016	; [V_0016] <- A
002BC2:	2118	LDE 	P  	=#18	; E <- #18
002BC4:	F402	RD  	P  		; privileged control instruction (RD)
002BC6:	F001	SLLS	P  	#01
002BC8:	C303	BCF 	RP 	L_2BCE	; 3 words forward: conditional branch (BCF) to L_2BCE
002BCA:	C407	BAN 	RP 	L_2BD8	; 7 words forward: conditional branch (BAN) to L_2BD8
002BCC:	C704	BRU 	RP 	L_2BD4	; 4 words forward: jump to L_2BD4
L_2BCE:				; branch target; 1 ref
002BCE:	F001	SLLS	P  	#01
002BD0:	C306	BCF 	RP 	L_2BDC	; 6 words forward: conditional branch (BCF) to L_2BDC
002BD2:	C713	BRU 	RP 	L_2BF8	; 19 words forward: jump to L_2BF8
L_2BD4:				; branch target; 1 ref
002BD4:	22FB	LDX 	P  	=#FB	; X <- #FB
002BD6:	C70C	BRU 	RP 	L_2BEE	; 12 words forward: jump to L_2BEE
L_2BD8:				; branch target; 1 ref
002BD8:	229D	LDX 	P  	=#9D	; X <- #9D
002BDA:	C70A	BRU 	RP 	L_2BEE	; 10 words forward: jump to L_2BEE
L_2BDC:				; branch target; 1 ref
002BDC:	000E	LDA 	DL 	PRTS_COUNT	; A <- [PRTS_COUNT]
002BDE:	2601	SUB 	P  	=#01	; A <- A - #01
002BE0:	110E	STA 	DL 	PRTS_COUNT	; [PRTS_COUNT] <- A
002BE2:	C505	BAZ 	RP 	L_2BEC	; 5 words forward: conditional branch (BAZ) to L_2BEC
002BE4:	1806	SPA 	DL 	V_0006	; [V_0006] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
002BE6:	D702	BRU 	IL 	*V_0002	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0002 = #0000: address = G + #0000, G unknown)
L_2BE8:				; branch target; 1 ref
002BE8:	F400	CLM 	P  		; privileged control instruction (CLM)
002BEA:	C707	BRU 	RP 	L_2BF8	; 7 words forward: jump to L_2BF8
L_2BEC:				; branch target; 1 ref
002BEC:	2200	LDX 	P  	=#00	; X <- #00
L_2BEE:				; branch target; 2 refs
002BEE:	000A	LDA 	DL 	CPT_PTR	; A <- [CPT_PTR]
002BF0:	0108	LDE 	DL 	V_0008	; E <- [V_0008]
002BF2:	5608	DST 	DG 	TWB_T0	; [TWB_T0] <- A,E (2 words)
002BF4:	F104	XAX 	P  		; exchange A and X
002BF6:	F703	CSV 	P  	=#03	; SUPERVISOR CALL #3 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry MON3  [-> MON3: privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_0798, returns to its CSV caller with RSV]
L_2BF8:				; branch target; 3 refs
002BF8:	F401	DIT 	P  		; DE-ACTIVATE INTERRUPT: save outgoing context, dispatch the highest-priority waiting level (not a plain return)
002BFA:	CF20	BRU 	RM 	INT13	; 32 words backward: jump to INT13  [-> INT13: privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, supervisor calls: #3, ends with DIT/DITR]
002BFC:	120A	STE 	DL 	CPT_PTR	; [CPT_PTR] <- E
002BFE:	1308	STX 	DL 	V_0008	; [V_0008] <- X
002C00:	1110	STA 	DL 	V_0010	; [V_0010] <- A
002C02:	2202	LDX 	P  	=#02	; X <- #02
002C04:	AE08	LBR 	ILX	*V_0008,X	; A <- right byte of [G+0000+X]  (pointer V_0008 = #0000: address = G + #0000, G unknown)
002C06:	1512	SBR 	DL 	V_0012	; right byte of [V_0012] <- A
002C08:	F0C4	SRLS	P  	#04
002C0A:	F104	XAX 	P  		; exchange A and X
002C0C:	AE18	LBR 	ILX	*V_0018,X	; A <- right byte of [G+2294+X]  (pointer V_0018 = #2294: address = G + #2294, G unknown)
002C0E:	2B01	CMP 	P  	=#01	; compare A with #01
002C10:	C009	BCT 	RP 	l_2C22	; 9 words forward: conditional branch (BCT) to l_2C22
002C12:	F108	XAA 	P  	
002C14:	C509	BAZ 	RP 	l_2C26	; 9 words forward: conditional branch (BAZ) to l_2C26
002C16:	C404	BAN 	RP 	l_2C1E	; 4 words forward: conditional branch (BAN) to l_2C1E
002C18:	2E02	LBR 	P  	=#02	; A <- right byte of #02
002C1A:	F108	XAA 	P  	
002C1C:	C726	BRU 	RP 	l_2C68	; 38 words forward: jump to l_2C68
l_2C1E:				; branch target; 1 ref; only from untraced words
002C1E:	2EBB	LBR 	P  	=#BB	; A <- right byte of #BB
002C20:	C724	BRU 	RP 	l_2C68	; 36 words forward: jump to l_2C68
l_2C22:				; branch target; 1 ref; only from untraced words
002C22:	2EA2	LBR 	P  	=#A2	; A <- right byte of #A2
002C24:	C722	BRU 	RP 	l_2C68	; 34 words forward: jump to l_2C68
l_2C26:				; branch target; 1 ref; only from untraced words
002C26:	0010	LDA 	DL 	V_0010	; A <- [V_0010]
002C28:	2601	SUB 	P  	=#01	; A <- A - #01
002C2A:	410E	LDE 	DG 	TWB_T3	; E <- [TWB_T3]
002C2C:	C01F	BCT 	RP 	l_2C6A	; 31 words forward: conditional branch (BCT) to l_2C6A
002C2E:	160E	DST 	DL 	PRTS_COUNT	; [PRTS_COUNT] <- A,E (2 words)
002C30:	0E12	LBR 	DL 	V_0012	; A <- right byte of [V_0012]
002C32:	2BB0	CMP 	P  	=#B0	; compare A with #B0
002C34:	C203	BOT 	RP 	l_2C3A	; 3 words forward: conditional branch (BOT) to l_2C3A
002C36:	140D	SBL 	DL 	PRTS_PTR	; left byte of [PRTS_PTR] <- A
002C38:	C711	BRU 	RP 	l_2C5A	; 17 words forward: jump to l_2C5A
l_2C3A:				; branch target; 1 ref; only from untraced words
002C3A:	F901	LDR 	P  	=#01	; LDR operand: #01
002C3C:	5116	STA 	DG 	TWB_T7	; [TWB_T7] <- A
002C3E:	2001	LDA 	P  	=#01	; A <- #01
002C40:	1710	ADM 	DL 	V_0010	; add A to [V_0010]
002C42:	6F10	LBX 	IL 	*V_0010	; LBX operand: [G+0000]  (pointer V_0010 = #0000: address = G + #0000, G unknown)
002C44:	130C	STX 	DL 	PRTS_PTR	; [PRTS_PTR] <- X
l_2C46:				; branch target; 1 ref; only from untraced words
002C46:	2E01	LBR 	P  	=#01	; A <- right byte of #01
002C48:	090A	AND 	DL 	CPT_PTR	; A <- A and [CPT_PTR]
002C4A:	C508	BAZ 	RP 	l_2C5A	; 8 words forward: conditional branch (BAZ) to l_2C5A
002C4C:	2201	LDX 	P  	=#01	; X <- #01
002C4E:	040D	LEA 	DL 	PRTS_PTR	; A <- PRTS_PTR  (effective address, memory not read)
002C50:	3604	DCL 	DL 	V_0004	; DCL operand: [V_0004]
002C52:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
002C54:	D72C	BRU 	IL 	*V_002C	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_002C = #0986: address = G + #0986, G unknown)
l_2C56:				; branch target; 1 ref; only from untraced words
002C56:	4016	LDA 	DG 	TWB_T7	; A <- [TWB_T7]
002C58:	FA01	STR 	P  	=#01	; STR operand: #01
l_2C5A:				; branch target; 2 refs; only from untraced words
002C5A:	0E0D	LBR 	DL 	PRTS_PTR	; A <- right byte of [PRTS_PTR]
002C5C:	2D80	LBL 	P  	=#80	; A <- left byte of #80
002C5E:	2118	LDE 	P  	=#18	; E <- #18
002C60:	F408	STM 	P  		; privileged control instruction (STM)
002C62:	1216	STE 	DL 	V_0016	; [V_0016] <- E
002C64:	F403	WD  	P  		; privileged control instruction (WD)
002C66:	2000	LDA 	P  	=#00	; A <- #00
l_2C68:				; branch target; 5 refs; only from untraced words
002C68:	D706	BRU 	IL 	*V_0006	; RETURN from subroutine: branch through return cell V_0006 (written by the SPA of the call)
l_2C6A:				; branch target; 1 ref; only from untraced words
002C6A:	2EBD	LBR 	P  	=#BD	; A <- right byte of #BD
002C6C:	CF02	BRU 	RM 	l_2C68	; 2 words backward: jump to l_2C68; system structure
; 
; ==============================================
CTX13:				; system structure
; ============================================

002C6E:	0014	.WORD	#0014	; CTX level 13: saved status
002C70:	0000	.WORD	#0000	; CTX level 13: saved X
002C72:	0000	.WORD	#0000	; CTX level 13: saved E
002C74:	0000	.WORD	#0000	; CTX level 13: saved A
002C76:	2B70	.WORD	#2B70	; CTX level 13: saved G
002C78:	2B90	.WORD	#2B90	; CTX level 13: saved L
002C7A:	2BBA	.WORD	#2BBA	; CTX level 13: saved P (entry of the task / interrupt routine of this level)
002C7C:	0200	LDX 	DL 	Unknow_1	; X <- [Unknow_1]
002C7E:	0020	LDA 	DL 	V_0020	; A <- [V_0020]
002C80:	013E	LDE 	DL 	V_003E	; E <- [V_003E]
002C82:	0020	LDA 	DL 	V_0020	; A <- [V_0020]
002C84:	0070	LDA 	DL 	v_0070	; A <- [v_0070]
002C86:	00D2	LDA 	DL 	V_00D2	; A <- [V_00D2]
002C88:	0020	LDA 	DL 	V_0020	; A <- [V_0020]
002C8A:	0014	LDA 	DL 	V_0014	; A <- [V_0014]
002C8C:	5050	DLD 	DG 	G+0050	; A,E <- [G+0050] (2 words)
002C8E:	22EE	LDX 	P  	=#EE	; X <- #EE
002C90:	2800	DIV 	P  	=#00	; divide by #00
002C92:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
002C94:	8505	ADD 	IGX	*TWB_IND+1,X	; A <- A + [*TWB_IND+1+X]  (pointer TWB_IND+1 lives in the calling task's area: its contents are only known at run time)
002C96:	07B2	IOR 	DL 	V_00B2	; A <- A or [V_00B2]
002C98:	0004	LDA 	DL 	V_0004	; A <- [V_0004]
002C9A:	0000	.ZERO	2	; 2 zero words (padding, reserved area or unused data; never reached as code)
002C9E:	0E00	LBR 	DL 	Unknow_1	; A <- right byte of [Unknow_1]
002CA0:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
002CA2:	0796	IOR 	DL 	V_0096	; A <- A or [V_0096]
002CA4:	07E8	IOR 	DL 	V_00E8	; A <- A or [V_00E8]
002CA6:	2DC4	LBL 	P  	=#C4	; A <- left byte of #C4
002CA8:	2EE6	LBR 	P  	=#E6	; A <- right byte of #E6
002CAA:	2CA6	MUL 	P  	=#A6	; multiply by #A6
002CAC:	0684	SUB 	DL 	v_0084	; A <- A - [v_0084]
002CAE:	043C	LEA 	DL 	V_003C	; A <- V_003C  (effective address, memory not read)
002CB0:	0000	.ZERO	2	; 2 zero words (padding, reserved area or unused data; never reached as code)
002CB4:	22EE	LDX 	P  	=#EE	; X <- #EE
002CB6:	81D0	LDE 	IGX	*G+00D0,X	; E <- [*G+00D0+X]  (pointer G+00D0 lives in the calling task's area: its contents are only known at run time)
002CB8:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
002CBA:	7D24	FDV 	IL 	*V_0024	; floating divide by [G+7CD6]  (pointer V_0024 = #7CD6: address = G + #7CD6, G unknown)
002CBC:	0010	LDA 	DL 	V_0010	; A <- [V_0010]
002CBE:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
002CC0:	2800	DIV 	P  	=#00	; divide by #00
002CC2:	7D26	FDV 	IL 	*V_0026	; floating divide by [G+7FFF]  (pointer V_0026 = #7FFF: address = G + #7FFF, G unknown)
002CC4:	0B46	CMP 	DL 	V_0046	; compare A with [V_0046]
002CC6:	C840	BCT 	RM 	l_2C46	; 64 words backward: conditional branch (BCT) to l_2C46
002CC8:	0080	LDA 	DL 	V_0080	; A <- [V_0080]
002CCA:	0001	LDA 	DL 	Unknow_1	; A <- [Unknow_1]
002CCC:	0002	LDA 	DL 	V_0002	; A <- [V_0002]
002CCE:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
002CD0:	8141	LDE 	IGX	*G+0041,X	; E <- [*G+0041+X]  (pointer G+0041 lives in the calling task's area: its contents are only known at run time)
002CD2:	01C1	LDE 	DL 	v_00C0	; E <- [v_00C0]
002CD4:	2CD0	MUL 	P  	=#D0	; multiply by #D0
002CD6:	4B00	CMP 	DG 	TWB_P	; compare A with [TWB_P]
002CD8:	0786	IOR 	DL 	v_0086	; A <- A or [v_0086]
002CDA:	0FFF	LBX 	DL 	v_00FE	; LBX operand: [v_00FE]
002CDC:	2CDE	MUL 	P  	=#DE	; multiply by #DE
002CDE:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
002CE0:	0102	LDE 	DL 	V_0002	; E <- [V_0002]
002CE2:	0202	LDX 	DL 	V_0002	; X <- [V_0002]
002CE4:	0202	LDX 	DL 	V_0002	; X <- [V_0002]
002CE6:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
002CE8:	0002	LDA 	DL 	V_0002	; A <- [V_0002]
002CEA:	FF02	??	FF02
002CEC:	FF02	??	FF02
002CEE:	A5C5	ADD 	ILX	*v_00C4,X	; A <- A + [G+0BB8+X]  (pointer v_00C4 = #0BB8: address = G + #0BB8, G unknown)
002CF0:	CF44	BRU 	RM 	l_2C68	; 68 words backward: jump to l_2C68
002CF2:	.EBCDIC	"%EOD"	; 2 words, raw: 6CC5 D6C4
L_2CF6:				; branch target; 1 ref
002CF6:	0018	LDA 	DL 	V_0018	; A <- [V_0018]
002CF8:	C531	BAZ 	RP 	L_2D5A	; 49 words forward: conditional branch (BAZ) to L_2D5A
002CFA:	2000	LDA 	P  	=#00	; A <- #00
002CFC:	1118	STA 	DL 	V_0018	; [V_0018] <- A
002CFE:	2115	LDE 	P  	=#15	; E <- #15
002D00:	F402	RD  	P  		; privileged control instruction (RD)
002D02:	1120	STA 	DL 	V_0020	; [V_0020] <- A
002D04:	0E10	LBR 	DL 	V_0010	; A <- right byte of [V_0010]
002D06:	29DF	AND 	P  	=#DF	; A <- A and #DF
002D08:	1510	SBR 	DL 	V_0010	; right byte of [V_0010] <- A
002D0A:	F108	XAA 	P  	
002D0C:	2590	ADD 	P  	=#90	; A <- A + #90
002D0E:	2105	LDE 	P  	=#05	; E <- #05
002D10:	F403	WD  	P  		; privileged control instruction (WD)
002D12:	0020	LDA 	DL 	V_0020	; A <- [V_0020]
002D14:	F004	SLLS	P  	#04
002D16:	C02E	BCT 	RP 	L_2D72	; 46 words forward: conditional branch (BCT) to L_2D72
002D18:	0E11	LBR 	DL 	V_0010	; A <- right byte of [V_0010]
002D1A:	2BB0	CMP 	P  	=#B0	; compare A with #B0
002D1C:	C021	BCT 	RP 	L_2D5E	; 33 words forward: conditional branch (BCT) to L_2D5E
L_2D1E:				; branch target; 1 ref
002D1E:	0016	LDA 	DL 	V_0016	; A <- [V_0016]
002D20:	2910	AND 	P  	=#10	; A <- A and #10
002D22:	C504	BAZ 	RP 	L_2D2A	; 4 words forward: conditional branch (BAZ) to L_2D2A
002D24:	220A	LDX 	P  	=#0A	; X <- #0A
002D26:	0024	LDA 	DL 	V_0024	; A <- [V_0024]
002D28:	B70E	ADM 	ILX	*PRTS_COUNT,X	; add A to [G+0038+X]  (pointer PRTS_COUNT = #0038: address = G + #0038, G unknown)
L_2D2A:				; branch target; 1 ref
002D2A:	0016	LDA 	DL 	V_0016	; A <- [V_0016]
002D2C:	2B80	CMP 	P  	=#80	; compare A with #80
002D2E:	C610	BOF 	RP 	L_2D4E	; 16 words forward: conditional branch (BOF) to L_2D4E
002D30:	701C	DLD 	IL 	*V_001C	; A,E <- [G+2214] (2 words)  (pointer V_001C = #2214: address = G + #2214, G unknown)
002D32:	F102	XAE 	P  		; exchange A and E
002D34:	0B48	CMP 	DL 	V_0048	; compare A with [V_0048]
002D36:	C305	BCF 	RP 	L_2D40	; 5 words forward: conditional branch (BCF) to L_2D40
002D38:	F102	XAE 	P  		; exchange A and E
002D3A:	0B4A	CMP 	DL 	V_004A	; compare A with [V_004A]
002D3C:	C007	BCT 	RP 	L_2D4A	; 7 words forward: conditional branch (BCT) to L_2D4A
002D3E:	F102	XAE 	P  		; exchange A and E
L_2D40:				; branch target; 1 ref
002D40:	0B4C	CMP 	DL 	V_004C	; compare A with [V_004C]
002D42:	C306	BCF 	RP 	L_2D4E	; 6 words forward: conditional branch (BCF) to L_2D4E
002D44:	F102	XAE 	P  		; exchange A and E
002D46:	0B4E	CMP 	DL 	V_004E	; compare A with [V_004E]
002D48:	C303	BCF 	RP 	L_2D4E	; 3 words forward: conditional branch (BCF) to L_2D4E
L_2D4A:				; branch target; 1 ref
002D4A:	2282	LDX 	P  	=#82	; X <- #82
002D4C:	C702	BRU 	RP 	L_2D50	; 2 words forward: jump to L_2D50
L_2D4E:				; branch target; 3 refs
002D4E:	2200	LDX 	P  	=#00	; X <- #00
L_2D50:				; branch target; 6 refs
002D50:	001A	LDA 	DL 	V_001A	; A <- [V_001A]
002D52:	010E	LDE 	DL 	PRTS_COUNT	; E <- [PRTS_COUNT]
002D54:	5608	DST 	DG 	TWB_T0	; [TWB_T0] <- A,E (2 words)
002D56:	F104	XAX 	P  		; exchange A and X
002D58:	F703	CSV 	P  	=#03	; SUPERVISOR CALL #3 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry MON3  [-> MON3: privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_0798, returns to its CSV caller with RSV]
L_2D5A:				; branch target; 2 refs
002D5A:	F401	DIT 	P  		; DE-ACTIVATE INTERRUPT: save outgoing context, dispatch the highest-priority waiting level (not a plain return)

; ===== INT26: INTERRUPT ROUTINE (ends with DIT/DITR: leaves through the interrupt scheduler) =====
; routine profile: 10comparisons, 10 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, supervisor calls: #3, ends with DIT/DITR
INT26:				; entry point; 1 ref
002D5C:	CF33	BRU 	RM 	L_2CF6	; 51 words backward: jump to L_2CF6
L_2D5E:				; branch target; 1 ref
002D5E:	0016	LDA 	DL 	V_0016	; A <- [V_0016]
002D60:	290F	AND 	P  	=#0F	; A <- A and #0F
002D62:	2B03	CMP 	P  	=#03	; compare A with #03
002D64:	C823	BCT 	RM 	L_2D1E	; 35 words backward: conditional branch (BCT) to L_2D1E
002D66:	20F0	LDA 	P  	=#F0	; A <- #F0
L_2D68:				; branch target; 1 ref
002D68:	1511	SBR 	DL 	V_0010	; right byte of [V_0010] <- A
L_2D6A:				; branch target; 1 ref
002D6A:	1806	SPA 	DL 	V_0006	; [V_0006] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
002D6C:	D702	BRU 	IL 	*V_0002	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0002 = #0000: address = G + #0000, G unknown)
L_2D6E:				; branch target; 1 ref
002D6E:	F400	CLM 	P  		; privileged control instruction (CLM)
002D70:	CF0B	BRU 	RM 	L_2D5A	; 11 words backward: jump to L_2D5A
L_2D72:				; branch target; 1 ref
002D72:	2980	AND 	P  	=#80	; A <- A and #80
002D74:	C317	BCF 	RP 	L_2DA2	; 23 words forward: conditional branch (BCF) to L_2DA2
002D76:	0E21	LBR 	DL 	V_0020	; A <- right byte of [V_0020]
002D78:	C519	BAZ 	RP 	L_2DAA	; 25 words forward: conditional branch (BAZ) to L_2DAA
002D7A:	2B80	CMP 	P  	=#80	; compare A with #80
002D7C:	C60A	BOF 	RP 	L_2D90	; 10 words forward: conditional branch (BOF) to L_2D90
002D7E:	2B40	CMP 	P  	=#40	; compare A with #40
002D80:	C313	BCF 	RP 	L_2DA6	; 19 words forward: conditional branch (BCF) to L_2DA6
002D82:	0E20	LBR 	DL 	V_0020	; A <- right byte of [V_0020]
002D84:	2B1F	CMP 	P  	=#1F	; compare A with #1F
002D86:	C614	BOF 	RP 	L_2DAE	; 20 words forward: conditional branch (BOF) to L_2DAE
002D88:	2902	AND 	P  	=#02	; A <- A and #02
002D8A:	C016	BCT 	RP 	L_2DB6	; 22 words forward: conditional branch (BCT) to L_2DB6
002D8C:	22BF	LDX 	P  	=#BF	; X <- #BF
002D8E:	CF1F	BRU 	RM 	L_2D50	; 31 words backward: jump to L_2D50
L_2D90:				; branch target; 1 ref
002D90:	2E20	LBR 	P  	=#20	; A <- right byte of #20
002D92:	0328	EOR 	DL 	V_0028	; A <- A xor [V_0028]
002D94:	C303	BCF 	RP 	L_2D9A	; 3 words forward: conditional branch (BCF) to L_2D9A
002D96:	1429	SBL 	DL 	V_0028	; left byte of [V_0028] <- A
002D98:	C707	BRU 	RP 	L_2DA6	; 7 words forward: jump to L_2DA6
L_2D9A:				; branch target; 1 ref
002D9A:	1728	ADM 	DL 	V_0028	; add A to [V_0028]
002D9C:	F108	XAA 	P  	
002D9E:	1710	ADM 	DL 	V_0010	; add A to [V_0010]
002DA0:	CF1B	BRU 	RM 	L_2D6A	; 27 words backward: jump to L_2D6A
L_2DA2:				; branch target; 1 ref
002DA2:	22F5	LDX 	P  	=#F5	; X <- #F5
002DA4:	CF2A	BRU 	RM 	L_2D50	; 42 words backward: jump to L_2D50
L_2DA6:				; branch target; 2 refs
002DA6:	22F1	LDX 	P  	=#F1	; X <- #F1
002DA8:	CF2C	BRU 	RM 	L_2D50	; 44 words backward: jump to L_2D50
L_2DAA:				; branch target; 1 ref
002DAA:	22FB	LDX 	P  	=#FB	; X <- #FB
002DAC:	CF2E	BRU 	RM 	L_2D50	; 46 words backward: jump to L_2D50
L_2DAE:				; branch target; 1 ref
002DAE:	0026	LDA 	DL 	V_0026	; A <- [V_0026]
002DB0:	C508	BAZ 	RP 	L_2DC0	; 8 words forward: conditional branch (BAZ) to L_2DC0
002DB2:	2601	SUB 	P  	=#01	; A <- A - #01
002DB4:	1126	STA 	DL 	V_0026	; [V_0026] <- A
L_2DB6:				; branch target; 1 ref
002DB6:	0E11	LBR 	DL 	V_0010	; A <- right byte of [V_0010]
002DB8:	2BF0	CMP 	P  	=#F0	; compare A with #F0
002DBA:	C302	BCF 	RP 	L_2DBE	; 2 words forward: conditional branch (BCF) to L_2DBE
002DBC:	20B0	LDA 	P  	=#B0	; A <- #B0
L_2DBE:				; branch target; 1 ref
002DBE:	CF2B	BRU 	RM 	L_2D68	; 43 words backward: jump to L_2D68
L_2DC0:				; branch target; 1 ref
002DC0:	22DD	LDX 	P  	=#DD	; X <- #DD
002DC2:	CF39	BRU 	RM 	L_2D50	; 57 words backward: jump to L_2D50
002DC4:	161A	DST 	DL 	V_001A	; [V_001A] <- A,E (2 words)
002DC6:	130E	STX 	DL 	PRTS_COUNT	; [PRTS_COUNT] <- X
002DC8:	2202	LDX 	P  	=#02	; X <- #02
002DCA:	1326	STX 	DL 	V_0026	; [V_0026] <- X
002DCC:	AE0E	LBR 	ILX	*PRTS_COUNT,X	; A <- right byte of [G+0038+X]  (pointer PRTS_COUNT = #0038: address = G + #0038, G unknown)
002DCE:	1116	STA 	DL 	V_0016	; [V_0016] <- A
002DD0:	F0C4	SRLS	P  	#04
002DD2:	F104	XAX 	P  		; exchange A and X
002DD4:	AE36	LBR 	ILX	*V_0036,X	; A <- right byte of [G+0003+X]  (pointer V_0036 = #0003: address = G + #0003, G unknown)
002DD6:	2B01	CMP 	P  	=#01	; compare A with #01
002DD8:	C009	BCT 	RP 	l_2DEA	; 9 words forward: conditional branch (BCT) to l_2DEA
002DDA:	F108	XAA 	P  	
002DDC:	C509	BAZ 	RP 	l_2DEE	; 9 words forward: conditional branch (BAZ) to l_2DEE
002DDE:	C404	BAN 	RP 	l_2DE6	; 4 words forward: conditional branch (BAN) to l_2DE6
002DE0:	2E02	LBR 	P  	=#02	; A <- right byte of #02
002DE2:	F108	XAA 	P  	
002DE4:	C792	BRU 	RP 	l_2F08	; 146 words forward: jump to l_2F08
l_2DE6:				; branch target; 1 ref; only from untraced words
002DE6:	2EBB	LBR 	P  	=#BB	; A <- right byte of #BB
002DE8:	C790	BRU 	RP 	l_2F08	; 144 words forward: jump to l_2F08
l_2DEA:				; branch target; 1 ref; only from untraced words
002DEA:	2EA2	LBR 	P  	=#A2	; A <- right byte of #A2
002DEC:	C78E	BRU 	RP 	l_2F08	; 142 words forward: jump to l_2F08
l_2DEE:				; branch target; 1 ref; only from untraced words
002DEE:	0016	LDA 	DL 	V_0016	; A <- [V_0016]
002DF0:	2B80	CMP 	P  	=#80	; compare A with #80
002DF2:	C603	BOF 	RP 	l_2DF8	; 3 words forward: conditional branch (BOF) to l_2DF8
002DF4:	20D0	LDA 	P  	=#D0	; A <- #D0
002DF6:	C702	BRU 	RP 	l_2DFA	; 2 words forward: jump to l_2DFA
l_2DF8:				; branch target; 1 ref; only from untraced words
002DF8:	20B0	LDA 	P  	=#B0	; A <- #B0
l_2DFA:				; branch target; 1 ref; only from untraced words
002DFA:	1511	SBR 	DL 	V_0010	; right byte of [V_0010] <- A
002DFC:	001C	LDA 	DL 	V_001C	; A <- [V_001C]
002DFE:	F0C1	SRLS	P  	#01
002E00:	C303	BCF 	RP 	l_2E06	; 3 words forward: conditional branch (BCF) to l_2E06
002E02:	2EB7	LBR 	P  	=#B7	; A <- right byte of #B7
002E04:	C782	BRU 	RP 	l_2F08	; 130 words forward: jump to l_2F08
l_2E06:				; branch target; 1 ref; only from untraced words
002E06:	F001	SLLS	P  	#01
002E08:	2602	SUB 	P  	=#02	; A <- A - #02
002E0A:	1114	STA 	DL 	V_0014	; [V_0014] <- A
002E0C:	400E	LDA 	DG 	TWB_T3	; A <- [TWB_T3]
002E0E:	C503	BAZ 	RP 	l_2E14	; 3 words forward: conditional branch (BAZ) to l_2E14
002E10:	F0C1	SRLS	P  	#01
002E12:	C303	BCF 	RP 	l_2E18	; 3 words forward: conditional branch (BCF) to l_2E18
l_2E14:				; branch target; 1 ref; only from untraced words
002E14:	2EBD	LBR 	P  	=#BD	; A <- right byte of #BD
002E16:	C779	BRU 	RP 	l_2F08	; 121 words forward: jump to l_2F08
l_2E18:				; branch target; 1 ref; only from untraced words
002E18:	1122	STA 	DL 	V_0022	; [V_0022] <- A
002E1A:	2201	LDX 	P  	=#01	; X <- #01
002E1C:	AE0E	LBR 	ILX	*PRTS_COUNT,X	; A <- right byte of [G+0038+X]  (pointer PRTS_COUNT = #0038: address = G + #0038, G unknown)
002E1E:	2920	AND 	P  	=#20	; A <- A and #20
002E20:	C302	BCF 	RP 	l_2E24	; 2 words forward: conditional branch (BCF) to l_2E24
002E22:	C778	BRU 	RP 	l_2F12	; 120 words forward: jump to l_2F12
l_2E24:				; branch target; 1 ref; only from untraced words
002E24:	F901	LDR 	P  	=#01	; LDR operand: #01
002E26:	5116	STA 	DG 	TWB_T7	; [TWB_T7] <- A
002E28:	3604	DCL 	DL 	V_0004	; DCL operand: [V_0004]
002E2A:	6162	LDE 	IL 	*l_0062	; E <- [G+0222]  (pointer l_0062 = #0222: address = G + #0222, G unknown)
002E2C:	4016	LDA 	DG 	TWB_T7	; A <- [TWB_T7]
002E2E:	FA01	STR 	P  	=#01	; STR operand: #01
002E30:	1230	STE 	DL 	V_0030	; [V_0030] <- E
002E32:	F209	ICX 	P  	=#09	; ICX operand: #09
002E34:	A00E	LDA 	ILX	*PRTS_COUNT,X	; A <- [G+0038+X]  (pointer PRTS_COUNT = #0038: address = G + #0038, G unknown)
002E36:	1108	STA 	DL 	V_0008	; [V_0008] <- A
002E38:	4E0E	LBR 	DG 	TWB_T3	; A <- right byte of [TWB_T3]
002E3A:	4F0F	LBX 	DG 	TWB_T3+1	; LBX operand: [TWB_T3+1]
002E3C:	C002	BCT 	RP 	l_2E40	; 2 words forward: conditional branch (BCT) to l_2E40
002E3E:	2501	ADD 	P  	=#01	; A <- A + #01
l_2E40:				; branch target; 1 ref; only from untraced words
002E40:	1124	STA 	DL 	V_0024	; [V_0024] <- A
002E42:	0508	ADD 	DL 	V_0008	; A <- A + [V_0008]
002E44:	C067	BCT 	RP 	l_2F12	; 103 words forward: conditional branch (BCT) to l_2F12
002E46:	510C	STA 	DG 	TWB_T2	; [TWB_T2] <- A
002E48:	C406	BAN 	RP 	l_2E54	; 6 words forward: conditional branch (BAN) to l_2E54
002E4A:	0030	LDA 	DL 	V_0030	; A <- [V_0030]
002E4C:	C408	BAN 	RP 	l_2E5C	; 8 words forward: conditional branch (BAN) to l_2E5C
002E4E:	4B0C	CMP 	DG 	TWB_T2	; compare A with [TWB_T2]
002E50:	C606	BOF 	RP 	l_2E5C	; 6 words forward: conditional branch (BOF) to l_2E5C
002E52:	C760	BRU 	RP 	l_2F12	; 96 words forward: jump to l_2F12
l_2E54:				; branch target; 1 ref; only from untraced words
002E54:	0030	LDA 	DL 	V_0030	; A <- [V_0030]
002E56:	C65E	BOF 	RP 	l_2F12	; 94 words forward: conditional branch (BOF) to l_2F12
002E58:	4B0C	CMP 	DG 	TWB_T2	; compare A with [TWB_T2]
002E5A:	C25C	BOT 	RP 	l_2F12	; 92 words forward: conditional branch (BOT) to l_2F12
l_2E5C:				; branch target; 2 refs; only from untraced words
002E5C:	0032	LDA 	DL 	V_0032	; A <- [V_0032]
002E5E:	091A	AND 	DL 	V_001A	; A <- A and [V_001A]
002E60:	F001	SLLS	P  	#01
002E62:	5411	SBL 	DG 	TWB_T4+1	; left byte of [TWB_T4+1] <- A
002E64:	290C	AND 	P  	=#0C	; A <- A and #0C
002E66:	F0C2	SRLS	P  	#02
002E68:	5510	SBR 	DG 	TWB_T4	; right byte of [TWB_T4] <- A
002E6A:	2203	LDX 	P  	=#03	; X <- #03
002E6C:	AE0E	LBR 	ILX	*PRTS_COUNT,X	; A <- right byte of [G+0038+X]  (pointer PRTS_COUNT = #0038: address = G + #0038, G unknown)
002E6E:	FA04	STR 	P  	=#04	; STR operand: #04
002E70:	0016	LDA 	DL 	V_0016	; A <- [V_0016]
002E72:	290F	AND 	P  	=#0F	; A <- A and #0F
002E74:	2B02	CMP 	P  	=#02	; compare A with #02
002E76:	C01F	BCT 	RP 	l_2EB4	; 31 words forward: conditional branch (BCT) to l_2EB4
002E78:	F901	LDR 	P  	=#01	; LDR operand: #01
002E7A:	5116	STA 	DG 	TWB_T7	; [TWB_T7] <- A
002E7C:	0034	LDA 	DL 	V_0034	; A <- [V_0034]
002E7E:	3604	DCL 	DL 	V_0004	; DCL operand: [V_0004]
002E80:	2202	LDX 	P  	=#02	; X <- #02
002E82:	A962	AND 	ILX	*l_0062,X	; A <- A and [G+0222+X]  (pointer l_0062 = #0222: address = G + #0222, G unknown)
002E84:	4310	EOR 	DG 	TWB_T4	; A <- A xor [TWB_T4]
002E86:	C315	BCF 	RP 	l_2EB0	; 21 words forward: conditional branch (BCF) to l_2EB0
002E88:	220C	LDX 	P  	=#0C	; X <- #0C
002E8A:	F904	LDR 	P  	=#04	; LDR operand: #04
002E8C:	2B0D	CMP 	P  	=#0D	; compare A with #0D
002E8E:	C011	BCT 	RP 	l_2EB0	; 17 words forward: conditional branch (BCT) to l_2EB0
002E90:	2B0B	CMP 	P  	=#0B	; compare A with #0B
002E92:	C207	BOT 	RP 	l_2EA0	; 7 words forward: conditional branch (BOT) to l_2EA0
002E94:	2B11	CMP 	P  	=#11	; compare A with #11
002E96:	C605	BOF 	RP 	l_2EA0	; 5 words forward: conditional branch (BOF) to l_2EA0
002E98:	260A	SUB 	P  	=#0A	; A <- A - #0A
002E9A:	F11C	CNA 	P  	
002E9C:	FA05	STR 	P  	=#05	; STR operand: #05
002E9E:	AF62	LBX 	ILX	*l_0062,X	; LBX operand: [G+0222+X]  (pointer l_0062 = #0222: address = G + #0222, G unknown)
l_2EA0:				; branch target; 2 refs; only from untraced words
002EA0:	400C	LDA 	DG 	TWB_T2	; A <- [TWB_T2]
002EA2:	A562	ADD 	ILX	*l_0062,X	; A <- A + [G+0222+X]  (pointer l_0062 = #0222: address = G + #0222, G unknown)
002EA4:	2601	SUB 	P  	=#01	; A <- A - #01
002EA6:	F202	ICX 	P  	=#02	; ICX operand: #02
002EA8:	AB62	CMP 	ILX	*l_0062,X	; compare A with [G+0222+X]  (pointer l_0062 = #0222: address = G + #0222, G unknown)
002EAA:	C632	BOF 	RP 	l_2F0E	; 50 words forward: conditional branch (BOF) to l_2F0E
002EAC:	2502	ADD 	P  	=#02	; A <- A + #02
002EAE:	510C	STA 	DG 	TWB_T2	; [TWB_T2] <- A
l_2EB0:				; branch target; 2 refs; only from untraced words
002EB0:	4016	LDA 	DG 	TWB_T7	; A <- [TWB_T7]
002EB2:	FA01	STR 	P  	=#01	; STR operand: #01
l_2EB4:				; branch target; 1 ref; only from untraced words
002EB4:	4E11	LBR 	DG 	TWB_T4+1	; A <- right byte of [TWB_T4+1]
002EB6:	110A	STA 	DL 	CPT_PTR	; [CPT_PTR] <- A
002EB8:	400C	LDA 	DG 	TWB_T2	; A <- [TWB_T2]
002EBA:	0624	SUB 	DL 	V_0024	; A <- A - [V_0024]
002EBC:	1108	STA 	DL 	V_0008	; [V_0008] <- A
002EBE:	2100	LDE 	P  	=#00	; E <- #00
002EC0:	2818	DIV 	P  	=#18	; divide by #18
002EC2:	F006	SLLS	P  	#06
002EC4:	111E	STA 	DL 	V_001E	; [V_001E] <- A
002EC6:	F904	LDR 	P  	=#04	; LDR operand: #04
002EC8:	C302	BCF 	RP 	l_2ECC	; 2 words forward: conditional branch (BCF) to l_2ECC
002ECA:	2518	ADD 	P  	=#18	; A <- A + #18
l_2ECC:				; branch target; 1 ref; only from untraced words
002ECC:	2601	SUB 	P  	=#01	; A <- A - #01
002ECE:	F001	SLLS	P  	#01
002ED0:	171E	ADM 	DL 	V_001E	; add A to [V_001E]
002ED2:	000A	LDA 	DL 	CPT_PTR	; A <- [CPT_PTR]
002ED4:	2903	AND 	P  	=#03	; A <- A and #03
002ED6:	FA05	STR 	P  	=#05	; STR operand: #05
002ED8:	000A	LDA 	DL 	CPT_PTR	; A <- [CPT_PTR]
002EDA:	2904	AND 	P  	=#04	; A <- A and #04
002EDC:	C502	BAZ 	RP 	l_2EE0	; 2 words forward: conditional branch (BAZ) to l_2EE0
002EDE:	2001	LDA 	P  	=#01	; A <- #01
l_2EE0:				; branch target; 1 ref; only from untraced words
002EE0:	171E	ADM 	DL 	V_001E	; add A to [V_001E]
002EE2:	AE2E	LBR 	ILX	*V_002E,X	; A <- right byte of [G+0968+X]  (pointer V_002E = #0968: address = G + #0968, G unknown)
002EE4:	1510	SBR 	DL 	V_0010	; right byte of [V_0010] <- A
002EE6:	2000	LDA 	P  	=#00	; A <- #00
002EE8:	FA3E	STR 	P  	=#3E	; STR operand: #3E
002EEA:	0014	LDA 	DL 	V_0014	; A <- [V_0014]
002EEC:	FA39	STR 	P  	=#39	; STR operand: #39
002EEE:	0022	LDA 	DL 	V_0022	; A <- [V_0022]
002EF0:	FA3A	STR 	P  	=#3A	; STR operand: #3A
002EF2:	001E	LDA 	DL 	V_001E	; A <- [V_001E]
002EF4:	FA3C	STR 	P  	=#3C	; STR operand: #3C
002EF6:	0010	LDA 	DL 	V_0010	; A <- [V_0010]
002EF8:	2105	LDE 	P  	=#05	; E <- #05
002EFA:	F403	WD  	P  		; privileged control instruction (WD)
002EFC:	F93C	LDR 	P  	=#3C	; LDR operand: #3C
002EFE:	2115	LDE 	P  	=#15	; E <- #15
002F00:	F408	STM 	P  		; privileged control instruction (STM)
002F02:	1218	STE 	DL 	V_0018	; [V_0018] <- E
002F04:	F403	WD  	P  		; privileged control instruction (WD)
002F06:	2000	LDA 	P  	=#00	; A <- #00
l_2F08:				; branch target; 7 refs; only from untraced words
002F08:	D706	BRU 	IL 	*V_0006	; RETURN from subroutine: branch through return cell V_0006 (written by the SPA of the call)
002F0A:	2EB9	LBR 	P  	=#B9	; A <- right byte of #B9
002F0C:	CF02	BRU 	RM 	l_2F08	; 2 words backward: jump to l_2F08
l_2F0E:				; branch target; 1 ref; only from untraced words
002F0E:	4016	LDA 	DG 	TWB_T7	; A <- [TWB_T7]
002F10:	FA01	STR 	P  	=#01	; STR operand: #01
l_2F12:				; branch target; 5 refs; only from untraced words
002F12:	2EBF	LBR 	P  	=#BF	; A <- right byte of #BF
002F14:	CF06	BRU 	RM 	l_2F08	; 6 words backward: jump to l_2F08; system structure
; 
; ==============================================
CTX26:				; system structure
; ============================================

002F16:	0014	.WORD	#0014	; CTX level 26: saved status
002F18:	0004	.WORD	#0004	; CTX level 26: saved X
002F1A:	0018	.WORD	#0018	; CTX level 26: saved E
002F1C:	0000	.WORD	#0000	; CTX level 26: saved A
002F1E:	2C86	.WORD	#2C86	; CTX level 26: saved G
002F20:	2CA6	.WORD	#2CA6	; CTX level 26: saved L
002F22:	2D5C	.WORD	#2D5C	; CTX level 26: saved P (entry of the task / interrupt routine of this level)
002F24:	0200	LDX 	DL 	Unknow_1	; X <- [Unknow_1]; system structure
; 
; ==============================================
PRTS55:				; system structure
; ============================================

002F26:	1C94	.WORD	#1C94	; supervisor SRD 55, word 1: absolute L = #1C94
002F28:	1CBE	.WORD	#1CBE	; supervisor SRD 55, word 2: absolute P = #1CBE (monitor module 55 entry); system structure
; 
; ==============================================
PRTS54:				; system structure
; ============================================

002F2A:	0108	.WORD	#0108	; supervisor SRD 54, word 1: absolute L = #0108
002F2C:	1C92	.WORD	#1C92	; supervisor SRD 54, word 2: absolute P = #1C92 (monitor module 54 entry); system structure
; 
; ==============================================
PRTS53:				; system structure
; ============================================

002F2E:	0000	.WORD	#0000	; supervisor SRD 53, word 1: absolute L = #0000
002F30:	1C70	.WORD	#1C70	; supervisor SRD 53, word 2: absolute P = #1C70 (monitor module 53 entry); system structure
; 
; ==============================================
PRTS52:				; system structure
; ============================================

002F32:	0000	.WORD	#0000	; supervisor SRD 52, word 1: absolute L = #0000
002F34:	1C6E	.WORD	#1C6E	; supervisor SRD 52, word 2: absolute P = #1C6E (monitor module 52 entry); system structure
; 
; ==============================================
PRTS51:				; system structure
; ============================================

002F36:	0108	.WORD	#0108	; supervisor SRD 51, word 1: absolute L = #0108
002F38:	1C62	.WORD	#1C62	; supervisor SRD 51, word 2: absolute P = #1C62 (monitor module 51 entry); system structure
; 
; ==============================================
PRTS50:				; system structure
; ============================================

002F3A:	0108	.WORD	#0108	; supervisor SRD 50, word 1: absolute L = #0108
002F3C:	1BF2	.WORD	#1BF2	; supervisor SRD 50, word 2: absolute P = #1BF2 (monitor module 50 entry); system structure
; 
; ==============================================
PRTS49:				; system structure
; ============================================

002F3E:	0000	.WORD	#0000	; supervisor SRD 49, word 1: absolute L = #0000
002F40:	1B80	.WORD	#1B80	; supervisor SRD 49, word 2: absolute P = #1B80 (monitor module 49 entry); system structure
; 
; ==============================================
PRTS48:				; system structure
; ============================================

002F42:	0000	.WORD	#0000	; supervisor SRD 48, word 1: absolute L = #0000
002F44:	1B7A	.WORD	#1B7A	; supervisor SRD 48, word 2: absolute P = #1B7A (monitor module 48 entry); system structure
; 
; ==============================================
PRTS47:				; system structure
; ============================================

002F46:	0000	.WORD	#0000	; supervisor SRD 47, word 1: absolute L = #0000
002F48:	1B7A	.WORD	#1B7A	; supervisor SRD 47, word 2: absolute P = #1B7A (monitor module 47 entry); system structure
; 
; ==============================================
PRTS46:				; system structure
; ============================================

002F4A:	0000	.WORD	#0000	; supervisor SRD 46, word 1: absolute L = #0000
002F4C:	1B7E	.WORD	#1B7E	; supervisor SRD 46, word 2: absolute P = #1B7E (monitor module 46 entry); system structure
; 
; ==============================================
PRTS45:				; system structure
; ============================================

002F4E:	0000	.WORD	#0000	; supervisor SRD 45, word 1: absolute L = #0000
002F50:	1B7E	.WORD	#1B7E	; supervisor SRD 45, word 2: absolute P = #1B7E (monitor module 45 entry); system structure
; 
; ==============================================
PRTS44:				; system structure
; ============================================

002F52:	0000	.WORD	#0000	; supervisor SRD 44, word 1: absolute L = #0000
002F54:	1B7E	.WORD	#1B7E	; supervisor SRD 44, word 2: absolute P = #1B7E (monitor module 44 entry); system structure
; 
; ==============================================
PRTS43:				; system structure
; ============================================

002F56:	0000	.WORD	#0000	; supervisor SRD 43, word 1: absolute L = #0000
002F58:	1B7E	.WORD	#1B7E	; supervisor SRD 43, word 2: absolute P = #1B7E (monitor module 43 entry); system structure
; 
; ==============================================
PRTS42:				; system structure
; ============================================

002F5A:	0000	.WORD	#0000	; supervisor SRD 42, word 1: absolute L = #0000
002F5C:	1B7E	.WORD	#1B7E	; supervisor SRD 42, word 2: absolute P = #1B7E (monitor module 42 entry); system structure
; 
; ==============================================
PRTS41:				; system structure
; ============================================

002F5E:	0000	.WORD	#0000	; supervisor SRD 41, word 1: absolute L = #0000
002F60:	1B7E	.WORD	#1B7E	; supervisor SRD 41, word 2: absolute P = #1B7E (monitor module 41 entry); system structure
; 
; ==============================================
PRTS40:				; system structure
; ============================================

002F62:	0000	.WORD	#0000	; supervisor SRD 40, word 1: absolute L = #0000
002F64:	1B7E	.WORD	#1B7E	; supervisor SRD 40, word 2: absolute P = #1B7E (monitor module 40 entry); system structure
; 
; ==============================================
PRTS39:				; system structure
; ============================================

002F66:	0000	.WORD	#0000	; supervisor SRD 39, word 1: absolute L = #0000
002F68:	1B7A	.WORD	#1B7A	; supervisor SRD 39, word 2: absolute P = #1B7A (monitor module 39 entry); system structure
; 
; ==============================================
PRTS38:				; system structure
; ============================================

002F6A:	1B62	.WORD	#1B62	; supervisor SRD 38, word 1: absolute L = #1B62
002F6C:	1B72	.WORD	#1B72	; supervisor SRD 38, word 2: absolute P = #1B72 (monitor module 38 entry); system structure
; 
; ==============================================
PRTS37:				; system structure
; ============================================

002F6E:	0108	.WORD	#0108	; supervisor SRD 37, word 1: absolute L = #0108
002F70:	1B5C	.WORD	#1B5C	; supervisor SRD 37, word 2: absolute P = #1B5C (monitor module 37 entry); system structure
; 
; ==============================================
PRTS36:				; system structure
; ============================================

002F72:	0000	.WORD	#0000	; supervisor SRD 36, word 1: absolute L = #0000
002F74:	1A7C	.WORD	#1A7C	; supervisor SRD 36, word 2: absolute P = #1A7C (monitor module 36 entry); system structure
; 
; ==============================================
PRTS35:				; system structure
; ============================================

002F76:	0000	.WORD	#0000	; supervisor SRD 35, word 1: absolute L = #0000
002F78:	1A7A	.WORD	#1A7A	; supervisor SRD 35, word 2: absolute P = #1A7A (monitor module 35 entry); system structure
; 
; ==============================================
PRTS34:				; system structure
; ============================================

002F7A:	0000	.WORD	#0000	; supervisor SRD 34, word 1: absolute L = #0000
002F7C:	1A6E	.WORD	#1A6E	; supervisor SRD 34, word 2: absolute P = #1A6E (monitor module 34 entry); system structure
; 
; ==============================================
PRTS33:				; system structure
; ============================================

002F7E:	0000	.WORD	#0000	; supervisor SRD 33, word 1: absolute L = #0000
002F80:	1A60	.WORD	#1A60	; supervisor SRD 33, word 2: absolute P = #1A60 (monitor module 33 entry); system structure
; 
; ==============================================
PRTS32:				; system structure
; ============================================

002F82:	0000	.WORD	#0000	; supervisor SRD 32, word 1: absolute L = #0000
002F84:	19E2	.WORD	#19E2	; supervisor SRD 32, word 2: absolute P = #19E2 (monitor module 32 entry); system structure
; 
; ==============================================
PRTS31:				; system structure
; ============================================

002F86:	0000	.WORD	#0000	; supervisor SRD 31, word 1: absolute L = #0000
002F88:	1918	.WORD	#1918	; supervisor SRD 31, word 2: absolute P = #1918 (monitor module 31 entry); system structure
; 
; ==============================================
PRTS30:				; system structure
; ============================================

002F8A:	0000	.WORD	#0000	; supervisor SRD 30, word 1: absolute L = #0000
002F8C:	190C	.WORD	#190C	; supervisor SRD 30, word 2: absolute P = #190C (monitor module 30 entry); system structure
; 
; ==============================================
PRTS29:				; system structure
; ============================================

002F8E:	0000	.WORD	#0000	; supervisor SRD 29, word 1: absolute L = #0000
002F90:	17C6	.WORD	#17C6	; supervisor SRD 29, word 2: absolute P = #17C6 (monitor module 29 entry); system structure
; 
; ==============================================
PRTS28:				; system structure
; ============================================

002F92:	0000	.WORD	#0000	; supervisor SRD 28, word 1: absolute L = #0000
002F94:	179A	.WORD	#179A	; supervisor SRD 28, word 2: absolute P = #179A (monitor module 28 entry); system structure
; 
; ==============================================
PRTS27:				; system structure
; ============================================

002F96:	0000	.WORD	#0000	; supervisor SRD 27, word 1: absolute L = #0000
002F98:	1774	.WORD	#1774	; supervisor SRD 27, word 2: absolute P = #1774 (monitor module 27 entry); system structure
; 
; ==============================================
PRTS26:				; system structure
; ============================================

002F9A:	0000	.WORD	#0000	; supervisor SRD 26, word 1: absolute L = #0000
002F9C:	174C	.WORD	#174C	; supervisor SRD 26, word 2: absolute P = #174C (monitor module 26 entry); system structure
; 
; ==============================================
PRTS25:				; system structure
; ============================================

002F9E:	0000	.WORD	#0000	; supervisor SRD 25, word 1: absolute L = #0000
002FA0:	1702	.WORD	#1702	; supervisor SRD 25, word 2: absolute P = #1702 (monitor module 25 entry); system structure
; 
; ==============================================
PRTS24:				; system structure
; ============================================

002FA2:	0000	.WORD	#0000	; supervisor SRD 24, word 1: absolute L = #0000
002FA4:	16F2	.WORD	#16F2	; supervisor SRD 24, word 2: absolute P = #16F2 (monitor module 24 entry); system structure
; 
; ==============================================
PRTS23:				; system structure
; ============================================

002FA6:	03C2	.WORD	#03C2	; supervisor SRD 23, word 1: absolute L = #03C2
002FA8:	16F0	.WORD	#16F0	; supervisor SRD 23, word 2: absolute P = #16F0 (monitor module 23 entry); system structure
; 
; ==============================================
PRTS22:				; system structure
; ============================================

002FAA:	03C2	.WORD	#03C2	; supervisor SRD 22, word 1: absolute L = #03C2
002FAC:	16EE	.WORD	#16EE	; supervisor SRD 22, word 2: absolute P = #16EE (monitor module 22 entry); system structure
; 
; ==============================================
PRTS21:				; system structure
; ============================================

002FAE:	03C2	.WORD	#03C2	; supervisor SRD 21, word 1: absolute L = #03C2
002FB0:	16EC	.WORD	#16EC	; supervisor SRD 21, word 2: absolute P = #16EC (monitor module 21 entry); system structure
; 
; ==============================================
PRTS20:				; system structure
; ============================================

002FB2:	0000	.WORD	#0000	; supervisor SRD 20, word 1: absolute L = #0000
002FB4:	165E	.WORD	#165E	; supervisor SRD 20, word 2: absolute P = #165E (monitor module 20 entry); system structure
; 
; ==============================================
PRTS19:				; system structure
; ============================================

002FB6:	0000	.WORD	#0000	; supervisor SRD 19, word 1: absolute L = #0000
002FB8:	163C	.WORD	#163C	; supervisor SRD 19, word 2: absolute P = #163C (monitor module 19 entry); system structure
; 
; ==============================================
PRTS18:				; system structure
; ============================================

002FBA:	0000	.WORD	#0000	; supervisor SRD 18, word 1: absolute L = #0000
002FBC:	15BA	.WORD	#15BA	; supervisor SRD 18, word 2: absolute P = #15BA (monitor module 18 entry); system structure
; 
; ==============================================
PRTS17:				; system structure
; ============================================

002FBE:	0000	.WORD	#0000	; supervisor SRD 17, word 1: absolute L = #0000
002FC0:	159E	.WORD	#159E	; supervisor SRD 17, word 2: absolute P = #159E (monitor module 17 entry); system structure
; 
; ==============================================
PRTS16:				; system structure
; ============================================

002FC2:	0000	.WORD	#0000	; supervisor SRD 16, word 1: absolute L = #0000
002FC4:	153A	.WORD	#153A	; supervisor SRD 16, word 2: absolute P = #153A (monitor module 16 entry); system structure
; 
; ==============================================
PRTS15:				; system structure
; ============================================

002FC6:	0000	.WORD	#0000	; supervisor SRD 15, word 1: absolute L = #0000
002FC8:	14EA	.WORD	#14EA	; supervisor SRD 15, word 2: absolute P = #14EA (monitor module 15 entry); system structure
; 
; ==============================================
PRTS14:				; system structure
; ============================================

002FCA:	0000	.WORD	#0000	; supervisor SRD 14, word 1: absolute L = #0000
002FCC:	1464	.WORD	#1464	; supervisor SRD 14, word 2: absolute P = #1464 (monitor module 14 entry); system structure
; 
; ==============================================
PRTS13:				; system structure
; ============================================

002FCE:	0000	.WORD	#0000	; supervisor SRD 13, word 1: absolute L = #0000
002FD0:	144E	.WORD	#144E	; supervisor SRD 13, word 2: absolute P = #144E (monitor module 13 entry); system structure
; 
; ==============================================
PRTS12:				; system structure
; ============================================

002FD2:	0000	.WORD	#0000	; supervisor SRD 12, word 1: absolute L = #0000
002FD4:	0BA4	.WORD	#0BA4	; supervisor SRD 12, word 2: absolute P = #0BA4 (monitor module 12 entry); system structure
; 
; ==============================================
PRTS11:				; system structure
; ============================================

002FD6:	0000	.WORD	#0000	; supervisor SRD 11, word 1: absolute L = #0000
002FD8:	09BC	.WORD	#09BC	; supervisor SRD 11, word 2: absolute P = #09BC (monitor module 11 entry); system structure
; 
; ==============================================
PRTS10:				; system structure
; ============================================

002FDA:	0000	.WORD	#0000	; supervisor SRD 10, word 1: absolute L = #0000
002FDC:	0980	.WORD	#0980	; supervisor SRD 10, word 2: absolute P = #0980 (monitor module 10 entry); system structure
; 
; ==============================================
PRTS9:				; system structure
; ============================================

002FDE:	0000	.WORD	#0000	; supervisor SRD 9, word 1: absolute L = #0000
002FE0:	0962	.WORD	#0962	; supervisor SRD 9, word 2: absolute P = #0962 (monitor module 9 entry); system structure
; 
; ==============================================
PRTS8:				; system structure
; ============================================

002FE2:	0000	.WORD	#0000	; supervisor SRD 8, word 1: absolute L = #0000
002FE4:	092E	.WORD	#092E	; supervisor SRD 8, word 2: absolute P = #092E (monitor module 8 entry); system structure
; 
; ==============================================
PRTS7:				; system structure
; ============================================

002FE6:	0000	.WORD	#0000	; supervisor SRD 7, word 1: absolute L = #0000
002FE8:	08F0	.WORD	#08F0	; supervisor SRD 7, word 2: absolute P = #08F0 (monitor module 7 entry); system structure
; 
; ==============================================
PRTS6:				; system structure
; ============================================

002FEA:	0000	.WORD	#0000	; supervisor SRD 6, word 1: absolute L = #0000
002FEC:	08D2	.WORD	#08D2	; supervisor SRD 6, word 2: absolute P = #08D2 (monitor module 6 entry); system structure
; 
; ==============================================
PRTS5:				; system structure
; ============================================

002FEE:	0000	.WORD	#0000	; supervisor SRD 5, word 1: absolute L = #0000
002FF0:	0890	.WORD	#0890	; supervisor SRD 5, word 2: absolute P = #0890 (monitor module 5 entry); system structure
; 
; ==============================================
PRTS4:				; system structure
; ============================================

002FF2:	0000	.WORD	#0000	; supervisor SRD 4, word 1: absolute L = #0000
002FF4:	083E	.WORD	#083E	; supervisor SRD 4, word 2: absolute P = #083E (monitor module 4 entry); system structure
; 
; ==============================================
PRTS3:				; system structure
; ============================================

002FF6:	0000	.WORD	#0000	; supervisor SRD 3, word 1: absolute L = #0000
002FF8:	0790	.WORD	#0790	; supervisor SRD 3, word 2: absolute P = #0790 (monitor module 3 entry); system structure
; 
; ==============================================
PRTS2:				; system structure
; ============================================

002FFA:	0000	.WORD	#0000	; supervisor SRD 2, word 1: absolute L = #0000
002FFC:	31A8	.WORD	#31A8	; supervisor SRD 2, word 2: absolute P = #31A8 (monitor module 2 entry); system structure
; 
; ==============================================
PRTS1:				; system structure
; ============================================

002FFE:	0000	.WORD	#0000	; supervisor SRD 1, word 1: absolute L = #0000
003000:	04D0	.WORD	#04D0	; supervisor SRD 1, word 2: absolute P = #04D0 (monitor module 1 entry); system structure
; 
; ==============================================
PRTS0:				; system structure
; ============================================

003002:	1E7E	.WORD	#1E7E	; supervisor SRD 0, word 1: absolute L = #1E7E
003004:	1EC8	.WORD	#1EC8	; supervisor SRD 0, word 2: absolute P = #1EC8 (monitor module 0 entry)
003006:	0358	EOR 	DL 	v_0058	; A <- A xor [v_0058]
003008:	0020	LDA 	DL 	V_0020	; A <- [V_0020]
00300A:	0014	LDA 	DL 	V_0014	; A <- [V_0014]
00300C:	0004	LDA 	DL 	V_0004	; A <- [V_0004]
00300E:	3490	??	3490
003010:	3001	SLLS	DL 	#01
003012:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
003014:	8000	LDA 	IGX	*TWB_P,X	; A <- [*TWB_P+X]  (pointer TWB_P lives in the calling task's area: its contents are only known at run time)
003016:	07B2	IOR 	DL 	V_00B2	; A <- A or [V_00B2]
003018:	0005	LDA 	DL 	V_0004	; A <- [V_0004]
00301A:	EB5C	LDP 	PX 	(X)	; LDP operand: [(X)]
00301C:	2CA6	MUL 	P  	=#A6	; multiply by #A6
00301E:	0700	IOR 	DL 	Unknow_1	; A <- A or [Unknow_1]
003020:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
003022:	0796	IOR 	DL 	V_0096	; A <- A or [V_0096]
003024:	07E8	IOR 	DL 	V_00E8	; A <- A or [V_00E8]
003026:	3222	ICX 	DL 	V_0022	; ICX operand: [V_0022]
003028:	3498	??	3498
00302A:	3026	SRCS	DL 	#06
00302C:	0684	SUB 	DL 	v_0084	; A <- A - [v_0084]
00302E:	0604	SUB 	DL 	V_0004	; A <- A - [V_0004]
003030:	004E	LDA 	DL 	V_004E	; A <- [V_004E]
003032:	32AE	ICX 	DL 	V_00AE	; ICX operand: [V_00AE]
003034:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
003036:	3001	SLLS	DL 	#01
003038:	3490	??	3490
00303A:	32FA	ICX 	DL 	v_00FA	; ICX operand: [v_00FA]
00303C:	3498	??	3498
00303E:	0000	.ZERO	3	; 3 zero words (padding, reserved area or unused data; never reached as code)
003044:	22A6	LDX 	P  	=#A6	; X <- #A6
003046:	006E	LDA 	DL 	v_006E	; A <- [v_006E]
003048:	.EBCDIC	"@OASLP"	; 3 words, raw: 7CD6 C1E2 D3D7
00304E:	2270	LDX 	P  	=#70	; X <- #70
003050:	C302	BCF 	RP 	l_3054	; 2 words forward: conditional branch (BCF) to l_3054
003052:	C306	BCF 	RP 	l_305E	; 6 words forward: conditional branch (BCF) to l_305E
l_3054:				; branch target; 1 ref; only from untraced words
003054:	C106	BRX 	RP 	$+X	; INDEXED JUMP (BRX): X selects one of the 0 BRU entries that follow (jump / dispatch table)
003056:	C107	BRX 	RP 	$+X	; INDEXED JUMP (BRX): X selects one of the 0 BRU entries that follow (jump / dispatch table)
003058:	C108	BRX 	RP 	$+X	; INDEXED JUMP (BRX): X selects one of the 0 BRU entries that follow (jump / dispatch table)
00305A:	C109	BRX 	RP 	$+X	; INDEXED JUMP (BRX): X selects one of the 0 BRU entries that follow (jump / dispatch table)
00305C:	3050	SAD 	DL 	#10
l_305E:				; branch target; 1 ref; only from untraced words
00305E:	30A8	SAS 	DL 	#08
003060:	0D15	LBL 	DL 	V_0014	; A <- left byte of [V_0014]
003062:	160C	DST 	DL 	PRTS_PTR	; [PRTS_PTR] <- A,E (2 words)
003064:	3600	DCL 	DL 	Unknow_1	; DCL operand: [Unknow_1]
003066:	0000	.ZERO	2	; 2 zero words (padding, reserved area or unused data; never reached as code)
00306A:	30A6	SAS 	DL 	#06
00306C:	30A7	SAS 	DL 	#07
00306E:	0000	.ZERO	2	; 2 zero words (padding, reserved area or unused data; never reached as code)
003072:	0651	SUB 	DL 	V_0050	; A <- A - [V_0050]
003074:	0601	SUB 	DL 	Unknow_1	; A <- A - [Unknow_1]
003076:	0000	.ZERO	2	; 2 zero words (padding, reserved area or unused data; never reached as code)
00307A:	1600	DST 	DL 	Unknow_1	; [Unknow_1] <- A,E (2 words)
00307C:	3600	DCL 	DL 	Unknow_1	; DCL operand: [Unknow_1]
00307E:	0000	.ZERO	2	; 2 zero words (padding, reserved area or unused data; never reached as code)
003082:	30F7	SRCD	DL 	#17
003084:	30A8	SAS 	DL 	#08
003086:	0000	.ZERO	2	; 2 zero words (padding, reserved area or unused data; never reached as code)
00308A:	1600	DST 	DL 	Unknow_1	; [Unknow_1] <- A,E (2 words)
00308C:	1600	DST 	DL 	Unknow_1	; [Unknow_1] <- A,E (2 words)
00308E:	0000	.ZERO	2	; 2 zero words (padding, reserved area or unused data; never reached as code)
003092:	30A8	SAS 	DL 	#08
003094:	8000	LDA 	IGX	*TWB_P,X	; A <- [*TWB_P+X]  (pointer TWB_P lives in the calling task's area: its contents are only known at run time)
003096:	0000	.ZERO	2	; 2 zero words (padding, reserved area or unused data; never reached as code)
00309A:	307A	SLCD	DL 	#1A
00309C:	331E	DCX 	DL 	V_001E	; DCX operand: [V_001E]
00309E:	0BB8	CMP 	DL 	v_00B8	; compare A with [v_00B8]
0030A0:	3378	DCX 	DL 	v_0078	; DCX operand: [v_0078]
0030A2:	5600	DST 	DG 	TWB_P	; [TWB_P] <- A,E (2 words)
0030A4:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
0030A6:	0002	LDA 	DL 	V_0002	; A <- [V_0002]
0030A8:	060A	SUB 	DL 	CPT_PTR	; A <- A - [CPT_PTR]
0030AA:	3330	DCX 	DL 	V_0030	; DCX operand: [V_0030]
0030AC:	3930	LDR 	DL 	V_0030	; LDR operand: [V_0030]
0030AE:	A0A0	LDA 	ILX	*V_00A0,X	; A <- [G+153C+X]  (pointer V_00A0 = #153C: address = G + #153C, G unknown)
0030B0:	A0A0	LDA 	ILX	*V_00A0,X	; A <- [G+153C+X]  (pointer V_00A0 = #153C: address = G + #153C, G unknown)
0030B2:	3333	DCX 	DL 	V_0032	; DCX operand: [V_0032]
0030B4:	B7B8	ADM 	ILX	*v_00B8,X	; add A to [G+1A76+X]  (pointer v_00B8 = #1A76: address = G + #1A76, G unknown)
0030B6:	A0A0	LDA 	ILX	*V_00A0,X	; A <- [G+153C+X]  (pointer V_00A0 = #153C: address = G + #153C, G unknown)
0030B8:	3536	ICL 	DL 	V_0036	; ICL operand: [V_0036]
0030BA:	3030	SRCS	DL 	#10
0030BC:	A0A0	LDA 	ILX	*V_00A0,X	; A <- [G+153C+X]  (pointer V_00A0 = #153C: address = G + #153C, G unknown)
0030BE:	3030	SRCS	DL 	#10
0030C0:	42B8	LDX 	DG 	G+00B8	; X <- [G+00B8]
0030C2:	A0A0	LDA 	ILX	*V_00A0,X	; A <- [G+153C+X]  (pointer V_00A0 = #153C: address = G + #153C, G unknown)
0030C4:	4130	LDE 	DG 	G+0030	; E <- [G+0030]
0030C6:	4130	LDE 	DG 	G+0030	; E <- [G+0030]
0030C8:	A0A0	LDA 	ILX	*V_00A0,X	; A <- [G+153C+X]  (pointer V_00A0 = #153C: address = G + #153C, G unknown)
0030CA:	3335	DCX 	DL 	V_0034	; DCX operand: [V_0034]
0030CC:	3336	DCX 	DL 	V_0036	; DCX operand: [V_0036]
0030CE:	A0A0	LDA 	ILX	*V_00A0,X	; A <- [G+153C+X]  (pointer V_00A0 = #153C: address = G + #153C, G unknown)
0030D0:	3333	DCX 	DL 	V_0032	; DCX operand: [V_0032]
0030D2:	3335	DCX 	DL 	V_0034	; DCX operand: [V_0034]
0030D4:	A0A0	LDA 	ILX	*V_00A0,X	; A <- [G+153C+X]  (pointer V_00A0 = #153C: address = G + #153C, G unknown)
0030D6:	3333	DCX 	DL 	V_0032	; DCX operand: [V_0032]
0030D8:	3336	DCX 	DL 	V_0036	; DCX operand: [V_0036]
0030DA:	A0A0	LDA 	ILX	*V_00A0,X	; A <- [G+153C+X]  (pointer V_00A0 = #153C: address = G + #153C, G unknown)
0030DC:	4130	LDE 	DG 	G+0030	; E <- [G+0030]
0030DE:	4130	LDE 	DG 	G+0030	; E <- [G+0030]
0030E0:	A0A0	LDA 	ILX	*V_00A0,X	; A <- [G+153C+X]  (pointer V_00A0 = #153C: address = G + #153C, G unknown)
0030E2:	A0DB	LDA 	ILX	*V_00DA,X	; A <- [G+0312+X]  (pointer V_00DA = #0312: address = G + #0312, G unknown)
0030E4:	A0A0	LDA 	ILX	*V_00A0,X	; A <- [G+153C+X]  (pointer V_00A0 = #153C: address = G + #153C, G unknown)
0030E6:	A0A0	LDA 	ILX	*V_00A0,X	; A <- [G+153C+X]  (pointer V_00A0 = #153C: address = G + #153C, G unknown)
0030E8:	A0A0	LDA 	ILX	*V_00A0,X	; A <- [G+153C+X]  (pointer V_00A0 = #153C: address = G + #153C, G unknown)
0030EA:	A0A0	LDA 	ILX	*V_00A0,X	; A <- [G+153C+X]  (pointer V_00A0 = #153C: address = G + #153C, G unknown)
0030EC:	A0A0	LDA 	ILX	*V_00A0,X	; A <- [G+153C+X]  (pointer V_00A0 = #153C: address = G + #153C, G unknown)
0030EE:	A0A0	LDA 	ILX	*V_00A0,X	; A <- [G+153C+X]  (pointer V_00A0 = #153C: address = G + #153C, G unknown)
0030F0:	A0A0	LDA 	ILX	*V_00A0,X	; A <- [G+153C+X]  (pointer V_00A0 = #153C: address = G + #153C, G unknown)
0030F2:	A0A0	LDA 	ILX	*V_00A0,X	; A <- [G+153C+X]  (pointer V_00A0 = #153C: address = G + #153C, G unknown)
0030F4:	DDA0	BAZ 	IG 	*G+00A0	; conditional branch, target not known statically  (pointer G+00A0 lives in the calling task's area: its contents are only known at run time)
0030F6:	0300	EOR 	DL 	Unknow_1	; A <- A xor [Unknow_1]
0030F8:	0000	.ZERO	12	; 12 zero words (padding, reserved area or unused data; never reached as code)
0031A0:	0000	.ZERO	4	; 4 zero words (padding, reserved area or unused data; never reached as code)

; ===== MON2: SUPERVISOR (monitor) MODULE: master mode, returns to its CSV caller with RSV =====
; routine profile: block move / translate string (MVS/TRS) 2 loops, stores into the caller's TWB, returns to its CSV caller with RSV
MON2:				; entry point; callee (section / supervisor module / SPA subroutine); 8 refs
0031A8:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0031AA:	D750	BRU 	IL 	*V_0050	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0050 = #051A: address = G + #051A, G unknown)
L_31AC:				; branch target; 1 ref
0031AC:	2026	LDA 	P  	=#26	; A <- #26
0031AE:	2D30	LBL 	P  	=#30	; A <- left byte of #30
0031B0:	FA01	STR 	P  	=#01	; STR operand: #01
0031B2:	4014	LDA 	DG 	TWB_T6	; A <- [TWB_T6]
0031B4:	5108	STA 	DG 	TWB_T0	; [TWB_T0] <- A
0031B6:	0B20	CMP 	DL 	V_0020	; compare A with [V_0020]
0031B8:	C333	BCF 	RP 	L_321E	; 51 words forward: conditional branch (BCF) to L_321E
0031BA:	3604	DCL 	DL 	V_0004	; DCL operand: [V_0004]
0031BC:	581C	SPA 	DG 	TWB_N1	; [TWB_N1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0031BE:	D752	BRU 	IL 	*V_0052	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_0052 = #0844: address = G + #0844, G unknown)
L_31C0:				; branch target; 1 ref
0031C0:	2026	LDA 	P  	=#26	; A <- #26
0031C2:	2D30	LBL 	P  	=#30	; A <- left byte of #30
0031C4:	FA01	STR 	P  	=#01	; STR operand: #01
0031C6:	6022	LDA 	IL 	*V_0022	; A <- [G+3006]  (pointer V_0022 = #3006: address = G + #3006, G unknown)
0031C8:	0B24	CMP 	DL 	V_0024	; compare A with [V_0024]
0031CA:	C32A	BCF 	RP 	L_321E	; 42 words forward: conditional branch (BCF) to L_321E
0031CC:	220A	LDX 	P  	=#0A	; X <- #0A
0031CE:	A022	LDA 	ILX	*V_0022,X	; A <- [G+3006+X]  (pointer V_0022 = #3006: address = G + #3006, G unknown)
0031D0:	0B26	CMP 	DL 	V_0026	; compare A with [V_0026]
0031D2:	C326	BCF 	RP 	L_321E	; 38 words forward: conditional branch (BCF) to L_321E
0031D4:	2206	LDX 	P  	=#06	; X <- #06
0031D6:	AE22	LBR 	ILX	*V_0022,X	; A <- right byte of [G+3006+X]  (pointer V_0022 = #3006: address = G + #3006, G unknown)
0031D8:	2205	LDX 	P  	=#05	; X <- #05
0031DA:	AD22	LBL 	ILX	*V_0022,X	; A <- left byte of [G+3006+X]  (pointer V_0022 = #3006: address = G + #3006, G unknown)
0031DC:	2220	LDX 	P  	=#20	; X <- #20
L_31DE:				; branch target; 1 ref
0031DE:	AB28	CMP 	ILX	*V_0028,X	; compare A with [G+1E7E+X]  (pointer V_0028 = #1E7E: address = G + #1E7E, G unknown)
0031E0:	C004	BCT 	RP 	L_31E8	; 4 words forward: conditional branch (BCT) to L_31E8
0031E2:	F302	DCX 	P  	=#02	; DCX operand: #02
0031E4:	C803	BCT 	RM 	L_31DE	; 3 words backward: conditional branch (BCT) to L_31DE
0031E6:	C71C	BRU 	RP 	L_321E	; 28 words forward: jump to L_321E
L_31E8:				; branch target; 1 ref
0031E8:	531C	STX 	DG 	TWB_N1	; [TWB_N1] <- X
0031EA:	F11E	CHX 	P  	
0031EC:	F905	LDR 	P  	=#05	; LDR operand: #05
0031EE:	220D	LDX 	P  	=#0D	; X <- #0D
0031F0:	AD22	LBL 	ILX	*V_0022,X	; A <- left byte of [G+3006+X]  (pointer V_0022 = #3006: address = G + #3006, G unknown)
0031F2:	F108	XAA 	P  	
0031F4:	27C1	IOR 	P  	=#C1	; A <- A or #C1
0031F6:	F108	XAA 	P  	
0031F8:	220A	LDX 	P  	=#0A	; X <- #0A
L_31FA:				; branch target; 1 ref
0031FA:	AB36	CMP 	ILX	*V_0036,X	; compare A with [G+0003+X]  (pointer V_0036 = #0003: address = G + #0003, G unknown)
0031FC:	C004	BCT 	RP 	L_3204	; 4 words forward: conditional branch (BCT) to L_3204
0031FE:	F302	DCX 	P  	=#02	; DCX operand: #02
003200:	C803	BCT 	RM 	L_31FA	; 3 words backward: conditional branch (BCT) to L_31FA
003202:	C70E	BRU 	RP 	L_321E	; 14 words forward: jump to L_321E
L_3204:				; branch target; 1 ref
003204:	F108	XAA 	P  	
003206:	2902	AND 	P  	=#02	; A <- A and #02
003208:	2302	EOR 	P  	=#02	; A <- A xor #02
00320A:	F0C1	SRLS	P  	#01
00320C:	2D3C	LBL 	P  	=#3C	; A <- left byte of #3C
00320E:	421C	LDX 	DG 	TWB_N1	; X <- [TWB_N1]
003210:	B11E	STA 	ILX	*V_001E,X	; [G+2224+X] <- A  (pointer V_001E = #2224: address = G + #2224, G unknown)
003212:	2110	LDE 	P  	=#10	; E <- #10
003214:	2201	LDX 	P  	=#01	; X <- #01
003216:	A422	LEA 	ILX	*V_0022,X	; A <- G+3006+X  (effective address, memory not read)  (pointer V_0022 = #3006: address = G + #3006, G unknown)
003218:	7F22	MVS 	IL 	*V_0022	; MVS operand: [G+3006]  (pointer V_0022 = #3006: address = G + #3006, G unknown)
00321A:	200D	LDA 	P  	=#0D	; A <- #0D
00321C:	7522	SBR 	IL 	*V_0022	; right byte of [G+3006] <- A  (pointer V_0022 = #3006: address = G + #3006, G unknown)
L_321E:				; branch target; 5 refs
00321E:	4008	LDA 	DG 	TWB_T0	; A <- [TWB_T0]
003220:	F10C	RSV 	P  		; RETURN SUPERVISOR: restore L, P and indicators from the TWB, resume after the CSV
003222:	1102	STA 	DL 	V_0002	; [V_0002] <- A
l_3224:				; branch target; 1 ref; only from untraced words
003224:	0E80	LBR 	DL 	V_0080	; A <- right byte of [V_0080]
003226:	CB01	BCF 	RM 	l_3224	; 1 word backward: conditional branch (BCF) to l_3224
003228:	1312	STX 	DL 	V_0012	; [V_0012] <- X
00322A:	1210	STE 	DL 	V_0010	; [V_0010] <- E
00322C:	6012	LDA 	IL 	*V_0012	; A <- [G+225A]  (pointer V_0012 = #225A: address = G + #225A, G unknown)
00322E:	2901	AND 	P  	=#01	; A <- A and #01
003230:	C003	BCT 	RP 	l_3236	; 3 words forward: conditional branch (BCT) to l_3236
003232:	0002	LDA 	DL 	V_0002	; A <- [V_0002]
003234:	D776	BRU 	IL 	*v_0076	; jump to G+#0314 (G unknown)  (pointer v_0076 = #0314: address = G + #0314, G unknown)
l_3236:				; branch target; 1 ref; only from untraced words
003236:	0002	LDA 	DL 	V_0002	; A <- [V_0002]
003238:	1116	STA 	DL 	V_0016	; [V_0016] <- A
00323A:	2001	LDA 	P  	=#01	; A <- #01
00323C:	1580	SBR 	DL 	V_0080	; right byte of [V_0080] <- A
00323E:	2202	LDX 	P  	=#02	; X <- #02
003240:	AE12	LBR 	ILX	*V_0012,X	; A <- right byte of [G+225A+X]  (pointer V_0012 = #225A: address = G + #225A, G unknown)
003242:	2990	AND 	P  	=#90	; A <- A and #90
003244:	2B80	CMP 	P  	=#80	; compare A with #80
003246:	C308	BCF 	RP 	l_3256	; 8 words forward: conditional branch (BCF) to l_3256
003248:	0E11	LBR 	DL 	V_0010	; A <- right byte of [V_0010]
00324A:	C02A	BCT 	RP 	l_329E	; 42 words forward: conditional branch (BCT) to l_329E
00324C:	003A	LDA 	DL 	V_003A	; A <- [V_003A]
00324E:	1182	STA 	DL 	V_0082	; [V_0082] <- A
003250:	2002	LDA 	P  	=#02	; A <- #02
003252:	110E	STA 	DL 	PRTS_COUNT	; [PRTS_COUNT] <- A
003254:	C725	BRU 	RP 	l_329E	; 37 words forward: jump to l_329E
l_3256:				; branch target; 1 ref; only from untraced words
003256:	2B90	CMP 	P  	=#90	; compare A with #90
003258:	C363	BCF 	RP 	l_331E	; 99 words forward: conditional branch (BCF) to l_331E
00325A:	2001	LDA 	P  	=#01	; A <- #01
00325C:	111A	STA 	DL 	V_001A	; [V_001A] <- A
00325E:	1716	ADM 	DL 	V_0016	; add A to [V_0016]
003260:	2200	LDX 	P  	=#00	; X <- #00
003262:	130E	STX 	DL 	PRTS_COUNT	; [PRTS_COUNT] <- X
003264:	F301	DCX 	P  	=#01	; DCX operand: #01
003266:	AE16	LBR 	ILX	*V_0016,X	; A <- right byte of [G+2270+X]  (pointer V_0016 = #2270: address = G + #2270, G unknown)
003268:	1118	STA 	DL 	V_0018	; [V_0018] <- A
00326A:	C01A	BCT 	RP 	l_329E	; 26 words forward: conditional branch (BCT) to l_329E
00326C:	200D	LDA 	P  	=#0D	; A <- #0D
00326E:	1582	SBR 	DL 	V_0082	; right byte of [V_0082] <- A
003270:	2001	LDA 	P  	=#01	; A <- #01
003272:	170E	ADM 	DL 	PRTS_COUNT	; add A to [PRTS_COUNT]
003274:	0018	LDA 	DL 	V_0018	; A <- [V_0018]
003276:	2970	AND 	P  	=#70	; A <- A and #70
003278:	2B60	CMP 	P  	=#60	; compare A with #60
00327A:	C012	BCT 	RP 	l_329E	; 18 words forward: conditional branch (BCT) to l_329E
00327C:	200C	LDA 	P  	=#0C	; A <- #0C
00327E:	1583	SBR 	DL 	V_0082	; right byte of [V_0082] <- A
003280:	2001	LDA 	P  	=#01	; A <- #01
003282:	170E	ADM 	DL 	PRTS_COUNT	; add A to [PRTS_COUNT]
003284:	0018	LDA 	DL 	V_0018	; A <- [V_0018]
003286:	2BF0	CMP 	P  	=#F0	; compare A with #F0
003288:	C60B	BOF 	RP 	l_329E	; 11 words forward: conditional branch (BOF) to l_329E
00328A:	2015	LDA 	P  	=#15	; A <- #15
00328C:	1583	SBR 	DL 	V_0082	; right byte of [V_0082] <- A
00328E:	0018	LDA 	DL 	V_0018	; A <- [V_0018]
003290:	2BC0	CMP 	P  	=#C0	; compare A with #C0
003292:	C006	BCT 	RP 	l_329E	; 6 words forward: conditional branch (BCT) to l_329E
003294:	C205	BOT 	RP 	l_329E	; 5 words forward: conditional branch (BOT) to l_329E
003296:	2015	LDA 	P  	=#15	; A <- #15
003298:	1584	SBR 	DL 	v_0084	; right byte of [v_0084] <- A
00329A:	2001	LDA 	P  	=#01	; A <- #01
00329C:	170E	ADM 	DL 	PRTS_COUNT	; add A to [PRTS_COUNT]
l_329E:				; branch target; 7 refs; only from untraced words
00329E:	400E	LDA 	DG 	TWB_T3	; A <- [TWB_T3]
0032A0:	C03F	BCT 	RP 	l_331E	; 63 words forward: conditional branch (BCT) to l_331E
0032A2:	0B78	CMP 	DL 	v_0078	; compare A with [v_0078]
0032A4:	C63D	BOF 	RP 	l_331E	; 61 words forward: conditional branch (BOF) to l_331E
0032A6:	061A	SUB 	DL 	V_001A	; A <- A - [V_001A]
0032A8:	050E	ADD 	DL 	PRTS_COUNT	; A <- A + [PRTS_COUNT]
0032AA:	180C	SPA 	DL 	PRTS_PTR	; [PRTS_PTR] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0032AC:	C702	BRU 	RP 	S_32B0	; 2 words forward: CALL subroutine S_32B0 (SPA above saved the return address); returns to l_32AE  [-> S_32B0: 3block move / translate string (MVS/TRS) stores into the caller's TWB, calls S_32FC, returns through its return cell]
l_32AE:				; branch target; 1 ref; only from untraced words
0032AE:	C736	BRU 	RP 	l_331A	; 54 words forward: jump to l_331A

; ===== S_32B0: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: 3block move / translate string (MVS/TRS) stores into the caller's TWB, calls S_32FC, returns through its return cell
S_32B0:				; callee (section / supervisor module / SPA subroutine); 2 refs
0032B0:	2200	LDX 	P  	=#00	; X <- #00
0032B2:	131A	STX 	DL 	V_001A	; [V_001A] <- X
0032B4:	131C	STX 	DL 	V_001C	; [V_001C] <- X
0032B6:	FA04	STR 	P  	=#04	; STR operand: #04
0032B8:	6416	LEA 	IL 	*V_0016	; A <- G+2270  (effective address, memory not read)  (pointer V_0016 = #2270: address = G + #2270, G unknown)
0032BA:	F102	XAE 	P  		; exchange A and E
0032BC:	2BFD	CMP 	P  	=#FD	; compare A with #FD
0032BE:	C207	BOT 	RP 	L_32CC	; 7 words forward: conditional branch (BOT) to L_32CC
0032C0:	26FC	SUB 	P  	=#FC	; A <- A - #FC
0032C2:	111C	STA 	DL 	V_001C	; [V_001C] <- A
0032C4:	20FC	LDA 	P  	=#FC	; A <- #FC
0032C6:	060E	SUB 	DL 	PRTS_COUNT	; A <- A - [PRTS_COUNT]
0032C8:	1716	ADM 	DL 	V_0016	; add A to [V_0016]
0032CA:	20FC	LDA 	P  	=#FC	; A <- #FC
L_32CC:				; branch target; 1 ref
0032CC:	110A	STA 	DL 	CPT_PTR	; [CPT_PTR] <- A
0032CE:	060E	SUB 	DL 	PRTS_COUNT	; A <- A - [PRTS_COUNT]
0032D0:	F102	XAE 	P  		; exchange A and E
0032D2:	020E	LDX 	DL 	PRTS_COUNT	; X <- [PRTS_COUNT]
0032D4:	BF38	MVS 	ILX	*V_0038,X	; MVS operand: [G+0004+X]  (pointer V_0038 = #0004: address = G + #0004, G unknown)
0032D6:	2000	LDA 	P  	=#00	; A <- #00
0032D8:	110E	STA 	DL 	PRTS_COUNT	; [PRTS_COUNT] <- A
0032DA:	020A	LDX 	DL 	CPT_PTR	; X <- [CPT_PTR]
0032DC:	2003	LDA 	P  	=#03	; A <- #03
0032DE:	B538	SBR 	ILX	*V_0038,X	; right byte of [G+0004+X] <- A  (pointer V_0038 = #0004: address = G + #0004, G unknown)
0032E0:	0E11	LBR 	DL 	V_0010	; A <- right byte of [V_0010]
0032E2:	C00A	BCT 	RP 	L_32F6	; 10 words forward: conditional branch (BCT) to L_32F6
0032E4:	020A	LDX 	DL 	CPT_PTR	; X <- [CPT_PTR]
0032E6:	F901	LDR 	P  	=#01	; LDR operand: #01
0032E8:	5116	STA 	DG 	TWB_T7	; [TWB_T7] <- A
0032EA:	0482	LEA 	DL 	V_0082	; A <- V_0082  (effective address, memory not read)
0032EC:	3604	DCL 	DL 	V_0004	; DCL operand: [V_0004]
0032EE:	581E	SPA 	DG 	TWB_N0	; [TWB_N0] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0032F0:	D72C	BRU 	IL 	*V_002C	; CALL subroutine, target not known statically (SPA above saved the return address)  (pointer V_002C = #0986: address = G + #0986, G unknown)
L_32F2:				; branch target; 1 ref
0032F2:	4016	LDA 	DG 	TWB_T7	; A <- [TWB_T7]
0032F4:	FA01	STR 	P  	=#01	; STR operand: #01
L_32F6:				; branch target; 1 ref
0032F6:	1814	SPA 	DL 	V_0014	; [V_0014] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
0032F8:	C702	BRU 	RP 	S_32FC	; 2 words forward: CALL subroutine S_32FC (SPA above saved the return address); returns to L_32FA  [-> S_32FC: block move / translate string (MVS/TRS), privileged I/O or mask ops (CLM/RD/WD/STM), returns through its return cell]
L_32FA:				; branch target; 1 ref
0032FA:	D70C	BRU 	IL 	*PRTS_PTR	; RETURN from subroutine: branch through return cell PRTS_PTR (written by the SPA of the call)

; ===== S_32FC: SUBROUTINE called with SPA + branch; returns by branch through its return cell =====
; routine profile: block move / translate string (MVS/TRS), privileged I/O or mask ops (CLM/RD/WD/STM), returns through its return cell
S_32FC:				; callee (section / supervisor module / SPA subroutine); 2 refs
0032FC:	000A	LDA 	DL 	CPT_PTR	; A <- [CPT_PTR]
0032FE:	2503	ADD 	P  	=#03	; A <- A + #03
003300:	154D	SBR 	DL 	V_004C	; right byte of [V_004C] <- A
003302:	043C	LEA 	DL 	V_003C	; A <- V_003C  (effective address, memory not read)
003304:	2118	LDE 	P  	=#18	; E <- #18
003306:	1F54	MVS 	DL 	V_0054	; MVS operand: [V_0054]
003308:	2000	LDA 	P  	=#00	; A <- #00
00330A:	2D40	LBL 	P  	=#40	; A <- left byte of #40
00330C:	211C	LDE 	P  	=#1C	; E <- #1C
00330E:	F403	WD  	P  		; privileged control instruction (WD)
003310:	2002	LDA 	P  	=#02	; A <- #02
003312:	2D40	LBL 	P  	=#40	; A <- left byte of #40
003314:	211C	LDE 	P  	=#1C	; E <- #1C
003316:	F403	WD  	P  		; privileged control instruction (WD)
003318:	D714	BRU 	IL 	*V_0014	; RETURN from subroutine: branch through return cell V_0014 (written by the SPA of the call)
l_331A:				; branch target; 1 ref; only from untraced words
00331A:	2000	LDA 	P  	=#00	; A <- #00
l_331C:				; branch target; 1 ref; only from untraced words
00331C:	D706	BRU 	IL 	*V_0006	; RETURN from subroutine: branch through return cell V_0006 (written by the SPA of the call)
l_331E:				; branch target; 3 refs; only from untraced words
00331E:	2EBD	LBR 	P  	=#BD	; A <- right byte of #BD
003320:	CF02	BRU 	RM 	l_331C	; 2 words backward: jump to l_331C
L_3322:				; branch target; 1 ref
003322:	212C	LDE 	P  	=#2C	; E <- #2C
003324:	F402	RD  	P  		; privileged control instruction (RD)
003326:	5112	STA 	DG 	TWB_T5	; [TWB_T5] <- A
003328:	6012	LDA 	IL 	*V_0012	; A <- [G+225A]  (pointer V_0012 = #225A: address = G + #225A, G unknown)
00332A:	2901	AND 	P  	=#01	; A <- A and #01
00332C:	D37A	BCF 	IL 	*V_007A	; conditional branch, target not known statically  (pointer V_007A = #22C8: address = G + #22C8, G unknown)
00332E:	4012	LDA 	DG 	TWB_T5	; A <- [TWB_T5]
003330:	097C	AND 	DL 	V_007C	; A <- A and [V_007C]
003332:	C323	BCF 	RP 	L_3378	; 35 words forward: conditional branch (BCF) to L_3378
003334:	2001	LDA 	P  	=#01	; A <- #01
003336:	1708	ADM 	DL 	V_0008	; add A to [V_0008]
003338:	2901	AND 	P  	=#01	; A <- A and #01
00333A:	C313	BCF 	RP 	L_3360	; 19 words forward: conditional branch (BCF) to L_3360
00333C:	0E82	LBR 	DL 	V_0082	; A <- right byte of [V_0082]
00333E:	2B06	CMP 	P  	=#06	; compare A with #06
003340:	C312	BCF 	RP 	L_3364	; 18 words forward: conditional branch (BCF) to L_3364
003342:	2000	LDA 	P  	=#00	; A <- #00
003344:	117E	STA 	DL 	V_007E	; [V_007E] <- A
003346:	001C	LDA 	DL 	V_001C	; A <- [V_001C]
003348:	C004	BCT 	RP 	L_3350	; 4 words forward: conditional branch (BCT) to L_3350
00334A:	180C	SPA 	DL 	PRTS_PTR	; [PRTS_PTR] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
00334C:	CF4E	BRU 	RM 	S_32B0	; 78 words backward: CALL subroutine S_32B0 (SPA above saved the return address); returns to L_334E  [-> S_32B0: 3block move / translate string (MVS/TRS) stores into the caller's TWB, calls S_32FC, returns through its return cell]
L_334E:				; branch target; 1 ref
00334E:	C709	BRU 	RP 	L_3360	; 9 words forward: jump to L_3360
L_3350:				; branch target; 1 ref
003350:	2200	LDX 	P  	=#00	; X <- #00
L_3352:				; branch target; 1 ref
003352:	2000	LDA 	P  	=#00	; A <- #00
003354:	1580	SBR 	DL 	V_0080	; right byte of [V_0080] <- A
003356:	0010	LDA 	DL 	V_0010	; A <- [V_0010]
003358:	0112	LDE 	DL 	V_0012	; E <- [V_0012]
00335A:	5608	DST 	DG 	TWB_T0	; [TWB_T0] <- A,E (2 words)
00335C:	F104	XAX 	P  		; exchange A and X
00335E:	F703	CSV 	P  	=#03	; SUPERVISOR CALL #3 (caller P,L,indicators saved in its TWB; module ends with RSV, resume at the next instruction)  -> entry MON3  [-> MON3: privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, calls S_0798, returns to its CSV caller with RSV]
L_3360:				; branch target; 3 refs
003360:	F401	DIT 	P  		; DE-ACTIVATE INTERRUPT: save outgoing context, dispatch the highest-priority waiting level (not a plain return)

; ===== INT7: INTERRUPT ROUTINE (ends with DIT/DITR: leaves through the interrupt scheduler) =====
; routine profile: comparisons, 5 loops, privileged I/O or mask ops (CLM/RD/WD/STM), stores into the caller's TWB, supervisor calls: #3, calls S_32FC S_32B0, ends with DIT/DITR
INT7:				; entry point; 1 ref
003362:	CF20	BRU 	RM 	L_3322	; 32 words backward: jump to L_3322
L_3364:				; branch target; 1 ref
003364:	297F	AND 	P  	=#7F	; A <- A and #7F
003366:	2B15	CMP 	P  	=#15	; compare A with #15
003368:	C308	BCF 	RP 	L_3378	; 8 words forward: conditional branch (BCF) to L_3378
00336A:	2001	LDA 	P  	=#01	; A <- #01
00336C:	177E	ADM 	DL 	V_007E	; add A to [V_007E]
00336E:	2B04	CMP 	P  	=#04	; compare A with #04
003370:	C004	BCT 	RP 	L_3378	; 4 words forward: conditional branch (BCT) to L_3378
003372:	1814	SPA 	DL 	V_0014	; [V_0014] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
003374:	CF3C	BRU 	RM 	S_32FC	; 60 words backward: CALL subroutine S_32FC (SPA above saved the return address); returns to L_3376  [-> S_32FC: block move / translate string (MVS/TRS), privileged I/O or mask ops (CLM/RD/WD/STM), returns through its return cell]
L_3376:				; branch target; 1 ref
003376:	CF0B	BRU 	RM 	L_3360	; 11 words backward: jump to L_3360
L_3378:				; branch target; 3 refs
003378:	22D0	LDX 	P  	=#D0	; X <- #D0
00337A:	CF14	BRU 	RM 	L_3352	; 20 words backward: jump to L_3352; system structure
; 
; ==============================================
CTX7:				; system structure
; ============================================

00337C:	0014	.WORD	#0014	; CTX level 7: saved status
00337E:	0005	.WORD	#0005	; CTX level 7: saved X
003380:	0028	.WORD	#0028	; CTX level 7: saved E
003382:	EB5C	.WORD	#EB5C	; CTX level 7: saved A
003384:	3006	.WORD	#3006	; CTX level 7: saved G
003386:	3026	.WORD	#3026	; CTX level 7: saved L
003388:	3362	.WORD	#3362	; CTX level 7: saved P (entry of the task / interrupt routine of this level)
00338A:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
00338C:	C705	BRU 	RP 	l_3396	; 5 words forward: jump to l_3396
00338E:	309A	SLCS	DL 	#1A
003390:	3026	SRCS	DL 	#06
003392:	3362	DCX 	DL 	l_0062	; DCX operand: [l_0062]
003394:	3386	DCX 	DL 	v_0086	; DCX operand: [v_0086]
l_3396:				; branch target; 1 ref; only from untraced words
003396:	208E	LDA 	P  	=#8E	; A <- #8E
003398:	2D33	LBL 	P  	=#33	; A <- left byte of #33
00339A:	FA01	STR 	P  	=#01	; STR operand: #01
00339C:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
00339E:	FA19	STR 	P  	=#19	; STR operand: #19
0033A0:	2000	LDA 	P  	=#00	; A <- #00
0033A2:	FA1B	STR 	P  	=#1B	; STR operand: #1B
0033A4:	2D82	LBL 	P  	=#82	; A <- left byte of #82
0033A6:	2121	LDE 	P  	=#21	; E <- #21
0033A8:	F403	WD  	P  		; privileged control instruction (WD)
0033AA:	1002	DLD 	DL 	V_0002	; A,E <- [V_0002] (2 words)
0033AC:	7606	DST 	IL 	*V_0006	; [*V_0006] <- A,E (2 words)
0033AE:	2000	LDA 	P  	=#00	; A <- #00
0033B0:	FA01	STR 	P  	=#01	; STR operand: #01
0033B2:	DF08	BRU 	IG 	*TWB_T0	; RETURN from subroutine: branch through return cell TWB_T0 (written by the SPA of the call)
0033B4:	04DC	LEA 	DL 	V_00DC	; A <- V_00DC  (effective address, memory not read)
0033B6:	04EA	LEA 	DL 	v_00EA	; A <- v_00EA  (effective address, memory not read)
0033B8:	0020	LDA 	DL 	V_0020	; A <- [V_0020]
0033BA:	02E8	LDX 	DL 	V_00E8	; X <- [V_00E8]
0033BC:	0452	LEA 	DL 	V_0052	; A <- V_0052  (effective address, memory not read)
0033BE:	0020	LDA 	DL 	V_0020	; A <- [V_0020]
0033C0:	0000	.ZERO	8	; 8 zero words (padding, reserved area or unused data; never reached as code)
007C00:	CF6F	BRU 	RM 	l_7B22	; 111 words backward: jump to l_7B22
007C02:	0042	LDA 	DL 	v_0042	; A <- [v_0042]
007C04:	1144	STA 	DL 	V_0044	; [V_0044] <- A
l_7C06:				; branch target; 3 refs; only from untraced words
007C06:	2100	LDE 	P  	=#00	; E <- #00
007C08:	CF11	BRU 	RM 	l_7BE6	; 17 words backward: jump to l_7BE6
007C0A:	0042	LDA 	DL 	v_0042	; A <- [v_0042]
007C0C:	1152	STA 	DL 	V_0052	; [V_0052] <- A
007C0E:	1116	STA 	DL 	V_0016	; [V_0016] <- A
007C10:	2000	LDA 	P  	=#00	; A <- #00
007C12:	1126	STA 	DL 	V_0026	; [V_0026] <- A
007C14:	0066	LDA 	DL 	L_0066	; A <- [L_0066]
007C16:	1146	STA 	DL 	V_0046	; [V_0046] <- A
007C18:	0132	LDE 	DL 	V_0032	; E <- [V_0032]
007C1A:	2200	LDX 	P  	=#00	; X <- #00
007C1C:	F803	CLS 	P  	=#03	; CALL SECTION #3 (CLS; caller P,L saved in the callee LDS; return = RTS)  (SRD not resolved: give -G, or PRT not in image)
007C1E:	2080	LDA 	P  	=#80	; A <- #80
007C20:	1526	SBR 	DL 	V_0026	; right byte of [V_0026] <- A
007C22:	CF0E	BRU 	RM 	l_7C06	; 14 words backward: jump to l_7C06
007C24:	184E	SPA 	DL 	V_004E	; [V_004E] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
007C26:	C716	BRU 	RP 	s_7C52	; 22 words forward: CALL subroutine s_7C52 (SPA above saved the return address); returns to l_7C28
l_7C28:				; branch target; 1 ref; only from untraced words
007C28:	CF11	BRU 	RM 	l_7C06	; 17 words backward: jump to l_7C06
007C2A:	184E	SPA 	DL 	V_004E	; [V_004E] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
007C2C:	C702	BRU 	RP 	s_7C30	; 2 words forward: CALL subroutine s_7C30 (SPA above saved the return address); returns to l_7C2E
l_7C2E:				; branch target; 1 ref; only from untraced words
007C2E:	CF14	BRU 	RM 	l_7C06	; 20 words backward: jump to l_7C06
s_7C30:				; callee (section / supervisor module / SPA subroutine); 1 ref; only from untraced words
007C30:	0044	LDA 	DL 	V_0044	; A <- [V_0044]
007C32:	0142	LDE 	DL 	v_0042	; E <- [v_0042]
007C34:	0246	LDX 	DL 	V_0046	; X <- [V_0046]
007C36:	C004	BCT 	RP 	l_7C3E	; 4 words forward: conditional branch (BCT) to l_7C3E
007C38:	29FF	AND 	P  	=#FF	; A <- A and #FF
007C3A:	F104	XAX 	P  		; exchange A and X
007C3C:	C707	BRU 	RP 	l_7C4A	; 7 words forward: jump to l_7C4A
l_7C3E:				; branch target; 1 ref; only from untraced words
007C3E:	0244	LDX 	DL 	V_0044	; X <- [V_0044]
007C40:	C405	BAN 	RP 	l_7C4A	; 5 words forward: conditional branch (BAN) to l_7C4A
007C42:	0B06	CMP 	DL 	V_0006	; compare A with [V_0006]
007C44:	C603	BOF 	RP 	l_7C4A	; 3 words forward: conditional branch (BOF) to l_7C4A
007C46:	B204	STE 	ILX	*V_0004,X	; [G+0000+X] <- E  (pointer V_0004 = #0000: address = G + #0000, G unknown)
007C48:	C702	BRU 	RP 	l_7C4C	; 2 words forward: jump to l_7C4C
l_7C4A:				; branch target; 3 refs; only from untraced words
007C4A:	B246	STE 	ILX	*V_0046,X	; [G+0000+X] <- E  (pointer V_0046 = #0000: address = G + #0000, G unknown)
l_7C4C:				; branch target; 1 ref; only from untraced words
007C4C:	2002	LDA 	P  	=#02	; A <- #02
007C4E:	1744	ADM 	DL 	V_0044	; add A to [V_0044]
007C50:	D74E	BRU 	IL 	*V_004E	; jump to G+#0000 (G unknown)  (pointer V_004E = #0000: address = G + #0000, G unknown)
s_7C52:				; callee (section / supervisor module / SPA subroutine); 1 ref; only from untraced words
007C52:	0052	LDA 	DL 	V_0052	; A <- [V_0052]
007C54:	1116	STA 	DL 	V_0016	; [V_0016] <- A
007C56:	0046	LDA 	DL 	V_0046	; A <- [V_0046]
007C58:	D54E	BAZ 	IL 	*V_004E	; conditional branch, target not known statically  (pointer V_004E = #0000: address = G + #0000, G unknown)
007C5A:	0132	LDE 	DL 	V_0032	; E <- [V_0032]
007C5C:	2200	LDX 	P  	=#00	; X <- #00
007C5E:	1344	STX 	DL 	V_0044	; [V_0044] <- X
007C60:	1346	STX 	DL 	V_0046	; [V_0046] <- X
007C62:	F803	CLS 	P  	=#03	; CALL SECTION #3 (CLS; caller P,L saved in the callee LDS; return = RTS)  (SRD not resolved: give -G, or PRT not in image)
007C64:	D74E	BRU 	IL 	*V_004E	; jump to G+#0000 (G unknown)  (pointer V_004E = #0000: address = G + #0000, G unknown)
007C66:	005A	LDA 	DL 	V_005A	; A <- [V_005A]
007C68:	2108	LDE 	P  	=#08	; E <- #08
007C6A:	22A2	LDX 	P  	=#A2	; X <- #A2
007C6C:	F803	CLS 	P  	=#03	; CALL SECTION #3 (CLS; caller P,L saved in the callee LDS; return = RTS)  (SRD not resolved: give -G, or PRT not in image)
007C6E:	0206	LDX 	DL 	V_0006	; X <- [V_0006]
007C70:	F302	DCX 	P  	=#02	; DCX operand: #02
l_7C72:				; branch target; 1 ref; only from untraced words
007C72:	A004	LDA 	ILX	*V_0004,X	; A <- [G+0000+X]  (pointer V_0004 = #0000: address = G + #0000, G unknown)
007C74:	B130	STA 	ILX	*V_0030,X	; [G+0000+X] <- A  (pointer V_0030 = #0000: address = G + #0000, G unknown)
007C76:	F302	DCX 	P  	=#02	; DCX operand: #02
007C78:	C803	BCT 	RM 	l_7C72	; 3 words backward: conditional branch (BCT) to l_7C72
007C7A:	F802	CLS 	P  	=#02	; CALL SECTION #2 (CLS; caller P,L saved in the callee LDS; return = RTS)  (SRD not resolved: give -G, or PRT not in image)
007C7C:	0E1A	LBR 	DL 	V_001A	; A <- right byte of [V_001A]
007C7E:	C304	BCF 	RP 	l_7C86	; 4 words forward: conditional branch (BCF) to l_7C86
007C80:	0154	LDE 	DL 	V_0054	; E <- [V_0054]
007C82:	FC81	SRLD	P  	#01
007C84:	FB01	LDP 	P  	=#01	; LDP operand: #01
l_7C86:				; branch target; 1 ref; only from untraced words
007C86:	2240	LDX 	P  	=#40	; X <- #40
007C88:	016A	LDE 	DL 	V_006A	; E <- [V_006A]
007C8A:	B256	STE 	ILX	*V_0056,X	; [G+08D8+X] <- E  (pointer V_0056 = #08D8: address = G + #08D8, G unknown)
007C8C:	2040	LDA 	P  	=#40	; A <- #40
007C8E:	FA08	STR 	P  	=#08	; STR operand: #08
007C90:	2121	LDE 	P  	=#21	; E <- #21
007C92:	2004	LDA 	P  	=#04	; A <- #04
007C94:	2DA0	LBL 	P  	=#A0	; A <- left byte of #A0
007C96:	F403	WD  	P  		; privileged control instruction (WD)
007C98:	FA03	STR 	P  	=#03	; STR operand: #03
007C9A:	F401	DIT 	P  		; DE-ACTIVATE INTERRUPT: save outgoing context, dispatch the highest-priority waiting level (not a plain return)
007C9C:	F100	RTS 	P  		; RETURN SECTION: restore L and P from the first 2 words of the current LDS, resume after the CLS
007C9E:	0015	LDA 	DL 	V_0014	; A <- [V_0014]
007CA0:	0040	LDA 	DL 	v_0040	; A <- [v_0040]
007CA2:	0021	LDA 	DL 	V_0020	; A <- [V_0020]
007CA4:	A004	LDA 	ILX	*V_0004,X	; A <- [G+0000+X]  (pointer V_0004 = #0000: address = G + #0000, G unknown)
007CA6:	787A	SPA 	IL 	*V_007A	; [G+22C8] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)  (pointer V_007A = #22C8: address = G + #22C8, G unknown)
007CA8:	787A	SPA 	IL 	*V_007A	; [G+22C8] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)  (pointer V_007A = #22C8: address = G + #22C8, G unknown)
007CAA:	7C9C	FMU 	IL 	*V_009C	; floating multiply by [G+0234]  (pointer V_009C = #0234: address = G + #0234, G unknown)
007CAC:	0300	EOR 	DL 	Unknow_1	; A <- A xor [Unknow_1]
007CAE:	7C6C	FMU 	IL 	*v_006C	; floating multiply by [G+03C6]  (pointer v_006C = #03C6: address = G + #03C6, G unknown)
007CB0:	787A	SPA 	IL 	*V_007A	; [G+22C8] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)  (pointer V_007A = #22C8: address = G + #22C8, G unknown)
007CB2:	00A2	LDA 	DL 	v_00A2	; A <- [v_00A2]
007CB4:	0000	.ZERO	2	; 2 zero words (padding, reserved area or unused data; never reached as code)
007CB8:	00C2	LDA 	DL 	v_00C2	; A <- [v_00C2]
007CBA:	00C4	LDA 	DL 	v_00C4	; A <- [v_00C4]
007CBC:	0008	LDA 	DL 	V_0008	; A <- [V_0008]
007CBE:	0010	LDA 	DL 	V_0010	; A <- [V_0010]
007CC0:	007E	LDA 	DL 	V_007E	; A <- [V_007E]
007CC2:	0034	LDA 	DL 	V_0034	; A <- [V_0034]
007CC4:	0042	LDA 	DL 	v_0042	; A <- [v_0042]
007CC6:	003E	LDA 	DL 	V_003E	; A <- [V_003E]
007CC8:	0040	LDA 	DL 	v_0040	; A <- [v_0040]
007CCA:	7D19	FDV 	IL 	*V_0018	; floating divide by [G+2294]  (pointer V_0018 = #2294: address = G + #2294, G unknown)
007CCC:	7CD2	FMU 	IL 	*V_00D2	; floating multiply by [G+0326]  (pointer V_00D2 = #0326: address = G + #0326, G unknown)
007CCE:	7CEC	FMU 	IL 	*v_00EC	; floating multiply by [G+23C0]  (pointer v_00EC = #23C0: address = G + #23C0, G unknown)
007CD0:	00C0	LDA 	DL 	v_00C0	; A <- [v_00C0]
007CD2:	0014	LDA 	DL 	V_0014	; A <- [V_0014]
007CD4:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
007CD6:	.EBCDIC	"C/DUMPM/"	; 4 words, raw: C361 C4E4 D4D7 D461
007CDE:	0D00	LBL 	DL 	Unknow_1	; A <- left byte of [Unknow_1]
007CE0:	0000	.ZERO	8	; 8 zero words (padding, reserved area or unused data; never reached as code)
007D20:	0000	.ZERO	3	; 3 zero words (padding, reserved area or unused data; never reached as code)
007D26:	.EBCDIC	"  33B4 33BC 38D2"	; 8 words, raw: 4040 F3F3 C2F4 40F3 F3C2 C340 F3F8 C4F2
007D36:	40CE	LDA 	DG 	G+00CE	; A <- [G+00CE]
007D38:	0000	.ZERO	12	; 12 zero words (padding, reserved area or unused data; never reached as code)
007E20:	0000	.ZERO	3	; 3 zero words (padding, reserved area or unused data; never reached as code)
007E26:	0D00	LBL 	DL 	Unknow_1	; A <- left byte of [Unknow_1]
007E28:	0000	.ZERO	5	; 5 zero words (padding, reserved area or unused data; never reached as code)
l_7E32:				; branch target; 1 ref; only from untraced words
007E32:	0000	.ZERO	7	; 7 zero words (padding, reserved area or unused data; never reached as code)
007E60:	0000	.WORD	#0000	; zero word (padding, reserved area or unused data; never reached as code)
l_7E62:				; branch target; 1 ref; only from untraced words
007E62:	0000	.ZERO	6	; 6 zero words (padding, reserved area or unused data; never reached as code)
007E6E:	.EBCDIC	"  33B4 3"	; 4 words, raw: 4040 F3F3 C2F4 40F3
007E76:	AE60	LBR 	ILX	*v_0060,X	; A <- right byte of [G+03C2+X]  (pointer v_0060 = #03C2: address = G + #03C2, G unknown)
007E78:	F112	AEE 	P  	
007E7A:	1106	STA 	DL 	V_0006	; [V_0006] <- A
007E7C:	C003	BCT 	RP 	l_7E82	; 3 words forward: conditional branch (BCT) to l_7E82
007E7E:	2B82	CMP 	P  	=#82	; compare A with #82
007E80:	C332	BCF 	RP 	l_7EE4	; 50 words forward: conditional branch (BCF) to l_7EE4
l_7E82:				; branch target; 1 ref; only from untraced words
007E82:	4E25	LBR 	DG 	G+0025	; A <- right byte of [G+0025]
007E84:	C329	BCF 	RP 	l_7ED6	; 41 words forward: conditional branch (BCF) to l_7ED6
007E86:	2204	LDX 	P  	=#04	; X <- #04
007E88:	AE60	LBR 	ILX	*v_0060,X	; A <- right byte of [G+03C2+X]  (pointer v_0060 = #03C2: address = G + #03C2, G unknown)
007E8A:	4F24	LBX 	DG 	G+0024	; LBX operand: [G+0024]
007E8C:	C00E	BCT 	RP 	l_7EA8	; 14 words forward: conditional branch (BCT) to l_7EA8
007E8E:	512E	STA 	DG 	G+002E	; [G+002E] <- A
007E90:	F104	XAX 	P  		; exchange A and X
007E92:	2000	LDA 	P  	=#00	; A <- #00
l_7E94:				; branch target; 1 ref; only from untraced words
007E94:	AD60	LBL 	ILX	*v_0060,X	; A <- left byte of [G+03C2+X]  (pointer v_0060 = #03C2: address = G + #03C2, G unknown)
007E96:	570E	ADM 	DG 	TWB_T3	; add A to [TWB_T3]
007E98:	F104	XAX 	P  		; exchange A and X
007E9A:	2601	SUB 	P  	=#01	; A <- A - #01
007E9C:	C503	BAZ 	RP 	l_7EA2	; 3 words forward: conditional branch (BAZ) to l_7EA2
007E9E:	F104	XAX 	P  		; exchange A and X
007EA0:	CF06	BRU 	RM 	l_7E94	; 6 words backward: jump to l_7E94
l_7EA2:				; branch target; 1 ref; only from untraced words
007EA2:	F104	XAX 	P  		; exchange A and X
007EA4:	C505	BAZ 	RP 	l_7EAE	; 5 words forward: conditional branch (BAZ) to l_7EAE
007EA6:	C721	BRU 	RP 	l_7EE8	; 33 words forward: jump to l_7EE8
l_7EA8:				; branch target; 1 ref; only from untraced words
007EA8:	2604	SUB 	P  	=#04	; A <- A - #04
007EAA:	512E	STA 	DG 	G+002E	; [G+002E] <- A
007EAC:	C715	BRU 	RP 	l_7ED6	; 21 words forward: jump to l_7ED6
l_7EAE:				; branch target; 1 ref; only from untraced words
007EAE:	4E23	LBR 	DG 	G+0023	; A <- right byte of [G+0023]
007EB0:	C513	BAZ 	RP 	l_7ED6	; 19 words forward: conditional branch (BAZ) to l_7ED6
007EB2:	2202	LDX 	P  	=#02	; X <- #02
007EB4:	AE60	LBR 	ILX	*v_0060,X	; A <- right byte of [G+03C2+X]  (pointer v_0060 = #03C2: address = G + #03C2, G unknown)
007EB6:	4B0C	CMP 	DG 	TWB_T2	; compare A with [TWB_T2]
007EB8:	C316	BCF 	RP 	l_7EE4	; 22 words forward: conditional branch (BCF) to l_7EE4
007EBA:	2501	ADD 	P  	=#01	; A <- A + #01
007EBC:	29FF	AND 	P  	=#FF	; A <- A and #FF
007EBE:	510C	STA 	DG 	TWB_T2	; [TWB_T2] <- A
007EC0:	0060	LDA 	DL 	v_0060	; A <- [v_0060]
007EC2:	2505	ADD 	P  	=#05	; A <- A + #05
007EC4:	0260	LDX 	DL 	v_0060	; X <- [v_0060]
007EC6:	F201	ICX 	P  	=#01	; ICX operand: #01
007EC8:	412E	LDE 	DG 	G+002E	; E <- [G+002E]
007ECA:	580A	SPA 	DG 	TWB_T1	; [TWB_T1] <- P+4-G' = return address of the call made by the NEXT branch (SPA idiom)
007ECC:	CF60	BRU 	RM 	s_7E0C	; 96 words backward: CALL subroutine s_7E0C (SPA above saved the return address); returns to l_7ECE
l_7ECE:				; branch target; 1 ref; only from untraced words
007ECE:	402E	LDA 	DG 	G+002E	; A <- [G+002E]
007ED0:	2604	SUB 	P  	=#04	; A <- A - #04
007ED2:	512E	STA 	DG 	G+002E	; [G+002E] <- A
007ED4:	1760	ADM 	DL 	v_0060	; add A to [v_0060]
l_7ED6:				; branch target; 3 refs; only from untraced words
007ED6:	0006	LDA 	DL 	V_0006	; A <- [V_0006]
007ED8:	CB86	BCF 	RM 	l_7DCC	; 134 words backward: conditional branch (BCF) to l_7DCC
007EDA:	000E	LDA 	DL 	PRTS_COUNT	; A <- [PRTS_COUNT]
007EDC:	462E	SUB 	DG 	G+002E	; A <- A - [G+002E]
007EDE:	CD89	BAZ 	RM 	l_7DCC	; 137 words backward: conditional branch (BAZ) to l_7DCC
007EE0:	110E	STA 	DL 	PRTS_COUNT	; [PRTS_COUNT] <- A
007EE2:	CF58	BRU 	RM 	l_7E32	; 88 words backward: jump to l_7E32
l_7EE4:				; branch target; 2 refs; only from untraced words
007EE4:	1078	DLD 	DL 	v_0078	; A,E <- [v_0078] (2 words)
007EE6:	C708	BRU 	RP 	l_7EF6	; 8 words forward: jump to l_7EF6
l_7EE8:				; branch target; 1 ref; only from untraced words
007EE8:	1074	DLD 	DL 	v_0074	; A,E <- [v_0074] (2 words)
007EEA:	C706	BRU 	RP 	l_7EF6	; 6 words forward: jump to l_7EF6
l_7EEC:				; branch target; 1 ref; only from untraced words
007EEC:	0170	LDE 	DL 	v_0070	; E <- [v_0070]
007EEE:	4026	LDA 	DG 	G+0026	; A <- [G+0026]
007EF0:	C402	BAN 	RP 	l_7EF4	; 2 words forward: conditional branch (BAN) to l_7EF4
007EF2:	0172	LDE 	DL 	v_0072	; E <- [v_0072]
l_7EF4:				; branch target; 1 ref; only from untraced words
007EF4:	2D20	LBL 	P  	=#20	; A <- left byte of #20
l_7EF6:				; branch target; 2 refs; only from untraced words
007EF6:	1670	DST 	DL 	v_0070	; [v_0070] <- A,E (2 words)
007EF8:	2000	LDA 	P  	=#00	; A <- #00
007EFA:	1104	STA 	DL 	V_0004	; [V_0004] <- A
007EFC:	20A2	LDA 	P  	=#A2	; A <- #A2
007EFE:	1136	STA 	DL 	V_0036	; [V_0036] <- A
007F00:	2107	LDE 	P  	=#07	; E <- #07
007F02:	021C	LDX 	DL 	V_001C	; X <- [V_001C]
007F04:	CF5B	BRU 	RM 	l_7E4E	; 91 words backward: jump to l_7E4E
l_7F06:				; branch target; 1 ref; only from untraced words
007F06:	003A	LDA 	DL 	V_003A	; A <- [V_003A]
007F08:	FA09	STR 	P  	=#09	; STR operand: #09
007F0A:	0038	LDA 	DL 	V_0038	; A <- [V_0038]
007F0C:	FA0A	STR 	P  	=#0A	; STR operand: #0A
007F0E:	003C	LDA 	DL 	V_003C	; A <- [V_003C]
007F10:	FA0B	STR 	P  	=#0B	; STR operand: #0B
007F12:	1034	DLD 	DL 	V_0034	; A,E <- [V_0034] (2 words)
007F14:	F403	WD  	P  		; privileged control instruction (WD)
l_7F16:				; branch target; 1 ref; only from untraced words
007F16:	0008	LDA 	DL 	V_0008	; A <- [V_0008]
007F18:	CD5B	BAZ 	RM 	l_7E62	; 91 words backward: conditional branch (BAZ) to l_7E62
007F1A:	CC0A	BAN 	RM 	l_7F06	; 10 words backward: conditional branch (BAN) to l_7F06
007F1C:	CF03	BRU 	RM 	l_7F16	; 3 words backward: jump to l_7F16
l_7F1E:				; branch target; 1 ref; only from untraced words
007F1E:	2110	LDE 	P  	=#10	; E <- #10
007F20:	F402	RD  	P  		; privileged control instruction (RD)
007F22:	C402	BAN 	RP 	l_7F26	; 2 words forward: conditional branch (BAN) to l_7F26
007F24:	2000	LDA 	P  	=#00	; A <- #00
l_7F26:				; branch target; 1 ref; only from untraced words
007F26:	1108	STA 	DL 	V_0008	; [V_0008] <- A
007F28:	2101	LDE 	P  	=#01	; E <- #01
007F2A:	20A0	LDA 	P  	=#A0	; A <- #A0
007F2C:	F403	WD  	P  		; privileged control instruction (WD)
007F2E:	2002	LDA 	P  	=#02	; A <- #02
007F30:	FA08	STR 	P  	=#08	; STR operand: #08
007F32:	F401	DIT 	P  		; DE-ACTIVATE INTERRUPT: save outgoing context, dispatch the highest-priority waiting level (not a plain return)
007F34:	CF0B	BRU 	RM 	l_7F1E	; 11 words backward: jump to l_7F1E
007F36:	004C	LDA 	DL 	V_004C	; A <- [V_004C]
007F38:	7110	STA 	IL 	*V_0010	; [G+0000] <- A  (pointer V_0010 = #0000: address = G + #0000, G unknown)
007F3A:	2010	LDA 	P  	=#10	; A <- #10
007F3C:	4F2B	LBX 	DG 	G+002B	; LBX operand: [G+002B]
007F3E:	B112	STA 	ILX	*V_0012,X	; [G+225A+X] <- A  (pointer V_0012 = #225A: address = G + #225A, G unknown)
007F40:	4E2B	LBR 	DG 	G+002B	; A <- right byte of [G+002B]
007F42:	F005	SLLS	P  	#05
007F44:	2111	LDE 	P  	=#11	; E <- #11
l_7F46:				; branch target; 1 ref; only from untraced words
007F46:	F403	WD  	P  		; privileged control instruction (WD)
007F48:	0004	LDA 	DL 	V_0004	; A <- [V_0004]
007F4A:	C505	BAZ 	RP 	l_7F54	; 5 words forward: conditional branch (BAZ) to l_7F54
007F4C:	4028	LDA 	DG 	G+0028	; A <- [G+0028]
007F4E:	C303	BCF 	RP 	l_7F54	; 3 words forward: conditional branch (BCF) to l_7F54
007F50:	4E2B	LBR 	DG 	G+002B	; A <- right byte of [G+002B]
007F52:	C313	BCF 	RP 	l_7F78	; 19 words forward: conditional branch (BCF) to l_7F78
l_7F54:				; branch target; 3 refs; only from untraced words
007F54:	0006	LDA 	DL 	V_0006	; A <- [V_0006]
007F56:	CDCB	BAZ 	RM 	l_7DC0	; 203 words backward: conditional branch (BAZ) to l_7DC0
007F58:	CC36	BAN 	RM 	l_7EEC	; 54 words backward: conditional branch (BAN) to l_7EEC
007F5A:	0D07	LBL 	DL 	V_0006	; A <- left byte of [V_0006]
007F5C:	C402	BAN 	RP 	l_7F60	; 2 words forward: conditional branch (BAN) to l_7F60
007F5E:	CF05	BRU 	RM 	l_7F54	; 5 words backward: jump to l_7F54
l_7F60:				; branch target; 1 ref; only from untraced words
007F60:	2001	LDA 	P  	=#01	; A <- #01
007F62:	1106	STA 	DL 	V_0006	; [V_0006] <- A
007F64:	C719	BRU 	RP 	l_7F96	; 25 words forward: jump to l_7F96
l_7F66:				; branch target; 3 refs; only from untraced words
007F66:	2D80	LBL 	P  	=#80	; A <- left byte of #80
l_7F68:				; branch target; 2 refs; only from untraced words
007F68:	1106	STA 	DL 	V_0006	; [V_0006] <- A
007F6A:	2000	LDA 	P  	=#00	; A <- #00
007F6C:	2100	LDE 	P  	=#00	; E <- #00
007F6E:	7618	DST 	IL 	*V_0018	; [G+2294] <- A,E (2 words)  (pointer V_0018 = #2294: address = G + #2294, G unknown)
007F70:	7110	STA 	IL 	*V_0010	; [G+0000] <- A  (pointer V_0010 = #0000: address = G + #0000, G unknown)
007F72:	2010	LDA 	P  	=#10	; A <- #10
007F74:	4F2B	LBX 	DG 	G+002B	; LBX operand: [G+002B]
007F76:	B112	STA 	ILX	*V_0012,X	; [G+225A+X] <- A  (pointer V_0012 = #225A: address = G + #225A, G unknown)
l_7F78:				; branch target; 1 ref; only from untraced words
007F78:	2002	LDA 	P  	=#02	; A <- #02
007F7A:	FA08	STR 	P  	=#08	; STR operand: #08
007F7C:	F401	DIT 	P  		; DE-ACTIVATE INTERRUPT: save outgoing context, dispatch the highest-priority waiting level (not a plain return)
007F7E:	4F2B	LBX 	DG 	G+002B	; LBX operand: [G+002B]
007F80:	C00A	BCT 	RP 	l_7F94	; 10 words forward: conditional branch (BCT) to l_7F94
l_7F82:				; branch target; 1 ref; only from untraced words
007F82:	ED7E	TES 	PX 	(X)	; TES operand: [(X)]
007F84:	CD01	BAZ 	RM 	l_7F82	; 1 word backward: conditional branch (BAZ) to l_7F82
007F86:	6018	LDA 	IL 	*V_0018	; A <- [G+2294]  (pointer V_0018 = #2294: address = G + #2294, G unknown)
007F88:	297E	AND 	P  	=#7E	; A <- A and #7E
007F8A:	CB12	BCF 	RM 	l_7F66	; 18 words backward: conditional branch (BCF) to l_7F66
007F8C:	601A	LDA 	IL 	*V_001A	; A <- [G+22A6]  (pointer V_001A = #22A6: address = G + #22A6, G unknown)
007F8E:	F108	XAA 	P  	
007F90:	293F	AND 	P  	=#3F	; A <- A and #3F
007F92:	CB16	BCF 	RM 	l_7F66	; 22 words backward: conditional branch (BCF) to l_7F66
l_7F94:				; branch target; 1 ref; only from untraced words
007F94:	C70D	BRU 	RP 	l_7FAE	; 13 words forward: jump to l_7FAE
l_7F96:				; branch target; 1 ref; only from untraced words
007F96:	0060	LDA 	DL 	v_0060	; A <- [v_0060]
007F98:	FA39	STR 	P  	=#39	; STR operand: #39
007F9A:	0062	LDA 	DL 	l_0062	; A <- [l_0062]
007F9C:	FA3A	STR 	P  	=#3A	; STR operand: #3A
007F9E:	0066	LDA 	DL 	L_0066	; A <- [L_0066]
007FA0:	FA3C	STR 	P  	=#3C	; STR operand: #3C
007FA2:	005A	LDA 	DL 	V_005A	; A <- [V_005A]
007FA4:	2105	LDE 	P  	=#05	; E <- #05
007FA6:	F403	WD  	P  		; privileged control instruction (WD)
007FA8:	0158	LDE 	DL 	v_0058	; E <- [v_0058]
007FAA:	0066	LDA 	DL 	L_0066	; A <- [L_0066]
007FAC:	CF33	BRU 	RM 	l_7F46	; 51 words backward: jump to l_7F46
l_7FAE:				; branch target; 1 ref; only from untraced words
007FAE:	2115	LDE 	P  	=#15	; E <- #15
007FB0:	F402	RD  	P  		; privileged control instruction (RD)
007FB2:	F003	SLLS	P  	#03
007FB4:	C40A	BAN 	RP 	l_7FC8	; 10 words forward: conditional branch (BAN) to l_7FC8
007FB6:	F93C	LDR 	P  	=#3C	; LDR operand: #3C
007FB8:	1166	STA 	DL 	L_0066	; [L_0066] <- A
007FBA:	293E	AND 	P  	=#3E	; A <- A and #3E
007FBC:	2B2E	CMP 	P  	=#2E	; compare A with #2E
007FBE:	C303	BCF 	RP 	l_7FC4	; 3 words forward: conditional branch (BCF) to l_7FC4
007FC0:	2040	LDA 	P  	=#40	; A <- #40
007FC2:	1766	ADM 	DL 	L_0066	; add A to [L_0066]
l_7FC4:				; branch target; 1 ref; only from untraced words
007FC4:	2000	LDA 	P  	=#00	; A <- #00
007FC6:	CF2F	BRU 	RM 	l_7F68	; 47 words backward: jump to l_7F68
l_7FC8:				; branch target; 1 ref; only from untraced words
007FC8:	F089	SLCS	P  	#09
007FCA:	C403	BAN 	RP 	l_7FD0	; 3 words forward: conditional branch (BAN) to l_7FD0
007FCC:	2904	AND 	P  	=#04	; A <- A and #04
007FCE:	CB34	BCF 	RM 	l_7F66	; 52 words backward: conditional branch (BCF) to l_7F66
l_7FD0:				; branch target; 1 ref; only from untraced words
007FD0:	2080	LDA 	P  	=#80	; A <- #80
007FD2:	2105	LDE 	P  	=#05	; E <- #05
007FD4:	F403	WD  	P  		; privileged control instruction (WD)
007FD6:	CF37	BRU 	RM 	l_7F68	; 55 words backward: jump to l_7F68
007FD8:	C718	BRU 	RP 	l_8008	; 24 words forward: jump to l_8008
007FDA:	4F2B	LBX 	DG 	G+002B	; LBX operand: [G+002B]
l_7FDC:				; branch target; 1 ref; only from untraced words
007FDC:	ED7E	TES 	PX 	(X)	; TES operand: [(X)]
007FDE:	CD01	BAZ 	RM 	l_7FDC	; 1 word backward: conditional branch (BAZ) to l_7FDC
007FE0:	0004	LDA 	DL 	V_0004	; A <- [V_0004]
007FE2:	C308	BCF 	RP 	l_7FF2	; 8 words forward: conditional branch (BCF) to l_7FF2
007FE4:	2202	LDX 	P  	=#02	; X <- #02
007FE6:	045E	LEA 	DL 	V_005E	; A <- V_005E  (effective address, memory not read)
007FE8:	210E	LDE 	P  	=#0E	; E <- #0E
007FEA:	BF10	MVS 	ILX	*V_0010,X	; MVS operand: [G+0000+X]  (pointer V_0010 = #0000: address = G + #0000, G unknown)
007FEC:	2204	LDX 	P  	=#04	; X <- #04
007FEE:	0156	LDE 	DL 	V_0056	; E <- [V_0056]
007FF0:	B214	STE 	ILX	*V_0014,X	; [*V_0014+X] <- E
l_7FF2:				; branch target; 1 ref; only from untraced words
007FF2:	1058	DLD 	DL 	v_0058	; A,E <- [v_0058] (2 words)
007FF4:	0204	LDX 	DL 	V_0004	; X <- [V_0004]
007FF6:	C101	BRX 	RP 	$+X	; INDEXED JUMP (BRX): X selects one of the 0 BRU entries that follow (jump / dispatch table)
007FF8:	2105	LDE 	P  	=#05	; E <- #05
007FFA:	C705	BRU 	RP 	l_8004	; 5 words forward: jump to l_8004
007FFC:	0066	LDA 	DL 	L_0066	; A <- [L_0066]
007FFE:	C703	BRU 	RP 	l_8004	; 3 words forward: jump to l_8004

; ============================
; M:DLAY INITIATING A DELAY
; * Call sequence:
; LEA CBD
; CSV M:DLAY
; At the time of CSV execution, register A must contain the address of the Delay Control Block (CBD) relative to G, with the CBD located in the program's CDS or LDS. A CBD must always be aligned on an even-word address.
; 
; * Function:
; Execution of the delay request
;
; Output data
; A: CBD address relative to G
; X:
;	0 Delay taken into account
;	1 Delay not taken into account
; A delay is not taken into account when:
; - the T bit in the flag byte is zero
; - the delay value is outside the permitted limits
; - the delay table is full.
; * TWB memories used: T0 to N0
;
; CBD description:
; - Byte 0: Event byte
; Bit 0:
; = 1: A delay associated with an M:DLAY using this CBD is in progress.
; The event (end of delay) is enabled.
; = 0: The delay has ended. The event (end of delay) is enabled.
; Bits 1 to 7: Rank of the disabled program waiting for the event.
; - Byte 1: Flags
; Bits 0 to 2: Zero
; Bit 3:
; = 1: Delay requested
; = 0: Delay not requested.
; Bit 4:
; = 1: Interrupt requested at end of delay
; = 0: No interrupt requested
; Bits 5 to 7: Zero
; - Bytes 2 and 3: Zero
; - Bytes 4 and 5: Delay value
; Bit 0:
; 0 = Scale 1 (time unit = 100 ms)
; 1 = Scale 2 (time unit = 10 s)
; Bits 1 to 15: Delay value in time units
; - Byte 6: Reserved
; - Byte 7: Interrupt level to be activated at end of delay
: * Called moduled: M:ZDLY
; ============================
; ============================
; M:ZDLY INITIATING A DELAY, CB IN COMMON AREA
; * Call sequence:
; LDA ACBZC (Control block address)
; CSV M:ZDLY
; 
; At the time of CSV execution, register A must contain the address of the start of the delay control block's common area.
; 
; * Function:
; Execution of the delay request (see Chapter IV-10).
; 
; * CB (Delay Control Block) description:
; The CB used in the common area is identical to the CB used by M:DLAY.
; 
; * Output information:
; Identical to the information provided by M:DLAY, except that A contains the address of the CE relative to the common area.
; 
; * TWB memory locations used: TO to NO:
; ============================
; ============================
; M:SDLY DELAY CANCELLATION
* Call sequence:
LEA CB
CSV M:SDLY
At the time of CSV execution, register A must contain the address (relative to C) of the delay control block to be cancelled (requested via M:DLAY).

* Function:
Cancels the event associated with the specified CB.

* Output data:
A = 0: Cancellation successful
A = 1: Invalid request
•TWB memory locations used: T0 to N0
•Called modules: M:ZSDL
; ============================
; ============================
; M:ZSDL DELAY CANCELLATION, CB IN COMMON AREA
; * Call sequence:
; LDA ACBZC (Control block address)
; CSV M:ZSDL
; 
; At the time of CSV execution, register A must contain the address of the start of the delay control block's common area.
; 
; * Function:
Cancels the event associated with the specified CB.

* Output data:
A = 0: Cancellation successful
A = 1: Invalid request
* TWB memory locations used: T0 to N0
;
; ============================
; ============================
; M:TIME TIME REQUEST
* Call sequence:
CSV M:TIME

* Function:
Time request. The time returned depends on the initialization performed by the %TIME command.

* Output elements:
TO = Year
X = Day of the year
E = Hour of the day
A = Time within the hour, expressed in multiples of 100 ms.
;
; ============================
; ============================
; M:PM PROTECTION/RELEASE OF A MEMORY AREA
; * Call sequence:
; LDE AFIN
; LDA ADEB
; LDX CODE
; CSV M: PM
; 
; At the time of the CSV execution, register A must contain the start address of the area whose protection bit is to be modified, register E must contain the end address of that area, and register X must contain the code indicating the nature of the operation and the reference point for the addresses.
; 
; Details of X (code)
; Bit 15:
; = 0 Addresses relative to G
; = 1 Addresses relative to the start of the common area
; 
; Bit 14:
; = 0 Release
; = 1 Protection
; 
; * Output parameters:
; A >= 0: Accepted
; A < 0:  Rejected (Call by a level-zero program; Start address > End address)
; 
; * TWB memory locations used:
; TO to NO
; 
; * Called modules:
; M:CMPA
;
; ============================
; M:RQST Reserving a Resource
; * Calling sequence:
; LDA NRES
; CSV M : RQST
; 
; At the time the CSV is executed, register A must contain the number of the resource to be reserved; user resources begin at number 10, while resources 1 through 9 are reserved for the System.
; 
; * Function:
; — Reserving a resource transfers ownership of that resource to the task making the reservation.
; — If the task already owns the resource, the reservation request is simply passed through and has no effect.
; If the resource is not available, the task is placed in a queue.
; — If a task that owns a resource is aborted, that resource will be released; the same applies upon an EXIT (at level 0).
; 
; * Output information:
; Register A is insignificant.
; 
; * TWB memory locations used:
; T0 through N0
; ============================
; ============================
; M:RLSE Releasing a resource
; * Calling sequence:
; LDA NRES
; CSV M: RLSE
; At the time the CSV is executed, register A must contain the number of the resource to be released. User resources begin at number 10; resources 1 through 9 are reserved for the System.
; 
; * Function:
; - Releases a resource that was previously reserved by a task.
; - Releasing a resource that is not currently reserved has no effect.
; 
; * Output information:
; A < 0: The resource was never reserved.
; A >= 0: The release operation was performed.
; 
; * TWB memory locations used:
; T0 through N0
; ============================
; ============================
; M:TAR Resource Status Check and Reservation
; * Calling sequence:
; LDA NRES
; CSV M:TAR
; At the time the CSV is executed, register A must contain the number of the resource whose status is to be checked.
; 
; * Function:
; - Checks the status of the resource (free or occupied).
; - Reserves the resource if it is free.
; 
; * Output information:
; A < 0: Resource occupied.
; A = 0: Reservation successful.
; 
; *TWB memory locations used:
; T0 to N0
; ============================
; ============================
; M:CSOL Activating a Subsystem
; 
; * Call sequence:
; LEA CB
; CSV M:CSOL
; At the time the CSV is executed, register A must contain the address (relative to G) of the caller's Control Block (CB) used for this action.
; 
; * CB format:
; 0: 0
; 2: 0
; 4: AD.BUF
; 6: CO
; 8: XX
; 
; AD.BUF: Address (relative to G) of the buffer where the subsystem wishes to receive operator commands intended for it.
; CO: Number of characters to be transmitted to it.
; XX: Subsystem recognition code (EBCDIC character).
; 
; * Function:
; — This call notifies the console interrupt handler (IT pupitre) that the subsystem (SS) is ready to receive operator commands.
; — Until an SS has performed a CSV M:CSOL, the console interrupt handler will consider it inactive and will reject any MTR-E commands.
; — Following the CSV M:CSOL, the SS may wait for a command by issuing a WAIT or WLST on its CB.
; Note: SS names are entered into the SS name table (within the console interrupt handler's LDS) during system generation.
; 
; * Output parameters:
; A = 0: Request accepted.
; A < 0: Request invalid (unknown SS, or SS already connected at another level).
; X: Address of the buffer in the ZC (common zone) allocated to the calling subsystem.
; 
; * TWB memory areas used:
; T0 through N0
; ============================
; ============================
; M:AFF Retrieving the device connected to a given operational label
;
; * Calling sequence:
; LDA NETI
; CSV M:AFF
; At the time of the CSV execution, register A must contain the number of the operational label for which the assignment is to be determined.
;
; * Function:
; For a given operational label, this module provides its assignment—that is, the EBCDIC name of the device connected to it at a specific moment—as well as its element.
;
; * Output elements:
; A: EBCDIC name of the device
; E: OLT element
; X=0: request successful
; X!=0: request failed; operational label does not exist
; 7
; Note: The caller's level determines whether the search applies to background labels or real-time labels.
;
; * TWB memory locations used:
; T0 through NO
;
; ============================
; ============================
; M:ACTV activation of an event (CB/ZC)

; * Calling sequence:
; LEA CB
; CSV M:ACTV
; At the time of the CSV execution, register A must contain the address of the CB (relative to G) for which the event bit is to be activated.
;
; * Function:
; - Activation of the CB's event bit (bit 0 of byte 0).
; - If a task is waiting, that task is awakened.
; - Activation of a potential secondary ET (event task) if bit 1 of byte 1 is set.
; - Transfer of an error code into the event byte.
;
; * Output elements:
; A = 0: request satisfied
; A < 0: event already activated or invalid secondary 1T
;
; * TWB memory locations used
; T0 through NO
;
; ============================
; ============================
; M:ZACTV - activation of an event (CB/ZC)

; * Calling sequence
; LEA ZCB
; CSV M:ZACT
; At the time of the CSV execution, register A must contain the address—relative to ZC—of the CB for which the event bit is to be activated.
;
; The processing is identical to M:ACTV
; ============================
; ============================
; %ABORT FIXME source unknown
; Aborting the current program
; The current program is aborted and logically removed from the system.
; Trap status word
; The various bits of this word have the following meanings:
; - Bit 15 PG (1: program trap, 0: trap caused by a coupler),
; - Bit 14 ES (Input/Output watchdog timer),
; - Bit 13 II (Non-existent instruction),
; - Bit 12 PA (Memory parity),
; - Bit 11 Al (Non-existent address),
; - Bit 10 PM (Memory protection),
; - Bit 9 VM (Mode violation).
; Multiple bits may be set simultaneously (specifically Al and PA).
; ============================


; --- addresses referenced but outside the loaded image ---
l_7B22	EQU	#7B22	; 1 ref
l_7BE6	EQU	#7BE6	; 1 ref
l_7DC0	EQU	#7DC0	; 1 ref
l_7DCC	EQU	#7DCC	; 2 refs
s_7E0C	EQU	#7E0C	; 1 ref
l_7E4E	EQU	#7E4E	; 1 ref
l_8004	EQU	#8004	; 2 refs
l_8008	EQU	#8008	; 1 ref
