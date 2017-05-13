
/* VXD control messages.
 * Windows 95 DDK level.
 * Messages for Windows 98 and higher are defined elsewhere.
 */

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

/* Last message in Windows 3.1 DDK
#define Power_Event                 0x001A
 */

/* VxD control message SYS_DYNAMIC_DEVICE_INIT.
 *
 * Meaning:
 *   The virtual device, as a dynamically loadable VxD, is being loaded
 *   and should perform any required initialization.
 *
 * Entry:
 *   EAX = SYS_DYNAMIC_DEVICE_INIT
 *
 * Exit:
 *   Carry flag = clear if success, set if failure
 *
 * Notes:
 *   Control dispatch routines for dynamically loadable VxDs should be in
 *   locked memory.
 *
 *   This message is sent by VXDLDR_LoadDevice with VXDLDR_INIT_DEVICE, or
 *   by the application that initializes the VxD.
 */
#define SYS_DYNAMIC_DEVICE_INIT     0x001B

/* VxD control message SYS_DYNAMIC_DEVICE_EXIT.
 *
 * Meaning:
 *   [FIXME: Not explained?]
 *
 * Entry:
 *   EAX = SYS_DYNAMIC_DEVICE_EXIT
 *
 * Exit:
 *   Carry flag = clear if success, set if failure
 *
 * Notes:
 *   Control dispatch routines for dynamically loadable VxDs should be in
 *   locked memory.
 */
#define SYS_DYNAMIC_DEVICE_EXIT     0x001C

/* ====== END OF WINDOWS 95 DDK ====== */

