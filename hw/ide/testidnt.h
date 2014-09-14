
int do_ide_identify(unsigned char *buf/*512*/,unsigned int sz,struct ide_controller *ide,unsigned char which,unsigned char command);
int do_ide_set_multiple_mode(struct ide_controller *ide,unsigned char which,unsigned char sz);
void do_drive_identify_device_test(struct ide_controller *ide,unsigned char which,unsigned char command);
int do_ide_flush(struct ide_controller *ide,unsigned char which,unsigned char lba48);
int do_ide_device_diagnostic(struct ide_controller *ide,unsigned char which);
int do_ide_media_card_pass_through(struct ide_controller *ide,unsigned char which,unsigned char enable);

