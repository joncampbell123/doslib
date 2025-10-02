
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

/* target settings */
target_cpu_t					target_cpu = CPU_NONE;
target_cpu_sub_t				target_cpusub = CPU_SUB_NONE;
target_cpu_rev_t				target_cpurev = CPU_REV_NONE;

/* data types */
data_type_set_t					data_types = data_types_default;
data_type_set_ptr_t				data_types_ptr_code = data_ptr_types_default;
data_type_set_ptr_t				data_types_ptr_data = data_ptr_types_default;
data_type_set_ptr_t				data_types_ptr_stack = data_ptr_types_default;

addrmask_t					default_packing = addrmask_none;
addrmask_t					current_packing = addrmask_none;
std::vector<pack_state_t>			packing_stack; /* #pragma pack */

