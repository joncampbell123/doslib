
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include <conio.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <hw/8254/8254.h>
#include <hw/dos/dosbox.h>

static unsigned char                loop_menu = 0;

static unsigned char                debug_ini = 0;

static char*                        menu_msg = NULL;            // strdup()'d

static const char*                  menu_ini = NULL;            // points at argv[] do not free

static unsigned int                 first_menu_item_y = 0;

static char*                        menu_default_name = NULL;   // strdup(), free at end
static int                          menu_default_timeout = 5;

enum {
    MIT_ITEM=0,
    MIT_INFO
};

struct menuitem {
    char*                           menu_text;                  // strdup()'d
    char*                           menu_item_name;             // strdup()'d
    uint8_t                         type;
};

/* [menu] */
#define MAX_MENU_ITEMS              20

struct menuitem                     menu[MAX_MENU_ITEMS];
int                                 menu_items = 0;
int                                 menu_sel = 0;

struct menuitem* menu_alloc(void) {
    if (menu_items >= MAX_MENU_ITEMS)
        return NULL;

    return &menu[menu_items++];
}

/* [common] */
#define MAX_COMMON_COMMANDS         100

char*                               common_command[MAX_COMMON_COMMANDS];
int                                 common_command_items = 0;

int add_common(const char *s) {
    if (common_command_items >= MAX_COMMON_COMMANDS)
        return -1;

    common_command[common_command_items++] = strdup(s);
    return 0;
}

/* [choice] */
#define MAX_CHOICE_COMMANDS         256

char*                               choice_command[MAX_COMMON_COMMANDS];
unsigned char                       choice_command_menu_item[MAX_COMMON_COMMANDS];  // NTS: Change to unsigned short when we allow >256 menu items
int                                 choice_command_items = 0;

int add_choice(const char *s,unsigned int menu_item) {
    if (choice_command_items >= MAX_COMMON_COMMANDS)
        return -1;

    choice_command_menu_item[choice_command_items] = menu_item;
    choice_command[choice_command_items++] = strdup(s);
    return 0;
}

#define TEMP_SZ                     4096

char*                               temp = NULL;

static int temp_alloc(void) {
    if (temp == NULL)
        temp = malloc(TEMP_SZ + 1);

    return (temp != NULL) ? 0 : -1;
}

static void temp_free(void) {
    if (temp != NULL) {
        free(temp);
        temp = NULL;
    }
}

static void help(void) {
    fprintf(stderr,"DSXMENU [options] <menu INI file>\n");
    fprintf(stderr," -h  --help   /?       Show this help\n");
    fprintf(stderr," -d                    Debug output\n");
}

static int parse_argv(int argc,char **argv) {
    int nonsw = 0;
    char *a;
    int i;

    for (i=1;i < argc;) {
        a = argv[i++];

        if (*a == '-' || *a == '/') {
            do { a++; } while (*a == '-' || *a == '/');

            if (!strcmp(a,"h") || !strcmp(a,"help") || !strcmp(a,"?")) {
                help();
                return -1;
            }
            else if (!strcmp(a,"d")) {
                debug_ini = 1;
            }
            else {
                fprintf(stderr,"Unknown switch '%s'\n",a);
                return -1;
            }
        }
        else {
            switch (nonsw) {
                case 0:
                    menu_ini = a;
                    break;
                default:
                    fprintf(stderr,"Unhandled argument '%s'\n",a);
                    return -1;
            };

            nonsw++;
        }
    }

    if (menu_ini == NULL) {
        fprintf(stderr,"Menu INI file required\n");
        return -1;
    }

    return 0;
}

void cstr_free(char **s) {
    if (s != NULL) {
        if (*s != NULL) {
            free(*s);
            *s = NULL;
        }
    }
}

void cstr_set(char **d,const char *s) {
    cstr_free(d);
    if (s) *d = strdup(s);
}

static void chomp(char *s) {
    char *e = s + strlen(s) - 1;
    while (e >= s && (*e == 13 || *e == 10)) *e-- = 0;
}

enum {
    LOAD_NONE=0,
    LOAD_MENU,
    LOAD_COMMON,
    LOAD_CHOICE
};

