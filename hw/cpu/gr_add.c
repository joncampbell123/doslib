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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <malloc.h>
#include <errno.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/cpu/libgrind.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

static unsigned int min_i=0,max_i=511;
static unsigned int min_j=0,max_j=511;

static unsigned char asm_buf[64];

static char *log_filename = NULL;

#if TARGET_MSDOS == 32
static unsigned char log_buf[63*1024];		// 63KB
#else
static unsigned char log_buf[30*1024];		// 30KB
#endif

static int log_reopen_counter=0;
static unsigned int log_bufi=0;
static int log_fd = -1;

static void do_pause() {
	unsigned char c;
	if (isatty(1)) { do { read(0,&c,1); } while (c != 13); }
}

static void log_close();
static int log_reopen();
static int log_open(const char *path);

static void log_incname() {
	unsigned int num;
	char *n;

	if (log_filename == NULL)
		return;

	n = log_filename;
	while (*n && n[1] != 0) n++; // scan until n = last char
	if (*n == 0) return;

	num = ++log_reopen_counter;

	do {
		*n = ((char)(num % 10U)) + '0';
		num /= 10U;
		n--;
		if (n < log_filename) break;
		if (*n == '.') break;
	} while (num != 0U);
}

static void log_flush() {
	if (log_fd >= 0 && log_bufi != 0) {
		unsigned int wd,wr,wo=0;

		do {
			assert(wo < log_bufi);
			wr = log_bufi - wo;
			wd = write(log_fd,log_buf+wo,wr);
			if (/*another way to check for wd == -1*/wd > sizeof(log_buf)) wd = 0;

			// note what we could write
			if (wd > 0) wo += wd;

			if (wd < wr) {
				// make sure DOS flushes to disk
				if (log_fd >= 0) {
					close(log_fd);
					log_fd = -1;
				}

				// strange behavior in MS-DOS 5.0: if there is not enough disk space for X bytes, then it will
				// write 0 bytes and return to us that it wrote 0 bytes. not what I expected, coming from the
				// Linux world that would write as much as it can before giving up. --J.C.
				if (errno == ENOSPC) {
					printf("\nWrite: out of space (%u / %u written)\n",wd,wr);
					printf("Unable to write full log. Swap floppies and hit ENTER or CTRL+C to quit.\n");
					printf("You will have to assemble the full file from fragments when this is done.\n");
					do_pause();

					log_incname();
					if (!log_reopen()) {
						printf("Unable to reopen log.\n");
						exit(1);
					}
				}
				else {
					printf("\nError writing log: %s.\n",strerror(errno));
					exit(1);
				}
			}
			else {
				break;
			}
		} while (wo < log_bufi);
	}

	log_bufi = 0;
}

static void log_lazy_flush(size_t extra) {
	if (log_fd >= 0 && (extra >= sizeof(log_buf) || log_bufi >= (sizeof(log_buf)-extra)))
		log_flush();
}

static void log_close() {
	log_flush();
	if (log_fd >= 0) {
		close(log_fd);
		log_fd = -1;
	}
}

static int log_reopen() {
	if (log_fd >= 0) return 1;
	if (log_filename == NULL) return 0;

	log_fd = open(log_filename,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0644);
	if (log_fd < 0) return 0;

	return 1;
}

static int log_open(const char *path) {
	log_close();

	if (log_filename) {
		free(log_filename);
		log_filename = NULL;
	}

	log_reopen_counter = 0;

	log_fd = open(path,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0644);
	if (log_fd < 0) return 0;

	log_filename = strdup(path);
	return 1;
}

