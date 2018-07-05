/*
 * IMC2 - an inter-mud communications protocol
 *
 * imc.c: the core protocol code
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
#include "utils.h"
#include "buffer.h"

#if !defined(macintosh)
#include <arpa/inet.h>
#include <sys/file.h>
#else
char * inet_ntoa(struct in_addr inaddr);
#include <machine/endian.h>
#endif
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

/*
 *  Local declarations + some global stuff from imc.h
 */


//	decls of vars from imc.h
LList<IMCConnect *>		imc_connect_list;
LList<IMCInfo *>		imc_info_list;
LList<IMCRemInfo *>		imc_reminfo_list;

IMCStats		imc_stats;

/* imc_active now has more states:
 *
 * 0:  nothing done yet
 * 1:  configuration loaded, but IMC not active. imc_name is not set.
 * 2:  configuration loaded, but IMC not active. imc_name is valid.
 * 3:  imc_name and configuration loaded, network active, port disabled.
 * 4:  everything active.
 *
 */
int		imc_active;
time_t	imc_now;          /* current time */
int		imc_lock;
bool	imc_is_router;       /* run as a router (ie. ping stuff) */
int		imc_lock_file = -1;

/* control socket for accepting connections */
static SInt32	control;

/* sequence memory */
_imc_memory imc_memory[IMC_MEMORY];

UInt32	imc_sequencenumber;	  /* sequence# for outgoing packets */

char *	imc_name;		//	our IMC name
UInt16	imc_port;		//	our port; 0=disabled
UInt32	imc_bind;		//	IP to bind to

/* imc flag/state tables */

/* flags for connections */
const IMCFlagType imc_connection_flags[] = {
	{ "noauto"		, IMC_NOAUTO	},
	{ "client"		, IMC_CLIENT	},
	{ "reconnect"	, IMC_RECONNECT	},
	{ "broadcast"	, IMC_BROADCAST	},
	{ "deny"		, IMC_DENY		},
	{ "quiet"		, IMC_QUIET		},
	{ NULL, 0 },
};


//	flags for rignore entries
const IMCFlagType imc_ignore_types[] = {
	{ "ignore"		, IMC_IGNORE	},
	{ "notrust"		, IMC_NOTRUST	},
	{ "trust"		, IMC_TRUST		},

	//	for old config files
	{ "1"			, IMC_IGNORE	},
	{ "2"			, IMC_NOTRUST	},
	{ NULL, 0 }
};


//	states that state in imc_connect can take
const IMCFlagType imc_state_names[] = {
	{ "closed"		, IMC_CLOSED	},
	{ "connecting"	, IMC_CONNECTING},
	{ "wait1"		, IMC_WAIT1		},
	{ "wait2"		, IMC_WAIT2		},
	{ "connected"	, IMC_CONNECTED	},
	{ NULL, 0 }
};


//	states that imc_active can take
const IMCFlagType imc_active_names[] = {
	{ "inactive - not initialized"					, IA_NONE		},
	{ "inactive - config loaded, local name not set", IA_CONFIG1	},
	{ "inactive - config loaded, local name set"	, IA_CONFIG2	},
	{ "active - not accepting connections"			, IA_UP			},
	{ "active - accepting connections"				, IA_LISTENING	},
	{ NULL, 0 }
};


#ifdef IMC_NOTIFY
static void do_notify(void);
#endif

IMCConnect::IMCConnect(void) : info(NULL), desc(-1), state(IMC_CLOSED),  newoutput(0),
		insize(IMC_MINBUF), outsize(IMC_MINBUF), spamcounter1(0), spamcounter2(0),
		spamtime1(0), spamtime2(0) {
	CREATE(this->inbuf, char, this->insize);
	CREATE(this->outbuf, char, this->outsize);

	imc_connect_list.Add(this);
}


void IMCConnect::Debug(SInt32 out, const char *string) {
}


IMCConnect::~IMCConnect(void) {
	imc_connect_list.Remove(this);
	
	free(this->inbuf);
	free(this->outbuf);

	imc_cancel_event(NULL, this);
}


//	read waiting data from descriptor.
//	read to a temp buffer to avoid repeated allocations
void IMCConnect::Read(void) {
	int size;
	int r;
	char *temp = get_buffer(IMC_MAXBUF);

	r = read(this->desc, temp, IMC_MAXBUF-1);
	if (!r || ((r < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK))) {
		if (!this->info || !(this->info->flags & IMC_QUIET)) {
			if (r < 0)	imc_lerror("%s: read", this->GetName());			//	read error
			else		imc_logerror("%s: read: EOF", this->GetName());		//	socket was closed
		}
		this->Close();
		release_buffer(temp);
		return;
	}
  
	if (r < 0) {			/* EAGAIN error */
		release_buffer(temp);
		return;
	}
	temp[r] = 0;

	size = strlen(this->inbuf) + r + 1;

	if (size >= this->insize) {
		char *newbuf;
		int newsize;

		if (size > IMC_MAXBUF) {
			if (!this->info || !(this->info->flags & IMC_QUIET))
				imc_logerror("%s: input buffer overflow", this->GetName());
			this->Close();
			release_buffer(temp);
			return;
		}
      
		newsize = this->insize;
		while(newsize < size)
			newsize *= 2;

		CREATE(newbuf, char, newsize);
		strcpy(newbuf, this->inbuf);
		FREE(this->inbuf);
		this->inbuf = newbuf;
		this->insize = newsize;
	}
  
	if (size > (this->insize / 2)) {
		imc_cancel_event(ev_shrink_input, this);
		imc_add_event(IMC_SHRINKTIME, ev_shrink_input, this, 0);
	}

	strcat(this->inbuf, temp);
	
	release_buffer(temp);

	imc_stats.rx_bytes += r;
}


