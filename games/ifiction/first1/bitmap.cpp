
#if defined(USE_WIN32)
# include <windows.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include "ifict.h"
#include "utils.h"
#include "debug.h"
#include "fatal.h"
#include "bitmap.h"

IFEBitmap::IFEBitmap() : bitmap(NULL), alloc(0), width(0), height(0), stride(0), subrects(NULL), subrects_alloc(0) {
}

IFEBitmap::~IFEBitmap() {
	free_subrects();
	free_storage();
}

bool IFEBitmap::alloc_subrects(size_t count) {
	if (count == 0 || count > 2048)
		return false; /* be reasonable */

	if (subrects == NULL) {
		subrects = (subrect*)calloc(count,sizeof(subrect)); /* which will also initialize to zero */
		if (subrects == NULL) return false;
	}
	else if (count != subrects_alloc) {
		subrect *np = (subrect*)realloc((void*)subrects,count * sizeof(subrect));
		if (np == NULL) return false;
		subrects = np;

		/* zero out new entries */
		if (subrects_alloc < count)
			memset((void*)(subrects+subrects_alloc),0,(count - subrects_alloc) * sizeof(subrect));
	}

	subrects_alloc = count;
	return true;
}

void IFEBitmap::free_subrects(void) {
	if (subrects) {
		free(subrects);
		subrects = NULL;
	}
	subrects_alloc = 0;
}

IFEBitmap::subrect &IFEBitmap::get_subrect(const size_t i) {
	if (subrects == NULL || i >= subrects_alloc)
		IFEFatalError("IFEBitmap::get_subrect index out of range");

	return subrects[i];
}

bool IFEBitmap::alloc_storage(unsigned int w,unsigned int h) {
	size_t nalloc,nstride;

	if (w == 0 || h == 0 || w > 2048 || h > 2048)
		return false; /* Oh come now, this is a mid 1990s point and click. We didn't have none of that newfangled Ultra High Definition back then :) */

	nstride = (w + 3u) & (~3u); /* allocate and render DWORD aligned */
	nalloc = nstride * h;

	if (bitmap == NULL) {
		bitmap = (unsigned char*)malloc(nalloc);
		if (bitmap == NULL) return false;
	}
	else if (nalloc != alloc) { /* NTS: Maybe caller wants to resize to reduce memory footprint */
		unsigned char *np = (unsigned char*)realloc((void*)bitmap,nalloc);
		if (np == NULL) return false;
		bitmap = np;
	}

	width = w;
	height = h;
	alloc = nalloc;
	stride = nstride;
	return true;
}

void IFEBitmap::free_storage(void) {
	if (bitmap != NULL) {
		free(bitmap);
		bitmap = NULL;
	}
	width = 0;
	height = 0;
	alloc = 0;
	stride = 0;
}

unsigned char *IFEBitmap::row(const unsigned int y,const unsigned int x) {
	/* this version error and range checks */
	if (bitmap != NULL && y < height && x < width)
		return bitmap + (y * stride) + x;

	return NULL;
}

unsigned char *IFEBitmap::rowfast(const unsigned int y,const unsigned int x) {
	return bitmap + (y * stride) + x; /* for code that has already validated the bitmap */
}

