
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

int main() {
    unsigned int cycle,cyclecnt,countmax;
    unsigned short reset_write_freq_bug;
    t8254_time_t pcc,cc;
    unsigned long freq;
    unsigned int count;
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

    printf("This program tries to determine how writing the counter\n");
    printf("affects the square wave depending on how many counter cycles\n");
    printf("(half of a full square wave) occur.\n");

	write_8254_system_timer(0); /* restore normal function to prevent BIOS from going crazy */
    write_8254_pc_speaker(0); /* make sure the PIT timer is in mode 3 (square wave) */
    t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_ON);

    for (freq=0x10000;freq >= 0x800;) {
        cyclecnt = 10;

        t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_OFF);
        write_8254_pc_speaker((t8254_time_t)freq);
        t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_ON);

#if defined(NO_PORT_43)
# define write_8254_pc_speaker writenm_8254_pc_speaker
#endif

        do {
            countmax = ((0x10000ul * 25ul) / freq) / cyclecnt;

            printf("Count 0x%lx cycles-until-change=%u %ux... ",freq,cyclecnt,countmax); fflush(stdout);

            reset_write_freq_bug = 0;
            for (count=0;count < countmax;count++) {
                cycle = cyclecnt;
                cc = read_8254(T8254_TIMER_PC_SPEAKER);
                /* wait for "cycle" countdown cycles to pass */
                do {
                    /* this code is fast enough to read back counter 0x0000 on a 486 */
                    while (cc == 0)
                        cc = read_8254(T8254_TIMER_PC_SPEAKER);

                    /* wait for one countdown cycle */
                    do {
                        pcc = cc;
                        cc = read_8254(T8254_TIMER_PC_SPEAKER);
                    } while (cc < pcc/*counts DOWN*/);
                } while (--cycle != 0u);
                cc = read_8254(T8254_TIMER_PC_SPEAKER);

                /* RAPI4.EXE: write frequency (43h + counter) which resets the counter */
                /* RAPN4.EXE: write frequency (counter only) which does NOT reset the counter */
                write_8254_pc_speaker((t8254_time_t)freq);

                /* NTS: According to real hardware, writing port 43h then the counter resets the counter right away. */
                /*      So it's considered a failure if the counter is not right back up at the counter value.
                 * NTS: On real hardware, this code is fast enough on a 486 to read the counter when it's still at 0x0000. */
                /* NTS: Not writing port 43h (RAPN4) and writing the counter should cause the current countdown to finish
                 *      before starting the new one. In that case the counter should continue without resetting. */
                pcc = cc;
                cc = read_8254(T8254_TIMER_PC_SPEAKER);
#if defined(NO_PORT_43)
                if (cc > pcc) /* if it reset, that's a failure */
                    reset_write_freq_bug = cc;
#else
                if (freq == 0x10000ul && cc == 0)
                    { /* OK */ }
                else if (cc < pcc) /* if it continued counting down despite writing the counter, that's a failure */
                    reset_write_freq_bug = cc;
#endif
            }

            printf("done\n");

            if (reset_write_freq_bug)
#if defined(NO_PORT_43)
                printf("* PIT timer (8254) reset counter on writing frequency without port 43h (0x%x)!\n",reset_write_freq_bug);
#else
                printf("* PIT timer (8254) did not reset counter on writing frequency (0x%x)!\n",reset_write_freq_bug);
#endif

            if (kbhit()) {
                c = getch();
                if (c == 27) {
                    freq = 0;
                    break;
                }
                else if (c == 32) {
                    break;
                }
            }
        } while (--cyclecnt != 0u);

        if (freq > 0xFFFCu)
            freq--;
         else if (freq >= 0x4000u) {
            freq -= 0x2000u;
            freq &= ~0x1FFFu;
        }
        else if (freq != 0u) {
            freq -= 0x400u;
            freq &= ~0x3FFu;
        }
    }

    t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_OFF);
    write_8254_system_timer(0); /* restore normal function to prevent BIOS from going crazy */
	return 0;
}

