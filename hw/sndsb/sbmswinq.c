
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/dos/dosbox.h>
#include <hw/8237/8237.h>		/* 8237 DMA */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/dos/doswin.h>
#include <hw/dos/tgusmega.h>
#include <hw/dos/tgussbos.h>

/* Windows 9x VxD enumeration */
#include <windows/w9xvmm/vxd_enum.h>

/* uncomment this to enable debugging messages */
//#define DBG

#if defined(DBG)
# define DEBUG(x) (x)
#else
# define DEBUG(x)
#endif

void sndsb_detect_windows_dosbox_vm_quirks(struct sndsb_ctx *cx) {
	if (windows_mode == WINDOWS_NT) {
		/* Windows NT would never let a DOS program like us talk directly to hardware,
		 * so if we see a Sound Blaster like device it's very likely an emulation driver.
		 * Working correctly then requires us to deduce what emulation driver we're running under. */
		cx->windows_emulation = 1;

		if (cx->dsp_copyright[0] == 0 && cx->dsp_vmaj == 2 && cx->dsp_vmin == 1)
			/* No copyright string and DSP v2.1: Microsoft Windows XP/Vista/7 native NTVDM.EXE SB emulation */
			cx->windows_xp_ntvdm = 1;
		else
			/* anything else: probably VDMSOUND.EXE, which provides good enough emulation we don't need workarounds */
			cx->vdmsound = 1;
	}
	else if (windows_mode == WINDOWS_ENHANCED) { /* Windows 9x/ME Sound Blaster */
		struct w9x_vmm_DDB_scan vxdscan;
		unsigned char vxdscanned = 0;

		/* Two possibilities from experience:
		 *    a) We can see and talk to the Sound Blaster because the driver
		 *       implements a "pass through" virtualization like mode. Meaning,
		 *       if we start talking to the Sound Blaster the driver catches it
		 *       and treats it as yet another process opening the sound card.
		 *       Meaning that, as long as we have the sound card playing audio,
		 *       other Windows applications cannot open it, and if other Windows
		 *       applications have it open, we cannot initialize the card.
		 *
		 *       That scenario is typical of:
		 *
		 *          - Microsoft Windows 95 with Microsoft stock Sound Blaster 16 drivers
		 *
		 *       That scenario brings up an interesting problem as well: Since
		 *       it literally "passes through" the I/O, we see the card for what
		 *       it is. So we can't check for inconsistencies in I/O like we can
		 *       with most emulations. If knowing about this matters enough, we
		 *       have to know how to poke around inside the Windows kernel and
		 *       autodetect which drivers are resident.
		 *
		 *    b) The Sound Blaster is virtual. In the most common case with
		 *       Windows 9x, it likely means there's a PCI-based sound card
		 *       installed with kernel-level emulation enabled. What we are able
		 *       to do depends on what the driver offers us.
		 *
		 *       That scenario is typical of:
		 *
		 *          Microsoft Windows 98/ME with PCI-based sound hardware. In
		 *          one test scenario of mine, it was a Sound Blaster Live!
		 *          value card and Creative "Sound Blaster 16 emulation" drivers.
		 *
		 *          Microsoft Windows ME, through SBEMUL.SYS, which uses the
		 *          systemwide default sound driver to completely virtualize
		 *          the sound card. On one virtual machine, Windows ME uses the
		 *          AC'97 codec driver to emulate a Sound Blaster Pro.
		 *
		 *       Since emulation can vary greatly, detecting the emulator through
		 *       I/O inconsistencies is unlikely to work, again, if it matters we
		 *       need a way to poke into the Windows kernel and look at which drivers
		 *       are resident.
		 *
		 *    c) The Sound Blaster is actual hardware, and Windows is not blocking
		 *       or virtualizing any I/O ports.
		 *
		 *       I know you're probably saying to yourself: Ick! But under Windows 95
		 *       such scenarios are possible: if there is a SB16 compatible sound
		 *       card out there and no drivers are installed to talk to it, Windows
		 *       95 itself will not talk to the card, but will allow DOS programs
		 *       to do so. Amazingly, it will still virtualize the DMA controller
		 *       in a manner that allows everything to work!
		 *
		 *       Whatever you do in this scenario: Don't let multiple DOS boxes talk
		 *       to the same Sound Blaster card!!!
		 *
		 * So unlike the Windows NT case, we can't assume emulators or capabilities
		 * because it varies greatly depending on the host configuration. But we
		 * can try our best and at least try to avoid things that might trip up
		 * Windows 9x. */
		cx->windows_emulation = 1; /* NTS: The "pass thru" scenario counts as emulation */

		if (!sndsb_probe_options.disable_windows_vxd_checks && w9x_vmm_first_vxd(&vxdscan)) {
			vxdscanned = 1;
			do {
				/* If SBEMUL.SYS is present, then it's definitely Windows 98/ME SBEMUL.SYS */
				if (!memcmp(vxdscan.ddb.name,"SBEMUL  ",8)) {
					cx->windows_9x_me_sbemul_sys = 1;
				}
				/* If a Sound Blaster 16 is present, usually Windows 9x/ME will install the
				 * stock Creative drivers shipped on the CD-ROM */
				else if (!memcmp(vxdscan.ddb.name,"VSB16   ",8)) {
					cx->windows_creative_sb16_drivers = 1;
					cx->windows_creative_sb16_drivers_ver = ((uint16_t)vxdscan.ddb.Mver << 8U) | ((uint16_t)vxdscan.ddb.minorver);
				}
			} while (w9x_vmm_next_vxd(&vxdscan));
			w9x_vmm_last_vxd(&vxdscan);
		}
	}
}

