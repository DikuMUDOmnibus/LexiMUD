/*
 * IMC2 - an inter-mud communications protocol
 *
 * imc-interp.c: packet interpretation code
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
#include <sys/ioctl.h>

#if defined(macintosh) || defined(CIRCLE_WINDOWS)
void gettimeofday(struct timeval *t, struct timezone *dummy);
#endif

#include "utils.h"

LList<IMCIgnore *>	imc_ignore_list;	//	rignore'd people
char *imc_prefix;						//	prefixes for all data files

/* called when a keepalive has been received */
void imc_recv_keepalive(const char *from, const char *version, const char *flags) {
	IMCRemInfo *p;

	if (!str_cmp(from, imc_name))
		return;
  
	//  this should never fail, imc.c should create an entry if one doesn't exist
	//	(in the path update code)
	p = imc_find_reminfo(from, 0);
	if (!p)		    /* boggle */
		return;

	p->hide = imc_hasname(flags, "hide") ? 1 : 0;
	
	//	lower-level code has already updated p->alive
	if (str_cmp(version, p->version)) {		//	remote version has changed?
		FREE(p->version);					//	if so, update it
		p->version = str_dup(version);
	}

	//	Only routers should ping - and even then, only directly connected muds
	if (imc_is_router && imc_getinfo(from)) {
		struct timeval tv;
		gettimeofday(&tv, NULL);

		imc_send_ping(from, tv.tv_sec, tv.tv_usec);
	}
}

/* called when a ping request is received */
void imc_recv_ping(const char *from, int time_s, int time_u, const char *path)
{
  /* ping 'em back */
  imc_send_pingreply(from, time_s, time_u, path);
}

/* called when a ping reply is received */
void imc_recv_pingreply(const char *from, int time_s, int time_u, const char *pathto, const char *pathfrom) {
	IMCRemInfo *p;
	struct timeval tv;

	p = imc_find_reminfo(from, 0);	//	should always exist
	if (!p)							//	boggle
		return;

	gettimeofday(&tv, NULL);		//	grab the exact time now and calc RTT
	p->ping = (tv.tv_sec - time_s) * 1000 + (tv.tv_usec - time_u) / 1000;

	//	check for pending traceroutes
	imc_traceroute(p->ping, pathto, pathfrom);
}

/* send a standard 'you are being ignored' rtell */
void imc_sendignore(const char *to)
{
  char buf[IMC_DATA_LENGTH];

  if (strcmp(imc_nameof(to), "*"))
  {
    sprintf(buf, "%s is ignoring you.", imc_name);
    imc_send_tell(NULL, to, buf, 1);
  }
}

/* imc_char_data representation:
 *
 *  Levels are simplified: >0 is a mortal, <0 is an immortal. The 'see' and
 *  'invis' fields are no longer used.
 *
 *  d->wizi  is -1 if the character is invisible to mortals (hidden/invis or
 *           wizi)
 *  d->level is the level of the character (-1=imm, 1=mortal)
 *
 *  also checks rignores for a 'notrust' flag which makes that person a
 *  level 1 mortal for the purposes of wizi visibility checks, etc
 *
 *  Default behavior is now: trusted.
 *  If there's a notrust flag, untrusted. If there's also a trust flag, trusted
 */

/* convert from the char data in 'p' to an internal representation in 'd' */
void IMCPacket::GetData(IMCCharData *d) {
	int trust = true;

	if (imc_findignore(this->from, IMC_NOTRUST))	trust = false;
	if (imc_findignore(this->from, IMC_TRUST))		trust = true;

	strcpy(d->name, this->from);
	d->wizi	= trust ? this->data.GetKey("wizi", 0) : 0;
	d->level= trust ? this->data.GetKey("level", 0) : 0;
	d->invis= 0;
}


/* convert back from 'd' to 'p' */
void IMCPacket::SetData(const IMCCharData *d) {
	this->data.Init();

	if (!d) {
		strcpy(this->from, "*");
		this->data.AddKey("level", -1);
		return;
	}

	strcpy(this->from, d->name);

	if (d->wizi) this->data.AddKey("wizi", d->wizi);
	this->data.AddKey("level", d->level);
}


