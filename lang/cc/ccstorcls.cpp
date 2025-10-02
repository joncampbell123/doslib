
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

const char *storage_class_idx_t_str[SCI__MAX] = {
	"typedef",		// 0
	"extern",
	"static",
	"auto",
	"register",
	"constexpr",		// 5
	"inline",
	"consteval",
	"constinit"
};

