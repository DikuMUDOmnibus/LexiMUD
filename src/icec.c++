/*
 * IMC2 - an inter-mud communications protocol
 *
 * icec.c: IMC-channel-extensions (ICE) client code
 *
 * Copyright (C) 1997 Oliver Jowett <oliver@jowett.manawatu.planet.co.nz>
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
#include "icec.h"

#include "utils.h"
#include "buffer.h"

void ev_icec_timeout(Ptr data);
void ev_icec_firstrefresh(Ptr dummy);

LList<ICEChannel *>	icec_channel_list;

ICEChannel *icec_findchannel(const char *name) {
	ICEChannel *c;

	LListIterator<ICEChannel *>		iter(icec_channel_list);
	while ((c = iter.Next()))
		if (!str_cmp(c->name, name))
			return c;

	return NULL;
}

bool IMCPacket::Receive(SInt32 bcast) {
	/* redirected msg */
	if (!str_cmp(this->type, "ice-msg-r")) {
		icec_recv_msg_r(this->from,
				this->data.GetKey("realfrom", ""),
				this->data.GetKey("channel"	, ""),
				this->data.GetKey("text"	, ""),
				this->data.GetKey("emote"	, 0));
	} else if (!str_cmp(this->type, "ice-msg-b")) {
		icec_recv_msg_b(this->from,
				this->data.GetKey("channel"	, ""),
				this->data.GetKey("text"	, ""),
				this->data.GetKey("emote"	, 0));
	} else if (!str_cmp(this->type, "ice-update")) {
		icec_recv_update(this->from,
				this->data.GetKey("channel"	, ""),
				this->data.GetKey("owner"	, ""),
				this->data.GetKey("operators", ""),
				this->data.GetKey("policy"	, ""),
				this->data.GetKey("invited"	, "" ),
				this->data.GetKey("excluded", ""));
	} else if (!str_cmp(this->type, "ice-destroy")) {
		icec_recv_destroy(this->from,
				this->data.GetKey("channel"	, ""));
	} else
		return false;
	return true;
}


void icec_recv_msg_r(const char *from, const char *realfrom, const char *chan,
		const char *txt, int emote) {
	ICEChannel *c;
	const char *mud;

	mud = imc_mudof(from);

	//	forged?
	if (!strchr(chan, ':') || str_cmp(mud, ice_mudof(chan)))
		return;

	c = icec_findchannel(chan);
	
	if (!c || !c->local || c->policy!=ICE_PRIVATE)
		return;

	//	we assume that anything redirected is automatically audible,
	//	since we trust the ICEd

	c->ShowChannel(realfrom, txt, emote);
}

void icec_recv_msg_b(const char *from, const char *chan, const char *txt, int emote) {
	ICEChannel *c;

	c = icec_findchannel(chan);
	
	if (!c || !c->local || c->policy==ICE_PRIVATE)
		return;
	
	if (!c->Audible(from))
		return;
  
	c->ShowChannel(from, txt, emote);
}

void icec_recv_update(const char *from, const char *chan, const char *owner,
		const char *operators, const char *policy, const char *invited,
		const char *excluded) {
	ICEChannel *c;
	const char *mud;

	mud = imc_mudof(from);

	//	forged?
	if (!strchr(chan, ':') || str_cmp(mud, ice_mudof(chan)))
		return;

	c = icec_findchannel(chan);

	if (!c) {
//		c = (_ice_channel *)imc_malloc(sizeof(*c));
		c = new ICEChannel();
		c->name		= str_dup(chan);
		c->owner	= str_dup(owner);
		c->operators= str_dup(operators);
		c->invited	= str_dup(invited);
		c->excluded	= str_dup(excluded);
		c->local	= NULL;
		c->active	= NULL;

//		c->next		= icec_channel_list;
//		icec_channel_list=c;
		icec_channel_list.Add(c);
	} else {
		if (c->owner)		free(c->owner);
		if (c->operators)	free(c->operators);
		if (c->invited)		free(c->invited);
		if (c->excluded)	free(c->excluded);
		c->name		= str_dup(chan);
		c->owner	= str_dup(owner);
		c->operators= str_dup(operators);
		c->invited	= str_dup(invited);
		c->excluded	= str_dup(excluded);
	}
    
	if (!str_cmp(policy, "open"))			c->policy=ICE_OPEN;
	else if (!str_cmp(policy, "closed"))	c->policy=ICE_CLOSED;
	else									c->policy=ICE_PRIVATE;

	if (c->local && !c->Audible(imc_name))
		c->FreeLocal();

	c->NotifyUpdate();

	imc_cancel_event(ev_icec_timeout, c);
	imc_add_event(ICEC_TIMEOUT, ev_icec_timeout, c, 0);
}


