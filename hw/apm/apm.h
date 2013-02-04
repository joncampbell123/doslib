/* apm.h
 *
 * Advanced Power Management BIOS library.
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box] */
 
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
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC */

#define APM_PM_SIG		0x504D /* 'PM' */

enum {
	APM_CONNECT_NONE=0,
	APM_CONNECT_REALMODE=1,
	APM_CONNECT_16_PM=2,
	APM_CONNECT_32_PM=3
};

enum {
	APM_POWER_STANDBY=0x0001,
	APM_POWER_SUSPEND=0x0002,
	APM_POWER_OFF=0x0003
};

#pragma pack(push,1)
struct apm_bios_ctx {
	/* do NOT move these structure members---the assembly language requires them */
	unsigned char	major,minor;	/* +0, +1 */
	unsigned short	signature;	/* +2 */
	unsigned short	flags;		/* +4 */
#define APM_FL_16BIT_PM		(1U << 0U)	/* 16-bit protected mode interface supported */
#define APM_FL_32BIT_PM		(1U << 1U)	/* 32-bit protected mode interface supported */
#define APM_FL_CPU_IDLE_SLOWS	(1U << 2U)	/* CPU idle call slows processor clock */
#define APM_FL_PM_DISABLED	(1U << 3U)	/* Power Management Disabled */
#define APM_FL_PM_DISENGAGED	(1U << 4U)	/* Power Management Disengaged */
	/* -------------------------------------------------- */
	unsigned char	connection;		/* last known connection to the BIOS */
	unsigned char	last_error;
	unsigned short	version_want;		/* which APM BIOS the caller wants */
	unsigned char	major_neg,minor_neg;	/* negotiated API with BIOS */
	unsigned short	capabilities;
	unsigned char	batteries;
	/* status */
	unsigned char	status_ac,status_battery;
	unsigned char	status_battery_flag,status_battery_life;
	unsigned short	status_battery_time;
};
#pragma pack(pop)

extern struct apm_bios_ctx *apm_bios;

#define apm_bios_disconnect() apm_bios_connect(APM_CONNECT_NONE)

void apm_bios_free();
int apm_bios_probe();
int apm_bios_cpu_busy();
int apm_bios_cpu_idle();
int apm_bios_connect(int n);
void apm_bios_update_status();
signed long apm_bios_pm_evnet();
void apm_bios_update_capabilities();
int apm_bios_power_state(unsigned short state);

