
#include <stdint.h>

#pragma pack(push,1)
struct IFEPaletteEntry {
	uint8_t	r,g,b;
};
#pragma pack(pop)

void IFESetPaletteColors(const unsigned int first,const unsigned int count,IFEPaletteEntry *pal);

