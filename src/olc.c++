/***************************************************************************
 *  OasisOLC - olc.c                                                       *
 *                                                                         *
 *  Copyright 1996 Harvey Gilpin.                                          *
 ***************************************************************************/

#define _OASIS_OLC_

#include "structs.h"
#include "interpreter.h"
#include "comm.h"
#include "utils.h"
#include "scripts.h"
#include "db.h"
#include "handler.h"
#include "find.h"
#include "olc.h"
#include "clans.h"
#include "shop.h"
#include "help.h"
#include "buffer.h"
#include "socials.h"
#include "staffcmds.h"


//	External Variables
extern int top_of_zone_table;
extern struct zone_data *zone_table;


//	External Functions.
extern void zedit_setup(DescriptorData *d, RNum room_num);
extern void zedit_save_to_disk(int zone_num);
extern void zedit_new_zone(CharData *ch, int new_zone);
extern void medit_setup_new(DescriptorData *d);
extern void medit_setup_existing(DescriptorData *d, RNum rmob_num);
extern void medit_save_to_disk(int zone_num);
extern void oedit_setup_new(DescriptorData *d);
extern void oedit_setup_existing(DescriptorData *d, RNum robj_num);
extern void oedit_save_to_disk(int zone_num);
extern void sedit_setup_new(DescriptorData *d);
extern void sedit_setup_existing(DescriptorData *d, RNum robj_num);
extern void sedit_save_to_disk(int zone_num);
extern void scriptedit_setup_new(DescriptorData *d);
extern void scriptedit_setup_existing(DescriptorData *d, RNum robj_num);
extern void scriptedit_save_to_disk(int zone_num);
extern void cedit_setup_new(DescriptorData *d);
extern void cedit_setup_existing(DescriptorData *d, RNum robj_num);
extern int real_shop(VNum vnum);
extern void free_shop(struct shop_data *shop);
extern void aedit_save_to_disk(int zone_num);
extern void free_action(struct social_messg *action);
extern int find_action(int cmd);
extern void do_stat_object(CharData * ch, ObjData * j);
extern void do_stat_character(CharData * ch, CharData * k);
extern void do_stat_room(CharData * ch);
extern void hedit_save_to_disk(DescriptorData *d);
extern void free_help(HelpElement *help);
RNum	find_help_rnum(char *keyword);
extern void hedit_setup_new(DescriptorData *d, char *arg1);
extern void hedit_setup_existing(DescriptorData *d);
extern void free_clan(ClanData *clan);
void free_intlist(struct int_list *intlist);


//	Internal Functions
ACMD(do_olc);
ACMD(do_zallow);
ACMD(do_zdeny);
ACMD(do_ostat);
ACMD(do_mstat);
ACMD(do_rstat);
ACMD(do_zstat);
ACMD(do_zlist);
ACMD(do_delete);
ACMD(do_liblist);
ACMD(do_move_element);
ACMD(do_massroomsave);

int read_zone_perms(int rnum);
int real_zone(int number);
void olc_saveinfo(CharData *ch);
int get_line2(FILE * fl, char *buf);
int get_zone_rnum(int zone);
int get_zon_num(int vnum);

/*. Internal data .*/

struct olc_scmd_data {
  char *text;
  int con_type;
};

struct olc_scmd_data olc_scmd_info[] =
{ {"room"	, CON_REDIT},
  {"object"	, CON_OEDIT},
  {"room"	, CON_ZEDIT},
  {"mobile"	, CON_MEDIT},
  {"shop"	, CON_SEDIT},
  {"action"	, CON_AEDIT},
  {"help"	, CON_HEDIT},
  {"script"	, CON_SCRIPTEDIT},
  {"clan"	, CON_CEDIT},
  {"\n"		, 0		   }
};

/*------------------------------------------------------------*/

/*
 * Exported ACMD do_olc function.
 *
 * This function is the OLC interface.  It deals with all the 
 * generic OLC stuff, then passes control to the sub-olc sections.
 */
