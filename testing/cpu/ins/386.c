/* 386.c
 *
 * 80386 CPU instruction and functionality test for MS-DOS.
 * (C) 2026 Jonathan Campbell, DOSLIB project.
 *
 * Tests the 386 in real mode, protected mode and virtual 8086 mode, with
 * 16-bit, 32-bit and mixed 16/32-bit code:
 *
 *   Real mode:   32-bit operands and addressing through the 66h/67h prefixes,
 *                every new 386 instruction (MOVZX/MOVSX, SHLD/SHRD, BT*,
 *                BSF/BSR, SETcc, Jcc rel16/32, IMUL forms, CWDE/CDQ, FS/GS,
 *                LFS/LGS/LSS, PUSHAD/POPAD, PUSHFD/POPFD, 32-bit string ops,
 *                LOOP/JECXZ with ECX, ENTER/CALL/RET/IRETD with 32-bit
 *                operands), ModR/M+SIB effective addresses, the 64KB offset
 *                limit in real mode, control and debug registers, hardware
 *                breakpoints, and the 386 vs 486 identification.
 *   Protected:   32-bit and 16-bit code segments and the transfers between
 *                them, 16/32-bit stacks, the granularity bit, expand-down
 *                segments, 386 and 286 gates, exceptions and error codes, ring
 *                3 with IOPL and the I/O permission bitmap, call gates with
 *                parameters, conforming code, 386 TSS task switching, paging
 *                (translation, #PF, CR2, error codes, accessed/dirty bits) and
 *                debug registers.
 *   V86 mode:    entry with IRETD, exit through interrupts, the stack frame,
 *                IOPL sensitive instructions, the I/O bitmap, and the
 *                instructions that are not allowed.
 *   Unreal mode: segment limits kept after returning to real mode.
 *
 * The register runner, the real/protected mode switch and all protected mode
 * and V86 code are generated machine code. The 386 returns to real mode by
 * clearing CR0.PE (no CPU reset needed). Every exception in protected mode is
 * recorded and resumes at the next test case.
 *
 * Requires a 386 or later. The protected mode and V86 tests require real
 * mode (not virtual 8086 mode under EMM386 etc). 16-bit large memory model
 * only. The 32-bit build is a stub.
 *
 * Results are also written to 386.TXT in the current directory. The file is
 * committed to disk (fflush + fsync) each time a screen page of results is
 * complete, and before each trip into protected mode.
 *
 * Command line: -n   do not wait for a key at the end */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <conio.h>
#include <dos.h>
#include <io.h>

#if TARGET_MSDOS == 32

int main(int argc,char **argv) {
	(void)argc;
	(void)argv;
	printf("No test for this build\n");
	return 0;
}

#else

/* ---------------------------------------------------------------- flags */
#define CF		0x0001
#define PF		0x0004
#define AF		0x0010
#define ZF		0x0040
#define SF		0x0080
#define TF		0x0100
#define IF		0x0200
#define DF		0x0400
#define OF		0x0800
#define ARITH		(CF|PF|AF|ZF|SF|OF)
#define CTRL		(DF|IF|TF)		/* always compared unless the test says otherwise */

/* register index, same order as the x86 reg16 encoding */
#define R_AX		0
#define R_CX		1
#define R_DX		2
#define R_BX		3
#define R_SP		4
#define R_BP		5
#define R_SI		6
#define R_DI		7
#define R_ES		8
#define R_FL		9
#define R_MAX		10

/* -------------------------------------------------------- global state */
unsigned short			ri[R_MAX];	/* registers in */
unsigned short			ro[R_MAX];	/* registers out */
unsigned short			re[R_MAX];	/* registers expected */
unsigned short			sp_before,sp_after;
void				(__far *thunk_ptr)(void);

static unsigned char __far	thunk[1024];	/* generated code */
static unsigned char __far	*tp;		/* thunk write pointer */
#define THUNK_DATA		0x380		/* CS: data area within thunk */

static unsigned short		memw[256];	/* DGROUP memory operand area */
#define memb			((unsigned char*)memw)

static unsigned short		dgroup,ss_val,thunk_seg,memw_off;

/* exception/interrupt capture, written by the interrupt handlers */
volatile unsigned short		x_cnt,x_vec,x_ip,x_cs,x_fl,x_live,x_resume;
volatile unsigned short		ss_cnt,ss_ip[16];
volatile unsigned short		x_faulted;	/* a fault handler resumed at the end of the thunk */
static unsigned char		x_unexpected;	/* ... and the test did not expect it */
static unsigned char		exc_expected = 0;	/* the test expects a fault, verify() should not flag it */

/* CPU detection results */
static unsigned char		c_flagsF;	/* FLAGS bits 12-15 stuck at 1 (8086/186/V20 class) */
static unsigned char		c_shmask;	/* shift count masked to 5 bits */
static unsigned char		c_necaad;	/* AAD ignores immediate (NEC V20/V30) */
static unsigned char		c_386;		/* 386 or later */
static unsigned char		c_v86;		/* MSW.PE set (virtual 8086 mode) */
static unsigned char		is8086;		/* plain Intel 8086/8088 */
static unsigned char		is286p;		/* 286 or later */
static char			cpu_name[48];

static unsigned char		pc98 = 0;
static FILE*			logf = NULL;

/* ------------------------------------------------------------- display */
enum { COL_NORM=0, COL_TITLE, COL_PASS, COL_FAIL, COL_INFO, COL_SKIP, COL_RUN, COL_MAX };

static const unsigned char	attr_color[COL_MAX] = { 0x07, 0x1F, 0x0A, 0x0C, 0x0B, 0x08, 0x0E };
static const unsigned char	attr_mono[COL_MAX]  = { 0x07, 0x70, 0x07, 0x0F, 0x0F, 0x07, 0x01 };
static const unsigned char	attr_pc98[COL_MAX]  = { 0xE1, 0xE5, 0x81, 0x41, 0xA1, 0x21, 0xC1 };	/* GRB color, bit 0 = visible */

static unsigned short __far	*vram_c;	/* IBM: char+attr, PC-98: char codes */
static unsigned short __far	*vram_a;	/* PC-98: attributes */
static const unsigned char	*attr_tab;
static unsigned char		ibm_mono = 0;
static unsigned char		pc98_fkey = 0;

static void put_ch(unsigned x,unsigned y,int col,char c) {
	unsigned o;

	if (x >= 80 || y >= 25) return;
	o = (y * 80u) + x;
	if (pc98) {
		vram_c[o] = (unsigned char)c;
		vram_a[o] = attr_tab[col];
	}
	else {
		vram_c[o] = (unsigned char)c | ((unsigned short)attr_tab[col] << 8u);
	}
}

static void put_str(unsigned x,unsigned y,int col,const char *s) {
	while (*s && x < 80) put_ch(x++,y,col,*s++);
}

static void put_strpad(unsigned x,unsigned y,unsigned w,int col,const char *s) {
	while (w > 0 && x < 80) { put_ch(x++,y,col,*s ? *s++ : ' '); w--; }
}

static void dos_puts(const char *s) {
	union REGS r;

	while (*s) {
		r.h.ah = 0x02;
		r.h.dl = (unsigned char)(*s++);
		int86(0x21,&r,&r);
	}
}

static void video_init(void) {
	union REGS r;
	unsigned i;

	if (pc98) {
		r.h.ah = 0x0A; r.h.al = 0x00; int86(0x18,&r,&r);	/* 80x25 text */
		r.h.ah = 0x0C; int86(0x18,&r,&r);			/* text layer on */
		r.h.ah = 0x12; int86(0x18,&r,&r);			/* cursor off */
		pc98_fkey = *((unsigned char __far*)MK_FP(0x60,0x111));	/* function key row shown? */
		if (pc98_fkey) dos_puts("\x1B[>1h");
		vram_c = (unsigned short __far*)MK_FP(0xA000,0);
		vram_a = (unsigned short __far*)MK_FP(0xA200,0);
		attr_tab = attr_pc98;
	}
	else {
		int86(0x11,&r,&r);
		ibm_mono = ((r.x.ax & 0x30) == 0x30) ? 1 : 0;		/* 80x25 monochrome initial video mode */
		r.x.ax = ibm_mono ? 0x0007 : 0x0003; int86(0x10,&r,&r);
		r.h.ah = 0x01; r.x.cx = 0x2000; int86(0x10,&r,&r);	/* cursor off */
		vram_c = (unsigned short __far*)MK_FP(ibm_mono ? 0xB000 : 0xB800,0);
		attr_tab = ibm_mono ? attr_mono : attr_color;
	}

	for (i=0;i < 80u*25u;i++) put_ch(i%80u,i/80u,COL_NORM,' ');
}

static void video_exit(void) {
	union REGS r;

	if (pc98) {
		r.h.ah = 0x11; int86(0x18,&r,&r);			/* cursor on */
		if (pc98_fkey) dos_puts("\x1B[>1l");
		dos_puts(pc98_fkey ? "\x1B[24;1H" : "\x1B[25;1H");
	}
	else {
		r.h.ah = 0x01; r.x.cx = ibm_mono ? 0x0B0C : 0x0607; int86(0x10,&r,&r);
		r.h.ah = 0x02; r.h.bh = 0; r.x.dx = 0x1800; int86(0x10,&r,&r);	/* row 24 col 0 */
	}
}

/* ----------------------------------------------------------- test groups */
#define GRID_TOP	2
#define GRID_ROWS	20
#define GRID_COLS	4
#define CELL_W		20
#define LOG_FAILS	10

static const char*	grp_name;
static unsigned		grp_cell = 0;
static unsigned long	grp_tests;
static unsigned		grp_fails;
static unsigned char	grp_info,grp_skip;
static unsigned		total_pass = 0,total_fail = 0,total_info = 0,total_skip = 0;
static char		diffbuf[256];
static char		linebuf[256];

static void cell_draw(int col,const char *status) {
	unsigned x = (grp_cell / GRID_ROWS) * CELL_W,y = GRID_TOP + (grp_cell % GRID_ROWS);

	put_strpad(x,y,11,COL_NORM,grp_name);
	put_strpad(x+12,y,7,col,status);
}

static void log_commit(void) {
	if (logf) {
		fflush(logf);
		if (fsync(fileno(logf)) != 0) {
			/* DOS before 3.30 has no commit call. Closing the file updates the directory entry */
			fclose(logf);
			logf = fopen("386.TXT","a");
		}
	}
}

/* screen page full: commit results to disk, then clear the grid for the next page */
static void page_done(void) {
	unsigned x,y;

	log_commit();
	for (y=GRID_TOP;y < GRID_TOP+GRID_ROWS;y++) {
		for (x=0;x < 80;x++) put_ch(x,y,COL_NORM,' ');
	}
	grp_cell = 0;
}

static void begin(const char *name) {
	grp_name = name;
	grp_tests = 0;
	grp_fails = 0;
	grp_info = 0;
	grp_skip = 0;
	cell_draw(COL_RUN,"...");
}

static void end(void) {
	char tmp[16];
	int col;

	if (grp_fails) {
		sprintf(tmp,"FAIL%3u",grp_fails > 999 ? 999 : grp_fails);
		col = COL_FAIL;
		total_fail++;
	}
	else if (grp_skip) {
		strcpy(tmp,"SKIP");
		col = COL_SKIP;
		total_skip++;
	}
	else if (grp_info) {
		strcpy(tmp,"INFO");
		col = COL_INFO;
		total_info++;
	}
	else {
		strcpy(tmp,"PASS");
		col = COL_PASS;
		total_pass++;
	}

	cell_draw(col,tmp);
	if (logf) {
		fprintf(logf,"%-12s %-8s tests=%lu fails=%u\n",grp_name,tmp,grp_tests,grp_fails);
		fflush(logf);
	}
	if (++grp_cell >= GRID_ROWS * GRID_COLS) page_done();
}

static void show_bottom(unsigned y,int col,const char *s) {
	put_strpad(0,y,80,col,s);
}

static void failv(int with_diff,const char *fmt,va_list va) {
	grp_fails++;
	if (grp_fails <= LOG_FAILS) {
		vsprintf(linebuf,fmt,va);
		if (logf) fprintf(logf,"  FAIL %s: %s%s\n",grp_name,linebuf,with_diff ? diffbuf : "");
		if (grp_fails == 1) {
			char tmp[96];

			sprintf(tmp,"FAIL %s: ",grp_name);
			strncat(tmp,linebuf,sizeof(tmp)-1-strlen(tmp));
			show_bottom(23,COL_FAIL,tmp);
			show_bottom(24,COL_FAIL,with_diff ? diffbuf : "");
		}
	}
}

/* failure, message only */
static void failm(const char *fmt,...) {
	va_list va;
	va_start(va,fmt);
	failv(0,fmt,va);
	va_end(va);
}

static void info(const char *fmt,...) {
	va_list va;
	char tmp[96];

	grp_info = 1;
	va_start(va,fmt);
	vsprintf(linebuf,fmt,va);
	va_end(va);
	if (logf) fprintf(logf,"  INFO %s: %s\n",grp_name,linebuf);
	sprintf(tmp,"INFO %s: ",grp_name);
	strncat(tmp,linebuf,sizeof(tmp)-1-strlen(tmp));
	show_bottom(22,COL_INFO,tmp);
}

/* count a check that is not done through verify() */
static int checkm(int ok) {
	grp_tests++;
	return ok;
}

/* ------------------------------------------------------- thunk emitter */
static unsigned here(void) {
	return FP_OFF(tp);
}

static void E(unsigned b) {
	*tp++ = (unsigned char)b;
}

static void EW(unsigned w) {
	E(w & 0xFF);
	E(w >> 8);
}

static void EB(unsigned n,...) {
	va_list va;

	va_start(va,n);
	while (n-- > 0) E(va_arg(va,unsigned));
	va_end(va);
}

/* ----------------------------------------------------------- the runner */
static void run(void) {
	__asm {
		push	bp
		push	si
		push	di
		push	es
		push	ds
		mov	ax,seg ri			; Watcom does not keep DS = DGROUP in the large model
		mov	ds,ax
		mov	ax,word ptr ri+16
		mov	es,ax
		mov	cx,word ptr ri+2
		mov	dx,word ptr ri+4
		mov	bx,word ptr ri+6
		mov	bp,word ptr ri+10
		mov	si,word ptr ri+12
		mov	di,word ptr ri+14
		push	word ptr ri+18
		mov	ax,word ptr ri+0
		popf
		mov	sp_before,sp
		call	dword ptr thunk_ptr
		mov	sp_after,sp
		pushf
		cld
		sti
		mov	word ptr ro+0,ax
		mov	word ptr ro+2,cx
		mov	word ptr ro+4,dx
		mov	word ptr ro+6,bx
		mov	word ptr ro+10,bp
		mov	word ptr ro+12,si
		mov	word ptr ro+14,di
		mov	ax,es
		mov	word ptr ro+16,ax
		pop	word ptr ro+18
		pop	ds
		pop	es
		pop	di
		pop	si
		pop	bp
	}
}

/* default register state, reset the thunk */
static void setup(void) {
	ri[R_AX] = 0x1A2B; ri[R_CX] = 0x3C4D; ri[R_DX] = 0x5E6F; ri[R_BX] = 0x7081;
	ri[R_SP] = 0;      ri[R_BP] = 0x92A3; ri[R_SI] = 0xB4C5; ri[R_DI] = 0xD6E7;
	ri[R_ES] = dgroup; ri[R_FL] = 0x0202;
	tp = thunk;
}

/* terminate the thunk with RETF, run it, expected state = input state.
 * The RETF is also where a fault (#DE, BOUND, #UD, #GP) resumes, so that an
 * instruction the CPU or emulator does not implement is reported instead of hanging */
static void go(void) {
	x_resume = here();
	x_faulted = 0;
	E(0xCB);
	memcpy(re,ri,sizeof(re));
	run();
	x_resume = 0;
	x_unexpected = (x_faulted && !exc_expected) ? 1 : 0;
}


/* compare ro against re. fmask selects which FLAGS bits to compare */
/* ---------------------------------------------------- CPU detection */
static void detect_cpu(void) {
	setup();
	EB(9, 0x9C,0x58,0x25,0xFF,0x0F,0x50,0x9D,0x9C,0x58);	/* pushf; pop ax; and ax,0FFFh; push ax; popf; pushf; pop ax */
	go();
	c_flagsF = ((ro[R_AX] & 0xF000u) == 0xF000u) ? 1 : 0;

	if (!c_flagsF) {
		is286p = 1;
		setup();
		EB(3, 0x0F,0x01,0xE0);					/* smsw ax */
		go();
		c_v86 = (ro[R_AX] & 1) ? 1 : 0;
		if (c_v86) {
			c_386 = 1;					/* a 286 cannot run DOS in protected mode */
		}
		else {
			setup();
			EB(9, 0xB8,0x02,0x72,0x50,0x9D,0x9C,0x58,0x9D,0x90);	/* mov ax,7202h; push ax; popf; pushf; pop ax */
			tp -= 2;
			go();
			c_386 = (ro[R_AX] & 0x7000u) ? 1 : 0;
		}
	}

	setup();
	EB(7, 0xB8,0x01,0x00,0xB1,0x21,0xD3,0xE0);			/* mov ax,1; mov cl,33; shl ax,cl */
	go();
	c_shmask = (ro[R_AX] == 2) ? 1 : 0;

	setup();
	EB(5, 0xB8,0x02,0x01,0xD5,0x08);				/* mov ax,0102h; aad 8 */
	go();
	c_necaad = ((ro[R_AX] & 0xFF) == 12) ? 1 : 0;

	is8086 = (c_flagsF && !c_shmask && !c_necaad) ? 1 : 0;

	if (c_flagsF) {
		if (c_necaad) strcpy(cpu_name,"NEC V20/V30");
		else if (c_shmask) strcpy(cpu_name,"80186/80188");
		else strcpy(cpu_name,"8086/8088");
	}
	else if (c_386) {
		strcpy(cpu_name,c_v86 ? "80386 or later (V86 mode)" : "80386 or later");
	}
	else {
		strcpy(cpu_name,"80286");
	}
}

/* ======================================================= 386 REAL MODE HARNESS */

/* 32-bit register state. The runner and everything else that needs 386
 * instructions is generated machine code: the 16-bit build is compiled for
 * the 8086 and the inline assembler would not accept 386 instructions. */
struct r32 {
	unsigned long	eax,ecx,edx,ebx,esp,ebp,esi,edi,efl;
	unsigned short	es,fs,gs,ds;
};

struct r32			I,O,X;		/* in, out, expected */
unsigned long			esp_b,esp_a;	/* ESP before and after the call to the test code */
static unsigned char		x_land_set = 0;
static unsigned			x_land;		/* resume point other than the final RETF */

#define memd			((unsigned long*)memw)

#define RMCODE_SIZE		1024
static unsigned char __far	rmcode[RMCODE_SIZE];
static unsigned char __far	*rp;
static void			(__far *runner32)(void);
static void			(__far *pm_enter)(void);
static unsigned			rm_dgv;		/* offset in rmcode of a word holding DGROUP */
static unsigned			rm_resume;	/* real mode resume point after protected mode */
static unsigned			rm_sv;		/* offset in rmcode of saved SS, SP */

static unsigned rhere(void) {
	return FP_OFF(rp);
}

static void R(unsigned b) {
	*rp++ = (unsigned char)b;
}

static void RW(unsigned w) {
	R(w & 0xFF);
	R(w >> 8);
}

static void RB(unsigned n,...) {
	va_list va;

	va_start(va,n);
	while (n-- > 0) R(va_arg(va,unsigned));
	va_end(va);
}

#define AOFF(x)		((unsigned)FP_OFF(&(x)))

/* thunk emitter extras */
static void ED(unsigned long d) {
	EW((unsigned)(d & 0xFFFFul));
	EW((unsigned)(d >> 16ul));
}

/* build the 32-bit runner: saves everything, loads I, far calls the thunk, stores O */
static void build_runner(void) {
	rp = rmcode;

	rm_dgv = rhere(); RW(dgroup);
	rm_sv = rhere(); RW(0); RW(0); RW(0);			/* saved SS, SP, spare */

	runner32 = (void (__far*)(void))MK_FP(FP_SEG(rmcode),rhere());
	RB(2, 0x66,0x9C);						/* pushfd */
	RB(2, 0x66,0x60);						/* pushad */
	RB(6, 0x1E,0x06,0x0F,0xA0,0x0F,0xA8);				/* push ds, es, fs, gs */
	RB(3, 0x2E,0x8E,0x1E); RW(rm_dgv);				/* mov ds,cs:[dgroup] */
	RB(2, 0x8E,0x06); RW(AOFF(I.es));
	RB(2, 0x8E,0x26); RW(AOFF(I.fs));
	RB(2, 0x8E,0x2E); RW(AOFF(I.gs));
	RB(3, 0x66,0x8B,0x0E); RW(AOFF(I.ecx));
	RB(3, 0x66,0x8B,0x16); RW(AOFF(I.edx));
	RB(3, 0x66,0x8B,0x1E); RW(AOFF(I.ebx));
	RB(3, 0x66,0x8B,0x2E); RW(AOFF(I.ebp));
	RB(3, 0x66,0x8B,0x36); RW(AOFF(I.esi));
	RB(3, 0x66,0x8B,0x3E); RW(AOFF(I.edi));
	RB(3, 0x66,0xFF,0x36); RW(AOFF(I.efl));				/* push dword [I.efl] */
	RB(3, 0x66,0x8B,0x06); RW(AOFF(I.eax));
	RB(2, 0x66,0x9D);						/* popfd */
	RB(3, 0x66,0x89,0x26); RW(AOFF(esp_b));
	RB(2, 0xFF,0x1E); RW(AOFF(thunk_ptr));				/* call far [thunk_ptr] */
	RB(3, 0x66,0x89,0x26); RW(AOFF(esp_a));
	RB(2, 0x66,0x9C);						/* pushfd */
	RB(3, 0x66,0x89,0x06); RW(AOFF(O.eax));
	RB(3, 0x66,0x89,0x0E); RW(AOFF(O.ecx));
	RB(3, 0x66,0x89,0x16); RW(AOFF(O.edx));
	RB(3, 0x66,0x89,0x1E); RW(AOFF(O.ebx));
	RB(3, 0x66,0x89,0x2E); RW(AOFF(O.ebp));
	RB(3, 0x66,0x89,0x36); RW(AOFF(O.esi));
	RB(3, 0x66,0x89,0x3E); RW(AOFF(O.edi));
	RB(3, 0x66,0x8F,0x06); RW(AOFF(O.efl));				/* pop dword [O.efl] */
	RB(2, 0x8C,0x06); RW(AOFF(O.es));
	RB(2, 0x8C,0x26); RW(AOFF(O.fs));
	RB(2, 0x8C,0x2E); RW(AOFF(O.gs));
	RB(2, 0x8C,0x1E); RW(AOFF(O.ds));
	RB(6, 0x0F,0xA9,0x0F,0xA1,0x07,0x1F);				/* pop gs, fs, es, ds */
	RB(2, 0x66,0x61);						/* popad */
	RB(2, 0x66,0x9D);						/* popfd */
	R(0xCB);
}

