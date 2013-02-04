
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include <conio.h>
#include <fcntl.h>
#include <dos.h>
#include <i86.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/dosbox.h>

#if TARGET_MSDOS == 32 && defined(TARGET_WINDOWS)
# ifdef NTVDM_VDD_DRIVER

enum {
	VDM_V86,
	VDM_PM
};

typedef struct {
	WORD		First;
	WORD		Last;
} VDD_IO_PORTRANGE;

typedef VOID WINAPI (*PFNVDD_INB)	(WORD iport,BYTE *data);
typedef VOID WINAPI (*PFNVDD_INW)	(WORD iport,WORD *data);
typedef VOID WINAPI (*PFNVDD_INSB)	(WORD iport,BYTE *data,WORD count);
typedef VOID WINAPI (*PFNVDD_INSW)	(WORD iport,WORD *data,WORD count);

typedef VOID WINAPI (*PFNVDD_OUTB)	(WORD iport,BYTE data);
typedef VOID WINAPI (*PFNVDD_OUTW)	(WORD iport,WORD data);
typedef VOID WINAPI (*PFNVDD_OUTSB)	(WORD iport,BYTE *data,WORD count);
typedef VOID WINAPI (*PFNVDD_OUTSW)	(WORD iport,WORD *data,WORD count);

typedef struct {
	PFNVDD_INB	inb_handler;
	PFNVDD_INW	inw_handler;
	PFNVDD_INSB	insb_handler;
	PFNVDD_INSW	insw_handler;
	PFNVDD_OUTB	outb_handler;
	PFNVDD_OUTW	outw_handler;
	PFNVDD_OUTSB	outsb_handler;
	PFNVDD_OUTSW	outsw_handler;
} VDD_IO_HANDLERS;

/* NTVDM.EXE functions */
WORD WINAPI getES();
void WINAPI setES(WORD x);

WORD WINAPI getDS();
void WINAPI setDS(WORD x);

WORD WINAPI getBX();
void WINAPI setBX(WORD x);

WORD WINAPI getCX();
void WINAPI setCX(WORD x);

DWORD WINAPI getEDX();
void WINAPI setEDX(DWORD x);

DWORD WINAPI getECX();
void WINAPI setECX(DWORD x);

DWORD WINAPI getEBX();
void WINAPI setEBX(DWORD x);

DWORD WINAPI getEAX();
void WINAPI setEAX(DWORD x);

DWORD WINAPI getESI();
void WINAPI setESI(DWORD x);

DWORD WINAPI getEDI();
void WINAPI setEDI(DWORD x);

WORD WINAPI getMSW();
void WINAPI setMSW(WORD x);

/* NTS: It turns out under Windows NT/2000/XP, NTVDM.EXE just maps DOS memory into the
 *      lowest 1MB of it's own address space (where normally Win32 apps leave unmapped
 *      to catch NULL pointer errors) and then run the DOS app in a v86 environment.
 *      Correspondingly, there's really nothing to do to "unmap" the linear range
 *      when VdmMapFlat() returns a pointer. This is verified as well from their
 *      Windows DDK header nt_vdd.h. Perhaps these were needed for the Alpha and
 *      such ports of NT where the emulation needed to synchronize buffers or
 *      something. */
static inline unsigned int VdmUnmapFlat(sel,off,buf,mode) { return 1; }
static inline unsigned int VdmFlushCache(sel,off,len,mode) { return 1; }
PVOID WINAPI VdmMapFlat(USHORT selector,ULONG offset,uint32_t mode);
BOOL WINAPI VDDInstallIOHook(HANDLE hVdd,WORD cPortRange,VDD_IO_PORTRANGE *pPortRange,VDD_IO_HANDLERS *IOhandler);
VOID WINAPI VDDDeInstallIOHook(HANDLE hVdd,WORD cPortRange,VDD_IO_PORTRANGE *IOhandler);

# endif
#endif

