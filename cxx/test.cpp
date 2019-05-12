
#include <stdio.h>
#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

int main(int /*argc*/,char ** /*argv*/,char ** /*envp*/) {
    printf("Hello world\n");

	cpu_probe();
	printf("Your CPU is basically a %s or higher\n",cpu_basic_level_to_string(cpu_basic_level));

	probe_dos();
	printf("DOS version %x.%02u\n",dos_version>>8,dos_version&0xFF);
	printf("    Method: '%s'\n",dos_version_method);
	printf("    Flavor: '%s'\n",dos_flavor_str(dos_flavor));

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

	return 0;
}