ACMD(do_olc) {
	int number = -1, save = 0, real_num;
	DescriptorData *d;
	char *arg1, *arg2, *type = "ERROR";

	if (IS_NPC(ch) || !ch->desc)	// No screwing arround
		return;

	if (subcmd == SCMD_OLC_SAVEINFO) {
		olc_saveinfo(ch);
		return;
	}
	
	arg1 = get_buffer(MAX_INPUT_LENGTH);
	arg2 = get_buffer(MAX_INPUT_LENGTH);
	
	/*. Parse any arguments .*/
	two_arguments(argument, arg1, arg2);
	
	if (!*arg1) {	//	No argument given
		switch(subcmd) {
			case SCMD_OLC_ZEDIT:
			case SCMD_OLC_REDIT:
				number = world[IN_ROOM(ch)].number;
				break;
			case SCMD_OLC_OEDIT:
			case SCMD_OLC_MEDIT:
			case SCMD_OLC_SEDIT:
			case SCMD_OLC_SCRIPTEDIT:
			case SCMD_OLC_CEDIT:
				ch->Send("Specify a %s VNUM to edit.\r\n", olc_scmd_info[subcmd].text);
				break;
			case SCMD_OLC_AEDIT:
				send_to_char("Specify an action to edit.\r\n", ch);
				break;
			case SCMD_OLC_HEDIT:
				send_to_char("Specify a help topic to edit.\r\n", ch);
				break;
		}
		if (number == -1) {
			release_buffer(arg1);
			release_buffer(arg2);
			return;
		}
	} else if (!isdigit (*arg1)) {
		if (!strncmp("save", arg1, 4)) {
			if ((subcmd == SCMD_OLC_AEDIT) || (subcmd == SCMD_OLC_HEDIT) || (subcmd == SCMD_OLC_CEDIT))  {
				save = 1;
				number = 0;
			} else if (!*arg1) {
				send_to_char("Save which zone?\r\n", ch);
				release_buffer(arg1);
				release_buffer(arg2);
				return;
			} else {
				save = 1;
				number = atoi(arg2) * 100;
			}
		} else if ((subcmd == SCMD_OLC_AEDIT) || (subcmd == SCMD_OLC_HEDIT))
			number = 0;
		else if (subcmd == SCMD_OLC_ZEDIT && GET_LEVEL(ch) >= LVL_ADMIN) {
			if (!strncmp("new", arg1, 3) && *arg2)
				zedit_new_zone(ch, atoi(arg2));
			else
				send_to_char("Specify a new zone number.\r\n", ch);
			release_buffer(arg1);
			release_buffer(arg2);
			return;
		} else {
			send_to_char ("Yikes!  Stop that, someone will get hurt!\r\n", ch);
			release_buffer(arg1);
			release_buffer(arg2);
			return;
		}
	}

	/*. If a numeric argument was given, get it .*/
	if ((number == -1) && (subcmd != SCMD_OLC_AEDIT) && (subcmd != SCMD_OLC_HEDIT))
		number = atoi(arg1);

	release_buffer(arg2);
	
	/*. Check whatever it is isn't already being edited .*/
	START_ITER(iter, d, DescriptorData *, descriptor_list) {
		if (STATE(d) == olc_scmd_info[subcmd].con_type)
			if ((number != -1) && d->olc && (OLC_NUM(d) == number)) {
				if (subcmd == SCMD_OLC_AEDIT)
					ch->Send("Actions are already being editted by %s.\r\n",
							(ch->CanSee(d->character) ? d->character->RealName() : "someone"));
				else if (subcmd == SCMD_OLC_HEDIT)
					ch->Send("Help files are currently being edited by %s\r\n", 
							(ch->CanSee(d->character) ? d->character->RealName() : "someone"));
				else
					ch->Send("That %s is currently being edited by %s.\r\n",
							olc_scmd_info[subcmd].text,
							(ch->CanSee(d->character) ? d->character->RealName() : "someone"));
				release_buffer(arg1);
				END_ITER(iter);
				return;
			}
	} END_ITER(iter);
	
	d = ch->desc;

	/*. Give descriptor an OLC struct .*/
	CREATE(d->olc, struct olc_data, 1);

	/*. Find the zone .*/
	if (subcmd == SCMD_OLC_HEDIT)
		OLC_ZNUM(d) = find_help_rnum(arg1);
	else if ((subcmd != SCMD_OLC_AEDIT) && (subcmd != SCMD_OLC_CEDIT)) {
		if ((OLC_ZNUM(d) = real_zone(number)) == -1) {
			send_to_char ("Sorry, there is no zone for that number!\r\n", ch); 
			FREE(d->olc);
			release_buffer(arg1);
			return;
		}

		/*. Everyone but IMPLs can only edit zones they have been assigned .*/
		if (!get_zone_perms(ch, OLC_ZNUM(d))) {
			FREE(d->olc);
			release_buffer(arg1);
			return;
		}
	}
/*
	else if (GET_LEVEL(ch) < LVL_SRSTAFF) {
		if (subcmd == SCMD_OLC_AEDIT)
			send_to_char("You do not have permission to edit actions.\r\n", ch);
		else if (subcmd == SCMD_OLC_HEDIT)
			send_to_char("You do not have permission to edit help files.\r\n", ch);
		else if (subcmd == SCMD_OLC_CEDIT)
			send_to_char("You do not have permission to edit clans.\r\n", ch);
		else
			send_to_char("You do not have permission to edit something or other...\r\n", ch);
		FREE(d->olc);
		release_buffer(arg1);
		return;
	}
*/

	if(save) {
		switch(subcmd) {
			case SCMD_OLC_REDIT: type = "room";	break;
			case SCMD_OLC_ZEDIT: type = "zone";	break;
			case SCMD_OLC_SEDIT: type = "shop"; break;
			case SCMD_OLC_MEDIT: type = "mobile"; break;
			case SCMD_OLC_OEDIT: type = "object"; break;
			case SCMD_OLC_SCRIPTEDIT: type = "script"; break;
			case SCMD_OLC_AEDIT:
				send_to_char("Saving all actions.\r\n", ch);
				mudlogf(NRM, LVL_SRSTAFF, TRUE, "OLC: %s saves all actions", ch->RealName());
				break;
			case SCMD_OLC_HEDIT:
				send_to_char("Saving all help files.\r\n", ch);
				mudlogf(NRM, LVL_SRSTAFF, TRUE, "OLC: %s saves all help files", ch->RealName());
				break;
			case SCMD_OLC_CEDIT:
				send_to_char("Saving all clans.\r\n", ch);
				mudlogf(NRM, LVL_SRSTAFF, TRUE, "OLC: %s saves all clans", ch->RealName());
				break;
		}
		if ((subcmd != SCMD_OLC_AEDIT) && (subcmd != SCMD_OLC_HEDIT) && (subcmd != SCMD_OLC_CEDIT)) {
			if (!type) {
				send_to_char("Oops, I forgot what you wanted to save.\r\n", ch);
				release_buffer(arg1);
				return;
			}
			ch->Send("Saving all %ss in zone %d.\r\n", type, zone_table[OLC_ZNUM(d)].number);
			mudlogf(NRM, MAX(LVL_BUILDER, GET_INVIS_LEV(ch)), TRUE, "OLC: %s saves %s info for zone %d.",
					ch->RealName(), type, zone_table[OLC_ZNUM(d)].number);
		}
		switch (subcmd) {
			case SCMD_OLC_REDIT: REdit::save_to_disk(OLC_ZNUM(d)); break;
			case SCMD_OLC_ZEDIT: zedit_save_to_disk(OLC_ZNUM(d)); break;
			case SCMD_OLC_OEDIT: oedit_save_to_disk(OLC_ZNUM(d)); break;
			case SCMD_OLC_MEDIT: medit_save_to_disk(OLC_ZNUM(d)); break;
			case SCMD_OLC_SEDIT: sedit_save_to_disk(OLC_ZNUM(d)); break;
			case SCMD_OLC_AEDIT: aedit_save_to_disk(OLC_ZNUM(d)); break;
			case SCMD_OLC_HEDIT: hedit_save_to_disk(d); break;
			case SCMD_OLC_CEDIT: clans_save_to_disk(); break;
			case SCMD_OLC_SCRIPTEDIT: scriptedit_save_to_disk(OLC_ZNUM(d)); break;
		}
		FREE(d->olc);
		release_buffer(arg1);
		return;
	}
 
	if (subcmd != SCMD_OLC_AEDIT)	OLC_NUM(d) = number;
	else if (subcmd == SCMD_OLC_AEDIT) {
		OLC_NUM(d) = 0;
		OLC_STORAGE(d) = str_dup(arg1);
		for (OLC_ZNUM(d) = 0; (OLC_ZNUM(d) <= top_of_socialt); OLC_ZNUM(d)++)  {
			if (is_abbrev(OLC_STORAGE(d), soc_mess_list[OLC_ZNUM(d)].command))
				break;
		}
		if (OLC_ZNUM(d) > top_of_socialt)  {
			if (find_command(OLC_STORAGE(d)) > NOTHING)  {
				cleanup_olc(d, CLEANUP_ALL);
				send_to_char("That command already exists.\r\n", ch);
				release_buffer(arg1);
				return;
			}
			ch->Send("Do you wish to add the '%s' action? ", OLC_STORAGE(d));
			OLC_MODE(d) = AEDIT_CONFIRM_ADD;
		} else {
			ch->Send("Do you wish to edit the '%s' action? ", soc_mess_list[OLC_ZNUM(d)].command);
			OLC_MODE(d) = AEDIT_CONFIRM_EDIT;
		}
	}

	/*. Steal players descriptor start up subcommands .*/
	switch(subcmd) {
		case SCMD_OLC_REDIT:
			if ((real_num = real_room(number)) >= 0)	REdit::setup_existing(d, real_num);
			else										REdit::setup_new(d);
			STATE(d) = CON_REDIT;
			break;
		case SCMD_OLC_ZEDIT:
			if ((real_num = real_room(number)) < 0) {
				send_to_char("That room does not exist.\r\n", ch); 
				FREE(d->olc);
				release_buffer(arg1);
				return;
			}
			zedit_setup(d, real_num);
			STATE(d) = CON_ZEDIT;
			break;
		case SCMD_OLC_MEDIT:
			if ((real_num = real_mobile(number)) >= 0)	medit_setup_existing(d, real_num);
			else										medit_setup_new(d);
			STATE(d) = CON_MEDIT;
			break;
		case SCMD_OLC_OEDIT:
			if ((real_num = real_object(number)) >= 0)	oedit_setup_existing(d, real_num);
			else										oedit_setup_new(d);
			STATE(d) = CON_OEDIT;
			break;
		case SCMD_OLC_SEDIT:
			if ((real_num = real_shop(number)) >= 0)	sedit_setup_existing(d, real_num);
			else										sedit_setup_new(d);
			STATE(d) = CON_SEDIT;
			break;
		case SCMD_OLC_AEDIT:
			STATE(d) = CON_AEDIT;
			break;
		case SCMD_OLC_HEDIT:
			if (OLC_ZNUM(d) >= 0)						hedit_setup_existing(d);
			else										hedit_setup_new(d, arg1);
			STATE(d) = CON_HEDIT;
			break;
		case SCMD_OLC_SCRIPTEDIT:
			if ((real_num = real_trigger(number)) >= 0)	scriptedit_setup_existing(d, real_num);
			else										scriptedit_setup_new(d);
			STATE(d) = CON_SCRIPTEDIT;
			break;
		case SCMD_OLC_CEDIT:
			if ((real_num = real_clan(number)) >= 0)	cedit_setup_existing(d, real_num);
			else										cedit_setup_new(d);
			STATE(d) = CON_CEDIT;
			break;
	}
	release_buffer(arg1);
	act("$n starts using OLC.", TRUE, d->character, 0, 0, TO_ROOM);
	SET_BIT(PLR_FLAGS(ch), PLR_WRITING);
}
/*------------------------------------------------------------*\
 Internal utilities 
\*------------------------------------------------------------*/

