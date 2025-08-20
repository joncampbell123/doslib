
#include <sys/types.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#if defined(LINUX)
# include <time.h>
#endif

#include "libbmp.h"

struct rgb_t {
	unsigned char			r,g,b;
	uint32_t			weight;
};

struct octtree_entry_t {
	uint16_t			next[8];
	uint16_t			flags;
	uint32_t			weight;
};

#define OCTTREE_EMPTY (0u)
#define OCTFL_TAKEN (0x8000u)

struct octtree_buf_t {
	unsigned int			allocd,next;
	unsigned int			colors;
	struct octtree_entry_t*		map;
};

/* weight largest to smallest */
int octtree_rgb_weight_sort(const void *a,const void *b) {
	const struct rgb_t *ra = (const struct rgb_t*)a;
	const struct rgb_t *rb = (const struct rgb_t*)b;

	if (ra->weight != rb->weight)
		return rb->weight - ra->weight;
	if (ra->g != rb->g)
		return ra->g - rb->g;
	if (ra->r != rb->r)
		return ra->r - rb->r;
	if (ra->b != rb->b)
		return ra->b - rb->b;

	return 0;
}

unsigned int octtree_find_empty_slot(struct octtree_buf_t *bt) {
	while (bt->next < bt->allocd) {
		if (bt->map[bt->next].flags == 0)
			return bt->next;
		else
			(bt->next)++;
	}

	bt->next = 1;
	while (bt->next < bt->allocd) {
		if (bt->map[bt->next].flags == 0)
			return bt->next;
		else
			(bt->next)++;
	}

	return OCTTREE_EMPTY;
}

void free_octtree_struct(struct octtree_buf_t *bt) {
	if (bt->map) {
		free(bt->map);
		bt->map = NULL;
	}
	bt->colors = 0;
	bt->allocd = 0;
	bt->next = 0;
}

void free_octtree(struct octtree_buf_t **bt) {
	if (*bt) {
		free_octtree_struct(*bt);
		free(*bt);
		*bt = NULL;
	}
}

unsigned int octtree_gen_pal_sub(struct octtree_buf_t *bt,unsigned int colors,struct rgb_t *pal,unsigned int cent,unsigned char rb,unsigned char gb,unsigned char bb,unsigned char bstp) {
	unsigned int count = 0,subc = 0,i;

	assert(bt->map[cent].flags & OCTFL_TAKEN);

	for (i=0;i < 8;i++) {
		if (bt->map[cent].flags & (1u << i)) {
			const unsigned char nrb = rb + ((i&0x4)?bstp:0);
			const unsigned char ngb = gb + ((i&0x2)?bstp:0);
			const unsigned char nbb = bb + ((i&0x1)?bstp:0);
			const unsigned int u = bt->map[cent].next[i];

			if (bt->map[u].flags == OCTFL_TAKEN) {
				if (colors != 0) {
					pal->weight = bt->map[u].weight;
					pal->r = nrb;
					pal->g = ngb;
					pal->b = nbb;
					colors--;
					count++;
					pal++;
				}
			}
			else {
				subc = octtree_gen_pal_sub(bt,colors,pal,u,nrb,ngb,nbb,bstp>>1u);
				assert(subc <= colors);
				colors -= subc;
				count += subc;
				pal += subc;
			}
		}
	}

	return count;

}

unsigned int octtree_gen_pal(struct octtree_buf_t *bt,unsigned int colors,struct rgb_t *pal) {
	return octtree_gen_pal_sub(bt,colors,pal,0,0x00,0x00,0x00,0x80);
}

static const unsigned char oct_trimorder[8] = {
	0x1/*rgb=001*/,
	0x5/*rgb=101*/,
	0x4/*rgb=100*/,
	0x3/*rgb=011*/,
	0x6/*rgb=110*/,
	0x2/*rgb=010*/,
	0x7/*rgb=111*/,
	0x0/*rgb=000*/
};

struct octtree_weight_sort_t {
	uint16_t	weight;
	uint16_t	index;
};