static inline grind_ADDw() {
	const unsigned int FL_mask = 0x8D5U; // CF(0),PF(2),AF(4),ZF(6),SF(7),OF(11)  1000 1101 0101
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

	log_open("gr_add.bin");
	// header
	*((uint32_t*)(log_buf+log_bufi)) = 0x444E5247;			// 'GRND'
	log_bufi += 4;
	*((uint32_t*)(log_buf+log_bufi)) = 0x77444441;			// 'ADDw'
	log_bufi += 4;
	*((uint8_t*)(log_buf+log_bufi)) = sizeof(unsigned int);		// sizeof unsigned int (2 or 4)
	log_bufi += 1;
	*((uint8_t*)(log_buf+log_bufi)) = cpu_flags;			// CPU flags
	log_bufi += 1;
	*((unsigned int*)(log_buf+log_bufi)) = min_i;			// min-max i inclusive
	log_bufi += sizeof(unsigned int);
	*((unsigned int*)(log_buf+log_bufi)) = max_i;			// min-max i inclusive
	log_bufi += sizeof(unsigned int);
	*((unsigned int*)(log_buf+log_bufi)) = min_j;			// min-max j inclusive
	log_bufi += sizeof(unsigned int);
	*((unsigned int*)(log_buf+log_bufi)) = max_j;			// min-max j inclusive
	log_bufi += sizeof(unsigned int);
	*((unsigned int*)(log_buf+log_bufi)) = FL_mask;			// flag test mask
	log_bufi += sizeof(unsigned int);

	log_lazy_flush(32);

	i = min_i;
	do {
		printf("i=%u j=%u..%u",i,min_j,max_j);
		fflush(stdout);

		j = min_j;
		do {
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
			w=grind_buf_w__and_Reg_const(w,GRIND_REG_AX,~FL_mask);			// AND AX,<mask off CF(0),PF(2),AF(4),ZF(6),SF(7),IF(9),OF(11)>  1010 1101 0101
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
			w=grind_buf_w__or_Reg_const(w,GRIND_REG_AX,FL_mask);			// OR AX,<set CF(0),PF(2),AF(4),ZF(6),SF(7),IF(9),OF(11)>  1010 1101 0101
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

			// sanity check
			if (*((grind_imm_t*)(asm_buf+0)) != *((grind_imm_t*)(asm_buf+4))) {
				printf("WARNING: Two ADD passes with different sums 0x%x != 0x%x\n",
					*((grind_imm_t*)(asm_buf+0)),
					*((grind_imm_t*)(asm_buf+4)));
			}

			// log results
			log_lazy_flush(64);
			*((unsigned int*)(log_buf+log_bufi)) = i;				// I
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = j;				// J
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = *((grind_imm_t*)(asm_buf+0));	// I+J
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = *((grind_imm_t*)(asm_buf+8));	// FLAGS before init #1
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = *((grind_imm_t*)(asm_buf+12));	// FLAGS after init #1
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = *((grind_imm_t*)(asm_buf+16));	// FLAGS before init #2
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = *((grind_imm_t*)(asm_buf+20));	// FLAGS after init #2
			log_bufi += sizeof(unsigned int);
			// log entry finish
			log_lazy_flush(16);
		} while ((j++) != max_j);

		printf("\x0D");
		fflush(stdout);
	} while ((i++) != max_i);

	grind_unlock_buf();
	grind_free_buf();
	grind_free();
	log_flush();
	return 0;
}

