
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
#if defined(TARGET_PC98)
#else
    unsigned long cycleout,cyclesub;
    t8254_time_t pcc,cc;
    unsigned long freq;
    unsigned int count;
    unsigned char ok;
    int c;
#endif

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

#if defined(TARGET_PC98)
    /* PC-98 does not have an AND gate on the output of the timer */
    printf("This test is not applicable to the PC-98 platform.\n");
#else
    printf("This program tests the AND gate on the PC speaker output.\n");

//////////////////////////////////////////////////////////////////////////////
    printf("Testing, applying gate to both halves of the square wave.\n");

	write_8254_system_timer(0); /* restore normal function to prevent BIOS from going crazy */
    t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_ON);

    for (freq=0x10000;freq >= 0x800;) {
        ok = 1;

        t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_OFF);
        write_8254_pc_speaker((t8254_time_t)freq);
        t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_ON);

        cyclesub = (0x100ul * (freq - 0x800ul)) / 0x10000ul;
        if (cyclesub == 0) cyclesub = 1;

        printf("Count 0x%lx sub=0x%lx... ",freq,cyclesub); fflush(stdout);

        for (count=0;count < 1;count++) {
            cc = read_8254(T8254_TIMER_PC_SPEAKER);
            for (cycleout=0x400;cycleout < freq;cycleout += cyclesub) {
                /* wait for countdown cycle to hit our threshhold */
                do {
                    pcc = cc;
                    cc = read_8254(T8254_TIMER_PC_SPEAKER);
                } while (cc >= cycleout);

                /* mask but do not gate clock, let it keep ticking */
                t8254_pc_speaker_set_gate(PC_SPEAKER_COUNTER_2_GATE);

                /* wait for countdown cycle to complete */
                do {
                    pcc = cc;
                    cc = read_8254(T8254_TIMER_PC_SPEAKER);
                } while (cc < pcc/*counts DOWN*/);

                t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_ON);
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

//////////////////////////////////////////////////////////////////////////////
    printf("Testing, applying gate to the first half of the square wave.\n");

	write_8254_system_timer(0); /* restore normal function to prevent BIOS from going crazy */
    t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_ON);

    for (freq=0x10000;freq >= 0x800;) {
        ok = 1;

        t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_OFF);
        write_8254_pc_speaker((t8254_time_t)freq);
        t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_ON);

        cyclesub = (0x200ul * (freq - 0x800ul)) / 0x10000ul;
        if (cyclesub == 0) cyclesub = 1;

        printf("Count 0x%lx sub=0x%lx... ",freq,cyclesub); fflush(stdout);

        for (count=0;count < 1;count++) {
            cc = read_8254(T8254_TIMER_PC_SPEAKER);
            for (cycleout=0x400;cycleout < freq;cycleout += cyclesub) {
                /* wait for countdown cycle to hit our threshhold */
                do {
                    pcc = cc;
                    cc = read_8254(T8254_TIMER_PC_SPEAKER);
                } while (cc >= cycleout);

                /* mask but do not gate clock, let it keep ticking */
                t8254_pc_speaker_set_gate(PC_SPEAKER_COUNTER_2_GATE);

                /* wait for countdown cycle to complete */
                do {
                    pcc = cc;
                    cc = read_8254(T8254_TIMER_PC_SPEAKER);
                } while (cc < pcc/*counts DOWN*/);

                t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_ON);

                /* wait for the second half */
                do {
                    pcc = cc;
                    cc = read_8254(T8254_TIMER_PC_SPEAKER);
                } while (cc < pcc/*counts DOWN*/);
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

//////////////////////////////////////////////////////////////////////////////
    printf("Testing, applying gate to the second half of the square wave.\n");
    printf("This test may cause no noticable effect on the square wave.\n");
    /* ^ Because in the second half of the square wave the 8254's output is LOW.
     *   Changing the gate while LOW has no effect on the output. */

	write_8254_system_timer(0); /* restore normal function to prevent BIOS from going crazy */
    t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_ON);

    for (freq=0x10000;freq >= 0x800;) {
        ok = 1;

        t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_OFF);
        write_8254_pc_speaker((t8254_time_t)freq);
        t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_ON);

        cyclesub = (0x200ul * (freq - 0x800ul)) / 0x10000ul;
        if (cyclesub == 0) cyclesub = 1;

        printf("Count 0x%lx sub=0x%lx... ",freq,cyclesub); fflush(stdout);

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

                /* mask but do not gate clock, let it keep ticking */
                t8254_pc_speaker_set_gate(PC_SPEAKER_COUNTER_2_GATE);

                /* wait for countdown cycle to complete */
                do {
                    pcc = cc;
                    cc = read_8254(T8254_TIMER_PC_SPEAKER);
                } while (cc < pcc/*counts DOWN*/);

                t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_ON);
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
#endif
	return 0;
}

