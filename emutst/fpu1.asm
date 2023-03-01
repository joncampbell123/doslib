
	bits	16
	org	100h

start:
	finit
	fldz
	fld1
	faddp
	fld	dword [data1]
	fld	qword [data2]
	fld	tword [data3]
	faddp
	faddp
	faddp

	nop
	nop

	finit
	fld	dword [data1]
	fst
	fmulp

end:
	ret

data1:
	dd	1.23
data2:
	dq	1.23
data3:
	dt	1.23
