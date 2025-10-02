
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

csliteral_pool_t csliteral;

////////////////////////////////////////////////////////////////////

const unsigned char csliteral_t::unit_size[type_t::__MAX__] = { 1, 1, 2, 4 };

