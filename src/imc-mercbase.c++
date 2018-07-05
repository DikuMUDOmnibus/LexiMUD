/*
 * IMC2 - an inter-mud communications protocol
 *
 * imc-mercbase.c: integrated interface code for Merc-derived codebases.
 *                 Now also includes patches for Circle.
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

/* Modifications by:
 *  Erwin S. Andreasen <erwin@pip.dknet.dk>    (Envy changes)
 *  Erin Moeller <altrag@game.org>             (Merc, SMAUG changes)
 *  Stephen Zepp <zenithar@fullnet.net>        (Ack! changes)
 *  Trevor Man <tman@iname.com>                (Circle changes)
 * 
 * integration by Oliver Jowett
 */

#include "imc.h"
#include "mail.h"
#include "handler.h"
#include "find.h"
#include "buffer.h"
#include "clans.h"
#include "staffcmds.h"

#if defined(macintosh) || defined(CIRCLE_WINDOWS)
void gettimeofday(struct timeval *t, struct timezone *dummy);
#endif

/* ick! */
#define IN_IMC
#include "imc-mercdefs.h"
#include "imc-mercbase.h"

ACMD(do_rchat);
ACMD(do_rimm);
ACMD(do_rcode);

int str_prefix(const char *astr, const char *bstr);


/* Color configuration (more tables to configure.. sowwy)
 *
 * The various #defines here are used within the IMC code to format 
 * things like rtells themselves (not the text of the message, but the
 * "a@b rtells you 'blah'" structure)
 *
 * Each C_? #define maps an IMC color code (the ?) to one of your color codes.
 *
 * The various FORMATs define colors for rbeep, etc
 *
 * this may seem ugly, but it makes things expandable - and it will probably
 * go away in the near future anyway
 *
 */

#define RTELL_FORMAT_1 "You rtell %s '%s" COL(d) "'.\r\n"
#define RTELL_FORMAT_2 "%s rtells you '%s" COL(d) "'.\r\n"
#define RBEEP_FORMAT_1 "You rbeep %s.\r\n"
#define RBEEP_FORMAT_2 "\a%s rbeeps you.\r\n"

#define COL(x) C_##x
#define C_b "`b"
#define C_g "`g"
#define C_r "`r"
#define C_y "`y"
#define C_m "`m"
#define C_c "`c"
#define C_w "`w"
#define C_D "`K"
#define C_B "`B"
#define C_G "`G"
#define C_R "`R"
#define C_Y "`Y"
#define C_M "`M"
#define C_C "`C"
#define C_W "`W"
#define C_x "`n"
#define C_d "`n"


/*  maps IMC standard -> mud local color codes
 *  let's be unique, noone uses ~ :>
 */

/* Most of this table will be generated from the #defines above;
 * you will just need to add the exceptions.
 */

struct {
	char *	imc;        /* IMC code to convert */
	char *	mud;        /* Equivalent mud code */
}
trans_table[]= {
	/* common color definitions, derived from the #defines above */

	{ "~!", COL(x) },	//	reset
	{ "~d", COL(d) },	//	default

	{ "~b", COL(b) },	//	dark blue
	{ "~g", COL(g) },	//	dark green
	{ "~r", COL(r) },	//	dark red
	{ "~y", COL(y) },	//	dark yellow / brown
	{ "~m", COL(m) },	//	dark magenta / purple
	{ "~c", COL(c) },	//	cyan
	{ "~w", COL(w) },	//	white

	{ "~D", COL(D) },	//	dark grey / "z" ?
	{ "~B", COL(B) },	//	light blue
	{ "~G", COL(G) },	//	light green
	{ "~R", COL(R) },	//	light red
	{ "~Y", COL(Y) },	//	light yellow
	{ "~M", COL(M) },	//	light magenta / purple
	{ "~C", COL(C) },	//	light cyan
	{ "~W", COL(W) },	//	bright white

	{ "\007", "`*" },
	{ "", "`f" },
	{ "", "`F" },
	{ "", "`i" },
	{ "", "`I" },

	{ "`",  "``" },
	{ "^",  "^^" },

	{ "~~", "~"  },

	/* background colors - no support for these in IMC */

	{ "",   "^b" },
	{ "",   "^g" },
	{ "",   "^r" },
	{ "",   "^O" },
	{ "",   "^p" },
	{ "",   "^c" },
	{ "",   "^w" },
	{ "",   "^z" },
	{ "",   "^B" },
	{ "",   "^G" },
	{ "",   "^R" },
	{ "",   "^Y" },
	{ "",   "^P" },
	{ "",   "^C" },
	{ "",   "^W" },

};

#define numtrans (sizeof(trans_table)/sizeof(trans_table[0]))

/* convert from imc color -> mud color */

const char *color_itom(const char *s) {
	char *		buf=imc_getsbuf(IMC_DATA_LENGTH);
	const char *current;
	char *		out;
	SInt32		i, l, count;

	for (current = s, out = buf, count = 0; *current && count < IMC_DATA_LENGTH; ) {
		if ((*current == '~') || (*current == IMC_COLORCHAR) || (*current == IMC_COLORCHAR2)) {
			for (i = 0; i < numtrans; i++) {
				l = strlen(trans_table[i].imc);
				if (l && !strncmp(current, trans_table[i].imc, l))
				break;
			}
			
			if (i != numtrans) {	//	match
				int len;

				len = strlen(trans_table[i].mud);
				count += len;
				if (count >= IMC_DATA_LENGTH)
					break;
				memcpy(out, trans_table[i].mud, len);
				out += len;
				current += l;
				continue;
			}
		}

		*out++ = *current++;
		count++;
	}
  
	*out = '\0';
	imc_shrinksbuf(buf);
	return buf;
}

/* convert from mud color -> imc color */

