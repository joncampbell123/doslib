
	.386p

	.XLIST
	INCLUDE VMM.Inc
	INCLUDE Debug.Inc
	INCLUDE VPICD.Inc
	INCLUDE VCD.Inc
	.LIST


Declare_Virtual_Device DBOXMPI, 3, 0, DBOXMPI_Control, Undefined_Device_ID, Undefined_Init_Order


VxD_LOCKED_DATA_SEG

    db      'Hello locked data',0

VxD_LOCKED_DATA_ENDS


VxD_ICODE_SEG

VxD_ICODE_ENDS


VxD_LOCKED_CODE_SEG

BeginProc DBOXMPI_Control

    clc
    ret

EndProc DBOXMPI_Control

BeginProc ldummycode

    ret

    db      'This is locked code segment',0

EndProc ldummycode

VxD_LOCKED_CODE_ENDS


VxD_CODE_SEG

BeginProc dummycode

    ret

    db      'This is code segment',0

EndProc dummycode

VxD_CODE_ENDS

	END
