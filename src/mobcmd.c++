/***************************************************************************
 * MOBProgram ported for CircleMUD 3.0 by Mattias Larsson		   *
 * Traveller@AnotherWorld (ml@eniac.campus.luth.se 4000) 		   *
 **************************************************************************/

/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/***************************************************************************
 *  The MOBprograms have been contributed by N'Atas-ha.  Any support for   *
 *  these routines should not be expected from Merc Industries.  However,  *
 *  under no circumstances should the blame for bugs, etc be placed on     *
 *  Merc Industries.  They are not guaranteed to work on all systems due   *
 *  to their frequent use of strxxx functions.  They are also not the most *
 *  efficient way to perform their tasks, but hopefully should be in the   *
 *  easiest possible way to install and begin using. Documentation for     *
 *  such installation can be found in INSTALL.  Enjoy........    N'Atas-Ha *
 ***************************************************************************/


#include "structs.h"
#include "utils.h"
#include "db.h"
#include "handler.h"
#include "find.h"
#include "interpreter.h"
#include "comm.h"
#include "event.h"
#include "spells.h"
#include "buffer.h"

extern IndexData *get_mob_index(int vnum);
extern IndexData *get_obj_index(int vnum);

EVENT(combat_event);
ACMD(do_trans);
ACMD(do_mpkill);
ACMD(do_mpjunk);
ACMD(do_mpechoaround);
ACMD(do_mpechoat);
ACMD(do_mpecho);
ACMD(do_mpmload);
ACMD(do_mpoload);
ACMD(do_mppurge);
ACMD(do_mpgoto);
ACMD(do_mpat);
ACMD(do_mptransfer);
ACMD(do_mpforce);
EVENT(delayed_command);
ACMD(do_mpdelayed);

ACMD(do_mpstat);
ACMD(do_mpdump);
ACMD(do_mpremember);
ACMD(do_mpforget);
EVENT(mprog_delay_trigger);
ACMD(do_mpdelayprog);
ACMD(do_mpcancel);
ACMD(do_mpremove);
ACMD(do_mpgforce);
ACMD(do_mpvforce);
ACMD(do_mpdamage);
ACMD(do_mpassist);
ACMD(do_mpgecho);
ACMD(do_mpzecho);

extern RNum find_target_room(CharData * ch, char *rawroomstr);

#define bug(x) mudlogf(NRM, LVL_STAFF, TRUE, x ": vnum %d, room %d.",\
		mob_index[ch->nr].vnum, world[IN_ROOM(ch)].number)
#define sbug(x, y) mudlogf(NRM, LVL_STAFF, TRUE, x ": vnum %d, room %d, arg %s.",\
		mob_index[ch->nr].vnum, world[IN_ROOM(ch)].number, (y))
#define dbug(x, y) mudlogf(NRM, LVL_STAFF, TRUE, x ": vnum %d, room %d, val %d.",\
		mob_index[ch->nr].vnum, world[IN_ROOM(ch)].number, (y))
#define sdbug(x, y, z) mudlogf(NRM, LVL_STAFF, TRUE, x ": vnum %d, room %d, val %d, arg %s.",\
		mob_index[ch->nr].vnum, world[IN_ROOM(ch)].number, (y), (z))
/*
 * Local functions.
 */

char *			mprog_type_to_name	(int type);
int str_prefix(const char *astr, const char *bstr);
ACMD(do_mpasound);

/* This routine transfers between alpha and numeric forms of the
 *  mob_prog bitvector types. It allows the words to show up in mpstat to
 *  make it just a hair bit easier to see what a mob should be doing.
 */