const char *color_mtoi(const char *s)
{
  char *buf=imc_getsbuf(IMC_DATA_LENGTH);
  const char *current;
  char *out;
  int i, l;
  int count;

  for (current=s, out=buf, count=0; *current && count<IMC_DATA_LENGTH; )
  {
    if (*current=='~' || *current==IMC_COLORCHAR || *current==IMC_COLORCHAR2)
    {
      for (i=0; i<numtrans; i++)
      {
	l=strlen(trans_table[i].mud);
	if (l && !strncmp(current, trans_table[i].mud, l))
	  break;
      }
      
      if (i!=numtrans)      /* match */
      {
	int len;

	len=strlen(trans_table[i].imc);
	count+=len;
	if (count>=IMC_DATA_LENGTH)
	  break;
	memcpy(out, trans_table[i].imc, len);
	out+=len;
	current+=l;
	continue;
      }
    }

    *out++=*current++;
    count++;
  }
  
  *out=0;
  imc_shrinksbuf(buf);
  return buf;
}


static struct {
	SInt32		number;
	const char *name;
	const char *chatstr;
	const char *emotestr;
	Flags		flag;
	SInt32		minlevel;
	char *		to;
} imc_channels[]= {
	{
		2,
		"RInfo",
		COL(y) "[" COL(G) "RInfo" COL(y) "] %s announces '%s" COL(d) "'\r\n",
		COL(y) "[" COL(G) "RInfo" COL(y) "] %s %s" COL(d) "\r\n",
		IMC_RINFO,
		LVL_CODER,
		"*",
	},
	{
		15,
		"ICE",
		COL(y) "[" COL(G) "ICE" COL(y) "] %s %s" COL(d) "\r\n",
		COL(y) "[" COL(G) "ICE" COL(y) "] %s %s" COL(d) "\r\n",
		IMC_ICE,
		LVL_STAFF,
		"*"
	}

  /*
   *  An example of how to set up a channel that is only distributed between
   *  the three muds 'mud1', 'mud2', and 'mud3'.

  {
    76,                            -- channel number to use, change this!
    "LocalChannel",                -- the channel name
    "%s localchannels '%s'\r\n",   -- format for a normal message
    "[LocalChannel] %s %s\r\n",    -- format for an emote
    IMC_LOCALCHANNEL,              -- flag in imc_deaf, etc
    LEVEL_IMMORTAL,                -- min. level for channel
    "mud1 mud2 mud3"               -- muds to distribute to (include your own)
  }
   *
   */
};

#define numchannels (sizeof(imc_channels)/sizeof(imc_channels[0]))


const IMCCharData *IMCCharData::Get(const CharData *ch) {
	static IMCCharData d;

	if (!ch)	{	//	fake system character
		d.wizi = 0;
		d.level = -1;
		d.invis = 0;
		strcpy(d.name, "*");
		return &d;
	}

	if (IS_RINVIS(ch))	d.invis = true;
	else				d.invis = false;

	d.wizi = GET_INVIS_LEV(ch);

	d.level = (GET_LEVEL(ch) < LVL_STAFF) ? 1 : -1;
	strcpy(d.name, ch->RealName());

	return &d;
}


bool IMCCharData::Visible(const IMCCharData *viewed) const {
	return !viewed->invis && ((this->level < 0) || (viewed->wizi == 0));
}

bool IMCCharData::Visible(const CharData *viewed) const {
	return this->Visible(IMCCharData::Get(viewed));
}


static const char *getname(CharData *ch, const IMCCharData *vict) {
	const char *mud, *name;
	const IMCCharData *chdata;

	mud = imc_mudof(vict->name);
	name = imc_nameof(vict->name);
	chdata = IMCCharData::Get(ch);

	if (!str_cmp(mud, imc_name))
		return IMCCharData::Get(ch)->Visible(vict) ? imc_nameof(name) : "someone";
	else if (IMCCharData::Get(ch)->Visible(vict))
		return vict->name;
	else {
		char *buf = imc_getsbuf(IMC_NAME_LENGTH);
		sprintf(buf, "someone@%s", mud);
		imc_shrinksbuf(buf);
		return buf;
	}
}


static void send_rchannel(CharData *ch, char *argument, int number) {
	char *arg;
	char *arg2;
	int chan;

	CHECKIMC(ch);

	skip_spaces(argument);

	for (chan = 0; chan < numchannels; chan++)
		if (imc_channels[chan].number == number)
			break;

	if (chan == numchannels) {
		send_to_char("That channel doesn't seem to exist!\r\n", ch);
		log("SYSERR: IMC: IMC channel %d doesn't exist?!", number);
	} else if (!CAN(ch, imc_channels[chan].flag, imc_channels[chan].minlevel))
		ch->Send("You are not authorized to use %s.\r\n", imc_channels[chan].name);
	else if (!*argument) {
		if (IS_SET(IMC_DEAF_FLAGS(ch), imc_channels[chan].flag)) {
			ch->Send("%s channel is now ON.\r\n", imc_channels[chan].name);
			REMOVE_BIT(IMC_DEAF_FLAGS(ch), imc_channels[chan].flag);
		} else {
			ch->Send("%s channel is now OFF.\r\n", imc_channels[chan].name);
			SET_BIT(IMC_DEAF_FLAGS(ch), imc_channels[chan].flag);
		}
	} else if (IS_NOCHAN(ch))
		send_to_char("The gods have revoked your channel priviliges.\r\n", ch);
	else if (IS_QUIET(ch))
		send_to_char("You must turn off quiet mode first.\r\n", ch);
	else if (IS_SILENT(ch))
		send_to_char ("You can't seem to break the silence.\r\n",ch);
	else if (IS_RINVIS(ch))
		send_to_char ("You cannot use IMC channels while invisible to IMC.\r\n", ch);
	else {
		REMOVE_BIT(IMC_DEAF_FLAGS(ch), imc_channels[chan].flag);

		//	NargoRoth's bugfix here
		arg = get_buffer(MAX_STRING_LENGTH);

		arg2 = one_argument(argument, arg);

		skip_spaces(arg2);

		if (!str_cmp(arg, ",") || !str_cmp(arg, "emote"))
			imc_send_emote(IMCCharData::Get(ch), number, color_mtoi(arg2), imc_channels[chan].to);
		else
			imc_send_chat(IMCCharData::Get(ch), number, color_mtoi(argument), imc_channels[chan].to);
		release_buffer(arg);
	}
}