static inline grind_SUBw() {
	const unsigned int FL_mask = 0x8D5U; // CF(0),PF(2),AF(4),ZF(6),SF(7),OF(11)  1000 1101 0101
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

	log_open("gr_sub.bin");
	// header
	*((uint32_t*)(log_buf+log_bufi)) = 0x444E5247;			// 'GRND'
	log_bufi += 4;
	*((uint32_t*)(log_buf+log_bufi)) = 0x77425553;			// 'SUBw'
	log_bufi += 4;
	*((uint8_t*)(log_buf+log_bufi)) = sizeof(unsigned int);		// sizeof unsigned int (2 or 4)
	log_bufi += 1;
	*((uint8_t*)(log_buf+log_bufi)) = cpu_flags;			// CPU flags
	log_bufi += 1;
	*((unsigned int*)(log_buf+log_bufi)) = min_i;			// min-max i inclusive
	log_bufi += sizeof(unsigned int);
	*((unsigned int*)(log_buf+log_bufi)) = max_i;			// min-max i inclusive
	log_bufi += sizeof(unsigned int);
	*((unsigned int*)(log_buf+log_bufi)) = min_j;			// min-max j inclusive
	log_bufi += sizeof(unsigned int);
	*((unsigned int*)(log_buf+log_bufi)) = max_j;			// min-max j inclusive
	log_bufi += sizeof(unsigned int);
	*((unsigned int*)(log_buf+log_bufi)) = FL_mask;			// flag test mask
	log_bufi += sizeof(unsigned int);

	log_lazy_flush(32);

	i = min_i;
	do {
		printf("i=%u j=%u..%u",i,min_j,max_j);
		fflush(stdout);

		j = min_j;
		do {
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
			w=grind_buf_w__and_Reg_const(w,GRIND_REG_AX,~FL_mask);			// AND AX,<mask off CF(0),PF(2),AF(4),ZF(6),SF(7),IF(9),OF(11)>  1010 1101 0101
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
			w=grind_buf_w__mov_Sub_const(w,GRIND_REG_AX,j);				// SUB (E)AX,const
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+0,GRIND_REG_AX);	// MOV [offset],(E)AX

			// store state of EFLAGS now, in asm_buf+12
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+12,GRIND_REG_AX);	// MOV [offset],(E)AX

			// set EFLAGS to known state
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__or_Reg_const(w,GRIND_REG_AX,FL_mask);			// OR AX,<set CF(0),PF(2),AF(4),ZF(6),SF(7),IF(9),OF(11)>  1010 1101 0101
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
			w=grind_buf_w__mov_Sub_const(w,GRIND_REG_AX,j);				// SUB (E)AX,const
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

			// sanity check
			if (*((grind_imm_t*)(asm_buf+0)) != *((grind_imm_t*)(asm_buf+4))) {
				printf("WARNING: Two SUB passes with different results 0x%x != 0x%x\n",
					*((grind_imm_t*)(asm_buf+0)),
					*((grind_imm_t*)(asm_buf+4)));
			}

			// log results
			log_lazy_flush(64);
			*((unsigned int*)(log_buf+log_bufi)) = i;				// I
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = j;				// J
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = *((grind_imm_t*)(asm_buf+0));	// I-J
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = *((grind_imm_t*)(asm_buf+8));	// FLAGS before init #1
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = *((grind_imm_t*)(asm_buf+12));	// FLAGS after init #1
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = *((grind_imm_t*)(asm_buf+16));	// FLAGS before init #2
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = *((grind_imm_t*)(asm_buf+20));	// FLAGS after init #2
			log_bufi += sizeof(unsigned int);
			// log entry finish
			log_lazy_flush(16);
		} while ((j++) != max_j);

		printf("\x0D");
		fflush(stdout);
	} while ((i++) != max_i);

	grind_unlock_buf();
	grind_free_buf();
	grind_free();
	log_flush();
	return 0;
}