char *mprog_type_to_name(int type)
{
    switch (type)
    {
    case IN_FILE_PROG:          return "in_file_prog";
    case ACT_PROG:              return "act_prog";
    case SPEECH_PROG:           return "speech_prog";
    case RAND_PROG:             return "rand_prog";
    case FIGHT_PROG:            return "fight_prog";
    case HITPRCNT_PROG:         return "hitprcnt_prog";
    case DEATH_PROG:            return "death_prog";
    case ENTRY_PROG:            return "entry_prog";
    case GREET_PROG:            return "greet_prog";
    case ALL_GREET_PROG:        return "all_greet_prog";
    case GIVE_PROG:             return "give_prog";
    case BRIBE_PROG:            return "bribe_prog";
    case EXIT_PROG:				return "exit_prog";
    case ALL_EXIT_PROG:			return "all_exit_prog";
    case DELAY_PROG:			return "delay_prog";
    default:                    return "ERROR_PROG";
    }
}


/* Display mobprograms associated with mobile */
ACMD(do_mpstat) {
	MPROG_DATA	*mprg;
	CharData *victim;
	int i;
	char *arg = get_buffer(MAX_STRING_LENGTH);

	one_argument(argument, arg);
	
	if (!*arg)
		send_to_char("MPStat whom?\r\n", ch);
	else if (!(victim = get_char_vis(ch, arg)))
		send_to_char("No such mob.\r\n", ch);
	else if (!IS_NPC(victim))
		send_to_char("That is not a mobile", ch);
	else {
		sprintf(arg, "Mobile #%-6d [%s]\r\n"
					 "Delay   %-6d [%s]\r\n",
				GET_MOB_VNUM(victim), victim->RealName(),
				victim->mprog_delay, victim->mprog_target ?
						victim->mprog_target->RealName() : "No target");
		send_to_char(arg, ch);
		if (!mob_index[victim->nr].mobprogs)
			send_to_char("[No programs]\r\n", ch);
		else for (i = 0, mprg = mob_index[victim->nr].mobprogs; mprg; mprg = mprg->next) {
			sprintf(arg, "[%2d] %-8s %s\r\n",
					++i, mprog_type_to_name(mprg->type),
					mprg->arglist ? mprg->arglist : "[No args]");
			send_to_char(arg, ch);
		}
	}
	release_buffer(arg);
}


ACMD(do_mpdump) {
	char *arg1 = get_buffer(MAX_INPUT_LENGTH);
	char *arg2 = get_buffer(MAX_INPUT_LENGTH);
	MPROG_DATA *mprg;
	int i, num;
	CharData *victim;
	
	two_arguments(argument, arg1, arg2);
	if (!*arg1 || !*arg2)
		send_to_char("Syntax: mpdump <mob> <prog #>\r\n", ch);
	else if (!(victim = get_char_vis(ch, arg1)))
		send_to_char("No such mob.\r\n", ch);
	else if (!IS_NPC(victim))
		send_to_char("That is not a mobile.\r\n", ch);
	else if ((num = atoi(arg2)) <= 0)
		send_to_char("Syntax: mpdump <mob> <prog #>\r\n", ch);
	else if (ch->desc) {
		for (i = 1, mprg = mob_index[victim->nr].mobprogs; mprg; mprg = mprg->next)
			if (i++ == num)
				break;
		if (!mprg)
			send_to_char("No such program on the mobile.\r\n", ch);
		else if (!mprg->comlist)
			send_to_char("Major Error!  The command list of the mobprog is NULL!", ch);
		else if (!*mprg->comlist)
			send_to_char("The command list of the mobprog is empty.", ch);
		else page_string(ch->desc, mprg->comlist, TRUE);	
	}
	release_buffer(arg1);
	release_buffer(arg2);
}


/* prints the argument to all the rooms aroud the mobile */
ACMD(do_mpasound) {
	RNum	was_in_room;
	int		door;
//	char *arg;
	
	if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc) {
		send_to_char("Huh?\r\n", ch);
		return;
	}

	if (argument[0] == '\0') {
		bug("Mpasound - No argument");
		return;
	}

