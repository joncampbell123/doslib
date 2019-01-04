
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
    printf("affects the square wave by writing at a fixed point within\n");
    printf("the interval.\n");

	write_8254_system_timer(0); /* restore normal function to prevent BIOS from going crazy */
    t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_ON);

    for (freq=0x10000;freq >= 0x800;) {
        cyclesub = (0x100ul * (freq - 0x800ul)) / 0x10000ul;
        if (cyclesub == 0) cyclesub = 1;

        printf("Count 0x%lx sub=0x%lx... ",freq,cyclesub); fflush(stdout);

        for (count=0;count < 1;count++) {
            t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_OFF);
            write_8254_pc_speaker((t8254_time_t)freq);
            t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_ON);

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

                write_8254_pc_speaker((t8254_time_t)freq);

                /* wait for countdown cycle to complete */
                do {
                    pcc = cc;
                    cc = read_8254(T8254_TIMER_PC_SPEAKER);
                } while (cc < pcc/*counts DOWN*/);
            }
        }

        printf("done\n");

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

    printf("Again, without waiting for the first countdown.\n");
    printf("The following test may be completely silent.\n");

    for (freq=0x10000;freq >= 0x800;) {
        cyclesub = (0x100ul * (freq - 0x800ul)) / 0x10000ul;
        if (cyclesub == 0) cyclesub = 1;

        printf("Without waiting for first cycle, count 0x%lx sub=0x%lx... ",freq,cyclesub); fflush(stdout);

        for (count=0;count < 1;count++) {
            t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_OFF);
            write_8254_pc_speaker((t8254_time_t)freq);
            t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_ON);

            cc = read_8254(T8254_TIMER_PC_SPEAKER);
            for (cycleout=0x400;cycleout < freq;cycleout += cyclesub) {
                /* wait for countdown cycle to hit our threshhold */
                do {
                    pcc = cc;
                    cc = read_8254(T8254_TIMER_PC_SPEAKER);
                } while (cc >= cycleout);

                write_8254_pc_speaker((t8254_time_t)freq);

                /* wait for countdown cycle to complete */
                do {
                    pcc = cc;
                    cc = read_8254(T8254_TIMER_PC_SPEAKER);
                } while (cc < pcc/*counts DOWN*/);
            }
        }

        printf("done\n");

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

    printf("Again, without waiting for the first countdown,\n");
    printf("but waiting one cycle after setting the counter\n");
    printf("The following test may be completely silent.\n");

    for (freq=0x10000;freq >= 0x800;) {
        cyclesub = (0x100ul * (freq - 0x800ul)) / 0x10000ul;
        if (cyclesub == 0) cyclesub = 1;

        printf("Without waiting for first cycle, count 0x%lx sub=0x%lx... ",freq,cyclesub); fflush(stdout);

        for (count=0;count < 1;count++) {
            t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_OFF);
            write_8254_pc_speaker((t8254_time_t)freq);
            t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_ON);

            cc = read_8254(T8254_TIMER_PC_SPEAKER);
            do {
                pcc = cc;
                cc = read_8254(T8254_TIMER_PC_SPEAKER);
            } while (cc < pcc/*counts DOWN*/);

            cc = read_8254(T8254_TIMER_PC_SPEAKER);
            for (cycleout=0x400;cycleout < freq;cycleout += cyclesub) {
                /* wait for countdown cycle to hit our threshhold */
                do {
                    pcc = cc;
                    cc = read_8254(T8254_TIMER_PC_SPEAKER);
                } while (cc >= cycleout);

                write_8254_pc_speaker((t8254_time_t)freq);

                /* wait for countdown cycle to complete */
                do {
                    pcc = cc;
                    cc = read_8254(T8254_TIMER_PC_SPEAKER);
                } while (cc < pcc/*counts DOWN*/);
            }
        }

        printf("done\n");

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

