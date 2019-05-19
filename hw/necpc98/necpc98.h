
#pragma pack(push,1)
struct nec_pc98_state_t {
	uint8_t			probed:1;
	uint8_t			is_pc98:1;
};
#pragma pack(pop)

extern struct nec_pc98_state_t		nec_pc98;

int probe_nec_pc98();

static inline unsigned char vram_pc98_doublewide(const uint16_t chcode) {
    if (chcode & 0xFF00u) {
        if ((chcode & 0x7Cu) != 0x08u)
            return 1;
    }

    return 0;
}

