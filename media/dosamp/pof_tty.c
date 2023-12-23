
#if defined(TARGET_WINDOWS)
# define HW_DOS_DONT_DEFINE_MMSYSTEM
# include <windows.h>
# include <commdlg.h>
# include <mmsystem.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#ifdef LINUX
#include <signal.h>
#include <endian.h>
#include <dirent.h>
#else
#include <hw/cpu/endian.h>
#endif
#ifndef LINUX
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <direct.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <malloc.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#ifndef LINUX
#include <dos.h>
#endif
#ifndef LINUX
#define strcasecmp strcmpi
#define strncasecmp strnicmp
#endif

#ifndef LINUX
#include <hw/dos/dos.h>
#include <hw/cpu/cpu.h>
#include <hw/8237/8237.h>       /* 8237 DMA */
#include <hw/8254/8254.h>       /* 8254 timer */
#include <hw/8259/8259.h>       /* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/cpu/cpurdtsc.h>
#include <hw/dos/doswin.h>
#include <hw/dos/tgusmega.h>
#include <hw/dos/tgussbos.h>
#include <hw/dos/tgusumid.h>
#include <hw/isapnp/isapnp.h>
#include <hw/sndsb/sndsbpnp.h>
#endif

#include "wavefmt.h"
#include "dosamp.h"
#include "timesrc.h"
#include "dosptrnm.h"
#include "filesrc.h"
#include "resample.h"
#include "cvrdbuf.h"
#include "cvip.h"
#include "trkrbase.h"
#include "tmpbuf.h"
#include "snirq.h"
#include "sndcard.h"
#include "termios.h"
#include "cstr.h"

#include "sc_sb.h"
#include "sc_oss.h"
#include "sc_alsa.h"
#include "sc_winmm.h"
#include "sc_dsound.h"

#include "dsound.h"
#include "winshell.h"
#include "mmsystem.h"
#include "commdlg.h"
#include "fs.h"

#include "pof_gofn.h"
#include "pof_tty.h"

int tty_tab_complete_filter(char *fname) {
    char *ext = strrchr(fname,'.');
    if (ext == NULL) return 0;
    ext++;

    if (!strcasecmp(ext,"wav"))
        return 1;

    return 0;
}

unsigned int tty_tab_completion(char *tmp,size_t tmpsize,int *tmpi) {
#if defined(TARGET_MSDOS) && TARGET_MSDOS == 16 && defined(__SMALL__)
    return 0;
#else
    unsigned int row = 0,rowmax = 23;
    unsigned int count = 0;
    unsigned int ret = 0;
    struct dirent *d;
    struct stat st;
    char *path_tmp;
    int newi = -1;
    int file_pos;
    char *ens;
    DIR *dir;
    int c;

    if (*tmp == 0) return 0;

    ens = strdup(tmp);
    if (ens == NULL) return 0;
    {
        /* remove filename and path separator from end in case that prevents DOS from reading the directory */
        char *e = ens + strlen(ens) - 1;
        while (e >= ens && *e != PATH_SEP) *e-- = 0;
#if defined(TARGET_MSDOS) || defined(TARGET_WINDOWS)
        /* make sure we retain trailing slash for root dir i.e. C:\ not C: */
        if (isalpha(ens[0]) && ens[1] == ':') {
            if (e >= (ens+3) && *e == PATH_SEP) *e-- = 0;
        }
        else {
            if (e >= ens && *e == PATH_SEP) *e-- = 0;
        }
#else
        if (e >= ens && *e == PATH_SEP) *e-- = 0;
#endif
    }

    {
        char *file;

        /* in tmp[] look for the filename */
        file = strrchr(tmp,PATH_SEP);
        if (file != NULL) file++;
        else file = tmp;

        file_pos = (int)(file - tmp);
    }

    path_tmp = malloc(PATH_MAX+1);
    if (path_tmp == NULL) {
        free(ens);
        return 0;
    }

    dir = opendir(ens);
    if (dir != NULL) {
        while ((d=readdir(dir)) != NULL) {
            if (d->d_name[0] == '.') continue;
            if (snprintf(path_tmp,PATH_MAX,"%s%c%s",ens,PATH_SEP,d->d_name) >= PATH_MAX) continue;

            if (stat(path_tmp,&st) != 0) continue;

            if (S_ISREG(st.st_mode)) {
                if (!tty_tab_complete_filter(d->d_name)) continue;
            }
            else if (S_ISDIR(st.st_mode)) {
            }
            else {
                continue;
            }

            /* TODO: If MS-DOS and the "hidden" bit is set... */

            {
                char *s1 = tmp + file_pos;
                char *s2 = d->d_name;

                while (s1 < (tmp + *tmpi) && *s1 != 0 && *s2 != 0) {
                    if (fs_comparechar(*s1,*s2)) {
                        s1++;
                        s2++;
                    }
                    else {
                        break;
                    }
                }

                if (s1 == (tmp + *tmpi)) {
                    int i = *tmpi;

                    count++;

                    if (newi >= 0) {
                        while (*s1 != 0 && *s2 != 0) {
                            if (fs_comparechar(*s1,*s2)) {
                                s1++;
                                s2++;
                                i++;
                            }
                            else {
                                break;
                            }
                        }

                        if (newi > i)
                            newi = i;
                    }
                    else {
#if defined(TARGET_MSDOS) || (defined(TARGET_WINDOWS) && TARGET_MSDOS == 16)
                        /* MS-DOS and 16-bit Windows variant */
                        if (*tmpi <= file_pos || isupper(tmp[file_pos])) {
                            while ((size_t)i < (tmpsize-1) && *s2 != 0)
                                tmp[i++] = toupper(*s2++);
                        }
                        else {
                            while ((size_t)i < (tmpsize-1) && *s2 != 0)
                                tmp[i++] = tolower(*s2++);
                        }
#else
                        while ((size_t)i < (tmpsize-1) && *s2 != 0)
                            tmp[i++] = *s2++;
#endif

                        newi = i;
                    }

                    tmp[newi] = 0;
                }
            }
        }
        closedir(dir);
    }

    if (newi >= 0) {
        tmp[newi] = 0;
        if (*tmpi < newi) {
            printf("%s",tmp+*tmpi);
            fflush(stdout);
        }
        *tmpi = newi;
    }

    if (count > 1) {
        dir = opendir(ens);
        if (dir != NULL) {
            while ((d=readdir(dir)) != NULL) {
                if (d->d_name[0] == '.') continue;
                if (snprintf(path_tmp,PATH_MAX,"%s%c%s",ens,PATH_SEP,d->d_name) >= PATH_MAX) continue;

                if (stat(path_tmp,&st) != 0) continue;

                if (S_ISREG(st.st_mode)) {
                    if (!tty_tab_complete_filter(d->d_name)) continue;
                }
                else if (S_ISDIR(st.st_mode)) {
                }
                else {
                    continue;
                }

                /* TODO: If MS-DOS and the "hidden" bit is set... */

                if (*tmpi <= file_pos || !fs_strncasecmp(tmp + file_pos, d->d_name, (size_t)(*tmpi - file_pos))) {
                    if (!ret) printf("\x0D" "                           " "\x0D");

                    printf("  %s\n",d->d_name);

                    ret = 1; /* need redraw */

                    if ((++row) >= rowmax) {
                        printf("(TAB to continue)");
                        fflush(stdout);
                        row = 0;

                        do {
                            c = getch();
                            if (c == 27 || c == 13 || c == ' ' || c == 9)
                                break;
                        } while (1);

                        printf("\x0D" "                          " "\x0D");
                        fflush(stdout);

                        if (c == 27)
                            break;
                    }
                }
            }
            closedir(dir);
        }
    }

    free(path_tmp);
    free(ens);

    return ret;
#endif
}

