
extern unsigned char		cdrom_read_mode;
extern unsigned char		pio_width_warning;
extern unsigned char		big_scary_write_test_warning;

extern char			tmp[1024];
extern uint16_t			ide_info[256];
extern unsigned char		cdrom_sector[512U*64U];	/* ~32KB, enough for 16 CD-ROM sector or 64 512-byte sectors */

