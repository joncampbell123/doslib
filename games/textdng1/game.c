
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

enum {
    THE_VOID='`',
    OPEN_SPACE=' ',
    EXIT_SPACE='e',
    TELE_SPACE='t'
};

enum {
    UP=0,
    DOWN,
    LEFT,
    RIGHT,
    NONE
};

enum {
    ET_NONE=0,
    ET_LEVEL,
    ET_TELE
};

enum {
    ET_LEVEL_BACK=255
};

enum {
    ET_TELE_BACK=255
};

#pragma pack(push,1)
struct game_cell {
    unsigned char               what;
    unsigned char               param;
};

struct game_exit {
    unsigned char               type;
    unsigned char               param;
    unsigned char               x,y;
};
#pragma pack(pop)

struct game_map {
    struct game_cell far*       map_base;
    unsigned int                map_width,map_height;
    unsigned int                map_scroll_x,map_scroll_y;
    unsigned int                map_display_x,map_display_y;
    unsigned int                map_display_w,map_display_h;
    struct game_exit            exit[10];
};

enum {
    CHAR_PLAYER=1
};

struct game_character {
    int                         map_x,map_y;
    unsigned char               what;
    unsigned char               param;
    unsigned char               param2;
};

#define CH_P_FALLING_DELAY      1

enum {
    CH_P_NORMAL=0,

    CH_P_TRAVEL_LOCK,

    CH_P_FALLING1,
    CH_P_FALLING2,
    CH_P_FALLING3,
    CH_P_FALLING4,
    CH_P_FALLING5,
    CH_P_FALLING6,

    CH_P_MAX
};

struct game_character           player;

const struct game_character     player_init = {0, 0, CHAR_PLAYER, 0, 0};

struct game_map*                current_level = NULL;
signed char                     current_level_N = -1;

signed char                     prev_level = -1;
signed char                     prev_level_exit = -1;
static unsigned char            prev_level_dir = UP;
struct game_character           prev_level_player;

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

struct game_cell far *map_get_cell_clip(struct game_map *m,unsigned int x,unsigned int y) {
    assert(m != NULL);
    if (x >= m->map_width) x = m->map_width - 1;
    if (y >= m->map_height) y = m->map_height - 1;

