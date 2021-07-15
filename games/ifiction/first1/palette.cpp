
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include "utils.h"
#include "debug.h"
#include "fatal.h"
#include "palette.h"

void priv_SetPaletteColorsRangeCheck(const unsigned int first,const unsigned int count) {
	if (first >= 256u || count > 256u || (first+count) > 256u)
		IFEFatalError("SetPaletteColors out of range first=%u count=%u",first,count);
}