//	arg = get_buffer(MAX_INPUT_LENGTH);
//	one_argument(argument, arg);

	was_in_room = IN_ROOM(ch);
	for (door = 0; door <= 5; door++) {
		struct RoomDirection	*pexit;

		if ((pexit = world[was_in_room].dir_option[door]) != NULL
				&& pexit->to_room != NOWHERE && pexit->to_room != was_in_room) {
			IN_ROOM(ch) = pexit->to_room;
			MOBTrigger  = FALSE;
			act(argument, FALSE, ch, NULL, NULL, TO_ROOM);
		}
	}
	IN_ROOM(ch) = was_in_room;
//	release_buffer(arg);
	return;
}

/* lets the mobile kill any player or mobile without murder*/

ACMD(do_mpkill) {
    char *arg;
    CharData *victim;
	
    if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc) {
        send_to_char("Huh?\r\n", ch);
		return;
    }
	
	arg = get_buffer(MAX_INPUT_LENGTH);
    one_argument(argument, arg);

	if (*arg == '\0')
		bug("MpKill - no argument");
	else if ((victim = get_char_room_vis(ch, arg)) == NULL)
		sbug("MpKill - Victim not in room",	arg);
	else if (victim == ch)
		bug("MpKill - Bad victim to attack (self)");
	else if (AFF_FLAGGED(ch, AFF_CHARM) && ch->master == victim)
		sbug("MpKill - Charmed mob attacking master", victim->RealName());
	else if (ch->general.position == POS_FIGHTING)
		bug("MpKill - Already fighting");
	else
		combat_event(ch, victim, 0);
	release_buffer(arg);
    return;
}


/* Lets mob assist others */
ACMD(do_mpassist) {
	char *arg = get_buffer(MAX_STRING_LENGTH);
	CharData *victim;
	
	one_argument(argument, arg);
	if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc)
		send_to_char("Huh?\r\n", ch);
	else if (!*arg)
		bug("MpAssist - no argument");
	else if ((victim = get_char_room_vis(ch, arg))) {
		if (victim != ch && FIGHTING(victim) && !FIGHTING(ch) && FIGHTING(victim) != ch)
			combat_event(ch, FIGHTING(victim), 0);
	}
	release_buffer(arg);
}


/* lets the mobile destroy an object in its inventory
   it can also destroy a worn object and it can destroy 
   items using all.xxxxx or just plain all of them */

ACMD(do_mpjunk)
{
    char *arg;
    int pos;
    ObjData *obj;

	if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc) {
		send_to_char("Huh?\r\n", ch);
		return;
	}
	
	arg = get_buffer(MAX_INPUT_LENGTH);
	one_argument(argument, arg);

	if (*arg == '\0')
		bug("Mpjunk - No argument");
	else if (str_cmp(arg, "all") && str_prefix("all.", arg)) {
		if ((obj=get_object_in_equip_vis(ch,arg,ch->equipment,&pos))!= NULL) {
			unequip_char(ch, pos);
			obj->extract();
		} else if ((obj = get_obj_in_list_vis(ch, arg, ch->carrying)) != NULL)
			obj->extract();
	} else {
		START_ITER(iter, obj, ObjData *, ch->carrying) {
			if (arg[3] == '\0' || isname(arg+4, SSData(obj->name)))
				obj->extract();
		} END_ITER(iter);
		while((obj=get_object_in_equip_vis(ch,arg,ch->equipment,&pos))!=NULL) {
			unequip_char(ch, pos);
			obj->extract();
		}
	}
	release_buffer(arg);
    return;
}


/* Prints argument to everyone in game */
ACMD(do_mpgecho) {
//	DescriptorData *d;
	
	if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc)
		send_to_char("Huh?\r\n", ch);
	else if (!*argument)
		bug("MpGEcho: missing argument");
	else {
		act(argument, TRUE, ch, 0, 0, TO_GAME);
//		for (d = descriptor_list; d; d = d->next) {
//		START_ITER(iter, d, DescriptorData *, descriptor_list) {
//			if (!d->character || PLR_FLAGGED(d->character, PLR_WRITING) ||
//					(d->connected != CON_PLAYING))
//				continue;
//			d->character->Send("%s%s\r\n", NO_STAFF_HASSLE(d->character) ? "Mob gecho> " : "", argument);
//		} END_ITER(iter);
//		}
	}
}


