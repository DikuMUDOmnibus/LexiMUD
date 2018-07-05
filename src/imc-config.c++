/*
 * IMC2 - an inter-mud communications protocol
 *
 * imc-config.c: configuration manipulation
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
#else
#include <machine/endian.h>
char * inet_ntoa(struct in_addr inaddr);
#endif
#include <netinet/in.h>

/*
 * I needed to split up imc.c (2600 lines and counting..) so this file now
 * contains:
 *
 * - config loading/saving (imc_readconfig, imc_saveconfig)
 * - ignores loading/saving (imc_readignores, imc_saveignores)
 * - config editing/displaying (imc_command)
 * - ignore editing/displaying (imc_ignore)
 */

/*  USEIOCTL #defined if TIOCINQ or TIOCOUTQ are - we assume that the ioctls
 *  work in that case.
 */

#if defined(TIOCINQ) && defined(TIOCOUTQ)
#define USEIOCTL
static int outqsize;
#endif

#define GETSTRING(key,var,len)											\
	else if (!str_cmp(word,key)) {										\
		if (var[0]) imc_logstring("Warning: duplicate '" key "' lines");\
		imc_sncpy(var,value,len);										\
	}

IMCSiteInfo imc_siteinfo;

/* read config file */
int imc_readconfig(void) {
	FILE *cf;
	IMCInfo *i;
	char name[IMC_NAME_LENGTH], host[200];
	char pw1[IMC_PW_LENGTH], pw2[IMC_PW_LENGTH];
	UInt16	port;
	SInt32	count;
	char buf[1000];
	char configfile[200];
	int noforward, rcvstamp, flags;
	char word[1000];
	short localport;
	char localname[IMC_NAME_LENGTH];
	const char *value;
	SInt32 version = -1;
	UInt32 localip;

	char iname[200], ihost[200], iemail[200], iimail[200], iwww[200],
			idetails[1000], iflags[200];
  
	localport = -1;
	localname[0]=iname[0]=ihost[0]=iemail[0]=iimail[0]=iwww[0]=idetails[0]=iflags[0]=0;
	localip = INADDR_ANY;
  
	imc_sncpy(configfile, imc_prefix, 193);
	strcat(configfile, "config");

//	imc_info_list = NULL;

	cf = fopen(configfile, "r");
	if (!cf) {
		imc_logerror("imc_readconfig: couldn't open %s", configfile);
		return 0;
	}

	while(1) {
		if (!fgets(buf, 1000, cf))
			break;
    
		while (buf[0] && isspace(buf[strlen(buf) -1 ]))
			buf[strlen(buf) - 1] = 0;

		if ((buf[0] == '#') || (buf[0] == '\n') || !buf[0])
			continue;

		value = imc_getarg(buf, word, 1000);
		if (!str_cmp(word, "Version")) {
			imc_getarg(value, word, 1000);
			version = atoi(word);
		} else if (!str_cmp(word, "Connection")) {
			imc_getarg(value, word, 1000);
			if (sscanf(value, "%[^:]:%[^:]:%hu:%[^:]:%[^:]:%d:%d:%n",
					name, host, &port, pw1, pw2, &rcvstamp, &noforward, &count) < 7) {
				imc_logerror("Bad 'Connection' line: %s", buf);
				continue;
			}

			flags = imc_flagvalue(word + count, imc_connection_flags);

			i = new IMCInfo();
			i->name			= str_dup(name);
			i->host			= str_dup(host);
			i->clientpw		= str_dup(pw1);
			i->serverpw		= str_dup(pw2);
			i->port			= port;
			i->rcvstamp		= rcvstamp;
			i->noforward	= noforward;
			i->flags		= flags;
		} else if (!str_cmp(word, "LocalPort")) {
			if (localport != -1)
				imc_logstring("Warning: duplicate 'LocalPort' lines");
			localport = atoi(value);
		} else if (!str_cmp(word, "Bind")) {
			if (localip != INADDR_ANY)
				imc_logstring("Warning: duplicate 'Bind' lines");
#if !defined(macintosh)
			localip = inet_addr((char *)value);
#endif
		}

		GETSTRING("LocalName", localname, 1000)
		GETSTRING("InfoName", iname, 200)
		GETSTRING("InfoHost", ihost, 200)
		GETSTRING("InfoEmail", iemail, 200)
		GETSTRING("InfoImail", iimail, 200)
		GETSTRING("InfoWWW", iwww, 200)
		GETSTRING("InfoFlags", iflags, 200)
		GETSTRING("InfoDetails", idetails, 1000)
      
		else if (version==-1) {
			if (sscanf(buf, "%[^:]:%[^:]:%hu:%[^:]:%[^:]:%d:%d:%n",
					name, host, &port, pw1, pw2, &rcvstamp, &noforward, &count) < 7) {
				imc_logerror("Bad config line: %s", buf);
				continue;
			}

			flags = imc_flagvalue(buf + count, imc_connection_flags);
	    
			i = new IMCInfo();
			i->name			= str_dup(name);
			i->host			= str_dup(host);
			i->clientpw		= str_dup(pw1);
			i->serverpw		= str_dup(pw2);
			i->port			= port;
			i->rcvstamp		= rcvstamp;
			i->noforward	= noforward;
			i->flags		= flags;
		} else
			imc_logerror("Bad config line: %s", buf);
	}

	if (ferror(cf)) {
		imc_lerror("imc_readconfig");
		fclose(cf);
		return 0;
	}

	fclose(cf);

	imc_siteinfo.name=str_dup(iname);
	imc_siteinfo.host=str_dup(ihost);
	imc_siteinfo.email=str_dup(iemail);
	imc_siteinfo.imail=str_dup(iimail);
	imc_siteinfo.www=str_dup(iwww);
	imc_siteinfo.details=str_dup(idetails);
	imc_siteinfo.flags=str_dup(iflags);
  
	if (!localname[0]) {
		imc_logstring("warning: missing 'LocalName' line in config");
		imc_name=NULL;
	} else
		imc_name=str_dup(localname);

	if (localport == -1) {
		imc_logerror("warning: missing 'LocalPort' line in config");
		imc_port = 0;
	} else
		imc_port = localport;

	imc_bind = localip;
  
	if (!iname[0] || !iemail[0])
		imc_logerror("InfoName and InfoEmail MUST be set to start IMC. See the IMC README");

	return 1;
}


