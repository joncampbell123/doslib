
#include <hw/cpu/cpu.h>
#include <stdint.h>

char *vga_gets(unsigned int maxlen);
void vga_moveto(unsigned char x,unsigned char y);
void vga_scroll_up(unsigned char lines);
void vga_cursor_down();
void vga_writec(char c);
void vga_write(const char *msg);
void vga_write_sync();
void vga_clear();

