/*
 * IMC2 - an inter-mud communications protocol
 *
 * icec-mercbase.c: IMC-channel-extensions (ICE) Merc-specific client code
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

#define IN_IMC

#include "imc.h"
#include "imc-mercbase.h"
#include "icec.h"
#include "icec-mercbase.h"

#include "buffer.h"

//	Local data
static LList<ICEChannel *>	saved_channel_list;

//	Local functions
void icec_save_channels(void);


void icec_save_channels(void) {
	ICEChannel *c;
	FILE *		fp;
	char *		name = get_buffer(MAX_STRING_LENGTH);

	sprintf(name, "%sicec", imc_prefix);
	fp = fopen(name, "w");
	if (!fp) {
		imc_logerror("Can't write to %s", name);
		release_buffer(name);
		return;
	}
	release_buffer(name);
  
//	for (c = saved_channel_list; c; c = c->next) {
	LListIterator<ICEChannel *>	iter(saved_channel_list);
	while ((c = iter.Next())) {
		//	update
		ICEChannel *current = icec_findlchannel(c->local->name);
		if (current) {
			if (c->name)			free(c->name);
			if (c->local->format1)	free(c->local->format1);
			if (c->local->format2)	free(c->local->format2);

			c->name				= str_dup(current->name);
			c->local->format1	= str_dup(current->local->format1);
			c->local->format2	= str_dup(current->local->format2);
			c->local->level		= current->local->level;
		}

		//	save
		fprintf(fp, "%s %s %d\n"
					"%s\n"
					"%s\n",
				c->name, c->local->name, c->local->level,
				c->local->format1,
				c->local->format2);
	}

	fclose(fp);
}


void icec_load_channels(void) {
	FILE *fp;
	char *name = get_buffer(MAX_INPUT_LENGTH);
	char *buf1, *buf2, *buf3, *buf4;
	int l;

	sprintf(name, "%sicec", imc_prefix);

	fp=fopen(name, "r");
	if (!fp) {
		imc_logerror("Can't open %s", name);
		release_buffer(name);
		return;
	}
	release_buffer(name);
	
	buf1 = get_buffer(MAX_STRING_LENGTH);
	buf2 = get_buffer(MAX_STRING_LENGTH);
	buf3 = get_buffer(MAX_STRING_LENGTH);
	buf4 = get_buffer(MAX_STRING_LENGTH);

	while (fscanf(fp,
				"%s %s %d\n"
				"%[^\n]\n"
				"%[^\n]\n", buf1, buf2, &l, buf3, buf4) == 5) {
		ICEChannel *c = new ICEChannel();

		c->local = new ICELChannel();
    
		c->name				= str_dup(buf1);
		c->local->name		= str_dup(buf2);
		c->local->format1	= str_dup(buf3);
		c->local->format2	= str_dup(buf4);
		c->local->level		= l;
    
//		c->next = saved_channel_list;
//		saved_channel_list = c;
		saved_channel_list.Add(c);
		
		imc_logstring("ICEc: configured %s as %s", c->name, c->local->name);
	}
	release_buffer(buf1);
	release_buffer(buf2);
	release_buffer(buf3);
	release_buffer(buf4);

	fclose(fp);
}


ICEChannel *icec_findlchannel(const char *name) {
	ICEChannel *c;

//	for (c = icec_channel_list; c; c = c->next)
	LListIterator<ICEChannel *>		iter(icec_channel_list);
	while ((c = iter.Next()))
		if (c->local && !str_cmp(c->local->name, name))
			break;

	return c;
}


void ICEChannel::FreeLocal(void) {
	if (this->local) {
		delete(this->local);
		this->local = NULL;
	}
}


ICELChannel::~ICELChannel(void) {
	if (this->name)		free(this->name);
	if (this->format1)	free(this->format1);
	if (this->format2)	free(this->format2);
}


//	need exactly 2 %s's, and no other format specifiers
static bool verify_format(const char *fmt) {
	const char *c;
	SInt32		i = 0;

	c = fmt;
	while((c = strchr(c, '%'))) {
		if (*(c + 1) == '%') {  /* %% */
			c += 2;
			continue;
		}
    
		if (*(c + 1) != 's')  /* not %s */
			return FALSE;

		c++;
		i++;
	}

	if (i != 2)
		return FALSE;

	return TRUE;
}