ACMD(do_rchat) {
	send_rchannel(ch, argument, 0);
}


ACMD(do_rimm) {
	send_rchannel(ch, argument, 1);
}


ACMD(do_rinfo) {
	send_rchannel(ch, argument, 2);
}


ACMD(do_rcode) {
	send_rchannel(ch, argument, 3);
}


ACMD(do_rtell) {
	char *buf = get_buffer(MAX_INPUT_LENGTH);

	CHECKIMC(ch);

	argument = one_argument(argument, buf);

	skip_spaces(argument);

	if (!*buf || !strchr(buf, '@') || !*argument)
		send_to_char("rtell who@where what?\r\n", ch);
	else if (!CAN(ch, IMC_RTELL, IMC_LEVEL_RTELL))
		send_to_char("You are not authorised to use rtell.\r\n", ch);
	else if (IS_NOTELL(ch))
		send_to_char("You cannot tell!\r\n", ch);
	else if (IS_RINVIS(ch))
		send_to_char ("You cannot rtell while invisible to IMC.\r\n", ch);
	else if (IS_SET(IMC_DEAF_FLAGS(ch), IMC_RTELL))
		send_to_char("Enable incoming rtells first ('rchannel +rtell').\r\n", ch);
	else if (IS_QUIET(ch))
		send_to_char("You must turn off quiet mode first.\r\n", ch);
	else {
		CHECKMUDOF(ch, buf);
		imc_send_tell(IMCCharData::Get(ch), buf, color_mtoi(argument), 0);

		ch->Send(RTELL_FORMAT_1, buf, argument);
	}
	release_buffer(buf);
}


ACMD(do_rreply) {
	CHECKIMC(ch);

	skip_spaces(argument);

	if (!IMC_RREPLY(ch))
		send_to_char("rreply to who?\r\n", ch);
	else if (!*argument)
		send_to_char("rreply what?\r\n", ch);
	/* just check for deny, since you can rtell someone < IMC_LEVEL_RTELL */
	else if (IS_SET(IMC_DENY_FLAGS(ch), IMC_RTELL))
		send_to_char("You are not authorised to use rreply.\r\n", ch);
	else if (IS_NOTELL(ch))
	    send_to_char("You cannot tell!\r\n", ch);
	else if (IS_RINVIS(ch))
		send_to_char ("You cannot rreply while invisible to IMC.\r\n", ch);
	else if (IS_SET(IMC_DEAF_FLAGS(ch), IMC_RTELL))
		send_to_char("Enable incoming rtells first ('rchannel +rtell').\r\n", ch);
	else if (IS_QUIET(ch))
		send_to_char("You must turn off quiet mode first.\r\n", ch);
	else {
		CHECKMUDOF(ch, IMC_RREPLY(ch));
		imc_send_tell(IMCCharData::Get(ch), IMC_RREPLY(ch), color_mtoi(argument), 1);

		ch->Send(RTELL_FORMAT_1, IMC_RREPLY_NAME(ch), argument);
	}
}


ACMD(do_rwho) {
	char *arg;

	CHECKIMC(ch);
	
	arg = get_buffer(MAX_STRING_LENGTH);
	
	argument = one_argument(argument, arg);

	skip_spaces(argument);

	if (!*arg)
		send_to_char("rwho where?\r\n", ch);
	else {
		CHECKMUD(ch, arg);
		imc_send_who(IMCCharData::Get(ch), arg, *argument ? argument : "who");
	}
	release_buffer(arg);
}


ACMD(do_rwhois) {
	CHECKIMC(ch);

	skip_spaces(argument);

	if (!*argument)	send_to_char("rwho whom?\r\n", ch);
	else			imc_send_whois(IMCCharData::Get(ch), argument);
}


ACMD(do_rfinger) {
	char *arg;
	char *name;

	CHECKIMC(ch);
	
	arg = get_buffer(MAX_STRING_LENGTH);

	argument = one_argument(argument, arg);

	skip_spaces(argument);

	if (!*arg || !strchr(arg, '@')) {
		send_to_char("rfinger who@where?\r\n", ch);
	} else {
		name = get_buffer(MAX_STRING_LENGTH);
		CHECKMUD(ch, imc_mudof(arg));
		sprintf(name, "finger %s", imc_nameof(arg));
		imc_send_who(IMCCharData::Get(ch), imc_mudof(arg), name);
		release_buffer(name);
	}
	release_buffer(arg);
}


ACMD(do_rquery) {
	char *arg;

	CHECKIMC(ch);

	arg = get_buffer(MAX_INPUT_LENGTH);

	argument = one_argument(argument, arg);

	skip_spaces(argument);

	if (!*arg)
		send_to_char("rquery where?\r\n", ch);
	else {
		CHECKMUD(ch, arg);
		imc_send_who(IMCCharData::Get(ch), arg, *argument ? argument : "help");
	}
	release_buffer(arg);
}



ACMD(do_rbeep) {
	CHECKIMC(ch);

	skip_spaces(argument);

	if (!*argument || !strchr(argument, '@'))
		send_to_char("rbeep who@where?\r\n", ch);
	else if (!CAN(ch, IMC_RBEEP, IMC_LEVEL_RBEEP))
		send_to_char("You are not authorised to rbeep.\r\n", ch);
	else if (IS_NOTELL(ch))
		send_to_char("You cannot beep!\r\n", ch);
	else if (IS_RINVIS(ch))
		send_to_char ("You cannot rbeep while invisible to IMC.\r\n", ch);
	else if (IS_QUIET(ch))
		send_to_char("You must turn off quiet mode first.\r\n", ch);
	else {
		CHECKMUDOF(ch, argument);
		imc_send_beep(IMCCharData::Get(ch), argument);
		ch->Send(RBEEP_FORMAT_1, argument);

		WAIT_STATE(ch, 5 RL_SEC);
	}
}


