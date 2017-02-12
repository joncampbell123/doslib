
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <hw/dos/exelehdr.h>

void vxd_control_proc(void);

/* VXD control messages */
#define Sys_Critical_Init           0x0000
#define Device_Init                 0x0001
#define Init_Complete               0x0002
#define Sys_VM_Init                 0x0003
#define Sys_VM_Terminate            0x0004
#define System_Exit                 0x0005
#define Sys_Critical_Exit           0x0006
#define Create_VM                   0x0007
#define VM_Critical_Init            0x0008
#define VM_Init                     0x0009
#define VM_Terminate                0x000A
#define VM_Not_Executeable          0x000B
#define Destroy_VM                  0x000C

#define VXD_Control_Dispatch(cmsg,cproc) __asm {    \
    __asm cmp     eax,cmsg                          \
    __asm jne     not_##cproc                       \
    __asm jmp     cproc                             \
    __asm not_##cproc:                              \
}

const struct windows_vxd_ddb_win31 __based( __segname("_CODE") ) DBOXMPI_DDB = {
    0,                                                      // +0x00 DDB_Next
    0x030A,                                                 // +0x04 DDB_SDK_Version
    0x0000,                                                 // +0x06 DDB_Req_Device_Number (Undefined_Device_ID)
    1,0,                                                    // +0x08 DDB_Dev_Major_Version, DDB_Dev_Minor_Version
    0x0000,                                                 // +0x0A DDB_Flags
    "DBOXMPI ",                                             // +0x0C DDB_Name
    0x80000000UL,                                           // +0x14 DDB_Init_Order
    (uint32_t)vxd_control_proc,                             // +0x18 DDB_Control_Proc
    0x00000000UL,                                           // +0x1C DDB_V86_API_Proc
    0x00000000UL,                                           // +0x20 DDB_PM_API_Proc
    0x00000000UL,                                           // +0x24 DDB_V86_API_CSIP
    0x00000000UL,                                           // +0x28 DDB_PM_API_CSIP
    0x00000000UL,                                           // +0x2C DDB_Reference_Data
    0x00000000UL,                                           // +0x30 DDB_Service_Table_Ptr
    0x00000000UL                                            // +0x34 DDB_Service_Table_Size
};

void __declspec(naked) my_sys_critical_init(void) {
    __asm {
        int     3
        clc
        ret
    }
}

void __declspec(naked) my_sys_critical_exit(void) {
    __asm {
        int     3
        clc
        ret
    }
}

void __declspec(naked) vxd_control_proc(void) {
    VXD_Control_Dispatch(Sys_Critical_Init, my_sys_critical_init);
    VXD_Control_Dispatch(Sys_Critical_Exit, my_sys_critical_exit);
    __asm {
        clc
        ret
    }
}

