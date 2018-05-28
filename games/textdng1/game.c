
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <hw/cpu/endian.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <direct.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <malloc.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/cpu/cpu.h>
#include <hw/vga/vga.h>
#include <hw/8237/8237.h>       /* 8237 DMA */
#include <hw/8254/8254.h>       /* 8254 timer */
#include <hw/8259/8259.h>       /* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/vga/vgatty.h>
#include <hw/dos/doswin.h>

static unsigned char apploop = 1;

#pragma pack(push,1)
struct game_cell {
    unsigned char               what;
    unsigned char               param;
};
#pragma pack(pop)

struct game_map {
    struct game_cell far*       map_base;
    unsigned int                map_width,map_height;
};

struct game_map*                current_level = NULL;

struct game_map *map_alloc(void) {
    struct game_map *m = calloc(1,sizeof(struct game_map));
    return m;
}

struct game_cell far *map_get_row(struct game_map *m,unsigned int y) {
    if (m->map_base == NULL)
        return NULL;
    if (y >= m->map_height)
        return NULL;

    return m->map_base + (y * m->map_width);
}

struct game_cell far *map_get_cell(struct game_map *m,unsigned int x,unsigned int y) {
    struct game_cell far *p;

    assert(m != NULL);
    if (x >= m->map_width) return NULL;

    p = map_get_row(m,y);

    if (p != NULL) p += x;

    return p;
}

void map_free_data(struct game_map *m) {
    if (m->map_base != NULL) {
        _ffree(m->map_base);
        m->map_base = NULL;
    }

    m->map_width = 0;
    m->map_height = 0;
}

int map_alloc_data(struct game_map *m,unsigned int w,unsigned int h) {
    if (m->map_base != NULL)
        map_free_data(m);
    if (w == 0 || h == 0 || w >= 256 || h >= 256)
        return -1;

    {
        unsigned long sz = (unsigned long)w * (unsigned long)h * (unsigned long)sizeof(struct game_cell);
        if (sz >= 0xFFF0UL)
            return -1;

        m->map_base = (struct game_cell far*)_fmalloc((unsigned)sz);
        if (m->map_base == NULL)
            return -1;
    }

    m->map_width = w;
    m->map_height = h;
    return 0;
}

struct game_map *map_free(struct game_map *m) {
    if (m) {
        map_free_data(m);
        free(m);
    }

    return NULL;
}

void clear_screen(void) {
    vga_clear();
    vga_moveto(0,0);
    vga_write_color(0x7);
}

void do_title_screen(void) {
    unsigned int i;
    int c;

    /* clear video RAM with background */
    for (i=0;i < (80*25);i++)
        vga_state.vga_alpha_ram[i] = 0x0100 + 176U;

    /* message */
    vga_write_color(0xF);
    vga_moveto(30,8);    vga_write("Text Dungeon 1");
    vga_moveto(22,9);    vga_write("A crappy example 2D dungeon map.");
    vga_moveto(14,10);    vga_write("(C) 2018 Hackipedia/DOSLIB/TheGreatCodeholio");

    vga_moveto(22,12);    vga_write("Hit ENTER or SPACE to continue.");
    vga_moveto(28,13);    vga_write("Hit ESC to exit to DOS.");

    do {
        c = getch();
        if (c == 27) {
            apploop = 0;
            break;
        }
        else if (c == ' ' || c == 13) {
            break;
        }
    } while (1);
}

char line[1024];

void ERROR(const char *fmt) {
    fprintf(stderr,"%s\n",fmt);
    while (getch() != 13);
}

int load_level_file(struct game_map *map, const char *fn) {
    unsigned int w=0,h=0,my=0;
    FILE *fp;
    char *p;

    fp = fopen(fn,"rb");
    if (fp == NULL) {
        ERROR("Unable to open level file");
        return -1;
    }

    while (!feof(fp) && !ferror(fp)) {
        if (fgets(line,sizeof(line)-1,fp) == NULL) break;

        p = line;
        if (*p == '#') {
            p++;

            if (!strncmp(p,"w=",2)) {
                p += 2;
                w = atoi(p);
            }
            else if (!strncmp(p,"h=",2)) {
                p += 2;
                h = atoi(p);
            }
        }
        else if (*p == '>') {
            p++;

            if (map->map_base == NULL) {
                if (map_alloc_data(map,w,h) < 0) {
                    ERROR("Failed to alloc map data");
                    break;
                }
            }

            if (my < map->map_height) {
                struct game_cell far *row = map_get_row(map,my);
                unsigned int mx = 0;
                assert(row != NULL);

                while (*p != 0 && *p != '\n' && *p != '\r' && mx < map->map_width) {
                    row->what = *p;
                    row->param = 0;

                    row++;
                    mx++;
                    p++;
                }
                while (mx < map->map_width) {
                    row->what = 0;
                    row->param = 0;

                    row++;
                    mx++;
                }
            }
        }
    }

    fclose(fp);

    if (map->map_base != NULL && map->map_width != 0) {
        while (my < map->map_height) {
            struct game_cell far *row = map_get_row(map,my);
            unsigned int mx = 0;
            assert(row != NULL);

            while (mx < map->map_width) {
                row->what = 0;
                row->param = 0;

                row++;
                mx++;
            }

            my++;
        }
    }

    if (map->map_base == NULL || map->map_width == 0 || map->map_height == 0) {
        ERROR("Map not alloc");
        return -1;
    }

    return 0;
}

int load_level(unsigned int N) {
    char tmp[14];

    if (N == 0U || N > 99U) return -1;

    current_level = map_free(current_level);

    current_level = map_alloc();
    if (current_level == NULL) {
        ERROR("map alloc failed");
        return -1;
    }

    sprintf(tmp,"level%02u.txt",N);
    if (load_level_file(current_level, tmp) < 0) {
        current_level = map_free(current_level);
        ERROR("Load level failed");
        return -1;
    }

    return 0;
}

int main(int argc,char **argv,char **envp) {
    probe_dos();
    cpu_probe();
    detect_windows();
    probe_8237();
    if (!probe_8259()) {
        printf("Cannot init 8259 PIC\n");
        return 1;
    }
    if (!probe_8254()) {
        printf("Cannot init 8254 timer\n");
        return 1;
    }
	if (!probe_vga()) { /* NTS: By "VGA" we mean any VGA, EGA, CGA, MDA, or other common graphics hardware on the PC platform
                                that acts in common ways and responds to I/O ports 3B0-3BF or 3D0-3DF as well as 3C0-3CF */
		printf("VGA probe failed\n");
		return 1;
	}

    /* this runs in text mode 80x25 */
	int10_setmode(3); /* 80x25 */
	update_state_from_vga(); /* make sure the VGA library knows so the VGA ptr values work. */
                             /* text is at either B000:0000 (mono/MDA) or B800:0000 (color/CGA) */

    while (apploop) {
        do_title_screen();
        if (!apploop) break;
        if (load_level(1) < 0) break;
    }

    clear_screen();
    vga_moveto(0,22); /* x,y near bottom of screen */
	vga_sync_bios_cursor();
    return 0;
}

