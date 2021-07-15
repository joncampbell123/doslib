
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include "ifict.h"
#include "utils.h"
#include "debug.h"
#include "fatal.h"
#include "palette.h"

IFESimpleQueue<IFEKeyEvent,IFEKeyQueueSize>			IFEKeyQueue;
IFESimpleQueue<IFECookedKeyEvent,IFECookedKeyQueueSize>		IFECookedKeyQueue;

void IFEKeyQueueEmptyAll(void) {
	IFEKeyQueue.clear();
	IFECookedKeyQueue.clear();
}