/* save the IMC config file */
int imc_saveconfig(void) {
	FILE *out;
	IMCInfo *i;
	char *configfile = get_buffer(200);

	if (imc_active == IA_NONE)
		return 0;

	imc_sncpy(configfile, imc_prefix, 193);
	strcat(configfile, "config");

	out = fopen(configfile, "w");
	if (!out) {
		imc_lerror("imc_saveconfig: error opening %s", configfile);
		release_buffer(configfile);
		return 0;
	}
	release_buffer(configfile);
  
	fprintf(out,"# Version <config_file_version>\n"
				"# LocalName <local_imc_name>\n"
				"# LocalPort <local_imc_port>\n"
				"# Connection Name:Host:Port:ClientPW:ServerPW:RcvStamp:NoForward:"
				"Flags\n");

	fprintf(out, "Version 1\n");

	if (imc_active >= IA_CONFIG2) {
		fprintf(out, "LocalName %s\n", imc_name);
		fprintf(out, "LocalPort %hu\n", imc_port);
		if (imc_bind!=htonl(INADDR_ANY)) {
			struct in_addr a;
			a.s_addr = imc_bind;
			fprintf(out, "Bind %s\n", inet_ntoa(a));
		}
	}

	if (imc_siteinfo.name[0])		fprintf(out, "InfoName %s\n", imc_siteinfo.name);
	if (imc_siteinfo.host[0])		fprintf(out, "InfoHost %s\n", imc_siteinfo.host);
	if (imc_siteinfo.email[0])		fprintf(out, "InfoEmail %s\n", imc_siteinfo.email);
	if (imc_siteinfo.imail[0])		fprintf(out, "InfoImail %s\n", imc_siteinfo.imail);
	if (imc_siteinfo.www[0])		fprintf(out, "InfoWWW %s\n", imc_siteinfo.www);
	if (imc_siteinfo.details[0])	fprintf(out, "InfoDetails %s\n", imc_siteinfo.details);
	if (imc_siteinfo.flags[0])		fprintf(out, "InfoFlags %s\n", imc_siteinfo.flags);

	LListIterator<IMCInfo *>	iter(imc_info_list);
	while ((i = iter.Next()))
		fprintf(out, "Connection %s:%s:%hu:%s:%s:%d:%d:%s\n",
				i->name, i->host, i->port, i->clientpw, i->serverpw, i->rcvstamp, i->noforward,
				imc_flagname(i->flags, imc_connection_flags));

	if (ferror(out)) {
		imc_lerror("imc_saveconfig: error saving %s", configfile);
		fclose(out);
		return 0;
	}

	fclose(out);
	return 1;
}