ACMD(do_icommand) {
	skip_spaces(argument);
	send_to_char(icec_command(ch->RealName(), argument), ch);
	send_to_char("\r\n", ch);
}


ACMD(do_isetup) {
	char *	imccmd = get_buffer(IMC_NAME_LENGTH);
	char *	chan = get_buffer(IMC_NAME_LENGTH);
	char *	arg1 = get_buffer(IMC_DATA_LENGTH);
	ICEChannel *c;
	const char *a, *a1;
  
	skip_spaces(argument);

	a = imc_getarg(argument, imccmd, IMC_NAME_LENGTH);
	a = a1 = imc_getarg(a, chan, IMC_NAME_LENGTH);
	a = imc_getarg(a, arg1, IMC_DATA_LENGTH);

	if (!*imccmd || !*chan) {
		send_to_char("Syntax: isetup <command> <channel> [<data..>]\r\n", ch);
		release_buffer(imccmd);
		release_buffer(chan);
		release_buffer(arg1);
		return;
	}

	if (!(c = icec_findchannel(chan)))
		c = icec_findlchannel(chan);

	if (!c)
		send_to_char("Unknown channel.\r\n", ch);
	else if (!str_cmp(imccmd, "add")) {
		ICEChannel *newc;

		if (c->local)
			send_to_char("Channel is already active.\r\n", ch);
		else if (!*arg1)
			send_to_char("Syntax: isetup add <channel> <local name>\r\n", ch);
		else {
			char *buf = get_buffer(IMC_DATA_LENGTH);
			
//			c->local			=(icec_lchannel *)imc_malloc(sizeof(*c->local));
			c->local			= new ICELChannel();
			c->local->name		= str_dup(arg1);
			c->local->level		= LVL_IMMORT;

			sprintf(buf, "(%s) %%s: %%s", arg1);
			c->local->format1	= str_dup(buf);
			sprintf(buf, "(%s) %%s %%s", arg1);
			c->local->format2	= str_dup(buf);

			imc_addname(&ICE_LISTEN(ch), c->local->name);

//			newc=(ice_channel *)imc_malloc(sizeof(*newc));
			newc				= new ICEChannel();
			newc->name			= str_dup(c->name);
//			newc->local			= (icec_lchannel *)imc_malloc(sizeof(*newc->local));
			newc->local			= new ICELChannel();
			newc->local->name	= str_dup(arg1);
			newc->local->level	= LVL_IMMORT;

			newc->local->format1= str_dup(c->local->format1);
			newc->local->format2= str_dup(c->local->format2);

//			newc->next			= saved_channel_list;
//			saved_channel_list = newc;
			saved_channel_list.Add(newc);
			
			send_to_char("Channel added.\r\n", ch);
			icec_save_channels();
			release_buffer(buf);
		}
	} else if (!c->local)
		send_to_char("Channel is not locally configured, use 'isetup add' first.\r\n", ch);
	else if (!str_cmp(imccmd, "delete")) {
		ICEChannel *saved;

//		for (last = &saved_channel_list, saved = *last; saved; saved = *last) {
//			if (!str_cmp(saved->local->name, c->local->name)) {
//				saved->FreeLocal();
//				FREE(saved->name);
//				*last = saved->next;
//				delete saved;
//				imc_free(saved, sizeof(*saved));
//			} else
//				last = &saved->next;
//		}
		LListIterator<ICEChannel *>		iter(saved_channel_list);
		while ((saved = iter.Next())) {
			if (!str_cmp(saved->local->name, c->local->name)) {
				saved_channel_list.Remove(saved);
				delete saved;
			}
		}

		c->FreeLocal();

		send_to_char("Channel is no longer locally configured.\r\n", ch);
		icec_save_channels();
	} else if (!str_cmp(imccmd, "rename")) {
		if (!*arg1)
			send_to_char("Syntax: isetup <channel> rename <newname>\r\n", ch);
		else if (icec_findlchannel(arg1))
			send_to_char("New channel name already exists.\r\n", ch);
		else {
			if (c->local->name)		free(c->local->name);
			c->local->name = str_dup(arg1);
			send_to_char("Renamed ok.\r\n", ch);

			icec_save_channels();
		}
	} else if (!str_cmp(imccmd, "format1")) {
		if (!*a1)
			send_to_char("Syntax: isetup <channel> format1 <string>\r\n", ch);
		else if (!verify_format(a1))
			send_to_char("Bad format - must contain exactly 2 %s's.\r\n", ch);
		else {
			if (c->local->format1)		free(c->local->format1);
			c->local->format1 = str_dup(a1);
			send_to_char("Format1 changed.\r\n", ch);

			icec_save_channels();
		}
	} else if (!str_cmp(imccmd, "format2")) {
		if (!*a1)
			send_to_char("Syntax: isetup <channel> format2 <string>\r\n", ch);
		else if (!verify_format(a1))
			send_to_char("Bad format - must contain exactly 2 %s's.\r\n", ch);
    	else {
			if (c->local->format2)		free(c->local->format2);
			c->local->format2 = str_dup(a1);
			send_to_char("Format2 changed.\r\n", ch);

			icec_save_channels();
		}
	} else if (!str_cmp(imccmd, "level")) {
		if (!*arg1)
			send_to_char("Syntax: isetup <channel> level <level>\r\n", ch);
		else if (atoi(arg1) <= 0)
			send_to_char("Positive level, please.\r\n", ch);
    	else {
			c->local->level = atoi(arg1);

			send_to_char("Level changed.\r\n", ch);
			icec_save_channels();
		}
	} else
		send_to_char("Unknown command.\r\n"
				"Available commands: add delete rename format1 format2 level.\r\n", ch);
	release_buffer(imccmd);
	release_buffer(chan);
	release_buffer(arg1);
}


