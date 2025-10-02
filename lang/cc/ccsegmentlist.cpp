
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

//////////////////////////////////////////////////////////////////////////////

std::vector<segment_t>				segments;

//////////////////////////////////////////////////////////////////////////////

segment_t &segref(segment_id_t id) { /* we can't call it segment() that's reserved by some compilers */
#if 1//DEBUG
	if (id < segments.size())
		return segments[id];

	throw std::out_of_range("segment out of range");
#else
	return segments[id];
#endif
}

segment_id_t new_segment(void) {
	const segment_id_t r = segments.size();
	segments.resize(r+1u);
	return r;
}

segment_id_t find_segment(identifier_id_t name) {
	if (name != identifier_none) {
		for (const auto &s : segments) {
			if (s.name != identifier_none && identifier(s.name) == identifier(name))
				return (segment_id_t)(&s - &segments[0]);
		}
	}

	return segment_none;
}

//////////////////////////////////////////////////////////////////////////////

void default_segment_setup(segment_t &so) {
	if (target_cpu == CPU_INTEL_X86) {
		if (target_cpusub == CPU_SUB_X86_16) {
			so.limit = data_size_t(0x10000u); // 64KB
			so.use = segment_t::use_t::X86_16; // 16-bit
		}
		else if (target_cpusub == CPU_SUB_X86_32) {
			so.limit = data_size_t(0x100000000ull); // 4GB
			so.use = segment_t::use_t::X86_32; // 32-bit
		}
		else {
			so.use = segment_t::use_t::X86_64; // 64-bit
		}
	}
}

