
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

identifier_t &identifier_t::clear(void) {
	free_data();
	return *this;
}

identifier_t &identifier_t::addref(void) {
	ref++;
	return *this;
}

identifier_t &identifier_t::release(void) {
	if (ref == 0) throw std::runtime_error("identifier attempt to release when ref == 0");
	if (--ref == 0) clear();
	return *this;
}

identifier_t::identifier_t() { }
identifier_t::identifier_t(identifier_t &&x) { common_move(x); }
identifier_t &identifier_t::operator=(identifier_t &&x) { common_move(x); return *this; }

identifier_t::~identifier_t() { free(); }

void identifier_t::free_data(void) {
	if (data) { ::free(data); data = NULL; }
	length = 0;
}

void identifier_t::free(void) {
	if (ref != 0) fprintf(stderr,"WARNING: identifier str free() to object with %u outstanding references\n",ref);
	ref = 0;

	free_data();
}

void identifier_t::common_move(identifier_t &other) {
	ref = other.ref; other.ref = 0;
	data = other.data; other.data = NULL;
	length = other.length; other.length = 0;
}

bool identifier_t::copy_from(const unsigned char *in_data,const size_t in_length) {
	free_data();

	if (in_length != 0 && in_length < 4096) {
		data = (unsigned char*)::malloc(in_length);
		if (data == NULL) return false;
		memcpy(data,in_data,in_length);
		length = in_length;
	}

	return true;
}

bool identifier_t::copy_from(const std::string &s) {
	return copy_from((const unsigned char*)s.c_str(),s.length());
}

std::string identifier_t::to_str(void) const {
	if (data != NULL && length != 0) return std::string((char*)data,length);
	return std::string();
}

bool identifier_t::operator==(const identifier_t &rhs) const {
	if (length != rhs.length) return false;
	if (length != 0) return memcmp(data,rhs.data,length) == 0;
	return true;
}

bool identifier_t::operator==(const csliteral_t &rhs) const {
	if (length != rhs.length) return false;
	if (length != 0) return memcmp(data,rhs.data,length) == 0;
	return true;
}

bool identifier_t::operator==(const std::string &rhs) const {
	if (length != rhs.length()) return false;
	if (length != 0) return memcmp(data,rhs.data(),length) == 0;
	return true;
}

