/* test.c
 *
 * Advanced Power Management BIOS test program.
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box]
 *
 * This program allows you to play with your APM BIOS interface. It should
 * not cause harm, but this code is provided under no warranty express or
 * implied. */
 
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/apm/apm.h>
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC */
#include <hw/dos/doswin.h>

static void help() {
	fprintf(stderr,"test [options] [action]\n");
	fprintf(stderr,"Test program for APM BIOS functions\n");
#if TARGET_MSDOS == 32
	fprintf(stderr,"32-bit protected mode build\n");
#else
	fprintf(stderr,"16-bit real mode ");
#  if defined(__LARGE__)
	fprintf(stderr,"large");
#  elif defined(__MEDIUM__)
	fprintf(stderr,"medium");
#  elif defined(__COMPACT__)
	fprintf(stderr,"compact");
#  else
	fprintf(stderr,"small");
#  endif
	fprintf(stderr," build\n");
#endif
	fprintf(stderr,"   /h            help\n");
	fprintf(stderr,"   /v1.<n>       Use APM v1.N interface (v1.0, v1.1, or v1.2)\n");
	fprintf(stderr,"   /vn           Don't connect to APM\n");
	fprintf(stderr,"   /nd           Don't disconnect after call\n");
	fprintf(stderr,"   /if:<n>       Use interface (real");
#if TARGET_MSDOS == 32
		fprintf(stderr,", 16pm, 32pm)\n");
#else
		fprintf(stderr,")\n");
#endif
	fprintf(stderr,"Commands\n");
	fprintf(stderr,"   cpu-idle\n");
	fprintf(stderr,"   cpu-busy\n");
	fprintf(stderr,"   events\n");
	fprintf(stderr,"   status\n");
	fprintf(stderr,"   standby\n");
	fprintf(stderr,"   suspend\n");
	fprintf(stderr,"   off\n");
}

