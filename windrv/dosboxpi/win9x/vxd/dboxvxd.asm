
	.386p

	.XLIST
	INCLUDE VMM.Inc
	INCLUDE Debug.Inc
	INCLUDE VPICD.Inc
	INCLUDE VCD.Inc
	.LIST

;----------------------------------------------

_LDATA          SEGMENT DWORD PUBLIC 'CODE'

PUBLIC          DBOXMPI_DDB

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

_LDATA          ENDS

;----------------------------------------------

_LTEXT          SEGMENT DWORD USE32 PUBLIC 'CODE'

Public          DBOXMPI_Control

DBOXMPI_Control proc near

    int 3
    clc
    ret

DBOXMPI_Control endp

_LTEXT          ENDS

;----------------------------------------------

	            END

