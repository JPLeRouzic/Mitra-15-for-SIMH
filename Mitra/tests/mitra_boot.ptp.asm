jplr@Devuan-JP:~/Documents/2026/Mitra/Disassembler$ ./mitra_disasm_v7.out mitra_boot.ptp.lst 
; ---------------------------------------------------------------------
; Mitra-15 disassembly with symbolic labels
; ALL ADDRESSES ARE BYTE ADDRESSES: a label suffix equals the address column of the listing.
; Prefixes: E_ entry  S_ SPA-called subroutine  L_ branch target  V_ variable  T_ text
;           lower case (l_ v_ ...) = referenced only from code the flow trace did not reach
; Named structures: SEC<n>/LDS<n>/PRT<n> (program sections), MON<n>/PRTS<n> (supervisor),
;           INT<k>/CTX<k>/CPT/DVT (interrupts), TWB_* (monitor-call work block)
; Assumed run-time values (static analysis cannot know them): G=#0000 (not given: G-relative operands shown as TWB_xx / G+xxxx; PRT not located)  L=#0000
;   indirect base G' = G;  data D = bytes;  branch D = words, P = address of the current instruction ($+n)
;   indirect pointers are followed when the cell in the image is non-zero and points inside the image
;   X is unknown: indexed accesses are labelled by their base (label+X)
; * CPT: pointer at &A = #0000, 3 context(s) found: CTX<level> (7 words: status X E A G L P), their P fields are the interrupt routines INT<level> (end with DIT/DITR); DVT = 32 words below the CPT
; ---------------------------------------------------------------------

