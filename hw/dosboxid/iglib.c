
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>

#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/8254/8254.h>		/* 8254 timer */

#include <hw/dosboxid/iglib.h>

#ifdef TARGET_PC98
uint16_t DOSBOXID_VAR dosbox_id_baseio = 0xDB28U;	// Default ports 0xDB28 - 0xDB2B
#else
uint16_t DOSBOXID_VAR dosbox_id_baseio = 0x28U;	// Default ports 0x28 - 0x2B
#endif

