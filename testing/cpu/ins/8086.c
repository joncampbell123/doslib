/* 8086.c
 *
 * 8086-level CPU instruction and functionality test for MS-DOS.
 * (C) 2026 Jonathan Campbell, DOSLIB project.
 *
 * Tests the 8086 instruction set (documented instructions plus a few well
 * known undocumented ones) and reports results directly in text video RAM.
 * Intended for real hardware, DOSBox-X, and any other emulator.
 *
 * How it works: each test case writes a few bytes of machine code into a
 * "thunk" buffer ending in RETF, loads the registers and FLAGS from ri[],
 * far calls the thunk, then saves the registers and FLAGS into ro[]. The
 * C code computes the expected state in re[] from a reference model and
 * compares. Undefined flags are masked out. Behavior known to differ between
 * the 8086, 80186, NEC V20/V30 and 286+ is checked against the CPU class
 * detected at startup.
 *
 * 16-bit large memory model only. The 32-bit build is a stub.
 *
 * Results are also written to 8086.TXT in the current directory. The file is
 * committed to disk (fflush + fsync) each time a screen page of results is
 * complete, so that it is readable after a reboot if a later test crashes.
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
			logf = fopen("8086.TXT","a");
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

/* forward reference placeholders */
static unsigned char __far *ph8(void) {
	unsigned char __far *p = tp;
	E(0);
	return p;
}

static unsigned char __far *ph16(void) {
	unsigned char __far *p = tp;
	EW(0);
	return p;
}

static void fix8(unsigned char __far *p) {
	*p = (unsigned char)(here() - (FP_OFF(p) + 1u));
}

static void fix16(unsigned char __far *p) {
	*((unsigned short __far*)p) = here() - (FP_OFF(p) + 2u);
}

static void fixabs16(unsigned char __far *p,unsigned v) {
	*((unsigned short __far*)p) = v;
}

