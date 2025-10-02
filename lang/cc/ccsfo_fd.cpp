
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

source_fd::source_fd() {
}

source_fd::source_fd(const int new_fd/*takes ownership*/,const std::string &new_name) : source_file_object(IF_FD), name(new_name), fd(new_fd) {
}

source_fd::~source_fd() {
	close();
}

void source_fd::common_move(source_fd &x) {
	name = std::move(x.name);
	fd = x.fd; x.fd = -1;
}

const char* source_fd::getname(void) {
	return name.c_str();
}

ssize_t source_fd::read(void *buffer,size_t count) {
	if (fd >= 0) {
		const int r = ::read(fd,buffer,count);
		if (r >= 0) {
			assert(size_t(r) <= count);
			return r;
		}
		else {
			return ssize_t(errno_return(r));
		}
	}

	return ssize_t(errno_return(EBADF));
}

void source_fd::close(void) {
	if (fd >= 0) {
		::close(fd);
		fd = -1;
	}
}

