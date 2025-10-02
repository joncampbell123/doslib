
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

template <const csliteral_t::type_t cslt,typename ptrat> int lgtok_strlit_wrch(ptrat* &p,ptrat* const f,const unicode_char_t v) = delete;

template <> int lgtok_strlit_wrch<csliteral_t::type_t::CHAR,unsigned char>(unsigned char* &p,unsigned char* const f,const unicode_char_t v) {
	if (v < 0x00 || v > 0xFF)
		return errno_return(EINVAL);

	assert((p+1) <= f);
	*p++ = (unsigned char)v;
	return 1;
}

template <> int lgtok_strlit_wrch<csliteral_t::type_t::UTF8,unsigned char>(unsigned char* &p,unsigned char* const f,const unicode_char_t v) {
	if (v < 0x00)
		return errno_return(EINVAL);

	utf8_to_str(p,f,v);
	assert(p <= f);
	return 1;
}

template <> int lgtok_strlit_wrch<csliteral_t::type_t::UNICODE16,uint16_t>(uint16_t* &p,uint16_t* const f,const unicode_char_t v) {
	if (v < 0x00l || v > 0x20FFFFl)
		return errno_return(EINVAL);

	utf16_to_str(p,f,v);
	assert(p <= f);
	return 1;
}

template <> int lgtok_strlit_wrch<csliteral_t::type_t::UNICODE32,uint32_t>(uint32_t* &p,uint32_t* const f,const unicode_char_t v) {
	if (v < 0x00)
		return errno_return(EINVAL);

	assert((p+1) <= f);
	*p++ = (uint32_t)v;
	return 1;
}

template <const csliteral_t::type_t cslt,typename ptrat> int lgtok_strlit_common_inner(rbuf &buf,source_file_object &sfo,const unsigned char separator,ptrat* &p,ptrat* const &f) {
	const bool unicode = !(cslt == csliteral_t::type_t::CHAR);
	int32_t v;
	int rr;

	do {
		if (buf.peekb() == separator) {
			buf.discardb();
			break;
		}

		if ((p+16) >= f)
			CCERR_RET(ENAMETOOLONG,buf.pos,"String constant is too long");

		v = lgtok_cslitget(buf,sfo,unicode);
		if (v == unicode_nothing) continue;
		if (v == unicode_bad_escape) CCERR_RET(EINVAL,buf.pos,"Invalid escape");
		if ((rr=lgtok_strlit_wrch<cslt,ptrat>(p,f,v)) <= 0) CCERR_RET(rr,buf.pos,"invalid encoding for string");
	} while(1);

	return 1;
}

template <const csliteral_t::type_t cslt,typename ptrat> int lgtok_strlit_common(rbuf &buf,source_file_object &sfo,token_t &t,const unsigned char separator) {
	assert(t.type == token_type_t::charliteral || t.type == token_type_t::strliteral || t.type == token_type_t::anglestrliteral);
	ptrat *p,*f;
	int rr;

	if (!csliteral(t.v.csliteral).alloc((4096+20)*sizeof(ptrat)))
		return errno_return(ENOMEM);

	p = (ptrat*)((char*)csliteral(t.v.csliteral).data);
	f = (ptrat*)((char*)csliteral(t.v.csliteral).data+csliteral(t.v.csliteral).length);

	rr = lgtok_strlit_common_inner<cslt,ptrat>(buf,sfo,separator,p,f);

	{
		const size_t fo = size_t(p-((ptrat*)csliteral(t.v.csliteral).data)) * sizeof(ptrat);
		assert(fo <= csliteral(t.v.csliteral).allocated);
		csliteral(t.v.csliteral).length = fo;
		csliteral(t.v.csliteral).shrinkfit();
	}

	return rr;
}

int lgtok_charstrlit(rbuf &buf,source_file_object &sfo,token_t &t,const csliteral_t::type_t cslt) {
	unsigned char separator = buf.peekb();

	if (separator == '\'' || separator == '\"' || separator == '<') {
		buf.discardb();

		assert(t.type == token_type_t::none);
		if (separator == '\"') t.type = token_type_t::strliteral;
		else if (separator == '<') t.type = token_type_t::anglestrliteral;
		else t.type = token_type_t::charliteral;
		t.v.csliteral = csliteral.alloc();
		csliteral(t.v.csliteral).type = cslt;

		if (separator == '<') separator = '>';

		switch (cslt) {
			case csliteral_t::type_t::CHAR:
				return lgtok_strlit_common<csliteral_t::type_t::CHAR,unsigned char>(buf,sfo,t,separator);
			case csliteral_t::type_t::UTF8:
				return lgtok_strlit_common<csliteral_t::type_t::UTF8,unsigned char>(buf,sfo,t,separator);
			case csliteral_t::type_t::UNICODE16:
				return lgtok_strlit_common<csliteral_t::type_t::UNICODE16,uint16_t>(buf,sfo,t,separator);
			case csliteral_t::type_t::UNICODE32:
				return lgtok_strlit_common<csliteral_t::type_t::UNICODE32,uint32_t>(buf,sfo,t,separator);
			default:
				break;
		}

		return 1;
	}
	else {
		abort();//should not happen
	}
}

