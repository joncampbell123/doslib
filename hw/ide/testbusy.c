
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/vga/vga.h>
#include <hw/pci/pci.h>
#include <hw/dos/dos.h>
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/vga/vgagui.h>
#include <hw/vga/vgatty.h>
#include <hw/ide/idelib.h>

#include "testutil.h"
#include "testmbox.h"
#include "testcmui.h"
#include "testbusy.h"
#include "test.h"

/* returns: -1 if user said to cancel
 *          0 if not busy
 *          1 if still busy, but user said to proceed */
int do_ide_controller_user_wait_busy_timeout_controller(struct ide_controller *ide,unsigned int timeout) {
	int ret = 0;
	
	if (ide == NULL)
		return -1;

	/* use the alt status register if possible, else the base I/O.
	 * the alt status register is said not to clear pending interrupts */
	idelib_controller_update_status(ide);
	if (idelib_controller_is_busy(ide)) {
		unsigned long show_countdown = (unsigned long)timeout * 10UL; /* ms -> 100us units */

		do {
			idelib_controller_update_status(ide);
			if (!idelib_controller_is_busy(ide)) break;

			/* if the drive&controller is busy then show the dialog and wait for non-busy
			 * or until the user forces us to proceed */
			if (show_countdown > 0UL) {
				if (--show_countdown == 0UL) {
					ret = 1;
					break;
				}
			}

			t8254_wait(t8254_us2ticks(100)); /* wait 100us (0.0001 seconds) */
		} while (1);
	}

	return ret;
}

/* returns: -1 if user said to cancel
 *          0 if not busy
 *          1 if still busy, but user said to proceed */
int do_ide_controller_user_wait_busy_controller(struct ide_controller *ide) {
	struct vga_msg_box vgabox;
	int ret = 0,c = 0;
	
	if (ide == NULL)
		return -1;

	/* use the alt status register if possible, else the base I/O.
	 * the alt status register is said not to clear pending interrupts */
	idelib_controller_update_status(ide);
	if (idelib_controller_is_busy(ide)) {
		unsigned long show_countdown = 500000UL / 100UL; /* 0.5s / 100us units */
		/* if the drive&controller is busy then show the dialog and wait for non-busy
		 * or until the user forces us to proceed */

		do {
			idelib_controller_update_status(ide);
			if (!idelib_controller_is_busy(ide)) break;

			/* if the drive&controller is busy then show the dialog and wait for non-busy
			 * or until the user forces us to proceed */
			if (show_countdown > 0UL) {
				if (--show_countdown == 0UL)
					vga_msg_box_create(&vgabox,"IDE controller busy, waiting...\n\nHit ESC to cancel, spacebar to proceed anyway",0,0);
			}

			if (show_countdown == 0UL && kbhit()) { /* if keyboard input and we're showing prompt */
				c = getch();
				if (c == 0) c = getch() << 8;

				if (c == 27) {
					ret = -1;
					break;
				}
				else if (c == ' ') {
					ret = 1;
					break;
				}
			}

			t8254_wait(t8254_us2ticks(100)); /* wait 100us (0.0001 seconds) */
		} while (1);

		if (show_countdown == 0UL)
			vga_msg_box_destroy(&vgabox);
	}

	return ret;
}

/* returns: -2 if an error happened
 *          -1 if user said to cancel
 *           0 if not busy
 *           1 if still busy, but user said to proceed */