static void setup32(void) {
	I.eax = 0x1A2B3C4Dul; I.ecx = 0x5E6F7081ul; I.edx = 0x92A3B4C5ul; I.ebx = 0xD6E7F809ul;
	I.esp = 0;            I.ebp = 0x1B2C3D4Eul; I.esi = 0x5F607182ul; I.edi = 0x93A4B5C6ul;
	I.efl = 0x0202ul;
	I.es = I.fs = I.gs = I.ds = dgroup;
	tp = thunk;
}

static void go32(void) {
	x_cnt = 0;
	x_faulted = 0;
	x_resume = x_land_set ? x_land : here();
	E(0xCB);
	X = I;
	runner32();
	x_resume = 0;
	x_land_set = 0;
	x_unexpected = (x_faulted && !exc_expected) ? 1 : 0;
}

/* run expecting a fault */
static void go32x(void) {
	exc_expected = 1;
	go32();
	exc_expected = 0;
}

static unsigned long *reg32(struct r32 *r,unsigned i) {
	switch (i & 7) {
		case 0: return &r->eax;
		case 1: return &r->ecx;
		case 2: return &r->edx;
		case 3: return &r->ebx;
		case 4: return &r->esp;
		case 5: return &r->ebp;
		case 6: return &r->esi;
	}
	return &r->edi;
}

static const char *rn32[8] = { "EAX","ECX","EDX","EBX","ESP","EBP","ESI","EDI" };

static int verify32(unsigned long fmask) {
	char *p = diffbuf;
	unsigned i;
	int ok = 1;

	grp_tests++;
	*p = 0;
	for (i=0;i < 8;i++) {
		if (i == 4) continue;
		if (*reg32(&O,i) != *reg32(&X,i)) {
			p += sprintf(p," %s=%08lX(%08lX)",rn32[i],*reg32(&O,i),*reg32(&X,i));
			ok = 0;
		}
	}
	if (O.es != X.es) { p += sprintf(p," ES=%04X(%04X)",O.es,X.es); ok = 0; }
	if (O.fs != X.fs) { p += sprintf(p," FS=%04X(%04X)",O.fs,X.fs); ok = 0; }
	if (O.gs != X.gs) { p += sprintf(p," GS=%04X(%04X)",O.gs,X.gs); ok = 0; }
	if ((O.efl ^ X.efl) & fmask) { p += sprintf(p," FL=%08lX(%08lX&%08lX)",O.efl,X.efl,fmask); ok = 0; }
	if (esp_b != esp_a) { p += sprintf(p," ESP %08lX->%08lX",esp_b,esp_a); ok = 0; }
	if (x_unexpected) { p += sprintf(p," INT %02X raised at %04X",x_vec,x_ip); ok = 0; }
	if (!ok) sprintf(p,"  in EAX=%08lX EBX=%08lX ECX=%08lX FL=%08lX",I.eax,I.ebx,I.ecx,I.efl);
	return ok;
}

/* failure with the diff from verify32() */
static void failr(const char *fmt,...) {
	va_list va;
	va_start(va,fmt);
	failv(1,fmt,va);
	va_end(va);
}

static void skip(const char *why) {
	grp_skip = 1;
	if (logf) fprintf(logf,"  SKIP %s: %s\n",grp_name,why);
}

/* ------------------------------------- real mode exception handlers */
static void (__interrupt __far *old_rm_vec[0x0E])();
static const unsigned char rm_hook_list[] = { 0x00,0x01,0x03,0x04,0x05,0x06,0x0C,0x0D };

static void x_rec(unsigned vec,union INTPACK __far *r) {
	x_cnt++;
	x_vec = vec;
	x_ip = r->w.ip;
	x_cs = r->w.cs;
	x_fl = r->w.flags;
	if (x_resume) {
		r->w.ip = x_resume;
		r->w.cs = thunk_seg;
		x_faulted = 1;
	}
}

static void __interrupt __far __loadds rm00_h(union INTPACK r) { x_rec(0x00,&r); }
static void __interrupt __far __loadds rm01_h(union INTPACK r) { x_rec(0x01,&r); }
static void __interrupt __far __loadds rm03_h(union INTPACK r) { x_rec(0x03,&r); }
static void __interrupt __far __loadds rm04_h(union INTPACK r) { x_rec(0x04,&r); }
static void __interrupt __far __loadds rm05_h(union INTPACK r) { x_rec(0x05,&r); }
static void __interrupt __far __loadds rm06_h(union INTPACK r) { x_rec(0x06,&r); }

/* INT 0Ch/0Dh are also IRQ 4/5. Take them only when they come from the test code */
static void __interrupt __far __loadds rm0c_h(union INTPACK r) {
	if (x_resume && r.w.cs == thunk_seg) { x_rec(0x0C,&r); return; }
	_chain_intr(old_rm_vec[0x0C]);
}

static void __interrupt __far __loadds rm0d_h(union INTPACK r) {
	if (x_resume && r.w.cs == thunk_seg) { x_rec(0x0D,&r); return; }
	_chain_intr(old_rm_vec[0x0D]);
}

static void hook_rm(void) {
	static void (__interrupt __far * const h[])() = { rm00_h,rm01_h,rm03_h,rm04_h,rm05_h,rm06_h,rm0c_h,rm0d_h };
	unsigned i;

	for (i=0;i < sizeof(rm_hook_list);i++) {
		old_rm_vec[rm_hook_list[i]] = _dos_getvect(rm_hook_list[i]);
		_dos_setvect(rm_hook_list[i],h[i]);
	}
}

static void unhook_rm(void) {
	unsigned i;

	for (i=0;i < sizeof(rm_hook_list);i++) _dos_setvect(rm_hook_list[i],old_rm_vec[rm_hook_list[i]]);
}

/* ------------------------------------------------- reference models */
static unsigned char partab[256];

static void init_parity(void) {
	unsigned i,j,c;

	for (i=0;i < 256;i++) {
		for (c=0,j=i;j;j>>=1) c += j & 1;
		partab[i] = (c & 1) ? 0 : 1;
	}
}

/* operand width: 0 = 8, 1 = 16, 2 = 32 bits */
static unsigned long wmask(int w) { return w == 2 ? 0xFFFFFFFFul : w == 1 ? 0xFFFFul : 0xFFul; }
static unsigned long wsign(int w) { return w == 2 ? 0x80000000ul : w == 1 ? 0x8000ul : 0x80ul; }
static unsigned wbits(int w) { return w == 2 ? 32u : w == 1 ? 16u : 8u; }

static unsigned szp(unsigned long r,int w) {
	unsigned f = 0;

	r &= wmask(w);
	if (r == 0) f |= ZF;
	if (r & wsign(w)) f |= SF;
	if (partab[r & 0xFF]) f |= PF;
	return f;
}

static unsigned long sext(unsigned long v,int w) {
	v &= wmask(w);
	if (v & wsign(w)) v |= ~wmask(w);
	return v;
}

enum { OP_ADD=0, OP_OR, OP_ADC, OP_SBB, OP_AND, OP_SUB, OP_XOR, OP_CMP };
static const char *alu_names[8] = { "ADD","OR","ADC","SBB","AND","SUB","XOR","CMP" };

static unsigned long alu_ref(int op,int w,unsigned long a,unsigned long b,unsigned long fin,unsigned long *fo,unsigned long *fm) {
	unsigned long m = wmask(w),sg = wsign(w),r,cin = fin & CF;
	unsigned long long t;
	unsigned long f = fin & ~(unsigned long)ARITH;

	a &= m; b &= m;
	*fm = ARITH;
	switch (op) {
		case OP_ADD: case OP_ADC:
			t = (unsigned long long)a + b + (op == OP_ADC ? cin : 0);
			r = (unsigned long)t & m;
			if (t > m) f |= CF;
			if ((~(a ^ b) & (a ^ r)) & sg) f |= OF;
			if ((a ^ b ^ r) & 0x10) f |= AF;
			break;
		case OP_SBB: case OP_SUB: case OP_CMP:
			t = (unsigned long long)b + (op == OP_SBB ? cin : 0);
			r = (a - (unsigned long)t) & m;
			if ((unsigned long long)a < t) f |= CF;
			if (((a ^ b) & (a ^ r)) & sg) f |= OF;
			if ((a ^ b ^ r) & 0x10) f |= AF;
			break;
		default:
			r = (op == OP_OR) ? (a | b) : (op == OP_AND) ? (a & b) : (a ^ b);
			*fm &= ~(unsigned long)AF;
			break;
	}
	f |= szp(r,w);
	*fo = f;
	return r;
}

enum { SH_ROL=0, SH_ROR, SH_RCL, SH_RCR, SH_SHL, SH_SHR, SH_SAL, SH_SAR };
static const char *sh_names[8] = { "ROL","ROR","RCL","RCR","SHL","SHR","SAL","SAR" };

/* shift/rotate, count masked to 5 bits (386) */
static unsigned long shift_ref(int op,int w,unsigned long v,unsigned cnt,unsigned long fin,unsigned long *fo,unsigned long *fm) {
	unsigned long msb = wsign(w),mask = wmask(w),orig,cf = fin & CF,n,f;
	unsigned c = cnt & 31,i;

	v &= mask;
	orig = v;
	*fm = ARITH;
	*fo = fin;
	if (c == 0) return v;

	for (i=0;i < c;i++) {
		switch (op) {
			case SH_ROL: cf = (v & msb) ? 1 : 0; v = ((v << 1) | cf) & mask; break;
			case SH_ROR: cf = v & 1; v = (v >> 1) | (cf ? msb : 0); break;
			case SH_RCL: n = (v & msb) ? 1 : 0; v = ((v << 1) | cf) & mask; cf = n; break;
			case SH_RCR: n = v & 1; v = (v >> 1) | (cf ? msb : 0); cf = n; break;
			case SH_SHL: cf = (v & msb) ? 1 : 0; v = (v << 1) & mask; break;
			case SH_SHR: cf = v & 1; v >>= 1; break;
			case SH_SAR: cf = v & 1; v = (v >> 1) | (v & msb); break;
		}
	}

	f = (fin & ~(unsigned long)(CF|OF)) | (cf ? CF : 0);
	switch (op) {
		case SH_ROL: case SH_RCL: case SH_SHL:
			if (((v & msb) ? 1 : 0) ^ cf) f |= OF;
			break;
		case SH_ROR: case SH_RCR:
			if (((v >> 1) ^ v) & (msb >> 1)) f |= OF;
			break;
		case SH_SHR:
			if (orig & msb) f |= OF;
			break;
	}
	if (c != 1) *fm &= ~(unsigned long)OF;
	if (op >= SH_SHL) {
		f = (f & ~(unsigned long)(SF|ZF|PF)) | szp(v,w);
		*fm &= ~(unsigned long)AF;
	}
	*fo = f;
	return v;
}

/* SHLD/SHRD, w = 1 or 2 */
static unsigned long shxd_ref(int right,int w,unsigned long d,unsigned long s,unsigned cnt,unsigned long fin,unsigned long *fo,unsigned long *fm) {
	unsigned bits = wbits(w),c = cnt & 31;
	unsigned long m = wmask(w),r,f;
	unsigned long long t;
	int cf;

	d &= m; s &= m;
	*fo = fin;
	*fm = ARITH;
	if (c == 0) return d;
	if (right) {
		t = ((unsigned long long)s << bits) | d;
		r = (unsigned long)(t >> c) & m;
		cf = (int)((t >> (c - 1)) & 1);
	}
	else {
		t = ((unsigned long long)d << bits) | s;
		r = (unsigned long)((t << c) >> bits) & m;
		cf = (int)((t >> (2 * bits - c)) & 1);
	}
	f = (fin & ~(unsigned long)ARITH) | (cf ? CF : 0) | szp(r,w);
	if (((r ^ d) & wsign(w)) != 0) f |= OF;
	*fm &= ~(unsigned long)AF;
	if (c != 1) *fm &= ~(unsigned long)OF;
	*fo = f;
	return r;
}

/* operand values */
static const unsigned long v32[16] = {
	0x00000000ul,0x00000001ul,0x00000002ul,0x0000007Ful,0x00000080ul,0x0000FFFFul,0x00010000ul,0x7FFFFFFEul,
	0x7FFFFFFFul,0x80000000ul,0x80000001ul,0xFFFFFFFEul,0xFFFFFFFFul,0x12345678ul,0xA5C3F00Ful,0x00008000ul
};

static const unsigned char v8[16] = {
	0x00,0x01,0x02,0x09,0x0F,0x10,0x3C,0x55,0x7E,0x7F,0x80,0x81,0x99,0xAA,0xFE,0xFF
};

static const unsigned short v16[16] = {
	0x0000,0x0001,0x0002,0x007F,0x0080,0x00FF,0x0100,0x7FFE,0x7FFF,0x8000,0x8001,0xFFFE,0xFFFF,0x1234,0x5A5A,0xA5C3
};

#define F32		(0x0202ul)			/* default flags */
#define FALL		(F32|ARITH)
#define CMASK		((unsigned long)(ARITH|CTRL))

/* ================================================================ REAL MODE TESTS */

/* ---- CPU identification: AC (486+) and ID (CPUID) flags */
static unsigned char c_486 = 0,c_cpuid = 0;

static int flag_toggles(unsigned long bit) {
	setup32();
	EB(4, 0x66,0x9C,0x66,0x58);				/* pushfd; pop eax */
	EB(3, 0x66,0x8B,0xD8);					/* mov ebx,eax */
	EB(2, 0x66,0x35); ED(bit);				/* xor eax,bit */
	EB(8, 0x66,0x50,0x66,0x9D,0x66,0x9C,0x66,0x58);	/* push eax; popfd; pushfd; pop eax */
	EB(4, 0x66,0x53,0x66,0x9D);				/* push ebx; popfd */
	go32();
	return ((O.eax ^ O.ebx) & bit) ? 1 : 0;
}

static void t_cpu386(void) {
	begin("CPU 386");
	c_486 = (unsigned char)flag_toggles(0x00040000ul);
	c_cpuid = (unsigned char)flag_toggles(0x00200000ul);
	info("EFLAGS.AC %s (%s), EFLAGS.ID %s",c_486 ? "writable" : "fixed at 0",c_486 ? "486 or later" : "386",
		c_cpuid ? "writable, CPUID present" : "fixed, no CPUID");
	if (c_cpuid) {
		char vendor[13];

		setup32(); I.eax = 0; EB(2, 0x0F,0xA2); go32();
		memcpy(vendor,&O.ebx,4); memcpy(vendor+4,&O.edx,4); memcpy(vendor+8,&O.ecx,4); vendor[12] = 0;
		setup32(); I.eax = 1; EB(2, 0x0F,0xA2); go32();
		info("CPUID vendor \"%s\" family %lu model %lu stepping %lu",vendor,(O.eax >> 8) & 15ul,(O.eax >> 4) & 15ul,O.eax & 15ul);
	}
	end();
}

/* ---- 32-bit MOV/XCHG */
static void t_mov32(void) {
	unsigned s,d;

	begin("MOV32");
	for (s=0;s < 8;s++) {
		for (d=0;d < 8;d++) {
			if (s == 4 || d == 4) continue;
			setup32(); EB(3, 0x66,0x89,0xC0|(s<<3)|d); go32(); *reg32(&X,d) = *reg32(&I,s);
			if (!verify32(CMASK)) failr("66 89 mov r%u,r%u",d,s);
			setup32(); EB(3, 0x66,0x8B,0xC0|(d<<3)|s); go32(); *reg32(&X,d) = *reg32(&I,s);
			if (!verify32(CMASK)) failr("66 8B mov r%u,r%u",d,s);
		}
		if (s != 4) {
			setup32(); EB(2, 0x66,0xB8+s); ED(0xC0DE0000ul | s); go32(); *reg32(&X,s) = 0xC0DE0000ul | s;
			if (!verify32(CMASK)) failr("66 B%X mov r%u,imm32",8+s,s);
			setup32(); EB(2, 0x66,0x90+s); go32(); X.eax = *reg32(&I,s); *reg32(&X,s) = I.eax;
			if (!verify32(CMASK)) failr("66 %02X xchg eax,r%u",0x90+s,s);
		}
	}
	setup32(); EB(3, 0x66,0x87,0xD9); go32(); X.ebx = I.ecx; X.ecx = I.ebx;
	if (!verify32(CMASK)) failr("66 87 D9 xchg ecx,ebx");

	setup32(); memd[0] = 0; I.ebx = memw_off; EB(3, 0x66,0x89,0x07); go32();
	if (!verify32(CMASK) || !checkm(memd[0] == I.eax)) failr("66 89 07 mov [bx],eax mem=%08lX",memd[0]);
	setup32(); memd[1] = 0xFEEDFACEul; I.ebx = memw_off; EB(4, 0x66,0x8B,0x47,0x04); go32(); X.eax = 0xFEEDFACEul;
	if (!verify32(CMASK)) failr("66 8B 47 04 mov eax,[bx+4]");
	setup32(); memd[2] = 0x13579BDFul; EB(2, 0x66,0xA1); EW(memw_off+8); go32(); X.eax = 0x13579BDFul;
	if (!verify32(CMASK)) failr("66 A1 mov eax,[moffs]");
	setup32(); memd[3] = 0; I.ebx = memw_off; EB(4, 0x66,0xC7,0x47,0x0C); ED(0x2468ACE0ul); go32();
	if (!verify32(CMASK) || !checkm(memd[3] == 0x2468ACE0ul)) failr("66 C7 47 0C mov dword [bx+12],imm32 mem=%08lX",memd[3]);
	setup32(); memd[0] = 0x0BADF00Dul; I.ebx = memw_off; EB(3, 0x66,0x87,0x07); go32(); X.eax = 0x0BADF00Dul;
	if (!verify32(CMASK) || !checkm(memd[0] == I.eax)) failr("66 87 07 xchg eax,[bx]");
	end();
}

/* ---- MOVZX/MOVSX */
static void t_movx(void) {
	unsigned i;

	begin("MOVZX/MOVSX");
	for (i=0;i < 16;i++) {
		setup32(); I.ebx = 0xDEAD0000ul | ((unsigned long)v16[i]); EB(3, 0x0F,0xB6,0xC3); go32();
		X.eax = (I.eax & 0xFFFF0000ul) | (I.ebx & 0xFFul);
		if (!verify32(CMASK)) failr("0F B6 movzx ax,bl %04X",v16[i]);
		setup32(); I.ebx = 0xDEAD0000ul | v16[i]; EB(4, 0x66,0x0F,0xB6,0xC3); go32(); X.eax = I.ebx & 0xFFul;
		if (!verify32(CMASK)) failr("66 0F B6 movzx eax,bl %04X",v16[i]);
		setup32(); I.ebx = 0xDEAD0000ul | v16[i]; EB(4, 0x66,0x0F,0xB7,0xC3); go32(); X.eax = v16[i];
		if (!verify32(CMASK)) failr("66 0F B7 movzx eax,bx %04X",v16[i]);
		setup32(); I.ebx = 0xDEAD0000ul | v16[i]; EB(3, 0x0F,0xBE,0xC3); go32();
		X.eax = (I.eax & 0xFFFF0000ul) | (sext(v16[i] & 0xFF,0) & 0xFFFFul);
		if (!verify32(CMASK)) failr("0F BE movsx ax,bl %04X",v16[i]);
		setup32(); I.ebx = 0xDEAD0000ul | v16[i]; EB(4, 0x66,0x0F,0xBE,0xC3); go32(); X.eax = sext(v16[i] & 0xFF,0);
		if (!verify32(CMASK)) failr("66 0F BE movsx eax,bl %04X",v16[i]);
		setup32(); I.ebx = 0xDEAD0000ul | v16[i]; EB(4, 0x66,0x0F,0xBF,0xC3); go32(); X.eax = sext(v16[i],1);
		if (!verify32(CMASK)) failr("66 0F BF movsx eax,bx %04X",v16[i]);
		setup32(); memw[0] = v16[i]; I.ebx = memw_off; EB(4, 0x66,0x0F,0xBF,0x07); go32(); X.eax = sext(v16[i],1);
		if (!verify32(CMASK)) failr("66 0F BF movsx eax,word [bx] %04X",v16[i]);
		setup32(); memw[0] = v16[i]; I.ebx = memw_off; EB(4, 0x66,0x0F,0xB6,0x07); go32(); X.eax = v16[i] & 0xFFu;
		if (!verify32(CMASK)) failr("66 0F B6 movzx eax,byte [bx] %04X",v16[i]);
	}
	end();
}

/* ---- 32-bit ALU, several encodings */
enum { A_RR=0, A_RRD, A_EAXI, A_81, A_83, A_M32, A_SIB, A_MAX };
static const char *a_names[A_MAX] = { "r/m32,r32","r32,r/m32","EAX,imm32","81 r32,imm32","83 r32,simm8","m32,r32","r32,[ebx+esi*2+d8]" };

static void alu32_case(unsigned op,unsigned form,unsigned long a,unsigned long b,unsigned long fin) {
	unsigned long res,fo,fm,mexp = 0;
	int memtest = 0,ok;

	setup32();
	I.efl = fin;
	switch (form) {
		case A_RR:   I.eax = a; I.ebx = b; EB(3, 0x66,op*8+1,0xD8); break;
		case A_RRD:  I.eax = a; I.ebx = b; EB(3, 0x66,op*8+3,0xC3); break;
		case A_EAXI: I.eax = a; EB(2, 0x66,op*8+5); ED(b); break;
		case A_81:   I.edx = a; EB(3, 0x66,0x81,0xC2|(op<<3)); ED(b); break;
		case A_83:   I.esi = a; b = sext(b & 0xFF,0); EB(4, 0x66,0x83,0xC6|(op<<3),(unsigned)(b & 0xFF)); break;
		case A_M32:  memd[0] = a; I.ebx = memw_off; I.eax = b; EB(3, 0x66,op*8+1,0x07); memtest = 1; break;
		case A_SIB:  memd[0] = b; I.eax = a; I.ebx = memw_off - 0x40; I.esi = 0x10;
			     EB(6, 0x67,0x66,op*8+3,0x44,0x73,0x20); break;
	}
	go32();
	res = alu_ref(op,2,a,b,fin,&fo,&fm);
	X.efl = fo;
	if (op != OP_CMP) {
		switch (form) {
			case A_RR: case A_RRD: case A_EAXI: case A_SIB: X.eax = res; break;
			case A_81: X.edx = res; break;
			case A_83: X.esi = res; break;
			case A_M32: mexp = res; break;
		}
	}
	else mexp = a;
	ok = verify32(fm | CTRL);
	if (memtest && !checkm(memd[0] == mexp)) {
		ok = 0;
		sprintf(diffbuf+strlen(diffbuf)," mem=%08lX(%08lX)",memd[0],mexp);
	}
	if (!ok) failr("%s32 %s a=%08lX b=%08lX",alu_names[op],a_names[form],a,b);
}

