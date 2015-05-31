
#ifndef __HW_CPU_LIBGRIND_H
#define __HW_CPU_LIBGRIND_H

#include <stdint.h>

#include <hw/cpu/cpu.h>

#if defined(TARGET_WINDOWS)
# include <windows.h>
#endif

#if TARGET_MSDOS == 32
# define GRIND_REGSIZE		32
# define GRIND_CODE32
typedef uint32_t		grind_off_t;
typedef uint16_t		grind_seg_t;
#else
# define GRIND_REGSIZE		16
# define GRIND_CODE16
# define GRIND_FAR
typedef uint16_t		grind_off_t;
typedef uint16_t		grind_seg_t;
#endif

#ifdef GRIND_FAR
typedef unsigned char far*	grind_buf_ptr_t;
# define GRIND_RET_INS		(0xCB) /* RETF */
#else
typedef unsigned char*		grind_buf_ptr_t;
# define GRIND_RET_INS		(0xC3) /* RET */
#endif

#if TARGET_MSDOS == 16 && defined(TARGET_WINDOWS) /* For this to work in Windows 3.1, we need a code segment alias of our data segment */
# define GRIND_NEED_CODE_ALIAS_SEL
#endif

extern grind_buf_ptr_t		grind_buf;
extern size_t			grind_buf_size;
extern uint8_t			grind_buf_init;
extern uint8_t			grind_buf_lock;

#ifdef GRIND_NEED_CODE_ALIAS_SEL
extern HGLOBAL			grind_buf_whnd;
extern uint16_t			grind_buf_dsel;
extern uint16_t			grind_buf_csel;
#endif

int grind_init();
int grind_lock_buf();
int grind_alloc_buf();
void grind_unlock_buf();
int grind_execute_buf();
void grind_free_buf();
void grind_free();

#endif /* __HW_CPU_LIBGRIND_H */

