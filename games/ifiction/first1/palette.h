
#include "fatal.h"

#include <stdint.h>

#pragma pack(push,1)
struct IFEPaletteEntry {
	uint8_t	r,g,b;
};
#pragma pack(pop)

void IFESetPaletteColors(const unsigned int first,const unsigned int count,IFEPaletteEntry *pal);

#define IFESETPALETTEASSERT(first,count) do { \
	if (first >= 256u || count > 256u || (first+count) > 256u) \
		IFEFatalError("SetPaletteColors out of range first=%u count=%u",first,count); \
} while (0)

