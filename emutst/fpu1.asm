
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
	fst	qword [store1b]
	fst	dword [store1c]
	fstp	tword [store1a]

	finit
	NOPS
	fld	dword [data1]
	NOPS
	fst
	NOPS
	fmulp
	NOPS
	fst	qword [store1b]
	fst	dword [store1c]
	fstp	tword [store1a]

	finit
	NOPS
	fld	dword [data1]
	NOPS
	fld	dword [data1]
	NOPS
	fdiv
	NOPS
	fst	qword [store1b]
	fst	dword [store1c]
	fstp	tword [store1a]

end:
	ret

data1:
	dd	1.23
data2:
	dq	1.23
data3:
	dt	1.23

store1a:
	dt	0.0
store1b:
	dq	0.0
store1c:
	dd	0.0