static void t_alu32(unsigned op) {
	char name[16];
	unsigned form,i,j;

	sprintf(name,"%s32",alu_names[op]);
	begin(name);
	for (form=0;form < A_MAX;form++) {
		for (i=0;i < 16;i++) {
			for (j=0;j < 16;j++) {
				if (op == OP_ADC || op == OP_SBB) {
					alu32_case(op,form,v32[i],v32[j],F32);
					alu32_case(op,form,v32[i],v32[j],FALL);
				}
				else alu32_case(op,form,v32[i],v32[j],((i ^ j) & 1) ? FALL : F32);
			}
		}
	}
	end();
}

/* ---- TEST/INC/DEC/NEG/NOT 32-bit */
static void t_unary32(void) {
	unsigned i,j,r;
	unsigned long fo,fm,fin,res;

	begin("UNARY32");
	for (i=0;i < 16;i++) {
		fin = (i & 1) ? FALL : F32;
		for (j=0;j < 16;j++) {
			setup32(); I.efl = fin; I.eax = v32[i]; I.ebx = v32[j]; EB(3, 0x66,0x85,0xD8); go32();
			alu_ref(OP_AND,2,v32[i],v32[j],fin,&fo,&fm); X.efl = fo;
			if (!verify32(fm|CTRL)) failr("66 85 test eax,ebx %08lX,%08lX",v32[i],v32[j]);
		}
		setup32(); I.efl = fin; I.eax = v32[i]; EB(2, 0x66,0xA9); ED(0x80008001ul); go32();
		alu_ref(OP_AND,2,v32[i],0x80008001ul,fin,&fo,&fm); X.efl = fo;
		if (!verify32(fm|CTRL)) failr("66 A9 test eax,imm32 %08lX",v32[i]);

		for (r=0;r < 8;r++) {
			if (r == 4) continue;
			setup32(); I.efl = fin; *reg32(&I,r) = v32[i]; EB(2, 0x66,0x40+r); go32();
			res = alu_ref(OP_ADD,2,v32[i],1,fin,&fo,&fm); *reg32(&X,r) = res; X.efl = (fo & ~(unsigned long)CF) | (fin & CF);
			if (!verify32(CMASK)) failr("66 %02X inc r%u %08lX",0x40+r,r,v32[i]);
			setup32(); I.efl = fin; *reg32(&I,r) = v32[i]; EB(2, 0x66,0x48+r); go32();
			res = alu_ref(OP_SUB,2,v32[i],1,fin,&fo,&fm); *reg32(&X,r) = res; X.efl = (fo & ~(unsigned long)CF) | (fin & CF);
			if (!verify32(CMASK)) failr("66 %02X dec r%u %08lX",0x48+r,r,v32[i]);
		}

		setup32(); I.efl = fin; I.edx = v32[i]; EB(3, 0x66,0xF7,0xDA); go32();
		X.edx = alu_ref(OP_SUB,2,0,v32[i],fin & ~(unsigned long)CF,&fo,&fm); X.efl = fo;
		if (!verify32(fm|CTRL)) failr("66 F7 DA neg edx %08lX",v32[i]);
		setup32(); I.efl = fin; I.edx = v32[i]; EB(3, 0x66,0xF7,0xD2); go32(); X.edx = ~v32[i];
		if (!verify32(CMASK)) failr("66 F7 D2 not edx %08lX",v32[i]);
		setup32(); I.efl = fin; memd[0] = v32[i]; I.ebx = memw_off; EB(3, 0x66,0xFF,0x07); go32();
		res = alu_ref(OP_ADD,2,v32[i],1,fin,&fo,&fm); X.efl = (fo & ~(unsigned long)CF) | (fin & CF);
		if (!verify32(CMASK) || !checkm(memd[0] == res)) failr("66 FF 07 inc dword [bx] %08lX mem=%08lX",v32[i],memd[0]);
		setup32(); I.efl = fin; memd[0] = v32[i]; I.ebx = memw_off; EB(3, 0x66,0xF7,0x1F); go32();
		res = alu_ref(OP_SUB,2,0,v32[i],fin & ~(unsigned long)CF,&fo,&fm); X.efl = fo;
		if (!verify32(fm|CTRL) || !checkm(memd[0] == res)) failr("66 F7 1F neg dword [bx] %08lX mem=%08lX",v32[i],memd[0]);
	}
	end();
}

/* ---- MUL/IMUL 32-bit, IMUL 2 and 3 operand forms */
static int imul_ovf(long long p,int w) {
	return p != (long long)(long)sext((unsigned long)p,w) ? 1 : 0;
}

static void t_mul32(void) {
	unsigned i,j;
	unsigned long long up;
	long long sp;
	unsigned long fin,ov;

	begin("MUL32/IMUL");
	for (i=0;i < 16;i++) {
		for (j=0;j < 16;j++) {
			fin = ((i ^ j) & 1) ? FALL : F32;
			up = (unsigned long long)v32[i] * v32[j];
			sp = (long long)(long)v32[i] * (long long)(long)v32[j];

			setup32(); I.efl = fin; I.eax = v32[i]; I.ebx = v32[j]; EB(3, 0x66,0xF7,0xE3); go32();
			X.eax = (unsigned long)up; X.edx = (unsigned long)(up >> 32);
			X.efl = X.edx ? (fin|CF|OF) : (fin & ~(unsigned long)(CF|OF));
			if (!verify32(CF|OF|CTRL)) failr("66 F7 E3 mul ebx %08lX*%08lX",v32[i],v32[j]);

			setup32(); I.efl = fin; I.eax = v32[i]; I.ebx = v32[j]; EB(3, 0x66,0xF7,0xEB); go32();
			X.eax = (unsigned long)sp; X.edx = (unsigned long)((unsigned long long)sp >> 32);
			ov = (unsigned long)imul_ovf(sp,2);
			X.efl = ov ? (fin|CF|OF) : (fin & ~(unsigned long)(CF|OF));
			if (!verify32(CF|OF|CTRL)) failr("66 F7 EB imul ebx %08lX*%08lX",v32[i],v32[j]);

			setup32(); I.efl = fin; I.eax = v32[i]; I.ebx = v32[j]; EB(4, 0x66,0x0F,0xAF,0xC3); go32();
			X.eax = (unsigned long)sp;
			X.efl = ov ? (fin|CF|OF) : (fin & ~(unsigned long)(CF|OF));
			if (!verify32(CF|OF|CTRL)) failr("66 0F AF imul eax,ebx %08lX*%08lX",v32[i],v32[j]);

			setup32(); I.efl = fin; I.ebx = v32[i]; EB(3, 0x66,0x69,0xC3); ED(v32[j]); go32();
			X.eax = (unsigned long)sp;
			X.efl = ov ? (fin|CF|OF) : (fin & ~(unsigned long)(CF|OF));
			if (!verify32(CF|OF|CTRL)) failr("66 69 imul eax,ebx,imm32 %08lX*%08lX",v32[i],v32[j]);

			{
				long long sp8 = (long long)(long)v32[i] * (long long)(long)sext(v8[j],0);

				setup32(); I.efl = fin; I.ebx = v32[i]; EB(4, 0x66,0x6B,0xC3,v8[j]); go32();
				X.eax = (unsigned long)sp8;
				X.efl = imul_ovf(sp8,2) ? (fin|CF|OF) : (fin & ~(unsigned long)(CF|OF));
				if (!verify32(CF|OF|CTRL)) failr("66 6B imul eax,ebx,simm8 %08lX*%02X",v32[i],v8[j]);
			}
			{
				long sp16 = (long)(short)v16[i] * (long)(short)v16[j];

				setup32(); I.efl = fin; I.eax = 0xAAAA0000ul | v16[i]; I.ebx = v16[j]; EB(3, 0x0F,0xAF,0xC3); go32();
				X.eax = 0xAAAA0000ul | ((unsigned long)sp16 & 0xFFFFul);
				X.efl = imul_ovf(sp16,1) ? (fin|CF|OF) : (fin & ~(unsigned long)(CF|OF));
				if (!verify32(CF|OF|CTRL)) failr("0F AF imul ax,bx %04X*%04X",v16[i],v16[j]);
			}
		}
	}
	end();
}

/* ---- DIV/IDIV 32-bit */
static void t_div32(void) {
	static const unsigned long hi[4] = { 0x00000000ul,0x00000001ul,0x00001234ul,0xFFFFFFFFul };
	unsigned i,j,k;
	unsigned long long dvd,q;
	long long sd,sq,sr;

	begin("DIV32");
	for (i=0;i < 16;i++) {
		for (j=0;j < 16;j++) {
			if (v32[j] == 0) continue;
			for (k=0;k < 4;k++) {
				dvd = ((unsigned long long)hi[k] << 32) | v32[i];
				q = dvd / v32[j];
				if (q <= 0xFFFFFFFFull) {
					setup32(); I.eax = v32[i]; I.edx = hi[k]; I.ebx = v32[j]; EB(3, 0x66,0xF7,0xF3); go32();
					X.eax = (unsigned long)q; X.edx = (unsigned long)(dvd % v32[j]);
					if (!verify32(CTRL)) failr("66 F7 F3 div ebx %08lX%08lX/%08lX",hi[k],v32[i],v32[j]);
				}
				sd = (long long)dvd;
				sq = sd / (long)v32[j];
				sr = sd % (long)v32[j];
				if (sq >= -2147483647ll && sq <= 2147483647ll) {
					setup32(); I.eax = v32[i]; I.edx = hi[k]; I.ebx = v32[j]; EB(3, 0x66,0xF7,0xFB); go32();
					X.eax = (unsigned long)sq; X.edx = (unsigned long)sr;
					if (!verify32(CTRL)) failr("66 F7 FB idiv ebx %08lX%08lX/%08lX",hi[k],v32[i],v32[j]);
				}
			}
		}
	}

	/* divide errors: IP points at the instruction, including the prefix */
	{
		unsigned s;

		setup32(); I.ebx = 0; s = here(); EB(3, 0x66,0xF7,0xF3); go32x();
		if (!checkm(x_cnt == 1 && x_vec == 0 && x_ip == s)) failm("div ebx=0: INT 0 count=%u ip=%04X(%04X)",x_cnt,x_ip,s);
		setup32(); I.edx = 1; I.eax = 0; I.ebx = 1; s = here(); EB(3, 0x66,0xF7,0xF3); go32x();
		if (!checkm(x_cnt == 1 && x_vec == 0 && x_ip == s)) failm("div quotient > 32 bits: INT 0 count=%u ip=%04X(%04X)",x_cnt,x_ip,s);
		setup32(); I.edx = 0xFFFFFFFFul; I.eax = 0x80000000ul; I.ebx = 0xFFFFFFFFul; s = here(); EB(3, 0x66,0xF7,0xFB); go32x();
		if (!checkm(x_cnt == 1 && x_vec == 0 && x_ip == s)) failm("idiv -80000000h/-1: INT 0 count=%u ip=%04X(%04X)",x_cnt,x_ip,s);
	}
	end();
}

/* ---- shifts and rotates, 32-bit */
static const unsigned char sh_counts[] = { 0,1,2,3,7,8,15,16,17,31,32,33,63,255 };
#define SH_N (sizeof(sh_counts)/sizeof(sh_counts[0]))

static void t_shift32(unsigned op) {
	char name[16];
	unsigned i,c,form,cnt;
	unsigned long fin,fo,fm,res;
	int ok;

	sprintf(name,"%s32",sh_names[op]);
	begin(name);
	for (i=0;i < 16;i++) {
		for (c=0;c < SH_N;c++) {
			cnt = sh_counts[c];
			for (form=0;form < 4;form++) {
				if (form == 0 && cnt != 1) continue;
				fin = ((i ^ c) & 1) ? FALL : F32;
				if (op == SH_RCL || op == SH_RCR) fin ^= (form & 1) ? CF : 0;
				setup32(); I.efl = fin;
				switch (form) {
					case 0: I.edx = v32[i]; EB(3, 0x66,0xD1,0xC2|(op<<3)); break;
					case 1: I.eax = v32[i]; I.ecx = 0x66660000ul | cnt; EB(3, 0x66,0xD3,0xC0|(op<<3)); break;
					case 2: I.esi = v32[i]; EB(4, 0x66,0xC1,0xC6|(op<<3),cnt); break;
					case 3: memd[0] = v32[i]; I.ebx = memw_off; I.ecx = cnt; EB(3, 0x66,0xD3,0x07|(op<<3)); break;
				}
				go32();
				res = shift_ref(op,2,v32[i],cnt,fin,&fo,&fm);
				X.efl = fo;
				if (form == 0) X.edx = res; else if (form == 1) X.eax = res; else if (form == 2) X.esi = res;
				ok = verify32(fm|CTRL);
				if (form == 3 && !checkm(memd[0] == res)) { ok = 0; sprintf(diffbuf+strlen(diffbuf)," mem=%08lX(%08lX)",memd[0],res); }
				if (!ok) failr("%s32 form %u v=%08lX cnt=%u",sh_names[op],form,v32[i],cnt);
			}
		}
	}
	end();
}

/* ---- SHLD/SHRD */
static void t_shxd(void) {
	static const unsigned char cnts[] = { 0,1,2,7,8,15,16,17,31,32,33 };
	unsigned i,j,c,right,cnt;
	unsigned long fin,fo,fm,res;

	begin("SHLD/SHRD");
	for (right=0;right < 2;right++) {
		for (i=0;i < 16;i += 3) {
			for (j=0;j < 16;j += 2) {
				for (c=0;c < sizeof(cnts);c++) {
					cnt = cnts[c];
					fin = ((i ^ j ^ c) & 1) ? FALL : F32;

					/* 32-bit, imm8 and CL */
					res = shxd_ref((int)right,2,v32[i],v32[j],cnt,fin,&fo,&fm);
					setup32(); I.efl = fin; I.eax = v32[i]; I.ebx = v32[j]; EB(5, 0x66,0x0F,right ? 0xAC : 0xA4,0xD8,cnt); go32();
					X.eax = res; X.efl = fo;
					if (!verify32(fm|CTRL)) failr("%s eax,ebx,%u %08lX %08lX",right ? "shrd" : "shld",cnt,v32[i],v32[j]);
					setup32(); I.efl = fin; I.eax = v32[i]; I.ebx = v32[j]; I.ecx = cnt; EB(4, 0x66,0x0F,right ? 0xAD : 0xA5,0xD8); go32();
					X.eax = res; X.efl = fo;
					if (!verify32(fm|CTRL)) failr("%s eax,ebx,cl=%u %08lX %08lX",right ? "shrd" : "shld",cnt,v32[i],v32[j]);
					/* 32-bit memory destination */
					setup32(); I.efl = fin; memd[0] = v32[i]; I.ebx = memw_off; I.edx = v32[j]; I.ecx = cnt;
					EB(4, 0x66,0x0F,right ? 0xAD : 0xA5,0x17); go32(); X.efl = fo;
					if (!verify32(fm|CTRL) || !checkm(memd[0] == res)) failr("%s [bx],edx,cl=%u %08lX %08lX mem=%08lX",right ? "shrd" : "shld",cnt,v32[i],v32[j],memd[0]);

					/* 16-bit, counts up to 16 */
					if (cnt <= 16) {
						res = shxd_ref((int)right,1,v16[i],v16[j],cnt,fin,&fo,&fm);
						setup32(); I.efl = fin; I.eax = 0x55550000ul | v16[i]; I.ebx = v16[j]; EB(4, 0x0F,right ? 0xAC : 0xA4,0xD8,cnt); go32();
						X.eax = 0x55550000ul | res; X.efl = fo;
						if (!verify32(fm|CTRL)) failr("%s ax,bx,%u %04X %04X",right ? "shrd" : "shld",cnt,v16[i],v16[j]);
					}
				}
			}
		}
	}
	end();
}

/* ---- BT/BTS/BTR/BTC */
static void t_bt(void) {
	static const long offs[] = { -70,-33,-32,-1,0,5,31,32,63,100 };
	static const unsigned char rop[4] = { 0xA3,0xAB,0xB3,0xBB };		/* bt bts btr btc r/m,r */
	unsigned op,i,b,k;
	unsigned long v,res,bit;
	int ok;

	begin("BT/BTS/BTR");
	for (op=0;op < 4;op++) {
		for (i=0;i < 16;i++) {
			for (b=0;b < 40;b += 3) {
				v = v32[i];
				bit = 1ul << (b & 31);
				res = (op == 1) ? (v | bit) : (op == 2) ? (v & ~bit) : (op == 3) ? (v ^ bit) : v;

				/* register, bit offset in register */
				setup32(); I.eax = v; I.ebx = b; EB(4, 0x66,0x0F,rop[op],0xD8); go32();
				X.eax = res; X.efl = (v & bit) ? (X.efl | CF) : (X.efl & ~(unsigned long)CF);
				if (!verify32(CF|CTRL)) failr("66 0F %02X eax,ebx v=%08lX bit=%u",rop[op],v,b);

				/* register, immediate bit offset */
				setup32(); I.eax = v; EB(5, 0x66,0x0F,0xBA,0xE0|(op<<3),b); go32();
				X.eax = res; X.efl = (v & bit) ? (X.efl | CF) : (X.efl & ~(unsigned long)CF);
				if (!verify32(CF|CTRL)) failr("66 0F BA /%u eax,%u v=%08lX",4+op,b,v);

				/* 16-bit register */
				{
					unsigned short v16b = (unsigned short)v,bit16 = (unsigned short)(1u << (b & 15));
					unsigned short r16 = (op == 1) ? (v16b | bit16) : (op == 2) ? (v16b & ~bit16) : (op == 3) ? (v16b ^ bit16) : v16b;

					setup32(); I.eax = 0x77770000ul | v16b; I.ebx = b; EB(3, 0x0F,rop[op],0xD8); go32();
					X.eax = 0x77770000ul | r16; X.efl = (v16b & bit16) ? (X.efl | CF) : (X.efl & ~(unsigned long)CF);
					if (!verify32(CF|CTRL)) failr("0F %02X ax,bx v=%04X bit=%u",rop[op],v16b,b);
				}
			}
		}

		/* memory with a register bit offset outside of the operand */
		for (k=0;k < sizeof(offs)/sizeof(offs[0]);k++) {
			long o = offs[k];
			unsigned base = memw_off + 0x40,idx;
			unsigned long *mp;

			for (i=0;i < 64;i++) memd[i] = 0xA5A5A5A5ul ^ (i * 0x01010101ul);
			idx = (unsigned)((long)(base - memw_off) / 4 + (o >> 5));
			mp = &memd[idx];
			v = *mp;
			bit = 1ul << ((unsigned long)o & 31ul);
			res = (op == 1) ? (v | bit) : (op == 2) ? (v & ~bit) : (op == 3) ? (v ^ bit) : v;

			setup32(); I.ebx = base; I.edx = (unsigned long)o; EB(4, 0x66,0x0F,rop[op],0x17); go32();
			X.efl = (v & bit) ? (X.efl | CF) : (X.efl & ~(unsigned long)CF);
			ok = verify32(CF|CTRL);
			if (!checkm(*mp == res)) { ok = 0; sprintf(diffbuf+strlen(diffbuf)," mem=%08lX(%08lX)",*mp,res); }
			if (!ok) failr("66 0F %02X [bx],edx bit offset %ld",rop[op],o);
		}
	}
	end();
}

/* ---- BSF/BSR */
static void t_bsf(void) {
	unsigned i,rev,k;
	unsigned long v,idx;

	begin("BSF/BSR");
	for (rev=0;rev < 2;rev++) {
		for (i=0;i < 16;i++) {
			v = v32[i];
			idx = 0;
			if (v) {
				if (rev) { for (k=31;!(v & (1ul << k));k--); }
				else { for (k=0;!(v & (1ul << k));k++); }
				idx = k;
			}
			setup32(); I.ebx = v; EB(4, 0x66,0x0F,rev ? 0xBD : 0xBC,0xC3); go32();
			if (v) X.eax = idx; else X.eax = O.eax;		/* destination undefined for 0 */
			X.efl = v ? (X.efl & ~(unsigned long)ZF) : (X.efl | ZF);
			if (!verify32(ZF|CTRL)) failr("66 0F %02X %s eax,ebx %08lX",rev ? 0xBD : 0xBC,rev ? "bsr" : "bsf",v);

			setup32(); memd[0] = v; I.ebx = memw_off; EB(4, 0x66,0x0F,rev ? 0xBD : 0xBC,0x07); go32();
			if (v) X.eax = idx; else X.eax = O.eax;
			X.efl = v ? (X.efl & ~(unsigned long)ZF) : (X.efl | ZF);
			if (!verify32(ZF|CTRL)) failr("66 0F %02X %s eax,[bx] %08lX",rev ? 0xBD : 0xBC,rev ? "bsr" : "bsf",v);

			v = v16[i];
			idx = 0;
			if (v) {
				if (rev) { for (k=15;!(v & (1ul << k));k--); }
				else { for (k=0;!(v & (1ul << k));k++); }
				idx = k;
			}
			setup32(); I.eax = 0xBBBB0000ul; I.ebx = v; EB(3, 0x0F,rev ? 0xBD : 0xBC,0xC3); go32();
			if (v) X.eax = 0xBBBB0000ul | idx; else X.eax = O.eax;
			X.efl = v ? (X.efl & ~(unsigned long)ZF) : (X.efl | ZF);
			if (!verify32(ZF|CTRL)) failr("0F %02X %s ax,bx %04lX",rev ? 0xBD : 0xBC,rev ? "bsr" : "bsf",v);
		}
	}
	end();
}

/* ---- SETcc and Jcc near */
static int cc_true(unsigned cc,unsigned long f) {
	int of = (f & OF) ? 1 : 0,sf = (f & SF) ? 1 : 0,zf = (f & ZF) ? 1 : 0,cf = (f & CF) ? 1 : 0,pf = (f & PF) ? 1 : 0,t = 0;

	switch (cc >> 1) {
		case 0: t = of; break;
		case 1: t = cf; break;
		case 2: t = zf; break;
		case 3: t = cf | zf; break;
		case 4: t = sf; break;
		case 5: t = pf; break;
		case 6: t = sf ^ of; break;
		case 7: t = (sf ^ of) | zf; break;
	}
	return (cc & 1) ? !t : t;
}

