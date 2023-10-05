; cl65 -t none prodos_hdc_rom.S
; bin2hdr -i prodos_hdc_rom -o prodos_hdc_rom.h -a prodos_hdc_rom

.feature c_comments

/*

MIT License

Copyright (c) 2023 Oliver Schmidt (https://a2retro.de/)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

SLOT    = $07

MAGIC   = $65

LOC0    := $00
LOC1    := $01
CMD     := $42          ; DISK DRIVER COMMAND
UNIT    := $43          ; DISK DRIVER UNIT
BUFL    := $44          ; DISK DRIVER BUFFER
BUFH    := $45
BLOCKL  := $46          ; DISK DRIVER BLOCK
BLOCKH  := $47

MSLOT   := $07F8        ; BUFFER FOR HI SLOT ADDR (SCN)

RC_A    := $C080        ; RETURN CODE REGISTER A
RC_X    := $C081        ; RETURN CODE REGISTER X
RC_Y    := $C082        ; RETURN CODE REGISTER Y
PARA    := $C087        ; PARAVIRTUALIZATION TRIGGER

COUT    := $FDED        ; CHARACTER OUT (THRU CSW)
SETKBD  := $FE89        ; SETS KSW TO APPLE KEYBOARD
SETSCR  := $FE93        ; SETS CSW TO APPLE SCREEN
SLOOP   := $FABA        ; CONTINUE SLOT SCAN
SETTXT  := $FB39        ; SET TEXT MODE
HOME    := $FC58        ; CLEAR SCREEN AND HOME CURSOR
BASIC   := $E000        ; BASIC INTERPRETER COLD START

        .ORG $C000+$0100*SLOT

        CPX #$20        ; $Cn01:$20
        LDX #$00        ; $Cn03:$00
        CPX #$03        ; $Cn05:$03
        CPX #$3C        ; $Cn07:$3C

        STX BLOCKL
        STX BLOCKH
        STX BUFL
        INX             ; 1 MEANS READ
        STX CMD
        LDX #>$0800
        STX BUFH
        LDX #$10*SLOT   ; DRIVE 1
        STX UNIT
        JSR DRIVER      ; READ BLOCK 0 FROM DRIVE 1 TO $0800
        BCS FAILURE
        LDX $0800       ; NUMBER OF DISK II SECTORS TO READ
        DEX
        BNE FAILURE     ; ... MUST BE 1
        LDX $0801       ; FIRST OPCODE
        BEQ FAILURE     ; ... MUSTN'T BE BRK
        LDX #$10*SLOT   ; DRIVE 1
        JMP $0801       ; GO FOR IT

FAILURE:LDX LOC0        ; POTENTIAL AUTOSTART SCAN ROM PTR LO
        BNE ERREXIT     ; ... MUST POINT TO A SLOT ROM START
        LDX LOC1        ; POTENTIAL AUTOSTART SCAN ROM PTR HI
        CPX MSLOT       
        BNE ERREXIT     ; ... MUST POINT TO CURRENT SLOT ROM
        CPX #>*
        BNE ERREXIT     ; ... MUST POINT TO THIS SLOT ROM
        JMP SLOOP       ; LOOKS LIKE AUTOSTART SCAN SO CONTINUE SCAN

ERREXIT:JSR SETSCR
        JSR SETKBD
        JSR SETTXT
        JSR HOME
        LDX #$08        ; 'HDD ERROR'
:       LDA ERRTEXT,X
        JSR COUT
        DEX
        BPL :-
        JMP BASIC

ERRTEXT:.BYTE 'R'|$80, 'O'|$80, 'R'|$80, 'R'|$80, 'E'|$80, ' '|$80, 'D'|$80, 'D'|$80, 'H'|$80

DRIVER: LDA #MAGIC
        STA PARA|$10*SLOT
                        ; ABRACADABRA
        LDA RC_A|$10*SLOT
        LDX RC_X|$10*SLOT
        LDY RC_Y|$10*SLOT
        CMP #$01        ; SET CARRY IF A IS NOT 0
        RTS

        .RES $0100-4-<* ; THE 4 BYTES BELOW GO TO THE END OF THE SLOT ROM

        .BYTE $00,$00   ; TOTAL BLOCKS
        .BYTE $F7       ; STATUS BYTE
        .BYTE <DRIVER