void olc_saveinfo(CharData *ch) {
	struct olc_save_info *entry;
	static char *save_info_msg[] = {
		"Rooms", "Objects", "Zone info", "Mobiles", "Shops", "Actions", "Help", "Scripts", "Clans"
	};
	char *buf;

	if (olc_save_list)	send_to_char("The following OLC components need saving:-\r\n", ch);
	else				send_to_char("The database is up to date.\r\n", ch);

	buf = get_buffer(MAX_INPUT_LENGTH);
	for (entry = olc_save_list; entry; entry = entry->next) {
		if (entry->type != SCMD_OLC_AEDIT && entry->type != SCMD_OLC_HEDIT && entry->type != SCMD_OLC_CEDIT)
			sprintf(buf, " - %s for zone %d.\r\n", save_info_msg[(int)entry->type], entry->zone);
		else
			sprintf(buf, " - %s.\r\n", save_info_msg[(int)entry->type]);
		send_to_char(buf, ch);
	}
	release_buffer(buf);
}


int real_zone(int number) {
	int counter;
	for (counter = 0; counter <= top_of_zone_table; counter++)
		if ((number >= (zone_table[counter].number * 100)) &&
				(number <= (zone_table[counter].top)))
			return counter;

	return -1;
}

/*------------------------------------------------------------*\
 Exported utilities 
\*------------------------------------------------------------*/

/*
 * Add an entry to the 'to be saved' list.
 */
void olc_add_to_save_list(int zone, UInt8 type) {
	struct olc_save_info *new_info;

	/*
	 * Return if it's already in the list
	 */
	for(new_info = olc_save_list; new_info; new_info = new_info->next)
		if ((new_info->zone == zone) && (new_info->type == type))
			return;

	CREATE(new_info, struct olc_save_info, 1);
	new_info->zone = zone;
	new_info->type = type;
	new_info->next = olc_save_list;
	olc_save_list = new_info;
}

/*
 * Remove an entry from the 'to be saved' list
 */
void olc_remove_from_save_list(int zone, UInt8 type) {
	struct olc_save_info **entry;
	struct olc_save_info *temp;

	for (entry = &olc_save_list; *entry; entry = &(*entry)->next)
		if (((*entry)->zone == zone) && ((*entry)->type == type)) {
			temp = *entry;
			*entry = temp->next;
			FREE(temp);
			return;
		}
}


/*
 * This procedure removes the '\r\n' from a string so that it may be
 * saved to a file.  Use it only on buffers, not on the original
 * strings.
 */
void strip_string(char *buffer) {
	char *ptr, *str;

	str = ptr = buffer;

	while((*str++ = *ptr++)) {
//		str++;
//		ptr++;
		if (*ptr == '\r')
			ptr++;
	}
}


/*. This procdure frees up the strings and/or the structures
    attatched to a descriptor, sets all flags back to how they
    should be .*/

void cleanup_olc(DescriptorData *d, UInt8 cleanup_type) {
	if (d->olc) {
		//	Check for storage -- aedit patch -- M. Scott
		if (OLC_STORAGE(d))		FREE(OLC_STORAGE(d));
		if (OLC_INTLIST(d))		free_intlist(OLC_INTLIST(d));
		if (OLC_CLAN(d))		free_clan(OLC_CLAN(d));
		if (OLC_OBJ(d))			delete OLC_OBJ(d);
		if (OLC_MOB(d))			delete OLC_MOB(d);
		if (OLC_TRIG(d))		delete OLC_TRIG(d);

		//	Check for a room.
		if(OLC_ROOM(d)) {
			switch(cleanup_type) {
				case CLEANUP_ALL:		delete OLC_ROOM(d);				break;
				case CLEANUP_STRUCTS:	REdit::free_room(OLC_ROOM(d));	break;
				default:	/* Caller has screwed up */					break;
			}
		}
		
		//	Check for zone
		if(OLC_ZONE(d)) {
			//	cleanup_type is irrelevant here, FREE() everything.
			FREE(OLC_ZONE(d)->name);
			FREE(OLC_ZONE(d)->cmd);
			FREE(OLC_ZONE(d));
		}

		//	Check for shop
		if(OLC_SHOP(d)) {
			//	free_shop doesn't perform sanity checks, we must be careful here.
			switch(cleanup_type) {
				case CLEANUP_ALL:		free_shop(OLC_SHOP(d));	break;
				case CLEANUP_STRUCTS:	FREE(OLC_SHOP(d));		break;
				default:										break;
			}
		}

		//	Check for aedit stuff -- M. Scott
		if (OLC_ACTION(d))  {
			switch(cleanup_type)  {
				case CLEANUP_ALL:		free_action(OLC_ACTION(d));	break;
				case CLEANUP_STRUCTS:	FREE(OLC_ACTION(d));		break;
				default:											break;
			}
		}

		if (OLC_HELP(d)) {
			switch (cleanup_type) {
				case CLEANUP_ALL:		delete OLC_HELP(d);			break;
				case CLEANUP_STRUCTS:	free_help(OLC_HELP(d));		break;
				default:											break;
			}
		}
		
		//	Restore desciptor playing status
		if (d->character) {
			REMOVE_BIT(PLR_FLAGS(d->character), PLR_WRITING);
			STATE(d) = CON_PLAYING;
			act("$n stops using OLC.", TRUE, d->character, 0, 0, TO_ROOM);
		}
		FREE(d->olc);
		d->olc = NULL;
	}
}