/*  runtime changing of IMC config
 *  returns  >0 success
 *           <0 error
 *          ==0 unknown command
 *
 *  commands:
 *    reload
 *    add <mudname>
 *    delete <mudname>
 *    rename <oldname> <newname>
 *    set <mudname> <host|port|clientpw|serverpw|flags|noforward|
 *                   rcvstamp> <newvalue>
 *    set <mudname> all <host> <port> <clientpw> <serverpw> <noforward>
 *                      <rcvstamp> <flags>
 *    localname <name>
 *    localport <port>
 */

int imc_command(const char *argument) {
	char *arg1;
	char *arg2;
	IMCInfo *i;


	if (imc_active==IA_NONE) {
		imc_qerror("IMC is not initialized");
		return -1;
	}
	
	arg1 = get_buffer(IMC_DATA_LENGTH);
	arg2 = get_buffer(IMC_DATA_LENGTH);

	argument = imc_getarg(argument, arg1, IMC_DATA_LENGTH);
	argument = imc_getarg(argument, arg2, IMC_DATA_LENGTH);
	
	if (!str_cmp(arg1, "reload")) {
		//	reload config file - shut down and restart
		char *temp;
		release_buffer(arg1);
		release_buffer(arg2);

		if (imc_lock) {
			imc_qerror("Reloading the config from within IMC is a Bad Thing <tm>");
			return -1;
		}

		temp = str_dup(imc_prefix); /* imc_prefix gets freed, so keep a copy */
    
		imc_shutdown();
		imc_startup(temp);

		free(temp);
		return 1;
	}

	if (!*arg1 || !*arg2) {
		release_buffer(arg1);
		release_buffer(arg2);
		return 0;
	}

	if (!str_cmp(arg1, "add")) {
		release_buffer(arg1);
		
		if (imc_name && !str_cmp(arg2, imc_name)) {
			imc_qerror("%s has been specified as the local mud name. Use 'imc add'"
					   "to add connections to _other_ muds", imc_name);
			release_buffer(arg2);
			return -1;
		}
    
		if (imc_getinfo(arg2) != NULL) {
			imc_qerror("A mud by that name is already configured");
			release_buffer(arg2);
			return -1;
		}

		i = new IMCInfo();

		i->name		= str_dup(arg2);
		i->host		= str_dup("");
		i->clientpw	= str_dup("");
		i->serverpw	= str_dup("");

		release_buffer(arg2);
		return 1;
	} else if (!str_cmp(arg1, "delete")) {
		i = imc_getinfo(arg2);
		release_buffer(arg1);
		release_buffer(arg2);

		if (!i) {
			imc_qerror("Entry not found");
			return -1;
		}

		delete i;
		imc_saveconfig();

		return 1;
	} else if (!str_cmp(arg1, "rename")) {
		release_buffer(arg1);
		i = imc_getinfo(arg2);
		
		if (!i) {
			imc_qerror("Entry not found");
			release_buffer(arg2);
			return -1;
		}

		if (i->connection)
			imc_disconnect(i->name);

		argument = imc_getarg(argument, arg2, IMC_DATA_LENGTH);
		if (!*arg2) {
			release_buffer(arg2);
			return 0;
		}
		
		if (i->name)	free(i->name);
		i->name = str_dup(arg2);

		imc_saveconfig();
		release_buffer(arg2);
		return 1;
	} else if (!str_cmp(arg1, "set")) {
		release_buffer(arg1);
		i = imc_getinfo(arg2);

		if (!i) {
			imc_qerror("Entry not found");
			release_buffer(arg2);
			return -1;
		}

		argument = imc_getarg(argument, arg2, IMC_DATA_LENGTH);

		if (!*arg2 || !*argument) {
			release_buffer(arg2);
			return 0;
		} else if (!str_cmp(arg2, "all")) {
			if (i->host)		free(i->host);
			if (i->clientpw)	free(i->clientpw);
			if (i->serverpw)	free(i->serverpw);

			argument	= imc_getarg(argument, arg2, IMC_DATA_LENGTH);
			i->host		= str_dup(arg2);
			argument	= imc_getarg(argument, arg2, IMC_DATA_LENGTH);
			i->port		= strtoul(arg2, NULL, 10);
			argument	= imc_getarg(argument, arg2, IMC_PW_LENGTH);
			i->clientpw	= str_dup(arg2);
			argument	= imc_getarg(argument, arg2, IMC_PW_LENGTH);
			i->serverpw	= str_dup(arg2);
			argument	= imc_getarg(argument, arg2, IMC_DATA_LENGTH);
			i->rcvstamp	= strtoul(arg2, NULL, 10);
			argument	= imc_getarg(argument, arg2, IMC_DATA_LENGTH);
			i->noforward= strtoul(arg2, NULL, 10);
			argument	= imc_getarg(argument, arg2, IMC_DATA_LENGTH);
			i->flags	= imc_flagvalue(arg2, imc_connection_flags);

			imc_saveconfig();

			release_buffer(arg2);
			return 1;
		} else if (!str_cmp(arg2, "host")) {
			if (i->host)	free(i->host);
			i->host = str_dup(argument);

			imc_saveconfig();
			release_buffer(arg2);
			return 1;
		} else if (!str_cmp(arg2, "port")) {
			i->port = strtoul(argument, NULL, 10);

			imc_saveconfig();
			release_buffer(arg2);
			return 1;
		} else if (!str_cmp(arg2, "clientpw")) {
			if (i->clientpw)	free(i->clientpw);
			i->clientpw = str_dup(argument);

			imc_saveconfig();
			release_buffer(arg2);
			return 1;
		} else if (!str_cmp(arg2, "serverpw")) {
			if (i->serverpw)	free(i->serverpw);
			i->serverpw = str_dup(argument);

			imc_saveconfig();
			release_buffer(arg2);
			return 1;
		} else if (!str_cmp(arg2, "rcvstamp")) {
			i->rcvstamp = strtoul(argument, NULL, 10);

			imc_saveconfig();
			release_buffer(arg2);
			return 1;
		} else if (!str_cmp(arg2, "noforward")) {
			i->noforward = strtoul(argument, NULL, 10);

			imc_saveconfig();
			release_buffer(arg2);
			return 1;
		} else if (!str_cmp(arg2, "flags")) {
			i->flags = imc_flagvalue(argument, imc_connection_flags);

			imc_saveconfig();
			release_buffer(arg2);
			return 1;
		}

		release_buffer(arg2);
		return 0;
	} else if (!str_cmp(arg1, "localname")) {
		release_buffer(arg1);
		if (imc_lock) {
			imc_qerror("Changing localname from within IMC is a Bad Thing <tm>");
			release_buffer(arg2);
			return -1;
		}

		//	shut down IMC, change name, and restart
		if (imc_active >= IA_UP)	imc_shutdown_network();
		if (imc_name)				free(imc_name);

		imc_name	= str_dup(arg2);
		imc_active	= IA_CONFIG2;

		imc_startup_network();
		imc_saveconfig();
		
		release_buffer(arg2);

		return 1;
	} else if (!str_cmp(arg1, "localport")) {
		release_buffer(arg1);
		int p = atoi(arg2);

		if ((p < 1024 || p > 65535) && p != 0) {
			imc_qerror("Port number must be 0 or 1024..65535");
			release_buffer(arg2);
			return -1;
		}

		if (imc_active >= IA_LISTENING)
			imc_shutdown_port();

		imc_port = p;

		if (imc_active == IA_UP && imc_port)
			imc_startup_port();

		imc_saveconfig();

		release_buffer(arg2);
		return 1;
	} else if (!str_cmp(arg1, "info")) {
		release_buffer(arg1);
		if (!str_cmp(arg2, "name")) {
			free(imc_siteinfo.name);
			imc_siteinfo.name = str_dup(argument);

			if (imc_active == IA_CONFIG2)
				imc_startup_network();
		} else if (!str_cmp(arg2, "host")) {
			free(imc_siteinfo.host);
			imc_siteinfo.host = str_dup(argument);
		} else if (!str_cmp(arg2, "email")) {
			free(imc_siteinfo.email);
			imc_siteinfo.email = str_dup(argument);
      
			if (imc_active == IA_CONFIG2)
				imc_startup_network();
		} else if (!str_cmp(arg2, "imail")) {
			free(imc_siteinfo.imail);
			imc_siteinfo.imail = str_dup(argument);
		} else if (!str_cmp(arg2, "www")) {
			free(imc_siteinfo.www);
			imc_siteinfo.www = str_dup(argument);
		} else if (!str_cmp(arg2, "details")) {
			free(imc_siteinfo.details);
			imc_siteinfo.details = str_dup(argument);
		} else if (!str_cmp(arg2, "flags")) {
			free(imc_siteinfo.flags);
			imc_siteinfo.flags = str_dup(argument);
		} else {
			release_buffer(arg2);
			return 0;
		}

		imc_saveconfig();
		release_buffer(arg2);
		return 1;
	}

	release_buffer(arg1);
	release_buffer(arg2);
	return 0;
}

