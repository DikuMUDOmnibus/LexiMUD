/*
 * IMC2 - an inter-mud communications protocol
 *
 * imc-util.c: misc utility functions for IMC
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

#include <stdarg.h>

/*
 * I needed to split up imc.c (2600 lines and counting..) so this file now
 * contains:
 *
 * in general: anything which is called from more than one file and is not
 * obviously an interface function is a candidate for here.
 *
 * specifically:
 * - general string manipulation functions
 * - flag and state lookup functions
 * - error logging
 * - imc_reminfo creation/lookup/deletion
 * - imc_info lookup
 * - connection naming
 * - reconnect setup
 * - static buffer allocation
 */

char imc_lasterror[IMC_DATA_LENGTH];	  /* last error reported */

/*
 *  Static buffer allocation - greatly reduces IMC's memory footprint
 */

/* reserves about 64k for static buffers */
#define ssize (IMC_DATA_LENGTH * 4)
static char sspace[ssize];
static int soffset;
static char *lastalloc;

char *imc_getsbuf(int len) {
	char *buf;
  
	if (soffset >= (ssize - len))
		soffset = 0;
  
	buf = &sspace[soffset];
	soffset = (soffset + len) % ssize;

	*buf = '\0';
	lastalloc = buf;
	return buf;
}


void imc_shrinksbuf(char *buf) {
	int offset;

	if (!buf || buf != lastalloc)
		return;

	offset = buf - sspace;
	soffset = offset + strlen(buf) + 1;
}


/*
 * Key/value manipulation
 */

/* clone packet data */

void IMCData::Clone(const IMCData *n) {
	int i;

	for (i = 0; i < IMC_MAX_KEYS; i++) {
    	this->key[i] = n->key[i] ? str_dup(n->key[i]) : NULL;
		this->value[i] = n->value[i] ? str_dup(n->value[i]) : NULL;
	}
}

/* get the value of "key" from "p"; if it isn't present, return "def" */
const char *IMCData::GetKey(const char *key, const char *def) const {
	int i;

	for (i = 0; i < IMC_MAX_KEYS; i++)
		if (this->key[i] && !str_cmp(this->key[i], key))
			return this->value[i];

	return def;
}


/* identical to imc_getkey, except get the integer value of the key */
SInt32 IMCData::GetKey(const char *key, int def) const {
	int i;

	for (i = 0; i < IMC_MAX_KEYS; i++)
		if (this->key[i] && !str_cmp(this->key[i], key))
			return atoi(this->value[i]);

	return def;
}


/* add "key=value" to "p" */
void IMCData::AddKey(const char *key, const char *value) {
	int i;

	for (i = 0; i < IMC_MAX_KEYS; i++) {
		if (this->key[i] && !str_cmp(key, this->key[i])) {
			FREE(this->key[i]);
			FREE(this->value[i]);
			this->key[i]	= NULL;
			this->value[i]	= NULL;
			break;
		}
	}
	
	if (!value)
		return;

	for (i = 0; i < IMC_MAX_KEYS; i++) {
		if (!this->key[i]) {
			this->key[i]   = str_dup(key);
			this->value[i] = str_dup(value);
			return;
		}
	}
}


/* add "key=value" for an integer value */
void IMCData::AddKey(const char *key, int value) {
	char *temp = get_buffer(20);
	sprintf(temp, "%d", value);
	this->AddKey(key, temp);
	release_buffer(temp);
}

/* clear all keys in "p" */
void IMCData::Init(void) {
	int i;

	for (i = 0; i < IMC_MAX_KEYS; i++) {
		this->key[i]	= NULL;
		this->value[i]	= NULL;
	}
}


/* free all the keys in "p" */
void IMCData::Free(void) {
	int i;

	for (i = 0; i < IMC_MAX_KEYS; i++) {
		if (this->key[i])	free(this->key[i]);
		this->key[i] = NULL;
		if (this->value[i])	free(this->value[i]);
		this->value[i] = NULL;
	}
}


//	Error logging

//	log a string
void imc_logstring(const char *format, ...) {
	va_list ap;
	char *buf = get_buffer(IMC_DATA_LENGTH);

	va_start(ap, format);
	vsnprintf(buf, IMC_DATA_LENGTH, format, ap);
	va_end(ap);

	imc_log(buf);
	release_buffer(buf);
}

//	log an error (log string and copy to lasterror)
void imc_logerror(const char *format, ...) {
	va_list ap;

	va_start(ap, format);
	vsnprintf(imc_lasterror, IMC_DATA_LENGTH, format, ap);
	va_end(ap);

	imc_log(imc_lasterror);
}

