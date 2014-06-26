#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <malloc.h>
#include <fcntl.h>
#include <ctype.h>
#include <i86.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <hw/vga/vgatty.h>
#include <hw/vga/vgagui.h>

#ifdef TARGET_WINDOWS
# include <hw/dos/winfcon.h>
# include <windows/apihelp.h>
# include <windows/dispdib/dispdib.h>
# include <windows/win16eb/win16eb.h>
#endif

struct vga_menu_bar_state	vga_menu_bar = {NULL,-1,0};
void				(*vga_menu_idle)() = NULL;

int vga_menu_item_nonselectable(const struct vga_menu_item *m) {
	if (m->text == (char*)1) return 1;
	return 0;
}

void vga_menu_bar_draw() {
	VGA_ALPHA_PTR vga = vga_alpha_ram + (80*vga_menu_bar.row);
	const struct vga_menu_bar_item *m = vga_menu_bar.bar;
	unsigned int x,i,color,colorh,ti;
	unsigned char hi;
	const char *msg;

	/* start */
	x = 0;
	i = 0;
	if (m != NULL) {
		while (x < 80 && m->name != NULL) {
			ti = 1;
			msg = m->name;
			hi = (i == vga_menu_bar.sel);
			color = hi ? 0x1F00 : 0x7000;
			colorh = hi ? 0x1E00 : 0x7100;
			if (m->x >= 80) break;
			while (x < m->x) vga[x++] = color | 0x20;
			while (x < (m->x+m->w) && *msg != 0) {
				if (ti && *msg == m->shortcut_key) {
					vga[x++] = colorh | (*msg++);
					ti = 0;
				}
				else {
					vga[x++] = color | (*msg++);
				}
			}
			while (x < (m->x+m->w)) vga[x++] = color | 0x20;
			m++;
			i++;
		}
	}

	/* finish the bar */
	while (x < 80) vga[x++] = 0x7020;
}

void vga_menu_draw_item(VGA_ALPHA_PTR screen,const struct vga_menu_item **scan,unsigned int i,unsigned int w,unsigned int color,unsigned int tcolor) {
	const struct vga_menu_item *sci = scan[i];
	const char *txt = sci->text;
	unsigned int x,ti=1;

	screen += (i * vga_width) + 1;
	if (txt == (char*)1) {
		screen[-1] = 204 | color;
		for (x=0;x < w;x++) screen[x] = 205 | color;
		screen[w] = 185 | color;
	}
	else {
		for (x=0;x < w && txt[x] != 0;x++) {
			if (ti && tolower(txt[x]) == tolower(sci->shortcut_key)) {
				screen[x] = txt[x] | tcolor;
				ti = 0;
			}
			else {
				screen[x] = txt[x] | color;
			}
		}
		for (;x < w;x++) screen[x] = 0x20 | color;
	}
}

