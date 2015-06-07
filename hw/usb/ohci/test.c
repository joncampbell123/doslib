/* Experimental code to control an OHCI compliant USB Host Controller.
   (C) 2012 Jonathan Campbell


   Test results:
     - VirtualBox: works fine, however the USB emulation is cheap w.r.t. the
	           Frame Counter: it advances when we read it rather than by
		   the passage of time.

     - Pentium 4 system with VIA graphics: Works fine

     - Pentium III 600Mhz IBM NetVista: Works fine claiming ownership from BIOS,
       but the BIOS seems to have difficulty with taking control back when we want
       it to do so.
 */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/flatreal/flatreal.h>
#include <hw/usb/ohci/ohci.h>
#include <hw/llmem/llmem.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/pcie/pcie.h>
#include <hw/pci/pci.h>
#include <hw/cpu/cpu.h>
#include <hw/vga/vga.h>
#include <hw/vga/vgagui.h>
#include <hw/vga/vgatty.h>
#include <hw/dos/doswin.h>

static char str_tmp[256];

static void help() {
	printf("test [options]\n");
	printf("USB OHCI Host Controller test program\n");
	printf("*NOTICE: Please check which standard your USB controller follows.\n");
	printf("         Currently there is UHCI, OHCI, EHCI, and XHCI. This program implements OHCI.\n");
	printf("   /dev <n>          specify which USB controller to use. defaults to first find.\n");
	printf("                       pci:<bus>:<dev>:<func> to specify PCI device\n");
	printf("                       pcie:<bus>:<dev>:<func> to specify PCIe device\n");
	printf("   /list             list USB controllers detected\n");
	printf("   /?                this help\n");
}

union usb_ohci_ci_hccontrol {
	struct {
		uint32_t	ControlBulkServiceRatio:2;	/* bit 0-1 */
		uint32_t	PeriodicListEnable:1;		/* bit 2 */
		uint32_t	IsochronousEnable:1;		/* bit 3 */
		uint32_t	ControlListEnable:1;		/* bit 4 */
		uint32_t	BulkListEnable:1;		/* bit 5 */
		uint32_t	HostControllerFunctionalState:2;/* bit 6-7 */
		uint32_t	InterruptRouting:1;		/* bit 8 */
		uint32_t	RemoteWakeupConnected:1;	/* bit 9 */
		uint32_t	RemoteWakeupEnable:1;		/* bit 10 */
		uint32_t	_reserved_:21;			/* bit 11-31 */
	} f;
	uint32_t	raw;
};

union usb_ohci_ci_rhd {
	struct {
		uint32_t	NumberDownstreamPorts:8;	/* bit 0-7 */
		uint32_t	PowerSwitchingMode:1;		/* bit 8 */
		uint32_t	NoPowerSwitching:1;		/* bit 9 */
		uint32_t	DeviceType:1;			/* bit 10 */
		uint32_t	OverCurrentProtectionMode:1;	/* bit 11 */
		uint32_t	NoOverCurrentProtection:1;	/* bit 12 */
		uint32_t	_reserved:11;			/* bit 13-23 */
		uint32_t	PowerOnToGoodTime:8;		/* bit 24-31 */

		uint32_t	DeviceRemovable:16;		/* bit 0-15 bitmask for each port (0=removable 1=not removeable) */
		uint32_t	PortPowerControlMask:16;	/* bit 16-31 bitmask for each port */

		uint32_t	LocalPowerStatus:1;		/* bit 0 */
		uint32_t	OverCurrentIndicator:1;		/* bit 1 */
		uint32_t	_reserved2:13;			/* bit 2-14 */
		uint32_t	DeviceRemoteWakeupEnable:1;	/* bit 15 */
		uint32_t	LocalPowerStatusChange:1;	/* bit 16 */
		uint32_t	OverCurrentIndicatorChange:1;	/* bit 17 */
		uint32_t	_reserved3:13;			/* bit 18-30 */
		uint32_t	ClearRemoteWakeupEnable:1;	/* bit 31 */
	} f;
	struct {
		uint32_t	a,b,c;
	} raw;
};

struct usb_ohci_ci_ctx {
	/* hardware resources */
	uint64_t	mmio_base;
	int8_t		IRQ;
	/* anything we learn about the controller */
	uint8_t		bcd_revision;
	uint8_t		legacy_support:1;		/* OHCI controller contains "Legacy support" I/O */
	uint8_t		legacy_emulation_active:1;	/* (last checked) OHCI controller is emulating ports 60h and 64h */
	uint8_t		legacy_irq_enable:1;		/* OHCI controller generates IRQ1/IRQ12 interrupts */
	uint8_t		legacy_external_irq_enable:1;	/* OHCI controller generates emulation interrupt when keyboard controller signals IRQ */
	uint8_t		legacy_a20_state:1;		/* The state of the A20 gate */
	uint8_t		bios_is_using_it:1;		/* whether or not (last checked) the BIOS is using the USB controller */
	uint8_t		bios_was_using_it:1;		/* whether or not on first init the BIOS was using it (and therefore we must return control when done) */
	uint8_t		bios_was_using_legacy_support:1;
	uint8_t		bios_was_using_legacy_external_irq:1;
	uint8_t		_reserved_1:1;

	union usb_ohci_ci_hccontrol control;

	uint16_t		FrameInterval;
	uint16_t		FullSpeedLargestDataPacket;
	uint16_t		FrameRemaining;
	uint32_t		FrameNumber;
	uint16_t		FrameNumberHi;
	uint16_t		PeriodicStart;
	volatile uint32_t	irq_events;
	volatile uint32_t	port_events[16];	/* NTS: port_events[0] is HcRhStatus */

	union usb_ohci_ci_rhd RootHubDescriptor;
};

struct usb_ohci_ci_ctx *usb_ohci_ci_create() {
	struct usb_ohci_ci_ctx *r = (struct usb_ohci_ci_ctx *)malloc(sizeof(struct usb_ohci_ci_ctx));
	if (r != NULL) {
		memset(r,0,sizeof(*r));
		r->IRQ = -1;
	}
	return r;
}

struct usb_ohci_ci_ctx *usb_ohci_ci_destroy(struct usb_ohci_ci_ctx *c) {
	if (c != NULL) {
		c->mmio_base = 0;
		c->IRQ = -1;
		free(c);
	}
	return NULL;
}

uint32_t usb_ohci_ci_read_reg(struct usb_ohci_ci_ctx *c,uint32_t reg) {
	if (reg >= 0xFFD) return 0;

	if (c != NULL) {
#if TARGET_MSDOS == 32
		if (dos_ltp_info.paging) return 0;
		return *((volatile uint32_t*)((uint32_t)c->mmio_base + reg));
#else
		if (!flatrealmode_allowed()) return 0;
		if (!flatrealmode_ok()) {
			if (!flatrealmode_setup(FLATREALMODE_4GB))
				return 0;
		}

		return flatrealmode_readd((uint32_t)c->mmio_base + reg);
#endif
	}

	return 0;
}

void usb_ohci_ci_write_reg(struct usb_ohci_ci_ctx *c,uint32_t reg,uint32_t val) {
	if (reg >= 0xFFD) return;

	if (c != NULL) {
#if TARGET_MSDOS == 32
		if (dos_ltp_info.paging) return;
		*((volatile uint32_t*)((uint32_t)c->mmio_base + reg)) = val;
#else
		if (!flatrealmode_allowed()) return;
		if (!flatrealmode_ok()) {
			if (!flatrealmode_setup(FLATREALMODE_4GB))
				return;
		}

		flatrealmode_writed((uint32_t)c->mmio_base + reg,val);
#endif
	}
}

