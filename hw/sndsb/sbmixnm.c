
#include <hw/sndsb/sndsb.h>

static const char *sndsb_mixer_chip_name[SNDSB_MIXER_MAX] = {
	"none",
	"CT1335",
	"CT1345",
	"CT1745",
	"ESS 688"
};

const char *sndsb_mixer_chip_str(const uint8_t c) {
	if (c >= SNDSB_MIXER_MAX) return NULL;
	return sndsb_mixer_chip_name[c];
}