/* read builders from zone file into memory */
int read_zone_perms(int rnum) {
	FILE *old_file;
	char *arg1, *arg2;
	char *buf = get_buffer(MAX_INPUT_LENGTH);

	sprintf(buf, ZON_PREFIX "%i.zon", zone_table[rnum].number);

	if(!(old_file = fopen(buf, "r"))) {
		log("SYSERR: Could not open %s for read.", buf);
		release_buffer(buf);
		return(FALSE);
	}
	arg1 = get_buffer(MAX_INPUT_LENGTH);
	arg2 = get_buffer(MAX_INPUT_LENGTH);
	do {
		get_line2(old_file, buf);
		if(*buf == '*') {
			half_chop(buf+1, arg1, arg2);
			if (*arg1 && *arg2 && is_abbrev(arg1, "Builder"))
				zone_table[rnum].builders.Append(str_dup(arg2));
		}
	} while(*buf != '$' && !feof(old_file));
	release_buffer(buf);
	release_buffer(arg1);
	release_buffer(arg2);
	
	fclose(old_file);
	
	return(TRUE);
}


ACMD(do_zallow) {
	int zone, rnum = -1;
	FILE *old_file, *new_file;
	char *old_fname, *new_fname, *line;
	char	*arg1 = get_buffer(MAX_INPUT_LENGTH),
			*arg2 = get_buffer(MAX_INPUT_LENGTH),
			*bldr;

	half_chop(argument, arg1, arg2);

	if(!*arg1 || !*arg2 || !isdigit(*arg2)) {
		send_to_char("Usage  : zallow <player> <zone>\r\n", ch);
		release_buffer(arg1);
		release_buffer(arg2);
		return;
	}
	*arg1 = UPPER(*arg1);
	zone = atoi(arg2);
	release_buffer(arg2);
	if((rnum = get_zone_rnum(zone)) == -1) {
		send_to_char("That zone doesn't seem to exist.\r\n", ch);
		release_buffer(arg1);
		return;
	}

//	if(zone_table[rnum].builders == NULL)
//		read_zone_perms(rnum);

	LListIterator<char *>	iter(zone_table[rnum].builders);
	while ((bldr = iter.Next())) {
		if(bldr && !str_cmp(bldr, arg1)) {
			send_to_char("That person already has access to that zone.\r\n", ch);	
			release_buffer(arg1);
			return;
		}
	}

	zone_table[rnum].builders.Add(str_dup(arg1));
	
	old_fname = get_buffer(256);
	new_fname = get_buffer(256);

	sprintf(old_fname, ZON_PREFIX SEPERATOR "%i.zon", zone);
	sprintf(new_fname, ZON_PREFIX SEPERATOR "%i.zon.temp", zone);
	
	if(!(old_file = fopen(old_fname, "r"))) {
		ch->Send("Error: Could not open %s for read.\r\n", old_fname);
		log("SYSERR: Could not open %s for read.", old_fname);
		release_buffer(old_fname);
		release_buffer(new_fname);
		release_buffer(arg1);
		return;
	}
	if(!(new_file = fopen(new_fname, "w"))) {
		ch->Send("Error: Could not open %s for write.\r\n", new_fname);
		log("SYSERR: Could not open %s for write.", new_fname);
		fclose(old_file);
		release_buffer(old_fname);
		release_buffer(new_fname);
		release_buffer(arg1);
		return;
	}
	line = get_buffer(256);
	get_line2(old_file, line);
	fprintf(new_file, "%s\n", line);
	get_line2(old_file, line);
	fprintf(new_file, "%s\n", line);
	get_line2(old_file, line);
	fprintf(new_file, "%s\n", line);
	fprintf(new_file, "* Builder %s\n", arg1);
	do {
		get_line2(old_file, line);
		fprintf(new_file, "%s\n", line);
	} while(*line != '$');
	release_buffer(line);
	fclose(old_file);
	fclose(new_file);
	remove(old_fname);
	rename(new_fname, old_fname);
	send_to_char("Zone file edited.\r\n", ch);
	mudlogf(BRF, LVL_STAFF, TRUE, "olc: %s gives %s zone #%i builder access.", ch->RealName(), arg1, zone);

	release_buffer(arg1);
	release_buffer(old_fname);
	release_buffer(new_fname);
}


ACMD(do_zdeny) {
	int zone, rnum, found = FALSE;
	FILE *old_file, *new_file;
	char *old_fname, *new_fname, *line;
	char *arg1, *arg2, *arg3;
	char *bldr;

	arg1 = get_buffer(MAX_INPUT_LENGTH);
	arg2 = get_buffer(MAX_INPUT_LENGTH);
	half_chop(argument, arg1, arg2);

	if(!*arg1 || !*arg2 || !isdigit(*arg2)) {
		send_to_char("Usage  : zdeny <player> <zone>\r\n", ch);
		release_buffer(arg1);
		release_buffer(arg2);
		return;
	}
	*arg1 = UPPER(*arg1);
	zone = atoi(arg2);
	release_buffer(arg2);
	if((rnum = get_zone_rnum(zone)) == -1) {
		send_to_char("That zone doesn't seem to exist.\r\n", ch);
		release_buffer(arg1);
		return;
	}

//	if(zone_table[rnum].builders == NULL)
//		read_zone_perms(rnum);

	LListIterator<char *>	iter(zone_table[rnum].builders);
	
	while ((bldr = iter.Next())) {
		if(!str_cmp(bldr, arg1)) {
			zone_table[rnum].builders.Remove(bldr);
			FREE(bldr);
			found = TRUE;
		}
	}

	if(!found) {
		send_to_char("That person doesn't have access to that zone.\r\n", ch);
		release_buffer(arg1);
		return;
	}

	old_fname = get_buffer(256);
	new_fname = get_buffer(256);
	
	sprintf(old_fname, ZON_PREFIX "%i.zon", zone);
	sprintf(new_fname, ZON_PREFIX "%i.zon.temp", zone);

	if(!(old_file = fopen(old_fname, "r"))) {
		ch->Send("Error: Could not open %s for read.\r\n", old_fname);
		log("SYSERR: Could not open %s for read.", old_fname);
		release_buffer(old_fname);
		release_buffer(new_fname);
		return;
	}
	if(!(new_file = fopen(new_fname, "w"))) {
		ch->Send("Error: Could not open %s for write.\r\n", new_fname);
		log("SYSERR: Could not open %s for write.", new_fname);
		fclose(old_file);
		release_buffer(old_fname);
		release_buffer(new_fname);
		return;
	}
	
	line = get_buffer(MAX_STRING_LENGTH);
	arg2 = get_buffer(MAX_INPUT_LENGTH);
	arg3 = get_buffer(MAX_INPUT_LENGTH);
	do {
		get_line2(old_file, line);
		if(*line == '*') {
			two_arguments(line + 1, arg2, arg3);
			if(!*arg2 || str_cmp(arg2, "Builder") || str_cmp(arg1, arg3))
				fprintf(new_file, "%s\n", line);
		} else	fprintf(new_file, "%s\n", line);
	} while(*line != '$');
	release_buffer(line);
	release_buffer(arg2);
	release_buffer(arg3);
	
	fclose(old_file);
	fclose(new_file);
	remove(old_fname);
	rename(new_fname, old_fname);
	send_to_char("Zone file edited.\r\n", ch);
	mudlogf(BRF, LVL_STAFF, TRUE, "olc: %s removes %s's zone #%i builder access.", ch->RealName(), arg1, zone);

	release_buffer(arg1);
	release_buffer(old_fname);
	release_buffer(new_fname);
}