const struct vga_menu_item *vga_menu_bar_menuitem(const struct vga_menu_bar_item *menu,unsigned char row,unsigned int *spec) {
	const struct vga_menu_item *ret = NULL,**scan,*sci;
	unsigned int w,h,i,l,x,y,o,ks,nks,items,sel,c,altup=0;
	static const unsigned int hicolor = 0x7000;
	static const unsigned int hitcolor = 0x7100;
	static const unsigned int color = 0x1700;
	static const unsigned int tcolor = 0x1E00;
	VGA_ALPHA_PTR screen,buf;
	unsigned char loop = 1;

	/* FIX: If re-inited because of arrow keys, then one more alt-up should trigger release */
	if (*spec == 0x4B00 || *spec == 0x4D00)
		altup++;

	*spec = 0;
	if (menu != NULL) {
		sel = 0;
		w = h = 1;
		ks = (read_bios_keystate() & BIOS_KS_ALT);
		scan = menu->items;
		for (i=0;(sci=scan[i]) != NULL;i++) {
			if (sci->text == (char*)1) l = 1;
			else l = (unsigned int)strlen(sci->text);
			if (l > 78) l = 78;
			if (w < (l+2)) w = (l+2);
			if (h < (i+2) && (h+2+row) <= 25) h = i+2;
		}
		items = i;

#if TARGET_MSDOS == 32
		buf = malloc(w * h * 2);
#else
		buf = _fmalloc(w * h * 2);
#endif
		screen = vga_alpha_ram + (row * vga_width) + menu->x;
		if (buf != NULL) {
			/* copy off the screen contents */
			for (y=0;y < h;y++) {
				o = w * y;
				i = vga_width * y;
				for (x=0;x < w;x++,o++,i++) buf[o] = screen[i];
			}

			/* draw the box */
			for (y=0;y < (h-1);y++) {
				o = vga_width * y;
				screen[o+0] = 186 | color;
				screen[o+w-1] = 186 | color;
			}
			o = vga_width * (h-1);
			screen[o+0] = 200 | color;
			for (x=1;x < (w-1);x++) screen[o+x] = 205 | color;
			screen[o+w-1] = 188 | color;

			/* draw the items */
			for (i=0;i < items;i++)
				vga_menu_draw_item(screen,scan,i,w-2,i == sel ? hicolor : color,i == sel ? hitcolor : tcolor);

			while (loop) {
				nks = (read_bios_keystate() & BIOS_KS_ALT);
				vga_menu_idle();

				if (ks && !nks) {
					if (++altup >= 2) break;
				}
				ks = nks;

				if (kbhit()) {
					c = getch();
					if (c == 0) c = getch() << 8;

					if (c == 27)
						break;
					else if (c == 13) {
						ret = scan[sel];
						break;
					}
					else if (c == 0x4800) {
						vga_menu_draw_item(screen,scan,sel,w-2,color,tcolor);
						do {
							if (sel == 0) sel = items-1;
							else sel--;
						} while (vga_menu_item_nonselectable(scan[sel]));
						vga_menu_draw_item(screen,scan,sel,w-2,hicolor,hitcolor);
					}
					else if (c == 0x5000) {
						vga_menu_draw_item(screen,scan,sel,w-2,color,tcolor);
						do {
							if (++sel >= items) sel = 0;
						} while (vga_menu_item_nonselectable(scan[sel]));
						vga_menu_draw_item(screen,scan,sel,w-2,hicolor,hitcolor);
					}
					else if (c == 0x4B00 || c == 0x4D00) {
						*spec = c;
						ret = NULL;
						break;
					}
					else if (c > 32 && c < 127) {
						int patience = items;

						vga_menu_draw_item(screen,scan,sel,w-2,color,tcolor);
						/* look for the first menu item with that shortcut key */
						if (++sel >= items) sel = 0;
						while (tolower(scan[sel]->shortcut_key) != tolower(c)) {
							if (--patience == 0) {
								sel = 0;
								break;
							}

							if (++sel >= items) sel = 0;
						}
						vga_menu_draw_item(screen,scan,sel,w-2,hicolor,hitcolor);
						if (patience > 0) {
							ret = scan[sel];
							break;
						}
					}
				}
			}

			/* copy screen contents back */
			for (y=0;y < h;y++) {
				i = w * y;
				o = vga_width * y;
				for (x=0;x < w;x++,o++,i++) screen[o] = buf[i];
			}

#if TARGET_MSDOS == 32
			free(buf);
#else
			_ffree(buf);
#endif
		}
	}

	return ret;
}

const struct vga_menu_item *vga_menu_bar_keymon() {
	const struct vga_menu_bar_item *m = NULL;
	const struct vga_menu_item *ret = NULL;
	unsigned int spec=0;
	int c,i;

	if (vga_menu_bar.bar == NULL)
		return ret;

	if (read_bios_keystate() & BIOS_KS_ALT) {
		vga_menu_bar.sel = 0;
		vga_menu_bar_draw();

again:
		do {
			vga_menu_idle();
			if (kbhit()) {
				c = getch();
				if (c == 0) {
					i = 0;
					c = getch();
					if (c == 0x9B) { /* ALT-left */
						if (--vga_menu_bar.sel < 0) {
							vga_menu_bar.sel = 0;
							while (vga_menu_bar.bar[vga_menu_bar.sel].name != NULL) vga_menu_bar.sel++;
							vga_menu_bar.sel--;
						}
						m = &vga_menu_bar.bar[vga_menu_bar.sel];
						vga_menu_bar_draw();
					}
					else if (c == 0x9D) { /* ALT-right */
						if (vga_menu_bar.bar[++vga_menu_bar.sel].name == NULL) vga_menu_bar.sel = 0;
						m = &vga_menu_bar.bar[vga_menu_bar.sel];
						vga_menu_bar_draw();
					}
					else {
						int oi = vga_menu_bar.sel;

						for (m=vga_menu_bar.bar;m->name != NULL;m++,i++) {
							if (c == m->shortcut_scan)
								break;
						}
						if (m->name == NULL) {
							m = NULL;
							vga_menu_bar.sel = -1;
						}
						else {
							vga_menu_bar.sel = i;
						}

						if (oi != vga_menu_bar.sel)
							vga_menu_bar_draw();

						if (m != NULL)
							break;
					}
				}
			}
		} while (read_bios_keystate() & BIOS_KS_ALT);

		if (!(read_bios_keystate() & BIOS_KS_ALT)) {
			while (kbhit()) getch();
		}

		if (m != NULL) {
			ret = vga_menu_bar_menuitem(m,vga_menu_bar.row+1,&spec);
			if (ret == NULL) {
				if (spec == 0x4B00) {
					if (--vga_menu_bar.sel < 0) {
						vga_menu_bar.sel = 0;
						while (vga_menu_bar.bar[vga_menu_bar.sel].name != NULL) vga_menu_bar.sel++;
						vga_menu_bar.sel--;
					}
					m = &vga_menu_bar.bar[vga_menu_bar.sel];
					vga_menu_bar_draw();
					goto again;
				}
				else if (spec == 0x4D00) {
					if (vga_menu_bar.bar[++vga_menu_bar.sel].name == NULL) vga_menu_bar.sel = 0;
					m = &vga_menu_bar.bar[vga_menu_bar.sel];
					vga_menu_bar_draw();
					goto again;
				}
			}
		}

		while (read_bios_keystate() & BIOS_KS_ALT);
		vga_menu_bar.sel = -1;
		vga_menu_bar_draw();
	}

	return ret;
}