/* read an IMC rignores file */
int imc_readignores(void) {
	FILE *inf;
	char *buf;
	char *name = get_buffer(200);
	char *arg;
	int type;
	int count;

	imc_sncpy(name, imc_prefix, 191);
	strcat(name, "rignores");

	inf = fopen(name, "r");
	if (!inf) {
		imc_logerror("imc_readignores: couldn't open %s", name);
		release_buffer(name);
		return 0;
	}
	release_buffer(name);
	
	buf = get_buffer(1000);
	arg = get_buffer(IMC_NAME_LENGTH);
	
	while (!ferror(inf) && !feof(inf)) {
		if (fgets(buf, 1000, inf) == NULL)
			break;

		if ((*buf == '#') || (*buf == '\n'))
			continue;

		sscanf(buf, "%[^ \n]%n", arg, &count);
		type = imc_statevalue(buf+count, imc_ignore_types);
		imc_addignore(arg, type);  /* add the entry */
	}

	release_buffer(buf);
	release_buffer(arg);

	if (ferror(inf)) {
		imc_lerror("imc_readignores");
		fclose(inf);
		return 0;
	}

	fclose(inf);
	return 1;
}

/* save the current rignore list */
int imc_saveignores(void) {
	FILE *out;
	char *name = get_buffer(200);
	IMCIgnore *ign;

	imc_sncpy(name, imc_prefix, 191);
	strcat(name, "rignores");

	out = fopen(name, "w");
	if (!out) {
		imc_lerror("imc_saveignores: error opening %s", name);
		release_buffer(name);
		return 0;
	}
	release_buffer(name);

	fprintf(out,"# IMC rignores file, one name per line, no leading spaces\n"
				"# types: ignore, notrust, trust\n"
				"# lines starting with '#' are discarded\n");

	LListIterator<IMCIgnore *>	iter(imc_ignore_list);
	while ((ign = iter.Next()))
			fprintf(out, "%s%s%s %s\n",
					(ign->match & IMC_IGNORE_PREFIX) ? "*" : "", ign->name,
					(ign->match & IMC_IGNORE_SUFFIX) ? "*" : "",
					imc_statename(ign->type, imc_ignore_types));

	if (ferror(out)) {
		imc_lerror("imc_saveignores: error saving %s", name);
		fclose(out);
		return 0;
	}

	fclose(out);
	return 1;
}

