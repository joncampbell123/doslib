
#include <sys/types.h>
#include <sys/stat.h>
#include <direct.h>
#include <stdio.h>
#include <ctype.h>
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
unsigned char st_gc_05;         /* Graphics controller mode register (0x05) */

void read_vga_state(void) {
    unsigned int i;

    st_gc_05 = vga_read_GC(0x05);
    st_ac_10 = vga_read_AC(0x10);
    st_ac_14 = vga_read_AC(0x14);
    st_dac_mask = inp(0x3C6);
    for (i=0;i < 16;i++) st_ac_pal[i] = vga_read_AC(i);
}

void int10_print(char *s,unsigned char c) {
    unsigned int i;

    for (i=0;s[i] != 0;i++) {
        unsigned char cv = s[i];

        __asm {
            mov     ah,0x0E     ; teletype output
            mov     al,cv
            xor     bh,bh
            mov     bl,c
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

unsigned char       vga_entry_sel = 0;

enum {
    VGAENT_MASK=0,
    VGAENT_P54S,
    VGAENT_8BIT,
    VGAENT_PPM,
    VGAENT_BLINK,
    VGAENT_LGA,
    VGAENT_ATGE,
    VGAENT_CS76,
    VGAENT_CS54,
    VGAENT_MONO,
    VGAENT_SH256,
    VGAENT_SRI,
    VGAENT_HOE,
    VGAENT_AC0,
    VGAENT_AC1,
    VGAENT_AC2,
    VGAENT_AC3,
    VGAENT_AC4,
    VGAENT_AC5,
    VGAENT_AC6,
    VGAENT_AC7,
    VGAENT_AC8,
    VGAENT_AC9,
    VGAENT_AC10,
    VGAENT_AC11,
    VGAENT_AC12,
    VGAENT_AC13,
    VGAENT_AC14,
    VGAENT_AC15,

    VGAENT_MAX
};

void vga_entry_next(void) {
    if (++vga_entry_sel == VGAENT_MAX)
        vga_entry_sel = 0;
}

void vga_entry_prev(void) {
    if (vga_entry_sel == 0)
        vga_entry_sel = VGAENT_MAX;

    vga_entry_sel--;
}

void print_vga_state(void) {
    /* --------------------------------*/
    sprintf(tmp,"%cMASK=%u%u%u%u%u%u%u%u",
        vga_entry_sel==VGAENT_MASK?0x1A:' ',
        (st_dac_mask>>7)&1, (st_dac_mask>>6)&1,
        (st_dac_mask>>5)&1, (st_dac_mask>>4)&1,
        (st_dac_mask>>3)&1, (st_dac_mask>>2)&1,
        (st_dac_mask>>1)&1, (st_dac_mask>>0)&1);

    int10_poscurs(3,17);
    int10_print(tmp,0x3F);

    /* --------------------------------*/
    sprintf(tmp,"%cP54S=%s%c8BIT=%u%cPPM=%u",
        vga_entry_sel==VGAENT_P54S?0x1A:' ',
        (st_ac_10&0x80)?"CSR":"PAL",
        vga_entry_sel==VGAENT_8BIT?0x1A:' ',
        (st_ac_10&0x40)?1:0,
        vga_entry_sel==VGAENT_PPM?0x1A:' ',
        (st_ac_10&0x20)?1:0);

    int10_poscurs(4,17);
    int10_print(tmp,0x3F);

    sprintf(tmp,"%cBLINK=%u%cLGA=%u%cATGE=%u",
        vga_entry_sel==VGAENT_BLINK?0x1A:' ',
        (st_ac_10&0x08)?1:0,
        vga_entry_sel==VGAENT_LGA?0x1A:' ',
        (st_ac_10&0x04)?1:0,
        vga_entry_sel==VGAENT_ATGE?0x1A:' ',
        (st_ac_10&0x01)?1:0);

    int10_poscurs(5,17);
    int10_print(tmp,0x3F);

    sprintf(tmp,"%cCS76=%u%cCS54=%u%cMONO=%u",
        vga_entry_sel==VGAENT_CS76?0x1A:' ',
        (st_ac_14>>2)&3,
        vga_entry_sel==VGAENT_CS54?0x1A:' ',
        (st_ac_14>>0)&3,
        vga_entry_sel==VGAENT_MONO?0x1A:' ',
        (st_ac_10>>1)&1);

    int10_poscurs(6,17);
    int10_print(tmp,0x3F);

    sprintf(tmp,"%cSH256=%u%cSRI=%u%cHOE=%u",
        vga_entry_sel==VGAENT_SH256?0x1A:' ',
        (st_gc_05&0x40)?1:0,
        vga_entry_sel==VGAENT_SRI?0x1A:' ',
        (st_gc_05&0x20)?1:0,
        vga_entry_sel==VGAENT_HOE?0x1A:' ',
        (st_gc_05&0x10)?1:0);

    int10_poscurs(7,17);
    int10_print(tmp,0x3F);

    sprintf(tmp,"AC:%c%02x%c%02x%c%02x%c%02x",
        vga_entry_sel==VGAENT_AC0?0x1A:' ', st_ac_pal[0],
        vga_entry_sel==VGAENT_AC1?0x1A:' ', st_ac_pal[1],
        vga_entry_sel==VGAENT_AC2?0x1A:' ', st_ac_pal[2],
        vga_entry_sel==VGAENT_AC3?0x1A:' ', st_ac_pal[3]);
    int10_poscurs(8,18);
    int10_print(tmp,0x3F);

    sprintf(tmp,"AC:%c%02x%c%02x%c%02x%c%02x",
        vga_entry_sel==VGAENT_AC4?0x1A:' ', st_ac_pal[4],
        vga_entry_sel==VGAENT_AC5?0x1A:' ', st_ac_pal[5],
        vga_entry_sel==VGAENT_AC6?0x1A:' ', st_ac_pal[6],
        vga_entry_sel==VGAENT_AC7?0x1A:' ', st_ac_pal[7]);
    int10_poscurs(9,18);
    int10_print(tmp,0x3F);

    sprintf(tmp,"AC:%c%02x%c%02x%c%02x%c%02x",
        vga_entry_sel==VGAENT_AC8?0x1A:' ', st_ac_pal[8],
        vga_entry_sel==VGAENT_AC9?0x1A:' ', st_ac_pal[9],
        vga_entry_sel==VGAENT_AC10?0x1A:' ', st_ac_pal[10],
        vga_entry_sel==VGAENT_AC11?0x1A:' ', st_ac_pal[11]);
    int10_poscurs(10,18);
    int10_print(tmp,0x3F);

    sprintf(tmp,"AC:%c%02x%c%02x%c%02x%c%02x",
        vga_entry_sel==VGAENT_AC12?0x1A:' ', st_ac_pal[12],
        vga_entry_sel==VGAENT_AC13?0x1A:' ', st_ac_pal[13],
        vga_entry_sel==VGAENT_AC14?0x1A:' ', st_ac_pal[14],
        vga_entry_sel==VGAENT_AC15?0x1A:' ', st_ac_pal[15]);
    int10_poscurs(11,18);
    int10_print(tmp,0x3F);
}

const char log_name[] = "video\\pc\\vga256b4.log";

void test(unsigned int cols);
void auto_test(unsigned int colors);

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

    /* EGA/VGA can relocate registers between 3Bxh and 3Dxh */
    if ((vga_state.vga_flags & (VGA_IS_EGA|VGA_IS_VGA)))
        update_state_from_vga();

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

void vga_test(unsigned int w,unsigned int h) {
    unsigned int o,x,y;
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

    sprintf(tmp,"%ux%u seg %xh VGA, mode %02xh",w,h,0xA000,read_int10_bd_mode());
    int10_poscurs(0,0);
    int10_print(tmp,0x3F);

    for (y=0;y < 32;y++) {
        o=(y+24)*w+(1*2);
        for (x=0;x < 32;x++) {
            vmem[o++] = ((y>>1u)<<4)+(x>>1u);
        }
    }

    {
        VGA_RAM_PTR font = get_8x8_font();
        const char *message1 = "This.is.written,.top.nibble--";
        const char *message2 = "Bottom.nibble.this.is.written";
        unsigned char c1,c2,color1,color2;
        unsigned int o1 = (320*140)+0;
        unsigned int o2 = (320*160)+0;
        unsigned int o = (320*120)+0;
        const char *s1 = message1;
        const char *s2 = message2;
        unsigned int x,y;
        VGA_RAM_PTR fb1;
        VGA_RAM_PTR fb2;

        color1 = 0x40;
        color2 = 0x04;
        while (1) {
            c1 = (unsigned char)(*s1++);
            c2 = (unsigned char)(*s2++);
            if (c1 == 0 || c2 == 0) break;

            fb1 = font + ((unsigned int)c1 * 8u);
            fb2 = font + ((unsigned int)c2 * 8u);
            for (y=0;y < 8;y++) {
                unsigned char b1 = *fb1++;
                unsigned char b2 = *fb2++;

                for (x=0;x < 8;x++,b1 <<= 1u,b2 <<= 1u) {
                    vmem[o +x] = ((b1 & 0x80) ? color1 : 0x00) + ((b2 & 0x80) ? color2 : 0x00);
                    vmem[o1+x] = ((b1 & 0x80) ? color1 : 0x00);
                    vmem[o2+x] =                                 ((b2 & 0x80) ? color2 : 0x00);
                }

                o2 += 320;
                o1 += 320;
                o += 320;
            }

            color1 += 0x10;
            if (color1 == 0x00) color1 = 0x40;

            color2 += 0x01;
            if (color2 == 0x10) color2 = 0x04;

            o2 -= 320*8 - 8;
            o1 -= 320*8 - 8;
            o -= 320*8 - 8;
       }
    }

    test(256);
}

void ac_ramp(void) {
    unsigned int i;

    for (i=0;i < 16;i++) vga_write_AC(i,i);
    vga_write_AC(VGA_AC_ENABLE|0x1F,0);
}

/* palette ramps appropriate for 256-color modes */
void dac_ramps256(void) {
    unsigned int i,j;
    unsigned char r,g,b;

    outp(0x3C8,0);
    for (i=0;i < 16;i++) {
        for (j=0;j < 16;j++) {
            r = ((i * 63) / 15);
            b = ((j * 63) / 15);
            g = 0;
            outp(0x3C9,r);
            outp(0x3C9,g);
            outp(0x3C9,b);
        }
    }
}

void st_ac_10_toggle(unsigned char b) {
    st_ac_10 ^= b;
    vga_write_AC(0x10,st_ac_10);
    vga_write_AC(VGA_AC_ENABLE|0x10,st_ac_10);
}

void st_ac_10_set(unsigned char i,unsigned char m) {
    st_ac_10 = (st_ac_10 & ~m) + (i & m);
    vga_write_AC(0x10,st_ac_10);
    vga_write_AC(VGA_AC_ENABLE|0x10,st_ac_10);
}

void st_ac_14_inc(unsigned char i,unsigned char m) {
    st_ac_14 = (st_ac_14 & ~m) + ((st_ac_14 + i) & m);
    vga_write_AC(0x14,st_ac_14);
    vga_write_AC(VGA_AC_ENABLE|0x14,st_ac_14);
}

void st_ac_14_set(unsigned char i,unsigned char m) {
    st_ac_14 = (st_ac_14 & ~m) + (i & m);
    vga_write_AC(0x14,st_ac_14);
    vga_write_AC(VGA_AC_ENABLE|0x14,st_ac_14);
}

void st_gc_05_toggle(unsigned char b) {
    st_gc_05 ^= b;
    vga_write_GC(0x05,st_gc_05);
}

void st_dac_mask_toggle(unsigned char b) {
    st_dac_mask ^= b;
    outp(0x3C6,st_dac_mask);
}

void update_ac_pal(unsigned char i) {
    vga_write_AC(i,              st_ac_pal[i]);
    vga_write_AC(i|VGA_AC_ENABLE,st_ac_pal[i]);
}

void test(unsigned int colors) {
    outp(0x3C6,0xFF);
    ac_ramp();

    dac_ramps256();

    read_vga_state();
    print_vga_state();

    auto_test(colors);
}

void auto_test(unsigned int colors) {
    while (1) {
        st_ac_10_toggle(0x40);
        print_vga_state();

        if (!test_pause(3))
            break;
    }
}

int main(int argc,char **argv) {
    int i;
    char *a;

    for (i=1;i < argc;) {
        a = argv[i++];

        {
            fprintf(stderr,"Unknown arg %s\n",a);
            return 1;
        }
    }

    vga_entry_sel = VGAENT_MAX;

    mkdir("video");
    mkdir("video\\pc");
    if (!log_init()) {
        printf("Unable to open log file %s\n",log_name);
        return 1;
    }

    LOG(LOG_INFO "VIDEO\\PC\\VGA256B4: VGA Attribute Controller / DAC test: 256-color shift register test\n");
    LOG(LOG_INFO "VIDEO\\PC\\VGA256B4: This test is written for original IBM VGA hardware, which appears\n");
    LOG(LOG_INFO "VIDEO\\PC\\VGA256B4: to render 256-color mode in hardware as a nibble-wise (4-bit) shift\n");
    LOG(LOG_INFO "VIDEO\\PC\\VGA256B4: operation. Setting mode 0x13 and then switching off the 8BIT bit\n");
    LOG(LOG_INFO "VIDEO\\PC\\VGA256B4: is very telling. This program might not do anything different on\n");
    LOG(LOG_INFO "VIDEO\\PC\\VGA256B4: non-IBM hardware.\n");

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

    /* do 320x200x256 FIRST to make the full color palette apparent to the user */
    LOG(LOG_INFO "Testing: INT 10h mode 19 320x200 VGA 256-color graphics mode\n");
    if (int10_setmode_and_check(19))// will LOG if mode set failure
        vga_test(320,200);

    /* set back to mode 3 80x25 text */
    int10_setmode(3);
    log_doecho();
    LOG(LOG_DEBUG "Restoring 80x25 text mode\n");

	return 0;
}