int vga_msg_box_create(struct vga_msg_box *b,const char *msg,unsigned int extra_y,unsigned int min_x) {
	unsigned int w=min_x,h=max(1,extra_y),x=0,y,px,py,i,o;
	static const unsigned int color = 0x1E00;
	const char *scan;

	scan = msg;
	while (*scan != 0) {
		if (*scan == '\n') {
			x=0;
			h++;
		}
		else if ((unsigned char)(*scan) >= 32) {
			x++;
			if (w < x) w = x;
		}
		scan++;
	}
	w += 4; if (w > 80) w = 80;
	h += 2; if (h > 25) h = 25;
	px = (vga_width - w) / 2;
	py = (vga_height - h) / 2;
	b->screen = vga_alpha_ram + (py * vga_width) + px;
	b->x = px;
	b->y = py;
	b->w = w;
	b->h = h;

#if TARGET_MSDOS == 32
	b->buf = malloc(w * h * 2);
#else
	b->buf = _fmalloc(w * h * 2);
#endif
	if (b->buf != NULL) {
		/* copy the screen to buffer */
		for (y=0;y < h;y++) {
			i = y * vga_width;
			o = y * w;
			for (x=0;x < w;x++,i++,o++) b->buf[o] = b->screen[i];
		}
	}

	/* draw border */
	for (y=1;y < (h-1);y++) {
		o = y * vga_width;
		b->screen[o+0] = 186 | color;
		b->screen[o+1] = 32 | color;
		b->screen[o+w-2] = 32 | color;
		b->screen[o+w-1] = 186 | color;
	}
	o = (h-1)*vga_width;
	for (x=1;x < (w-1);x++) {
		b->screen[x] = 205 | color;
		b->screen[x+o] = 205 | color;
	}
	b->screen[0] = 201 | color;
	b->screen[w-1] = 187 | color;
	b->screen[o] = 200 | color;
	b->screen[o+w-1] = 188 | color;

	x = 0;
	y = 0;
	o = vga_width + 2;
	scan = msg;
	while (*scan != 0) {
		if (*scan == '\n') {
			while (x < (w-4)) {
				b->screen[o+x] = 32 | color;
				x++;
			}
			if (++y >= (h-2)) break;
			x = 0;
			o += vga_width;
		}
		else if ((unsigned char)(*scan) >= 32) {
			if (x < (w-4)) b->screen[o+x] = *scan | color;
			x++;
		}
		scan++;
	}
	while (y < (h-2)) {
		while (x < (w-4)) {
			b->screen[o+x] = 32 | color;
			x++;
		}
		++y;
		x = 0;
		o += vga_width;
	}

	b->w = w;
	b->h = h;
	return 1;
}

void vga_msg_box_destroy(struct vga_msg_box *b) {
	unsigned int x,y,i,o;

	if (b) {
		if (b->buf) {
			/* copy screen back */
			for (y=0;y < b->h;y++) {
				i = y * b->w;
				o = y * vga_width;
				for (x=0;x < b->w;x++,i++,o++) b->screen[o] = b->buf[i];
			}

#if TARGET_MSDOS == 32
			free(b->buf);
#else
			_ffree(b->buf);
#endif
			b->buf = NULL;
		}
		b->screen = NULL;
	}
}

int confirm_yes_no_dialog(const char *message) {
	struct vga_msg_box box = {NULL,NULL,0,0};
	unsigned int x,bw;
	int ret = 1,c;

	bw = 8;
	if (vga_msg_box_create(&box,message,2,(bw*2)+2)) {
		x = ((box.w+2-(bw*2))/2)+box.x;
		vga_write_color(0x70);
		vga_moveto(x,box.y+box.h-2);
		vga_write("  ");
		vga_write_color(0x71);
		vga_write("Y");
		vga_write_color(0x70);
		vga_write("es  ");
		vga_moveto(x+bw,box.y+box.h-2);
		vga_write("  ");
		vga_write_color(0x71);
		vga_write("N");
		vga_write_color(0x70);
		vga_write("o  ");

		while (1) {
			vga_menu_idle();
			if (kbhit()) {
				c = getch();
				if (c == 0) c = getch() << 8;

				if (c == 'Y' || c == 'y') {
					ret = 1;
					break;
				}
				else if (c == 'N' || c == 'n') {
					ret = 0;
					break;
				}
				else if (c == 27) {
					ret = 0;
					break;
				}
			}
		}

		vga_msg_box_destroy(&box);
	}

	return ret;
}

