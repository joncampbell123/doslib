
	.386p

	.XLIST
	INCLUDE VMM.Inc
	INCLUDE Debug.Inc
	INCLUDE VPICD.Inc
	INCLUDE VCD.Inc
	.LIST

;----------------------------------------------

Declare_Virtual_Device DBOXMPI, 3, 0, DBOXMPI_Control, Undefined_Device_ID, Undefined_Init_Order

;----------------------------------------------

VxD_LOCKED_CODE_SEG

BeginProc DBOXMPI_Control

    clc
    ret

EndProc DBOXMPI_Control

VxD_LOCKED_CODE_ENDS

;----------------------------------------------

	END

