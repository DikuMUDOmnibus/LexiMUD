/*************************************************************************
*   File: event.c++                  Part of Aliens vs Predator: The MUD *
*  Usage: Primary code for events                                        *
*************************************************************************/

#include "types.h"
#include "event.h"
#include "queue.h"
#include "utils.h"

void run_events(void);

int in_event_handler=0;
struct event_info *pending_events = NULL;
struct event_info *prev = NULL;

void run_events(void) {
  struct event_info *temp, *prox;

  in_event_handler=1;

  prev=NULL;
  for(temp=pending_events;temp;temp=prox) {
    prox=temp->next;
    temp->ticks_to_go--;
    if (temp->ticks_to_go==0) {
      
      /* run the event */
      if (!temp->func)
        log("SYSERR: Attempting to run a NULL event. Ignoring");
      else
        (temp->func)(temp->causer,temp->victim,(long)temp->info);

      /* remove the event from the list. */
      if (!prev)
        pending_events=prox;
      else
        prev->next=prox;
      FREE(temp);
    } else if (temp->ticks_to_go<=-1) {
      if (!prev)
        pending_events=prox;
      else
        prev->next=prox;
      FREE(temp);
    } else  
      prev=temp;
  }; 

  in_event_handler=0;
}

struct event_info *add_an_event(int delay, EVENT(*func), Ptr causer, Ptr victim, Ptr info, char*caller, int line)
{
  struct event_info *new_event;

  CREATE(new_event,struct event_info,1);

  new_event->ticks_to_go=delay;
  new_event->func=func;
  new_event->causer=causer;
  new_event->victim=victim;
  new_event->info=info;
  
  new_event->caller = caller;
  new_event->line = line;
  
  new_event->next=pending_events;
  pending_events=new_event;
  if (in_event_handler && !prev)
    prev=pending_events;
  return new_event;
}

void clean_events(Ptr pointer)
{
  struct event_info *temp,*prox;
  EVENT(grenade_explosion);
/*  struct event_info *previous;*/

/*  if (in_event_handler)
    log("SYSERR: Trying to remove events inside the handler. Attempting to continue.");*/
/*  previous=NULL;*/
  for(temp=pending_events;temp;temp=prox) {
    prox=temp->next;
    if (temp->causer==pointer || temp->victim==pointer ||
        (Ptr)(temp->func)==pointer)
      temp->ticks_to_go=0;
    if ((Ptr)(temp->func)==grenade_explosion && temp->info==pointer)
    	temp->info = NULL;
  };
}

void clean_special_events(Ptr pointer, Ptr func, int type) {
	struct event_info *temp,*prox;
	
	for(temp=pending_events;temp;temp=prox) {
		prox=temp->next;
		if (((temp->causer==pointer && (type & EVENT_CAUSER)) ||
				(temp->victim==pointer && (type & EVENT_VICTIM))) &&
				((!func) || ((Ptr)(temp->func)==func)))
			temp->ticks_to_go=0;
	}
}


void clean_causer_events(Ptr causer, Ptr func) {
  struct event_info *temp,*prox;

  for(temp=pending_events;temp;temp=prox) {
    prox=temp->next;
    if (temp->causer==causer && (Ptr)(temp->func)==func)
      temp->ticks_to_go=0;
  };
}



int event_pending(Ptr pointer, Ptr func, int type) {
	struct event_info *temp,*prox;
	
	for(temp=pending_events;temp;temp=prox) {
		prox=temp->next;
		if (((temp->causer==pointer && (type & EVENT_CAUSER)) ||
				(temp->victim==pointer && (type & EVENT_VICTIM))) &&
				(temp->ticks_to_go > 0) &&
				(Ptr)(temp->func)==func)
			return TRUE;
	}
	return FALSE;
}


struct event_info * get_event(Ptr pointer, Ptr func, int type) {
	struct event_info *temp,*prox;
	
	for(temp=pending_events;temp;temp=prox) {
		prox=temp->next;
		if (((temp->causer==pointer && (type & EVENT_CAUSER)) ||
				(temp->victim==pointer && (type & EVENT_VICTIM))) &&
				(temp->ticks_to_go > 0) &&
				(!func || (Ptr)(temp->func)==func))
			return temp;
	}
	return NULL;
}


// New Event System

extern UInt32 pulse;
Queue *EventQueue;


void InitEvents(void) {
	EventQueue = new Queue;
}


void ProcessEvents(void) {
	Event *	TheEvent;
	SInt32	NewTime;
	
	while (pulse >= EventQueue->QueueKey()) {
		if (!(TheEvent = (Event *)EventQueue->QueueHead())) {
			log("SYSERR: Attempt to get a NULL event!");
			return;
		}
		if ((NewTime = TheEvent->Run()) > 0)
			TheEvent->queue = EventQueue->Enqueue(TheEvent, NewTime + pulse);
		else {
			delete TheEvent;
		}
	}
}


void FreeAllEvents(void) {
	Event *TheEvent;
	while ((TheEvent = (Event *)EventQueue->QueueHead())) {
		delete TheEvent;
	}
	delete EventQueue;
}


Event::Event(EVENTFUNC(*func), Ptr event_obj, UInt32 when) {
	if (when < 1)
		when = 1;
	
	this->func = func;
	this->event_obj = event_obj;
	this->queue = EventQueue->Enqueue(this, when + pulse);
}


Event::~Event(void) {
	if (this->event_obj)
		FREE(this->event_obj);
}


UInt32 Event::Time(void) {
	return (this->queue->key - pulse);
}


SInt32 Event::Run(void) {
	return (this->func)(this->event_obj, this);
}


void Event::Cancel(void) {
	EventQueue->Dequeue(this->queue);
	delete this;
}


Event *FindEvent(LList<Event *> & list, EVENTFUNC(*func)) {
	Event *	event;
	LListIterator<Event *>	iter(list);
	
	while ((event = iter.Next())) {
		if (event->func == func)
			break;
	}
	return event;
}
