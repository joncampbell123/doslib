/* NOTE: Because of our ASM interfacing needs, you must compile this with
 *       GCC, not Open Watcom. */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <hw/dos/exelehdr.h>

#include <hw/dosboxid/iglib.h>

void vxd_control_proc(void);

/* USEFUL */
#if defined(__GNUC__)
# define VXD_INT3()                 __asm__ __volatile__ ("int $3")
#else
# define VXD_INT3()                 __asm int 3
#endif

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
#define VM_Suspend                  0x000D
#define VM_Resume                   0x000E
#define Set_Device_Focus            0x000F
#define Begin_Message_Mode          0x0010
#define End_Message_Mode            0x0011
#define Reboot_Processor            0x0012
#define Query_Destroy               0x0013
#define Debug_Query                 0x0014
#define Begin_PM_App                0x0015
#define End_PM_App                  0x0016
#define Device_Reboot_Notify        0x0017
#define Crit_Reboot_Notify          0x0018
#define Close_VM_Notify             0x0019
#define Power_Event                 0x001A

/* VXD device IDs */
#define VMM_Device_ID               0x0001
#define snr_Get_VMM_Version             0x0000
#define snr_Get_Cur_VM_Handle           0x0001
#define snr_Test_Cur_VM_Handle          0x0002
#define snr_Get_Sys_VM_Handle           0x0003
#define snr_Test_Sys_VM_Handle          0x0004
#define snr_Validate_VM_Handle          0x0005
#define Debug_Device_ID             0x0002
#define VPICD_Device_ID             0x0003
#define VDMAD_Device_ID             0x0004

/* VMM services */
#define Fatal_Memory_Error          0x00BF

static inline void VXD_CF_SUCCESS(void) {
    __asm__ __volatile__ ("clc");
}

static inline void VXD_CF_FAILURE(void) {
    __asm__ __volatile__ ("stc");
}

#define VXD_CONTROL_DISPATCH(ctrlmsg, ctrlfunc) \
    __asm__ __volatile__ (                      \
        "cmp    %0,%%eax\n"                     \
        "jz     " #ctrlfunc "\n"                \
        : /*outputs*/                           \
        : /*inputs*/  /*%0*/ "i" (ctrlmsg)      \
        : /*clobbers*/       "cc")

typedef uint32_t vxd_vm_handle_t;

const struct windows_vxd_ddb_win31 DBOXMPI_DDB = {
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

/* keep track of the system VM */
vxd_vm_handle_t System_VM_Handle = 0;
vxd_vm_handle_t Focus_VM_Handle = 0;

/* VxD control message Sys_Critical_Init.
 *
 * Entry:
 *   EAX = Sys_Critical_Init
 *   EBX = handle of System VM
 *   ESI = Pointer to command tail retrived from PSP of WIN386.EXE
 *
 * Exit:
 *   Carry flag = clear if success, set if failure
 *
 * Notes:
 *   Do not use Simulate_Int or Exec_Int at this stage. */
void my_sys_critical_init(void) {
    VXD_INT3();
    VXD_CF_SUCCESS();
}

/* VxD control message Device_Init.
 *
 * Entry:
 *   EAX = Device_Init
 *   EBX = handle of System VM
 *   ESI = Pointer to command tail retrieved from PSP of WIN386.EXE
 *
 * Exit:
 *   Carry flag = clear if success, set if failure */

/* VxD control message Init_Complete.
 *
 * Entry:
 *   EAX = Init_Complete
 *   EBX = handle of System VM
 *   ESI = Pointer to command tail retrieved from PSP of WIN386.EXE
 *
 * Exit:
 *   Carry flag = clear if success, set if failure
 *
 * Notes:
 *   The system will send this message out just before releasing it's
 *   INIT pages and taking the instance snapshot. */

/* VxD control message Sys_VM_Init.
 *
 * Entry:
 *   EAX = Sys_VM_Init
 *   EBX = handle of System VM
 *
 * Exit:
 *   Carry flag = clear if success, set if failure */

/* VxD control message Set_Device_Focus.
 *
 * Entry:
 *   EAX = Sys_Device_Focus
 *   EBX = Virtual machine handle
 *   ESI = Flags
 *   EDX = Virtual device to recieve focus, or 0 for all
 *   EDI = AssocVM
 *
 * Exit:
 *   CF = 0 */

/* VxD control message Sys_VM_Terminate.
 *
 * Entry:
 *   EAX = Sys_VM_Terminate
 *   EBX = handle of System VM
 *
 * Exit:
 *   Carry flag = clear if success, set if failure */

/* WARNING: When compiled with GCC you must use -fomit-frame-pointer to prevent this
 *          dispatch routine from pushing anything on the stack or setting up any
 *          stack frame. */
void vxd_control_proc(void) {
    VXD_CONTROL_DISPATCH(Sys_Critical_Init, my_sys_critical_init);
    VXD_CF_SUCCESS();
}

