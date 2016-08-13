
int sndsb_sb16_asp_probe_chip_ram_size(struct sndsb_ctx *cx);
int sndsb_sb16asp_set_codec_parameter(struct sndsb_ctx *cx,uint8_t param,uint8_t value);
int sndsb_sb16asp_read_last_codec_parameter(struct sndsb_ctx *cx);
int sndsb_sb16asp_set_mode_register(struct sndsb_ctx *cx,uint8_t val);
int sndsb_sb16asp_set_register(struct sndsb_ctx *cx,uint8_t reg,uint8_t val);
int sndsb_sb16asp_get_register(struct sndsb_ctx *cx,uint8_t reg);
int sndsb_sb16asp_get_version(struct sndsb_ctx *cx);
int sndsb_check_for_sb16_asp(struct sndsb_ctx *cx);
int sndsb_sb16_asp_ram_test(struct sndsb_ctx *cx);