int octtree_trim_sub(struct octtree_buf_t *bt,unsigned int to_colors,unsigned int *max_colors,unsigned int cent,unsigned int depthrem) {
	unsigned int mi,i;

	assert(bt->map[cent].flags & OCTFL_TAKEN);

	for (mi=0;mi < 8;mi++) {
		i = oct_trimorder[mi];
		if (bt->map[cent].flags & (1u << i)) {
			const unsigned int u = bt->map[cent].next[i];

			if (octtree_trim_sub(bt,to_colors,max_colors,u,depthrem ? (depthrem - 1u) : 0u))
				return 1;
		}
	}

	if (depthrem == 0u) {
		if (bt->map[cent].flags != OCTFL_TAKEN) {
			struct octtree_weight_sort_t srt[8],srttmp;
			unsigned int srti = 0;

			for (mi=0;mi < 8;mi++) {
				i = oct_trimorder[mi];
				if (bt->map[cent].flags & (1u << i)) {
					const unsigned int u = bt->map[cent].next[i];

					srt[srti].index = i;
					srt[srti].weight = bt->map[u].weight;
					srti++;
				}
			}

			if (srti > 1u) {
				unsigned int i=0;

				/* sort by weight */
				while ((i+1u) < srti) {
					if (srt[i].weight > srt[i+1u].weight) {
						memcpy(&srttmp,&srt[i],sizeof(srttmp));
						memcpy(&srt[i],&srt[i+1u],sizeof(srttmp));
						memcpy(&srt[i+1u],&srttmp,sizeof(srttmp));
						i = 0;
					}
					else {
						i++;
					}
				}
			}

			for (mi=0;mi < srti;mi++) {
				i = srt[mi].index;
				if (bt->map[cent].flags & (1u << i)) {
					const unsigned int u = bt->map[cent].next[i];

					if (bt->colors > to_colors && *max_colors != 0u) {
						if (bt->map[u].flags == OCTFL_TAKEN) {
							bt->map[cent].flags &= ~(1u << i);
							bt->map[u].flags = 0;
							bt->next = u;
							(*max_colors)--;
							bt->colors--;
						}
					}
				}
			}

			if (bt->map[cent].flags == OCTFL_TAKEN) {
				(*max_colors)++;
				bt->colors++;
			}
		}
	}

	return 0;
}

int octtree_trim(struct octtree_buf_t *bt,unsigned int to_colors,unsigned int max_colors) {
	unsigned int i;

	if (max_colors == 0)
		max_colors = bt->allocd;

	i = 7;
	do {
		if (octtree_trim_sub(bt,to_colors,&max_colors,0/*root node*/,i/*at this depth*/))
			return 1;
	} while (i-- != 0u);

	return 0;
}

int octtree_add(struct octtree_buf_t *bt,unsigned char r,unsigned char g,unsigned char b,unsigned char bits) {
	unsigned int cent = 0,nent;
	unsigned char idx;

	if (bits == 0)
		return 1;

	while (bits > 0) {
		assert(bt->map[cent].flags & OCTFL_TAKEN);

		idx = ((r>>(7u-2u))&0x4) + ((g>>(7u-1u))&0x2) + ((b>>(7u-0u))&0x1);
		r <<= 1u; g <<= 1u; b <<= 1u;
		bits--;

		if (bt->map[cent].flags & (1u << idx)) {
			cent = bt->map[cent].next[idx];
		}
		else {
			nent = octtree_find_empty_slot(bt);
			if (nent == OCTTREE_EMPTY) return 1;

			if (bt->map[cent].flags == OCTFL_TAKEN)
				bt->colors--;

			bt->map[cent].flags |= 1u << idx;
			cent = bt->map[cent].next[idx] = nent;
			bt->map[cent].flags = OCTFL_TAKEN;
			bt->map[cent].weight = 0;
			bt->colors++;
		}

		bt->map[cent].weight++;
	}

	return 0;
}

struct octtree_buf_t *alloc_octtree(unsigned int tsize) {
	struct octtree_buf_t *bt = malloc(sizeof(struct octtree_buf_t));
	size_t maxx = UINT_MAX / sizeof(struct octtree_buf_t);

	if (maxx > 0xFFFFu)
		maxx = 0xFFFFu;

	if (tsize == 0 || tsize > maxx)
		tsize = maxx;

	if (tsize < 1024)
		tsize = 1024;

	if (bt) {
		bt->allocd = tsize;
		bt->colors = 1;
		bt->next = 1;

		bt->map = malloc(sizeof(struct octtree_entry_t) * bt->allocd);
		if (!bt->map) {
			free_octtree(&bt);
			return NULL;
		}
		memset(bt->map,0,sizeof(struct octtree_entry_t) * bt->allocd);
		bt->map[0].flags = OCTFL_TAKEN;
	}

	return bt;
}

