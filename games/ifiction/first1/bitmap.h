
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* this holds a bitmap.
 * it might hold multiple images combined into one bitmap, especially for animation,
 * in which case, regions within the bitmap are drawn */
class IFEBitmap {
public:
	unsigned char*		bitmap;
	size_t			alloc;
	unsigned int		width,height,stride;
public:
	IFEBitmap();
	~IFEBitmap();
public:
	bool alloc_storage(unsigned int w,unsigned int h);
	void free_storage(void);
};

