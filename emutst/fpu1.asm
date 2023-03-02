
	bits	16
	org	100h

%macro	NOPS	0
	; large enough that not even dynamic core will step over more than one instruction
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
%endmacro

start:
	finit
	NOPS
	fldz
	NOPS
	fld1
	NOPS
	faddp
	NOPS
	fld	dword [data1]
	NOPS
	fld	qword [data2]
	NOPS
	fld	tword [data3]
	NOPS
	faddp
	NOPS
	faddp
	NOPS
	faddp
	NOPS

	finit
	NOPS
	fld	dword [data1]
	NOPS
	fst
	NOPS
	fmulp
	NOPS

	finit
	NOPS
	fld	dword [data1]
	NOPS
	fld	dword [data1]
	NOPS
	fdiv
	NOPS

end:
	ret

data1:
	dd	1.23
data2:
	dq	1.23
data3:
	dt	1.23
