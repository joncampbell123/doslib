
#include "simplequeue.h"

struct IFEMouseStatus {
	int			x;
	int			y;
	int			rx;
	int			ry;
	uint32_t		status;
};

/* NTS: Mouse movement is accumulated before adding to queue, just like SDL2 */
struct IFEMouseEvent {
	int			x;
	int			y;
	uint32_t		status;
};

#define IFEMouseQueueSize		256

extern IFESimpleQueue<IFEMouseEvent,IFEMouseQueueSize>			IFEMouseQueue;

#define IFEMouseStatus_LBUTTON		(1u << 0u)
#define IFEMouseStatus_MBUTTON		(1u << 1u)
#define IFEMouseStatus_RBUTTON		(1u << 2u)
#define IFEMouseStatus_BUTTONCLICK	(1u << 3u)
#define IFEMouseStatus_MOVED		(1u << 4u)
#define IFEMouseStatus_RELMOVED		(1u << 5u)
#define IFEMouseStatus_ABSMOVED		(1u << 6u)

/* moved: mouse moved */
/* rel moved: mouse movement was relative and added to rx/ry */
/* abs moved: mouse movement was absolute and set to x/y (i.e. touchscreen, tablet, DOSBox-X mouse integration) */

