
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
		SCHRMAP(IFEKEY_A,                                'a',    'A',    0x01,   0);
		SCHRMAP(IFEKEY_B,                                'b',    'B',    0x02,   0);
		SCHRMAP(IFEKEY_C,                                'c',    'C',    0x03,   0);
		SCHRMAP(IFEKEY_D,                                'd',    'D',    0x04,   0);
		SCHRMAP(IFEKEY_E,                                'e',    'E',    0x05,   0);
		SCHRMAP(IFEKEY_F,                                'f',    'F',    0x06,   0);
		SCHRMAP(IFEKEY_G,                                'g',    'G',    0x07,   0);
		SCHRMAP(IFEKEY_H,                                'h',    'H',    0x08,   0);
		SCHRMAP(IFEKEY_I,                                'i',    'I',    0x09,   0);
		SCHRMAP(IFEKEY_J,                                'j',    'J',    0x0A,   0);
		SCHRMAP(IFEKEY_K,                                'k',    'K',    0x0B,   0);
		SCHRMAP(IFEKEY_L,                                'l',    'L',    0x0C,   0);
		SCHRMAP(IFEKEY_M,                                'm',    'M',    0x0D,   0);
		SCHRMAP(IFEKEY_N,                                'n',    'N',    0x0E,   0);
		SCHRMAP(IFEKEY_O,                                'o',    'O',    0x0F,   0);
		SCHRMAP(IFEKEY_P,                                'p',    'P',    0x10,   0);
		SCHRMAP(IFEKEY_Q,                                'q',    'Q',    0x11,   0);
		SCHRMAP(IFEKEY_R,                                'r',    'R',    0x12,   0);
		SCHRMAP(IFEKEY_S,                                's',    'S',    0x13,   0);
		SCHRMAP(IFEKEY_T,                                't',    'T',    0x14,   0);
		SCHRMAP(IFEKEY_U,                                'u',    'U',    0x15,   0);
		SCHRMAP(IFEKEY_V,                                'v',    'V',    0x16,   0);
		SCHRMAP(IFEKEY_W,                                'w',    'W',    0x17,   0);
		SCHRMAP(IFEKEY_X,                                'x',    'X',    0x18,   0);
		SCHRMAP(IFEKEY_Y,                                'y',    'Y',    0x19,   0);
		SCHRMAP(IFEKEY_Z,                                'z',    'Z',    0x1A,   0);
#if 0

    IFEKEY_1 = 30,
    IFEKEY_2 = 31,
    IFEKEY_3 = 32,
    IFEKEY_4 = 33,
    IFEKEY_5 = 34,
    IFEKEY_6 = 35,
    IFEKEY_7 = 36,
    IFEKEY_8 = 37,
    IFEKEY_9 = 38,
    IFEKEY_0 = 39,


#endif

		default: break;
	}

	if (ev.code != 0) {
		if (!IFECookedKeyQueue.add(cke))
			IFEDBG("ProcessKeyboardEvent: Cooked queue full");
	}

#undef SCHRMAP
}