/* NTS: USB standard: port 0 is the root hub, ports 1-15 are the actual "ports",
        and the OS is supposed to do this funky virtualization thing. Right, ok then... */
uint32_t usb_ohci_ci_read_port_status(struct usb_ohci_ci_ctx *ohci,uint8_t port) {
	if (ohci == NULL || port > ohci->RootHubDescriptor.f.NumberDownstreamPorts)
		return ~0UL;

	return usb_ohci_ci_read_reg(ohci,0x50+(port*4));
}

void usb_ohci_ci_write_port_status(struct usb_ohci_ci_ctx *ohci,uint8_t port,uint32_t d) {
	if (ohci == NULL || port > ohci->RootHubDescriptor.f.NumberDownstreamPorts)
		return;

	usb_ohci_ci_write_reg(ohci,0x50+(port*4),d);
}

int usb_ohci_ci_software_online(struct usb_ohci_ci_ctx *c) {
	unsigned int port;
	uint32_t tmp;

	if (c == NULL) return -1;

	/* do NOT reset the controller if the BIOS is using it!!! */
	if (c->bios_is_using_it) {
		fprintf(stderr,"OHCI BUG: Don't resume the controller while the BIOS is using it, you moron.\n");
		return -1;
	}

	/* don't write resume if the controller is already in resume or operational mode */
	c->control.raw = usb_ohci_ci_read_reg(c,0x04/*HcControl*/);
	if (c->control.f.HostControllerFunctionalState == 2/*USBOperational*/) return 0;

	/* write it */
	c->control.f.HostControllerFunctionalState = 2/*USBOperational*/;
	usb_ohci_ci_write_reg(c,0x04/*HcControl*/,c->control.raw);

	/* wait */
	t8254_wait(t8254_us2ticks(3000/*3ms*/));

	/* if it kept the state, then we're good */
	c->control.raw = usb_ohci_ci_read_reg(c,0x04/*HcControl*/);

	/* root hub */
	tmp = usb_ohci_ci_read_port_status(c,0/*root hub*/) & 0x00020000UL; /* mask off all but "status change" bits */
	usb_ohci_ci_write_port_status(c,0,tmp); /* write back to clear status */
	c->port_events[0] |= tmp; /* note them in our context so the main program can react */

	/* and ports */
	for (port=1;port < c->RootHubDescriptor.f.NumberDownstreamPorts;port++) {
		tmp = usb_ohci_ci_read_port_status(c,port) & 0x001F0000UL; /* mask off all but "status change" bits */
		usb_ohci_ci_write_port_status(c,port,tmp); /* write back to clear status in HC */
		c->port_events[port] |= tmp; /* note them in our context so the main program can react */
	}

	return (c->control.f.HostControllerFunctionalState == 2/*USBOperational*/)?0:-1;
}

/* enter resume state, and wait 10ms */
int usb_ohci_ci_software_resume(struct usb_ohci_ci_ctx *c) {
	if (c == NULL) return -1;

	/* do NOT reset the controller if the BIOS is using it!!! */
	if (c->bios_is_using_it) {
		fprintf(stderr,"OHCI BUG: Don't resume the controller while the BIOS is using it, you moron.\n");
		return -1;
	}

	/* don't write resume if the controller is already in resume or operational mode */
	c->control.raw = usb_ohci_ci_read_reg(c,0x04/*HcControl*/);
	if (c->control.f.HostControllerFunctionalState == 1/*USBResume*/) return 0;
	else if (c->control.f.HostControllerFunctionalState == 2/*USBOperational*/) return 0;

	/* write it */
	c->control.f.HostControllerFunctionalState = 1/*USBResume*/;
	usb_ohci_ci_write_reg(c,0x04/*HcControl*/,c->control.raw);

	/* wait */
	t8254_wait(t8254_us2ticks(10000/*10ms*/));

	/* if it kept the state, then we're good */
	c->control.raw = usb_ohci_ci_read_reg(c,0x04/*HcControl*/);

	return (c->control.f.HostControllerFunctionalState == 1/*USBResume*/)?0:-1;
}

/* explicitly put the OHCI controller into USBReset state */
int usb_ohci_ci_set_usb_reset_state(struct usb_ohci_ci_ctx *c) {
	int ret = 0;

	if (c == NULL) return -1;

	/* do NOT reset the controller if the BIOS is using it!!! */
	if (c->bios_is_using_it) {
		fprintf(stderr,"OHCI BUG: Don't reset the controller while the BIOS is using it, you moron.\n");
		return -1;
	}

	/* write it */
	c->control.f.HostControllerFunctionalState = 0/*USBReset*/;
	usb_ohci_ci_write_reg(c,0x04/*HcControl*/,c->control.raw);

	if (ret == 0) {
		c->control.raw = usb_ohci_ci_read_reg(c,0x04/*HcControl*/);
		if (c->control.f.HostControllerFunctionalState != 0/*USBReset*/) {
			fprintf(stderr,"OHCI: controller did not accept USBReset state\n");
			ret = -1;
		}
	}	
	
	return ret;
}

int usb_ohci_ci_update_root_hub_status(struct usb_ohci_ci_ctx *c) {
	if (c == NULL) return -1;

	/* Load port info */
	c->RootHubDescriptor.raw.a = usb_ohci_ci_read_reg(c,0x48/*HcRhDescriptorA*/);
	c->RootHubDescriptor.raw.b = usb_ohci_ci_read_reg(c,0x4C/*HcRhDescriptorB*/);
	c->RootHubDescriptor.raw.c = usb_ohci_ci_read_reg(c,0x50/*HcRhStatus*/);

	/* sanitize results */
	if (c->RootHubDescriptor.f.NumberDownstreamPorts > 15)
		c->RootHubDescriptor.f.NumberDownstreamPorts = 15;

	return 0;
}

int usb_ohci_ci_update_frame_status(struct usb_ohci_ci_ctx *c) {
	unsigned int cpu_flags;
	uint32_t tmp;

	if (c == NULL) return -1;

	cpu_flags = get_cpu_flags(); _cli();

	tmp = usb_ohci_ci_read_reg(c,0x38/*HcFmRemaining*/);
	c->FrameRemaining = tmp & 0x3FFF;

	tmp = usb_ohci_ci_read_reg(c,0x3C/*HcFmNumber*/);
	c->FrameNumber = (tmp & 0xFFFF) | (((uint32_t)c->FrameNumberHi) << 16UL);

	set_cpu_flags(cpu_flags);
	return 0;
}

/* cause OHCI reset. note that by USB standard, the USB controller will
   leave reset with function state == USBSuspend and it's the caller's
   job to do OHCI compliant resume -> operational state changes */
