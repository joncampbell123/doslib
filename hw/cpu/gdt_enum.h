/* gdt_enum.h
 *
 * Library for reading the Global Descriptor Table if possible.
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS
 *   - Windows 3.0/3.1/95/98/ME
 *   - Windows NT 3.1/3.51/4.0/2000/XP/Vista/7
 *   - OS/2 16-bit
 *   - OS/2 32-bit
 *
 */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>

#pragma pack(push,1)
struct x86_gdtr {
	uint16_t	limit;
	uint32_t	base;
	uint16_t	pad;
};
#pragma pack(pop)

#pragma pack(push,1)
struct cpu_gdtlib_entry {
	uint32_t	limit;
	uint32_t	base;
	uint8_t		access,granularity;
};
#pragma pack(pop)

extern unsigned char			cpu_gdtlib_can_read,cpu_gdtlib_can_write,cpu_gdtlib_result;
extern uint16_t				cpu_gdtlib_ldtr;
extern struct x86_gdtr			cpu_gdtlib_gdtr;
extern struct cpu_gdtlib_entry		cpu_gdtlib_ldt_ent;
#if TARGET_MSDOS == 16 && defined(TARGET_WINDOWS)
extern uint16_t				cpu_gdtlib_gdt_sel;	/* selector for reading GDT table */
extern uint16_t				cpu_gdtlib_ldt_sel;	/* selector for reading LDT table */
#endif

/* HACK: Win32 builds must be careful. Windows 9x/ME sets the flat selector as base == 0 limit = 0xFFFFFFFF while
 *       Windows 3.1 Win32s sets base = 0xFFFF0000 limit = 0xFFFFFFFF for whatever weird reason. */
#if TARGET_MSDOS == 32 && defined(TARGET_WINDOWS)
extern uint32_t				cpu_gdtlib_lin_bias;
#else
# define cpu_gdtlib_lin_bias (0UL)
#endif

#define cpu_gdtlib_empty_ldt_entry	cpu_gdtlib_empty_gdt_entry

unsigned int cpu_gdtlib_gdt_entries(struct x86_gdtr *r);
unsigned int cpu_gdtlib_ldt_entries(struct cpu_gdtlib_entry *r);
int cpu_gdtlib_read_ldtr(uint16_t *sel);
int cpu_gdtlib_read_gdtr(struct x86_gdtr *raw);
int cpu_gdtlib_init();
void cpu_gdtlib_free();
int cpu_gdtlib_ldt_read_entry(struct cpu_gdtlib_entry *e,unsigned int i);
int cpu_gdtlib_gdt_read_entry(struct cpu_gdtlib_entry *e,unsigned int i);
int cpu_gdtlib_empty_gdt_entry(struct cpu_gdtlib_entry *e);
int cpu_gdtlib_entry_is_special(struct cpu_gdtlib_entry *e);
int cpu_gdtlib_entry_is_executable(struct cpu_gdtlib_entry *e);
int cpu_gdtlib_read_current_regs();
int cpu_gdtlib_prepare_to_read_ldt();