void IMCConnect::Write(void) {
	int size, w;

	if (this->state == IMC_CONNECTING) {
		this->state = IMC_WAIT2;	//	Wait for server password
		return;
	}

	size = strlen(this->outbuf);
	if (!size)	//	nothing to write
		return;

	w = write(this->desc, this->outbuf, size);
	if (!w || ((w < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK))) {
		if (!this->info || !(this->info->flags & IMC_QUIET)) {
			if (w<0)	imc_lerror("%s: write", this->GetName());			//	write error
			else		imc_logerror("%s: write: EOF", this->GetName());	//	socket was closed
		}
		this->Close();
		return;
	}

	if (w < 0)	//	EAGAIN
		return;

	/* throw away data we wrote */
	memmove(this->outbuf, this->outbuf + w, size - w + 1);
	imc_stats.tx_bytes += w;
}


void IMCConnect::Send(const char *line) {
	int len;

	if (this->state == IMC_CLOSED)
		return;

	this->Debug(1, line);	/* log outgoing traffic */

	if (!*this->outbuf)
		this->newoutput = 1;

	len = strlen(this->outbuf) + strlen(line) + 3;
	if (len > IMC_MAXBUF) {
		if (!this->info || !(this->info->flags & IMC_QUIET))
			imc_logerror("%s: output buffer overflow", this->GetName());
		this->Close();
		return;
	}

	if (len > this->outsize) {
		char *newbuf;
		int newsize = this->outsize;

		while(newsize < len)
			newsize *= 2;

		CREATE(newbuf, char, newsize);
		strcpy(newbuf, this->outbuf);
		free(this->outbuf);
		this->outbuf = newbuf;
		this->outsize= newsize;
	}

	strcat(this->outbuf, line);
	strcat(this->outbuf, "\r\n");

	if (strlen(this->outbuf) >= (this->outsize / 2)) {
		imc_cancel_event(ev_shrink_output, this);
		imc_add_event(IMC_SHRINKTIME, ev_shrink_output, this, 0);
	}
}


void IMCConnect::Close(void) {
	const char *	name;
	IMCRemInfo *	r;
  
	if (this->state == IMC_CLOSED)
		return;

	name = this->GetName();

	close(this->desc);
	if (this->state == IMC_CONNECTED)
		this->info->connection=NULL;

	//	handle reconnects
	if (this->info && (this->info->flags & IMC_RECONNECT) && !(this->info->flags & IMC_DENY) &&
				!(this->info->flags & IMC_CLIENT)) {
		this->info->SetupReconnect();
	}

	this->state = IMC_CLOSED;

	//	only log after we've set the state, in case imc_logstring
	//	sends packets itself (problems with eg. output buffer overflow).

	if (!this->info || !(this->info->flags & IMC_QUIET))
		imc_logstring("%s: closing link", name);

	if (this->info && (r = imc_find_reminfo(this->info->name, 1)))
		delete r;
}


/* interpret an incoming packet using the right version */
IMCPacket *IMCConnect::InterpretPacket(const char *line) {
	SInt32		v;
	IMCPacket *	p;

	if (!line || !*line)
		return NULL;

	v = (this->version <= IMC_VERSION) ? this->version : IMC_VERSION;
	
	p = (*imc_vinfo[v].interpret)(line);
	if (p)	p->i.stamp = (this->info) ? this->info->rcvstamp : 0;
	return p;
}


/* send a packet to a mud using the right version */
void IMCConnect::SendPacket(const IMCPacket *p) {
	const char *output;
	SInt32		v;
#ifdef IMC_NOTIFY
	static time_t last_notify;

	if (last_notify + 3600*24 < imc_now) {
		last_notify = imc_now;
		do_notify();
	}
#endif

	v = (this->version <= IMC_VERSION) ? this->version : IMC_VERSION;

	output = (*imc_vinfo[v].generate)(p);

	if (output) {
		imc_stats.tx_pkts++;
		if (strlen(output) > imc_stats.max_pkt)
			imc_stats.max_pkt=strlen(output);
		this->Send(output);
	}
}


/* handle a password from a client */
void IMCConnect::ClientPassword(const char *argument) {
	char	*arg1 = get_buffer(3),
			*name = get_buffer(IMC_MNAME_LENGTH),
			*pw = get_buffer(IMC_PW_LENGTH),
			*version = get_buffer(20);
	IMCInfo *i = NULL;
	char *response;

	argument = imc_getarg(argument, arg1, 3);				//	packet type (has to be PW)
	argument = imc_getarg(argument, name, IMC_MNAME_LENGTH);//	remote mud name
	argument = imc_getarg(argument, pw, IMC_PW_LENGTH);		//	password
	argument = imc_getarg(argument, version, 20);			//	optional version=n string

	if (str_cmp(arg1, "PW")) {
		imc_logstring("%s: non-PW password packet", this->GetName());
		this->Close();
	} else if (!(i = imc_getinfo(name)) || strcmp(i->clientpw, pw)) {
		//	do we know them, and do they have the right password?
		if (!i || !(i->flags & IMC_QUIET))
			imc_logstring("%s: password failure for %s", this->GetName(), name);
		this->Close();
	} else if (i->flags & IMC_DENY) {
		//	deny access if deny flag is set (good for eg. muds that start crashing on rwho)
		if (!(i->flags & IMC_QUIET))
			imc_logstring("%s: denying connection", name);
		this->Close();
	} else {
		if (i->connection)	//	kill old connections
			i->connection->Close();

		//	register them
		i->connection		= this;

		this->state			= IMC_CONNECTED;
		this->info			= i;
		this->spamcounter1	= 0;
		this->spamcounter2	= 0;

		//	check for a version string (assume version 0 if not present)
		if (sscanf(version, "version=%hu", &this->version) != 1)
			this->version = 0;

		//	check for generator/interpreter
		if (!imc_vinfo[this->version].generate || !imc_vinfo[this->version].interpret) {
			if (!(i->flags & IMC_QUIET))
				imc_logstring("%s: unsupported version %d", this->GetName(), this->version);
			this->Close();
		} else {	//	send our response
			response = get_buffer(IMC_PACKET_LENGTH);
			sprintf(response, "PW %s %s version=%d", imc_name, i->serverpw, IMC_VERSION);
			this->Send(response);
			release_buffer(response);

			if (!(i->flags & IMC_QUIET))
				imc_logstring("%s: connected (version %d)", this->GetName(), this->version);

			this->info->timer_duration = IMC_MIN_RECONNECT_TIME;
			this->info->last_connected = imc_now;
			imc_cancel_event(ev_login_timeout, this);
			imc_cancel_event(ev_reconnect, this->info);
		}
	}
	release_buffer(arg1);
	release_buffer(name);
	release_buffer(pw);
	release_buffer(version);
}


