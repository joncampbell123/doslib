
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

bool ptrmergeable(const ddip_t &to,const ddip_t &from) {
	if (!to.parameters.empty() || !from.parameters.empty())
		return false;
	if (!to.arraydef.empty())
		return false;
	if (to.dd_flags != 0 || from.dd_flags != 0)
		return false;

	if (!to.ptr.empty() || !from.ptr.empty())
		return true;

	return false;
}

bool arraymergeable(const ddip_t &to,const ddip_t &from) {
	if (!to.parameters.empty() || !from.parameters.empty())
		return false;
	if (!to.ptr.empty() || !from.ptr.empty())
		return false;
	if (to.dd_flags != 0 || from.dd_flags != 0)
		return false;

	if (!to.arraydef.empty() || !from.arraydef.empty())
		return true;

	return false;
}