/* handle a packet destined for us, or a broadcast */
void IMCPacket::Receive(void) {
	IMCCharData d;
	int bcast;

	bcast = !strcmp(imc_mudof(this->i.to), "*") ? 1 : 0;
  
	this->GetData(&d);

	//	chat: message to a channel (broadcast)
	if (!str_cmp(this->type, "chat") && !imc_isignored(this->from))
		imc_recv_chat(&d, this->data.GetKey("channel", 0), this->data.GetKey("text", ""));
	//	emote: emote to a channel (broadcast)
	else if (!str_cmp(this->type, "emote") && !imc_isignored(this->from))
		imc_recv_emote(&d, this->data.GetKey("channel", 0), this->data.GetKey("text", ""));
	//	tell: tell a player here something
	else if (!str_cmp(this->type, "tell")) {
		if (imc_isignored(this->from))	imc_sendignore(this->from);
		else imc_recv_tell(&d, this->to, this->data.GetKey("text", ""), this->data.GetKey("isreply", 0));
	//	who-reply: receive a who response
	} else if (!str_cmp(this->type, "who-reply"))
		imc_recv_whoreply(this->to, this->data.GetKey("text", ""), this->data.GetKey("sequence", -1));
	//	who: receive a who request
	else if (!str_cmp(this->type, "who")) {
		if (imc_isignored(this->from))	imc_sendignore(this->from);
		else	imc_recv_who(&d, this->data.GetKey("type", "who"));
	//	whois-reply: receive a whois response
	} else if (!str_cmp(this->type, "whois-reply"))
		imc_recv_whoisreply(this->to, this->data.GetKey("text", ""));
	//	whois: receive a whois request
	else if (!str_cmp(this->type, "whois"))
		imc_recv_whois(&d, this->to);
	//	beep: beep a player
	else if (!str_cmp(this->type, "beep")) {
		if (imc_isignored(this->from))	imc_sendignore(this->from);
		else							imc_recv_beep(&d, this->to);
	//	is-alive: receive a keepalive (broadcast)
	} else if (!str_cmp(this->type, "is-alive"))
		imc_recv_keepalive(imc_mudof(this->from),
				this->data.GetKey("versionid", "unknown"),
				this->data.GetKey("flags", ""));
	//	ping: receive a ping request
	else if (!str_cmp(this->type, "ping"))
		imc_recv_ping(imc_mudof(this->from), this->data.GetKey("time-s", 0),
				this->data.GetKey("time-us", 0), this->i.path);
	//	ping-reply: receive a ping reply
	else if (!str_cmp(this->type, "ping-reply"))
		imc_recv_pingreply(imc_mudof(this->from), this->data.GetKey("time-s", 0),
				this->data.GetKey("time-us", 0),
				this->data.GetKey("path", (const char *)NULL), this->i.path);
	//	mail: mail something to a local player
	else if (!str_cmp(this->type, "mail"))
		imc_recv_mail(this->data.GetKey("from", "error@hell"),
				this->data.GetKey("to", "error@hell"),
				this->data.GetKey("date", "(IMC error: bad date)"),
				this->data.GetKey("subject", "no subject"),
				this->data.GetKey("id", "bad_id"),
				this->data.GetKey("text", ""));
	//	mail-ok: remote confirmed that they got the mail ok
	else if (!str_cmp(this->type, "mail-ok"))
		imc_recv_mailok(this->from, this->data.GetKey("id", "bad_id"));
	//	mail-reject: remote rejected our mail, bounce it
	else if (!str_cmp(this->type, "mail-reject"))
		imc_recv_mailrej(this->from, this->data.GetKey("id", "bad_id"),
				this->data.GetKey("reason", "(IMC error: no reason supplied"));
	else if (!str_cmp(this->type, "info-request"))
		imc_recv_inforequest(this->from, this->data.GetKey("category", ""));
	//	call catch-all fn if present
	else {
		IMCPacket *out;

		if (this->Receive(bcast))
			return;

		if (bcast || !str_cmp(this->type, "reject"))
			return;
    
		//	reject packet
		out = new IMCPacket();

		strcpy(out->type, "reject");
		strcpy(out->to, this->from);
		strcpy(out->from, this->to);

		this->data.Clone(&out->data);
		out->data.AddKey("old-type", this->type);

		out->Send();
		out->data.Free();
		delete out;
	}
}


/* Commands called by the interface layer */

