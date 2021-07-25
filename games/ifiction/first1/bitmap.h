
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* this holds a bitmap.
 * it might hold multiple images combined into one bitmap, especially for animation,
 * in which case, regions within the bitmap are drawn */
class IFEBitmap {
public:
	struct subrect {
		iferect_t	r;
		short int	offset_x,offset_y;	/* draw displacement of bitmap i.e. cursor hot spot */
		uint8_t		index_bias;		/* this offset has been added to the pixels after loading */
		bool		has_mask;		/* image is twice the width of r.w, right half is mask to AND screen pixels by when doing a transparent BitBlt */
	};
public:
	unsigned char*		bitmap;			/* allocated bitmap */
	unsigned char*		bitmap_first_row;	/* first row in bitmap to count from, last scanline if upside down DIB in Windows 3.1 (stride < 0) */
	size_t			alloc;
	unsigned int		width,height;
	int			stride;			/* positive if top-down DIB, negative if bottom-up DIB */

	subrect*		subrects;
	size_t			subrects_alloc;

	IFEPaletteEntry*	palette;
	size_t			palette_alloc;
	size_t			palette_size;

	bool			must_lock;		/* must lock_surface() before access */
	unsigned int		is_locked;		/* is locked, lock count */
	bool			ctrl_palette;		/* palette can be controlled/allocated/freed and is stored in the bitmap (screen objects will say no) */
	bool			ctrl_storage;		/* storage can be allocated/freed (not screen objects) */
	bool			ctrl_bias;		/* can bias subrects (not screen objects) */
	bool			ctrl_subrect;		/* can allocate/free subrects (not screen objects) */
public:
	IFEBitmap();
	virtual ~IFEBitmap();
public:
	virtual bool lock_surface(void);
	virtual bool unlock_surface(void);
	virtual bool alloc_palette(size_t count);
	virtual void free_palette(void);
	virtual bool alloc_storage(unsigned int w,unsigned int h);
	virtual void free_storage(void);
	virtual bool alloc_subrects(size_t count);
	virtual void free_subrects(void);
	subrect &get_subrect(const size_t i);
	const subrect &get_subrect(const size_t i) const;
	unsigned char *row(const unsigned int y,const unsigned int x=0);
	unsigned char *rowfast(const unsigned int y,const unsigned int x=0);
	const unsigned char *row(const unsigned int y,const unsigned int x=0) const;
	const unsigned char *rowfast(const unsigned int y,const unsigned int x=0) const;
	virtual bool bias_subrect(subrect &s,uint8_t new_bias); /* bias (add to pixel values) for simple palette remapping */
};