/* Prints argument to everyone in zone */
ACMD(do_mpzecho) {
//	DescriptorData *d;
	
	if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc)
		send_to_char("Huh?\r\n", ch);
	else if (!*argument)
		bug("MpZEcho: missing argument");
	else if (IN_ROOM(ch) == NOWHERE)
		bug("MpZEcho: Mob is NOWHERE");
	else {
		act(argument, TRUE, ch, 0, 0, TO_ZONE);
//		for (d = descriptor_list; d; d = d->next) {
//			if (!d->character || PLR_FLAGGED(d->character, PLR_WRITING) ||
//					(d->connected != CON_PLAYING) ||
//					(world[IN_ROOM(ch)].zone != world[IN_ROOM(d->character)].zone))
//				continue;
//			if (NO_STAFF_HASSLE(d->character))
//				send_to_char("Mob zecho> ", d->character);
//			send_to_char(argument, d->character);
//			send_to_char("\r\n", d->character);
//		}
	}
}


/* prints the message to everyone in the room other than the mob and victim */
ACMD(do_mpechoaround) {
	CharData *victim;
	char *arg, *p;

	if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc) {
		send_to_char("Huh?\r\n", ch);
		return;
	}

	arg = get_buffer(MAX_INPUT_LENGTH);
	p=one_argument(argument, arg);
	while(isspace(*p)) p++; /* skip over leading space */

	if (!*arg)
		bug("Mpechoaround - No argument");
	else if (!(victim=get_char_room_vis(ch, arg)))
		sbug("Mpechoaround - victim does not exist", arg);
	else
		act(p, FALSE, ch, NULL, victim, TO_NOTVICT);
	
	release_buffer(arg);
	return;
}

/* prints the message to only the victim */

ACMD(do_mpechoat) {
	CharData *victim;
	char *arg, *p;

	if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc) {
		send_to_char("Huh?\r\n", ch);
		return;
	}

	arg = get_buffer(MAX_INPUT_LENGTH);
	p = one_argument(argument, arg);
	while(isspace(*p)) p++; /* skip over leading space */

	if (!*arg)
		bug("Mpechoat - No argument");
	else if (!(victim = get_char_room_vis(ch, arg)))
		sbug("Mpechoat - victim does not exist", arg);
	else
		act(p,FALSE,  ch, NULL, victim, TO_VICT);
		
	release_buffer(arg);
	return;
}

/* prints the message to the room at large */

ACMD(do_mpecho) {
	char *p;

	if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc) {
		send_to_char("Huh?\r\n", ch);
		return;
	}

	if (argument[0] == '\0') {
		bug("Mpecho - No argument");
		return;
	}
	p = argument;
	while(isspace(*p)) p++;

	act(p, FALSE, ch, NULL, NULL, TO_ROOM);
	return;
}
 
/* lets the mobile load an item or mobile.  All items
are loaded into inventory.  you can specify a level with
the load object portion as well. */

ACMD(do_mpmload) {
	char *arg;
	IndexData *pMobIndex;
	CharData      *victim;

	if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc) {
		send_to_char("Huh?\r\n", ch);
		return;
	}

	arg = get_buffer(MAX_INPUT_LENGTH);
	one_argument(argument, arg);

	if (!arg[0])
		bug("Mpmload - No arguement");
	else if (!is_number(arg))
		sbug("Mpmload - Bad vnum as arg", arg);
	else if ((pMobIndex = get_mob_index(atoi(arg))) == NULL)
		dbug("Mpmload - Bad mob vnum", atoi(arg));
	else if (pMobIndex->number < 20) {
		victim = read_mobile(pMobIndex->vnum, VIRTUAL);
		victim->to_room(IN_ROOM(ch));
	}
	release_buffer(arg);
	return;
}

