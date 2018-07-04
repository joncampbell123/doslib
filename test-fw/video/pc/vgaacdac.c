
#include <sys/types.h>
#include <sys/stat.h>
#include <direct.h>
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stdarg.h>
#include <fcntl.h>
#include <math.h>
#include <bios.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>
#include <hw/vga/vgatty.h>
#include <hw/dos/doswin.h>

char tmp[1024];
char log_tmp[1024];
FILE *log_fp = NULL;

unsigned char log_echo = 1;
unsigned char manual_mode = 0xFFu;
unsigned char log_atexit_set = 0;

/* VGA state */
unsigned char st_ac_10;         /* 0x10 Attribute Mode Control */
unsigned char st_ac_14;         /* 0x14 Color Select Register */
unsigned char st_dac_mask;      /* 0x3C6 pel mask */
unsigned char st_ac_pal[16];    /* Attribute palette */

void read_vga_state(void) {
    unsigned int i;

    st_ac_10 = vga_read_AC(0x10);
    st_ac_14 = vga_read_AC(0x14);
    st_dac_mask = inp(0x3C6);
    for (i=0;i < 16;i++) st_ac_pal[i] = vga_read_AC(i);
}

void int10_print(char *s) {
    unsigned int i;

    for (i=0;s[i] != 0;i++) {
        unsigned char cv = s[i];

        __asm {
            mov     ah,0x0E     ; teletype output
            mov     al,cv
            xor     bh,bh
            mov     bl,0x0F     ; foreground color (white)
            int     10h
        }
    }
}

void int10_poscurs(unsigned char y,unsigned char x) {
    __asm {
        mov     ah,0x02     ; set cursor pos
        mov     bh,0x00     ; page 0
        mov     dh,y
        mov     dl,x
        int     10h
    }
}

void print_vga_state(void) {
    /* --------------------------------*/
    sprintf(tmp,"MASK=%u%u%u%u%u%u%u%u",
        (st_dac_mask>>7)&1, (st_dac_mask>>6)&1,
        (st_dac_mask>>5)&1, (st_dac_mask>>4)&1,
        (st_dac_mask>>3)&1, (st_dac_mask>>2)&1,
        (st_dac_mask>>1)&1, (st_dac_mask>>0)&1);

    int10_poscurs(3,18);
    int10_print(tmp);

    /* --------------------------------*/
    sprintf(tmp,"P54S=%s 8BIT=%u PPM=%u",
        (st_ac_10&0x80)?"PAL":"CSR",
        (st_ac_10&0x40)?1:0,
        (st_ac_10&0x20)?1:0);

    int10_poscurs(4,18);
    int10_print(tmp);

    sprintf(tmp,"BLINK=%u LGA=%u ATGE=%u",
        (st_ac_10&0x08)?1:0,
        (st_ac_10&0x04)?1:0,
        (st_ac_10&0x01)?1:0);

    int10_poscurs(5,18);
    int10_print(tmp);

    sprintf(tmp,"CS76=%u CS54=%u",
        (st_ac_14>>2)&3,
        (st_ac_14>>0)&3);

    int10_poscurs(6,18);
    int10_print(tmp);

    sprintf(tmp,"AC: %02x %02x %02x %02x",
        st_ac_pal[0], st_ac_pal[1], st_ac_pal[2], st_ac_pal[3]);
    int10_poscurs(7,18);
    int10_print(tmp);

    sprintf(tmp,"AC: %02x %02x %02x %02x",
        st_ac_pal[4], st_ac_pal[5], st_ac_pal[6], st_ac_pal[7]);
    int10_poscurs(8,18);
    int10_print(tmp);

    sprintf(tmp,"AC: %02x %02x %02x %02x",
        st_ac_pal[8], st_ac_pal[9], st_ac_pal[10],st_ac_pal[11]);
    int10_poscurs(9,18);
    int10_print(tmp);

    sprintf(tmp,"AC: %02x %02x %02x %02x",
        st_ac_pal[12],st_ac_pal[13],st_ac_pal[14],st_ac_pal[15]);
    int10_poscurs(10,18);
    int10_print(tmp);
}

const char log_name[] = "video\\pc\\vgaacdac.log";