//	log an error quietly (just copy to lasterror)
void imc_qerror(const char *format, ...) {
	va_list ap;

	va_start(ap, format);
	vsnprintf(imc_lasterror, IMC_DATA_LENGTH, format, ap);
	va_end(ap);
}

/* log a system error (log string, ": ", string representing errno)   */
/* this is particularly broken on SunOS (which doesn't have strerror) */
void imc_lerror(const char *format, ...) {
	va_list ap;

	va_start(ap, format);
	vsnprintf(imc_lasterror, IMC_DATA_LENGTH, format, ap);
	strcat(imc_lasterror, ": ");
	strcat(imc_lasterror, strerror(errno));

	imc_log(imc_lasterror);
}

const char *imc_error(void) {
	return imc_lasterror;
}


//	String manipulation functions, mostly exported

//	lowercase what
void imc_slower(char *what) {
	char *p = what;
	while (*p) {
		*p = tolower(*p);
		p++;
	}
}


//	copy src->dest, max count, null-terminate
void imc_sncpy(char *dest, const char *src, int count) {
	strncpy(dest, src, count-1);
	dest[count-1] = 0;
}


//	return 'mud' from 'player@mud'
const char *imc_mudof(const char *fullname) {
	char *buf = imc_getsbuf(IMC_MNAME_LENGTH);
	char *where;

	where = strchr(fullname, '@');
	imc_sncpy(buf, where ? where+1 : fullname, IMC_MNAME_LENGTH);
	
	imc_shrinksbuf(buf);
	return buf;
}


/* return 'player' from 'player@mud' */
const char *imc_nameof(const char *fullname) {
	char *buf = imc_getsbuf(IMC_PNAME_LENGTH);
	char *where = buf;
	int count = 0;

	while (*fullname && (*fullname != '@') && (count < IMC_PNAME_LENGTH - 1))
		*where++ = *fullname++, count++;

	*where = 0;
	imc_shrinksbuf(buf);
	return buf;
}

/* return 'player@mud' from 'player' and 'mud' */
const char *imc_makename(const char *player, const char *mud) {
	char *buf = imc_getsbuf(IMC_NAME_LENGTH);

	imc_sncpy(buf, player, IMC_PNAME_LENGTH);
	strcat(buf, "@");
	imc_sncpy(buf + strlen(buf), mud, IMC_MNAME_LENGTH);
	imc_shrinksbuf(buf);
	return buf;
}


/* return 'e' from 'a!b!c!d!e' */
const char *imc_lastinpath(const char *path) {
	const char *where;
	char *buf = imc_getsbuf(IMC_NAME_LENGTH);

	where = path + strlen(path)-1;
	while ((*where != '!') && (where >= path))
		where--;

	imc_sncpy(buf, where + 1, IMC_NAME_LENGTH);
	imc_shrinksbuf(buf);
	return buf;
}


/* return 'a' from 'a!b!c!d!e' */
const char *imc_firstinpath(const char *path) {
	char *buf = imc_getsbuf(IMC_NAME_LENGTH);
	char *p;

	for (p = buf; *path && (*path != '!'); *p++ = *path++);

	*p = 0;
	imc_shrinksbuf(buf);
	return buf;
}


//	imc_getarg: extract a single argument (with given max length) from
//	argument to arg; if arg==NULL, just skip an arg, don't copy it out
const char *imc_getarg(const char *argument, char *arg, int length) {
	int len = 0;

	while (*argument && isspace(*argument))
		argument++;

	if (arg)	while (*argument && !isspace(*argument) && (len < (length - 1)))
					*arg++ = *argument++, len++;
	else		while (*argument && !isspace(*argument))
					argument++;

	while (*argument && !isspace(*argument))	argument++;
	while (*argument && isspace(*argument))		argument++;

	if (arg)	*arg = '\0';

	return argument;
}

/* Check for a name in a list */
int imc_hasname(const char *list, const char *name) {
	const char *p;
	char *arg = get_buffer(IMC_NAME_LENGTH);

	p = imc_getarg(list, arg, IMC_NAME_LENGTH);
	while (*arg) {
		if (!str_cmp(name, arg)) {
			release_buffer(arg);
			return 1;
		}
		p = imc_getarg(p, arg, IMC_NAME_LENGTH);
	}
	release_buffer(arg);
	return 0;
}

/* Add a name to a list */
void imc_addname(char **list, const char *name) {
	char *buf;

	if (imc_hasname(*list, name))
		return;

	buf = get_buffer(IMC_DATA_LENGTH);
	
	if (*(*list))	sprintf(buf, "%s %s", *list, name);
	else			strcpy(buf, name);
  
	if (*list) free(*list);
	*list = str_dup(buf);
	release_buffer(buf);
}