ACMD(do_mpoload) {
	char *arg;
	IndexData *pObjIndex;
	ObjData       *obj;

	if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc) {
		send_to_char("Huh?\r\n", ch);
		return;
	}

	arg = get_buffer(MAX_INPUT_LENGTH);
	argument = one_argument(argument, arg);

	if (*arg == '\0')
		bug("Mpoload - No arguement");
	else if (!is_number(arg))
		sbug("Mpoload - Bad syntax", arg);
	else if ((pObjIndex = get_obj_index(atoi(arg))) == NULL)
		dbug("Mpoload - Bad vnum arg", atoi(arg));
	else {
		obj = read_object(atoi(arg), VIRTUAL);
		if (obj == NULL)
			dbug("Mpoload - NULL Object returned", atoi(arg));
		else if (CAN_WEAR(obj, ITEM_WEAR_TAKE))
			obj->to_char(ch);
		else
			obj->to_room(IN_ROOM(ch));
	}
	release_buffer(arg);
	return;
}

/* lets the mobile purge all objects and other npcs in the room,
   or purge a specified object or mob in the room.  It can purge
   itself, but this had best be the last command in the MOBprogram
   otherwise ugly stuff will happen */

ACMD(do_mppurge) {
	CharData *victim;
	ObjData  *obj;
	char *arg;

	if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc) {
		send_to_char("Huh?\r\n", ch);
		return;
	}
	
	arg = get_buffer(MAX_INPUT_LENGTH);
	one_argument(argument, arg);

	if (!*arg) { /* 'purge' */
		START_ITER(ch_iter, victim, CharData *, world[IN_ROOM(ch)].people) {
			if (IS_NPC(victim) && victim != ch)
				victim->extract();
		} END_ITER(ch_iter);

		START_ITER(obj_iter, obj, ObjData *, world[IN_ROOM(ch)].contents) {
			obj->extract();
		} END_ITER(obj_iter);
	} else if (!(victim = get_char_room_vis(ch, arg))) {
		if ((obj = get_obj_vis(ch, arg))) {
			obj->extract();
		} else {
			sbug("Mppurge - Bad argument", arg);
		}
	} else if (!IS_NPC(victim))
		sbug("Mppurge - Purging a PC", arg);
	else
		victim->extract();
		
	release_buffer(arg);
	
	return;
}
 
 
/* lets the mobile goto any location it wishes that is not private */
 
ACMD(do_mpgoto) {
	char *	arg;
	RNum	location;

	if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc) {
		send_to_char("Huh?\r\n", ch);
		return;
	}

	arg = get_buffer(MAX_INPUT_LENGTH);
	one_argument(argument, arg);
	if (*arg == '\0')
		bug("Mpgoto - No argument");
	else if ((location = find_target_room(ch, arg)) < 0)
		sbug("Mpgoto - No such location", arg);
	else {
		if (FIGHTING(ch) != NULL)
			ch->stop_fighting();

		ch->from_room();
		ch->to_room(location);
	}
	release_buffer(arg);
	return;
}

/* lets the mobile do a command at another location. Very useful */
 
ACMD(do_mpat) {
	char *	arg;
	RNum	location;
	RNum	original;
/*    CharData       *wch; */

	if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc) {
		send_to_char("Huh?\r\n", ch);
		return;
	}

	arg = get_buffer(MAX_INPUT_LENGTH);
	argument = one_argument(argument, arg);

	if (*arg == '\0')
		bug("Mpat - Bad argument 1");
	else if (argument[0] == '\0')
		bug("Mpat - Bad argument 2");
	else if ((location = find_target_room(ch, arg)) < 0)
		sbug("Mpat - No such location", arg);
	else {
		original = IN_ROOM(ch);
		IN_ROOM(ch) = location;
		command_interpreter(ch, argument);

		/*
		 * See if 'ch' still exists before continuing!
		 * Handles 'at XXXX quit' case.
		 */
		if(IN_ROOM(ch) == location) {
//			char_from_room(ch);
//			char_to_room(ch, original);
			IN_ROOM(ch) = original;
		}
	}
	release_buffer(arg);	
	return;
}

