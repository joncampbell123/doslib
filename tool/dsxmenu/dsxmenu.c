
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
#include <hw/dos/dosbox.h>

static unsigned char                debug_ini = 0;

static const char*                  menu_ini = NULL;            // points at argv[] do not free

static char*                        menu_default_name = NULL;   // strdup(), free at end
static int                          menu_default_timeout = 5;

struct menuitem {
    char*                           menu_text;                  // strdup()'d
    char*                           menu_item_name;             // strdup()'d
};

#define MAX_MENUCHOICE_COMMANDS     100

struct menuchoice {
    char*                           menu_item_name;             // strdup()'d
    char*                           command[MAX_MENUCHOICE_COMMANDS];
    int                             command_items;
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
    }

    return 0;
}

int main(int argc,char **argv,char **envp) {
    if (parse_argv(argc,argv))
        return 1;

    if (load_menu())
        return 1;

	return 0;
}

