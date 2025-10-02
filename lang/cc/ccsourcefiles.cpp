
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

std::vector<source_file_t> source_files;

size_t alloc_source_file(const std::string &path) {
	size_t i=0;

	for (;i < source_files.size();i++) {
		if (source_files[i].path == path)
			return i;
	}

	assert(i == source_files.size());

	{
		source_file_t nt;
		nt.path = path;
		source_files.push_back(std::move(nt));
	}

	return i;
}

void clear_source_file(const size_t i) {
	if (i < source_files.size())
		source_files[i].clear();
}

void release_source_file(const size_t i) {
	if (i < source_files.size()) {
		if (source_files[i].refcount > 0)
			source_files[i].refcount--;
		if (source_files[i].refcount == 0)
			clear_source_file(i);
	}
}

void addref_source_file(const size_t i) {
	if (i < source_files.size())
		source_files[i].refcount++;
}

void source_file_refcount_check(void) {
	for (size_t i=0;i < source_files.size();i++) {
		if (source_files[i].refcount != 0) {
			fprintf(stderr,"Leftover refcount=%u for source '%s'\n",
					source_files[i].refcount,
					source_files[i].path.c_str());
		}
	}
}

