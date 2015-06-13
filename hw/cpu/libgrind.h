
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
typedef uint32_t		grind_imm_t;
#else
# define GRIND_REGSIZE		16
# define GRIND_CODE16
# define GRIND_FAR
typedef uint16_t		grind_off_t;
typedef uint16_t		grind_seg_t;
typedef uint16_t		grind_imm_t;
#endif

#ifdef GRIND_FAR
typedef unsigned char far*	grind_buf_ptr_t;
# define GRIND_RET_INS		(0xCB) /* RETF */
#else
typedef unsigned char*		grind_buf_ptr_t;
# define GRIND_RET_INS		(0xC3) /* RET */
#endif

#define GRIND_INT3_INS		(0xCC) /* INT 3 */

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

// regs
enum {
	GRIND_REG_AX=0,
	GRIND_REG_CX,
	GRIND_REG_DX,
	GRIND_REG_BX,
	GRIND_REG_SP,
	GRIND_REG_BP,
	GRIND_REG_SI,
	GRIND_REG_DI
};

// sregs
enum {
	GRIND_SEG_ES=0,
	GRIND_SEG_CS,
	GRIND_SEG_SS,
	GRIND_SEG_DS
};

// PUSH sreg
static inline grind_buf_ptr_t grind_buf_w__push_Sreg(grind_buf_ptr_t w,unsigned char sreg) {
	if (sreg >= 4) return w;
	*w++ = 0x06 + (sreg << 3);
	return w;
}

// PUSH (E)reg
static inline grind_buf_ptr_t grind_buf_w__push_Reg(grind_buf_ptr_t w,unsigned char reg) {
	if (reg >= 8) return w;
	*w++ = 0x50 + (reg&7);
	return w;
}

// PUSHF(D)
static inline grind_buf_ptr_t grind_buf_w__push_Flags(grind_buf_ptr_t w) {
	*w++ = 0x9C;
	return w;
}

// ADD (E)reg,const
static inline grind_buf_ptr_t grind_buf_w__mov_Add_const(grind_buf_ptr_t w,unsigned char reg,unsigned int c) {
	if (reg >= 8) return w;
	if (reg == GRIND_REG_AX) {
		*w++ = 0x05;		// ADD (E)AX,const
	}
	else {
		*w++ = 0x81;		// ADD reg,const
		*w++ = (3 << 6) + (0/*ADD*/ << 3) + reg;	// mod=3 reg=0 r/m=reg
	}
	*((grind_imm_t*)w) = c; w += sizeof(grind_imm_t);
	return w;
}

// SUB (E)reg,const
static inline grind_buf_ptr_t grind_buf_w__mov_Sub_const(grind_buf_ptr_t w,unsigned char reg,unsigned int c) {
	if (reg >= 8) return w;
	if (reg == GRIND_REG_AX) {
		*w++ = 0x05+0x28;	// SUB (E)AX,const
	}
	else {
		*w++ = 0x81;		// SUB reg,const
		*w++ = (3 << 6) + (5/*SUB*/ << 3) + reg;	// mod=3 reg=0 r/m=reg
	}
	*((grind_imm_t*)w) = c; w += sizeof(grind_imm_t);
	return w;
}

// OR (E)reg,const
static inline grind_buf_ptr_t grind_buf_w__or_Reg_const(grind_buf_ptr_t w,unsigned char reg,unsigned int c) {
	if (reg >= 8) return w;
	if (reg == GRIND_REG_AX) {
		*w++ = 0x0D;		// OR (E)AX,const
	}
	else {
		*w++ = 0x81;		// OR reg,const
		*w++ = (3 << 6) + (1/*OR*/ << 3) + reg;		// mod=3 reg=0 r/m=reg
	}
	*((grind_imm_t*)w) = c; w += sizeof(grind_imm_t);
	return w;
}

// AND (E)reg,const
static inline grind_buf_ptr_t grind_buf_w__and_Reg_const(grind_buf_ptr_t w,unsigned char reg,unsigned int c) {
	if (reg >= 8) return w;
	if (reg == GRIND_REG_AX) {
		*w++ = 0x25;		// AND (E)AX,const
	}
	else {
		*w++ = 0x81;		// AND reg,const
		*w++ = (3 << 6) + (4/*AND*/ << 3) + reg;	// mod=3 reg=0 r/m=reg
	}
	*((grind_imm_t*)w) = c; w += sizeof(grind_imm_t);
	return w;
}

