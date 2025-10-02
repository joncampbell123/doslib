
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

segment_id_t					code_segment = segment_none;
segment_id_t					const_segment = segment_none;
segment_id_t					conststr_segment = segment_none;
segment_id_t					data_segment = segment_none;
segment_id_t					stack_segment = segment_none;
segment_id_t					bss_segment = segment_none;
segment_id_t					fardata_segment = segment_none;

