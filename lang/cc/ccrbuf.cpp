
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

rbuf::rbuf() { pos.col=1; pos.row=1; pos.ofs=0; }
rbuf::~rbuf() { free(); }

rbuf::rbuf(rbuf &&x) { common_move(x); }
rbuf &rbuf::operator=(rbuf &&x) { common_move(x); return *this; }

void rbuf::set_source_file(const size_t i) {
	if (i == source_file) return;

	if (source_file != no_source_file) {
		release_source_file(source_file);
		source_file = no_source_file;
	}

	if (i < source_files.size()) {
		source_file = i;
		addref_source_file(source_file);
	}
}

void rbuf::common_move(rbuf &x) {
	assert(x.sanity_check());
	base  = x.base;  x.base = NULL;
	data  = x.data;  x.data = NULL;
	end   = x.end;   x.end = NULL;
	fence = x.fence; x.fence = NULL;
	flags = x.flags; x.flags = 0;
	err   = x.err;   x.err = 0;
	pos   = x.pos;
	source_file = x.source_file; x.source_file = no_source_file;
	assert(sanity_check());
}

size_t rbuf::buffer_size(void) const {
	return size_t(fence - base);
}

size_t rbuf::data_offset(void) const {
	return size_t(data - base);
}

size_t rbuf::data_avail(void) const {
	return size_t(end - data);
}

size_t rbuf::can_write(void) const {
	return size_t(fence - end);
}

bool rbuf::sanity_check(void) const {
	return (base <= data) && (data <= end) && (end <= fence);
}

void rbuf::free(void) {
	if (base) {
		set_source_file(no_source_file);
		::free((void*)base);
		base = data = end = fence = NULL;
	}
}

unsigned char rbuf::peekb(const size_t ofs) {
	if ((data+ofs) < end) return data[ofs];
	return 0;
}

void rbuf::discardb(size_t count) {
	while (data < end && count > 0) {
		pos_track(*data++);
		count--;
	}
}

unsigned char rbuf::getb(void) {
	if (data < end) { pos_track(*data); return *data++; }
	return 0;
}

bool rbuf::allocate(const size_t sz) {
	if (base != NULL || sz < 64 || sz > 1024*1024)
		return false;

	if ((base=data=end=(unsigned char*)::malloc(sz+1)) == NULL)
		return false;

	fence = base + sz;
	*fence = 0;
	return true;
}

void rbuf::flush(void) {
	if (data_offset() != 0) {
		const size_t mv = data_avail();

		assert(sanity_check());
		if (mv != 0) memmove(base,data,mv);
		data = base;
		end = data + mv;
		assert(sanity_check());
	}
}

void rbuf::lazy_flush(void) {
	if (data_offset() >= (buffer_size()/2))
		flush();
}

void rbuf::pos_track(const unsigned char c) {
	pos.ofs++;
	if (c == '\r') {
	}
	else if (c == '\n') {
		pos.col=1;
		pos.row++;
	}
	else if (c < 0x80 || c >= 0xc0) {
		pos.col++;
	}
}

void rbuf::pos_track(const unsigned char *from,const unsigned char *to) {
	while (from < to) pos_track(*from++);
}