IMCIgnore::IMCIgnore(void) : name(NULL), match(IMC_IGNORE_EXACT), type(-1) {
	imc_ignore_list.Add(this);
}



IMCIgnore::~IMCIgnore(void) {
	if (this->name)	free(this->name);
}


int imc_delignore(const char *what) {
	IMCIgnore *ign;
	char *who;
	char *buf = get_buffer(IMC_DATA_LENGTH);
	int match=0;

	strcpy(buf, what);
	who = buf;

	if (*who == '*') {
		who++;
		match |= IMC_IGNORE_PREFIX;
	}

	if (*who && (who[strlen(who)-1] == '*')) {
		who[strlen(who)-1] = '\0';
		match |= IMC_IGNORE_SUFFIX;
	}

	LListIterator<IMCIgnore *>	iter(imc_ignore_list);
	while ((ign = iter.Next())) {
		if ((match == ign->match) && !str_cmp(who, ign->name)) {
			imc_ignore_list.Remove(ign);
			
			delete ign;
			release_buffer(buf);
			return 1;
		}
	}

	release_buffer(buf);
	return 0;
}


void imc_addignore(const char *what, int type) {
	IMCIgnore 	*ign;
	char 		*buf = get_buffer(IMC_DATA_LENGTH);
	char 		*who;
	int 		match = 0;

	ign = new IMCIgnore();
	ign->type = type;

	strcpy(buf, what);
	who = buf;

	if (*who == '*') {
		match |= IMC_IGNORE_PREFIX;
		who++;
	}

	if (*who && (who[strlen(who) - 1] == '*')) {
		who[strlen(who) - 1] = 0;
		match |= IMC_IGNORE_SUFFIX;
	}

	ign->match = match;
	ign->name = str_dup(who);
	release_buffer(buf);
}

