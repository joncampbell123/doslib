
#ifdef TARGET_WINDOWS
# include <windows.h>
#endif

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/dos/dosntvdm.h>

#if TARGET_MSDOS == 16 || (TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS))
unsigned char dpmi_entered = 0;	/* 0=not yet entered, 16=entered as 16bit, 32=entered as 32bit */
uint64_t dpmi_rm_entry = 0;
uint32_t dpmi_pm_entry = 0;

/* once having entered DPMI, keep track of the selectors registers given to us in p-mode */
uint16_t dpmi_pm_cs,dpmi_pm_ds,dpmi_pm_es,dpmi_pm_ss;
#endif

