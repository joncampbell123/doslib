
extern unsigned char		cdrom_read_mode;
extern unsigned char		pio_width_warning;
extern unsigned char		big_scary_write_test_warning;

extern char			tmp[1024];
extern uint16_t			ide_info[256];

#if TARGET_MSDOS == 32
extern unsigned char		cdrom_sector[512U*256U];/* ~128KB, enough for 64 CD-ROM sector or 256 512-byte sectors */
#else
# if defined(__LARGE__) || defined(__COMPACT__)
extern unsigned char		cdrom_sector[512U*16U];	/* ~8KB, enough for 4 CD-ROM sector or 16 512-byte sectors */
# else
extern unsigned char		cdrom_sector[512U*64U];	/* ~32KB, enough for 16 CD-ROM sector or 64 512-byte sectors */
# endif
#endif

void do_ide_controller_hook_irq(struct ide_controller *ide);
void do_ide_controller_unhook_irq(struct ide_controller *ide);
void do_ide_controller_enable_irq(struct ide_controller *ide,unsigned char en);
void do_ide_controller_emergency_halt_irq(struct ide_controller *ide);

