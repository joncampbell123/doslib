/* dos.c
 *
 * Code to detect the surrounding DOS/Windows environment and support routines to work with it
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

#ifdef TARGET_WINDOWS
# include <windows.h>
#endif

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/dos/dosntvdm.h>

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 32 && !defined(WIN386)

/* this library of code deals with the problem of getting ordinal-only exported functions out of KERNEL32.DLL in
 * Windows 9x/ME. GetProcAddress() won't do it for us, so instead, we forcibly read it out from memory ourself.
 * That's what you get for being a jack-ass about Win16 compatibility Microsoft */

/* Note that this code would work under Windows 9x/ME and NT/2000/XP, but it is only needed for 9x/ME.
 * Windows XP SP2 and later change the handle slightly to try and confuse code like this (they take the HANDLE
 * value of the module and set the least significant bit), and I have reason to believe Microsoft will eventually
 * outright change the interface to make the handle an actual opaque handle someday (while of course making it
 * utterly impossible for programs like us to get to the API functions we need to do our fucking job). Because they're
 * Microsoft, and that's what they do with the Windows API. */

/* How to use: Use the 32-bit GetModuleHandle() function to get the HMODULE value. In Windows so far, this HMODULE
 * value is literally the linear memory address where Windows loaded (or mapped) the base of the DLL image, complete
 * with MS-DOS header and PE image. This hack relies on that to then traverse the PE structure directly and forcibly
 * retrieve from the ordinal export table the function we desire. */

/* returns: DWORD* pointer to PE image's export ordinal table, *entries is filled with the number of entries, *base
 * is filled with the ordinal number of the first entry. */

static IMAGE_NT_HEADERS *Win32ValidateHModuleMSDOS_PE_Header(BYTE *p) {
	if (!memcmp(p,"MZ",2)) {
		/* then at offset 0x3C there should be the offset to the PE header */
		DWORD offset = *((DWORD*)(p+0x3C));
		if (offset < 0x40 || offset > 0x10000) return NULL;
		p += offset;
		if (IsBadReadPtr(p,4096)) return NULL;
		if (!memcmp(p,"PE\0\0",4)) {
			/* wait, before we celebrate, make sure it's sane! */
			IMAGE_NT_HEADERS *pp = (IMAGE_NT_HEADERS*)p;

			if (pp->FileHeader.Machine != IMAGE_FILE_MACHINE_I386)
				return NULL;
			if (pp->FileHeader.SizeOfOptionalHeader < 88) /* <- FIXME */
				return NULL;
			if (pp->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC)
				return NULL;

			return pp;
		}
	}

	return NULL;
}

static IMAGE_DATA_DIRECTORY *Win32GetDataDirectory(IMAGE_NT_HEADERS *p) {
	return p->OptionalHeader.DataDirectory;
}

DWORD *Win32GetExportOrdinalTable(HMODULE mod,DWORD *entries,DWORD *base,DWORD *base_addr) {
	IMAGE_EXPORT_DIRECTORY *exdir;
	IMAGE_DATA_DIRECTORY *dir;
	IMAGE_NT_HEADERS *ptr;

	/* Hack for Windows XP SP2: Clear the LSB, the OS sets it for some reason */
	mod = (HMODULE)((DWORD)mod & 0xFFFFF000UL);
	/* reset vars */
	*entries = *base = 0;
	if (mod == NULL) return NULL;

	/* the module pointer should point an image of the DLL in memory. Right at the pointer we should see
	 * the letters "MZ" and the MS-DOS stub EXE header */
	ptr = Win32ValidateHModuleMSDOS_PE_Header((BYTE*)mod);
	if (ptr == NULL) return NULL;

	/* OK, now locate the Data Directory. The number of entries is in ptr->OptionalHeader.NumberOfRvaAndSizes */
	dir = Win32GetDataDirectory(ptr);
	if (ptr == NULL) return NULL;

	/* the first directory is the Export Address Table */
	exdir = (IMAGE_EXPORT_DIRECTORY*)((DWORD)mod + (DWORD)dir->VirtualAddress);
	if (IsBadReadPtr(exdir,2048)) return NULL;

	*base = exdir->Base;
	*entries = exdir->NumberOfFunctions;
	*base_addr = (DWORD)mod;
	return (DWORD*)((DWORD)mod + exdir->AddressOfFunctions);
}

int Win32GetOrdinalLookupInfo(HMODULE mod,Win32OrdinalLookupInfo *info) {
	DWORD *x = Win32GetExportOrdinalTable(mod,&info->entries,&info->base,&info->base_addr);
	if (x == NULL) return 0;
	info->table = x;
	return 1;
}

void *Win32GetOrdinalAddress(Win32OrdinalLookupInfo *nfo,unsigned int ord) {
	if (nfo == NULL || nfo->table == NULL) return NULL;
	if (ord < nfo->base) return NULL;
	if (ord >= (nfo->base+nfo->entries)) return NULL;
	return (void*)((char*)nfo->table[ord-nfo->base] + nfo->base_addr);
}
#endif