    return map_get_row(m,y) + x;
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

void player_says(const char *msg) {
    unsigned int x;

    vga_write_color(0x0F);
    vga_moveto(0,22);

    for (x=0;x < 80;x++) {
        char c = *msg;

        vga_writec(c);
        if (c != 0) msg++;
    }
}

uint16_t offscreen_composite[80*25];

uint16_t player_chars[CH_P_MAX] = {
    0x0E02,     // CH_P_NORMAL
    0x0C02,     // CH_P_TRAVEL_LOCK
    0x0E01,     // CH_P_FALLING1
    0x0701,     // CH_P_FALLING2
    0x0801,     // CH_P_FALLING3
    0x08F8,     // CH_P_FALLING4
    0x08FA,     // CH_P_FALLING5
    0x0000      // CH_P_FALLING6
};

void draw_character_composite(unsigned int dx,unsigned int dy,unsigned int dw,unsigned int dh,unsigned int sx,unsigned int sy,struct game_character *c) {
    int drawx = c->map_x - (int)sx;
    int drawy = c->map_y - (int)sy;
    if (drawx < 0 || drawy < 0 || (unsigned int)drawx >= dw || (unsigned int)drawy >= dh) return;

    switch (c->what) {
        case CHAR_PLAYER:
            offscreen_composite[((unsigned int)drawy * dw) + (unsigned int)drawx] = player_chars[c->param];
            break;
    }
}

unsigned char exit_proc_dir = UP;
unsigned char level_run = 0;
int exit_proc = -1;

unsigned int can_move(struct game_character *chr, unsigned int dir, struct game_cell far *cur, struct game_cell far *next, unsigned int do_action) {
    if (next == NULL)
        return 0;
    if (cur == NULL)
        return 1;

    /* if the player is falling, the player cannot move out of the void */
    if (chr->what == CHAR_PLAYER) {
        if (chr->param >= CH_P_FALLING1) {
            if (chr->param >= CH_P_FALLING6)
                return 0;
            if (cur->what == THE_VOID && next->what != THE_VOID)
                return 0;
        }
        else if (chr->param == CH_P_TRAVEL_LOCK)
            return 0;
    }

    switch (next->what) {
        case THE_VOID:
        case OPEN_SPACE:
            return 1;
        case EXIT_SPACE:
        case TELE_SPACE:
            if (next->param < 10) {
                struct game_exit *ex = &current_level->exit[next->param];

                if (ex->type == ET_LEVEL) {
                    if (ex->param < 100) {
                        if (chr->what == CHAR_PLAYER) {
                            chr->param = CH_P_TRAVEL_LOCK;
                            chr->param2 = 5;
                            exit_proc_dir = dir;
                            exit_proc = next->param;
                        }
                    }
                    else if (ex->param == ET_LEVEL_BACK) {
                        chr->param = CH_P_TRAVEL_LOCK;
                        chr->param2 = 3;
                        exit_proc_dir = dir;
                        exit_proc = next->param;
                    }
                }
                else if (ex->type == ET_TELE) {
                    if (ex->param < 100) {
                        if (chr->what == CHAR_PLAYER) {
                            chr->param = CH_P_TRAVEL_LOCK;
                            chr->param2 = 1;
                            exit_proc_dir = dir;
                            exit_proc = next->param;
                        }
                    }
                    else if (ex->param == ET_TELE_BACK) {
                        chr->param = CH_P_TRAVEL_LOCK;
                        chr->param2 = 1;
                        exit_proc_dir = dir;
                        exit_proc = next->param;
                    }
                }
            }
            return 0;
    };

    return 0;
}

uint16_t game_cell_to_VGA(struct game_cell far *c) {
    switch (c->what) {
        case THE_VOID:      return (uint16_t)(0x0000);
        case OPEN_SPACE:    return (uint16_t)(0x08B0);
        case EXIT_SPACE:    return (uint16_t)(0x0B08);
        case TELE_SPACE:    return (uint16_t)(0x0B08);
        default:            return (uint16_t)(c->what + 0x0700);
    };

    return 0;
}

void draw_level(void) {
    unsigned int x,y,dx,dy,dw,dh,sx,sy;

    if (current_level->map_base == 0 || current_level->map_width == 0 || current_level->map_height == 0)
        return;

    sx = current_level->map_scroll_x;
    sy = current_level->map_scroll_y;
    dx = current_level->map_display_x;
    dy = current_level->map_display_y;

    assert(sx < current_level->map_width);
    assert(sy < current_level->map_height);

    dw = current_level->map_width - sx;
    dh = current_level->map_height - sy;
    if (dw > current_level->map_display_w)
        dw = current_level->map_display_w;
    if (dh > current_level->map_display_h)
        dh = current_level->map_display_h;

    assert((sx+dw) <= current_level->map_width);
    assert((sy+dh) <= current_level->map_height);
    assert((dx+dw) <= 80 && dw <= 80);
    assert((dy+dh) <= 25 && dh <= 25);

    {
        VGA_ALPHA_PTR vstart;
        uint16_t *crow;
        
        crow = offscreen_composite;
        for (y=0;y < dh;y++) {
            struct game_cell far * srow = map_get_cell(current_level,sx,y + sy);

            for (x=0;x < dw;x++)
                *crow++ = game_cell_to_VGA(srow++);
        }

        draw_character_composite(dx,dy,dw,dh,sx,sy,&player);

        crow = offscreen_composite;
        for (y=0;y < dh;y++) {
            vstart = vga_state.vga_alpha_ram + ((dy + y) * 80) + dx;

            for (x=0;x < dw;x++)
                *vstart++ = *crow++;
        }
    }
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

static unsigned int extra_edge_scroll = 2;

void scroll_to_player() {
    if (player.map_x >= extra_edge_scroll) {
        if (current_level->map_scroll_x > (player.map_x - extra_edge_scroll))
            current_level->map_scroll_x = (player.map_x - extra_edge_scroll);
    }
    if (current_level->map_display_w >= (1 + extra_edge_scroll) &&
            player.map_x >= (current_level->map_display_w - (1 + extra_edge_scroll))) {
        if (current_level->map_scroll_x < (player.map_x + 1 + extra_edge_scroll - current_level->map_display_w))
            current_level->map_scroll_x = (player.map_x + 1 + extra_edge_scroll - current_level->map_display_w);
        if (current_level->map_scroll_x > (current_level->map_width - current_level->map_display_w))
            current_level->map_scroll_x = (current_level->map_width - current_level->map_display_w);
    }

    if (player.map_y >= extra_edge_scroll) {
        if (current_level->map_scroll_y > (player.map_y - extra_edge_scroll))
            current_level->map_scroll_y = (player.map_y - extra_edge_scroll);
    }
    if (current_level->map_display_h >= (1 + extra_edge_scroll) &&
            player.map_y >= (current_level->map_display_h - (1 + extra_edge_scroll))) {
        if (current_level->map_scroll_y < (player.map_y + 1 + extra_edge_scroll - current_level->map_display_h))
            current_level->map_scroll_y = (player.map_y + 1 + extra_edge_scroll - current_level->map_display_h);
        if (current_level->map_scroll_y > (current_level->map_height - current_level->map_display_h))
            current_level->map_scroll_y = (current_level->map_height - current_level->map_display_h);
    }
}

void act_player(struct game_character *chr,struct game_cell far *cell) {
    /* if the player walked into the void, fall */
    if (cell->what == THE_VOID) {
        if (chr->what == CHAR_PLAYER) {
            if (chr->param < CH_P_FALLING1) {
                chr->param = CH_P_FALLING1;
                chr->param2 = CH_P_FALLING_DELAY; // delay

                player_says("AAAAAAAAaaaaaaaaaaaahhhh!!! (poof)");
            }
        }
    }
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

    player = player_init;

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
            else if (!strncmp(p,"start=",6)) {
                p += 6;
                player.map_x = (int)strtol(p,&p,10);
                if (*p == ',') {
                    p++;
                    player.map_y = (int)strtol(p,&p,10);
                }
            }
            else if (!strncmp(p,"exit",4) && isdigit(p[4]) && p[5] == '=') { /* exit1=level 3 */
                struct game_exit *ex = &map->exit[p[4] - '0'];

                p += 6;
                if (!strncmp(p,"level",5) && isdigit(p[5])) {
                    p += 5;
                    while (*p == ' ') p++;
                    ex->type = ET_LEVEL;
                    ex->param = atoi(p);
                }
                else if (!strncmp(p,"levelback",9)) {
                    ex->type = ET_LEVEL;
                    ex->param = ET_LEVEL_BACK;
                }
                else if (!strncmp(p,"tele",4) && isdigit(p[4])) {
                    p += 4;
                    while (*p == ' ') p++;
                    ex->type = ET_TELE;
                    ex->param = atoi(p);
                }
                else if (!strncmp(p,"teleback",8)) {
                    ex->type = ET_TELE;
                    ex->param = ET_TELE_BACK;
                }
                else {
                    ex->type = ET_NONE;
                }
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
                char last_p = OPEN_SPACE;

                assert(row != NULL);
                while (*p != 0 && *p != '\n' && *p != '\r' && mx < map->map_width) {
                    last_p = *p;

                    row->what = (unsigned char)(*p);
                    row->param = 0;

                    row++;
                    mx++;
                    p++;
                }
                while (mx < map->map_width) {
                    row->what = (unsigned char)(last_p);
                    row->param = 0;

                    row++;
                    mx++;
                }

                my++;
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
                row->what = THE_VOID;
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

    /* some things need to be converted */
    {
        unsigned int mx,my;

        for (my=0;my < map->map_height;my++) {
            struct game_cell far *row = map_get_row(map,my);
            assert(row != NULL);

            for (mx=0;(mx+1U) < map->map_width;) {
                if (row[mx].what == EXIT_SPACE || row[mx].what == TELE_SPACE) { /* e1 = exit 1    two cells */
                    if (row[mx+1].what >= '0' && row[mx+1].what <= '9') {
                        row[mx].param = row[mx+1].what - '0';
                        row[mx+1] = row[mx];

                        current_level->exit[row[mx].param].x = mx;
                        current_level->exit[row[mx].param].y = my;

                        /* so now the exit space is two cells wide.
                         * trim one or the other based on which side is against a wall */
                        {
                            unsigned char okr=0;
                            unsigned char okl=0;

                            if (mx == 0)
                                okr = 1;
                            else if (row[mx-1].what != OPEN_SPACE)
                                okr = 1;

                            if (mx > 0 && row[mx+2].what != OPEN_SPACE)
                                okl = 1;

                            /* must trim one or the other */
                            if (!okr && !okl) okr = 1;

                            if (okr) row[mx+1] = row[mx+2];
                            if (okl) row[mx] = row[mx-1];

                            if (okl) current_level->exit[row[mx+1].param].x++;
                        }

                        mx += 2;
                    }
                    else {
                        row[mx].what = THE_VOID;
                        mx++;
                    }
                }
                else {
                    mx++;
                }
            }
        }
    }

    return 0;
}

void game_over(void) {
    vga_write_color(0x4E);
    vga_moveto(20,14);
    vga_write("* GAME OVER *");
}

int load_level(unsigned int N) {
    char tmp[14];

    if (N == 0U || N > 99U) return -1;

    current_level_N = N;

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

    current_level->map_display_x = 2;
    current_level->map_display_y = 2;
    current_level->map_display_w = 60;
    current_level->map_display_h = 15;
    return 0;
}

int level_loop(void) {
    int c;

    exit_proc_dir = UP;
    exit_proc = -1;
    level_run = 1;
    clear_screen();
    scroll_to_player();
    draw_level();

    do {
        /* if the player is falling, animate */
        if (player.param >= CH_P_FALLING1) {
            if (player.param2 > 0) {
                player.param2--;
            }
            else {
                if (player.param < CH_P_FALLING6) {
                    player.param++;
                }
                else {
                    /* game over */
                    game_over();
                    do { c=getch(); } while (!(c == 27 || c == 13));
                    break;
                }

                player.param2 = CH_P_FALLING_DELAY;
            }
        }
        else if (player.param == CH_P_TRAVEL_LOCK) {
            if (player.param2 > 0)
                player.param2--;
            else {
                if (exit_proc >= 0 && exit_proc < 10) {
                    struct game_exit *ex = &current_level->exit[exit_proc];

                    if (ex->type == ET_LEVEL) {
                        if (ex->param == ET_LEVEL_BACK) {
                            if (prev_level >= 0) {
                                struct game_character pl = prev_level_player;
                                signed char level = prev_level;
//                              signed char exit = prev_level_exit;

                                prev_level_dir = exit_proc_dir;
                                prev_level_player = player;
                                prev_level = current_level_N;
                                prev_level_exit = exit_proc;
                                if (load_level(level) < 0)
                                    return 0;

                                if (pl.param == CH_P_TRAVEL_LOCK) {
                                    player = pl;
                                    player.param = CH_P_NORMAL;
                                    player.param2 = 0;
                                }
                            }
                        }
                        else if (ex->param < 100) {
                            prev_level_dir = exit_proc_dir;
                            prev_level_player = player;
                            prev_level = current_level_N;
                            prev_level_exit = exit_proc;
                            if (load_level(ex->param) < 0)
                                return 0;
                        }

                        return 1;
                    }
                    else if (ex->type == ET_TELE) {
                        player.param = CH_P_NORMAL;
                        player.param2 = 0;
                        if (ex->param == ET_TELE_BACK) {
                            struct game_character pl = prev_level_player;

                            prev_level_player = player;

                            player = pl;
                            scroll_to_player();
                        }
                        else if (ex->param < 10) {
                            struct game_exit *nex = &current_level->exit[ex->param];

                            prev_level_player = player;

                            player.map_x = nex->x;
                            player.map_y = nex->y;

                            /* blocking check */
                            if (exit_proc_dir == UP && map_get_cell_clip(current_level,player.map_x,player.map_y-1)->what != OPEN_SPACE)
                                exit_proc_dir = DOWN;
                            if (exit_proc_dir == DOWN && map_get_cell_clip(current_level,player.map_x,player.map_y+1)->what != OPEN_SPACE)
                                exit_proc_dir = UP;
                            if (exit_proc_dir == UP && map_get_cell_clip(current_level,player.map_x,player.map_y-1)->what != OPEN_SPACE)
                                exit_proc_dir = LEFT;
                            if (exit_proc_dir == LEFT && map_get_cell_clip(current_level,player.map_x-1,player.map_y)->what != OPEN_SPACE)
                                exit_proc_dir = RIGHT;
                            if (exit_proc_dir == RIGHT && map_get_cell_clip(current_level,player.map_x+1,player.map_y)->what != OPEN_SPACE)
                                exit_proc_dir = LEFT;
                            if (exit_proc_dir == LEFT && map_get_cell_clip(current_level,player.map_x-1,player.map_y)->what != OPEN_SPACE)
                                exit_proc_dir = NONE;

                            /* need to step out of the block itself, based on player direction and walls */
                            if (exit_proc_dir == UP) {
                                player.map_y--;
                            }
                            else if (exit_proc_dir == DOWN) {
                                player.map_y++;
                            }
                            else if (exit_proc_dir == LEFT) {
                                player.map_x--;
                            }
                            else if (exit_proc_dir == RIGHT) {
                                player.map_x++;
                            }

                            scroll_to_player();
                        }

                        continue;
                    }

                    ERROR("Unhandled exit");
                    break;
                }
                else {
                    ERROR("Exit proc out of range");
                    break;
                }
            }
        }
        else {
            if (!level_run) break;
        }

        if (kbhit()) {
            c = getch();
            if (c == 0) c = getch() << 8;
            if (c == 27) break;

            /* avoid key buffering by eating the rest of the pending input NOW.
             * This gives us the same "buffered and delayed" keyboard input problems
             * I remember having with Chips Challenge, but, it's better than nothing. */
            while (kbhit()) getch();

            if (c == 0x4800) {//UP
                if (player.map_y > 0) {
                    if (can_move(&player, UP,
                                map_get_cell(current_level, player.map_x, player.map_y),
                                map_get_cell(current_level, player.map_x, player.map_y - 1), 1)) {
                        player.map_y--;
                        act_player(&player,map_get_cell(current_level, player.map_x, player.map_y));
                        scroll_to_player();
                    }
                }
            }
            else if (c == 0x5000) {//DOWN
                if ((player.map_y + 1) < current_level->map_height) {
                    if (can_move(&player, DOWN,
                                map_get_cell(current_level, player.map_x, player.map_y),
                                map_get_cell(current_level, player.map_x, player.map_y + 1), 1)) {
                        player.map_y++;
                        act_player(&player,map_get_cell(current_level, player.map_x, player.map_y));
                        scroll_to_player();
                    }
                }
            }
            else if (c == 0x4B00) {//LEFT
                if (player.map_x > 0) {
                    if (can_move(&player, LEFT,
                                map_get_cell(current_level, player.map_x,     player.map_y),
                                map_get_cell(current_level, player.map_x - 1, player.map_y), 1)) {
                        player.map_x--;
                        act_player(&player,map_get_cell(current_level, player.map_x, player.map_y));
                        scroll_to_player();
                    }
                }
            }
            else if (c == 0x4D00) {//RIGHT
                if ((player.map_x + 1) < current_level->map_width) {
                    if (can_move(&player, RIGHT,
                                map_get_cell(current_level, player.map_x,     player.map_y),
                                map_get_cell(current_level, player.map_x + 1, player.map_y), 1)) {
                        player.map_x++;
                        act_player(&player,map_get_cell(current_level, player.map_x, player.map_y));
                        scroll_to_player();
                    }
                }
            }
        }

        /* redraw */
        draw_level();

        /* pause for 100ms (10 frames/sec) */
        t8254_wait(t8254_us2ticks(100000UL));
    } while (1);

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

    write_8254_system_timer(0); /* 18.2 tick/sec on our terms (proper PIT mode) */

    while (apploop) {
        do_title_screen();
        if (!apploop) break;

        prev_level = -1;
        prev_level_exit = -1;
        prev_level_player = player_init;

        if (load_level(1) < 0) break;

        while (level_loop());
    }

    clear_screen();
    vga_moveto(0,22); /* x,y near bottom of screen */
	vga_sync_bios_cursor();
    return 0;
}

