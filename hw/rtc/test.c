
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

#include <hw/rtc/rtc.h>
#include <hw/vga/vga.h>
#include <hw/dos/dos.h>
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC */
#include <hw/dos/doswin.h>
#include <hw/vga/vgagui.h>
#include <hw/vga/vgatty.h>

static const char *months[12] = {
	"January",
	"February",
	"March",
	"April",
	"May",
	"June",
	"July",
	"August",
	"September",
	"October",
	"November",
	"December"
};

static const char *weekdays[7] = {
	"Sunday",
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday"
};

static volatile unsigned int alarm_ints = 0;
static volatile unsigned int update_ints = 0;
static volatile unsigned long periodic_ints = 0;
static volatile unsigned long periodic_sec_ints = 0;
static volatile unsigned long periodic_measured_ints = 0;
static volatile unsigned long irq8_ints = 0;

static unsigned char bcd_to_binary(unsigned char c) {
	return ((c >> 4) * 10) + (c & 0xF);
}

static const char *weekday_to_str(unsigned char c) {
	if (c == 0 || c > 7) return "??";
	return weekdays[c-1];
}

static const char *month_to_str(unsigned char c) {
	if (c == 0 || c > 12) return "??";
	return months[c-1];
}

void interrupt far *old_irq8 = NULL;
void interrupt far rtc_int() {
	unsigned char fl,patience=64;

	_cli();

	/* ack PIC for IRQ 8 */
	p8259_OCW2(8,P8259_OCW2_NON_SPECIFIC_EOI);
	p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);

	/* Real hardware experience: If you set the periodic rate to max
	   (8192Hz) it IS possible for both a periodic and update-ended
	   interrupt to occur at the same time. If that happens, then
	   apparently the RTC will still only fire one IRQ event. If we
	   don't check for additional events, we can leave the RTC IRQ
	   "stuck" waiting for service. (Observed on a Pentium 133MHz
	   Intel 430-based motherboard). */
	/* Also observed on the same P133MHz system: If this ISR is slow
	   enough or the CPU has enough overhead, and the Periodic interrupt
	   is cranked up to 8192Hz, it is possible for this code to miss
	   update-ended interrupts entirely. You'll see evidence of this
	   when every so often the clock on-screen seems to stop, then skip
	   a whole second. It doesn't seem to happen at lower periodic
	   rates. Prior to moving the PIC ACK to the top, that scenario
	   would eventually result in us missing an interrupt and the
	   on-screen display "freezing" (when in fact the PIC was waiting
	   for another ACK) */
	do {
		fl = rtc_io_read(0x0C);	/* read status register C */
		if (fl & RTC_STATUS_C_INTERRUPT_REQUEST) { /* if there actually was an interrupt, then do something */
			irq8_ints++;
			if (fl & RTC_STATUS_C_PERIODIC_INTERRUPT) {
				/* Real hardware experience:
				   Most motherboard RTCs will set this
				   bit even if periodic interrupts are
				   disabled. If all you have are update-ended
				   interrupts, then this code will simply
				   register periodic interrupts as happening
				   one tick per second. It's almost as if
				   the "enable" bits are just gates to whether
				   or not they trigger IRQ 8. */
				periodic_sec_ints++;
				periodic_ints++;
			}
			if (fl & RTC_STATUS_C_ALARM_INTERRUPT) {
				alarm_ints++;
			}
			if (fl & RTC_STATUS_C_UPDATE_ENDED_INTERRUPT) {
				/* Real hardware experience:
				   Most motherboard RTCs will actually
				   signal update-ended per second if
				   either the Update-Ended interrupt
				   or the Periodic interrupt is enabled.
				   You can have the Update-Ended interrupt
				   disabled and still receive these events
				   anyway from your ISR during periodic.

				   Also noted: If the "freeze" bit (bit 7)
				   is set in Status Register B, and left that
				   way, the RTC will not send update-ended
				   events, and will not run the clock. */
				periodic_measured_ints = periodic_sec_ints;
				periodic_sec_ints = 0;
				update_ints++;
			}
		}
		if (--patience == 0) break;
	} while (fl & RTC_STATUS_C_INTERRUPT_REQUEST);

	rtc_io_finished();
}