/* add/remove/list rignores */
const char *imc_ignore(const char *what) {
	int count;
	IMCIgnore *ign;
	char *arg = get_buffer(IMC_NAME_LENGTH);
	int type;

	what = imc_getarg(what, arg, IMC_NAME_LENGTH);

	if (!*arg) {
		char *buf = imc_getsbuf(IMC_DATA_LENGTH);

		strcpy(buf, "Current entries:\r\n");
		
		count = 0;
		LListIterator<IMCIgnore *>	iter(imc_ignore_list);
		while ((ign = iter.Next())) {
			sprintf(buf + strlen(buf), " %10s   %s%s%s\r\n",
					imc_statename(ign->type, imc_ignore_types),
					(ign->match & IMC_IGNORE_PREFIX) ? "*" : "", ign->name,
					(ign->match & IMC_IGNORE_SUFFIX) ? "*" : "");
			count++;
		}

		if (!count)		strcat(buf, " none");
		else			sprintf(buf + strlen(buf), "[total %d]", count);

		release_buffer(arg);
		imc_shrinksbuf(buf);
		return buf;
	}

	if (!*what) {
		release_buffer(arg);
		return "Must specify both action and name.";
	}
	
	if (!str_cmp(arg, "delete")) {
		release_buffer(arg);
		if (imc_delignore(what)) {
			imc_saveignores();
			return "Entry deleted.";
		}
		return "Entry not found.";
	}

	type = imc_statevalue(arg, imc_ignore_types);
	
	release_buffer(arg);
	
	if (type < 0)
		return "Unknown ignore type";

	imc_addignore(what, type);
	imc_saveignores();

	return "Entry added.";
}

/* check if needle is in haystack (case-insensitive) */
static int substr(const char *needle, const char *haystack) {
	int len = strlen(needle);
	
	if (!haystack) {
		log("Substr called with no haystack!");
		return 0;
	}
	
	if (!len)
		return 1;

//	while ((haystack = strchr(haystack, *needle)))		-- Case insensitive, huh?
	while ((haystack = strchr(haystack, LOWER(*needle))))
		if (!strn_cmp(haystack, needle, len))
			return 1;

	return 0;
}

/* find ignore data on someone */
IMCIgnore *imc_findignore(const char *who, int type) {
	IMCIgnore *ign;
	int len;
	int wlen=strlen(who);

	LListIterator<IMCIgnore *>	iter(imc_ignore_list);
	while ((ign = iter.Next())) {
		if ((type >= 0) && (type != ign->type))	continue;

		len = strlen(ign->name);

		switch (ign->match) {
			case 0:										//	exact match
				if (!str_cmp(ign->name, who))		return ign;
				break;
			case IMC_IGNORE_SUFFIX:						//	abcd*
				if (!strn_cmp(ign->name, who, len))	return ign;
				break;
			case IMC_IGNORE_PREFIX:						//	*abcd
				wlen = strlen(who);
				if (wlen>=len && !strn_cmp(ign->name, who+wlen-len, len))
					return ign;
				break;
			case IMC_IGNORE_PREFIX|IMC_IGNORE_SUFFIX:	//	*abcd*
				if (substr(ign->name, who))			return ign;
				break;
		}
	}

	return NULL;
}

/* check if a packet from a given source should be ignored */
int imc_isignored(const char *who)
{
  return (imc_findignore(who, IMC_IGNORE)!=NULL);
}


