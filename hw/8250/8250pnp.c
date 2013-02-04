
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/8250/8250.h>
#include <hw/isapnp/isapnp.h>

int is_rs232_or_compat_pnp_device(struct isa_pnp_device_node far *devn) {
	char id[9];

	if (devn->type_code[0] == 7 && devn->type_code[1] == 0)
		return 1;

	/* and then there are known devices that act like RS-232 but do not list themselves as RS-232 */
	isa_pnp_product_id_to_str(id,devn->product_id);

	/* Toshiba Satellite Pro 465CDX: The IR port is a 0x07,0x80 ("other" communications device)
	 * when in fact it looks and acts like a UART on IO port 0x3E8 IRQ 9. Identifies itself
	 * as "TOS7009" (TODO: The PnP BIOS also announces something at 0x3E0, is that related to this?).
	 * UART notes: For the most part you can treat it like a serial port BUT there is one catch
	 *             that is related to IR transmission: both computers cannot send bytes at the
	 *             same time. If you do, the light interferes and both computers end up receiving
	 *             gibberish. In some cases, it can throw off the bit clock and make the remaining
	 *             valid data also gibberish. The only way to resolve the issue it seems, is to
	 *             temporarily stop transmitting and give each IR port a chance to reset themselves. */
	if (devn->type_code[0] == 7 && devn->type_code[1] == 0x80 && !memcmp(id,"TOS7009",7))
		return 1;

	return 0;
}

