
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/8254/8254.h>		/* 8254 timer */

#include <hw/dosboxid/iglib.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

int main(int argc,char **argv,char **envp) {
	char tmp[128];

	(void)argc;
	(void)argv;
	(void)envp;

	probe_dos();
	detect_windows();

	if (windows_mode == WINDOWS_NT) {
		printf("This program is not compatible with Windows NT\n");
		return 1;
	}

	if (!probe_dosbox_id()) {
		printf("DOSBox integration device not found\n");
		return 1;
	}
	printf("DOSBox integration device found at I/O port %xh\n",dosbox_id_baseio);

	if (probe_dosbox_id_version_string(tmp,sizeof(tmp)))
		printf("DOSBox version string: '%s'\n",tmp);
	else
		printf("DOSBox version string N/A\n");

	{
		uint32_t major,minor,sub;

		dosbox_id_write_regsel(DOSBOX_ID_REG_DOSBOX_VERSION_MAJOR);
		major = dosbox_id_read_data();

		dosbox_id_write_regsel(DOSBOX_ID_REG_DOSBOX_VERSION_MINOR);
		minor = dosbox_id_read_data();

		dosbox_id_write_regsel(DOSBOX_ID_REG_DOSBOX_VERSION_SUB);
		sub = dosbox_id_read_data();

		printf("DOSBox version: %04u.%02u.%02u\n",(unsigned int)major,(unsigned int)minor,(unsigned int)sub);
	}

	{
		uint32_t t;

		dosbox_id_write_regsel(DOSBOX_ID_REG_DOSBOX_PLATFORM_TYPE);
		t = dosbox_id_read_data();
		printf("Platform type code: 0x%08lx\n",(unsigned long)t);
	}

	{
		uint32_t t;

		dosbox_id_write_regsel(DOSBOX_ID_REG_DOSBOX_MACHINE_TYPE);
		t = dosbox_id_read_data();
		printf("Machine type code: 0x%08lx\n",(unsigned long)t);
	}

	{
		uint32_t t;

		dosbox_id_write_regsel(DOSBOX_ID_REG_USER_MOUSE_STATUS);
		t = dosbox_id_read_data();
		printf("Mouse status:");
		if (t & (1ul << 0ul))
			printf(" [User capture lock]");
		if (t & (1ul << 1ul))
			printf(" [Captured]");
		printf("\n");
	}

	dosbox_id_debug_message("This is a debug message\n");
	dosbox_id_debug_message("This is a multi-line debug message\n(second line here)\n");

	{
		uint32_t mixq;

		dosbox_id_write_regsel(DOSBOX_ID_REG_MIXER_QUERY);
		mixq = dosbox_id_read_data();

		printf("Mixer: %u-channel %luHz mute=%u sound=%u swapstereo=%u\n",
				(unsigned int)((mixq >> 20ul) & 0xFul),
				(unsigned long)(mixq & 0xFFFFFul),
				(unsigned int)((mixq >> 30ul) & 1ul),
				((unsigned int)((mixq >> 31ul) & 1ul)) ^ 1,
				(unsigned int)((mixq >> 29ul) & 1ul));
	}
	{
		uint32_t vgainfo;

		dosbox_id_write_regsel(DOSBOX_ID_REG_GET_VGA_SIZE);
		vgainfo = dosbox_id_read_data();
		printf("VGA: %u x %u\n",
				(unsigned int)(vgainfo & 0xFFFFUL),
				(unsigned int)(vgainfo >> 16UL));
	}
	{
		uint32_t cycles,percent,info;

		dosbox_id_write_regsel(DOSBOX_ID_REG_CPU_CYCLES);
		cycles = dosbox_id_read_data();

		dosbox_id_write_regsel(DOSBOX_ID_REG_CPU_MAX_PERCENT);
		percent = dosbox_id_read_data();

		dosbox_id_write_regsel(DOSBOX_ID_REG_CPU_CYCLES_INFO);
		info = dosbox_id_read_data();

		printf("CPU: cycles=%lu percent=%lu",(unsigned long)cycles,(unsigned long)percent);
		switch (info & DOSBOX_ID_REG_CPU_CYCLES_INFO_MODE_MASK) {
			case DOSBOX_ID_REG_CPU_CYCLES_INFO_FIXED:
				printf(" FIXED");
				break;
			case DOSBOX_ID_REG_CPU_CYCLES_INFO_MAX:
				printf(" MAX");
				break;
			case DOSBOX_ID_REG_CPU_CYCLES_INFO_AUTO:
				printf(" AUTO");
				break;
			default:
				break;
		};
		printf("\n");
	}

	return 0;
}

