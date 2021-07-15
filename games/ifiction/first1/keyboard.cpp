
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

IFEKeyEvent 		IFEKeyQueue[IFEKeyQueueSize];
unsigned short		IFEKeyQueueHead=0,IFEKeyQueueTail=0;

IFECookedKeyEvent	IFECookedKeyQueue[IFECookedKeyQueueSize];
unsigned short		IFECookedKeyQueueHead=0,IFECookedKeyQueueTail=0;

void IFEKeyQueueEmpty(void) {
	IFEKeyQueueHead = IFEKeyQueueTail = 0;
}

void IFECookedKeyQueueEmpty(void) {
	IFECookedKeyQueueHead = IFECookedKeyQueueTail = 0;
}

void IFEKeyQueueEmptyAll(void) {
	IFEKeyQueueEmpty();
	IFECookedKeyQueueEmpty();
}

