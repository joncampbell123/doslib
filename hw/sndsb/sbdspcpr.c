
#include <hw/sndsb/sndsb.h>

/* uncomment this to enable debugging messages */
//#define DBG

#if defined(DBG)
# define DEBUG(x) (x)
#else
# define DEBUG(x)
#endif

int sndsb_read_dsp_copyright(struct sndsb_ctx *cx,char *buf,unsigned int buflen) {
	unsigned int i;
	int c;

	if (buflen == 0) return 0;
	sndsb_write_dsp(cx,0xE3);
	for (i=0;i < (buflen-1U);i++) {
		c = sndsb_read_dsp(cx);
		if (c < 0) break;
		cx->dsp_copyright[i] = (char)c;
		if (c == 0) break;
	}
	cx->dsp_copyright[i] = (char)0;
	DEBUG(fprintf(stdout,"sndsb_init_card() copyright == '%s'\n",cx->dsp_copyright));
	return 1;
}

