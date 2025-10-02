
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

source_file_object::source_file_object(const unsigned int new_iface) : iface(new_iface) {
}

source_file_object::~source_file_object() {
}

const char* source_file_object::getname(void) {
	return "(null)";
}

ssize_t source_file_object::read(void *,size_t) {
	return ssize_t(errno_return(ENOSYS));
}

void source_file_object::close(void) {
}

void source_file_object::common_move(source_file_object &) {
}