ACMD(do_ilist) {
	char *buf;
	ICEChannel *c;

	if (*argument) {
		if (!(c = icec_findlchannel(argument)))
			c = icec_findchannel(argument);

		if (!c) {
			send_to_char("No such channel.\r\n", ch);
			return;
		}

	    ch->Send("Channel %s:\n"
	    		 "  Local name: %s\n"
	    		 "  Format 1:   %s\n"
	    		 "  Format 2:   %s\n"
	    		 "  Level:      %d\n"
	    		 "\n"
	    		 "  Policy:     %s\n"
	    		 "  Owner:      %s\n"
	    		 "  Operators:  %s\n"
	    		 "  Invited:    %s\n"
	    		 "  Excluded:   %s\n",

				c->name,
				
				c->local ? c->local->name : "",
				c->local ? c->local->format1 : "",
				c->local ? c->local->format2 : "",
				c->local ? c->local->level : 0,
				
				c->policy == ICE_OPEN ? "open" :
				c->policy == ICE_CLOSED ? "closed" : "private",

				c->owner,
				c->operators,
				c->invited,
				c->excluded);
		return;
	}

	buf = get_buffer(MAX_STRING_LENGTH);

	sprintf(buf, "%-15s %-15s %-15s %s\r\n", "Name", "Local name", "Owner", "Policy");

	LListIterator<ICEChannel *>		iter(icec_channel_list);
	while ((c = iter.Next())) {
		sprintf(buf + strlen(buf), "%-15s %-15s %-15s %s\r\n",
				c->name, c->local ? c->local->name : "(not local)",
				c->owner, c->policy == ICE_OPEN ? "open" :
				c->policy == ICE_CLOSED ? "closed" : "private");
	}

	LListIterator<ICEChannel *>		savedIter(saved_channel_list);
	while ((c = savedIter.Next())) {
		if (!icec_findchannel(c->name)) {
			sprintf(buf+strlen(buf), "%-15s %-15s %-15s %-7s  (inactive)\r\n",
					c->name, c->local ? c->local->name : "(not local)", "", "");
		}
	}

	page_to_char(buf, ch);
	release_buffer(buf);
}