#define REQ_VER_NONE 1
int main(int argc,char **argv) {
	unsigned int req_ver = 0;
	unsigned int dont_disconnect = 0;
	unsigned int req_mode = APM_CONNECT_NONE;
	const char *action = NULL;
	int i;

	for (i=1;i < argc;) {
		const char *a = argv[i++];

		if (*a == '/' || *a == '-') {
			do { a++; } while (*a == '/' || *a == '-');

			if (!strncmp(a,"v1.",3)) {
				a += 3;
				req_ver = 0x100 + (atoi(a) & 0xFF);
			}
			else if (!strcmp(a,"vn")) {
				req_ver = REQ_VER_NONE;
			}
			else if (!strcmp(a,"nd")) {
				dont_disconnect = 1;
			}
			else if (!strcmp(a,"h") || !strcmp(a,"help")) {
				help();
			}
			else if (!strncmp(a,"if:",3)) {
				a += 3;
				if (!strcmp(a,"real"))
					req_mode = APM_CONNECT_REALMODE;
				else if (!strcmp(a,"16pm"))
					req_mode = APM_CONNECT_16_PM;
				else if (!strcmp(a,"32pm"))
					req_mode = APM_CONNECT_32_PM;
				else {
					fprintf(stderr,"Unknown interface %s\n",a);
					return 1;
				}
			}
			else {
				fprintf(stderr,"Unknown param '%s'\n",a);
				help();
				return 1;
			}
		}
		else if (action == NULL) {
			action = a;
		}
		else {
			fprintf(stderr,"Unknown param\n");
			return 1;
		}
	}

	if (!probe_8254()) {
		printf("Cannot init 8254 timer\n");
		return 1;
	}
	if (!probe_8259()) {
		printf("Cannot init 8259 PIC\n");
		return 1;
	}
	probe_dos();
	detect_windows();
	if (!apm_bios_probe()) {
		printf("APM BIOS not found\n");
		return 1;
	}

	if (req_mode == APM_CONNECT_NONE)
		req_mode = APM_CONNECT_REALMODE;

	printf("APM BIOS v%u.%u: ",apm_bios->major,apm_bios->minor);
	if (apm_bios->flags & APM_FL_16BIT_PM) printf("[16-bit pm] ");
	if (apm_bios->flags & APM_FL_32BIT_PM) printf("[32-bit pm] ");
	if (apm_bios->flags & APM_FL_CPU_IDLE_SLOWS) printf("[cpu-idle-slow] ");
	if (apm_bios->flags & APM_FL_PM_DISABLED) printf("[disabled] ");
	if (apm_bios->flags & APM_FL_PM_DISENGAGED) printf("[disengaged] ");
	printf("\n");

	if (req_ver >= 0x100) apm_bios->version_want = req_ver;

	if (req_ver != REQ_VER_NONE) {
		if (!apm_bios_connect(req_mode)) {
			fprintf(stderr,"Failed to connect to APM BIOS (last=0x%02X)\n",apm_bios->last_error);
			return 1;
		}

		printf("Connected to APM BIOS v%u.%u interface\n",apm_bios->major_neg,apm_bios->minor_neg);
		printf("  batteries:    %u\n",apm_bios->batteries);
		printf("  capabilities:\n");
		if (apm_bios->capabilities & 1) printf("    Can enter global standby state\n");
		if (apm_bios->capabilities & 2) printf("    Can enter global suspend state\n");
		if (apm_bios->capabilities & 4) printf("    Resume timer will wake from standby\n");
		if (apm_bios->capabilities & 8) printf("    Resume timer will wake from suspend\n");
		if (apm_bios->capabilities & 16) printf("    Resume on ring will wake from standby\n");
		if (apm_bios->capabilities & 32) printf("    Resume on ring will wake from suspend\n");
		if (apm_bios->capabilities & 64) printf("    PCMCIA ring indicator will wake up from standby\n");
		if (apm_bios->capabilities & 128) printf("    PCMCIA ring indicator will wake up from suspend\n");
		printf("\n");
	}

	if (action == NULL) {
	}
	else if (!strcmp(action,"cpu-idle")) {
		printf("Issuing CPU idle command\n");
		if (!apm_bios_cpu_idle())
			fprintf(stderr,"CPU Idle failed\n");
	}
	else if (!strcmp(action,"cpu-busy")) {
		printf("Issuing CPU busy command\n");
		if (!apm_bios_cpu_busy())
			fprintf(stderr,"CPU Busy failed\n");
	}
	else if (!strcmp(action,"events")) {
		while (1) {
			signed long ev = apm_bios_pm_evnet();
			if (ev >= 0LL) {
				printf("Event 0x%04X\n",(unsigned int)ev);
			}

			if (kbhit()) {
				if (getch() == 27) break;
			}
		}
	}
	else if (!strcmp(action,"status")) {
		apm_bios_update_status();

		printf("AC=0x%X Batt=0x%X BattFlag=0x%X BattPercent=%u BattLife=%u%s\n",
			apm_bios->status_ac,
			apm_bios->status_battery,
			apm_bios->status_battery_flag,
			apm_bios->status_battery_life,
			apm_bios->status_battery_time&0x7FFF,
			(apm_bios->status_battery_time&0x8000)?"m":"s");
	}
	else if (!strcmp(action,"standby")) {
		if (!apm_bios_power_state(APM_POWER_STANDBY))
			fprintf(stderr,"Unable to set power state\n");
	}
	else if (!strcmp(action,"suspend")) {
		if (!apm_bios_power_state(APM_POWER_SUSPEND))
			fprintf(stderr,"Unable to set power state\n");
	}
	else if (!strcmp(action,"off")) {
		if (!apm_bios_power_state(APM_POWER_OFF))
			fprintf(stderr,"Unable to set power state\n");
	}

	if (req_ver != REQ_VER_NONE && !dont_disconnect) {
		if (!apm_bios_connect(APM_CONNECT_NONE)) {
			fprintf(stderr,"Failed to disconnect\n");
		}

		printf("Disconnected APM BIOS\n");
	}

	return 0;
}

