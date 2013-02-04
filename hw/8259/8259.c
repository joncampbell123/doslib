/* 8259.c
 *
 * 8259 programmable interrupt controller library.
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box]
 *
 * In PC hardware the 8259 is responsible for taking IRQ signals off the ISA bus, prioritizing
 * them, and interrupting the CPU to service the interrupts. The chip includes logic to keep
 * track of what interrupts are pending, active, not yet acknowledged by the CPU, etc. so that
 * the x86 CPU can properly handle them.
 *
 * In mid 1990s hardware the PIC was retained and often connected through a PCI core chipset
 * that routed both ISA and PCI interrupts into it. Some chipsets have additional registers
 * that allow "routing control", to determine whether a particular IRQ is to be associated
 * with a PCI device or the ISA bus.
 *
 * Starting in late 1990s hardware, the APIC (Advanced Programmable Interrupt Controller)
 * appeared on motherboards and was either put alongside with, or replaced, the traditional
 * PIC. But the traditional I/O ports are emulated just the same to ensure compatibility with
 * older software. Even today (in 2012) DOS programs can still communicate with I/O ports
 * 20-21h and A0-A1h to manage interrupts, at least until an APIC aware OS or program takes
 * control.
 */

/* NTS: As of 2011/02/27 the 8254 routines no longer do cli/sti for us, we are expected
 *      to do them ourself. This is for performance reasons as well as for sanity reasons
 *      should we ever need to use the subroutines from within an interrupt handler */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/8259/8259.h>

unsigned char p8259_slave_present = 0;
signed char p8259_probed = 0;

void p8259_ICW(unsigned char a,unsigned char b,unsigned char c,unsigned char d) {
	outp(p8259_irq_to_base_port(c,0),a | 0x10);	/* D4 == 1 */
	outp(p8259_irq_to_base_port(c,1),b);
	outp(p8259_irq_to_base_port(c,1),c);
	if (a & 1) outp(p8259_irq_to_base_port(c,1),d);
}

/* NTS: bit 7 is set if there was an interrupt */
/* WARNING: This code crashes DOSBox 0.74 with "PIC poll not handled" error message */
unsigned char p8259_poll(unsigned char c) {
	/* issue poll command to read and ack an interrupt */
	p8259_OCW3(c,0x0C);	/* OCW3 = POLL=1 SMM=0 RR=0 */
	return inp(p8259_irq_to_base_port(c,0));
}

int probe_8259() {
	unsigned char om,cm,c2;
	unsigned int flags;

	if (p8259_probed < 0)
		return (int)p8259_probed;

	/* don't let the BIOS fiddle with the mask during
	   the test. Fixes: Pentium machine where 1 out of
	   100 times programs fail with "cannot init PIC" */
	flags = get_cpu_flags(); _cli();
	om = p8259_read_mask(0);
	p8259_write_mask(0,0xFF);
	cm = p8259_read_mask(0);
	p8259_write_mask(0,0x00);
	c2 = p8259_read_mask(0);
	p8259_write_mask(0,om);
	set_cpu_flags(flags);

	if (cm != 0xFF || c2 != 0x00)
		return (p8259_probed=0);

	/* is a slave present too? */
	flags = get_cpu_flags(); _cli();
	om = p8259_read_mask(8);
	p8259_write_mask(8,0xFF);
	cm = p8259_read_mask(8);
	p8259_write_mask(8,0x00);
	c2 = p8259_read_mask(8);
	p8259_write_mask(8,om);
	set_cpu_flags(flags);

	if (cm == 0xFF && c2 == 0x00)
		p8259_slave_present = 1;

	return (p8259_probed=1);
}

