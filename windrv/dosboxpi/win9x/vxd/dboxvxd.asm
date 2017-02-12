
	.386p

	.XLIST
	INCLUDE VMM.Inc
	INCLUDE Debug.Inc
	INCLUDE VPICD.Inc
	INCLUDE VCD.Inc
	.LIST

;----------------------------------------------

;Declare_Virtual_Device DBOXMPI, 3, 0, DBOXMPI_Control, Undefined_Device_ID, Undefined_Init_Order

VxD_LOCKED_DATA_SEG

PUBLIC DBOXMPI_DDB

DBOXMPI_DDB     dd      0                               ; +0x00 DDB_Next
                dw      030Ah                           ; +0x04 DDB_SDK_Version (3.10)
                dw      0000h                           ; +0x06 DDB_Req_Device_Number (undefined)
                db      1, 0                            ; +0x08 DDB_Dev_Major/Minor_Version
                dw      0000h                           ; +0x0A DDB_Flags
                db      "DBOXMPI "                      ; +0x0C DDB_Name
                dd      80000000h                       ; +0x14 DDB_Init_Order
                dd      OFFSET32 DBOXMPI_Control        ; +0x18 DDB_Control_Proc
                dd      0                               ; +0x1C DDB_V86_API_Proc
                dd      0                               ; +0x20 DDB_PM_API_Proc
                dd      0                               ; +0x24 DDB_V86_API_CSIP
                dd      0                               ; +0x28 DDB_PM_API_CSIP
                dd      0                               ; +0x2C DDB_Reference_Data
                dd      0                               ; +0x30 DDB_Service_Table_Ptr
                dd      0                               ; +0x34 DDB_Service_Table_Size
                                                        ; =0x38

VxD_LOCKED_DATA_ENDS

;----------------------------------------------

VxD_LOCKED_CODE_SEG

BeginProc DBOXMPI_Control

    int 3
    clc
    ret

EndProc DBOXMPI_Control

VxD_LOCKED_CODE_ENDS

;----------------------------------------------

	END

