
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/vga/vga.h>
#include <hw/vga/vgagui.h>
#include <hw/vga/vgatty.h>
#include <hw/ide/idelib.h>

#include "testutil.h"
#include "test.h"

void common_ide_success_or_error_vga_msg_box(struct ide_controller *ide,struct vga_msg_box *vgabox);
int do_ide_controller_drive_check_select(struct ide_controller *ide,unsigned char which);
void do_common_show_ide_taskfile(struct ide_controller *ide,unsigned char which);
void common_failed_to_read_taskfile_vga_msg_box(struct vga_msg_box *vgabox);
void do_warn_if_atapi_not_in_data_input_state(struct ide_controller *ide);
void do_warn_if_atapi_not_in_complete_state(struct ide_controller *ide);
void do_warn_if_atapi_not_in_command_state(struct ide_controller *ide);
int confirm_pio32_warning(struct ide_controller *ide);
unsigned long prompt_cdrom_sector_number();
unsigned long prompt_cdrom_sector_count();
int prompt_sector_count();

