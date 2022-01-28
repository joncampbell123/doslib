/* rdtsc.c
 *
 * Test program: Detect and display the Pentium Time Stamp Counter using RDTSC.
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <time.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/cpu/cpurdtsc.h>
#ifndef TARGET_WINDOWS
# include <hw/8254/8254.h>
#endif

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

#if defined(TARGET_OS2) && TARGET_MSDOS == 32
static ULONG GetMsCount() {
	ULONG ret=0;
	DosQuerySysInfo(QSV_MS_COUNT,QSV_MS_COUNT,&ret,sizeof(ret));
	return ret;
}
#endif

int main(int argc,char *argv[],char *envp[]) {
	rdtsc_t start,measure,ticks_per_sec;
    unsigned char force = 0;
	double t;
	int c;

    {
        int i = 1;
        char *a;

        while (i < argc) {
            a = argv[i++];

            if (*a == '-') {
                do { a++; } while (*a == '-');

                if (!strcmp(a,"f") || !strcmp(a,"force"))
                    force = 1;
                else
                    return 1;
            }
            else {
                return 1;
            }
        }
    }

	cpu_probe();
	printf("Your CPU is basically a %s or higher\n",cpu_basic_level_to_string(cpu_basic_level));
	if (cpu_v86_active)
		printf(" - Your CPU is currently running me in virtual 8086 mode\n");

    if (force) {
        printf(" - You're forcing me to use RDTSC. I may crash if your CPU doesn't\n");
        printf("   support it or the environment doesn't enable it.\n");
        printf("   OK! Here we go....!\n");
    }
    else {
        if (!(cpu_flags & CPU_FLAG_CPUID)) {
            printf(" - Your CPU doesn't support CPUID, how can it support RDTSC?\n");
            return 1;
        }

        if (!(cpu_cpuid_features.a.raw[2] & 0x10)) {
            printf(" - Your CPU does not support RDTSC\n");
            return 1;
        }

        if (cpu_flags & CPU_FLAG_DONT_USE_RDTSC) {
            printf(" - Your CPU does support RDTSC but it's not recommended in this environment.\n");
            printf("   This is usually due to running a 16-bit build in pure DOS under EMM386.EXE.\n");
            return 1;
        }
    }

#if defined(TARGET_OS2)
# if TARGET_MSDOS == 32
	/* OS/2 32-bit: We can use DosQuerySysInfo() */
	printf("Measuring CPU speed, using DosQuerySysInfo(), over 3 seconds\n");
	{
		ULONG startTick,tmp;
		start = cpu_rdtsc();
		startTick = GetMsCount();
		do { tmp = GetMsCount();
		} while ((tmp - startTick) < 3000); /* NTS: <- I know this rolls over in 49 days,
		the math though should overflow the 32-bit integer and produce correct results anyway */
		measure = cpu_rdtsc();

		/* use the precise tick count to better compute ticks_per_sec */
		ticks_per_sec = ((measure - start) * 1000ULL) / ((rdtsc_t)(tmp - startTick));
		printf("Measurement: %lums = %lld ticks\n",tmp - startTick,(int64_t)ticks_per_sec);
		printf("             From 0x%llX to 0x%llX\n",start,measure);
	}
# else
	/* OS/2 16-bit: There is no API (that I know of) to get tick count. Use system clock. */
	printf("Measuring CPU speed, using system clock with 1-second resolution, across 3 seconds\n");
	{
		time_t startTick,tmp;

		/* wait for the immediate start of a one-second tick, then record RDTSC and count until 3 seconds */
		startTick = time(NULL);
		do { tmp = time(NULL); } while (tmp == startTick);
		start = cpu_rdtsc(); startTick = tmp;

		/* NOW! Count 3 seconds and measure CPU ticks */
		do { tmp = time(NULL);
		} while ((tmp - startTick) < 3);
		measure = cpu_rdtsc();

		/* use the precise tick count to better compute ticks_per_sec */
		ticks_per_sec = ((measure - start) * 1ULL) / ((rdtsc_t)(tmp - startTick));
		printf("Measurement: %lu seconds = %lld ticks\n",tmp - startTick,(int64_t)ticks_per_sec);
		printf("             From 0x%llX to 0x%llX\n",start,measure);
	}