ACMD(do_imclist) {
	skip_spaces(argument);

	if (!*argument)									page_to_char(imc_list(3), ch);
	else if (!str_prefix(argument, "all"))			page_to_char(imc_list(6), ch);
	else if (!str_prefix(argument, "direct")) {
		if (GET_LEVEL(ch) >= LVL_CODER - 1)			page_to_char(imc_list(2), ch);
		else if (IS_STAFF(ch))						page_to_char(imc_list(1), ch);
		else										page_to_char(imc_list(0), ch);
	} else if (!str_prefix(argument, "config")) {
		if (IS_STAFF(ch))							page_to_char(imc_list(5), ch);
		else										page_to_char(imc_list(4), ch);
	} else											page_to_char("Unknown option.", ch);
//	page_to_char("\r\n", ch);
}


ACMD(do_rsockets) {
	CHECKIMC(ch);

	page_to_char(imc_sockets(), ch);
}


ACMD(do_imc) {
	int r;

	skip_spaces(argument);

	r = imc_command(argument);

	if (r > 0)
		send_to_char("Ok.\r\n", ch);
	else if (r==0) {
		send_to_char("Syntax:  imc add <mudname>\r\n"
					 "         imc delete <mudname>\r\n"
					 "         imc set <mudname> all <host> <port> <clientpw> <serverpw>\r\n"
					 "                 <rcvstamp> <noforward> <flags>\r\n"
					 "         imc set <mudname> host|port|clientpw|serverpw|rcvstamp|\r\n"
					 "                           noforward|flags <value>\r\n"
					 "         imc rename <oldmudname> <newmudname>\r\n"
					 "         imc localname <mudname>\r\n"
					 "         imc localport <portnum>\r\n"
					 "         imc reload\r\n",
				ch);
	} else {
		ch->Send("%s\r\n", imc_error());
	}
}


ACMD(do_rignore) {
	skip_spaces(argument);
	ch->Send("%s\r\n", imc_ignore(argument));
}


ACMD(do_rconnect) {
	CHECKIMC(ch);

	skip_spaces(argument);

	if (!argument[0])
		send_to_char("rconnect to where?\r\n", ch);
	else if (imc_connect_to(argument))
		send_to_char("Ok.\r\n", ch);
	else {
		ch->Send("%s\r\n", imc_error());
	}
}


ACMD(do_rdisconnect) {
	CHECKIMC(ch);

	skip_spaces(argument);

	if (!argument[0])
		send_to_char("rdisconnect where?\r\n", ch);
	else if (imc_disconnect(argument))
		send_to_char("Ok.\r\n", ch);
	else {
		ch->Send("%s\r\n", imc_error());
	}
}


ACMD(do_mailqueue) {
	CHECKIMC(ch);

	page_to_char(imc_mail_showqueue(), ch);
}


ACMD(do_istats) {
	CHECKIMC(ch);

	send_to_char(imc_getstats(), ch);
}

ACMD(do_rchannels) {
	char *arg;
	bool toggle;
	int i;
	const char *name;
	long flag;
  
	if (IS_NPC(ch)) {
		send_to_char("NPCs cannot use IMC.\r\n", ch);
		return;
	}

	arg = get_buffer(MAX_INPUT_LENGTH);
	
	one_argument(argument, arg);

	skip_spaces(argument);
  
	if (!*arg) {	//	list channel status
		bool found = false;
		char *buf = get_buffer(MAX_STRING_LENGTH);
    
		strcpy(buf, "channel      status\r\n"
					"-------------------\r\n");

		for (i = 0; i < numchannels; i++) {
			if (IS_SET(IMC_DENY_FLAGS(ch), imc_channels[i].flag)) {
				sprintf(buf + strlen(buf), "%-10s : you are denied from this channel.\r\n",
					imc_channels[i].name);
				found = true;
			} else if (CAN(ch, imc_channels[i].flag, imc_channels[i].minlevel)) {
				sprintf(buf + strlen(buf), "%-10s : %s\r\n",
					imc_channels[i].name,
					(IS_SET(IMC_DEAF_FLAGS(ch), imc_channels[i].flag) ? "OFF" : "ON"));
				found = true;
			}
		}

		if (IS_SET(IMC_DENY_FLAGS(ch), IMC_RTELL)) {
			strcat(buf, "RTells     : you cannot rtell.\r\n");
			found = true;
		} else if (CAN(ch, IMC_RTELL, IMC_LEVEL_RTELL)) {
			sprintf(buf + strlen(buf), "%-10s : %s\r\n", "RTell",
					IS_SET(IMC_DEAF_FLAGS(ch), IMC_RTELL) ? "OFF" : "ON");
			found = true;
		}

		if (IS_SET(IMC_DENY_FLAGS(ch), IMC_RBEEP)) {
			strcat(buf, "RBeeps     : you cannot rbeep.\r\n");
			found = true;
		} else if (CAN(ch, IMC_RBEEP, IMC_LEVEL_RBEEP)) {
			sprintf(buf + strlen(buf), "%-10s : %s\r\n", "RBeep",
					IS_SET(IMC_DEAF_FLAGS(ch), IMC_RBEEP) ? "OFF" : "ON");
			found = true;
		}

		if (IS_SET(IMC_DEAF_FLAGS(ch), IMC_RINVIS))
			sprintf(buf + strlen(buf), "You are not visible to others over IMC.\r\n");

		if (!found)		send_to_char("You have access to no IMC channels.\r\n", ch);
		else			send_to_char(buf, ch);

		release_buffer(buf);
		release_buffer(arg);
		return;
	}

	/* turn some things on or off */

	while((argument = one_argument(argument, arg)), *arg) {
		if (*arg == '-')		toggle = false;
		else if (*arg == '+')	toggle = true;
		else {
			send_to_char("Syntax: rchannels          displays current IMC channels\r\n"
						 "        rchannels +chan    turn on a channel\r\n"
						 "        rchannels -chan    turn off a channel\r\n"
						 "chan may be an IMC channel name, 'rbeep', 'rtell', 'rinvis', or 'all'.\r\n"
						 "Multiple settings may be given in one command.\r\n", ch);
			release_buffer(arg);
			return;
		}

		if (!str_cmp(arg + 1, "all")) {
			if (toggle) {
				send_to_char("ALL available IMC channels are now on.\r\n", ch);
				IMC_DEAF_FLAGS(ch) = 0;
			} else {
				send_to_char("ALL available IMC channels are now off.\r\n", ch);
				IMC_DEAF_FLAGS(ch) = 0xFFFFFFFF;
			}
			continue;
		} else if (!str_cmp(arg + 1, "rtell") && CAN(ch, IMC_RTELL, IMC_LEVEL_RTELL)) {
			name = "RTell";
			flag = IMC_RTELL;
		} else if (!str_cmp(arg + 1, "rbeep") && CAN(ch, IMC_RBEEP, IMC_LEVEL_RBEEP)) {
			name = "RBeep";
			flag = IMC_RBEEP;
		} else if (!str_cmp(arg + 1, "rinvis")) {
			name = "RInvis";
			flag = IMC_RINVIS;
			toggle =! toggle;
		} else {
			for (i = 0; i<numchannels; i++) {
				if (CAN(ch, imc_channels[i].flag, imc_channels[i].minlevel) &&
						!str_cmp(imc_channels[i].name, arg + 1))
					break;
			}

			if (i==numchannels) {
				ch->Send("You don't have access to an IMC channel called \"%s\".\r\n", arg);
				continue;
			}

			name=imc_channels[i].name;
			flag=imc_channels[i].flag;
		}

		if (toggle && !IS_SET(IMC_DEAF_FLAGS(ch), flag)) {
			if (flag == IMC_RINVIS)	send_to_char("You are already visible on IMC.\r\n", ch);
			else					ch->Send("%s is already on.\r\n", name);
		} else if (!toggle && IS_SET(IMC_DEAF_FLAGS(ch), flag)) {
			if (flag == IMC_RINVIS)	send_to_char("You are already invisible on IMC.\r\n", ch);
			else					ch->Send("%s is already off.\r\n", name);
		} else if (toggle) {
			if (flag == IMC_RINVIS)	send_to_char("You are no longer invisible to IMC.\r\n", ch);
			else					ch->Send("%s is now ON.\r\n", name);
			REMOVE_BIT(IMC_DEAF_FLAGS(ch), flag);
		} else {
			if (flag == IMC_RINVIS)	send_to_char("You are now invisible to rwho, rtell and rbeep on IMC.\r\n", ch);
			else					ch->Send("%s is now OFF.\r\n", name);
			SET_BIT(IMC_DEAF_FLAGS(ch), flag);
		}
	}
	release_buffer(arg);
}