/* return mud information.
 * yes, this is protocol level, and -required-
 */
void imc_recv_inforequest(const char *from, const char *category) {
	IMCPacket reply;

	strcpy(reply.to, from);
	strcpy(reply.from, "*");

	reply.data.Init();

	if (imc_isignored(from)) {
		strcpy(reply.type, "info-unavailable");
		reply.Send();
	} else if (!str_cmp(category, "site")) {
		strcpy(reply.type, "info-reply");
    
		reply.data.AddKey("name",	imc_siteinfo.name);
		reply.data.AddKey("host",	imc_siteinfo.host);
		reply.data.AddKey("email",	imc_siteinfo.email);
		reply.data.AddKey("imail",	imc_siteinfo.imail);
		reply.data.AddKey("www",	imc_siteinfo.www);
		reply.data.AddKey("version",IMC_VERSIONID);
		reply.data.AddKey("details",imc_siteinfo.details);
		reply.data.AddKey("flags",	imc_siteinfo.flags);

		reply.Send();
	} else {
		strcpy(reply.type, "info-unavailable");
		reply.Send();
	}

	reply.data.Free();
}

/* send a message out on a channel */
void imc_send_chat(const IMCCharData *from, int channel, const char *argument, const char *to) {
	IMCPacket out;
	char tobuf[IMC_MNAME_LENGTH];

	if (imc_active<IA_UP)
		return;

	out.SetData(from);

	strcpy(out.type, "chat");
	strcpy(out.to, "*@*");
	out.data.AddKey("text", argument);
	out.data.AddKey("channel", channel);

	to = imc_getarg(to, tobuf, IMC_MNAME_LENGTH);
	while (tobuf[0]) {
		if (!strcmp(tobuf, "*") || !str_cmp(tobuf, imc_name) || imc_find_reminfo(tobuf, 0)) {
			strcpy(out.to, "*@");
			strcat(out.to, tobuf);
			out.Send();
		}

		to = imc_getarg(to, tobuf, IMC_MNAME_LENGTH);
	}

	out.data.Free();
}

/* send an emote out on a channel */
void imc_send_emote(const IMCCharData *from, int channel,
		    const char *argument, const char *to)
{
  IMCPacket out;
  char tobuf[IMC_MNAME_LENGTH];

  if (imc_active<IA_UP)
    return;

  out.SetData(from);

  strcpy(out.type, "emote");
	out.data.AddKey("channel", channel);
	out.data.AddKey("text", argument);

  to=imc_getarg(to, tobuf, IMC_MNAME_LENGTH);
  while (tobuf[0])
  {
    if (!strcmp(tobuf, "*") || !str_cmp(tobuf, imc_name) ||
	imc_find_reminfo(tobuf, 0))
    {
      strcpy(out.to, "*@");
      strcat(out.to, tobuf);
      out.Send();
    }

    to=imc_getarg(to, tobuf, IMC_MNAME_LENGTH);
  }

	out.data.Free();
}

/* send a tell to a remote player */
void imc_send_tell(const IMCCharData *from, const char *to,
		   const char *argument, int isreply)
{
  IMCPacket out;

  if (imc_active<IA_UP)
    return;

  if (!strcmp(imc_mudof(to), "*"))
    return; /* don't let them do this */

  out.SetData(from);

	imc_sncpy(out.to, to, IMC_NAME_LENGTH);
	strcpy(out.type, "tell");
	out.data.AddKey("text", argument);
	if (isreply)
		out.data.AddKey("isreply", isreply);

	out.Send();
	out.data.Free();
}

/* send a who-request to a remote mud */
void imc_send_who(const IMCCharData *from, const char *to, const char *type)
{
  IMCPacket out;

  if (imc_active<IA_UP)
    return;

  if (!strcmp(imc_mudof(to), "*"))
    return; /* don't let them do this */

  out.SetData(from);

  sprintf(out.to, "*@%s", to);
  strcpy(out.type, "who");

  out.data.AddKey("type", type);

  out.Send();
  out.data.Free();
}

