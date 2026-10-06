        ORG     $0
IDLE    LDA   =&80	     ; Transfer 128 byte of secondary boot code
        STR   =&09	     ; Store secondary boot address in R9
	LDA   =&64	     ; Address where to store the secondary boot code
        STR   =&0A	     ; Store secondary boot address in R10
IDLE    BRU   IDLE         ; Level-0 idle loop — "MITRA runs at level zero...awaiting the interrupt"
FIN