int usb_ohci_ci_software_reset(struct usb_ohci_ci_ctx *c) {
	unsigned int patience;
	uint32_t tmp;
	int ret = 0;
	size_t i;

	if (c == NULL) return -1;

	/* do NOT reset the controller if the BIOS is using it!!! */
	if (c->bios_is_using_it) {
		fprintf(stderr,"OHCI BUG: Don't reset the controller while the BIOS is using it, you moron.\n");
		return -1;
	}

	/* hit the reset bit */
	usb_ohci_ci_write_reg(c,0x08/*HcCommandStatus*/,1UL);

	/* wait */
	t8254_wait(t8254_us2ticks(1000/*1ms*/));

	/* wait for the reset bit to clear */
	patience = 500;
	do {
		tmp = usb_ohci_ci_read_reg(c,0x08/*HcCommandStatus*/);
		if (!(tmp&1UL)) {
			/* the bit cleared, it's finished resetting */
			break;
		}

		if (--patience == 0) {
			fprintf(stderr,"OHCI: Ran out of patience waiting for reset to complete\n");
			ret = -1;
			break;
		}

		t8254_wait(t8254_us2ticks(10000/*10ms*/));
	} while (1);

	if (ret == 0) {
		c->control.raw = usb_ohci_ci_read_reg(c,0x04/*HcControl*/);
		if (!(c->control.f.HostControllerFunctionalState == 3/*USBSuspend*/ || c->control.f.HostControllerFunctionalState == 3/*USBSuspend*/)) {
			fprintf(stderr,"OHCI: Despite software reset, controller is not in suspend or reset state\n");
			ret = -1;
		}
	}

	/* complete the reset by disabling interrupts and functions */
	c->control.f.BulkListEnable=0;
	c->control.f.IsochronousEnable=0;
	c->control.f.ControlListEnable=0;
	c->control.f.PeriodicListEnable=0;
	c->control.f.RemoteWakeupEnable=0;
	c->control.f.RemoteWakeupConnected=0; /* FIXME: OHCI code in Linux kernel does this---do we have to do it? */
	for (i=0;i < 16;i++) c->port_events[i] = 0;
	c->irq_events = 0;
	usb_ohci_ci_write_reg(c,0x04/*HcControl*/,c->control.raw);
	usb_ohci_ci_write_reg(c,0x14/*HcInterruptDisable*/,0xC000007FUL); /* disable all event interrupts, including Ownership Change and Master */

	tmp = usb_ohci_ci_read_reg(c,0x34/*HcFmInterval*/);
	c->FrameInterval = tmp & 0x3FFF;
	c->FullSpeedLargestDataPacket = (tmp >> 16) & 0x7FFF;

	tmp = usb_ohci_ci_read_reg(c,0x40/*HcPeriodicStart*/);
	c->PeriodicStart = tmp & 0x3FFF;

	/* use this time to set up power management */
	tmp = usb_ohci_ci_read_reg(c,0x48/*HcRhDescriptorA*/);
	tmp &= ~0x1F00UL;
	tmp |=  0x0900UL; /* OCPM=1 PSM=1 NPS=0 */
	usb_ohci_ci_write_reg(c,0x48/*HcRhDescriptorA*/,tmp);

	usb_ohci_ci_update_frame_status(c);
	usb_ohci_ci_update_root_hub_status(c);

	return ret;
}

int usb_ohci_ci_update_ownership_status(struct usb_ohci_ci_ctx *c) {
	uint32_t tmp;

	if (c == NULL) return -1;

	tmp = usb_ohci_ci_read_reg(c,0x00);
	c->legacy_support = (tmp>>8)&1;

	if (c->legacy_support) tmp = usb_ohci_ci_read_reg(c,0x100);
	else tmp = 0;

	c->legacy_emulation_active = tmp&1;
	c->legacy_irq_enable = (tmp>>3)&1;
	c->legacy_external_irq_enable = (tmp>>4)&1;
	c->legacy_a20_state = (tmp>>8)&1;

	c->control.raw = usb_ohci_ci_read_reg(c,0x04/*HcControl*/);
	c->bios_is_using_it = c->control.f.InterruptRouting;
	return 0;
}

/* this is called at program startup or first use of the controller */
int usb_ohci_ci_learn(struct usb_ohci_ci_ctx *c) {
	uint32_t tmp;

	if (c == NULL) return -1;

	tmp = usb_ohci_ci_read_reg(c,0x00);
	c->bcd_revision = tmp&0xFF;
	if (c->bcd_revision == 0) return -1;
	usb_ohci_ci_update_ownership_status(c);
	/* if the SMM trap is active or the controller was found active, then assume the BIOS was using it */
	c->bios_was_using_it = c->bios_is_using_it ||
		(c->control.f.HostControllerFunctionalState != 0/*USBReset*/ &&
		c->control.f.HostControllerFunctionalState != 3/*USBSuspend*/);
	c->bios_was_using_legacy_support = c->legacy_emulation_active && c->legacy_irq_enable;
	c->bios_was_using_legacy_external_irq = c->legacy_external_irq_enable;

	tmp = usb_ohci_ci_read_reg(c,0x34/*HcFmInterval*/);
	c->FrameInterval = tmp & 0x3FFF;
	c->FullSpeedLargestDataPacket = (tmp >> 16) & 0x7FFF;

	tmp = usb_ohci_ci_read_reg(c,0x40/*HcPeriodicStart*/);
	c->PeriodicStart = tmp & 0x3FFF;

	usb_ohci_ci_update_frame_status(c);
	usb_ohci_ci_update_root_hub_status(c);
	return 0;
}

int usb_ohci_ci_pci_device_is_ohci(struct usb_ohci_ci_ctx *ohci,uint8_t bus,uint8_t dev,uint8_t func) {
	/* make sure something is there before announcing it */
	uint16_t vendor,device,subsystem,subvendor_id;
	uint32_t class_code,t32a,t32b,t32c;
	uint8_t revision_id;

	ohci->IRQ = -1;
	ohci->mmio_base = 0;
	vendor = pci_read_cfgw(bus,dev,func,0x00); if (vendor == 0xFFFF) return 0;
	device = pci_read_cfgw(bus,dev,func,0x02); if (device == 0xFFFF) return 0;
	subvendor_id = pci_read_cfgw(bus,dev,func,0x2C);
	subsystem = pci_read_cfgw(bus,dev,func,0x2E);

	class_code = pci_read_cfgl(bus,dev,func,0x08);
	revision_id = class_code & 0xFF;
	class_code >>= 8UL;

	if (class_code == 0x0C0310UL) { /* USB OHCI controller */
		/* OK before we claim it, try to extract IRQ and MMIO base */
		{
			uint8_t l = pci_read_cfgb(bus,dev,func,0x3C);
			uint8_t p = pci_read_cfgb(bus,dev,func,0x3D);
			if (p != 0) ohci->IRQ = l;
		}

		{
			uint8_t bar;

			_cli();
			for (bar=0;bar < 6;bar++) {
				uint32_t lower=0,higher=0;
				uint8_t reg = 0x10+(bar*4);

				t32a = pci_read_cfgl(bus,dev,func,reg);
				if (t32a == 0xFFFFFFFFUL) continue;

				/* ignore BAR if I/O port */
				if (t32a & 1) continue;

				/* read/write BAR to test size, then readback location */
				lower = t32a & 0xFFFFFFF0UL;
				pci_write_cfgl(bus,dev,func,reg,0);
				t32b = pci_read_cfgl(bus,dev,func,reg);
				pci_write_cfgl(bus,dev,func,reg,~0UL);
				t32c = pci_read_cfgl(bus,dev,func,reg);
				pci_write_cfgl(bus,dev,func,reg,t32a); /* restore prior contents */
				if (t32a == t32b && t32b == t32c) {
					/* hm, can't change it? */
					continue;
				}
				else {
					uint32_t size = ~(t32c & ~(15UL));
					if ((size+1UL) == 0UL) continue;
					higher = lower + size;
				}

				if (higher == lower) continue;
				if (higher < (lower+0xFFFUL)) continue; /* must be at least 4KB large */

				ohci->mmio_base = (uint64_t)lower;
				break;
			}
			_sti();
		}

		return (ohci->mmio_base != 0ULL);
	}

	return 0;
}