void free_menu(void) {
    {
        unsigned int i;
        for (i=0;i < menu_items;i++) {
            struct menuitem *item = &menu[i];
            cstr_free(&item->menu_item_name);
            cstr_free(&item->menu_text);
        }
        menu_items = 0;
    }

    {
        unsigned int i;
        for (i=0;i < common_command_items;i++) {
            cstr_free(&common_command[i]);
        }
        common_command_items = 0;
    }

    {
        unsigned int i;
        for (i=0;i < choice_command_items;i++) {
            cstr_free(&choice_command[i]);
            choice_command_menu_item[i] = 0;
        }
        choice_command_items = 0;
    }

    cstr_free(&menu_msg);
}

int load_menu(void) {
    char *current_section = NULL;
    unsigned int load_choice_index = 0;
    int load_mode = LOAD_NONE;
    char *s = NULL;
    FILE *fp;

    if (menu_ini == NULL) {
        fprintf(stderr,"menu_ini == NULL??\n");
        return 1;
    }

    if (temp_alloc()) {
        fprintf(stderr,"Cannot alloc temp\n");
        return 1;
    }

    fp = fopen(menu_ini,"r");
    if (fp == NULL) {
        fprintf(stderr,"Unable to open file '%s'\n",menu_ini);
        return 1;
    }

    while (!feof(fp) && !ferror(fp)) {
        if (fgets(temp,TEMP_SZ,fp) == NULL) break;
        chomp(temp);

        // lines starting with '#' are comments, Unix style
        s = temp;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '#') continue;
        if (*s == 0) continue;

        // [name] is INI style section header
        if (*s == '[') {
            char *e = strchr(++s,']'); /* step ptr s past the '[' and to the first char, find the ']' */
            if (e) *e = 0; /* eat the ']' if found */

            /* 's' should point to a section header, without brackets */
            cstr_set(&current_section,s);
            assert(current_section != NULL);

            if (!strcmp(current_section,"menu")) {
                load_mode = LOAD_MENU;
            }
            else if (!strcmp(current_section,"common")) {
                load_mode = LOAD_COMMON;
            }
            else {
                load_mode = LOAD_NONE;

                if (*current_section != 0) {
                    unsigned int i=0;

                    while (i < (unsigned int)menu_items) {
                        struct menuitem *item = &menu[i];

                        assert(item->menu_item_name != NULL);

                        if (!strcmp(item->menu_item_name,current_section)) {
                            load_mode = LOAD_CHOICE;
                            load_choice_index = i;
                            break;
                        }

                        i++;
                    }
                }
            }

            continue;
        }

        if (load_mode == LOAD_MENU) {
            /* name=value pairs */
            char *name = s;
            char *value = strchr(name,'=');

            if (value != NULL) {
                *value++ = 0; /* cut out the equals sign */

                if (!strcmp(name,"menuitem")) {
                    /* <choice>,<display text> */
                    char *choice = value;
                    char *display = NULL;

                    while (*value != 0 && *value != ',') value++;
                    if (*value == ',') {
                        *value++ = 0;
                        display = value;
                    }

                    if (display != NULL && *display != 0) {
                        struct menuitem *item = menu_alloc();
                        if (item == NULL) {
                            fprintf(stderr,"Too many menu items\n");
                            break;
                        }

                        item->type = MIT_ITEM;

                        /* if choice is __INFO__ then change to MIT_INFO */
                        if (!strcmp(choice,"__INFO__")) {
                            item->type = MIT_INFO;
                            choice = "";
                        }

                        item->menu_text = strdup(display);
                        item->menu_item_name = strdup(choice);
                    }
                }
                else if (!strcmp(name,"menudefault")) {
                    char *choice = value;
                    char *timeout = NULL;

                    while (*value != 0 && *value != ',') value++;
                    if (*value == ',') {
                        *value++ = 0;
                        timeout = value;
                    }

                    menu_default_timeout = 0;
                    if (timeout != NULL && *timeout != 0)
                        menu_default_timeout = atoi(timeout);

                    if (menu_default_timeout <= 0)
                        menu_default_timeout = 5;

                    cstr_set(&menu_default_name,choice);
                }
                else if (!strcmp(name,"menumessage")) {
                    cstr_set(&menu_msg,value);
                }
                else if (!strcmp(name,"menuloop")) {
                    loop_menu = atoi(value) > 0 ? 1 : 0;
                }
            }
        }
        else if (load_mode == LOAD_COMMON) {
            if (add_common(s)) {
                fprintf(stderr,"Too many common commands\n");
                break;
            }
        }
        else if (load_mode == LOAD_CHOICE) {
            assert(load_choice_index < menu_items);

            if (add_choice(s,load_choice_index)) {
                fprintf(stderr,"Too many choice commands\n");
                break;
            }
        }
    }

    cstr_free(&current_section);

    fclose(fp);
    temp_free();

    /* NTS: Note we allow a blank line */
    if (menu_msg == NULL)
        menu_msg = strdup("Make a selection and hit ENTER");

    if (*menu_msg != 0)
        first_menu_item_y = 2;
    else
        first_menu_item_y = 0;

    menu_sel = 0;
    {
        unsigned int i;
        for (i=0;i < menu_items;i++) {
            struct menuitem *item = &menu[i];

            assert(item->menu_item_name != NULL);
            if (!strcmp(item->menu_item_name,menu_default_name)) {
                menu_sel = i;
                break;
            }
        }
    }

    if (debug_ini) {
        fprintf(stderr,"Menu items:\n");
        {
            unsigned int i;
            for (i=0;i < menu_items;i++) {
                struct menuitem *item = &menu[i];

                assert(item->menu_item_name != NULL);
                assert(item->menu_text != NULL);

                fprintf(stderr," [%d] item='%s' text='%s'\n",i,item->menu_item_name,item->menu_text);
            }
        }

        fprintf(stderr,"Common commands:\n");
        {
            unsigned int i;
            for (i=0;i < common_command_items;i++) {
                char *s = common_command[i];

                assert(s != NULL);

                fprintf(stderr," '%s'\n",s);
            }
        }

        fprintf(stderr,"Choice commands:\n");
        {
            unsigned int i;
            for (i=0;i < choice_command_items;i++) {
                char *s = choice_command[i];
                unsigned int menuidx = choice_command_menu_item[i];

                assert(s != NULL);
                assert(menuidx < menu_items);

                {
                    struct menuitem *item = &menu[menuidx];

                    assert(item->menu_item_name != NULL);

                    fprintf(stderr," For choice '%s': '%s'\n",
                        item->menu_item_name,s);
                }
            }
        }

        if (menu_default_name != NULL)
            fprintf(stderr,"Default item: '%s'\n",menu_default_name);

        fprintf(stderr,"Default timeout: %d seconds\n",menu_default_timeout);
        fprintf(stderr,"Initial menu select: %d\n",menu_sel);
        fprintf(stderr,"Menu message: '%s'\n",menu_msg);
    }

    if (menu_items == 0) {
        fprintf(stderr,"No menu items defined\n");
        return 1;
    }

    return 0;
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

