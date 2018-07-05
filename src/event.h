

#ifndef __EVENT_H__
#define __EVENT_H__

#include "stl.llist.h"

#define EVENT(name) void (name)(Ptr causer, Ptr victim, long info)

struct event_info {
  int ticks_to_go;
  EVENT(*func);
  Ptr causer, victim;
  Ptr info;
  char *caller;
  int line;
  struct event_info *next;
};

/*  The macros provide the type casting useful when writing event drivers. */
#define VICTIM_CH  ((CharData *)victim)
#define CAUSER_CH  ((CharData *)causer)
#define VICTIM_OBJ ((ObjData *)victim)
#define CAUSER_OBJ ((ObjData *)causer)

#define EVENT_CAUSER  1
#define EVENT_VICTIM  2
#define EVENT_BOTH    3

struct event_info *add_an_event(int delay,EVENT(*func), Ptr causer, Ptr victim, Ptr info, char *caller, int line);
void run_events();
void clean_events(Ptr pointer);
void clean_special_events(Ptr pointer, Ptr func, int type);
void clean_causer_events(Ptr causer, Ptr func);
int event_pending(Ptr pointer, Ptr func, int type);
struct event_info * get_event(Ptr pointer, Ptr func, int type);

#if !defined(__GNUC__)
#define __FUNCTION__	__FILE__
#endif

#define add_event(delay, func, causer, victim, info)\
		add_an_event(delay, func, causer, victim, info, __FUNCTION__, __LINE__)


//	The NEW event system
#define EVENTFUNC(name) SInt32 (name)(Ptr event_obj, Event *event)

class QueueElement;

class Event {
public:
	Event(EVENTFUNC(*func), Ptr event_obj, UInt32 when);
	~Event(void);
	SInt32	Run(void);
	void	Cancel(void);
	UInt32	Time(void);
	
	EVENTFUNC(*func);
	Ptr				event_obj;
	QueueElement *	queue;
};

void InitEvents(void);
void ProcessEvents(void);
void FreeAllEvents(void);
Event *FindEvent(LList<Event *> & list, EVENTFUNC(*func));

#endif
