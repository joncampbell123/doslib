
#ifndef __DOSLIB_HW_VGA_VGAGUI_H
#define __DOSLIB_HW_VGA_VGAGUI_H

#include <hw/cpu/cpu.h>
#include <stdint.h>

#define MAX_MENU_BAR		16

struct vga_menu_item {
	char*		text;
	unsigned char	shortcut_key;
	unsigned char	disabled:1;
	unsigned char	reserved:7;
};

struct vga_menu_bar_item {
	char*				name;
	unsigned char			shortcut_key,shortcut_scan;
	unsigned char			x,w;
	const struct vga_menu_item**	items;
};

struct vga_menu_bar_state {
	const struct vga_menu_bar_item*	bar;
	int				sel;
	unsigned char			row;
};

struct vga_msg_box {
	VGA_ALPHA_PTR	screen,buf;
	unsigned int	w,h,x,y;
};

extern struct vga_menu_bar_state vga_menu_bar;

extern void (*vga_menu_idle)();

void vga_menu_draw_item(VGA_ALPHA_PTR screen,const struct vga_menu_item **scan,unsigned int i,unsigned int w,unsigned int color,unsigned int tcolor);
const struct vga_menu_item *vga_menu_bar_menuitem(const struct vga_menu_bar_item *menu,unsigned char row,unsigned int *spec);
int vga_msg_box_create(struct vga_msg_box *b,const char *msg,unsigned int extra_y,unsigned int min_x);
int vga_menu_item_nonselectable(const struct vga_menu_item *m);
void vga_msg_box_destroy(struct vga_msg_box *b);
int confirm_yes_no_dialog(const char *message);
const struct vga_menu_item *vga_menu_bar_keymon();
void vga_menu_bar_draw();

#endif /* __DOSLIB_HW_VGA_VGAGUI_H */