static void t_setcc(void) {
	unsigned cc,i;
	unsigned long f;
	int t;

	begin("SETcc/Jcc");
	for (cc=0;cc < 16;cc++) {
		for (i=0;i < 32;i++) {
			f = F32 | ((i & 1) ? OF : 0) | ((i & 2) ? SF : 0) | ((i & 4) ? ZF : 0) | ((i & 8) ? CF : 0) | ((i & 16) ? PF : 0);
			t = cc_true(cc,f);

			setup32(); I.efl = f; I.eax = 0xEEEEEEEEul; EB(3, 0x0F,0x90+cc,0xC0); go32();
			X.eax = 0xEEEEEE00ul | (unsigned long)t;
			if (!verify32(CMASK)) failr("0F %02X setcc al flags=%04lX",0x90+cc,f);

			setup32(); I.efl = f; memw[0] = 0x5555; I.ebx = memw_off; EB(3, 0x0F,0x90+cc,0x07); go32();
			if (!verify32(CMASK) || !checkm(memw[0] == (0x5500u | (unsigned)t))) failr("0F %02X setcc [bx] flags=%04lX mem=%04X",0x90+cc,f,memw[0]);

			/* jcc rel16 forward: mov al,0; jcc +2; mov al,1 */
			setup32(); I.efl = f; EB(4, 0xB0,0x00,0x0F,0x80+cc); EW(2); EB(2, 0xB0,0x01); go32();
			X.eax = (I.eax & 0xFFFFFF00ul) | (t ? 0 : 1);
			if (!verify32(CMASK)) failr("0F %02X jcc rel16 forward flags=%04lX",0x80+cc,f);

			/* jcc rel16 backward: mov al,0; jmp +1; retf; jcc -5; mov al,1 */
			setup32(); I.efl = f; EB(7, 0xB0,0x00,0xEB,0x01,0xCB,0x0F,0x80+cc); EW(0xFFFBu); EB(2, 0xB0,0x01); go32();
			X.eax = (I.eax & 0xFFFFFF00ul) | (t ? 0 : 1);
			if (!verify32(CMASK)) failr("0F %02X jcc rel16 backward flags=%04lX",0x80+cc,f);

			/* jcc rel32 with operand size prefix */
			setup32(); I.efl = f; EB(5, 0xB0,0x00,0x66,0x0F,0x80+cc); ED(2); EB(2, 0xB0,0x01); go32();
			X.eax = (I.eax & 0xFFFFFF00ul) | (t ? 0 : 1);
			if (!verify32(CMASK)) failr("66 0F %02X jcc rel32 flags=%04lX",0x80+cc,f);
		}
	}
	end();
}

/* ---- CWDE/CDQ */
static void t_cwde(void) {
	unsigned i;

	begin("CWDE/CDQ");
	for (i=0;i < 16;i++) {
		setup32(); I.eax = 0x5A5A0000ul | v16[i]; EB(2, 0x66,0x98); go32(); X.eax = sext(v16[i],1);
		if (!verify32(CMASK)) failr("66 98 cwde ax=%04X",v16[i]);
		setup32(); I.eax = v32[i]; EB(2, 0x66,0x99); go32(); X.edx = (v32[i] & 0x80000000ul) ? 0xFFFFFFFFul : 0;
		if (!verify32(CMASK)) failr("66 99 cdq eax=%08lX",v32[i]);
	}
	end();
}

/* ---- PUSH/POP 32-bit, PUSHAD/POPAD, segment registers FS/GS */
static void t_push32(void) {
	static const unsigned char order[8] = { 7,6,5,4,3,2,1,0 };		/* EDI ESI EBP ESP EBX EDX ECX EAX */
	unsigned s,d,k;
	unsigned long e;
	int ok;

	begin("PUSH/POP32");
	for (s=0;s < 8;s++) {
		for (d=0;d < 8;d++) {
			if (s == 4 || d == 4) continue;
			setup32(); EB(4, 0x66,0x50+s,0x66,0x58+d); go32(); *reg32(&X,d) = *reg32(&I,s);
			if (!verify32(CMASK)) failr("push e%u; pop e%u",s,d);
		}
	}
	/* push eax; mov bp,sp; mov edx,[bp]; pop ecx */
	setup32(); EB(9, 0x66,0x50,0x8B,0xEC,0x66,0x8B,0x56,0x00,0x66); E(0x59); go32();
	X.edx = X.ecx = I.eax; X.ebp = (I.ebp & 0xFFFF0000ul) | ((esp_b - 8ul) & 0xFFFFul);
	if (!verify32(CMASK)) failr("66 50 push eax stack image");

	setup32(); EB(2, 0x66,0x68); ED(0xCAFEBABEul); EB(2, 0x66,0x58); go32(); X.eax = 0xCAFEBABEul;
	if (!verify32(CMASK)) failr("66 68 push imm32");
	setup32(); EB(5, 0x66,0x6A,0x80,0x66,0x58); go32(); X.eax = 0xFFFFFF80ul;
	if (!verify32(CMASK)) failr("66 6A push simm8 dword");
	setup32(); memd[0] = 0x31415926ul; I.ebx = memw_off; EB(5, 0x66,0xFF,0x37,0x66,0x58); go32(); X.eax = 0x31415926ul;
	if (!verify32(CMASK)) failr("66 FF 37 push dword [bx]");
	setup32(); memd[0] = 0; I.ebx = memw_off; EB(5, 0x66,0x50,0x66,0x8F,0x07); go32();
	if (!verify32(CMASK) || !checkm(memd[0] == I.eax)) failr("66 8F 07 pop dword [bx]");

	/* FS/GS push/pop */
	setup32(); I.fs = 0x1234; EB(3, 0x0F,0xA0,0x58); go32(); X.eax = (I.eax & 0xFFFF0000ul) | 0x1234u;
	if (!verify32(CMASK)) failr("0F A0 push fs");
	setup32(); I.gs = 0x4321; EB(3, 0x0F,0xA8,0x5B); go32(); X.ebx = (I.ebx & 0xFFFF0000ul) | 0x4321u;
	if (!verify32(CMASK)) failr("0F A8 push gs");
	setup32(); I.eax = 0x2222; EB(3, 0x50,0x0F,0xA1); go32(); X.fs = 0x2222;
	if (!verify32(CMASK)) failr("0F A1 pop fs");
	setup32(); I.eax = 0x3333; EB(3, 0x50,0x0F,0xA9); go32(); X.gs = 0x3333;
	if (!verify32(CMASK)) failr("0F A9 pop gs");
	setup32(); I.fs = 0x1234; EB(5, 0x66,0x0F,0xA0,0x66,0x58); go32(); X.eax = (O.eax & 0xFFFF0000ul) | 0x1234u;
	if (!verify32(CMASK)) failr("66 0F A0 push fs (32-bit, upper word undefined)");

	/* PUSHAD, then pop the image into memory */
	setup32(); EB(2, 0x66,0x60); for (k=0;k < 8;k++) { EB(3, 0x66,0x8F,0x06); EW(memw_off + k*4u); } go32();
	ok = verify32(CMASK);
	for (k=0;k < 8;k++) {
		e = (order[k] == 4) ? (esp_b - 4ul) : *reg32(&I,order[k]);
		if (!checkm(memd[k] == e)) { ok = 0; sprintf(diffbuf+strlen(diffbuf)," %s=%08lX(%08lX)",rn32[order[k]],memd[k],e); }
	}
	if (!ok) failr("66 60 pushad image");

	/* push 8 dwords, POPAD */
	for (k=0;k < 8;k++) memd[k] = 0x11111111ul * (k + 1);
	setup32(); for (k=0;k < 8;k++) { EB(3, 0x66,0xFF,0x36); EW(memw_off + k*4u); } EB(2, 0x66,0x61); go32();
	X.eax = memd[0]; X.ecx = memd[1]; X.edx = memd[2]; X.ebx = memd[3];
	X.ebp = memd[5]; X.esi = memd[6]; X.edi = memd[7];
	if (!verify32(CMASK)) failr("66 61 popad");
	end();
}

/* ---- PUSHFD/POPFD */
static void t_pushfd(void) {
	begin("PUSHFD/POPFD");
	setup32(); I.efl = 0x0ED7ul & ~(unsigned long)TF; EB(4, 0x66,0x9C,0x66,0x58); go32(); X.eax = O.eax;
	if (!verify32(CMASK) || !checkm((O.eax & 0x0FD5ul) == (I.efl & 0x0FD5ul)) || !checkm((O.eax & 0x0003802Aul) == 0x2ul))
		failr("pushfd image %08lX",O.eax);

	setup32(); EB(2, 0x66,0x68); ED(0x00037202ul); EB(6, 0x66,0x9D,0x66,0x9C,0x66,0x58); go32(); X.eax = O.eax; X.efl = O.efl;
	if (!checkm((O.eax & 0x00030000ul) == 0)) failm("popfd set VM or RF: %08lX",O.eax);
	if (!checkm((O.eax & 0x7000ul) == 0x7000ul)) failm("real mode popfd IOPL/NT: %08lX",O.eax);
	end();
}

/* ---- MOV to/from FS and GS, overrides, LFS/LGS/LSS */
static void t_fsgs(void) {
	unsigned alt,T = memw_off + 0x40;

	begin("FS/GS/LxS");
	setup32(); I.eax = 0x1234; EB(4, 0x8E,0xE0,0x8C,0xE3); go32(); X.fs = 0x1234; X.ebx = (I.ebx & 0xFFFF0000ul) | 0x1234u;
	if (!verify32(CMASK)) failr("8E E0 / 8C E3 mov fs / from fs");
	setup32(); I.eax = 0x4321; EB(4, 0x8E,0xE8,0x8C,0xEB); go32(); X.gs = 0x4321; X.ebx = (I.ebx & 0xFFFF0000ul) | 0x4321u;
	if (!verify32(CMASK)) failr("8E E8 / 8C EB mov gs / from gs");

	/* fs:/gs: overrides through an alias segment */
	alt = dgroup + 1;					/* alt:T-16 = DGROUP:T */
	memw[0x20] = 0x7E57;
	setup32(); I.fs = alt; I.ebx = T - 16; EB(3, 0x64,0x8B,0x07); go32(); X.eax = (I.eax & 0xFFFF0000ul) | 0x7E57u;
	if (!verify32(CMASK)) failr("64 8B 07 mov ax,fs:[bx]");
	setup32(); I.gs = alt; I.ebx = T - 16; EB(3, 0x65,0x8B,0x07); go32(); X.eax = (I.eax & 0xFFFF0000ul) | 0x7E57u;
	if (!verify32(CMASK)) failr("65 8B 07 mov ax,gs:[bx]");
	setup32(); I.gs = alt; I.ebx = T - 16; I.eax = 0xBEEF; EB(3, 0x65,0x89,0x07); go32();
	if (!verify32(CMASK) || !checkm(memw[0x20] == 0xBEEF)) failr("65 89 07 mov gs:[bx],ax");

	/* LFS/LGS m16:16 and m16:32 */
	memw[0] = 0x1357; memw[1] = 0x2468;
	setup32(); I.ebx = memw_off; EB(3, 0x0F,0xB4,0x37); go32(); X.esi = (I.esi & 0xFFFF0000ul) | 0x1357u; X.fs = 0x2468;
	if (!verify32(CMASK)) failr("0F B4 37 lfs si,[bx]");
	setup32(); I.ebx = memw_off; EB(3, 0x0F,0xB5,0x3F); go32(); X.edi = (I.edi & 0xFFFF0000ul) | 0x1357u; X.gs = 0x2468;
	if (!verify32(CMASK)) failr("0F B5 3F lgs di,[bx]");
	memd[0] = 0x9ABCDEF0ul; memw[2] = 0x3579;
	setup32(); I.ebx = memw_off; EB(4, 0x66,0x0F,0xB4,0x37); go32(); X.esi = 0x9ABCDEF0ul; X.fs = 0x3579;
	if (!verify32(CMASK)) failr("66 0F B4 37 lfs esi,[bx] (m16:32)");

	/* LSS: load an alias of the current stack (seg-1, sp+16), read it back, restore */
	memw[1] = ss_val - 1;
	setup32(); I.ebx = memw_off;
	EB(4, 0x8C,0xD0,0x89,0xE1);					/* mov ax,ss; mov cx,sp */
	EB(5, 0x89,0xE2,0x83,0xC2,0x10); EB(2, 0x89,0x17);		/* mov dx,sp; add dx,16; mov [bx],dx */
	EB(3, 0x0F,0xB2,0x27);						/* lss sp,[bx] */
	EB(4, 0x8C,0xD7,0x89,0xE6);					/* mov di,ss; mov si,sp */
	EB(4, 0x8E,0xD0,0x89,0xCC);					/* mov ss,ax; mov sp,cx */
	go32();
	X.eax = (I.eax & 0xFFFF0000ul) | ss_val; X.ecx = O.ecx; X.edx = O.edx;
	X.edi = (I.edi & 0xFFFF0000ul) | (unsigned)(ss_val - 1);
	X.esi = (I.esi & 0xFFFF0000ul) | ((esp_b - 4ul + 16ul) & 0xFFFFul);
	if (!verify32(CTRL)) failr("0F B2 27 lss sp,[bx]");
	end();
}

/* ---- 32-bit addressing: LEA with every ModR/M and SIB form */
static unsigned long ea_base(unsigned r,unsigned long esp_in) {
	return (r == 4) ? esp_in : *reg32(&I,r);
}

static void t_lea32(void) {
	static const unsigned long rset[2][8] = {
		{ 0x00001000ul,0x00000100ul,0x00000010ul,0x00000001ul,0,0x00002000ul,0x00000300ul,0x00000030ul },
		{ 0x80000000ul,0x7FFFFFF0ul,0xFFFFFFFFul,0x12345678ul,0,0xFEDCBA98ul,0x00010000ul,0x40000000ul }
	};
	static const unsigned long d8v[2] = { 0x7F,0x80 };
	static const unsigned long d32v[2] = { 0x12345678ul,0xFFFFFFF0ul };
	unsigned set,mod,rm,base,index,scale,dv,r;
	unsigned long ea,disp,esp_in;

	begin("LEA32 EA");
	for (set=0;set < 2;set++) {
		for (mod=0;mod < 3;mod++) {
			for (rm=0;rm < 8;rm++) {
				for (base=0;base < (rm == 4 ? 8u : 1u);base++) {
					for (index=0;index < (rm == 4 ? 8u : 1u);index++) {
						for (scale=0;scale < (rm == 4 ? 4u : 1u);scale++) {
							for (dv=0;dv < 2;dv++) {
								setup32();
								for (r=0;r < 8;r++) if (r != 4) *reg32(&I,r) = rset[set][r];
								I.edx = 0x5A5A5A5Aul;
								EB(4, 0x67,0x66,0x8D,(mod<<6)|(2<<3)|rm);
								if (rm == 4) E((scale<<6)|(index<<3)|base);
								disp = 0;
								if (mod == 1) { disp = sext(d8v[dv],0); E((unsigned)d8v[dv]); }
								else if (mod == 2 || (mod == 0 && rm == 5) || (mod == 0 && rm == 4 && base == 5)) { disp = d32v[dv]; ED(disp); }
								go32();
								esp_in = esp_b - 4ul;
								if (rm == 4) {
									ea = (mod == 0 && base == 5) ? 0 : ea_base(base,esp_in);
									if (index != 4) ea += *reg32(&I,index) << scale;
								}
								else if (mod == 0 && rm == 5) ea = 0;
								else ea = ea_base(rm,esp_in);
								X.edx = ea + disp;
								if (!verify32(CMASK)) failr("67 66 8D lea edx mod=%u rm=%u sib=%u/%u/%u disp=%08lX",mod,rm,scale,index,base,disp);
							}
						}
					}
				}
			}
		}
	}

	/* 16-bit address with 32-bit destination is zero extended */
	setup32(); I.ebx = 0xFFFFFFF0ul; I.esi = 0x00000020ul; EB(4, 0x66,0x8D,0x50,0x05); go32(); X.edx = 0x00000015ul;
	if (!verify32(CMASK)) failr("66 8D 50 05 lea edx,[bx+si+5] (16-bit address)");
	end();
}

static unsigned char sbuf[64];
static unsigned char __far dbuf[64];

/* ---- 32-bit addressing memory accesses, default segment, offset limit */
static void t_addr32(void) {
	unsigned T = memw_off + 0x40,alt,s;
	unsigned long ssw;

	begin("Addr32 mem");
	{
		/* alt:T addresses dbuf */
		unsigned long l = ((unsigned long)FP_SEG(dbuf) << 4ul) + FP_OFF(dbuf);
		alt = (unsigned)(((l + 15ul) >> 4ul) - (unsigned long)(T >> 4u));
	}
	memd[2] = 0x11223344ul; memd[3] = 0x55667788ul;
	setup32(); I.esi = memw_off; I.ebx = 8; EB(5, 0x67,0x66,0x8B,0x04,0x1E); go32(); X.eax = 0x11223344ul;
	if (!verify32(CMASK)) failr("67 66 8B 04 1E mov eax,[esi+ebx]");
	setup32(); I.edi = memw_off; I.ecx = 3; EB(6, 0x67,0x66,0x8B,0x44,0x8F,0x00); go32(); X.eax = 0x55667788ul;
	if (!verify32(CMASK)) failr("67 66 8B 44 8F 00 mov eax,[edi+ecx*4+0]");
	setup32(); EB(4, 0x67,0x66,0x8B,0x05); ED(memw_off + 8u); go32(); X.eax = 0x11223344ul;
	if (!verify32(CMASK)) failr("67 66 8B 05 mov eax,[disp32]");
	setup32(); I.ebx = memw_off; I.eax = 0xA1B2C3D4ul; EB(5, 0x67,0x66,0x89,0x43,0x0C); go32();
	if (!verify32(CMASK) || !checkm(memd[3] == 0xA1B2C3D4ul)) failr("67 66 89 43 0C mov [ebx+12],eax");

	/* EBP/ESP base default to SS: push ds; mov ds,cx (alias); mov ax,[ebp+0]; pop ds */
	ssw = *((unsigned short __far*)MK_FP(ss_val,T));
	*((unsigned short __far*)MK_FP(alt,T)) = (unsigned short)(ssw ^ 0x5AA5u);
	setup32(); I.ecx = alt; I.ebp = T; EB(8, 0x1E,0x8E,0xD9,0x67,0x8B,0x45,0x00,0x1F); go32();
	X.eax = (I.eax & 0xFFFF0000ul) | ssw;
	if (!verify32(CMASK)) failr("67 8B 45 00 mov ax,[ebp] should use SS");
	setup32(); I.ecx = alt; I.ebx = T; EB(7, 0x1E,0x8E,0xD9,0x67,0x8B,0x03,0x1F); go32();
	X.eax = (I.eax & 0xFFFF0000ul) | ((ssw ^ 0x5AA5u) & 0xFFFFu);
	if (!verify32(CMASK)) failr("67 8B 03 mov ax,[ebx] should use DS");
	setup32(); I.ecx = alt; EB(8, 0x1E,0x8E,0xD9,0x67,0x8B,0x04,0x24,0x1F); go32();
	X.eax = (I.eax & 0xFFFF0000ul) | dgroup;			/* [esp] = the DS just pushed */
	if (!verify32(CMASK)) failr("67 8B 04 24 mov ax,[esp] should use SS");

	/* real mode offsets above FFFFh fault */
	setup32(); I.ebx = 0x00010000ul; s = here(); EB(3, 0x67,0x8B,0x03); go32x();
	if (!checkm(x_cnt == 1 && x_vec == 0x0D && x_ip == s)) failm("mov ax,[ebx=10000h]: INT 0Dh count=%u vec=%02X ip=%04X(%04X)",x_cnt,x_vec,x_ip,s);
	setup32(); I.ebx = 0x0000FFFFul; s = here(); EB(3, 0x67,0x8B,0x03); go32x();
	if (!checkm(x_cnt == 1 && x_vec == 0x0D && x_ip == s)) failm("mov ax,[ebx=FFFFh] (word past limit): INT 0Dh count=%u vec=%02X ip=%04X(%04X)",x_cnt,x_vec,x_ip,s);
	setup32(); I.ebp = 0x00010000ul; s = here(); EB(4, 0x67,0x8B,0x45,0x00); go32x();
	if (!checkm(x_cnt == 1 && x_vec == 0x0C && x_ip == s)) failm("mov ax,[ebp=10000h]: INT 0Ch count=%u vec=%02X ip=%04X(%04X)",x_cnt,x_vec,x_ip,s);
	end();
}

/* ---- string instructions, 32-bit operands and addresses */

static void t_str32(void) {
	unsigned i,S0 = FP_OFF(sbuf),D0 = FP_OFF(dbuf);
	unsigned long ex;

	begin("String32");
	for (i=0;i < 64;i++) { sbuf[i] = (unsigned char)(0x40 + i); dbuf[i] = 0xEE; }

	setup32(); I.es = FP_SEG(dbuf); I.esi = S0; I.edi = D0; EB(2, 0x66,0xA5); go32(); X.esi = S0 + 4; X.edi = D0 + 4;
	if (!verify32(CMASK) || !checkm(_fmemcmp(dbuf,sbuf,4) == 0 && dbuf[4] == 0xEE)) failr("66 A5 movsd");

	for (i=0;i < 64;i++) dbuf[i] = 0xEE;
	setup32(); I.es = FP_SEG(dbuf); I.esi = S0; I.edi = D0; I.ecx = 0xABCD0003ul; EB(3, 0xF3,0x66,0xA5); go32();
	X.esi = S0 + 12; X.edi = D0 + 12; X.ecx = 0xABCD0000ul;
	if (!verify32(CMASK) || !checkm(_fmemcmp(dbuf,sbuf,12) == 0 && dbuf[12] == 0xEE)) failr("F3 66 A5 rep movsd uses CX");

	for (i=0;i < 64;i++) dbuf[i] = 0xEE;
	setup32(); I.es = FP_SEG(dbuf); I.esi = S0; I.edi = D0; I.ecx = 3; EB(4, 0x67,0xF3,0x66,0xA5); go32();
	X.esi = S0 + 12; X.edi = D0 + 12; X.ecx = 0;
	if (!verify32(CMASK) || !checkm(_fmemcmp(dbuf,sbuf,12) == 0)) failr("67 F3 66 A5 rep movsd with ECX/ESI/EDI");

	setup32(); I.esi = S0 + 8; EB(2, 0x66,0xAD); go32(); X.eax = *((unsigned long*)(sbuf + 8)); X.esi = S0 + 12;
	if (!verify32(CMASK)) failr("66 AD lodsd");
	setup32(); I.esi = S0 + 8; I.efl |= DF; EB(3, 0x67,0x66,0xAD); go32(); X.eax = *((unsigned long*)(sbuf + 8)); X.esi = S0 + 4;
	if (!verify32(CMASK)) failr("67 66 AD lodsd [esi] DF=1");

	for (i=0;i < 64;i++) dbuf[i] = 0xEE;
	setup32(); I.es = FP_SEG(dbuf); I.edi = D0 + 4; I.eax = 0x12345678ul; I.ecx = 2; EB(3, 0xF3,0x66,0xAB); go32();
	X.edi = D0 + 12; X.ecx = (I.ecx & 0xFFFF0000ul);
	ex = *((unsigned long __far*)(dbuf + 8));
	if (!verify32(CMASK) || !checkm(ex == 0x12345678ul && dbuf[3] == 0xEE && dbuf[12] == 0xEE)) failr("F3 66 AB rep stosd");

	/* cmpsd / scasd */
	_fmemcpy(dbuf,sbuf,64); dbuf[9] = 0;
	setup32(); I.es = FP_SEG(dbuf); I.esi = S0; I.edi = D0; I.ecx = 8; EB(3, 0xF3,0x66,0xA7); go32();
	{
		unsigned long a = *((unsigned long*)(sbuf + 8)),b = *((unsigned long __far*)(dbuf + 8)),fo,fm;

		alu_ref(OP_CMP,2,a,b,I.efl,&fo,&fm);
		X.esi = S0 + 12; X.edi = D0 + 12; X.ecx = 5; X.efl = fo;
		if (!verify32(CMASK)) failr("F3 66 A7 repe cmpsd mismatch at dword 2");
	}
	setup32(); I.es = FP_SEG(dbuf); I.edi = D0; I.ecx = 16; I.eax = *((unsigned long __far*)(dbuf + 20)); EB(3, 0xF2,0x66,0xAF); go32();
	X.edi = D0 + 24; X.ecx = 10; X.efl = (I.efl & ~(unsigned long)ARITH) | ZF | PF;
	if (!verify32(CMASK)) failr("F2 66 AF repne scasd found at dword 5");
	end();
}

