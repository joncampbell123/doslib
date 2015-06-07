/* testemm.c
 *
 * Test program: Expanded Memory Manager functions
 * (C) 2011-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/emm.h>
#include <hw/dos/doswin.h>

static const char *message = "Hello world. How are you?";
static const char *message2 = "Pingas. Sup dawg. Hello world. Dork. Hahahajajaja.";
static char tmp[128],tmp2[128];

#if 1
# define x_memcpy(a,b,c) memcpy(a,b,c)
#else
/* what have we come to when friggin' memcpy() causes a GPF?!? */
static void x_memcpy(unsigned char *dst,const unsigned char *src,size_t c) {
	fprintf(stderr,"memcpy %p -> %p (%lu)\n",
		dst,src,(unsigned long)c);

	while (c != 0) {
	    *dst++ = *src++;
	    c--;
	}
}
#endif

int main() {
	size_t message_l = strlen(message),message2_l = strlen(message2);

	probe_dos();
	printf("DOS version %x.%02u\n",dos_version>>8,dos_version&0xFF);
	if (detect_windows()) {
		printf("I am running under Windows.\n");
		printf("    Mode: %s\n",windows_mode_str(windows_mode));
		printf("    Ver:  %x.%02u\n",windows_version>>8,windows_version&0xFF);
	}
	else {
		printf("Not running under Windows or OS/2\n");
	}

	sanity();
	if (probe_emm()) {
		int h1,h2,h3;

		sanity();
		printf("Expanded memory manager present. Status = 0x%02x Page frame @0x%04x v%x.%x\n",emm_status,emm_page_frame_segment,emm_version>>4,emm_version&0x0F);
		emm_update_page_count();
		sanity();
		printf("  Unallocated pages:     %u (%luKB)\n",
			emm_unallocated_pages,
			(unsigned long)emm_unallocated_pages << 4UL); /* 2^14 = 16384 */
		printf("  Total pages:           %u (%luKB)\n",
			emm_total_pages,
			(unsigned long)emm_total_pages << 4UL);
		printf("  Physical pages:        %u (%luKB)\n",
			emm_phys_pages,
			(unsigned long)emm_phys_pages << 4UL);

		while (getch() != 13);
		sanity();

		/* print out the mapping table, if available */
		if (emm_phys_map != NULL) {
			struct emm_phys_page_map *me;
			unsigned int i;

			printf("Physical page to segment table\n");
			for (i=0;i < emm_phys_pages;i++) {
				me = emm_phys_map + i;
				printf("  %02x: 0x%04x",me->number,me->segment);
				if ((i%5) == 4) printf("\n");
			}
			printf("\n");
			sanity();
		}

		printf("Allocating EMM pages (1): ");
		h1 = emm_alloc_pages(1);
		sanity();
		if (h1 >= 0) {
			printf("OK, handle=%u\n",h1);
			if (!emm_free_pages(h1)) printf("cannot free\n");
		}
		else printf("FAILED\n");

		printf("Allocating EMM pages (16KB): ");
		h1 = emm_alloc_pages(1);
		sanity();
		if (h1 >= 0) printf("OK, handle=%u\n",h1);
		else printf("FAILED\n");

		printf("Allocating EMM pages (1MB): ");
		h2 = emm_alloc_pages(0x100000UL >> 14UL);
		sanity();
		if (h2 >= 0) printf("OK, handle=%u\n",h2);
		else printf("FAILED\n");

		printf("Allocating EMM pages (12MB): ");
		h3 = emm_alloc_pages(0xC00000UL >> 14UL);
		sanity();
		if (h3 >= 0) printf("OK, handle=%u\n",h3);
		else printf("FAILED\n");

		while (getch() != 13);

		if (h1 >= 0 && !emm_free_pages(h1)) printf("Cannot free\n");
		sanity();
		if (h2 >= 0 && !emm_free_pages(h2)) printf("Cannot free\n");
		sanity();
		if (h3 >= 0 && !emm_free_pages(h3)) printf("Cannot free\n");
		sanity();

		printf("Allocating EMM pages (32KB): ");
		h1 = emm_alloc_pages(2);
		sanity();
		if (h1 >= 0) {
			printf("OK, handle=%u\n",h1);
			if (emm_map_page(h1,/*physical*/0,/*logical*/0)) {
				unsigned int segm = emm_last_phys_page_segment(0);
				printf("Seg %04x\n",segm);
				sanity();
				if (segm > 0) {
#if TARGET_MSDOS == 16
					char far *ptr = MK_FP(segm,0);
#else
					char *ptr = (char*)(segm << 4UL);
#endif

#if TARGET_MSDOS == 16
					_fmemcpy(ptr,(char far*)message,message_l+1);
					_fmemcpy((char far*)tmp,ptr,message_l+1);
#else
					x_memcpy(ptr,message,message_l+1);
					x_memcpy(tmp,ptr,message_l+1);
#endif
					printf("After writing message there, I read back: '%s'\n",tmp);

					if (!emm_map_page(h1,0,1)) printf("Cannot remap\n");

#if TARGET_MSDOS == 16
					_fmemcpy(ptr,(char far*)message2,message2_l+1);
					_fmemcpy((char far*)tmp,ptr,message2_l+1);
#else
					x_memcpy(ptr,message2,message2_l+1);
					x_memcpy(tmp,ptr,message2_l+1);
#endif
					printf("After mapping to page 2 and writing there, I read back: '%s'\n",tmp);

					if (!emm_map_page(h1,0,0)) printf("Cannot remap\n");

#if TARGET_MSDOS == 16
					_fmemcpy((char far*)tmp,ptr,message_l+1);
#else
					x_memcpy(tmp,ptr,message_l+1);
#endif
					printf("After mapping back to 1, I read back: '%s'\n",tmp);

					if (emm_map_page(h1,0,2)) printf("Whoops, I was able to map logical pages beyond what I allocated\n");
				}
				else {
					printf("Cannot get segment\n");
				}
				if (!emm_map_page(h1,0,0xFFFF)) printf("Cannot unmap\n");
			}
			else {
				printf("Cannot map\n");
			}
			if (!emm_free_pages(h1)) printf("Cannot free\n");
		}
		else printf("FAILED\n");

		printf("Allocating EMM pages (32KB): ");
		h1 = emm_alloc_pages(2);
		if (h1 >= 0) {
			printf("OK, handle=%u\n",h1);
			if (	emm_map_page(h1,/*physical*/0,/*logical*/0) &&
				emm_map_page(h1,/*physical*/1,/*logical*/1)) {
				unsigned int seg1 = emm_last_phys_page_segment(0);
				unsigned int seg2 = emm_last_phys_page_segment(1);
				printf("Seg %04x,%04x\n",seg1,seg2);
				if (seg1 > 0 && seg2 > 0) {
#if TARGET_MSDOS == 16
					char far *ptr1 = MK_FP(seg1,0);
					char far *ptr2 = MK_FP(seg2,0);
#else
					char *ptr1 = (char*)(seg1 << 4UL);
					char *ptr2 = (char*)(seg2 << 4UL);
#endif

#if TARGET_MSDOS == 16
					_fmemcpy(ptr1,(char far*)message,message_l+1);
					_fmemcpy(ptr2,(char far*)message2,message2_l+1);
#else
					memcpy(ptr1,message,message_l+1);
					memcpy(ptr2,message2,message2_l+1);
#endif

#if TARGET_MSDOS == 16
					_fmemcpy((char far*)tmp,ptr1,message_l+1);
					_fmemcpy((char far*)tmp2,ptr2,message2_l+1);
#else
					memcpy(tmp,ptr1,message_l+1);
					memcpy(tmp2,ptr2,message2_l+1);
#endif

					printf("After writing message there, I read back:\n'%s'\n'%s'\n",tmp,tmp2);

					/* now swap the pages */
					if (!emm_map_page(h1,1,0)) printf("cannot map log 1 -> phys 0\n");
					if (!emm_map_page(h1,0,1)) printf("cannot map log 0 -> phys 1\n");

#if TARGET_MSDOS == 16
					_fmemcpy((char far*)tmp,ptr1,message2_l+1);
					_fmemcpy((char far*)tmp2,ptr2,message_l+1);
#else
					memcpy(tmp,ptr1,message2_l+1);
					memcpy(tmp2,ptr2,message_l+1);
#endif

					printf("After swapping pages, I read back:\n'%s'\n'%s'\n",tmp,tmp2);
				}
				else {
					printf("Cannot get segment\n");
				}
				if (!emm_map_page(h1,0,0xFFFF) || !emm_map_page(h1,1,0xFFFF)) printf("Cannot unmap\n");
			}
			else {
				printf("Cannot map\n");
			}
			if (!emm_free_pages(h1)) printf("Cannot free\n");
		}
		else printf("FAILED\n");

		/* we do this test because Microsoft EMM386.EXE seems to max out at 32MB.
		 * the host could have 256MB of total memory and it would still report 32MB in EMS */
		printf("Allocating EMM pages (48MB): ");
		h1 = emm_alloc_pages((48UL << 20UL) >> 14UL);
		if (h1 >= 0) {
			printf("OK, handle=%u\n",h1);
			if (!emm_free_pages(h1)) printf("cannot free\n");
		}
		else printf("FAILED\n");

		printf("Allocating EMM pages (96MB): ");
		h1 = emm_alloc_pages((96UL << 20UL) >> 14UL);
		if (h1 >= 0) {
			printf("OK, handle=%u\n",h1);
			if (!emm_free_pages(h1)) printf("cannot free\n");
		}
		else printf("FAILED\n");
	}

	return 0;
}

