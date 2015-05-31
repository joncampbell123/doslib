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

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

#if TARGET_MSDOS == 32
# define GRIND_REGSIZE		32
# define GRIND_CODE32
typedef uint32_t		grind_off_t;
typedef uint16_t		grind_seg_t;
#else
# define GRIND_REGSIZE		16
# define GRIND_CODE16
# define GRIND_FAR
typedef uint16_t		grind_off_t;
typedef uint16_t		grind_seg_t;
#endif

#ifdef GRIND_FAR
typedef unsigned char far*	grind_buf_ptr_t;
#else
typedef unsigned char*		grind_buf_ptr_t;
#endif

#if TARGET_MSDOS == 16 && defined(TARGET_WINDOWS) /* For this to work in Windows 3.1, we need a code segment alias of our data segment */
# define GRIND_NEED_CODE_ALIAS_SEL
#endif

grind_buf_ptr_t		grind_buf = NULL;
size_t			grind_buf_size = 0;
uint8_t			grind_buf_init = 0;
uint8_t			grind_buf_lock = 0;

#ifdef GRIND_NEED_CODE_ALIAS_SEL
HGLOBAL			grind_buf_whnd = 0; // GlobalAlloc() return value
uint16_t		grind_buf_dsel = 0;
uint16_t		grind_buf_csel = 0;
#endif

int grind_init() {
	cpu_probe();

#if TARGET_MSDOS == 32 && defined(TARGET_WINDOWS) && defined(WIN386)
	// double-check: even though the Win386 extender does not alloc segment bases from linear addr zero,
	// our code assumes that at least the code and data 32-bit segments have the same base. is that true?
	// if not, this code won't work.
	{
		WORD s1=0,s2=0;

		__asm {
			mov	s1,cs
			mov	s2,ds
		}

		if (GetSelectorBase(s1) != GetSelectorBase(s2)) {
			MessageBox((HWND)NULL,"Win386 error: GRIND library not available. CS.base != DS.base","",MB_OK);
			return 0;
		}
		if (GetSelectorLimit(s1) > GetSelectorLimit(s2)) {
			MessageBox((HWND)NULL,"Win386 error: GRIND library not available. CS.limit > DS.limit","",MB_OK);
			return 0;
		}
	}
#endif

	if (!grind_buf_init) {
		grind_buf_size = 512;
		grind_buf_init = 1;
	}

	return grind_buf_init;
}

void grind_unlock_buf();

int grind_alloc_buf() {
#ifdef GRIND_NEED_CODE_ALIAS_SEL
	if (grind_buf_whnd == 0) {
		if (grind_buf_size < 128) return 0;
		grind_buf_whnd = GlobalAlloc(GPTR,grind_buf_size);
		if (grind_buf_whnd == 0) return 0;
	}

	return (grind_buf_whnd != 0)?1:0;
#else
	if (grind_buf == NULL) {
		if (grind_buf_size < 128) return 0;
# ifdef GRIND_FAR
		grind_buf = _fmalloc(grind_buf_size);
# else
		grind_buf = malloc(grind_buf_size);
#endif
	}

	return (grind_buf != NULL)?1:0;
#endif
}

int grind_lock_buf() {
#ifdef GRIND_NEED_CODE_ALIAS_SEL
	if (grind_buf_dsel == 0 && grind_buf_whnd != 0) {
		grind_buf = GlobalLock(grind_buf_whnd);
		if (grind_buf == NULL) return 0;
		grind_buf_dsel = FP_SEG(grind_buf);
	}
	if (grind_buf_csel == 0) {
		grind_buf_csel = AllocDStoCSAlias(grind_buf_dsel);
		if (grind_buf_csel == 0) {
			grind_unlock_buf();
			return 0;
		}
	}
#endif
	grind_buf_lock = 1;
	return grind_buf_lock;
}

void grind_unlock_buf() {
#ifdef GRIND_NEED_CODE_ALIAS_SEL
	if (grind_buf_csel) {
		// NTS: Omitting the call to FreeSelector() doesn't seem to leak in Windows 3.1. Does GlobalAlloc
		//      somehow track aliases to the segment? Seems like an invitation for bad coding practice to me.
		FreeSelector(grind_buf_csel);
		grind_buf_csel = 0;
	}
	if (grind_buf_dsel) {
		GlobalUnlock(grind_buf_whnd);
		grind_buf_dsel = 0;
	}
#endif
	grind_buf_lock = 0;
}

void grind_free_buf() {
	if (grind_buf != NULL) {
		grind_unlock_buf();
#ifdef GRIND_NEED_CODE_ALIAS_SEL
		if (grind_buf_whnd) {
			GlobalFree(grind_buf_whnd);
			grind_buf_whnd = 0;
		}
#else
# ifdef GRIND_FAR
		_ffree(grind_buf);
# else
		free(grind_buf);
# endif
#endif
		grind_buf = NULL;
	}
}

void grind_free() {
	grind_unlock_buf();
	grind_free_buf();
}

int grind_execute_buf() {
#ifdef GRIND_NEED_CODE_ALIAS_SEL
	void far *x;

	if (grind_buf_csel == 0) return 0;

	x = MK_FP(grind_buf_csel,FP_OFF(grind_buf));
	__asm callf	[x]
#else
	if (grind_buf == NULL) return 0;

# ifdef GRIND_FAR
	__asm callf	[grind_buf]
# else
	__asm call	[grind_buf]
# endif
#endif

	return 1;
}

static void pause_if_tty() {
	unsigned char c;
	if (isatty(1)) { do { read(0,&c,1); } while (c != 13); }
}

int main() {
	unsigned int i;

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
#ifdef GRIND_FAR
		grind_buf[0] = 0xCB; // RETF
#else
		grind_buf[0] = 0xC3; // RET
#endif

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

