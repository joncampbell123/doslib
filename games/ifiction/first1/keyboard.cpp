
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

IFESimpleQueue<IFEKeyEvent,IFEKeyQueueSize>			IFEKeyQueue;
IFESimpleQueue<IFECookedKeyEvent,IFECookedKeyQueueSize>		IFECookedKeyQueue;

void IFEKeyQueueEmptyAll(void) {
	IFEKeyQueue.clear();
	IFECookedKeyQueue.clear();
}

void IFEKeyboardProcessRawToCooked(const IFEKeyEvent &ev) {
	IFECookedKeyEvent cke;

	memset(&cke,0,sizeof(cke));

#define SCHRMAP(x,norm,shift,ctrl,alt) \
		case x: \
			if (ev.flags & (IFEKeyEvent_FLAG_LALT|IFEKeyEvent_FLAG_RALT)) \
				cke.code = (alt); \
			else if (ev.flags & (IFEKeyEvent_FLAG_LCTRL|IFEKeyEvent_FLAG_RCTRL)) \
				cke.code = (ctrl); \
			else if (ev.flags & (IFEKeyEvent_FLAG_LSHIFT|IFEKeyEvent_FLAG_RSHIFT)) \
				cke.code = (shift); \
			else \
				cke.code = (norm); \
			break;

	switch (ev.code) {
		/* IFEKEY                                        Norm,   Shift,  Ctrl,   Alt */
		SCHRMAP(IFEKEY_RETURN,                           13,     0,      0,      0);
		SCHRMAP(IFEKEY_ESCAPE,                           27,     0,      0,      0);
		SCHRMAP(IFEKEY_BACKSPACE,                        8,      0,      0,      0);
		SCHRMAP(IFEKEY_TAB,                              9,      0,      0,      0);
		SCHRMAP(IFEKEY_SPACE,                            32,     32,     0,      0);
		default: break;
	}

	if (ev.code != 0) {
		if (!IFECookedKeyQueue.add(cke))
			IFEDBG("ProcessKeyboardEvent: Cooked queue full");
	}

#undef SCHRMAP
}

