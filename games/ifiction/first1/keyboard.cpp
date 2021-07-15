
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

template <typename T,const unsigned short QueueSize> size_t IFESimpleQueue<T,QueueSize>::queueSize(void) const {
	return QueueSize;
}

template <typename T,const unsigned short QueueSize> void IFESimpleQueue<T,QueueSize>::clear(void) {
	head = tail = 0;
}

template <typename T,const unsigned short QueueSize> void IFESimpleQueue<T,QueueSize>::clearold(void) {
	if (head != 0) {
		unsigned short i;

		for (i=head;i < tail;i++)
			queue[i-head] = queue[i];

		tail -= head;
		head = 0;
	}
}

template <typename T,const unsigned short QueueSize> bool IFESimpleQueue<T,QueueSize>::add(const T *ev) {
	if (tail == QueueSize)
		clearold();

	if (tail < QueueSize) {
		queue[tail++] = *ev;
		return true;
	}

	return false;
}

template <typename T,const unsigned short QueueSize> T* IFESimpleQueue<T,QueueSize>::get(void) {
	if (head < tail)
		return &queue[head++];

	return NULL;
}

void IFEKeyQueueEmptyAll(void) {
	IFEKeyQueue.clear();
	IFECookedKeyQueue.clear();
}