void icec_recv_destroy(const char *from, const char *channel) {
	ICEChannel *c;
	const char *mud;

	mud = imc_mudof(from);

	if (!strchr(channel, ':') || str_cmp(mud, ice_mudof(channel)))
		return;

	c = icec_findchannel(channel);
	if (!c)
		return;

	icec_channel_list.Remove(c);

	delete c;
}


const char *icec_command(const char *from, const char *arg) {
	char *cmd = get_buffer(IMC_NAME_LENGTH);
	char *chan = get_buffer(IMC_NAME_LENGTH);
	char *data;
	const char *p;
	const char *result;
	IMCPacket out;
	ICEChannel *c;

	p = imc_getarg(arg, cmd, IMC_NAME_LENGTH);
	p = imc_getarg(p, chan, IMC_NAME_LENGTH);

	if (!*cmd || !*chan) {
		result = "Syntax: icommand <command> <node:channel> [<data..>]";
	} else {
		data = get_buffer(IMC_DATA_LENGTH);
		
		strcpy(data, p);
		p = strchr(chan, ':');
		if (!p) {
			c = icec_findlchannel(chan);
			if (c)
				strcpy(chan, c->name);
		}

		sprintf(out.to, "ICE@%s", ice_mudof(chan));
		strcpy(out.type, "ice-cmd");
		strcpy(out.from, from);
		out.data.Init();
		out.data.AddKey("channel", chan);
		out.data.AddKey("command", cmd);
		out.data.AddKey("data", data);

		out.Send();
		out.data.Free();

		release_buffer(data);

		result = "Command sent.";
	}
	release_buffer(cmd);
	release_buffer(chan);
	return result;
}


void ICEChannel::SendMessage(const char *name, const char *text, bool emote) {
	IMCPacket out;
  
	strcpy(out.from, name);
	out.data.Init();
	out.data.AddKey("channel", this->name);
	out.data.AddKey("text", text);
	out.data.AddKey("emote", emote);
  
	//	send a message out on a channel
	if (this->policy == ICE_PRIVATE) {
		//	send to the daemon to distribute
		//	send locally
		this->ShowChannel(imc_makename(name, imc_name), text, emote);
    
		sprintf(out.to, "ICE@%s", ice_mudof(this->name));
		strcpy(out.type, "ice-msg-p");
	} else {
		//	broadcast
		strcpy(out.type, "ice-msg-b");
		strcpy(out.to, "*@*");
	}

	out.Send();
	out.data.Free();
}


void ev_icec_firstrefresh(Ptr dummy) {
	IMCPacket out;

	if (imc_active < IA_UP)
		return;
  
	strcpy(out.from, "*");
	strcpy(out.to, "ICE@*");
	strcpy(out.type, "ice-refresh");
	out.data.Init();
	out.data.AddKey("channel", "*");
	out.Send();
	out.data.Free();
}

void ev_icec_timeout(Ptr data) {
	ICEChannel *c = (ICEChannel *)data;
	
	icec_channel_list.Remove(c);
	
	delete c;
}


//	global init
void icec_init(void) {
	imc_logstring("ICE client starting.");

	imc_add_event(60, ev_icec_firstrefresh, NULL, 1);

	icec_load_channels();
}
