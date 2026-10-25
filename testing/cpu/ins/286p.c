/* 286p.c
 *
 * 80286 protected mode instruction and functionality test for MS-DOS.
 * (C) 2026 Jonathan Campbell, DOSLIB project.
 *
 * Enters 16-bit protected mode with 286 descriptors and tests segment
 * descriptor loading and checks (limits, expand-down, type, present,
 * privilege, null selector), the LDT, LAR/LSL/VERR/VERW/ARPL, SGDT/SIDT,
 * LMSW, interrupt and trap gates and the exceptions and error codes they
 * raise, IRET, IOPL, a round trip through ring 3 with call gate and
 * interrupt gate stack switching, and a task switch through a 286 TSS.
 *
 * Getting back to real mode: the 286 cannot leave protected mode except by
 * a CPU reset. Like Windows 3.0/3.1, this program causes a triple fault
 * (LIDT with limit 0, then INT 3) and lets the BIOS hand control back:
 *
 *   IBM PC/AT: CMOS shutdown byte 0Fh = 0Ah, BIOS jumps through 0040:0067.
 *   PC-98:     SHUT0 = SHUT1 = 0 (port 37h), BIOS loads SS:SP from 0000:0404
 *              and executes RETF.
 *
 * The reset destroys the CPU state, so the resume code assumes nothing and
 * reloads SS:SP and DS from data stored in its own code segment. The PIC
 * masks, CMOS shutdown byte, reset vector and shutdown bits are restored
 * afterwards. A triple fault caused by a bug or by a CPU/emulator problem
 * returns the same way, and is reported as a crash in the case that was
 * running at the time.
 *
 * The protected mode code is generated machine code (the Watcom real mode
 * code cannot run in protected mode). Each test group is one trip into
 * protected mode. Every exception vector goes to a handler that records the
 * vector, error code, faulting CS:IP and registers, then resumes at the next
 * test case. All IRQs are masked at the PIC while in protected mode.
 *
 * Requires a 286 or later in real mode, not virtual 8086 mode (EMM386 etc).
 * 16-bit large memory model only. The 32-bit build is a stub.
 *
 * Results are also written to 286P.TXT in the current directory. The file is
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
			logf = fopen("286P.TXT","a");
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

/* forward reference placeholders */
static unsigned char __far *ph16(void) {
	unsigned char __far *p = tp;
	EW(0);
	return p;
}

