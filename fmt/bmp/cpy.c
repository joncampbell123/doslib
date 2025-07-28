
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#include "libbmp.h"

void bitmap_memcpy(unsigned char BMPFAR *d,const unsigned char BMPFAR *s,unsigned int l) {
#if TARGET_MSDOS == 16
	_fmemcpy(d,s,l);
#else
	memcpy(d,s,l);
#endif
}