ACMD(do_rchanset) {
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	CharData *victim;
	int flag, i;
	const char *name;
	int fn;
	bool found = false;

	argument = one_argument(argument, arg);

	skip_spaces(argument);

	if (!*arg)
	    send_to_char("Syntax: rchanset <char>            - check flag settings\r\n"
					 "        rchanset <char> +<channel> - set allow flag\r\n"
					 "        rchanset <char> -<channel> - set deny flag\r\n"
					 "        rchanset <char> =<channel> - reset allow/deny\r\n",
				 ch);
	else if (!(victim = get_char_vis(ch, arg)))
		send_to_char("They're not here.\r\n", ch);
	else if (IS_NPC(victim))
		send_to_char("Can't set rchannel status on NPCs.\r\n", ch);
	else if ((IS_STAFF(victim) && !STF_FLAGGED(ch, STAFF_ADMIN)) || STF_FLAGGED(victim, STAFF_ADMIN))
		send_to_char("You failed.\r\n", ch);
	else if (!*argument) {
		ch->Send("%s's IMC channel flags:\r\n"
				 "------------------------------\r\n",
				victim->RealName());

		for (i = 0; i < numchannels; i++) {
			if (IS_SET(IMC_ALLOW_FLAGS(victim), imc_channels[i].flag)) {
				ch->Send("%-10s : allow flag set.\r\n", imc_channels[i].name);
				found = true;
			}

			if (IS_SET(IMC_DENY_FLAGS(victim), imc_channels[i].flag)) {
				ch->Send("%-10s : deny flag set.\r\n", imc_channels[i].name);
				found = true;
			}
		}

		if (IS_SET(IMC_ALLOW_FLAGS(victim), IMC_RTELL)) {
			ch->Send("%-10s : allow flag set.\r\n", "RTell");
			found = true;
		}

		if (IS_SET(IMC_DENY_FLAGS(victim), IMC_RTELL)) {
			ch->Send("%-10s : deny flag set.\r\n", "RTell");
			found = true;
		}

		if (IS_SET(IMC_ALLOW_FLAGS(victim), IMC_RBEEP)) {
			ch->Send("%-10s : allow flag set.\r\n", "RBeep");
			found = true;
		}

		if (IS_SET(IMC_DENY_FLAGS(victim), IMC_RBEEP)) {
			ch->Send("%-10s : deny flag set.\r\n", "RBeep");
			found = true;
		}

		if (!found)
			ch->Send("%s has no IMC flags set.\r\n", victim->RealName());
	} else {
		if (*argument == '-')		fn = 0;
		else if (*argument == '+')	fn = 1;
		else if (*argument == '=')	fn = 2;
		else {
			send_to_char("Channel name must be preceeded by +, -, or =.\r\n", ch);
			release_buffer(arg);
			return;
		}

		argument++;

		if (!str_cmp(argument, "rtell")) {
			flag = IMC_RTELL;
			name = "RTell";
		} else if (!str_cmp(argument, "rbeep")) {
			flag = IMC_RBEEP;
			name = "RBeep";
		} else {
			for (i = 0; i < numchannels; i++)
				if (!str_cmp(imc_channels[i].name, argument))
					break;

			if (i == numchannels) {
				send_to_char("No such channel.\r\n", ch);
				return;
			}

			flag = imc_channels[i].flag;
			name = imc_channels[i].name;
		}

		switch(fn) {
			case 0: /* set deny flag */
				if (IS_SET(IMC_DENY_FLAGS(victim), flag))
					send_to_char("Deny flag already set.\r\n", ch);
				else {
					SET_BIT(IMC_DENY_FLAGS(victim), flag);
					REMOVE_BIT(IMC_ALLOW_FLAGS(victim), flag);
					victim->Send("The staff have revoked your %s priviliges.\r\n", name);
					send_to_char("Ok.\r\n", ch);
				}
				break;

			case 1: /* set allow flag */
				if (IS_SET(IMC_ALLOW_FLAGS(victim), flag))
					send_to_char("Allow flag already set.\r\n", ch);
				else {
					SET_BIT(IMC_ALLOW_FLAGS(victim), flag);
					REMOVE_BIT(IMC_DENY_FLAGS(victim), flag);
					victim->Send("The staff have allowed you access to %s.\r\n", name);
					send_to_char("Ok.\r\n", ch);
				}
				break;

			case 2: /* clears flags */
				if (IS_SET(IMC_ALLOW_FLAGS(victim), flag)) {
					REMOVE_BIT(IMC_ALLOW_FLAGS(victim), flag);
					victim->Send("The staff have removed your special access to %s.\r\n", name);
					found = true;
				}
				
				if (IS_SET(IMC_DENY_FLAGS(victim), flag)) {
					REMOVE_BIT(IMC_DENY_FLAGS(victim), flag);
					victim->Send("The gods have restored your %s priviliges.\r\n", name);
					found = true;
				}
	    
				if (!found)	send_to_char("They have no flags to clear.\r\n", ch);
				else		send_to_char("Ok.\r\n", ch);
				break;
		}
	}
	release_buffer(arg);
}


