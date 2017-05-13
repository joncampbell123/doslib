
/* VXD control messages.
 * Windows 3.1 DDK level.
 * Messages for Windows 95 and higher are defined elsewhere.
 *
 * FIXME: I don't have the Windows 3.0 DDK, I don't know at what point new messages were
 *        added to Windows 3.1 and which ones originally existed in Windows 3.0 */

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
 *   If failure (CF=1), Windows will abort loading this VxD and continue normally.
 *
 * Notes from Windows 95 DDK:
 *   For dynamically loaded VxDs, EBX does not contain a VM handle [FIXME: What does it contain then?]
 */
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
 * After this call, initialization code and data are discarded.
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
 *   This message is NOT sent in the case of an abnormal exit such as a crash.
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
 *   message.
 *
 * Notes from Windows 95 DDK:
 *   The virtual device may not access pageable data at this time
 *   because the swap file might be invalid or in a bad state if
 *   the system is crashing down instead of stopping normally.
 *
 *   If a device needs pageable data during this message, it should
 *   lock the data during Sys_VM_Terminate. But, if the system is
 *   crashing Sys_VM_Terminate will not be called. Virtual devices
 *   should be prepared to skip steps involving pageable data if
 *   it was never notified of Sys_VM_Terminate.
 */
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

/* VxD control message Create_VM.
 *
 * Meaning:
 *   The system is creating a new VM. Virtual devices can use this
 *   call to initialize data associated with the virtual machine
 *   such as data in the control block for the new VM.
 *
 * Entry:
 *   EAX = Create_VM
 *   EBX = New VM handle
 *
 * Exit:
 *   Carry flag = clear if success, set if failure.
 *   Failure (CF=1) prevents the creation of a VM.
 *
 * Notes:
 *   The Windows 95 DDK clarifies that this is called "out of context",
 *   meaning the VM has not yet started. Most devices will act on
 *   VM_Init instead.
 */
#define Create_VM                   0x0007

/* VxD control message VM_Critical_Init.
 *
 * Meaning:
 *   The virtual device should initialize itself for the new
 *   virtual machine. Interrupts are disabled during this call.
 *
 * Entry:
 *   EAX = VM_Critical_Init
 *   EBX = New VM handle
 *
 * Exit:
 *   Carry flag = clear if success, set if failure.
 *   Failure (CF=1) prevents the creation of a VM.
 *
 * Note:
 *   Virtual device must not use Simulate_Int or Exec_int
 *   in the specified virtual machine within this call.
 */
#define VM_Critical_Init            0x0008

/* VxD control message VM_Init.
 *
 * Meaning:
 *   The virtual device should initialize itself for the new
 *   virtual machine. Interrupts are enabled during this call.
 *
 * Entry:
 *   EAX = VM_Init
 *   EBX = New VM handle
 *
 * Exit:
 *   Carry flag = clear if success, set if failure.
 *   Failure (CF=1) prevents the creation of a VM.
 *
 * Note:
 *   Virtual device can use Simulate_Int or Exec_int
 *   in the specified virtual machine within this call.
 */
#define VM_Init                     0x0009

/* VxD control message VM_Terminate.
 *
 * Meaning:
 *   The system is terminating the specified VM. This is only
 *   sent when the VM is being terminated normally. This is
 *   the first stage of Destroy_VM.
 *
 * Entry:
 *   EAX = VM_Terminate
 *   EBX = VM handle
 *
 * Exit:
 *   CF = 0
 *
 * Note:
 *   Virtual device can use Simulate_Int or Exec_int
 *   in the specified virtual machine within this call.
 */
#define VM_Terminate                0x000A

/* VxD control message VM_Not_Executeable.
 *
 * Meaning:
 *   The system is telling the virtual device the VM is no longer
 *   capable of detecting. If the VM is running, VM_Terminate is
 *   not called and this is sent first instead. This is the second
 *   stage of Destroy_VM.
 *
 * Entry:
 *   EAX = VM_Not_Executeable
 *   EBX = VM handle
 *   EDX = Flags
 *
 * Flags:
 *   The reason the virtual machine is no longer executable.
 *   It can be one of the following values.
 *
 *      VME_Crashed                 Virtual machine has crashed.
 *
 *      VME_Nuked                   Virtual machine was destroyed while active.
 *
 *      VME_CreateFail              Some device failed Create_VM
 *
 *      VME_CrInitFail              Some device failed VM_Critical_Init
 *
 *      VME_InitFail                Some device failed VM_Init
 *
 * Exit:
 *   CF = 0
 *
 * Note:
 *   Virtual device can not use Simulate_Int or Exec_int
 *   in the specified virtual machine within this call.
 */
#define VM_Not_Executeable          0x000B

#  define VME_Crashed_Bit               0UL
#  define VME_Crashed                   (1UL << VME_Crashed_Bit)

