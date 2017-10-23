;
; D52.CTL - Sample Control File for D52
;
;   Control codes allowed in the CTL file:
;
;  A - Address		Specifies that the word entry is the address of
;			something for which a label should be generated.
;
;  B - Byte binary	Eight bit binary data (db).
;
;  C - Code		Executable code that must be analyzed.
;
;  F - SFR label	Specify name for SFR.
;
;  I - Ignore		Treat as uninitialized space. Will not be dis-
;			assembled as anything unless some other valid
;			code reaches it.
;
;  K - SFR bit		Specify name for SFR bit.
;
;  L - Label		Generate a label for this address.
;
;  M - Memory		Generate a label for bit addressable memory.
;
;  P - Patch		Add inline code (macro, for example)
;
;  R - Register		Specify name for register
;			(instead of rb2r5 for example).
;
;  S - Symbol		Generate a symbol for this value.
;
;  T - Text		ASCII text (db).
;
;  W - Word binary	Sixteen bit binary data (dw).
;
;  X - Operand name	Specify special name for operand.
;
;  # - Comment		Add header comment to output file.
;
;  ! - Inline comment	Add comment to end of line.
;
;  The difference between labels and symbols is that a label refers
;  to a referenced address, whereas a symbol may be used for 8 or 16
;  bit immediate data. For some opcodes (eg: mov r2,#xx) only the symbol
;  table will be searched. Other opcodes (eg: mov dptr,#) will search
;  the label table first and then search the symbol table only if the
;  value is not found in the label table.
;
;  Values may specify ranges, ie:
;
;	A 100-11f specifies a table of addresses starting at address
;		0x100 and continuing to address 0x11f.
;
;	T 100-120 specifies that addresses 0x100 through (and including)
;		address 0x120 contain ascii text data.
;
;  Range specifications are allowed for codes A, B, C, I, T, and W, but
;  obviously don't apply to codes F, K, L, R, and S.
;
;---------------------------------------------------------------------------
;
; labels for interrupt vectors
;
L 0	reset
L 3	ie0vec
L b	tf0vec
L 13	ie1vec
L 1b	tf1vec
L 23	servec
;
; tell D52 to disassemble code at interrupt vector locations
;
C 3	; external 0 interrupt
C b	; timer 0 overflow interrupt
C 13	; external 1 interrupt
C 1b	; timer 1 overflow interrupt
C 23	; serial xmit/recv interrupt
;
; end of control file
;