int usb_ohci_ci_ownership_change(struct usb_ohci_ci_ctx *c,unsigned int bios_owner) {
	unsigned int patience;
	unsigned int cpu_flags;
	int ret = 0;

	if (c == NULL) return -1;
	if (!c->legacy_support) return 0;

	c->control.raw = usb_ohci_ci_read_reg(c,0x04/*HcControl*/);
	c->bios_is_using_it = c->control.f.InterruptRouting;
	if ((c->bios_is_using_it?1:0) == (bios_owner?1:0)) return 0; /* break loop when ownership becomes what we want */

	cpu_flags = get_cpu_flags(); _cli();

	if (bios_owner && !c->bios_is_using_it) {
		/* disable all other interrupts */
		usb_ohci_ci_write_reg(c,0x14/*HcInterruptDisable*/,0x8000007FUL); /* disable all event interrupts, except Ownership Change */
	}

	/* enable the Ownership Change interrupt, make sure BIOS responds to it */
	usb_ohci_ci_write_reg(c,0x10/*HcInterruptEnable*/,(1UL << 30UL)/*Ownership Change Interrupt*/ | (1UL << 31UL)/*Master Interrupt Enable*/);

	/* forcibly clear interrupt status to ensure it triggers */
	usb_ohci_ci_write_reg(c,0x0C/*HcInterruptStatus*/,0x7FFFFFFFUL);

	/* write InterruptStatus to change ownership */
	usb_ohci_ci_write_reg(c,0x08/*HcCommandStatus*/,1UL << 3UL);

	set_cpu_flags(cpu_flags);

	/* wait for status to clear (NTS: on one test system, the BIOS seems to take 3 seconds to regain control) */
	patience = 150; /* 100ms x 150 = 15 seconds */
	while (1) {
		/* the controller will reset the bit when ownership has changed hands */
		c->control.raw = usb_ohci_ci_read_reg(c,0x04/*HcControl*/);
		c->bios_is_using_it = c->control.f.InterruptRouting;
		if ((c->bios_is_using_it?1:0) == (bios_owner?1:0)) break; /* break loop when ownership becomes what we want */

		if (--patience == 0) {
			cpu_flags = get_cpu_flags(); _cli();

			/* the BIOS failed to get our change request */
			fprintf(stderr,"OHCI warning: Ownership Change not acknowledged\n");

			set_cpu_flags(cpu_flags);
			ret = -1;
			break;
		}

		t8254_wait(t8254_us2ticks(100000/*100ms*/));
	}

	if (!bios_owner && !c->control.f.InterruptRouting) {
		fprintf(stderr,"OHCI debug: Disabling all interrupts, I own the controller now\n");
		cpu_flags = get_cpu_flags(); _cli();
		usb_ohci_ci_write_reg(c,0x14/*HcInterruptDisable*/,0xC000007FUL); /* disable all event interrupts, including Ownership Change */
		set_cpu_flags(cpu_flags);
	}

	usb_ohci_ci_update_ownership_status(c);
	return ret;
}

int usb_ohci_ci_pcie_device_is_ohci(struct usb_ohci_ci_ctx *ohci,uint8_t bus,uint8_t dev,uint8_t func) {
	/* make sure something is there before announcing it */
	uint16_t vendor,device,subsystem,subvendor_id;
	uint32_t class_code,t32a,t32b,t32c;
	uint8_t revision_id;

	ohci->IRQ = -1;
	ohci->mmio_base = 0;
	vendor = pcie_read_cfgw(bus,dev,func,0x00); if (vendor == 0xFFFF) return 0;
	device = pcie_read_cfgw(bus,dev,func,0x02); if (device == 0xFFFF) return 0;
	subvendor_id = pcie_read_cfgw(bus,dev,func,0x2C);
	subsystem = pcie_read_cfgw(bus,dev,func,0x2E);

	class_code = pcie_read_cfgl(bus,dev,func,0x08);
	revision_id = class_code & 0xFF;
	class_code >>= 8UL;

	if (class_code == 0x0C0310UL) { /* USB OHCI controller */
		/* OK before we claim it, try to extract IRQ and MMIO base */
		{
			uint8_t l = pcie_read_cfgb(bus,dev,func,0x3C);
			uint8_t p = pcie_read_cfgb(bus,dev,func,0x3D);
			if (p != 0) ohci->IRQ = l;
		}

		{
			uint8_t bar;

			_cli();
			for (bar=0;bar < 6;bar++) {
				uint32_t lower=0,higher=0;
				uint8_t reg = 0x10+(bar*4);

				t32a = pcie_read_cfgl(bus,dev,func,reg);
				if (t32a == 0xFFFFFFFFUL) continue;

				/* ignore BAR if I/O port */
				if (t32a & 1) continue;

				/* read/write BAR to test size, then readback location */
				lower = t32a & 0xFFFFFFF0UL;
				pcie_write_cfgl(bus,dev,func,reg,0);
				t32b = pcie_read_cfgl(bus,dev,func,reg);
				pcie_write_cfgl(bus,dev,func,reg,~0UL);
				t32c = pcie_read_cfgl(bus,dev,func,reg);
				pcie_write_cfgl(bus,dev,func,reg,t32a); /* restore prior contents */
				if (t32a == t32b && t32b == t32c) {
					/* hm, can't change it? */
					continue;
				}
				else {
					uint32_t size = ~(t32c & ~(15UL));
					if ((size+1UL) == 0UL) continue;
					higher = lower + size;
				}

				if (higher == lower) continue;
				if (higher < (lower+0xFFFUL)) continue; /* must be at least 4KB large */

				ohci->mmio_base = (uint64_t)lower;
				break;
			}
			_sti();
		}

		return (ohci->mmio_base != 0ULL);
	}

	return 0;
}

static const char *hexes = "0123456789ABCDEF";
static struct usb_ohci_ci_ctx *ohci = NULL;

static void (interrupt *usb_old_irq)() = NULL;
static void interrupt usb_irq() {
	uint32_t pending,tmp;

	/* show IRQ activity */
	(*vga_alpha_ram)++;

	/* read back what the USB controller says happened */
	pending = usb_ohci_ci_read_reg(ohci,0x0C/*HcInterruptStatus*/);
	vga_alpha_ram[1+0] = hexes[(pending>>28UL)&0xFUL] | 0x1E00;
	vga_alpha_ram[1+1] = hexes[(pending>>24UL)&0xFUL] | 0x1E00;
	vga_alpha_ram[1+2] = hexes[(pending>>20UL)&0xFUL] | 0x1E00;
	vga_alpha_ram[1+3] = hexes[(pending>>16UL)&0xFUL] | 0x1E00;
	vga_alpha_ram[1+4] = hexes[(pending>>12UL)&0xFUL] | 0x1E00;
	vga_alpha_ram[1+5] = hexes[(pending>> 8UL)&0xFUL] | 0x1E00;
	vga_alpha_ram[1+6] = hexes[(pending>> 4UL)&0xFUL] | 0x1E00;
	vga_alpha_ram[1+7] = hexes[(pending>> 0UL)&0xFUL] | 0x1E00;

	/* Frame Number Overflow */
	if (pending & 32) {
		tmp = usb_ohci_ci_read_reg(ohci,0x3C/*HcFmNumber*/);
		if (!(tmp & 0x8000)) ohci->FrameNumberHi++; /* if it carried 0xFFFF -> 0x0000 then increment high 16 bits */
	}
	/* Root Hub status change.
	   NTS: We MUST handle it here to clear the interrupt. This is where VirtualBox is so goddam misleading:
	        actual OHCI controllers will nag and pester and hammer the CPU with IRQ signals until the condition
		is cleared, and if we never clear it, the IRQ shitstorm is enough to slow the system to a crawl!
		So to be an effective USB driver, you HAVE to take care of it! */
	if (pending & 64) {
		unsigned int port;

		/* root hub */
		tmp = usb_ohci_ci_read_port_status(ohci,0/*root hub*/) & 0x00020000UL; /* mask off all but "status change" bits */
		usb_ohci_ci_write_port_status(ohci,0,tmp); /* write back to clear status */
		ohci->port_events[0] |= tmp; /* note them in our context so the main program can react */

		/* and ports */
		for (port=1;port <= ohci->RootHubDescriptor.f.NumberDownstreamPorts;port++) {
			tmp = usb_ohci_ci_read_port_status(ohci,port) & 0x001F0000UL; /* mask off all but "status change" bits */
			usb_ohci_ci_write_port_status(ohci,port,tmp); /* write back to clear status in HC */
			ohci->port_events[port] |= tmp; /* note them in our context so the main program can react */
		}
	}

	/* USB ack */
	ohci->irq_events |= pending & (~(4UL|32UL));
	if (pending != 0UL) usb_ohci_ci_write_reg(ohci,0x0C/*HcInterruptStatus*/,pending);

	/* PIC ack */
	if (ohci->IRQ >= 8) p8259_OCW2(8,P8259_OCW2_NON_SPECIFIC_EOI);
	p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
}

