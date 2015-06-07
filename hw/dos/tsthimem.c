/* tsthimem.c
 *
 * Test program: HIMEM.SYS functions
 * (C) 2010-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/dos/himemsys.h>

int main() {
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

	if (probe_himem_sys()) {
		int h1,h2,h3;

		printf("HIMEM.SYS detected. Entry point %04x:%04x. v%x.%02x\n",
				(unsigned int)((himem_sys_entry >> 16) & 0xFFFFUL),
				(unsigned int)(himem_sys_entry & 0xFFFFUL),
				(unsigned int)(himem_sys_version >> 8),
				(unsigned int)(himem_sys_version & 0xFF));

		if (himem_sys_flags & HIMEM_F_HMA)
			printf("  - HMA is present\n");
		if (himem_sys_flags & HIMEM_F_4GB)
			printf("  - Extensions are present to address up to 4GB of memory\n");

		printf("A20 status: %u\n",himem_sys_query_a20());
		printf("Global A20 line:         "); fflush(stdout);
		printf("en=%d ",himem_sys_global_a20(1)); fflush(stdout);
		printf("query=%d ",himem_sys_query_a20(1)); fflush(stdout);
		printf("dis=%d ",himem_sys_global_a20(0)); fflush(stdout);
		printf("query=%d ",himem_sys_query_a20(1)); fflush(stdout);
		printf("\n");

		printf("Local A20 line:          "); fflush(stdout);
		printf("en=%d ",himem_sys_local_a20(1)); fflush(stdout);
		printf("query=%d ",himem_sys_query_a20(1)); fflush(stdout);
		printf("dis=%d ",himem_sys_local_a20(0)); fflush(stdout);
		printf("query=%d ",himem_sys_query_a20(1)); fflush(stdout);
		printf("\n");

		himem_sys_update_free_memory_status();
		printf("Free memory: %luKB (largest block %luKB)\n",
			(unsigned long)himem_sys_total_free,
			(unsigned long)himem_sys_largest_free);

		printf("Attempting to alloc 4KB: ");
		h1 = himem_sys_alloc(4); /* NTS: This is in KB, not bytes */
		if (h1 != -1) printf("ok, handle %u\n",h1);
		else printf("failed\n");

		printf("Attempting to alloc 64KB: ");
		h2 = himem_sys_alloc(46); /* NTS: This is in KB, not bytes */
		if (h2 != -1) printf("ok, handle %u\n",h2);
		else printf("failed\n");

		printf("Attempting to alloc 1MB: ");
		h3 = himem_sys_alloc(1024); /* NTS: This is in KB, not bytes */
		if (h3 != -1) printf("ok, handle %u\n",h3);
		else printf("failed\n");

		if (h1 != -1) {
			if (!himem_sys_free(h1)) printf(" - Free failed\n");
		}
		if (h2 != -1) {
			if (!himem_sys_free(h2)) printf(" - Free failed\n");
		}
		if (h3 != -1) {
			if (!himem_sys_free(h3)) printf(" - Free failed\n");
		}

		printf("Attempting to alloc 1MB (for writing to): ");
		h3 = himem_sys_alloc(1024); /* NTS: This is in KB, not bytes */
		if (h3 != -1) {
			uint32_t ofs;
			unsigned int i;
			struct himem_block_info binf;
#if TARGET_MSDOS == 32
			char *msg;
			unsigned char *tmp;
			uint16_t tmpsel=0,msgsel=0;
			const char *msgref = "Testing 123 hello";
#else
			unsigned char tmp[16];
			const char *msg = "Testing 123 hello";
#endif

#if TARGET_MSDOS == 32
			tmp = dpmi_alloc_dos(16,&tmpsel);
			if (tmp == NULL) abort();
			msg = dpmi_alloc_dos(strlen(msgref)+16,&msgsel);
			if (msg == NULL) abort();
			memcpy(msg,msgref,strlen(msgref)+1);
#endif

			printf("ok, handle %u\n",h3);

			if (himem_sys_get_handle_info(h3,&binf)) {
				printf("Handle info:\n");
				printf("     Lock count=%u  Free handles=%u  Size=%luKB\n",
					(unsigned int)binf.lock_count,
					(unsigned int)binf.free_handles,
					(unsigned long)binf.block_length_kb);
			}
			else {
				printf("Cannot get handle info\n");
			}

#if TARGET_MSDOS == 32
			if (!himem_sys_move(h3,0,0/*conventional memory*/,(unsigned long)(msg),16))
#else
			if (!himem_sys_move(h3,0,0/*conventional memory*/,((unsigned long)FP_SEG(msg) << 4UL) + (unsigned long)FP_OFF(msg),sizeof(tmp)))
#endif
				printf("Copy didn't work\n");

			for (i=0;i < 16;i += 2) {
				tmp[i+0] = 0x55;
				tmp[i+1] = 0xAA;
			}

#if TARGET_MSDOS == 32
			if (!himem_sys_move(0/*conventional memory*/,(unsigned long)(tmp),h3,0,16))
#else
			if (!himem_sys_move(0/*conventional memory*/,((unsigned long)FP_SEG(tmp) << 4UL) + (unsigned long)FP_OFF(tmp),h3,0,sizeof(tmp)))
#endif
				printf("Copy didn't work\n");

			for (i=0;i < 16;i++) printf("%02x ",tmp[i]);
			printf("\n");

			ofs = himem_sys_lock(h3);
			if (ofs != 0UL) {
				printf(" - Locked: Physical memory address 0x%08lX\n",(unsigned long)ofs);
				if (!himem_sys_unlock(h3)) printf(" - Cannot unlock\n");
			}
			else {
				printf(" - Cannot lock\n");
			}

			printf("now resizing to 2MB\n");
			if (himem_sys_realloc(h3,2048)) {
#if TARGET_MSDOS == 32
				if (!himem_sys_move(0/*conventional memory*/,(unsigned long)(tmp),h3,0,16))
#else
				if (!himem_sys_move(0/*conventional memory*/,((unsigned long)FP_SEG(tmp) << 4UL) + (unsigned long)FP_OFF(tmp),h3,0,sizeof(tmp)))
#endif
					printf("Copy didn't work\n");

				for (i=0;i < 16;i++) printf("%02x ",tmp[i]);
				printf("\n");

				ofs = himem_sys_lock(h3);
				if (ofs != 0UL) {
					printf(" - Locked: Physical memory address 0x%08lX\n",(unsigned long)ofs);
					if (!himem_sys_unlock(h3)) printf(" - Cannot unlock\n");
				}
				else {
					printf(" - Cannot lock\n");
				}
			}
			else {
				printf(" - Cannot realloc\n");
			}

			if (!himem_sys_free(h3)) printf(" - Free failed\n");

#if TARGET_MSDOS == 32
			dpmi_free_dos(tmpsel); tmp=NULL;
			dpmi_free_dos(msgsel); msg=NULL;
#endif
		}
		else printf("failed\n");

		printf("Attempting to alloc 129MB (for writing to): ");
		h3 = himem_sys_alloc(129UL * 1024UL); /* NTS: This is in KB, not bytes */
		if (h3 != -1) {
			uint32_t ofs;
			unsigned int i;
			unsigned char tmp[16];
			struct himem_block_info binf;
			const char *msg = "Testing 123 hello";

			printf("ok, handle %u\n",h3);

			if (himem_sys_get_handle_info(h3,&binf)) {
				printf("Handle info:\n");
				printf("     Lock count=%u  Free handles=%u  Size=%luKB\n",
					(unsigned int)binf.lock_count,
					(unsigned int)binf.free_handles,
					(unsigned long)binf.block_length_kb);
			}
			else {
				printf("Cannot get handle info\n");
			}

#if TARGET_MSDOS == 32
			if (!himem_sys_move(h3,0,0/*conventional memory*/,(unsigned long)(msg),16))
#else
			if (!himem_sys_move(h3,0,0/*conventional memory*/,((unsigned long)FP_SEG(msg) << 4UL) + (unsigned long)FP_OFF(msg),sizeof(tmp)))
#endif
				printf("Copy didn't work\n");

			for (i=0;i < 16;i += 2) {
				tmp[i+0] = 0x55;
				tmp[i+1] = 0xAA;
			}

#if TARGET_MSDOS == 32
			if (!himem_sys_move(0/*conventional memory*/,(unsigned long)(tmp),h3,0,16))
#else
			if (!himem_sys_move(0/*conventional memory*/,((unsigned long)FP_SEG(tmp) << 4UL) + (unsigned long)FP_OFF(tmp),h3,0,sizeof(tmp)))
#endif
				printf("Copy didn't work\n");

			for (i=0;i < 16;i++) printf("%02x ",tmp[i]);
			printf("\n");

			ofs = himem_sys_lock(h3);
			if (ofs != 0UL) {
				printf(" - Locked: Physical memory address 0x%08lX\n",(unsigned long)ofs);
				if (!himem_sys_unlock(h3)) printf(" - Cannot unlock\n");
			}
			else {
				printf(" - Cannot lock\n");
			}

			printf("now resizing to 144MB\n");
			if (himem_sys_realloc(h3,144UL*1024UL)) {
				if (himem_sys_get_handle_info(h3,&binf)) {
					printf("Handle info:\n");
					printf("     Lock count=%u  Free handles=%u  Size=%luKB\n",
						(unsigned int)binf.lock_count,
						(unsigned int)binf.free_handles,
						(unsigned long)binf.block_length_kb);
				}
				else {
					printf("Cannot get handle info\n");
				}

#if TARGET_MSDOS == 32
				if (!himem_sys_move(0/*conventional memory*/,(unsigned long)(tmp),h3,0,16))
#else
				if (!himem_sys_move(0/*conventional memory*/,((unsigned long)FP_SEG(tmp) << 4UL) + (unsigned long)FP_OFF(tmp),h3,0,sizeof(tmp)))
#endif
					printf("Copy didn't work\n");

				for (i=0;i < 16;i++) printf("%02x ",tmp[i]);
				printf("\n");

				ofs = himem_sys_lock(h3);
				if (ofs != 0UL) {
					printf(" - Locked: Physical memory address 0x%08lX\n",(unsigned long)ofs);
					if (!himem_sys_unlock(h3)) printf(" - Cannot unlock\n");
				}
				else {
					printf(" - Cannot lock\n");
				}
			}
			else {
				printf(" - Cannot realloc\n");
			}

			if (!himem_sys_free(h3)) printf(" - Free failed\n");
		}
		else printf("failed\n");
	}
	else {
		printf("HIMEM.SYS not found\n");
	}

	return 0;
}

