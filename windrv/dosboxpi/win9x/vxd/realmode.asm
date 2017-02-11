
	.386p

	.XLIST
	INCLUDE VMM.Inc
	INCLUDE Debug.Inc
	INCLUDE VDD.Inc
	INCLUDE VMD.Inc
	INCLUDE DOSMGR.Inc
	INCLUDE Int2FAPI.Inc
	INCLUDE V86MMGR.Inc
	INCLUDE VDD.Inc
	INCLUDE OptTest.Inc
	.LIST

VxD_REAL_INIT_SEG


BeginProc Int33_Real_Init
    int 3
	xor	bx, bx
	xor	si, si
	mov	ax, Device_Load_Ok
	ret

EndProc Int33_Real_Init


VxD_REAL_INIT_ENDS

	END	Int33_Real_Init ; LE entry point always points to real-mode init code

