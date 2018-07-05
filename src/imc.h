/*
 * IMC2 - an inter-mud communications protocol
 *
 * imc.h: the core protocol definitions
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

#ifndef IMC_H
#define IMC_H

#include "types.h"
#include "stl.llist.h"
#include "imc-config.h"

//	Activation states
#define IA_NONE				0
#define IA_CONFIG1			1
#define IA_CONFIG2			2
#define IA_UP				3
#define IA_LISTENING		4

//	IMCConnection states
#define IMC_CLOSED			0
#define IMC_CONNECTING		1
#define IMC_WAIT1			2
#define IMC_WAIT2			3
#define IMC_CONNECTED		4

//	IMCConnection flags
#define IMC_NOAUTO			(1 << 0)
#define IMC_CLIENT			(1 << 1)
#define IMC_RECONNECT		(1 << 2)
#define IMC_BROADCAST		(1 << 3)
#define IMC_DENY			(1 << 4)
#define IMC_QUIET			(1 << 5)

//	IMCIgnore types
#define IMC_IGNORE			1		//	ignore sender
#define IMC_NOTRUST			2		//	don't trust sender's levels
#define IMC_TRUST			3		//	do trust sender's levels

//	IMCIgnore match flags
#define IMC_IGNORE_EXACT	0		//	match exactly
#define IMC_IGNORE_PREFIX	1		//	ignore prefixes when matching
#define IMC_IGNORE_SUFFIX	2		//	ignore suffixes when matching

//	IMCCharData invisibility state
#define IMC_INVIS			1
#define IMC_HIDDEN			2


class IMCFlagType;
class IMCData;
class IMCCharData;
class IMCPacket;
class IMCConnect;
class IMCInfo;


struct IMCFlagType {
	char *				name;
	SInt32				value;
};


class IMCData {
public:
						IMCData(void) { value[0] = key[0] = '\0'; };
						~IMCData(void) { };

	void				Init(void);
	void				Free(void);
	void				Clone(const IMCData *p);

	const char *		GetKey(const char *key, const char *def) const;
	SInt32				GetKey(const char *key, SInt32 def) const;
	
	void				AddKey(const char *key, const char *value);
	void				AddKey(const char *key, SInt32 value);
	
	char *				key[IMC_MAX_KEYS];
	char *				value[IMC_MAX_KEYS];
};


//	a player on IMC
class IMCCharData {
public:
						IMCCharData(void) { };
						~IMCCharData(void) { };
	
	static const IMCCharData *	Get(const CharData *ch);
	bool				Visible(const IMCCharData *viewed) const;
	bool				Visible(const CharData *viewed) const;
	
	char				name[IMC_NAME_LENGTH];	//	name of character
	bool				invis;					//	invisible to IMC?
	SInt8				level;					//	trust level
	SInt8				wizi;					//	wizi level
};


class IMCPacket {
public:
						IMCPacket(void) {
							to[0] = from[0] = type[0] = '\0';
							i.to[0] = i.from[0] = i.path[0] = '\0';
							i.sequence = 0;
							i.stamp = 0;
						};
						~IMCPacket(void) { };

	void				Send(void);
	void				Receive(void);
	bool				Receive(SInt32 bcast);	//	ICE
	
	void				Forward(void);
	void				GetData(IMCCharData *d);
	void				SetData(const IMCCharData *d);
	
	char				to[IMC_NAME_LENGTH];	//	Destination
	char				from[IMC_NAME_LENGTH];	//	Source
	char				type[IMC_NAME_LENGTH];	//	Type of packet
	IMCData				data;
	struct {
		char			to[IMC_NAME_LENGTH];
		char			from[IMC_NAME_LENGTH];
		char			path[IMC_NAME_LENGTH];
		UInt32			sequence;
		SInt32			stamp;
	} i;
protected:
	bool				CanForward(void);
};


class IMCConnect {
public:
						IMCConnect(void);
						~IMCConnect(void);
	
	void				Close(void);
	void				Read(void);
	void				Write(void);
	void				Send(const char *line);
	void				Debug(SInt32 out, const char *string);
	
	IMCPacket *			InterpretPacket(const char *line);
	void				SendPacket(const IMCPacket *p);
	
	void				ClientPassword(const char *argument);
	void				ServerPassword(const char *argument);
	
	const char *		GetName(void);
	
	IMCInfo *			info;
	
	SInt32				desc;
	UInt16				state;
	UInt16				version;
	
	SInt16				newoutput;
	
	char *				inbuf;
	SInt32				insize;
	
	char *				outbuf;
	SInt32				outsize;
	
	SInt32				spamcounter1, spamcounter2;
	SInt32				spamtime1, spamtime2;
};


class IMCInfo {
public:
						IMCInfo(void);
						~IMCInfo(void);
						
	void				SetupReconnect(void);
						
	char *				name;
	char *				host;
	
	IMCConnect *		connection;
	UInt16				port;
	
	char *				serverpw;
	char *				clientpw;
	
	Flags				flags;

	time_t				timer_duration;
	
	SInt32				rcvstamp;
	Flags				noforward;
	
	time_t				last_connected;
};


class IMCStats {
public:
	time_t				start;
	SInt32				rx_pkts;
	SInt32				tx_pkts;
	SInt32				rx_bytes;
	SInt32				tx_bytes;
	
	SInt32				max_pkt;
	SInt32				sequence_drops;
};

#define IMC_REMINFO_NORMAL 0
#define IMC_REMINFO_EXPIRED 1

class IMCRemInfo {
public:
					IMCRemInfo(void);
					~IMCRemInfo(void);
	
	char *			name;
	char *			version;
	time_t			alive;
	SInt32			ping;
	SInt32			type;
	SInt32			hide;
	char *			route;
	UInt32			top_sequence;
};


class IMCEvent {
public:
					IMCEvent(void);
					~IMCEvent(void);
	time_t			when;
	void			(*callback)(Ptr data);
	Ptr				data;
	SInt32			timed;
};


struct IMCVInfo {
	SInt32			version;
	const char *	(*generate)(const IMCPacket *);
	IMCPacket *		(*interpret)(const char *);
};


/* an entry in the memory table */
typedef struct
{
  char *from;
  unsigned long sequence;
} _imc_memory;