// MOV (E)reg,const
static inline grind_buf_ptr_t grind_buf_w__mov_Reg_const(grind_buf_ptr_t w,unsigned char reg,unsigned int c) {
	if (reg >= 8) return w;
	*w++ = 0xB8 + (reg&7);
	*((grind_imm_t*)w) = c; w += sizeof(grind_imm_t);
	return w;
}

// MOV (E)reg,const
static inline grind_buf_ptr_t grind_buf_w__mov_Reg16_const(grind_buf_ptr_t w,unsigned char reg,uint16_t c) {
	if (reg >= 8) return w;
#ifdef GRIND_CODE32
	*w++ = 0x66;		// 32-bit data, override to 16-bit
#endif
	*w++ = 0xB8 + (reg&7);
	*((uint16_t*)w) = c; w += sizeof(uint16_t);
	return w;
}

// MOV <seg>,<reg>
static inline grind_buf_ptr_t grind_buf_w__mov_Seg_from_Reg(grind_buf_ptr_t w,unsigned char sreg,unsigned char reg) {
	if (sreg >= 4) return w;
	if (reg >= 8) return w;
	*w++ = 0x8E;					// MOV Sreg,r/m16
	*w++ = (3 << 6) + (sreg << 3) + reg;		// mod=3 reg=sreg r/m=reg
	return w;
}

// POPF(D)
static inline grind_buf_ptr_t grind_buf_w__pop_Flags(grind_buf_ptr_t w) {
	*w++ = 0x9D;
	return w;
}

// POP (E)reg
static inline grind_buf_ptr_t grind_buf_w__pop_Reg(grind_buf_ptr_t w,unsigned char reg) {
	if (reg >= 8) return w;
	*w++ = 0x58 + (reg&7);
	return w;
}

// POP sreg
static inline grind_buf_ptr_t grind_buf_w__pop_Sreg(grind_buf_ptr_t w,unsigned char sreg) {
	if (sreg >= 4) return w;
	*w++ = 0x07 + (sreg << 3);
	return w;
}

// MOV [offset],reg
static inline grind_buf_ptr_t grind_buf_w__mov_memoff_from_reg(grind_buf_ptr_t w,grind_off_t o,unsigned char reg) {
	if (reg == GRIND_REG_AX) {
		*w++ = 0xA3;				// MOV [offset],(E)AX
	}
	else {
		*w++ = 0x89;				// MOV [offset],(E)reg
#ifdef GRIND_CODE32
		*w++ = (0 << 6) + (reg << 3) + (5);	// mod=0 reg=reg r/m=5 32-bit encoding
#else
		*w++ = (0 << 6) + (reg << 3) + (6);	// mod=0 reg=reg r/m=6 16-bit encoding
#endif
	}
	*((grind_off_t*)w) = o; w += sizeof(grind_off_t);
	return w;
}

// XOR reg,reg
static inline grind_buf_ptr_t grind_buf_w__xor_Reg_Reg(grind_buf_ptr_t w,unsigned char reg1,unsigned char reg2) {
	if (reg1 >= 8 || reg2 >= 8) return w;
	*w++ = 0x31;
	*w++ = (3 << 6) + (reg2 << 3) + reg1;	// mod=3 reg=reg2 r/m=reg1
	return w;
}

// DIV reg
static inline grind_buf_ptr_t grind_buf_w__div_Reg(grind_buf_ptr_t w,unsigned char reg) {
	if (reg >= 8) return w;
	*w++ = 0xF7;
	*w++ = (3 << 6) + (6/*DIV*/ << 3) + reg;	// mod=3 reg=6 r/m=reg1
	return w;
}

// MUL reg
static inline grind_buf_ptr_t grind_buf_w__mul_Reg(grind_buf_ptr_t w,unsigned char reg) {
	if (reg >= 8) return w;
	*w++ = 0xF7;
	*w++ = (3 << 6) + (4/*MUL*/ << 3) + reg;	// mod=3 reg=4 r/m=reg1
	return w;
}

#endif /* __HW_CPU_LIBGRIND_H */

