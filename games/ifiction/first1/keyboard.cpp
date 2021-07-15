
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

void IFEKeyQueueMoveBack(void) {
	unsigned short i;

	if (IFEKeyQueueHead != 0) {
		for (i=IFEKeyQueueHead;i < IFEKeyQueueTail;i++)
			IFEKeyQueue[i-IFEKeyQueueHead] = IFEKeyQueue[i];

		IFEKeyQueueTail -= IFEKeyQueueHead;
		IFEKeyQueueHead = 0;
	}
}

bool IFEKeyQueueAdd(const IFEKeyEvent *ev) {
	if (IFEKeyQueueTail == IFEKeyQueueSize)
		IFEKeyQueueMoveBack();

	if (IFEKeyQueueTail < IFEKeyQueueSize) {
		IFEKeyQueue[IFEKeyQueueTail++] = *ev;
		return true;
	}

	return false;
}

void IFEKeyQueueEmpty(void) {
	IFEKeyQueueHead = IFEKeyQueueTail = 0;
}

void IFECookedKeyQueueMoveBack(void) {
	unsigned short i;

	if (IFECookedKeyQueueHead != 0) {
		for (i=IFECookedKeyQueueHead;i < IFECookedKeyQueueTail;i++)
			IFECookedKeyQueue[i-IFECookedKeyQueueHead] = IFECookedKeyQueue[i];

		IFECookedKeyQueueTail -= IFECookedKeyQueueHead;
		IFECookedKeyQueueHead = 0;
	}
}

bool IFECookedKeyQueueAdd(const IFECookedKeyEvent *ev) {
	if (IFECookedKeyQueueTail == IFECookedKeyQueueSize)
		IFECookedKeyQueueMoveBack();

	if (IFECookedKeyQueueTail < IFECookedKeyQueueSize) {
		IFECookedKeyQueue[IFECookedKeyQueueTail++] = *ev;
		return true;
	}

	return false;
}

void IFECookedKeyQueueEmpty(void) {
	IFECookedKeyQueueHead = IFECookedKeyQueueTail = 0;
}

void IFEKeyQueueEmptyAll(void) {
	IFEKeyQueueEmpty();
	IFECookedKeyQueueEmpty();
}