/* lets the mobile transfer people.  the all argument transfers
   everyone in the current room to the specified location */

ACMD(do_mptransfer) {
	char *arg1;
	char *arg2;
	RNum location;
	DescriptorData *d; //, *next_desc;
	CharData       *victim;

	if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc) {
		send_to_char("Huh?\r\n", ch);
		return;
	}
	arg1 = get_buffer(MAX_INPUT_LENGTH);
	argument = one_argument(argument, arg1);

	if (!*arg1) {
		bug("Mptransfer - No arguement");
		release_buffer(arg1);
		return;
	}

	arg2 = get_buffer(MAX_INPUT_LENGTH);
	argument = one_argument(argument, arg2);
	
	/*
	 * Thanks to Grodyn for the optional location parameter.
	 */
	if (!*arg2) {
		location = IN_ROOM(ch);
	} else {
		if ((location = find_target_room(ch, arg2)) < 0) {
			sbug("Mptransfer - No such location", arg2);
			release_buffer(arg1);
			release_buffer(arg2);
			return;
		}

		if (ROOM_FLAGGED(location, ROOM_PRIVATE)) {
			sdbug("Mptransfer - Private room", location, arg2);
			release_buffer(arg1);
			release_buffer(arg2);
			return;
		}
	}
	
	release_buffer(arg2);
	
	if (!str_cmp(arg1, "game")) {
		START_ITER(iter, d, DescriptorData *, descriptor_list) {
			if ((STATE(d) == CON_PLAYING)
					&& (d->character != ch)
					&& (IN_ROOM(d->character) != NOWHERE)
					&& ch->CanSee(d->character)) {
				victim = d->character;
				if (FIGHTING(victim))
					victim->stop_fighting();

				victim->from_room();
				victim->to_room(location);
			}
		} END_ITER(iter);
	} else if (!str_cmp(arg1, "all")) {
		START_ITER(iter, victim, CharData *, world[IN_ROOM(ch)].people) {
			if (victim == ch)		continue;
			if (FIGHTING(victim))	victim->stop_fighting();

			victim->from_room();
			victim->to_room(location);
		} END_ITER(iter);
	} else if (!(victim = get_char_vis(ch, arg1)))
		sbug("Mptransfer - No such person", arg1);
	else if (IN_ROOM(victim) == 0)
		sbug("Mptransfer - Victim in Limbo", arg1);
	else {
		if (FIGHTING(victim))
			victim->stop_fighting();

		victim->from_room();
		victim->to_room(location);
	}
	release_buffer(arg1);

	return;
}
 
/* lets the mobile force someone to do something.  must be mortal level
   and the all argument only affects those in the room with the mobile */

ACMD(do_mpforce) {
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	CharData *victim;
	DescriptorData *i;
	
	argument = one_argument(argument, arg);
	
	if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc)
		send_to_char("Huh?\r\n", ch);
	else if (*arg == '\0')
		bug("Mpforce - No argument 1");
	else if (*argument == '\0')
		bug("Mpforce - No argument 2");
	else if (!str_cmp(arg, "all")) {
		START_ITER(iter, i, DescriptorData *, descriptor_list) {
			if(i->character != ch && (STATE(i) == CON_PLAYING) &&
					IN_ROOM(i->character) == IN_ROOM(ch)) {
				victim = i->character;
				if(ch->CanSee(victim) && !IS_STAFF(victim)) {
					command_interpreter(victim, argument);
				}
			}
		} END_ITER(iter);
	} else {
		if ((victim = get_char_room_vis(ch, arg))) {
			if (victim == ch)
				bug("Mpforce - Forcing oneself");
			else if (!IS_STAFF(victim))
				command_interpreter(victim, argument);
		}
	}
	release_buffer(arg);
	return;
}


