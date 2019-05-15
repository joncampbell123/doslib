
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

int isapnp_read_tag(uint8_t far **pptr,uint8_t far *fence,struct isapnp_tag *tag) {
	uint8_t far *ptr = *pptr;
	unsigned char c;

	if (ptr >= fence)
		return 0;

	c = *ptr++;
	if (c & 0x80) { /* large tag */
		tag->tag = 0x10 + (c & 0x7F);
		tag->len = *((uint16_t far*)(ptr));
		ptr += 2;
		tag->data = ptr;
		ptr += tag->len;
		if (ptr > fence)
			return 0;
	}
	else { /* small tag */
		tag->tag = (c >> 3) & 0xF;
		tag->len = c & 7;
		tag->data = ptr;
		ptr += tag->len;
		if (ptr > fence)
			return 0;
	}

	*pptr = ptr;
	return 1;
}

#endif //!defined(TARGET_PC98)