/* renamed from do_rchannel */
static void do_imcchannel(const IMCCharData *from, int number, const char *argument, int emote) {
	DescriptorData *d;
	CharData *victim;
	const char *str;
	const char *arg;
	int chan;

	for (chan = 0; chan < numchannels; chan++)
		if (imc_channels[chan].number == number)
			break;

	if (chan == numchannels)
		return;
	
	str = emote ? imc_channels[chan].emotestr : imc_channels[chan].chatstr;
	arg = color_itom(argument);

	START_ITER(iter, d, DescriptorData *, descriptor_list) {
		if ((STATE(d) == CON_PLAYING) && (victim = d->Original()) &&
				!PLR_FLAGGED(victim, PLR_WRITING || PLR_MAILING) &&
				!IS_QUIET(victim) && !IS_SILENT(victim) &&
				!IS_SET(IMC_DEAF_FLAGS(victim), imc_channels[chan].flag) &&
				CAN(victim, imc_channels[chan].flag, imc_channels[chan].minlevel)) {
			victim->Send(str, getname(victim, from), arg);
		}
	} END_ITER(iter);
}


/*  Traceroute and ping.
 *
 *  Be lazy - only remember the last traceroute
 */
static char lastping[IMC_MNAME_LENGTH];
static char pinger[100];

ACMD(do_rping) {
	char *arg;
	struct timeval tv;

	CHECKIMC(ch);

	arg = get_buffer(MAX_INPUT_LENGTH);
	
	skip_spaces(argument);

	one_argument(argument, arg);

	if (!*arg)
		send_to_char("Ping which mud?\r\n", ch);
	else {
		CHECKMUD(ch, arg);

		gettimeofday(&tv, NULL);
		strncpy(lastping, arg, IMC_MNAME_LENGTH-1);
		lastping[IMC_MNAME_LENGTH-1] = '\0';
		strncpy(pinger, ch->RealName(), 99);
		pinger[99] = '\0';
		imc_send_ping(arg, tv.tv_sec, tv.tv_usec);
	}

	release_buffer(arg);
}


void imc_traceroute(int ping, const char *pathto, const char *pathfrom) {
	DescriptorData *d;
	CharData *ch = NULL;
	
	if (!str_cmp(imc_firstinpath(pathfrom), lastping)) {

		START_ITER(iter, d, DescriptorData *, descriptor_list) {
			if ((STATE(d) == CON_PLAYING) && (ch = d->Original()) &&
					!str_cmp(ch->RealName(), pinger))
				break;
		} END_ITER(iter);
    
		if (!ch)
			return;

		ch->Send("%s: %dms round-trip-time.\r\n"
				 "Return path: %s\r\n"
				 "Send path:   %s\r\n",
				imc_firstinpath(pathfrom), ping,
				pathfrom,
				pathto ? pathto : "unknown");
	}
}

void imc_recv_chat(const IMCCharData *from, int channel, const char *argument) {
	do_imcchannel(from, channel, argument, 0);
}

void imc_recv_emote(const IMCCharData *from, int channel, const char *argument) {
	do_imcchannel(from, channel, argument, 1);
}

/* new imc_whoreply_* support, see imc-interp.c */

/* expanded for minimal mud-specific code. I really don't want to replicate
 * stock in-game who displays here, since it's one of the most commonly
 * changed pieces of code. shrug.
 */
