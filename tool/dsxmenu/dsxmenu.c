
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

static const char*                  menu_ini = NULL;

static const char*                  menu_default_name;
static int                          menu_default_timeout = 5;

struct menuitem {
    char*                           menu_text;
    char*                           menu_item_name;
};

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

int main(int argc,char **argv,char **envp) {
    if (parse_argv(argc,argv))
        return 1;

	return 0;
}

