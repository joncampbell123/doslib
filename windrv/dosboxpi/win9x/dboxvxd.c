/* NOTE: Because of our ASM interfacing needs, you must compile this with
 *       GCC, not Open Watcom. */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <hw/dos/exelehdr.h>

#if 1

static inline void outp(const uint16_t port,const uint8_t data) {
    __asm__ __volatile__ ("outb %%al,%%dx" : /* no output */ : /* inputs */ "d" (port), "a" (data));
}

static inline uint32_t inpd(const uint16_t port) {
    register uint32_t data;

    __asm__ __volatile__ ("inl %%dx,%%eax" : /* outputs */ "=a" (data) : /* inputs */ "d" (port));

    return data;
}

static inline void outpd(const uint16_t port,const uint32_t data) {
    __asm__ __volatile__ ("outl %%eax,%%dx" : /* no output */ : /* inputs */ "d" (port), "a" (data));
}

#endif

#include <hw/dosboxid/iglib.h>

void vxd_control_proc(void);

#if 1

uint16_t dosbox_id_baseio = 0x28;

int probe_dosbox_id() {
	uint32_t t;

	if (!dosbox_id_reset()) return 0;

	t = dosbox_id_read_identification();
	if (t != DOSBOX_ID_IDENTIFICATION) return 0;

	return 1;
}

uint32_t dosbox_id_read_regsel() {
	uint32_t r;

	dosbox_id_reset_latch();

#if TARGET_MSDOS == 32
	r  = (uint32_t)inpd(DOSBOX_IDPORT(DOSBOX_ID_INDEX));
#else
	r  = (uint32_t)inpw(DOSBOX_IDPORT(DOSBOX_ID_INDEX));
	r |= (uint32_t)inpw(DOSBOX_IDPORT(DOSBOX_ID_INDEX)) << (uint32_t)16UL;
#endif

	return r;
}

uint32_t dosbox_id_read_data_nrl() {
	uint32_t r;

#if TARGET_MSDOS == 32
	r  = (uint32_t)inpd(DOSBOX_IDPORT(DOSBOX_ID_DATA));
#else
	r  = (uint32_t)inpw(DOSBOX_IDPORT(DOSBOX_ID_DATA));
	r |= (uint32_t)inpw(DOSBOX_IDPORT(DOSBOX_ID_DATA)) << (uint32_t)16UL;
#endif

	return r;
}

uint32_t dosbox_id_read_data() {
	dosbox_id_reset_latch();
	return dosbox_id_read_data_nrl();
}


int dosbox_id_reset() {
	uint32_t t1,t2;

	/* on reset, data should return DOSBOX_ID_RESET_DATA_CODE and index should return DOSBOX_ID_RESET_INDEX_CODE */
	dosbox_id_reset_interface();
	t1 = dosbox_id_read_data();
	t2 = dosbox_id_read_regsel();
	if (t1 != DOSBOX_ID_RESET_DATA_CODE || t2 != DOSBOX_ID_RESET_INDEX_CODE) return 0;
	return 1;
}

uint32_t dosbox_id_read_identification() {
	/* now read the identify register */
	dosbox_id_write_regsel(DOSBOX_ID_REG_IDENTIFY);
	return dosbox_id_read_data();
}

void dosbox_id_write_regsel(const uint32_t reg) {
	dosbox_id_reset_latch();

	outpd(DOSBOX_IDPORT(DOSBOX_ID_INDEX),reg);
}

void dosbox_id_write_data_nrl(const uint32_t val) {
	outpd(DOSBOX_IDPORT(DOSBOX_ID_DATA),val);
}

void dosbox_id_write_data(const uint32_t val) {
	dosbox_id_reset_latch();
	dosbox_id_write_data_nrl(val);
}

#endif


/* USEFUL */
#if defined(__GNUC__)
# define VXD_INT3()                 __asm__ __volatile__ ("int $3")
#else
# define VXD_INT3()                 __asm int 3
#endif

#define VXD_STRINGIFY2(x)   #x
#define VXD_STRINGIFY(x)    VXD_STRINGIFY2(x)

#define VXD_AsmCall(dev,srv) \
        "int    $0x20\n"                                                        \
        ".word  " VXD_STRINGIFY(srv) "\n"                                       \
        ".word  " VXD_STRINGIFY(dev) "\n"