bool get_zone_perms(CharData * ch, int rnum) {
	int allowed = FALSE /*, rnum*/;
	char *bldr;
	
	if(STF_FLAGGED(ch, STAFF_OLCADMIN))
		return true;

//	if(zone_table[rnum].builders == NULL)
//		read_zone_perms(rnum);

    /* check memory */
//	for(bldr = zone_table[rnum].builders; bldr; bldr = bldr->next)
	LListIterator<char *>	iter(zone_table[rnum].builders);
	while ((bldr = iter.Next()))
		if(!str_cmp(SSData(ch->general.name), bldr))
			return true;

	ch->Send("You don't have builder rights in zone #%i\r\n", zone_table[rnum].number);
	return false;
}


ACMD(do_ostat) {
	ObjData *obj;
	int number, r_num;
	
	skip_spaces(argument);
	
	if(*argument && isdigit(*argument)) {
		if ((number = atoi(argument)) < 0)
			send_to_char("A NEGATIVE number??\r\n", ch);
		else if ((r_num = real_object(number)) < 0)
			send_to_char("There is no object with that number.\r\n", ch);
		else {
			obj = read_object(r_num, REAL);
			do_stat_object(ch, obj);
		    obj->extract();
		}
	} else
		send_to_char("Usage: ostat [vnum]\r\n", ch);
}


ACMD(do_mstat) {
	CharData *mob;
	int number, r_num;
	
	skip_spaces(argument);
	
	if(*argument && isdigit(*argument)) {
		if ((number = atoi(argument)) < 0)
			send_to_char("A NEGATIVE number??\r\n", ch);
		else if ((r_num = real_mobile(number)) < 0)
			send_to_char("There is no mob with that number.\r\n", ch);
		else {
			mob = read_mobile(r_num, REAL);
			mob->to_room(0);
			do_stat_character(ch, mob);
			mob->extract();
		}
	} else
		send_to_char("Usage: mstat [vnum]\r\n", ch);
}



ACMD(do_rstat) {
	int number, r_num, room_temp;
	
	skip_spaces(argument);

	if(*argument) {
		if (!isdigit(*argument))
			send_to_char("Usage: rstat [vnum]\r\n", ch);
		else if ((number = atoi(argument)) < 0)
			send_to_char("A NEGATIVE number??\r\n", ch);
		else if ((r_num = real_room(number)) < 0)
			send_to_char("There is no room with that number.\r\n", ch);
		else {
			room_temp = IN_ROOM(ch);
			ch->from_room();
			ch->to_room(r_num);
			do_stat_room(ch);
			ch->from_room();
			ch->to_room(room_temp);
		}
	} else
		do_stat_room(ch);
}



ACMD(do_zstat) {
	int i, zn = -1, pos = 0, none = TRUE;
	char *bldr;
	char *rmode[] = {	"Never resets",
						"Resets only when deserted",
						"Always resets"  };

	skip_spaces(argument);

	if(*argument) {
		if(isdigit(*argument))
			zn = atoi(argument);
		else {
			send_to_char("Usage: zstat [zone number]\r\n", ch);
			return;
		}
	} else zn = get_zon_num(world[IN_ROOM(ch)].number);

	if(zn < 0) {
		log("SYSERR: Zone #%d doesn't seem to exist.", world[IN_ROOM(ch)].number / 100);
		send_to_char("Warning!  The room you're in doesn't belong to any zone in memory.\r\n", ch);
		return;
	}

	for(i = 0; i <= top_of_zone_table && zone_table[i].number != zn; i++);
	if(i > top_of_zone_table) {
		send_to_char("That zone doesn't exist.\r\n", ch);
		return;
	}

	if((zone_table[i].reset_mode < 0) || (zone_table[i].reset_mode > 2))
		zone_table[i].reset_mode = 2;

	ch->Send("`mZone #      : `w%3d       `mName      : `w%s\r\n"
			 "`mAge/Lifespan: `w%3d`m/`w%3d   `mTop vnum  : `w%5d\r\n"
			 "`mReset mode  : `w%s\r\n"
			 "`mBuilders    : `w",
			zone_table[i].number, zone_table[i].name,
			zone_table[i].age, zone_table[i].lifespan, zone_table[i].top,
			rmode[zone_table[i].reset_mode]);

	LListIterator<char *>	iter(zone_table[i].builders);
	while ((bldr = iter.Next())) {
		if(*(bldr) != '*') {
			none = FALSE;
			switch (pos) {
				case 0:
					send_to_char(bldr, ch);
					pos = 2;
					break;
				case 1:
					ch->Send("\r\n              %s", bldr);
					pos = 2;
					break;
				case 2:
					ch->Send(", %s", bldr);
					pos = 3;
					break;
				case 3:
					ch->Send(", %s", bldr);
					pos = 1;
					break;
			}
		}
	}

	if(none)	send_to_char("NONE", ch);
	send_to_char("`n\r\n", ch);
}


/* return zone rnum */
int get_zone_rnum(int zone) {
	int i;

	for(i = 0; i <= top_of_zone_table && zone_table[i].number != zone; i++);
	if(i > top_of_zone_table)	return -1;
	else						return i;
}