char *prompt_open_file_tty(void) {
    struct stat st;
    char tmp[300];
    char redraw=1;
    char loop=1;
    int tmpi=0;
    char ok=0;
    int c;

    memset(tmp,0,sizeof(tmp));
    if (wav_file != NULL) strncpy(tmp,wav_file,sizeof(tmp)-1);
    tmpi=strlen(tmp);

    do {
        /* redraw */
        if (redraw) {
            char *cwd = getcwd(NULL, 0);

            printf("\n");
            printf("CWD: %s\n",cwd?cwd:"(null)");
            printf("File? %s",tmp); fflush(stdout);

            if (cwd) free(cwd);
            redraw=0;
        }

        c = getch();
        if (c == 27)
            break;
        else if (c == 13) {
            if (tmpi != 0) {
                if (stat(tmp,&st) == 0) {
                    if (S_ISREG(st.st_mode)) {
                        ok = 1;
                        break;
                    }
                    else if (S_ISDIR(st.st_mode)) {
#if defined(TARGET_MSDOS) || defined(TARGET_WINDOWS)
                        /* if the drive exists at the start (A:\, C:\ etc)
                         * then change current drive as well */
                        if (isalpha(tmp[0]) && tmp[1] == ':')
                            _chdrive(tolower(tmp[0]) + 1 - 'a');

                        if (strlen(tmp) > 2) {
                            if (chdir(tmp) < 0)
                                printf("\n* Unable to change directory\n");
                        }
#else
                        if (chdir(tmp) < 0)
                            printf("\n* Unable to change directory\n");
#endif

                        redraw = 1;
                    }
                }
                else {
                    printf("\n* Does not exist\n");
                    redraw = 1;
                }
            }
        }
        else if (c == 8) {
            if (tmpi > 0) {
                printf("\x08 \x08"); fflush(stdout);
                tmp[--tmpi] = 0;
            }
        }
        else if (c == 9) {
            if (tmpi == 0) {
#if defined(TARGET_MSDOS) || defined(TARGET_WINDOWS)
                tmpi = sprintf(tmp,"%c:",_getdrive()+'A'-1);
#else
                tmpi = sprintf(tmp,"/");
#endif
                printf("%s",tmp); fflush(stdout);
            }
            else if (tmp[tmpi-1] == PATH_SEP) {
                redraw = tty_tab_completion(tmp,sizeof(tmp),&tmpi);
            }
            else if (stat(tmp,&st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    tmpi += sprintf(tmp+tmpi,"%c",PATH_SEP);
                    printf("%c",PATH_SEP); fflush(stdout);
                }
            }
            else {
                redraw = tty_tab_completion(tmp,sizeof(tmp),&tmpi);
            }
        }
        else if (c == 23/*CTRL+W*/) {
            if (tmpi > 0 && tmp[tmpi-1] == PATH_SEP) {
                printf("\x08 \x08"); fflush(stdout);
                tmp[--tmpi] = 0;
            }

            while (tmpi > 0) {
                if (tmp[tmpi-1] == PATH_SEP)
                    break;

                printf("\x08 \x08"); fflush(stdout);
                tmp[--tmpi] = 0;
            }
        }
        else if (c >= 32 || c < 0) {
            if ((size_t)tmpi < (sizeof(tmp)-1)) {
                printf("%c",c); fflush(stdout);
                tmp[tmpi++] = c;
                tmp[tmpi] = 0;
            }
        }
    } while (loop);
    printf("\n");

    return ok ? strdup(tmp) : NULL;
}

