
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

int rbuf_sfd_refill(rbuf &buf,source_file_object &sfo) {
	assert(buf.base != NULL);
	assert(buf.sanity_check());
	buf.lazy_flush();

	const size_t to_rd = buf.can_write();
	if (to_rd != size_t(0)) {
		const ssize_t rd = sfo.read(buf.end,to_rd);
		if (rd > 0) {
			buf.end += rd;
			assert(buf.sanity_check());
		}
		else if (rd < 0) {
			return (buf.err=errno_return(rd));
		}
		else {
			buf.flags |= rbuf::PFL_EOF;
			return 0;
		}
	}
	else if (buf.err) {
		return buf.err;
	}
	else if (buf.flags & rbuf::PFL_EOF) {
		return 0;
	}

	return 1;
}