ACMD(do_zlist) {
	int i, found = FALSE, rnum, zone, zn;
	char *buf, *arg = get_buffer(MAX_INPUT_LENGTH);

	one_argument(argument, arg);

	if(!*arg) {
		send_to_char("Usage: zlist mobs|objects|rooms\r\n", ch);
		release_buffer(arg);
		return;
	}

	zone = get_zon_num(world[IN_ROOM(ch)].number);
	zn = get_zone_rnum(zone);
	buf = get_buffer(1024 * 32);
	
	if(is_abbrev(arg, "mobs")) {
		sprintf(buf, "Mobs in zone #%d:\r\n\r\n", zone);
		for(i = zone * 100; i <= zone_table[zn].top; i++) {
			if((rnum = real_mobile(i)) >=0) {
				sprintf(buf + strlen(buf), "%5d - %s\r\n", i,
						SSData(((CharData *)mob_index[rnum].proto)->general.shortDesc) ?
						SSData(((CharData *)mob_index[rnum].proto)->general.shortDesc) : "Unnamed");
				found = TRUE;
			}
		}
	} else if(is_abbrev(arg, "objects")) {
		sprintf(buf, "Objects in zone #%d:\r\n\r\n", zone);
		for(i = zone * 100; i <= zone_table[zn].top; i++) {
			if((rnum = real_object(i)) >=0) {
				sprintf(buf + strlen(buf), "%5d - %s\r\n", i,
					SSData(((ObjData *)obj_index[rnum].proto)->shortDesc) ?
					SSData(((ObjData *)obj_index[rnum].proto)->shortDesc) : "Unnamed");
				found = TRUE;
			}
		}
	} else if(is_abbrev(arg, "rooms")) {
		sprintf(buf, "Rooms in zone #%d:\r\n\r\n", zone);
		for(i = zone * 100; i <= zone_table[zn].top; i++) {
			if((rnum = real_room(i)) >=0) {
				sprintf(buf + strlen(buf), "%5d - %s\r\n", i, world[rnum].GetName("Unnamed"));
				found = TRUE;
			}
		}
	} else if (is_abbrev(arg, "triggers")) {
		sprintf(buf, "Triggers in zone #%d:\r\n\r\n", zone);
		for (i = zone * 100; i <= zone_table[zn].top; i++) {
			if ((rnum = real_trigger(i)) >= 0) {
				sprintf(buf + strlen(buf), "%5d - %s\r\n", i,
					SSData(((TrigData *)trig_index[rnum].proto)->name) ?
					SSData(((TrigData *)trig_index[rnum].proto)->name) : "Unnamed");
				found = TRUE;
			}
		}
	} else {
		strcpy(buf, "Invalid argument.\r\n"
					"Usage: zlist mobs|objects|rooms|triggers\r\n");
		found = true;
	}

	if (!found) strcat(buf, "None!\r\n");

	page_string(ch->desc, buf, 1);
	
	release_buffer(arg);
	release_buffer(buf);
}


int get_line2(FILE * fl, char *buf) {
	char *temp = get_buffer(256);
	int lines = 0;

	do {
		lines++;
		fgets(temp, 256, fl);
		if (*temp)	temp[strlen(temp) - 1] = '\0';
	} while (!feof(fl) && (!*temp));

	if (feof(fl)) {
		release_buffer(temp);
		return 0;
	} else {
		strcpy(buf, temp);
		release_buffer(temp);
		return lines;
	}
}


/* check for true zone number */
int get_zon_num(int vnum) {
	int i, zn = -1, zone;

	zone = vnum / 100;

	for(i = 0; i <= top_of_zone_table && zone_table[i].number <= zone; i++);
		if(i <= top_of_zone_table || zone_table[i - 1].top >= vnum)
			zn = zone_table[i - 1].number;

	return zn;
}


ACMD(do_delete) {
	CharData *victim = NULL;
	ObjData *object = NULL;
	RoomData *room = NULL;
	TrigData *trigger = NULL;
	int which, zone;
	char	*arg1, *arg2;
	
	if (subcmd != SCMD_DELETE && subcmd != SCMD_UNDELETE) {
		send_to_char("ERROR: Unknown subcmd to do_delete", ch);
		return;
	}
	arg1 = get_buffer(MAX_INPUT_LENGTH);
	arg2 = get_buffer(MAX_INPUT_LENGTH);
	
	two_arguments(argument, arg1, arg2);
	
	if (!*arg1 || !*arg2 || (which = atoi(arg2)) <= 0) {
		if (subcmd == SCMD_DELETE)
			send_to_char("Usage: delete mob|obj|room <vnum>\r\n", ch);
		else if (subcmd == SCMD_UNDELETE)
			send_to_char("Usage: undelete mob|obj|room vnum\r\n", ch);
	} else if ((zone = real_zone(which)) == -1)
		send_to_char("That number is not valid.\r\n", ch);
	else if (GET_LEVEL(ch) < LVL_ADMIN && !get_zone_perms(ch, zone))
		send_to_char("You don't have permission to (un)delete that.\r\n", ch);
	else if (is_abbrev(arg1, "room")) {
		which = real_room(which);
		if (which == NOWHERE)
			send_to_char("That is not a valid room number.\r\n",ch);
		else {
			if (subcmd == SCMD_DELETE)	SET_BIT(ROOM_FLAGS(which), ROOM_DELETED);
			else						REMOVE_BIT(ROOM_FLAGS(which), ROOM_DELETED);
			mudlogf(NRM, LVL_BUILDER, TRUE, "OLC: %s marks room %d as %sdeleted.",
					ch->RealName(), world[which].number, subcmd == SCMD_DELETE ? "" : "un");
			olc_add_to_save_list(zone_table[zone].number, OLC_SAVE_ROOM);
		}
	} else if (is_abbrev(arg1, "mob")) {
		which = real_mobile(which);
		if (which == NOBODY)
			send_to_char("That is not a valid mobile number.\r\n",ch);
		else {
			victim = (CharData *)mob_index[which].proto;
			if (subcmd == SCMD_DELETE)	SET_BIT(MOB_FLAGS(victim), MOB_DELETED);
			else						REMOVE_BIT(MOB_FLAGS(victim), MOB_DELETED);
			mudlogf(NRM, LVL_BUILDER, TRUE, "OLC: %s marks mob %d as %sdeleted.",
					ch->RealName(), GET_MOB_VNUM(victim), subcmd == SCMD_DELETE ? "" : "un");
			olc_add_to_save_list(zone_table[zone].number, OLC_SAVE_MOB);
		}
	} else if (is_abbrev(arg1, "object")) {
		which = real_object(which);
		if (which == NOTHING)
			send_to_char("That is not a valid object number.\r\n",ch);
		else {
			object = (ObjData *)obj_index[which].proto;
			if (subcmd == SCMD_DELETE)	SET_BIT(GET_OBJ_EXTRA(object), ITEM_DELETED);
			else						REMOVE_BIT(GET_OBJ_EXTRA(object), ITEM_DELETED);
			mudlogf(NRM, LVL_BUILDER, TRUE, "OLC: %s marks object %d as %sdeleted.",
					ch->RealName(), GET_OBJ_VNUM(object), subcmd == SCMD_DELETE ? "" : "un");
			olc_add_to_save_list(zone_table[zone].number, OLC_SAVE_OBJ);
		}
	} else if (is_abbrev(arg1, "trigger") || is_abbrev(arg1, "script")) {
		which = real_trigger(which);
		if (which == NOTHING)
			send_to_char("That is not a valid trigger number.\r\n",ch);
		else {
			trigger = (TrigData *)trig_index[which].proto;
			if (subcmd == SCMD_DELETE)	SET_BIT(GET_TRIG_TYPE(trigger), TRIG_DELETED);
			else						REMOVE_BIT(GET_TRIG_TYPE(trigger), TRIG_DELETED);
			mudlogf(NRM, LVL_BUILDER, TRUE, "OLC: %s marks trigger %d as %sdeleted.",
					ch->RealName(), GET_TRIG_VNUM(trigger), subcmd == SCMD_DELETE ? "" : "un");
			olc_add_to_save_list(zone_table[zone].number, OLC_SAVE_TRIGGER);
		}
	} else if (subcmd == SCMD_DELETE)
		send_to_char("Usage: delete mob|obj|room|trigger <vnum>\r\n", ch);
	else if (subcmd == SCMD_UNDELETE)
		send_to_char("Usage: undelete mob|obj|room|trigger <vnum>\r\n", ch);
	else
		mudlogf(NRM, LVL_BUILDER, TRUE, "SYSERR: Reached default ELSE in do_delete!  Builder: %s  Argument: %s", ch->RealName(), argument);

	release_buffer(arg1);
	release_buffer(arg2);
}