static const struct vga_menu_item menu_separator =
	{(char*)1,		's',	0,	0};


static const struct vga_menu_item main_menu_file_quit =
	{"Quit",		'q',	0,	0};

static const struct vga_menu_item* main_menu_file[] = {
	&main_menu_file_quit,
	NULL
};


static const struct vga_menu_item main_menu_port_acknowledge =
	{"Acknowledge",					'a',	0,	0};

static const struct vga_menu_item main_menu_port_psm =
	{"Toggle PSM (Power Switching Mode) [root]",	'p',	0,	0};

static const struct vga_menu_item main_menu_port_nps =
	{"Toggle NPS (No Power Switching) [root]",	'n',	0,	0};

static const struct vga_menu_item main_menu_port_ocpm =
	{"Toggle OCPM (OverCurrentProtMode) [root]",	'o',	0,	0};

static const struct vga_menu_item main_menu_port_nocp =
	{"Toggle NOCP [root]",				0,	0,	0};

static const struct vga_menu_item main_menu_port_clear_global_power =
	{"Global Power Off [root]",			0,	0,	0};

static const struct vga_menu_item main_menu_port_set_global_power =
	{"Global Power On [root]",			0,	0,	0};

static const struct vga_menu_item main_menu_port_toggle_removeable =
	{"Toggle Device Removeable [port]",		'r',	0,	0};

static const struct vga_menu_item main_menu_port_toggle_port_power_mask =
	{"Toggle Port Power Mask [port]",		'm',	0,	0};

static const struct vga_menu_item main_menu_port_toggle_port_enable =
	{"Toggle Port Enable [port]",			'e',	0,	0};

static const struct vga_menu_item main_menu_port_toggle_port_suspend =
	{"Toggle Port Suspend [port]",			's',	0,	0};

static const struct vga_menu_item main_menu_port_enable_port_power =
	{"Enable Port Power [port]",			'p',	0,	0};

static const struct vga_menu_item main_menu_port_disable_port_power =
	{"Disable Port Power [port]",			'd',	0,	0};

static const struct vga_menu_item main_menu_port_do_port_reset =
	{"Reset Port [port]",				't',	0,	0};

static const struct vga_menu_item* main_menu_port[] = {
	&main_menu_port_acknowledge,
	&main_menu_port_psm,
	&main_menu_port_nps,
	&main_menu_port_ocpm,
	&main_menu_port_nocp,
	&main_menu_port_clear_global_power,
	&main_menu_port_set_global_power,
	&menu_separator,
	&main_menu_port_toggle_removeable,
	&main_menu_port_toggle_port_power_mask,
	&main_menu_port_toggle_port_enable,
	&main_menu_port_toggle_port_suspend,
	&main_menu_port_disable_port_power,
	&main_menu_port_enable_port_power,
	&main_menu_port_do_port_reset,
	NULL
};


static const struct vga_menu_item main_menu_help_about =
	{"About",		'r',	0,	0};

static const struct vga_menu_item* main_menu_help[] = {
	&main_menu_help_about,
	NULL
};


static const struct vga_menu_bar_item main_menu_bar[] = {
	/* name                 key     scan    x       w       id */
	{" File ",		'F',	0x21,	0,	6,	&main_menu_file}, /* ALT-F */
	{" Port ",		'P',	0x19,	6,	6,	&main_menu_port}, /* ALT-P */
	{" Help ",		'H',	0x23,	12,	6,	&main_menu_help}, /* ALT-H */
	{NULL,			0,	0x00,	0,	0,	0}
};

static void ui_anim(int force) {
	sprintf(str_tmp,"Frame=%08lX IRQEV=0x%08lX",(unsigned long)ohci->FrameNumber,(unsigned long)ohci->irq_events);
	vga_moveto(10,0);
	vga_write_color(0xE);
	vga_write(str_tmp);
}

static void my_vga_menu_idle() {
	ui_anim(0);
}

