
#ifndef IFE_PALETTE_H
#define IFE_PALETTE_H

#include <stdint.h>

#pragma pack(push,1)
struct IFEPaletteEntry {
	uint8_t	r,g,b;
};
#pragma pack(pop)

void priv_SetPaletteColorsRangeCheck(const unsigned int first,const unsigned int count);

typedef void ifefunc_SetPaletteColors_t(const unsigned int first,const unsigned int count,IFEPaletteEntry *pal);

#endif //IFE_PALETTE_H

