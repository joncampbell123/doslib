
#ifndef __WINDOWS_NTVDM_NTVDMLIB_H
#define __WINDOWS_NTVDM_NTVDMLIB_H

/* this does NOT apply to OS/2 */
#if !defined(TARGET_OS2)

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include <conio.h>
#include <fcntl.h>
#include <dos.h>
#include <i86.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/dosbox.h>

/* NTVDMLIB library separation:
 *
 *  If compiled as MS-DOS, or Win16, the library provides functions to
 *  allow this code (as a client of NTVDM.EXE) to talk to NTVDM and load
 *  Win32 code.
 *
 *  If compiled as Win32, the library provides functions to talk to NTVDM.EXE
 *  as an external service (Win32 processes locating NTVDM.EXE and manipulating
 *  the DOS VM or Win16 world within).
 *
 *  BUT: We also allow the code using us to indicate that it is a VDD driver */
#if TARGET_MSDOS == 32 && defined(TARGET_WINDOWS)
# ifdef NTVDM_VDD_DRIVER
# else
#  define NTVDM_EXTERNAL 1
#endif
#else
# ifdef NTVDM_VDD_DRIVER
#  error You cant be a VDD driver as a 16bit DOS app dumbass
# else
#  define NTVDM_CLIENT 1
# endif
#endif

#if defined(NTVDM_CLIENT)
int i_am_ntvdm_client();
#else
# define i_am_ntvdm_client(x) (0)
#endif

/* WARNING: The way into NTVDM.EXE is basically an invalid encoding of "LDS" which NTVDM.EXE
 *          catched by the invalid opcode exception. If we aren't under NTVDM.EXE we will
 *          crash. */
#define ntvdm_RM_ERR_DLL_NOT_FOUND		0xFFFFU
#define ntvdm_RM_ERR_DISPATCH_NOT_FOUND		0xFFFEU
#define ntvdm_RM_ERR_INIT_NOT_FOUND		0xFFFDU
#define ntvdm_RM_ERR_NOT_ENOUGH_MEMORY		0xFFFCU
#define ntvdm_RM_ERR_NOT_AVAILABLE		0xFFF0U		/* this is not returned by NTVDM.EXE, this is one of our own */
#define ntvdm_RM_ERR(x)				(((uint16_t)(x)) >= 0xFFF0U)

#define ntvdm_Dispatch_ins_asm_db \
	db		0xC4,0xC4,0x58,0x02

const char *ntvdm_RM_ERR_str(uint16_t c);
void ntvdm_UnregisterModule(uint16_t handle);
uint16_t ntvdm_RegisterModule(const char *dll_name,const char *dll_init_routine,const char *dll_dispatch_routine);

#if !defined(TARGET_WINDOWS) && TARGET_MSDOS == 32
int ntvdm_rm_code_alloc();
void ntvdm_DispatchCall_dpmi(uint16_t handle,struct dpmi_realmode_call *rc);
#else
static inline int ntvdm_rm_code_alloc() { return 1; }
#endif

#endif /* !defined(TARGET_OS2) */

#endif /* __WINDOWS_NTVDM_NTVDMLIB_H */