/* relative displacement to an already emitted target */
static void rel16(unsigned target) {
	EW(target - (here() + 2u));
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

static unsigned get8(const unsigned short *r,unsigned i) {
	return (i & 4) ? (r[i&3] >> 8u) : (r[i&3] & 0xFFu);
}

/* segment value such that seg:off addresses p (within 31 bytes) */
static unsigned alias_seg(void __far *p,unsigned off) {
	unsigned long lin = ((unsigned long)FP_SEG(p) << 4ul) + (unsigned long)FP_OFF(p);
	return (unsigned)(((lin + 15ul) >> 4ul) - (unsigned long)(off >> 4u));
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

static void __interrupt __far __loadds int00_h(union INTPACK r) {
	x_common(0,&r);
}

static void __interrupt __far __loadds int01_h(union INTPACK r) {
	unsigned short f = 0;

	__asm {
		pushf
		pop	ax
		mov	f,ax
	}
	if (ss_cnt < 16) ss_ip[ss_cnt] = r.w.ip;
	ss_cnt++;
	x_live |= f;
}

/* INT 6: invalid opcode (80186 and later) */
static void __interrupt __far __loadds int06_h(union INTPACK r) {
	x_common(6,&r);
}

static void __interrupt __far __loadds int03_h(union INTPACK r) {
	x_common(3,&r);
}

static void __interrupt __far __loadds int04_h(union INTPACK r) {
	x_common(4,&r);
}

static void __interrupt __far __loadds int66_h(union INTPACK r) {
	unsigned short f = 0;

	__asm {
		pushf
		pop	ax
		mov	f,ax
	}
	x_live = f;
	x_common(0x66,&r);
	r.w.ax = 0x6666;
	r.w.flags ^= CF;
}

static void (__interrupt __far *old_int00)();
static void (__interrupt __far *old_int01)();
static void (__interrupt __far *old_int03)();
static void (__interrupt __far *old_int04)();
static void (__interrupt __far *old_int06)();
static void (__interrupt __far *old_int66)();

static void hook_vectors(void) {
	old_int00 = _dos_getvect(0x00); _dos_setvect(0x00,int00_h);
	old_int01 = _dos_getvect(0x01); _dos_setvect(0x01,int01_h);
	old_int03 = _dos_getvect(0x03); _dos_setvect(0x03,int03_h);
	old_int04 = _dos_getvect(0x04); _dos_setvect(0x04,int04_h);
	old_int06 = _dos_getvect(0x06); _dos_setvect(0x06,int06_h);
	old_int66 = _dos_getvect(0x66); _dos_setvect(0x66,int66_h);
}

static void unhook_vectors(void) {
	_dos_setvect(0x00,old_int00);
	_dos_setvect(0x01,old_int01);
	_dos_setvect(0x03,old_int03);
	_dos_setvect(0x04,old_int04);
	_dos_setvect(0x06,old_int06);
	_dos_setvect(0x66,old_int66);
}

static void x_clear(void) {
	x_cnt = x_vec = x_ip = x_cs = x_fl = x_live = x_resume = x_faulted = 0;
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

enum { OP_ADD=0, OP_OR, OP_ADC, OP_SBB, OP_AND, OP_SUB, OP_XOR, OP_CMP };
static const char *alu_names[8] = { "ADD","OR","ADC","SBB","AND","SUB","XOR","CMP" };

/* ALU reference. returns result, *fo = flags (non-arith bits from fin), *fm = defined flags */
static unsigned alu_ref(int op,int w,unsigned a,unsigned b,unsigned fin,unsigned *fo,unsigned *fm) {
	unsigned long m = w ? 0xFFFFul : 0xFFul,sg = w ? 0x8000ul : 0x80ul;
	unsigned long la = a & m,lb = b & m,r,bb;
	unsigned f = fin & ~ARITH,cin = fin & CF;

	*fm = ARITH;
	switch (op) {
		case OP_ADD:
		case OP_ADC:
			r = la + lb + (op == OP_ADC ? cin : 0);
			if (r > m) f |= CF;
			if ((~(la ^ lb) & (la ^ r)) & sg) f |= OF;
			if ((la ^ lb ^ r) & 0x10) f |= AF;
			break;
		case OP_SBB:
		case OP_SUB:
		case OP_CMP:
			bb = lb + (op == OP_SBB ? cin : 0);
			r = la - bb;
			if (la < bb) f |= CF;
			if (((la ^ lb) & (la ^ r)) & sg) f |= OF;
			if ((la ^ lb ^ r) & 0x10) f |= AF;
			break;
		default:
			r = (op == OP_OR) ? (la | lb) : (op == OP_AND) ? (la & lb) : (la ^ lb);
			*fm &= ~AF;	/* undefined */
			break;
	}
	r &= m;
	f |= szp(r,w);
	*fo = f;
	return (unsigned)r;
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

/* ---- MOV reg,reg */
static void t_mov_reg(void) {
	unsigned s,d;

	begin("MOV reg");
	for (s=0;s < 8;s++) {
		for (d=0;d < 8;d++) {
			if (s != R_SP && d != R_SP) {
				setup(); EB(2, 0x89,0xC0|(s<<3)|d); go();
				re[d] = ri[s];
				if (!verify(ARITH|CTRL)) failr("89 mov r%u,r%u",d,s);

				setup(); EB(2, 0x8B,0xC0|(d<<3)|s); go();
				re[d] = ri[s];
				if (!verify(ARITH|CTRL)) failr("8B mov r%u,r%u",d,s);
			}

			setup(); EB(2, 0x88,0xC0|(s<<3)|d); go();
			set8(re,d,get8(ri,s));
			if (!verify(ARITH|CTRL)) failr("88 mov r8 %u,%u",d,s);

			setup(); EB(2, 0x8A,0xC0|(d<<3)|s); go();
			set8(re,d,get8(ri,s));
			if (!verify(ARITH|CTRL)) failr("8A mov r8 %u,%u",d,s);
		}
	}
	end();
}

/* ---- MOV reg,imm */
static void t_mov_imm(void) {
	unsigned r,v;

	begin("MOV imm");
	for (r=0;r < 8;r++) {
		v = 0x5A ^ (r * 0x13);
		setup(); EB(2, 0xB0+r,v); go();
		set8(re,r,v);
		if (!verify(ARITH|CTRL)) failr("B%X mov r8,%02X",r,v);

		if (r != R_SP) {
			v = 0x9C37 ^ (r * 0x1111);
			setup(); E(0xB8+r); EW(v); go();
			re[r] = v;
			if (!verify(ARITH|CTRL)) failr("B%X mov r16,%04X",8+r,v);
		}
	}
	end();
}

/* ---- MOV with memory operands */
static void t_mov_mem(void) {
	begin("MOV mem");

	/* mov [bx],ax */
	setup(); memw[0] = 0; memw[1] = 0x1111; ri[R_BX] = memw_off; ri[R_AX] = 0xBEEF;
	EB(2, 0x89,0x07); go();
	if (!verify(ARITH|CTRL) || !checkm(memw[0] == 0xBEEF && memw[1] == 0x1111)) failr("89 07 mov [bx],ax mem=%04X",memw[0]);

	/* mov ax,[bx] */
	setup(); memw[0] = 0xCAFE; ri[R_BX] = memw_off;
	EB(2, 0x8B,0x07); go(); re[R_AX] = 0xCAFE;
	if (!verify(ARITH|CTRL)) failr("8B 07 mov ax,[bx]");

	/* mov [bx],ah */
	setup(); memw[0] = 0x3344; ri[R_BX] = memw_off; ri[R_AX] = 0x9988;
	EB(2, 0x88,0x27); go();
	if (!verify(ARITH|CTRL) || !checkm(memw[0] == 0x3399)) failr("88 27 mov [bx],ah mem=%04X",memw[0]);

	/* mov cl,[bx] */
	setup(); memw[0] = 0x3344; ri[R_BX] = memw_off;
	EB(2, 0x8A,0x0F); go(); set8(re,1,0x44);
	if (!verify(ARITH|CTRL)) failr("8A 0F mov cl,[bx]");

	/* moffs forms */
	setup(); memw[3] = 0x7172;
	E(0xA1); EW(memw_off+6); go(); re[R_AX] = 0x7172;
	if (!verify(ARITH|CTRL)) failr("A1 mov ax,[moffs]");

	setup(); memw[3] = 0x7172;
	E(0xA0); EW(memw_off+7); go(); set8(re,0,0x71);
	if (!verify(ARITH|CTRL)) failr("A0 mov al,[moffs]");

	setup(); memw[3] = 0; ri[R_AX] = 0x8182;
	E(0xA3); EW(memw_off+6); go();
	if (!verify(ARITH|CTRL) || !checkm(memw[3] == 0x8182)) failr("A3 mov [moffs],ax mem=%04X",memw[3]);

	setup(); memw[3] = 0x5555; ri[R_AX] = 0x8182;
	E(0xA2); EW(memw_off+6); go();
	if (!verify(ARITH|CTRL) || !checkm(memw[3] == 0x5582)) failr("A2 mov [moffs],al mem=%04X",memw[3]);

	/* mov [bx],imm */
	setup(); memw[0] = 0x1234; ri[R_BX] = memw_off;
	EB(3, 0xC6,0x07,0xA5); go();
	if (!verify(ARITH|CTRL) || !checkm(memw[0] == 0x12A5)) failr("C6 07 mov byte [bx],imm mem=%04X",memw[0]);

	setup(); memw[0] = 0x1234; ri[R_BX] = memw_off;
	EB(2, 0xC7,0x07); EW(0x6789); go();
	if (!verify(ARITH|CTRL) || !checkm(memw[0] == 0x6789)) failr("C7 07 mov word [bx],imm mem=%04X",memw[0]);

	/* displacements */
	setup(); memw[1] = 0; ri[R_BX] = memw_off; ri[R_AX] = 0x4242;
	EB(3, 0x89,0x47,0x02); go();
	if (!verify(ARITH|CTRL) || !checkm(memw[1] == 0x4242)) failr("89 47 02 mov [bx+2],ax mem=%04X",memw[1]);

	setup(); memw[2] = 0; ri[R_BX] = memw_off - 0x1000; ri[R_AX] = 0x4343;
	EB(2, 0x89,0x87); EW(0x1004); go();
	if (!verify(ARITH|CTRL) || !checkm(memw[2] == 0x4343)) failr("89 87 mov [bx+1004h],ax mem=%04X",memw[2]);

	setup(); memw[2] = 0x2468; ri[R_BX] = memw_off + 4; ri[R_SI] = 2;
	EB(3, 0x8B,0x40,0xFE); go(); re[R_AX] = 0x2468;
	if (!verify(ARITH|CTRL)) failr("8B 40 FE mov ax,[bx+si-2]");

	end();
}

/* ---- MOV / PUSH / POP segment registers */
static void t_mov_sreg(void) {
	begin("MOV sreg");

	setup(); ri[R_AX] = 0x1234; EB(2, 0x8E,0xC0); go(); re[R_ES] = 0x1234;
	if (!verify(ARITH|CTRL)) failr("8E C0 mov es,ax");

	setup(); EB(2, 0x8C,0xC8); go(); re[R_AX] = thunk_seg;
	if (!verify(ARITH|CTRL)) failr("8C C8 mov ax,cs");

	setup(); EB(2, 0x8C,0xD3); go(); re[R_BX] = ss_val;
	if (!verify(ARITH|CTRL)) failr("8C D3 mov bx,ss");

	setup(); EB(2, 0x8C,0xD9); go(); re[R_CX] = dgroup;
	if (!verify(ARITH|CTRL)) failr("8C D9 mov cx,ds");

	setup(); ri[R_ES] = 0x4321; EB(2, 0x8C,0xC2); go(); re[R_DX] = 0x4321;
	if (!verify(ARITH|CTRL)) failr("8C C2 mov dx,es");

	setup(); memw[0] = 0x2222; ri[R_BX] = memw_off; EB(2, 0x8E,0x07); go(); re[R_ES] = 0x2222;
	if (!verify(ARITH|CTRL)) failr("8E 07 mov es,[bx]");

	setup(); memw[0] = 0; ri[R_BX] = memw_off; EB(2, 0x8C,0x1F); go();
	if (!verify(ARITH|CTRL) || !checkm(memw[0] == dgroup)) failr("8C 1F mov [bx],ds mem=%04X",memw[0]);

	setup(); EB(4, 0x8C,0xD2,0x8E,0xD2); go(); re[R_DX] = ss_val;
	if (!verify(ARITH|CTRL)) failr("8C D2 8E D2 mov dx,ss; mov ss,dx");

	setup(); EB(2, 0x0E,0x07); go(); re[R_ES] = thunk_seg;
	if (!verify(ARITH|CTRL)) failr("0E 07 push cs; pop es");

	setup(); EB(2, 0x16,0x07); go(); re[R_ES] = ss_val;
	if (!verify(ARITH|CTRL)) failr("16 07 push ss; pop es");

	setup(); ri[R_ES] = 0x1111; EB(2, 0x1E,0x07); go(); re[R_ES] = dgroup;
	if (!verify(ARITH|CTRL)) failr("1E 07 push ds; pop es");

	setup(); EB(2, 0x16,0x17); go();
	if (!verify(ARITH|CTRL)) failr("16 17 push ss; pop ss");

	/* push ds; push es; pop ds; mov ax,[bx]; pop ds */
	{
		unsigned T = memw_off + 0x40;
		unsigned alt = alias_seg(xbuf,T);

		*((unsigned short __far*)MK_FP(alt,T)) = 0x7E57;
		memw[0x20] = 0x1DEA;
		setup(); ri[R_ES] = alt; ri[R_BX] = T;
		EB(6, 0x1E,0x06,0x1F,0x8B,0x07,0x1F); go(); re[R_AX] = 0x7E57;
		if (!verify(ARITH|CTRL)) failr("06 1F pop ds then mov ax,[bx]");
	}

	end();
}

/* ---- LEA with every 16-bit ModR/M memory form */
static const unsigned short ea_sets[3][4] = {	/* BX, SI, DI, BP */
	{ 0x1234,0x0101,0x2020,0x4321 },
	{ 0xFFF0,0x0020,0x8000,0x7FFF },
	{ 0x0000,0x0000,0x0000,0x0000 }
};

static unsigned ea_calc(unsigned mod,unsigned rm,unsigned bx,unsigned si,unsigned di,unsigned bp,unsigned disp) {
	unsigned ea = 0;

	switch (rm) {
		case 0: ea = bx + si; break;
		case 1: ea = bx + di; break;
		case 2: ea = bp + si; break;
		case 3: ea = bp + di; break;
		case 4: ea = si; break;
		case 5: ea = di; break;
		case 6: ea = (mod == 0) ? 0 : bp; break;
		case 7: ea = bx; break;
	}
	return ea + disp;
}

static void t_lea(void) {
	static const unsigned short d8[4] = { 0x00,0x7F,0x80,0xFF };
	static const unsigned short d16[4] = { 0x0000,0x1234,0x8000,0xFFFF };
	unsigned s,mod,rm,di,disp;

	begin("LEA/EA");
	for (s=0;s < 3;s++) {
		for (mod=0;mod < 3;mod++) {
			for (rm=0;rm < 8;rm++) {
				for (di=0;di < 4;di++) {
					setup();
					ri[R_BX] = ea_sets[s][0]; ri[R_SI] = ea_sets[s][1];
					ri[R_DI] = ea_sets[s][2]; ri[R_BP] = ea_sets[s][3];
					EB(2, 0x8D,(mod<<6)|(R_DX<<3)|rm);
					if (mod == 1) { disp = d8[di]; E(disp); disp = sext8(disp); }
					else if (mod == 2 || (mod == 0 && rm == 6)) { disp = d16[di]; EW(disp); }
					else disp = 0;
					go();
					re[R_DX] = ea_calc(mod,rm,ri[R_BX],ri[R_SI],ri[R_DI],ri[R_BP],disp);
					if (!verify(ARITH|CTRL)) failr("8D lea dx mod=%u rm=%u disp=%04X",mod,rm,disp);
				}
			}
		}
	}
	end();
}

/* ---- default segment of each ModR/M form (BP based = SS, others DS) */
static void t_seg_default(void) {
	unsigned T = memw_off + 0x40;
	unsigned alt = alias_seg(xbuf,T);
	unsigned mod,rm,disp,ss_word,uses_ss;
	unsigned short __far *ssp = (unsigned short __far*)MK_FP(ss_val,T);
	unsigned short __far *altp = (unsigned short __far*)MK_FP(alt,T);

	begin("Seg default");
	ss_word = *ssp;
	*altp = ss_word ^ 0x5AA5;
	for (mod=0;mod < 3;mod++) {
		for (rm=0;rm < 8;rm++) {
			setup();
			ri[R_CX] = alt;
			disp = (mod == 1) ? 0xFFF0 : (mod == 2) ? 0x1111 : (rm == 6) ? T : 0;
			/* index register 0x22, base register gets the rest */
			switch (rm) {
				case 0: ri[R_SI] = 0x22; ri[R_BX] = T - disp - 0x22; break;
				case 1: ri[R_DI] = 0x22; ri[R_BX] = T - disp - 0x22; break;
				case 2: ri[R_SI] = 0x22; ri[R_BP] = T - disp - 0x22; break;
				case 3: ri[R_DI] = 0x22; ri[R_BP] = T - disp - 0x22; break;
				case 4: ri[R_SI] = T - disp; break;
				case 5: ri[R_DI] = T - disp; break;
				case 6: ri[R_BP] = T - disp; break;
				case 7: ri[R_BX] = T - disp; break;
			}
			EB(3, 0x1E,0x8E,0xD9);				/* push ds; mov ds,cx */
			EB(2, 0x8B,(mod<<6)|rm);			/* mov ax,[ea] */
			if (mod == 1) E(disp & 0xFF);
			else if (mod == 2 || (mod == 0 && rm == 6)) EW(disp);
			E(0x1F);					/* pop ds */
			go();
			uses_ss = (rm == 2 || rm == 3 || (rm == 6 && mod != 0)) ? 1 : 0;
			re[R_AX] = uses_ss ? ss_word : (ss_word ^ 0x5AA5);
			if (!verify(ARITH|CTRL)) failr("mov ax,[ea] mod=%u rm=%u should use %s",mod,rm,uses_ss ? "SS" : "DS");
		}
	}
	end();
}

/* ---- segment override prefixes */
static void t_seg_prefix(void) {
	unsigned T = memw_off + 0x40;
	unsigned alt = alias_seg(xbuf,T);
	unsigned short __far *altp = (unsigned short __far*)MK_FP(alt,T);
	unsigned short __far *ssp = (unsigned short __far*)MK_FP(ss_val,T);
	unsigned cso = FP_OFF(thunk) + THUNK_DATA;

	begin("Seg prefix");
	memw[0x20] = 0xD5D5;
	*altp = 0xE5E5;
	*((unsigned short __far*)(thunk + THUNK_DATA)) = 0xC5C5;

	setup(); ri[R_ES] = alt; ri[R_BX] = T; EB(3, 0x26,0x8B,0x07); go(); re[R_AX] = 0xE5E5;
	if (!verify(ARITH|CTRL)) failr("26 8B 07 mov ax,es:[bx]");

	setup(); ri[R_BX] = cso; EB(3, 0x2E,0x8B,0x07); go(); re[R_AX] = 0xC5C5;
	if (!verify(ARITH|CTRL)) failr("2E 8B 07 mov ax,cs:[bx]");

	setup(); ri[R_BX] = T; EB(3, 0x36,0x8B,0x07); go(); re[R_AX] = *ssp;
	if (!verify(ARITH|CTRL)) failr("36 8B 07 mov ax,ss:[bx]");

	setup(); ri[R_BP] = T; EB(4, 0x3E,0x8B,0x46,0x00); go(); re[R_AX] = 0xD5D5;
	if (!verify(ARITH|CTRL)) failr("3E 8B 46 00 mov ax,ds:[bp+0]");

	setup(); ri[R_ES] = alt; ri[R_BP] = T; EB(4, 0x26,0x8B,0x46,0x00); go(); re[R_AX] = 0xE5E5;
	if (!verify(ARITH|CTRL)) failr("26 8B 46 00 mov ax,es:[bp+0]");

	setup(); ri[R_ES] = alt; ri[R_BX] = T; ri[R_AX] = 0x7A7B; EB(3, 0x26,0x89,0x07); go();
	if (!verify(ARITH|CTRL) || !checkm(*altp == 0x7A7B && memw[0x20] == 0xD5D5)) failr("26 89 07 mov es:[bx],ax");

	setup(); ri[R_ES] = alt; ri[R_BX] = T; EB(3, 0x26,0x8A,0x27); go(); set8(re,4,0x7B);
	if (!verify(ARITH|CTRL)) failr("26 8A 27 mov ah,es:[bx]");

	end();
}

/* ---- word access at offset FFFFh wraps to offset 0000h (8086/80186; 286+ raise #GP, see 286.c).
 * Runs with DS = a 64KB+16 block of our own, so that offset 0000h, offset FFFFh and the byte
 * just past the segment (linear base+10000h, where a non-wrapping emulator would go) are all ours */
static void t_seg_wrap(void) {
	unsigned seg;
	unsigned char __far *p0,__far *pf,__far *pn;

	begin("Seg wrap");
	if (is286p) { skip("286+ raise #GP, tested by 286.EXE"); end(); return; }
	if (_dos_allocmem(0x1001,&seg) != 0) { skip("cannot allocate 64KB"); end(); return; }
	p0 = (unsigned char __far*)MK_FP(seg,0);
	pf = (unsigned char __far*)MK_FP(seg,0xFFFF);
	pn = (unsigned char __far*)MK_FP(seg+0x1000,0);
	*p0 = 0x11; *pf = 0x22; *pn = 0x33;

	/* push ds; mov ds,cx; mov ax,[bx]; pop ds */
	setup(); ri[R_CX] = seg; ri[R_BX] = 0xFFFF; EB(6, 0x1E,0x8E,0xD9,0x8B,0x07,0x1F); go(); re[R_AX] = 0x1122;
	if (!verify(ARITH|CTRL)) failr("mov ax,[FFFF] should wrap to offset 0 (past segment would be 3322h)");

	/* mov ax,[bx+si] with bx+si = 1FFFFh */
	setup(); ri[R_CX] = seg; ri[R_BX] = 0xFFF0; ri[R_SI] = 0x000F; EB(6, 0x1E,0x8E,0xD9,0x8B,0x00,0x1F); go(); re[R_AX] = 0x1122;
	if (!verify(ARITH|CTRL)) failr("mov ax,[bx+si] = [FFFF] should wrap to offset 0");

	/* mov [bx],ax */
	setup(); ri[R_CX] = seg; ri[R_BX] = 0xFFFF; ri[R_AX] = 0xA5B6; EB(6, 0x1E,0x8E,0xD9,0x89,0x07,0x1F); go();
	if (!verify(ARITH|CTRL) || !checkm(*pf == 0xB6 && *p0 == 0xA5 && *pn == 0x33))
		failr("mov [FFFF],ax: [FFFF]=%02X [0]=%02X [past]=%02X, expected B6 A5 33",*pf,*p0,*pn);

	/* movsw */
	*p0 = 0x11; *pf = 0x22; *pn = 0x33;
	setup(); ri[R_CX] = seg; ri[R_SI] = 0xFFFF; ri[R_ES] = FP_SEG(xbuf); ri[R_DI] = FP_OFF(xbuf); xbuf[0] = xbuf[1] = 0xEE;
	EB(5, 0x1E,0x8E,0xD9,0xA5,0x1F); go(); re[R_SI] = 0x0001; re[R_DI] = FP_OFF(xbuf) + 2;
	if (!verify(ARITH|CTRL) || !checkm(xbuf[0] == 0x22 && xbuf[1] == 0x11)) failr("movsw from [FFFF]: got %02X %02X, expected 22 11",xbuf[0],xbuf[1]);

	_dos_freemem(seg);
	end();
}

/* ---- XCHG */
static void t_xchg(void) {
	unsigned r;

	begin("XCHG");
	for (r=0;r < 8;r++) {
		if (r == R_SP) continue;
		setup(); E(0x90+r); go();
		re[R_AX] = ri[r]; re[r] = ri[R_AX];
		if (!verify(ARITH|CTRL)) failr("%02X xchg ax,r%u",0x90+r,r);
	}

	setup(); EB(2, 0x87,0xD9); go(); re[R_BX] = ri[R_CX]; re[R_CX] = ri[R_BX];
	if (!verify(ARITH|CTRL)) failr("87 D9 xchg bx,cx");

	setup(); EB(2, 0x86,0xE0); go(); re[R_AX] = (ri[R_AX] >> 8) | (ri[R_AX] << 8);
	if (!verify(ARITH|CTRL)) failr("86 E0 xchg ah,al");

	setup(); EB(2, 0x86,0xCA); go(); set8(re,1,get8(ri,2)); set8(re,2,get8(ri,1));
	if (!verify(ARITH|CTRL)) failr("86 CA xchg cl,dl");

	setup(); memw[0] = 0x0BAD; ri[R_BX] = memw_off; EB(2, 0x87,0x07); go(); re[R_AX] = 0x0BAD;
	if (!verify(ARITH|CTRL) || !checkm(memw[0] == ri[R_AX])) failr("87 07 xchg ax,[bx] mem=%04X",memw[0]);

	setup(); memw[0] = 0x0BAD; ri[R_BX] = memw_off; EB(2, 0x86,0x27); go(); set8(re,4,0xAD);
	if (!verify(ARITH|CTRL) || !checkm(memw[0] == (0x0B00 | (ri[R_AX] >> 8)))) failr("86 27 xchg ah,[bx] mem=%04X",memw[0]);

	end();
}

/* ---- XLAT */
static void t_xlat(void) {
	unsigned i,T = FP_OFF(tbl),alt = alias_seg(xbuf,T);
	unsigned char __far *altp = (unsigned char __far*)MK_FP(alt,T);

	begin("XLAT");
	for (i=0;i < 256;i++) {
		tbl[i] = (unsigned char)(i * 7 + 3);
		altp[i] = (unsigned char)~tbl[i];
	}
	for (i=0;i < 256;i++) {
		setup(); ri[R_BX] = T; ri[R_AX] = 0x7700 | i; E(0xD7); go();
		re[R_AX] = 0x7700 | tbl[i];
		if (!verify(ARITH|CTRL)) failr("D7 xlat al=%02X",i);
	}
	for (i=0;i < 256;i += 15) {
		setup(); ri[R_BX] = T; ri[R_ES] = alt; ri[R_AX] = 0x7700 | i; EB(2, 0x26,0xD7); go();
		re[R_AX] = 0x7700 | altp[i];
		if (!verify(ARITH|CTRL)) failr("26 D7 xlat es: al=%02X",i);
	}
	end();
}

/* ---- LDS/LES */
static void t_ldsles(void) {
	begin("LDS/LES");
	memw[0] = 0x1357; memw[1] = 0x2468; memw[2] = 0x9ABC; memw[3] = 0x3579;

	setup(); ri[R_BX] = memw_off; EB(6, 0x1E,0xC5,0x37,0x8C,0xDA,0x1F); go();
	re[R_SI] = 0x1357; re[R_DX] = 0x2468;
	if (!verify(ARITH|CTRL)) failr("C5 37 lds si,[bx]");

	setup(); ri[R_BX] = memw_off; EB(2, 0xC4,0x3F); go();
	re[R_DI] = 0x1357; re[R_ES] = 0x2468;
	if (!verify(ARITH|CTRL)) failr("C4 3F les di,[bx]");

	setup(); ri[R_BX] = memw_off; EB(3, 0xC4,0x57,0x04); go();
	re[R_DX] = 0x9ABC; re[R_ES] = 0x3579;
	if (!verify(ARITH|CTRL)) failr("C4 57 04 les dx,[bx+4]");

	end();
}

/* ---- PUSH/POP */
static void t_pushpop(void) {
	unsigned s,d;

	begin("PUSH/POP");
	for (s=0;s < 8;s++) {
		for (d=0;d < 8;d++) {
			if (s == R_SP || d == R_SP) continue;
			setup(); EB(2, 0x50+s,0x58+d); go(); re[d] = ri[s];
			if (!verify(ARITH|CTRL)) failr("push r%u; pop r%u",s,d);
		}
	}

	/* push ax; mov bp,sp; mov dx,[bp+0]; pop cx */
	setup(); EB(7, 0x50,0x8B,0xEC,0x8B,0x56,0x00,0x59); go();
	re[R_DX] = re[R_CX] = ri[R_AX]; re[R_BP] = sp_before - 6;	/* far return address + pushed word */
	if (!verify(ARITH|CTRL)) failr("push ax; stack contents");

	setup(); memw[0] = 0x6543; ri[R_BX] = memw_off; EB(3, 0xFF,0x37,0x58); go(); re[R_AX] = 0x6543;
	if (!verify(ARITH|CTRL)) failr("FF 37 push [bx]");

	setup(); memw[0] = 0; ri[R_BX] = memw_off; EB(3, 0x50,0x8F,0x07); go();
	if (!verify(ARITH|CTRL) || !checkm(memw[0] == ri[R_AX])) failr("8F 07 pop [bx] mem=%04X",memw[0]);

	setup(); EB(3, 0xFF,0xF1,0x5A); go(); re[R_DX] = ri[R_CX];
	if (!verify(ARITH|CTRL)) failr("FF F1 push cx (r/m form)");

	setup(); EB(3, 0x51,0x8F,0xC2); go(); re[R_DX] = ri[R_CX];
	if (!verify(ARITH|CTRL)) failr("8F C2 pop dx (r/m form)");

	end();
}

/* ---- PUSH SP: 8086/80186 push the decremented SP, 286+ push the original */
static void t_push_sp(void) {
	unsigned dx;

	begin("PUSH SP");
	setup(); EB(4, 0x8B,0xD4,0x54,0x58); go();
	dx = ro[R_DX];
	if (checkm(ro[R_AX] == dx - 2u)) {
		info("PUSH SP pushes SP-2 (8086 behavior)");
		if (is286p) failm("286+ should push the original SP");
	}
	else if (ro[R_AX] == dx) {
		info("PUSH SP pushes original SP (286+ behavior)");
		if (c_flagsF) failm("8086/186 class should push SP-2");
	}
	else {
		failm("PUSH SP pushed %04X, SP was %04X",ro[R_AX],dx);
	}
	end();
}

/* ---- PUSHF/POPF */
static void t_pushf(void) {
	static const unsigned short pat[6] = { 0x0202,0x0ED7,0x0A83,0x06C6,0x0213,0x0E55 };
	unsigned i,hi;

	begin("PUSHF/POPF");
	for (i=0;i < 6;i++) {
		setup(); E(0xB8); EW(pat[i]); EB(4, 0x50,0x9D,0x9C,0x58); go();
		re[R_AX] = ro[R_AX]; re[R_FL] = pat[i];
		if (!verify(ARITH|CTRL) || !checkm(((ro[R_AX] ^ pat[i]) & 0x0FD5) == 0) || !checkm((ro[R_AX] & 0x002A) == 0x0002))
			failr("popf %04X; pushf -> %04X",pat[i],ro[R_AX]);
	}

	/* upper 4 bits */
	setup(); E(0xB8); EW(0x0202); EB(4, 0x50,0x9D,0x9C,0x58); go();
	hi = ro[R_AX] & 0xF000;
	setup(); E(0xB8); EW(0xF202); EB(4, 0x50,0x9D,0x9C,0x58); go();
	info("FLAGS[15:12] after POPF 0000h=%X F000h=%X",hi >> 12,(ro[R_AX] & 0xF000) >> 12);
	if (c_flagsF && !checkm(hi == 0xF000 && (ro[R_AX] & 0xF000) == 0xF000)) failm("8086 class: bits 12-15 should read 1");
	if (is286p && !c_386 && !checkm(hi == 0 && (ro[R_AX] & 0xF000) == 0)) failm("286 real mode: bits 12-15 should read 0");
	if (c_386 && !checkm((ro[R_AX] & 0x8000) == 0)) failm("386+: bit 15 should read 0");
	end();
}

/* ---- LAHF/SAHF */
static void t_lahf(void) {
	unsigned i,f;

	begin("LAHF/SAHF");
	for (i=0;i < 256;i++) {
		setup(); ri[R_AX] = (i << 8) | 0x33; ri[R_FL] = 0x0202 | ((i & 1) ? OF : 0); E(0x9E); go();
		re[R_FL] = (ri[R_FL] & 0xFF00) | (i & 0xD5) | 2;
		if (!verify(ARITH|CTRL)) failr("9E sahf ah=%02X",i);
	}
	for (i=0;i < 32;i++) {
		f = ((i & 1) ? CF : 0) | ((i & 2) ? PF : 0) | ((i & 4) ? AF : 0) | ((i & 8) ? ZF : 0) | ((i & 16) ? SF : 0);
		setup(); ri[R_FL] = 0x0202 | f | ((i & 1) ? OF : 0); E(0x9F); go();
		re[R_AX] = (ri[R_AX] & 0xFF) | ((f | 2) << 8);
		if (!verify(ARITH|CTRL)) failr("9F lahf flags=%04X",ri[R_FL]);
	}
	end();
}

/* ---- CBW/CWD */
static void t_cbw(void) {
	unsigned i;

	begin("CBW/CWD");
	for (i=0;i < 16;i++) {
		setup(); ri[R_AX] = 0x5A00 | v8[i]; E(0x98); go(); re[R_AX] = sext8(v8[i]);
		if (!verify(ARITH|CTRL)) failr("98 cbw al=%02X",v8[i]);

		setup(); ri[R_AX] = v16[i]; E(0x99); go(); re[R_DX] = (v16[i] & 0x8000) ? 0xFFFF : 0x0000;
		if (!verify(ARITH|CTRL)) failr("99 cwd ax=%04X",v16[i]);
	}
	end();
}

/* ---- ADD/OR/ADC/SBB/AND/SUB/XOR/CMP, all encodings */
enum {
	AF_RM8_R8=0, AF_RM16_R16, AF_R8_RM8, AF_R16_RM16, AF_AL_I8, AF_AX_I16,
	AF_80, AF_81, AF_83, AF_M8_R8, AF_M16_R16, AF_R16_M16, AF_83M, AF_R8_M8,
	AF_MAX
};

static const char *alu_form_names[AF_MAX] = {
	"r/m8,r8","r/m16,r16","r8,r/m8","r16,r/m16","AL,imm8","AX,imm16",
	"80 r8,imm8","81 r16,imm16","83 r16,simm8","m8,r8","m16,r16","r16,m16","83 m16,simm8","r8,m8"
};

static void alu_case(unsigned op,unsigned form,unsigned a,unsigned b,unsigned fin) {
	unsigned w,res,fo,fm,mem_exp = 0,ok;
	int memtest = 0;

	setup();
	ri[R_FL] = fin;
	switch (form) {
		case AF_RM8_R8:   w=0; ri[R_AX] = 0x5A00|a; ri[R_BX] = 0xC300|b; EB(2, op*8+0,0xD8); break;
		case AF_RM16_R16: w=1; ri[R_AX] = a; ri[R_BX] = b; EB(2, op*8+1,0xD8); break;
		case AF_R8_RM8:   w=0; ri[R_AX] = 0x5A00|a; ri[R_BX] = 0xC300|b; EB(2, op*8+2,0xC3); break;
		case AF_R16_RM16: w=1; ri[R_AX] = a; ri[R_BX] = b; EB(2, op*8+3,0xC3); break;
		case AF_AL_I8:    w=0; ri[R_AX] = 0x5A00|a; EB(2, op*8+4,b); break;
		case AF_AX_I16:   w=1; ri[R_AX] = a; E(op*8+5); EW(b); break;
		case AF_80:       w=0; ri[R_CX] = 0x7700|a; EB(3, 0x80,0xC1|(op<<3),b); break;
		case AF_81:       w=1; ri[R_DX] = a; EB(2, 0x81,0xC2|(op<<3)); EW(b); break;
		case AF_83:       w=1; ri[R_SI] = a; EB(3, 0x83,0xC6|(op<<3),b); b = sext8(b); break;
		case AF_M8_R8:    w=0; memw[0] = 0x9900|a; ri[R_BX] = memw_off; ri[R_AX] = 0x6600|b; EB(2, op*8+0,0x07); memtest=1; break;
		case AF_M16_R16:  w=1; memw[0] = a; ri[R_BX] = memw_off; ri[R_AX] = b; EB(2, op*8+1,0x07); memtest=1; break;
		case AF_R16_M16:  w=1; memw[0] = b; ri[R_BX] = memw_off; ri[R_AX] = a; EB(2, op*8+3,0x07); break;
		case AF_83M:      w=1; memw[0] = a; ri[R_BX] = memw_off; EB(3, 0x83,0x07|(op<<3),b); b = sext8(b); memtest=1; break;
		case AF_R8_M8:    w=0; memw[0] = 0x9900|b; ri[R_BX] = memw_off; ri[R_AX] = 0x5A00|a; EB(2, op*8+2,0x07); break;
		default: return;
	}
	go();

	res = alu_ref(op,w,a,b,fin,&fo,&fm);
	re[R_FL] = fo;
	if (op != OP_CMP) {
		switch (form) {
			case AF_RM8_R8: case AF_R8_RM8: case AF_AL_I8: case AF_R8_M8:
				set8(re,0,res); break;
			case AF_RM16_R16: case AF_R16_RM16: case AF_AX_I16: case AF_R16_M16:
				re[R_AX] = res; break;
			case AF_80: set8(re,1,res); break;
			case AF_81: re[R_DX] = res; break;
			case AF_83: re[R_SI] = res; break;
			case AF_M8_R8: mem_exp = 0x9900|res; break;
			case AF_M16_R16: case AF_83M: mem_exp = res; break;
		}
	}
	else if (memtest) {
		mem_exp = (form == AF_M8_R8) ? (0x9900|a) : a;
	}

	ok = verify(fm|CTRL);
	if (memtest && !checkm(memw[0] == mem_exp)) {
		ok = 0;
		sprintf(diffbuf+strlen(diffbuf)," mem=%04X(%04X)",memw[0],mem_exp);
	}
	if (!ok) failr("%s %s a=%04X b=%04X",alu_names[op],alu_form_names[form],a,b);
}

static void t_alu(unsigned op) {
	unsigned form,i,j,a,b,wide;

	begin(alu_names[op]);
	for (form=0;form < AF_MAX;form++) {
		wide = (form == AF_RM16_R16 || form == AF_R16_RM16 || form == AF_AX_I16 || form == AF_81 ||
			form == AF_83 || form == AF_M16_R16 || form == AF_R16_M16 || form == AF_83M);
		for (i=0;i < 16;i++) {
			for (j=0;j < 16;j++) {
				a = wide ? v16[i] : v8[i];
				b = (wide && form != AF_83 && form != AF_83M) ? v16[j] : v8[j];
				if (op == OP_ADC || op == OP_SBB) {
					alu_case(op,form,a,b,0x0202);
					alu_case(op,form,a,b,0x0202|ARITH);
				}
				else {
					alu_case(op,form,a,b,((i ^ j) & 1) ? (0x0202|ARITH) : 0x0202);
				}
			}
		}
	}
	end();
}

/* ---- opcode 82h, an alias of 80h */
static void t_alu82(void) {
	unsigned op,i,j,res,fo,fm,fin;

	begin("ALU 82h");
	for (op=0;op < 8;op++) {
		for (i=0;i < 16;i += 3) {
			for (j=0;j < 16;j += 2) {
				fin = (j & 2) ? (0x0202|ARITH) : 0x0202;
				setup(); ri[R_CX] = 0x7700 | v8[i]; ri[R_FL] = fin;
				EB(3, 0x82,0xC1|(op<<3),v8[j]); go();
				res = alu_ref(op,0,v8[i],v8[j],fin,&fo,&fm);
				if (op != OP_CMP) set8(re,1,res);
				re[R_FL] = fo;
				if (!verify(fm|CTRL)) failr("82 %s cl,%02X cl=%02X",alu_names[op],v8[j],v8[i]);
			}
		}
	}
	end();
}

/* ---- TEST */
static void t_test(void) {
	unsigned i,j,fo,fm,fin;

	begin("TEST");
	for (i=0;i < 16;i++) {
		for (j=0;j < 16;j++) {
			fin = ((i ^ j) & 1) ? (0x0202|ARITH) : 0x0202;

			setup(); ri[R_FL] = fin; ri[R_AX] = 0x5A00|v8[i]; ri[R_BX] = 0xC300|v8[j]; EB(2, 0x84,0xD8); go();
			alu_ref(OP_AND,0,v8[i],v8[j],fin,&fo,&fm); re[R_FL] = fo;
			if (!verify(fm|CTRL)) failr("84 test al,bl %02X,%02X",v8[i],v8[j]);

			setup(); ri[R_FL] = fin; ri[R_AX] = v16[i]; ri[R_BX] = v16[j]; EB(2, 0x85,0xD8); go();
			alu_ref(OP_AND,1,v16[i],v16[j],fin,&fo,&fm); re[R_FL] = fo;
			if (!verify(fm|CTRL)) failr("85 test ax,bx %04X,%04X",v16[i],v16[j]);

			setup(); ri[R_FL] = fin; ri[R_AX] = 0x5A00|v8[i]; EB(2, 0xA8,v8[j]); go();
			alu_ref(OP_AND,0,v8[i],v8[j],fin,&fo,&fm); re[R_FL] = fo;
			if (!verify(fm|CTRL)) failr("A8 test al,imm %02X,%02X",v8[i],v8[j]);

			setup(); ri[R_FL] = fin; ri[R_AX] = v16[i]; E(0xA9); EW(v16[j]); go();
			alu_ref(OP_AND,1,v16[i],v16[j],fin,&fo,&fm); re[R_FL] = fo;
			if (!verify(fm|CTRL)) failr("A9 test ax,imm %04X,%04X",v16[i],v16[j]);

			setup(); ri[R_FL] = fin; ri[R_CX] = 0x7700|v8[i]; EB(3, 0xF6,0xC1,v8[j]); go();
			alu_ref(OP_AND,0,v8[i],v8[j],fin,&fo,&fm); re[R_FL] = fo;
			if (!verify(fm|CTRL)) failr("F6 /0 test cl,imm %02X,%02X",v8[i],v8[j]);

			setup(); ri[R_FL] = fin; ri[R_DX] = v16[i]; EB(2, 0xF7,0xC2); EW(v16[j]); go();
			alu_ref(OP_AND,1,v16[i],v16[j],fin,&fo,&fm); re[R_FL] = fo;
			if (!verify(fm|CTRL)) failr("F7 /0 test dx,imm %04X,%04X",v16[i],v16[j]);

			setup(); ri[R_FL] = fin; memw[0] = v16[i]; ri[R_BX] = memw_off; ri[R_AX] = v16[j]; EB(2, 0x85,0x07); go();
			alu_ref(OP_AND,1,v16[i],v16[j],fin,&fo,&fm); re[R_FL] = fo;
			if (!verify(fm|CTRL) || !checkm(memw[0] == v16[i])) failr("85 07 test [bx],ax %04X,%04X",v16[i],v16[j]);
		}
	}
	end();
}

/* INC/DEC flags: like ADD/SUB 1 but CF preserved */
static unsigned incdec_ref(int dec,int w,unsigned a,unsigned fin,unsigned *fo) {
	unsigned fm,r;

	r = alu_ref(dec ? OP_SUB : OP_ADD,w,a,1,fin,fo,&fm);
	*fo = (*fo & ~CF) | (fin & CF);
	return r;
}

/* ---- INC/DEC r16 (40h-4Fh) */
static void t_incdec16(void) {
	unsigned r,i,dec,fin,fo,res;

	begin("INC/DEC r16");
	for (dec=0;dec < 2;dec++) {
		for (r=0;r < 8;r++) {
			if (r == R_SP) continue;
			for (i=0;i < 16;i++) {
				fin = (i & 1) ? (0x0202|ARITH) : 0x0202;
				setup(); ri[R_FL] = fin; ri[r] = v16[i]; E((dec ? 0x48 : 0x40) + r); go();
				res = incdec_ref(dec,1,v16[i],fin,&fo);
				re[r] = res; re[R_FL] = fo;
				if (!verify(ARITH|CTRL)) failr("%s r%u %04X",dec ? "dec" : "inc",r,v16[i]);
			}
		}
	}
	end();
}

/* ---- INC/DEC r/m (FEh/FFh /0 /1) */
static void t_incdec_rm(void) {
	unsigned i,dec,fin,fo,res,ok;

	begin("INC/DEC r/m");
	for (dec=0;dec < 2;dec++) {
		for (i=0;i < 16;i++) {
			fin = (i & 1) ? (0x0202|ARITH) : 0x0202;

			setup(); ri[R_FL] = fin; ri[R_CX] = 0x7700|v8[i]; EB(2, 0xFE,dec ? 0xC9 : 0xC1); go();
			res = incdec_ref(dec,0,v8[i],fin,&fo); set8(re,1,res); re[R_FL] = fo;
			if (!verify(ARITH|CTRL)) failr("FE %s cl %02X",dec ? "dec" : "inc",v8[i]);

			setup(); ri[R_FL] = fin; ri[R_DX] = v16[i]; EB(2, 0xFF,dec ? 0xCA : 0xC2); go();
			res = incdec_ref(dec,1,v16[i],fin,&fo); re[R_DX] = res; re[R_FL] = fo;
			if (!verify(ARITH|CTRL)) failr("FF %s dx %04X",dec ? "dec" : "inc",v16[i]);

			setup(); ri[R_FL] = fin; memw[0] = 0x9900|v8[i]; ri[R_BX] = memw_off; EB(2, 0xFE,dec ? 0x0F : 0x07); go();
			res = incdec_ref(dec,0,v8[i],fin,&fo); re[R_FL] = fo;
			ok = verify(ARITH|CTRL);
			if (!checkm(memw[0] == (0x9900|res))) ok = 0;
			if (!ok) failr("FE %s byte [bx] %02X mem=%04X",dec ? "dec" : "inc",v8[i],memw[0]);

			setup(); ri[R_FL] = fin; memw[0] = v16[i]; ri[R_BX] = memw_off; EB(2, 0xFF,dec ? 0x0F : 0x07); go();
			res = incdec_ref(dec,1,v16[i],fin,&fo); re[R_FL] = fo;
			ok = verify(ARITH|CTRL);
			if (!checkm(memw[0] == res)) ok = 0;
			if (!ok) failr("FF %s word [bx] %04X mem=%04X",dec ? "dec" : "inc",v16[i],memw[0]);
		}
	}
	end();
}

/* ---- NEG */
static void t_neg(void) {
	unsigned i,fin,fo,fm,res,ok;

	begin("NEG");
	for (i=0;i < 16;i++) {
		fin = (i & 1) ? (0x0202|ARITH) : 0x0202;

		setup(); ri[R_FL] = fin; ri[R_CX] = 0x7700|v8[i]; EB(2, 0xF6,0xD9); go();
		res = alu_ref(OP_SUB,0,0,v8[i],fin & ~CF,&fo,&fm); set8(re,1,res); re[R_FL] = fo;
		if (!verify(fm|CTRL)) failr("F6 D9 neg cl %02X",v8[i]);

		setup(); ri[R_FL] = fin; ri[R_DX] = v16[i]; EB(2, 0xF7,0xDA); go();
		res = alu_ref(OP_SUB,1,0,v16[i],fin & ~CF,&fo,&fm); re[R_DX] = res; re[R_FL] = fo;
		if (!verify(fm|CTRL)) failr("F7 DA neg dx %04X",v16[i]);

		setup(); ri[R_FL] = fin; memw[0] = v16[i]; ri[R_BX] = memw_off; EB(2, 0xF7,0x1F); go();
		res = alu_ref(OP_SUB,1,0,v16[i],fin & ~CF,&fo,&fm); re[R_FL] = fo;
		ok = verify(fm|CTRL);
		if (!checkm(memw[0] == res)) ok = 0;
		if (!ok) failr("F7 1F neg word [bx] %04X mem=%04X",v16[i],memw[0]);
	}
	end();
}

/* ---- NOT */
static void t_not(void) {
	unsigned i,fin,ok;

	begin("NOT");
	for (i=0;i < 16;i++) {
		fin = (i & 1) ? (0x0202|ARITH) : 0x0202;

		setup(); ri[R_FL] = fin; ri[R_CX] = 0x7700|v8[i]; EB(2, 0xF6,0xD1); go();
		set8(re,1,~v8[i]);
		if (!verify(ARITH|CTRL)) failr("F6 D1 not cl %02X",v8[i]);

		setup(); ri[R_FL] = fin; ri[R_DX] = v16[i]; EB(2, 0xF7,0xD2); go();
		re[R_DX] = ~v16[i];
		if (!verify(ARITH|CTRL)) failr("F7 D2 not dx %04X",v16[i]);

		setup(); ri[R_FL] = fin; memw[0] = v16[i]; ri[R_BX] = memw_off; EB(2, 0xF7,0x17); go();
		ok = verify(ARITH|CTRL);
		if (!checkm(memw[0] == (unsigned short)~v16[i])) ok = 0;
		if (!ok) failr("F7 17 not word [bx] %04X mem=%04X",v16[i],memw[0]);
	}
	end();
}

/* ---- MUL/IMUL */
static void t_mul(int sgn) {
	unsigned i,j,fin,mr = sgn ? 5 : 4;
	unsigned long r;
	long sr;

	begin(sgn ? "IMUL" : "MUL");
	for (i=0;i < 16;i++) {
		for (j=0;j < 16;j++) {
			fin = ((i ^ j) & 1) ? (0x0202|ARITH) : 0x0202;

			/* byte: reg and mem */
			if (sgn) { sr = (long)(signed char)v8[i] * (long)(signed char)v8[j]; r = (unsigned long)sr & 0xFFFFul; }
			else r = (unsigned long)v8[i] * (unsigned long)v8[j];

			setup(); ri[R_FL] = fin; ri[R_AX] = 0x5A00|v8[i]; ri[R_BX] = 0xC300|v8[j]; EB(2, 0xF6,0xC3|(mr<<3)); go();
			re[R_AX] = (unsigned)r;
			re[R_FL] = (sgn ? (sext8((unsigned)r & 0xFF) != (unsigned)r) : ((r & 0xFF00ul) != 0)) ? (fin|CF|OF) : (fin & ~(CF|OF));
			if (!verify(CF|OF|CTRL)) failr("F6 /%u %s bl %02X*%02X",mr,sgn ? "imul" : "mul",v8[i],v8[j]);

			setup(); ri[R_FL] = fin; ri[R_AX] = 0x5A00|v8[i]; memw[0] = 0xC300|v8[j]; ri[R_BX] = memw_off; EB(2, 0xF6,0x07|(mr<<3)); go();
			re[R_AX] = (unsigned)r;
			re[R_FL] = (sgn ? (sext8((unsigned)r & 0xFF) != (unsigned)r) : ((r & 0xFF00ul) != 0)) ? (fin|CF|OF) : (fin & ~(CF|OF));
			if (!verify(CF|OF|CTRL)) failr("F6 /%u %s byte [bx] %02X*%02X",mr,sgn ? "imul" : "mul",v8[i],v8[j]);

			/* word */
			if (sgn) { sr = (long)(short)v16[i] * (long)(short)v16[j]; r = (unsigned long)sr; }
			else r = (unsigned long)v16[i] * (unsigned long)v16[j];

			setup(); ri[R_FL] = fin; ri[R_AX] = v16[i]; ri[R_BX] = v16[j]; EB(2, 0xF7,0xC3|(mr<<3)); go();
			re[R_AX] = (unsigned)(r & 0xFFFFul); re[R_DX] = (unsigned)(r >> 16ul);
			if (sgn) re[R_FL] = (re[R_DX] != ((re[R_AX] & 0x8000) ? 0xFFFF : 0)) ? (fin|CF|OF) : (fin & ~(CF|OF));
			else re[R_FL] = (re[R_DX] != 0) ? (fin|CF|OF) : (fin & ~(CF|OF));
			if (!verify(CF|OF|CTRL)) failr("F7 /%u %s bx %04X*%04X",mr,sgn ? "imul" : "mul",v16[i],v16[j]);

			setup(); ri[R_FL] = fin; ri[R_AX] = v16[i]; memw[0] = v16[j]; ri[R_BX] = memw_off; EB(2, 0xF7,0x07|(mr<<3)); go();
			re[R_AX] = (unsigned)(r & 0xFFFFul); re[R_DX] = (unsigned)(r >> 16ul);
			if (sgn) re[R_FL] = (re[R_DX] != ((re[R_AX] & 0x8000) ? 0xFFFF : 0)) ? (fin|CF|OF) : (fin & ~(CF|OF));
			else re[R_FL] = (re[R_DX] != 0) ? (fin|CF|OF) : (fin & ~(CF|OF));
			if (!verify(CF|OF|CTRL)) failr("F7 /%u %s word [bx] %04X*%04X",mr,sgn ? "imul" : "mul",v16[i],v16[j]);
		}
	}
	end();
}

/* ---- DIV/IDIV without overflow */
static void t_div(int sgn) {
	static const unsigned short dxv[5] = { 0x0000,0x0001,0x1234,0x7FFF,0xFFFE };
	unsigned i,j,k,mr = sgn ? 7 : 6,q,rm;
	unsigned long dvd,uq;
	long sd,sq,sr;

	begin(sgn ? "IDIV" : "DIV");
	for (i=0;i < 16;i++) {
		for (j=0;j < 16;j++) {
			/* byte: AX / r/m8 */
			if (v8[j] != 0) {
				if (sgn) {
					sd = (long)(short)v16[i];
					sq = sd / (long)(signed char)v8[j];
					sr = sd % (long)(signed char)v8[j];
					if (sq >= -127l && sq <= 127l) {
						q = (unsigned)sq & 0xFF; rm = (unsigned)sr & 0xFF;
						goto do_byte;
					}
				}
				else {
					uq = (unsigned long)v16[i] / v8[j];
					if (uq <= 0xFFul) {
						q = (unsigned)uq; rm = v16[i] % v8[j];
						goto do_byte;
					}
				}
				goto skip_byte;
do_byte:
				setup(); ri[R_AX] = v16[i]; ri[R_BX] = 0xC300|v8[j]; EB(2, 0xF6,0xC3|(mr<<3)); go();
				re[R_AX] = (rm << 8) | q;
				if (!verify(CTRL)) failr("F6 /%u %s bl %04X/%02X",mr,sgn ? "idiv" : "div",v16[i],v8[j]);

				setup(); ri[R_AX] = v16[i]; memw[0] = 0xC300|v8[j]; ri[R_BX] = memw_off; EB(2, 0xF6,0x07|(mr<<3)); go();
				re[R_AX] = (rm << 8) | q;
				if (!verify(CTRL)) failr("F6 /%u %s byte [bx] %04X/%02X",mr,sgn ? "idiv" : "div",v16[i],v8[j]);
skip_byte:
				;
			}

			/* word: DX:AX / r/m16 */
			if (v16[j] != 0) {
				for (k=0;k < 5;k++) {
					dvd = ((unsigned long)dxv[k] << 16ul) | v16[i];
					if (sgn) {
						sd = (long)dvd;
						sq = sd / (long)(short)v16[j];
						sr = sd % (long)(short)v16[j];
						if (sq < -32767l || sq > 32767l) continue;
						q = (unsigned)sq; rm = (unsigned)sr;
					}
					else {
						uq = dvd / v16[j];
						if (uq > 0xFFFFul) continue;
						q = (unsigned)uq; rm = (unsigned)(dvd % v16[j]);
					}

					setup(); ri[R_AX] = v16[i]; ri[R_DX] = dxv[k]; ri[R_BX] = v16[j]; EB(2, 0xF7,0xC3|(mr<<3)); go();
					re[R_AX] = q; re[R_DX] = rm;
					if (!verify(CTRL)) failr("F7 /%u %s bx %04X%04X/%04X",mr,sgn ? "idiv" : "div",dxv[k],v16[i],v16[j]);

					setup(); ri[R_AX] = v16[i]; ri[R_DX] = dxv[k]; memw[0] = v16[j]; ri[R_BX] = memw_off; EB(2, 0xF7,0x07|(mr<<3)); go();
					re[R_AX] = q; re[R_DX] = rm;
					if (!verify(CTRL)) failr("F7 /%u %s word [bx] %04X%04X/%04X",mr,sgn ? "idiv" : "div",dxv[k],v16[i],v16[j]);
				}
			}
		}
	}
	end();
}

/* run a thunk expected to raise INT 0. start = offset of the faulting instruction (incl. prefixes), len = its length.
 * returns 1 = pushed IP points at the instruction (286+), 2 = after it (8086), 0 = failed */
static int de_style_8086 = 0,de_style_286 = 0;

static int de_case(const char *what,unsigned start,unsigned len) {
	x_clear();
	go();

	if (!checkm(x_cnt == 1 && x_vec == 0 && x_cs == thunk_seg)) {
		failm("%s: INT 0 not raised (count=%u)",what,x_cnt);
		return 0;
	}
	if (sp_before != sp_after) {
		failm("%s: stack imbalance",what);
		return 0;
	}
	if (x_ip == start) {
		de_style_286++;
		if (is8086) failm("%s: 8086 should push IP after the instruction",what);
		return 1;
	}
	if (x_ip == start + len) {
		de_style_8086++;
		if (is286p) failm("%s: 286+ should push IP of the faulting instruction",what);
		return 2;
	}
	failm("%s: INT 0 pushed IP=%04X, instruction at %04X len %u",what,x_ip,start,len);
	return 0;
}

/* ---- divide error exceptions */
static void t_div_fault(void) {
	unsigned s;

	begin("#DE fault");
	de_style_8086 = de_style_286 = 0;

	setup(); ri[R_AX] = 0x1234; ri[R_BX] = 0; s = here(); EB(2, 0xF6,0xF3); de_case("div bl, bl=0",s,2);
	setup(); ri[R_AX] = 0x1000; ri[R_BX] = 0x10; s = here(); EB(2, 0xF6,0xF3); de_case("div bl, quotient 100h",s,2);
	setup(); ri[R_BX] = 0; s = here(); EB(2, 0xF7,0xF3); de_case("div bx, bx=0",s,2);
	setup(); ri[R_DX] = 0x10; ri[R_AX] = 0; ri[R_BX] = 0x10; s = here(); EB(2, 0xF7,0xF3); de_case("div bx, quotient 10000h",s,2);
	setup(); ri[R_AX] = 0x8000; ri[R_BX] = 1; s = here(); EB(2, 0xF6,0xFB); de_case("idiv bl, -32768/1",s,2);
	setup(); ri[R_DX] = 1; ri[R_AX] = 0; ri[R_BX] = 1; s = here(); EB(2, 0xF7,0xFB); de_case("idiv bx, 65536/1",s,2);
	setup(); memw[0] = 0; ri[R_BX] = memw_off; s = here(); EB(2, 0xF6,0x37); de_case("div byte [bx]=0",s,2);
	setup(); memw[1] = 0; ri[R_BX] = memw_off; s = here(); EB(4, 0xF7,0xB7,0x02,0x00); de_case("div word [bx+2]=0",s,4);
	setup(); ri[R_ES] = dgroup; memw[0] = 0; ri[R_BX] = memw_off; s = here(); EB(3, 0x26,0xF6,0x37); de_case("es: div byte [bx]=0",s,3);
	if (!c_necaad) { setup(); ri[R_AX] = 0x0055; s = here(); EB(2, 0xD4,0x00); de_case("aam 0",s,2); }

	if (de_style_8086 && !de_style_286) info("INT 0 return address: after the instruction (8086)");
	else if (de_style_286 && !de_style_8086) info("INT 0 return address: the faulting instruction (286+)");
	else if (de_style_286 || de_style_8086) info("INT 0 return address: mixed (%u after, %u at)",de_style_8086,de_style_286);
	end();
}

/* ---- IDIV with quotient -128 / -32768: #DE on 8086, valid on 286+ */
static void t_idiv_min(void) {
	unsigned faulted;

	begin("IDIV -80h");

	setup(); ri[R_AX] = 0xFF80; ri[R_BX] = 1; EB(2, 0xF6,0xFB);
	x_clear(); go();
	faulted = x_cnt;
	if (faulted) info("IDIV byte -128/1 raises #DE (8086 behavior)");
	else info("IDIV byte -128/1 = AL=%02X AH=%02X (286+ behavior)",ro[R_AX] & 0xFF,ro[R_AX] >> 8);
	if (is8086 && !checkm(faulted)) failm("8086 should raise #DE for quotient -128");
	if (is286p && !checkm(!faulted && ro[R_AX] == 0x0080)) failm("286+ should return quotient -128, got AX=%04X",ro[R_AX]);

	setup(); ri[R_DX] = 0xFFFF; ri[R_AX] = 0x8000; ri[R_BX] = 1; EB(2, 0xF7,0xFB);
	x_clear(); go();
	faulted = x_cnt;
	if (faulted) info("IDIV word -32768/1 raises #DE (8086 behavior)");
	else info("IDIV word -32768/1 = AX=%04X DX=%04X (286+ behavior)",ro[R_AX],ro[R_DX]);
	if (is8086 && !checkm(faulted)) failm("8086 should raise #DE for quotient -32768");
	if (is286p && !checkm(!faulted && ro[R_AX] == 0x8000 && ro[R_DX] == 0)) failm("286+ should return quotient -32768, got AX=%04X DX=%04X",ro[R_AX],ro[R_DX]);

	end();
}

static unsigned tobcd(unsigned v) {
	return ((v / 10u) << 4u) | (v % 10u);
}

/* ---- DAA/DAS on the results of valid packed BCD add/sub */
static void t_daa(void) {
	static const unsigned char bv[10] = { 0,1,9,10,19,45,50,55,90,99 };
	unsigned a,j,b,s,al,cf;

	begin("DAA/DAS");
	for (a=0;a < 100;a++) {
		for (j=0;j < 10;j++) {
			b = bv[j];

			setup(); ri[R_AX] = 0x5A00 | tobcd(a); ri[R_BX] = tobcd(b); EB(3, 0x00,0xD8,0x27); go();
			s = a + b; cf = (s >= 100) ? CF : 0; al = tobcd(s % 100u);
			set8(re,0,al); re[R_FL] = (0x0202 | cf | szp(al,0));
			if (!verify(CF|ZF|SF|PF|CTRL)) failr("add+daa %u+%u",a,b);

			setup(); ri[R_AX] = 0x5A00 | tobcd(a); ri[R_BX] = tobcd(b); EB(3, 0x28,0xD8,0x2F); go();
			cf = (a < b) ? CF : 0; al = tobcd((a + 100u - b) % 100u);
			set8(re,0,al); re[R_FL] = (0x0202 | cf | szp(al,0));
			if (!verify(CF|ZF|SF|PF|CTRL)) failr("sub+das %u-%u",a,b);
		}
	}
	end();
}

/* ---- AAA/AAS on the results of unpacked BCD add/sub */
static void t_aaa(void) {
	static const unsigned char ahv[3] = { 0x00,0x7F,0xFF };
	unsigned a,b,k,c;

	begin("AAA/AAS");
	for (a=0;a < 10;a++) {
		for (b=0;b < 10;b++) {
			for (k=0;k < 3;k++) {
				setup(); ri[R_AX] = (ahv[k] << 8) | a; ri[R_BX] = b; EB(3, 0x00,0xD8,0x37); go();
				c = (a + b >= 10) ? 1 : 0;
				re[R_AX] = (((ahv[k] + c) & 0xFF) << 8) | ((a + b) % 10u);
				re[R_FL] = c ? (0x0202|CF|AF) : 0x0202;
				if (!verify(CF|AF|CTRL)) failr("add+aaa %u+%u ah=%02X",a,b,ahv[k]);

				setup(); ri[R_AX] = (ahv[k] << 8) | a; ri[R_BX] = b; EB(3, 0x28,0xD8,0x3F); go();
				c = (a < b) ? 1 : 0;
				re[R_AX] = (((ahv[k] - c) & 0xFF) << 8) | ((a + 10u - b) % 10u);
				re[R_FL] = c ? (0x0202|CF|AF) : 0x0202;
				if (!verify(CF|AF|CTRL)) failr("sub+aas %u-%u ah=%02X",a,b,ahv[k]);
			}
		}
	}
	end();
}

/* ---- AAA with AL >= FAh: 8086 adds 6 to AL only, later CPUs add 106h to AX */
static void t_aaa_quirk(void) {
	begin("AAA quirk");
	setup(); ri[R_AX] = 0x00FA; E(0x37); go();
	if (checkm(ro[R_AX] == 0x0100)) info("AAA AL=FAh -> AX=0100h (AL+6, AH+1, 8086 style)");
	else if (ro[R_AX] == 0x0200) info("AAA AL=FAh -> AX=0200h (AX+106h, 286+ style)");
	else failm("AAA AX=00FAh gave AX=%04X, expected 0100h or 0200h",ro[R_AX]);
	end();
}

/* ---- AAM/AAD base 10 */
static void t_aam(void) {
	unsigned i,j,al;

	begin("AAM/AAD");
	for (i=0;i < 256;i++) {
		setup(); ri[R_AX] = 0x5A00 | i; EB(2, 0xD4,0x0A); go();
		re[R_AX] = ((i / 10u) << 8) | (i % 10u);
		re[R_FL] = 0x0202 | szp(i % 10u,0);
		if (!verify(SF|ZF|PF|CTRL)) failr("aam al=%02X",i);
	}
	for (i=0;i < 16;i++) {
		for (j=0;j < 16;j++) {
			setup(); ri[R_AX] = (v8[i] << 8) | v8[j]; EB(2, 0xD5,0x0A); go();
			al = (v8[i] * 10u + v8[j]) & 0xFF;
			re[R_AX] = al; re[R_FL] = 0x0202 | szp(al,0);
			if (!verify(SF|ZF|PF|CTRL)) failr("aad ax=%04X",ri[R_AX]);
		}
	}
	end();
}

/* ---- AAM/AAD with other bases (undocumented, honored by Intel CPUs, not NEC) */
static void t_aam_imm(void) {
	unsigned i,al;

	begin("AAM/AAD imm");
	if (c_necaad) info("AAD ignores the immediate operand (NEC V20/V30 behavior)");
	for (i=0;i < 256;i += 7) {
		setup(); ri[R_AX] = 0x5A00 | i; EB(2, 0xD4,0x10); go();
		re[R_AX] = ((i >> 4) << 8) | (i & 15);
		re[R_FL] = 0x0202 | szp(i & 15,0);
		if (!c_necaad && !verify(SF|ZF|PF|CTRL)) failr("aam 16 al=%02X",i);

		setup(); ri[R_AX] = (i << 8) | 0x05; EB(2, 0xD5,0x07); go();
		al = (i * 7u + 5u) & 0xFF;
		re[R_AX] = al; re[R_FL] = 0x0202 | szp(al,0);
		if (!c_necaad && !verify(SF|ZF|PF|CTRL)) failr("aad 7 ax=%04X",ri[R_AX]);
	}
	end();
}

/* ---- SALC (undocumented D6h) */
static void t_salc(void) {
	begin("SALC");
	if (c_necaad) {
		skip("NEC V20/V30 does not implement SALC");
	}
	else {
		setup(); ri[R_FL] = 0x0202; E(0xD6); go(); set8(re,0,0x00);
		if (!verify(ARITH|CTRL)) failr("salc CF=0");
		setup(); ri[R_FL] = 0x0203; E(0xD6); go(); set8(re,0,0xFF);
		if (!verify(ARITH|CTRL)) failr("salc CF=1");
	}
	end();
}

/* ---- shifts and rotates */
enum { SF_R8_1=0, SF_R16_1, SF_R8_CL, SF_R16_CL, SF_M16_1, SF_M16_CL, SF_M8_CL, SF_MAX };
static const char *sh_form_names[SF_MAX] = { "r8,1","r16,1","r8,CL","r16,CL","m16,1","m16,CL","m8,CL" };
static const unsigned char sh_counts[] = { 0,1,2,3,4,7,8,9,15,16,17,31,32,33,255 };
#define SH_COUNTS (sizeof(sh_counts)/sizeof(sh_counts[0]))

static void sh_case(unsigned op,unsigned form,unsigned v,unsigned cnt,unsigned fin) {
	unsigned w,res,fo,fm,ok,mem_exp = 0;
	int memtest = 0;

	setup();
	ri[R_FL] = fin;
	switch (form) {
		case SF_R8_1:   w=0; ri[R_AX] = 0x5A00|v; EB(2, 0xD0,0xC0|(op<<3)); break;
		case SF_R16_1:  w=1; ri[R_DX] = v; EB(2, 0xD1,0xC2|(op<<3)); break;
		case SF_R8_CL:  w=0; ri[R_BX] = 0xC300|v; ri[R_CX] = 0x6600|cnt; EB(2, 0xD2,0xC3|(op<<3)); break;
		case SF_R16_CL: w=1; ri[R_AX] = v; ri[R_CX] = 0x6600|cnt; EB(2, 0xD3,0xC0|(op<<3)); break;
		case SF_M16_1:  w=1; memw[0] = v; ri[R_BX] = memw_off; EB(2, 0xD1,0x07|(op<<3)); memtest = 1; break;
		case SF_M16_CL: w=1; memw[0] = v; ri[R_BX] = memw_off; ri[R_CX] = 0x6600|cnt; EB(2, 0xD3,0x07|(op<<3)); memtest = 1; break;
		case SF_M8_CL:  w=0; memw[0] = 0x9900|v; ri[R_BX] = memw_off; ri[R_CX] = 0x6600|cnt; EB(2, 0xD2,0x07|(op<<3)); memtest = 1; break;
		default: return;
	}
	go();

	res = shift_ref(op,w,v,cnt,fin,&fo,&fm);
	re[R_FL] = fo;
	switch (form) {
		case SF_R8_1:   set8(re,0,res); break;
		case SF_R16_1:  re[R_DX] = res; break;
		case SF_R8_CL:  set8(re,3,res); break;
		case SF_R16_CL: re[R_AX] = res; break;
		case SF_M16_1: case SF_M16_CL: mem_exp = res; break;
		case SF_M8_CL:  mem_exp = 0x9900|res; break;
	}

	ok = verify(fm|CTRL);
	if (memtest && !checkm(memw[0] == mem_exp)) {
		ok = 0;
		sprintf(diffbuf+strlen(diffbuf)," mem=%04X(%04X)",memw[0],mem_exp);
	}
	if (!ok) failr("%s %s v=%04X cnt=%u",sh_names[op],sh_form_names[form],v,cnt);
}

static void t_shift(unsigned op) {
	unsigned form,i,c,v,fin,cl;

	begin(sh_names[op]);
	for (form=0;form < SF_MAX;form++) {
		cl = (form == SF_R8_CL || form == SF_R16_CL || form == SF_M16_CL || form == SF_M8_CL);
		for (i=0;i < 16;i++) {
			v = (form == SF_R8_1 || form == SF_R8_CL || form == SF_M8_CL) ? v8[i] : v16[i];
			for (c=0;c < (cl ? SH_COUNTS : 1);c++) {
				if (cl && sh_counts[c] == 255 && i > 1) continue;	/* slow on 8086 */
				fin = ((i ^ c) & 1) ? (0x0202|ARITH) : 0x0202;
				sh_case(op,form,v,cl ? sh_counts[c] : 1,fin);
				if (op == SH_RCL || op == SH_RCR) sh_case(op,form,v,cl ? sh_counts[c] : 1,fin ^ CF);
			}
		}
	}
	end();
}

/* ---- string instructions. source in tbl (DS), destination in xbuf (ES) */
static unsigned char sexp[64];

static void str_prep(unsigned char fill) {
	unsigned i;

	for (i=0;i < 64;i++) {
		tbl[i] = (unsigned char)(0x30 + i);
		xbuf[i] = fill;
		sexp[i] = fill;
	}
}

static int str_check_dst(void) {
	unsigned i;

	for (i=0;i < 64;i++) {
		if (xbuf[i] != sexp[i]) {
			sprintf(diffbuf+strlen(diffbuf)," es:[%u]=%02X(%02X)",i,xbuf[i],sexp[i]);
			return 0;
		}
	}
	return 1;
}

static unsigned S0,D0;	/* buffer start offsets */

static void str_setup(unsigned si,unsigned di,unsigned cx,int df) {
	setup();
	ri[R_SI] = S0 + si; ri[R_DI] = D0 + di; ri[R_CX] = cx; ri[R_ES] = FP_SEG(xbuf);
	if (df) ri[R_FL] |= DF;
}

/* MOVS reference: copy n elements of size sz */
static void movs_case(const char *what,unsigned sz,unsigned si,unsigned di,unsigned n,unsigned cx_in,int df,int rep) {
	unsigned i,k,ok;
	int step = df ? -(int)sz : (int)sz;

	str_prep(0xEE);
	str_setup(si,di,cx_in,df);
	if (rep) E(rep);
	E(sz == 2 ? 0xA5 : 0xA4);
	go();
	for (i=0;i < n;i++) {
		for (k=0;k < sz;k++) sexp[di + k] = tbl[si + k];
		si += step; di += step;
	}
	re[R_SI] = S0 + si; re[R_DI] = D0 + di; re[R_CX] = rep ? (cx_in - n) : cx_in;
	ok = verify(ARITH|CTRL);
	if (!str_check_dst()) ok = 0;
	if (!ok) failr("%s",what);
}

static void t_movs(void) {
	begin("MOVS");
	movs_case("A4 movsb",1,0,0,1,7,0,0);
	movs_case("A5 movsw",2,0,0,1,7,0,0);
	movs_case("A4 movsb DF=1",1,9,9,1,7,1,0);
	movs_case("F3 A4 rep movsb cx=5",1,0,2,5,5,0,0xF3);
	movs_case("F3 A5 rep movsw cx=4",2,1,0,4,4,0,0xF3);
	movs_case("F3 A4 rep movsb cx=5 DF=1",1,9,9,5,5,1,0xF3);
	movs_case("F3 A5 rep movsw cx=3 DF=1",2,10,20,3,3,1,0xF3);
	movs_case("F3 A4 rep movsb cx=0",1,0,0,0,0,0,0xF3);
	movs_case("F2 A4 repne movsb cx=6",1,3,0,6,6,0,0xF2);
	end();
}

static void t_lods_stos(void) {
	unsigned i;

	begin("LODS/STOS");
	str_prep(0xEE);

	str_setup(4,0,9,0); E(0xAC); go(); re[R_AX] = (ri[R_AX] & 0xFF00) | tbl[4]; re[R_SI] = ri[R_SI] + 1;
	if (!verify(ARITH|CTRL)) failr("AC lodsb");

	str_setup(4,0,9,0); E(0xAD); go(); re[R_AX] = tbl[4] | (tbl[5] << 8); re[R_SI] = ri[R_SI] + 2;
	if (!verify(ARITH|CTRL)) failr("AD lodsw");

	str_setup(10,0,9,1); E(0xAD); go(); re[R_AX] = tbl[10] | (tbl[11] << 8); re[R_SI] = ri[R_SI] - 2;
	if (!verify(ARITH|CTRL)) failr("AD lodsw DF=1");

	str_setup(4,0,3,0); EB(2, 0xF3,0xAC); go(); re[R_AX] = (ri[R_AX] & 0xFF00) | tbl[6]; re[R_SI] = ri[R_SI] + 3; re[R_CX] = 0;
	if (!verify(ARITH|CTRL)) failr("F3 AC rep lodsb cx=3");

	str_prep(0xEE);
	str_setup(0,5,9,0); ri[R_AX] = 0x1234; E(0xAA); go(); re[R_DI] = ri[R_DI] + 1; sexp[5] = 0x34;
	if (!verify(ARITH|CTRL) || !str_check_dst()) failr("AA stosb");

	str_prep(0xEE);
	str_setup(0,5,9,0); ri[R_AX] = 0x1234; E(0xAB); go(); re[R_DI] = ri[R_DI] + 2; sexp[5] = 0x34; sexp[6] = 0x12;
	if (!verify(ARITH|CTRL) || !str_check_dst()) failr("AB stosw");

	str_prep(0xEE);
	str_setup(0,2,4,0); ri[R_AX] = 0x5678; EB(2, 0xF3,0xAB); go(); re[R_DI] = ri[R_DI] + 8; re[R_CX] = 0;
	for (i=0;i < 4;i++) { sexp[2+i*2] = 0x78; sexp[3+i*2] = 0x56; }
	if (!verify(ARITH|CTRL) || !str_check_dst()) failr("F3 AB rep stosw cx=4");

	str_prep(0xEE);
	str_setup(0,20,3,1); ri[R_AX] = 0x00A5; EB(2, 0xF3,0xAA); go(); re[R_DI] = ri[R_DI] - 3; re[R_CX] = 0;
	sexp[20] = sexp[19] = sexp[18] = 0xA5;
	if (!verify(ARITH|CTRL) || !str_check_dst()) failr("F3 AA rep stosb cx=3 DF=1");

	end();
}

/* CMPS: compare DS:SI with ES:DI */
static void cmps_case(const char *what,unsigned sz,unsigned rep,unsigned cx,int df,unsigned base) {
	unsigned n = 0,fo,fm,a = 0,b = 0,step = df ? (unsigned)(-(int)sz) : sz,off = base,f;

	str_setup(base,base,cx,df);
	if (rep) E(rep);
	E(sz == 2 ? 0xA7 : 0xA6);
	go();

	f = ri[R_FL];
	do {
		if (rep && cx == 0) break;
		a = tbl[off]; b = xbuf[off];
		if (sz == 2) { a |= tbl[off+1] << 8; b |= xbuf[off+1] << 8; }
		alu_ref(OP_CMP,sz == 2,a,b,ri[R_FL],&fo,&fm);
		f = fo;
		off += step; n++;
		if (!rep) break;
		cx--;
		if (rep == 0xF3 && !(f & ZF)) break;
		if (rep == 0xF2 && (f & ZF)) break;
	} while (cx != 0);

	re[R_SI] = ri[R_SI] + n * step; re[R_DI] = ri[R_DI] + n * step;
	if (rep) re[R_CX] = ri[R_CX] - n;
	re[R_FL] = f;
	if (!verify(ARITH|CTRL)) failr("%s",what);
}

static void t_cmps(void) {
	unsigned i;

	begin("CMPS");
	str_prep(0); for (i=0;i < 64;i++) xbuf[i] = tbl[i];
	xbuf[4] = 0x99; xbuf[12] = 0x01;
	cmps_case("A6 cmpsb equal",1,0,9,0,0);
	cmps_case("A6 cmpsb differ",1,0,9,0,4);
	cmps_case("A6 cmpsb differ (below)",1,0,9,0,12);
	cmps_case("A7 cmpsw",2,0,9,0,3);
	cmps_case("F3 A6 repe cmpsb mismatch",1,0xF3,10,0,0);
	cmps_case("F3 A6 repe cmpsb all equal",1,0xF3,4,0,5);
	cmps_case("F3 A6 repe cmpsb cx=0",1,0xF3,0,0,0);
	cmps_case("F3 A7 repe cmpsw mismatch",2,0xF3,8,0,0);
	cmps_case("F3 A6 repe cmpsb DF=1",1,0xF3,10,1,9);

	str_prep(0); for (i=0;i < 64;i++) xbuf[i] = (unsigned char)~tbl[i];
	xbuf[6] = tbl[6];
	cmps_case("F2 A6 repne cmpsb match",1,0xF2,20,0,0);
	cmps_case("F2 A6 repne cmpsb no match",1,0xF2,5,0,7);
	cmps_case("F2 A7 repne cmpsw",2,0xF2,9,0,20);
	end();
}

/* SCAS: compare AL/AX with ES:DI */
static void scas_case(const char *what,unsigned sz,unsigned rep,unsigned cx,int df,unsigned base,unsigned ax) {
	unsigned n = 0,fo,fm,b,step = df ? (unsigned)(-(int)sz) : sz,off = base,f;

	str_setup(0,base,cx,df);
	ri[R_AX] = ax;
	if (rep) E(rep);
	E(sz == 2 ? 0xAF : 0xAE);
	go();

	f = ri[R_FL];
	do {
		if (rep && cx == 0) break;
		b = xbuf[off];
		if (sz == 2) b |= xbuf[off+1] << 8;
		alu_ref(OP_CMP,sz == 2,sz == 2 ? ax : (ax & 0xFF),b,ri[R_FL],&fo,&fm);
		f = fo;
		off += step; n++;
		if (!rep) break;
		cx--;
		if (rep == 0xF3 && !(f & ZF)) break;
		if (rep == 0xF2 && (f & ZF)) break;
	} while (cx != 0);

	re[R_DI] = ri[R_DI] + n * step;
	if (rep) re[R_CX] = ri[R_CX] - n;
	re[R_FL] = f;
	if (!verify(ARITH|CTRL)) failr("%s",what);
}

static void t_scas(void) {
	unsigned i;

	begin("SCAS");
	str_prep(0xEE);
	xbuf[7] = 0x42; xbuf[8] = 0x43; xbuf[30] = 0x10;
	scas_case("AE scasb",1,0,5,0,0,0x11EE);
	scas_case("AE scasb below",1,0,5,0,30,0x1105);
	scas_case("AF scasw",2,0,5,0,7,0x4342);
	scas_case("F2 AE repne scasb found",1,0xF2,20,0,0,0x0042);
	scas_case("F2 AE repne scasb not found",1,0xF2,5,0,0,0x0042);
	scas_case("F3 AE repe scasb",1,0xF3,20,0,0,0x00EE);
	scas_case("F3 AE repe scasb cx=0",1,0xF3,0,0,0,0x00EE);
	scas_case("F2 AE repne scasb DF=1",1,0xF2,20,1,15,0x0043);
	for (i=40;i < 64;i++) xbuf[i] = (unsigned char)i;
	scas_case("F2 AF repne scasw",2,0xF2,10,0,40,0x2D2C);
	scas_case("F3 AF repe scasw",2,0xF3,10,0,0,0xEEEE);
	end();
}

/* ---- segment overrides on string instructions */
static void t_str_seg(void) {
	unsigned T = memw_off + 0x40,cso = FP_OFF(thunk) + THUNK_DATA;

	begin("Str segovr");
	str_prep(0xEE);
	xbuf[0] = 0x5C; xbuf[1] = 0x5D;

	/* es: movsb, source from ES:SI */
	str_setup(0,32,9,0); ri[R_SI] = D0; EB(2, 0x26,0xA4); go();
	re[R_SI] = D0 + 1; re[R_DI] = D0 + 33; sexp[0] = 0x5C; sexp[1] = 0x5D; sexp[32] = 0x5C;
	if (!verify(ARITH|CTRL) || !str_check_dst()) failr("26 A4 es: movsb");

	/* rep es: movsb */
	str_prep(0xEE); xbuf[0] = 0x5C; xbuf[1] = 0x5D; xbuf[2] = 0x5E;
	str_setup(0,40,3,0); ri[R_SI] = D0; EB(3, 0xF3,0x26,0xA4); go();
	re[R_SI] = D0 + 3; re[R_DI] = D0 + 43; re[R_CX] = 0;
	sexp[0] = 0x5C; sexp[1] = 0x5D; sexp[2] = 0x5E; sexp[40] = 0x5C; sexp[41] = 0x5D; sexp[42] = 0x5E;
	if (!verify(ARITH|CTRL) || !str_check_dst()) failr("F3 26 A4 rep es: movsb");

	/* cs: lodsb */
	thunk[THUNK_DATA+4] = 0xC7;
	str_setup(0,0,9,0); ri[R_SI] = cso + 4; EB(2, 0x2E,0xAC); go();
	re[R_AX] = (ri[R_AX] & 0xFF00) | 0xC7; re[R_SI] = cso + 5;
	if (!verify(ARITH|CTRL)) failr("2E AC cs: lodsb");

	/* ss: lodsw */
	str_setup(0,0,9,0); ri[R_SI] = T; EB(2, 0x36,0xAD); go();
	re[R_AX] = *((unsigned short __far*)MK_FP(ss_val,T)); re[R_SI] = T + 2;
	if (!verify(ARITH|CTRL)) failr("36 AD ss: lodsw");

	/* cs: stosb, destination cannot be overridden */
	str_prep(0xEE);
	thunk[THUNK_DATA+8] = 0x11;
	str_setup(0,8,9,0); ri[R_AX] = 0x77; EB(2, 0x2E,0xAA); go();
	re[R_DI] = D0 + 9; sexp[8] = 0x77;
	if (!verify(ARITH|CTRL) || !str_check_dst() || !checkm(thunk[THUNK_DATA+8] == 0x11)) failr("2E AA cs: stosb must write ES:DI");

	/* es: cmpsb compares ES:SI with ES:DI */
	str_prep(0xEE); xbuf[3] = 0x20; xbuf[10] = 0x20;
	str_setup(0,10,9,0); ri[R_SI] = D0 + 3; EB(2, 0x26,0xA6); go();
	re[R_SI] = D0 + 4; re[R_DI] = D0 + 11; re[R_FL] = (ri[R_FL] & ~ARITH) | ZF | PF;
	if (!verify(ARITH|CTRL)) failr("26 A6 es: cmpsb");

	end();
}

/* ---- REP with segment override interrupted by an IRQ (8086 loses the REP prefix) */
static void t_rep_irq(void) {
	unsigned seg,tries;
	volatile unsigned long __far *ticks = (volatile unsigned long __far*)MK_FP(0x40,0x6C);
	unsigned long t0;
	int interrupted = 0,lost = 0;

	begin("REP+IRQ");
	if (pc98) {
		skip("no periodic timer IRQ on PC-98");
		end();
		return;
	}
	if (_dos_allocmem(0x1000,&seg) != 0) {
		skip("cannot allocate 64KB");
		end();
		return;
	}
	for (tries=0;tries < 16 && !lost;tries++) {
		setup(); ri[R_ES] = seg; ri[R_SI] = 0; ri[R_DI] = 0; ri[R_CX] = 0xFFFF;
		EB(3, 0xF3,0x26,0xA4);		/* rep es: movsb */
		t0 = *ticks;
		go();
		if (*ticks != t0) interrupted = 1;
		if (ro[R_CX] != 0) lost = 1;
	}
	_dos_freemem(seg);

	grp_tests++;
	if (lost) {
		info("REP lost after IRQ in 'rep es: movsb' (8086 behavior), CX=%04X",ro[R_CX]);
		if (is286p) failm("286+ should resume the REP prefix after an interrupt");
	}
	else if (interrupted) {
		info("REP with segment override survives an interrupt");
	}
	else {
		info("no timer IRQ occurred during the test");
	}
	end();
}

/* ---- JMP */
static void jmp_tail(unsigned char __far *fwd) {
	EB(3, 0xB0,0x01,0xCB);		/* wrong path: mov al,1; retf */
	if (fwd) fix8(fwd);
}

static void t_jmp(void) {
	unsigned char __far *p;
	unsigned t,b,i;

	begin("JMP");

	/* EB short forward */
	setup(); EB(3, 0xB0,0x00,0xEB); p = ph8(); EB(3, 0xB0,0x01,0xCB); fix8(p); EB(2, 0xB0,0x55); go();
	set8(re,0,0x55);
	if (!verify(ARITH|CTRL)) failr("EB jmp short forward");

	/* EB short +127 */
	setup(); EB(4, 0xB0,0x00,0xEB,0x7F); for (i=0;i < 124;i++) E(0x90); EB(3, 0xB0,0x01,0xCB); EB(2, 0xB0,0x55); go();
	set8(re,0,0x55);
	if (!verify(ARITH|CTRL)) failr("EB 7F jmp short +127");

	/* EB short backward -128 */
	setup(); EB(3, 0xB0,0x00,0xE9); p = ph16(); b = here(); EB(3, 0xB0,0x55,0xCB);
	for (i=0;i < 123;i++) E(0x90);
	fix16(p); t = here(); EB(2, 0xEB,0x80);	/* target = t+2-128 = b */
	EB(3, 0xB0,0x01,0xCB);
	go();
	set8(re,0,0x55);
	if (!checkm(t + 2u - 128u == b) || !verify(ARITH|CTRL)) failr("EB 80 jmp short -128");

	/* E9 near forward with a large displacement */
	setup(); EB(3, 0xB0,0x00,0xE9); p = ph16(); EB(3, 0xB0,0x01,0xCB); for (i=0;i < 300;i++) E(0x90);
	fix16(p); EB(2, 0xB0,0x55); go();
	set8(re,0,0x55);
	if (!verify(ARITH|CTRL)) failr("E9 jmp near forward");

	/* E9 near backward */
	setup(); EB(3, 0xB0,0x00,0xEB); p = ph8(); b = here(); EB(3, 0xB0,0x55,0xCB); fix8(p); E(0xE9); rel16(b); jmp_tail(NULL); tp -= 3; go();
	set8(re,0,0x55);
	if (!verify(ARITH|CTRL)) failr("E9 jmp near backward");

	/* FF /4 jmp bx */
	setup(); EB(4, 0xB0,0x00,0xFF,0xE3); EB(3, 0xB0,0x01,0xCB); t = here(); EB(2, 0xB0,0x55); ri[R_BX] = t; go();
	set8(re,0,0x55);
	if (!verify(ARITH|CTRL)) failr("FF E3 jmp bx");

	/* FF /4 jmp [bx] */
	setup(); EB(4, 0xB0,0x00,0xFF,0x27); EB(3, 0xB0,0x01,0xCB); t = here(); EB(2, 0xB0,0x55);
	memw[0] = t; ri[R_BX] = memw_off; go();
	set8(re,0,0x55);
	if (!verify(ARITH|CTRL)) failr("FF 27 jmp [bx]");

	/* EA jmp far direct */
	setup(); EB(3, 0xB0,0x00,0xEA); p = ph16(); EW(thunk_seg); EB(3, 0xB0,0x01,0xCB); t = here(); fixabs16(p,t);
	EB(4, 0x8C,0xCA,0xB0,0x55); go();
	set8(re,0,0x55); re[R_DX] = thunk_seg;
	if (!verify(ARITH|CTRL)) failr("EA jmp far");

	/* EA jmp far to an aliased seg:off of the same address */
	setup(); EB(3, 0xB0,0x00,0xEA); p = ph16(); EW(thunk_seg - 1); EB(3, 0xB0,0x01,0xCB); t = here(); fixabs16(p,t + 16);
	EB(4, 0x8C,0xCA,0xB0,0x55); go();
	set8(re,0,0x55); re[R_DX] = thunk_seg - 1;
	if (!verify(ARITH|CTRL)) failr("EA jmp far seg-1:off+16");

	/* FF /5 jmp far [bx] */
	setup(); EB(4, 0xB0,0x00,0xFF,0x2F); EB(3, 0xB0,0x01,0xCB); t = here(); EB(4, 0x8C,0xCA,0xB0,0x55);
	memw[0] = t; memw[1] = thunk_seg; ri[R_BX] = memw_off; go();
	set8(re,0,0x55); re[R_DX] = thunk_seg;
	if (!verify(ARITH|CTRL)) failr("FF 2F jmp far [bx]");

	end();
}

/* ---- CALL/RET */
static void t_call(void) {
	unsigned char __far *p;
	unsigned s,r;

	begin("CALL/RET");

	/* E8 call near; sub: mov bp,sp; mov cx,[bp+0]; ret */
	setup(); EB(2, 0xB0,0x00); E(0xE8); p = ph16(); r = here(); EB(3, 0xB0,0x55,0xCB);
	fix16(p); EB(7, 0x8B,0xEC,0x8B,0x4E,0x00,0xB0,0x11); E(0xC3); tp--; E(0xC3); go();
	set8(re,0,0x55); re[R_CX] = r; re[R_BP] = sp_before - 6;
	if (!verify(ARITH|CTRL)) failr("E8 call near / C3 ret");

	/* FF /2 call bx */
	setup(); EB(4, 0xB0,0x00,0xFF,0xD3); r = here(); EB(3, 0xB0,0x55,0xCB);
	s = here(); EB(5, 0x8B,0xEC,0x8B,0x4E,0x00); E(0xC3); ri[R_BX] = s; go();
	set8(re,0,0x55); re[R_CX] = r; re[R_BP] = sp_before - 6;
	if (!verify(ARITH|CTRL)) failr("FF D3 call bx");

	/* FF /2 call [bx] */
	setup(); EB(4, 0xB0,0x00,0xFF,0x17); r = here(); EB(3, 0xB0,0x55,0xCB);
	s = here(); EB(5, 0x8B,0xEC,0x8B,0x4E,0x00); E(0xC3); memw[0] = s; ri[R_BX] = memw_off; go();
	set8(re,0,0x55); re[R_CX] = r; re[R_BP] = sp_before - 6;
	if (!verify(ARITH|CTRL)) failr("FF 17 call [bx]");

	/* 9A call far; sub: mov bp,sp; mov cx,[bp+0]; mov dx,[bp+2]; retf */
	setup(); EB(3, 0xB0,0x00,0x9A); p = ph16(); EW(thunk_seg); r = here(); EB(3, 0xB0,0x55,0xCB);
	s = here(); fixabs16(p,s); EB(8, 0x8B,0xEC,0x8B,0x4E,0x00,0x8B,0x56,0x02); go();	/* go() appends the RETF */
	set8(re,0,0x55); re[R_CX] = r; re[R_DX] = thunk_seg; re[R_BP] = sp_before - 8;
	if (!verify(ARITH|CTRL)) failr("9A call far / CB retf");

	/* FF /3 call far [bx] */
	setup(); EB(4, 0xB0,0x00,0xFF,0x1F); r = here(); EB(3, 0xB0,0x55,0xCB);
	s = here(); EB(8, 0x8B,0xEC,0x8B,0x4E,0x00,0x8B,0x56,0x02);
	memw[0] = s; memw[1] = thunk_seg; ri[R_BX] = memw_off; go();
	set8(re,0,0x55); re[R_CX] = r; re[R_DX] = thunk_seg; re[R_BP] = sp_before - 8;
	if (!verify(ARITH|CTRL)) failr("FF 1F call far [bx]");

	/* C2 ret imm16 */
	setup(); EB(4, 0xB0,0x00,0x50,0x50); E(0xE8); p = ph16(); EB(3, 0xB0,0x55,0xCB); fix16(p); EB(3, 0xC2,0x04,0x00); go();
	set8(re,0,0x55);
	if (!verify(ARITH|CTRL)) failr("C2 04 00 ret 4");

	/* CA retf imm16 */
	setup(); EB(5, 0xB0,0x00,0x50,0x50,0x9A); p = ph16(); EW(thunk_seg); EB(3, 0xB0,0x55,0xCB); fixabs16(p,here()); EB(3, 0xCA,0x04,0x00); go();
	set8(re,0,0x55);
	if (!verify(ARITH|CTRL)) failr("CA 04 00 retf 4");

	/* push cs; call near; sub does retf */
	setup(); EB(3, 0xB0,0x00,0x0E); E(0xE8); p = ph16(); EB(3, 0xB0,0x55,0xCB); fix16(p); go();
	set8(re,0,0x55);
	if (!verify(ARITH|CTRL)) failr("push cs; call near; retf");

	end();
}

/* ---- Jcc, all 16 conditions, all combinations of OF SF ZF CF PF */
static int jcc_taken(unsigned cc,unsigned f) {
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

static void t_jcc(void) {
	unsigned cc,i,f,taken;

	begin("Jcc");
	for (cc=0;cc < 16;cc++) {
		for (i=0;i < 32;i++) {
			f = 0x0202 | ((i & 1) ? OF : 0) | ((i & 2) ? SF : 0) | ((i & 4) ? ZF : 0) | ((i & 8) ? CF : 0) | ((i & 16) ? PF : 0);
			taken = jcc_taken(cc,f);

			/* forward: mov al,0; jcc +2; mov al,1; retf */
			setup(); ri[R_FL] = f; EB(6, 0xB0,0x00,0x70+cc,0x02,0xB0,0x01); go();
			set8(re,0,taken ? 0 : 1);
			if (!verify(ARITH|CTRL)) failr("%02X jcc forward flags=%04X",0x70+cc,f);

			/* backward: mov al,0; jmp +1; retf; jcc -3; mov al,1; retf */
			setup(); ri[R_FL] = f; EB(9, 0xB0,0x00,0xEB,0x01,0xCB,0x70+cc,0xFD,0xB0,0x01); go();
			set8(re,0,taken ? 0 : 1);
			if (!verify(ARITH|CTRL)) failr("%02X jcc backward flags=%04X",0x70+cc,f);
		}
	}
	end();
}

/* ---- LOOP/LOOPE/LOOPNE/JCXZ */
static void t_loop(void) {
	static const unsigned short cxv[5] = { 1,2,7,0x100,0 };
	unsigned i,n;

	begin("LOOP/JCXZ");

	/* L: inc dx; jnz +1; inc bx; loop L   (BX:DX counts iterations) */
	for (i=0;i < 5;i++) {
		setup(); ri[R_CX] = cxv[i]; ri[R_DX] = 0; ri[R_BX] = 0; EB(6, 0x42,0x75,0x01,0x43,0xE2,0xFA); go();
		n = cxv[i];
		re[R_CX] = 0; re[R_DX] = n; re[R_BX] = (n == 0) ? 1 : 0;
		if (!verify(CTRL)) failr("E2 loop cx=%04X",cxv[i]);
	}

	/* L: inc dx; cmp dx,bx; loope L */
	setup(); ri[R_CX] = 10; ri[R_DX] = 0; ri[R_BX] = 1; EB(5, 0x42,0x3B,0xD3,0xE1,0xFB); go();
	re[R_CX] = 8; re[R_DX] = 2; re[R_FL] = ro[R_FL];
	if (!verify(CTRL)) failr("E1 loope exit on ZF=0");

	setup(); ri[R_CX] = 1; ri[R_DX] = 0; ri[R_BX] = 1; EB(5, 0x42,0x3B,0xD3,0xE1,0xFB); go();
	re[R_CX] = 0; re[R_DX] = 1;
	if (!verify(CTRL)) failr("E1 loope exit on CX=0");

	/* L: inc dx; cmp dx,bx; loopne L */
	setup(); ri[R_CX] = 10; ri[R_DX] = 0; ri[R_BX] = 3; EB(5, 0x42,0x3B,0xD3,0xE0,0xFB); go();
	re[R_CX] = 7; re[R_DX] = 3;
	if (!verify(CTRL)) failr("E0 loopne exit on ZF=1");

	setup(); ri[R_CX] = 2; ri[R_DX] = 0; ri[R_BX] = 3; EB(5, 0x42,0x3B,0xD3,0xE0,0xFB); go();
	re[R_CX] = 0; re[R_DX] = 2;
	if (!verify(CTRL)) failr("E0 loopne exit on CX=0");

	/* LOOP does not change flags */
	setup(); ri[R_CX] = 3; ri[R_FL] = 0x0202|ARITH; EB(2, 0xE2,0xFE); go(); re[R_CX] = 0;
	if (!verify(ARITH|CTRL)) failr("E2 loop $ flags");
	setup(); ri[R_CX] = 3; ri[R_FL] = 0x0202; EB(2, 0xE2,0xFE); go(); re[R_CX] = 0;
	if (!verify(ARITH|CTRL)) failr("E2 loop $ flags");

	/* JCXZ: mov al,0; jcxz +2; mov al,1 */
	for (i=0;i < 5;i++) {
		setup(); ri[R_CX] = cxv[i] ^ 0x8000u; ri[R_FL] = (i & 1) ? (0x0202|ARITH) : 0x0202; if (i == 4) ri[R_CX] = 0;
		EB(6, 0xB0,0x00,0xE3,0x02,0xB0,0x01); go();
		set8(re,0,ri[R_CX] == 0 ? 0 : 1);
		if (!verify(ARITH|CTRL)) failr("E3 jcxz cx=%04X",ri[R_CX]);
	}
	end();
}

/* ---- INT n / IRET */
static void t_int(void) {
	static const unsigned short fv[4] = { 0x0202,0x0203,0x0ED7,0x0AC6 };
	static const unsigned short iv[3] = { 0x0202,0x0ED7 & ~TF,0x0A83 };
	unsigned char __far *p;
	unsigned i,s;

	begin("INT/IRET");
	for (i=0;i < 4;i++) {
		setup(); ri[R_FL] = fv[i]; s = here(); EB(2, 0xCD,0x66); x_clear(); go();
		re[R_AX] = 0x6666; re[R_FL] = fv[i] ^ CF;
		if (!verify(ARITH|CTRL)) failr("CD 66 int 66h, handler sets AX and flips CF");
		if (!checkm(x_cnt == 1 && x_vec == 0x66 && x_ip == s + 2 && x_cs == thunk_seg))
			failm("int 66h: count=%u ip=%04X(%04X) cs=%04X(%04X)",x_cnt,x_ip,s+2,x_cs,thunk_seg);
		if (!checkm(((x_fl ^ fv[i]) & (ARITH|CTRL)) == 0)) failm("int 66h: pushed flags %04X, expected %04X",x_fl,fv[i]);
		if (!checkm((x_live & (TF|IF)) == 0)) failm("int 66h: TF/IF not cleared in handler, flags=%04X",x_live);
	}

	/* iret from a hand built frame: mov ax,flags; push ax; push cs; mov ax,L; push ax; iret; L: pushf; pop ax */
	for (i=0;i < 3;i++) {
		setup(); E(0xB8); EW(iv[i]); EB(3, 0x50,0x0E,0xB8); p = ph16(); EB(2, 0x50,0xCF); fixabs16(p,here()); EB(2, 0x9C,0x58); go();
		re[R_AX] = ro[R_AX]; re[R_FL] = iv[i];
		if (!verify(ARITH|CTRL) || !checkm(((ro[R_AX] ^ iv[i]) & (ARITH|CTRL)) == 0)) failr("CF iret flags=%04X",iv[i]);
	}
	end();
}

/* ---- INT 3 / INTO */
static void t_int3(void) {
	unsigned s;

	begin("INT3/INTO");
	setup(); s = here(); E(0xCC); x_clear(); go();
	if (!verify(ARITH|CTRL) || !checkm(x_cnt == 1 && x_vec == 3 && x_ip == s + 1)) failr("CC int3 count=%u ip=%04X(%04X)",x_cnt,x_ip,s+1);

	setup(); s = here(); EB(2, 0xCD,0x03); x_clear(); go();
	if (!verify(ARITH|CTRL) || !checkm(x_cnt == 1 && x_vec == 3 && x_ip == s + 2)) failr("CD 03 int 3 count=%u ip=%04X(%04X)",x_cnt,x_ip,s+2);

	setup(); ri[R_FL] = 0x0202|OF; s = here(); E(0xCE); x_clear(); go();
	if (!verify(ARITH|CTRL) || !checkm(x_cnt == 1 && x_vec == 4 && x_ip == s + 1)) failr("CE into OF=1 count=%u ip=%04X(%04X)",x_cnt,x_ip,s+1);

	setup(); ri[R_FL] = 0x0202|(ARITH & ~OF); E(0xCE); x_clear(); go();
	if (!verify(ARITH|CTRL) || !checkm(x_cnt == 0)) failr("CE into OF=0 count=%u",x_cnt);
	end();
}

/* ---- single step trap */
static void t_trap(void) {
	unsigned s,i,ok = 1;

	begin("Single step");
	/* xor bx,bx; pushf; pop ax; or ah,1; push ax; popf; inc bx x3; pushf; pop ax; and ah,FEh; push ax; popf */
	setup(); ri[R_FL] = 0x0002;	/* IF=0, no IRQs while stepping */
	EB(9, 0x31,0xDB,0x9C,0x58,0x80,0xCC,0x01,0x50,0x9D);
	s = here();
	EB(3, 0x43,0x43,0x43);
	EB(6, 0x9C,0x58,0x80,0xE4,0xFE,0x50); E(0x9D);
	x_clear(); go();
	re[R_BX] = 3; re[R_AX] = ro[R_AX];
	if (!verify(CTRL)) failr("single step");

	if (!checkm(ss_cnt == 7 || ss_cnt == 8)) { failm("INT 1 count %u, expected 7 or 8",ss_cnt); ok = 0; }
	for (i=0;i < 3 && i < ss_cnt;i++) {
		if (!checkm(ss_ip[i] == s + 1 + i)) { failm("trap %u at IP=%04X, expected %04X",i,ss_ip[i],s+1+i); ok = 0; }
	}
	if (!checkm((x_live & TF) == 0)) { failm("TF set inside INT 1 handler"); ok = 0; }
	if (ok) info("INT 1 taken %u times (%s trap after the POPF that clears TF)",ss_cnt,ss_cnt == 8 ? "with" : "no");
	end();
}

/* ---- flag instructions */
static void t_flagops(void) {
	static const unsigned short fv[4] = { 0x0202,0x0203,0x0602,0x0ED7 };
	unsigned i;

	begin("Flag ops");
	for (i=0;i < 4;i++) {
		setup(); ri[R_FL] = fv[i]; E(0xF8); go(); re[R_FL] = fv[i] & ~CF;
		if (!verify(ARITH|CTRL)) failr("F8 clc");
		setup(); ri[R_FL] = fv[i]; E(0xF9); go(); re[R_FL] = fv[i] | CF;
		if (!verify(ARITH|CTRL)) failr("F9 stc");
		setup(); ri[R_FL] = fv[i]; E(0xF5); go(); re[R_FL] = fv[i] ^ CF;
		if (!verify(ARITH|CTRL)) failr("F5 cmc");
		setup(); ri[R_FL] = fv[i]; E(0xFC); go(); re[R_FL] = fv[i] & ~DF;
		if (!verify(ARITH|CTRL)) failr("FC cld");
		setup(); ri[R_FL] = fv[i]; E(0xFD); go(); re[R_FL] = fv[i] | DF;
		if (!verify(ARITH|CTRL)) failr("FD std");
		setup(); ri[R_FL] = fv[i]; E(0xFA); go(); re[R_FL] = fv[i] & ~IF;
		if (!verify(ARITH|CTRL)) failr("FA cli");
		setup(); ri[R_FL] = fv[i] & ~IF; E(0xFB); go(); re[R_FL] = fv[i] | IF;
		if (!verify(ARITH|CTRL)) failr("FB sti");
	}
	end();
}

/* ---- prefixes on instructions where they have no effect, NOP */
static void t_misc(void) {
	begin("LOCK/NOP");

	setup(); E(0x90); go();
	if (!verify(ARITH|CTRL)) failr("90 nop");

	setup(); memw[0] = 0x1000; ri[R_BX] = memw_off; ri[R_AX] = 0x0234; EB(3, 0xF0,0x01,0x07); go();
	re[R_FL] = 0x0202;
	if (!verify(ARITH|CTRL) || !checkm(memw[0] == 0x1234)) failr("F0 01 07 lock add [bx],ax mem=%04X",memw[0]);

	setup(); memw[0] = 0x1000; ri[R_BX] = memw_off; ri[R_AX] = 0x0234; EB(3, 0xF0,0x87,0x07); go();
	re[R_AX] = 0x1000;
	if (!verify(ARITH|CTRL) || !checkm(memw[0] == 0x0234)) failr("F0 87 07 lock xchg [bx],ax mem=%04X",memw[0]);

	setup(); ri[R_AX] = 0x00FF; EB(2, 0x26,0x40); go();
	re[R_AX] = 0x0100; re[R_FL] = 0x0202 | AF | PF;
	if (!verify(ARITH|CTRL)) failr("26 40 es: inc ax");

	setup(); ri[R_AX] = 0x00FF; EB(3, 0x2E,0x36,0x40); go();
	re[R_AX] = 0x0100; re[R_FL] = 0x0202 | AF | PF;
	if (!verify(ARITH|CTRL)) failr("2E 36 40 cs: ss: inc ax");

	end();
}

/* ================================================================== MAIN */

static int key_wait = 1;

int main(int argc,char **argv) {
	unsigned op,i;
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
	init_parity();

	logf = fopen("8086.TXT","w");

	video_init();
	hook_vectors();

	detect_cpu();
	sprintf(tmp,"8086 CPU instruction test (%s)",pc98 ? "NEC PC-98" : (ibm_mono ? "IBM PC, mono" : "IBM PC, color"));
	put_str(0,0,COL_TITLE,tmp);
	sprintf(tmp,"CPU: %s  [FLAGS 15:12 %s, shift count %s, AAD imm %s]",cpu_name,
		c_flagsF ? "set" : "clear",c_shmask ? "masked" : "unmasked",c_necaad ? "ignored" : "used");
	put_str(0,1,COL_NORM,tmp);
	if (logf) {
		fprintf(logf,"8086 CPU instruction test, %s\n",pc98 ? "NEC PC-98" : "IBM PC");
		fprintf(logf,"%s\n\n",tmp);
		log_commit();
	}

	t_mov_reg();
	t_mov_imm();
	t_mov_mem();
	t_mov_sreg();
	t_lea();
	t_seg_default();
	t_seg_prefix();
	t_seg_wrap();
	t_xchg();
	t_xlat();
	t_ldsles();
	t_pushpop();
	t_push_sp();
	t_pushf();
	t_lahf();
	t_cbw();
	for (op=0;op < 8;op++) t_alu(op);
	t_alu82();
	t_test();
	t_incdec16();
	t_incdec_rm();
	t_neg();
	t_not();
	t_mul(0);
	t_mul(1);
	t_div(0);
	t_div(1);
	t_div_fault();
	t_idiv_min();
	t_daa();
	t_aaa();
	t_aaa_quirk();
	t_aam();
	t_aam_imm();
	t_salc();
	t_shift(SH_ROL);
	t_shift(SH_ROR);
	t_shift(SH_RCL);
	t_shift(SH_RCR);
	t_shift(SH_SHL);
	t_shift(SH_SHR);
	t_shift(SH_SAR);
	t_movs();
	t_lods_stos();
	t_cmps();
	t_scas();
	t_str_seg();
	t_rep_irq();
	t_jmp();
	t_call();
	t_jcc();
	t_loop();
	t_int();
	t_int3();
	t_trap();
	t_flagops();
	t_misc();

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
