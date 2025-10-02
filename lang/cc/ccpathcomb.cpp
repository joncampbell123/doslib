
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

std::string path_combine(const std::string &base,const std::string &rel) {
	if (!base.empty() && !rel.empty())
		return base + "/" + rel;
	if (!rel.empty())
		return rel;

	return std::string();
}

