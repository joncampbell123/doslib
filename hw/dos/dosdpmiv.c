
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

#if TARGET_MSDOS == 32 && !defined(TARGET_OS2)
int8_t dpmi_no_0301h = -1; /* whether or not the DOS extender provides function 0301h */
#endif

#if TARGET_MSDOS == 16 || (TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS))
uint16_t dpmi_flags=0;
uint16_t dpmi_version=0;
unsigned char dpmi_init=0;
uint32_t dpmi_entry_point=0;
unsigned char dpmi_present=0;
unsigned char dpmi_processor_type=0;
uint16_t dpmi_private_data_length_paragraphs=0;
uint16_t dpmi_private_data_segment=0xFFFF;	/* when we DO enter DPMI, we store the private data segment here. 0 = no private data. 0xFFFF = not yet entered */
#endif

