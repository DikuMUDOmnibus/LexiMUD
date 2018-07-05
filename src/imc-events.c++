/*
 * IMC2 - an inter-mud communications protocol
 *
 * imc-events.c: IMC event handling
 *
 * Copyright (C) 1996,1997 Oliver Jowett <oliver@jowett.manawatu.planet.co.nz>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, write to the
 * Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "imc.h"

#if !defined(macintosh)
#include <arpa/inet.h>
#endif

#include "utils.h"

/*
 * I needed to split up imc.c (2600 lines and counting..) so this file now
 * contains:
 *
 * - event support functions
 * - event callbacks
 */


/*
 *  event support functions
 */

/* we use size-limited recycle lists, since events will be being
 * queued/unqueued quite often
 */

LList<IMCEvent *>	imc_event_list;
static int event_freecount;

IMCEvent::IMCEvent(void) : when(0), callback(NULL), data(NULL), timed(0) {
}

IMCEvent::~IMCEvent(void) {
}


void imc_add_event(int when, void (*callback)(Ptr), Ptr data, int timed) {
	IMCEvent *p, *scan;

	p = new IMCEvent();
	p->when		= imc_now + when;
	p->callback	= callback;
	p->data		= data;
	p->timed	= timed;
	
	if (!imc_event_list.Count())	//	Hehe, little speed hack.
		imc_event_list.Add(p);
	else {
		LListIterator<IMCEvent *>	iter(imc_event_list);
		while ((scan = iter.Next()))
			if (scan->when >= p->when)
				break;
		
		imc_event_list.InsertBefore(p, scan);
	}
/*
	for (last = &imc_event_list, scan = *last; scan;
			last = &scan->next, scan=*last)
		if (scan->when >= p->when)
			break;

	p->next=scan;
	*last = p;
*/
}

void imc_cancel_event(void (*callback)(Ptr), Ptr data) {
	IMCEvent *p;

/*	for (last = &imc_event_list, p = *last; p; p = p_next) {
		p_next=p->next;

		if ((!callback || p->callback == callback) && p->data == data) {
			*last = p_next;
			delete p;
		} else
			last = &p->next;
	}
*/
	START_ITER(iter, p, IMCEvent *, imc_event_list) {
		if ((!callback || p->callback == callback) && p->data == data) {
			imc_event_list.Remove(p);
			delete p;
		}
	} END_ITER(iter);
}

void imc_run_events(time_t newtime) {
	IMCEvent *	p;
	void		(*callback)(Ptr);
	Ptr			data;

	while((p = imc_event_list.Top())) {
		if (p->when > newtime)
				break;

		imc_event_list.Remove(p);
		callback = p->callback;
		data = p->data;
		imc_now = p->when;

		delete p;

		if (callback)	(*callback)(data);
		else			imc_logerror("imc_run_events: NULL callback");
	}

	imc_now = newtime;
}

int imc_next_event(void (*callback)(Ptr), Ptr data) {
	IMCEvent *p;

	LListIterator<IMCEvent *>	iter(imc_event_list);
	while((p = iter.Next()))
		if (p->callback==callback && p->data==data)
			return p->when - imc_now;

	return -1;
}



/*
 *  Events
 */

/* expire the imc_reminfo entry pointed at by data */
void ev_expire_reminfo(Ptr data) {
	IMCRemInfo *r=(IMCRemInfo *)data;

	r->type=IMC_REMINFO_EXPIRED;
	imc_cancel_event(NULL, data);
	imc_add_event(IMC_DROP_TIMEOUT, ev_drop_reminfo, data, 0);
}

/* drop the imc_reminfo entry */
void ev_drop_reminfo(Ptr data)
{
  imc_cancel_event(NULL, data);
  delete (IMCRemInfo *)data;
}

/* send a keepalive, and queue the next keepalive event */
void ev_keepalive(Ptr data)
{
  imc_send_keepalive();
  imc_add_event(IMC_KEEPALIVE_TIME, ev_keepalive, NULL, 1);
}

/* maybe shrink the input buffer of the connection 'data' */
void ev_shrink_input(Ptr data) {
	IMCConnect *c = (IMCConnect *)data;
	SInt32		len, newsize;
	Ptr			newbuf;

	len = strlen(c->inbuf)+1;
	newsize = c->insize/2;

	if (len<=newsize) {
		if (newsize<IMC_MINBUF) /* huh? should never happen.. */
			return;

		/* shrink the buffer one step */
		CREATE(newbuf, char, newsize);
		strcpy((char *)newbuf, c->inbuf);
		free(c->inbuf);
		c->inbuf=(char *)newbuf;
		c->insize=newsize;

		if (newsize<=IMC_MINBUF)
			return;
	}
  
	imc_add_event(IMC_SHRINKTIME, ev_shrink_input, c, 0);
}

/* maybe shrink the output buffer of the connection 'data' */
void ev_shrink_output(Ptr data)
{
  IMCConnect *	c = (IMCConnect *)data;
  SInt32		len, newsize;
  Ptr			newbuf;

  len=strlen(c->outbuf)+1;
  newsize=c->outsize/2;

  if (len<=newsize)
  {
    if (newsize<IMC_MINBUF) /* huh? should never happen.. */
      return;

    /* shrink the buffer one step */
    CREATE(newbuf, char, newsize);
    strcpy((char *)newbuf, c->outbuf);
    free(c->outbuf);
    c->outbuf=(char *)newbuf;
    c->outsize=newsize;

    if (newsize<=IMC_MINBUF)
      return;
  }
  
  imc_add_event(IMC_SHRINKTIME, ev_shrink_output, c, 0);
}

/* try a reconnect to the given imc_info */
void ev_reconnect(Ptr data)
{
  IMCInfo *info=(IMCInfo *)data;

  if (!info->connection &&
      (info->flags & IMC_RECONNECT))
  {
    if (!imc_connect_to(info->name))
      info->SetupReconnect();
  }
}

/* handle a spam event for counter 1 */
void ev_spam1(Ptr data)
{
  IMCConnect *c=(IMCConnect *)data;

  if (c->spamcounter1 > IMC_SPAM1MAX)
  {
    if (c->spamtime1 < 0 || ++c->spamtime1 >= IMC_SPAM1TIME)
      c->spamtime1=-IMC_SPAM1TIME;
  }
  else
  {
    if (c->spamtime1<0)
      c->spamtime1++;
  }

  c->spamcounter1=0;
  if (c->spamtime1>0)
    imc_add_event(IMC_SPAM1INTERVAL, ev_spam1, c, 0);
  else if (c->spamtime1<0)
    imc_add_event(IMC_SPAM1INTERVAL, ev_spam1, c, 1);
}

/* handle a spam event for counter 2 */
void ev_spam2(Ptr data)
{
  IMCConnect *c=(IMCConnect *)data;

  if (c->spamcounter2 > IMC_SPAM2MAX)
  {
    if (c->spamtime2 < 0 || ++c->spamtime2 >= IMC_SPAM2TIME)
      c->spamtime2=-IMC_SPAM2TIME;
  }
  else
    if (c->spamtime2<0)
      c->spamtime2++;

  c->spamcounter2=0;
  if (c->spamtime2>0)
    imc_add_event(IMC_SPAM1INTERVAL, ev_spam2, c, 0);
  else if (c->spamtime2<0)
    imc_add_event(IMC_SPAM1INTERVAL, ev_spam2, c, 1);
}