void main_menu() {
	const struct vga_menu_item *mitem = NULL;
	unsigned char fullredraw=1,redraw=1;
	uint32_t last_redraw_usb_frame=0,redrawcount=0,tmp;
	unsigned int die=0;
	VGA_ALPHA_PTR vga;
	unsigned int x,y;
	int selector=0;
	int i,port,c;

	while (!die) {
		usb_ohci_ci_update_frame_status(ohci);

		if (fullredraw) {
			fullredraw=0;
			redraw=1;

			vga = vga_alpha_ram;
			x = vga_width * vga_height;
			for (y=0;y < x;y++) vga[y] = 0x0700 | ' ';

			vga_menu_bar_draw();
		}

		/* if the USB controller indicates a change of the USB root hub, update our GUI */
		_cli();
		if (ohci->irq_events & 0x40UL) {
			ohci->irq_events &= ~0x40UL;
			usb_ohci_ci_update_root_hub_status(ohci);
			redraw = 1;
		}
		/* or if some number of USB frames go by without any event, update our GUI */
		else {
			uint32_t dif = ohci->FrameNumber - last_redraw_usb_frame;
			if (dif >= 5000) { /* 5 seconds of USB frames */
				usb_ohci_ci_update_root_hub_status(ohci);
				redraw = 1;
			}
		}
		_sti();

		if (redraw) {
			_cli();
			last_redraw_usb_frame = ohci->FrameNumber;
			_sti();

			vga_write_color(selector == 0 ? 0x70 : 0x07);

			vga_moveto(0,3);
			sprintf(str_tmp,"Root Hub Descriptor: %02u ports [redraw=%lu]\n",
				ohci->RootHubDescriptor.f.NumberDownstreamPorts,(unsigned long)(redrawcount++));
			vga_write(str_tmp);

			sprintf(str_tmp,"PSM=%u NPS=%u OCPM=%u NOCP=%u POTGT=%-3ums\n",
				ohci->RootHubDescriptor.f.PowerSwitchingMode,
				ohci->RootHubDescriptor.f.NoPowerSwitching,
				ohci->RootHubDescriptor.f.OverCurrentProtectionMode,
				ohci->RootHubDescriptor.f.NoOverCurrentProtection,
				(unsigned int)ohci->RootHubDescriptor.f.PowerOnToGoodTime * 2U);
			vga_write(str_tmp);

			sprintf(str_tmp,"REMOVEABLE=%08lX POWERCTRL=%08lX OCI=%u DRWE=%u OCIC=%u\n",
				(unsigned long)(ohci->RootHubDescriptor.f.DeviceRemovable),
				(unsigned long)(ohci->RootHubDescriptor.f.PortPowerControlMask),
				ohci->RootHubDescriptor.f.OverCurrentIndicator,
				ohci->RootHubDescriptor.f.DeviceRemoteWakeupEnable,
				ohci->RootHubDescriptor.f.OverCurrentIndicatorChange);
			vga_write(str_tmp);

			vga_write("\n");

			for (port=1;port <= ohci->RootHubDescriptor.f.NumberDownstreamPorts;port++) {
				uint32_t ps = usb_ohci_ci_read_port_status(ohci,port);

				if (!(ps&0x00000001UL)) /* not connected: grayed out */
					vga_write_color(selector == port ? 0x78 : 0x08);
				else if (ps&0x00000008UL) /* overcurrent: red */
					vga_write_color(selector == port ? 0x7C : 0x0C);
				else if (ps&0x001F0000UL) /* something changed: bright green */
					vga_write_color(selector == port ? 0x20 : 0x0A);
				else
					vga_write_color(selector == port ? 0x70 : 0x07);

				sprintf(str_tmp,"PORT%d Connected=%u Enabled=%u Suspended=%u OCI=%u Reset=%u Power=%u LowSpeed=%u\n",
					port,
					(ps&0x00000001UL)?1:0,	/* CCS */
					(ps&0x00000002UL)?1:0,	/* PES */
					(ps&0x00000004UL)?1:0,	/* PSS */
					(ps&0x00000008UL)?1:0,	/* POCI Overcurrent Indicator */
					(ps&0x00000010UL)?1:0,	/* PRS */
					(ps&0x00000100UL)?1:0,	/* PPS */
					(ps&0x00000200UL)?1:0);	/* LSDA */
				vga_write(str_tmp);

				sprintf(str_tmp,"  ConnChg=%u EnChg=%u SuspChg=%u OCIC=%u ResetChg=%u CantRem=%u PerPortPwr=%u\n",
					(ps&0x00010000UL)?1:0,	/* CSC */
					(ps&0x00020000UL)?1:0,	/* PESC */
					(ps&0x00040000UL)?1:0,	/* PSSC */
					(ps&0x00080000UL)?1:0,	/* OCIC */
					(ps&0x00100000UL)?1:0,	/* PRSC */
					(ohci->RootHubDescriptor.f.DeviceRemovable>>port)&1,
					(ohci->RootHubDescriptor.f.PortPowerControlMask>>port&1));
				vga_write(str_tmp);
			}

			redraw=0;
		}

		ui_anim(0);

		if ((mitem = vga_menu_bar_keymon()) != NULL) {
			/* act on it */
			if (mitem == &main_menu_file_quit) {
				die = 1;
			}
			else if (mitem == &main_menu_help_about) {
				struct vga_msg_box box;
				vga_msg_box_create(&box,"USB OHCI test program v1.0 for DOS\n\n(C) 2011-2012 Jonathan Campbell\nALL RIGHTS RESERVED\n"
#if TARGET_MSDOS == 32
					"32-bit protected mode version"
#elif defined(__LARGE__)
					"16-bit real mode (large model) version"
#elif defined(__MEDIUM__)
					"16-bit real mode (medium model) version"
#elif defined(__COMPACT__)
					"16-bit real mode (compact model) version"
#else
					"16-bit real mode (small model) version"
#endif
					,0,0);
				while (1) {
					ui_anim(0);
					if (kbhit()) {
						i = getch();
						if (i == 0) i = getch() << 8;
						if (i == 13 || i == 27) break;
					}
				}
				vga_msg_box_destroy(&box);
			}
			else if (mitem == &main_menu_port_acknowledge) {
				if (selector == 0) {
					/* root hub */
					usb_ohci_ci_write_reg(ohci,0x50/*HcRhStatus*/,1UL<<17UL);
					redraw = 1;
				}
				else {
					/* port */
					usb_ohci_ci_write_port_status(ohci,selector,0x1F0000UL);
					redraw = 1;
				}

				usb_ohci_ci_update_root_hub_status(ohci);
			}
			else if (mitem == &main_menu_port_nps) {
				if (selector == 0) {
					tmp = usb_ohci_ci_read_reg(ohci,0x48/*HcRhDescriptorA*/);
					tmp ^= 0x200;/*NPS*/
					usb_ohci_ci_write_reg(ohci,0x48/*HcRhDescriptorA*/,tmp);
					usb_ohci_ci_update_root_hub_status(ohci);
					redraw = 1;
				}
			}
			else if (mitem == &main_menu_port_psm) {
				if (selector == 0) {
					tmp = usb_ohci_ci_read_reg(ohci,0x48/*HcRhDescriptorA*/);
					tmp ^= 0x100;/*PSM*/
					usb_ohci_ci_write_reg(ohci,0x48/*HcRhDescriptorA*/,tmp);
					usb_ohci_ci_update_root_hub_status(ohci);
					redraw = 1;
				}
			}
			else if (mitem == &main_menu_port_ocpm) {
				if (selector == 0) {
					tmp = usb_ohci_ci_read_reg(ohci,0x48/*HcRhDescriptorA*/);
					tmp ^= 0x800;/*OCPM*/
					usb_ohci_ci_write_reg(ohci,0x48/*HcRhDescriptorA*/,tmp);
					usb_ohci_ci_update_root_hub_status(ohci);
					redraw = 1;
				}
			}
			else if (mitem == &main_menu_port_nocp) {
				if (selector == 0) {
					tmp = usb_ohci_ci_read_reg(ohci,0x48/*HcRhDescriptorA*/);
					tmp ^= 0x1000;/*NOCP*/
					usb_ohci_ci_write_reg(ohci,0x48/*HcRhDescriptorA*/,tmp);
					usb_ohci_ci_update_root_hub_status(ohci);
					redraw = 1;
				}
			}
			else if (mitem == &main_menu_port_clear_global_power) {
				usb_ohci_ci_write_reg(ohci,0x50/*HcRhStatus*/,0x00000001UL);
				usb_ohci_ci_update_root_hub_status(ohci);
				redraw = 1;
			}
			else if (mitem == &main_menu_port_set_global_power) {
				usb_ohci_ci_write_reg(ohci,0x50/*HcRhStatus*/,0x00010000UL);
				usb_ohci_ci_update_root_hub_status(ohci);
				redraw = 1;
			}
			else if (mitem == &main_menu_port_toggle_removeable) {
				if (selector != 0) {
					ohci->RootHubDescriptor.f.DeviceRemovable ^= 1UL << (uint32_t)selector;
					usb_ohci_ci_write_reg(ohci,0x4C/*HcRhDescriptorB*/,ohci->RootHubDescriptor.raw.b);
					usb_ohci_ci_update_root_hub_status(ohci);
					redraw = 1;
				}
			}
			else if (mitem == &main_menu_port_toggle_port_power_mask) {
				if (selector != 0) {
					ohci->RootHubDescriptor.f.PortPowerControlMask ^= 1UL << (uint32_t)selector;
					usb_ohci_ci_write_reg(ohci,0x4C/*HcRhDescriptorB*/,ohci->RootHubDescriptor.raw.b);
					usb_ohci_ci_update_root_hub_status(ohci);
					redraw = 1;
				}
			}
			else if (mitem == &main_menu_port_toggle_port_enable) {
				if (selector != 0) {
					tmp = usb_ohci_ci_read_port_status(ohci,selector);
					if (tmp & 2UL) {
						/* port is enabled, disable */
						usb_ohci_ci_write_port_status(ohci,selector,1UL/*ClearPortEnable*/);
					}
					else {
						/* port is disabled, enable */
						usb_ohci_ci_write_port_status(ohci,selector,2UL/*SetPortEnable*/);
					}
					usb_ohci_ci_update_root_hub_status(ohci);
					redraw = 1;
				}
			}
			else if (mitem == &main_menu_port_toggle_port_suspend) {
				if (selector != 0) {
					tmp = usb_ohci_ci_read_port_status(ohci,selector);
					if (tmp & 4UL) {
						usb_ohci_ci_write_port_status(ohci,selector,8UL/*ClearPortSuspend*/);
					}
					else {
						usb_ohci_ci_write_port_status(ohci,selector,4UL/*SetPortSuspend*/);
					}
					usb_ohci_ci_update_root_hub_status(ohci);
					redraw = 1;
				}
			}
			/* NTS: Some USB controllers, especially the "virtual" one in VirtualBox, don't reliably reset the
				Port Power state bit even when we do power it off. So we can't offer a "toggle" function */
			else if (mitem == &main_menu_port_enable_port_power) {
				if (selector != 0) {
					usb_ohci_ci_write_port_status(ohci,selector,0x100UL/*SetPortPower*/);
					usb_ohci_ci_update_root_hub_status(ohci);
					redraw = 1;
				}
			}
			else if (mitem == &main_menu_port_disable_port_power) {
				if (selector != 0) {
					usb_ohci_ci_write_port_status(ohci,selector,0x200UL/*ClearPortPower*/);
					usb_ohci_ci_update_root_hub_status(ohci);
					redraw = 1;
				}
			}
			else if (mitem == &main_menu_port_do_port_reset) {
				if (selector != 0) {
					usb_ohci_ci_write_port_status(ohci,selector,16UL/*SetPortReset*/);
					usb_ohci_ci_update_root_hub_status(ohci);
					redraw = 1;
				}
			}
		}

		if (kbhit()) {
			c = getch();
			if (c == 0) c = getch() << 8;

			if (c == 0x4800) { /* uparrow */
				if (selector == 0) selector = ohci->RootHubDescriptor.f.NumberDownstreamPorts;
				else selector--;
				redraw = 1;
			}
			else if (c == 0x5000) { /* downarrow */
				if (selector == ohci->RootHubDescriptor.f.NumberDownstreamPorts) selector = 0;
				else selector++;
				redraw = 1;
			}
		}
	}
}