/* show current IMC socket states */
const char *imc_sockets(void) {
	IMCConnect *c;
	char *buf = imc_getsbuf(IMC_DATA_LENGTH);
	char *state;
	int r, s;

	if (imc_active < IA_UP)
		return "IMC is not active.\r\n";

	sprintf(buf, "%4s %-9s %-15s %-11s %-11s %-3s %-3s\r\n",
			"Desc", "Mud", "State", "Inbuf", "Outbuf", "Sp1", "Sp2");

	START_ITER(iter, c, IMCConnect *, imc_connect_list) {
		switch (c->state) {
			case IMC_CLOSED:		state = "closed";		break;
			case IMC_CONNECTING:	state = "connecting";	break;
			case IMC_WAIT1:			state = "wait1";		break;
			case IMC_WAIT2:			state = "wait2";		break;
			case IMC_CONNECTED:		state = "connected";	break;
			default:				state = "unknown";		break;
		}
    
#ifdef USEIOCTL
		/* try to work out the system buffer sizes */
		r = 0;
		ioctl(c->desc, TIOCINQ, &r);
		r += strlen(c->inbuf);
    
		s = outqsize;
		if (s) {
			ioctl(c->desc, TIOCOUTQ, &s);
			s = outqsize-s;
		}
		s += strlen(c->outbuf);
#else
		r = strlen(c->inbuf);
		s = strlen(c->outbuf);
#endif

		sprintf(buf + strlen(buf), "%4d %-9s %-15s %5d/%-5d %5d/%-5d %3d %3d\r\n",
				c->desc, c->info ? c->info->name : "unknown", state, r, c->insize, s,
				c->outsize, c->spamcounter1, c->spamcounter2);
	} END_ITER(iter);

	imc_shrinksbuf(buf);
	return buf;
}

/*  list current connections/known muds
 *  level=0 is mortal-level access (mudnames and connection states)
 *  level=1 is imm-level access (names, hosts, ports, states)
 *  level=2 is full access (names, hosts, ports, passwords, flags, states)
 *  level=3 is all known muds on IMC, and no direct connection status
 *  level=4 is IMC local config info (mortal-safe)
 *  level=5 is IMC local config info
 */
