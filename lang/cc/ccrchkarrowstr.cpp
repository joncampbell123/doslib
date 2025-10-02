
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>

#include "cc.hpp"

bool looks_like_arrowstr(rbuf &buf,source_file_object &sfo) {
	rbuf_sfd_refill(buf,sfo);

	unsigned char *p = buf.data,*f = buf.end;

	if (p >= f) return false;
	if (*p != '<') return false;
	p++;

	while (p < f && is_hchar(*p)) p++;

	if (p >= f) return false;
	if (*p != '>') return false;
	p++;

	return true;
}

