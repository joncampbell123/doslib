
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

uint8_t pptok_state_t::macro_hash_id(const identifier_id_t &i) const {
	return macro_hash_id(identifier(i));
}

pptok_state_t::pptok_macro_ent_t::pptok_macro_ent_t() { }
pptok_state_t::pptok_macro_ent_t::~pptok_macro_ent_t() {
	identifier.release(name);
}

void pptok_state_t::include_push(source_file_object *sfo) {
	if (sfo != NULL) {
		include_t i;
		i.sfo = sfo;
		i.rb.set_source_file(alloc_source_file(sfo->getname()));
		assert(i.rb.allocate());
		include_stk.push(std::move(i));
	}
}

void pptok_state_t::include_pop(void) {
	if (!include_stk.empty()) {
		include_t &e = include_stk.top();
		e.rb.set_source_file(no_source_file);
		if (e.sfo) delete e.sfo;
		include_stk.pop();
	}
}

void pptok_state_t::include_popall(void) {
	while (!include_stk.empty()) {
		include_t &e = include_stk.top();
		e.rb.set_source_file(no_source_file);
		if (e.sfo) delete e.sfo;
		include_stk.pop();
	}
}

bool pptok_state_t::condb_true(void) const {
	if (cond_block.empty())
		return true;
	else
		return cond_block.top().state == cond_block_t::IS_TRUE;
}

const pptok_state_t::pptok_macro_ent_t* pptok_state_t::lookup_macro(const identifier_id_t &i) const {
	const pptok_macro_ent_t *p = macro_buckets[macro_hash_id(i)];
	while (p != NULL) {
		if (identifier(p->name) == identifier(i))
			return p;

		p = p->next;
	}

	return NULL;
}

bool pptok_state_t::create_macro(const identifier_id_t i,pptok_macro_t &m) {
	pptok_macro_ent_t **p = &macro_buckets[macro_hash_id(i)];

	while ((*p) != NULL) {
		if (identifier((*p)->name) == identifier(i)) {
			/* already exists. */
			/* this is an error, unless the attempt re-states the same macro and definition,
			 * which is not an error.
			 *
			 * not an error:
			 *
			 * #define X 5
			 * #define X 5
			 *
			 * error:
			 *
			 * #define X 5
			 * #define X 4 */
			if (	(*p)->ment.tokens == m.tokens &&
					(*p)->ment.parameters == m.parameters &&
					(*p)->ment.flags == m.flags)
				return true;

			return false;
		}

		p = &((*p)->next);
	}

	(*p) = new pptok_macro_ent_t;
	(*p)->ment = std::move(m);
	identifier.assign(/*to*/(*p)->name,/*from*/i);
	(*p)->next = NULL;
	return true;
}

bool pptok_state_t::delete_macro(const identifier_id_t i) {
	pptok_macro_ent_t **p = &macro_buckets[macro_hash_id(i)];
	while ((*p) != NULL) {
		if (identifier((*p)->name) == identifier(i)) {
			pptok_macro_ent_t* d = *p;
			(*p) = (*p)->next;
			delete d;
			return true;
		}

		p = &((*p)->next);
	}

	return false;
}

void pptok_state_t::free_macro_bucket(const unsigned int bucket) {
	pptok_macro_ent_t *d;

	while ((d=macro_buckets[bucket]) != NULL) {
		macro_buckets[bucket] = d->next;
		delete d;
	}
}

void pptok_state_t::free_macros(void) {
	for (unsigned int i=0;i < macro_bucket_count;i++)
		free_macro_bucket(i);
}

uint8_t pptok_state_t::macro_hash_id(const unsigned char *data,const size_t len) {
	unsigned int h = 0x2222;

	for (size_t c=0;c < len;c++)
		h = (h ^ (h << 9)) + data[c];

	h ^= (h >> 16);
	h ^= ~(h >> 8);
	h ^= (h >> 4) ^ 3;
	return h & (macro_bucket_count - 1u);
}

pptok_state_t::pptok_state_t() { }
pptok_state_t::~pptok_state_t() { include_popall(); free_macros(); }

