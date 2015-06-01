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

static inline grind_ADDw() {
	unsigned int i,j;

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

	for (i=0;i < 64;i++) {
		for (j=0;j < 64;j++) {
			grind_buf_ptr_t w = grind_buf;

			*((grind_imm_t*)(asm_buf+0)) = 0;					// ADD result
			*((grind_imm_t*)(asm_buf+4)) = 0;					// ADD result (2)
			*((grind_imm_t*)(asm_buf+8)) = 0;					// FLAGS before (after init)
			*((grind_imm_t*)(asm_buf+12)) = 0;					// FLAGS after
			*((grind_imm_t*)(asm_buf+16)) = 0;					// FLAGS before (after init)
			*((grind_imm_t*)(asm_buf+20)) = 0;					// FLAGS after

//			*w++ = GRIND_INT3_INS;							// INT3

			// save DS, EAX, FLAGS
			w=grind_buf_w__push_Sreg(w,GRIND_SEG_DS);				// PUSH DS
			w=grind_buf_w__push_Reg(w,GRIND_REG_AX);				// PUSH (E)AX
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)

			// DS = segment of asm_buf
			w=grind_buf_w__mov_Reg16_const(w,GRIND_REG_AX,FP_SEG(asm_buf));		// MOV AX,const
			w=grind_buf_w__mov_Seg_from_Reg(w,GRIND_SEG_DS,GRIND_REG_AX);		// MOV <seg>,<reg>

			// set EFLAGS to known state
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__and_Reg_const(w,GRIND_REG_AX,~0xAD5);			// AND AX,<mask off CF(0),PF(2),AF(4),ZF(6),SF(7),IF(9),OF(11)>  1010 1101 0101
			w=grind_buf_w__push_Reg(w,GRIND_REG_AX);				// PUSH (E)AX
			w=grind_buf_w__pop_Flags(w);						// POPF(D)
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX

			// store state of EFLAGS now, store result in asm_buf+8
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+8,GRIND_REG_AX);	// MOV [offset],(E)AX

			// asm_buf+0 = i + j, store result in asm_buf+0
			w=grind_buf_w__mov_Reg_const(w,GRIND_REG_AX,i);				// MOV (E)AX,const
			w=grind_buf_w__mov_Add_const(w,GRIND_REG_AX,j);				// ADD (E)AX,const
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+0,GRIND_REG_AX);	// MOV [offset],(E)AX

			// store state of EFLAGS now, in asm_buf+12
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+12,GRIND_REG_AX);	// MOV [offset],(E)AX

			// set EFLAGS to known state
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__or_Reg_const(w,GRIND_REG_AX,0xAD5);			// OR AX,<set CF(0),PF(2),AF(4),ZF(6),SF(7),IF(9),OF(11)>  1010 1101 0101
			w=grind_buf_w__push_Reg(w,GRIND_REG_AX);				// PUSH (E)AX
			w=grind_buf_w__pop_Flags(w);						// POPF(D)
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX

			// store state of EFLAGS now, store result in asm_buf+16
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+16,GRIND_REG_AX);	// MOV [offset],(E)AX

			// asm_buf+0 = i + j, store result in asm_buf+4
			w=grind_buf_w__mov_Reg_const(w,GRIND_REG_AX,i);				// MOV (E)AX,const
			w=grind_buf_w__mov_Add_const(w,GRIND_REG_AX,j);				// ADD (E)AX,const
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+4,GRIND_REG_AX);	// MOV [offset],(E)AX

			// store state of EFLAGS now, store result in asm_buf+20
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+20,GRIND_REG_AX);	// MOV [offset],(E)AX

			// restore FLAGS, EAX, DS, exit subroutine
			w=grind_buf_w__pop_Flags(w);						// POPF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__pop_Sreg(w,GRIND_SEG_DS);				// POP DS
			*w++ = GRIND_RET_INS;							// RET

			// SANITY CHECK
			if (w > (grind_buf + grind_buf_size)) {
				printf("<--BUFFER OVERRUN\n");
				grind_free();
				return 1;
			}

			// EXECUTE IT
			if (!grind_execute_buf()) {
				printf("<--FAIL\n");
				grind_free();
				return 1;
			}

			// note what checked
			{
				// what changed after init, before ADDs
				grind_imm_t chf = *((grind_imm_t*)(asm_buf+8)) ^ *((grind_imm_t*)(asm_buf+16));
				// what changed between ADDs. IF(9) is supposed to change, therefore the ^0x200. ideally chf2 == 0 unless emulation problems.
				grind_imm_t chf2 = *((grind_imm_t*)(asm_buf+12)) ^ *((grind_imm_t*)(asm_buf+20)) ^ 0x200;
				// what flags were changed by both DIV instructions
				grind_imm_t chbf = ((*((grind_imm_t*)(asm_buf+8)) ^ *((grind_imm_t*)(asm_buf+12))) |
					(*((grind_imm_t*)(asm_buf+16)) ^ *((grind_imm_t*)(asm_buf+20)))) &
					chf;

				if (*((grind_imm_t*)(asm_buf+0)) != *((grind_imm_t*)(asm_buf+4))) {
					printf("WARNING: Two ADD passes with different sums 0x%x != 0x%x\n",
						*((grind_imm_t*)(asm_buf+0)),
						*((grind_imm_t*)(asm_buf+4)));
				}

				// NTS: We want to know if EFLAGS differed after both ADD instructions to detect problems.
				//      Executing the ADD instruction twice should show same results and same changes to the flags.
				printf("ADDw %3u + %-3u = %3u\n",i,j,(unsigned int)(*((grind_imm_t*)(asm_buf+0))));
				printf("    zero/set flags that changed:      0x%08x (wanted=0xAD5)\n",chf); // what flags we were able to change vs what we changed
				printf("    ...then after ADD:                0x%08x (^0x200 expect IF change)\n",chf2); // what differed between the two ADD instructions.
				printf("    flag changes (both):              0x%08x\n",chbf); // what was changed by DIV
				printf("    flag result:                      CF%u PF%u AF%u ZF%u SF%u TF%u IF%u DF%u OF%u\n",
					(((unsigned int)(*((grind_imm_t*)(asm_buf+12)))) >> 0) & 1,	// CF
					(((unsigned int)(*((grind_imm_t*)(asm_buf+12)))) >> 2) & 1,	// PF
					(((unsigned int)(*((grind_imm_t*)(asm_buf+12)))) >> 4) & 1,	// AF
					(((unsigned int)(*((grind_imm_t*)(asm_buf+12)))) >> 6) & 1,	// ZF
					(((unsigned int)(*((grind_imm_t*)(asm_buf+12)))) >> 7) & 1,	// SF
					(((unsigned int)(*((grind_imm_t*)(asm_buf+12)))) >> 8) & 1,	// TF
					(((unsigned int)(*((grind_imm_t*)(asm_buf+12)))) >> 9) & 1,	// IF
					(((unsigned int)(*((grind_imm_t*)(asm_buf+12)))) >> 10) & 1,	// DF
					(((unsigned int)(*((grind_imm_t*)(asm_buf+12)))) >> 11) & 1);	// OF
			}
		}
	}

	grind_unlock_buf();
	grind_free_buf();
	grind_free();
	return 0;
}

