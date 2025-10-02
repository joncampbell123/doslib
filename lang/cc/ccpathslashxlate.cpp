
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

void path_slash_translate(std::string &path) {
	for (auto &c : path) {
		if (c == '\\') c = '/';
	}
}

