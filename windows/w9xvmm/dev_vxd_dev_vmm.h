/* VMM device ID and services */

#define VMM_Device_ID                   0x0001

#define VMM_snr_Get_VMM_Device_ID           0x0000
#define VMM_snr_Get_Cur_VM_Handle           0x0001
#define VMM_snr_Test_Cur_VM_Handle          0x0002
#define VMM_snr_Get_Sys_VM_Handle           0x0003
#define VMM_snr_Test_Sys_VM_Handle          0x0004

/*==============================================================*/

/* VMM Get_VMM_Version
 *
 * in:
 *   none
 *
 * out:
 *   AH, AL = major, minor version number
 *   ECX = debug development revision number */
typedef struct Get_VMM_Version__response {
    unsigned short int  version;                /*  AX */
    unsigned int        debug_dev_rev;          /* ECX */
} Get_VMM_Version__response;

/* GCC's optimizer will turn the struct return into the register access we need. */
static inline Get_VMM_Version__response Get_VMM_Version(void) { /* returns via struct, all contents */
    register Get_VMM_Version__response r;

    __asm__ (
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Get_VMM_Device_ID)
        : /* outputs */ "=a" (r.version),
                        "=c" (r.debug_dev_rev)
        : /* inputs */ /* none */
        : /* clobbered */ /* none */);

    return r;
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

/* VMM Test_Cur_VM_Handle
 *
 * in:
 *   EBX = VM handle to test
 *
 * out:
 *   ZF = set if handle is for the current VM, clear if not */

static inline _Bool Test_Cur_VM_Handle(const vxd_vm_handle_t vm_handle) {
    register _Bool r;

#if (__GNUC__ >= 6 && __GNUC_MINOR__ >= 1) || (__GNUC__ >= 7)
    /* GCC 6.x with support for returning CPU flags */
    __asm__ (                                                                   \
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Test_Cur_VM_Handle)                   \
        : /* outputs */ "=@ccz" (r)                                             \
        : /* inputs */ "b" (vm_handle)                                          \
        : /* clobbered */);
#else
    /* GCC 4.x compatible inline ASM. GCC 4.x doesn't support storing ZF flag into result,
     * therefore we have to use setz */
    __asm__ (                                                                   \
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Test_Cur_VM_Handle)                   \
        "setzb %%al"                                                            \
        : /* outputs */ "=a" (r)                                                \
        : /* inputs */ "b" (vm_handle)                                          \
        : /* clobbered */);
#endif

    return r;
}

/*==============================================================*/

/* VMM Get_Sys_VM_Handle
 *
 * in:
 *   none
 *
 * out:
 *   EBX = Handle of the current VM */

static inline vxd_vm_handle_t Get_Sys_VM_Handle(void) {
    register vxd_vm_handle_t r;

    __asm__ (                                                                   \
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Get_Sys_VM_Handle)                    \
        : /* outputs */ "=b" (r)                                                \
        : /* inputs */                                                          \
        : /* clobbered */);

    return r;
}

/*==============================================================*/

/* VMM Test_Sys_VM_Handle
 *
 * in:
 *   EBX = VM handle to test
 *
 * out:
 *   ZF = set if handle is for the current VM, clear if not */

static inline _Bool Test_Sys_VM_Handle(const vxd_vm_handle_t vm_handle) {
    register _Bool r;

#if (__GNUC__ >= 6 && __GNUC_MINOR__ >= 1) || (__GNUC__ >= 7)
    /* GCC 6.x with support for returning CPU flags */
    __asm__ (                                                                   \
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Test_Sys_VM_Handle)                   \
        : /* outputs */ "=@ccz" (r)                                             \
        : /* inputs */ "b" (vm_handle)                                          \
        : /* clobbered */);
#else
    /* GCC 4.x compatible inline ASM. GCC 4.x doesn't support storing ZF flag into result,
     * therefore we have to use setz */
    __asm__ (                                                                   \
        VXD_AsmCall(VMM_Device_ID,VMM_snr_Test_Sys_VM_Handle)                   \
        "setzb %%al"                                                            \
        : /* outputs */ "=a" (r)                                                \
        : /* inputs */ "b" (vm_handle)                                          \
        : /* clobbered */);
#endif

    return r;
}

/*==============================================================*/

