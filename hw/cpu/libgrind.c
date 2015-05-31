
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

