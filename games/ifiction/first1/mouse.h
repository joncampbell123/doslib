
#include "simplequeue.h"

struct IFEMouseStatus {
	int			x;
	int			y;
	uint32_t		status;
};

/* mouse movement is not an event */
struct IFEMouseEvent {
	int			x;
	int			y;
	uint32_t		pstatus;
	uint32_t		status;
};

#define IFEMouseQueueSize		256

extern IFESimpleQueue<IFEMouseEvent,IFEMouseQueueSize>			IFEMouseQueue;

#define IFEMouseStatus_LBUTTON		(1u << 0u)
#define IFEMouseStatus_MBUTTON		(1u << 1u)
#define IFEMouseStatus_RBUTTON		(1u << 2u)

#define IFEStdCursor_NONE		(0u)
#define IFEStdCursor_POINTER		(1u)
#define IFEStdCursor_WAIT		(2u)

/* moved: mouse moved */
/* rel moved: mouse movement was relative and added to rx/ry */
/* abs moved: mouse movement was absolute and set to x/y (i.e. touchscreen, tablet, DOSBox-X mouse integration) */

void IFEMouseQueueEmptyAll(void);

