
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

csliteral_pool_t csliteral;

////////////////////////////////////////////////////////////////////

const unsigned char csliteral_t::unit_size[type_t::__MAX__] = { 1, 1, 2, 4 };

////////////////////////////////////////////////////////////////////

csliteral_t &csliteral_t::clear(void) {
	free();
	return *this;
}

csliteral_t &csliteral_t::addref(void) {
	ref++;
	return *this;
}

csliteral_t &csliteral_t::release(void) {
	if (ref == 0) throw std::runtime_error("csliteral attempt to release when ref == 0");
	if (--ref == 0) clear();
	return *this;
}

void csliteral_t::csliteral_t::free(void) {
	if (data) ::free(data);
	data = NULL;
	length = 0;
	allocated = 0;
	type = type_t::CHAR;
}

bool csliteral_t::copy_from(const csliteral_t &x) {
	free();
	type = x.type;
	assert(data == NULL);
	assert(length == 0);
	assert(allocated == 0);

	if (x.length != 0) {
		if (!alloc(x.length))
			return false;

		assert(data != NULL);
		assert(x.data != NULL);
		assert(length == x.length);
		memcpy(data,x.data,x.length);
	}

	return true;
}

const char *csliteral_t::as_char(void) const { return (const char*)data; }
const unsigned char *csliteral_t::as_binary(void) const { return (const unsigned char*)data; }
const unsigned char *csliteral_t::as_utf8(void) const { return (const unsigned char*)data; }
const uint16_t *csliteral_t::as_u16(void) const { return (const uint16_t*)data; }
const uint32_t *csliteral_t::as_u32(void) const { return (const uint32_t*)data; }

bool csliteral_t::operator==(const csliteral_t &rhs) const {
	if (length != rhs.length) return false;

	if ((type == type_t::CHAR || type == type_t::UTF8) == (rhs.type == type_t::CHAR || rhs.type == type_t::UTF8)) { /* OK */ }
	else if (type != rhs.type) return false;

	return memcmp(data,rhs.data,length) == 0;
}

bool csliteral_t::operator==(const std::string &rhs) const {
	if (length != rhs.size() || !(type == type_t::CHAR || type == type_t::UTF8)) return false;
	return memcmp(data,rhs.data(),length) == 0;
}

size_t csliteral_t::unitsize(void) const {
	return unit_size[type];
}

size_t csliteral_t::units(void) const {
	return length / unit_size[type];
}

bool csliteral_t::alloc(const size_t sz) {
	if (data != NULL || sz == 0 || sz >= (1024*1024))
		return false;

	data = malloc(sz);
	if (data == NULL)
		return false;

	length = sz;
	allocated = sz;
	return true;
}

std::string csliteral_t::to_escape(const uint32_t c) {
	char tmp[32];

	sprintf(tmp,"\\x%02lx",(unsigned long)c);
	return std::string(tmp);
}

bool csliteral_t::shrinkfit(void) {
	if (data) {
		if (allocated > length) {
			/* NTS: GNU glibc realloc(data,0) frees the buffer */
			if (length == 0) {
				free();
			}
			else {
				void *np = ::realloc(data,length);
				if (np == NULL)
					return false;

				data = np;
				allocated = length;
			}
		}
	}

	return true;
}

std::string csliteral_t::makestring(void) const {
	if (data) {
		std::string s;

		switch (type) {
			case type_t::CHAR: {
				const unsigned char *p = as_binary();
				const unsigned char *f = p + length;
				while (p < f) {
					if (*p < 0x20 || *p >= 0x80)
						s += to_escape(*p++);
					else
						s += (char)(*p++);
				}
				break; }
			case type_t::UTF8: {
				const unsigned char *p = as_binary();
				const unsigned char *f = p + length;
				while (p < f) {
					const int32_t v = p_utf8_decode(p,f);
					if (v < 0l)
						s += "?";
					else if (v < 0x20l || v > 0x10FFFFl)
						s += to_escape(v);
					else
						s += utf8_to_str((uint32_t)v);
				}
				break; }
			case type_t::UNICODE16: {
				const uint16_t *p = as_u16();
				const uint16_t *f = p + units();
				while (p < f) {
					const int32_t v = p_utf16_decode(p,f);
					if (v < 0l)
						s += "?";
					else if (v < 0x20l || v > 0x10FFFFl)
						s += to_escape(v);
					else
						s += utf8_to_str((uint32_t)v);
				}
				break; }
			case type_t::UNICODE32: {
				const uint32_t *p = as_u32();
				const uint32_t *f = p + units();
				while (p < f) {
					if (*p < 0x20u || *p > 0x10FFFFu)
						s += to_escape(*p++);
					else
						s += utf8_to_str(*p++);
				}
				break; }
			default:
				break;
		}

		return s;
	}

	return std::string();
}

std::string csliteral_t::to_str(void) const {
	std::string s;
	char tmp[128];

	if (data) {
		s += "v=\"";
		s += makestring();
		s += "\"";
	}

	s += " t=\"";
	switch (type) {
		case type_t::CHAR:       s += "char"; break;
		case type_t::UTF8:       s += "utf8"; break;
		case type_t::UNICODE16:  s += "unicode16"; break;
		case type_t::UNICODE32:  s += "unicode32"; break;
		default: break;
	}
	s += "\"";

	sprintf(tmp," lenb=%zu lenu=%zu",length,units());
	s += tmp;

	return s;
}