unsigned int            screen_w,screen_h;
#if defined(TARGET_PC98)
VGA_ALPHA_PTR           text_vram = NULL; // 16-bit per cell

static inline VGA_ALPHA_PTR attr_vram(void) {
    return text_vram + 0x1000;  /* A200:0000 (0x1000 x 16-bit) */
}
#else
VGA_ALPHA_PTR           text_vram = NULL; // 16-bit per cell
#endif

void draw_string(unsigned int vramoff,const char *s,unsigned int attrw) {
    unsigned int x = 0;

    while (x < screen_w && *s != 0) {
#if defined(TARGET_PC98)
        text_vram[vramoff+x] = (unsigned char)(*s++);
        attr_vram()[vramoff+x] = attrw;
#else
        text_vram[vramoff+x] = *s++ + attrw;
#endif

        x++;
    }
}

void draw_menu_item(unsigned int idx) {
    if (idx >= menu_items) return;

    {
        struct menuitem *item = &menu[idx];
        unsigned int vramoff = (idx + first_menu_item_y) * screen_w;
        unsigned short attrw;

#if defined(TARGET_PC98)
        if (item->type == MIT_INFO)
            attrw = 0x81; /* green, you should not be able to select these */
        else
            attrw = (idx == menu_sel) ? 0xE5 : 0xE1; /* white, not invisible. use reverse attribute if selected */
#else
        if (item->type == MIT_INFO)
            attrw = 0x0200; /* green, you should not be able to select these */
        else
            attrw = (idx == menu_sel) ? 0x7000 : 0x0700; /* white, use reverse if selected */
#endif

        draw_string(vramoff,item->menu_text,attrw);
    }
}

