
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <malloc.h>
#include <stdio.h>
#include <conio.h>
#include <fcntl.h>
#include <dos.h>
#include <i86.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/dos/dosbox.h>
#include <windows/ntvdm/ntvdmlib.h>

const char *ntvdm_RM_ERR_str(uint16_t c) {
	switch (c) {
		case ntvdm_RM_ERR_DLL_NOT_FOUND:	return "DLL not found";
		case ntvdm_RM_ERR_DISPATCH_NOT_FOUND:	return "Dispatch routine not found";
		case ntvdm_RM_ERR_INIT_NOT_FOUND:	return "Init routine not found";
		case ntvdm_RM_ERR_NOT_ENOUGH_MEMORY:	return "Not enough memory";
		/* our own */
		case ntvdm_RM_ERR_NOT_AVAILABLE:	return "Interface not available";
	}

	return "?";
}

