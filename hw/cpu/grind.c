/* grind.c
 *
 * Run instruction encodings through combinations and record changes to registers and flags.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <malloc.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/cpu/libgrind.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

static unsigned char asm_buf[64];

static void pause_if_tty() {
	unsigned char c;
	if (isatty(1)) { do { read(0,&c,1); } while (c != 13); }
}


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




static int grind_selftest_buffer() {
	unsigned int i;

	for (i=0;i < 24;i++) {
		printf("Buffer test... ");fflush(stdout);

		printf("INIT ");fflush(stdout);
		if (!grind_init()) {
			printf("<--FAIL\n");
			grind_free();
			return 1;
		}

		printf("ALLOC(%zu) ",grind_buf_size);fflush(stdout);
		if (!grind_alloc_buf()) {
			printf("<--FAIL\n");
			grind_free();
			return 1;
		}

		printf("LOCK ");fflush(stdout);
		if (!grind_lock_buf()) {
			printf("<--FAIL\n");
			grind_free();
			return 1;
		}
#ifdef GRIND_FAR
		printf("[buf=%x:%x] ",FP_SEG(grind_buf),FP_OFF(grind_buf));
#else
		printf("[buf=%p] ",grind_buf);
#endif

		printf("UNLOCK ");fflush(stdout);
		grind_unlock_buf();

		printf("FREE ");fflush(stdout);
		grind_free_buf();

		printf("OK\n");
	}

	return 0;
}

static int grind_selftest_bufwrite() {
	unsigned int i;

	for (i=0;i < 24;i++) {
		printf("Buffer write... ");fflush(stdout);

		printf("INIT ");fflush(stdout);
		if (!grind_init()) {
			printf("<--FAIL\n");
			grind_free();
			return 1;
		}

		printf("ALLOC ");fflush(stdout);
		if (!grind_alloc_buf()) {
			printf("<--FAIL\n");
			grind_free();
			return 1;
		}
#ifdef GRIND_NEED_CODE_ALIAS_SEL
		printf("[hnd=%x] ",grind_buf_whnd);
#endif

		printf("LOCK ");fflush(stdout);
		if (!grind_lock_buf()) {
			printf("<--FAIL\n");
			grind_free();
			return 1;
		}
#ifdef GRIND_FAR
		printf("[buf=%x:%x] ",FP_SEG(grind_buf),FP_OFF(grind_buf));
#else
		printf("[buf=%p] ",grind_buf);
#endif

		printf("WRITE ");fflush(stdout);
		grind_buf[0] = i;
		grind_buf[1] = i+1;

		printf("UNLOCK ");fflush(stdout);
		grind_unlock_buf();

		printf("FREE ");fflush(stdout);
		grind_free_buf();

		printf("OK\n");
	}

	return 0;
}

static int grind_selftest_nullret() {
	unsigned int i;

	for (i=0;i < 24;i++) {
#ifdef GRIND_FAR
		printf("RETF... ");fflush(stdout);
#else
		printf("RET... ");fflush(stdout);
#endif

		printf("INIT ");fflush(stdout);
		if (!grind_init()) {
			printf("<--FAIL\n");
			grind_free();
			return 1;
		}

		printf("ALLOC ");fflush(stdout);
		if (!grind_alloc_buf()) {
			printf("<--FAIL\n");
			grind_free();
			return 1;
		}

		printf("LOCK ");fflush(stdout);
		if (!grind_lock_buf()) {
			printf("<--FAIL\n");
			grind_free();
			return 1;
		}

		printf("WRITE ");fflush(stdout);
		grind_buf[0] = GRIND_RET_INS;

		printf("EXEC ");fflush(stdout);
		if (!grind_execute_buf()) {
			printf("<--FAIL\n");
			grind_free();
			return 1;
		}

		printf("UNLOCK ");fflush(stdout);
		grind_unlock_buf();

		printf("FREE ");fflush(stdout);
		grind_free_buf();

		printf("OK\n");
	}

	return 0;
}

int main() {
	unsigned int i,j;

	cpu_probe();
	if (!grind_init()) {
		printf("Failed to init grind library\n");
		return 1;
	}

	if (grind_selftest_buffer()) return 1;
	pause_if_tty();
	if (grind_selftest_bufwrite()) return 1;
	pause_if_tty();
	if (grind_selftest_nullret()) return 1;
	pause_if_tty();

	if (!grind_init()) {
		grind_free();
		return 1;
	}
	if (!grind_alloc_buf()) {
		grind_free();
		return 1;
	}
	if (!grind_lock_buf()) {
		grind_free();
		return 1;
	}

	for (i=0;i < 256;i++) {
		printf("\x0D                           \x0D");
		printf("ADD%u %u,%u ",GRIND_REGSIZE,i,j);fflush(stdout);

		for (j=0;j < 256;j++) {
			grind_buf_ptr_t w = grind_buf;

			*w++ = GRIND_INT3_INS;						// INT3
#ifdef GRIND_FAR
			w=grind_buf_w__push_Sreg(w,GRIND_SEG_DS);			// PUSH DS
#endif
			w=grind_buf_w__push_Reg(w,GRIND_REG_AX);			// PUSH (E)AX
			w=grind_buf_w__push_Flags(w);					// PUSHF(D)

			w=grind_buf_w__mov_Reg16_const(w,GRIND_REG_AX,FP_SEG(asm_buf));	// MOV AX,const
			w=grind_buf_w__mov_Seg_from_Reg(w,GRIND_SEG_DS,GRIND_REG_AX);	// MOV <seg>,<reg>

			w=grind_buf_w__mov_Reg_const(w,GRIND_REG_AX,i);			// MOV (E)AX,const
			w=grind_buf_w__mov_Add_const(w,GRIND_REG_AX,j);			// ADD (E)AX,const

			w=grind_buf_w__pop_Flags(w);					// POPF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);				// POP (E)AX
#ifdef GRIND_FAR
			w=grind_buf_w__pop_Sreg(w,GRIND_SEG_DS);			// POP DS
#endif
			*w++ = GRIND_RET_INS;						// RET

			if (!grind_execute_buf()) {
				printf("<--FAIL\n");
				grind_free();
				return 1;
			}
		}
	}
	printf("-- DONE\n");

	grind_unlock_buf();
	grind_free_buf();
	grind_free();
	return 0;
}