unsigned char map2palette(unsigned char r,unsigned char g,unsigned char b,struct rgb_t *pal,unsigned int colors) {
	unsigned long mdst = ~0lu,dst;
	unsigned int col = 0,i = 0,cd;

	while (i < colors) {
		cd = (unsigned int)abs((int)r - (int)(pal->r)); dst  = cd*cd;
		cd = (unsigned int)abs((int)g - (int)(pal->g)); dst += cd*cd;
		cd = (unsigned int)abs((int)b - (int)(pal->b)); dst += cd*cd;

		if (mdst > dst) {
			mdst = dst;
			col = i;
		}

		pal++;
		i++;
	}

	return col;
}

int main(int argc,char **argv) {
	struct BMPFILEIMAGE *membmp = NULL;
	struct BMPFILEIMAGE *memdst = NULL;
	unsigned int target_colors = 256;
	unsigned int final_colors = 256;
	unsigned int init_palette = 4096;
	struct octtree_buf_t *oct = NULL;
	unsigned char paldepthbits = 8;
	struct rgb_t *palette;
	unsigned int i;

	if (argc < 3)
		return 1;

	membmp = bmpfileimage_alloc();
	if (!membmp)
		return 1;

	memdst = bmpfileimage_alloc();
	if (!memdst)
		return 1;

	oct = alloc_octtree(0);
	if (!oct)
		return 1;

	fprintf(stderr,"%u nodes allocated\n",oct->allocd);

	if (argc > 3) {
		char *s = argv[3];

		target_colors = strtoul(s,&s,0);
		if (target_colors == 0u || target_colors > 256u)
			return 1;

		if (*s == ':') {
			s++;
			paldepthbits = strtoul(s,&s,0);
			if (paldepthbits == 0 || paldepthbits > 8)
				return 1;
		}
	}

	fprintf(stderr,"Quant to %u colors with %u bits per palette entry\n",target_colors,paldepthbits);

	palette = malloc(sizeof(struct rgb_t) * init_palette);
	if (!palette)
		return 1;

	memset(palette,0,sizeof(struct rgb_t) * init_palette);

	{
		struct BMPFILEREAD *bfr;

		bfr = open_bmp(argv[1]);
		if (bfr == NULL) {
			fprintf(stderr,"Failed to open BMP, errno %s\n",strerror(errno));
			return 1;
		}
		if (!(bfr->bpp == 24 || bfr->bpp == 32)) {
			fprintf(stderr,"BMP wrong format\n");
			return 1;
		}

		membmp->bpp = 24;
		membmp->width = bfr->width;
		membmp->height = bfr->height;

		if (bmpfileimage_alloc_image(membmp)) {
			fprintf(stderr,"Failed to allocate memory\n");
			return 1;
		}

		while (read_bmp_line(bfr) == 0) {
			unsigned char *dest = bmpfileimage_row(membmp,(unsigned int)bfr->current_line);
			assert(dest != NULL);

			if (bfr->bpp == 32)
				bitmap_memcpy32to24(dest,bfr->scanline,membmp->width,bfr);
			else
				memcpy(dest,bfr->scanline,membmp->stride);
		}

		/* done reading */
		close_bmp(&bfr);
	}

	memdst->bpp = 8;
	memdst->width = membmp->width;
	memdst->height = membmp->height;

	if (bmpfileimage_alloc_image(memdst)) {
		fprintf(stderr,"Failed to allocate memory\n");
		return 1;
	}

	/* make color palette */
	{
		unsigned char *s24;
		unsigned int x,y;

		for (y=0;y < membmp->height;y++) {
			s24 = bmpfileimage_row(membmp,y);
			assert(s24 != NULL);

			for (x=0;x < membmp->width;x++) {
				if (octtree_add(oct,s24[2],s24[1],s24[0],paldepthbits)) {
//					fprintf(stderr,"Need to trim from %u colors to add %u,%u,%u\n",oct->colors,s24[2],s24[1],s24[0]);
					if (octtree_trim(oct,target_colors,oct->allocd / 4u))
						fprintf(stderr,"Failure to trim\n");
					if (octtree_add(oct,s24[2],s24[1],s24[0],paldepthbits))
						fprintf(stderr,"Failed to add %u,%u,%u\n",s24[2],s24[1],s24[0]);
				}

				s24 += 3;
			}
		}

		fprintf(stderr,"%u colors in tree\n",oct->colors);
		octtree_trim(oct,init_palette,0);
		fprintf(stderr,"%u final colors in tree\n",oct->colors);
	}

	{
		unsigned int i;
		unsigned int gen = octtree_gen_pal(oct,init_palette,palette);
		fprintf(stderr,"%u/%u-color palette generated\n",gen,oct->colors);

		/* sort by weight */
		qsort(palette,gen,sizeof(struct rgb_t),octtree_rgb_weight_sort);

		/* while the palette is larger than target, merge together like colors,
		 * prioritizing the least used first */
		{
			unsigned int i,cd,chg,j,ogen;
			unsigned long dst,thr = 1ul;

			while (gen > target_colors) {
				chg = 0;
				i = gen / 3u;

				ogen = gen;
				while ((i+1u) < gen) {
					if (palette[i].weight) {
						cd = abs((int)palette[i].r - (int)palette[i+1].r); dst  = cd;
						cd = abs((int)palette[i].g - (int)palette[i+1].g); dst += cd;
						cd = abs((int)palette[i].b - (int)palette[i+1].b); dst += cd;

						if (dst < thr && ogen > target_colors) {
							palette[i].r = ((unsigned int)palette[i].r + (unsigned int)palette[i+1].r) / 2u;
							palette[i].g = ((unsigned int)palette[i].g + (unsigned int)palette[i+1].g) / 2u;
							palette[i].b = ((unsigned int)palette[i].b + (unsigned int)palette[i+1].b) / 2u;
							palette[i].weight += palette[i+1].weight;
							palette[i+1].weight = 0;
							chg = 1u;
							i += 2u;
							ogen--;
						}
						else {
							i++;
						}
					}
					else {
						i++;
					}
				}

#if 0
				if (ogen != gen)
					fprintf(stderr,"Removed %u -> %u colors\n",gen,ogen);
#endif

				if (chg) {
					i = j = 0;
					ogen = gen;
					while (i < ogen) {
						if (palette[i].weight) {
							if (i != j) memcpy(&palette[j],&palette[i],sizeof(struct rgb_t));
							j++;
						}
						else {
							gen--;
						}

						i++;
					}

#if 0
					fprintf(stderr,"reduced %u -> %u\n",ogen,gen);
#endif

					/* sort by weight again -- this seems to help with desaturation problems too */
					qsort(palette,gen,sizeof(struct rgb_t),octtree_rgb_weight_sort);
				}


				thr++;
			}
		}

		fprintf(stderr,"X:");
		for (i=0;i < gen;i++)
			fprintf(stderr," %02x%02x%02x:%u",palette[i].r,palette[i].g,palette[i].b,palette[i].weight);
		fprintf(stderr,"\n");

		final_colors = gen;
		assert(final_colors <= target_colors);
	}

	/* convert 24bpp to 8bpp image */
	{
		unsigned char *s24,*d8;
		unsigned int x,y;

		for (y=0;y < membmp->height;y++) {
			s24 = bmpfileimage_row(membmp,y);
			d8 = bmpfileimage_row(memdst,y);
			assert(d8 != NULL && s24 != NULL);

			for (x=0;x < membmp->width;x++) {
				*d8++ = map2palette(s24[2],s24[1],s24[0],palette,final_colors); /* Windows BMPs are BGR order */
				s24 += 3;
			}
		}
	}

	/* write it out */
	{
		struct BMPFILEWRITE *bfw;
		unsigned char *s;
		unsigned int y;

		bfw = create_write_bmp();
		if (!bfw) {
			fprintf(stderr,"Cannot alloc write bmp\n");
			return 1;
		}

		bfw->bpp = 8; // TODO: 2 colors = 1bpp  3-16 colors = 4bpp   anything else 8bpp
		bfw->width = membmp->width;
		bfw->height = membmp->height;
		bfw->colors_used = final_colors;

		if (createpalette_write_bmp(bfw)) {
			fprintf(stderr,"Cannot make write bmp palette\n");
			return 1;
		}

		if (bfw->palette) {
			bfw->colors_used = final_colors;
			for (i=0;i < final_colors;i++) {
				bfw->palette[i].rgbRed = palette[i].r;
				bfw->palette[i].rgbGreen = palette[i].g;
				bfw->palette[i].rgbBlue = palette[i].b;
			}
		}

		if (open_write_bmp(bfw,argv[2])) {
			fprintf(stderr,"Cannot write bitmap\n");
			return 1;
		}

		for (y=0;y < membmp->height;y++) {
			s = bmpfileimage_row(memdst,bfw->current_line);
			assert(s != NULL);

			if (write_bmp_line(bfw,s,bfw->stride)) {
				fprintf(stderr,"Scanline write err\n");
				return 1;
			}
		}

		close_write_bmp(&bfw);
	}

	/* free bitmap */
	bmpfileimage_free(&membmp);
	bmpfileimage_free(&memdst);
	free_octtree(&oct);
	free(palette);
	return 0;
}

