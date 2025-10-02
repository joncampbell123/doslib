
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

int32_t lgtok_cslitget(rbuf &buf,source_file_object &sfo,const bool unicode) {
	int32_t v;

	v = unicode ? getc(buf,sfo) : buf.getb();
	if (v == 0) return int32_t(-1);

	if (v == uint32_t('\\')) {
		v = unicode ? getc(buf,sfo) : buf.getb();

		switch (v) {
			case '\'':
			case '\"':
			case '\?':
			case '\\':
				return v;
			case '\r':
				if (buf.peekb() == '\n') buf.discardb(); /* \r\n */
				return unicode_nothing;
			case '\n':
				return unicode_nothing;
			case 'a':
				return int32_t(7);
			case 'b':
				return int32_t(8);
			case 'f':
				return int32_t(12);
			case 'n':
				return int32_t(10);
			case 'r':
				return int32_t(13);
			case 't':
				return int32_t(9);
			case 'v':
				return int32_t(11);
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7': {
				v = cc_parsedigit((unsigned char)v,8);
				assert(v >= int32_t(0));

				for (unsigned int c=0;c < 2;c++) {
					const int n = cc_parsedigit(buf.peekb(),8);
					if (n >= 0) {
						v = int32_t(((unsigned int)v << 3u) | (unsigned int)n);
						buf.discardb();
					}
					else {
						break;
					}
				}
				break; }
			case 'x': {
				int n,count=0;

				v = 0;
				while ((n=cc_parsedigit(buf.peekb(),16)) >= 0) {
					v = int32_t(((unsigned int)v << 4u) | (unsigned int)n);
					buf.discardb();
					count++;
				}
				if (count < 2) return unicode_eof;
				break; }
			default:
				return unicode_bad_escape;
		}
	}

	return v;
}

