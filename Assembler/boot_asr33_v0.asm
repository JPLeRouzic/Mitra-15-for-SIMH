; ===================================================================
; boot_asr33.asm - tiny Mitra-15 ASR-33 INI bootstrap (128 bytes/64 words)
; Assemble: python3 Mitra-15_assembler_v13.py boot_asr33.asm boot_asr33.bin
; Load:     sim> attach asr33 boot_asr33.bin
;           sim> boot asr33
;
; Memory layout (64 words / 128 bytes, word-indexed 0–63)
; Word(s)		Contents		Purpose
; 0			BRU 0					Level-0 idle loop — "MITRA runs at level zero...awaiting the interrupt"
; 1–15			0 (padding)				unused
; 10			32					CPT base pointer (M[10])
; 16–19			handler code (below)			level-6 (ASR-33) interrupt handler
; 32–63			CPT table (32 words)			only entries 0 and 6 are non-zero; rest = 0 → "no program connected," which cpt_lookup() already treats safely (STOP_ILLVEC)
; word 32 (CPT[0])	48					outgoing/background context save area
; word 38 (CPT[6])	56					incoming context for the ASR-33's level
; 48–54			level-0 save area (7 words, init 0)	filled in on the first interrupt once bug 2 is fixed
; 56–62			level-6 context (7 words, pre-loaded)	ind=0, X=0, E=0, A=0, G=0, L=0, P=16 — this is what makes execution jump to word 16
; 
; ===================================================================

        ORG     $0
IDLE    BRU   HANDLER         ; Level-0 idle loop — "MITRA runs at level zero...awaiting the interrupt"

	ORG $10
CPT_BASE_WORD
        DATA  CPT_LVL0       ; M[10] = &0A : CPT table starts at word index &0A

HANDLER
	LDA   =&80	     ; Transfer 128 byte of secondary boot code
        STR   =&09	     ; Store secondary boot address in R9
	LDA   =&64	     ; Address where to store the secondary boot code
        STR   =&0A	     ; Store secondary boot address in R10
	LDE   =&01           ; select ASR-33 WD channel (E=1)
        LDA   =&04           ; command = STOP / idle the device
        WD                   ; "...places the device in an idle state"
        DIT                  ; de-activate interrupt, return to level 0

CPT_LVL0
        DATA  CTX_LVL0       ; CPT[0] (word &0A+0): -> level-0 ctx area
        RES   &5             ; pad to level 6 interrupt

        DATA  CTX_LVL6       ; CPT[6] (word &0A+&C=&16): -> level-6 ctx area
        RES   &1             ; aesthetic pad

CTX_LVL0
        DATA  &00,&00,&00,&00,&00,&00,&00,&00
                             ; ind,X,E,A,L,P,G for the OUTGOING (level 0)
                             ; context, saved here on the first interrupt
        RES   &1             ; aesthetic pad

CTX_LVL6
        DATA  &00,&00,&00,&00,&00,&00,&10,&00
                             ; ind,X,E,A,L,P,G for the INCOMING (level 6)
                             ; context - P=&10 (16) = HANDLER's word index
        RES   &1D             ; pad out to the full 128-byte/64-word block

        END
