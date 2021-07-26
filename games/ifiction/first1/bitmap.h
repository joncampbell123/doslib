
#ifndef IFE_BITMAP_H
#define IFE_BITMAP_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* this holds a bitmap.
 * it might hold multiple images combined into one bitmap, especially for animation,
 * in which case, regions within the bitmap are drawn */
class IFEBitmap {
public:
	enum image_type_t {
		IMT_OPAQUE=0,
		IMT_TRANSPARENT_MASK=1,

		IMT_MAX
	};
	enum subclass_t {
		SC_BASE=0,
		SC_SCREEN=1
	};
public:
	struct subrect {
		iferect_t	r;
		short int	offset_x,offset_y;	/* draw displacement of bitmap i.e. cursor hot spot */
		uint8_t		index_bias;		/* this offset has been added to the pixels after loading */

		void		reset(void);
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

	iferect_t		scissor;
	enum image_type_t	image_type;

	enum subclass_t		subclass;
public:
	IFEBitmap(enum subclass_t _subclass=SC_BASE);
	virtual ~IFEBitmap();
public:
	virtual bool lock_surface(void);
	virtual bool unlock_surface(void);
	virtual bool alloc_palette(size_t count);
	virtual void free_palette(void);
	virtual bool alloc_storage(unsigned int w,unsigned int h,enum image_type_t new_image_type=IMT_OPAQUE);
	virtual void free_storage(void);
	virtual bool alloc_subrects(size_t count);
	virtual void free_subrects(void);
	subrect &get_subrect(const size_t i);
	const subrect &get_subrect(const size_t i) const;
	unsigned char *row(const unsigned int y);
	unsigned char *rowfast(const unsigned int y);
	const unsigned char *row(const unsigned int y) const;
	const unsigned char *rowfast(const unsigned int y) const;
	virtual bool bias_subrect(subrect &s,uint8_t new_bias); /* bias (add to pixel values) for simple palette remapping */
	void set_scissor_rect(int x1,int y1,int x2,int y2);
	void reset_scissor_rect(void);

	inline unsigned int get_transparent_mask_offset(void) const { /* assume IMT_TRANSPARENT_MASK */
		return (width + 3u) & (~3u); /* at width rounded up to DWORD boundary */
	}
};

class IFEScreenBitmap : public IFEBitmap {
public:
	IFEScreenBitmap(enum subclass_t _subclass=SC_SCREEN);
	virtual ~IFEScreenBitmap();
public:
	virtual bool alloc_palette(size_t count);
	virtual void free_palette(void);
	virtual bool alloc_storage(unsigned int w,unsigned int h,enum image_type_t new_image_type=IMT_OPAQUE);
	virtual void free_storage(void);
	virtual bool alloc_subrects(size_t count);
	virtual void free_subrects(void);
	virtual bool bias_subrect(subrect &s,uint8_t new_bias); /* bias (add to pixel values) for simple palette remapping */
};

#endif //IFE_BITMAP_H