int do_ide_controller_user_wait_drive_drq(struct ide_controller *ide) {
	struct vga_msg_box vgabox;
	int ret = 0,c = 0;

	if (ide == NULL)
		return -1;

	/* NTS: We ignore the Drive Ready Bit, because logically, that bit reflects
	 *      when the drive is ready for another command. Obviously if we're waiting
	 *      for DATA related to a command, then the command is not complete!
	 *      I'm also assuming there are dumbshit IDE controller and hard drive
	 *      implementations dumb enough to return such confusing state. */
	/* use the alt status register if possible, else the base I/O.
	 * the alt status register is said not to clear pending interrupts */
	idelib_controller_update_status(ide);
	if (!idelib_controller_is_drq_ready(ide)) {
		unsigned long show_countdown = 500000UL / 100UL; /* 0.5s / 100us units */

		do {
			idelib_controller_update_status(ide);
			if (idelib_controller_is_drq_ready(ide))
				break;
			else if (idelib_controller_is_error(ide)) {
				ret = -2;
				break;
			}

			/* if the drive&controller is busy then show the dialog and wait for non-busy
			 * or until the user forces us to proceed */
			if (show_countdown > 0UL) {
				if (--show_countdown == 0UL)
					vga_msg_box_create(&vgabox,"Waiting for Data Request from IDE device\n\nHit ESC to cancel, spacebar to proceed anyway",0,0);
			}

			if (show_countdown == 0UL && kbhit()) { /* if keyboard input and we're showing prompt */
				c = getch();
				if (c == 0) c = getch() << 8;

				if (c == 27) {
					ret = -1;
					break;
				}
				else if (c == ' ') {
					ret = 1;
					break;
				}
			}

			t8254_wait(t8254_us2ticks(100)); /* wait 100us (0.0001 seconds) */
		} while (1);

		if (show_countdown == 0UL)
			vga_msg_box_destroy(&vgabox);
	}

	return ret;
}

/* returns: -1 if user said to cancel
 *          0 if not busy
 *          1 if still busy, but user said to proceed */
int do_ide_controller_user_wait_irq(struct ide_controller *ide,uint16_t count) {
	struct vga_msg_box vgabox;
	int ret = 0,c = 0;

	if (ide == NULL)
		return -1;

	if (ide->irq_fired < count) {
		unsigned long show_countdown = 500000UL / 100UL; /* 0.5s / 100us units */

		do {
			if (ide->irq_fired >= count)
				break;

			/* if the drive&controller is busy then show the dialog and wait for non-busy
			 * or until the user forces us to proceed */
			if (show_countdown > 0UL) {
				if (--show_countdown == 0UL)
					vga_msg_box_create(&vgabox,"Waiting for IDE IRQ\n\nHit ESC to cancel, spacebar to proceed anyway",0,0);
			}

			if (show_countdown == 0UL && kbhit()) { /* if keyboard input and we're showing prompt */
				c = getch();
				if (c == 0) c = getch() << 8;

				if (c == 27) {
					ret = -1;
					break;
				}
				else if (c == ' ') {
					ret = 1;
					break;
				}
			}

			t8254_wait(t8254_us2ticks(100)); /* wait 100us (0.0001 seconds) */
		} while (1);

		if (show_countdown == 0UL)
			vga_msg_box_destroy(&vgabox);
	}

	return ret;
}

/* returns: -1 if user said to cancel
 *          0 if not busy
 *          1 if still busy, but user said to proceed */
int do_ide_controller_user_wait_drive_ready(struct ide_controller *ide) {
	struct vga_msg_box vgabox;
	int ret = 0,c;

	if (ide == NULL)
		return -1;

	/* use the alt status register if possible, else the base I/O.
	 * the alt status register is said not to clear pending interrupts */
	idelib_controller_update_status(ide);
	if (!idelib_controller_is_drive_ready(ide)) {
		unsigned long show_countdown = 500000UL / 100UL; /* 0.5s / 100us units */

		/* if the drive&controller is busy then show the dialog and wait for non-busy
		 * or until the user forces us to proceed */

		do {
			idelib_controller_update_status(ide);
			if (idelib_controller_is_drive_ready(ide))
				break;
			else if (idelib_controller_is_error(ide))
				break; /* this is possible too?? or is VirtualBox fucking with us when the CD-ROM drive is on Primary slave? */

			if (show_countdown > 0UL) {
				if (--show_countdown == 0UL)
					vga_msg_box_create(&vgabox,"IDE device not ready, waiting...\n\nHit ESC to cancel, spacebar to proceed anyway",0,0);
			}

			if (show_countdown == 0UL && kbhit()) { /* if keyboard input and we're showing prompt */
				c = getch();
				if (c == 0) c = getch() << 8;

				if (c == 27) {
					ret = -1;
					break;
				}
				else if (c == ' ') {
					ret = 1;
					break;
				}
			}

			t8254_wait(t8254_us2ticks(100)); /* wait 100us (0.0001 seconds) */
		} while (1);

		if (show_countdown == 0UL)
			vga_msg_box_destroy(&vgabox);
	}

	return ret;
}