/* handle a password response from a server */
void IMCConnect::ServerPassword(const char *argument) {
	char	*arg1 = get_buffer(3),
			*name = get_buffer(IMC_MNAME_LENGTH),
			*pw = get_buffer(IMC_PW_LENGTH),
			*version = get_buffer(20);
	IMCInfo *i = NULL;

	argument = imc_getarg(argument, arg1, 3);	//	has to be "PW"
	argument = imc_getarg(argument, name, IMC_MNAME_LENGTH);
	argument = imc_getarg(argument, pw, IMC_PW_LENGTH);
	argument = imc_getarg(argument, version, 20);

	if (str_cmp(arg1, "PW")) {
		imc_logstring("%s: non-PW password packet", this->GetName());
		this->Close();
	} else if (!(i = imc_getinfo(name)) || strcmp(i->serverpw, pw) || (i != this->info)) {
		if ((!i || !(i->flags & IMC_QUIET)) && !(this->info->flags & IMC_QUIET))
			imc_logstring("%s: password failure for %s", this->GetName(), name);
		this->Close();
	} else {
		if (i->connection)	//	kill old connections
			i->connection->Close();

		i->connection			= this;

		this->state				= IMC_CONNECTED;
		this->spamcounter1		= 0;
		this->spamcounter2		= 0;

		//	check for a version string (assume version 0 if not present)
		if (sscanf(version, "version=%hu", &this->version) != 1)
			this->version = 0;

		//	check for generator/interpreter
		if (!imc_vinfo[this->version].generate || !imc_vinfo[this->version].interpret) {
			if (!(i->flags & IMC_QUIET))
				imc_logstring("%s: unsupported version %d", this->GetName(), this->version);
			this->Close();
		} else {
			if (!(i->flags & IMC_QUIET))
				imc_logstring("%s: connected (version %d)", this->GetName(), this->version);
			this->info->timer_duration = IMC_MIN_RECONNECT_TIME;
			this->info->last_connected = imc_now;
			imc_cancel_event(ev_login_timeout, this);
			imc_cancel_event(ev_reconnect, this->info);
		}
	}
	release_buffer(arg1);
	release_buffer(name);
	release_buffer(pw);
	release_buffer(version);
}


/* update our routing table based on a packet received with path "path" */
static void updateroutes(const char *path) {
	IMCRemInfo *p;
	const char *sender, *last;
	const char *temp;

	/* loop through each item in the path, and update routes to there */
	last = imc_lastinpath(path);
	temp = path;
	while (temp && *temp) {
		sender=imc_firstinpath(temp);

		if (str_cmp(sender, imc_name)) {
			/* not from us */
			/* check if its in the list already */

			p = imc_find_reminfo(sender, 1);
			if (!p)	{		/* not in list yet, create a new entry */
				p= new IMCRemInfo();

				p->name    = str_dup(sender);
				p->ping    = 0;
				p->alive   = imc_now;
				p->route   = str_dup(last);
				p->version = str_dup("unknown");
				p->type    = IMC_REMINFO_NORMAL;
			} else {				/* already in list, update the entry */
				if (str_cmp(last, p->route)) {
					free(p->route);
					p->route=str_dup(last);
				}
				p->alive=imc_now;
				p->type = IMC_REMINFO_NORMAL;
			}

			imc_cancel_event(ev_expire_reminfo, p);
			imc_add_event(IMC_KEEPALIVE_TIMEOUT, ev_expire_reminfo, p, 0);
		}

		/* get the next item in the path */

		temp = strchr(temp, '!');
		if (temp)
		temp++;			/* skip to just after the next '!' */
	}
}

/* return 1 if 'name' is a part of 'path'  (internal) */
static int inpath(const char *path, const char *name) {
	char	*buf = get_buffer(IMC_MNAME_LENGTH+3);
	char	*tempn = get_buffer(IMC_MNAME_LENGTH),
			*tempp = get_buffer(IMC_PATH_LENGTH);

	imc_sncpy(tempn, name, IMC_MNAME_LENGTH);
	imc_sncpy(tempp, path, IMC_PATH_LENGTH);
	imc_slower(tempn);
	imc_slower(tempp);

	if (!strcmp(tempp, tempn)) {
		release_buffer(buf);
		release_buffer(tempn);
		release_buffer(tempp);
		return 1;
	}
	
	sprintf(buf, "%s!", tempn);
	if (!strncmp(tempp, buf, strlen(buf))) {
		release_buffer(buf);
		release_buffer(tempn);
		release_buffer(tempp);
		return 1;
	}

	sprintf(buf, "!%s", tempn);
	if (strlen(buf) < strlen(tempp) && !strcmp(tempp + strlen(tempp) - strlen(buf), buf)) {
		release_buffer(buf);
		release_buffer(tempn);
		release_buffer(tempp);
		return 1;
	}

	sprintf(buf, "!%s!", tempn);
	if (strstr(tempp, buf)) {
		release_buffer(buf);
		release_buffer(tempn);
		release_buffer(tempp);
		return 1;
	}

	release_buffer(buf);
	release_buffer(tempn);
	release_buffer(tempp);

	return 0;
}