static inline grind_MULw() {
	const unsigned int FL_mask = 0x8D5U; // CF(0),PF(2),AF(4),ZF(6),SF(7),OF(11)  1000 1101 0101
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

	log_open("gr_mul.bin");
	// header
	*((uint32_t*)(log_buf+log_bufi)) = 0x444E5247;			// 'GRND'
	log_bufi += 4;
	*((uint32_t*)(log_buf+log_bufi)) = 0x774C554D;			// 'MULw'
	log_bufi += 4;
	*((uint8_t*)(log_buf+log_bufi)) = sizeof(unsigned int);		// sizeof unsigned int (2 or 4)
	log_bufi += 1;
	*((uint8_t*)(log_buf+log_bufi)) = cpu_flags;			// CPU flags
	log_bufi += 1;
	*((unsigned int*)(log_buf+log_bufi)) = min_i;			// min-max i inclusive
	log_bufi += sizeof(unsigned int);
	*((unsigned int*)(log_buf+log_bufi)) = max_i;			// min-max i inclusive
	log_bufi += sizeof(unsigned int);
	*((unsigned int*)(log_buf+log_bufi)) = min_j;			// min-max j inclusive
	log_bufi += sizeof(unsigned int);
	*((unsigned int*)(log_buf+log_bufi)) = max_j;			// min-max j inclusive
	log_bufi += sizeof(unsigned int);
	*((unsigned int*)(log_buf+log_bufi)) = FL_mask;			// flag test mask
	log_bufi += sizeof(unsigned int);

	log_lazy_flush(32);

	i = min_i;
	do {
		printf("i=%u j=%u..%u",i,min_j,max_j);
		fflush(stdout);

		j = min_j;
		do {
			grind_buf_ptr_t w = grind_buf;

			*((grind_imm_t*)(asm_buf+0)) = 0;					// MUL result (AX)
			*((grind_imm_t*)(asm_buf+4)) = 0;					// MUL result (DX)
			*((grind_imm_t*)(asm_buf+8)) = 0;					// MUL result 2 (AX)
			*((grind_imm_t*)(asm_buf+12)) = 0;					// MUL result 2 (DX)
			*((grind_imm_t*)(asm_buf+16)) = 0;					// FLAGS before (after init)
			*((grind_imm_t*)(asm_buf+20)) = 0;					// FLAGS after
			*((grind_imm_t*)(asm_buf+24)) = 0;					// FLAGS before (after init)
			*((grind_imm_t*)(asm_buf+28)) = 0;					// FLAGS after

//			*w++ = GRIND_INT3_INS;							// INT3

			// save DS, EAX, FLAGS
			w=grind_buf_w__push_Sreg(w,GRIND_SEG_DS);				// PUSH DS
			w=grind_buf_w__push_Reg(w,GRIND_REG_AX);				// PUSH (E)AX
			w=grind_buf_w__push_Reg(w,GRIND_REG_BX);				// PUSH (E)BX
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)

			// DS = segment of asm_buf
			w=grind_buf_w__mov_Reg16_const(w,GRIND_REG_AX,FP_SEG(asm_buf));		// MOV AX,const
			w=grind_buf_w__mov_Seg_from_Reg(w,GRIND_SEG_DS,GRIND_REG_AX);		// MOV <seg>,<reg>

			// set EFLAGS to known state
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__and_Reg_const(w,GRIND_REG_AX,~FL_mask);			// AND AX,<mask off CF(0),PF(2),AF(4),ZF(6),SF(7),IF(9),OF(11)>  1010 1101 0101
			w=grind_buf_w__push_Reg(w,GRIND_REG_AX);				// PUSH (E)AX
			w=grind_buf_w__pop_Flags(w);						// POPF(D)
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX

			// store state of EFLAGS now, store result in asm_buf+16
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+16,GRIND_REG_AX);	// MOV [offset],(E)AX

			// asm_buf+0 = i + j, store result in asm_buf+0
			w=grind_buf_w__mov_Reg_const(w,GRIND_REG_AX,i);				// MOV (E)AX,const
			w=grind_buf_w__mov_Reg_const(w,GRIND_REG_BX,j);				// MOV (E)BX,const
			w=grind_buf_w__mul_Reg(w,GRIND_REG_BX);					// MUL (E)BX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+0,GRIND_REG_AX);	// MOV [offset],(E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+4,GRIND_REG_DX);	// MOV [offset],(E)AX

			// store state of EFLAGS now, in asm_buf+20
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+20,GRIND_REG_AX);	// MOV [offset],(E)AX

			// set EFLAGS to known state
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__or_Reg_const(w,GRIND_REG_AX,FL_mask);			// OR AX,<set CF(0),PF(2),AF(4),ZF(6),SF(7),IF(9),OF(11)>  1010 1101 0101
			w=grind_buf_w__push_Reg(w,GRIND_REG_AX);				// PUSH (E)AX
			w=grind_buf_w__pop_Flags(w);						// POPF(D)
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX

			// store state of EFLAGS now, store result in asm_buf+24
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+24,GRIND_REG_AX);	// MOV [offset],(E)AX

			// asm_buf+0 = i + j, store result in asm_buf+4
			w=grind_buf_w__mov_Reg_const(w,GRIND_REG_AX,i);				// MOV (E)AX,const
			w=grind_buf_w__mov_Reg_const(w,GRIND_REG_BX,j);				// MOV (E)BX,const
			w=grind_buf_w__mul_Reg(w,GRIND_REG_BX);					// MUL (E)BX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+8,GRIND_REG_AX);	// MOV [offset],(E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+12,GRIND_REG_DX);	// MOV [offset],(E)DX

			// store state of EFLAGS now, store result in asm_buf+28
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+28,GRIND_REG_AX);	// MOV [offset],(E)AX

			// restore FLAGS, EAX, DS, exit subroutine
			w=grind_buf_w__pop_Flags(w);						// POPF(D)
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

			// sanity check
			if (*((grind_imm_t*)(asm_buf+0)) != *((grind_imm_t*)(asm_buf+8)) ||
				*((grind_imm_t*)(asm_buf+4)) != *((grind_imm_t*)(asm_buf+12))) {
				printf("WARNING: Two MUL passes with different products %x:%x != %x:%x\n",
					*((grind_imm_t*)(asm_buf+4)),	*((grind_imm_t*)(asm_buf+0)),
					*((grind_imm_t*)(asm_buf+12)),	*((grind_imm_t*)(asm_buf+8)));
			}

			// log results
			log_lazy_flush(64);
			*((unsigned int*)(log_buf+log_bufi)) = i;				// I
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = j;				// J
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = *((grind_imm_t*)(asm_buf+0));	// I*J (low word)
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = *((grind_imm_t*)(asm_buf+4));	// I*J (high word)
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = *((grind_imm_t*)(asm_buf+16));	// FLAGS before init #1
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = *((grind_imm_t*)(asm_buf+20));	// FLAGS after init #1
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = *((grind_imm_t*)(asm_buf+24));	// FLAGS before init #2
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = *((grind_imm_t*)(asm_buf+28));	// FLAGS after init #2
			log_bufi += sizeof(unsigned int);
			// log entry finish
			log_lazy_flush(16);
		} while ((j++) != max_j);

		printf("\x0D");
		fflush(stdout);
	} while ((i++) != max_i);

	grind_unlock_buf();
	grind_free_buf();
	grind_free();
	log_flush();
	return 0;
}