typedef uint32_t vxd_vm_handle_t;

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
#define VMM_snr_Get_VMM_Device_ID       0x0000
#define VMM_snr_Get_Cur_VM_Handle       0x0001
#define Debug_Device_ID             0x0002
#define VPICD_Device_ID             0x0003
#define VDMAD_Device_ID             0x0004

/*==============================================================*/

/* VMM Get_VMM_Version
 *
 * in:
 *   none
 *
 * out:
 *   AH, AL = major, minor version number
 *   ECX = debug development revision number */
#define Get_VMM_Version_raw(ax,ecx)                                             \
    __asm__ (                                                                   \
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Get_VMM_Device_ID)                    \
        : /* outputs */ "=a" (ax), "=c" (ecx)                                   \
        : /* inputs */                                                          \
        : /* clobbered */)

typedef struct Get_VMM_Version__response {
    unsigned short int  version;                /* EAX */
    unsigned int        debug_dev_rev;          /* ECX */
} Get_VMM_Version__response;

/* GCC's optimizer will turn the struct return into the register access we need. */
static inline Get_VMM_Version__response Get_VMM_Version(void) { /* returns only AX */
    register Get_VMM_Version__response r;

    Get_VMM_Version_raw(r.version,r.debug_dev_rev);

    return r;
}

/* GCC's optimizer will turn the struct return into the register access we need. */
static inline unsigned short int Get_VMM_Version__ax(void) { /* returns only AX */
    return Get_VMM_Version().version;
}

/*==============================================================*/

/* VMM Get_Cur_VM_Handle
 *
 * in:
 *   none
 *
 * out:
 *   EBX = Handle of the current VM */

static inline vxd_vm_handle_t Get_Cur_VM_Handle(void) {
    register vxd_vm_handle_t r;

    __asm__ (                                                                   \
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Get_Cur_VM_Handle)                    \
        : /* outputs */ "=b" (r)                                                \
        : /* inputs */                                                          \
        : /* clobbered */);

    return r;
}

/*==============================================================*/

/* useful macros */

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
    const register int sysvm_handle asm("ebx");

    System_VM_Handle = sysvm_handle;

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
void my_device_init(void) {
    if (Get_VMM_Version__ax() < 0x30A)
        goto fail;

    if (!probe_dosbox_id())
        goto fail;

    VXD_CF_SUCCESS();
    return;

fail:
    VXD_CF_FAILURE();
}

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
void my_sys_vm_init(void) {
    const register int sysvm_handle asm("ebx");

    System_VM_Handle = sysvm_handle;

    VXD_CF_SUCCESS();
}

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
void my_set_device_focus(void) {
    const register int vm_handle asm("ebx");

    if (Focus_VM_Handle != vm_handle) {
        Focus_VM_Handle = vm_handle;

        if (Focus_VM_Handle == System_VM_Handle) {
            dosbox_id_write_regsel(DOSBOX_ID_REG_USER_MOUSE_CURSOR_NORMALIZED);
            dosbox_id_write_data(1); /* PS/2 mouse */
        }
        else {
            dosbox_id_write_regsel(DOSBOX_ID_REG_USER_MOUSE_CURSOR_NORMALIZED);
            dosbox_id_write_data(0); /* disable */
        }
    }

    VXD_CF_SUCCESS();
}

/* VxD control message Sys_VM_Terminate.
 *
 * Entry:
 *   EAX = Sys_VM_Terminate
 *   EBX = handle of System VM
 *
 * Exit:
 *   Carry flag = clear if success, set if failure */
void my_sys_vm_terminate(void) {
    if (Focus_VM_Handle != 0) {
        dosbox_id_write_regsel(DOSBOX_ID_REG_USER_MOUSE_CURSOR_NORMALIZED);
        dosbox_id_write_data(0); /* disable */
    }

    System_VM_Handle = 0;
    VXD_CF_SUCCESS();
}

/* WARNING: When compiled with GCC you must use -fomit-frame-pointer to prevent this
 *          dispatch routine from pushing anything on the stack or setting up any
 *          stack frame. */
void vxd_control_proc(void) {
    VXD_CONTROL_DISPATCH(Sys_Critical_Init, my_sys_critical_init);
    VXD_CONTROL_DISPATCH(Sys_VM_Init,       my_sys_vm_init);
    VXD_CONTROL_DISPATCH(Sys_VM_Terminate,  my_sys_vm_terminate);
    VXD_CONTROL_DISPATCH(Set_Device_Focus,  my_set_device_focus);
    VXD_CONTROL_DISPATCH(Device_Init,       my_device_init);
    VXD_CF_SUCCESS();
}