/* Lets a mobile GROUP FORCE something */
ACMD(do_mpgforce) {
	char *arg = get_buffer(MAX_STRING_LENGTH);
	CharData *victim, *vch;
	
	argument = one_argument(argument, arg);
	
	if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc)
		send_to_char("Huh?\r\n", ch);
	else if (!*arg)
		bug("MPVForce - no argument");
	else if ((victim = get_char_room_vis(ch, arg))) {
		START_ITER(iter, vch, CharData *, world[IN_ROOM(victim)].people) {
			if (is_same_group(victim, vch) && vch != ch && !IS_STAFF(vch))
				command_interpreter(vch, arg);
		} END_ITER(iter);
	}
	release_buffer(arg);
}


/* Force all mobs of a vnum to do something */
ACMD(do_mpvforce) {
	CharData *victim;
	char *arg = get_buffer(MAX_STRING_LENGTH);
	int vnum;
	
	argument = one_argument(argument, arg);
	
	if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc)
		send_to_char("Huh?\r\n", ch);
	else if (!*arg)
		bug("MpVForce - No argument");
	else if (!is_number(arg))
		sbug("MpVForce - bad syntax", arg);
	else if ((real_mobile(vnum = atoi(arg))) < 0)
		dbug("MpVForce - no mobile of vnum", vnum);
	else {

		START_ITER(iter, victim, CharData *, character_list) {
			if (GET_MOB_VNUM(victim) == vnum && !FIGHTING(victim) && ch != victim)
				command_interpreter(victim, arg);
		} END_ITER(iter);
	}
	release_buffer(arg);
}


EVENT(delayed_command) {
	if (info) {
		command_interpreter(CAUSER_CH,(char *)info);
		free((Ptr)info);							// Can't use the FREE macro here.
	}
}


ACMD(do_mpdelayed) {
	char *arg1, *arg2;
	int delay;

	if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM)) {
		send_to_char("Huh?\r\n", ch);
		return;
	}
	
	arg1 = get_buffer(MAX_INPUT_LENGTH);
	arg2 = get_buffer(MAX_INPUT_LENGTH);
	
	half_chop(argument, arg1, arg2);
	delay = atoi(arg1);
	release_buffer(arg1);
	
	if (delay < 1)
		dbug("Mpdelayed - the delay is invalid", delay);
	else {
//		sprintf(buf,"Ok. Executing '%s' in %d seconds.\r\n", arg2, delay);
//		send_to_char(buf,ch);
		add_event(delay, delayed_command, ch, NULL, (Ptr)str_dup(arg2));
	}
	release_buffer(arg2);
}


/* Do damage target(s) */
ACMD(do_mpdamage) {
	CharData *victim;
	char *target, *min, *max;
	int low, high;
	int fAll, fKill;
	int dam;
	
	fAll = FALSE;
	fKill = FALSE;
	
	target = get_buffer(MAX_INPUT_LENGTH);
	argument = one_argument(argument, target);
	
	if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc)
		send_to_char("Huh?\r\n", ch);
	else if (!*target)
		bug("MpDamage - no target, minimum, or maximum");
	else {
		min = get_buffer(MAX_INPUT_LENGTH);
		max = get_buffer(MAX_INPUT_LENGTH);
		
		argument = two_arguments(argument, min, max);
		
		if (!*min)
			bug("MpDamage - no minimum or maximum");
		else if (!is_number(min))
			bug("MpDamage - minimum is not a number");
		else if (!*max)
			bug("MpDamage - no maximum");
		else if (!is_number(max))
			bug("MpDamage - maximum is not a number");
		else {
			low = atoi(min);
			high = atoi(max);
			if (!str_cmp(target, "all"))
				fAll = TRUE;
			else if ((victim = get_char_room_vis(ch, target))) {
				one_argument(argument, target);
				
				if (!*target)
					fKill = TRUE;
				
				if (fAll) {
					START_ITER(iter, victim, CharData *, world[IN_ROOM(ch)].people) {
						if (victim == ch)
							continue;
						dam = Number(low, high);
						dam = fKill ? dam : MIN(GET_HIT(victim), dam);
						damage(ch, victim, dam, TYPE_UNDEFINED, 1);
					} END_ITER(iter);
				} else {
					dam = Number(low, high);
					dam = fKill ? dam : MIN(GET_HIT(victim), dam);
					damage(ch, victim, dam, TYPE_UNDEFINED, 1);
				}
				
			}
		}
		release_buffer(min);
		release_buffer(max);
	}
	release_buffer(target);
}


