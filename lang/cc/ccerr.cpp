
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

void CCerr(const position_t &pos,const char *fmt,...) {
	va_list va;

	fprintf(stderr,"Error");
	if (pos.row > 0 || pos.col > 0) fprintf(stderr,"(%d,%d)",pos.row,pos.col);
	fprintf(stderr,": ");

	va_start(va,fmt);
	vfprintf(stderr,fmt,va);
	va_end(va);
	fprintf(stderr,"\n");
}

