/* 8254.h
 *
 * 8254 programmable interrupt timer control library.
 * (C) 2008-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box] */
 
#ifndef __HW_8254_8254_H
#define __HW_8254_8254_H

#include <hw/cpu/cpu.h>
#include <stdint.h>

/* WARNING: When calling these functions it is recommended that you disable interrupts
 *          during the programming procedure. In MS-DOS mode there is nothing to stop
 *          the BIOS from trying to use the 8254 chip at the same time you are. It is
 *          entirely possible that it might read values back during an interrupt. If
 *          you do not watch for that, you will get erroneous results from these
 *          routines. Use hw/cpu.h _cli() and _sti() functions.
 *
 *          In contrast, it is extremely rare for interrupt handlers to mess with the
 *          PC speaker gate port (and even if they do, the worst that can happen is
 *          our code overrides what the IRQ is trying to do) */

#ifdef TARGET_PC98
/* NEC PC-98 */
# define PC_SPEAKER_GATE					0x35
# define PC_SPEAKER_GATE_MASK					0x01 /* mask prior to shift */
# define PC_SPEAKER_GATE_XOR                                    0x01
# define PC_SPEAKER_GATE_SHIFT					3

/* values to use with pc_speaker_set_gate */
# define PC_SPEAKER_GATE_ON					0x1
# define PC_SPEAKER_GATE_OFF					0x0
#else
/* IBM PC/XT/AT */
# define PC_SPEAKER_GATE					0x61
# define PC_SPEAKER_GATE_MASK					0x03 /* mask prior to shift */
# define PC_SPEAKER_GATE_XOR                                    0x00
# define PC_SPEAKER_GATE_SHIFT					0

/* values to use with pc_speaker_set_gate */
# define PC_SPEAKER_GATE_ON					0x3
# define PC_SPEAKER_GATE_OFF					0x0

/* there's more nuance to it if you care to use it */
# define PC_SPEAKER_OUTPUT_TTL_AND_GATE				0x1 /* the AND gate between counter 2 output and the speaker */
# define PC_SPEAKER_COUNTER_2_GATE				0x2 /* the GATE line going into counter 2 */
#endif

#ifdef TARGET_PC98
/* NEC PC-98 */
/* The base clock can be one of two frequencies: 1.9968MHz, or 2.4576MHz, depending on whether the CPU is 5MHz, 8MHz, or 10MHz */
extern uint32_t __T8254_REF_CLOCK_HZ;
# define T8254_REF_CLOCK_HZ					(__T8254_REF_CLOCK_HZ)
#else
/* IBM PC/XT/AT */
/* 1.19318MHz from which the counter values divide down from */
# define T8254_REF_CLOCK_HZ					(1193180UL)
#endif

/* IBM PC/XT/AT and PC-98 use the same IRQ */
#define T8254_IRQ						0

#ifdef TARGET_PC98
# define T8254_TIMER_INTERRUPT_TICK				0
# define T8254_TIMER_PC_SPEAKER					1
# define T8254_TIMER_RS232					2
#else
# define T8254_TIMER_INTERRUPT_TICK				0
# define T8254_TIMER_DRAM_REFRESH				1
# define T8254_TIMER_PC_SPEAKER					2
#endif

#ifdef TARGET_PC98
/* PC-98 I/O ports 71h-77h odd only */
# define T8254_PORT(x)						(((x) * 2U) + 0x71U)
#else
/* IBM PC/XT/AT I/O ports 40h-43h */
# define T8254_PORT(x)						((x) + 0x40U)
#endif

#define T8254_TIMER_PORT(x)					T8254_PORT(x)
#define T8254_CONTROL_PORT					T8254_PORT(3)

/* Control port mode (M2,M1,M0) */
#define T8254_MODE_0_INT_ON_TERMINAL_COUNT			0			/* 000 */
#define T8254_MODE_1_HARDWARE_RETRIGGERABLE_ONE_SHOT		1			/* 001 */
#define T8254_MODE_2_RATE_GENERATOR				2			/* X10 */
#define T8254_MODE_3_SQUARE_WAVE_MODE				3			/* X11 */
#define T8254_MODE_4_SOFTWARE_TRIGGERED_STROBE			4			/* 100 */
#define T8254_MODE_5_HARDWARE_TRIGGERED_STROBE			5			/* 101 */

/* Control port read/write (RW1,RW0) */
#define T8254_RW_COUNTER_LATCH					0			/* Counter latch (read operation) */
#define T8254_RW_LSB_ONLY					1			/* Read/write LSB only */
#define T8254_RW_MSB_ONLY					2			/* Read/write MSB only */
#define T8254_RW_LSB_THEN_MSB					3			/* Read/write LSB, then MSB */

/* readback bitmask */
#define T8254_READBACK_COUNT					0x20
#define T8254_READBACK_STATUS					0x10
#define T8254_READBACK_TIMER_2					0x08
#define T8254_READBACK_TIMER_1					0x04
#define T8254_READBACK_TIMER_0					0x02
#define T8254_READBACK_ALL					0x3E