static void fixabs16(unsigned char __far *p,unsigned v) {
	*((unsigned short __far*)p) = v;
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

/* ======================================================= PROTECTED MODE */

/* GDT selectors */
#define SEL_CODE0	0x08		/* code, DPL 0, readable, base = pm_code */
#define SEL_DATA0	0x10		/* data, DPL 0, base = DGROUP, limit FFFFh */
#define SEL_DLIM	0x18		/* data, base = tbuf, limit 00FFh */
#define SEL_DED		0x20		/* expand-down data, base = tbuf - 1000h, limit 0FFFh */
#define SEL_CODEX	0x28		/* code, execute only */
#define SEL_CODER	0x30		/* code, readable (for loading into DS) */
#define SEL_DRO		0x38		/* data, read only, base = tbuf */
#define SEL_DNP		0x40		/* data, not present */
#define SEL_D3		0x48		/* data, DPL 3, base = DGROUP (ring 3 data and stack) */
#define SEL_C3		0x50		/* code, DPL 3, base = pm_code */
#define SEL_S3ED	0x58		/* expand-down data, DPL 3, limit 7FFFh, offset 7F00h = tbuf (ring 3 stack fault) */
#define SEL_S3LIM	0x60		/* data, DPL 3, base = tbuf, limit 01FFh (ring 3 stack fault) */
#define SEL_SNP		0x68		/* data, not present (SS load) */
#define SEL_LDT		0x70		/* LDT */
#define SEL_TSS0	0x78		/* 286 TSS of the test program */
#define SEL_TSS1	0x80		/* 286 TSS for the task switch test */
#define SEL_GATE3	0x88		/* call gate, DPL 3, to ring 0 */
#define SEL_CODE0B	0x90		/* code, DPL 0, base = pm_code + 16 */
#define SEL_CLIM	0x98		/* code, DPL 0, limit 01FFh, offset 0300h = landing pad */
#define GDT_N		20
#define SEL_BEYOND	0x01F8		/* beyond the GDT limit */

#define IDT_N		0x50		/* vectors 00h-4Fh */
#define V_INTG		0x40		/* interrupt gate to the INT test handler */
#define V_TRAPG		0x41		/* trap gate to the INT test handler */
#define V_INTG3		0x42		/* interrupt gate, DPL 3 */
#define V_NP		0x43		/* not present */
#define V_INTG0		0x44		/* interrupt gate, DPL 0 (ring 3 uses it to fault back to ring 0) */
#define V_BEYOND	0x4A		/* beyond the small IDT limit */
#define IDT_SMALL_N	0x48

struct desc {
	unsigned short	w0,w1;
	unsigned char	b4,acc;
	unsigned short	w3;
};

static struct desc		gdt[GDT_N];
static struct desc		idt[IDT_N];
static struct desc		ldt[2];
static unsigned short		gdtr[3],idtr[3],idtr_small[3],idtr_zero[3];
static unsigned short		tss0[22],tss1[22];
static unsigned short		stk0[512],stk1[128],stk3[128];
static unsigned char		tbuf[512];
static unsigned char		ldtbuf[64];
static unsigned short		sx_save[6];		/* SGDT/SIDT results */
static unsigned short		t1_rec[4];		/* recorded by the task switch target: flags, TR, AX, SI */

/* TSS word offsets (286) */
#define TSS_LINK	0
#define TSS_SP0		1
#define TSS_SS0		2
#define TSS_IP		7
#define TSS_FL		8
#define TSS_AX		9
#define TSS_SP		13
#define TSS_ES		17
#define TSS_CS		18
#define TSS_SS		19
#define TSS_DS		20
#define TSS_LDT		21

/* per test case results, written by the protected mode code */
struct pmres {
	unsigned short	done,xcnt,vec,w[6];
	unsigned short	ax,bx,cx,dx,si,di,bp,ds,es,ss,sp,fl;
	unsigned short	h[5],hfl,hss,hcnt;
};

#define MAX_CASES	48
static struct pmres		res[MAX_CASES];
static struct pmres		res_pre;		/* exceptions outside of a test case */
static unsigned			case_mark[MAX_CASES];	/* code offset recorded by mark() */
static unsigned			ncase;

/* protected mode variables */
static unsigned short		pm_cur_res,pm_recover,pm_saved_sp,pm_case,pm_xtotal;
static unsigned char		pm_done;

#define PM_CODE_SIZE	8192
static unsigned char __far	pm_code[PM_CODE_SIZE];
static unsigned char __far	*pp;
static unsigned			stub_off[IDT_N];
static unsigned			h40_off,store0_off,task1_off,gate3_off,exit_off,clim_pad;
static unsigned char __far	*pm_rec_ph;
static unsigned char __far	*exit_jmp_ph;
static unsigned long		pm_code_lin,dgroup_lin;

static unsigned long lin(void __far *p) {
	return ((unsigned long)FP_SEG(p) << 4ul) + (unsigned long)FP_OFF(p);
}

#define DOFF(x)		((unsigned)FP_OFF(&(x)))
#define ROFF(f)		((unsigned)offsetof(struct pmres,f))

/* ---- protected mode code emitter, offsets relative to pm_code (= CODE0 base) */
static unsigned pc(void) {
	return FP_OFF(pp) - FP_OFF(pm_code);
}

static void P(unsigned b) {
	*pp++ = (unsigned char)b;
}

static void PW(unsigned w) {
	P(w & 0xFF);
	P(w >> 8);
}

static void PB(unsigned n,...) {
	va_list va;

	va_start(va,n);
	while (n-- > 0) P(va_arg(va,unsigned));
	va_end(va);
}

static unsigned char __far *pph16(void) {
	unsigned char __far *p = pp;
	PW(0);
	return p;
}

static void pfix16(unsigned char __far *p) {
	*((unsigned short __far*)p) = pc() - ((FP_OFF(p) - FP_OFF(pm_code)) + 2u);
}

static void pfixabs(unsigned char __far *p,unsigned v) {
	*((unsigned short __far*)p) = v;
}

static void prel16(unsigned target) {
	PW(target - (pc() + 2u));
}

/* common instruction sequences */
static void p_movax(unsigned v)		{ P(0xB8); PW(v); }
static void p_movbx(unsigned v)		{ P(0xBB); PW(v); }
static void p_movds_ax(void)		{ PB(2, 0x8E,0xD8); }
static void p_moves_ax(void)		{ PB(2, 0x8E,0xC0); }
static void p_movss_ax(void)		{ PB(2, 0x8E,0xD0); }
static void p_pushw(unsigned v)		{ P(0x68); PW(v); }
static void p_flags(unsigned v)		{ p_pushw(v); P(0x9D); }		/* push imm16; popf */
static void p_lidt(unsigned off)	{ PB(3, 0x0F,0x01,0x1E); PW(off); }

/* store the registers into the current result slot (near call, any SS, DS selector dsel).
 * BX, DS and FLAGS are not restored, the case ends right after */
static void p_store_sub(unsigned dsel) {
	PB(3, 0x9C,0x1E,0x53);					/* pushf; push ds; push bx */
	p_movbx(dsel); PB(2, 0x8E,0xDB);			/* mov ds,bx */
	PB(2, 0x8B,0x1E); PW(DOFF(pm_cur_res));			/* mov bx,[pm_cur_res] */
	PB(2, 0x89,0x87); PW(ROFF(ax));
	PB(2, 0x89,0x8F); PW(ROFF(cx));
	PB(2, 0x89,0x97); PW(ROFF(dx));
	PB(2, 0x89,0xB7); PW(ROFF(si));
	PB(2, 0x89,0xBF); PW(ROFF(di));
	PB(2, 0x89,0xAF); PW(ROFF(bp));
	PB(2, 0x8F,0x87); PW(ROFF(bx));
	PB(2, 0x8F,0x87); PW(ROFF(ds));
	PB(2, 0x8F,0x87); PW(ROFF(fl));
	PB(2, 0x8C,0x87); PW(ROFF(es));
	PB(2, 0x8C,0x97); PW(ROFF(ss));
	PB(2, 0x89,0xA7); PW(ROFF(sp));				/* SP - 2 (return address) */
	PB(2, 0xC7,0x87); PW(ROFF(done)); PW(1);
	P(0xC3);
}

/* restore the ring 0 environment: segments, stack, IDT, flags */
static void p_restore(void) {
	p_movax(SEL_DATA0); p_movss_ax();
	PB(2, 0x8B,0x26); PW(DOFF(pm_saved_sp));		/* mov sp,[pm_saved_sp] */
	p_movds_ax(); p_moves_ax();
	p_lidt(DOFF(idtr));
	p_flags(0x0002);
}

/* fixed part of the protected mode code: entry, exception stubs, handlers */
static void pm_begin(void) {
	unsigned char __far *to_cases;
	unsigned v,common;

	pp = pm_code;
	ncase = 0;
	memset(res,0,sizeof(res));

	/* entry (offset 0) */
	p_movax(SEL_DATA0); p_movds_ax(); p_moves_ax(); p_movss_ax();
	P(0xBC); PW(DOFF(stk0[512-16]));				/* mov sp,stack top */
	p_movax(SEL_TSS0); PB(3, 0x0F,0x00,0xD8);			/* ltr ax */
	PB(2, 0x31,0xC0); PB(3, 0x0F,0x00,0xD0);			/* xor ax,ax; lldt ax */
	p_flags(0x0002);
	P(0xE9); to_cases = pph16();

	/* exception stubs: push vector, jump to the common handler */
	common = pc() + IDT_N * 5u;
	for (v=0;v < IDT_N;v++) {
		stub_off[v] = pc();
		PB(2, 0x6A,v); P(0xE9); prel16(common);
	}

	/* common handler: record registers, vector, the 6 words above it, then resume at the case recovery point */
	PB(4, 0x55,0x8B,0xEC,0x1E); P(0x53);				/* push bp; mov bp,sp; push ds; push bx */
	p_movbx(SEL_DATA0); PB(2, 0x8E,0xDB);
	PB(2, 0x8B,0x1E); PW(DOFF(pm_cur_res));
	PB(2, 0x89,0x87); PW(ROFF(ax));
	PB(2, 0x89,0x8F); PW(ROFF(cx));
	PB(2, 0x89,0x97); PW(ROFF(dx));
	PB(2, 0x89,0xB7); PW(ROFF(si));
	PB(2, 0x89,0xBF); PW(ROFF(di));
	PB(2, 0x8F,0x87); PW(ROFF(bx));
	PB(2, 0x8F,0x87); PW(ROFF(ds));
	PB(2, 0x8C,0x87); PW(ROFF(es));
	PB(3, 0x8B,0x46,0x00); PB(2, 0x89,0x87); PW(ROFF(bp));
	PB(3, 0x8B,0x46,0x02); PB(2, 0x89,0x87); PW(ROFF(vec));
	for (v=0;v < 6;v++) { PB(3, 0x8B,0x46,4+v*2); PB(2, 0x89,0x87); PW(ROFF(w) + v*2u); }
	PB(2, 0xFF,0x87); PW(ROFF(xcnt));				/* inc word [bx+xcnt] */
	PB(2, 0xFF,0x06); PW(DOFF(pm_xtotal));				/* inc word [pm_xtotal] */
	PB(2, 0x83,0x3E); PW(DOFF(pm_xtotal)); P(64);			/* cmp word [pm_xtotal],64 */
	PB(2, 0x72,0x03);						/* jb +3 */
	P(0xE9); { unsigned char __far *p = pph16(); exit_jmp_ph = p; }	/* jmp exit (too many exceptions) */
	p_restore();
	PB(2, 0xFF,0x26); PW(DOFF(pm_recover));			/* jmp [pm_recover] */

	/* INT test handler: records the frame and its own flags, sets SI=4242h, IRET */
	h40_off = pc();
	PB(4, 0x55,0x8B,0xEC,0x1E); PB(2, 0x53,0x50);			/* push bp; mov bp,sp; push ds; push bx; push ax */
	p_movbx(SEL_DATA0); PB(2, 0x8E,0xDB);
	PB(2, 0x8B,0x1E); PW(DOFF(pm_cur_res));
	PB(3, 0x9C,0x8F,0x87); PW(ROFF(hfl));				/* pushf; pop [bx+hfl] */
	for (v=0;v < 5;v++) { PB(3, 0x8B,0x46,2+v*2); PB(2, 0x89,0x87); PW(ROFF(h) + v*2u); }
	PB(2, 0x8C,0xD0); PB(2, 0x89,0x87); PW(ROFF(hss));		/* mov ax,ss */
	PB(2, 0xFF,0x87); PW(ROFF(hcnt));
	PB(4, 0x58,0x5B,0x1F,0x5D);
	P(0xBE); PW(0x4242);						/* mov si,4242h */
	P(0xCF);

	store0_off = pc(); p_store_sub(SEL_DATA0);

	/* task switch target: record flags, TR, AX, SI, then IRET (nested task) back */
	task1_off = pc();
	PB(2, 0x9C,0x5A);						/* pushf; pop dx */
	PB(3, 0x0F,0x00,0xCB);						/* str bx */
	PB(2, 0x89,0x16); PW(DOFF(t1_rec[0]));
	PB(2, 0x89,0x1E); PW(DOFF(t1_rec[1]));
	P(0xA3); PW(DOFF(t1_rec[2]));
	PB(2, 0x89,0x36); PW(DOFF(t1_rec[3]));
	P(0xCF);

	/* landing pad for CLIM:0300h, reached only if a far jump past the CS limit does not fault.
	 * INT 44h (DPL 0 gate, allowed at CPL 0) goes to the exception handler as vector 44h */
	while (pc() < 0x300u) P(0x90);
	clim_pad = pc();
	PB(2, 0xCD,V_INTG0);

	pfix16(to_cases);
}

/* start a test case */
static void case_begin(void) {
	unsigned id = ncase++;

	PB(2, 0xC7,0x06); PW(DOFF(pm_cur_res)); PW(DOFF(res[id]));
	PB(2, 0xC7,0x06); PW(DOFF(pm_case)); PW(id);
	PB(2, 0xC7,0x06); PW(DOFF(pm_recover)); pm_rec_ph = pph16();
	PB(2, 0x89,0x26); PW(DOFF(pm_saved_sp));
	case_mark[id] = 0;
}

/* remember the offset of the instruction expected to fault (or the return address of a trap) */
static void mark(void) {
	case_mark[ncase-1] = pc();
}

/* finish a ring 0 test case: store registers, recovery point */
static void case_end(void) {
	P(0xE8); prel16(store0_off);
	pfixabs(pm_rec_ph,pc());
	p_restore();
}

/* enter ring 3 at the code that follows, with SS:SP = ss3:sp3 */
static void p_to_ring3(unsigned ss3,unsigned sp3) {
	p_pushw(ss3);
	p_pushw(sp3);
	p_pushw(SEL_C3|3);
	p_pushw(pc() + 4u);						/* offset of the code after the RETF */
	P(0xCB);
}

/* finish a ring 3 test case: fault back to ring 0 through the DPL 0 interrupt gate
 * (#GP, error code V_INTG0*8+2). The exception handler records the registers */
static void case_end3(void) {
	PB(2, 0xCD,V_INTG0);
	pfixabs(pm_rec_ph,pc());
	p_restore();
}

static void set_seg(struct desc *d,unsigned long base,unsigned limit,unsigned char acc) {
	d->w0 = limit;
	d->w1 = (unsigned short)(base & 0xFFFFul);
	d->b4 = (unsigned char)((base >> 16ul) & 0xFFul);
	d->acc = acc;
	d->w3 = 0;
}

static void set_gate(struct desc *d,unsigned sel,unsigned off,unsigned char acc) {
	d->w0 = off;
	d->w1 = sel;
	d->b4 = 0;
	d->acc = acc;
	d->w3 = 0;
}

static void set_pseudo(unsigned short *p,void __far *base,unsigned limit) {
	unsigned long l = base ? lin(base) : 0ul;

	p[0] = limit;
	p[1] = (unsigned short)(l & 0xFFFFul);
	p[2] = (unsigned short)((l >> 16ul) & 0xFFul);
}

/* (re)build the descriptor tables. Done before every trip because LTR and task switches set busy bits */
static void pm_tables(void) {
	unsigned long tl = lin(tbuf);
	unsigned v,i;

	memset(gdt,0,sizeof(gdt));
	set_seg(&gdt[SEL_CODE0>>3],pm_code_lin,0xFFFF,0x9A);
	set_seg(&gdt[SEL_DATA0>>3],dgroup_lin,0xFFFF,0x92);
	set_seg(&gdt[SEL_DLIM>>3],tl,0x00FF,0x92);
	set_seg(&gdt[SEL_DED>>3],tl - 0x1000ul,0x0FFF,0x96);
	set_seg(&gdt[SEL_CODEX>>3],pm_code_lin,0xFFFF,0x98);
	set_seg(&gdt[SEL_CODER>>3],pm_code_lin,0xFFFF,0x9A);
	set_seg(&gdt[SEL_DRO>>3],tl,0xFFFF,0x90);
	set_seg(&gdt[SEL_DNP>>3],tl,0xFFFF,0x12);
	set_seg(&gdt[SEL_D3>>3],dgroup_lin,0xFFFF,0xF2);
	set_seg(&gdt[SEL_C3>>3],pm_code_lin,0xFFFF,0xFA);
	set_seg(&gdt[SEL_S3ED>>3],tl - 0x7F00ul,0x7FFF,0xF6);		/* a PUSH that fails to fault lands in tbuf */
	set_seg(&gdt[SEL_S3LIM>>3],tl,0x01FF,0xF2);
	set_seg(&gdt[SEL_SNP>>3],tl,0xFFFF,0x12);
	set_seg(&gdt[SEL_LDT>>3],lin(ldt),sizeof(ldt)-1,0x82);
	set_seg(&gdt[SEL_TSS0>>3],lin(tss0),43,0x81);
	set_seg(&gdt[SEL_TSS1>>3],lin(tss1),43,0x81);
	set_gate(&gdt[SEL_GATE3>>3],SEL_CODE0,gate3_off,0xE4);
	set_seg(&gdt[SEL_CODE0B>>3],pm_code_lin + 16ul,0xFFFF,0x9A);
	set_seg(&gdt[SEL_CLIM>>3],pm_code_lin + (unsigned long)clim_pad - 0x300ul,0x01FF,0x9A);	/* a JMP that fails to fault lands on the pad */

	set_seg(&ldt[0],lin(ldtbuf),sizeof(ldtbuf)-1,0x92);
	set_seg(&ldt[1],lin(ldtbuf),sizeof(ldtbuf)-1,0x92);

	for (v=0;v < IDT_N;v++) set_gate(&idt[v],SEL_CODE0,stub_off[v],0x86);
	set_gate(&idt[V_INTG],SEL_CODE0,h40_off,0x86);
	set_gate(&idt[V_TRAPG],SEL_CODE0,h40_off,0x87);
	set_gate(&idt[V_INTG3],SEL_CODE0,h40_off,0xE6);
	set_gate(&idt[V_NP],SEL_CODE0,h40_off,0x06);
	set_gate(&idt[V_INTG0],SEL_CODE0,stub_off[V_INTG0],0x86);

	memset(tss0,0,sizeof(tss0));
	tss0[TSS_SP0] = DOFF(stk0[512-2]);
	tss0[TSS_SS0] = SEL_DATA0;

	memset(tss1,0,sizeof(tss1));
	tss1[TSS_IP] = task1_off;
	tss1[TSS_FL] = 0x0002;
	for (i=0;i < 8;i++) tss1[TSS_AX+i] = 0x1111u * (i + 1u);
	tss1[TSS_SP] = DOFF(stk1[128-2]);
	tss1[TSS_ES] = SEL_DATA0;
	tss1[TSS_CS] = SEL_CODE0;
	tss1[TSS_SS] = SEL_DATA0;
	tss1[TSS_DS] = SEL_DATA0;

	set_pseudo(gdtr,gdt,sizeof(gdt)-1);
	set_pseudo(idtr,idt,sizeof(idt)-1);
	set_pseudo(idtr_small,idt,IDT_SMALL_N*8-1);
	set_pseudo(idtr_zero,NULL,0);

	for (i=0;i < sizeof(tbuf);i++) tbuf[i] = (unsigned char)(i * 3 + 1);
	for (i=0;i < sizeof(ldtbuf);i++) ldtbuf[i] = (unsigned char)(0xC0 + i);
	memset(sx_save,0,sizeof(sx_save));
	memset(t1_rec,0,sizeof(t1_rec));
	memset(stk0,0,sizeof(stk0));
}

/* real mode code that enters protected mode, and the resume point after the reset */
static unsigned rm_build(void) {
	unsigned d = FP_OFF(thunk) + THUNK_DATA,R;
	unsigned char __far *p = NULL;

	setup();
	E(0xFA);							/* cli */
	EB(3, 0x2E,0x8C,0x16); EW(d+0);					/* mov cs:[ss],ss */
	EB(3, 0x2E,0x89,0x26); EW(d+2);					/* mov cs:[sp],sp */
	EB(3, 0x2E,0x8C,0x1E); EW(d+4);					/* mov cs:[ds],ds */
	if (pc98) {
		/* return stack for the BIOS: far pointer to the resume code, SS:SP at 0000:0404 */
		E(0x0E); E(0xB8); p = ph16(); E(0x50);			/* push cs; mov ax,R; push ax */
		EB(4, 0x31,0xC0,0x8E,0xC0);				/* xor ax,ax; mov es,ax */
		EB(5, 0x26,0x89,0x26,0x04,0x04);			/* mov es:[0404],sp */
		EB(5, 0x26,0x8C,0x16,0x06,0x04);			/* mov es:[0406],ss */
	}
	EB(3, 0x0F,0x01,0x16); EW(DOFF(gdtr));				/* lgdt [gdtr] */
	EB(3, 0x0F,0x01,0x1E); EW(DOFF(idtr));				/* lidt [idtr] */
	EB(3, 0x0F,0x01,0xE0); EB(2, 0x0C,0x01); EB(3, 0x0F,0x01,0xF0);	/* smsw ax; or al,1; lmsw ax */
	E(0xEA); EW(0); EW(SEL_CODE0);					/* jmp far CODE0:0 */

	/* resume after the reset. Nothing can be assumed about the registers */
	R = here();
	if (p) fixabs16(p,R);
	E(0xFA);
	EB(3, 0x2E,0x8E,0x16); EW(d+0);					/* mov ss,cs:[ss] */
	EB(3, 0x2E,0x8B,0x26); EW(d+2);					/* mov sp,cs:[sp] */
	EB(3, 0x2E,0x8E,0x1E); EW(d+4);					/* mov ds,cs:[ds] */
	E(0xCB);							/* back to run() */
	return R;
}

/* end the protected mode code (triple fault), go there and come back. returns 1 if the code ran to the end */
static int pm_go(void) {
	unsigned char m1,m2;
	unsigned short __far *rv;
	unsigned short old_rv[2];
	unsigned R;

	PB(2, 0xC7,0x06); PW(DOFF(pm_cur_res)); PW(DOFF(res_pre));
	PB(2, 0xC6,0x06); PW(DOFF(pm_done)); P(1);			/* mov byte [pm_done],1 */
	exit_off = pc();
	p_lidt(DOFF(idtr_zero));					/* lidt limit 0 */
	PB(4, 0xCC,0xF4,0xEB,0xFD);					/* int 3 -> triple fault; hlt; jmp $-1 */
	*((unsigned short __far*)exit_jmp_ph) = exit_off - ((FP_OFF(exit_jmp_ph) - FP_OFF(pm_code)) + 2u);

	pm_tables();
	pm_done = 0;
	pm_case = 0xFFFF;
	pm_xtotal = 0;
	pm_recover = exit_off;						/* until the first case starts */
	pm_cur_res = DOFF(res_pre);
	memset(&res_pre,0,sizeof(res_pre));
	R = rm_build();
	log_commit();

	_disable();
	if (pc98) {
		m1 = inp(0x02); m2 = inp(0x0A);
		outp(0x02,0xFF); outp(0x0A,0xFF);
		rv = (unsigned short __far*)MK_FP(0x0000,0x0404);
		old_rv[0] = rv[0]; old_rv[1] = rv[1];
		outp(0x37,0x0E);					/* SHUT0 = 0 */
		outp(0x37,0x0A);					/* SHUT1 = 0 */
	}
	else {
		m1 = inp(0x21); m2 = inp(0xA1);
		outp(0x21,0xFF); outp(0xA1,0xFF);
		rv = (unsigned short __far*)MK_FP(0x0040,0x0067);
		old_rv[0] = rv[0]; old_rv[1] = rv[1];
		rv[0] = R; rv[1] = thunk_seg;
		outp(0x70,0x8F); outp(0x71,0x0A);			/* shutdown byte: JMP DWORD PTR 40:67 without EOI */
	}

	run();

	_disable();
	if (pc98) {
		outp(0x37,0x0F);					/* SHUT0 = 1 */
		outp(0x37,0x0B);					/* SHUT1 = 1 */
		rv[0] = old_rv[0]; rv[1] = old_rv[1];
		outp(0x02,m1); outp(0x0A,m2);
	}
	else {
		outp(0x70,0x8F); outp(0x71,0x00);
		outp(0x70,0x0D); (void)inp(0x71);			/* NMI back on */
		rv[0] = old_rv[0]; rv[1] = old_rv[1];
		outp(0x21,m1); outp(0xA1,m2);
	}
	_enable();

	if (res_pre.xcnt) {
		failm("exception %02Xh outside of a test case (case %u), words %04X %04X %04X %04X",res_pre.vec,pm_case,
			res_pre.w[0],res_pre.w[1],res_pre.w[2],res_pre.w[3]);
		return 0;
	}
	if (pm_xtotal >= 64) {
		failm("too many exceptions, gave up in case %u",pm_case);
		return 0;
	}
	if (!pm_done) {
		if (pm_case < ncase) failm("protected mode crashed (reset) during case %u",pm_case);
		else failm("protected mode crashed (reset) before the first case");
		return 0;
	}
	return 1;
}

/* ------------------------------------------------------ result checks */
static int has_errcode(unsigned vec) {
	return vec == 8 || (vec >= 10 && vec <= 13);
}

static const char *pm_what;

static int ex_ok(unsigned i) {
	struct pmres *r = &res[i];

	if (!checkm(r->done && r->xcnt == 0)) {
		if (r->xcnt) failm("%s: unexpected exception %02Xh err=%04X at %04X:%04X",pm_what,r->vec,
			has_errcode(r->vec) ? r->w[0] : 0,has_errcode(r->vec) ? r->w[2] : r->w[1],has_errcode(r->vec) ? r->w[1] : r->w[0]);
		else failm("%s: did not run",pm_what);
		return 0;
	}
	return 1;
}

/* err < 0: no error code. ip = expected pushed IP */
static int ex_fault(unsigned i,unsigned vec,long err,unsigned ip) {
	struct pmres *r = &res[i];
	unsigned rip,rerr;

	if (!checkm(r->xcnt == 1 && r->vec == vec)) {
		if (r->xcnt) failm("%s: exception %02Xh err=%04X, expected %02Xh",pm_what,r->vec,has_errcode(r->vec) ? r->w[0] : 0,vec);
		else failm("%s: exception %02Xh not raised",pm_what,vec);
		return 0;
	}
	rerr = has_errcode(vec) ? r->w[0] : 0;
	rip = has_errcode(vec) ? r->w[1] : r->w[0];
	if (err >= 0 && !checkm(rerr == (unsigned)err)) {
		failm("%s: exception %02Xh error code %04X, expected %04lX",pm_what,vec,rerr,err);
		return 0;
	}
	if (!checkm(rip == ip)) {
		failm("%s: exception %02Xh at IP=%04X, expected %04X",pm_what,vec,rip,ip);
		return 0;
	}
	return 1;
}

static int ex_eq(const char *name,unsigned got,unsigned exp) {
	if (!checkm(got == exp)) {
		failm("%s: %s=%04X, expected %04X",pm_what,name,got,exp);
		return 0;
	}
	return 1;
}

/* ------------------------------------------------------- test groups */

/* ---- entering protected mode, basic state */
static void g_entry(void) {
	unsigned c[8],j;

	begin("PM entry");
	pm_begin();

	case_begin(); c[0] = ncase-1;
	PB(3, 0x0F,0x01,0xE0);						/* smsw ax */
	PB(2, 0x8C,0xCB); PB(2, 0x8C,0xD9); PB(2, 0x8C,0xD2);		/* mov bx,cs; mov cx,ds; mov dx,ss */
	PB(3, 0x0F,0x00,0xC6); PB(3, 0x0F,0x00,0xCF);			/* sldt si; str di */
	case_end();

	case_begin(); c[1] = ncase-1;
	PB(3, 0x0F,0x01,0xE0); PB(2, 0x24,0xFE); PB(3, 0x0F,0x01,0xF0);	/* smsw ax; and al,FEh; lmsw ax */
	PB(3, 0x0F,0x01,0xE3);						/* smsw bx */
	case_end();

	case_begin(); c[2] = ncase-1;
	PB(2, 0x0F,0x01); P(0x06); PW(DOFF(sx_save[0]));		/* sgdt [sx_save] */
	PB(2, 0x0F,0x01); P(0x0E); PW(DOFF(sx_save[3]));		/* sidt [sx_save+6] */
	case_end();

	/* data segment with a base */
	case_begin(); c[3] = ncase-1;
	p_movax(SEL_DLIM); p_movds_ax();
	PB(3, 0xA1,0x00,0x00); PB(4, 0x8B,0x1E,0x10,0x00);		/* mov ax,[0]; mov bx,[10h] */
	PB(6, 0xC7,0x06,0x20,0x00,0x34,0x12);				/* mov word [20h],1234h */
	case_end();

	/* far jump through a code segment with a different base */
	case_begin(); c[4] = ncase-1;
	P(0xEA); PW(pc() + 4u - 16u); PW(SEL_CODE0B);
	PB(2, 0x8C,0xC8);						/* mov ax,cs */
	P(0xEA); PW(pc() + 4u); PW(SEL_CODE0);
	case_end();

	/* far call / retf */
	case_begin(); c[5] = ncase-1;
	{
		unsigned char __far *ps,*pj;
		unsigned ret;

		P(0x9A); ps = pph16(); PW(SEL_CODE0); ret = pc();
		P(0xE9); pj = pph16();
		pfixabs(ps,pc());
		PB(2, 0x8C,0xCA); PB(2, 0x8B,0xEC); PB(3, 0x8B,0x76,0x00); PB(3, 0x8B,0x7E,0x02); P(0xCB);
		pfix16(pj);
		case_end();
		case_mark[c[5]] = ret;
	}

	if (pm_go()) {
		pm_what = "smsw/cs/ds/ss/sldt/str";
		if (ex_ok(c[0])) {
			ex_eq("MSW.PE",res[c[0]].ax & 1,1);
			ex_eq("CS",res[c[0]].bx,SEL_CODE0);
			ex_eq("DS",res[c[0]].cx,SEL_DATA0);
			ex_eq("SS",res[c[0]].dx,SEL_DATA0);
			ex_eq("LDTR",res[c[0]].si,0);
			ex_eq("TR",res[c[0]].di,SEL_TSS0);
			info("MSW in protected mode = %04X",res[c[0]].ax);
		}
		pm_what = "lmsw cannot clear PE";
		if (ex_ok(c[1])) ex_eq("MSW.PE",res[c[1]].bx & 1,1);
		pm_what = "sgdt/sidt";
		if (ex_ok(c[2])) {
			for (j=0;j < 2;j++) {
				unsigned short *s = sx_save + j*3,*e = j ? idtr : gdtr;

				ex_eq(j ? "IDTR limit" : "GDTR limit",s[0],e[0]);
				ex_eq(j ? "IDTR base 15:0" : "GDTR base 15:0",s[1],e[1]);
				ex_eq(j ? "IDTR base 23:16" : "GDTR base 23:16",s[2] & 0xFF,e[2]);
				if (!c_386) ex_eq(j ? "SIDT byte 5 (286 stores FFh)" : "SGDT byte 5 (286 stores FFh)",s[2] >> 8,0xFF);
			}
		}
		pm_what = "data segment base";
		if (ex_ok(c[3])) {
			ex_eq("[0]",res[c[3]].ax,tbuf[0] | (tbuf[1] << 8));
			ex_eq("[10h]",res[c[3]].bx,tbuf[0x10] | (tbuf[0x11] << 8));
			ex_eq("written [20h]",tbuf[0x20] | (tbuf[0x21] << 8),0x1234);
			ex_eq("DLIM accessed bit",gdt[SEL_DLIM>>3].acc & 1,1);
			ex_eq("DRO accessed bit (not loaded)",gdt[SEL_DRO>>3].acc & 1,0);
		}
		pm_what = "jmp far, code base+16";
		if (ex_ok(c[4])) ex_eq("CS",res[c[4]].ax,SEL_CODE0B);
		pm_what = "call far / retf";
		if (ex_ok(c[5])) {
			ex_eq("CS",res[c[5]].dx,SEL_CODE0);
			ex_eq("return IP",res[c[5]].si,case_mark[c[5]]);
			ex_eq("return CS",res[c[5]].di,SEL_CODE0);
		}
	}
	end();
}

/* ---- segment limits */
static void g_limits(void) {
	unsigned c[16],n = 0;

	begin("PM limits");
	pm_begin();

	case_begin(); c[n++] = ncase-1; p_movax(SEL_DLIM); p_movds_ax(); PB(3, 0xA0,0xFF,0x00); case_end();		/* mov al,[FF] */
	case_begin(); c[n++] = ncase-1; p_movax(SEL_DLIM); p_movds_ax(); mark(); PB(3, 0xA0,0x00,0x01); case_end();	/* mov al,[100] */
	case_begin(); c[n++] = ncase-1; p_movax(SEL_DLIM); p_movds_ax(); mark(); PB(3, 0xA1,0xFF,0x00); case_end();	/* mov ax,[FF] */
	case_begin(); c[n++] = ncase-1; p_movax(SEL_DLIM); p_movds_ax(); mark(); PB(3, 0xA2,0x00,0x01); case_end();	/* mov [100],al */
	case_begin(); c[n++] = ncase-1; p_movax(SEL_DLIM); p_moves_ax(); mark(); PB(4, 0x26,0xA0,0x00,0x01); case_end();	/* mov al,es:[100] */
	case_begin(); c[n++] = ncase-1; p_movax(SEL_DED); p_movds_ax(); PB(3, 0xA0,0x00,0x10); case_end();		/* expand-down [1000] */
	case_begin(); c[n++] = ncase-1; p_movax(SEL_DED); p_movds_ax(); mark(); PB(3, 0xA0,0xFF,0x0F); case_end();	/* expand-down [0FFF] */
	case_begin(); c[n++] = ncase-1; p_movax(SEL_DED); p_movds_ax(); PB(3, 0xA0,0xFF,0xFF); case_end();		/* expand-down byte [FFFF] */
	case_begin(); c[n++] = ncase-1; p_movax(SEL_DED); p_movds_ax(); mark(); PB(3, 0xA1,0xFF,0xFF); case_end();	/* expand-down word [FFFF] */
	case_begin(); c[n++] = ncase-1; mark(); P(0xEA); PW(0x0300); PW(SEL_CLIM); case_end();			/* jmp far beyond code limit */
	case_begin(); c[n++] = ncase-1; PB(2, 0x31,0xC0); p_movds_ax(); mark(); PB(3, 0xA0,0x00,0x00); case_end();	/* null DS access */
	case_begin(); c[n++] = ncase-1; PB(2, 0x31,0xC0); mark(); p_movss_ax(); case_end();				/* mov ss,null */
	case_begin(); c[n++] = ncase-1; p_movbx(0xFFFF); mark(); PB(2, 0x8B,0x07); case_end();			/* mov ax,[FFFF] limit FFFF */

	if (pm_go()) {
		pm_what = "mov al,[00FF] limit 00FF";	if (ex_ok(c[0])) ex_eq("AL",res[c[0]].ax & 0xFF,tbuf[0xFF]);
		pm_what = "mov al,[0100] limit 00FF";	ex_fault(c[1],13,0,case_mark[c[1]]);
		pm_what = "mov ax,[00FF] limit 00FF";	ex_fault(c[2],13,0,case_mark[c[2]]);
		pm_what = "mov [0100],al limit 00FF";	ex_fault(c[3],13,0,case_mark[c[3]]);
		pm_what = "mov al,es:[0100]";		ex_fault(c[4],13,0,case_mark[c[4]]);
		pm_what = "expand-down mov al,[1000]";	if (ex_ok(c[5])) ex_eq("AL",res[c[5]].ax & 0xFF,tbuf[0]);
		pm_what = "expand-down mov al,[0FFF]";	ex_fault(c[6],13,0,case_mark[c[6]]);
		pm_what = "expand-down mov al,[FFFF]";	ex_ok(c[7]);
		pm_what = "expand-down mov ax,[FFFF]";	ex_fault(c[8],13,0,case_mark[c[8]]);
		pm_what = "jmp far past code limit";	ex_fault(c[9],13,0,case_mark[c[9]]);
		pm_what = "access through null DS";	ex_fault(c[10],13,0,case_mark[c[10]]);
		pm_what = "mov ss,null";		ex_fault(c[11],13,0,case_mark[c[11]]);
		pm_what = "mov ax,[FFFF] limit FFFF";	ex_fault(c[12],13,0,case_mark[c[12]]);
	}
	end();
}

/* ---- loading segment registers with various descriptors */
static void ld_case(unsigned sel,int ss,int do_mark) {
	case_begin();
	p_movax(sel);
	if (do_mark) mark();
	if (ss) p_movss_ax(); else p_movds_ax();
}

static void g_selectors(void) {
	unsigned c[20],n = 0;

	begin("PM selectors");
	pm_begin();

	ld_case(SEL_BEYOND,0,1); c[n++] = ncase-1; case_end();
	ld_case(SEL_CODEX,0,1); c[n++] = ncase-1; case_end();
	ld_case(SEL_CODER,0,0); c[n++] = ncase-1; PB(3, 0xA0,0x00,0x00); case_end();		/* read code through DS */
	ld_case(SEL_CODER,0,0); c[n++] = ncase-1; mark(); PB(3, 0xA2,0x00,0x00); case_end();	/* write code through DS */
	ld_case(SEL_DRO,0,0); c[n++] = ncase-1; PB(3, 0xA0,0x00,0x00); case_end();
	ld_case(SEL_DRO,0,0); c[n++] = ncase-1; mark(); PB(3, 0xA2,0x00,0x00); case_end();
	ld_case(SEL_DRO,1,1); c[n++] = ncase-1; case_end();
	ld_case(SEL_DNP,0,1); c[n++] = ncase-1; case_end();
	ld_case(SEL_SNP,1,1); c[n++] = ncase-1; case_end();
	ld_case(SEL_DLIM|3,0,1); c[n++] = ncase-1; case_end();
	ld_case(SEL_D3|3,0,0); c[n++] = ncase-1; case_end();
	ld_case(SEL_TSS1,0,1); c[n++] = ncase-1; case_end();
	ld_case(SEL_GATE3,0,1); c[n++] = ncase-1; case_end();
	ld_case(SEL_D3|3,1,1); c[n++] = ncase-1; case_end();

	/* LDT */
	case_begin(); c[n++] = ncase-1;
	p_movax(SEL_LDT); PB(3, 0x0F,0x00,0xD0);			/* lldt ax */
	p_movax(0x0004); p_moves_ax();					/* LDT entry 0 */
	PB(4, 0x26,0xA1,0x00,0x00);					/* mov ax,es:[0] */
	PB(3, 0x0F,0x00,0xC3);						/* sldt bx */
	case_end();

	case_begin(); c[n++] = ncase-1;
	p_movax(SEL_LDT); PB(3, 0x0F,0x00,0xD0);
	p_movax(0x0014); mark(); p_moves_ax();				/* LDT entry 2, beyond the LDT limit */
	case_end();

	case_begin(); c[n++] = ncase-1;
	PB(2, 0x31,0xC0); PB(3, 0x0F,0x00,0xD0);			/* lldt 0 */
	p_movax(0x0004); mark(); p_moves_ax();
	case_end();

	if (pm_go()) {
		pm_what = "mov ds, beyond GDT limit";	ex_fault(c[0],13,SEL_BEYOND,case_mark[c[0]]);
		pm_what = "mov ds, execute-only code";	ex_fault(c[1],13,SEL_CODEX,case_mark[c[1]]);
		pm_what = "mov ds, readable code; read"; if (ex_ok(c[2])) ex_eq("AL",res[c[2]].ax & 0xFF,0xB8);	/* mov ax,imm at CODE0:0 */
		pm_what = "write through code DS";	ex_fault(c[3],13,0,case_mark[c[3]]);
		pm_what = "mov ds, read-only; read";	if (ex_ok(c[4])) ex_eq("AL",res[c[4]].ax & 0xFF,0x01);	/* tbuf[0] as set by pm_tables() */
		pm_what = "write through read-only DS";	ex_fault(c[5],13,0,case_mark[c[5]]);
		pm_what = "mov ss, read-only data";	ex_fault(c[6],13,SEL_DRO,case_mark[c[6]]);
		pm_what = "mov ds, not present";	ex_fault(c[7],11,SEL_DNP,case_mark[c[7]]);
		pm_what = "mov ss, not present";	ex_fault(c[8],12,SEL_SNP,case_mark[c[8]]);
		pm_what = "mov ds, DPL 0 with RPL 3";	ex_fault(c[9],13,SEL_DLIM,case_mark[c[9]]);
		pm_what = "mov ds, DPL 3 with RPL 3";	ex_ok(c[10]);
		pm_what = "mov ds, TSS";		ex_fault(c[11],13,SEL_TSS1,case_mark[c[11]]);
		pm_what = "mov ds, call gate";		ex_fault(c[12],13,SEL_GATE3,case_mark[c[12]]);
		pm_what = "mov ss, DPL 3 at CPL 0";	ex_fault(c[13],13,SEL_D3,case_mark[c[13]]);
		pm_what = "LDT selector";
		if (ex_ok(c[14])) {
			ex_eq("es:[0]",res[c[14]].ax,ldtbuf[0] | (ldtbuf[1] << 8));
			ex_eq("SLDT",res[c[14]].bx,SEL_LDT);
		}
		pm_what = "LDT selector beyond LDT limit"; ex_fault(c[15],13,0x0014,case_mark[c[15]]);
		pm_what = "LDT selector with no LDT";	ex_fault(c[16],13,0x0004,case_mark[c[16]]);
		pm_what = "accessed bits";
		ex_eq("DRO accessed",gdt[SEL_DRO>>3].acc & 1,1);
		ex_eq("CODER accessed",gdt[SEL_CODER>>3].acc & 1,1);
		ex_eq("DNP accessed",gdt[SEL_DNP>>3].acc & 1,0);
		ex_eq("LDT[0] accessed",ldt[0].acc & 1,1);
	}
	end();
}

/* ---- LAR/LSL/VERR/VERW/ARPL */
struct sel_op {
	const char	*what;
	unsigned char	op2,modrm;		/* 0F op2 modrm, or op2=0x63 for ARPL */
	unsigned short	ax,bx;			/* inputs */
	unsigned char	zf;			/* expected ZF */
	unsigned short	ax_exp,ax_mask;		/* expected AX (if ZF=1 or ARPL) */
};

static const struct sel_op sel_ops[] = {
	{ "lar CODE0",		0x02,0xC3, 0x5555,SEL_CODE0,	1, 0x9A00,0xFEFF },
	{ "lar null",		0x02,0xC3, 0x5555,0x0000,	0, 0x5555,0xFFFF },
	{ "lar DPL0 with RPL3",	0x02,0xC3, 0x5555,SEL_DLIM|3,	0, 0x5555,0xFFFF },
	{ "lar busy TSS",	0x02,0xC3, 0x5555,SEL_TSS0,	1, 0x8300,0xFFFF },
	{ "lar call gate",	0x02,0xC3, 0x5555,SEL_GATE3,	1, 0xE400,0xFFFF },
	{ "lar beyond GDT",	0x02,0xC3, 0x5555,SEL_BEYOND,	0, 0x5555,0xFFFF },
	{ "lar DPL3 data",	0x02,0xC3, 0x5555,SEL_D3|3,	1, 0xF200,0xFEFF },
	{ "lsl limit 00FF",	0x03,0xC3, 0x5555,SEL_DLIM,	1, 0x00FF,0xFFFF },
	{ "lsl expand-down",	0x03,0xC3, 0x5555,SEL_DED,	1, 0x0FFF,0xFFFF },
	{ "lsl TSS",		0x03,0xC3, 0x5555,SEL_TSS0,	1, 0x002B,0xFFFF },
	{ "lsl LDT",		0x03,0xC3, 0x5555,SEL_LDT,	1, sizeof(ldt)-1,0xFFFF },
	{ "lsl call gate",	0x03,0xC3, 0x5555,SEL_GATE3,	0, 0x5555,0xFFFF },
	{ "verr readable code",	0x00,0xE3, 0x5555,SEL_CODE0,	1, 0x5555,0xFFFF },
	{ "verr execute-only",	0x00,0xE3, 0x5555,SEL_CODEX,	0, 0x5555,0xFFFF },
	{ "verr read-only data",0x00,0xE3, 0x5555,SEL_DRO,	1, 0x5555,0xFFFF },
	{ "verr null",		0x00,0xE3, 0x5555,0x0000,	0, 0x5555,0xFFFF },
	{ "verw data",		0x00,0xEB, 0x5555,SEL_DATA0,	1, 0x5555,0xFFFF },
	{ "verw read-only data",0x00,0xEB, 0x5555,SEL_DRO,	0, 0x5555,0xFFFF },
	{ "verw code",		0x00,0xEB, 0x5555,SEL_CODER,	0, 0x5555,0xFFFF },
	{ "verw DPL0 with RPL3",0x00,0xEB, 0x5555,SEL_DLIM|3,	0, 0x5555,0xFFFF },
	{ "arpl raise RPL",	0x63,0xD8, 0x0010,0x0003,	1, 0x0013,0xFFFF },
	{ "arpl keep RPL",	0x63,0xD8, 0x0013,0x0001,	0, 0x0013,0xFFFF },
};
#define N_SEL_OPS (sizeof(sel_ops)/sizeof(sel_ops[0]))

static void g_selops(void) {
	unsigned i,first;

	begin("LAR/LSL/VER");
	pm_begin();
	first = ncase;
	for (i=0;i < N_SEL_OPS;i++) {
		const struct sel_op *s = &sel_ops[i];

		case_begin();
		p_movax(s->ax); p_movbx(s->bx);
		p_flags(s->zf ? 0x0002 : 0x0042);			/* ZF set to the opposite of the expected result */
		if (s->op2 == 0x63) PB(2, 0x63,s->modrm);
		else PB(3, 0x0F,s->op2,s->modrm);
		case_end();
	}

	if (pm_go()) {
		for (i=0;i < N_SEL_OPS;i++) {
			const struct sel_op *s = &sel_ops[i];
			struct pmres *r = &res[first+i];

			pm_what = s->what;
			if (!ex_ok(first+i)) continue;
			ex_eq("ZF",(r->fl & ZF) ? 1 : 0,s->zf);
			if (s->zf || s->op2 == 0x63 || s->ax_exp == 0x5555) ex_eq("AX",r->ax & s->ax_mask,s->ax_exp & s->ax_mask);
		}
	}
	end();
}

/* ---- interrupts and exceptions in protected mode */
static void g_int(void) {
	unsigned c[12],n = 0,a[12];

	begin("PM INT/IRET");
	pm_begin();

	/* interrupt gate clears IF */
	case_begin(); c[n] = ncase-1; P(0xFB); PB(2, 0xCD,V_INTG); a[n++] = pc(); PB(3, 0x9C,0x5A,0xFA); case_end();
	/* trap gate leaves IF alone */
	case_begin(); c[n] = ncase-1; P(0xFB); PB(2, 0xCD,V_TRAPG); a[n++] = pc(); PB(3, 0x9C,0x5A,0xFA); case_end();
	/* INT 3, INTO (traps: IP after the instruction) */
	case_begin(); c[n] = ncase-1; P(0xCC); a[n++] = pc(); case_end();
	case_begin(); c[n] = ncase-1; PB(4, 0xB0,0x7F,0x04,0x01); P(0xCE); a[n++] = pc(); case_end();
	/* #DE, #UD (faults: IP of the instruction) */
	case_begin(); c[n] = ncase-1; PB(2, 0xB3,0x00); a[n++] = pc(); PB(2, 0xF6,0xF3); case_end();
	case_begin(); c[n] = ncase-1; a[n++] = pc(); PB(2, 0x0F,0x0B); case_end();
	/* INT beyond the IDT limit, INT through a not present gate */
	case_begin(); c[n] = ncase-1; p_lidt(DOFF(idtr_small)); a[n++] = pc(); PB(2, 0xCD,V_BEYOND); case_end();
	case_begin(); c[n] = ncase-1; a[n++] = pc(); PB(2, 0xCD,V_NP); case_end();
	/* IRET loads IOPL at CPL 0 */
	case_begin(); c[n] = ncase-1;
	{
		unsigned char __far *p;

		p_pushw(0x3002); P(0x0E); P(0x68); p = pph16(); P(0xCF); pfixabs(p,pc());
		PB(2, 0x9C,0x5A);
	}
	a[n++] = 0; case_end();
	/* POPF loads IOPL and NT at CPL 0 */
	case_begin(); c[n] = ncase-1; p_flags(0x3002); PB(2, 0x9C,0x5A); a[n++] = 0; case_end();
	case_begin(); c[n] = ncase-1; p_flags(0x4002); PB(2, 0x9C,0x5A); p_flags(0x0002); a[n++] = 0; case_end();

	if (pm_go()) {
		struct pmres *r;

		pm_what = "int via interrupt gate"; r = &res[c[0]];
		if (ex_ok(c[0])) {
			ex_eq("handler count",r->hcnt,1);
			ex_eq("IF in handler",(r->hfl & IF) ? 1 : 0,0);
			ex_eq("frame IP",r->h[0],a[0]);
			ex_eq("frame CS",r->h[1],SEL_CODE0);
			ex_eq("frame IF",(r->h[2] & IF) ? 1 : 0,1);
			ex_eq("IF after IRET",(r->dx & IF) ? 1 : 0,1);
			ex_eq("SI from handler",r->si,0x4242);
		}
		pm_what = "int via trap gate"; r = &res[c[1]];
		if (ex_ok(c[1])) {
			ex_eq("handler count",r->hcnt,1);
			ex_eq("IF in handler",(r->hfl & IF) ? 1 : 0,1);
		}
		pm_what = "int 3";		ex_fault(c[2],3,-1,a[2]);
		pm_what = "into";		ex_fault(c[3],4,-1,a[3]);
		pm_what = "div by zero";	ex_fault(c[4],0,-1,a[4]);
		pm_what = "invalid opcode";	ex_fault(c[5],6,-1,a[5]);
		pm_what = "int beyond IDT limit"; ex_fault(c[6],13,V_BEYOND*8+2,a[6]);
		pm_what = "int not present gate"; ex_fault(c[7],11,V_NP*8+2,a[7]);
		pm_what = "iret IOPL=3";	if (ex_ok(c[8])) ex_eq("IOPL",(res[c[8]].dx >> 12) & 3,3);
		pm_what = "popf IOPL=3";	if (ex_ok(c[9])) ex_eq("IOPL",(res[c[9]].dx >> 12) & 3,3);
		pm_what = "popf NT=1";		if (ex_ok(c[10])) ex_eq("NT",(res[c[10]].dx >> 14) & 1,1);
	}
	end();
}

/* ---- ring 3 */
static void g_ring3(void) {
	unsigned c[8],n = 0,a[8],sp3 = DOFF(stk3[128-4]);

	begin("Ring 3");
	pm_begin();

	/* in ring 3: CS/SS/SP, INT through a DPL 3 interrupt gate (stack switch) */
	case_begin(); c[n] = ncase-1; p_to_ring3(SEL_D3|3,sp3);
	PB(2, 0x8C,0xC8); PB(2, 0x8C,0xD3); PB(2, 0x89,0xE1);		/* mov ax,cs; mov bx,ss; mov cx,sp */
	PB(2, 0xCD,V_INTG3); a[n++] = pc(); case_end3();
	/* CLI and IN with IOPL 0 */
	case_begin(); c[n] = ncase-1; p_to_ring3(SEL_D3|3,sp3); a[n++] = pc(); P(0xFA); case_end3();
	case_begin(); c[n] = ncase-1; p_to_ring3(SEL_D3|3,sp3); P(0xBA); PW(pc98 ? 0x5F : 0x80); a[n++] = pc(); P(0xEC); case_end3();
	/* POPF cannot change IOPL or IF */
	case_begin(); c[n] = ncase-1; p_to_ring3(SEL_D3|3,sp3); p_flags(0x3202); PB(2, 0x9C,0x5A); a[n++] = 0; case_end3();
	/* DPL 0 data segment */
	case_begin(); c[n] = ncase-1; p_to_ring3(SEL_D3|3,sp3); p_movax(SEL_DLIM); a[n++] = pc(); p_movds_ax(); case_end3();
	/* stack faults: the exception switches to the ring 0 stack */
	case_begin(); c[n] = ncase-1; p_to_ring3(SEL_S3ED|3,0x8000); a[n++] = pc(); P(0x50); case_end3();
	case_begin(); c[n] = ncase-1; p_to_ring3(SEL_S3LIM|3,0x01FE); P(0x58); a[n++] = pc(); P(0x58); case_end3();
	/* call gate back to ring 0 */
	case_begin(); c[n] = ncase-1; p_to_ring3(SEL_D3|3,sp3);
	P(0x9A); PW(0); PW(SEL_GATE3|3); a[n++] = pc();
	gate3_off = pc();
	PB(2, 0x8C,0xC8); PB(2, 0x8C,0xD3); PB(2, 0x8B,0xEC);		/* mov ax,cs; mov bx,ss; mov bp,sp */
	PB(3, 0x8B,0x4E,0x00); PB(3, 0x8B,0x56,0x02); PB(3, 0x8B,0x76,0x04); PB(3, 0x8B,0x7E,0x06);
	case_end();

	if (pm_go()) {
		struct pmres *r;
		unsigned exit_err = V_INTG0*8+2;

		pm_what = "ring 3 + int via DPL3 gate"; r = &res[c[0]];
		if (ex_fault(c[0],13,exit_err,res[c[0]].w[1])) {
			ex_eq("CS",r->ax,SEL_C3|3);
			ex_eq("SS",r->bx,SEL_D3|3);
			ex_eq("SP",r->cx,sp3);
			ex_eq("handler count",r->hcnt,1);
			ex_eq("frame IP",r->h[0],a[0]);
			ex_eq("frame CS",r->h[1],SEL_C3|3);
			ex_eq("frame SP",r->h[3],sp3);
			ex_eq("frame SS",r->h[4],SEL_D3|3);
			ex_eq("handler SS",r->hss,SEL_DATA0);
			ex_eq("SI from handler",r->si,0x4242);
			ex_eq("#GP frame CS",r->w[2],SEL_C3|3);
		}
		pm_what = "cli at CPL 3, IOPL 0";	ex_fault(c[1],13,0,a[1]);
		pm_what = "in at CPL 3, IOPL 0";	ex_fault(c[2],13,0,a[2]);
		pm_what = "popf at CPL 3";
		if (ex_fault(c[3],13,exit_err,res[c[3]].w[1])) {
			ex_eq("IOPL",(res[c[3]].dx >> 12) & 3,0);
			ex_eq("IF",(res[c[3]].dx & IF) ? 1 : 0,0);
		}
		pm_what = "mov ds, DPL 0 at CPL 3";	ex_fault(c[4],13,SEL_DLIM,a[4]);
		pm_what = "push, expand-down SS";
		if (ex_fault(c[5],12,0,a[5])) ex_eq("frame SS",res[c[5]].w[5],SEL_S3ED|3);
		pm_what = "pop past SS limit";		ex_fault(c[6],12,0,a[6]);
		pm_what = "call gate to ring 0"; r = &res[c[7]];
		if (ex_ok(c[7])) {
			ex_eq("CS",r->ax,SEL_CODE0);
			ex_eq("SS from TSS",r->bx,SEL_DATA0);
			ex_eq("return IP",r->cx,a[7]);
			ex_eq("return CS",r->dx,SEL_C3|3);
			ex_eq("caller SP",r->si,sp3);
			ex_eq("caller SS",r->di,SEL_D3|3);
		}
	}
	end();
}

/* ---- task switch through a 286 TSS */
static void g_task(void) {
	unsigned c[2],a1;

	begin("Task switch");
	pm_begin();

	case_begin(); c[0] = ncase-1;
	p_movax(0x5A5A); P(0xBE); PW(0x1122);				/* mov ax,5A5Ah; mov si,1122h */
	P(0x9A); PW(0); PW(SEL_TSS1);					/* call far TSS1:0 */
	PB(2, 0x9C,0x5A);						/* pushf; pop dx */
	case_end();

	case_begin(); c[1] = ncase-1; a1 = pc(); P(0xEA); PW(0); PW(SEL_TSS0); case_end();	/* jmp to the busy TSS */

	if (pm_go()) {
		struct pmres *r = &res[c[0]];

		pm_what = "call TSS / iret";
		if (ex_ok(c[0])) {
			ex_eq("AX preserved",r->ax,0x5A5A);
			ex_eq("SI preserved",r->si,0x1122);
			ex_eq("NT after return",(r->dx >> 14) & 1,0);
			ex_eq("new task NT",(t1_rec[0] >> 14) & 1,1);
			ex_eq("new task TR",t1_rec[1],SEL_TSS1);
			ex_eq("new task AX",t1_rec[2],0x1111);
			ex_eq("new task SI",t1_rec[3],0x7777);
			ex_eq("back link",tss1[TSS_LINK],SEL_TSS0);
			ex_eq("TSS1 IP saved after IRET",tss1[TSS_IP],task1_off + 21u);
			ex_eq("TSS1 type after IRET (available)",gdt[SEL_TSS1>>3].acc,0x81);
			ex_eq("TSS0 type (busy)",gdt[SEL_TSS0>>3].acc,0x83);
		}
		pm_what = "jmp to busy TSS"; ex_fault(c[1],13,SEL_TSS0,a1);
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
	pm_code_lin = lin(pm_code);
	dgroup_lin = (unsigned long)dgroup << 4ul;

	logf = fopen("286P.TXT","w");

	video_init();

	detect_cpu();
	sprintf(tmp,"286 CPU protected mode test (%s)",pc98 ? "NEC PC-98" : (ibm_mono ? "IBM PC, mono" : "IBM PC, color"));
	put_str(0,0,COL_TITLE,tmp);
	sprintf(tmp,"CPU: %s",cpu_name);
	put_str(0,1,COL_NORM,tmp);
	if (logf) {
		fprintf(logf,"286 CPU protected mode test, %s\n",pc98 ? "NEC PC-98" : "IBM PC");
		fprintf(logf,"%s\n\n",tmp);
		log_commit();
	}

	if (!is286p || c_v86) {
		strcpy(tmp,c_v86 ? "Virtual 8086 mode is active. Remove EMM386 or similar and try again." :
			"This test requires an 80286 or later CPU.");
		show_bottom(23,COL_FAIL,tmp);
		if (logf) fprintf(logf,"%s\n",tmp);
	}
	else if (ss_val != dgroup) {
		strcpy(tmp,"SS != DGROUP, cannot run.");
		show_bottom(23,COL_FAIL,tmp);
		if (logf) fprintf(logf,"%s\n",tmp);
	}
	else {
		g_entry();
		g_limits();
		g_selectors();
		g_selops();
		g_int();
		g_ring3();
		g_task();
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
