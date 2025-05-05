
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

enum {
	MODE_CURRENT=0,
	MODE_FIXED,
	MODE_AUTO,
	MODE_MAX
};

int main(int argc,char **argv,char **envp) {
	unsigned int mode = MODE_CURRENT;
	unsigned long val = 0;
	uint32_t t;

	(void)argc;
	(void)argv;
	(void)envp;

	{
		int i = 1;

		if (i < argc) {
			const char *s = argv[i];
			if (!strcmp(s,"-?") || !strcmp(s,"/?") || !strcmp(s,"--help")) {
				printf("CYCLES [fixed|max|auto] [fixed cycle count|max/auto percent]\n");
				return 1;
			}
		}

		if (i < argc) {
			const char *s = argv[i];
			if (!isdigit(*s)) {
				i++;

				if (!strcmp(s,"fixed"))
					mode = MODE_FIXED;
				else if (!strcmp(s,"max"))
					mode = MODE_MAX;
				else if (!strcmp(s,"auto"))
					mode = MODE_AUTO;
				else {
					printf("Unknown mode. Must be fixed/max/auto\n");
					return 1;
				}
			}
		}
		if (i < argc) {
			const char *s = argv[i];
			if (isdigit(*s)) val = strtoul(s,NULL,0);
		}
	}

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

	/* NTS: reading CPU_CYCLES_INFO resets writing state */
	dosbox_id_write_regsel(DOSBOX_ID_REG_CPU_CYCLES_INFO);
	t = dosbox_id_read_data();

	/* does not take effect until write to CPU_CYCLES_INFO */
	switch (mode) {
		case MODE_CURRENT:
			switch (t & DOSBOX_ID_REG_CPU_CYCLES_INFO_MODE_MASK) {
				case DOSBOX_ID_REG_CPU_CYCLES_INFO_FIXED:
					printf("fixed cycles ");
					dosbox_id_write_regsel(DOSBOX_ID_REG_CPU_CYCLES);
					t = dosbox_id_read_data();
					printf("%lu",(unsigned long)t);
					break;
				case DOSBOX_ID_REG_CPU_CYCLES_INFO_AUTO:
					printf("auto ");
					dosbox_id_write_regsel(DOSBOX_ID_REG_CPU_MAX_PERCENT);
					t = dosbox_id_read_data();
					printf("percent %lu",(unsigned long)t);
					break;
				case DOSBOX_ID_REG_CPU_CYCLES_INFO_MAX:
					printf("max ");
					dosbox_id_write_regsel(DOSBOX_ID_REG_CPU_MAX_PERCENT);
					t = dosbox_id_read_data();
					printf("percent %lu",(unsigned long)t);
					break;
				default:
					printf("? ");
					break;
			};
			printf("\n");
			break;
		case MODE_FIXED:
			dosbox_id_write_regsel(DOSBOX_ID_REG_CPU_CYCLES);
			dosbox_id_write_data((uint32_t)val);
			dosbox_id_write_regsel(DOSBOX_ID_REG_CPU_CYCLES_INFO);
			dosbox_id_write_data(DOSBOX_ID_REG_CPU_CYCLES_INFO_FIXED);
			break;
		case MODE_AUTO:
			if (val == 0) val = 100;
			dosbox_id_write_regsel(DOSBOX_ID_REG_CPU_MAX_PERCENT);
			dosbox_id_write_data((uint32_t)val);
			dosbox_id_write_regsel(DOSBOX_ID_REG_CPU_CYCLES_INFO);
			dosbox_id_write_data(DOSBOX_ID_REG_CPU_CYCLES_INFO_AUTO);
			break;
		case MODE_MAX:
			if (val == 0) val = 100;
			dosbox_id_write_regsel(DOSBOX_ID_REG_CPU_MAX_PERCENT);
			dosbox_id_write_data((uint32_t)val);
			dosbox_id_write_regsel(DOSBOX_ID_REG_CPU_CYCLES_INFO);
			dosbox_id_write_data(DOSBOX_ID_REG_CPU_CYCLES_INFO_MAX);
			break;
		default:
			break;
	};

	return 0;
}

