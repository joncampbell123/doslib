
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

void cc_state_t::tq_ft(void) {
	token_t t(token_type_t::eof);
	int r;

	do {
		if (err == 0) {
			if ((r=lctok(pst,lst,*buf,*sfo,t)) < 1)
				err = r;
		}

		if (ignore_whitespace && err == 0) {
			if (t.type == token_type_t::newline)
				continue;
		}

		break;
	} while (1);

	tq.push_back(std::move(t));
}

void cc_state_t::tq_refill(const size_t i) {
	while (tq.size() < (i+tq_tail))
		tq_ft();
}

const token_t &cc_state_t::tq_peek(const size_t i) {
	tq_refill(i+1);
	assert((tq_tail+i) < tq.size());
	return tq[tq_tail+i];
}

void cc_state_t::tq_discard(const size_t i) {
	tq_refill(i);
	tq_tail += i;
	assert(tq_tail <= tq.size());

	if (tq_tail == tq.size()) {
		tq_tail = 0;
		tq.clear();
	}
}

token_t &cc_state_t::tq_get(void) {
	tq_refill();
	assert(tq_tail < tq.size());
	return tq[tq_tail++];
}

