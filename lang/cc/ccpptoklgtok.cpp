
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

int pptok_lgtok(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
	if (!pst.macro_expansion.empty()) {
		t = std::move(pst.macro_expansion.front());
		pst.macro_expansion.pop_front();
		return 1;
	}
	else {
		pst.macro_expansion_counter = 0;
		return lgtok(lst,buf,sfo,t);
	}
}

void pptok_lgtok_ungetch(pptok_state_t &pst,token_t &t) {
	pst.macro_expansion.push_front(std::move(t));
}

