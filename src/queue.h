
#ifndef __QUEUE_H__
#define __QUEUE_H__

#include "types.h"

#define NUM_EVENT_QUEUES	10

class QueueElement {
public:
	QueueElement(Ptr data, SInt32 key);
	
	Ptr		data;
	SInt32	key;
	QueueElement *prev, *next;
};

class Queue {
public:
	Queue(void);
	~Queue(void);
	QueueElement *	Enqueue(Ptr data, SInt32 key);
	void			Dequeue(QueueElement *qe);
	Ptr				QueueHead(void);
	SInt32			QueueKey(void);
	
	QueueElement *head[NUM_EVENT_QUEUES], *tail[NUM_EVENT_QUEUES];
};

#endif