ACMD(do_ichannels) {
	char *arg = get_buffer(MAX_STRING_LENGTH);

	argument = one_argument(argument, arg);
	
	if (!*arg) {
		send_to_char("Currently tuned into:\r\n", ch);
		send_to_char(ICE_LISTEN(ch), ch);
		send_to_char("\r\n", ch);
	} else if (imc_hasname(ICE_LISTEN(ch), arg)) {
		imc_removename(&ICE_LISTEN(ch), arg);
		send_to_char("Removed.\r\n", ch);
	} else if (!icec_findlchannel(arg)) {
		send_to_char("No such channel configured locally.\r\n", ch);
	} else {
		imc_addname(&ICE_LISTEN(ch), arg);
		send_to_char("Added.\r\n", ch);
	}
	release_buffer(arg);
}


void ICEChannel::ShowChannel(const char *from, const char *txt, bool emote) {
	DescriptorData *d;
	CharData *ch;
	char *buf;

	if (!this->local)
		return;
		
	buf = get_buffer(MAX_STRING_LENGTH);

	sprintf(buf, emote ? this->local->format2 : this->local->format1, from, color_itom(txt));
	strcat(buf, "\r\n");

	START_ITER(iter, d, DescriptorData *, descriptor_list) {
		ch = d->Original();

		if (!ch || IS_NPC(ch) || (GET_LEVEL(ch) < this->local->level) ||
				!this->Audible(imc_makename(ch->RealName(), imc_name)) ||
				!imc_hasname(ICE_LISTEN(ch), this->local->name))
			continue;
		send_to_char(buf, ch);
	} END_ITER(iter);
	
	release_buffer(buf);
}


/* check for icec channels, return TRUE to stop command processing, FALSE otherwise */
bool icec_command_hook(CharData *ch, const char *command, char *argument) {
	ICEChannel *c;
	char *		arg;
	const char *word2;
	bool		emote = false;
  
	if (IS_NPC(ch))
		return FALSE;
  
	skip_spaces(argument);
  
	c = icec_findlchannel(command);
	if (!c || !c->local)
		return FALSE;

	if (c->local->level > GET_LEVEL(ch))
		return FALSE;

	if (!imc_hasname(ICE_LISTEN(ch), c->local->name))
		return FALSE;
  
	if (!c->Audible(imc_makename(ch->RealName(), imc_name))) {
		send_to_char("You cannot use that channel.\r\n", ch);
		return TRUE;
	}

	arg = get_buffer(MAX_STRING_LENGTH);
	word2 = imc_getarg(argument, arg, MAX_STRING_LENGTH);

	if (!*arg)
		send_to_char("Use ichan to toggle the channel - or provide some text.\r\n", ch);
	else {
		if (!str_cmp(arg, ",") || !str_cmp(arg, "emote")) {
			emote = true;
			argument = const_cast<char *>(word2);
		}

		c->SendMessage(ch->RealName(), color_mtoi(argument), emote);
	}
	release_buffer(arg);
	return TRUE;
}


void ICEChannel::NotifyUpdate(void) {
	if (!this->local) {	//	saved channel?
		ICEChannel *saved;
		
		LListIterator<ICEChannel *>		iter(saved_channel_list);
		while ((saved = iter.Next())) {
			if (!str_cmp(saved->name, this->name)) {
				this->local				= new ICELChannel();
				this->local->name		= str_dup(saved->local->name);
				this->local->format1	= str_dup(saved->local->format1);
				this->local->format2	= str_dup(saved->local->format2);
				this->local->level		= saved->local->level;

				return;
			}
		}
	}
}
