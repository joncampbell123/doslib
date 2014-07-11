
/* NTS: The caller is expected to pnp_wake_scn() then siphon off the device id */
int isa_pnp_sound_blaster_get_resources(uint32_t id,unsigned char csn,struct sndsb_ctx *cx);
int isa_pnp_bios_sound_blaster_get_resources(uint32_t id,unsigned char node,struct isa_pnp_device_node far *devn,unsigned int devn_size,struct sndsb_ctx *cx);
/* NTS: same for this function */
int sndsb_try_isa_pnp(uint32_t id,uint8_t csn);
int sndsb_try_isa_pnp_bios(uint32_t id,uint8_t node,struct isa_pnp_device_node far *devn,unsigned int devn_size);

/* possible error codes (zero or negative) from try_isa_pnp and get_resources */

/* Error: The Sound Blaster failed the I/O and DSP checks */
#define ISAPNPSB_FAILED				0
/* Error: PnP card found but none or insufficient resources are assigned to it (and this library by default will not auto-assign for OBVIOUS SAFETY REASONS) */
#define ISAPNPSB_NO_RESOURCES			-1
/* Error: There are already too many cards being tracked by the library */
#define ISAPNPSB_TOO_MANY_CARDS			-2

/* end error codes */

int isa_pnp_is_sound_blaster_compatible_id(uint32_t id,char const **whatis);
int isa_pnp_iobase_typical_mpu(uint16_t io);
int isa_pnp_iobase_typical_sb(uint16_t io);

