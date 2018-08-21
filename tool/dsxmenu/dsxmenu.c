
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

static const char*                  menu_ini = NULL;            // points at argv[] do not free

static const char*                  menu_default_name = NULL;   // strdup(), free at end
static int                          menu_default_timeout = 5;

struct menuitem {
    char*                           menu_text;                  // strdup()'d
    char*                           menu_item_name;             // strdup()'d
};

#define MAX_MENU_ITEMS              20

struct menuitem                     menu[MAX_MENU_ITEMS];
int                                 menu_items = 0;
int                                 menu_sel = 0;

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

int load_menu(void) {
    char *current_section = NULL;
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

            // TODO
        }
    }

    cstr_free(&current_section);

    fclose(fp);
    temp_free();
    return 0;
}

int main(int argc,char **argv,char **envp) {
    if (parse_argv(argc,argv))
        return 1;

    if (load_menu())
        return 1;

	return 0;
}