static void process_rwho(const IMCCharData *from, const char *argument) {
	CharData *victim;
	SInt32		staff = 0;
	SInt32		players = 0;
	char *		staff_buf = get_buffer(MAX_STRING_LENGTH);
	char *		player_buf = get_buffer(MAX_STRING_LENGTH);
	char *		buf = get_buffer(MAX_STRING_LENGTH);
	DescriptorData *d;
	RNum		clan;
	extern char *level_string[];
	extern UInt32 boot_high;
	extern char *race_abbrevs[];
	extern char *relation_colors;

	imc_whoreply_start(from->name);

	strcpy(staff_buf,	"Staff currently online\r\n"
						"----------------------\r\n");
	strcpy(player_buf,	"Players currently online\r\n"
						"------------------------\r\n");

	START_ITER(iter, d, DescriptorData *, descriptor_list) {
		victim = d->Original();

		if ((STATE(d) != CON_PLAYING) || !from->Visible(victim) || IS_NPC(victim))
			continue;
		
		if (IS_STAFF(victim)) {
			sprintf(buf, "`y %s %s %s`n", level_string[GET_LEVEL(victim)-LVL_IMMORT],
					victim->GetName(), GET_TITLE(victim));
			staff++;
		} else {
			sprintf(buf, "`n[%3d %s] %s %s`n", GET_LEVEL(victim), RACE_ABBR(victim),
					GET_TITLE(victim), victim->GetName());
			players++;
		}
		
//		if (GET_INVIS_LEV(victim))						sprintf(buf, "%s (`wi%d`n)", GET_INVIS_LEV(victim));
//		else if (AFF_FLAGGED(victim, AFF_INVISIBLE))	strcat(buf, " (`winvis`n)");
		
		if (PLR_FLAGGED(victim, PLR_MAILING))		strcat(buf, " (`bmailing`n)");
		else if (PLR_FLAGGED(victim, PLR_WRITING))	strcat(buf, " (`rwriting`n)");

		if (CHN_FLAGGED(victim, Channel::NoShout))	strcat(buf, " (`gdeaf`n)");
		if (CHN_FLAGGED(victim, Channel::NoTell))	strcat(buf, " (`gnotell`n)");
		if (PLR_FLAGGED(victim, PLR_TRAITOR))		strcat(buf, " (`RTRAITOR`n)");
		if (((clan = real_clan(GET_CLAN(victim))) != -1) && (GET_CLAN_LEVEL(victim) > CLAN_APPLY)) {
				sprintf(buf + strlen(buf), " <`C%s`n>", clan_index[clan].name);
		}
		if (GET_AFK(victim))						strcat(buf, " `c[AFK]");
		strcat(buf, "`n\r\n");

		if (IS_STAFF(victim))				strcat(staff_buf, buf);
		else								strcat(player_buf, buf);
	} END_ITER(iter);
	
	strcat(staff_buf, "\r\n");
	strcat(player_buf, "\r\n");

	if (staff)		imc_whoreply_add(color_mtoi(staff_buf));
	if (players)	imc_whoreply_add(color_mtoi(player_buf));
	
	*buf = '\0';
	
	if (!(staff + players)) {
		strcpy(buf, "No staff or players are currently visible to you.\r\n");
	} else {
		if (staff)		sprintf(buf + strlen(buf), "There %s %d visible staff%s",
								(staff == 1 ? "is" : "are"), staff, players ? " and there" : ".");
		if (players)	sprintf(buf + strlen(buf), "%s %s %d visible player%s.",
								staff ? "" : "There", (players == 1 ? "is" : "are"),
								players, (players == 1 ? "" : "s"));
	}
	strcat(buf, "\r\n");

	if ((staff + players) > boot_high)
		boot_high = staff + players;
	sprintf(buf + strlen(buf), "There is a boot time high of %d player%s.\r\n", boot_high, (boot_high == 1 ? "" : "s"));
	
	imc_whoreply_add(color_mtoi(buf));
	imc_whoreply_end();
	
	release_buffer(staff_buf);
	release_buffer(player_buf);
	release_buffer(buf);
}

/* edit this if you want to support rfinger */
static void process_rfinger(const IMCCharData *from, const char *argument)
{
  imc_send_whoreply(from->name,
		    "Sorry, no information is available of that type.\r\n", -1);
}

void imc_recv_who(const IMCCharData *from, const char *type) {
	char arg[MAX_STRING_LENGTH];
	char output[MAX_STRING_LENGTH];

	type=imc_getarg(type, arg, MAX_STRING_LENGTH);

	if (!str_cmp(arg, "who")) {
		process_rwho(from, type);
		return;
	} else if (!str_cmp(arg, "finger")) {
		process_rfinger(from, type);
		return;
	} else if (!str_cmp(arg, "info"))	strcpy(output, IMC_MUD_INFO);
	else if (!str_cmp(arg, "list"))		strcpy(output, imc_list(3));
	else if (!str_cmp(arg, "config"))	strcpy(output, imc_list(4));
	else if (!str_cmp(arg, "direct"))	strcpy(output, imc_list(0));
	else if (!str_cmp(arg, "options") || !str_cmp(arg, "services") || !str_cmp(arg, "help"))
			strcpy(output,
					"Available rquery types:\r\n"
					"help       - this list\r\n"
					"who        - who listing\r\n"
					"info       - mud information\r\n"
					"list       - active IMC connections\r\n"
					"direct     - direct IMC connections\r\n"
					"config     - local IMC configuration\r\n"
//					"finger xxx - finger player xxx\r\n"
			);
	else
		strcpy(output, "Sorry, no information is available of that type.\r\n");

	imc_send_whoreply(from->name, color_mtoi(output), -1);
}

void imc_recv_whoreply(const char *to, const char *text, int sequence) {
	DescriptorData *d;
	CharData *victim = NULL;

	START_ITER(iter, d, DescriptorData *, descriptor_list) {
		if ((STATE(d) == CON_PLAYING) && (victim = d->Original()) &&
				is_abbrev(to, victim->RealName())) {
			page_to_char(color_itom(text), victim);
			break;
		}
	} END_ITER(iter);
}


void imc_recv_whois(const IMCCharData *from, const char *to) {
	DescriptorData *d;
	CharData *victim = NULL;
	char *buf = get_buffer(MAX_STRING_LENGTH);

	START_ITER(iter, d, DescriptorData *, descriptor_list) {
		if ((STATE(d) == CON_PLAYING) && (victim = d->Original()) &&
				!IS_NPC(victim) && from->Visible(victim) &&
				(!str_cmp(to, victim->RealName()) ||
				(strlen(to)>3 && !str_prefix(to, victim->RealName())))) {
			sprintf(buf, "rwhois %s : %s@%s is online.\r\n", to,
					victim->RealName(), imc_name);
			imc_send_whoisreply(from->name, buf);
		}
	} END_ITER(iter);
	release_buffer(buf);
}

