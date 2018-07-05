#include "conf.h"
#include "sysdep.h"

#include "queue.h"

//	External variables
extern UInt32 pulse;

QueueElement::QueueElement(Ptr data, SInt32 key) {
	this->data = data;
	this->key = key;
	this->next = NULL;
	this->prev = NULL;
}


Queue::Queue(void) {
	memset(this, 0, sizeof(Queue));
}


Queue::~Queue(void) {
	SInt32 i;
	QueueElement *qe, *next_qe;
	
	for (i = 0; i < NUM_EVENT_QUEUES; i++) {
		for (qe = this->head[i]; qe; qe = next_qe) {
			next_qe = qe->next;
			delete qe;
		}
	}
}


QueueElement *Queue::Enqueue(Ptr data, SInt32 key) {
	QueueElement *qe, *i;
	SInt32	bucket;
	
	qe = new QueueElement(data, key);
	bucket = key % NUM_EVENT_QUEUES;
	
	if (!this->head[bucket]) {
		this->head[bucket] = qe;
		this->tail[bucket] = qe;
	} else {
		for (i = this->tail[bucket]; i; i = i->prev) {
			if (i->key < key) {
				if (i == this->tail[bucket])
					this->tail[bucket] = qe;
				else {
					qe->next = i->next;
					i->next->prev = qe;
				}
				qe->prev = i;
				i->next = qe;
				break;
			}
		}
		if (i == NULL) {
			qe->next = this->head[bucket];
			this->head[bucket] = qe;
			qe->next->prev = qe;
		}
	}
	return qe;
}


void Queue::Dequeue(QueueElement *qe) {
	SInt32 i;
	
	if (!qe)	return;
	
	i = qe->key % NUM_EVENT_QUEUES;
	
	if (qe->prev)	qe->prev->next = qe->next;
	else			this->head[i] = qe->next;
	
	if (qe->next)	qe->next->prev = qe->prev;
	else			this->tail[i] = qe->prev;
	
	delete qe;
}


Ptr Queue::QueueHead(void) {
	Ptr data;
	SInt32 i;
	
	i = pulse % NUM_EVENT_QUEUES;
	
	if (!this->head[i])
		return NULL;
	
	data = this->head[i]->data;
	this->Dequeue(this->head[i]);
	
	return data;
}


SInt32 Queue::QueueKey(void) {
	SInt32 i;
	
	i = pulse % NUM_EVENT_QUEUES;
	
	if (this->head[i])	return this->head[i]->key;
	else				return LONG_MAX;
}
