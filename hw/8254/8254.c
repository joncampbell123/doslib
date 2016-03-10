/* 8254.c
 *
 * 8254 programmable interrupt timer control library.
 * (C) 2008-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box]
 *
 * The 8254 Programmable Interrupt Timer is used on the PC platform for
 * several purposes. 3 Timers are provided, which are used as:
 *
 *  Timer 0 - System timer. This is tied to IRQ 0 and under normal operation
 *            provides the usual IRQ 0 18.2 ticks per second. DOS programs that
 *            need higher resolution reprogram this timer to get it.
 *
 *  Timer 1 - Misc, varies. On older PC hardware this was tied to DRAM refresh.
 *            Modern hardware cycles it for whatever purpose. Don't assume it's function.
 *
 *  Timer 2 - PC Speaker. This timer is configured to run as a square wave and it's
 *            output is gated through the 8042 before going directly to a speaker in the
 *            PC's computer case. When used, this timer allows your program to generate
 *            audible beeps.
 *
 * On modern hardware this chip is either integrated into the core motherboard chipset or
 * emulated for backwards compatibility.
 */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>

#include <hw/cpu/cpu.h>
#include <hw/8254/8254.h>

uint32_t t8254_counter[3] = {0x10000,0x10000,0x10000};

