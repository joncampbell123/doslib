
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

void cc_state_t::tq_ft_chking(void) {
	size_t pi = tq.size(),ei;
	tq_ft();

	/* tq_ft() will always append even if eof tokens */
	assert(pi < tq.size());

	/* look for consecutive string segments and combine together.
	 * they must be the same type of string---no combining UTF-8 and UTF-16 together! */
	if (tq[pi].type == token_type_t::strliteral) {
		do {
			ei = tq.size();
			tq_ft();
			assert(ei < tq.size());
		} while (tq[ei].type == token_type_t::strliteral && tq[ei].type == tq[pi].type);

		if ((ei-pi) >= 2u) { /* multiples */
			token_t sav = std::move(tq.back()); /* save the last non-string token */
			tq.pop_back(); assert(tq.size() == ei);

			size_t cmblen = 0;
			for (size_t i=pi;i < ei;i++)
				cmblen += csliteral(tq[i].v.csliteral).length;

			/* create a new csliteral and allocate the combined size */
			const csliteral_id_t ncs = csliteral.alloc();
			if (ncs == csliteral_none) { err = -1; return; /*FIXME*/ }
			csliteral_t &nc = csliteral(ncs);
			if (!nc.alloc(cmblen)) { err = -1; return; /*FIXME*/ }
			assert(nc.data != NULL);

			/* copy all literals into the one big new literal */
			size_t wrp = 0;
			for (size_t i=pi;i < ei;i++) {
				csliteral_t &n = csliteral(tq[i].v.csliteral);
				if (n.length) {
					assert(n.data != NULL);
					assert((wrp+n.length) <= nc.length);
					memcpy((unsigned char*)nc.data+wrp,n.data,n.length);
					wrp += n.length;
				}
				csliteral.release(/*&*/tq[i].v.csliteral);/*also clears csliteral by reference*/
			}
			assert(wrp == nc.length);

			/* truncate the queue to remove all strliterals we combined, and then append
			 * the new csliteral along with the token we saved. */
			tq.resize(pi+1u);
			tq[pi] = token_t(token_type_t::strliteral,tq[pi].pos,tq[pi].source_file);
			tq[pi].v.csliteral = ncs;
			tq.push_back(std::move(sav));
		}
	}
}

void cc_state_t::tq_refill(const size_t i) {
	while (tq.size() < (i+tq_tail))
		tq_ft_chking();
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

