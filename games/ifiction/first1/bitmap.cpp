
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

IFEBitmap::IFEBitmap() : bitmap(NULL), alloc(0), width(0), height(0), stride(0) {
}

IFEBitmap::~IFEBitmap() {
	free_storage();
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

