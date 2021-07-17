
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* this holds a bitmap.
 * it might hold multiple images combined into one bitmap, especially for animation,
 * in which case, regions within the bitmap are drawn */
class IFEBitmap {
public:
	struct subrect {
		unsigned short	x,y,w,h;
		short int	offset_x,offset_y;	/* draw displacement of bitmap i.e. cursor hot spot */
	};
public:
	unsigned char*		bitmap;
	size_t			alloc;
	unsigned int		width,height,stride;

	subrect*		subrects;
	size_t			subrects_alloc;
public:
	IFEBitmap();
	~IFEBitmap();
public:
	bool alloc_storage(unsigned int w,unsigned int h);
	void free_storage(void);
	bool alloc_subrects(size_t count);
	void free_subrects(void);
	subrect &get_subrect(const size_t i);
	unsigned char *row(const unsigned int y,const unsigned int x=0);
	unsigned char *rowfast(const unsigned int y,const unsigned int x=0);
};