void auto_test(unsigned int colors);
void manual_test(unsigned int colors);

void log_noecho(void) {
    log_echo = 0;
}

void log_doecho(void) {
    log_echo = 1;
}

void log_close(void) {
    if (log_fp != NULL) {
        fclose(log_fp);
        log_fp = NULL;
    }
}

void LOG(const char *fmt,...) {
    va_list va;
    int l;

    va_start(va,fmt);
    l = vsnprintf(log_tmp,sizeof(log_tmp),fmt,va);//NTS:I trust snprintf includes the NUL at the end in the string limit!
    va_end(va);

    if (log_fp != NULL)
        fwrite(log_tmp,l,1,log_fp);
    if (log_echo && stdout != NULL) {
        fwrite(log_tmp,l,1,stdout);
        fflush(stdout);
    }
}

#define LOG_DEBUG   "DEBUG> "
#define LOG_INFO    "INFO> "
#define LOG_WARN    "WARN> "
#define LOG_ERROR   "ERROR> "

/* WARNING: We trust the C library will still have working STDIO at this point */
void log_atexit(void) {
    LOG(LOG_INFO "NORMAL EXIT\n");
    log_close();
}

int log_init(void) {
    if (log_fp == NULL) {
        if ((log_fp=fopen(log_name,"w")) == NULL)
            return 0;

        if (!log_atexit_set) {
            log_atexit_set = 1;
            atexit(log_atexit);
        }
    }

    return 1;
}

void log_cpu_info(void) {
	LOG(LOG_INFO "CPU: %s or higher\n",cpu_basic_level_to_string(cpu_basic_level));

	if (cpu_v86_active)
		LOG(LOG_INFO "CPU: Is in virtual 8086 mode\n");
	if (cpu_flags & CPU_FLAG_PROTECTED_MODE)
		LOG(LOG_INFO "CPU: CPU is in protected mode\n");
	if (cpu_flags & CPU_FLAG_PROTECTED_MODE_32)
		LOG(LOG_INFO "CPU: CPU is in 32-bit protected mode\n");
}

void log_dos_info(void) {
	LOG(LOG_INFO "DOS: version %x.%02u\n",dos_version>>8,dos_version&0xFF);
	LOG(LOG_INFO "DOS: Method: '%s'\n",dos_version_method);
	LOG(LOG_INFO "DOS: Flavor: '%s'\n",dos_flavor_str(dos_flavor));
}

void log_windows_info(void) {
    if (windows_mode != WINDOWS_NONE) {
		LOG(LOG_INFO "WINDOWS: Mode is %s\n",windows_mode_str(windows_mode));
		LOG(LOG_INFO "WINDOWS: Version %x.%u\n",windows_version>>8,windows_version&0xFF);
		LOG(LOG_INFO "WINDOWS: Method is %s\n",windows_version_method);
    }
}

void log_vga_info(void) {
    LOG(LOG_INFO "According to VGA library, video hardware is compatible with:\n");
    if (vga_state.vga_flags & VGA_IS_MDA)
        LOG(LOG_INFO "- MDA\n");
    if (vga_state.vga_flags & VGA_IS_CGA)
        LOG(LOG_INFO "- CGA\n");
    if (vga_state.vga_flags & VGA_IS_AMSTRAD)
        LOG(LOG_INFO "- AMSTRAD\n");
    if (vga_state.vga_flags & VGA_IS_TANDY)
        LOG(LOG_INFO "- TANDY\n");
    if (vga_state.vga_flags & VGA_IS_PCJR)
        LOG(LOG_INFO "- PCjr\n");
    if (vga_state.vga_flags & VGA_IS_MCGA)
        LOG(LOG_INFO "- MCGA\n");
    if (vga_state.vga_flags & VGA_IS_HGC)
        LOG(LOG_INFO "- HGC\n");
    if (vga_state.vga_flags & VGA_IS_EGA)
        LOG(LOG_INFO "- EGA\n");
    if (vga_state.vga_flags & VGA_IS_VGA)
        LOG(LOG_INFO "- VGA\n");
}

uint8_t read_bda8(unsigned int ofs) {
#if TARGET_MSDOS == 32
    return *((uint8_t*)(0x400 + ofs));
#else
    return *((uint8_t far*)MK_FP(0x40,ofs));
#endif
}

