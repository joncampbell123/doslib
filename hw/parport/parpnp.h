
#include <hw/isapnp/isapnp.h>
#include <hw/cpu/cpu.h>
#include <stdint.h>
#include <conio.h>

int is_parport_or_compat_pnp_device(struct isa_pnp_device_node far *devn);
void pnp_parport_scan();