int main(int argc,char **argv) {
	char *arg_dev = NULL;
	int arg_list = 0;
	int bus,dev,func;
	int i,c;

	assert(sizeof(union usb_ohci_ci_rhd) == 12);

	for (i=1;i < argc;) {
		char *a = argv[i++];

		if (*a == '-' || *a == '/') {
			do { a++; } while (*a == '-' || *a == '/');

			if (!strcmp(a,"h") || !strcmp(a,"help") || !strcmp(a,"?")) {
				help();
				return 1;
			}
			else if (!strcmp(a,"list")) {
				arg_list = 1;
			}
			else if (!strcmp(a,"dev")) {
				arg_dev = argv[i++];
			}
			else {
				printf("Unknown switch '%s'\n",a);
				help();
				return 1;
			}
		}
		else {
			printf("Unknown arg '%s'\n",a);
			return 1;
		}
	}

	cpu_probe();
#if TARGET_MSDOS == 32
	probe_dpmi();
	dos_ltp_probe();
#endif
	if (!probe_8254()) {
		printf("8254 timer not found\n");
		return 1;
	}

	if (!probe_8259()) {
		printf("There does not appear to be a PIC on your system\n");
		return 1;
	}
	
	if (!probe_vga()) { /* NTS: By "VGA" we mean any VGA, EGA, CGA, MDA, or other common graphics hardware on the PC platform
                               that acts in common ways and responds to I/O ports 3B0-3BF or 3D0-3DF as well as 3C0-3CF */
		printf("No VGA hardware!\n");
		return 1;
	}

	if (!llmem_init()) {
		printf("Your system is not suitable to use with the Long-Long memory access library\n");
		printf("Reason: %s\n",llmem_reason);
	}

#if TARGET_MSDOS == 16
	if (!flatrealmode_setup(FLATREALMODE_4GB)) {
		printf("Unable to set up flat real mode (needed for 16-bit builds)\n");
		printf("Most ACPI functions require access to the full 4GB range.\n");
	}
#endif

	if (pci_probe(-1) != PCI_CFG_NONE)
		printf("PCI bus detected\n");
	if (pcie_probe(-1) != PCIE_CFG_NONE)
		printf("PCIe bus detected\n");

	ohci = usb_ohci_ci_create();
	if (ohci == NULL) {
		printf("Failed to create OHCI context\n");
		return 1;
	}

	if (pci_cfg != PCI_CFG_NONE) {
		if (pci_bios_last_bus == -1) pci_probe_for_last_bus();

		if (arg_dev == NULL) {
			for (bus=0;(arg_list || ohci->mmio_base == 0ULL) && bus <= pci_bios_last_bus;bus++) {
				for (dev=0;(arg_list || ohci->mmio_base == 0ULL) && dev < 32;dev++) {
					uint8_t functions = pci_probe_device_functions(bus,dev);
					for (func=0;(arg_list || ohci->mmio_base == 0ULL) && func < functions;func++) {
						if (usb_ohci_ci_pci_device_is_ohci(ohci,bus,dev,func)) {
							if (arg_list) {
								printf("PCI:%u:%u:%u IRQ=%d MMIO=%08llX\n",bus,dev,
									func,ohci->IRQ,(unsigned long long)ohci->mmio_base);
							}
						}
					}
				}
			}
		}
	}

	if (pcie_cfg != PCIE_CFG_NONE) {
		if (arg_dev == NULL) {
			for (bus=0;(arg_list || ohci->mmio_base == 0ULL) && bus <= pcie_bios_last_bus;bus++) {
				for (dev=0;(arg_list || ohci->mmio_base == 0ULL) && dev < 32;dev++) {
					uint8_t functions = pcie_probe_device_functions(bus,dev);
					for (func=0;(arg_list || ohci->mmio_base == 0ULL) && func < functions;func++) {
						if (usb_ohci_ci_pcie_device_is_ohci(ohci,bus,dev,func)) {
							if (arg_list) {
								printf("PCIE:%u:%u:%u IRQ=%d MMIO=%08llX\n",bus,dev,
									func,ohci->IRQ,(unsigned long long)ohci->mmio_base);
							}
						}
					}
				}
			}
		}
	}

	if (!arg_list && ohci->mmio_base == 0ULL && arg_dev != NULL) {
		char *p = arg_dev;

		if (!strncasecmp(p,"pci:",4)) {
			p += 4;
			bus = (int)strtoul(p,&p,10);
			if (*p == ':') p++;
			dev = (int)strtoul(p,&p,10);
			if (*p == ':') p++;
			func = (int)strtoul(p,&p,10);

			if (!usb_ohci_ci_pci_device_is_ohci(ohci,bus,dev,func))
				fprintf(stderr,"PCI %u:%u:%u is not correct device\n",bus,dev,func);
		}
		else if (!strncasecmp(p,"pcie:",5)) {
			p += 5;
			bus = (int)strtoul(p,&p,10);
			if (*p == ':') p++;
			dev = (int)strtoul(p,&p,10);
			if (*p == ':') p++;
			func = (int)strtoul(p,&p,10);

			if (!usb_ohci_ci_pcie_device_is_ohci(ohci,bus,dev,func))
				fprintf(stderr,"PCIE %u:%u:%u is not correct device\n",bus,dev,func);
		}
	}

	if (arg_list || ohci->mmio_base == 0ULL || ohci->IRQ <= 0) {
		if (!arg_list) printf("No USB controllers found\n");
		ohci = usb_ohci_ci_destroy(ohci);
		return 1;
	}

	printf("Using USB OHCI controller @%08llX IRQ %d\n",(unsigned long long)ohci->mmio_base,ohci->IRQ);
	if (usb_ohci_ci_learn(ohci)) {
		fprintf(stderr,"Failed to learn about the controller\n");
		return 1;
	}
	printf("OHCI %u.%u compliant controller\n",ohci->bcd_revision>>4,ohci->bcd_revision&0xF);
	if (ohci->legacy_support) printf(" - With Legacy Support\n");
	if (ohci->legacy_emulation_active) printf(" - Legacy emulation is active\n");
	if (ohci->legacy_irq_enable) printf(" - Legacy IRQ enabled (IRQ1 and IRQ12)\n");
	if (ohci->legacy_external_irq_enable) printf(" - Keyboard controller IRQs cause Legacy Support emulation interrupt\n");
	if (ohci->legacy_a20_state) printf(" - Legacy A20 gate active\n");
	if (ohci->bios_is_using_it) printf(" - BIOS is using it now\n");
	if (ohci->bios_was_using_it) printf(" - BIOS was using it when we started\n");
	if (ohci->control.f.RemoteWakeupConnected) printf(" - Remote Wakeup connected\n");
	printf(" - Control Bulk Service Ratio: %u\n",ohci->control.f.ControlBulkServiceRatio);
	printf(" - Interrupt routine (to SMI): %u\n",ohci->control.f.InterruptRouting);
	printf(" - Raw control: %08lX\n",(unsigned long)(ohci->control.raw));
	printf(" - Full speed largest data packet %u\n",ohci->FullSpeedLargestDataPacket);
	printf(" - Frame interval/remain/number %u/%u/%lu Periodic start %u\n",ohci->FrameInterval,ohci->FrameRemaining,(unsigned long)ohci->FrameNumber,ohci->PeriodicStart);
	printf(" - Controller state: ");
	switch (ohci->control.f.HostControllerFunctionalState) {
		case 0: printf("Reset"); break;
		case 1: printf("Resume"); break;
		case 2: printf("Operational"); break;
		case 3: printf("Suspend"); break;
	};
	printf("\n");

	printf("Hit ENTER to continue. Continuation may include grabbing control of the\n");
	printf("USB controller from your BIOS, which may make your USB keyboard unresponsive.\n");
	printf("For reliability, connect a PS/2 keyboard and reboot before continuing.\n");
	printf("Hit ESC to exit\n");
	do {
		c = getch();
		if (c == 27) return 1;
	} while (c != 13);

	/* if the BIOS is using it then we have to request ownership */
	if (ohci->bios_is_using_it) {
		printf("Asking BIOS to relinquish control of OHCI controller...\n");
		if (usb_ohci_ci_ownership_change(ohci,0/*BIOS should release ownership*/))
			printf("Failed to change ownership.\n");
		if (ohci->bios_is_using_it)
			printf("Failed to change ownership, function succeeded but BIOS is still using it\n");
	}

	/* software reset the controller */
	printf("Resetting controller...\n");
	if (usb_ohci_ci_software_reset(ohci))
		printf("Controller reset failure\n");

	usb_old_irq = _dos_getvect(irq2int(ohci->IRQ));
	_dos_setvect(irq2int(ohci->IRQ),usb_irq);
	p8259_unmask(ohci->IRQ);

	printf("Resuming controller...\n");
	if (usb_ohci_ci_software_resume(ohci))
		printf("Controller resume failure\n");

	printf("Bringing controller online...\n");
	if (usb_ohci_ci_software_online(ohci))
		printf("Controller resume failure\n");

	/* we have the IRQ hooked, it's safe now to enable interrupts */
	/* NTS: Early versions of this code left the SOF interrupt enabled, which meant that at minimum the USB
		controller would fire an IRQ every 1ms. But an IRQ firing 1000 times/sec seems to cause stack overflows
		on slower machines under the DOS extender, and stack overflows on 16-bit real mode builds because of
		the overhead in checking and validating Flat Real Mode. So we leave it off. */
	usb_ohci_ci_write_reg(ohci,0x10/*HcInterruptEnable*/,0xC0000068); /* enable MIE+OC+RHSC+FNO+RD */

	printf(" - Controller state: ");
	switch (ohci->control.f.HostControllerFunctionalState) {
		case 0: printf("Reset"); break;
		case 1: printf("Resume"); break;
		case 2: printf("Operational"); break;
		case 3: printf("Suspend"); break;
	};
	printf("\n");

	usb_ohci_ci_update_root_hub_status(ohci);

	printf(" - Root Hub A=%08lX: %u ports, PSM=%u NPS=%u DT=%u OCPM=%u NOCP=%u POTGT=%u\n",
		(unsigned long)(ohci->RootHubDescriptor.raw.a),
		ohci->RootHubDescriptor.f.NumberDownstreamPorts,
		ohci->RootHubDescriptor.f.PowerSwitchingMode,
		ohci->RootHubDescriptor.f.NoPowerSwitching,
		ohci->RootHubDescriptor.f.DeviceType,
		ohci->RootHubDescriptor.f.OverCurrentProtectionMode,
		ohci->RootHubDescriptor.f.NoOverCurrentProtection,
		ohci->RootHubDescriptor.f.PowerOnToGoodTime);
	printf(" - Root hub B=%08lX REMOVEABLE=0x%08lX POWER=0x%08lX\n",
		(unsigned long)(ohci->RootHubDescriptor.raw.b),
		(unsigned long)(ohci->RootHubDescriptor.f.DeviceRemovable),
		(unsigned long)(ohci->RootHubDescriptor.f.PortPowerControlMask));
	printf(" - Root hub C=%08lX status LPS=%u OCI=%u DRWE=%u LPSC=%u OCIC=%u CRWE=%u\n",
		(unsigned long)(ohci->RootHubDescriptor.raw.c),
		ohci->RootHubDescriptor.f.LocalPowerStatus,
		ohci->RootHubDescriptor.f.OverCurrentIndicator,
		ohci->RootHubDescriptor.f.DeviceRemoteWakeupEnable,
		ohci->RootHubDescriptor.f.LocalPowerStatusChange,
		ohci->RootHubDescriptor.f.OverCurrentIndicatorChange,
		ohci->RootHubDescriptor.f.ClearRemoteWakeupEnable);

	printf("Hit ENTER to continue.\n");
	do {
		if (kbhit()) {
			c = getch();
			if (c == 13 || c == 27) break;
		}
	} while (1);

	if (ohci->RootHubDescriptor.f.NumberDownstreamPorts > 7)
		vga_bios_set_80x50_text();

	vga_menu_bar.bar = main_menu_bar;
	vga_menu_bar.sel = -1;
	vga_menu_bar.row = 1;
	vga_menu_idle = my_vga_menu_idle;

	main_menu();

	int10_setmode(3);
	update_state_from_vga();

	vga_clear();
	vga_moveto(0,0);
	vga_write_color(7);
	vga_sync_bios_cursor();

	/* restore IRQ. do NOT mask IRQ if the BIOS is involved in any way */
	_dos_setvect(irq2int(ohci->IRQ),usb_old_irq);
	if (!ohci->bios_was_using_it && !ohci->bios_is_using_it) p8259_mask(ohci->IRQ);
	
	/* shutdown process: reset the controller */
	printf("Resetting controller...\n");
	if (usb_ohci_ci_software_reset(ohci))
		printf("Controller reset failure\n");
	if (usb_ohci_ci_set_usb_reset_state(ohci))
		printf("Controller reset-state failure\n");

	/* if the BIOS was using it, then we have to give ownership back */
	if (ohci->bios_was_using_it) {
		printf("Giving control back to BIOS...\n");

		if (usb_ohci_ci_ownership_change(ohci,1/*BIOS should regain ownership*/))
			printf("Failed to change ownership.\n");
		if (!ohci->bios_is_using_it)
			printf("Failed to change ownership, function succeeded but BIOS didn't claim ownership\n");
	}

	ohci = usb_ohci_ci_destroy(ohci);
	return 0;
}

