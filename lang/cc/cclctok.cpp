
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

int lctok(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
	int r;

	if ((r=pptok(pst,lst,buf,sfo,t)) < 1)
		return r;

	/* it might be a reserved keyword, check */
	if (t.type == token_type_t::identifier) {
		for (const ident2token_t *i2t=ident2tok_cc;i2t < (ident2tok_cc+ident2tok_cc_length);i2t++) {
			if (identifier(t.v.identifier).length == i2t->len) {
				if (!memcmp(identifier(t.v.identifier).data,i2t->str,i2t->len)) {
					t.clear_v();
					t.type = token_type_t(i2t->token);
					return 1;
				}
			}
		}
	}

	return 1;
}