#  define VNE_Nuked_Bit                 1UL
#  define VNE_Nuked                     (1UL << VNE_Nuked_Bit)

#  define VNE_CreateFail_Bit            2UL
#  define VNE_CreateFail                (1UL << VNE_CreateFail_Bit)

#  define VNE_CrInitFail_Bit            3UL
#  define VNE_CrInitFail                (1UL << VNE_CrInitFail_Bit)

#  define VNE_InitFail_Bit              4UL
#  define VNE_InitFail                  (1UL << VNE_InitFail_Bit)

#  define VNE_Closed_Bit                5UL
#  define VNE_Closed                    (1UL << VNE_Closed_Bit)

/* VxD control message Destroy_VM.
 *
 * Meaning:
 *   The system has destroyed the VM. This is the final step
 *   in destroying a VM.
 *
 * Entry:
 *   EAX = Destroy_VM
 *   EBX = VM handle
 *
 * Exit:
 *   CF = 0
 *
 * Note:
 *   Virtual device can not use Simulate_Int or Exec_int
 *   in the specified virtual machine within this call.
 *
 *   The time between VM_Not_Executeable and this call
 *   can be considerable, don't assume it's quick.
 */
#define Destroy_VM                  0x000C

/* VxD control message VM_Suspend.
 *
 * Meaning:
 *   The system is suspending execution of the virtual machine.
 *   The virtual device should unlock and release resources as
 *   appropriate for the VM.
 *
 * Entry:
 *   EAX = VM_Suspend
 *   EBX = VM handle
 *
 * Exit:
 *   CF = 0
 *
 * Note:
 *   The virtual machine remains suspended until explicitly
 *   resumed.
 *
 *   You can check the CB_VM_Status field to see whether or
 *   not the VM is suspended.
 */
#define VM_Suspend                  0x000D

/* VxD control message VM_Resume.
 *
 * Meaning:
 *   The virtual machine is resuming from suspended state.
 *   The virtual device should lock any resources it needs
 *   and prepare data structures to start running again.
 *
 * Entry:
 *   EAX = VM_Resume
 *   EBX = VM handle
 *
 * Exit:
 *   Carry flag = clear if success, set if failure.
 *   Failure (CF=1) prevents resuming the a VM.
 *
 * Note:
 *   The system will never send VM_Resume without first
 *   calling VM_Suspend first (according to docs)
 */
#define VM_Resume                   0x000E

/* VxD control message Set_Device_Focus.
 *
 * Meaning:
 *   Focus is changing to the specified virtual machine.
 *
 *   The virtual device can use this time to carry out steps
 *   such as disabling I/O trapping for the virtual machine
 *   if that virtual machine "owns" the device.
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

/* VxD control message Begin_Message_Mode.
 *
 * Meaning:
 *   The system is calling display, keyboard, mouse, etc. devices
 *   to prepare to display messages and read input from the keyboard.
 *
 *   "The system sends this message if Windows cannot display the
 *   requested warning or error message." Huh??
 *
 * Entry:
 *   EAX = Begin_Message_Mode
 *   EBX = VM handle
 *
 * Exit:
 *   CF = 0 */
#define Begin_Message_Mode          0x0010

/* VxD control message End_Message_Mode.
 *
 * Meaning:
 *   The system is calling for the virtual device to end message mode processing.
 *
 * Entry:
 *   EAX = End_Message_Mode
 *   EBX = VM handle
 *
 * Exit:
 *   CF = 0 */
#define End_Message_Mode            0x0011

/* VxD control message Reboot_Processor.
 *
 * Meaning:
 *   If the virtual device can reboot the computer, it should do so.
 *   Windows will call each device with this message until one device
 *   does so.
 *
 * Entry:
 *   EAX = Reboot_Processor
 *
 * Exit:
 *   CF = 0 */
#define Reboot_Processor            0x0012

/* VxD control message Query_Destroy.
 *
 * Meaning:
 *   The system is asking virtual machines to display a warning message if destroying
 *   the VM will disrupt the virtual device. This call should set CF true if it will,
 *   and it should also use SHELL_Message to display a message to the user.
 *
 *   The system will send this message before attempting to destroy a VM that has not
 *   terminated normally.
 *
 * Entry:
 *   EAX = Query_Destroy
 *   EBX = VM handle
 *
 * Exit:
 *   Carry flag = clear if success, set if destruction of the VM will disrupt the virtual device.
 */
#define Query_Destroy               0x0013

/* VxD control message Debug_Query.
 *
 * Meaning:
 *   The virtual device should enable it's debugging commands, if any.
 *   It should display a list of debugging commands [how?] then read the debugging
 *   device for command input from the user.
 *
 *   The system will send this message when the user enters a virtual-device command
 *   at the WDEB386 prompt (a period followed by the name of the virtual device).
 *
 *   This is intended for use only with debug builds of the virtual device. Release
 *   builds should not include debugging support.
 *
 * Entry:
 *   EAX = Debug_Query
 *
 * Exit:
 *   CF = 0
 *
 * Notes:
 *   Windows 95: handler for this message must not exist in pageable memory. It should
 *   be placed in a Debug_Only code segment.
 */
