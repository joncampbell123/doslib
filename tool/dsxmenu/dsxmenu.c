
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
#include <hw/dos/dosbox.h>

static unsigned char                debug_ini = 0;

static const char*                  menu_ini = NULL;            // points at argv[] do not free

static char*                        menu_default_name = NULL;   // strdup(), free at end
static int                          menu_default_timeout = 5;

struct menuitem {
    char*                           menu_text;                  // strdup()'d
    char*                           menu_item_name;             // strdup()'d
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

        if (menu_default_name != NULL) {
            fprintf(stderr,"Default item: '%s'\n",menu_default_name);

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
        }
        fprintf(stderr,"Default timeout: %d seconds\n",menu_default_timeout);
        fprintf(stderr,"Initial menu select: %d\n",menu_sel);
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

void draw_menu_item(unsigned int idx) {
    if (idx >= menu_items) return;

    {
        struct menuitem *item = &menu[idx];
        unsigned int vramoff = idx * screen_w;
        const char *s = item->menu_text;
        unsigned short attrw;
        unsigned int x = 0;

#if defined(TARGET_PC98)
        attrw = (idx == menu_sel) ? 0xE5 : 0xE1; /* white, not invisible. use reverse attribute if selected */
#else
        attrw = (idx == menu_sel) ? 0x7000 : 0x0700;
#endif

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
}

int run_menu(void) {
#if defined(TARGET_PC98)
 #if TARGET_MSDOS == 32
    text_vram = (VGA_ALPHA_PTR)0xA0000;
 #else
    text_vram = (VGA_ALPHA_PTR)MK_FP(0xA000,0x0000);
 #endif

    screen_w = 80;                  // TODO: When would such a system use 40-column mode?
    screen_h = 25;                  // TODO: PC-98 systems have a 20-line text mode too!
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

    {
        unsigned int i;
        unsigned char doit = 0;
        unsigned char redraw = 1;
        struct menuitem *item;
        int c;

        do {
            if (redraw) {
                for (i=0;i < menu_items;i++)
                    draw_menu_item(i);

                redraw = 1;
            }

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
                if ((--menu_sel) < 0)
                    menu_sel = menu_items - 1;

                if (i != menu_sel) {
                    draw_menu_item(i);
                    draw_menu_item(menu_sel);
                }
            }
            else if (c == DNARROW) {
                i = menu_sel;
                if ((++menu_sel) >= menu_items)
                    menu_sel = 0;

                if (i != menu_sel) {
                    draw_menu_item(i);
                    draw_menu_item(menu_sel);
                }
            }
            else if (c == 13) {
                if (menu_sel >= 0 && menu_sel < menu_items) {
                    doit = 1;
                    break;
                }
            }
        } while(1);
    }

    return 0;
}

int main(int argc,char **argv,char **envp) {
    if (parse_argv(argc,argv))
        return 1;

#if !defined(TARGET_PC98)
    if (!probe_vga())
        return 1;
#endif

    if (load_menu() == 0) {
        if (run_menu() == 0) {
        }
    }

    free_menu();
	return 0;
}

