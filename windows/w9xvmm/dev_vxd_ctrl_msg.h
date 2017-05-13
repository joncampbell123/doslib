
/* VXD control messages */

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
#define Sys_Critical_Init           0x0000

/* VxD control message Device_Init.
 *
 * Entry:
 *   EAX = Device_Init
 *   EBX = handle of System VM
 *   ESI = Pointer to command tail retrieved from PSP of WIN386.EXE
 *
 * Exit:
 *   Carry flag = clear if success, set if failure */
#define Device_Init                 0x0001

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
#define Init_Complete               0x0002

/* VxD control message Sys_VM_Init.
 *
 * Entry:
 *   EAX = Sys_VM_Init
 *   EBX = handle of System VM
 *
 * Exit:
 *   Carry flag = clear if success, set if failure */
#define Sys_VM_Init                 0x0003

/* VxD control message Sys_VM_Terminate.
 *
 * Entry:
 *   EAX = Sys_VM_Terminate
 *   EBX = handle of System VM
 *
 * Exit:
 *   Carry flag = clear if success, set if failure */
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