/* ---- LOOP/JECXZ with the address size prefix (ECX) */
static void t_loop32(void) {
	begin("LOOP/JECXZ");
	setup32(); I.ecx = 0x00010002ul; EB(2, 0xE2,0xFE); go32(); X.ecx = 0x00010000ul;
	if (!verify32(CMASK)) failr("E2 loop uses CX");
	setup32(); I.ecx = 0x00010002ul; EB(3, 0x67,0xE2,0xFD); go32(); X.ecx = 0;
	if (!verify32(CMASK)) failr("67 E2 loop uses ECX (65538 iterations)");
	setup32(); I.ecx = 0x00010000ul; EB(6, 0xB0,0x00,0xE3,0x02,0xB0,0x01); go32(); X.eax = I.eax & 0xFFFFFF00ul;
	if (!verify32(CMASK)) failr("E3 jcxz taken on CX=0");
	setup32(); I.ecx = 0x00010000ul; EB(7, 0xB0,0x00,0x67,0xE3,0x02,0xB0,0x01); go32(); X.eax = (I.eax & 0xFFFFFF00ul) | 1;
	if (!verify32(CMASK)) failr("67 E3 jecxz not taken on ECX=10000h");
	setup32(); I.ecx = 0; EB(7, 0xB0,0x00,0x67,0xE3,0x02,0xB0,0x01); go32(); X.eax = I.eax & 0xFFFFFF00ul;
	if (!verify32(CMASK)) failr("67 E3 jecxz taken on ECX=0");
	/* L: inc edx; cmp edx,ebx; loope L  (with ECX) */
	setup32(); I.ecx = 0x00010005ul; I.edx = 0; I.ebx = 1; EB(7, 0x66,0x42,0x66,0x3B,0xD3,0x67,0xE1); E(0xF8); go32();
	X.ecx = 0x00010003ul; X.edx = 2; X.efl = O.efl;
	if (!verify32(CTRL)) failr("67 E1 loope with ECX");
	end();
}

/* ---- ENTER/LEAVE, CALL/RET, IRETD with 32-bit operand size in real mode */
static void t_ctl32(void) {
	unsigned char __far *p;
	unsigned r,s;

	begin("CALL/RET32");
	/* enter 8,0 / leave with 32-bit operand size */
	setup32(); EB(5, 0x66,0xC8,0x08,0x00,0x00); EB(6, 0x66,0x8B,0xD5,0x66,0x8B,0xCC); EB(2, 0x66,0xC9); go32();
	X.edx = (O.edx & 0xFFFF0000ul) | ((esp_b - 8ul) & 0xFFFFul);
	X.ecx = (O.ecx & 0xFFFF0000ul) | ((esp_b - 16ul) & 0xFFFFul);
	if (!verify32(CMASK)) failr("66 C8 enter 8,0 / 66 C9 leave");

	/* call near with 32-bit operand: pushes EIP */
	setup32(); EB(2, 0xB0,0x00); EB(2, 0x66,0xE8); p = tp; ED(0); r = here(); EB(3, 0xB0,0x55,0xCB);
	*((unsigned long __far*)p) = (unsigned long)(here() - r);
	EB(3, 0x66,0x8B,0xEC); EB(4, 0x66,0x8B,0x4E,0x00); EB(2, 0x66,0xC3); go32();
	X.eax = (I.eax & 0xFFFFFF00ul) | 0x55; X.ecx = r; X.ebp = (O.ebp & 0xFFFF0000ul) | ((esp_b - 8ul) & 0xFFFFul);
	if (!verify32(CMASK)) failr("66 E8 call rel32 / 66 C3 ret");

	/* far call with 32-bit operand: pushes CS as a dword, then EIP */
	setup32(); EB(2, 0xB0,0x00); EB(2, 0x66,0x9A); p = tp; ED(0); EW(thunk_seg); r = here(); EB(3, 0xB0,0x55,0xCB);
	s = here(); *((unsigned long __far*)p) = s;
	EB(3, 0x66,0x8B,0xEC); EB(4, 0x66,0x8B,0x4E,0x00); EB(4, 0x66,0x8B,0x56,0x04); EB(2, 0x66,0xCB); go32();
	X.eax = (I.eax & 0xFFFFFF00ul) | 0x55; X.ecx = r; X.edx = (O.edx & 0xFFFF0000ul) | thunk_seg;
	X.ebp = (O.ebp & 0xFFFF0000ul) | ((esp_b - 12ul) & 0xFFFFul);
	if (!verify32(CMASK)) failr("66 9A call far m16:32 / 66 CB retf");

	/* ret imm16 with 32-bit operand */
	setup32(); EB(4, 0x66,0x50,0x66,0x50); EB(2, 0x66,0xE8); p = tp; ED(0); r = here(); EB(3, 0xB0,0x55,0xCB);
	*((unsigned long __far*)p) = (unsigned long)(here() - r); EB(4, 0x66,0xC2,0x08,0x00); go32();
	X.eax = (I.eax & 0xFFFFFF00ul) | 0x55;
	if (!verify32(CMASK)) failr("66 C2 08 00 ret 8 (32-bit)");

	/* IRETD in real mode: push dword flags, dword cs, dword ip */
	setup32(); I.efl = F32 | CF | ZF; EB(2, 0x66,0x68); ED(0x00000ED7ul & ~(unsigned long)TF); EB(2, 0x66,0x0E); EB(2, 0x66,0x68); p = tp; ED(0);
	EB(2, 0x66,0xCF); EB(3, 0xB0,0x01,0xCB); *((unsigned long __far*)p) = here(); EB(4, 0x66,0x9C,0x66,0x5B); go32();
	X.ebx = O.ebx; X.efl = O.efl;
	if (!verify32(CMASK) || !checkm((O.ebx & 0x0ED7ul) == (0x0ED7ul & ~(unsigned long)TF))) failr("66 CF iretd flags=%08lX",O.ebx);

	/* XLAT with 32-bit address (EBX) */
	sbuf[0x13] = 0x99;
	setup32(); I.ebx = FP_OFF(sbuf); I.eax = 0x13; EB(2, 0x67,0xD7); go32(); X.eax = 0x99;
	if (!verify32(CMASK)) failr("67 D7 xlat [ebx+al]");
	end();
}

/* ---- control and debug registers in real mode */
static void t_crdr(void) {
	unsigned r,s;
	unsigned long cr0;

	begin("CRx/DRx");
	setup32(); EB(3, 0x0F,0x20,0xC0); go32(); cr0 = O.eax; X.eax = O.eax;
	if (!verify32(CMASK) || !checkm((cr0 & 1) == 0)) failr("mov eax,cr0 = %08lX (PE should be 0)",cr0);
	info("CR0 in real mode = %08lX",cr0);
	setup32(); EB(6, 0x0F,0x20,0xC0,0x0F,0x22,0xC0); go32(); X.eax = O.eax;
	if (!verify32(CMASK)) failr("mov cr0,eax (same value)");
	setup32(); I.eax = 0x12345678ul; EB(6, 0x0F,0x22,0xD0,0x0F,0x20,0xD3); go32(); X.ebx = 0x12345678ul;
	if (!verify32(CMASK)) failr("mov cr2,eax / mov ebx,cr2");
	setup32(); I.eax = 0x00123000ul; EB(9, 0x0F,0x20,0xD9,0x0F,0x22,0xD8,0x0F,0x20,0xDB); EB(3, 0x0F,0x22,0xD9); go32();
	X.ebx = 0x00123000ul; X.ecx = O.ecx;
	if (!verify32(CMASK)) failr("mov cr3,eax / mov ebx,cr3");
	for (r=0;r < 4;r++) {
		setup32(); I.eax = 0x00012340ul + r * 0x111ul; EB(3, 0x0F,0x23,0xC0|(r<<3)); EB(3, 0x0F,0x21,0xC3|(r<<3));
		EB(3, 0x66,0x31,0xC9); EB(3, 0x0F,0x23,0xC1|(r<<3)); go32();		/* restore DRn = 0 */
		X.ebx = I.eax; X.ecx = 0;
		if (!verify32(CTRL)) failr("mov dr%u,eax / mov ebx,dr%u",r,r);
	}

	/* execute breakpoint on DR0 */
	setup32();
	EB(3, 0x66,0x31,0xD2); EB(3, 0x0F,0x23,0xF2);				/* xor edx,edx; mov dr6,edx */
	EB(2, 0x66,0xB8); { unsigned char __far *p = tp; ED(0);
	EB(3, 0x0F,0x23,0xC0);							/* mov dr0,eax */
	EB(2, 0x66,0xB8); ED(0x00000201ul); EB(3, 0x0F,0x23,0xF8);		/* mov dr7,201h */
	s = here(); *((unsigned long __far*)p) = ((unsigned long)thunk_seg << 4ul) + s; }
	EB(2, 0xB0,0x55);							/* breakpoint target: mov al,55h */
	x_land = here(); x_land_set = 1;
	EB(3, 0x0F,0x21,0xF3);							/* mov ebx,dr6 */
	EB(3, 0x66,0x31,0xD2); EB(6, 0x0F,0x23,0xFA,0x0F,0x23,0xF2);		/* dr7 = dr6 = 0 */
	go32x();
	if (!checkm(x_cnt == 1 && x_vec == 1 && x_ip == s && (O.ebx & 1))) failm("DR0 execute breakpoint: INT 1 count=%u ip=%04X(%04X) DR6=%08lX",x_cnt,x_ip,s,O.ebx);

	/* data write breakpoint on DR1, 4 bytes: trap after the write */
	setup32(); I.ebx = 0;
	EB(3, 0x66,0x31,0xD2); EB(3, 0x0F,0x23,0xF2);
	EB(2, 0x66,0xB8); ED(((unsigned long)dgroup << 4ul) + memw_off + 16u); EB(3, 0x0F,0x23,0xC8);	/* mov dr1,eax */
	EB(2, 0x66,0xB8); ED(0x00D00304ul); EB(3, 0x0F,0x23,0xF8);					/* mov dr7 */
	EB(2, 0x66,0xA3); EW(memw_off + 16u);							/* mov [memd+16],eax */
	s = here();
	EB(2, 0xB0,0x55);
	x_land = here(); x_land_set = 1;
	EB(3, 0x0F,0x21,0xF3);
	EB(3, 0x66,0x31,0xD2); EB(6, 0x0F,0x23,0xFA,0x0F,0x23,0xF2);
	go32x();
	if (!checkm(x_cnt == 1 && x_vec == 1 && x_ip == s && (O.ebx & 2))) failm("DR1 data write breakpoint: INT 1 count=%u ip=%04X(%04X) DR6=%08lX",x_cnt,x_ip,s,O.ebx);
	end();
}

/* ---- 486 instructions: #UD on a 386 */
static void t_486(void) {
	unsigned s;

	begin("486 instr");
	setup32(); I.eax = 0x11223344ul; s = here(); EB(3, 0x66,0x0F,0xC8); go32x();
	if (x_cnt) {
		info("BSWAP raises #UD (386)");
		if (!checkm(x_vec == 6 && x_ip == s)) failm("BSWAP: INT %02X at %04X, expected INT 6 at %04X",x_vec,x_ip,s);
		if (c_486) failm("486 or later but BSWAP raised #UD");
	}
	else {
		info("BSWAP supported (486 or later)");
		if (!checkm(O.eax == 0x44332211ul)) failm("BSWAP eax = %08lX",O.eax);
	}
	end();
}
/* ======================================================= PROTECTED MODE */

/* GDT selectors */
#define S_C32		0x08		/* code, 32-bit, DPL 0, base = pmcode */
#define S_C16		0x10		/* code, 16-bit, DPL 0, base = pmcode */
#define S_D		0x18		/* data, 16-bit, DGROUP, limit FFFFh (also real mode compatible) */
#define S_S32		0x20		/* data, B=1, DGROUP, limit FFFFh (ring 0 stack) */
#define S_FLAT		0x28		/* data, base 0, limit 4GB */
#define S_C32_3		0x30		/* code, 32-bit, DPL 3 */
#define S_D3		0x38		/* data, B=1, DPL 3, DGROUP (ring 3 data and stack) */
#define S_FLAT3		0x40		/* data, DPL 3, base 0, limit 4GB */
#define S_TSS0		0x48		/* 386 TSS with I/O bitmap */
#define S_TSS1		0x50		/* 386 TSS, task switch target */
#define S_GATE3		0x58		/* 386 call gate, DPL 3, 2 dword parameters */
#define S_CONF		0x60		/* conforming code, 32-bit, DPL 0 */
#define S_GLIM		0x68		/* data, G=1, limit 0 (= FFFh), base = tbuf */
#define S_EDB		0x70		/* expand-down, B=1, limit 0FFFh, base = tbuf - 1000h */
#define S_DSTK16	0x78		/* stack, B=0, limit FFFFh, offset FFFCh = stk1+80h */
#define S_S32B		0x80		/* stack, B=1, G=1, limit 4GB, offset FFFEh = stk1+80h */
#define S_DNP		0x88		/* data, not present */
#define GDT_N		18
#define S_BEYOND	0x01F8

#define IDT_N		0x50
#define V_INTG		0x40		/* 386 interrupt gate to h40 */
#define V_TRAPG		0x41		/* 386 trap gate to h40 */
#define V_INTG3		0x42		/* 386 interrupt gate, DPL 3, to h40 */
#define V_INT286	0x43		/* 286 interrupt gate to the 16-bit handler */
#define V_EXIT0		0x44		/* DPL 0: ring 3 code faults back to ring 0 through it */
#define V_EXIT3		0x45		/* DPL 3: V86 code leaves through it (IOPL 3) */
#define V_BEYOND	0x4A
#define IDT_SMALL_N	0x48

struct desc {
	unsigned short	lim;
	unsigned short	base0;
	unsigned char	base1;
	unsigned char	acc;
	unsigned char	flg;		/* G D 0 AVL, limit 19:16 (offset high byte for gates) */
	unsigned char	base2;
};

static struct desc		gdt[GDT_N];
static struct desc		idt[IDT_N];
static unsigned short		gdtr[3],idtr[3],idtr_small[3],idtr_zero[3],idtr_rm[3];
static unsigned char		tss0[104+129];
static unsigned char		tss1[104];
static unsigned long		stk0[512],stk1[64],stk3[256];
static unsigned char		tbuf[512];
static unsigned long		sx_save[4];
static unsigned long		t1_rec[4];
static unsigned long		dbvar[4];
static unsigned long		marker = 0x4D41524Bul;		/* "MARK" */

struct pmres {
	unsigned long	done,xcnt,vec,w[10];
	unsigned long	eax,ecx,edx,ebx,esp,ebp,esi,edi,efl;
	unsigned long	ds,es,fs,gs,ss,cr2,dr6;
	unsigned long	h[6],hfl,hss,hcnt;
};

#define MAX_CASES	32
static struct pmres		res[MAX_CASES];
static struct pmres		res_pre;
static unsigned			case_mark[MAX_CASES];
static unsigned			ncase;

static unsigned long		pm_cur_res,pm_saved_esp,pm_case,pm_xtotal;
static struct { unsigned long off; unsigned short sel; } pm_recover;
static unsigned char		pm_done;
static unsigned char		pm_unreal = 0;	/* leave FS with a 4GB limit when returning to real mode */

#define PMCODE_SIZE	12288
static unsigned char __far	pmcode[PMCODE_SIZE];
static unsigned char __far	*pp;
static unsigned			stub_off[IDT_N];
static unsigned			common_off,h40_off,h43_off,store_off,task1_off,task1_end,exit_off,gate3_off,conf_off;
static unsigned char __far	*pm_rec_ph;
static unsigned long		pmcode_lin,dgroup_lin;
static unsigned			io_ok_port,io_bad_port;

/* paging: page directory, one page table (identity 0-4MB), two test pages */
static unsigned			pg_seg = 0;
static unsigned long		pg_pd,pg_pt,pg_x,pg_y;

#define DOFF(x)		((unsigned long)FP_OFF(&(x)))
#define ROFF(f)		((unsigned long)offsetof(struct pmres,f))

static unsigned long lin(void __far *p) {
	return ((unsigned long)FP_SEG(p) << 4ul) + (unsigned long)FP_OFF(p);
}

static unsigned long __far *linp(unsigned long l) {
	return (unsigned long __far*)MK_FP((unsigned)(l >> 4ul),(unsigned)(l & 15ul));
}

/* ---- protected mode code emitter (offsets relative to pmcode = code segment base) */
static unsigned pc(void) {
	return FP_OFF(pp) - FP_OFF(pmcode);
}

static void P(unsigned b) {
	*pp++ = (unsigned char)b;
}

static void PW(unsigned w) {
	P(w & 0xFF);
	P(w >> 8);
}

static void PD(unsigned long d) {
	PW((unsigned)(d & 0xFFFFul));
	PW((unsigned)(d >> 16ul));
}

static void PB(unsigned n,...) {
	va_list va;

	va_start(va,n);
	while (n-- > 0) P(va_arg(va,unsigned));
	va_end(va);
}

static unsigned char __far *pph32(void) {
	unsigned char __far *p = pp;
	PD(0);
	return p;
}

static void pfix32(unsigned char __far *p,unsigned long v) {
	*((unsigned long __far*)p) = v;
}

/* rel32 to a target */
static void prel32(unsigned target) {
	PD((unsigned long)target - (unsigned long)(pc() + 4u));
}

/* 32-bit code helpers */
static void p_sreg(unsigned sreg,unsigned sel)	{ PB(2, 0x66,0xB8); PW(sel); PB(2, 0x8E,0xC0|(sreg<<3)); }
static void p_movr(unsigned r,unsigned long v)	{ P(0xB8+r); PD(v); }
static void p_flags(unsigned long v)		{ P(0x68); PD(v); P(0x9D); }
static void p_lidt(unsigned long a)		{ PB(3, 0x0F,0x01,0x1D); PD(a); }
static void p_movmem(unsigned long a,unsigned long v) { PB(2, 0xC7,0x05); PD(a); PD(v); }
static void p_bx_store(unsigned r,unsigned long f) { PB(2, 0x89,0x83|(r<<3)); PD(f); }	/* mov [ebx+f],r32 */
#define SR_ES 0
#define SR_SS 2
#define SR_DS 3
#define SR_FS 4
#define SR_GS 5

/* restore the ring 0 environment: segments, stack, IDT, debug registers, flags */
static void p_restore(void) {
	p_sreg(SR_DS,S_D); p_sreg(SR_ES,S_D); p_sreg(SR_FS,S_D); p_sreg(SR_GS,S_D);
	p_sreg(SR_SS,S_S32);
	PB(2, 0x8B,0x25); PD(DOFF(pm_saved_esp));			/* mov esp,[pm_saved_esp] */
	p_lidt(DOFF(idtr));
	PB(2, 0x31,0xC0); PB(6, 0x0F,0x23,0xF8,0x0F,0x23,0xF0);	/* dr7 = dr6 = 0 */
	PB(3, 0x6A,0x02,0x9D);						/* push 2; popfd */
}