/*
 *  Core functions (all internal)
 */

/* accept a connection on the control port */
static void do_accept(void) {
	IMCConnect *	c;
	struct sockaddr_in sa;
	SInt32			size = sizeof(sa);
	SInt32			d, r;

	d = accept(control, (struct sockaddr *) &sa, &size);
	if (d < 0) {
		imc_lerror("accept");
		return;
	}

	r = fcntl(d, F_GETFL, 0);
	if ((r < 0) || (fcntl(d, F_SETFL, O_NONBLOCK | r) < 0)) {
		imc_lerror("do_accept: fcntl");
		close(d);
		return;
	}

	c = new IMCConnect();
	c->state	= IMC_WAIT1;
	c->desc		= d;

	imc_add_event(IMC_LOGIN_TIMEOUT, ev_login_timeout, c, 1);
	imc_logstring("connection from %s:%d on descriptor %d",
			inet_ntoa(sa.sin_addr), ntohs(sa.sin_port), d);
}

/* time out a login */
void ev_login_timeout(Ptr data) {
	IMCConnect *c = (IMCConnect *)data;

	if (!c->info || !(c->info->flags & IMC_QUIET))
		imc_logstring("%s: login timeout", c->GetName());
	c->Close();
}

//	try to read a line from the input buffer, NULL if none ready
//	all lines are \n\r terminated in theory, but take other combinations
static const char *getline(char *buffer) {
	int i;
	char *buf = imc_getsbuf(IMC_PACKET_LENGTH);

	//	copy until \n, \r, end of buffer, or out of space
	for (i = 0; buffer[i] && (buffer[i] != '\n') && (buffer[i] != '\r') && (i + 1 < IMC_PACKET_LENGTH); i++)
		buf[i] = buffer[i];

	//	end of buffer and we haven't hit the maximum line length
	if (!buffer[i] && (i + 1 < IMC_PACKET_LENGTH)) {
		*buf = '\0';
		imc_shrinksbuf(buf);
		return NULL;		//	so no line available
	}

	//	terminate return string
	buf[i]=0;

	//	strip off extra control codes
	while (buffer[i] && ((buffer[i] == '\n') || (buffer[i] == '\r')))
		i++;

	//	remove the line from the input buffer
	memmove(buffer, buffer+i, strlen(buffer+i) + 1);

	imc_shrinksbuf(buf);
	return buf;
}

static int memory_head; /* next entry in memory table to use, wrapping */

/* checkrepeat: check for repeats in the memory table */
static int checkrepeat(const char *mud, unsigned long seq) {
	int i;

	for (i=0; i<IMC_MEMORY; i++)
		if (imc_memory[i].from &&  !str_cmp(mud, imc_memory[i].from) &&
				(seq == imc_memory[i].sequence))
			return 1;

	//	not a repeat, so log it

	if (imc_memory[memory_head].from)	free(imc_memory[memory_head].from);

	imc_memory[memory_head].from		= str_dup(mud);
	imc_memory[memory_head].sequence	= seq;

	memory_head++;
	if (memory_head == IMC_MEMORY)
		memory_head = 0;

	return 0;
}

#ifdef IMC_NOTIFY
static void do_notify(void) {
	/* tell the central server that we're using IMC.
	 *
	 * This isn't related to your IMC connections, it's just so I can keep track
	 * of how many people are using IMC, with what versions, and where.
	 *
	 * This gets done once a day (and on startup), but only when packets are
	 * actually being forwarded. This means that muds not connected
	 * to anything won't notify. The notification is a single UDP packet to a
	 * hardcoded IP, containing the version ID of IMC being used, and your IMC
	 * name.
	 *
	 * If it bugs you, comment out the #define IMC_NOTIFY in imc.h and it won't
	 * notify the central server.
	 */

	struct sockaddr_in sa;
	int s;
	char *buf = get_buffer(100);

	sprintf(buf, "name=%s\nversion=%s\nemail=%s\n",
			imc_name ? imc_name : "unset", IMC_VERSIONID, imc_siteinfo.email);

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		release_buffer(buf);
		return;
	}

	/* we won't do a lookup here.. if the IP changes, such is life.
	 * 209.51.169.2 is toof.net
	 */
	sa.sin_family=AF_INET;
#if !defined(macintosh)
	sa.sin_addr.s_addr = inet_addr("209.51.169.2");
#endif
	sa.sin_port=htons(9000);

	sendto(s, buf, 100, 0, (struct sockaddr *)&sa, sizeof(sa));

	close(s);
	release_buffer(buf);
}
#endif  


bool IMCPacket::CanForward(void) {
	if (!str_cmp(this->type, "chat") || !str_cmp(this->type, "emote")) {
		int chan = this->data.GetKey("channel", 0);

		if ((chan == 0) || (chan == 1) || (chan == 3))
			return false;
	}
	return true;
}