int run_menu(void) {
    unsigned long timeout;
    unsigned long timeout_disp_update;
    unsigned char doit = 0;
    t8254_time_t pr,cr;

    if (temp_alloc()) return 1;

	write_8254_system_timer(0);

    timeout = (unsigned long)T8254_REF_CLOCK_HZ * (unsigned long)menu_default_timeout;
    timeout_disp_update = timeout;
    cr = read_8254(0);

#if defined(TARGET_PC98)
 #if TARGET_MSDOS == 32
    text_vram = (VGA_ALPHA_PTR)0xA0000;
 #else
    text_vram = (VGA_ALPHA_PTR)MK_FP(0xA000,0x0000);
 #endif

    screen_w = 80;                  // TODO: When would such a system use 40-column mode?
    screen_h = 25;                  // TODO: PC-98 systems have a 20-line text mode too!

    // print escapes to hide the function row and cursor
    printf("\x1B[1>h"); /* hide function row */
    printf("\x1B[5>h"); /* hide cursor (so we can directly control it) */
    fflush(stdout);
#else
    // IBM PC/XT/AT

    // make sure we're in a text mode
	if (!vga_state.vga_alpha_mode) {
        int10_setmode(3);
        update_state_from_vga();
    }

    if (vga_state.vga_alpha_ram == NULL)
        return 1;

    screen_w = read_bda16(0x4A);    // number of columns
    if (screen_w == 0) screen_w = 80;
    screen_h = read_bda8(0x84);     // number of rows (may not exist if BIOS is old enough)
    if (screen_h == 0) screen_h = 24;
    screen_h++;

    text_vram = vga_state.vga_alpha_ram;

    /* use the BIOS to hide the cursor */
    __asm {
        mov     ah,0x01
        mov     cx,0x2000
        int     10h
    }
#endif

    if (debug_ini) {
        fprintf(stderr,"Screen is %u x %u\n",screen_w,screen_h);
#if TARGET_MSDOS == 32
        fprintf(stderr,"Ram ptr %p\n",text_vram);
#else
        fprintf(stderr,"Ram ptr %Fp\n",text_vram);
#endif

#if defined(TARGET_PC98)
 #if TARGET_MSDOS == 32
        fprintf(stderr,"Ram ptr2 %p\n",attr_vram());
 #else
        fprintf(stderr,"Ram ptr2 %Fp\n",attr_vram());
 #endif
#endif
    }

    /* erase background */
    {
        unsigned int x,y,erh,o;

        erh = first_menu_item_y + menu_items + 2;
        if (erh > screen_h) erh = screen_h;

#if defined(TARGET_PC98)
        for (y=0;y < erh;y++) {
            o = screen_w * y;
            for (x=0;x < screen_w;x++) text_vram[o+x] = 0x20;
            for (x=0;x < screen_w;x++) attr_vram()[o+x] = 0x00; /* no color, invisible */
        }
#else
        for (y=0;y < erh;y++) {
            o = screen_w * y;
            for (x=0;x < screen_w;x++) text_vram[o+x] = 0x0720;
        }
#endif
    }

    {
        unsigned int i;
        unsigned char redraw = 1;
        int c;

        do {
            if (timeout != (~0UL)) {
                /* NTS: The 8254 counts DOWN, not up */
                pr = cr;
                cr = read_8254(0);

                /* count down to timeout */
                {
                    unsigned long countdown = (pr - cr) & 0xFFFFUL;
                    if (timeout >= countdown)
                        timeout -= countdown;
                    else
                        timeout = 0;
                }

                if (timeout < timeout_disp_update) {
                    if (timeout_disp_update >= T8254_REF_CLOCK_HZ)
                        timeout_disp_update -= T8254_REF_CLOCK_HZ;
                    else
                        timeout_disp_update = 0;

                    {
                        unsigned int sec = (timeout_disp_update / (unsigned long)T8254_REF_CLOCK_HZ);
                        unsigned short attrw;

#if defined(TARGET_PC98)
                        attrw = 0xA1; /* cyan, not invisible */
#else
                        attrw = 0x0300; /* cyan */
#endif

                        assert(temp != NULL);
                        sprintf(temp,"Will choose default in %u seconds ",sec + 1u);

                        draw_string((first_menu_item_y + menu_items + 1) * screen_w,temp,attrw);
                    }
                }

                if (timeout == 0) {
                    unsigned short attrw;

#if defined(TARGET_PC98)
                    attrw = 0xE1; /* white, not invisible. use reverse attribute if selected */
#else
                    attrw = 0x0700;
#endif

                    draw_string((first_menu_item_y + menu_items + 1) * screen_w,"                                        ",attrw);
                    if (menu_sel >= 0 && menu_sel < menu_items)
                        doit = 1;

                    break;
                }
            }

            if (redraw) {
                if (first_menu_item_y > 0) {
                    unsigned short attrw;

#if defined(TARGET_PC98)
                    attrw = 0xE1; /* white, not invisible. use reverse attribute if selected */
#else
                    attrw = 0x0700;
#endif

                    draw_string(0,menu_msg,attrw);
                }

                for (i=0;i < menu_items;i++)
                    draw_menu_item(i);

                redraw = 0;
            }

            if (kbhit()) {
                c = getch();
#if defined(TARGET_PC98)
# define UPARROW        0x0B
# define DNARROW        0x0A
                // TODO ANSI escapes
#else
# define UPARROW        0x4800
# define DNARROW        0x5000
                if (c == 0) c = getch() << 8;
#endif

                if (c == 27) {
                    break;
                }
                else if (c == UPARROW) {
                    i = menu_sel; 
                    do {
                        if ((--menu_sel) < 0)
                            menu_sel = menu_items - 1;                    
                    } while (menu[menu_sel].type == MIT_INFO);

                    if (i != menu_sel) {
                        draw_menu_item(i);
                        draw_menu_item(menu_sel);
                    }

                    if (timeout != (~0UL)) {
                        unsigned short attrw;

#if defined(TARGET_PC98)
                        attrw = 0xE1; /* white, not invisible. use reverse attribute if selected */
#else
                        attrw = 0x0700;
#endif

                        timeout = ~0UL;
                        draw_string((first_menu_item_y + menu_items + 1) * screen_w,"                                        ",attrw);
                    }
                }
                else if (c == DNARROW) {
                    i = menu_sel;
                    do {
                        if ((++menu_sel) >= menu_items)
                            menu_sel = 0;
                    } while (menu[menu_sel].type == MIT_INFO);

                    if (i != menu_sel) {
                        draw_menu_item(i);
                        draw_menu_item(menu_sel);
                    }

                    if (timeout != (~0UL)) {
                        unsigned short attrw;

#if defined(TARGET_PC98)
                        attrw = 0xE1; /* white, not invisible. use reverse attribute if selected */
#else
                        attrw = 0x0700;
#endif

                        timeout = ~0UL;
                        draw_string((first_menu_item_y + menu_items + 1) * screen_w,"                                        ",attrw);
                    }
                }
                else if (c == 13) {
                    if (menu_sel >= 0 && menu_sel < menu_items) {
                        doit = 1;
                        break;
                    }
                }
            }
        } while(1);
    }

#if defined(TARGET_PC98)
    printf("\n");
    printf("\x1B[1>l"); /* show function row */
    printf("\x1B[5>l"); /* show cursor */
    printf("\x1B[%dB",screen_h); /* put the cursor at the bottom */
    fflush(stdout);
#else
    /* use the BIOS to show the cursor */
    __asm {
        mov     ah,0x01
        mov     cx,0x0607       ; FIXME: Assumes CGA cursor emulation is active. Map to CGA lines 6-7
        int     10h
    }
    /* and then put it at the bottom */
    {
        uint16_t x = (screen_h - 1u) << 8u;

        __asm {
            mov     ah,0x02
            xor     bh,bh
            mov     dx,x
            int     10h
        }
    }
#endif

    if (!doit)
        return 1;

    return 0;
}

void run_commands(void) {
    struct menuitem *item;
    unsigned int i;

    if (menu_sel >= menu_items)
        return;

    item = &menu[menu_sel];
    assert(item->menu_item_name != NULL);

    /* first run common commands */
    for (i=0;i < common_command_items;i++) {
        char *s = common_command[i];
        assert(s != NULL);
        system(s);
    }

    /* then run choice commands */
    for (i=0;i < choice_command_items;i++) {
        if (choice_command_menu_item[i] == menu_sel) {
            char *s = choice_command[i];
            assert(s != NULL);
            system(s);
        }
    }
}

int main(int argc,char **argv,char **envp) {
    if (parse_argv(argc,argv))
        return 1;

#if !defined(TARGET_PC98)
    if (!probe_vga())
        return 1;
#endif

    if (!probe_8254())
		return 1;

    if (load_menu() == 0) {
again:
        if (run_menu() == 0) {
            run_commands();
            if (loop_menu) goto again;
        }
    }

    free_menu();
	return 0;
}