/* fixed part: entry, exception stubs, handlers, the store routine, task target, exit */
static void pm_begin(void) {
	unsigned char __far *to_cases,*to_exit;
	unsigned v;

	pp = pmcode;
	ncase = 0;
	memset(res,0,sizeof(res));
	memset(case_mark,0,sizeof(case_mark));

	/* entry, offset 0 */
	p_sreg(SR_DS,S_D); p_sreg(SR_ES,S_D); p_sreg(SR_FS,S_D); p_sreg(SR_GS,S_D);
	p_sreg(SR_SS,S_S32);
	P(0xBC); PD(DOFF(stk0[480]));					/* mov esp,stack top */
	PB(2, 0x66,0xB8); PW(S_TSS0); PB(3, 0x0F,0x00,0xD8);		/* ltr ax */
	PB(2, 0x31,0xC0); PB(3, 0x0F,0x00,0xD0);			/* lldt 0 */
	PB(3, 0x6A,0x02,0x9D);
	P(0xE9); to_cases = pp; PD(0);

	/* stubs: push vector, jump to the common handler */
	common_off = pc() + IDT_N * 7u;
	for (v=0;v < IDT_N;v++) {
		stub_off[v] = pc();
		PB(2, 0x6A,v); P(0xE9); prel32(common_off);
	}

	/* common handler: record registers, vector, 10 dwords of the frame, CR2, DR6, resume at the case recovery point */
	PB(5, 0x55,0x8B,0xEC,0x1E,0x53);
	PB(2, 0x66,0xBB); PW(S_D); PB(2, 0x8E,0xDB);
	PB(2, 0x8B,0x1D); PD(DOFF(pm_cur_res));
	p_bx_store(0,ROFF(eax)); p_bx_store(1,ROFF(ecx)); p_bx_store(2,ROFF(edx));
	p_bx_store(6,ROFF(esi)); p_bx_store(7,ROFF(edi));
	PB(2, 0x8F,0x83); PD(ROFF(ebx));
	PB(2, 0x8F,0x83); PD(ROFF(ds));
	PB(2, 0x8C,0x83); PD(ROFF(es));
	PB(2, 0x8C,0xA3); PD(ROFF(fs));
	PB(2, 0x8C,0xAB); PD(ROFF(gs));
	PB(3, 0x8B,0x45,0x00); p_bx_store(0,ROFF(ebp));
	PB(3, 0x8B,0x45,0x04); p_bx_store(0,ROFF(vec));
	for (v=0;v < 10;v++) { PB(3, 0x8B,0x45,8+v*4); p_bx_store(0,ROFF(w) + v*4ul); }
	PB(3, 0x0F,0x20,0xD0); p_bx_store(0,ROFF(cr2));
	PB(3, 0x0F,0x21,0xF0); p_bx_store(0,ROFF(dr6));
	PB(2, 0xFF,0x83); PD(ROFF(xcnt));
	PB(2, 0xFF,0x05); PD(DOFF(pm_xtotal));
	PB(2, 0x83,0x3D); PD(DOFF(pm_xtotal)); P(64);
	PB(2, 0x72,0x05);						/* jb +5 */
	P(0xE9); to_exit = pp; PD(0);					/* too many exceptions: leave */
	p_restore();
	PB(2, 0xFF,0x2D); PD(DOFF(pm_recover));			/* jmp far [pm_recover] */

	/* INT test handler (386 gate): frame, own flags, SS; ESI = 42424242h; IRETD */
	h40_off = pc();
	PB(6, 0x55,0x8B,0xEC,0x1E,0x53,0x50);
	PB(2, 0x66,0xBB); PW(S_D); PB(2, 0x8E,0xDB);
	PB(2, 0x8B,0x1D); PD(DOFF(pm_cur_res));
	PB(3, 0x9C,0x8F,0x83); PD(ROFF(hfl));
	for (v=0;v < 6;v++) { PB(3, 0x8B,0x45,4+v*4); p_bx_store(0,ROFF(h) + v*4ul); }
	PB(3, 0x66,0x8C,0xD0); PB(3, 0x0F,0xB7,0xC0); p_bx_store(0,ROFF(hss));	/* mov ax,ss; movzx eax,ax */
	PB(2, 0xFF,0x83); PD(ROFF(hcnt));
	PB(4, 0x58,0x5B,0x1F,0x5D);
	p_movr(6,0x42424242ul);
	P(0xCF);

	/* INT test handler for the 286 gate: 16-bit code, 16-bit frame; ESI = 43434343h; IRET */
	h43_off = pc();
	PB(6, 0x55,0x8B,0xEC,0x1E,0x66,0x53); P(0x50);
	P(0xBB); PW(S_D); PB(2, 0x8E,0xDB);
	PB(3, 0x66,0x8B,0x1E); PW((unsigned)DOFF(pm_cur_res));
	PB(2, 0x9C,0x58); PB(3, 0x67,0x89,0x83); PD(ROFF(hfl));
	for (v=0;v < 3;v++) { PB(3, 0x8B,0x46,2+v*2); PB(3, 0x67,0x89,0x83); PD(ROFF(h) + v*4ul); }
	PB(4, 0x67,0x66,0xFF,0x83); PD(ROFF(hcnt));
	PB(5, 0x58,0x66,0x5B,0x1F,0x5D);
	PB(2, 0x66,0xBE); PD(0x43434343ul);
	P(0xCF);

	/* store the registers into the current slot (far call from 16 or 32-bit code, ring 0) */
	store_off = pc();
	PB(3, 0x9C,0x1E,0x53);
	PB(2, 0x66,0xBB); PW(S_D); PB(2, 0x8E,0xDB);
	PB(2, 0x8B,0x1D); PD(DOFF(pm_cur_res));
	p_bx_store(0,ROFF(eax)); p_bx_store(1,ROFF(ecx)); p_bx_store(2,ROFF(edx));
	p_bx_store(5,ROFF(ebp)); p_bx_store(6,ROFF(esi)); p_bx_store(7,ROFF(edi));
	PB(2, 0x8F,0x83); PD(ROFF(ebx));
	PB(2, 0x8F,0x83); PD(ROFF(ds));
	PB(2, 0x8F,0x83); PD(ROFF(efl));
	PB(2, 0x8C,0x83); PD(ROFF(es));
	PB(2, 0x8C,0xA3); PD(ROFF(fs));
	PB(2, 0x8C,0xAB); PD(ROFF(gs));
	PB(2, 0x8C,0x93); PD(ROFF(ss));
	p_bx_store(4,ROFF(esp));
	PB(2, 0xC7,0x83); PD(ROFF(done)); PD(1);
	P(0xCB);

	/* task switch target: flags, TR, EAX, ESI, then IRETD back (nested task) */
	task1_off = pc();
	PB(2, 0x9C,0x5A);
	PB(3, 0x0F,0x00,0xCB);
	PB(2, 0x89,0x15); PD(DOFF(t1_rec[0]));
	PB(2, 0x89,0x1D); PD(DOFF(t1_rec[1]));
	P(0xA3); PD(DOFF(t1_rec[2]));
	PB(2, 0x89,0x35); PD(DOFF(t1_rec[3]));
	P(0xCF);
	task1_end = pc();

	/* conforming code target: mov dx,cs; retf */
	conf_off = pc();
	PB(4, 0x66,0x8C,0xCA,0xCB);

	/* exit: paging off, CR3/DR7 0, TS clear, to 16-bit code, real mode compatible segments, PE off */
	exit_off = pc();
	pfix32(to_exit,(unsigned long)exit_off - (unsigned long)(FP_OFF(to_exit) - FP_OFF(pmcode) + 4u));
	PB(3, 0x0F,0x20,0xC0); P(0x25); PD(0x7FFFFFFFul); PB(3, 0x0F,0x22,0xC0);
	PB(2, 0x31,0xC0); PB(3, 0x0F,0x22,0xD8); PB(3, 0x0F,0x23,0xF8); PB(2, 0x0F,0x06);
	P(0xEA); PD(pc() + 6u); PW(S_C16);
	/* 16-bit code from here */
	P(0xB8); PW(S_D);
	PB(10, 0x8E,0xD8,0x8E,0xC0,0x8E,0xE0,0x8E,0xE8,0x8E,0xD0);
	if (pm_unreal) { P(0xB8); PW(S_FLAT); PB(2, 0x8E,0xE0); }
	PB(3, 0x0F,0x20,0xC0); PB(2, 0x24,0xFE); PB(3, 0x0F,0x22,0xC0);
	P(0xEA); PW(rm_resume); PW(FP_SEG(rmcode));

	pfix32(to_cases,(unsigned long)pc() - (unsigned long)(FP_OFF(to_cases) - FP_OFF(pmcode) + 4u));
}

static void case_begin(void) {
	unsigned id = ncase++;

	p_movmem(DOFF(pm_cur_res),DOFF(res[id]));
	p_movmem(DOFF(pm_case),id);
	PB(2, 0xC7,0x05); PD(DOFF(pm_recover.off)); pm_rec_ph = pph32();
	PB(3, 0x66,0xC7,0x05); PD(DOFF(pm_recover.sel)); PW(S_C32);
	PB(2, 0x89,0x25); PD(DOFF(pm_saved_esp));
}

static void case_rec(void) {
	pfix32(pm_rec_ph,pc());
	p_restore();
}

/* finish a ring 0 case: store registers (far call), recovery point */
static void case_end(void) {
	P(0x9A); PD(store_off); PW(S_C32);
	case_rec();
}

/* finish a case that left ring 0 (ring 3, V86): leave through INT 44h, which is a #GP below ring 0 */
static void case_end_int(unsigned v) {
	PB(2, 0xCD,v);
	case_rec();
}

/* ring 0 -> ring 3 at the code that follows */
static void p_to_ring3(unsigned long esp3,unsigned long iopl) {
	p_flags(0x00000002ul | (iopl << 12));
	P(0x68); PD(S_D3|3);
	P(0x68); PD(esp3);
	P(0x68); PD(S_C32_3|3);
	P(0x68); PD(pc() + 5u);
	P(0xCB);
}

/* ring 0 -> V86 mode at the code that follows (16-bit code, executed at FP_SEG(pmcode):FP_OFF(pmcode)+offset) */
static void p_to_v86(unsigned long iopl,unsigned fs,unsigned gs) {
	P(0x68); PD(gs);
	P(0x68); PD(fs);
	P(0x68); PD(dgroup);
	P(0x68); PD(dgroup);
	P(0x68); PD(dgroup);
	P(0x68); PD(DOFF(stk3[240]));
	P(0x68); PD(0x00020002ul | (iopl << 12));
	P(0x68); PD(FP_SEG(pmcode));
	P(0x68); PD((unsigned long)FP_OFF(pmcode) + pc() + 5u);
	P(0xCF);
}

/* V86 view of a code offset */
static unsigned v86ip(unsigned off) {
	return FP_OFF(pmcode) + off;
}

static void set_seg(struct desc *d,unsigned long base,unsigned long limit,unsigned char acc,unsigned char flags) {
	d->lim = (unsigned short)(limit & 0xFFFFul);
	d->base0 = (unsigned short)(base & 0xFFFFul);
	d->base1 = (unsigned char)((base >> 16ul) & 0xFFul);
	d->acc = acc;
	d->flg = (unsigned char)((flags << 4) | ((limit >> 16ul) & 0x0Ful));
	d->base2 = (unsigned char)(base >> 24ul);
}

static void set_gate(struct desc *d,unsigned sel,unsigned long off,unsigned char acc,unsigned char count) {
	d->lim = (unsigned short)(off & 0xFFFFul);
	d->base0 = sel;
	d->base1 = count;
	d->acc = acc;
	d->flg = (unsigned char)((off >> 16ul) & 0xFFul);
	d->base2 = (unsigned char)(off >> 24ul);
}

static void set_pseudo(unsigned short *p,unsigned long base,unsigned limit) {
	p[0] = limit;
	p[1] = (unsigned short)(base & 0xFFFFul);
	p[2] = (unsigned short)(base >> 16ul);
}

#define TSS_ESP0	0x04
#define TSS_SS0		0x08
#define TSS_CR3		0x1C
#define TSS_EIP		0x20
#define TSS_EFL		0x24
#define TSS_EAX		0x28
#define TSS_ESP		0x38
#define TSS_ES		0x48
#define TSS_CS		0x4C
#define TSS_SS		0x50
#define TSS_DS		0x54
#define TSS_FS		0x58
#define TSS_GS		0x5C
#define TSS_IOMAP	0x66

static void tssd(unsigned char *t,unsigned o,unsigned long v) { *((unsigned long*)(t + o)) = v; }
static unsigned long tssr(unsigned char *t,unsigned o) { return *((unsigned long*)(t + o)); }

static void pm_tables(void) {
	unsigned long tl = lin(tbuf),sl = lin(stk1);
	unsigned v;

	memset(gdt,0,sizeof(gdt));
	set_seg(&gdt[S_C32>>3],pmcode_lin,0xFFFF,0x9A,0x4);
	set_seg(&gdt[S_C16>>3],pmcode_lin,0xFFFF,0x9A,0x0);
	set_seg(&gdt[S_D>>3],dgroup_lin,0xFFFF,0x92,0x0);
	set_seg(&gdt[S_S32>>3],dgroup_lin,0xFFFF,0x92,0x4);
	set_seg(&gdt[S_FLAT>>3],0,0xFFFFF,0x92,0xC);
	set_seg(&gdt[S_C32_3>>3],pmcode_lin,0xFFFF,0xFA,0x4);
	set_seg(&gdt[S_D3>>3],dgroup_lin,0xFFFF,0xF2,0x4);
	set_seg(&gdt[S_FLAT3>>3],0,0xFFFFF,0xF2,0xC);
	set_seg(&gdt[S_TSS0>>3],lin(tss0),sizeof(tss0)-1,0x89,0x0);
	set_seg(&gdt[S_TSS1>>3],lin(tss1),sizeof(tss1)-1,0x89,0x0);
	set_gate(&gdt[S_GATE3>>3],S_C32,gate3_off,0xEC,2);
	set_seg(&gdt[S_CONF>>3],pmcode_lin,0xFFFF,0x9E,0x4);
	set_seg(&gdt[S_GLIM>>3],tl,0,0x92,0x8);
	set_seg(&gdt[S_EDB>>3],tl - 0x1000ul,0x0FFF,0x96,0x4);
	set_seg(&gdt[S_DSTK16>>3],sl + 0x80ul - 0xFFFCul,0xFFFF,0x92,0x0);
	set_seg(&gdt[S_S32B>>3],sl + 0x80ul - 0xFFFEul,0xFFFFF,0x92,0xC);
	set_seg(&gdt[S_DNP>>3],tl,0xFFFF,0x12,0x0);

	for (v=0;v < IDT_N;v++) set_gate(&idt[v],S_C32,stub_off[v],0x8E,0);
	set_gate(&idt[V_INTG],S_C32,h40_off,0x8E,0);
	set_gate(&idt[V_TRAPG],S_C32,h40_off,0x8F,0);
	set_gate(&idt[V_INTG3],S_C32,h40_off,0xEE,0);
	set_gate(&idt[V_INT286],S_C16,h43_off,0x86,0);
	set_gate(&idt[V_EXIT3],S_C32,stub_off[V_EXIT3],0xEE,0);

	/* TSS0: ring 0 stack, I/O bitmap for ports 0-3FFh, only io_ok_port allowed */
	memset(tss0,0,sizeof(tss0));
	tssd(tss0,TSS_ESP0,DOFF(stk0[500]));
	tssd(tss0,TSS_SS0,S_S32);
	*((unsigned short*)(tss0 + TSS_IOMAP)) = 104;
	memset(tss0 + 104,0xFF,129);
	tss0[104 + (io_ok_port >> 3)] &= (unsigned char)~(1u << (io_ok_port & 7));

	/* TSS1: task switch target */
	memset(tss1,0,sizeof(tss1));
	tssd(tss1,TSS_EIP,task1_off);
	tssd(tss1,TSS_EFL,0x00000002ul);
	for (v=0;v < 8;v++) tssd(tss1,TSS_EAX + v*4,0x11111111ul * (v + 1));
	tssd(tss1,TSS_ESP,DOFF(stk1[60]));
	tssd(tss1,TSS_ES,S_D); tssd(tss1,TSS_CS,S_C32); tssd(tss1,TSS_SS,S_S32);
	tssd(tss1,TSS_DS,S_D); tssd(tss1,TSS_FS,S_D); tssd(tss1,TSS_GS,S_D);
	*((unsigned short*)(tss1 + TSS_IOMAP)) = 104;

	set_pseudo(gdtr,lin(gdt),sizeof(gdt)-1);
	set_pseudo(idtr,lin(idt),sizeof(idt)-1);
	set_pseudo(idtr_small,lin(idt),IDT_SMALL_N*8-1);
	set_pseudo(idtr_zero,0,0);
	set_pseudo(idtr_rm,0,0x3FF);

	for (v=0;v < sizeof(tbuf);v++) tbuf[v] = (unsigned char)(v * 3 + 1);
	memset(sx_save,0,sizeof(sx_save));
	memset(t1_rec,0,sizeof(t1_rec));
	memset(dbvar,0,sizeof(dbvar));
}

/* real mode code that enters protected mode and gets control back */
static void build_pm_enter(void) {
	unsigned sv = rm_sv;

	pm_enter = (void (__far*)(void))MK_FP(FP_SEG(rmcode),rhere());
	RB(2, 0x66,0x9C); RB(2, 0x66,0x60); RB(6, 0x1E,0x06,0x0F,0xA0,0x0F,0xA8);
	R(0xFA);
	RB(3, 0x2E,0x8E,0x1E); RW(rm_dgv);
	RB(3, 0x2E,0x8C,0x16); RW(sv);					/* mov cs:[sv],ss */
	RB(3, 0x2E,0x89,0x26); RW(sv+2);				/* mov cs:[sv+2],sp */
	RB(4, 0x66,0x0F,0x01,0x16); RW(AOFF(gdtr));			/* lgdt (32-bit base) */
	RB(4, 0x66,0x0F,0x01,0x1E); RW(AOFF(idtr));			/* lidt */
	RB(3, 0x0F,0x20,0xC0); RB(2, 0x0C,0x01); RB(3, 0x0F,0x22,0xC0);	/* mov eax,cr0; or al,1; mov cr0,eax */
	RB(2, 0x66,0xEA); RW(0); RW(0); RW(S_C32);			/* jmp far S_C32:00000000 */

	rm_resume = rhere();
	RB(3, 0x2E,0x8E,0x16); RW(sv);
	RB(3, 0x2E,0x8B,0x26); RW(sv+2);
	RB(3, 0x2E,0x8E,0x1E); RW(rm_dgv);
	RB(4, 0x66,0x0F,0x01,0x1E); RW(AOFF(idtr_rm));			/* real mode IVT */
	RB(6, 0x0F,0xA9,0x0F,0xA1,0x07,0x1F); RB(2, 0x66,0x61); RB(2, 0x66,0x9D);
	R(0xCB);
}

/* end the protected mode code, run it, check that it came back cleanly */
static int pm_go(void) {
	unsigned char m1,m2;

	p_movmem(DOFF(pm_cur_res),DOFF(res_pre));
	PB(2, 0xC6,0x05); PD(DOFF(pm_done)); P(1);
	P(0xE9); prel32(exit_off);

	pm_tables();
	pm_done = 0;
	pm_case = 0xFFFFul;
	pm_xtotal = 0;
	pm_recover.off = exit_off;
	pm_recover.sel = S_C32;
	pm_cur_res = DOFF(res_pre);
	memset(&res_pre,0,sizeof(res_pre));
	log_commit();

	_disable();
	if (pc98) {
		m1 = inp(0x02); m2 = inp(0x0A);
		outp(0x02,0xFF); outp(0x0A,0xFF);
	}
	else {
		m1 = inp(0x21); m2 = inp(0xA1);
		outp(0x21,0xFF); outp(0xA1,0xFF);
		outp(0x70,0x8F); (void)inp(0x71);			/* NMI off */
	}

	pm_enter();

	if (pc98) {
		outp(0x02,m1); outp(0x0A,m2);
	}
	else {
		outp(0x70,0x0D); (void)inp(0x71);			/* NMI on */
		outp(0x21,m1); outp(0xA1,m2);
	}
	_enable();

	if (res_pre.xcnt) {
		failm("exception %02lXh outside of a test case (case %ld), frame %08lX %08lX %08lX %08lX",res_pre.vec,(long)pm_case,
			res_pre.w[0],res_pre.w[1],res_pre.w[2],res_pre.w[3]);
		return 0;
	}
	if (pm_xtotal >= 64) {
		failm("too many exceptions, gave up in case %ld",(long)pm_case);
		return 0;
	}
	if (!pm_done) {
		failm("protected mode did not finish (case %ld)",(long)pm_case);
		return 0;
	}
	return 1;
}

/* ------------------------------------------------------ result checks */
static int has_errcode(unsigned long vec) {
	return vec == 8 || (vec >= 10 && vec <= 14) || vec == 17;
}

static const char *pm_what;

static unsigned long r_ip(struct pmres *r) {
	return has_errcode(r->vec) ? r->w[1] : r->w[0];
}

static int ex_ok(unsigned i) {
	struct pmres *r = &res[i];

	if (!checkm(r->done && r->xcnt == 0)) {
		if (r->xcnt) failm("%s: unexpected exception %02lXh err=%04lX at %04lX:%08lX",pm_what,r->vec,
			has_errcode(r->vec) ? r->w[0] : 0ul,has_errcode(r->vec) ? r->w[2] : r->w[1],r_ip(r));
		else failm("%s: did not run",pm_what);
		return 0;
	}
	return 1;
}

/* err < 0: not checked */
static int ex_fault(unsigned i,unsigned long vec,long err,unsigned long ip) {
	struct pmres *r = &res[i];

	if (!checkm(r->xcnt == 1 && r->vec == vec)) {
		if (r->xcnt) failm("%s: exception %02lXh err=%04lX at %08lX, expected %02lXh",pm_what,r->vec,has_errcode(r->vec) ? r->w[0] : 0ul,r_ip(r),vec);
		else failm("%s: exception %02lXh not raised",pm_what,vec);
		return 0;
	}
	if (err >= 0 && !checkm((has_errcode(vec) ? r->w[0] : 0ul) == (unsigned long)err)) {
		failm("%s: exception %02lXh error code %04lX, expected %04lX",pm_what,vec,r->w[0],(unsigned long)err);
		return 0;
	}
	if (!checkm(r_ip(r) == ip)) {
		failm("%s: exception %02lXh at EIP=%08lX, expected %08lX",pm_what,vec,r_ip(r),ip);
		return 0;
	}
	return 1;
}

static int ex_eq(const char *name,unsigned long got,unsigned long exp) {
	if (!checkm(got == exp)) {
		failm("%s: %s=%08lX, expected %08lX",pm_what,name,got,exp);
		return 0;
	}
	return 1;
}

/* exit of a ring 3 case through INT 44h */
static int ex_exit3(unsigned i) {
	return ex_fault(i,13,V_EXIT0*8+2,res[i].w[1]);
}

/* ============================================================ PROTECTED MODE TESTS */

/* ---- entering protected mode, 32-bit code */
static void g_pm32(void) {
	unsigned c[8],n = 0;
	unsigned char __far *p;
	unsigned r;

	begin("PM 32-bit");
	pm_begin();

	case_begin(); c[n++] = ncase-1;
	PB(3, 0x0F,0x20,0xC0);						/* mov eax,cr0 */
	PB(3, 0x66,0x8C,0xCB); PB(3, 0x66,0x8C,0xD9); PB(3, 0x66,0x8C,0xD2);	/* mov bx,cs; mov cx,ds; mov dx,ss */
	PB(3, 0x0F,0x00,0xCE);						/* str si */
	case_end();

	case_begin(); c[n++] = ncase-1;					/* 32-bit operations without prefixes */
	p_movr(3,0x11111111ul); P(0x8D); PB(2, 0x04,0x5B);		/* lea eax,[ebx+ebx*2] */
	P(0x05); PD(0x01020304ul);					/* add eax,01020304h */
	PB(2, 0x50,0x59);						/* push eax; pop ecx */
	case_end();

	case_begin(); c[n++] = ncase-1;					/* 66h: 16-bit operations in 32-bit code */
	p_movr(0,0xFFFFFFFFul); PB(2, 0x66,0xB8); PW(0x1234);
	PB(2, 0x89,0xE2); PB(2, 0x66,0x50); PB(2, 0x89,0xE6); PB(2, 0x66,0x5F);	/* mov edx,esp; push ax; mov esi,esp; pop di */
	case_end();

	case_begin(); c[n++] = ncase-1;					/* 67h: 16-bit addressing in 32-bit code */
	memd[0] = 0xC0FFEE00ul;
	p_movr(3,0xFFFF0000ul | memw_off); PB(3, 0x67,0x8B,0x07);	/* mov eax,[bx] */
	case_end();

	case_begin(); c[n++] = ncase-1;					/* SGDT/SIDT 32-bit */
	PB(3, 0x0F,0x01,0x05); PD(DOFF(sx_save[0]));
	PB(3, 0x0F,0x01,0x0D); PD(DOFF(sx_save[2]));
	case_end();

	case_begin(); c[n++] = ncase-1;					/* near call/ret 32-bit */
	P(0xE8); p = pp; PD(0); r = pc(); P(0xE9); { unsigned char __far *j = pph32();
	pfix32(p,(unsigned long)(pc() - r)); PB(4, 0x8B,0x0C,0x24,0xC3);	/* mov ecx,[esp]; ret */
	pfix32(j,(unsigned long)(pc() - (FP_OFF(j) - FP_OFF(pmcode) + 4u))); }
	case_end();
	case_mark[c[n-1]] = r;

	case_begin(); c[n++] = ncase-1;					/* SMSW, LMSW cannot clear PE */
	PB(3, 0x0F,0x01,0xE0); PB(2, 0x24,0xFE); PB(3, 0x0F,0x01,0xF0); PB(3, 0x0F,0x01,0xE3);
	case_end();

	if (pm_go()) {
		struct pmres *rr;

		pm_what = "PM state"; rr = &res[c[0]];
		if (ex_ok(c[0])) {
			ex_eq("CR0.PE",rr->eax & 1,1);
			ex_eq("CS",rr->ebx & 0xFFFF,S_C32);
			ex_eq("DS",rr->ecx & 0xFFFF,S_D);
			ex_eq("SS",rr->edx & 0xFFFF,S_S32);
			ex_eq("TR",rr->esi & 0xFFFF,S_TSS0);
			info("CR0 in protected mode = %08lX",rr->eax);
		}
		pm_what = "32-bit ops"; rr = &res[c[1]];
		if (ex_ok(c[1])) { ex_eq("EAX",rr->eax,0x33333333ul + 0x01020304ul); ex_eq("ECX",rr->ecx,0x33333333ul + 0x01020304ul); }
		pm_what = "66h in 32-bit code"; rr = &res[c[2]];
		if (ex_ok(c[2])) { ex_eq("EAX",rr->eax,0xFFFF1234ul); ex_eq("push ax ESP",rr->edx - rr->esi,2); ex_eq("DI",rr->edi & 0xFFFF,0x1234); }
		pm_what = "67h in 32-bit code";
		if (ex_ok(c[3])) ex_eq("EAX",res[c[3]].eax,0xC0FFEE00ul);
		pm_what = "SGDT/SIDT 32-bit";
		if (ex_ok(c[4])) {
			ex_eq("GDTR limit",sx_save[0] & 0xFFFF,gdtr[0]);
			ex_eq("GDTR base",(sx_save[0] >> 16) | ((sx_save[1] & 0xFFFF) << 16),lin(gdt));
			ex_eq("IDTR limit",sx_save[2] & 0xFFFF,idtr[0]);
			ex_eq("IDTR base",(sx_save[2] >> 16) | ((sx_save[3] & 0xFFFF) << 16),lin(idt));
		}
		pm_what = "call/ret near 32";
		if (ex_ok(c[5])) ex_eq("return EIP",res[c[5]].ecx,case_mark[c[5]]);
		pm_what = "SMSW/LMSW";
		if (ex_ok(c[6])) ex_eq("MSW.PE after LMSW with PE=0",res[c[6]].ebx & 1,1);
	}
	end();
}

