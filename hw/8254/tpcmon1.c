/* Test whether the PC speaker counter can be read back, and if it counts down within the range given.
 * Some tests in this collection require this to work. */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/8254/8254.h>

#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/dos/dosbox.h>
#include <hw/dos/dosntvdm.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

#define DO_TEST() \
    { \
        tim = T8254_REF_CLOCK_HZ; \
        spcnt = tmcnt = 0; \
        spreset = 0; \
        sprange = 0; \
        ok = 0; \
 \
        tcc = read_8254(0); \
        scc = read_8254(T8254_TIMER_PC_SPEAKER); \
 \
        do { \
            tpc = tcc; \
            spc = scc; \
 \
            tcc = read_8254(0); \
            scc = read_8254(T8254_TIMER_PC_SPEAKER); \
 \
            delta = tpc - tcc; /* 8254 counts DOWN. interval of 0 (0x10000) means we just use unsigned integer overflow */ \
            tmcnt += delta; \
            tim -= delta; \
 \
            if ((unsigned long)scc > cn) /* TODO: Should we check scc >= cn? */ \
                sprange++; /* out of range */ \
 \
            /* 8254 counts DOWN. interval is (cn). If the new value is larger then the counter hit 0 and rolled back */ \
            if (scc < spc) {/*likely*/ \
                delta = spc - scc; \
            } \
            else {/*reset condition*/ \
                delta = spc + (t8254_time_t)cn - scc; \
                spreset++; \
            } \
 \
            spcnt += delta; \
        } while (tim > 0); \
    }

int main() {
    t8254_time_t tpc,tcc;
    t8254_time_t spc,scc;
    t8254_time_t delta;
    unsigned long spreset;
    unsigned long sprange;
    unsigned long tmcnt;
    unsigned long spcnt;
    unsigned long cn;
    unsigned char ok;
    int32_t tim;

#if defined(TARGET_WINDOWS) && TARGET_WINDOWS == 32 && !defined(WIN386)
    /* As a 32-bit Windows application, we *can* talk directly to the 8254 but only if running under Windows 3.1/95/98/ME.
     * Windows NT would never permit us to directly talk to IO.
     *
     * However if we're a Win16 program or Watcom 386 application, Windows NT will trap and emulate the 8254 through NTVDM.EXE */
    detect_windows();
    if (!(windows_mode == WINDOWS_REAL || windows_mode == WINDOWS_STANDARD || windows_mode == WINDOWS_ENHANCED)) {
        printf("This test program is only applicable to Windows 3.x/9x\n");
        return 1;
    }
#endif

	printf("8254 library test program\n");
	if (!probe_8254()) {
		printf("Chip not present. Your computer might be 2010-era hardware that dropped support for it.\n");
		return 1;
	}

    printf("This program tests whether the PIT timer tied to PC speaker works.\n");

	write_8254_system_timer(0); /* restore normal function to prevent BIOS from going crazy */

    t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_ON);
    for (cn=0x10000;cn >= 0xB0/*routine may be too slow for smaller values*/;) {
        if (cn >= 0x1000)
            cn -= 0xFB3;
        else if (cn >= 0x100)
            cn -= 0x51;
        else
            cn -= 0x7;

        printf("Testing: count=%lu %.3fHz:\n",(unsigned long)cn,((double)T8254_REF_CLOCK_HZ) / cn);

        write_8254_pc_speaker(cn);

        DO_TEST();

        /* Mode 3 is a square wave mode, which means the 8254 will load the counter and count by 2 with output HIGH.
         * When the counter hits zero, it reloads and counts by 2 again with output LOW.
         *
         * The timer is set to mode 2 by this code, and will count down by 1 per tick.
         *
         * So the expected result is spcnt = (tmcnt * 2ul) and spreset = frequency in Hz.
         *
         * Of course hardware isn't perfect so there is some leeway. */
        ok = 1;
        {
            signed long threshhold = (signed long)(T8254_REF_CLOCK_HZ / 10000); /* 0.1ms tolerance */
            signed long d = (signed long)(tmcnt * 2ul) - (signed long)spcnt;
            if (labs(d) > threshhold) ok = 0;
        }
        {
            signed long d = (signed long)spreset - (signed long)((T8254_REF_CLOCK_HZ * 2ul) / cn);
            if (labs(d) > 2l) ok = 0;
        }

        printf("* %s -- result: timrtck=%lu spktck=%lu spkltch=%lu spkrnge=%lu\n",ok?"PASS":"FAIL",tmcnt,spcnt,spreset,sprange);

        if (kbhit()) {
            int c = getch();
            if (c == 27 || c == ' ') break;
        }
    }
    t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_OFF);

	write_8254_system_timer(0); /* restore normal function to prevent BIOS from going crazy */
	return 0;
}

