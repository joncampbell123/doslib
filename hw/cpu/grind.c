/* grind.c
 *
 * Run instruction encodings through combinations and record changes to registers and flags.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <malloc.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/cpu/libgrind.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

static void pause_if_tty() {
	unsigned char c;
	if (isatty(1)) { do { read(0,&c,1); } while (c != 13); }
}

int main() {
	unsigned int i;

	cpu_probe();
	if (!grind_init()) {
		printf("Failed to init grind library\n");
		return 1;
	}

	for (i=0;i < 24;i++) {
		printf("Buffer test... ");fflush(stdout);

		printf("INIT ");fflush(stdout);
		if (!grind_init()) {
			printf("<--FAIL\n");
			grind_free();
			return 1;
		}

		printf("ALLOC(%zu) ",grind_buf_size);fflush(stdout);
		if (!grind_alloc_buf()) {
			printf("<--FAIL\n");
			grind_free();
			return 1;
		}

		printf("LOCK ");fflush(stdout);
		if (!grind_lock_buf()) {
			printf("<--FAIL\n");
			grind_free();
			return 1;
		}
#ifdef GRIND_FAR
		printf("[buf=%x:%x] ",FP_SEG(grind_buf),FP_OFF(grind_buf));
#else
		printf("[buf=%p] ",grind_buf);
#endif

		printf("UNLOCK ");fflush(stdout);
		grind_unlock_buf();

		printf("FREE ");fflush(stdout);
		grind_free_buf();

		printf("OK\n");
	}
	pause_if_tty();

	for (i=0;i < 24;i++) {
		printf("Buffer write... ");fflush(stdout);

		printf("INIT ");fflush(stdout);
		if (!grind_init()) {
			printf("<--FAIL\n");
			grind_free();
			return 1;
		}

		printf("ALLOC ");fflush(stdout);
		if (!grind_alloc_buf()) {
			printf("<--FAIL\n");
			grind_free();
			return 1;
		}
#ifdef GRIND_NEED_CODE_ALIAS_SEL
		printf("[hnd=%x] ",grind_buf_whnd);
#endif

		printf("LOCK ");fflush(stdout);
		if (!grind_lock_buf()) {
			printf("<--FAIL\n");
			grind_free();
			return 1;
		}
#ifdef GRIND_FAR
		printf("[buf=%x:%x] ",FP_SEG(grind_buf),FP_OFF(grind_buf));
#else
		printf("[buf=%p] ",grind_buf);
#endif

		printf("WRITE ");fflush(stdout);
		grind_buf[0] = i;
		grind_buf[1] = i+1;

		printf("UNLOCK ");fflush(stdout);
		grind_unlock_buf();

		printf("FREE ");fflush(stdout);
		grind_free_buf();

		printf("OK\n");
	}
	pause_if_tty();

	for (i=0;i < 24;i++) {
#ifdef GRIND_FAR
		printf("RETF... ");fflush(stdout);
#else
		printf("RET... ");fflush(stdout);
#endif

		printf("INIT ");fflush(stdout);
		if (!grind_init()) {
			printf("<--FAIL\n");
			grind_free();
			return 1;
		}

		printf("ALLOC ");fflush(stdout);
		if (!grind_alloc_buf()) {
			printf("<--FAIL\n");
			grind_free();
			return 1;
		}

		printf("LOCK ");fflush(stdout);
		if (!grind_lock_buf()) {
			printf("<--FAIL\n");
			grind_free();
			return 1;
		}

		printf("WRITE ");fflush(stdout);
		grind_buf[0] = GRIND_RET_INS;

		printf("EXEC ");fflush(stdout);
		if (!grind_execute_buf()) {
			printf("<--FAIL\n");
			grind_free();
			return 1;
		}

		printf("UNLOCK ");fflush(stdout);
		grind_unlock_buf();

		printf("FREE ");fflush(stdout);
		grind_free_buf();

		printf("OK\n");
	}

	grind_free();
	return 0;
}

