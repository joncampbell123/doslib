
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
    unsigned long cycleout,cyclesub;
    t8254_time_t pcc,cc;
    unsigned long freq;
    unsigned int count;
    unsigned char ok;
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

    printf("This program tests whether toggling the clock gate resets the counter\n");
    printf("and affects the sound.\n");

    /* NTS: This should work on both PC-98 (which has only the clock gate control) and IBM PC (which has clock gate AND speaker enable) */

	write_8254_system_timer(0); /* restore normal function to prevent BIOS from going crazy */
    t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_ON);

    for (freq=0x10000;freq >= 0x800;) {
        ok = 1;

        t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_OFF);
        write_8254_pc_speaker((t8254_time_t)freq);
        t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_ON);

        /* the toggle should have reset the counter.
         * we should read a value back that is "freq" or close to "freq" */
        cc = read_8254(T8254_TIMER_PC_SPEAKER);
        if ((unsigned long)cc < (freq - 0x80ul) || (unsigned long)cc > freq) {
            printf("* unusual counter after reset: 0x%x (out of expected range)\n",cc);
            ok = 0;
        }

        cyclesub = (0x100ul * (freq - 0x800ul)) / 0x10000ul;
        if (cyclesub == 0) cyclesub = 1;

        printf("Count 0x%lx after-reset=0x%x sub=0x%lx... ",freq,cc,cyclesub); fflush(stdout);

        for (count=0;count < 1;count++) {
            cc = read_8254(T8254_TIMER_PC_SPEAKER);
            for (cycleout=0x400;cycleout < freq;cycleout += cyclesub) {
                /* wait for the first half */
                do {
                    pcc = cc;
                    cc = read_8254(T8254_TIMER_PC_SPEAKER);
                } while (cc < pcc/*counts DOWN*/);

                /* wait for countdown cycle to hit our threshhold */
                do {
                    pcc = cc;
                    cc = read_8254(T8254_TIMER_PC_SPEAKER);
                } while (cc >= cycleout);

                /* reset */
                t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_OFF);
                t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_ON);

                /* FIXME: DOSBox-X appears to have a bug where NOT reading back the counter
                 *        after reset results in silence instead of an audible square wave.
                 *
                 *        #if 0 the following readback, compile and run on real hardware and
                 *        check this case. */

                /* the toggle should have reset the counter.
                 * we should read a value back that is "freq" or close to "freq" */
                cc = read_8254(T8254_TIMER_PC_SPEAKER);
                if ((unsigned long)cc < (freq - 0x80ul) || (unsigned long)cc > freq) {
                    printf("* unusual counter after reset: 0x%x (out of expected range)\n",cc);
                    ok = 0;
                }
            }
        }

        printf("%s\n",ok?"PASS":"FAIL");

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

        if (freq > 0xFFFEu)
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