uint16_t read_bda16(unsigned int ofs) {
#if TARGET_MSDOS == 32
    return *((uint16_t*)(0x400 + ofs));
#else
    return *((uint16_t far*)MK_FP(0x40,ofs));
#endif
}

uint8_t read_int10_bd_mode(void) {
    return read_bda8(0x49);
}

uint16_t int10_bd_read_cga_crt_io(void) {
    return read_bda16(0x63);
}

uint8_t int10_bd_read_cga_mode_byte(void) {
    return read_bda8(0x65);
}

uint16_t int11_info = 0;

int int10_setmode_and_check(uint8_t mode) {
    uint8_t amode;

    int10_setmode(mode);

    if ((amode=read_int10_bd_mode()) != mode) {
        LOG(LOG_WARN "INT 10h mode set 0x%02x failed, BIOS is still in mode 0x%02x according to BIOS data area\n",mode,amode);
        return 0;
    }

    return 1;
}

unsigned int getchex(void) {
    unsigned int c;

    c = getch();
    if (c == 0) c = getch() << 8;

    return c;
}

void LOG_INT11_VIDEOMODE(uint16_t int11) {
    LOG(LOG_INFO "INT 11h video mode: ");
    switch ((int11 >> 4) & 0x3) {
        case 0: LOG("EGA or later"); break; 
        case 1: LOG("CGA color 40x25"); break; 
        case 2: LOG("CGA color 80x25"); break; 
        case 3: LOG("MDA monochrome 80x25"); break; 
    }
    LOG("\n");
}

const char hexes[16] = "0123456789ABCDEF";

int test_pause_ms(unsigned int msecs) {
	const unsigned long delay = t8254_us2ticks(1000UL);
    unsigned int i;

    for (i=0;i < msecs;i++) {
		t8254_wait(delay);
        if (kbhit()) {
            unsigned int c = getchex();
            if (c == ' ') break;
            else if (c == 'x') {
                while (getch() != 13);
                break;
            }
            else if (c == 27)
                return 0;
        }
    }

    return 1;
}

int test_pause_10ths(unsigned int dsecs) {
	const unsigned long delay = t8254_us2ticks(1000UL);
    unsigned int i;

    dsecs *= 100u;
    for (i=0;i < dsecs;i++) {
		t8254_wait(delay);
        if (kbhit()) {
            unsigned int c = getchex();
            if (c == ' ') break;
            else if (c == 'x') {
                while (getch() != 13);
                break;
            }
            else if (c == 27)
                return 0;
        }
    }

    return 1;
}

int test_pause(unsigned int secs) {
	const unsigned long delay = t8254_us2ticks(1000UL);
    unsigned int i;

    secs *= 1000u;
    for (i=0;i < secs;i++) {
		t8254_wait(delay);
        if (kbhit()) {
            unsigned int c = getchex();
            if (c == ' ') break;
            else if (c == 'x') {
                while (getch() != 13);
                break;
            }
            else if (c == 27)
                return 0;
        }
    }

    return 1;
}

VGA_RAM_PTR get_8x8_font(void) {
#if TARGET_MSDOS == 32
    VGA_RAM_PTR p = (VGA_RAM_PTR)0xFFA6E; /* IBM BIOS F000:FA6E font. MUST NOT BE ON STACK BECAUSE OF BP */

    if ((vga_state.vga_flags & (VGA_IS_EGA|VGA_IS_MCGA|VGA_IS_VGA))) {
        struct dpmi_realmode_call rc;
        struct dpmi_realmode_call *p_rc = &rc;

        rc.eax = 0x1130;
        rc.ebx = 0x0300;
        rc.flags = 0;
        rc.es = 0;

        __asm {
            mov	ax,0x0300
            mov	bx,0x0010   ; INT 10h
            xor	cx,cx
            mov	edi,p_rc    ; we trust Watcom has left ES == DS
            int	0x31		; call DPMI
        }

        if (((rc.ebp | rc.es) & 0xFFFFu) != 0u)
            p = (VGA_RAM_PTR)((rc.es << 4u) + (rc.ebp & 0xFFFFu));
    }

    return p;
#else
    static unsigned short s=0xF000,o=0xFA6E; /* IBM BIOS F000:FA6E font. MUST NOT BE ON STACK BECAUSE OF BP */

    if ((vga_state.vga_flags & (VGA_IS_EGA|VGA_IS_MCGA|VGA_IS_VGA))) {
        __asm {
            push    es
            push    bp
            push    cx
            push    dx

            xor     ax,ax
            mov     es,ax
            mov     bp,ax

            mov     ax,0x1130       ; Get Font Information
            mov     bh,0x03         ; ROM 8x8 double dot, 0x00-0x7F
            int     10h             ; returns pointer in ES:BP

            mov     ax,es
            or      ax,bp
            jz      noptr

            mov     s,es
            mov     o,bp

noptr:
            pop     dx
            pop     cx
            pop     bp
            pop     es
        }
    }

    return (VGA_RAM_PTR)MK_FP(s,o);
#endif
}