/* Remove a name from a list */
void imc_removename(char **list, const char *name) {
	char *buf = get_buffer(1000);
	char *arg = get_buffer(IMC_NAME_LENGTH);
	const char *p;

	*buf = 0;
	p = imc_getarg(*list, arg, IMC_NAME_LENGTH);
	while (*arg) {
		if (str_cmp(arg, name)) {
			if (*buf)
				strcat(buf, " ");
			strcat(buf, arg);
		}
		p = imc_getarg(p, arg, IMC_NAME_LENGTH);
	}

	if (*list)	free(*list);
	*list = str_dup(buf);
	
	release_buffer(buf);
	release_buffer(arg);
}

/*
 *  Flag interpretation
 */

/* look up a value in a table */
const char *imc_statename(int value, const IMCFlagType *table) {
	int i;

	for (i=0; table[i].name; i++)
		if (value == table[i].value)
			return table[i].name;

	return "unknown";
}

/* return the name of a particular set of flags */
const char *imc_flagname(int value, const IMCFlagType *table) {
	char *buf = imc_getsbuf(100);
	int i;

	*buf = 0;

	for (i = 0; table[i].name; i++) {
		if (IS_SET(value, table[i].value) == table[i].value) {
			strcat(buf, table[i].name);
			strcat(buf, " ");
			REMOVE_BIT(value, table[i].value);
//			value &= ~table[i].value;
		}
	}

	if (*buf)	buf[strlen(buf)-1] = 0;
	else		strcpy(buf, "none");

	imc_shrinksbuf(buf);
	return buf;
}


//	return the value corresponding to a set of names
int imc_flagvalue(const char *name, const IMCFlagType *table) {
	char *buf = get_buffer(20);
	int i, value = 0;

	while (1) {
		name = imc_getarg(name, buf, 20);
		if (!*buf)
			break;

		for (i = 0; table[i].name; i++)
			if (!str_cmp(table[i].name, buf))
				SET_BIT(value, table[i].value);
//				value |= table[i].value;
	}
	release_buffer(buf);
	return value;
}


//	return the value corresponding to a name
int imc_statevalue(const char *name, const IMCFlagType *table) {
	SInt32	i, value = -1;
	char *	buf = get_buffer(20);

	imc_getarg(name, buf, 20);

	for (i = 0; table[i].name; i++) {
		if (!str_cmp(table[i].name, buf)) {
			value = table[i].value;
			break;
		}
	}
	release_buffer(buf);

	return value;
}


//	IMCRemInfo handling

//	find an info entry for "name"
IMCRemInfo *imc_find_reminfo(const char *name, int type) {
	IMCRemInfo *p;
	LListIterator<IMCRemInfo *>	iter(imc_reminfo_list);
	
	while((p = iter.Next())) {
		if ((p->type == IMC_REMINFO_EXPIRED) && !type)	continue;
		if (!str_cmp(name, p->name))					return p;
	}
	
	return NULL;
}


IMCRemInfo::IMCRemInfo(void) : name(NULL), version(NULL), alive(0), ping(0),
		type(IMC_REMINFO_NORMAL), hide(0), route(NULL), top_sequence(0) {
	imc_reminfo_list.Add(this);
}


IMCRemInfo::~IMCRemInfo(void) {
	imc_reminfo_list.Remove(this);

	if (this->name)		free(this->name);
	if (this->version)	free(this->version);
	if (this->route)	free(this->route);

	imc_cancel_event(NULL, this);
}


/* get info struct for given mud */
IMCInfo *imc_getinfo(const char *mud) {
	IMCInfo *p;

	START_ITER(iter, p, IMCInfo *, imc_info_list) {
		if (!str_cmp(mud, p->name))
			break;
	} END_ITER(iter);

	return p;
}


/* get name of a connection */
const char *IMCConnect::GetName(void) {
	char *buf = imc_getsbuf(IMC_NAME_LENGTH);
	const char *n;

	if (this->info)	n = this->info->name;
	else			n = "unknown";

	sprintf(buf, "%s[%d]", n, this->desc);
	imc_shrinksbuf(buf);
	return buf;
}


/* set up for a reconnect */
void IMCInfo::SetupReconnect(void) {
	time_t temp;
	int t;

	//	add a bit of randomness so we don't get too many simultaneous reconnects
	temp = this->timer_duration + (rand() % 21) - 20;
	t = imc_next_event(ev_reconnect, this);

	if ((t >= 0) && (t < temp))
		return;

	this->timer_duration *= 2;
	if (this->timer_duration > IMC_MAX_RECONNECT_TIME)
		this->timer_duration = IMC_MAX_RECONNECT_TIME;
	imc_add_event(temp, ev_reconnect, this, 1);
}