static inline grind_DIVw() {
	const unsigned int FL_mask = 0x8D5U; // CF(0),PF(2),AF(4),ZF(6),SF(7),OF(11)  1000 1101 0101
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

	log_open("gr_div.bin");
	// header
	*((uint32_t*)(log_buf+log_bufi)) = 0x444E5247;			// 'GRND'
	log_bufi += 4;
	*((uint32_t*)(log_buf+log_bufi)) = 0x77564944;			// 'DIVw'
	log_bufi += 4;
	*((uint8_t*)(log_buf+log_bufi)) = sizeof(unsigned int);		// sizeof unsigned int (2 or 4)
	log_bufi += 1;
	*((uint8_t*)(log_buf+log_bufi)) = cpu_flags;			// CPU flags
	log_bufi += 1;
	*((unsigned int*)(log_buf+log_bufi)) = min_i;			// min-max i inclusive
	log_bufi += sizeof(unsigned int);
	*((unsigned int*)(log_buf+log_bufi)) = max_i;			// min-max i inclusive
	log_bufi += sizeof(unsigned int);
	*((unsigned int*)(log_buf+log_bufi)) = min_j;			// min-max j inclusive
	log_bufi += sizeof(unsigned int);
	*((unsigned int*)(log_buf+log_bufi)) = max_j;			// min-max j inclusive
	log_bufi += sizeof(unsigned int);
	*((unsigned int*)(log_buf+log_bufi)) = FL_mask;			// flag test mask
	log_bufi += sizeof(unsigned int);

	log_lazy_flush(32);

	i = min_i;
	do {
		printf("i=%u j=%u..%u",i,min_j,max_j);
		fflush(stdout);

		j = min_j;
		do {
			grind_buf_ptr_t w = grind_buf;

			*((grind_imm_t*)(asm_buf+0)) = 0;					// DIV result (AX)
			*((grind_imm_t*)(asm_buf+4)) = 0;					// DIV result (DX)
			*((grind_imm_t*)(asm_buf+8)) = 0;					// DIV result 2 (AX)
			*((grind_imm_t*)(asm_buf+12)) = 0;					// DIV result 2 (DX)
			*((grind_imm_t*)(asm_buf+16)) = 0;					// FLAGS before (after init)
			*((grind_imm_t*)(asm_buf+20)) = 0;					// FLAGS after
			*((grind_imm_t*)(asm_buf+24)) = 0;					// FLAGS before (after init)
			*((grind_imm_t*)(asm_buf+28)) = 0;					// FLAGS after

			// do NOT divide by zero!
			if (j != 0) {
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
			w=grind_buf_w__and_Reg_const(w,GRIND_REG_AX,~FL_mask);			// AND AX,<mask off CF(0),PF(2),AF(4),ZF(6),SF(7),IF(9),OF(11)>  1010 1101 0101
			w=grind_buf_w__push_Reg(w,GRIND_REG_AX);				// PUSH (E)AX
			w=grind_buf_w__pop_Flags(w);						// POPF(D)
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX

			// store state of EFLAGS now, store result in asm_buf+16
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+16,GRIND_REG_AX);	// MOV [offset],(E)AX

			// asm_buf+0 = i + j, store result in asm_buf+0
			w=grind_buf_w__mov_Reg_const(w,GRIND_REG_AX,i);				// MOV (E)AX,const
			w=grind_buf_w__mov_Reg_const(w,GRIND_REG_BX,j);				// MOV (E)BX,const
			w=grind_buf_w__xor_Reg_Reg(w,GRIND_REG_DX,GRIND_REG_DX);		// XOR (E)DX,(E)DX
			w=grind_buf_w__div_Reg(w,GRIND_REG_BX);					// DIV (E)BX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+0,GRIND_REG_AX);	// MOV [offset],(E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+4,GRIND_REG_DX);	// MOV [offset],(E)AX

			// store state of EFLAGS now, in asm_buf+20
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+20,GRIND_REG_AX);	// MOV [offset],(E)AX

			// set EFLAGS to known state
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__or_Reg_const(w,GRIND_REG_AX,FL_mask);			// OR AX,<set CF(0),PF(2),AF(4),ZF(6),SF(7),IF(9),OF(11)>  1010 1101 0101
			w=grind_buf_w__push_Reg(w,GRIND_REG_AX);				// PUSH (E)AX
			w=grind_buf_w__pop_Flags(w);						// POPF(D)
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX

			// store state of EFLAGS now, store result in asm_buf+24
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+24,GRIND_REG_AX);	// MOV [offset],(E)AX

			// asm_buf+0 = i + j, store result in asm_buf+4
			w=grind_buf_w__mov_Reg_const(w,GRIND_REG_AX,i);				// MOV (E)AX,const
			w=grind_buf_w__mov_Reg_const(w,GRIND_REG_BX,j);				// MOV (E)BX,const
			w=grind_buf_w__xor_Reg_Reg(w,GRIND_REG_DX,GRIND_REG_DX);		// XOR (E)DX,(E)DX
			w=grind_buf_w__div_Reg(w,GRIND_REG_BX);					// DIV (E)BX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+8,GRIND_REG_AX);	// MOV [offset],(E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+12,GRIND_REG_DX);	// MOV [offset],(E)DX

			// store state of EFLAGS now, store result in asm_buf+28
			w=grind_buf_w__push_Flags(w);						// PUSHF(D)
			w=grind_buf_w__pop_Reg(w,GRIND_REG_AX);					// POP (E)AX
			w=grind_buf_w__mov_memoff_from_reg(w,FP_OFF(asm_buf)+28,GRIND_REG_AX);	// MOV [offset],(E)AX

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

			// sanity check
			if (*((grind_imm_t*)(asm_buf+0)) != *((grind_imm_t*)(asm_buf+8)) ||
				*((grind_imm_t*)(asm_buf+4)) != *((grind_imm_t*)(asm_buf+12))) {
				printf("WARNING: Two DIV passes with different results (rem:res) %x:%x != %x:%x\n",
					*((grind_imm_t*)(asm_buf+4)),	*((grind_imm_t*)(asm_buf+0)),
					*((grind_imm_t*)(asm_buf+12)),	*((grind_imm_t*)(asm_buf+8)));
			}
			}//end j != 0

			// log results
			log_lazy_flush(64);
			*((unsigned int*)(log_buf+log_bufi)) = i;				// I
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = j;				// J
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = *((grind_imm_t*)(asm_buf+0));	// I/J (result)
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = *((grind_imm_t*)(asm_buf+4));	// I%J (remainder)
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = *((grind_imm_t*)(asm_buf+16));	// FLAGS before init #1
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = *((grind_imm_t*)(asm_buf+20));	// FLAGS after init #1
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = *((grind_imm_t*)(asm_buf+24));	// FLAGS before init #2
			log_bufi += sizeof(unsigned int);
			*((unsigned int*)(log_buf+log_bufi)) = *((grind_imm_t*)(asm_buf+28));	// FLAGS after init #2
			log_bufi += sizeof(unsigned int);
			// log entry finish
			log_lazy_flush(16);
		} while ((j++) != max_j);

		printf("\x0D");
		fflush(stdout);
	} while ((i++) != max_i);

	grind_unlock_buf();
	grind_free_buf();
	grind_free();
	log_flush();
	return 0;
}

int main() {
	cpu_probe();

	printf("Testing ADDw x+y (%u <= x <= %u)+(%u <= y <= %u)\n",min_i,max_i,min_j,max_j);
	grind_ADDw();
	log_close();

	printf("Testing SUBw x-y (%u <= x <= %u)+(%u <= y <= %u)\n",min_i,max_i,min_j,max_j);
	grind_SUBw();
	log_close();

	printf("Testing MULw x*y (%u <= x <= %u)+(%u <= y <= %u)\n",min_i,max_i,min_j,max_j);
	grind_MULw();
	log_close();

	printf("Testing DIVw x*y (%u <= x <= %u)+(%u <= y <= %u)\n",min_i,max_i,min_j,max_j);
	grind_DIVw();
	log_close();
	return 0;
}

