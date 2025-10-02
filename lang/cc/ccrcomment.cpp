
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

int eat_c_comment(unsigned int level,rbuf &buf,source_file_object &sfo) {
	/* caller already ate the / and the * if level == 1 */
	assert(level == 0 || level == 1);

	do {
		if (buf.data_avail() < 8) rbuf_sfd_refill(buf,sfo);

		if (buf.peekb(0) == '/' && buf.peekb(1) == '*') {
			buf.discardb(2);
			level++;
		}
		else if (buf.peekb(0) == '*' && buf.peekb(1) == '/') {
			buf.discardb(2);
			if (--level == 0) break;
		}
		else {
			buf.discardb();
		}
	} while(1);

	return 1;
}

int eat_cpp_comment(rbuf &buf,source_file_object &sfo) {
	/* caller already ate the / and the / */

	do {
		if (buf.data_avail() < 8) rbuf_sfd_refill(buf,sfo);

		if (buf.peekb() == '\r' || buf.peekb() == '\n')
			break;
		else
			buf.discardb();
	} while(1);

	return 1;
}

