/* emm.h
 *
 * Expanded Memory Manager library.
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

#ifndef __HW_DOS_EMM_H
#define __HW_DOS_EMM_H

#if !defined(TARGET_OS2) && !defined(TARGET_WINDOWS)

#include <stdio.h>
#include <stdint.h>

/* FIXME: 32-bit protected mode: Who the fuck keeps changing the
          value of ES?!? Watcom and our code rely on ES == DS! */
#if TARGET_MSDOS == 32
# define sanity() _sanity(__LINE__,__FILE__)
static inline void _sanity(unsigned int line,const char *f) {
	uint16_t d=0,e=0;

	__asm {
		mov	ax,ds
		mov	d,ax
		mov	ax,es
		mov	e,ax
	}

	if (d != e) {
		fprintf(stderr,"%s(%u) DS(%X) != ES(%X)\n",f,line,d,e);
		abort();
	}
}
#else
# define sanity()
#endif

#pragma pack(push,1)
struct emm_phys_page_map {
	uint16_t	segment;
	uint16_t	number;
};
#pragma pack(pop)

extern unsigned char emm_status;
extern unsigned char emm_present;
extern unsigned char emm_version;
extern unsigned char emm_phys_pages;
extern unsigned short emm_total_pages;
extern unsigned int emm_page_frame_segment;
extern unsigned short emm_unallocated_pages;
extern struct emm_phys_page_map *emm_phys_map;

int probe_emm();
void emm_phys_pages_sort();
void emm_update_page_count();
int emm_alloc_pages(unsigned int pages);
int emm_free_pages(unsigned int handle);
unsigned short emm_last_phys_page_segment(unsigned int phys_page);
int emm_map_page(unsigned int handle,unsigned int phys_page,unsigned int log_page);

#define emm_was_probed() (emm_status != 0xFF)

#endif /* !defined(TARGET_OS2) && !defined(TARGET_WINDOWS) */

#endif /* __HW_DOS_EMM_H */