/* ---- transfers between 16-bit and 32-bit code, 16/32-bit stacks */
static void g_mixed(void) {
	unsigned c[8],n = 0,a[8],sub;
	unsigned char __far *j;

	begin("PM 16/32");
	pm_begin();

	/* 32 -> 16: 16-bit far call (66 9A ptr16:16); sub16: mov bp,sp; mov ax,[bp]; mov dx,[bp+2]; retf */
	case_begin(); c[n] = ncase-1;
	p_movr(0,0xAAAA0000ul); p_movr(2,0xDDDD0000ul);
	P(0xE9); j = pph32(); sub = pc(); PB(10, 0x8B,0xEC,0x8B,0x46,0x00,0x8B,0x56,0x02,0xCB,0x90);
	pfix32(j,(unsigned long)(pc() - sub));
	PB(2, 0x66,0x9A); PW(sub); PW(S_C16); a[n++] = pc();
	case_end();

	/* 32 -> 16: 32-bit far call (9A ptr16:32); sub16: mov ebp,esp; mov eax,[bp]; mov edx,[bp+4]; retf (66 CB) */
	case_begin(); c[n] = ncase-1;
	P(0xE9); j = pph32(); sub = pc(); PB(13, 0x66,0x8B,0xEC,0x66,0x8B,0x46,0x00,0x66,0x8B,0x56,0x04,0x66,0xCB);
	pfix32(j,(unsigned long)(pc() - sub));
	P(0x9A); PD(sub); PW(S_C16); a[n++] = pc();
	case_end();

	/* 16 -> 32: jump to 16-bit code, 32-bit far call (66 9A ptr16:32) to a 32-bit sub, jump back */
	case_begin(); c[n] = ncase-1;
	P(0xE9); j = pph32(); sub = pc(); PB(8, 0x8B,0x04,0x24,0x8B,0x54,0x24,0x04,0xCB);	/* mov eax,[esp]; mov edx,[esp+4]; retf */
	pfix32(j,(unsigned long)(pc() - sub));
	P(0xEA); PD(pc() + 6u); PW(S_C16);				/* into 16-bit code */
	PB(2, 0x66,0x9A); PD(sub); PW(S_C32); a[n++] = pc();
	PB(2, 0x66,0xEA); PD(pc() + 6u); PW(S_C32);			/* back to 32-bit code */
	case_end();

	/* 16 -> 32: 16-bit far call (9A ptr16:16) to 32-bit code that returns with 66 CB */
	case_begin(); c[n] = ncase-1;
	p_movr(0,0); p_movr(2,0);
	P(0xE9); j = pph32(); sub = pc(); PB(11, 0x0F,0xB7,0x04,0x24,0x0F,0xB7,0x54,0x24,0x02,0x66,0xCB);
	pfix32(j,(unsigned long)(pc() - sub));
	P(0xEA); PD(pc() + 6u); PW(S_C16);
	P(0x9A); PW(sub); PW(S_C32); a[n++] = pc();
	PB(2, 0x66,0xEA); PD(pc() + 6u); PW(S_C32);
	case_end();

	/* 16-bit code: near call with 32-bit operand (66 E8 rel32), ret with 66 C3 */
	case_begin(); c[n] = ncase-1;
	P(0xEA); PD(pc() + 6u); PW(S_C16);
	PB(2, 0x66,0xE8); { unsigned char __far *q = pp; PD(0); a[n++] = pc(); PB(2, 0xEB,0x07);	/* jmp over the sub */
	pfix32(q,(unsigned long)(pc() - a[n-1])); PB(7, 0x67,0x66,0x8B,0x0C,0x24,0x66,0xC3); }
	PB(2, 0x66,0xEA); PD(pc() + 6u); PW(S_C32);
	case_end();

	/* 32-bit code with a 16-bit stack (B=0): push uses SP, upper ESP kept */
	case_begin(); c[n] = ncase-1;
	p_sreg(SR_SS,S_DSTK16); P(0xBC); PD(0x55550000ul);
	p_movr(0,0x13572468ul);
	PB(6, 0x50,0x89,0xE3,0x59,0x89,0xE2);				/* push eax; mov ebx,esp; pop ecx; mov edx,esp */
	p_sreg(SR_SS,S_S32); PB(2, 0x8B,0x25); PD(DOFF(pm_saved_esp));
	a[n++] = 0; case_end();

	/* 16-bit code with a 32-bit stack (B=1): push uses ESP */
	case_begin(); c[n] = ncase-1;
	P(0xEA); PD(pc() + 6u); PW(S_C16);
	P(0xB8); PW(S_S32B); PB(2, 0x8E,0xD0); PB(2, 0x66,0xBC); PD(0x00010000ul);
	PB(4, 0x50,0x66,0x8B,0xDC); P(0x58);				/* push ax; mov ebx,esp; pop ax */
	P(0xB8); PW(S_S32); PB(2, 0x8E,0xD0); PB(3, 0x66,0x8B,0x26); PW((unsigned)DOFF(pm_saved_esp));
	PB(2, 0x66,0xEA); PD(pc() + 6u); PW(S_C32);
	a[n++] = 0; case_end();

	if (pm_go()) {
		struct pmres *r;

		pm_what = "32->16 call far 16-bit"; r = &res[c[0]];
		if (ex_ok(c[0])) { ex_eq("return IP",r->eax & 0xFFFF,a[0]); ex_eq("return CS",r->edx & 0xFFFF,S_C32); ex_eq("EAX upper",r->eax >> 16,0xAAAA); }
		pm_what = "32->16 call far 32-bit"; r = &res[c[1]];
		if (ex_ok(c[1])) { ex_eq("return EIP",r->eax,a[1]); ex_eq("return CS",r->edx & 0xFFFF,S_C32); }
		pm_what = "16->32 call far 32-bit"; r = &res[c[2]];
		if (ex_ok(c[2])) { ex_eq("return EIP",r->eax,a[2]); ex_eq("return CS",r->edx & 0xFFFF,S_C16); }
		pm_what = "16->32 call far 16-bit"; r = &res[c[3]];
		if (ex_ok(c[3])) { ex_eq("return IP",r->eax,a[3]); ex_eq("return CS",r->edx,S_C16); }
		pm_what = "16-bit code 66 E8/66 C3"; r = &res[c[4]];
		if (ex_ok(c[4])) ex_eq("return EIP",r->ecx,a[4]);
		pm_what = "32-bit code, 16-bit stack"; r = &res[c[5]];
		if (ex_ok(c[5])) { ex_eq("ESP after push",r->ebx,0x5555FFFCul); ex_eq("popped",r->ecx,0x13572468ul); ex_eq("ESP after pop",r->edx,0x55550000ul); }
		pm_what = "16-bit code, 32-bit stack"; r = &res[c[6]];
		if (ex_ok(c[6])) ex_eq("ESP after push",r->ebx,0x0000FFFEul);
	}
	end();
}

/* ---- segment limits: granularity, expand-down B=1, LSL/LAR on 386 descriptors */
static void g_limits(void) {
	unsigned c[16],n = 0,a[16];

	begin("PM limits");
	pm_begin();

#define LIMCASE(sel,code,len) do { case_begin(); c[n] = ncase-1; p_sreg(SR_DS,sel); a[n++] = pc(); code; (void)len; case_end(); } while (0)
	LIMCASE(S_GLIM,(P(0xA0),PD(0x0FFFul)),0);			/* mov al,[0FFFh] */
	LIMCASE(S_GLIM,(P(0xA0),PD(0x1000ul)),0);			/* mov al,[1000h] */
	LIMCASE(S_GLIM,(P(0xA1),PD(0x0FFCul)),0);			/* mov eax,[0FFCh] */
	LIMCASE(S_GLIM,(P(0xA1),PD(0x0FFEul)),0);			/* mov eax,[0FFEh] */
	LIMCASE(S_EDB,(P(0xA0),PD(0x1000ul)),0);
	LIMCASE(S_EDB,(P(0xA0),PD(0x0FFFul)),0);
	LIMCASE(S_EDB,(P(0xA1),PD(0xFFFFFFFCul)),0);
	LIMCASE(S_EDB,(P(0xA1),PD(0xFFFFFFFDul)),0);
	LIMCASE(S_D,(P(0xA0),PD(0x10000ul)),0);				/* beyond the 64KB limit */
#undef LIMCASE

	/* LSL/LAR with 386 descriptors */
	case_begin(); c[n++] = ncase-1;
	PB(2, 0x66,0xBB); PW(S_GLIM); PB(3, 0x0F,0x03,0xC3);		/* lsl eax,bx */
	PB(2, 0x66,0xBB); PW(S_EDB); PB(3, 0x0F,0x03,0xCB);		/* lsl ecx,bx */
	PB(2, 0x66,0xBB); PW(S_S32B); PB(3, 0x0F,0x03,0xD3);		/* lsl edx,bx */
	PB(2, 0x66,0xBB); PW(S_C32); PB(3, 0x0F,0x02,0xF3);		/* lar esi,bx */
	PB(2, 0x66,0xBB); PW(S_FLAT); PB(3, 0x0F,0x02,0xFB);		/* lar edi,bx */
	case_end();

	if (pm_go()) {
		struct pmres *r;

		pm_what = "G=1 limit 0: byte FFFh";	ex_ok(c[0]);
		pm_what = "G=1 limit 0: byte 1000h";	ex_fault(c[1],13,0,a[1]);
		pm_what = "G=1 limit 0: dword FFCh";	ex_ok(c[2]);
		pm_what = "G=1 limit 0: dword FFEh";	ex_fault(c[3],13,0,a[3]);
		pm_what = "expand-down B=1: byte 1000h";	if (ex_ok(c[4])) ex_eq("AL",res[c[4]].eax & 0xFF,tbuf[0]);
		pm_what = "expand-down B=1: byte FFFh";	ex_fault(c[5],13,0,a[5]);
		pm_what = "expand-down B=1: dword FFFFFFFCh"; ex_ok(c[6]);
		pm_what = "expand-down B=1: dword FFFFFFFDh"; ex_fault(c[7],13,0,a[7]);
		pm_what = "64KB limit: byte 10000h";	ex_fault(c[8],13,0,a[8]);
		pm_what = "LSL/LAR"; r = &res[c[9]];
		if (ex_ok(c[9])) {
			ex_eq("LSL G=1 limit 0",r->eax,0x00000FFFul);
			ex_eq("LSL expand-down",r->ecx,0x00000FFFul);
			ex_eq("LSL G=1 4GB",r->edx,0xFFFFFFFFul);
			ex_eq("LAR 32-bit code",r->esi & 0x00F0FE00ul,0x00409A00ul);
			ex_eq("LAR flat data",r->edi & 0x00F0FE00ul,0x00C09200ul);
		}
	}
	end();
}

/* ---- gates, exceptions and error codes */
static void g_gates(void) {
	unsigned c[16],n = 0,a[16];

	begin("PM gates");
	pm_begin();

	case_begin(); c[n] = ncase-1; P(0xFB); PB(2, 0xCD,V_INTG); a[n++] = pc(); PB(3, 0x9C,0x5A,0xFA); case_end();
	case_begin(); c[n] = ncase-1; P(0xFB); PB(2, 0xCD,V_TRAPG); a[n++] = pc(); PB(3, 0x9C,0x5A,0xFA); case_end();
	case_begin(); c[n] = ncase-1; PB(2, 0xCD,V_INT286); a[n++] = pc(); case_end();
	case_begin(); c[n] = ncase-1; P(0xCC); a[n++] = pc(); case_end();
	case_begin(); c[n] = ncase-1; PB(4, 0xB0,0x7F,0x04,0x01); P(0xCE); a[n++] = pc(); case_end();
	case_begin(); c[n] = ncase-1; PB(2, 0x31,0xDB); a[n++] = pc(); PB(2, 0xF7,0xF3); case_end();
	case_begin(); c[n] = ncase-1; a[n++] = pc(); PB(2, 0x0F,0x0B); case_end();
	/* BOUND 32: [ebx] = (-5, 10) */
	case_begin(); c[n] = ncase-1;
	p_movmem(DOFF(dbvar[0]),0xFFFFFFFBul); p_movmem(DOFF(dbvar[1]),10);
	p_movr(3,DOFF(dbvar[0])); p_movr(0,11); a[n++] = pc(); PB(2, 0x62,0x03); case_end();
	case_begin(); c[n] = ncase-1; PB(2, 0x66,0xB8); PW(S_BEYOND); a[n++] = pc(); PB(2, 0x8E,0xC0); case_end();
	case_begin(); c[n] = ncase-1; PB(2, 0x66,0xB8); PW(S_DNP); a[n++] = pc(); PB(2, 0x8E,0xC0); case_end();
	case_begin(); c[n] = ncase-1; p_lidt(DOFF(idtr_small)); a[n++] = pc(); PB(2, 0xCD,V_BEYOND); case_end();
	/* IRETD at CPL 0 loads IOPL */
	case_begin(); c[n] = ncase-1;
	P(0x68); PD(0x00003002ul); P(0x0E); P(0x68); { unsigned char __far *q = pph32(); P(0xCF); pfix32(q,pc()); }
	PB(2, 0x9C,0x5A); a[n++] = 0; case_end();

	if (pm_go()) {
		struct pmres *r;

		pm_what = "386 interrupt gate"; r = &res[c[0]];
		if (ex_ok(c[0])) {
			ex_eq("handler count",r->hcnt,1);
			ex_eq("IF in handler",(r->hfl & IF) ? 1 : 0,0);
			ex_eq("frame EIP",r->h[0],a[0]);
			ex_eq("frame CS",r->h[1] & 0xFFFF,S_C32);
			ex_eq("frame IF",(r->h[2] & IF) ? 1 : 0,1);
			ex_eq("IF after IRETD",(r->edx & IF) ? 1 : 0,1);
			ex_eq("ESI from handler",r->esi,0x42424242ul);
		}
		pm_what = "386 trap gate"; r = &res[c[1]];
		if (ex_ok(c[1])) ex_eq("IF in handler",(r->hfl & IF) ? 1 : 0,1);
		pm_what = "286 gate from 32-bit code"; r = &res[c[2]];
		if (ex_ok(c[2])) {
			ex_eq("ESI from 16-bit handler",r->esi,0x43434343ul);
			ex_eq("frame IP (16-bit)",r->h[0],a[2]);
			ex_eq("frame CS (16-bit)",r->h[1],S_C32);
		}
		pm_what = "int3";		ex_fault(c[3],3,-1,a[3]);
		pm_what = "into";		ex_fault(c[4],4,-1,a[4]);
		pm_what = "div ebx=0";		ex_fault(c[5],0,-1,a[5]);
		pm_what = "invalid opcode";	ex_fault(c[6],6,-1,a[6]);
		pm_what = "bound eax=11 (-5..10)"; ex_fault(c[7],5,-1,a[7]);
		pm_what = "mov es, beyond GDT";	ex_fault(c[8],13,S_BEYOND,a[8]);
		pm_what = "mov es, not present"; ex_fault(c[9],11,S_DNP,a[9]);
		pm_what = "int beyond IDT limit"; ex_fault(c[10],13,V_BEYOND*8+2,a[10]);
		pm_what = "iretd IOPL=3";	if (ex_ok(c[11])) ex_eq("IOPL",(res[c[11]].edx >> 12) & 3,3);
	}
	end();
}

/* ---- ring 3: IOPL, I/O bitmap, privileged instructions, call gate with parameters, conforming code */
static void g_ring3(void) {
	unsigned c[10],n = 0,a[10];
	unsigned long sp3 = DOFF(stk3[200]);

	begin("PM ring 3");
	pm_begin();

	case_begin(); c[n] = ncase-1; p_to_ring3(sp3,0);
	PB(9, 0x66,0x8C,0xC8,0x66,0x8C,0xD3,0x89,0xE1,0xCD); P(V_INTG3); a[n++] = pc(); case_end_int(V_EXIT0);
	case_begin(); c[n] = ncase-1; p_to_ring3(sp3,0); a[n++] = pc(); P(0xFA); case_end_int(V_EXIT0);
	case_begin(); c[n] = ncase-1; p_to_ring3(sp3,0); p_movr(2,io_ok_port); P(0xEC); a[n++] = pc(); case_end_int(V_EXIT0);
	case_begin(); c[n] = ncase-1; p_to_ring3(sp3,0); p_movr(2,io_bad_port); a[n++] = pc(); P(0xEC); case_end_int(V_EXIT0);
	case_begin(); c[n] = ncase-1; p_to_ring3(sp3,0); a[n++] = pc(); PB(3, 0x0F,0x20,0xC0); case_end_int(V_EXIT0);
	case_begin(); c[n] = ncase-1; p_to_ring3(sp3,3); P(0xFA); p_movr(2,io_bad_port); P(0xEC); a[n++] = pc(); case_end_int(V_EXIT0);

	/* call gate with 2 dword parameters, to ring 0 */
	case_begin(); c[n] = ncase-1; p_to_ring3(sp3,0);
	P(0x68); PD(0x11111111ul); P(0x68); PD(0x22222222ul);
	P(0x9A); PD(0); PW(S_GATE3|3); a[n++] = pc();
	gate3_off = pc();
	PB(3, 0x8B,0x04,0x24); PB(4, 0x8B,0x74,0x24,0x04); PB(4, 0x8B,0x5C,0x24,0x08);
	PB(4, 0x8B,0x4C,0x24,0x0C); PB(4, 0x8B,0x54,0x24,0x10); PB(4, 0x8B,0x7C,0x24,0x14);
	case_end();

	/* conforming code from ring 3: CPL stays 3 */
	case_begin(); c[n] = ncase-1; p_to_ring3(sp3,0); P(0x9A); PD(conf_off); PW(S_CONF|3); a[n++] = pc(); case_end_int(V_EXIT0);

	if (pm_go()) {
		struct pmres *r;

		pm_what = "ring 3 + int via DPL3 386 gate"; r = &res[c[0]];
		if (ex_exit3(c[0])) {
			ex_eq("CS",r->eax & 0xFFFF,S_C32_3|3);
			ex_eq("SS",r->ebx & 0xFFFF,S_D3|3);
			ex_eq("ESP",r->ecx,sp3);
			ex_eq("handler count",r->hcnt,1);
			ex_eq("frame EIP",r->h[0],a[0]);
			ex_eq("frame CS",r->h[1] & 0xFFFF,S_C32_3|3);
			ex_eq("frame ESP",r->h[3],sp3);
			ex_eq("frame SS",r->h[4] & 0xFFFF,S_D3|3);
			ex_eq("handler SS",r->hss,S_S32);
		}
		pm_what = "cli at CPL 3, IOPL 0";		ex_fault(c[1],13,0,a[1]);
		pm_what = "in, port allowed by the I/O bitmap";	if (ex_exit3(c[2])) ex_eq("exit EIP",r_ip(&res[c[2]]),a[2]);
		pm_what = "in, port denied by the I/O bitmap";	ex_fault(c[3],13,0,a[3]);
		pm_what = "mov eax,cr0 at CPL 3";		ex_fault(c[4],13,0,a[4]);
		pm_what = "IOPL 3: cli and any port";		if (ex_exit3(c[5])) ex_eq("exit EIP",r_ip(&res[c[5]]),a[5]);
		pm_what = "386 call gate, 2 parameters"; r = &res[c[6]];
		if (ex_ok(c[6])) {
			ex_eq("return EIP",r->eax,a[6]);
			ex_eq("return CS",r->esi & 0xFFFF,S_C32_3|3);
			ex_eq("parameter 0",r->ebx,0x22222222ul);
			ex_eq("parameter 1",r->ecx,0x11111111ul);
			ex_eq("caller ESP",r->edx,sp3 - 8ul);
			ex_eq("caller SS",r->edi & 0xFFFF,S_D3|3);
		}
		pm_what = "conforming code from CPL 3"; r = &res[c[7]];
		if (ex_exit3(c[7])) ex_eq("CS in conforming code",r->edx & 0xFFFF,S_CONF|3);
	}
	end();
}

