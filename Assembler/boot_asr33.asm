; ===================================================================
; boot_asr33.asm - tiny Mitra-15 ASR-33 INI bootstrap (128 bytes/64 words)
; Assemble: python3 Mitra-15_assembler_v13.py boot_asr33.asm boot_asr33.bin
; Load:     sim> attach asr33 boot_asr33.bin
;           sim> boot asr33
;
; Memory layout (64 words / 128 bytes, word-indexed 0–63)
; Word(s)     Contents            Purpose
; 0           BRU 0               Level-0 idle loop
; 10          CPT Base Pointer    M[10] = &0A : CPT table starts at word index &0A
; 16-19       HANDLER code        Level-6 (ASR-33) interrupt handler
; 32-39       CPT entries         CPT[0] and CPT[6] pointers
; 48-55       CTX_LVL0            Level-0 context save area
; 56-63       CTX_LVL6            Level-6 context (Pre-loaded with P=16 to jump to HANDLER)
; ===================================================================
    ORG     $0
IDLE    BRU   IDLE             ; word 0: level-0 background loop (wait for interrupt)
    RES   &F             ; padding to word 15

    ORG     $10
CPT_BASE_WORD
DATA  CPT_LVL0            ; M[10] = &0A : CPT table starts at word index &0A

    ORG     $16             ; HANDLER starts at word 16 (P=&10 in context)
HANDLER
    ; R9 = words/bytes remaining to transfer
    ; R10 = current destination memory address
    ; R11 = temporary storage for the word read from tape
    
    ; Check if transfer is complete
    LDR   =&09      ; A = R9 (remaining count)
    BAZ   BOOT_DONE ; If R9 == 0, transfer is complete
    
    ; Read a word/byte from ASR-33 Reader
    LDE   =&01      ; E=1 : Select ASR-33 Reader channel
    RD              ; Read Direct: loads data into A register
    
    ; Save the read data into R11 temporarily
    STR   =&0B      ; R11 = A (save the read word)
    
    ; Store the word into the destination memory address
    LDR   =&0A      ; A = R10 (destination address)
    STA   =&40      ; Store destination address in word 64 (temporary pointer)
    LDR   =&0B      ; A = R11 (retrieve the read word)
    STA   @&40      ; Store Indirect Local: write word to memory[word 64]
    
    ; Increment destination address (R10 = R10 + 1)
    LDR   =&0A      ; A = R10
    ADD   =&01      ; A = A + 1
    STR   =&0A      ; R10 = A
    
    ; Decrement remaining count (R9 = R9 - 1)
    LDR   =&09      ; A = R9
    SUB   =&01      ; A = A - 1
    STR   =&09      ; R9 = A
    
    ; De-activate interrupt and return to level 0 (will loop back on next reader interrupt)
    DIT             

BOOT_DONE
    ; Transfer complete: Jump to the loaded secondary bootloader at address 0
    BRU   =&00      ; Branch Unconditional to address 0
    DIT             ; Fallback return just in case

    ORG     $20
CPT_LVL0
DATA  CTX_LVL0       ; CPT[0] (word &0A+0): -> level-0 ctx area
    RES   &5         ; pad to level 6 interrupt
    DATA  CTX_LVL6   ; CPT[6] (word &0A+&C=&16): -> level-6 ctx area
    RES   &1         ; aesthetic pad

    ORG     $30
CTX_LVL0
DATA  &00,&00,&00,&00,&00,&00,&00,&00
; ind,X,E,A,L,P,G for the OUTGOING (level 0) context
    RES   &1         ; aesthetic pad

    ORG     $38
CTX_LVL6
DATA  &00,&00,&00,&00,&00,&00,&10,&00
; ind,X,E,A,L,P,G for the INCOMING (level 6) context
; P=&10 (16) = HANDLER's word index. This is what makes execution jump to word 16 on interrupt.
    RES   &1D        ; pad out to the full 128-byte/64-word block
    END