; ===== CPT: program entry point =====
CPT:				; system structure; entry point; read; byte access; 2 refs
000000:	0000	.WORD	#0000	; CPT[0]: context pointer of interrupt level 0
000002:	0000	.WORD	#0000	; CPT[1]: pointer to the context (CTX) of interrupt level 1
000004:	0000	.WORD	#0000	; CPT[2]: pointer to the context (CTX) of interrupt level 2
000006:	0000	.WORD	#0000	; CPT[3]: pointer to the context (CTX) of interrupt level 3
000008:	0000	.WORD	#0000	; CPT[4]: pointer to the context (CTX) of interrupt level 4
CPT_PTR:				; system structure; read; byte access; 2 refs; only from untraced words
00000A:	0000	.WORD	#0000	; absolute &A: pointer to the Context Pointer Table (CPT)
v_000C:				; read; 1 ref; only from untraced words
00000C:	0000	.WORD	#0000	; CPT[6]: pointer to the context (CTX) of interrupt level 6
00000E:	0000	.WORD	#0000	; CPT[7]: pointer to the context (CTX) of interrupt level 7
000010:	0300	.WORD	#0300	; CPT[8]: pointer to the context (CTX) of interrupt level 8
000012:	0000	.WORD	#0000	; CPT[9]: pointer to the context (CTX) of interrupt level 9
000014:	0000	.WORD	#0000	; CPT[10]: pointer to the context (CTX) of interrupt level 10
000016:	0014	.WORD	#0014	; CPT[11]: pointer to the context (CTX) of interrupt level 11
000018:	0000	.WORD	#0000	; CPT[12]: pointer to the context (CTX) of interrupt level 12
00001A:	0000	.WORD	#0000	; CPT[13]: pointer to the context (CTX) of interrupt level 13
00001C:	0014	.WORD	#0014	; CPT[14]: pointer to the context (CTX) of interrupt level 14
00001E:	0143	.WORD	#0143	; CPT[15]: pointer to the context (CTX) of interrupt level 15
v_0020:				; pointer cell; read; 2 refs; only from untraced words
000020:	007F	.WORD	#007F	; CPT[16]: pointer to the context (CTX) of interrupt level 16
000022:	0100	.WORD	#0100	; CPT[17]: pointer to the context (CTX) of interrupt level 17
000024:	2000	.WORD	#2000	; CPT[18]: pointer to the context (CTX) of interrupt level 18
000026:	0000	.WORD	#0000	; CPT[19]: pointer to the context (CTX) of interrupt level 19
000028:	0000	.WORD	#0000	; CPT[20]: pointer to the context (CTX) of interrupt level 20
00002A:	0000	.WORD	#0000	; CPT[21]: pointer to the context (CTX) of interrupt level 21
00002C:	0000	.WORD	#0000	; CPT[22]: pointer to the context (CTX) of interrupt level 22
00002E:	0000	.WORD	#0000	; CPT[23]: pointer to the context (CTX) of interrupt level 23
000030:	0022	.WORD	#0022	; CPT[24]: pointer to the context (CTX) of interrupt level 24
000032:	F408	.WORD	#F408	; CPT[25]: pointer to the context (CTX) of interrupt level 25
000034:	400E	.WORD	#400E	; CPT[26]: pointer to the context (CTX) of interrupt level 26
000036:	FA09	.WORD	#FA09	; CPT[27]: pointer to the context (CTX) of interrupt level 27
000038:	4010	.WORD	#4010	; CPT[28]: pointer to the context (CTX) of interrupt level 28
00003A:	FA0A	.WORD	#FA0A	; CPT[29]: pointer to the context (CTX) of interrupt level 29
00003C:	2101	.WORD	#2101	; CPT[30]: pointer to the context (CTX) of interrupt level 30
00003E:	2003	.WORD	#2003	; CPT[31]: pointer to the context (CTX) of interrupt level 31
000040:	F403	WD  	P  		; privileged control instruction (WD)
l_0042:				; branch target; 1 ref; only from untraced words
000042:	F909	LDR 	P  	=#09	; LDR operand: #09
000044:	C502	BAZ 	RP 	l_0048	; 2 words forward: conditional branch (BAZ) to l_0048
000046:	CF02	BRU 	RM 	l_0042	; 2 words backward: jump to l_0042
l_0048:				; branch target; 1 ref; only from untraced words
000048:	DF12	BRU 	IG 	*TWB_T5	; jump, target not known statically  (pointer TWB_T5 lives in the calling task's area: its contents are only known at run time)
00004A:	0000	.ZERO	11	; 11 zero words (padding, reserved area or unused data; never reached as code)
000090:	5365	STX 	DG 	G+0065	; [G+0065] <- X
000092:	636F	EOR 	IL 	*v_006E	; A <- A xor [*v_006E]  (pointer v_006E not in image / not followed, target unknown at disassembly time)
000094:	6E64	LBR 	IL 	*v_0064	; A <- right byte of [*v_0064]  (pointer v_0064 not in image / not followed, target unknown at disassembly time)
000096:	6172	LDE 	IL 	*v_0072	; E <- [*v_0072]  (pointer v_0072 not in image / not followed, target unknown at disassembly time)
000098:	7920	STS 	IL 	*v_0020	; STS operand: [G+007F]  (pointer v_0020 = #007F: address = G + #007F, G unknown)
00009A:	626F	LDX 	IL 	*v_006E	; X <- [*v_006E]  (pointer v_006E not in image / not followed, target unknown at disassembly time)
00009C:	6F74	LBX 	IL 	*v_0074	; LBX operand: [*v_0074]  (pointer v_0074 not in image / not followed, target unknown at disassembly time)
00009E:	206C	LDA 	P  	=#6C	; A <- #6C
0000A0:	6973	AND 	IL 	*v_0072	; A <- A and [*v_0072]  (pointer v_0072 not in image / not followed, target unknown at disassembly time)
0000A2:	7465	SBL 	IL 	*v_0064	; left byte of [*v_0064] <- A  (pointer v_0064 not in image / not followed, target unknown at disassembly time)
0000A4:	6E69	LBR 	IL 	*v_0068	; A <- right byte of [*v_0068]  (pointer v_0068 not in image / not followed, target unknown at disassembly time)
0000A6:	6E67	LBR 	IL 	*v_0066	; A <- right byte of [*v_0066]  (pointer v_0066 not in image / not followed, target unknown at disassembly time)
0000A8:	0D0A	LBL 	DL 	CPT_PTR	; A <- left byte of [CPT_PTR]
0000AA:	4475	LEA 	DG 	G+0075	; A <- G+0075  (effective address, memory not read)
0000AC:	6D6D	LBL 	IL 	*v_006C	; A <- left byte of [*v_006C]  (pointer v_006C not in image / not followed, target unknown at disassembly time)
0000AE:	7920	STS 	IL 	*v_0020	; STS operand: [G+007F]  (pointer v_0020 = #007F: address = G + #007F, G unknown)
0000B0:	7375	STX 	IL 	*v_0074	; [*v_0074] <- X  (pointer v_0074 not in image / not followed, target unknown at disassembly time)
0000B2:	6272	LDX 	IL 	*v_0072	; X <- [*v_0072]  (pointer v_0072 not in image / not followed, target unknown at disassembly time)
0000B4:	6F75	LBX 	IL 	*v_0074	; LBX operand: [*v_0074]  (pointer v_0074 not in image / not followed, target unknown at disassembly time)
0000B6:	7469	SBL 	IL 	*v_0068	; left byte of [*v_0068] <- A  (pointer v_0068 not in image / not followed, target unknown at disassembly time)
0000B8:	6E65	LBR 	IL 	*v_0064	; A <- right byte of [*v_0064]  (pointer v_0064 not in image / not followed, target unknown at disassembly time)
0000BA:	2072	LDA 	P  	=#72	; A <- #72
0000BC:	756E	SBR 	IL 	*v_006E	; right byte of [*v_006E] <- A  (pointer v_006E not in image / not followed, target unknown at disassembly time)
0000BE:	6E69	LBR 	IL 	*v_0068	; A <- right byte of [*v_0068]  (pointer v_0068 not in image / not followed, target unknown at disassembly time)
0000C0:	6E67	LBR 	IL 	*v_0066	; A <- right byte of [*v_0066]  (pointer v_0066 not in image / not followed, target unknown at disassembly time)
0000C2:	0D0A	LBL 	DL 	CPT_PTR	; A <- left byte of [CPT_PTR]
0000C4:	5245	STE 	DG 	G+0045	; [G+0045] <- E
0000C6:	5045	DLD 	DG 	G+0045	; A,E <- [G+0045] (2 words)
0000C8:	4154	LDE 	DG 	G+0054	; E <- [G+0054]
0000CA:	474F	IOR 	DG 	G+004F	; A <- A or [G+004F]
0000CC:	0D00	LBL 	DL 	CPT	; A <- left byte of [CPT]
0000CE:	DFDF	BRU 	IG 	*G+00DF	; jump, target not known statically  (pointer G+00DF lives in the calling task's area: its contents are only known at run time)
0000D0:	FF00	??	FF00
0000D2:	00FF	LDA 	DL 	v_00FE	; A <- [v_00FE]
0000D4:	000D	LDA 	DL 	v_000C	; A <- [v_000C]
0000D6:	801F	LDA 	IGX	*TWB_N0+1,X	; A <- [*TWB_N0+1+X]  (pointer TWB_N0+1 lives in the calling task's area: its contents are only known at run time)
0000D8:	0000	.ZERO	12	; 12 zero words (padding, reserved area or unused data; never reached as code)
000110:	F408	STM 	P  		; privileged control instruction (STM)
000112:	2019	LDA 	P  	=#19	; A <- #19
000114:	FA09	STR 	P  	=#09	; STR operand: #09
000116:	207F	LDA 	P  	=#7F	; A <- #7F
000118:	FA0A	STR 	P  	=#0A	; STR operand: #0A
00011A:	F900	LDR 	P  	=#00	; LDR operand: #00
00011C:	2506	ADD 	P  	=#06	; A <- A + #06
00011E:	51C8	STA 	DG 	G+00C8	; [G+00C8] <- A
000120:	C746	BRU 	RP 	l_01AC	; 70 words forward: jump to l_01AC
l_0122:				; branch target; 4 refs; only from untraced words
000122:	2000	LDA 	P  	=#00	; A <- #00
000124:	51E0	STA 	DG 	G+00E0	; [G+00E0] <- A
000126:	51E2	STA 	DG 	G+00E2	; [G+00E2] <- A
000128:	51E4	STA 	DG 	G+00E4	; [G+00E4] <- A
00012A:	51E6	STA 	DG 	G+00E6	; [G+00E6] <- A
00012C:	51E8	STA 	DG 	G+00E8	; [G+00E8] <- A
00012E:	51EA	STA 	DG 	G+00EA	; [G+00EA] <- A
000130:	51EC	STA 	DG 	G+00EC	; [G+00EC] <- A
000132:	51EE	STA 	DG 	G+00EE	; [G+00EE] <- A
000134:	51F0	STA 	DG 	G+00F0	; [G+00F0] <- A
000136:	51F2	STA 	DG 	G+00F2	; [G+00F2] <- A
000138:	51F4	STA 	DG 	G+00F4	; [G+00F4] <- A
00013A:	51F6	STA 	DG 	G+00F6	; [G+00F6] <- A
00013C:	51F8	STA 	DG 	G+00F8	; [G+00F8] <- A
00013E:	51FA	STA 	DG 	G+00FA	; [G+00FA] <- A
000140:	51FC	STA 	DG 	G+00FC	; [G+00FC] <- A
000142:	51FE	STA 	DG 	G+00FE	; [G+00FE] <- A
000144:	40C6	LDA 	DG 	G+00C6	; A <- [G+00C6]
000146:	FA09	STR 	P  	=#09	; STR operand: #09
000148:	20DF	LDA 	P  	=#DF	; A <- #DF
00014A:	FA0A	STR 	P  	=#0A	; STR operand: #0A
00014C:	40BC	LDA 	DG 	G+00BC	; A <- [G+00BC]
00014E:	FA0B	STR 	P  	=#0B	; STR operand: #0B
000150:	2101	LDE 	P  	=#01	; E <- #01
000152:	2001	LDA 	P  	=#01	; A <- #01
000154:	F403	WD  	P  		; privileged control instruction (WD)
l_0156:				; branch target; 1 ref; only from untraced words
000156:	F90B	LDR 	P  	=#0B	; LDR operand: #0B
000158:	49C2	AND 	DG 	G+00C2	; A <- A and [G+00C2]
00015A:	4BC4	CMP 	DG 	G+00C4	; compare A with [G+00C4]
00015C:	C002	BCT 	RP 	l_0160	; 2 words forward: conditional branch (BCT) to l_0160
00015E:	CF04	BRU 	RM 	l_0156	; 4 words backward: jump to l_0156
l_0160:				; branch target; 1 ref; only from untraced words
000160:	40E0	LDA 	DG 	G+00E0	; A <- [G+00E0]
000162:	49BE	AND 	DG 	G+00BE	; A <- A and [G+00BE]
000164:	4BB4	CMP 	DG 	G+00B4	; compare A with [G+00B4]
000166:	C316	BCF 	RP 	l_0192	; 22 words forward: conditional branch (BCF) to l_0192
000168:	40E2	LDA 	DG 	G+00E2	; A <- [G+00E2]
00016A:	49BE	AND 	DG 	G+00BE	; A <- A and [G+00BE]
00016C:	4BB6	CMP 	DG 	G+00B6	; compare A with [G+00B6]
00016E:	C312	BCF 	RP 	l_0192	; 18 words forward: conditional branch (BCF) to l_0192
000170:	40E4	LDA 	DG 	G+00E4	; A <- [G+00E4]
000172:	49BE	AND 	DG 	G+00BE	; A <- A and [G+00BE]
000174:	4BB8	CMP 	DG 	G+00B8	; compare A with [G+00B8]
000176:	C30E	BCF 	RP 	l_0192	; 14 words forward: conditional branch (BCF) to l_0192
000178:	40E6	LDA 	DG 	G+00E6	; A <- [G+00E6]
00017A:	49C0	AND 	DG 	G+00C0	; A <- A and [G+00C0]
00017C:	4BBC	CMP 	DG 	G+00BC	; compare A with [G+00BC]
00017E:	C30A	BCF 	RP 	l_0192	; 10 words forward: conditional branch (BCF) to l_0192
000180:	2019	LDA 	P  	=#19	; A <- #19
000182:	FA09	STR 	P  	=#09	; STR operand: #09
000184:	207F	LDA 	P  	=#7F	; A <- #7F
000186:	FA0A	STR 	P  	=#0A	; STR operand: #0A
000188:	F900	LDR 	P  	=#00	; LDR operand: #00
00018A:	2506	ADD 	P  	=#06	; A <- A + #06
00018C:	51C8	STA 	DG 	G+00C8	; [G+00C8] <- A
00018E:	C70F	BRU 	RP 	l_01AC	; 15 words forward: jump to l_01AC
000190:	CF37	BRU 	RM 	l_0122	; 55 words backward: jump to l_0122
l_0192:				; branch target; 4 refs; only from untraced words
000192:	40E0	LDA 	DG 	G+00E0	; A <- [G+00E0]
000194:	49BE	AND 	DG 	G+00BE	; A <- A and [G+00BE]
000196:	4BBA	CMP 	DG 	G+00BA	; compare A with [G+00BA]
000198:	CB3B	BCF 	RM 	l_0122	; 59 words backward: conditional branch (BCF) to l_0122
00019A:	40E2	LDA 	DG 	G+00E2	; A <- [G+00E2]
00019C:	49C0	AND 	DG 	G+00C0	; A <- A and [G+00C0]
00019E:	4BBC	CMP 	DG 	G+00BC	; compare A with [G+00BC]
0001A0:	CB3F	BCF 	RM 	l_0122	; 63 words backward: conditional branch (BCF) to l_0122
0001A2:	F900	LDR 	P  	=#00	; LDR operand: #00
0001A4:	2506	ADD 	P  	=#06	; A <- A + #06
0001A6:	51CA	STA 	DG 	G+00CA	; [G+00CA] <- A
0001A8:	C709	BRU 	RP 	l_01BA	; 9 words forward: jump to l_01BA
0001AA:	CF44	BRU 	RM 	l_0122	; 68 words backward: jump to l_0122
l_01AC:				; branch target; 3 refs; only from untraced words
0001AC:	2101	LDE 	P  	=#01	; E <- #01
0001AE:	2002	LDA 	P  	=#02	; A <- #02
0001B0:	F403	WD  	P  		; privileged control instruction (WD)
l_01B2:				; branch target; 1 ref; only from untraced words
0001B2:	F909	LDR 	P  	=#09	; LDR operand: #09
0001B4:	C502	BAZ 	RP 	l_01B8	; 2 words forward: conditional branch (BAZ) to l_01B8
0001B6:	CF02	BRU 	RM 	l_01B2	; 2 words backward: jump to l_01B2
l_01B8:				; branch target; 1 ref; only from untraced words
0001B8:	DFC8	BRU 	IG 	*G+00C8	; jump, target not known statically  (pointer G+00C8 lives in the calling task's area: its contents are only known at run time)
l_01BA:				; branch target; 1 ref; only from untraced words
0001BA:	2055	LDA 	P  	=#55	; A <- #55
0001BC:	51CC	STA 	DG 	G+00CC	; [G+00CC] <- A
0001BE:	2019	LDA 	P  	=#19	; A <- #19
0001C0:	FA09	STR 	P  	=#09	; STR operand: #09
0001C2:	2099	LDA 	P  	=#99	; A <- #99
0001C4:	FA0A	STR 	P  	=#0A	; STR operand: #0A
0001C6:	F900	LDR 	P  	=#00	; LDR operand: #00
0001C8:	2506	ADD 	P  	=#06	; A <- A + #06
0001CA:	51C8	STA 	DG 	G+00C8	; [G+00C8] <- A
0001CC:	CF10	BRU 	RM 	l_01AC	; 16 words backward: jump to l_01AC
0001CE:	20AA	LDA 	P  	=#AA	; A <- #AA
0001D0:	51CC	STA 	DG 	G+00CC	; [G+00CC] <- A
0001D2:	DFCA	BRU 	IG 	*G+00CA	; jump, target not known statically  (pointer G+00CA lives in the calling task's area: its contents are only known at run time)

; --- addresses referenced but outside the loaded image ---
v_0064	EQU	#0064	; 3 refs
v_0066	EQU	#0066	; 2 refs
v_0068	EQU	#0068	; 3 refs
v_006C	EQU	#006C	; 1 ref
v_006E	EQU	#006E	; 3 refs
v_0072	EQU	#0072	; 3 refs
v_0074	EQU	#0074	; 3 refs
v_00FE	EQU	#00FE	; 1 ref