ACMD(do_liblist) {	
	char	*buf = get_buffer(MAX_INPUT_LENGTH);
	char	*buf2 = get_buffer(MAX_INPUT_LENGTH);
	RNum	nr;
	VNum	vnum;
	int first, last, found = 0;

	two_arguments(argument, buf, buf2);

	if (!*buf || !*buf2) {
		switch (subcmd) {
			case SCMD_RLIST:
				send_to_char("Usage: rlist <begining number> <ending number>\r\n", ch);
				break;
			case SCMD_OLIST:
				send_to_char("Usage: olist <begining number> <ending number>\r\n", ch);
				break;
			case SCMD_MLIST:
				send_to_char("Usage: mlist <begining number> <ending number>\r\n", ch);
				break;
			case SCMD_TLIST:
				send_to_char("Usage: tlist <begining number> <ending number>\r\n", ch);
				break;
			default:
				mudlogf(BRF, LVL_STAFF, TRUE, "SYSERR:: invalid SCMD passed to do_liblist!  (SCMD = %d)", subcmd);
				break;
		}
		release_buffer(buf);
		release_buffer(buf2);
		return;
	}

	first = atoi(buf);
	last = atoi(buf2);

	release_buffer(buf2);
	release_buffer(buf);
	
	if ((first < 0) || (first > 99999) || (last < 0) || (last > 99999)) {
		send_to_char("Values must be between 0 and 99999.\r\n", ch);
		return;
	}

	if (first >= last) {
		send_to_char("Second value must be greater than first.\r\n", ch);
		return;
	}
	
	if((first+300) <= last){
		send_to_char("May not exceed 300 items, try using a smaller end number!\r\n" ,ch);
		return;
	}
	
	buf = get_buffer(LARGE_BUFSIZE);
	
	switch (subcmd) {
		case SCMD_RLIST:
			sprintf(buf, "Room List From Vnum %d to %d\r\n", first, last);
			for (vnum = first; vnum <= last; vnum++) {
				if ((nr = real_room(vnum)) >= 0) {
					sprintf(buf + strlen(buf), "%5d. [%5d] (%3d) %s\r\n", ++found,
							world[nr].number, world[nr].zone, world[nr].GetName("<ERROR>"));
				}
			}
			break;
		case SCMD_OLIST:
			sprintf(buf, "Object List From Vnum %d to %d\r\n", first, last);
			for (vnum = first; vnum <= last; vnum++) {
				if ((nr = real_object(vnum)) >= 0) {
					sprintf(buf + strlen(buf), "%5d. [%5d] %s\r\n", ++found,
							obj_index[nr].vnum, SSData(((ObjData *)obj_index[nr].proto)->shortDesc));
				}
			}
			break;
		case SCMD_MLIST:
			sprintf(buf, "Mob List From Vnum %d to %d\r\n", first, last);
			for (vnum = first; vnum <= last; vnum++) {
				if ((nr = real_mobile(vnum)) >= 0) {
					sprintf(buf + strlen(buf), "%5d. [%5d] %s\r\n", ++found,
							mob_index[nr].vnum, SSData(((CharData *)mob_index[nr].proto)->general.shortDesc));
				}
			}
			break;
		case SCMD_TLIST:
			sprintf(buf, "Trigger List From Vnum %d to %d\r\n", first, last);
			for (vnum = first; vnum <= last; vnum++) {
				if ((nr = real_trigger(vnum)) >= 0) {
					sprintf(buf + strlen(buf), "%5d. [%5d] %s\r\n", ++found,
							trig_index[nr].vnum, SSData(((TrigData *)trig_index[nr].proto)->name));
				}
			}
			break;
	}

	if (!found) {
		switch (subcmd) {
			case SCMD_RLIST:
				send_to_char("No rooms found within those parameters.\r\n", ch);
				break;
			case SCMD_OLIST:
				send_to_char("No objects found within those parameters.\r\n", ch);
				break;
			case SCMD_MLIST:
				send_to_char("No mobiles found within those parameters.\r\n", ch);
				break;
			case SCMD_TLIST:
				send_to_char("No triggers found within those parameters.\r\n", ch);
				break;
			default:
				mudlogf(BRF, LVL_STAFF, TRUE, "SYSERR:: invalid SCMD passed to do_liblist!  (SCMD = %d)", subcmd);
				break;
		}
	} else
		page_string(ch->desc, buf, 1);
		
	release_buffer(buf);
}


#define ZCMD (zone_table[zone].cmd[cmd_no])
#define S_ROOM(i, num)		(shop_index[i].in_room[(num)])
#define S_PRODUCT(i, num)	(shop_index[i].producing[(num)])
#define S_KEEPER(i)		(shop_index[i].keeper)

extern int top_shop;
extern struct shop_data *shop_index;

