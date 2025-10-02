
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

const char *target_cpu_str_t[CPU__MAX] = {
	"none",				// 0
	"intel x86"
};

static_assert( sizeof(target_cpu_str_t) / sizeof(target_cpu_str_t[0]) == CPU__MAX, "oops" );

const char *target_cpu_sub_str_t[CPU_SUB__MAX] = {
	"none",				// 0
	"x86:16",
	"x86:32",
	"x86:64"
};

static_assert( sizeof(target_cpu_sub_str_t) / sizeof(target_cpu_sub_str_t[0]) == CPU_SUB__MAX, "oops" );

const char *target_cpu_rev_str_t[CPU_REV__MAX] = {
	"none",				// 0
	"x86:8086",
	"x86:80186",
	"x86:80286",
	"x86:80386",
	"x86:80486",			// 5
	"x86:80586",
	"x86:80686"
};

static_assert( sizeof(target_cpu_rev_str_t) / sizeof(target_cpu_rev_str_t[0]) == CPU_REV__MAX, "oops" );

