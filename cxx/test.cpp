
#include <stdio.h>
#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>

int main() {
    printf("Hello world\n");

	cpu_probe();
	printf("Your CPU is basically a %s or higher\n",cpu_basic_level_to_string(cpu_basic_level));

//	detect_window_enable_ntdvm(); // we care about whether or not we're running in NTVDM.EXE under Windows NT
//	detect_dos_version_enable_win9x_qt_thunk(); // we care about DOS version from Win32 builds

	probe_dos();
	printf("DOS version %x.%02u\n",dos_version>>8,dos_version&0xFF);
	printf("    Method: '%s'\n",dos_version_method);
	printf("    Flavor: '%s'\n",dos_flavor_str(dos_flavor));

	return 0;
}