void mcga2c_test(unsigned int w,unsigned int h) {
    unsigned int i,x,y;
    VGA_RAM_PTR vmem;

    int11_info = _bios_equiplist(); /* IBM PC BIOS equipment list INT 11h */
    LOG(LOG_DEBUG "INT 11h equipment list: 0x%04x\n",int11_info);
    LOG_INT11_VIDEOMODE(int11_info);

    if (((int11_info >> 4) & 3) == 3) // MDA can't do CGA!
        LOG(LOG_WARN "CGA 2-color mode allowed to work despite MDA configuration\n");

#if TARGET_MSDOS == 32
    vmem = (VGA_RAM_PTR)(0xA000u << 4u);
    LOG(LOG_DEBUG "Internal ptr: %p\n",vmem);
#else
    vmem = (VGA_RAM_PTR)MK_FP(0xA000u,0);
    LOG(LOG_DEBUG "Internal ptr: %Fp\n",vmem);
#endif

    test_pause(3);
}

void vga_test(unsigned int w,unsigned int h) {
    unsigned int o,i,x,y;
    unsigned char attrpal[16];
    VGA_RAM_PTR vmem; // do not optimize this code, EGA/VGA planar operations require it

    int11_info = _bios_equiplist(); /* IBM PC BIOS equipment list INT 11h */
    LOG(LOG_DEBUG "INT 11h equipment list: 0x%04x\n",int11_info);
    LOG_INT11_VIDEOMODE(int11_info);

    LOG(LOG_DEBUG "VGA 256-color planar test %u x %u\n",w,h);

    if (((int11_info >> 4) & 3) == 3) // MDA can't do EGA!
        LOG(LOG_WARN "VGA 256-color mode allowed to work despite MDA configuration\n");

#if TARGET_MSDOS == 32
    vmem = (VGA_RAM_PTR)(0xA000u << 4u);
    LOG(LOG_DEBUG "Internal ptr: %p\n",vmem);
#else
    vmem = (VGA_RAM_PTR)MK_FP(0xA000u,0);
    LOG(LOG_DEBUG "Internal ptr: %Fp\n",vmem);
#endif

    {
        uint16_t port = int10_bd_read_cga_crt_io();
        if (port != 0x3D4)
            LOG(LOG_WARN "BIOS CRT I/O port in bios DATA area 0x%x is unusual for this video mode\n",
                port);
    }
 
    test_pause(3);
}

void ega_test(unsigned int w,unsigned int h) {
    unsigned int o,i,x,y;
    unsigned char attrpal[16];
    volatile VGA_RAM_PTR vmem; // do not optimize this code, EGA/VGA planar operations require it

    int11_info = _bios_equiplist(); /* IBM PC BIOS equipment list INT 11h */
    LOG(LOG_DEBUG "INT 11h equipment list: 0x%04x\n",int11_info);
    LOG_INT11_VIDEOMODE(int11_info);

    LOG(LOG_DEBUG "EGA 16-color planar test %u x %u\n",w,h);

    if (((int11_info >> 4) & 3) == 3) // MDA can't do EGA!
        LOG(LOG_WARN "EGA 16-color mode allowed to work despite MDA configuration\n");

#if TARGET_MSDOS == 32
    vmem = (VGA_RAM_PTR)(0xA000u << 4u);
    LOG(LOG_DEBUG "Internal ptr: %p\n",vmem);
#else
    vmem = (VGA_RAM_PTR)MK_FP(0xA000u,0);
    LOG(LOG_DEBUG "Internal ptr: %Fp\n",vmem);
#endif

    {
        uint16_t port = int10_bd_read_cga_crt_io();
        if (port != 0x3D4)
            LOG(LOG_WARN "BIOS CRT I/O port in bios DATA area 0x%x is unusual for this video mode\n",
                port);
    }
 
    test_pause(3);
}

