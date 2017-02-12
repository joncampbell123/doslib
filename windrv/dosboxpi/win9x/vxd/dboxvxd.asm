
	.386p

;----------------------------------------------

_LDATA          SEGMENT DWORD PUBLIC 'CODE'
                ASSUME  CS:FLAT, DS:FLAT, ES:FLAT, SS:FLAT

PUBLIC          DBOXMPI_DDB

DBOXMPI_DDB     dd      0                               ; +0x00 DDB_Next
                dw      030Ah                           ; +0x04 DDB_SDK_Version (3.10)
                dw      0000h                           ; +0x06 DDB_Req_Device_Number (undefined)
                db      1, 0                            ; +0x08 DDB_Dev_Major/Minor_Version
                dw      0000h                           ; +0x0A DDB_Flags
                db      "DBOXMPI "                      ; +0x0C DDB_Name
                dd      80000000h                       ; +0x14 DDB_Init_Order
                dd      OFFSET DBOXMPI_Control          ; +0x18 DDB_Control_Proc
                dd      0                               ; +0x1C DDB_V86_API_Proc
                dd      0                               ; +0x20 DDB_PM_API_Proc
                dd      0                               ; +0x24 DDB_V86_API_CSIP
                dd      0                               ; +0x28 DDB_PM_API_CSIP
                dd      0                               ; +0x2C DDB_Reference_Data
                dd      0                               ; +0x30 DDB_Service_Table_Ptr
                dd      0                               ; +0x34 DDB_Service_Table_Size
                                                        ; =0x38

; NTS: Okay, get this: Microsoft's Linker will ignore all the DD 0's I have written here and put the
;      VXD description string in it's place. Then Windows will follow the obviously bad pointers
;      in the struct as a result, and CRASH. XD. Open Watcom's linker has the sanity not to do that!

; NTS: This structure *MUST* exist in a data segment. Windows will consider your VXD "corrupt" if
;      it sees this structure in a code segment.

_LDATA          ENDS

;----------------------------------------------

_LTEXT          SEGMENT DWORD USE32 PUBLIC 'CODE'
                ASSUME  CS:FLAT, DS:FLAT, ES:FLAT, SS:FLAT

Public          DBOXMPI_Control

DBOXMPI_Control proc near

    int 3
    clc
    ret

DBOXMPI_Control endp

_LTEXT          ENDS

;----------------------------------------------

	            END

