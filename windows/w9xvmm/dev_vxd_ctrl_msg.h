
/* VXD control messages */

/* Common VxD Control message register layout.
 * This register/parameter layout is set in stone
 * by the VMM VxD call System_Control:
 *
 * EAX = Message                System control message
 * EBX = VM                     VM handle (if needed by message)
 * ESI = Param1                 message-specific parameter
 * EDI = Param2                 message-specific parameter
 * EDX = Param3                 message-specific parameter
 *
 * These registers are passes as-is to the VxD control procedure.
 *
 * Nothing is mentioned about register preservation by the DDK, and
 * all samples seem to indicate that you can freely destroy and overwrite
 * these registers with no consequences within your VxD control
 * procedure. Apparently Win386 will restore them when you return
 * when it then goes on to call the next VxD control proc in the chain.
 */

/* VxD control message Sys_Critical_Init.
 *
 * Meaning: Virtual devices initializing. Interrupts are disabled at this time.
 *          Windows is starting.
 *
 * The system sends this message to direct virtual devices to carry out as
 * quickly as possible the minimum tasks needed to prepare the device for
 * enabled interrupts.
 *
 * "Devices that have a critical function that needs initializing before
 * interrupts are enabled should do it at Sys_Critical_Init. Devices which
 * REQUIRE a certain range of V86 pages to operate (such as the VDD video
 * memory) should claim them at Sys_Critical_Init."
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
 * Meaning: Virtual devices initializing. Interrupts are enabled at this time.
 *
 * The virtual device should initialize itself. This is when the virtual device
 * should allocate memory for a device-specific section in the control block,
 * allocate other memory, hook interrupts and I/O ports, and specify instance
 * data.
 *
 * The virtual device should allocate a device-specific section in the control
 * block of the system VM and initialize the section here.
 *
 * The VxD can call Simulate_Int and Exec_Int from this call.
 *
 * Entry:
 *   EAX = Device_Init
 *   EBX = handle of System VM
 *   ESI = Pointer to command tail retrieved from PSP of WIN386.EXE
 *
 * Exit:
 *   Carry flag = clear if success, set if failure.
 *   If failure (CF=1), Windows will abort loading this VxD and continue normally. */
#define Device_Init                 0x0001

/* VxD control message Init_Complete.
 *
 * Meaning:
 *   System and virtual devices have initialized successfully.
 *   Virtual devices that use virtual 8086 memory might use this
 *   time to search for available pages A0h to 100h
 *   (adapter ROM A0000-FFFFF) when processing this message.
 *
 * "Init_Complete is the final phase of device init called just before the
 * WIN386 INIT pages are released and the Instance snapshot is taken."
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
 *   INIT pages and taking the instance snapshot.
 *
 *   Signalling failure (CF=1) causes Windows to unload the virtual device
 *   and continue normally. */
#define Init_Complete               0x0002

/* VxD control message Sys_VM_Init.
 *
 * Meaning:
 *   The System VM wants the virtual device to initialize the state of
 *   the software in the system virtual machine. For example, a virtual
 *   display device would issue INT 10H to set the initial display mode
 *   at this time.
 *
 * Entry:
 *   EAX = Sys_VM_Init
 *   EBX = handle of System VM
 *
 * Exit:
 *   Carry flag = clear if success, set if failure.
 *
 * Notes:
 *   Failure (CF=1) here will cause Windows to immediately exit. */
#define Sys_VM_Init                 0x0003

/* VxD control message Sys_VM_Terminate.
 *
 * Meaning:
 *   The System VM is terminating. This is sent after all other virtual machines
 *   have terminated, and only when the system is terminating normally.
 *
 * Entry:
 *   EAX = Sys_VM_Terminate
 *   EBX = handle of System VM
 *
 * Exit:
 *   Carry flag = clear if success, set if failure.
 *   According to documentation, you must set CF=0. */
#define Sys_VM_Terminate            0x0004

/* VxD control message System_Exit.
 *
 * Meaning:
 *   The system is terminating normally, or as a result of a crash.
 *   Interrupts are enabled while VxDs process this message.
 *
 * Entry:
 *   EAX = System_Exit
 *   EBX = handle of System VM
 *
 * Exit:
 *   Carry flag = clear if success, set if failure.
 *   According to documentation, you must set CF=0.
 *
 * Notes:
 *   Must not call Simulate_Int or Exec_Int service, but may modify
 *   the system virtual machine memory to restore system state to
 *   allow Windows to exit without problems.
 *
 *   The system restores the instance snapshot before sending this
 *   message. */
#define System_Exit                 0x0005

/* VxD control message Sys_Critical_Exit.
 *
 * Meaning:
 *   The system is terminating normally, or as a result of a crash.
 *   Interrupts are disabled while VxDs process this message.
 *   VxDs should reset their hardware to allow for a return to the
 *   state that it existed in before Windows started.
 *
 * Entry:
 *   EAX = Sys_Critical_Exit
 *
 * Exit:
 *   Carry flag = clear if success, set if failure.
 *   According to documentation, you must set CF=0.
 *
 * Notes:
 *   Must not call Simulate_Int or Exec_Int service.
 */
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
 *   EBX = Virtual machine handle (VID). If zero, all virtual devices recieve the focus.
 *   ESI = Flags. How to set the focus if the VID parameter is zero.
 *   EDX = Virtual device to recieve focus, or 0 for all
 *   EDI = AssocVM
 *
 * Flags (ESI) meaning:
 *   1 = Used by the virtual shell to determine which virtual machine to set focus for.
 *       If this value is given, AssocVM parameter may specify a virtual machine.
 *
 * AssocVM (EDI) meaning:
 *   Specifies a handle identifying a virtual machine associated with a problem, or
 *   zero if no such machine. This parameter is only used if Flags is 1.
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