void cga4_test(unsigned int w,unsigned int h) {
    unsigned int i,x,y;
    VGA_RAM_PTR vmem;

    int11_info = _bios_equiplist(); /* IBM PC BIOS equipment list INT 11h */
    LOG(LOG_DEBUG "INT 11h equipment list: 0x%04x\n",int11_info);
    LOG_INT11_VIDEOMODE(int11_info);

    if (((int11_info >> 4) & 3) == 3) // MDA can't do CGA!
        LOG(LOG_WARN "CGA 4-color mode allowed to work despite MDA configuration\n");

#if TARGET_MSDOS == 32
    vmem = (VGA_RAM_PTR)(0xB800u << 4u);
    LOG(LOG_DEBUG "Internal ptr: %p\n",vmem);
#else
    vmem = (VGA_RAM_PTR)MK_FP(0xB800u,0);
    LOG(LOG_DEBUG "Internal ptr: %Fp\n",vmem);
#endif

    {
        uint16_t port = int10_bd_read_cga_crt_io();
        if (port != 0x3D4)
            LOG(LOG_WARN "BIOS CRT I/O port in bios DATA area 0x%x is unusual for this video mode\n",
                port);
    }

    test_pause(3);
}

void cga2_test(unsigned int w,unsigned int h) {
    unsigned int i,x,y;
    VGA_RAM_PTR vmem;

    int11_info = _bios_equiplist(); /* IBM PC BIOS equipment list INT 11h */
    LOG(LOG_DEBUG "INT 11h equipment list: 0x%04x\n",int11_info);
    LOG_INT11_VIDEOMODE(int11_info);

    if (((int11_info >> 4) & 3) == 3) // MDA can't do CGA!
        LOG(LOG_WARN "CGA 2-color mode allowed to work despite MDA configuration\n");

#if TARGET_MSDOS == 32
    vmem = (VGA_RAM_PTR)(0xB800u << 4u);
    LOG(LOG_DEBUG "Internal ptr: %p\n",vmem);
#else
    vmem = (VGA_RAM_PTR)MK_FP(0xB800u,0);
    LOG(LOG_DEBUG "Internal ptr: %Fp\n",vmem);
#endif

    {
        uint16_t port = int10_bd_read_cga_crt_io();
        if (port != 0x3D4)
            LOG(LOG_WARN "BIOS CRT I/O port in bios DATA area 0x%x is unusual for this video mode\n",
                port);
    }

    test_pause(3);
}