#define Debug_Query                 0x0014

/* VxD control message Begin_PM_App.
 *
 * Meaning:
 *
 * Entry:
 *   EAX = Begin_PM_App
 *   EBX = VM handle
 *   EDX = Flags
 *   EDI = pointer to Application Control Block
 *
 * Flags:
 *   BPA_32_bit                     Application has 32-bit segments. Else, it only has 16-bit segments.
 *
 * Exit:
 *   Carry flag = clear if success, set if failure.
 *   Signal failure if the virtual device cannot support the protected mode application
 */
#define Begin_PM_App                0x0015

#  define BPA_32_Bit_Flag               1UL
#  define BPA_32_Bit                    1UL

/* VxD control message End_PM_App.
 *
 * Meaning:
 *
 * Entry:
 *   EAX = End_PM_App
 *   EBX = VM handle
 *   EDI = pointer to Application Control Block
 *
 * Exit:
 *   CF = 0
 */
#define End_PM_App                  0x0016

/* VxD control message Device_Reboot_Notify.
 *
 * Meaning:
 *   The system VM is notifying all virtual devices that the
 *   system is about to restart. Virtual devices should prepare
 *   by cleaning up data, resetting devices, etc. Interrupts
 *   are enabled.
 *
 * Entry:
 *   EAX = Device_Reboot_Notify
 *
 * Exit:
 *   CF = 0 */
#define Device_Reboot_Notify        0x0017

/* VxD control message Crit_Reboot_Notify.
 *
 * Meaning:
 *   The system VM is notifying all virtual devices that the
 *   system is about to restart. Virtual devices should prepare
 *   by cleaning up data, resetting devices, etc. Interrupts
 *   are disabled.
 *
 * Entry:
 *   EAX = Crit_Reboot_Notify
 *
 * Exit:
 *   CF = 0 */
#define Crit_Reboot_Notify          0x0018

/* VxD control message Close_VM_Notify.
 *
 * Meaning:
 *
 * Entry:
 *   EAX = Close_VM_Notify
 *   EBX = VM handle
 *   EDX = Flags
 *
 * Flags:
 *   CVNF_Crit_CLose                The virtual machine has not released the critical section
 *
 * Exit:
 *   Carry flag = clear if success, set if failure.
 *   Signal failure if the virtual device cannot support the termination of the VM.
 */
#define Close_VM_Notify             0x0019

#  define CVNF_Crit_Close_Bit           0UL
#  define CVNF_Crit_Close               (1UL << CVNF_Crit_Close_Bit)

/* VxD control message Power_Event.
 *
 * Meaning:
 *
 * Entry:
 *   EAX = Power_Event
 *   EBX = 0
 *   ESI = Event notification message
 *   EDI = Pointer to DWORD return value.
 *   EDX = undefined (must be preserved)
 *
 * Note:
 *   Return DWORD by *((DWORD*)EDI) = return value
 *
 * Event notification message:
 *   PWR_SUSPENDREQUEST                     Suspend operation
 *
 *   PWR_SUSPENDRESUME                      Resume after suspend
 *
 *   PWR_CRITICALRESUME                     Resume critical operations after suspend
 *
 * Return value *EDI:
 *   PWR_OK                                 Virtual device processed successfully
 *
 *   PWR_FAIL                               Virtual device failed to process event
 *
 * Notes from the Windows 95 DDK:
 *   Only the virtual power device (VPOWERD) is permitted to send this message to drivers.
 *   [FIXME: Yeah, but does it enforce that policy?]
 *
 *   Microsoft clarifies that *EDI is initialized to PWR_OK and devices should only overwrite
 *   it with PWR_FAIL on failure, else, leave it alone, so that another device's PWR_FAIL
 *   is not overwritten with success.
 *
 *   Windows 95 sends this message for compat. with Windows 3.1 device drivers and Windows 95
 *   drivers should use VPOWERD_Register_Power_Handler to be notified of changes in power state.
 *
 * Exit:
 *   CF = 0 */
#define Power_Event                 0x001A

/* NTS: If I read the DDK headers right, PWR_FAIL is defined as -1,
 *      which in 16-bit code becomes 0xFFFF,
 *      and in 32-bit code becomes 0xFFFFFFFF.
 *      Does that cause problems? */
#  define PWR_OK                        (1U)
#  define PWR_FAIL                      ((unsigned int)(-1L))

#  define PWR_SUSPENDREQUEST            (1U)
#  define PWR_SUSPENDRESUME             (2U)
#  define PWR_CRITICALRESUME            (3U)

/* ====== END OF WINDOWS 3.1 DDK ====== */

