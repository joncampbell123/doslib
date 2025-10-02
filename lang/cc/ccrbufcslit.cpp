
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

int rbuf_copy_csliteral(rbuf &dbuf,csliteral_id_t &csid) {
	dbuf.free();

	if (csid == csliteral_none)
		return 1;

	csliteral_t &cslit = csliteral(csid);
	if (cslit.length == 0)
		return 1;

	if (!dbuf.allocate(std::max(cslit.length,size_t(128)))) /*allocate will reject small amounts*/
		return errno_return(ENOMEM);

	memcpy(dbuf.data,cslit.data,cslit.length);
	dbuf.end = dbuf.data + cslit.length;
	return 1;
}

