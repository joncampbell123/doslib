
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include "ifict.h"
#include "utils.h"
#include "debug.h"
#include "fatal.h"
#include "palette.h"

IFESimpleQueue<IFEMouseEvent,IFEMouseQueueSize>			IFEMouseQueue;

void IFEMouseQueueEmptyAll(void) {
	IFEMouseQueue.clear();
}


