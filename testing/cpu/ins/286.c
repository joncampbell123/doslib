/* 286.c
 *
 * 80186/80286 CPU instruction and functionality test for MS-DOS, real mode only.
 * (C) 2026 Jonathan Campbell, DOSLIB project.
 *
 * Tests the instructions and behavior the 80186 and 80286 added to the 8086
 * as seen from real mode: PUSH imm, PUSHA/POPA, IMUL imm, shift/rotate by
 * imm8 and shift count masking, ENTER/LEAVE, BOUND, INS/OUTS, then 286 only:
 * SMSW/LMSW/CLTS, SGDT/SIDT/LGDT/LIDT, PUSH SP and FLAGS behavior, the invalid
 * opcode exception, and the #GP raised by a word access that wraps past
 * offset FFFFh. Protected mode is not tested: returning to real mode from
 * protected mode on a 286 needs platform specific tricks (reset via the
 * keyboard controller, triple fault, LOADALL) that do not belong here.
 *
 * The 80186 level tests also run on an 80186/80188 and NEC V20/V30. The 286
 * level tests are skipped there, and the privileged and exception tests are
 * skipped in virtual 8086 mode (EMM386 and friends) where they would trap
 * to the memory manager instead of the real mode IVT.
 *
 * How it works: same as 8086.c. Each test case writes a few bytes of machine
 * code into a "thunk" buffer ending in RETF, loads the registers and FLAGS
 * from ri[], far calls the thunk, then saves the registers and FLAGS into
 * ro[]. The C code computes the expected state in re[] and compares.
 *
 * 16-bit large memory model only. The 32-bit build is a stub.
 *
 * Results are also written to 286.TXT in the current directory. The file is
 * committed to disk (fflush + fsync) each time a screen page of results is
 * complete, so that it is readable after a reboot if a later test crashes.
 *
 * Command line: -n   do not wait for a key at the end */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
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
static unsigned char __far	xbuf[1024];	/* far data buffer (ES target) */
static unsigned char __far	*tp;		/* thunk write pointer */
#define THUNK_DATA		0x380		/* CS: data area within thunk */

static unsigned short		memw[256];	/* DGROUP memory operand area */
#define memb			((unsigned char*)memw)
static unsigned char		tbl[256];	/* DGROUP table (XLAT, strings) */

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
			logf = fopen("286.TXT","a");
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