void alphanumeric_test(unsigned int w,unsigned int h) {
    unsigned int o=0,i,x,y;
    VGA_ALPHA_PTR vmem;
    uint16_t crtbase;
    uint16_t sv;

    // NTS: The PCjr puts the CGA "mode register" in 0x3DA register 0 instead of 0x3D8.
    //      We have to write it directly to control blinking. INT 10h does not provide
    //      the blink control function on PCjr.

    // real hardware vs emulation check:
    //
    // If DOSBox emulation is correct, Tandy/PCjr palette registers have no effect in text mode.
    // Is that true of real hardware?

    int11_info = _bios_equiplist(); /* IBM PC BIOS equipment list INT 11h */
    LOG(LOG_DEBUG "INT 11h equipment list: 0x%04x\n",int11_info);
    LOG_INT11_VIDEOMODE(int11_info);

    /* check columns */
    LOG(LOG_DEBUG "BIOS data area reports %u columns per row\n",read_bda16(0x4A));
    if (read_bda16(0x4A) != w)
        LOG(LOG_WARN "BIOS data area reports different columns/row than our expected %u\n",w);

    /* check rows.
     * I noticed from the 1984 edition of the IBM hardware reference that
     * byte 0x84 in the BDA didn't exist until the 1986 reference,
     * therefore it's best not to check unless EGA/MCGA or better. */
    LOG(LOG_DEBUG "BIOS data area reports %u rows (may not exist if BIOS is old enough)\n",read_bda8(0x84)+1);
    if ((vga_state.vga_flags & (VGA_IS_EGA|VGA_IS_MCGA|VGA_IS_VGA)) != 0) {
        y = read_bda8(0x84);
        if (y != 0 && (y+1) != h)
            LOG(LOG_WARN "BIOS data area reports different row count than our expected %u\n",h);
    }

    /* MDA monochrome mode means the text VRAM is at B000:0000
     * Otherwise, the text VRAM is at B800:0000
     * BUT on EGA/VGA despite this equipment check the segment is B000 */
    if (((int11_info >> 4) & 3) == 3) {//MDA
        crtbase = 0x3B4;
        sv = 0xB000u;
    }
    else {
        crtbase = 0x3D4;
        sv = 0xB800u;
    }

    if ((vga_state.vga_flags & (VGA_IS_EGA|VGA_IS_MCGA|VGA_IS_VGA)) != 0 && read_int10_bd_mode() == 7) {
        LOG(LOG_DEBUG "But this is EGA/VGA/MCGA and INT 10h mode 7, therefore monochrome mode\n");
        crtbase = 0x3B4;
        sv = 0xB000u;
    }

    /* did we determine the CRT base I/O port correctly?
     * check the BIOS data area.
     * EGA/VGA seems to emulate this correctly. */
    {
        uint16_t port = int10_bd_read_cga_crt_io();
        if (port != crtbase)
            LOG(LOG_WARN "BIOS says CRT I/O port is 0x%x, my guess was 0x%x. This might be a problem.\n",
                port,crtbase);

        if (port != 0x3D4 && port != 0x3B4)
            LOG(LOG_WARN "BIOS CRT I/O port in bios DATA area 0x%x is unusual.\n",
                port);
    }

    LOG(LOG_INFO "Therefore, using video RAM segment 0x%04x\n",sv);
#if TARGET_MSDOS == 32
    vmem = (VGA_ALPHA_PTR)(sv << 4u);
    LOG(LOG_DEBUG "Internal ptr: %p\n",vmem);
#else
    vmem = (VGA_ALPHA_PTR)MK_FP(sv,0);
    LOG(LOG_DEBUG "Internal ptr: %Fp\n",vmem);
#endif

    sprintf(tmp,"%ux%u seg %xh text, mode %02xh",w,h,sv,read_int10_bd_mode());
    for (i=0;tmp[i] != 0;i++)
        vmem[i] = 0x0700 + ((unsigned char)tmp[i]);

    for (x=0;x < 16;x++)
        vmem[2*w+x+1] = hexes[x] + 0x7000;
    for (y=0;y < 16;y++)
        vmem[(y+3)*w] = hexes[y] + 0x7000;

    for (y=0;y < 16;y++) {
        o=(y+3)*w+1;
        for (x=0;x < 16;x++) {
            vmem[o+x] = (y * 0x1000) + (x * 0x0100) + 'A';
        }
    }

    if (manual_mode == 0xFF)
        auto_test(16);
    else
        manual_test(16);
}

void ac_ramp(void) {
    unsigned int i;

    for (i=0;i < 16;i++) vga_write_AC(i,i);
    vga_write_AC(VGA_AC_ENABLE|0x1F,0);
}

/* palette ramps appropriate for 16-color modes */
void dac_ramps16(void) {
    unsigned int i,j,sj;
    unsigned char r,g,b;
    unsigned char rm,gm,bm;

    outp(0x3C8,0);
    for (i=0;i < 16;i++) {
        rm = ((i&3) == 0) || ((i&3) == 1);
        gm = ((i&3) == 0) || ((i&3) == 2);
        bm = ((i&3) == 0) || ((i&3) == 3);
        for (j=0;j < 16;j++) {
            sj = (j << 2) + (j >> 2); /* equiv sj = j * 63 / 15 */
            r = rm ? sj : 0;
            g = gm ? sj : 0;
            b = bm ? sj : 0;
            r >>= (i >> 2u);
            g >>= (i >> 2u);
            b >>= (i >> 2u);
            outp(0x3C9,r);
            outp(0x3C9,g);
            outp(0x3C9,b);
        }
    }
}