class IMCIgnore {
public:
						IMCIgnore(void);
						~IMCIgnore(void);
	char *				name;
	SInt32				match;
	SInt32				type;
};


class IMCMail {
public:
						IMCMail(void);
						~IMCMail(void);
	
	void				Write(FILE *out);
	
	char *				from;
	char *				to;
	char *				text;
	char *				date;
	char *				subject;
	char *				id;
	time_t				received;
	SInt32				usage;
};


class IMCQNode {
public:
						IMCQNode(void);
						~IMCQNode(void);
						
	void				Write(FILE *out);
						
	IMCMail *			data;
	char *				tomud;
	IMCQNode *			next;
};


class IMCMailID {
public:
						IMCMailID(void);
						~IMCMailID(void);
						
	void				Write(FILE *out);
	
	char *				id;
	time_t				received;
};


class IMCSiteInfo {
public:
	char *				name;
	char *				host;
	char *				email;
	char *				imail;
	char *				www;
	char *				details;
	char *				flags;
};
/* data structures */

extern IMCSiteInfo imc_siteinfo;

/* the packet memory table */
extern _imc_memory imc_memory[IMC_MEMORY];
/* the version lookup table */
extern IMCVInfo imc_vinfo[];

/* global stats struct */
extern IMCStats imc_stats;

/* flag and state tables */
extern const IMCFlagType	imc_connection_flags[];
extern const IMCFlagType	imc_ignore_types[];
extern const IMCFlagType	imc_state_names[];
extern const IMCFlagType	imc_active_names[];