# endif
#elif defined(TARGET_WINDOWS)
# if TARGET_MSDOS == 16
	/* Windows 2.x/3.0/3.1: Use GetTickCount() or
	 * Windows 3.1: Use TOOLHELP.DLL TimerCount() which is more accurate (really?) */
	if (ToolHelpInit()) {
		TIMERINFO ti;

		ti.dwSize = sizeof(ti);
		printf("Measuring CPU speed, using TOOLHELP TimerCount() over 1 second\n");
		{
			DWORD startTick,tmp;
			start = cpu_rdtsc();
			if (!__TimerCount(&ti)) {
				printf("TimerCount() failed\n");
				return 1;
			}
			startTick = ti.dwmsSinceStart;
			do {
#  if defined(WIN_STDOUT_CONSOLE)
				_win_pump(); /* <- you MUST call this. The message pump must run, or else the timer won't advance and this loop will run forever.
			                           The fact that GetTickCount() depends on a working message pump under Windows 3.1 seems to be a rather serious
						   oversight on Microsoft's part. Note that the problem described here does not apply to Windows 9x/ME. Also note
						   the Toolhelp function TimerCount() relies on GetTickCount() as a basic for time (though Toolhelp uses VxD services
						   or direct I/O port hackery to refine the timer count) */
#  endif

				if (!__TimerCount(&ti)) {
					printf("TimerCount() failed\n");
					return 1;
				}

				tmp = ti.dwmsSinceStart;
			} while ((tmp - startTick) < 1000); /* NTS: <- I know this rolls over in 49 days,
			the math though should overflow the 32-bit integer and produce correct results anyway */
			measure = cpu_rdtsc();

			/* use the precise tick count to better compute ticks_per_sec */
			ticks_per_sec = ((measure - start) * 1000ULL) / ((rdtsc_t)(tmp - startTick));
			printf("Measurement: %lums = %lld ticks\n",tmp - startTick,(int64_t)ticks_per_sec);
			printf("             From 0x%llX to 0x%llX\n",start,measure);
		}
	}
	else {
# else
	{
# endif
		printf("Measuring CPU speed, using GetTickCount() over 3 second\n");
		{
			DWORD startTick,tmp;
			start = cpu_rdtsc();
			startTick = GetTickCount();
			/* NTS: Dunno yet about Windows 3.1, but Windows 95 seems to require we Yield(). If we don't, the GetTickCount() return
			 *      value never updates and we're forever stuck in a loop. */
			do {
#  if defined(WIN_STDOUT_CONSOLE)
				_win_pump(); /* <- you MUST call this. The message pump must run, or else the timer won't advance and this loop will run forever.
			                           The fact that GetTickCount() depends on a working message pump under Windows 3.1 seems to be a rather serious
						   oversight on Microsoft's part. Note that the problem described here does not apply to Windows 9x/ME */
#  endif
				tmp = GetTickCount();
			} while ((tmp - startTick) < 3000); /* NTS: <- I know this rolls over in 49 days,
			the math though should overflow the 32-bit integer and produce correct results anyway */
			measure = cpu_rdtsc();

			/* use the precise tick count to better compute ticks_per_sec */
			ticks_per_sec = ((measure - start) * 1000ULL) / ((rdtsc_t)(tmp - startTick));
			printf("Measurement: %lums = %lld ticks\n",tmp - startTick,(int64_t)ticks_per_sec);
			printf("             From 0x%llX to 0x%llX\n",start,measure);
		}
	}
#else
	/* MS-DOS: Init the 8254 timer library for precise measurement */
	if (!probe_8254()) {
		printf("Cannot init 8254 timer\n");
		return 1;
	}

    /* DOSBox-X and most motherboards leave the PIT in a mode that counts down by 2.
     * Our code sets the counter and puts it in a mode that counts down by 1.
     * We have to do this or our wait functions exit in half the time (which explains
     * why testing this code in DOSBox-X often comes up with RDTSC running at 2x
     * normal speed. */
    write_8254_system_timer(0); /* 18.2 tick/sec on our terms (proper PIT mode) */

	printf("Measuring CPU speed (relative to 8254 timer)\n");

	_cli();
	start = cpu_rdtsc();
	t8254_wait(t8254_us2ticks(1000000));
	measure = cpu_rdtsc();
	_sti();

	printf("Measurement: 1 sec = %lld ticks\n",(int64_t)(measure - start));
	printf("             From 0x%llX to 0x%llX\n",start,measure);
	ticks_per_sec = (measure - start);
#endif

	if ((int64_t)ticks_per_sec < 0) {
		printf("Cannot determine CPU cycle count\n");
		ticks_per_sec = 100000ULL;
	}

	while (1) {
		measure = cpu_rdtsc();
		t = (double)((int64_t)(measure - start));
		t /= ticks_per_sec;

		printf("\x0D" "0x%llX = %.3f   ",measure,t);
#if !defined(WINFCON_STOCK_WIN_MAIN)
		fflush(stdout); /* FIXME: The fake console code should intercept fflush() too */
#endif

		if (kbhit()) {
			c = getch();
			if (c == 0) c = getch() << 8;

			if (c == 27)
				break;
			else if (c == 'r' || c == 'R') {
				if (c == 'r' && (cpu_flags & CPU_FLAG_DONT_WRITE_RDTSC)) {
					printf("\nI am not able to write the TSC register within this environment\nYou can force me by typing SHIFT+R, don't blame me when I crash...\n");
				}
				else {
					printf("\nUsing MSR to reset TSC to 0\n");
					/* demonstrating WRMSR to write the TSC (yes, you can!) */
					cpu_rdtsc_write(start = 0ULL);
					printf("Result: 0x%llX\n",cpu_rdtsc());
				}
			}
			else if (c == 's') {
				if (c == 's' && (cpu_flags & CPU_FLAG_DONT_WRITE_RDTSC)) {
					printf("\nI am not able to write the TSC register within this environment\nYou can force me by typing SHIFT+S, don't blame me when I crash...\n");
				}
				else {
					printf("\nUsing MSR to reset TSC to 0x123456789ABCDEF\n");
					/* demonstrating WRMSR to write the TSC (yes, you can!) */
					cpu_rdtsc_write(start = 0x123456789ABCDEFULL);
					printf("Result: 0x%llX\n",cpu_rdtsc());
				}
			}
		}
	}
	printf("\n");

	return 0;
}