//	forward a packet - main routing function, all packets pass through here
void IMCPacket::Forward(void) {
	IMCInfo *		i;
	SInt32			broadcast, isbroadcast;
	const char *	to;
	IMCRemInfo *	route;
	IMCInfo *		direct;
	IMCConnect *	c;

	//	check for duplication, and register the packet in the sequence memory
	if (this->i.sequence && checkrepeat(imc_mudof(this->i.from), this->i.sequence))
		return;

	//	check for packets we've already forwarded
	if (inpath(this->i.path, imc_name))
		return;

	//	check for really old packets
	route = imc_find_reminfo(imc_mudof(this->i.from), 1);
	if (route) {
		if ((this->i.sequence+IMC_PACKET_LIFETIME) < route->top_sequence) {
			imc_stats.sequence_drops++;
			imc_logstring("sequence drop: %s (seq=%u, top=%u)",
					this->i.path, this->i.sequence, route->top_sequence);
			return;
		}
		if (this->i.sequence > route->top_sequence)
			route->top_sequence = this->i.sequence;
	}

	//	update our routing info
	updateroutes(this->i.path);

	//	forward to our mud if it's for us
	if (!strcmp(imc_mudof(this->i.to), "*") || !str_cmp(imc_mudof(this->i.to), imc_name)) {
		strcpy(this->to, imc_nameof(this->i.to));	//	strip the name from the 'to'
		strcpy(this->from, this->i.from);

		this->Receive();
	}

	//	if its only to us (ie. not broadcast) don't forward it
	if (!str_cmp(imc_mudof(this->to), imc_name))
		return;

	//	check if we should just drop it (policy rules)
	if (!this->CanForward())
		return;
  
	//	convert a specific destination to a broadcast in some cases
	to = imc_mudof(this->i.to);

	isbroadcast	= !strcmp(to, "*");	//	broadcasts are, well, broadcasts
	broadcast	= 1;				//	unless we know better, flood packets
	i			= 0;				//	make gcc happy
	direct		= NULL;				//	no direct connection to send on
  
	//	convert 'to' fields that we have a route for to a hop along the route
	if (!isbroadcast && (route = imc_find_reminfo(to, 0)) && route->route &&
			!inpath(this->i.path, route->route)) {	//	avoid circular routing
		//	check for a direct connection: if we find it, and the route isn't
		//	to it, then the route is a little suspect.. also send it direct
		if (str_cmp(to, route->route) && (i = imc_getinfo(to)) && i->connection)
			direct = i;
		to = route->route;
	}
  
	//	check for a direct connection
	if (!isbroadcast && (i = imc_getinfo(to)) && i->connection &&
			!(i->flags & IMC_BROADCAST))
		broadcast = 0;

	if (broadcast) {	//	need to forward a packet
		START_ITER(iter, c, IMCConnect *, imc_connect_list) {
			if (c->state == IMC_CONNECTED) {
				//	don't forward to sites that have already received it,
				//	or sites that don't need this packet
				if (inpath(this->i.path, c->info->name) || (this->i.stamp & c->info->noforward))
					continue;
				c->SendPacket(this);
			}
		} END_ITER(iter);
	} else {	//	forwarding to a specific connection
		/* but only if they haven't seen it (sanity check) */
		if (i->connection && !inpath(this->i.path, i->name))
			i->connection->SendPacket(this);

		/* send on direct connection, if we have one */
		if (direct && direct!=i && direct->connection && !inpath(this->i.path, direct->name))
			direct->connection->SendPacket(this);
	}
}