extern SInt32	imc_lock;				//	global recursion lock
extern char *	imc_name;				//	the local IMC name
extern UInt16	imc_port;				//	the local IMC port
extern UInt32	imc_bind;				//	IP to bind to
extern char *	imc_prefix;				//	the configuration prefix
extern bool		imc_is_router;			//	run as a router?

extern LList<IMCIgnore *>	imc_ignore_list;	//	the ignore list
extern LList<IMCRemInfo *>	imc_reminfo_list;	//	the reminfo list

extern LList<IMCConnect *>	imc_connect_list;	//	the connection list
extern LList<IMCInfo *>		imc_info_list;		//	the configured connection list
extern LList<IMCEvent *>	imc_event_list;	//	the event list, and recycle list


extern time_t imc_now;					//	the current time
extern UInt32 imc_sequencenumber;		//	next sequence number to use
extern SInt32 imc_active;				//	IMC state
extern SInt32 imc_lock_file;			//	-1 if prefix is in use (when network is up), >=0 otherwise

extern LList<IMCMail *>		imc_ml_head;			//	mail list
extern IMCQNode *imc_mq_head, *imc_mq_tail;	//	mail queue
extern LList<IMCMailID *>	imc_idlist;			//	ID list

extern char imc_lasterror[IMC_DATA_LENGTH];	//	last reported error

/* imc-util.c exported functions */

/* static buffer handling */
char *imc_getsbuf(int len);
void imc_shrinksbuf(char *buf);

/* reminfo handling */
IMCRemInfo *imc_find_reminfo(const char *name, int type);

/* info handling */
IMCInfo *imc_getinfo(const char *name);

/* state/flag handling */
const char *imc_flagname(int value, const IMCFlagType *table);
int imc_flagvalue(const char *name, const IMCFlagType *table);
const char *imc_statename(int value, const IMCFlagType *table);
int imc_statevalue(const char *name, const IMCFlagType *table);

/* string manipulations */
const char *imc_nameof(const char *name);
const char *imc_mudof(const char *name);
const char *imc_makename(const char *name, const char *mud);
const char *imc_firstinpath(const char *path);
const char *imc_lastinpath(const char *path);
const char *imc_getarg(const char *arg, char *buf, int length);
int imc_hasname(const char *list, const char *name);
void imc_addname(char **list, const char *name);
void imc_removename(char **list, const char *name);
void imc_slower(char *what);
void imc_sncpy(char *dest, const char *src, int count);

/* logging */

void imc_logstring(const char *format,...) __attribute__((format(printf,1,2)));
void imc_logerror(const char *format,...) __attribute__((format(printf,1,2)));
void imc_qerror(const char *format,...) __attribute__((format(printf,1,2)));
void imc_lerror(const char *format,...) __attribute__((format(printf,1,2)));
const char *imc_error(void);

/* external log interfaces */
void imc_log(const char *string);

/* imc-events.c exported functions */

/* event handling */
void imc_add_event(int when, void (*callback)(Ptr), Ptr data, int timed);
void imc_cancel_event(void (*callback)(Ptr), Ptr data);
void imc_run_events(time_t);
int imc_next_event(void (*callback)(Ptr), Ptr data);

/* all events (in imc-events.c unless otherwise specified) */
void ev_expire_reminfo(Ptr data);
void ev_drop_reminfo(Ptr data);
void ev_login_timeout(Ptr data);       /* imc.c */
void ev_reconnect(Ptr data);
void ev_shrink_input(Ptr data);
void ev_shrink_output(Ptr data);
void ev_keepalive(Ptr data);
void ev_spam1(Ptr data);
void ev_spam2(Ptr data);
void ev_qnode_expire(Ptr data);        /* imc-mail.c */
void ev_mailid_expire(Ptr data);       /* imc-mail.c */
void ev_qnode_send(Ptr data);          /* imc-mail.c */


/* imc.c exported functions */

