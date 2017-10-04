
#include <hw/sndsb/sndsb.h>

/* uncomment this to enable debugging messages */
//#define DBG

#if defined(DBG)
# define DEBUG(x) (x)
#else
# define DEBUG(x)
#endif

int sndsb_query_dsp_version(struct sndsb_ctx * const cx) {
    unsigned char tmp[2];

	if (!sndsb_write_dsp(cx,SNDSB_DSPCMD_GET_VERSION))
		return 0;

    if (sndsb_bread_dsp(cx,tmp,2) != 2)
        return 0;

	cx->dsp_vmaj = (uint8_t)tmp[0];
	cx->dsp_vmin = (uint8_t)tmp[1];
	DEBUG(fprintf(stdout,"sndsb_query_dsp_version() == v%u.%u\n",a,b));
	return 1;
}