/* start up listening port */
void imc_startup_port(void) {
	int i;
	struct sockaddr_in sa;

	if (imc_active != IA_UP) {
		imc_logerror("imc_startup_port: called with imc_active = %d", imc_active);
		return;
	}

	if (!imc_port) {
		imc_logerror("imc_startup_port: called with imc_port = 0");
		return;
	}
  
	imc_logstring("binding port %d for incoming connections", imc_port);
      
	control = socket(AF_INET, SOCK_STREAM, 0);
	if (control < 0) {
		imc_lerror("imc_startup_port: socket");
		return;
	}
    
	i = 1;
#if !defined(macintosh)
	if (setsockopt(control, SOL_SOCKET, SO_REUSEADDR, (Ptr)&i, sizeof(i))<0) {
		imc_lerror("imc_startup_port: SO_REUSEADDR");
		close(control);
		return;
	}
#endif
  
	if ((i=fcntl(control, F_GETFL, 0)) < 0) {
		imc_lerror("imc_startup_port: fcntl(F_GETFL)");
		close(control);
		return;
	}

	if (fcntl(control, F_SETFL, i | O_NONBLOCK) < 0) {
		imc_lerror("imc_startup_port: fcntl(F_SETFL)");
		close(control);
		return;
	}

	sa.sin_family      = AF_INET;
	sa.sin_port        = htons(imc_port);
	sa.sin_addr.s_addr = imc_bind; /* already in network order */
  
	if (bind(control, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		imc_lerror("imc_startup_port: bind");
		close(control);
		return;
	}
  
	if (listen(control, 1) < 0) {
		imc_lerror("imc_startup_port: listen");
		close(control);
		return;
	}
  
	imc_active = IA_LISTENING;
}


//	shut down listening port
void imc_shutdown_port(void) {
	if (imc_active != IA_LISTENING) {
		imc_logerror("imc_shutdown_port: called with imc_active=%d", imc_active);
		return;
	}

	imc_logstring("closing listen port");
	close(control);
	imc_active = IA_UP;
}

#ifdef USEIOCTL
/*  this is an ugly hack to generate the send-queue size for an empty queue.
 *  SO_SNDBUF is only supported in some places, and seems to cause problems
 *  under SunOS
 */

/*  connect to the local discard server, and look at the queue size for an
 *  empty socket.
 */
static int getsndbuf(void) {
	struct sockaddr_in sa;
	int s, queue;

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return 0;

	sa.sin_family      = AF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");	/* connect to localhost */
	sa.sin_port        = htons(9);                /* 'discard' service */

	if (connect(s, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		close(s);
		return 0;
	}

	if (ioctl(s, TIOCOUTQ, &queue) < 0) {
		close(s);
		return 0;
	}

	close(s);
	return queue;
}
#endif

static int lock_prefix(void)
{
#if !defined(macintosh)
	char *lockfile = get_buffer(1000);

	sprintf(lockfile, "%slock", imc_prefix);
	imc_lock_file = open(lockfile, O_CREAT|O_EXCL|O_RDWR, 0644);
	if (imc_lock_file < 0)
		imc_lock_file = open(lockfile, O_RDWR, 0644);
	if (imc_lock_file < 0) {
		imc_lerror("lock_prefix: open %s", lockfile);
		release_buffer(lockfile);
		return 0;
	}
	if (lockf(imc_lock_file, F_TLOCK, 1) < 0) {
		close(imc_lock_file);
		imc_lock_file = -1;
		release_buffer(lockfile);
		return 0;
	}
	release_buffer(lockfile);
#endif
	return 1;
}


static void unlock_prefix(void) {
#if !defined(macintosh)
	if (imc_lock_file < 0)
		return;

	lockf(imc_lock_file, F_ULOCK, 1);
	close(imc_lock_file);
	imc_lock_file=-1;
#endif
}

/* start up IMC */
void imc_startup_network(void) {
	IMCInfo *info;

	if (imc_active != IA_CONFIG2) {
		imc_logerror("imc_startup_network: called with imc_active = %d", imc_active);
		return;
	}

	if (!imc_siteinfo.name || !*imc_siteinfo.name) {
		imc_logerror("InfoName not set, not initializing");
		return;
	}

	if (!imc_siteinfo.email || !*imc_siteinfo.email) {
		imc_logerror("InfoEmail not set, not initializing");
		return;
	}

	imc_logstring("network initializing");

	imc_active = IA_UP;

	control = -1;

	if (imc_port)
		imc_startup_port();
	imc_stats.start			= imc_now;
	imc_stats.rx_pkts		= 0;
	imc_stats.tx_pkts		= 0;
	imc_stats.rx_bytes		= 0;
	imc_stats.tx_bytes		= 0;
	imc_stats.sequence_drops= 0;

	imc_add_event(20, ev_keepalive, NULL, 1);

	imc_mail_startup();		/* start up the mailer */

	if (!lock_prefix()) {
		imc_logstring("another process is using the same config prefix, not autoconnecting.");
		return;
	}

	//	do autoconnects
	START_ITER(iter, info, IMCInfo *, imc_info_list) {
		if (!IS_SET(info->flags, IMC_NOAUTO | IMC_CLIENT | IMC_DENY))
			imc_connect_to(info->name);
	} END_ITER(iter);
}


void imc_startup(const char *prefix) {
	if (imc_active != IA_NONE) {
		imc_logstring("imc_startup: called with imc_active = %d", imc_active);
		return;
	}

	imc_now = time(NULL);	//	start our clock

	imc_logstring("%s initializing", IMC_VERSIONID);

#ifdef USEIOCTL
	outqsize = getsndbuf();
	imc_logstring("found TIOCOUTQ=%d", outqsize);
#endif

	imc_prefix = str_dup(prefix);

	imc_sequencenumber = imc_now;
	strcpy(imc_lasterror, "no error");

	imc_readconfig();
	imc_readignores();

	imc_active = imc_name ? IA_CONFIG2 : IA_CONFIG1;

	if (imc_active == IA_CONFIG2)
		imc_startup_network();
}


void imc_shutdown_network(void) {
	IMCEvent *ev;
	IMCConnect *c;
	IMCRemInfo *p;

	if (imc_active < IA_UP) {
		imc_logerror("imc_shutdown_network: called with imc_active==%d", imc_active);
		return;
	}

	if (imc_lock) {
		imc_logerror("imc_shutdown_network: called from within imc_idle_select");
		return;
	}

	imc_logstring("shutting down network");

	if (imc_active == IA_LISTENING)
		imc_shutdown_port();

	imc_logstring("rx %d packets, %d bytes (%ld/second)", imc_stats.rx_pkts, imc_stats.rx_bytes,
			(imc_now == imc_stats.start) ? 0 : imc_stats.rx_bytes / (imc_now - imc_stats.start));
	imc_logstring("tx %d packets, %d bytes (%ld/second)", imc_stats.tx_pkts, imc_stats.tx_bytes,
			(imc_now == imc_stats.start) ? 0 : imc_stats.tx_bytes / (imc_now - imc_stats.start));
	imc_logstring("largest packet %d bytes", imc_stats.max_pkt);
	imc_logstring("dropped %d packets by sequence number", imc_stats.sequence_drops);

	imc_mail_shutdown();

	while ((c = imc_connect_list.Top())) {
		c->Close();
		delete c;
	}

	while ((p = imc_reminfo_list.Top())) {
		delete p;
	}

	while ((ev = imc_event_list.Top())) {
		imc_event_list.Remove(ev);
		delete ev;
	}

	unlock_prefix();

	imc_active = IA_CONFIG2;
}


/* close down imc */
void imc_shutdown(void) {
	IMCIgnore *ign;
	IMCInfo *info;

	if (imc_active == IA_NONE) {
		imc_logerror("imc_shutdown: called with imc_active = 0");
		return;
	}

	if (imc_active >= IA_UP)
		imc_shutdown_network();

	while ((ign = imc_ignore_list.Top())) {
		imc_ignore_list.Remove(ign);
		delete ign;
	}

	while ((info = imc_info_list.Top())) {
		delete info;
	}

	if (imc_active >= IA_UP)
		imc_shutdown_network();

	FREE(imc_prefix);

	if (imc_active >= IA_CONFIG2)
		FREE(imc_name);

	imc_name = NULL;
	imc_active = IA_NONE;
}


int imc_fill_fdsets(int maxfd, fd_set *read, fd_set *write, fd_set *exc) {
	IMCConnect *c;

	if (imc_active<IA_UP)
		return maxfd;

	/* set up fd_sets for select */
	if (imc_active>=IA_LISTENING) {
		if (maxfd < control)
			maxfd = control;
		FD_SET(control, read);
	}

	START_ITER(iter, c, IMCConnect *, imc_connect_list) {
		if (maxfd < c->desc)
			maxfd = c->desc;

		switch (c->state) {
			case IMC_CONNECTING:	/* connected/error when writable */
				FD_SET(c->desc, write);
				break;
			case IMC_CONNECTED:
			case IMC_WAIT1:
			case IMC_WAIT2:
				FD_SET(c->desc, read);
				if (c->outbuf[0])
					FD_SET(c->desc, write);
				break;
		}
	} END_ITER(iter);

	return maxfd;
}

int imc_get_max_timeout(void) {
	IMCEvent *p;

	START_ITER(iter, p, IMCEvent *, imc_event_list) {
		if (p->timed)
			return p->when - imc_now;
	} END_ITER(iter);

	return 60;	//	make sure we don't get too backlogged with events
}

/* shell around imc_idle_select */
void imc_idle(int s, int us) {
	fd_set read, write, exc;
	int maxfd;
	struct timeval timeout;
	int i;

	FD_ZERO(&read);
	FD_ZERO(&write);
	FD_ZERO(&exc);

	maxfd = imc_fill_fdsets(0, &read, &write, &exc);
	timeout.tv_sec = s;
	timeout.tv_usec = us;
	
	//	loop, ignoring signals
	if (maxfd)	while ((i = select(maxfd+1, &read, &write, &exc, &timeout)) < 0 && errno == EINTR);
	else		while ((i = select(0, NULL, NULL, NULL, &timeout)) < 0 && errno == EINTR);
    
	if (i < 0) {
		imc_lerror("imc_idle: select");
		imc_shutdown_network();
		return;
	}

	imc_idle_select(&read, &write, &exc, time(NULL));
}

/* low-level idle function: read/write buffers as needed, etc */
void imc_idle_select(fd_set *read, fd_set *write, fd_set *exc, time_t now) {
	const char *command;
	IMCPacket *p;
	IMCConnect *c;

	if (imc_active < IA_CONFIG1)
		return;

	if (imc_lock) {
		imc_logerror("imc_idle_select: recursive call");
		return;
	}

	imc_lock = 1;

	if (imc_sequencenumber < (unsigned long)imc_now)
		imc_sequencenumber = (unsigned long)imc_now;

	imc_run_events(now);

	if (imc_active < IA_UP) {
		imc_lock=0;
		return;
	}

	//	handle results of the select

	if (imc_active >= IA_LISTENING && FD_ISSET(control, read))
		do_accept();

	LListIterator<IMCConnect *>	iter(imc_connect_list);

	while ((c = iter.Next())) {
		if (c->state != IMC_CLOSED && FD_ISSET(c->desc, exc))
			c->Close();

		if (c->state != IMC_CLOSED && FD_ISSET(c->desc, read))
			c->Read();

		while (c->state != IMC_CLOSED &&
				(c->spamtime1 >= 0 || c->spamcounter1 <= IMC_SPAM1MAX) &&
				(c->spamtime2 >= 0 || c->spamcounter2 <= IMC_SPAM2MAX) &&
				(command = getline(c->inbuf))) {
			if (strlen(command) > imc_stats.max_pkt)
				imc_stats.max_pkt = strlen(command);

			if (!c->spamcounter1 && !c->spamtime1)
				imc_add_event(IMC_SPAM1INTERVAL, ev_spam1, c, 0);
			c->spamcounter1++;

			if (!c->spamcounter2 && !c->spamtime2)
				imc_add_event(IMC_SPAM2INTERVAL, ev_spam2, c, 0);
			c->spamcounter2++;

			c->Debug(0, command);	//	log incoming packets

			switch (c->state) {
				case IMC_CLOSED:									break;
				case IMC_WAIT1:		c->ClientPassword(command);		break;
				case IMC_WAIT2:		c->ServerPassword(command);		break;
				case IMC_CONNECTED:
					p = c->InterpretPacket(command);
					if (p) {
#ifdef IMC_PARANOIA
						/* paranoia: check the last entry in the path is the same as the
						 * sending mud. Also check the first entry to see that it matches
						 * the sender.
						 */
						imc_stats.rx_pkts++;

						if (str_cmp(c->info->name, imc_lastinpath(p->i.path)))
							imc_logerror("PARANOIA: packet from %s allegedly from %s",
									 c->info->name, imc_lastinpath(p->i.path));
						else if (str_cmp(imc_mudof(p->i.from), imc_firstinpath(p->i.path)))
							imc_logerror("PARANOIA: packet from %s has firstinpath %s",
									 p->i.from, imc_firstinpath(p->i.path));
						else
							p->Forward();	//	only forward if its a valid packet!
#else
						imc_stats.rx_pkts++;
						p->Forward();
#endif
						p->data.Free();
					}
					break;
			}
		}
	}

	iter.Reset();
	while ((c = iter.Next())) {
		if (c->state != IMC_CLOSED && (FD_ISSET(c->desc, write) || c->newoutput)) {
			c->newoutput=0;
			c->Write();
		}
	}

	iter.Reset();
	while ((c = iter.Next())) {
		if (c->state == IMC_CLOSED)
			delete c;
	}

	imc_lock = 0;
}


/* connect to given mud */
int imc_connect_to(const char *mud) {
	IMCInfo *	i;
	IMCConnect *c;
	SInt32		desc;
	struct sockaddr_in sa;
	char *		buf;
	SInt32		r;

	if (imc_active == IA_NONE) {
		imc_qerror("IMC is not active");
		return 0;
	}
    
	if (!(i = imc_getinfo(mud))) {
		imc_qerror("%s: unknown mud name", mud);
		return 0;
	}

	if (i->connection) {
		imc_qerror("%s: already connected", mud);
		return 0;
	}

	if (i->flags & IMC_CLIENT) {
		imc_qerror("%s: client-only flag is set", mud);
		return 0;
	}

	if (i->flags & IMC_DENY) {
		imc_qerror("%s: deny flag is set", mud);
		return 0;
	}

	if (!(i->flags & IMC_QUIET))
		imc_logstring("connect to %s", mud);

	/*  warning: this blocks. It would be better to farm the query out to
	 *  another process, but that is difficult to do without lots of changes
	 *  to the core mud code. You may want to change this code if you have an
	 *  existing resolver process running.
	 */
#if !defined(macintosh)
	sa.sin_addr.s_addr = inet_addr(i->host);
#else
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
	if (sa.sin_addr.s_addr == -1UL) {
		struct hostent *hostinfo;

		if (!(hostinfo = gethostbyname(i->host))) {
			imc_logerror("imc_connect: couldn't resolve hostname");
			return 0;
		}

		sa.sin_addr.s_addr = *(unsigned long *) hostinfo->h_addr;
	}

	sa.sin_port   = htons(i->port);
	sa.sin_family = AF_INET;

	desc = socket(AF_INET, SOCK_STREAM, 0);
	if (desc < 0) {
		imc_lerror("socket");
		return 0;
	}

	r = fcntl(desc, F_GETFL, 0);
	if ((r < 0) || (fcntl(desc, F_SETFL, O_NONBLOCK | r) < 0)) {
		imc_lerror("imc_connect: fcntl");
		close(desc);
		return 0;
	}

	if (connect(desc, (struct sockaddr *)&sa, sizeof(sa)) < 0)
		if (errno != EINPROGRESS) {
			imc_lerror("connect");
			close(desc);
			return 0;
		}

	c = new IMCConnect();

	c->desc			= desc;
	c->state		= IMC_CONNECTING;
	c->info			= i;

	imc_add_event(IMC_LOGIN_TIMEOUT, ev_login_timeout, c, 1);

	buf = get_buffer(IMC_DATA_LENGTH);
	sprintf(buf, "PW %s %s version=%d", imc_name, i->clientpw, IMC_VERSION);
	c->Send(buf);
	release_buffer(buf);
	
	return 1;
}

int imc_disconnect(const char *mud) {
	IMCConnect *c;
	IMCInfo *i;
	int d;

	if (imc_active == IA_NONE) {
		imc_qerror("IMC is not active");
		return 0;
	}

	LListIterator<IMCConnect *>	iter(imc_connect_list);
	
	if ((d=atoi(mud))!=0) {
		/* disconnect a specific descriptor */

		while ((c = iter.Next())) {
			if (c->desc==d) {
				imc_logstring("disconnect descriptor %s", c->GetName());
				c->Close();
				return 1;
			}
		}

		imc_qerror("%d: no matching descriptor", d);
		return 0;
	}

	i = imc_getinfo(mud);
	if (!i) {
		if (str_cmp(mud, "unknown")) { /* disconnect all unknown muds */
			imc_qerror("%s: unknown mud", mud);
			return 0;
		}
	}

	imc_logstring("disconnect %s", mud);

	while ((c = iter.Next()))
		if (c->info==i)
			c->Close();

	return 1;
}

void IMCPacket::Send(void) {
	if (imc_active < IA_UP) {
		imc_logerror("imc_send when not active!");
		return;
	}
  
	//	initialize packet fields that the caller shouldn't/doesn't set
	this->i.stamp		= 0;
	this->i.path[0]		= 0;
  	this->i.sequence	= imc_sequencenumber++;
	if (!imc_sequencenumber)
		imc_sequencenumber++;
  
	imc_sncpy(this->i.to, this->to, IMC_NAME_LENGTH);
	imc_sncpy(this->i.from, this->from, IMC_NAME_LENGTH - 1);
	strcat(this->i.from, "@");
	imc_sncpy(this->i.from + strlen(this->i.from), imc_name, IMC_NAME_LENGTH - strlen(this->i.from));

	this->Forward();
}


IMCInfo::IMCInfo(void) : name(NULL), host(NULL), connection(NULL), port(0),
		serverpw(NULL), clientpw(NULL), flags(0), timer_duration(IMC_MIN_RECONNECT_TIME),
		rcvstamp(0), noforward(0), last_connected(0) {
	imc_info_list.Append(this);
}


IMCInfo::~IMCInfo(void) {
	IMCConnect *c;

	LListIterator<IMCConnect *>	iter(imc_connect_list);
	
	while ((c = iter.Next()))
		if (c->info == this)
			c->Close();

	imc_info_list.Remove(this);
	
	if (this->name)		free(this->name);
	if (this->host)		free(this->host);
	if (this->clientpw)	free(this->clientpw);
	if (this->serverpw)	free(this->serverpw);

	imc_cancel_event(NULL, this);
}