/* respond to a who request with the given data */
void imc_send_whoreply(const char *to, const char *data, int sequence) {
	IMCPacket out;

	if (imc_active<IA_UP)
		return;

	if (!strcmp(imc_mudof(to), "*"))
		return;	// don't let them do this

	out.data.Init();

	imc_sncpy(out.to, to, IMC_NAME_LENGTH);
	strcpy(out.type, "who-reply");
	strcpy(out.from, "*");
	out.data.AddKey("text", data);
	if (sequence != -1)
		out.data.AddKey("sequence", sequence);
  
	out.Send();
	out.data.Free();
}

/* special handling of whoreply construction for sequencing */
static char *wr_to;
static char *wr_buf;
static int wr_sequence;

void imc_whoreply_start(const char *to)
{
  wr_sequence=0;
  wr_to=str_dup(to);
  wr_buf=imc_getsbuf(IMC_DATA_LENGTH);
}

void imc_whoreply_add(const char *text)
{
  /* give a bit of a margin for error here */
  if (strlen(wr_to) + strlen(text) >= IMC_DATA_LENGTH-500)
  {
    imc_send_whoreply(wr_to, wr_buf, wr_sequence);
    wr_sequence++;
    imc_sncpy(wr_buf, text, IMC_DATA_LENGTH);
    return;
  }

  strcat(wr_buf, text);
}

void imc_whoreply_end(void)
{
  imc_send_whoreply(wr_to, wr_buf, -(wr_sequence+1));
  FREE(wr_to);
  wr_buf[0]=0;
  imc_shrinksbuf(wr_buf);
}

/* send a whois-request to a remote mud */
void imc_send_whois(const IMCCharData *from, const char *to)
{
  IMCPacket out;

  if (imc_active<IA_UP)
    return;

  if (strchr(to, '@'))
    return;

  out.SetData(from);

  sprintf(out.to, "%s@*", to);
  strcpy(out.type, "whois");

  out.Send();
  out.data.Free();
}

/* respond with a whois-reply */
void imc_send_whoisreply(const char *to, const char *data) {
	IMCPacket out;

	if (imc_active<IA_UP)
		return;

	if (!strcmp(imc_mudof(to), "*"))
		return;	//	don't let them do this

	out.data.Init();

	imc_sncpy(out.to, to, IMC_NAME_LENGTH);
	strcpy(out.type, "whois-reply");
	strcpy(out.from, "*");
	out.data.AddKey("text", data);

	out.Send();
	out.data.Free();
}

/* beep a remote player */
void imc_send_beep(const IMCCharData *from, const char *to)
{
  IMCPacket out;

  if (imc_active<IA_UP)
    return;

  if (!strcmp(imc_mudof(to), "*"))
    return; /* don't let them do this */

  out.SetData(from);
  strcpy(out.type, "beep");
  imc_sncpy(out.to, to, IMC_NAME_LENGTH);

  out.Send();
  out.data.Free();
}

/* send a keepalive to everyone */
void imc_send_keepalive(void) {
	IMCPacket out;

	if (imc_active<IA_UP)
		return;

	out.data.Init();
	strcpy(out.type, "is-alive");
	strcpy(out.from, "*");
	strcpy(out.to, "*@*");
	out.data.AddKey("versionid", IMC_VERSIONID);
	if (imc_siteinfo.flags[0])
		out.data.AddKey("flags", imc_siteinfo.flags);

	out.Send();
	out.data.Free();
}

/* send a ping with a given timestamp */
void imc_send_ping(const char *to, int time_s, int time_u) {
	IMCPacket out;

	if (imc_active < IA_UP)
		return;

	out.data.Init();
	strcpy(out.type, "ping");
	strcpy(out.from, "*");
	strcpy(out.to, "*@");
	imc_sncpy(out.to+2, to, IMC_MNAME_LENGTH-2);
	out.data.AddKey("time-s", time_s);
	out.data.AddKey("time-us", time_u);

	out.Send();
	out.data.Free();
}

/* send a pingreply with the given timestamp */
void imc_send_pingreply(const char *to, int time_s, int time_u, const char *path) {
	IMCPacket out;

	if (imc_active<IA_UP)
		return;

	out.data.Init();
	strcpy(out.type, "ping-reply");
	strcpy(out.from, "*");
	strcpy(out.to, "*@");
	imc_sncpy(out.to + 2, to, IMC_MNAME_LENGTH - 2);
	out.data.AddKey("time-s", time_s);
	out.data.AddKey("time-us", time_u);
	out.data.AddKey("path", path);

	out.Send();
	out.data.Free();
}
