* Example from Section VIII-5.1
* Not all parts are complete, but it illustrates the syntax.

                ORG     $100

* CDS section (Common Data Segment) - Not fully implemented
CDS
                RES     16          * TWB
FIN

* LDS section (Local Data Segment)
LDS1
CB1             DATA    0
                DATA,1  0           * Command: 00 (Read)
                DATA,1  1           * Operational Label: M:BI
                DATA    #BBINS      * Buffer address (will be resolved)
                DATA    16          * Number of bytes
                DATA    0,0,0,0,0,0,0,0
BBINS           RES     16
CB2             DATA    0
                DATA,1  0
                DATA,1  3           * M:OC
                DATA    #BBANS
                DATA    14
                DATA    0,0,0,0,0,0,0,0
BBANS           RES     14
FIN

* LPS section (Local Program Segment)
LPS1
DEB             LEA     CB1         * Load effective address of CB1
                CSV     M:IO        * Call Supervisor for I/O (not in opcode map, handle as CSV)
                CSV     M:WAIT      * Wait for I/O completion
                LEA     CB2
                CSV     M:IO
                CSV     M:WAIT
                FIN     DEB
                END     LPS1
