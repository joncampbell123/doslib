/* Test whether the PIT timer output of the PC speaker can be read back and how accurately.
 * This is only applicable to IBM PC. PC-98 does not have this function in hardware. */

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
        rdmiss = 0; \
        rdhit = 0; \
        ok = 0; \
 \
        tcc = read_8254(0); \
        scc = read_8254(T8254_TIMER_PC_SPEAKER); \
        crd = read_8254_pc_speaker_output(); \
        exrd = crd; \
 \
        do { \
            crd = read_8254_pc_speaker_output(); \
            if (((crd^exrd) & 0x20) == 0) rdhit++; \
            else rdmiss++; \
            \
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
            if (scc <= spc) {/*likely*/ \
                delta = spc - scc; \
            } \
            else {/*reset condition*/ \
                delta = spc + (t8254_time_t)cn - scc; \
                exrd ^= 0x20; \
                spreset++; \
            } \
 \
            spcnt += delta; \
        } while (tim > 0); \
    }

int main() {
#if !defined(TARGET_PC98)//IBM PC only. PC-98 does not have a separate gate bit for the "buzzer" as far as I know
    t8254_time_t tpc,tcc;
    t8254_time_t spc,scc;
    t8254_time_t delta;
    unsigned char exrd,crd; // expected vs actual
    unsigned long rdhit;
    unsigned long rdmiss;
    unsigned long spreset;
    unsigned long sprange;
    unsigned long tmcnt;
    unsigned long spcnt;
    unsigned long cn;
    unsigned char ok;
    int32_t tim;
    int c;

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

    printf("tt = Timer ticks (PIT 0) counted during test\n");
    printf("st = Speaker ticks (PIT %u) counted during test\n",T8254_TIMER_PC_SPEAKER);
    printf("sl = Number of times counter reset (relatched) on PC speaker during test\n");
    printf("sr = Number of times counter was out of range on PC speaker counter\n");
    printf("rh = Readback hits (matches expectations)\n");
    printf("rm = Readback misses (did not meet expectations)\n");

    printf("This program tests whether the PIT timer output, tied to PC speaker,\n");
    printf("can be read back.\n");

	write_8254_system_timer(0); /* restore normal function to prevent BIOS from going crazy */

////////////////////////////////////////////////////////////////////////////////////////
    c = 0;
    t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_ON);
    for (cn=0x10000;cn >= 0x80/*routine may be too slow for smaller values*/;) {
        if (cn >= 0x2000)
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
        {
            if (rdhit >= 256) {/*even the slowest computer could do this */
                /* rdmiss is usually not zero. there is I/O delay between reading output and the timer
                 * between which the PIT can change state. It's usually (so far) 200 or less, usually
                 * 10-80. Consider it a failure if more than 2000 or 10% of the hit rate */
                if (rdmiss >= (rdhit / 10ul)) ok = 0;
            }
            else {
                ok = 0;
            }
        }

        printf("* %s -- result: tt=%lu st=%lu sl=%lu sr=%lu rh=%lu rm=%lu\n",ok?"PASS":"FAIL",tmcnt,spcnt,spreset,sprange,rdhit,rdmiss);

        if (kbhit()) {
            c = getch();
            if (c == 27 || c == ' ') break;
        }
    }
    t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_OFF);
    if (c == 27) return 0;
////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////
    c = 0;
    t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_OFF);
    for (cn=0x10000;cn >= 0x80/*routine may be too slow for smaller values*/;) {
        if (cn >= 0x2000)
            cn -= 0xFB3;
        else if (cn >= 0x100)
            cn -= 0x51;
        else
            cn -= 0x7;

        printf("Testing with speaker OFF: count=%lu %.3fHz:\n",(unsigned long)cn,((double)T8254_REF_CLOCK_HZ) / cn);

        write_8254_pc_speaker(cn);

        DO_TEST();

        /* This time, no longer how much time is given, the PIT counter should not change */
        ok = 1;
        if (spcnt != 0ul || spreset != 0ul) ok = 0;
        {
            if (rdhit >= 256) {/*even the slowest computer could do this */
                if (rdmiss != 0ul) ok = 0; /* timer isn't cycling, it shouldn't have any misses! */
            }
            else {
                ok = 0;
            }
        }

        printf("* %s -- result: tt=%lu st=%lu sl=%lu sr=%lu rh=%lu rm=%lu\n",ok?"PASS":"FAIL",tmcnt,spcnt,spreset,sprange,rdhit,rdmiss);

        if (kbhit()) {
            c = getch();
            if (c == 27 || c == ' ') break;
        }
    }
    t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_OFF);
    if (c == 27) return 0;
////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////
    c = 0;
    t8254_pc_speaker_set_gate(PC_SPEAKER_OUTPUT_TTL_AND_GATE);
    for (cn=0x10000;cn >= 0x80/*routine may be too slow for smaller values*/;) {
        if (cn >= 0x2000)
            cn -= 0xFB3;
        else if (cn >= 0x100)
            cn -= 0x51;
        else
            cn -= 0x7;

        printf("Testing, spkr ON, counter OFF: count=%lu %.3fHz:\n",(unsigned long)cn,((double)T8254_REF_CLOCK_HZ) / cn);

        write_8254_pc_speaker(cn);

        DO_TEST();

        /* This time, no longer how much time is given, the PIT counter should not change */
        ok = 1;
        if (spcnt != 0ul || spreset != 0ul) ok = 0;
        {
            if (rdhit >= 256) {/*even the slowest computer could do this */
                if (rdmiss != 0ul) ok = 0; /* timer isn't cycling, it shouldn't have any misses! */
            }
            else {
                ok = 0;
            }
        }

        printf("* %s -- result: tt=%lu st=%lu sl=%lu sr=%lu rh=%lu rm=%lu\n",ok?"PASS":"FAIL",tmcnt,spcnt,spreset,sprange,rdhit,rdmiss);

        if (kbhit()) {
            c = getch();
            if (c == 27 || c == ' ') break;
        }
    }
    t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_OFF);
    if (c == 27) return 0;
////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////
    c = 0;
    t8254_pc_speaker_set_gate(PC_SPEAKER_COUNTER_2_GATE);
    for (cn=0x10000;cn >= 0x80/*routine may be too slow for smaller values*/;) {
        if (cn >= 0x2000)
            cn -= 0xFB3;
        else if (cn >= 0x100)
            cn -= 0x51;
        else
            cn -= 0x7;

        printf("Testing, spkr OFF, counter ON: count=%lu %.3fHz:\n",(unsigned long)cn,((double)T8254_REF_CLOCK_HZ) / cn);

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
        {
            if (rdhit >= 256) {/*even the slowest computer could do this */
                /* rdmiss is usually not zero. there is I/O delay between reading output and the timer
                 * between which the PIT can change state. It's usually (so far) 200 or less, usually
                 * 10-80. Consider it a failure if more than 2000 or 10% of the hit rate */
                if (rdmiss >= (rdhit / 10ul)) ok = 0;
            }
            else {
                ok = 0;
            }
        }

        printf("* %s -- result: tt=%lu st=%lu sl=%lu sr=%lu rh=%lu rm=%lu\n",ok?"PASS":"FAIL",tmcnt,spcnt,spreset,sprange,rdhit,rdmiss);

        if (kbhit()) {
            c = getch();
            if (c == 27 || c == ' ') break;
        }
    }
    t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_OFF);
    if (c == 27) return 0;
////////////////////////////////////////////////////////////////////////////////////////

	write_8254_system_timer(0); /* restore normal function to prevent BIOS from going crazy */
#else
    printf("This test is not applicable to this target\n");
#endif
	return 0;
}