void imc_recv_whoisreply(const char *to, const char *text) {
	DescriptorData *d;
	CharData *victim = NULL;

	START_ITER(iter, d, DescriptorData *, descriptor_list) {
		if ((STATE(d) == CON_PLAYING) && (victim = d->Original()) &&
				!str_cmp(to, victim->RealName())) {
			send_to_char(color_itom(text), victim);
			return;
		}
	} END_ITER(iter);
}

void imc_recv_tell(const IMCCharData *from, const char *to, const char *argument, int isreply) {
	DescriptorData *d;
	CharData *victim = NULL, *vch = NULL;
	char *buf;

	if (!strcmp(to, "*")) /* ignore messages to system */
		return;

	buf = get_buffer(IMC_DATA_LENGTH);

	LListIterator<DescriptorData *>	iter(descriptor_list);
	while ((d = iter.Next())) {
		if ((STATE(d) == CON_PLAYING) && (vch = d->Original()) &&
				!IS_NPC(vch) && !IS_RINVIS(vch) &&
				(isreply || from->Visible(vch))) {
			if (!str_cmp(to, vch->RealName())) {
				victim=vch;
				break;
			}
			if (is_abbrev(to, vch->RealName()))
				victim=vch;
		}
	}

	if (victim) {
		if (IS_SET(IMC_DEAF_FLAGS(victim), IMC_RTELL) || IS_QUIET(victim) ||
				IS_SILENT(victim) || IS_SET(IMC_DENY_FLAGS(victim), IMC_RTELL)) {
			if (str_cmp(imc_nameof(from->name), "*")) {
				sprintf(buf, "%s is not receiving tells.", to);
				imc_send_tell(NULL, from->name, buf, 1);
			}
		} else {
			if (str_cmp(imc_nameof(from->name), "*")) {	/* not a system message */
				if (IMC_RREPLY(victim))			free(IMC_RREPLY(victim));
				if (IMC_RREPLY_NAME(victim))	free(IMC_RREPLY_NAME(victim));

				IMC_RREPLY(victim) = str_dup(from->name);
				IMC_RREPLY_NAME(victim) = str_dup(getname(victim, from));
			}	
    
#if NO_VSNPRINTF
			sprintf(buf, RTELL_FORMAT_2, getname(victim, from), color_itom(argument));
#else
			snprintf(buf, IMC_DATA_LENGTH, RTELL_FORMAT_2, getname(victim, from), color_itom(argument));
#endif

			page_to_char(buf, victim);
		}
	} else {
		sprintf(buf, "%s is not here.", to);
		imc_send_tell(NULL, from->name, buf, 1);
	}
	release_buffer(buf);
}

void imc_recv_beep(const IMCCharData *from, const char *to) {
	DescriptorData *d;
	CharData *victim = NULL, *vch = NULL;
	char *buf;

	if (!strcmp(to, "*")) /* ignore messages to system */
		return;

	buf = get_buffer(IMC_DATA_LENGTH);
	
	LListIterator<DescriptorData *>		iter(descriptor_list);
	while ((d = iter.Next())) {
		if ((STATE(d) == CON_PLAYING) && (vch = d->Original()) && from->Visible(vch)) {
			if (!str_cmp(to, vch->RealName())) {
				victim = vch;
				break;
			}
			if (is_abbrev(to, vch->RealName()))
			victim = vch;
		}
	}

	if (victim) {
		if (IS_SET(IMC_DEAF_FLAGS(victim), IMC_RBEEP) || IS_SILENT(victim) ||
				!CAN(victim, IMC_RTELL, IMC_LEVEL_RTELL)) {
			sprintf(buf, "%s is not receiving beeps.", to);
			imc_send_tell(NULL, from->name, buf, 1);
		} else
			victim->Send(RBEEP_FORMAT_2, from->name);	//	always display the true name here
	} else {
		sprintf(buf, "%s is not here", to);
		imc_send_tell(NULL, from->name, buf, 1);
	}
	release_buffer(buf);
}



/**  IMC MAIL  **/

/* shell for sending IMC mail */

void imc_post_mail(CharData *from, const char *sender, const char *to_list,
		const char *subject, const char *date, const char *text) {
	char *arg;
	const char *temp;

	CHECKIMC(from);

	if (!strchr(to_list, '@'))
		return;
	
	arg = get_buffer(MAX_STRING_LENGTH);
	
	for (temp = one_argument(to_list, arg); *arg; temp = one_argument(temp, arg))
		if (strchr(arg, '@'))
			CHECKMUD(from, imc_mudof(arg));

	imc_send_mail(sender, to_list, subject, date, color_mtoi(text));
	
	release_buffer(arg);
}


//	Since there are half-a-dozen different ways that people do this, I'm
//	sticking to the stock code, plus Erwin's board system.
char *imc_mail_arrived(const char *from, const char *to, const char *date,
		const char *subject, const char *text) {
	char *buf;

	//	don't let people write to 'all' over IMC
	if (isname("all", to))
		return "Notes to 'all' are not accepted.";
	
	if (!get_id_by_name(to))
		return "Recipient does not exist.";
	
	buf = get_buffer(MAX_STRING_LENGTH);
	
	sprintf(buf, "-= IMC Header =-\r\n"
				 "   From: %s\r\n"
				 "     To: %s\r\n"
				 "   Date: %s\r\n"
				 "Subject: %s\r\n"
				 "\r\n"
				 "%s",
			from,
			to,
			date,
			subject,
			text);

	store_mail(get_id_by_name(to), 0, buf);

	release_buffer(buf);

	return NULL;	//	Errors? What errors? <G>
}


void imc_log(const char *string) {
	log("IMC: %s", string);
}