static inline grind_DIVw() {
	unsigned int i,j;

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

	for (i=0;i < 64;i++) {
		for (j=1;j < 64;j++) { // do NOT divide by zero
			grind_buf_ptr_t w = grind_buf;

			*((grind_imm_t*)(asm_buf+0)) = 0;					// DIV result
			*((grind_imm_t*)(asm_buf+4)) = 0;					// DIV result (2)
			*((grind_imm_t*)(asm_buf+8)) = 0;					// FLAGS before (after init)
			*((grind_imm_t*)(asm_buf+12)) = 0;					// FLAGS after
			*((grind_imm_t*)(asm_buf+16)) = 0;					// FLAGS before (after init)
			*((grind_imm_t*)(asm_buf+20)) = 0;					// FLAGS after
			*((grind_imm_t*)(asm_buf+24)) = 0;					// DIV remainder
			*((grind_imm_t*)(asm_buf+28)) = 0;					// DIV remainder (2)

//			*w++ = GRIND_INT3_INS;							// INT3

			// save DS, EAX, FLAGS
			w=grind_buf_w__push_Sreg(w,GRIND_SEG_DS);				// PUSH DS
			w=grind_buf_w__push_Reg(w,GRIND_REG_AX);				// PUSH (E)AX
			w=grind_buf_w__push_Reg(w,GRIND_REG_BX);				// PUSH (E)BX
			w=grind_buf_w__push_Reg(w,GRIND_REG_DX);				// PUSH (E)DX
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)

			// DS = segment of asm_buf
			w=grind_buf_w__mov_Reg16_const(w,GRIND_REG_AX,FP_SEG(asm_buf));		// MOV AX,const
			w=grind_buf_w__mov_Seg_from_Reg(w,GRIND_SEG_DS,GRIND_REG_AX);		// MOV <seg>,<reg>

			// set EFLAGS to known state
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__and_Reg_const(w,GRIND_REG_AX,~0xAD5);			// AND AX,<mask off CF(0),PF(2),AF(4),ZF(6),SF(7),IF(9),OF(11)>  1010 1101 0101
			w=grind_buf_w__push_Reg(w,GRIND_REG_AX);				// PUSH (E)AX
			w=grind_buf_w__pop_Flags(w);						// POPF(D)
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX

			// store state of EFLAGS now, store result in asm_buf+8
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+8,GRIND_REG_AX);	// MOV [offset],(E)AX

			// asm_buf+0 = i / j, store result in asm_buf+0
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__mov_Reg_const(w,GRIND_REG_AX,i);				// MOV (E)AX,const
			w=grind_buf_w__mov_Reg_const(w,GRIND_REG_BX,j);				// MOV (E)BX,const
			w=grind_buf_w__xor_Reg_Reg(w,GRIND_REG_DX,GRIND_REG_DX);		// XOR (E)DX,(E)DX
			w=grind_buf_w__pop_Flags(w);						// POPF(D)
			w=grind_buf_w__div_Reg(w,GRIND_REG_BX);					// DIV (E)BX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+0,GRIND_REG_AX);	// MOV [offset],(E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+24,GRIND_REG_DX);	// MOV [offset],(E)DX

			// store state of EFLAGS now, in asm_buf+12
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+12,GRIND_REG_AX);	// MOV [offset],(E)AX

			// set EFLAGS to known state
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__or_Reg_const(w,GRIND_REG_AX,0xAD5);			// OR AX,<set CF(0),PF(2),AF(4),ZF(6),SF(7),IF(9),OF(11)>  1010 1101 0101
			w=grind_buf_w__push_Reg(w,GRIND_REG_AX);				// PUSH (E)AX
			w=grind_buf_w__pop_Flags(w);						// POPF(D)
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX

			// store state of EFLAGS now, store result in asm_buf+16
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+16,GRIND_REG_AX);	// MOV [offset],(E)AX

			// asm_buf+0 = i / j, store result in asm_buf+4
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__mov_Reg_const(w,GRIND_REG_AX,i);				// MOV (E)AX,const
			w=grind_buf_w__mov_Reg_const(w,GRIND_REG_BX,j);				// MOV (E)BX,const
			w=grind_buf_w__xor_Reg_Reg(w,GRIND_REG_DX,GRIND_REG_DX);		// XOR (E)DX,(E)DX
			w=grind_buf_w__pop_Flags(w);						// POPF(D)
			w=grind_buf_w__div_Reg(w,GRIND_REG_BX);					// DIV (E)BX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+4,GRIND_REG_AX);	// MOV [offset],(E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+28,GRIND_REG_DX);	// MOV [offset],(E)DX

			// store state of EFLAGS now, store result in asm_buf+20
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+20,GRIND_REG_AX);	// MOV [offset],(E)AX

			// restore FLAGS, EAX, DS, exit subroutine
			w=grind_buf_w__pop_Flags(w);						// POPF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_DX);					// POP (E)DX
			w=grind_buf_w__pop_Reg(w,GRIND_REG_BX);					// POP (E)BX
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__pop_Sreg(w,GRIND_SEG_DS);				// POP DS
			*w++ = GRIND_RET_INS;							// RET

			// SANITY CHECK
			if (w > (grind_buf + grind_buf_size)) {
				printf("<--BUFFER OVERRUN\n");
				grind_free();
				return 1;
			}

			// EXECUTE IT
			if (!grind_execute_buf()) {
				printf("<--FAIL\n");
				grind_free();
				return 1;
			}

			// note what checked
			{
				// what changed after init, before DIVs
				grind_imm_t chf = *((grind_imm_t*)(asm_buf+8)) ^ *((grind_imm_t*)(asm_buf+16));
				// what changed between DIVs. IF(9) is supposed to change, therefore the ^0x200. ideally chf2 == 0 unless emulation problems.
				grind_imm_t chf2 = *((grind_imm_t*)(asm_buf+12)) ^ *((grind_imm_t*)(asm_buf+20)) ^ 0x200;
				// what flags were changed by both DIV instructions
				grind_imm_t chbf = ((*((grind_imm_t*)(asm_buf+8)) ^ *((grind_imm_t*)(asm_buf+12))) |
					(*((grind_imm_t*)(asm_buf+16)) ^ *((grind_imm_t*)(asm_buf+20)))) &
					chf;

				if (*((grind_imm_t*)(asm_buf+0)) != *((grind_imm_t*)(asm_buf+4))) {
					printf("WARNING: Two DIV passes with different results 0x%x != 0x%x\n",
						*((grind_imm_t*)(asm_buf+0)),
						*((grind_imm_t*)(asm_buf+4)));
				}
				if (*((grind_imm_t*)(asm_buf+24)) != *((grind_imm_t*)(asm_buf+28))) {
					printf("WARNING: Two DIV passes with different remainders 0x%x != 0x%x\n",
						*((grind_imm_t*)(asm_buf+24)),
						*((grind_imm_t*)(asm_buf+28)));
				}

				// NTS: We want to know if EFLAGS differed after both DIV instructions to detect problems.
				//      Executing the DIV instruction twice should show same results and same changes to the flags.
				printf("DIVw %3u / %-3u = %3u remainder %u\n",i,j,(unsigned int)(*((grind_imm_t*)(asm_buf+0))),(unsigned int)(*((grind_imm_t*)(asm_buf+24))));
				printf("    zero/set flags that changed:      0x%08x (wanted=0xAD5)\n",chf); // what flags we were able to change vs what we changed
				printf("    ...then after DIV:                0x%08x (^0x200 expect IF change)\n",chf2); // what differed between the two DIV instructions.
				printf("    flag changes (both):              0x%08x\n",chbf); // what was changed by DIV
				printf("    flag result:                      CF%u PF%u AF%u ZF%u SF%u TF%u IF%u DF%u OF%u\n",
					(((unsigned int)(*((grind_imm_t*)(asm_buf+12)))) >> 0) & 1,	// CF
					(((unsigned int)(*((grind_imm_t*)(asm_buf+12)))) >> 2) & 1,	// PF
					(((unsigned int)(*((grind_imm_t*)(asm_buf+12)))) >> 4) & 1,	// AF
					(((unsigned int)(*((grind_imm_t*)(asm_buf+12)))) >> 6) & 1,	// ZF
					(((unsigned int)(*((grind_imm_t*)(asm_buf+12)))) >> 7) & 1,	// SF
					(((unsigned int)(*((grind_imm_t*)(asm_buf+12)))) >> 8) & 1,	// TF
					(((unsigned int)(*((grind_imm_t*)(asm_buf+12)))) >> 9) & 1,	// IF
					(((unsigned int)(*((grind_imm_t*)(asm_buf+12)))) >> 10) & 1,	// DF
					(((unsigned int)(*((grind_imm_t*)(asm_buf+12)))) >> 11) & 1);	// OF
			}
		}
	}

	grind_unlock_buf();
	grind_free_buf();
	grind_free();
	return 0;
}

int main() {
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
	if (grind_DIVw()) return 1;
	pause_if_tty();
	if (grind_ADDw()) return 1;
	pause_if_tty();
	return 0;
}