ACMD(do_move_element) {
	char	*arg = get_buffer(MAX_INPUT_LENGTH),
			*arg2 = get_buffer(MAX_INPUT_LENGTH);
	VNum	orig, targ;
	RNum	real_orig;
	SInt32	znum, znum2;
	UInt32	zone, cmd_no, room, dir;
	SInt32	i, n;
	struct int_list *trig;
	
	two_arguments(argument, arg, arg2);
	
	if (!*arg || !*arg2) {
		switch (subcmd) {
			case SCMD_RMOVE:	send_to_char("Usage: rmove <orig> <new>\r\n",ch);	break;
			case SCMD_OMOVE:	send_to_char("Usage: omove <orig> <new>\r\n",ch);	break;
			case SCMD_MMOVE:	send_to_char("Usage: mmove <orig> <new>\r\n",ch);	break;
			case SCMD_TMOVE:	send_to_char("Usage: tmove <orig> <new>\r\n",ch);	break;
			default:
				send_to_char("Hm, reached default case on do_move_element.\r\n", ch);
				mudlogf(BRF, LVL_STAFF, TRUE, "SYSERR:: invalid SCMD passed to do_move_element!  (SCMD = %d)", subcmd);
				break;
		}
	} else {
		orig = atoi(arg);
		targ = atoi(arg2);
		znum = real_zone(orig);
		znum2 = real_zone(targ);
		if ((znum == -1) || (znum2== -1)) {
			ch->Send("Zone %d does not exist.\r\n", (znum == -1) ? (orig / 100) : (targ / 100));
		} else if (get_zone_perms(ch, znum) && get_zone_perms(ch, znum2)) {
			switch (subcmd) {
				case SCMD_RMOVE:
					if ((real_orig = real_room(orig)) != NOWHERE) {
						if (real_room(targ) == NOWHERE) {
							world[real_orig].number = targ;
							ch->Send("Room %d moved to %d.", orig, targ);
							olc_add_to_save_list(zone_table[znum].number, OLC_SAVE_ROOM);
							olc_add_to_save_list(zone_table[znum2].number, OLC_SAVE_ROOM);
							for (zone = 0; zone <= top_of_zone_table; zone++) {
								for (cmd_no = 0; ZCMD.command != 'S'; cmd_no++) {
									switch(ZCMD.command) {
										case 'M':
										case 'O':
											if (ZCMD.arg3 == real_orig)
												olc_add_to_save_list(zone_table[zone].number, OLC_SAVE_ZONE);
											break;
										case 'D':
										case 'R':
											if (ZCMD.arg1 == real_orig)
												olc_add_to_save_list(zone_table[zone].number, OLC_SAVE_ZONE);
											break;
										case 'T':
											if ((ZCMD.arg1 == WLD_TRIGGER) && (ZCMD.arg4 == real_orig))
												olc_add_to_save_list(zone_table[zone].number, OLC_SAVE_ZONE);
									}
								}
							}
							for (room = 0; room <= top_of_world; room++) {
								for (dir = 0; dir < NUM_OF_DIRS; dir++) {
									if (EXITN(room, dir))
										if (EXITN(room, dir)->to_room == real_orig) {
											olc_add_to_save_list(zone_table[znum2].number, OLC_SAVE_ROOM);
//											ch->Send("Zone %d ROOMS affected.\r\n", zone);
										}
								}
							}
							for (i = 0; i < top_shop; i++) {
								for (n = 0; S_ROOM(i, n) != -1; n++) {
									if (S_ROOM(i, n) == real_orig)
										olc_add_to_save_list(zone_table[real_zone(SHOP_NUM(i))].number, OLC_SAVE_SHOP);
								}
							}
						} else 
							send_to_char("You can't replace an existing room.", ch);
					} else
						send_to_char("No such room exists.", ch);
					break;
				case SCMD_OMOVE:
					if ((real_orig = real_object(orig)) != NOTHING) {
						if (real_object(targ) == NOTHING) {
							obj_index[real_orig].vnum = targ;
							ch->Send("Obj %d moved to %d.", orig, targ);
							olc_add_to_save_list(zone_table[znum].number, OLC_SAVE_OBJ);
							olc_add_to_save_list(zone_table[znum2].number, OLC_SAVE_OBJ);
							for (zone = 0; zone <= top_of_zone_table; zone++) {
								for (cmd_no = 0; ZCMD.command != 'S'; cmd_no++) {
									switch(ZCMD.command) {
										case 'P':
											if (ZCMD.arg3 == real_orig)
												olc_add_to_save_list(zone_table[zone].number, OLC_SAVE_ZONE);
											break;
										case 'O':
										case 'G':
										case 'E':
											if (ZCMD.arg1 == real_orig)
												olc_add_to_save_list(zone_table[zone].number, OLC_SAVE_ZONE);
											break;
										case 'R':
											if (ZCMD.arg2 == real_orig)
												olc_add_to_save_list(zone_table[zone].number, OLC_SAVE_ZONE);
											break;
										default:
											continue;
									}
//									olc_add_to_save_list(zone_table[zone].number, OLC_SAVE_ZONE);
//									ch->Send("Zone %d ZEDITS affected.\r\n", zone);
								}
							}
							for (i = 0; i < top_shop; i++) {
								for (n = 0; S_PRODUCT(i, n) != -1; n++) {
									if (S_PRODUCT(i, n) == real_orig)
										olc_add_to_save_list(zone_table[real_zone(SHOP_NUM(i))].number, OLC_SAVE_SHOP);
								}
							}
						} else 
							send_to_char("You can't replace an existing object.", ch);
					} else
						send_to_char("No such object exists.", ch);
					break;
				case SCMD_MMOVE:
					if ((real_orig = real_mobile(orig)) != NOBODY) {
						if (real_mobile(targ) == NOBODY) {
							mob_index[real_orig].vnum = targ;
							ch->Send("Mob %d moved to %d.", orig, targ);
							olc_add_to_save_list(zone_table[znum].number, OLC_SAVE_MOB);
							olc_add_to_save_list(zone_table[znum2].number, OLC_SAVE_MOB);
							// First find all rooms linked to this, and mark them changed
							for (zone = 0; zone <= top_of_zone_table; zone++) {
								for (cmd_no = 0; ZCMD.command != 'S'; cmd_no++) {
									if ((ZCMD.command == 'M') && (ZCMD.arg1 == real_orig))
										olc_add_to_save_list(zone_table[zone].number, OLC_SAVE_ZONE);
//										ch->Send("Zone %d ZEDITS affected.\r\n", zone);
								}
							}
							for (i = 0; i < top_shop; i++) {
								if (S_KEEPER(i) == real_orig)
									olc_add_to_save_list(zone_table[real_zone(SHOP_NUM(i))].number, OLC_SAVE_SHOP);
							}
						} else
							send_to_char("You can't replace an existing mobile.", ch);
					} else
						send_to_char("No such mobile exists.", ch);
					break;
				case SCMD_TMOVE:
					if ((real_orig = real_mobile(orig)) != NOBODY) {
						if (real_mobile(targ) == NOBODY) {
							trig_index[real_orig].vnum = targ;
							ch->Send("Trig %d moved to %d.", orig, targ);
							olc_add_to_save_list(zone_table[znum].number, OLC_SAVE_TRIGGER);
							olc_add_to_save_list(zone_table[znum2].number, OLC_SAVE_TRIGGER);
							// First find all rooms linked to this, and mark them changed
							for (zone = 0; zone <= top_of_zone_table; zone++) {
								for (cmd_no = 0; ZCMD.command != 'S'; cmd_no++) {
									if ((ZCMD.command == 'T') && (ZCMD.arg2 == real_orig))
										olc_add_to_save_list(zone_table[zone].number, OLC_SAVE_ZONE);
								}
							}
							for (i = 0; i <= top_of_mobt; i++) {
								for (trig = mob_index[i].triggers; trig; trig = trig->next) {
									if (trig->i == orig) {
										trig->i = targ;
										olc_add_to_save_list(zone_table[real_zone(mob_index[i].vnum)].number, OLC_SAVE_MOB);
									}
								}
							}
							for (i = 0; i <= top_of_objt; i++) {
								for (trig = obj_index[i].triggers; trig; trig = trig->next) {
									if (trig->i == orig) {
										trig->i = targ;
										olc_add_to_save_list(zone_table[real_zone(obj_index[i].vnum)].number, OLC_SAVE_OBJ);
									}
								}
							}
						} else 
							send_to_char("You can't replace an existing trigger.", ch);
					} else
						send_to_char("No such trigger exists.", ch);
					break;
				default:
					send_to_char("Hm, reached default case on do_move_element.\r\n", ch);
					mudlogf(BRF, LVL_STAFF, TRUE, "SYSERR:: invalid SCMD passed to do_move_element!  (SCMD = %d)", subcmd);
					break;
			}
		}
	}
	release_buffer(arg);
	release_buffer(arg2);
}


ACMD(do_massroomsave) {
	int	zone;
	char *buf = get_buffer(MAX_INPUT_LENGTH);
	for (zone = 0; zone <= top_of_zone_table; zone++) {
//		sprintf(buf, "save %d", zone_table[zone].number);
//		do_olc(ch, buf, 0, "medit", SCMD_OLC_REDIT);
		zedit_save_to_disk(zone);
	}
	release_buffer(buf);
}
