/* test.c
 *
 * Test program: Various info about DOS
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

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

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/dos/dosbox.h>
#include <hw/dos/dosntvdm.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

int main() {
	probe_dos();
	printf("DOS version %x.%02u\n",dos_version>>8,dos_version&0xFF);
	printf("    Method: '%s'\n",dos_version_method);
	printf("    Flavor: '%s'\n",dos_flavor_str(dos_flavor));
	if (dos_flavor == DOS_FLAVOR_FREEDOS) {
		printf("    FreeDOS kernel %u.%u.%u (%lX)\n",
			(unsigned int)((freedos_kernel_version >> 16UL) & 0xFFUL),
			(unsigned int)((freedos_kernel_version >> 8UL) & 0xFFUL),
			(unsigned int)((freedos_kernel_version) & 0xFFUL),
			(unsigned long)freedos_kernel_version);
		if (freedos_kernel_version_str != NULL) {
#if TARGET_MSDOS == 32
			printf("    FreeDOS kernel version string: %s\n",
				freedos_kernel_version_str);
#else
			printf("    FreeDOS kernel version string: %Fs\n",
				freedos_kernel_version_str);
#endif
		}
	}

	if (detect_windows()) {
		printf("I am running under Windows.\n");
		printf("    Mode: %s\n",windows_mode_str(windows_mode));
		printf("    Ver:  %x.%u\n",windows_version>>8,windows_version&0xFF);
		printf("    Method: '%s'\n",windows_version_method);
		if (windows_emulation != WINEMU_NONE)
			printf("    Emulation: '%s'\n",windows_emulation_str(windows_emulation));
		if (windows_emulation_comment_str != NULL)
			printf("    Emulation comment: '%s'\n",windows_emulation_comment_str);
	}
	else {
		printf("Not running under Windows or OS/2\n");
	}

#if defined(NTVDM_CLIENT) && !defined(TARGET_WINDOWS)
	if (ntvdm_dosntast_handle != DOSNTAST_HANDLE_UNASSIGNED) {
		OSVERSIONINFO o;
		WAVEOUTCAPS woc;
		unsigned int i,j;
		uint32_t dw;

		printf("This program is using the DOSNTAST.VDD driver (handle=%u io=0x%03X)\n",ntvdm_dosntast_handle,ntvdm_dosntast_io_base);

		printf("GetTickCount() = %lu\n",ntvdm_dosntast_GetTickCount());
		printf("waveOutGetNumDevs() = %d\n",j=ntvdm_dosntast_waveOutGetNumDevs());
		for (i=0;i < j;i++) {
			memset(&woc,0,sizeof(woc));
			if ((dw=ntvdm_dosntast_waveOutGetDevCaps(i,&woc,sizeof(woc))) == 0) {
				printf("  [%u]: %s v%u.%u\n",i,woc.szPname,woc.vDriverVersion>>8,woc.vDriverVersion&0xFF);
				printf("        MID=0x%04lX PID=0x%04lX FMTS=0x%08lX chan=%u\n",
					(unsigned long)woc.wMid,
					(unsigned long)woc.wPid,
					(unsigned long)woc.dwFormats,
					woc.wChannels);
				printf("        CAPS: ");
				if (woc.dwSupport & WAVECAPS_LRVOLUME) printf("LRVOL ");
				if (woc.dwSupport & WAVECAPS_PITCH) printf("PITCH ");
				if (woc.dwSupport & WAVECAPS_PLAYBACKRATE) printf("PLAYRATE ");
				if (woc.dwSupport & WAVECAPS_SYNC) printf("SYNC ");
				if (woc.dwSupport & WAVECAPS_VOLUME) printf("VOL ");
				if (woc.dwSupport & WAVECAPS_SAMPLEACCURATE) printf("SAMPLEACCURATE ");
				printf("\n");
			}
			else {
				printf("  [%u]: Cannot read err=0x%08lX\n",i,dw);
			}
		}

		printf("GetVersionEx() = ");
		o.dwOSVersionInfoSize = sizeof(o);
		if (ntvdm_dosntast_getversionex(&o)) {
			printf("v%lu.%lu build #%lu platform=%lu '%s'",
				o.dwMajorVersion,
				o.dwMinorVersion,
				o.dwBuildNumber,
				o.dwPlatformId,
				o.szCSDVersion);
		}
		else {
			printf("failed?");
		}
		printf("\n");

		ntvdm_dosntast_MessageBox("Hello!\n\nIf you can read this, DOS programs are able to use the driver successfully!");
	}
#endif

	if (detect_dosbox_emu())
		printf("I am also running under DOSBox\n");
	if (detect_virtualbox_emu())
		printf("I am also running under Sun/Oracle VirtualBox %s\n",virtualbox_version_str);

	probe_dpmi();
#if dpmi_present != 0
	if (dpmi_present) {
		printf("DPMI present:\n");
# if TARGET_MSDOS == 16 || (TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS))
		if (dpmi_no_0301h > 0) printf(" - DPMI function 0301H: Call real-mode far routine NOT AVAILABLE\n");
		printf(" - Flags: 0x%04x\n",dpmi_flags);
		printf(" - Entry: %04x:%04x (real mode)\n",(unsigned int)(dpmi_entry_point>>16UL),(unsigned int)(dpmi_entry_point & 0xFFFFUL));
		printf(" - Processor type: %02x\n",dpmi_processor_type);
		printf(" - Version: %u.%u\n",dpmi_version>>8,dpmi_version&0xFF);
		printf(" - Private data length: %u paras\n",dpmi_private_data_length_paragraphs);
# endif
	}
#endif

	if (probe_vcpi()) {
		printf("VCPI present (v%d.%d)\n",
			vcpi_major_version,
			vcpi_minor_version);
	}

#ifdef WINFCON_STOCK_WIN_MAIN
	{
		char c;

		printf("---------[Type junk here]---------\n");
		do {
			c = getch();
			if (c == 27) break;
			else if (c == 13) printf("\n");
			else printf("%c",c);
		} while (1);
	}
#endif

	return 0;
}