int main(int argc,char **argv) {
	char tmp[128];
	unsigned int y,i;
	unsigned long pirq8_ints = 0;
	unsigned int pupdate_ints = 0;
	unsigned char redraw_all=1,redraw=1,update_rdtime=1;

	if (!probe_8254()) {
		printf("Cannot init 8254 timer\n");
		return 1;
	}
	if (!probe_8259()) {
		printf("Cannot init 8259 PIC\n");
		return 1;
	}
	if (!probe_rtc()) {
		printf("RTC/CMOS not found\n");
		return 1;
	}
	if (!probe_vga()) {
		printf("Cannot detect VGA\n");
		return 1;
	}
	probe_dos();
	detect_windows();

	/* set 80x25 mode 3 */
	__asm {
		mov	ax,3
		int	10h
	}
	update_state_from_vga();

	/* enable the Update Ended interrupt for display niceness */
	/* NOTE: DOSBox 0.74 does not emulate the Alarm and Update Ended RTC interrupts */
	rtc_io_write(0xB,rtc_io_read(0xB) | 0x10);
	rtc_io_finished();

	/* take IRQ 8 */
	old_irq8 = _dos_getvect(0x70);
	_dos_setvect(0x70,rtc_int);
	p8259_unmask(8);

	while (1) {
		if (redraw_all) {
			VGA_ALPHA_PTR a = vga_alpha_ram;

			for (y=0;y < (80*25);y++)
				*a++ = 0x1E20;

			redraw_all = 0;
			redraw = 1;
		}
		_cli();
		if (irq8_ints != pirq8_ints || redraw) {
			unsigned char reg_b;

			vga_moveto(0,0);
			vga_write_color(0x1E);

			/* we need a copy of Status Register B to know what format
			 * the values are given in */
			reg_b = rtc_io_read(0xB);

			sprintf(tmp,"IRQ 8: %lu total (%lu per, %u alarm, %u up, %lu per/sec)     \n",
				(unsigned long)irq8_ints,
				(unsigned long)periodic_ints,
				(unsigned int)alarm_ints,
				(unsigned int)update_ints,
				(unsigned long)periodic_measured_ints);
			vga_write(tmp);

			pirq8_ints = irq8_ints;
			rtc_io_finished();
		}
		if (update_ints != pupdate_ints || update_rdtime || redraw) {
			unsigned char reg_b,month,dom,dow,hour,minutes,seconds;
			unsigned int year;

			/* we need a copy of Status Register B to know what format
			 * the values are given in */
			reg_b = rtc_io_read(0xB);

			/* read the time. if an Update In Progress is seen, start over */
retry:			if (rtc_wait_for_update_complete()) goto retry;
retry2:			year = rtc_io_read(9);
			if (rtc_wait_for_update_complete()) goto retry2;
			month = rtc_io_read(8);
			if (rtc_wait_for_update_complete()) goto retry2;
			dom = rtc_io_read(7); /* day of month */
			if (rtc_wait_for_update_complete()) goto retry2;
			dow = rtc_io_read(6); /* day of week */
			if (rtc_wait_for_update_complete()) goto retry2;
			hour = rtc_io_read(4);
			if (rtc_wait_for_update_complete()) goto retry2;
			minutes = rtc_io_read(2);
			if (rtc_wait_for_update_complete()) goto retry2;
			seconds = rtc_io_read(0);
			if (rtc_wait_for_update_complete()) goto retry2;
			rtc_io_finished();

			vga_moveto(0,1);
			vga_write_color(0x1E);
			sprintf(tmp,"NOW[%s]: ",reg_b & 0x04 ? "BIN" : "BCD");
			vga_write(tmp);

			if (!(reg_b & 0x04)) {
				year = bcd_to_binary(year);
				month = bcd_to_binary(month);
				dom = bcd_to_binary(dom);
				dow = bcd_to_binary(dow);
				hour = bcd_to_binary(hour&0x7F) | (hour&0x80);
				minutes = bcd_to_binary(minutes);
				seconds = bcd_to_binary(seconds);
			}
			year += 1900;
			/* Y2K compliance: Personal computers using this chipset did not exist
			 * prior to about 1985 or so, so let's assume years prior to 1980 are
			 * in the 2000's */
			if (year < 1980) year += 100;

			if (reg_b & 0x02) {/* 24 hour mode */
				sprintf(tmp,"%02d:%02d:%02d [24hr] ",
					hour,minutes,seconds);
			}
			else {
				/* 12-hour mode, bit 7 of "hours" is set for PM */
				sprintf(tmp,"%02d:%02d:%02d %s ",
					hour&0x7F,minutes,seconds,
					hour&0x80?"PM":"AM");
			}
			vga_write(tmp);
			sprintf(tmp,"%s, %s %d, %d",weekday_to_str(dow),month_to_str(month),dom,year);
			vga_write(tmp);
			vga_write("                ");

			pupdate_ints = update_ints;
			update_rdtime = 0;
		}
		if (redraw) {
			unsigned char reg_b,hour,minutes,seconds;

			/* we need a copy of Status Register B to know what format
			 * the values are given in */
			reg_b = rtc_io_read(0xB);

			/* read the time. if an Update In Progress is seen, start over */
aretry:			if (rtc_wait_for_update_complete()) goto aretry;
aretry2:		hour = rtc_io_read(5);
			if (rtc_wait_for_update_complete()) goto aretry2;
			minutes = rtc_io_read(3);
			if (rtc_wait_for_update_complete()) goto aretry2;
			seconds = rtc_io_read(1);
			if (rtc_wait_for_update_complete()) goto aretry2;
			rtc_io_finished();

			vga_moveto(0,2);
			vga_write_color(0x1E);
			sprintf(tmp,"ALARM[%s]: ",reg_b & 0x04 ? "BIN" : "BCD");
			vga_write(tmp);

			if (!(reg_b & 0x04)) {
				hour = (hour >= 0xC0 ? 0xFF : bcd_to_binary(hour&0x7F) | (hour&0x80));
				minutes = (minutes >= 0xC0 ? 0xFF : bcd_to_binary(minutes));
				seconds = (seconds >= 0xC0 ? 0xFF : bcd_to_binary(seconds));
			}

			if (reg_b & 0x02) {/* 24 hour mode */
				if (hour == 0xFF) strcpy(tmp,"XX:");
				else sprintf(tmp,"%02d:",hour);
				vga_write(tmp);

				if (minutes == 0xFF) strcpy(tmp,"XX:");
				else sprintf(tmp,"%02d:",minutes);
				vga_write(tmp);

				if (seconds == 0xFF) strcpy(tmp,"XX");
				else sprintf(tmp,"%02d",seconds);
				vga_write(tmp);

				vga_write(" [24hr]");
			}
			else {
				/* 12-hour mode, bit 7 of "hours" is set for PM */
				if (hour == 0xFF) strcpy(tmp,"XX:");
				else sprintf(tmp,"%02d:",hour&0x7F);
				vga_write(tmp);

				if (minutes == 0xFF) strcpy(tmp,"XX:");
				else sprintf(tmp,"%02d:",minutes);
				vga_write(tmp);

				if (seconds == 0xFF) strcpy(tmp,"XX");
				else sprintf(tmp,"%02d",seconds);
				vga_write(tmp);

				if (hour == 0xFF) strcpy(tmp," ??\n");
				else sprintf(tmp," %s",hour&0x80?"PM":"AM");
				vga_write(tmp);
			}

			vga_write("                ");
		}
		if (redraw) {
			unsigned int rate;
			unsigned char c,b;
			const char *str;

			c = rtc_io_read(0xA);
			vga_moveto(0,3);
			vga_write_color(0x1E);
			vga_write("REG A: ");

			vga_write("TimeBase=");
			b = (c >> 4) & 7;
			if (b == 0) str = "4.194304MHz";
			else if (b == 1) str = "1.048576MHz";
			else if (b == 2) str = "32.768KHz";
			else if (b == 6 || b == 7) str = "[DivReset]";
			else str = "??";
			vga_write(str);

			b = c & 0xF;
			if (b > 0) {
				rate = 32768UL >> (b - 1);
				if (((c >> 4) & 7) == 2 && rate > 8192) rate >>= 7;
				sprintf(tmp," PeriodicRate=%uHz",rate);
				vga_write(tmp);
			}
			else {
				vga_write(" PeriodicRate=Off");
			}

			vga_write("            ");
			rtc_io_finished();
		}
		if (redraw) {
			unsigned char c;

			c = rtc_io_read(0xB);
			vga_moveto(0,4);
			vga_write_color(0x1E);
			vga_write("REG B: ");

			sprintf(tmp,"DaylightSaving=%u SquareWave=%u Freeze=%u ",
				c & 0x01 ? 1 : 0,
				c & 0x08 ? 1 : 0,
				c & 0x80 ? 1 : 0);
			vga_write(tmp);

			sprintf(tmp,"INTERRUPTS: PI=%u AI=%u UEI=%u\n",
				c & 0x40 ? 1 : 0,
				c & 0x20 ? 1 : 0,
				c & 0x10 ? 1 : 0);
			vga_write(tmp);

			rtc_io_finished();
		}
		_sti();
		if (redraw) {
			vga_moveto(0,20);
			vga_write_color(0x1F);
			vga_write("SPACEBAR=Refresh time     U=toggle UEI    A=toggle AI   P=toggle PI\n");
			vga_write_color(0x1C);
			vga_write("2=12/24 hour mode         ");
			vga_write("B=BCD/BIN mode! ");
			vga_write_color(0x1F);
			vga_write("r/R=change rate\n");
			vga_write_color(0x1C);
			vga_write("t/T=change timebase [!]   ");
			vga_write_color(0x1F);
			vga_write("F=Freeze/set    S=toggle square wave");

			redraw = 0;
		}

		if (kbhit()) {
			unsigned char c;

			i = getch();
			if (i == 0) i = getch() << 8;

			if (i == 27)
				break;
			else if (i == ' ')
				update_rdtime=1;
			else if (i == 's') {
				_cli();
				c = rtc_io_read(0xB) ^ 0x08;
				rtc_io_write(0xB,c);
				rtc_io_finished();
				redraw = 1;
				_sti();
			}
			else if (i == 'f') {
				/* Real hardware experience: Some RTC
				   implementations automatically set the
				   Update-Ended Interrupt enable to 0 if
				   you set bit 7 of register B. Observed
				   on a Pentium 133MHz test machine with
				   Intel 430VX based motherboard. Also
				   observed is that if you leave bit 7
				   enabled, the RTC will NOT let you
				   set the UEI bit to 1. */
				_cli();
				c = rtc_io_read(0xB) ^ 0x80;
				rtc_io_write(0xB,c);
				rtc_io_finished();
				redraw = 1;
				_sti();
			}
			else if (i == 't' || i == 'T') {
				/* WARNING: This changes the time base the
				   RTC uses for keeping time! On actual
				   RTC hardware this can cause the clock to
				   run fast. That's why the UI shows this
				   option in RED.

				   Note that on most PCs the RTC is faithfully
				   emulated but most implementations will
				   simply stop the clock if the time base
				   is set to anything other than 32768Hz. */
				_cli();
				c = rtc_io_read(0xA);
				if (i == 't')
					c = (c & 0x8F) | (((c & 0x70) - 0x10) & 0x70);
				else
					c = (c & 0x8F) | (((c & 0x70) + 0x10) & 0x70);

				rtc_io_write(0xA,c);
				rtc_io_finished();
				redraw = 1;
				_sti();
			}
			else if (i == 'r' || i == 'R') {
				_cli();
				c = rtc_io_read(0xA);
				if (i == 'r')
					c = (c & 0xF0) | (((c & 0xF) - 1) & 0xF);
				else
					c = (c & 0xF0) | (((c & 0xF) + 1) & 0xF);

				rtc_io_write(0xA,c);
				rtc_io_finished();
				redraw = 1;
				_sti();
			}
			else if (i == 'u') {
				/* Real hardware experience: If the Periodic
				   interrupt is enabled, and update-ended
				   interrupt is not, the RTC will not signal
				   IRQ 8 for Update-Ended but will report
				   it regardless when our interrupt handler
				   reads the status register. Observed on a
				   Pentium 133MHz test system: disabling
				   the Update-Ended interrupt here but leaving
				   periodic on still results in our ISR counting
				   update-ended events. So if you observe the
				   same on your computer, don't worry, there's
				   nothing wrong with this code.

				   Also observed: If the "freeze" bit (bit 7)
				   is set, then the RTC will NOT let you set
				   the UEI bit. All writes will force it to
				   zero until "freeze" is zero. */
				_cli();
				c = rtc_io_read(0xB) ^ 0x10; /* toggle Update-Ended */
				rtc_io_write(0xB,c);
				rtc_io_finished();
				redraw = 1;
				_sti();
			}
			else if (i == 'a') {
				_cli();
				c = rtc_io_read(0xB) ^ 0x20; /* toggle Alarm */
				rtc_io_write(0xB,c);
				rtc_io_finished();
				redraw = 1;
				_sti();
			}
			else if (i == 'p') {
				/* Real hardware experience: If we disable
				   IRQ 8 on periodic interrupt, the RTC still
				   sets the periodic bit in the status register.
				   So if you disable the periodic interrupt here,
				   our ISR will still count periodic interrupts.
				   Observed on a Pentium 133MHz test machine. */
				_cli();
				c = rtc_io_read(0xB) ^ 0x40; /* toggle Periodic */
				rtc_io_write(0xB,c);
				rtc_io_finished();
				redraw = 1;
				_sti();
			}
			else if (i == '2') {
				/* Observed on an actual Pentium 133MHz test
				   system: Changing 12/24 hour formats does NOT
				   change the actual byte value, it merely changes
				   how the chipset (and this software) interpret
				   them, usually with hilarious results and some
				   confusion */
				_cli();
				c = rtc_io_read(0xB) ^ 0x02;
				rtc_io_write(0xB,c);
				rtc_io_finished();
				redraw = 1;
				_sti();
			}
			else if (i == 'b') {
				/* Observed on an actual Pentium 133MHz test
				   system: Changing binary -> BCD or BCD -> binary
				   merely causes the chipset (and this program)
				   to re-interpret whatever bytes were there,
				   it does not magically change the values over.
				   That means whatever BCD values you had in the
				   RTC clock will suddenly be incremented on update
				   as if they were binary, with hilarious results. */
				_cli();
				c = rtc_io_read(0xB) ^ 0x04;
				rtc_io_write(0xB,c);
				rtc_io_finished();
				redraw = 1;
				_sti();
			}
		}
	}

	vga_moveto(0,vga_height-2);
	vga_sync_bios_cursor();
	vga_sync_hw_cursor();

	/* restore old IRQ 8 */
	_dos_setvect(0x70,old_irq8);
	return 0;
}

