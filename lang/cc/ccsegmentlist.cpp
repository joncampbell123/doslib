
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