void imc_startup_port(void);
void imc_startup_network(void);
void imc_startup(const char *prefix);
void imc_shutdown(void);
void imc_shutdown_network(void);
void imc_shutdown_port(void);
void imc_idle(int s, int us);

int imc_get_max_timeout(void);
int imc_fill_fdsets(int maxfd, fd_set *read, fd_set *write, fd_set *exc);
void imc_idle_select(fd_set *read, fd_set *write, fd_set *exc, time_t now);

int imc_connect_to(const char *mud);
int imc_disconnect(const char *mud);

/* imc-interp.c exported functions */

void imc_sendignore(const char *to);

void imc_send_chat(const IMCCharData *from, int channel,
		   const char *argument, const char *to);
void imc_send_emote(const IMCCharData *from, int channel,
		    const char *argument, const char *to);
void imc_send_tell(const IMCCharData *from, const char *to,
		   const char *argument, int isreply);
void imc_send_who(const IMCCharData *from, const char *to,
		  const char *type);
void imc_send_whoreply(const char *to, const char *data, int sequence);
void imc_send_whois(const IMCCharData *from, const char *to);
void imc_send_whoisreply(const char *to, const char *data);
void imc_send_beep(const IMCCharData *from, const char *to);
void imc_send_keepalive(void);
void imc_send_ping(const char *to, int time_s, int time_u);
void imc_send_pingreply(const char *to, int time_s, int time_u, const char *path);
void imc_send_whois(const IMCCharData *from, const char *to);
void imc_send_whoisreply(const char *to, const char *text);

void imc_whoreply_start(const char *to);
void imc_whoreply_add(const char *text);
void imc_whoreply_end(void);

/* callbacks that need to be provided by the interface layer */

void imc_recv_chat(const IMCCharData *from, int channel,
		   const char *argument);
void imc_recv_emote(const IMCCharData *from, int channel,
		    const char *argument);
void imc_recv_tell(const IMCCharData *from, const char *to,
		   const char *argument, int isreply);
void imc_recv_whoreply(const char *to, const char *data, int sequence);
void imc_recv_who(const IMCCharData *from, const char *type);
void imc_recv_beep(const IMCCharData *from, const char *to);
void imc_recv_keepalive(const char *from, const char *version, const char *flags);
void imc_recv_ping(const char *from, int time_s, int time_u, const char *path);
void imc_recv_pingreply(const char *from, int time_s, int time_u, const char *pathto, const char *pathfrom);
void imc_recv_whois(const IMCCharData *from, const char *to);
void imc_recv_whoisreply(const char *to, const char *text);
void imc_recv_inforequest(const char *from, const char *category);

void imc_traceroute(int ping, const char *pathto, const char *pathfrom);

/* other functions */
const char *imc_sockets(void);
const char *imc_getstats(void);


/* imc-config.c exported functions */

/* ignore handling */
IMCIgnore *imc_findignore(const char *who, int type);
int imc_isignored(const char *who);
void imc_addignore(const char *who, int flags);
int imc_delignore(const char *who);
//imc_ignore_data *imc_newignore(void);
//void imc_freeignore(imc_ignore_data *ign);

const char *imc_ignore(const char *what);
const char *imc_list(int level);
int imc_command(const char *argument);
int imc_saveconfig(void);
int imc_readconfig(void);
int imc_saveignores(void);
int imc_readignores(void);




/* imc-mail.c exported functions */

void imc_recv_mailok(const char *from, const char *id);
void imc_recv_mailrej(const char *from, const char *id, const char *reason);
void imc_recv_mail(const char *from, const char *to, const char *date,
		   const char *subject, const char *id, const char *text);
void imc_send_mail(const char *from, const char *to, const char *date,
		   const char *subject, const char *text);

char *imc_mail_arrived(const char *from, const char *to, const char *date,
		       const char *subject, const char *text);

void imc_mail_startup(void);
void imc_mail_shutdown(void);

char *imc_mail_showqueue(void);

#endif
