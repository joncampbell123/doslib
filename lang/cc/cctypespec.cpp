
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

const char *type_specifier_idx_t_str[TSI__MAX] = {
	"void",			// 0
	"char",
	"short",
	"int",
	"long",
	"float",		// 5
	"double",
	"signed",
	"unsigned",
	"longlong",
	"enum",			// 10
	"struct",
	"union",
	"matches-typedef",
	"matches-builtin",
	"sz8",			// 15
	"sz16",
	"sz32",
	"sz64"
};

