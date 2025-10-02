
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

const char *type_qualifier_idx_t_str[TSI__MAX] = {
	"const",		// 0
	"volatile",
	"near",
	"far",
	"huge",
	"restrict"		// 5
};

