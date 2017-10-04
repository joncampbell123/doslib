
#include <hw/sndsb/sndsb.h>

/* this MUST follow conio.h */
#define DOSLIB_REDEFINE_INP
#include <hw/cpu/liteio.h>

int sndsb_read_mixer(struct sndsb_ctx *cx,uint8_t i) {
	outp(cx->baseio+SNDSB_BIO_MIXER_INDEX,i);
	return inp(cx->baseio+SNDSB_BIO_MIXER_DATA);
}

void sndsb_write_mixer(struct sndsb_ctx *cx,uint8_t i,uint8_t d) {
	outp(cx->baseio+SNDSB_BIO_MIXER_INDEX,i);
	outp(cx->baseio+SNDSB_BIO_MIXER_DATA,d);
}