/* Set target */
ACMD(do_mpremember) {
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	one_argument(argument, arg);
	if (!IS_NPC(ch) || ch->desc || AFF_FLAGGED(ch, AFF_CHARM))
		send_to_char("Huh?\r\n", ch);
	else if (!*arg)	bug("MpRemember: missing argument");
	else			TARGET(ch) = get_char_vis(ch, arg);
	release_buffer(arg);
}


/* Forget target */
ACMD(do_mpforget) {
	if (!IS_NPC(ch) || ch->desc || AFF_FLAGGED(ch, AFF_CHARM))
		send_to_char("Huh?\r\n", ch);
	TARGET(ch) = NULL;
}


/* Set delayedprog */
ACMD(do_mpdelayprog) {
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	one_argument(argument, arg);
	if (!IS_NPC(ch) || ch->desc || AFF_FLAGGED(ch, AFF_CHARM))
		send_to_char("Huh?\r\n", ch);
	else if (!*arg)
		bug("MpDelayProg - No argument");
	else if (!is_number(arg))
		bug("MpDelayProg - Argument is not a number");
	else {
		ch->mprog_delay = atoi(arg);
		add_event(ch->mprog_delay, mprog_delay_trigger, ch, 0, 0);
	}
	release_buffer(arg);
}


/* Cancel delayedprog */
ACMD(do_mpcancel) {
	if (!IS_NPC(ch) || ch->desc || AFF_FLAGGED(ch, AFF_CHARM))
		send_to_char("Huh?\r\n", ch);
	else {
		ch->mprog_delay = -1;
		clean_special_events(ch, mprog_delay_trigger, EVENT_CAUSER);
	}
}


/*
 * Call another mobprog's mobprog
 * Syntax: mob call [vnum] [victim|'null'] [object1|'null'] [object2|'null']
 */
// NOT IMPLEMENTED YET
#if 0
ACMD(do_mpcall) {
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	CharData *vict;
	ObjData *obj1, *obj2;
	MPROG_DATA *prg;
	
	argument = one_argument(argument, arg);
	if (!*arg)
		bug("MpCall: missing argument");
	else if (prg = get_mprog-index

}
#endif


/* Lets mobile strip an object or all objects from victim */
ACMD(do_mpremove) {
	CharData *victim;
	ObjData *obj;
	int vnum = 0, x;
	int fAll = FALSE;
	char *vict = get_buffer(MAX_INPUT_LENGTH),
		 *what = get_buffer(MAX_INPUT_LENGTH);
	
	two_arguments(argument, vict, what);
	
	if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc)
		send_to_char("Huh?\r\n", ch);
	else if (!*vict)
		bug("MpRemove - No arguments");
	else if (!*what)
		bug("MpRemove - Missing second argument");
	else if (!is_number(what) && !str_cmp(what, "all"))
		sbug("MpRemove - invalid 2nd argument", what);
	else if ((victim = get_char_room_vis(ch, vict))) {
		if (!str_cmp(what, "all"))
			fAll = TRUE;
		else vnum = atoi(what);
		START_ITER(iter, obj, ObjData *, victim->carrying) {
			if (fAll || (GET_OBJ_VNUM(obj) == vnum))
				obj->extract();
		} END_ITER(iter);
		for (x = 0; x < NUM_WEARS; x++) {
			obj = GET_EQ(victim, x);
			if (obj && (fAll || (GET_OBJ_VNUM(obj) == vnum)))
				obj->extract();
		}

	}
	release_buffer(vict);
	release_buffer(what);
}

