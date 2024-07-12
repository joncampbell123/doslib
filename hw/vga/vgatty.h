
#include <hw/cpu/cpu.h>
#include <stdint.h>

extern unsigned char vga_break_signal;

char *vga_gets(unsigned int maxlen);
void vga_moveto(unsigned char x,unsigned char y);
void vga_scroll_up(unsigned char lines);
void vga_cursor_down();
void vga_writec(char c);
void vga_write(const char *msg);
void vga_write_sync();
void vga_clear();

int vga_getch();

#if defined(TARGET_PC98)
# include <hw/necpc98/necpc98.h>
void vga_tty_pc98_mapping(nec_pc98_intdc_keycode_state_ext *map);
void vga_writecw(unsigned short c);
void vga_writew(const char *msg);

# define VGATTY_SC_Fn(x)        (0x7F30 + (x) - 1)
# define VGATTY_SC_VFn(x)       (0x7F3A + (x) - 1)
# define VGATTY_SC_sFn(x)       (0x7F40 + (x) - 1)
# define VGATTY_SC_sVFn(x)      (0x7F4A + (x) - 1)
# define VGATTY_SC_cFn(x)       (0x7F50 + (x) - 1)
# define VGATTY_SC_cVFn(x)      (0x7F5A + (x) - 1)
# define VGATTY_LEFT_ARROW      (0x7F60 + PC98_INTDC_EK_LEFT_ARROW)
# define VGATTY_RIGHT_ARROW     (0x7F60 + PC98_INTDC_EK_RIGHT_ARROW)
# define VGATTY_UP_ARROW        (0x7F60 + PC98_INTDC_EK_UP_ARROW)
# define VGATTY_DOWN_ARROW      (0x7F60 + PC98_INTDC_EK_DOWN_ARROW)
# define VGATTY_ALT_LEFT_ARROW  (0x7F60 + PC98_INTDC_EK_LEFT_ARROW)
# define VGATTY_ALT_RIGHT_ARROW (0x7F60 + PC98_INTDC_EK_RIGHT_ARROW)
# define VGATTY_ALT_UP_ARROW    (0x7F60 + PC98_INTDC_EK_UP_ARROW)
# define VGATTY_ALT_DOWN_ARROW  (0x7F60 + PC98_INTDC_EK_DOWN_ARROW)
#else
# define VGATTY_SC_Fn(x)        (0x3B00 + ((x) << 8) - 0x100 + ((x) >= 11 ? (0x8500-0x4500) : 0))
# define VGATTY_SC_sFn(x)       (0x5400 + ((x) << 8) - 0x100 + ((x) >= 11 ? (0x8700-0x5E00) : 0))
# define VGATTY_SC_cFn(x)       (0x5E00 + ((x) << 8) - 0x100 + ((x) >= 11 ? (0x8900-0x6800) : 0))
# define VGATTY_LEFT_ARROW      (0x4B00)
# define VGATTY_RIGHT_ARROW     (0x4D00)
# define VGATTY_UP_ARROW        (0x4800)
# define VGATTY_DOWN_ARROW      (0x5000)
# define VGATTY_ALT_LEFT_ARROW  (0x9B00)
# define VGATTY_ALT_RIGHT_ARROW (0x9D00)
# define VGATTY_ALT_UP_ARROW    (0x9800)
# define VGATTY_ALT_DOWN_ARROW  (0xA000)
#endif