const char *imc_list(int level) {
	IMCInfo *i;
	char *buf = imc_getsbuf(IMC_DATA_LENGTH);
	char *state;
	IMCRemInfo *p;

	if (level <= 2) {
		strcpy(buf, "Direct connections:\r\n");
    
		switch (level) {
			case 0:
				sprintf(buf + strlen(buf), "%-10s %-15s %-8s", "Name", "State", "LastConn");
				break;
			case 1:
				sprintf(buf + strlen(buf), "%-10s %-30s %5s %-13s %-8s", "Name", "Host",
						"Port", "State", "LastConn");
				break;
			case 2:
				sprintf(buf + strlen(buf),
						"%-8s %-25s %5s %-13s %-10s %-10s\n"
						"         %-8s %-9s %-5s %-5s %-8s %s",
						"Name", "Host", "Port", "State", "ClientPW", "ServerPW",
						"RcvStamp", "NoForward", "Timer", "TimeD", "LastConn", "Flags");
				break;
		}
    	
    	char *lastconn = get_buffer(20);
    	START_ITER(iter, i, IMCInfo *, imc_info_list) {
			state = i->connection ? "connected" : "not connected";

			if (i->connection)				lastconn[0] = 0;
			else if (!i->last_connected)	strcpy(lastconn, "never");
			else {
				long diff = (long)imc_now - (long)i->last_connected;
				sprintf(lastconn, "%5ld:%02ld", diff/3600, (diff/60) % 60);
			}

			switch (level) {
				case 0:
					sprintf(buf + strlen(buf), "\r\n%-10s %-15s %8s", i->name, state, lastconn);
					break;
				case 1:
					sprintf(buf + strlen(buf), "\r\n%-10s %-30s %5hu %-13s %8s",
							i->name, i->host, i->port, state, lastconn);
					break;
				case 2:
					sprintf(buf + strlen(buf), "\r\n%-8s %-25s %5hu %-13s %-10s %-10s"
												"\r\n         %-8d %-9d %-5d %-5ld %8s %s",
							i->name, i->host, i->port, state, i->clientpw, i->serverpw,
							i->rcvstamp, i->noforward, imc_next_event(ev_reconnect, i),
							i->timer_duration, lastconn, imc_flagname(i->flags, imc_connection_flags));
					break;
			}
		} END_ITER(iter);
		release_buffer(lastconn);
    
		imc_shrinksbuf(buf);
		return buf;
	}

	if ((level == 3) || (level == 6)) {
		strcpy(buf, "Active muds on IMC:\r\n");
		if (imc_is_router)
			sprintf(buf + strlen(buf), "%-10s  %-10s  %-9s  %-20s  %-10s",
					"Name", "Last alive", "Ping time", "IMC Version", "Route");
		else
			sprintf(buf + strlen(buf), "%-10s  %-10s  %-20s  %-10s",
					"Name", "Last alive", "IMC Version", "Route");
    	
    	START_ITER(iter, p, IMCRemInfo *, imc_reminfo_list) {
			if (p->hide && (level == 3))
				continue;
      
			if (imc_is_router) {
				if (p->ping)
					sprintf(buf + strlen(buf), "\r\n%-10s  %9ds  %7dms  %-20s  %-10s %s",
							p->name, (int) (imc_now - p->alive), p->ping, p->version,
							p->route ? p->route : "broadcast", p->type ? "expired" : "");
				else
					sprintf(buf + strlen(buf), "\r\n%-10s  %9ds  %9s  %-20s  %-10s %s",
							p->name, (int) (imc_now - p->alive), "unknown", p->version,
							p->route ? p->route : "broadcast", p->type ? "expired" : "");
			} else
				sprintf(buf + strlen(buf), "\r\n%-10s  %9ds  %-20s  %-10s %s",
						p->name, (int) (imc_now - p->alive), p->version,
						p->route ? p->route : "broadcast", p->type ? "expired" : "");
		} END_ITER(iter);

		imc_shrinksbuf(buf);
		return buf;
	}

	if (level == 4) {
		sprintf(buf,"Local IMC configuration:\r\n"
					"  IMC name:    %s\r\n"
					"  IMC version: %s\r\n"
					"  IMC state:   %s",
				imc_name ? imc_name : "not set",
				IMC_VERSIONID,
				(imc_active>=IA_UP) ? "active" : "not active");
		imc_shrinksbuf(buf);
		return buf;
	}

	if (level == 5) {
		sprintf(buf,"Local IMC configuration:\r\n"
					"  IMC name:    %s\r\n"
					"  IMC port:    %hu\r\n"
					"  IMC version: %s\r\n"
					"  IMC state:   %s\r\n"
					"Site information:\r\n"
					"  Full name:   %s\r\n"
					"  Host/port:   %s\r\n"
					"  Email:       %s\r\n"
					"  IMC mail:    %s\r\n"
					"  Webpage:     %s\r\n"
					"  Details:     %s\r\n"
					"  Flags:       %s\r\n",
			(imc_active>=IA_CONFIG2) ? imc_name : "not set",
			imc_port,
			IMC_VERSIONID,
			imc_statename(imc_active, imc_active_names),

			imc_siteinfo.name,
			imc_siteinfo.host,
			imc_siteinfo.email,
			imc_siteinfo.imail,
			imc_siteinfo.www,
			imc_siteinfo.details,
			imc_siteinfo.flags);
    
		imc_shrinksbuf(buf);
		return buf;
	}

	imc_shrinksbuf(buf);
	return "Bad invocaton of imc_list.";
}


/* get some IMC stats, return a string describing them */
const char *imc_getstats(void) {
	char *buf = imc_getsbuf(300);

	sprintf(buf,
			"IMC statistics\r\n"
			"\r\n"
			"Received packets:    %d\r\n"
			"Received bytes:      %d (%ld/second)\r\n"
			"Transmitted packets: %d\r\n"
			"Transmitted bytes:   %d (%ld/second)\r\n"
			"Maximum packet size: %d\r\n"
			"Pending events:      %d\r\n"
			"Sequence drops:      %d\r\n",

			imc_stats.rx_pkts,
			imc_stats.rx_bytes,
			imc_stats.rx_bytes / ((imc_now - imc_stats.start) ? (imc_now - imc_stats.start) : 1),
			imc_stats.tx_pkts,
			imc_stats.tx_bytes,
			imc_stats.tx_bytes / ((imc_now - imc_stats.start) ? (imc_now - imc_stats.start) : 1),
			imc_stats.max_pkt,
			imc_event_list.Count(),
			imc_stats.sequence_drops);

	imc_shrinksbuf(buf);
	return buf;
}