/* failure with register diff from verify() */
static void failr(const char *fmt,...) {
	va_list va;
	va_start(va,fmt);
	failv(1,fmt,va);
	va_end(va);
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

static void skip(const char *why) {
	grp_skip = 1;
	if (logf) fprintf(logf,"  SKIP %s: %s\n",grp_name,why);
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

static const char *rname[R_MAX] = { "AX","CX","DX","BX","SP","BP","SI","DI","ES","FL" };

/* compare ro against re. fmask selects which FLAGS bits to compare */
static int verify(unsigned fmask) {
	char *p = diffbuf;
	int i,ok = 1;

	grp_tests++;
	*p = 0;
	for (i=0;i < R_FL;i++) {
		if (i == R_SP) continue;
		if (ro[i] != re[i]) {
			p += sprintf(p," %s=%04X(%04X)",rname[i],ro[i],re[i]);
			ok = 0;
		}
	}
	if ((ro[R_FL] ^ re[R_FL]) & fmask) {
		p += sprintf(p," FL=%04X(%04X&%04X)",ro[R_FL],re[R_FL],fmask);
		ok = 0;
	}
	if (sp_before != sp_after) {
		p += sprintf(p," SP%+d",(int)(sp_after - sp_before));
		ok = 0;
	}
	if (x_unexpected) {
		p += sprintf(p," INT %02X raised at %04X",x_vec,x_ip);
		ok = 0;
	}
	if (!ok) sprintf(p,"  in AX=%04X CX=%04X DX=%04X BX=%04X FL=%04X",ri[R_AX],ri[R_CX],ri[R_DX],ri[R_BX],ri[R_FL]);
	return ok;
}

/* 8-bit register access by reg8 encoding (AL CL DL BL AH CH DH BH) */
static void set8(unsigned short *r,unsigned i,unsigned v) {
	if (i & 4) r[i&3] = (r[i&3] & 0x00FFu) | ((v & 0xFFu) << 8u);
	else       r[i&3] = (r[i&3] & 0xFF00u) | (v & 0xFFu);
}

/* ------------------------------------------------- interrupt handlers */
static void x_common(unsigned vec,union INTPACK __far *r) {
	x_cnt++;
	x_vec = vec;
	x_ip = r->w.ip;
	x_cs = r->w.cs;
	x_fl = r->w.flags;
	/* faults resume at the end of the thunk, software interrupts return normally */
	if (x_resume && (vec == 0x00 || vec == 0x05 || vec == 0x06 || vec == 0x0D)) {
		r->w.ip = x_resume;
		r->w.cs = thunk_seg;
		x_faulted = 1;
	}
}

static void (__interrupt __far *old_int05)();
static void (__interrupt __far *old_int06)();
static void (__interrupt __far *old_int0d)();

volatile unsigned short		x_gp_ip;	/* IP of the instruction expected to #GP */

/* INT 5: BOUND range exceeded (also Print Screen on the IBM PC) */
static void __interrupt __far __loadds int05_h(union INTPACK r) {
	x_common(5,&r);
}

/* INT 6: invalid opcode */
static void __interrupt __far __loadds int06_h(union INTPACK r) {
	x_common(6,&r);
}

/* INT 0Dh: #GP, but also IRQ 5 on both the IBM PC and PC-98. Only take it
 * if it came from the instruction under test, chain to the previous handler otherwise */
static void __interrupt __far __loadds int0d_h(union INTPACK r) {
	if (x_resume && r.w.cs == thunk_seg && r.w.ip == x_gp_ip) {
		x_common(0x0D,&r);
		return;
	}
	_chain_intr(old_int0d);
}

static void hook_vectors(void) {
	old_int05 = _dos_getvect(0x05); _dos_setvect(0x05,int05_h);
	old_int06 = _dos_getvect(0x06); _dos_setvect(0x06,int06_h);
	old_int0d = _dos_getvect(0x0D); _dos_setvect(0x0D,int0d_h);
}

static void unhook_vectors(void) {
	_dos_setvect(0x05,old_int05);
	_dos_setvect(0x06,old_int06);
	_dos_setvect(0x0D,old_int0d);
}

static void x_clear(void) {
	x_cnt = x_vec = x_ip = x_cs = x_fl = x_live = x_resume = 0;
	ss_cnt = 0;
}

/* ------------------------------------------------- reference model */
static unsigned char partab[256];

static void init_parity(void) {
	unsigned i,j,c;

	for (i=0;i < 256;i++) {
		for (c=0,j=i;j;j>>=1) c += j & 1;
		partab[i] = (c & 1) ? 0 : 1;
	}
}

static unsigned szp(unsigned long r,int w) {
	unsigned f = 0;

	if (w) { if ((r & 0xFFFFul) == 0) f |= ZF; if (r & 0x8000ul) f |= SF; }
	else   { if ((r & 0xFFul) == 0) f |= ZF;   if (r & 0x80ul) f |= SF; }
	if (partab[r & 0xFF]) f |= PF;
	return f;
}


enum { SH_ROL=0, SH_ROR, SH_RCL, SH_RCR, SH_SHL, SH_SHR, SH_SAL, SH_SAR };
static const char *sh_names[8] = { "ROL","ROR","RCL","RCR","SHL","SHR","SAL","SAR" };

/* shift/rotate reference */
static unsigned shift_ref(int op,int w,unsigned v,unsigned cnt,unsigned fin,unsigned *fo,unsigned *fm) {
	unsigned msb = w ? 0x8000u : 0x80u,mask = w ? 0xFFFFu : 0xFFu;
	unsigned c = cnt,cf = fin & CF,orig,n,i,f;

	if (c_shmask) c &= 31;
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

	f = fin & ~CF;
	if (cf) f |= CF;

	/* OF, defined only for count == 1 */
	f &= ~OF;
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
	if (c != 1) *fm &= ~OF;

	if (op >= SH_SHL) {
		f = (f & ~(SF|ZF|PF)) | szp(v,w);
		*fm &= ~AF;	/* undefined */
	}

	*fo = f;
	return v;
}

/* test operand values */
static const unsigned short v16[16] = {
	0x0000,0x0001,0x0002,0x007F,0x0080,0x00FF,0x0100,0x7FFE,
	0x7FFF,0x8000,0x8001,0xFFFE,0xFFFF,0x1234,0x5A5A,0xA5C3
};

static const unsigned char v8[16] = {
	0x00,0x01,0x02,0x09,0x0F,0x10,0x3C,0x55,
	0x7E,0x7F,0x80,0x81,0x99,0xAA,0xFE,0xFF
};

static unsigned sext8(unsigned v) {
	return (v & 0x80) ? (v | 0xFF00u) : (v & 0xFFu);
}

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

/* ================================================================== TESTS */

static unsigned char		is186p;		/* 80186 instruction set available */

/* run the thunk, resuming at its final RETF if an exception handler fires */
static void go_exc(void) {
	x_clear();
	exc_expected = 1;
	go();
	exc_expected = 0;
}

/* ---- PUSH imm16 / PUSH simm8 */
static void t_push_imm(void) {
	unsigned i;

	begin("PUSH imm");
	for (i=0;i < 16;i++) {
		setup(); E(0x68); EW(v16[i]); E(0x58); go(); re[R_AX] = v16[i];
		if (!verify(ARITH|CTRL)) failr("68 push %04X; pop ax",v16[i]);

		setup(); EB(3, 0x6A,v8[i],0x5A); go(); re[R_DX] = sext8(v8[i]);
		if (!verify(ARITH|CTRL)) failr("6A push %02X; pop dx",v8[i]);
	}

	/* stack contents: push imm16; mov bp,sp; mov cx,[bp+0]; pop ax */
	setup(); E(0x68); EW(0x8421); EB(5, 0x8B,0xEC,0x8B,0x4E,0x00); E(0x58); go();
	re[R_AX] = re[R_CX] = 0x8421; re[R_BP] = sp_before - 6;
	if (!verify(ARITH|CTRL)) failr("68 push imm16 stack contents");
	end();
}

/* ---- PUSHA/POPA */
static void t_pusha(void) {
	static const unsigned char order[8] = { R_DI,R_SI,R_BP,R_SP,R_BX,R_DX,R_CX,R_AX };	/* pop order after PUSHA */
	unsigned k,ok;

	begin("PUSHA/POPA");

	/* pusha, then pop all eight words into memw[0..7] */
	setup(); E(0x60); for (k=0;k < 8;k++) { EB(2, 0x8F,0x06); EW(memw_off + k*2u); } go();
	ok = verify(ARITH|CTRL);
	for (k=0;k < 8;k++) {
		unsigned e = (order[k] == R_SP) ? (sp_before - 4u) : ri[order[k]];

		if (!checkm(memw[k] == e)) {
			sprintf(diffbuf+strlen(diffbuf)," %s=%04X(%04X)",rname[order[k]],memw[k],e);
			ok = 0;
		}
	}
	if (!ok) failr("60 pusha stack image");

	/* push eight words in PUSHA order, then popa. The SP image is discarded */
	for (k=0;k < 8;k++) memw[k] = 0x1111u * (k + 1u) ^ 0x0F0Fu;
	setup(); for (k=0;k < 8;k++) { EB(2, 0xFF,0x36); EW(memw_off + k*2u); } E(0x61); go();
	re[R_AX] = memw[0]; re[R_CX] = memw[1]; re[R_DX] = memw[2]; re[R_BX] = memw[3];
	re[R_BP] = memw[5]; re[R_SI] = memw[6]; re[R_DI] = memw[7];
	if (!verify(ARITH|CTRL)) failr("61 popa");

	/* pusha; popa round trip */
	setup(); EB(2, 0x60,0x61); go();
	if (!verify(ARITH|CTRL)) failr("60 61 pusha; popa");
	end();
}

/* ---- IMUL r16,r/m16,imm */
static void imul3_expect(unsigned dst,unsigned a,unsigned b,unsigned fin) {
	long r = (long)(short)a * (long)(short)b;

	re[dst] = (unsigned)(r & 0xFFFFl);
	re[R_FL] = (r != (long)(short)(unsigned)(r & 0xFFFFl)) ? (fin|CF|OF) : (fin & ~(CF|OF));
}

static void t_imul3(void) {
	unsigned i,j,fin;

	begin("IMUL imm");
	for (i=0;i < 16;i++) {
		for (j=0;j < 16;j++) {
			fin = ((i ^ j) & 1) ? (0x0202|ARITH) : 0x0202;

			setup(); ri[R_FL] = fin; ri[R_BX] = v16[i]; EB(2, 0x69,0xD3); EW(v16[j]); go();
			imul3_expect(R_DX,v16[i],v16[j],fin);
			if (!verify(CF|OF|CTRL)) failr("69 D3 imul dx,bx,%04X bx=%04X",v16[j],v16[i]);

			setup(); ri[R_FL] = fin; ri[R_BX] = v16[i]; EB(3, 0x6B,0xD3,v8[j]); go();
			imul3_expect(R_DX,v16[i],sext8(v8[j]),fin);
			if (!verify(CF|OF|CTRL)) failr("6B D3 imul dx,bx,%02X bx=%04X",v8[j],v16[i]);

			setup(); ri[R_FL] = fin; ri[R_AX] = v16[i]; EB(2, 0x69,0xC0); EW(v16[j]); go();
			imul3_expect(R_AX,v16[i],v16[j],fin);
			if (!verify(CF|OF|CTRL)) failr("69 C0 imul ax,ax,%04X ax=%04X",v16[j],v16[i]);

			setup(); ri[R_FL] = fin; memw[0] = v16[i]; ri[R_BX] = memw_off; EB(3, 0x6B,0x37,v8[j]); go();
			imul3_expect(R_SI,v16[i],sext8(v8[j]),fin);
			if (!verify(CF|OF|CTRL) || !checkm(memw[0] == v16[i])) failr("6B 37 imul si,[bx],%02X mem=%04X",v8[j],v16[i]);
		}
	}
	end();
}

/* ---- shifts/rotates by imm8 (C0h/C1h), and by CL with count masking */
enum { SI_R8=0, SI_R16, SI_M16, SI_M8, SI_R16_CL, SI_MAX };
static const char *si_form_names[SI_MAX] = { "C0 r8,imm8","C1 r16,imm8","C1 m16,imm8","C0 m8,imm8","D3 r16,CL" };
static const unsigned char si_counts[] = { 0,1,2,3,7,8,9,15,16,17,31,32,33,40,255 };
#define SI_COUNTS (sizeof(si_counts)/sizeof(si_counts[0]))

static void shimm_case(unsigned op,unsigned form,unsigned v,unsigned cnt,unsigned fin) {
	unsigned w,res,fo,fm,ok,mem_exp = 0;
	int memtest = 0;

	setup();
	ri[R_FL] = fin;
	switch (form) {
		case SI_R8:     w=0; ri[R_AX] = 0x5A00|v; EB(3, 0xC0,0xC0|(op<<3),cnt); break;
		case SI_R16:    w=1; ri[R_DX] = v; EB(3, 0xC1,0xC2|(op<<3),cnt); break;
		case SI_M16:    w=1; memw[0] = v; ri[R_BX] = memw_off; EB(3, 0xC1,0x07|(op<<3),cnt); memtest = 1; break;
		case SI_M8:     w=0; memw[0] = 0x9900|v; ri[R_BX] = memw_off; EB(3, 0xC0,0x07|(op<<3),cnt); memtest = 1; break;
		case SI_R16_CL: w=1; ri[R_SI] = v; ri[R_CX] = 0x6600|cnt; EB(2, 0xD3,0xC6|(op<<3)); break;
		default: return;
	}
	go();

	res = shift_ref(op,w,v,cnt,fin,&fo,&fm);
	re[R_FL] = fo;
	switch (form) {
		case SI_R8:     set8(re,0,res); break;
		case SI_R16:    re[R_DX] = res; break;
		case SI_M16:    mem_exp = res; break;
		case SI_M8:     mem_exp = 0x9900|res; break;
		case SI_R16_CL: re[R_SI] = res; break;
	}

	ok = verify(fm|CTRL);
	if (memtest && !checkm(memw[0] == mem_exp)) {
		ok = 0;
		sprintf(diffbuf+strlen(diffbuf)," mem=%04X(%04X)",memw[0],mem_exp);
	}
	if (!ok) failr("%s %s v=%04X cnt=%u",sh_names[op],si_form_names[form],v,cnt);
}

static void t_shift_imm(unsigned op) {
	char name[16];
	unsigned form,i,c,v,fin;

	sprintf(name,"%s imm",sh_names[op]);
	begin(name);
	if (is286p && !checkm(c_shmask)) failm("286+ should mask shift counts to 5 bits");
	for (form=0;form < SI_MAX;form++) {
		for (i=0;i < 16;i++) {
			v = (form == SI_R8 || form == SI_M8) ? v8[i] : v16[i];
			for (c=0;c < SI_COUNTS;c++) {
				fin = ((i ^ c) & 1) ? (0x0202|ARITH) : 0x0202;
				shimm_case(op,form,v,si_counts[c],fin);
				if (op == SH_RCL || op == SH_RCR) shimm_case(op,form,v,si_counts[c],fin ^ CF);
			}
		}
	}
	end();
}

/* ---- ENTER/LEAVE. Needs SS == DGROUP so the display area can live in memw[] */
static void enter_case(unsigned size,unsigned level) {
	unsigned T = memw_off + 0x100u,L = level & 31u,npop = L ? (L + 1u) : 1u;
	unsigned sp0,frame,k,ok,e;

	for (k=0;k < 8;k++) memw[120+k] = 0xA000u | k;	/* SS:[T-16] .. SS:[T-2] */
	for (k=0;k < 8;k++) memw[k] = 0;

	setup(); ri[R_BP] = T;
	E(0xC8); EW(size); E(level);			/* enter size,level */
	EB(4, 0x8B,0xD4,0x8B,0xCD);			/* mov dx,sp; mov cx,bp */
	EB(2, 0x81,0xC4); EW(size);			/* add sp,size */
	for (k=0;k < npop;k++) { EB(2, 0x8F,0x06); EW(memw_off + k*2u); }	/* pop [memw+k*2] */
	go();

	sp0 = sp_before - 4u;				/* SP inside the thunk */
	frame = sp0 - 2u;
	re[R_DX] = frame - 2u*L - size;
	re[R_CX] = re[R_BP] = frame;
	ok = verify(CTRL);

	for (k=0;k < npop;k++) {
		if (L == 0) e = T;
		else if (k == 0) e = frame;			/* frame pointer pushed last */
		else if (k == L) e = T;				/* old BP pushed first */
		else e = memw[128u - (L - k)];			/* copy of SS:[T - 2*(L-k)] */
		if (!checkm(memw[k] == e)) {
			sprintf(diffbuf+strlen(diffbuf)," stack[%u]=%04X(%04X)",k,memw[k],e);
			ok = 0;
		}
	}
	if (!ok) failr("C8 enter %u,%u",size,level);
}

static void t_enter(void) {
	static const unsigned short sizes[3] = { 0,6,0x100 };
	static const unsigned char levels[6] = { 0,1,2,3,5,7 };
	unsigned s,l;

	begin("ENTER/LEAVE");
	if (ss_val != dgroup) {
		skip("SS != DGROUP");
		end();
		return;
	}
	for (s=0;s < 3;s++) {
		for (l=0;l < 6;l++) enter_case(sizes[s],levels[l]);
	}
	if (is286p) {
		enter_case(4,33);				/* level is taken modulo 32 */
	}

	/* push bp; mov bp,sp; sub sp,10; leave */
	setup(); EB(7, 0x55,0x8B,0xEC,0x83,0xEC,0x0A,0xC9); go();
	if (!verify(CTRL)) failr("C9 leave");

	/* enter 16,3; leave */
	if (checkm(1)) {
		unsigned T = memw_off + 0x100u;

		setup(); ri[R_BP] = T; EB(5, 0xC8,0x10,0x00,0x03,0xC9); go();
		if (!verify(CTRL)) failr("C8 10 00 03 C9 enter 16,3; leave");
	}
	end();
}

/* ---- BOUND */
static void t_bound(void) {
	static const unsigned short av[8] = { 0xFFFA,0xFFFB,0x0000,0x0005,0x000A,0x000B,0x8000,0x7FFF };
	unsigned i,s,out;

	begin("BOUND");
	for (i=0;i < 8;i++) {
		memw[0] = 0xFFFB; memw[1] = 10;			/* -5 .. 10 */
		memw[2] = 0x8000; memw[3] = 0x0000;		/* -32768 .. 0 */

		out = ((short)av[i] < -5 || (short)av[i] > 10) ? 1 : 0;
		setup(); ri[R_AX] = av[i]; ri[R_BX] = memw_off; s = here(); EB(2, 0x62,0x07); go_exc();
		if (!verify(ARITH|CTRL)) failr("62 07 bound ax,[bx] ax=%04X",av[i]);
		if (out) {
			if (!checkm(x_cnt == 1 && x_vec == 5 && x_cs == thunk_seg))
				failm("bound ax=%04X -5..10: INT 5 not raised (count=%u)",av[i],x_cnt);
			else if (!checkm(x_ip == s || (!is286p && x_ip == s + 2u)))
				failm("bound ax=%04X: INT 5 pushed IP=%04X, instruction at %04X",av[i],x_ip,s);
		}
		else if (!checkm(x_cnt == 0)) {
			failm("bound ax=%04X -5..10: INT 5 raised for a value in range",av[i]);
		}

		out = ((short)av[i] > 0) ? 1 : 0;
		setup(); ri[R_DX] = av[i]; ri[R_BX] = memw_off; s = here(); EB(3, 0x62,0x57,0x04); go_exc();
		if (!verify(ARITH|CTRL)) failr("62 57 04 bound dx,[bx+4] dx=%04X",av[i]);
		if (!checkm(out ? (x_cnt == 1 && x_vec == 5 && (x_ip == s || (!is286p && x_ip == s + 3u))) : (x_cnt == 0)))
			failm("bound dx=%04X -32768..0: INT 5 count=%u ip=%04X(%04X)",av[i],x_cnt,x_ip,s);
	}
	end();
}

/* ---- INS/OUTS. Uses the I/O delay port (80h on IBM PC, 5Fh on PC-98), only the
 * pointer and count updates are checked since the data read back is undefined */
static unsigned S0,D0;

static void ins_check(const char *what,unsigned di_rel,unsigned bytes,int df) {
	unsigned i,lo = df ? (di_rel + 1u - bytes) : di_rel,hi = lo + bytes;

	for (i=0;i < 64;i++) {
		if (i >= lo && i < hi) continue;
		if (!checkm(xbuf[i] == 0xEE)) {
			failm("%s wrote outside the destination, es:[%u]=%02X",what,i,xbuf[i]);
			return;
		}
	}
}

static void t_insouts(void) {
	unsigned port = pc98 ? 0x5F : 0x80;

	begin("INS/OUTS");

	setup(); ri[R_DX] = port; ri[R_SI] = S0; E(0x6E); go(); re[R_SI] = S0 + 1;
	if (!verify(ARITH|CTRL)) failr("6E outsb");

	setup(); ri[R_DX] = port; ri[R_SI] = S0; E(0x6F); go(); re[R_SI] = S0 + 2;
	if (!verify(ARITH|CTRL)) failr("6F outsw");

	setup(); ri[R_DX] = port; ri[R_SI] = S0; ri[R_CX] = 3; EB(2, 0xF3,0x6E); go(); re[R_SI] = S0 + 3; re[R_CX] = 0;
	if (!verify(ARITH|CTRL)) failr("F3 6E rep outsb cx=3");

	setup(); ri[R_DX] = port; ri[R_SI] = S0 + 10; ri[R_CX] = 2; ri[R_FL] |= DF; EB(2, 0xF3,0x6F); go(); re[R_SI] = S0 + 6; re[R_CX] = 0;
	if (!verify(ARITH|CTRL)) failr("F3 6F rep outsw cx=2 DF=1");

	setup(); ri[R_DX] = port; ri[R_SI] = S0; ri[R_CX] = 0; EB(2, 0xF3,0x6E); go();
	if (!verify(ARITH|CTRL)) failr("F3 6E rep outsb cx=0");

	setup(); ri[R_DX] = port; ri[R_SI] = D0; ri[R_ES] = FP_SEG(xbuf); EB(2, 0x26,0x6E); go(); re[R_SI] = D0 + 1;
	if (!verify(ARITH|CTRL)) failr("26 6E es: outsb");

	_fmemset(xbuf,0xEE,64);
	setup(); ri[R_DX] = port; ri[R_ES] = FP_SEG(xbuf); ri[R_DI] = D0 + 8; E(0x6C); go(); re[R_DI] = D0 + 9;
	if (!verify(ARITH|CTRL)) failr("6C insb");
	ins_check("6C insb",8,1,0);

	_fmemset(xbuf,0xEE,64);
	setup(); ri[R_DX] = port; ri[R_ES] = FP_SEG(xbuf); ri[R_DI] = D0 + 8; E(0x6D); go(); re[R_DI] = D0 + 10;
	if (!verify(ARITH|CTRL)) failr("6D insw");
	ins_check("6D insw",8,2,0);

	_fmemset(xbuf,0xEE,64);
	setup(); ri[R_DX] = port; ri[R_ES] = FP_SEG(xbuf); ri[R_DI] = D0 + 8; ri[R_CX] = 4; EB(2, 0xF3,0x6C); go(); re[R_DI] = D0 + 12; re[R_CX] = 0;
	if (!verify(ARITH|CTRL)) failr("F3 6C rep insb cx=4");
	ins_check("F3 6C rep insb",8,4,0);

	_fmemset(xbuf,0xEE,64);
	setup(); ri[R_DX] = port; ri[R_ES] = FP_SEG(xbuf); ri[R_DI] = D0 + 20; ri[R_CX] = 2; ri[R_FL] |= DF; EB(2, 0xF3,0x6D); go(); re[R_DI] = D0 + 16; re[R_CX] = 0;
	if (!verify(ARITH|CTRL)) failr("F3 6D rep insw cx=2 DF=1");
	ins_check("F3 6D rep insw DF=1",21,4,1);

	/* segment override does not apply to the INS destination */
	_fmemset(xbuf,0xEE,64);
	setup(); ri[R_DX] = port; ri[R_ES] = FP_SEG(xbuf); ri[R_DI] = D0 + 8; EB(2, 0x2E,0x6C); go(); re[R_DI] = D0 + 9;
	if (!verify(ARITH|CTRL)) failr("2E 6C cs: insb");
	ins_check("2E 6C cs: insb",8,1,0);
	end();
}

/* ---- PUSH SP: 286+ push the value of SP before the push */
static void t_push_sp(void) {
	begin("PUSH SP");
	if (!is286p) { skip("not a 286"); end(); return; }
	setup(); EB(4, 0x8B,0xD4,0x54,0x58); go();
	re[R_DX] = re[R_AX] = sp_before - 4u;
	if (!verify(ARITH|CTRL)) failr("8B D4 54 58 mov dx,sp; push sp; pop ax");
	end();
}

/* ---- FLAGS bits 12-15 in real mode */
static void t_flags(void) {
	unsigned lo,hi;

	begin("FLAGS 15:12");
	if (!is286p) { skip("not a 286"); end(); return; }
	setup(); E(0xB8); EW(0x0202); EB(4, 0x50,0x9D,0x9C,0x58); go();
	lo = ro[R_AX] & 0xF000;
	setup(); E(0xB8); EW(0xF202); EB(4, 0x50,0x9D,0x9C,0x58); go();
	hi = ro[R_AX] & 0xF000;
	info("FLAGS[15:12] after POPF 0000h=%X F000h=%X",lo >> 12,hi >> 12);
	if (!c_386) {
		if (!checkm(lo == 0 && hi == 0)) failm("286 real mode: IOPL/NT/bit 15 should read 0");
	}
	else {
		if (!checkm(lo == 0 && (hi & 0x8000) == 0)) failm("386+: bit 15 should read 0");
		if (!c_v86 && !checkm(hi == 0x7000)) failm("386+ real mode: IOPL and NT should be writable");
	}
	end();
}

/* ---- SMSW */
static unsigned msw_val;

static void t_smsw(void) {
	begin("SMSW");
	if (!is286p) { skip("not a 286"); end(); return; }
	setup(); EB(3, 0x0F,0x01,0xE0); go();
	msw_val = ro[R_AX];
	re[R_AX] = msw_val;
	if (!verify(ARITH|CTRL)) failr("0F 01 E0 smsw ax");
	info("MSW=%04X (PE=%u MP=%u EM=%u TS=%u)",msw_val,msw_val & 1,(msw_val >> 1) & 1,(msw_val >> 2) & 1,(msw_val >> 3) & 1);
	if (!c_v86 && !checkm((msw_val & 1) == 0)) failm("MSW.PE set but not in virtual 8086 mode");
	if (is286p && !c_386 && !checkm((msw_val & 0xFFF0) == 0xFFF0)) failm("286: MSW bits 4-15 should read 1");

	setup(); memw[0] = 0x5555; ri[R_BX] = memw_off; EB(3, 0x0F,0x01,0x27); go();
	if (!verify(ARITH|CTRL) || !checkm(memw[0] == msw_val)) failr("0F 01 27 smsw [bx] = %04X",memw[0]);
	end();
}

/* ---- SGDT/SIDT */
static void t_sxdt(void) {
	unsigned long base;

	begin("SGDT/SIDT");
	if (!is286p) { skip("not a 286"); end(); return; }

	memw[0] = memw[1] = memw[2] = 0x5A5A;
	setup(); ri[R_BX] = memw_off; EB(3, 0x0F,0x01,0x0F); go();
	if (!verify(ARITH|CTRL)) failr("0F 01 0F sidt [bx]");
	base = memw[1] | ((unsigned long)(memw[2] & 0xFF) << 16ul);
	info("IDTR limit=%04X base=%06lX byte 5=%02X",memw[0],base,memw[2] >> 8);
	if (!c_v86 && !checkm(memw[0] == 0x03FF && base == 0)) failm("real mode IDTR should be base 0 limit 3FFh");
	if (!c_386 && !checkm((memw[2] >> 8) == 0xFF)) failm("286: SIDT should store FFh in byte 5");

	memw[0] = memw[1] = memw[2] = 0x5A5A;
	setup(); ri[R_BX] = memw_off; EB(3, 0x0F,0x01,0x07); go();
	if (!verify(ARITH|CTRL)) failr("0F 01 07 sgdt [bx]");
	base = memw[1] | ((unsigned long)(memw[2] & 0xFF) << 16ul);
	info("GDTR limit=%04X base=%06lX byte 5=%02X",memw[0],base,memw[2] >> 8);
	if (!c_386 && !checkm((memw[2] >> 8) == 0xFF)) failm("286: SGDT should store FFh in byte 5");
	end();
}

/* ---- LMSW/CLTS/LIDT/LGDT, written back with the values already loaded */
static void t_lxdt(void) {
	unsigned k;

	begin("LMSW/LIDT");
	if (!is286p || c_v86) { skip(c_v86 ? "virtual 8086 mode" : "not a 286"); end(); return; }

	/* smsw ax; lmsw ax; smsw bx */
	setup(); EB(9, 0x0F,0x01,0xE0,0x0F,0x01,0xF0,0x0F,0x01,0xE3); go();
	re[R_AX] = re[R_BX] = ro[R_AX];
	if (!verify(ARITH|CTRL) || !checkm((ro[R_AX] & 1) == 0)) failr("smsw; lmsw; smsw");

	/* clts; smsw ax */
	setup(); EB(5, 0x0F,0x06,0x0F,0x01,0xE0); go();
	re[R_AX] = ro[R_AX];
	if (!verify(ARITH|CTRL) || !checkm((ro[R_AX] & 8) == 0)) failr("0F 06 clts, MSW=%04X",ro[R_AX]);

	/* sidt [bx]; lidt [bx]; sidt [bx+8] */
	for (k=0;k < 8;k++) memw[k] = 0x5A5A;
	setup(); ri[R_BX] = memw_off; EB(10, 0x0F,0x01,0x0F,0x0F,0x01,0x1F,0x0F,0x01,0x4F,0x08); go();
	if (!verify(ARITH|CTRL) || !checkm(memw[0] == memw[4] && memw[1] == memw[5] && (memw[2] & 0xFF) == (memw[6] & 0xFF)))
		failr("lidt round trip %04X %04X %04X / %04X %04X %04X",memw[0],memw[1],memw[2],memw[4],memw[5],memw[6]);

	/* sgdt [bx]; lgdt [bx]; sgdt [bx+8] */
	for (k=0;k < 8;k++) memw[k] = 0x5A5A;
	setup(); ri[R_BX] = memw_off; EB(10, 0x0F,0x01,0x07,0x0F,0x01,0x17,0x0F,0x01,0x47,0x08); go();
	if (!verify(ARITH|CTRL) || !checkm(memw[0] == memw[4] && memw[1] == memw[5] && (memw[2] & 0xFF) == (memw[6] & 0xFF)))
		failr("lgdt round trip %04X %04X %04X / %04X %04X %04X",memw[0],memw[1],memw[2],memw[4],memw[5],memw[6]);
	end();
}

/* ---- invalid opcode exception (INT 6), pushed IP points at the instruction */
static void ud_case(const char *what,unsigned n,const unsigned char *code) {
	unsigned s,i;

	setup(); ri[R_AX] = thunk_seg;		/* if "mov cs,ax" executes anyway, CS does not change */
	ri[R_BX] = memw_off;
	s = here();
	for (i=0;i < n;i++) E(code[i]);
	go_exc();
	if (!checkm(x_cnt == 1 && x_vec == 6 && x_cs == thunk_seg && x_ip == s && sp_before == sp_after))
		failm("%s: INT 6 count=%u vec=%02X ip=%04X(%04X)",what,x_cnt,x_vec,x_ip,s);
}

static void t_ud(void) {
	static const unsigned char c0[] = { 0x0F,0x0B };
	static const unsigned char c1[] = { 0x8D,0xC0 };
	static const unsigned char c2[] = { 0xC4,0xC0 };
	static const unsigned char c3[] = { 0xC5,0xC0 };
	static const unsigned char c4[] = { 0x8E,0xC8 };
	static const unsigned char c5[] = { 0x62,0xC0 };
	static const unsigned char c6[] = { 0x63,0xC0 };
	static const unsigned char c7[] = { 0x0F,0x00,0xC0 };
	static const unsigned char c8[] = { 0x0F,0x02,0xC0 };
	static const unsigned char c9[] = { 0x0F,0x03,0xC0 };
	static const unsigned char c10[] = { 0x0F,0x00,0xE0 };

	begin("#UD");
	if (!is286p || c_v86) { skip(c_v86 ? "virtual 8086 mode" : "not a 286"); end(); return; }
	ud_case("0F 0B",2,c0);
	ud_case("8D C0 lea ax,ax",2,c1);
	ud_case("C4 C0 les ax,ax",2,c2);
	ud_case("C5 C0 lds ax,ax",2,c3);
	ud_case("8E C8 mov cs,ax",2,c4);
	ud_case("62 C0 bound ax,ax",2,c5);
	ud_case("63 C0 arpl (real mode)",2,c6);
	ud_case("0F 00 C0 sldt (real mode)",3,c7);
	ud_case("0F 02 C0 lar (real mode)",3,c8);
	ud_case("0F 03 C0 lsl (real mode)",3,c9);
	ud_case("0F 00 E0 verr (real mode)",3,c10);
	end();
}

/* ---- word access at offset FFFFh: wraps to offset 0000h on 8086/80186, #GP (INT 0Dh) on 286+.
 * Runs with DS = a 64KB+16 block of our own, so that offset 0000h, offset FFFFh and the byte
 * just past the segment (linear base+10000h, where a non-wrapping emulator would go) are all ours */
static unsigned wrap_seg;

static unsigned char __far *wrap_p(unsigned long off) {
	return (unsigned char __far*)MK_FP(wrap_seg + (unsigned)(off >> 4ul),(unsigned)(off & 15ul));
}

static void wrap_fill(void) {
	*wrap_p(0x00000ul) = 0x11;
	*wrap_p(0x00001ul) = 0x12;
	*wrap_p(0x0FFFFul) = 0x22;
	*wrap_p(0x10000ul) = 0x33;
}

/* push ds; mov ds,cx; <instruction>; pop ds. A fault resumes at the POP DS */
static void wrap_case(const char *what,unsigned n,const unsigned char *code,int expect_fault) {
	unsigned s,i;

	ri[R_CX] = wrap_seg;
	EB(3, 0x1E,0x8E,0xD9);
	s = here();
	for (i=0;i < n;i++) E(code[i]);
	x_gp_ip = s;
	x_clear();
	x_resume = here();
	x_faulted = 0;
	EB(2, 0x1F,0xCB);
	memcpy(re,ri,sizeof(re));
	exc_expected = 1;
	run();
	exc_expected = 0;
	x_resume = 0;
	x_unexpected = 0;

	if (expect_fault) {
		if (!checkm(x_cnt == 1 && x_vec == 0x0D && x_ip == s && sp_before == sp_after))
			failm("%s: #GP count=%u ip=%04X(%04X)",what,x_cnt,x_ip,s);
	}
	else {
		if (!checkm(x_cnt == 0)) failm("%s: unexpected #GP",what);
	}
}

static void t_segwrap(void) {
	static const unsigned char c0[] = { 0x8B,0x07 };			/* mov ax,[bx] */
	static const unsigned char c1[] = { 0x8B,0x47,0xEF };			/* mov ax,[bx-11h] */
	static const unsigned char c2[] = { 0x8A,0x07 };			/* mov al,[bx] */
	static const unsigned char c3[] = { 0x89,0x07 };			/* mov [bx],ax */
	static const unsigned char c4[] = { 0xA5 };				/* movsw */
	static const unsigned char c5[] = { 0xFF,0x37,0x58 };			/* push [bx]; pop ax */
	int f = is286p;

	begin("Wrap #GP");
	if (c_v86) { skip("virtual 8086 mode"); end(); return; }
	if (_dos_allocmem(0x1001,&wrap_seg) != 0) { skip("cannot allocate 64KB"); end(); return; }

	wrap_fill();
	setup(); ri[R_BX] = 0xFFFF; wrap_case("8B 07 mov ax,[FFFF]",2,c0,f);
	if (x_cnt) info("word access at FFFFh raises #GP");
	else if (ro[R_AX] == 0x1122) info("word access at FFFFh wraps to offset 0000h");
	else info("word access at FFFFh gave AX=%04X (wrap=1122h, past segment=3322h)",ro[R_AX]);
	if (!f) { re[R_AX] = 0x1122; if (!verify(ARITH|CTRL)) failr("mov ax,[FFFF] should wrap to offset 0"); }

	setup(); ri[R_BX] = 0x0010; wrap_case("8B 47 EF mov ax,[bx-11h] = [FFFF]",3,c1,f);
	if (!f) { re[R_AX] = 0x1122; if (!verify(ARITH|CTRL)) failr("mov ax,[bx-11h] should wrap to offset 0"); }

	setup(); ri[R_BX] = 0xFFFF; wrap_case("8A 07 mov al,[FFFF] (byte)",2,c2,0);
	set8(re,0,0x22);
	if (!verify(ARITH|CTRL)) failr("8A 07 mov al,[FFFF]");

	wrap_fill();
	setup(); ri[R_BX] = 0xFFFF; ri[R_AX] = 0xA5B6; wrap_case("89 07 mov [FFFF],ax",2,c3,f);
	if (f) {
		if (!checkm(*wrap_p(0xFFFFul) == 0x22 && *wrap_p(0ul) == 0x11 && *wrap_p(0x10000ul) == 0x33))
			failm("mov [FFFF],ax: memory changed despite #GP");
	}
	else if (!checkm(*wrap_p(0xFFFFul) == 0xB6 && *wrap_p(0ul) == 0xA5 && *wrap_p(0x10000ul) == 0x33)) {
		failm("mov [FFFF],ax should write offsets FFFFh and 0000h: [FFFF]=%02X [0]=%02X [past]=%02X",
			*wrap_p(0xFFFFul),*wrap_p(0ul),*wrap_p(0x10000ul));
	}

	wrap_fill();
	_fmemset(xbuf,0xEE,4);
	setup(); ri[R_SI] = 0xFFFF; ri[R_DI] = D0; ri[R_ES] = FP_SEG(xbuf); wrap_case("A5 movsw from [FFFF]",1,c4,f);
	if (!f && !checkm(xbuf[0] == 0x22 && xbuf[1] == 0x11)) failm("movsw from [FFFF] should wrap, got %02X %02X",xbuf[0],xbuf[1]);

	wrap_fill();
	setup(); ri[R_BX] = 0xFFFF; wrap_case("FF 37 push [FFFF]",3,c5,f);
	if (!f) { re[R_AX] = 0x1122; if (!verify(ARITH|CTRL)) failr("push [FFFF] should wrap to offset 0"); }

	_dos_freemem(wrap_seg);
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
	S0 = FP_OFF(tbl);
	D0 = FP_OFF(xbuf);
	for (i=0;i < 256;i++) tbl[i] = (unsigned char)(0x30 + i);
	init_parity();

	logf = fopen("286.TXT","w");

	video_init();
	hook_vectors();

	detect_cpu();
	is186p = is8086 ? 0 : 1;
	sprintf(tmp,"286 CPU instruction test, real mode (%s)",pc98 ? "NEC PC-98" : (ibm_mono ? "IBM PC, mono" : "IBM PC, color"));
	put_str(0,0,COL_TITLE,tmp);
	sprintf(tmp,"CPU: %s  [FLAGS 15:12 %s, shift count %s, AAD imm %s]",cpu_name,
		c_flagsF ? "set" : "clear",c_shmask ? "masked" : "unmasked",c_necaad ? "ignored" : "used");
	put_str(0,1,COL_NORM,tmp);
	if (logf) {
		fprintf(logf,"286 CPU instruction test (real mode), %s\n",pc98 ? "NEC PC-98" : "IBM PC");
		fprintf(logf,"%s\n\n",tmp);
		log_commit();
	}

	if (!is186p) {
		strcpy(tmp,"This test requires an 80186, NEC V20/V30, 80286 or later CPU.");
		show_bottom(23,COL_FAIL,tmp);
		if (logf) fprintf(logf,"%s\n",tmp);
	}
	else {
		t_push_imm();
		t_pusha();
		t_imul3();
		t_shift_imm(SH_ROL);
		t_shift_imm(SH_ROR);
		t_shift_imm(SH_RCL);
		t_shift_imm(SH_RCR);
		t_shift_imm(SH_SHL);
		t_shift_imm(SH_SHR);
		t_shift_imm(SH_SAR);
		t_enter();
		t_bound();
		t_insouts();
		t_push_sp();
		t_flags();
		t_smsw();
		t_sxdt();
		t_lxdt();
		t_ud();
		t_segwrap();
	}

	unhook_vectors();

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
