/* 8259.h
 *
 * 8259 programmable interrupt controller library.
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box] */
 
#ifndef __HW_8259_8259_H
#define __HW_8259_8259_H

#include <hw/cpu/cpu.h>
#include <stdint.h>

/* PIC hardware register description
 *
 *    ICW1        I/O port base + 0  (NTS: bit D4 == 1 when writing this)
 *    ICW2        I/O port base + 1
 *    ICW3        I/O port base + 1
 *    ICW4        I/O port base + 1
 *
 *    OCW1        R/W interrupt mask register. setting the bit masks the interrupt. Use I/O port base + 0
 *    OCW2        R/? EOI and rotate command opcodes (NTS: bit D3-4 == 0). I/O port base + 1
 *    OCW3        R/? poll/read/etc command opcodes (NTS: bit D3-4 == 1) I/O port base + 1
 *
 */
/* PIC library warning:
 *
 * For performance and sanity reasons this code does NOT mask interrupts during the function. You are
 * expected to wrap your calls with cli/sti functions. Though it is unlikely, leaving interrupts enabled
 * while doing this can cause problems in case the BIOS fiddles with the PIC during an IRQ */

#define P8259_MASTER_DATA		0x20
#define P8259_MASTER_MASK		0x21

#define P8259_SLAVE_DATA		0xA0
#define P8259_SLAVE_MASK		0xA1

/* OCW2 command bits. For most commands you are expected to OR the low 3 bits with the IRQ for the command to take effect on */
#define P8259_OCW2_ROTATE_AUTO_EOI_CLEAR	(0U << 5U)
#define P8259_OCW2_NON_SPECIFIC_EOI		(1U << 5U)
#define P8259_OCW2_NO_OP			(2U << 5U)
#define P8259_OCW2_SPECIFIC_EOI			(3U << 5U)
#define P8259_OCW2_ROTATE_AUTO_EOI_SET		(4U << 5U)
#define P8259_OCW2_ROTATE_NON_SPECIFIC_EOI	(5U << 5U)
#define P8259_OCW2_SET_PRIORITY			(6U << 5U)
#define P8259_OCW2_ROTATE_SPECIFIC_EOI		(7U << 5U)

#define P8259_MASK_BIT(x)		(1U << ((x)&7U))

extern unsigned char p8259_slave_present;

/* c = IRQ  which = I/O port */
static inline unsigned char p8259_irq_to_base_port(unsigned char c,unsigned char which) {
	return ((c & 8) ? P8259_SLAVE_DATA : P8259_MASTER_DATA) + which;
}

/* c = IRQ. */
static inline unsigned char p8259_read_mask(unsigned char c) { /* mask register AKA OCW1 */
	outp(p8259_irq_to_base_port(c,0),P8259_OCW2_NO_OP); /* issue NO-OP to make sure the PIC is ready to accept OCW1 */
	return inp(p8259_irq_to_base_port(c,1)); /* mask register */
}

static inline void p8259_write_mask(unsigned char c,unsigned char m) { /* mask register AKA OCW1 */
	outp(p8259_irq_to_base_port(c,0),P8259_OCW2_NO_OP); /* issue NO-OP to make sure the PIC is ready to accept OCW1 */
	outp(p8259_irq_to_base_port(c,1),m); /* write mask register */
}

static inline void p8259_OCW2(unsigned char c,unsigned char w) {
	outp(p8259_irq_to_base_port(c,0),w & 0xE7);	/* D3-4 == 0 */
}

static inline void p8259_OCW3(unsigned char c,unsigned char w) {
	outp(p8259_irq_to_base_port(c,0),(w & 0xE7) | 0x08);	/* D3-4 == 1 */
}

static inline unsigned char p8259_read_IRR(unsigned char c) {
	p8259_OCW3(c,0x02);	/* OCW3 = read register command RR=1 RIS=0 */
	return inp(p8259_irq_to_base_port(c,0)); /* mask register */
}

static inline unsigned char p8259_read_ISR(unsigned char c) {
	p8259_OCW3(c,0x03);	/* OCW3 = read register command RR=1 RIS=1 */
	return inp(p8259_irq_to_base_port(c,0)); /* mask register */
}

static inline unsigned char irq2int(unsigned char c) {
	c &= 0xF;
	if (c & 8) return c-8+0x70;
	return c+0x08;
}

static inline void p8259_unmask(unsigned char c) {
	unsigned char m = p8259_read_mask(c);
	p8259_write_mask(c,m & ~(1 << (c&7)));
}

static inline void p8259_mask(unsigned char c) {
	unsigned char m = p8259_read_mask(c);
	p8259_write_mask(c,m | (1 << (c&7)));
}

static inline unsigned char p8259_is_masked(unsigned char c) {
	return (p8259_read_mask(c) & (1 << (c&7)));
}

void p8259_ICW(unsigned char a,unsigned char b,unsigned char c,unsigned char d);
unsigned char p8259_poll(unsigned char c);
int probe_8259();

#endif /* __HW_8259_8259_H */

