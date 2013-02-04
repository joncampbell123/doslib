
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
#include <windows/w9xvmm/vxd_enum.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

/* NTS: This is private to this test program */
static void w9x_vmm_printf_vxd(struct w9x_vmm_DDB_scan *v) {
	char tmp[9];
	int i;

	memcpy(tmp,v->ddb.name,8); tmp[8]=0;
	for (i=7;i > 0 && tmp[i] == ' ';) tmp[i--] = 0;
	printf("VXD '%s' @ 0x%08lX\n",tmp,(unsigned long)v->found_at);

	printf("    version=%u.%u number=%u v%u.%u flags=0x%04X\n",
		v->ddb.ver>>8,
		v->ddb.ver&0xFF,
		v->ddb.no,
		v->ddb.Mver,
		v->ddb.minorver,
		v->ddb.flags);
	printf("    Init=0x%08lX Control=0x%08lX v86=0x%08lX Pm=0x%08lX\n",
		(unsigned long)v->ddb.Init,
		(unsigned long)v->ddb.ControlProc,
		(unsigned long)v->ddb.v86_proc,
		(unsigned long)v->ddb.Pm_Proc);
	printf("    v86()=0x%08lX pm()=0x%08lX data=0x%08lX service_size=0x%08lx\n",
		(unsigned long)v->ddb.v86,
		(unsigned long)v->ddb.PM,
		(unsigned long)v->ddb.data,
		(unsigned long)v->ddb.service_size);
	printf("    win32_ptr=0x%08lX next=0x%08lX\n",
		(unsigned long)v->ddb.win32_ptr,
		(unsigned long)v->ddb.next);
}

int main() {
	int c;
	struct w9x_vmm_DDB_scan vmddb;

	probe_dos();
	cpu_probe();
	printf("DOS version %x.%02u\n",dos_version>>8,dos_version&0xFF);
	printf("    Method: '%s'\n",dos_version_method);
	if (detect_windows()) {
		printf("I am running under Windows.\n");
		printf("    Mode: %s\n",windows_mode_str(windows_mode));
		printf("    Ver:  %x.%02u\n",windows_version>>8,windows_version&0xFF);
		printf("    Method: '%s'\n",windows_version_method);
		printf("    Emulation: '%s'\n",windows_emulation_str(windows_emulation));
		if (windows_emulation_comment_str != NULL)
			printf("    Emulation comment: '%s'\n",windows_emulation_comment_str);
	}
	else {
		printf("Not running under Windows or OS/2\n");
	}

	if (detect_dosbox_emu())
		printf("I am also running under DOSBox\n");
	if (detect_virtualbox_emu())
		printf("I am also running under Sun/Oracle VirtualBox %s\n",virtualbox_version_str);

	if (!w9x_vmm_first_vxd(&vmddb)) {
		printf("Unable to begin enumerating\n");
		return 1;
	}
	do {
		w9x_vmm_printf_vxd(&vmddb);
		if (isatty(0)) {
			while (1) {
				c = getch();
				if (c == 27) {
					w9x_vmm_last_vxd(&vmddb);
					return 1;
				}
				else if (c == 13) {
					break;
				}
			}
		}
	} while (w9x_vmm_next_vxd(&vmddb));
	w9x_vmm_last_vxd(&vmddb);

	return 0;
}

