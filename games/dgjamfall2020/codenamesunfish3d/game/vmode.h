
#define VGA_PAGE_FIRST          0x0000
#define VGA_PAGE_SECOND         0x4000

extern VGA_RAM_PTR orig_vga_graphics_ram;
extern unsigned int vga_cur_page,vga_next_page;

void restore_text_mode();
void init_vga256unchained();