/* ---- task switch through a 386 TSS */
static void g_task(void) {
	unsigned c[2],a1;

	begin("Task 386");
	pm_begin();

	case_begin(); c[0] = ncase-1;
	p_movr(0,0x5A5A5A5Aul); p_movr(6,0x11223344ul);
	P(0x9A); PD(0); PW(S_TSS1);
	PB(2, 0x9C,0x5A);
	case_end();
	case_begin(); c[1] = ncase-1; a1 = pc(); P(0xEA); PD(0); PW(S_TSS0); case_end();

	if (pm_go()) {
		struct pmres *r = &res[c[0]];

		pm_what = "call TSS / iretd";
		if (ex_ok(c[0])) {
			ex_eq("EAX preserved",r->eax,0x5A5A5A5Aul);
			ex_eq("ESI preserved",r->esi,0x11223344ul);
			ex_eq("NT after return",(r->edx >> 14) & 1,0);
			ex_eq("new task NT",(t1_rec[0] >> 14) & 1,1);
			ex_eq("new task TR",t1_rec[1] & 0xFFFF,S_TSS1);
			ex_eq("new task EAX",t1_rec[2],0x11111111ul);
			ex_eq("new task ESI",t1_rec[3],0x77777777ul);
			ex_eq("back link",tssr(tss1,0) & 0xFFFF,S_TSS0);
			ex_eq("TSS1 EIP saved after IRETD",tssr(tss1,TSS_EIP),task1_end);
			ex_eq("TSS1 type after IRETD (available)",gdt[S_TSS1>>3].acc,0x89);
			ex_eq("TSS0 type (busy)",gdt[S_TSS0>>3].acc,0x8B);
		}
		pm_what = "jmp to busy TSS"; ex_fault(c[1],13,S_TSS0,a1);
	}
	end();
}

/* ---- paging */
static void p_pte(unsigned long page_lin,unsigned long v) {
	p_sreg(SR_ES,S_FLAT);
	PB(3, 0x26,0xC7,0x05); PD(pg_pt + (page_lin >> 12) * 4ul); PD(v);	/* mov dword es:[pte],v */
	PB(6, 0x0F,0x20,0xD8,0x0F,0x22,0xD8);				/* reload CR3 */
}

static void g_paging(void) {
	unsigned c[10],n = 0,a[10];
	unsigned long sp3 = DOFF(stk3[200]),i;
	unsigned long __far *pt;

	begin("Paging");
	if (!pg_seg) { skip("cannot allocate memory for the page tables"); end(); return; }

	/* page directory: one table, identity map of 0-4MB, user/RW. X and Y test pages */
	_fmemset(linp(pg_pd),0,4096);
	*linp(pg_pd) = pg_pt | 7ul;
	pt = linp(pg_pt);
	for (i=0;i < 1024;i++) pt[i] = (i << 12) | 7ul;
	for (i=0;i < 1024;i++) { linp(pg_x)[i] = 0x11111111ul; linp(pg_y)[i] = 0x22222222ul; }

	pm_begin();
	case_begin(); c[n] = ncase-1;					/* enable paging */
	p_movr(0,pg_pd); PB(3, 0x0F,0x22,0xD8);
	PB(3, 0x0F,0x20,0xC0); P(0x0D); PD(0x80000000ul); PB(3, 0x0F,0x22,0xC0); PB(2, 0xEB,0x00);
	P(0xA1); PD(DOFF(marker)); PB(3, 0x0F,0x20,0xC3);		/* mov eax,[marker]; mov ebx,cr0 */
	a[n++] = 0; case_end();
	case_begin(); c[n] = ncase-1; p_sreg(SR_ES,S_FLAT); PB(2, 0x26,0xA1); PD(pg_x); a[n++] = 0; case_end();
	case_begin(); c[n] = ncase-1;					/* remap X to Y */
	p_pte(pg_x,pg_y | 7ul); PB(3, 0x26,0x8B,0x35); PD(pg_x + 8ul); p_pte(pg_x,pg_x | 7ul); a[n++] = 0; case_end();	/* mov esi,es:[X+8] */
	case_begin(); c[n] = ncase-1;					/* not present, read */
	p_pte(pg_x,pg_x | 6ul); a[n++] = pc(); PB(2, 0x26,0xA1); PD(pg_x + 0x10ul); case_end();
	case_begin(); c[n] = ncase-1;					/* not present, write */
	p_pte(pg_x,pg_x | 6ul); a[n++] = pc(); PB(3, 0x26,0xC7,0x05); PD(pg_x + 0x20ul); PD(0); case_end();
	case_begin(); c[n] = ncase-1;					/* supervisor page from ring 3 */
	p_pte(pg_x,pg_x | 3ul); p_to_ring3(sp3,0); p_sreg(SR_ES,S_FLAT3|3); a[n++] = pc(); PB(2, 0x26,0xA1); PD(pg_x); case_end_int(V_EXIT0);
	case_begin(); c[n] = ncase-1;					/* user read-only page, write from ring 3 */
	p_pte(pg_x,pg_x | 5ul); p_to_ring3(sp3,0); p_sreg(SR_ES,S_FLAT3|3); a[n++] = pc(); PB(3, 0x26,0xC7,0x05); PD(pg_x); PD(0); case_end_int(V_EXIT0);
	case_begin(); c[n] = ncase-1;					/* user read-only page, write from ring 0 */
	p_pte(pg_x,pg_x | 5ul); PB(3, 0x26,0xC7,0x05); PD(pg_x + 0x30ul); PD(0x33333333ul); a[n++] = 0; case_end();
	case_begin(); c[n] = ncase-1;					/* accessed and dirty bits */
	p_pte(pg_x,pg_x | 7ul); p_pte(pg_y,pg_y | 7ul);
	PB(2, 0x26,0xA1); PD(pg_x + 0x40ul); PB(3, 0x26,0xC7,0x05); PD(pg_y + 0x40ul); PD(0x44444444ul);
	a[n++] = 0; case_end();

	if (pm_go()) {
		struct pmres *r;

		pt = linp(pg_pt);
		pm_what = "enable paging"; r = &res[c[0]];
		if (ex_ok(c[0])) { ex_eq("CR0.PG",r->ebx >> 31,1); ex_eq("read through identity map",r->eax,marker); }
		pm_what = "read page X";	if (ex_ok(c[1])) ex_eq("X",res[c[1]].eax,0x11111111ul);
		pm_what = "X mapped to Y";	if (ex_ok(c[2])) ex_eq("X->Y",res[c[2]].esi,0x22222222ul);
		pm_what = "#PF read, not present";
		if (ex_fault(c[3],14,0,a[3])) ex_eq("CR2",res[c[3]].cr2,pg_x + 0x10ul);
		pm_what = "#PF write, not present";
		if (ex_fault(c[4],14,2,a[4])) ex_eq("CR2",res[c[4]].cr2,pg_x + 0x20ul);
		pm_what = "#PF ring 3 read, supervisor page";
		if (ex_fault(c[5],14,5,a[5])) ex_eq("CR2",res[c[5]].cr2,pg_x);
		pm_what = "#PF ring 3 write, read-only page";
		if (ex_fault(c[6],14,7,a[6])) ex_eq("CR2",res[c[6]].cr2,pg_x);
		pm_what = "ring 0 write, read-only page (no WP on 386)";
		if (ex_ok(c[7])) ex_eq("written",linp(pg_x)[0x30/4],0x33333333ul);
		pm_what = "accessed/dirty";
		if (ex_ok(c[8])) {
			ex_eq("X PTE A,D",pt[pg_x >> 12] & 0x60ul,0x20ul);
			ex_eq("Y PTE A,D",pt[pg_y >> 12] & 0x60ul,0x60ul);
			ex_eq("PDE A",*linp(pg_pd) & 0x20ul,0x20ul);
		}
	}
	end();
}

/* ---- debug registers in protected mode */
static void g_pmdebug(void) {
	unsigned c[4],n = 0,a[4];

	begin("PM debug");
	pm_begin();
	case_begin(); c[n] = ncase-1;					/* DR0 read/write 4 bytes */
	p_movr(0,dgroup_lin + DOFF(dbvar[1])); PB(3, 0x0F,0x23,0xC0);
	p_movr(0,0x000F0301ul); PB(3, 0x0F,0x23,0xF8);
	P(0xA1); PD(DOFF(dbvar[1])); a[n++] = pc(); PB(2, 0xB0,0x55); case_end();
	case_begin(); c[n] = ncase-1;					/* DR1 execute */
	P(0xB8); { unsigned char __far *q = pph32(); PB(3, 0x0F,0x23,0xC8);
	p_movr(0,0x00000304ul); PB(3, 0x0F,0x23,0xF8);
	a[n] = pc(); pfix32(q,pmcode_lin + a[n]); n++; } PB(2, 0xB0,0x55); case_end();
	case_begin(); c[n] = ncase-1;					/* DR2 write 2 bytes, not hit by a write next to it */
	p_movr(0,dgroup_lin + DOFF(dbvar[2])); PB(3, 0x0F,0x23,0xD0);
	p_movr(0,0x05000330ul); PB(3, 0x0F,0x23,0xF8);
	PB(2, 0x66,0xA3); PD(DOFF(dbvar[2]) + 2ul); PB(2, 0x66,0xA3); PD(DOFF(dbvar[2])); a[n++] = pc(); case_end();

	if (pm_go()) {
		pm_what = "DR0 data breakpoint";
		if (ex_fault(c[0],1,-1,a[0])) ex_eq("DR6.B0",res[c[0]].dr6 & 1,1);
		pm_what = "DR1 execute breakpoint";
		if (ex_fault(c[1],1,-1,a[1])) ex_eq("DR6.B1",(res[c[1]].dr6 >> 1) & 1,1);
		pm_what = "DR2 2-byte write breakpoint";
		if (ex_fault(c[2],1,-1,a[2])) ex_eq("DR6.B2",(res[c[2]].dr6 >> 2) & 1,1);
	}
	end();
}

/* ---- virtual 8086 mode */
static int ex_v86exit(unsigned i,unsigned long ip) {
	return ex_fault(i,V_EXIT3,-1,ip);
}

static void g_v86(void) {
	unsigned c[12],n = 0,a[12];

	begin("V86 basic");
	pm_begin();

	memw[0] = 0x6789; memw[0x20] = 0x7E57; memd[1] = 0xFACE0FF5ul;
	case_begin(); c[n] = ncase-1; p_to_v86(3,0x1234,0x5678);
	P(0xA1); PW(memw_off); PB(2, 0x66,0xBB); PD(0xCAFEF00Dul); PB(2, 0x8C,0xC9);
	PB(2, 0xCD,V_EXIT3); a[n++] = pc(); case_rec();
	case_begin(); c[n] = ncase-1; p_to_v86(3,0,0);			/* segment * 16 */
	P(0xB8); PW(dgroup + 1); PB(2, 0x8E,0xC0); PB(2, 0x26,0xA1); PW(memw_off + 0x40 - 16);
	PB(2, 0xCD,V_EXIT3); a[n++] = pc(); case_rec();
	case_begin(); c[n] = ncase-1; p_to_v86(3,0,0); PB(4, 0xFA,0xFB,0x9C,0x9D); PB(2, 0xCD,V_EXIT3); a[n++] = pc(); case_rec();
	case_begin(); c[n] = ncase-1; p_to_v86(3,0,0); PB(4, 0x67,0x66,0x8B,0x05); PD(memw_off + 4u); PB(2, 0xCD,V_EXIT3); a[n++] = pc(); case_rec();
	case_begin(); c[n] = ncase-1; p_to_v86(3,0,0); a[n++] = pc(); PB(4, 0x8B,0x1E,0xFF,0xFF); PB(2, 0xCD,V_EXIT3); case_rec();
	case_begin(); c[n] = ncase-1; p_to_v86(3,0,0); a[n++] = pc(); P(0xF4); PB(2, 0xCD,V_EXIT3); case_rec();
	case_begin(); c[n] = ncase-1; p_to_v86(3,0,0); a[n++] = pc(); PB(3, 0x0F,0x20,0xC0); PB(2, 0xCD,V_EXIT3); case_rec();
	case_begin(); c[n] = ncase-1; p_to_v86(3,0,0); a[n++] = pc(); PB(3, 0x0F,0x01,0xF0); PB(2, 0xCD,V_EXIT3); case_rec();
	case_begin(); c[n] = ncase-1; p_to_v86(3,0,0); a[n++] = pc(); PB(2, 0x63,0xC0); PB(2, 0xCD,V_EXIT3); case_rec();
	case_begin(); c[n] = ncase-1; p_to_v86(3,0,0); a[n++] = pc(); PB(3, 0x0F,0x02,0xC0); PB(2, 0xCD,V_EXIT3); case_rec();

	if (pm_go()) {
		struct pmres *r;

		pm_what = "V86 entry/exit"; r = &res[c[0]];
		if (ex_v86exit(c[0],v86ip(a[0]))) {
			ex_eq("frame CS",r->w[1],FP_SEG(pmcode));
			ex_eq("frame EFLAGS.VM",(r->w[2] >> 17) & 1,1);
			ex_eq("frame EFLAGS.IOPL",(r->w[2] >> 12) & 3,3);
			ex_eq("frame ESP",r->w[3],DOFF(stk3[240]));
			ex_eq("frame SS",r->w[4],dgroup);
			ex_eq("frame ES",r->w[5],dgroup);
			ex_eq("frame DS",r->w[6],dgroup);
			ex_eq("frame FS",r->w[7],0x1234);
			ex_eq("frame GS",r->w[8],0x5678);
			ex_eq("AX read in V86",r->eax & 0xFFFF,0x6789);
			ex_eq("EBX (32-bit operand in V86)",r->ebx,0xCAFEF00Dul);
			ex_eq("CS in V86",r->ecx & 0xFFFF,FP_SEG(pmcode));
			ex_eq("DS cleared on exit",r->ds & 0xFFFF,0);
			ex_eq("ES cleared on exit",r->es & 0xFFFF,0);
			ex_eq("FS cleared on exit",r->fs & 0xFFFF,0);
			ex_eq("GS cleared on exit",r->gs & 0xFFFF,0);
		}
		pm_what = "V86 segment*16";	if (ex_v86exit(c[1],v86ip(a[1]))) ex_eq("AX",res[c[1]].eax & 0xFFFF,0x7E57);
		pm_what = "V86 IOPL 3: cli sti pushf popf"; ex_v86exit(c[2],v86ip(a[2]));
		pm_what = "V86 67h 32-bit address"; if (ex_v86exit(c[3],v86ip(a[3]))) ex_eq("EAX",res[c[3]].eax,0xFACE0FF5ul);
		pm_what = "V86 word at offset FFFFh";	ex_fault(c[4],13,0,v86ip(a[4]));
		pm_what = "V86 hlt";			ex_fault(c[5],13,0,v86ip(a[5]));
		pm_what = "V86 mov eax,cr0";		ex_fault(c[6],13,0,v86ip(a[6]));
		pm_what = "V86 lmsw";			ex_fault(c[7],13,0,v86ip(a[7]));
		pm_what = "V86 arpl";			ex_fault(c[8],6,-1,v86ip(a[8]));
		pm_what = "V86 lar";			ex_fault(c[9],6,-1,v86ip(a[9]));
	}
	end();
}

/* ---- V86 mode, IOPL 0: IOPL sensitive instructions fault with #GP(0) */
static void g_v86iopl(void) {
	static const struct { const char *what; unsigned char n,b[5]; } ins[] = {
		{ "cli",1,{0xFA} }, { "sti",1,{0xFB} }, { "pushf",1,{0x9C} }, { "popf",1,{0x9D} },
		{ "int 21h",2,{0xCD,0x21} }, { "int3",1,{0xCC} }, { "into",1,{0xCE} }, { "iret",1,{0xCF} }
	};
	unsigned c[8],a[8],i,k;

	begin("V86 IOPL0");
	pm_begin();
	for (i=0;i < 8;i++) {
		case_begin(); c[i] = ncase-1; p_to_v86(0,0,0);
		if (i == 6) PB(4, 0xB0,0x7F,0x04,0x01);			/* OF=1 for INTO */
		a[i] = pc();
		for (k=0;k < ins[i].n;k++) P(ins[i].b[k]);
		PB(2, 0xCD,V_EXIT3); case_rec();
	}
	if (pm_go()) {
		for (i=0;i < 8;i++) {
			pm_what = ins[i].what;
			/* INT3 and INTO are not IOPL sensitive: they go through the IDT, where the DPL 0 gate gives #GP(vector*8+2) */
			ex_fault(c[i],13,(i == 5) ? (3*8+2) : (i == 6) ? (4*8+2) : 0,v86ip(a[i]));
		}
	}
	end();
}

/* ---- V86 mode I/O: the I/O bitmap decides, IOPL does not */
static void g_v86io(void) {
	unsigned c[4],a[4];

	begin("V86 I/O");
	pm_begin();
	case_begin(); c[0] = ncase-1; p_to_v86(0,0,0); P(0xBA); PW(io_ok_port); P(0xEC); a[0] = pc(); PB(2, 0xCD,V_EXIT3); case_rec();
	case_begin(); c[1] = ncase-1; p_to_v86(0,0,0); P(0xBA); PW(io_bad_port); a[1] = pc(); P(0xEC); PB(2, 0xCD,V_EXIT3); case_rec();
	case_begin(); c[2] = ncase-1; p_to_v86(3,0,0); P(0xBA); PW(io_bad_port); a[2] = pc(); P(0xEC); PB(2, 0xCD,V_EXIT3); case_rec();
	case_begin(); c[3] = ncase-1; p_to_v86(3,0,0); P(0xBA); PW(io_ok_port); P(0xEC); a[3] = pc(); PB(2, 0xCD,V_EXIT3); case_rec();
	if (pm_go()) {
		pm_what = "IOPL 0, port allowed";	ex_fault(c[0],13,0,v86ip(a[0]));	/* faults at the INT 45h exit */
		pm_what = "IOPL 0, port denied";	ex_fault(c[1],13,0,v86ip(a[1]));
		pm_what = "IOPL 3, port denied";	ex_fault(c[2],13,0,v86ip(a[2]));
		pm_what = "IOPL 3, port allowed";	ex_v86exit(c[3],v86ip(a[3]) + 2u);
	}
	end();
}

/* ---- unreal mode: segment limits survive the return to real mode */
static void g_unreal(void) {
	unsigned s;
	int got;

	begin("Unreal");
	pm_unreal = 1;
	pm_begin();
	got = pm_go();
	pm_unreal = 0;
	if (got) {
		/* FS keeps the 4GB limit, loading FS in real mode changes only the base */
		setup32(); I.fs = 0; I.esi = dgroup_lin + DOFF(marker); EB(5, 0x64,0x67,0x66,0x8B,0x06); go32();
		X.eax = marker;
		if (!verify32(CMASK)) failr("mov eax,fs:[esi] beyond 64KB in unreal mode");

		/* a normal trip resets the limits; then the same access faults */
		pm_begin();
		if (pm_go()) {
			setup32(); I.fs = 0; I.esi = dgroup_lin + DOFF(marker); s = here(); EB(5, 0x64,0x67,0x66,0x8B,0x06); go32x();
			if (!checkm(x_cnt == 1 && x_vec == 0x0D && x_ip == s)) failm("after normal return: fs:[esi] beyond 64KB: INT 0Dh count=%u vec=%02X",x_cnt,x_vec);
		}
	}
	end();
}
/* ================================================================== MAIN */

static int key_wait = 1;

int main(int argc,char **argv) {
	unsigned i;
	char tmp[96];

	for (i=1;i < (unsigned)argc;i++) {
		if (!strcmp(argv[i],"-n") || !strcmp(argv[i],"/n") || !strcmp(argv[i],"/N")) key_wait = 0;
	}

#if defined(TARGET_PC98)
	pc98 = 1;
#endif

	__asm {
		mov	ax,seg dgroup
		mov	ds,ax
		mov	dgroup,ax
		mov	ax,ss
		mov	ss_val,ax
	}

	thunk_ptr = (void (__far*)(void))thunk;
	thunk_seg = FP_SEG(thunk);
	memw_off = FP_OFF(memw);
	init_parity();

	logf = fopen("386.TXT","w");
	video_init();
	detect_cpu();

	sprintf(tmp,"386 CPU instruction test (%s)",pc98 ? "NEC PC-98" : (ibm_mono ? "IBM PC, mono" : "IBM PC, color"));
	put_str(0,0,COL_TITLE,tmp);
	sprintf(tmp,"CPU: %s",cpu_name);
	put_str(0,1,COL_NORM,tmp);
	if (logf) {
		fprintf(logf,"386 CPU instruction test, %s\n",pc98 ? "NEC PC-98" : "IBM PC");
		fprintf(logf,"%s\n\n",tmp);
		log_commit();
	}

	if (!c_386) {
		strcpy(tmp,"This test requires an 80386 or later CPU.");
		show_bottom(23,COL_FAIL,tmp);
		if (logf) fprintf(logf,"%s\n",tmp);
	}
	else {
		build_runner();
		hook_rm();
		t_cpu386();
		t_mov32();
		t_movx();
		{ unsigned op; for (op=0;op < 8;op++) t_alu32(op); }
		t_unary32();
		t_mul32();
		t_div32();
		{ static const unsigned char ops[7] = { SH_ROL,SH_ROR,SH_RCL,SH_RCR,SH_SHL,SH_SHR,SH_SAR }; unsigned k; for (k=0;k < 7;k++) t_shift32(ops[k]); }
		t_shxd();
		t_bt();
		t_bsf();
		t_setcc();
		t_cwde();
		t_push32();
		t_pushfd();
		t_fsgs();
		t_lea32();
		t_addr32();
		t_str32();
		t_loop32();
		t_ctl32();
		t_crdr();
		t_486();
		unhook_rm();
		if (c_v86) {
			strcpy(tmp,"Virtual 8086 mode is active: protected mode and V86 tests skipped.");
			show_bottom(22,COL_INFO,tmp);
			if (logf) fprintf(logf,"%s\n",tmp);
		}
		else {
			unsigned long l;

			pmcode_lin = lin(pmcode);
			dgroup_lin = (unsigned long)dgroup << 4ul;
			io_ok_port = pc98 ? 0x5F : 0x80;
			io_bad_port = pc98 ? 0x31 : 0x61;
			if (_dos_allocmem(0x600,&pg_seg) != 0) pg_seg = 0;
			if (pg_seg) {
				l = (((unsigned long)pg_seg << 4ul) + 0xFFFul) & ~0xFFFul;
				pg_pd = l; pg_pt = l + 0x1000ul; pg_x = l + 0x2000ul; pg_y = l + 0x3000ul;
			}
			build_pm_enter();
			hook_rm();
			g_pm32();
			g_mixed();
			g_limits();
			g_gates();
			g_ring3();
			g_task();
			g_paging();
			g_pmdebug();
			g_v86();
			g_v86iopl();
			g_v86io();
			g_unreal();
			unhook_rm();
			if (pg_seg) _dos_freemem(pg_seg);
		}
	}

	sprintf(tmp,"Done: %u pass, %u fail, %u info, %u skip.%s",total_pass,total_fail,total_info,total_skip,
		key_wait ? " Press any key." : "");
	put_strpad(0,0,80,total_fail ? COL_FAIL : COL_PASS,tmp);
	if (logf) {
		fprintf(logf,"\n%s\n",tmp);
		log_commit();
		fclose(logf);
	}

	if (key_wait) getch();
	video_exit();
	return total_fail ? 1 : 0;
}

#endif
