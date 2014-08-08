
void do_construct_atapi_scsi_mmc_read(unsigned char *buf/*must be 12 bytes*/,uint32_t sector,uint32_t tlen_sect,unsigned char read_mode);
int ide_memcmp_every_other_word(unsigned char *pio16,unsigned char *pio32,unsigned int words);
int do_ide_controller_atapi_device_check_post_host_reset(struct ide_controller *ide);
int ide_memcmp_all_FF(unsigned char *d,unsigned int bytes);
unsigned char sanitizechar(unsigned char c);
int wait_for_enter_or_escape();