void manual_test(unsigned int colors) {
    ac_ramp();

    if (colors == 16)
        dac_ramps16();

    read_vga_state();
    print_vga_state();

    test_pause(3);
}

void auto_test(unsigned int colors) {
    test_pause(1);
}

int main(int argc,char **argv) {
    int i;
    char *a;

    for (i=1;i < argc;) {
        a = argv[i++];

        if (!strcmp(a,"-m")) {
            a = argv[i++];
            if (a == NULL) return 1;
            manual_mode = strtoul(a,NULL,0);;
        }
        else {
            fprintf(stderr,"Unknown arg %s\n",a);
            return 1;
        }
    }

    mkdir("video");
    mkdir("video\\pc");
    if (!log_init()) {
        printf("Unable to open log file %s\n",log_name);
        return 1;
    }

    LOG(LOG_INFO "VIDEO\\PC\\VGAACDAC: VGA Attribute Controller / DAC tests\n");

    /* Expected environment: Pure DOS mode (not Windows or OS/2).
     *                       User may run us under EMM386.EXE or not.
     *                       CPU is expected to be 8086 or higher. */
    cpu_probe();
    log_cpu_info();

	probe_dos();
    log_dos_info();

	detect_windows();
    log_windows_info();

    if (windows_mode != WINDOWS_NONE) {
        printf("This test requires pure DOS mode. Do not run under Windows or OS/2.\n");
        return 1;
    }

	if (!probe_8254()) {
		printf("8254 not found (I need this for time-sensitive portions of the driver)\n");
		return 1;
	}

	if (!probe_vga()) {
		printf("Video probe failed\n");
		return 1;
	}

    log_vga_info();

    // additional debug info
    {
        uint16_t bv=0,cv=0;

        __asm {
            mov     ah,0x12             ; Alternate function select - Get EGA INFO
            mov     bx,0xFF10
            int     10h
            mov     bv,bx
            mov     cv,cx
        }

        LOG(LOG_DEBUG "INT 10h AH=0x12 BL=0x10 Get EGA INFO results:\n"
            LOG_DEBUG "- BH(video state)=0x%02x\n"
            LOG_DEBUG "- BL(installed memory in 64KB)=0x%02x\n"
            LOG_DEBUG "- CH(feature connector bits)=0x%02x\n"
            LOG_DEBUG "- CL(switch settings)=0x%02x\n",
            bv>>8u,bv&0xFFu,
            cv>>8u,cv&0xFFu);

        __asm {
            mov     ax,0x1A00
            xor     bx,bx
            int     10h
            mov     bv,bx
            mov     cv,cx
        }

        LOG(LOG_DEBUG "INT 10h AX=0x1A00 Get Display Combination Code:\n"
            LOG_DEBUG "- BL(active display code)=0x%02x\n"
            LOG_DEBUG "- BH(alternate display code)=0x%02x\n",
            bv&0xFFu,bv>>8u);
    }

    /* NTS: Cannot use INT 10h to print in graphics mode, because INT 10h does not
     *      know or understand the HGC graphics mode. */
    {
        VGA_RAM_PTR font8 = get_8x8_font();

#if TARGET_MSDOS == 32
        LOG(LOG_DEBUG "Font ptr: %p\n",font8);
#else
        LOG(LOG_DEBUG "Font ptr: %Fp\n",font8);
#endif
    }

    if (!(vga_state.vga_flags & (VGA_IS_VGA))) {
        LOG(LOG_ERROR "This test program requires VGA\n");
        return 1;
    }

	_cli();
	write_8254_system_timer(0); // 18.2
	_sti();

    /* basic pattern tests */
    /* we need the screen */
    log_noecho();

    if (manual_mode == 0xFF || manual_mode == 0) {
        LOG(LOG_INFO "Testing: INT 10h mode 0 40x25 mono text mode\n");
        if (int10_setmode_and_check(0))// will LOG if mode set failure
            alphanumeric_test(40,25); // should be 40x25
    }

    if (manual_mode == 0xFF || manual_mode == 1) {
        LOG(LOG_INFO "Testing: INT 10h mode 1 40x25 color text mode\n");
        if (int10_setmode_and_check(1))// will LOG if mode set failure
            alphanumeric_test(40,25); // should be 40x25
    }

    if (manual_mode == 0xFF || manual_mode == 2) {
        LOG(LOG_INFO "Testing: INT 10h mode 2 80x25 mono text mode\n");
        if (int10_setmode_and_check(2))// will LOG if mode set failure
            alphanumeric_test(80,25); // should be 40x25
    }

    if (manual_mode == 0xFF || manual_mode == 3) {
        LOG(LOG_INFO "Testing: INT 10h mode 3 80x25 color text mode\n");
        if (int10_setmode_and_check(3))// will LOG if mode set failure
            alphanumeric_test(80,25); // should be 40x25
    }

    if (manual_mode == 0xFF || manual_mode == 7) {
        LOG(LOG_INFO "Testing: INT 10h mode 7 80x25 mono text mode\n");
        if (int10_setmode_and_check(7))// will LOG if mode set failure
            alphanumeric_test(80,25); // should be 40x25
    }

    if (manual_mode == 0xFF || manual_mode == 4) {
        LOG(LOG_INFO "Testing: INT 10h mode 4 320x200 CGA color 4-color graphics mode\n");
        if (int10_setmode_and_check(4))// will LOG if mode set failure
            cga4_test(320,200);
    }

    if (manual_mode == 0xFF || manual_mode == 5) {
        LOG(LOG_INFO "Testing: INT 10h mode 5 320x200 CGA mono 4-color graphics mode\n");
        if (int10_setmode_and_check(5))// will LOG if mode set failure
            cga4_test(320,200);
    }

    if (manual_mode == 0xFF || manual_mode == 6) {
        LOG(LOG_INFO "Testing: INT 10h mode 6 640x200 CGA mono 2-color graphics mode\n");
        if (int10_setmode_and_check(6))// will LOG if mode set failure
            cga2_test(640,200);
    }

    if (manual_mode == 0xFF || manual_mode == 13) {
        LOG(LOG_INFO "Testing: INT 10h mode 13 320x200 EGA 16-color graphics mode\n");
        if (int10_setmode_and_check(13))// will LOG if mode set failure
            ega_test(320,200);
    }

    if (manual_mode == 0xFF || manual_mode == 14) {
        LOG(LOG_INFO "Testing: INT 10h mode 14 640x200 EGA 16-color graphics mode\n");
        if (int10_setmode_and_check(14))// will LOG if mode set failure
            ega_test(640,200);
    }

    if (manual_mode == 0xFF || manual_mode == 16) {
        LOG(LOG_INFO "Testing: INT 10h mode 16 640x350 EGA 16-color graphics mode\n");
        if (int10_setmode_and_check(16))// will LOG if mode set failure
            ega_test(640,350);
    }

    if (manual_mode == 0xFF || manual_mode == 18) {
        LOG(LOG_INFO "Testing: INT 10h mode 18 640x480 VGA 16-color graphics mode\n");
        if (int10_setmode_and_check(18))// will LOG if mode set failure
            ega_test(640,480);
    }

    if (manual_mode == 0xFF || manual_mode == 19) {
        LOG(LOG_INFO "Testing: INT 10h mode 19 320x200 VGA 256-color graphics mode\n");
        if (int10_setmode_and_check(19))// will LOG if mode set failure
            vga_test(320,200);
    }

    if (manual_mode == 0xFF || manual_mode == 15) {
        LOG(LOG_INFO "Testing: INT 10h mode 15 640x350 EGA 2-color graphics mode\n");
        if (int10_setmode_and_check(15))// will LOG if mode set failure
            mcga2c_test(640,350);
    }

    if (manual_mode == 0xFF || manual_mode == 17) {
        LOG(LOG_INFO "Testing: INT 10h mode 17 640x480 MCGA 2-color graphics mode\n");
        if (int10_setmode_and_check(17))// will LOG if mode set failure
            mcga2c_test(640,480);
    }

    /* set back to mode 3 80x25 text */
    int10_setmode(3);
    log_doecho();
    LOG(LOG_DEBUG "Restoring 80x25 text mode\n");

	return 0;
}

