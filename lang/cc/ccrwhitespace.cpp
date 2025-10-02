
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

bool eat_whitespace(rbuf &buf,source_file_object &sfo) {
	bool r = false;

	do {
		if (buf.data_avail() < 1) rbuf_sfd_refill(buf,sfo);
		if (is_whitespace(buf.peekb())) { r = true; buf.discardb(); }
		else break;
	} while (1);

	return r;
}

void eat_newline(rbuf &buf,source_file_object &sfo) {
	if (buf.data_avail() < 4) rbuf_sfd_refill(buf,sfo);
	if (buf.peekb() == '\r') buf.discardb();
	if (buf.peekb() == '\n') buf.discardb();
}

