
#if !defined(TARGET_PC98)

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <fcntl.h>
#include <dos.h>

#include <dos/dos.h>
#include <isapnp/isapnp.h>

#include <hw/8254/8254.h>		/* 8254 timer */

const char *isapnp_config_block_str[3] = {
	"Allocated",
	"Possible",
	"Compatible"
};

const char *isapnp_fml32_miosize_str[4] = {
	"8-bit",
	"16-bit",
	"8/16-bit",
	"32-bit"
};

const char *isapnp_dma_speed_str[4] = {
	"Compatible",
	"Type A",
	"Type B",
	"Type F"
};

const char *isapnp_dma_xfer_preference_str[4] = {
	"8-bit",
	"8/16-bit",
	"16-bit",
	"??"
};

#endif //!defined(TARGET_PC98)