/* this represents one counter value in the 8254/8253 chipset library, including the
 * value the chip reloads on finishing countdown.
 * NTS: In 8254 hardware, a value of 0 is treated as 65536 because the chip decrements
 *      THEN checks against zero. This allows the rate to drop as low as
 *      1193180 / 65536 = 18.20648Hz on PC hardware */
typedef uint16_t t8254_time_t;

/* the 8254 (NOT 8253!) has a command that allows us to read the status and counter
 * value of one or more latches (though contemporary hardware fails to emulate multi-
 * counter latching from one command!). The status allows us to know what the counter
 * was programmed as, the output of the counter at the time, and whether a new counter
 * value was being loaded. The t8254_readback_t structure is used to read this info */
struct t8254_readback_entry_t {
	unsigned char		status;
	t8254_time_t		count;
};

struct t8254_readback_t {
	struct t8254_readback_entry_t	timer[3];
};

extern uint32_t t8254_counter[3];
extern int8_t probed_8254_result;

int probe_8254();
void readback_8254(unsigned char what,struct t8254_readback_t *t); /* WARNING: 8254 only, will not work on 8253 chips in original PC/XT hardware */
unsigned long t8254_us2ticks(unsigned long a);
unsigned long t8254_us2ticksr(unsigned long a,unsigned long *rem);
void t8254_wait(unsigned long ticks);

static inline t8254_time_t read_8254_ncli(unsigned char timer) {
	t8254_time_t x;

	if (timer > 2) return 0;
	outp(T8254_CONTROL_PORT,(timer << 6) | (T8254_RW_COUNTER_LATCH << 4) | 0/*don't care*/); /* latch counter N, counter latch read */
	x  = (t8254_time_t)inp(T8254_TIMER_PORT(timer));
	x |= (t8254_time_t)inp(T8254_TIMER_PORT(timer)) << 8U;
	return x;
}

static inline t8254_time_t read_8254(unsigned char timer) {
	unsigned int flags;
	t8254_time_t x;

	flags = get_cpu_flags();
	x = read_8254_ncli(timer);
	_sti_if_flags(flags);
	return x;
}

/* NTS: At the hardware level, count == 0 is equivalent to programming 0x10000 into it.
 *      t8254_time_t is a 16-bit integer, and we write 16 bits, so 0 and 0x10000 is
 *      the same thing to us anyway */
static inline void write_8254_ncli(unsigned char timer,t8254_time_t count,unsigned char mode) {
	if (timer > 2) return;
	outp(T8254_CONTROL_PORT,(timer << 6) | (T8254_RW_LSB_THEN_MSB << 4) | (mode << 1)); /* set new time */
	outp(T8254_TIMER_PORT(timer),count);
	outp(T8254_TIMER_PORT(timer),count >> 8);
	/* for our own timing code, keep track of what that count was. we can't read it back from H/W anyway */
	t8254_counter[timer] = (count == 0 ? 0x10000 : count);
}

static inline void write_8254(unsigned char timer,t8254_time_t count,unsigned char mode) {
	unsigned int flags;

	flags = get_cpu_flags();
	write_8254_ncli(timer,count,mode);
	_sti_if_flags(flags);
}

static inline unsigned char t8254_pc_speaker_read_gate() {
	return ((inp(PC_SPEAKER_GATE) >> PC_SPEAKER_GATE_SHIFT) & PC_SPEAKER_GATE_MASK) ^ PC_SPEAKER_GATE_XOR;
}

static inline void t8254_pc_speaker_set_gate(unsigned char m) {
	unsigned char x;

	x = inp(PC_SPEAKER_GATE);
	x &= ~(PC_SPEAKER_GATE_MASK << PC_SPEAKER_GATE_SHIFT);
	x |=  ((m ^ PC_SPEAKER_GATE_XOR) & PC_SPEAKER_GATE_MASK) << PC_SPEAKER_GATE_SHIFT;
	outp(PC_SPEAKER_GATE,x);
}

static inline void write_8254_system_timer(t8254_time_t max) {
	write_8254(T8254_TIMER_INTERRUPT_TICK,max,T8254_MODE_2_RATE_GENERATOR);
}

static inline void write_8254_pc_speaker(t8254_time_t max) {
	write_8254(T8254_TIMER_PC_SPEAKER,max,T8254_MODE_3_SQUARE_WAVE_MODE);
}

#ifdef TARGET_PC98
/* NEC PC-98 */
/* FIXME: Does the PC-98 have a way to read PC speaker output back like IBM PC? */
# define read_8254_pc_speaker_output() (0)
#else
/* IBM PC/XT/AT */
static inline uint8_t read_8254_pc_speaker_output() {
	/* bit 5 of port 61h gives us the output of the counter (prior to the AND gate) */
	return (inp(PC_SPEAKER_GATE) & 0x20);
}
#endif

#endif /* __HW_8254_8254_H */

