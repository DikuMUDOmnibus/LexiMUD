/*************************************************************************
*   File: act.informative.c++        Part of Aliens vs Predator: The MUD *
*  Usage: Player-level information commands                              *
*************************************************************************/


#include "characters.h"
#include "objects.h"
#include "rooms.h"
#include "descriptors.h"
#include "utils.h"
#include "buffer.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "find.h"
#include "db.h"
#include "clans.h"
#include "boards.h"
#include "help.h"
#include "extradesc.h"


/* extern variables */
extern struct title_type titles[NUM_RACES][16];

extern struct TimeInfoData time_info;

extern char *weekdays[];
extern char *month_name[];
extern char *help;
extern char *connected_types[];
extern char circlemud_version[];
extern char *credits;
extern char *news;
extern char *info;
extern char *motd;
extern char *imotd;
extern char *wizlist;
extern char *immlist;
extern char *policies;
extern char *handbook;
extern char *dirs[];
extern char *where[];
extern char *color_liquid[];
extern char *fullness[];
extern char *connected_types[];
extern char *room_bits[];
extern char *skills[];
extern char *sector_types[];
extern char *race_abbrevs[];
extern char *relation_colors;

/* extern functions */
struct TimeInfoData real_time_passed(time_t t2, time_t t1);
void master_parser (char *buf, CharData *ch, RoomData *room, ObjData *obj);
int wear_message_number (ObjData *obj, int where);
long find_race_bitvector(char arg);
ACMD(do_action);

/* prototypes */
void diag_char_to_char(CharData * i, CharData * ch);
void look_at_char(CharData * i, CharData * ch);
void list_one_char(CharData * i, CharData * ch);
void list_char_to_char(LList<CharData *> &list, CharData * ch);
void do_auto_exits(CharData * ch, RNum room);
void look_in_direction(CharData * ch, SInt32 dir, SInt32 maxDist);
void look_in_obj(CharData * ch, char *arg);
char *find_exdesc(char *word, ExtraDesc * list);
void look_at_target(CharData * ch, char *arg, int read);
void perform_mortal_where(CharData * ch, char *arg);
void perform_immort_where(CharData * ch, char *arg);
void sort_commands(void);
void show_obj_to_char(ObjData * object, CharData * ch, int mode);
void list_obj_to_char(LList<ObjData *> &list, CharData * ch, int mode,  int show);
void print_object_location(int num, ObjData * obj, CharData * ch, int recur);
void list_scanned_chars(LList<CharData *> &list, CharData * ch, int distance, int door);
void look_out(CharData * ch);
ACMD(do_exits);
ACMD(do_look);
ACMD(do_examine);
ACMD(do_gold);
ACMD(do_score);
ACMD(do_inventory);
ACMD(do_equipment);
ACMD(do_time);
ACMD(do_weather);
ACMD(do_help);
ACMD(do_who);
ACMD(do_users);
ACMD(do_gen_ps);
ACMD(do_where);
ACMD(do_levels);
ACMD(do_consider);
ACMD(do_diagnose);
ACMD(do_toggle);
ACMD(do_commands);
ACMD(do_attributes);


void show_obj_to_char(ObjData * object, CharData * ch, int mode) {
	int found;
	char *buf = get_buffer(MAX_STRING_LENGTH);

	*buf = '\0';
	strcpy(buf, "`g");
	if ((mode == 0) && SSData(object->description)) {
		if ((ch->sitting_on == object) && (GET_POS(ch) <= POS_SITTING))
			sprintf(buf + strlen(buf), "You are %s on %s.",
					(GET_POS(ch) == POS_RESTING) ? "resting" : "sitting",
					SSData(object->shortDesc));
		else
			strcat(buf, SSData(object->description));
	} else if (SSData(object->shortDesc) && ((mode == 1) ||
			(mode == 2) || (mode == 3) || (mode == 4)))
		strcat(buf, SSData(object->shortDesc));
	else if (mode == 5) {
		if (GET_OBJ_TYPE(object) == ITEM_NOTE) {
			if (SSData(object->actionDesc)) {
				strcpy(buf, "There is something written upon it:\r\n\r\n");
				strcat(buf, SSData(object->actionDesc));
				page_string(ch->desc, buf, 1);
			} else
				act("It's blank.", FALSE, ch, 0, 0, TO_CHAR);
			release_buffer(buf);
			return;
		} else if (GET_OBJ_TYPE(object) == ITEM_BOARD) {
			Board_show_board(GET_OBJ_VNUM(object), ch);
			release_buffer(buf);
			return;
		} else if (GET_OBJ_TYPE(object) == ITEM_DRINKCON)
			strcpy(buf, "It looks like a drink container.");
		else
			strcpy(buf, "You see nothing special..");
	}
	if (mode != 3) {
		found = FALSE;
		if (OBJ_FLAGGED(object, ITEM_INVISIBLE)) {
			strcat(buf, " (invisible)");
			found = TRUE;
		}
		if (OBJ_FLAGGED(object, ITEM_GLOW)) {
			strcat(buf, " (glowin)");
			found = TRUE;
		}
		if (OBJ_FLAGGED(object, ITEM_HUM)) {
			strcat(buf, " (humming)");
			found = TRUE;
		}
	}
	strcat(buf, "\r\n");
	page_string(ch->desc, buf, 1);
	release_buffer(buf);
}


void list_obj_to_char(LList<ObjData *> &list, CharData * ch, int mode, int show) {
	ObjData *i, *j;
	SInt32	num;
	LListIterator<ObjData *>	iter(list), sub_iter(list);
	bool	found = false;
	
	while ((i = iter.Next())) {
		num = 0;								/* None found */
		
		sub_iter.Reset();
		while ((j = sub_iter.Next())) {
			if (j == i)
				break;
			if (GET_OBJ_RNUM(j) == NOTHING) {
				if(!strcmp(SSData(j->shortDesc), SSData(i->shortDesc)))
					break;
			} else if (GET_OBJ_RNUM(j) == GET_OBJ_RNUM(i))
				break;
		}
		
		if ((GET_OBJ_TYPE(i) != ITEM_CONTAINER)	&& (j!=i) &&
				(!IS_SITTABLE(i)) && (GET_OBJ_TYPE(i) != ITEM_VEHICLE))
			continue;

		sub_iter.Reset();
		while ((j = sub_iter.Next())) {
			if (j == i)
				break;
		}
		do {
			if (GET_OBJ_RNUM(j) == NOTHING) {
				if(!strcmp(SSData(j->shortDesc), SSData(i->shortDesc)))
					num++;
			} else if (GET_OBJ_RNUM(j) == GET_OBJ_RNUM(i))
				num++;
		} while ((j = sub_iter.Next()));
		
		if (ch->CanSee(i)) {
			if ((GET_OBJ_TYPE(i) != ITEM_CONTAINER) && (num!=1) &&
					(!IS_SITTABLE(i)) && (GET_OBJ_TYPE(i) != ITEM_VEHICLE))
				ch->Send("(%2.2i) ", num);
			if (mode != 0) {
				show_obj_to_char(i, ch, mode);
				found = true;
			} else if (IS_SITTABLE(i)) {
				if ((get_num_chars_on_obj(i) < GET_OBJ_VAL(i, 0)) ||
					(ch->sitting_on == i)) {
					show_obj_to_char(i, ch, mode);
					found = true;
				}
			} else if (IS_MOUNTABLE(i)) {
				if ((ch->sitting_on == i) || !get_num_chars_on_obj(i)) {
					show_obj_to_char(i, ch, mode);
					found = true;
				}
			} else {
				show_obj_to_char(i, ch, mode);
				found = true;
			}
		}
	}
	
	if (!found && show)
		send_to_char("`g Nothing.\r\n", ch);
}


void diag_char_to_char(CharData * i, CharData * ch) {
	int percent;
	char *buf;

	if (GET_MAX_HIT(i) > 0)		percent = (100 * GET_HIT(i)) / GET_MAX_HIT(i);
	else						percent = -1;		//	How could MAX_HIT be < 1??

	if (percent >= 100)			buf = "$N is in excellent condition.\r\n";
	else if (percent >= 90)		buf = "$N has a few scratches.\r\n";
	else if (percent >= 75)		buf = "$N has some small wounds and bruises.\r\n";
	else if (percent >= 50)		buf = "$N has quite a few wounds.\r\n";
	else if (percent >= 30)		buf = "$N has some big nasty wounds and scratches.\r\n";
	else if (percent >= 15)		buf = "$N looks pretty hurt.\r\n";
	else if (percent >= 0)		buf = "$N is in awful condition.\r\n";
	else						buf = "$N is bleeding awfully from big wounds.\r\n";

	act(buf, TRUE, ch, 0, i, TO_CHAR);
}


void look_at_char(CharData * i, CharData * ch) {
	int j, found;
	ObjData *tmp_obj;

	if (!ch->desc)
		return;
		
	if (i->general.description)
		send_to_char(SSData(i->general.description), ch);
	else
		act("You see nothing special about $m.", FALSE, i, 0, ch, TO_VICT);

	diag_char_to_char(i, ch);

	found = FALSE;
	for (j = 0; !found && j < NUM_WEARS; j++)
		if (GET_EQ(i, j) && ch->CanSee(GET_EQ(i, j)))
			found = TRUE;

	if (found) {
		act("\r\n$n is using:", FALSE, i, 0, ch, TO_VICT);
		for (j = 0; j < NUM_WEARS; j++)
			if (GET_EQ(i, j) && ch->CanSee(GET_EQ(i, j))) {
				send_to_char(where[wear_message_number(GET_EQ(i, j), j)], ch);
				show_obj_to_char(GET_EQ(i, j), ch, 1);
			}
	}
	if (NO_STAFF_HASSLE(ch)) {
		found = FALSE;
		act("\r\nYou attempt to peek at $s inventory:", FALSE, i, 0, ch, TO_VICT);
		START_ITER(iter, tmp_obj, ObjData *, i->carrying) {
			if (ch->CanSee(tmp_obj) && (Number(0, 20) < GET_LEVEL(ch))) {
				show_obj_to_char(tmp_obj, ch, 1);
				found = TRUE;
			}
		}

		if (!found)
			send_to_char("You can't see anything.\r\n", ch);
	}
}


void list_one_char(CharData * i, CharData * ch) {
	char *buf = get_buffer(MAX_STRING_LENGTH);
	char *positions[] = {
		" is lying here, dead.",
		" is lying here, mortally wounded.",
		" is lying here, incapacitated.",
		" is lying here, stunned.",
		" is sleeping here.",
		" is resting here.",
		" is sitting here.",
		"!FIGHTING!",
		" is standing here."
	};
	char *sit_positions[] = {
		"!DEAD!",
		"!MORTALLY WOUNDED!",
		"!INCAPACITATED!",
		"!STUNNED!",
		"sleeping",
		"resting",
		"sitting",
		"!FIGHTING!",
		"!STANDING!"
	};
	
	char relation = relation_colors[i->GetRelation(ch)];
	
/*	strcpy(buf, '\0');*/
	*buf = '\0';
	
	if (IS_NPC(i) && i->general.longDesc && GET_POS(i) == GET_DEFAULT_POS(i)) {
/*		if (AFF_FLAGGED(i, AFF_INVISIBLE))
			strcpy(buf, "*");
		else
			*buf = '\0';
*/		
		ch->Send("%s`%c%s`y", AFF_FLAGGED(i, AFF_INVISIBLE) ? "`y*" : "",
				relation, SSData(i->general.longDesc));

//		if (AFF_FLAGGED(i, AFF_SANCTUARY))
//			act("...$e glows with a bright light!", FALSE, i, 0, ch, TO_VICT);
		if (AFF_FLAGGED(i, AFF_BLIND))
			act("...$e is groping around blindly!", FALSE, i, 0, ch, TO_VICT);

		release_buffer(buf);
		return;
	}
  
	if (IS_NPC(i)) {
		sprintf(buf, "`%c%s`y", relation, SSData(i->general.shortDesc));
		CAP(buf+2);
	} else {
		if (IS_STAFF(i))	sprintf(buf, "`%c%s `y%s`y", relation, i->GetName(), GET_TITLE(i));
		else				sprintf(buf, "`y%s `%c%s`y", GET_TITLE(i), relation, i->GetName());
	}
  
	if (AFF_FLAGGED(i, AFF_INVISIBLE))	strcat(buf, " (invisible)");
	if (AFF_FLAGGED(i, AFF_HIDE))		strcat(buf, " (hidden)");
	if (!IS_NPC(i) && !i->desc)			strcat(buf, " (linkless)");
	if (PLR_FLAGGED(i, PLR_WRITING))	strcat(buf, " (writing)");
    if (GET_AFK(i))						strcat(buf, " `c[AFK]`y");
	if (MOB_FLAGGED(i, MOB_PROGMOB))	strcat(buf, " `r(PROG-MOB)`y");
	if (AFF_FLAGGED(i, AFF_SANCTUARY))	strcat(buf, " `W(glowing)`y");

	if (i->sitting_on) {
		if ((GET_POS(i) < POS_SLEEPING) || (GET_POS(i) > POS_SITTING) ||
				(IN_ROOM(i->sitting_on) != IN_ROOM(i))) {
			strcat(buf, positions[(int) GET_POS(i)]);
			i->sitting_on = NULL;
		} else
			sprintf(buf + strlen(buf), "`y is %s on `g%s`y.", sit_positions[(int)GET_POS(i)],
					SSData(i->sitting_on->shortDesc));
	} else if (GET_POS(i) != POS_FIGHTING)
		strcat(buf, positions[(int) GET_POS(i)]);
	else {
		if (FIGHTING(i)) {
			sprintf(buf + strlen(buf), " is here, `Rfighting %s`y!",
				FIGHTING(i) == ch ? "YOU`y!" : (IN_ROOM(i) == IN_ROOM(FIGHTING(i)) ?
						PERS(FIGHTING(i), ch) : "someone who has already left"));
		} else			/* NIL fighting pointer */
			strcat(buf, " is here struggling with thin air.");
	}

	strcat(buf, "`n\r\n");
	send_to_char(buf, ch);

//	if (AFF_FLAGGED(i, AFF_SANCTUARY))
//		act("...$e glows with a `Wbright light`n`y!", FALSE, i, 0, ch, TO_VICT);
	if (AFF_FLAGGED(i, AFF_BLIND))
		act("...$e is groping around blindly!", FALSE, i, 0, ch, TO_VICT);

	release_buffer(buf);
}



void list_char_to_char(LList<CharData *> &list, CharData * ch) {
	CharData *i;

	START_ITER(iter, i, CharData *, list) {
		if (ch != i) {
			if (ch->CanSee(i))
				list_one_char(i, ch);
			else if (IS_DARK(IN_ROOM(ch)) &&
					!CAN_SEE_IN_DARK(ch) &&
					AFF_FLAGGED(i, AFF_INFRAVISION))
				send_to_char("`yYou see a pair of `Rglowing red eyes`y looking your way.\r\n", ch);
		}
	} END_ITER(iter);
}


void do_auto_exits(CharData * ch, RNum room) {
	SInt8	door;
	SInt32	buflen = 0;
	char *buf = get_buffer(MAX_STRING_LENGTH);

	if (room < 0)
		return;
	for (door = 0; door < NUM_OF_DIRS; door++)
		if ((EXITN(room, door) && EXITN(room, door)->to_room != NOWHERE) &&
				!EXITN_IS_HIDDEN(room, door, ch))
			buflen += sprintf(buf + buflen /* strlen(buf) */, "%s%c%s ",
					(IS_HIDDEN_EXITN(room, door)) ? "`g" : "",
					(EXIT_FLAGGED(EXITN(room, door), EX_CLOSED)) ?
					LOWER(*dirs[door]) : UPPER(*dirs[door]),
					(IS_HIDDEN_EXITN(room, door)) ? "`c" : "");

	ch->Send("`c[ Exits: %s]`n\r\n", *buf ? buf : "None! ");
	release_buffer(buf);
}


ACMD(do_exits) {
	int door;
	char *buf2, *buf;
	
	if (AFF_FLAGGED(ch, AFF_BLIND)) {
		send_to_char("You can't see a damned thing, you're blind!\r\n", ch);
		return;
	}
	buf = get_buffer(MAX_STRING_LENGTH);
	buf2 = get_buffer(MAX_STRING_LENGTH);
	
	for (door = 0; door < NUM_OF_DIRS; door++)
		if (EXIT(ch, door) && EXIT(ch, door)->to_room != NOWHERE &&
/*		!IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED)*/
				!EXIT_IS_HIDDEN(ch, door)) {
			if (IS_STAFF(ch))
				sprintf(buf2, "%-5s - [%5d] %s\r\n", dirs[door],
						world[EXIT(ch, door)->to_room].number,
						world[EXIT(ch, door)->to_room].GetName("<ERROR>"));
			else {
				sprintf(buf2, "%-5s - ", dirs[door]);
				if (IS_DARK(EXIT(ch, door)->to_room) && !CAN_SEE_IN_DARK(ch))
					strcat(buf2, "Too dark to tell\r\n");
				else {
					strcat(buf2, world[EXIT(ch, door)->to_room].GetName("<ERROR>"));
					strcat(buf2, "\r\n");
				}
			}
			strcat(buf, CAP(buf2));
//			CAP(buf2);
		}
		
	release_buffer(buf2);
	
	send_to_char("Obvious exits:\r\n", ch);

	if (*buf)
		send_to_char(buf, ch);
	else
		send_to_char(" None.\r\n", ch);
	
	release_buffer(buf);
}


void look_at_rnum(CharData * ch, RNum room, int ignore_brief) {
	char *buf;
	
	if (!ch->desc || room == NOWHERE)
		return;
	if (IS_DARK(room) && !CAN_SEE_IN_DARK(ch)) {
		send_to_char("It is pitch black...\r\n", ch);
		return;
	} else if (AFF_FLAGGED(ch, AFF_BLIND)) {
		send_to_char("You see nothing but infinite darkness...\r\n", ch);
		return;
	}
	
	send_to_char("`c", ch);
	if (PRF_FLAGGED(ch, PRF_ROOMFLAGS)) {
		buf = get_buffer(MAX_STRING_LENGTH);
		char *bitbuf = get_buffer(MAX_STRING_LENGTH);
		char *secbuf = get_buffer(MAX_STRING_LENGTH);
		sprintbit(ROOM_FLAGS(room), room_bits, bitbuf);
		sprinttype(SECT(IN_ROOM(ch)), sector_types, secbuf);
		strcpy(buf, world[room].GetName("<ERROR>"));
		if (ROOM_FLAGGED(room, ROOM_PARSE))
			master_parser(buf, ch, world + room, NULL);
		ch->Send("[%5d] %s [ %s] [%s]", world[room].number, buf, bitbuf, secbuf);
		release_buffer(buf);
		release_buffer(bitbuf);
		release_buffer(secbuf);
	} else if (ROOM_FLAGGED(room, ROOM_PARSE)) {
		buf = get_buffer(MAX_STRING_LENGTH);
		strcpy(buf, world[room].GetName("<ERROR>"));
		master_parser(buf, ch, world + room, NULL);
		send_to_char(buf, ch);
		release_buffer(buf);
	} else
		send_to_char(world[room].GetName("<ERROR>"), ch);
	
	send_to_char("`n\r\n", ch);

	if (!PRF_FLAGGED(ch, PRF_BRIEF) || ignore_brief || ROOM_FLAGGED(room, ROOM_DEATH)) {
		if (ROOM_FLAGGED(room, ROOM_PARSE)) {
			buf = get_buffer(MAX_STRING_LENGTH);
			strcpy(buf, world[room].GetDesc("<ERROR>"));
			master_parser(buf, ch, world + room, NULL);
			ch->Send("%s\r\n", buf);
			release_buffer(buf);
		} else
			send_to_char(world[room].GetDesc("<ERROR>"), ch);
	}
	
	/* autoexits */
	if (PRF_FLAGGED(ch, PRF_AUTOEXIT))
		do_auto_exits(ch, room);

	/* now list characters & objects */
	list_obj_to_char(world[room].contents, ch, 0, FALSE);
	list_char_to_char(world[room].people, ch);
	send_to_char("`n", ch);
	
}


void look_at_room(CharData * ch, int ignore_brief) {
	if (IN_ROOM(ch) == NOWHERE) {
		ch->to_room(0);
	}
	look_at_rnum(ch, IN_ROOM(ch), ignore_brief);
}


void list_scanned_chars(LList<CharData *> &list, CharData * ch, int distance, int door) {
	const char *	how_far[] = {
		"close by",
		"nearby",
		"to the",
		"far off",
		"far in the distance"
	};
	char			relation;
	CharData *		i;
	UInt32			count = 0, one_sent = 0;
	LListIterator<CharData *>	iter(list);

	while((i = iter.Next())) {
		if (ch->CanSee(i))
			count++;
	}

	if (!count)
		return;

	iter.Reset();
	while((i = iter.Next())) {
		if (!ch->CanSee(i))
			continue;
		
		relation = relation_colors[ch->GetRelation(i)];
		
		if (!one_sent)			ch->Send("You see `%c%s", relation, i->GetName());
		else 					ch->Send("`%c%s", relation, i->GetName());
		
		if (--count > 1)		send_to_char("`n, ", ch);
		else if (count == 1)	send_to_char(" `nand ", ch);
		else					ch->Send(" `n%s %s.\r\n", how_far[distance], dirs[door]);
		
		one_sent++;
	}
}



void look_in_direction(CharData * ch, SInt32 dir, SInt32 maxDist) {
	int		distance;
	RNum	room = IN_ROOM(ch);
	
	if (EXIT(ch, dir)) {
		send_to_char(EXIT(ch, dir)->GetDesc("You see nothing special.\r\n"), ch);
		
		if (EXIT(ch, dir)->GetKeyword()) {
			if (EXIT_FLAGGED(EXIT(ch, dir), EX_CLOSED))
				act("The $T is closed.\r\n", FALSE, ch, 0, fname(EXIT(ch, dir)->GetKeyword()), TO_CHAR);
			else if (EXIT_FLAGGED(EXIT(ch, dir), EX_ISDOOR))
				act("The $T is open.\r\n", FALSE, ch, 0, fname(EXIT(ch, dir)->GetKeyword()), TO_CHAR);
		}
		room = CAN_GO2(room, dir) ? EXIT2(room, dir)->to_room : NOWHERE;

		for (distance = 0; (room != NOWHERE) && (distance < maxDist); distance++) {
			list_scanned_chars(world[room].people, ch, distance, dir);

			room = CAN_GO2(room, dir) ? EXIT2(room, dir)->to_room : NOWHERE;
		}
	} else
		send_to_char("Nothing special there...\r\n", ch);
}



void look_in_obj(CharData * ch, char *arg) {
	ObjData *obj = NULL;
	CharData *dummy = NULL;
	int amt, bits;

	if (!*arg)
		send_to_char("Look in what?\r\n", ch);
	else if (!(bits = generic_find(arg, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP, ch, &dummy, &obj)))
		ch->Send("There doesn't seem to be %s %s here.\r\n", AN(arg), arg);
	else if ((GET_OBJ_TYPE(obj) != ITEM_DRINKCON) &&
			(GET_OBJ_TYPE(obj) != ITEM_FOUNTAIN) &&
			(GET_OBJ_TYPE(obj) != ITEM_CONTAINER)) {
		if (IS_GUN(obj) /* GET_OBJ_TYPE(obj) == ITEM_FIREWEAPON */)
			ch->Send("It contains %d units of ammunition.\r\n", GET_GUN_AMMO(obj));
		else if (GET_OBJ_TYPE(obj) == ITEM_MISSILE)
			ch->Send("It contains %d units of ammunition.\r\n", GET_OBJ_VAL(obj, 1));
		else
			ch->Send("There's nothing inside that!\r\n");
	} else {
		if (GET_OBJ_TYPE(obj) == ITEM_CONTAINER) {
			if (OBJVAL_FLAGGED(obj, CONT_CLOSED))
				send_to_char("It is closed.\r\n", ch);
			else {
				send_to_char(fname(SSData(obj->name)), ch);
				switch (bits) {
					case FIND_OBJ_INV:
						send_to_char(" (carried): \r\n", ch);
						break;
					case FIND_OBJ_ROOM:
						send_to_char(" (here): \r\n", ch);
						break;
					case FIND_OBJ_EQUIP:
						send_to_char(" (used): \r\n", ch);
						break;
				}

				list_obj_to_char(obj->contains, ch, 2, TRUE);
			}
		} else if (GET_OBJ_VAL(obj, 1) <= 0)		/* item must be a fountain or drink container */
			send_to_char("It is empty.\r\n", ch);
		else if (GET_OBJ_VAL(obj,0) <= 0 || GET_OBJ_VAL(obj,1)>GET_OBJ_VAL(obj,0))
			ch->Send("Its contents seem somewhat murky.\r\n"); /* BUG */
		else {
			char *buf2 = get_buffer(128);
			amt = (GET_OBJ_VAL(obj, 1) * 3) / GET_OBJ_VAL(obj, 0);
			sprinttype(GET_OBJ_VAL(obj, 2), color_liquid, buf2);
			ch->Send("It's %sfull of a %s liquid.\r\n", fullness[amt], buf2);
			release_buffer(buf2);
		}
	}
}



char *find_exdesc(char *word, ExtraDesc * list) {
	ExtraDesc *i;

	for (i = list; i; i = i->next)
		if (isname(word, SSData(i->keyword)))
			return SSData(i->description);

	return NULL;
}


/*
 * Given the argument "look at <target>", figure out what object or char
 * matches the target.  First, see if there is another char in the room
 * with the name.  Then check local objs for exdescs.
 */
void look_at_target(CharData * ch, char *arg, int read) {
	int bits, j, msg = 1;
	CharData *found_char = NULL;
	ObjData *obj = NULL, *found_obj = NULL;
	char *	desc;
	char *	number;
	bool	found = false;

	if (!ch->desc)
		return;
	else if (!*arg)
		send_to_char("Look at what?\r\n", ch);
	else if (read) {
		if (!(obj = get_obj_in_list_type(ITEM_BOARD, ch->carrying)))
			obj = get_obj_in_list_type(ITEM_BOARD, world[IN_ROOM(ch)].contents);
			
		if (obj) {
			number = get_buffer(MAX_INPUT_LENGTH);
			one_argument(arg, number);
			if (!*number)
				send_to_char("Read what?\r\n",ch);
			else if (isname(number, SSData(obj->name)))
				Board_show_board(GET_OBJ_VNUM(obj), ch);
			else if (!isdigit(*number) || !(msg = atoi(number)) || strchr(number,'.'))
				look_at_target(ch, arg, 0);
			else
				Board_display_msg(GET_OBJ_VNUM(obj), ch, msg);
			release_buffer(number);
		} else
			look_at_target(ch, arg, 0);
	} else {
		bits = generic_find(arg, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP |
				FIND_CHAR_ROOM, ch, &found_char, &found_obj);

		/* Is the target a character? */
		if (found_char) {
			look_at_char(found_char, ch);
			if (ch != found_char) {
				if (found_char->CanSee(ch))
					act("$n looks at you.", TRUE, ch, 0, found_char, TO_VICT);
				act("$n looks at $N.", TRUE, ch, 0, found_char, TO_NOTVICT);
			}
			return;
		}
		/* Does the argument match an extra desc in the room? */
		if ((desc = find_exdesc(arg, world[IN_ROOM(ch)].ex_description)) != NULL) {
			page_string(ch->desc, desc, 0);
			return;
		}
		/* Does the argument match an extra desc in the char's equipment? */
		for (j = 0; j < NUM_WEARS && !found; j++)
			if (GET_EQ(ch, j) && ch->CanSee(GET_EQ(ch, j)))
				if ((desc = find_exdesc(arg, GET_EQ(ch, j)->exDesc)) != NULL) {
					send_to_char(desc, ch);
					found = true;
				}
		/* Does the argument match an extra desc in the char's inventory? */
		START_ITER(ch_iter, obj, ObjData *, ch->carrying) {
			if (found)
				break;
			if (ch->CanSee(obj) && (desc = find_exdesc(arg, obj->exDesc))) {
				if(GET_OBJ_TYPE(obj) == ITEM_BOARD)
					Board_show_board(GET_OBJ_VNUM(obj), ch);
				else
					send_to_char(desc, ch);
				found = true;
			}
		} END_ITER(ch_iter);

		/* Does the argument match an extra desc of an object in the room? */
		START_ITER(rm_iter, obj, ObjData *, world[IN_ROOM(ch)].contents) {
			if (found)
				break;
			if (ch->CanSee(obj) && (desc = find_exdesc(arg, obj->exDesc))) {
				if(GET_OBJ_TYPE(obj) == ITEM_BOARD) 
					Board_show_board(GET_OBJ_VNUM(obj), ch);
				else
					send_to_char(desc, ch);
				found = true;
			}
		} END_ITER(rm_iter);
		
		if (bits) {			/* If an object was found back in generic_find */
			if (!found)	show_obj_to_char(found_obj, ch, 5);	/* Show no-description */
			else		show_obj_to_char(found_obj, ch, 6);	/* Find hum, glow etc */
		} else if (!found)
			send_to_char("You do not see that here.\r\n", ch);
	}
}


void look_out(CharData * ch) {
	ObjData * viewport, * vehicle;
	viewport = get_obj_in_list_type(ITEM_V_WINDOW, world[IN_ROOM(ch)].contents);
	if (!viewport) {
		send_to_char("There is no window to see out of.\r\n", ch);
		return;
	}
	vehicle = find_vehicle_by_vnum(GET_OBJ_VAL(viewport, 0));
	if (!vehicle || IN_ROOM(vehicle) == NOWHERE) {
		send_to_char("All you see is an infinite blackness.\r\n", ch);
		return;
	}
	look_at_rnum(ch, IN_ROOM(vehicle), FALSE);
	return;
}


ACMD(do_look) {
	int look_type;

	if (!ch->desc)
		return;

	if (GET_POS(ch) < POS_SLEEPING)
		send_to_char("You can't see anything but stars!\r\n", ch);
	else if (AFF_FLAGGED(ch, AFF_BLIND))
		send_to_char("You can't see a damned thing, you're blind!\r\n", ch);
	else if (IS_DARK(IN_ROOM(ch)) && !CAN_SEE_IN_DARK(ch)) {
		send_to_char("It is pitch black...\r\n", ch);
		list_char_to_char(world[IN_ROOM(ch)].people, ch);	/* glowing red eyes */
	} else {
		char * arg2 = get_buffer(MAX_INPUT_LENGTH);
		char * arg = get_buffer(MAX_INPUT_LENGTH);
		
		half_chop(argument, arg, arg2);

		if (subcmd == SCMD_READ) {										//	read
			if (!*arg)	send_to_char("Read what?\r\n", ch);
			else		look_at_target(ch, arg, 1);
		} else if (!*arg)												//	look
			look_at_room(ch, 1);
		else if (is_abbrev(arg, "in"))									//	look in
			look_in_obj(ch, arg2);
		else if (is_abbrev(arg, "out"))									//	look out
			look_out(ch);
		else if ((look_type = search_block(arg, dirs, FALSE)) >= 0)		//	look <dir>
			look_in_direction(ch, look_type, 5);
		else if (is_abbrev(arg, "at"))									//	look at
			look_at_target(ch, arg2, 0);
		else
			look_at_target(ch, arg, 0);									//	look <something>
			
		release_buffer(arg);
		release_buffer(arg2);
	}
}



ACMD(do_examine) {
	int bits;
	CharData *tmp_char;
	ObjData *tmp_object;
	char * arg = get_buffer(MAX_STRING_LENGTH);
	
	one_argument(argument, arg);

	if (!*arg) {
		send_to_char("Examine what?\r\n", ch);
	} else {
		look_at_target(ch, arg, 0);

		bits = generic_find(arg, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_CHAR_ROOM |
					FIND_OBJ_EQUIP, ch, &tmp_char, &tmp_object);

		if (tmp_object) {
			if ((GET_OBJ_TYPE(tmp_object) == ITEM_DRINKCON) ||
					(GET_OBJ_TYPE(tmp_object) == ITEM_FOUNTAIN) ||
					(GET_OBJ_TYPE(tmp_object) == ITEM_CONTAINER)) {
				send_to_char("When you look inside, you see:\r\n", ch);
				look_in_obj(ch, arg);
			}
		}
	}
	release_buffer(arg);
}


ACMD(do_gold) {
	ch->Send("You have %d mission point%s.\r\n", GET_MISSION_POINTS(ch), (GET_MISSION_POINTS(ch) != 1) ? "s" : "");
}


ACMD(do_score) {
	struct TimeInfoData playing_time;
	
	if (IS_NPC(ch))
		return;
	
	ch->Send(	"You are %d years old.%s\r\n"
				"You have %d(%d) hit and %d(%d) movement points.\r\n"
				"Your armor will absorb %d damage.\r\n"
				"You have have %d mission points.\r\n",
				GET_AGE(ch), (!age(ch).month && !age(ch).day) ? "  It's your birthday today." : "",
				GET_HIT(ch), GET_MAX_HIT(ch), GET_MOVE(ch), GET_MAX_MOVE(ch),
				GET_AC(ch),
				GET_MISSION_POINTS(ch));
				
	
	if (!IS_NPC(ch)) {
		int tempP = ch->player->special.PKills,
			tempM = ch->player->special.MKills,
			tempT = tempP + tempM;
		ch->Send("You have killed %d beings (%d (%2d%%) players, %d (%2d%%) mobs).\r\n",
				tempP + tempM, tempP, tempT ? (tempP * 100)/tempT : 0,
				tempM, tempT ? (tempM * 100)/tempT : 0);
		
		tempP = ch->player->special.PDeaths;
		tempM = ch->player->special.MDeaths;
		tempT = tempP + tempM;
		ch->Send("You have been killed by %d beings (%d (%2d%%) players, %d (%2d%%) mobs).\r\n",
				tempP + tempM, tempP, tempT ? (tempP * 100)/tempT : 0,
				tempM, tempT ? (tempM * 100)/tempT : 0);
		
		playing_time = real_time_passed((time(0) - ch->player->time.logon) + ch->player->time.played, 0);
		ch->Send("You have been playing for %d days and %d hours.\r\n"
				"This ranks you as %s %s (level %d).\r\n",
				playing_time.day, playing_time.hours,
				GET_TITLE(ch), ch->GetName(), GET_LEVEL(ch));
	}

	switch (GET_POS(ch)) {
		case POS_DEAD:		send_to_char("You are DEAD!\r\n", ch);										break;
		case POS_MORTALLYW:	send_to_char("You are mortally wounded!  You should seek help!\r\n", ch);	break;
		case POS_INCAP:		send_to_char("You are incapacitated, slowly fading away...\r\n", ch);		break;
		case POS_STUNNED:	send_to_char("You are stunned!  You can't move!\r\n", ch);					break;
		case POS_SLEEPING:	send_to_char("You are sleeping.\r\n", ch);									break;
		case POS_RESTING:	send_to_char("You are resting.\r\n", ch);									break;
		case POS_SITTING:	send_to_char("You are sitting.\r\n", ch);									break;
		case POS_FIGHTING:
			if (FIGHTING(ch))	ch->Send("You are fighting %s.\r\n", PERS(FIGHTING(ch), ch));
			else				send_to_char("You are fighting thin air.\r\n", ch);
			break;
		case POS_STANDING:	send_to_char("You are standing.\r\n", ch);									break;
		default:	send_to_char("You are floating.\r\n", ch);											break;
	}

	if (GET_COND(ch, DRUNK) > 10)			send_to_char("You are intoxicated.\r\n", ch);
	if (GET_COND(ch, FULL) == 0)			send_to_char("You are hungry.\r\n", ch);
	if (GET_COND(ch, THIRST) == 0)			send_to_char("You are thirsty.\r\n", ch);
	if (AFF_FLAGGED(ch, AFF_BLIND))			send_to_char("You have been blinded!\r\n", ch);
	if (AFF_FLAGGED(ch, AFF_INVISIBLE))		send_to_char("You are invisible.\r\n", ch);
	if (AFF_FLAGGED(ch, AFF_DETECT_INVIS))	send_to_char("You are sensitive to the presence of invisible things.\r\n", ch);
	if (AFF_FLAGGED(ch, AFF_SANCTUARY))		send_to_char("You are protected by Sanctuary.\r\n", ch);
	if (AFF_FLAGGED(ch, AFF_POISON))		send_to_char("You are poisoned!\r\n", ch);
	if (AFF_FLAGGED(ch, AFF_CHARM))			send_to_char("You have been charmed!\r\n", ch);
	if (AFF_FLAGGED(ch, AFF_INFRAVISION))	send_to_char("You have infrared vision capabilities.\r\n", ch);
	if (PRF_FLAGGED(ch, PRF_SUMMONABLE))	send_to_char("You are summonable by other players.\r\n", ch);
}


ACMD(do_inventory) {
	send_to_char("You are carrying:\r\n", ch);
	list_obj_to_char(ch->carrying, ch, 1, TRUE);
}


ACMD(do_equipment) {
	int		i;
	bool	found = false;

	send_to_char("You are using:\r\n", ch);
	for (i = 0; i < NUM_WEARS; i++) {
		if (GET_EQ(ch, i)) {
			if (ch->CanSee(GET_EQ(ch, i))) {
				send_to_char(where[wear_message_number(GET_EQ(ch, i),i)], ch);
				show_obj_to_char(GET_EQ(ch, i), ch, 1);
				found = true;
			} else {
				send_to_char(where[wear_message_number(GET_EQ(ch, i),i)], ch);
				send_to_char("Something.\r\n", ch);
				found = true;
			}
		}
	}
	if (!found)	send_to_char(" Nothing.\r\n", ch);
}


ACMD(do_time) {
	extern UInt32 pulse;
	char *suf; //, *buf = get_buffer(256);
	int weekday, day, minute, second;


	/* 30 days in a month */
	weekday = ((int)(30.417 * time_info.month) + time_info.day + 1) % 7;

	day = time_info.day + 1;	/* day in [1..30] */

	if (day == 1)					suf = "st";
	else if (day == 2)				suf = "nd";
	else if (day == 3)				suf = "rd";
	else if (day < 20)				suf = "th";
	else if ((day % 10) == 1)		suf = "st";
	else if ((day % 10) == 2)		suf = "nd";
	else if ((day % 10) == 3)		suf = "rd";
	else							suf = "th";
	
	minute = (int)((double)(pulse % PULSES_PER_MUD_HOUR) / 12.5);
	second = (int)(((pulse % PULSES_PER_MUD_HOUR) - (minute * 12.5)) * 4.8);
	
	ch->Send("It is %02d:%02d:%02d (%02d:%02d %cm) on %s\r\n"
			 "%s %d%s, %d.\r\n",
			 time_info.hours, minute, second,
			 (time_info.hours <= 12) ? ((time_info.hours == 0) ? 12 : time_info.hours) :
			 (time_info.hours - 12), minute,
			 (time_info.hours < 12) ? 'a' : 'p',
			 weekdays[weekday],
			 month_name[time_info.month], day, suf, time_info.year);
}


ACMD(do_weather) {
	extern struct zone_data *zone_table;
	static char *sky_look[] = {
		"cloudless",
		"cloudy",
		"rainy",
		"lit by flashes of lightning"
	};

	if (OUTSIDE(ch)) {
		ch->Send("The sky is %s and %s.\r\n", sky_look[zone_table[world[IN_ROOM(ch)].zone].sky],
				(zone_table[world[IN_ROOM(ch)].zone].change >= 0 ?
				"you feel a warm wind from south" :
				"your foot tells you bad weather is due"));
	} else
		send_to_char("You have no feeling about the weather at all.\r\n", ch);
}


ACMD(do_help) {
	HelpElement *this_help;
	char *entry;

	if (!ch->desc)	return;
	
	skip_spaces(argument);

	if (!*argument)
		page_string(ch->desc, help, 0);
	else if (!help_table)
		send_to_char("No help available.\r\n", ch);
	else if (!(this_help = find_help(argument))) {
		send_to_char("There is no help on that word.\r\n", ch);
		log("HELP: %s tried to get help on %s", ch->RealName(), argument);
	} else if (this_help->min_level > GET_LEVEL(ch))
		send_to_char("There is no help on that word.\r\n", ch);
	else {
		entry = get_buffer(MAX_STRING_LENGTH);
		sprintf(entry, "%s\r\n%s", this_help->keywords, this_help->entry);
		page_string(ch->desc, entry, true);
		release_buffer(entry);
	}
}


/*********************************************************************
* New 'do_who' by Daniel Koepke [aka., "Argyle Macleod"] of The Keep *
******************************************************************* */

char *WHO_USAGE =
	"Usage: who [minlev[-maxlev]] [-n name] [-c races] [-rzmspt]\r\n"
	"\r\n"
	"Races: (H)uman, (S)ynthetic, (P)redator, (A)lien\r\n"
	"\r\n"
	" Switches: \r\n"
	"\r\n"
	"  -r = who is in the current room\r\n"
	"  -z = who is in the current zone\r\n"
	"\r\n"
	"  -m = only show those on mission\r\n"
	"  -s = only show staff\r\n"
	"  -p = only show players\r\n"
	"  -t = only show traitors\r\n"
	"\r\n";

UInt32	boot_high = 0;
ACMD(do_who) {
	DescriptorData *d;
	CharData *		wch;
	RNum			clan;
	SInt32			low = 0,
					high = LVL_CODER,
					i;
	bool			who_room = false,
					who_zone = false,
					who_mission = false,
					traitors = false,
					no_staff = false,
					no_players = false;
	UInt32			staff = 0,
					players = 0;
	Flags			showrace = 0;

	int  players_clan = 0, players_clannum = 0, ccmd = 0;
	extern char *level_string[];

	char	*buf = get_buffer(MAX_STRING_LENGTH),
			*arg = get_buffer(MAX_STRING_LENGTH),
			*name_search = get_buffer(MAX_INPUT_LENGTH),
			*staff_buf, *player_buf,
			mode;

	skip_spaces(argument);
	strcpy(buf, argument);
	
	while (*buf) {
		half_chop(buf, arg, buf);
		if (isdigit(*arg)) {
			sscanf(arg, "%d-%d", &low, &high);
		} else if (*arg == '-') {
			mode = *(arg + 1);	/* just in case; we destroy arg in the switch */
			switch (mode) {
				case 't':				// Only traitors
					traitors = true;
					break;
				case 'z':				// In Zone
					who_zone = true;
					break;
				case 'm':				// Only Missoin
					who_mission = true;
					break;
				case 'l':				// Range
					half_chop(buf, arg, buf);
					sscanf(arg, "%d-%d", &low, &high);
					break;
				case 'n':				// Name
					half_chop(buf, name_search, buf);
					break;
				case 'r':				// In room
					who_room = true;
					break;
				case 's':				// Only staff
					no_players = true;
					break;
				case 'p':				// Only Players
					no_staff = true;
					break;
				case 'c':				//	Race specific...
					half_chop(buf, arg, buf);
					for (i = 0; i < strlen(arg); i++)
						showrace |= find_race_bitvector(arg[i]);
					break;
				default:
					send_to_char(WHO_USAGE, ch);
					release_buffer(buf);
					release_buffer(arg);
					release_buffer(name_search);
					return;
			}				/* end of switch */
		} else {			/* endif */
			send_to_char(WHO_USAGE, ch);
			release_buffer(buf);
			release_buffer(arg);
			release_buffer(name_search);
			return;
		}
	}
	
	staff_buf = get_buffer(MAX_STRING_LENGTH);
	player_buf = get_buffer(MAX_STRING_LENGTH);
	
	strcpy(staff_buf,	"Staff currently online\r\n"
						"----------------------\r\n");
	strcpy(player_buf,	"Players currently online\r\n"
						"------------------------\r\n");

	START_ITER(iter, d, DescriptorData *, descriptor_list) {
		if (STATE(d) != CON_PLAYING)		continue;
		if (!(wch = d->Original()))			continue;
		if (!ch->CanSee(wch))				continue;
		if ((GET_LEVEL(wch) < low) || (GET_LEVEL(wch) > high))
			continue;
		if ((no_staff && IS_STAFF(wch)) || (no_players && !IS_STAFF(wch)))
			continue;
		if (*name_search && str_cmp(wch->GetName(), name_search) && !strstr(GET_TITLE(wch), name_search))
			continue;
		if (traitors && !PLR_FLAGGED(wch, PLR_TRAITOR))
			continue;
		if (who_mission && !CHN_FLAGGED(wch, Channel::Mission))
			continue;
		if (who_zone && world[IN_ROOM(ch)].zone != world[IN_ROOM(wch)].zone)
			continue;
		if (who_room && (IN_ROOM(wch) != IN_ROOM(ch)))
			continue;
		if (showrace && !(showrace & (1 << (GET_RACE(wch)))))
			continue;

		*buf = '\0';
		
		if (IS_STAFF(wch)) {
			sprintf(buf, "`y %s %s %s`n", level_string[GET_LEVEL(wch)-LVL_IMMORT],
					wch->GetName(), GET_TITLE(wch));
			staff++;
		} else {
			sprintf(buf, "`n[%3d %s] `%c%s %s`n", GET_LEVEL(wch), RACE_ABBR(wch),
					relation_colors[ch->GetRelation(wch)], GET_TITLE(wch), wch->GetName());
			players++;
		}
		
		if (GET_INVIS_LEV(wch))						sprintf(buf + strlen(buf), " (`wi%d`n)", GET_INVIS_LEV(wch));
		else if (AFF_FLAGGED(wch, AFF_INVISIBLE))	strcat(buf, " (`winvis`n)");
		
		if (PLR_FLAGGED(wch, PLR_MAILING))			strcat(buf, " (`bmailing`n)");
		else if (PLR_FLAGGED(wch, PLR_WRITING))		strcat(buf, " (`rwriting`n)");

		if (CHN_FLAGGED(wch, Channel::NoShout))		strcat(buf, " (`gdeaf`n)");
		if (CHN_FLAGGED(wch, Channel::NoTell))		strcat(buf, " (`gnotell`n)");
		if (CHN_FLAGGED(wch, Channel::Mission))		strcat(buf, " (`mmission`n)");
		if (PLR_FLAGGED(wch, PLR_TRAITOR))			strcat(buf, " (`RTRAITOR`n)");
		if (((clan = real_clan(GET_CLAN(wch))) != -1) && (GET_CLAN_LEVEL(wch) > CLAN_APPLY)) {
				sprintf(buf + strlen(buf), " <`C%s`n>", clan_index[clan].name);
		}
		if (GET_AFK(wch))							strcat(buf, " `c[AFK]");
		strcat(buf, "`n\r\n");
		
		if (IS_STAFF(wch))		strcat(staff_buf, buf);
		else					strcat(player_buf, buf);
	} END_ITER(iter);
	
	*buf = '\0';
	
	if (staff) {
		strcat(buf, staff_buf);
		strcat(buf, "\r\n");
	}
	if (players) {
		strcat(buf, player_buf);
		strcat(buf, "\r\n");
	}
	
	if ((staff + players) == 0) {
		strcat(buf, "No staff or players are currently visible to you.\r\n");
	}
	if (staff)
		sprintf(buf + strlen(buf), "There %s %d visible staff%s", (staff == 1 ? "is" : "are"), staff, players ? " and there" : ".");
	if (players)
		sprintf(buf + strlen(buf), "%s %s %d visible player%s.", staff ? "" : "There", (players == 1 ? "is" : "are"), players, (players == 1 ? "" : "s"));
	strcat(buf, "\r\n");
	
	if ((staff + players) > boot_high)
		boot_high = staff+players;

	sprintf(buf + strlen(buf), "There is a boot time high of %d player%s.\r\n", boot_high, (boot_high == 1 ? "" : "s"));
	
	page_string(ch->desc, buf, 1);
	
	
	release_buffer(buf);
	release_buffer(arg);
	release_buffer(name_search);
	release_buffer(staff_buf);
	release_buffer(player_buf);
}


#define USERS_FORMAT \
"format: users [-l minlevel[-maxlevel]] [-n name] [-h host] [-o] [-p] [-d] [-c] [-m]\r\n"

ACMD(do_users) {
	CharData *	tch;
	DescriptorData *d, *match;
	SInt32		low = 0,
				high = LVL_CODER,
				num_can_see = 0;
	bool 		traitors = false,
				playing = false,
				deadweight = false,
				complete = false,
				matching = false;
	/*
	 * I think this function needs more (char *)'s, it doesn't have enough.
	 * XXX: Help! I'm drowning in (char *)'s!
	 */
	char		*buf = get_buffer(MAX_STRING_LENGTH),
				*arg = get_buffer(MAX_INPUT_LENGTH),
				*name_search = get_buffer(MAX_INPUT_LENGTH),
				*host_search = get_buffer(MAX_INPUT_LENGTH),
				*idletime,
				*timeptr,
				mode;
	const char *state;

	strcpy(buf, argument);
	while (*buf) {
		half_chop(buf, arg, buf);
		if (*arg == '-') {
			mode = *(arg + 1);  /* just in case; we destroy arg in the switch */
			switch (mode) {
				case 'm':	matching = true;							break;
				case 'c':	complete = true;							break;
				case 'o':	traitors = true;	playing = true;			break;
				case 'p':	playing = true;								break;
				case 'd':	deadweight = true;							break;
				case 'l':
					playing = true;
					half_chop(buf, arg, buf);
					sscanf(arg, "%d-%d", &low, &high);
					break;
				case 'n':
					playing = true;
					half_chop(buf, name_search, buf);
					break;
				case 'h':
					playing = true;
					half_chop(buf, host_search, buf);
					break;
				default:
					send_to_char(USERS_FORMAT, ch);
					release_buffer(buf);
					release_buffer(arg);
					release_buffer(name_search);
					release_buffer(host_search);
					return;
			}
		} else {
			send_to_char(USERS_FORMAT, ch);
			release_buffer(buf);
			release_buffer(arg);
			release_buffer(name_search);
			release_buffer(host_search);
			return;
		}
	}
	
	idletime = get_buffer(10);
	
	strcpy(buf, "Num Race      Name         State          Idl Login@   Site\r\n");
	strcat(buf, "--- --------- ------------ -------------- --- -------- ------------------------\r\n");

	one_argument(argument, arg);

	LListIterator<DescriptorData *>	iter(descriptor_list);
	LListIterator<DescriptorData *>	match_iter(descriptor_list);
	
	while ((d = iter.Next())) {
		tch = d->Original();
		if ((STATE(d) == CON_PLAYING)) {
			if (deadweight)												continue;
			if (!tch)													continue;
			if (*host_search && !strstr(d->host, host_search))			continue;
			if (*name_search && str_cmp(tch->GetName(), name_search))	continue;
			if (!ch->CanSee(tch))										continue;
			if ((GET_LEVEL(tch) < low) || (GET_LEVEL(tch) > high))		continue;
			if (traitors && !PLR_FLAGGED(tch, PLR_TRAITOR))				continue;
		} else {
			if (playing)												continue;
			if (!ch->CanSee(tch))										continue;
		}
		if (matching) {
			if (!d->host)												continue;
			match_iter.Reset();
			while ((match = match_iter.Next())) {
				if (!match->host || (match == d))						continue;
				if (!str_cmp(match->host, d->host))
					break;
			}
			if (!match)													continue;
		}
		
		
		timeptr = asctime(localtime(&d->login_time));
		timeptr += 11;
		*(timeptr + 8) = '\0';

		state = ((STATE(d) == CON_PLAYING) && d->original) ? "Switched" : connected_types[STATE(d)];

		if ((STATE(d) == CON_PLAYING) && d->character && !IS_STAFF(d->character))
			sprintf(idletime, "%3d", d->character->player->timer * SECS_PER_MUD_HOUR / SECS_PER_REAL_MIN);
		else
			strcpy(idletime, "");

		strcat(buf, STATE(d) == CON_PLAYING ? "`n" : "`g");

		if (tch && SSData(tch->general.name))
			sprintf(buf + strlen(buf), "%3d [%3d %3s] %-12s %-14.14s %-3s %-8s ", d->desc_num,
					GET_LEVEL(tch), RACE_ABBR(tch), SSData(tch->general.name), state, idletime, timeptr);
		else
			sprintf(buf + strlen(buf), "%3d     -     %-12s %-14.14s %-3s %-8s ", d->desc_num,
					"UNDEFINED", state, idletime, timeptr);

		sprintf(buf + strlen(buf), complete ? "[%-22s]\r\n" : "[%-22.22s]\r\n", (d->host && *d->host) ? d->host : "Hostname unknown");

		num_can_see++;
	}

	sprintf(buf + strlen(buf), "\r\n`n%d visible sockets connected.\r\n", num_can_see);

	page_string(ch->desc, buf, true);

	release_buffer(buf);
	release_buffer(arg);
	release_buffer(name_search);
	release_buffer(host_search);
	release_buffer(idletime);
}


/* Generic page_string function for displaying text */
ACMD(do_gen_ps) {
	switch (subcmd) {
		case SCMD_CREDITS:	page_string(ch->desc, credits, 0);			break;
		case SCMD_NEWS:		page_string(ch->desc, news, 0);				break;
		case SCMD_INFO:		page_string(ch->desc, info, 0);				break;
		case SCMD_WIZLIST:	page_string(ch->desc, wizlist, 0);			break;
		case SCMD_IMMLIST:	page_string(ch->desc, immlist, 0);			break;
		case SCMD_HANDBOOK:	page_string(ch->desc, handbook, 0);			break;
		case SCMD_POLICIES:	page_string(ch->desc, policies, 0);			break;
		case SCMD_MOTD:		page_string(ch->desc, motd, 0);				break;
		case SCMD_IMOTD:	page_string(ch->desc, imotd, 0);			break;
		case SCMD_CLEAR:	send_to_char("\033[H\033[J", ch);			break;
		case SCMD_VERSION:	send_to_char(circlemud_version, ch);		break;
		case SCMD_WHOAMI:	ch->Send("%s\r\n", ch->GetName());			break;
	}
}


void perform_mortal_where(CharData * ch, char *arg) {
	CharData *i;
	DescriptorData *d;
	
	if (!*arg) {
		send_to_char("Players in your Zone\r\n--------------------\r\n", ch);
		START_ITER(iter, d, DescriptorData *, descriptor_list) {
			if (STATE(d) == CON_PLAYING) {
				i = d->Original();
				if (i && ch->CanSee(i) && (IN_ROOM(i) != NOWHERE) &&
						(world[IN_ROOM(ch)].zone == world[IN_ROOM(i)].zone)) {
					ch->Send("%-20s - %s\r\n", i->GetName(), world[IN_ROOM(i)].GetName("<ERROR>"));
				}
			}
		} END_ITER(iter);
	} else {			/* print only FIRST char, not all. */
		START_ITER(iter, i, CharData *, character_list) {
			if (world[IN_ROOM(i)].zone == world[IN_ROOM(ch)].zone && ch->CanSee(i) &&
					(IN_ROOM(i) != NOWHERE) && isname(arg, i->GetName())) {
				ch->Send("%-25s - %s\r\n", i->GetName(), world[IN_ROOM(i)].GetName("<ERROR>"));
				END_ITER(iter);
				return;
			}
		} END_ITER(iter);
		send_to_char("No-one around by that name.\r\n", ch);
	}
}


void print_object_location(int num, ObjData * obj, CharData * ch, int recur) {
	if (num > 0)	ch->Send("O%3d. %-25s - ", num, SSData(obj->shortDesc));
	else			ch->Send("%33s", " - ");

	if (IN_ROOM(obj) > NOWHERE) {
		ch->Send("[%5d] %s\r\n", world[IN_ROOM(obj)].number, world[IN_ROOM(obj)].GetName("<ERROR>"));
	} else if (obj->carried_by) {
		ch->Send("carried by %s\r\n", PERS(obj->carried_by, ch));
	} else if (obj->worn_by) {
		ch->Send("worn by %s\r\n", PERS(obj->worn_by, ch));
	} else if (obj->in_obj) {
		ch->Send("inside %s%s\r\n", SSData(obj->in_obj->shortDesc), (recur ? ", which is" : " "));
		if (recur)
			print_object_location(0, obj->in_obj, ch, recur);
	} else
		ch->Send("in an unknown location\r\n");
}


void perform_immort_where(CharData * ch, char *arg) {
	CharData *i;
	ObjData *k;
	DescriptorData *d;
	int num = 0, found = 0;

	if (!*arg) {
		send_to_char("Players\r\n-------\r\n", ch);
		START_ITER(iter, d, DescriptorData *, descriptor_list) {
			if (STATE(d) == CON_PLAYING) {
				i = d->Original();
				if (i && ch->CanSee(i) && (IN_ROOM(i) != NOWHERE)) {
					if (d->original)
						ch->Send("%-20s - [%5d] %s (in %s)\r\n",
								i->GetName(), world[IN_ROOM(d->character)].number,
								world[IN_ROOM(d->character)].GetName("<ERROR>"), d->character->GetName());
					else
						ch->Send("%-20s - [%5d] %s\r\n", i->GetName(),
								world[IN_ROOM(i)].number, world[IN_ROOM(i)].GetName("<ERROR>"));
				}
			}
		} END_ITER(iter);
	} else {
		START_ITER(ch_iter, i, CharData *, character_list) {
			if (ch->CanSee(i) && (IN_ROOM(i) != NOWHERE) && isname(arg, SSData(i->general.name))) {
				found = 1;
				ch->Send("M%3d. %-25s - [%5d] %s\r\n", ++num, i->GetName(),
						world[IN_ROOM(i)].number, world[IN_ROOM(i)].GetName("<ERROR>"));
			}
		} END_ITER(ch_iter);
		num = 0;
		START_ITER(obj_iter, k, ObjData *, object_list) {
			if (ch->CanSee(k) && isname(arg, SSData(k->name)) &&
					(!k->carried_by || ch->CanSee(k->carried_by))) {
				found = 1;
				print_object_location(++num, k, ch, TRUE);
			}
		} END_ITER(obj_iter);
		if (!found)
		send_to_char("Couldn't find any such thing.\r\n", ch);
	}
}



ACMD(do_where) {
	char * arg = get_buffer(MAX_STRING_LENGTH);
	one_argument(argument, arg);

	if (NO_STAFF_HASSLE(ch))
		perform_immort_where(ch, arg);
	else
		perform_mortal_where(ch, arg);
	release_buffer(arg);
}



ACMD(do_levels) {
	char * buf;
	int	i;
	int final_level = GET_LEVEL(ch) < LVL_IMMORT ? 10 : 15;
	
	if (IS_NPC(ch) || !ch->desc) {
		send_to_char("You ain't nothin' but a hound-dog.\r\n", ch);
		return;
	}

	buf = get_buffer(MAX_STRING_LENGTH);
	
	for (i = 1 ; i <= final_level; i++) {
		sprintf(buf + strlen(buf), "[%3d] : ", (i <= 10) ? ((i * 10) - 9) : (i + 90));
		switch (GET_SEX(ch)) {
			case SEX_MALE:
			case SEX_NEUTRAL:
				strcat(buf, titles[(int) GET_RACE(ch)][i].title_m);
				break;
			case SEX_FEMALE:
				strcat(buf, titles[(int) GET_RACE(ch)][i].title_f);
				break;
			default:
				send_to_char("Oh dear.  You seem to be sexless.\r\n", ch);
				release_buffer(buf);
				return;
		}
		strcat(buf, "\r\n");
	}
	page_string(ch->desc, buf, true);
	release_buffer(buf);
}


ACMD(do_consider) {
	CharData *victim;
	int diff;
	char *buf = get_buffer(MAX_STRING_LENGTH);

	one_argument(argument, buf);

	if (!(victim = get_char_room_vis(ch, buf))) {
		send_to_char("Consider killing who?\r\n", ch);
		release_buffer(buf);
		return;
	}
	
	release_buffer(buf);
	
	if (victim == ch) {
		send_to_char("Easy!  Very easy indeed!\r\n", ch);
		return;
	}
	
	if (!IS_NPC(victim)) {
		send_to_char("Would you like to borrow a cross and a shovel?\r\n", ch);
		return;
	}
	diff = (GET_LEVEL(victim) - GET_LEVEL(ch));

	if (diff <= -10)		send_to_char("Now where did that chicken go?\r\n", ch);
	else if (diff <= -5)	send_to_char("You could do it with a needle!\r\n", ch);
	else if (diff <= -2)	send_to_char("Easy.\r\n", ch);
	else if (diff <= -1)	send_to_char("Fairly easy.\r\n", ch);
	else if (diff == 0)		send_to_char("The perfect match!\r\n", ch);
	else if (diff <= 1)		send_to_char("You would need some luck!\r\n", ch);
	else if (diff <= 2)		send_to_char("You would need a lot of luck!\r\n", ch);
	else if (diff <= 3)		send_to_char("You would need a lot of luck and great equipment!\r\n", ch);
	else if (diff <= 5)		send_to_char("Do you feel lucky, punk?\r\n", ch);
	else if (diff <= 10)	send_to_char("Are you mad!?\r\n", ch);
	else if (diff <= 99)	send_to_char("You ARE mad!\r\n", ch);
	else 					send_to_char("Why not jump in a death trap?  It's slower.\r\n", ch);
}



ACMD(do_diagnose) {
	CharData *vict;
	char *buf = get_buffer(MAX_STRING_LENGTH);

	one_argument(argument, buf);

	if (*buf) {
		if (!(vict = get_char_room_vis(ch, buf))) {
			send_to_char(NOPERSON, ch);
			release_buffer(buf);
			return;
		} else
			diag_char_to_char(vict, ch);
	} else {
		if (FIGHTING(ch))
			diag_char_to_char(FIGHTING(ch), ch);
		else
			send_to_char("Diagnose who?\r\n", ch);
	}
	release_buffer(buf);
}


ACMD(do_toggle) {
	char *buf;

	if (IS_NPC(ch))	return;

	buf = get_buffer(4);

	if (GET_WIMP_LEV(ch) == 0)	strcpy(buf, "OFF");
	else						sprintf(buf, "%-3d", GET_WIMP_LEV(ch));

	ch->Send(
			"`y[`GGeneral config`y]`n\r\n"
			"     Brief Mode: %-3s    "" Summon Protect: %-3s    ""   Compact Mode: %-3s\r\n"
			"          Color: %-3s    ""   Repeat Comm.: %-3s    ""     Show Exits: %-3s\r\n"
			"`y[`GChannels      `y]`n\r\n"
			"           Chat: %-3s    ""          Music: %-3s    ""          Grats: %-3s\r\n"
			"           Info: %-3s    ""          Shout: %-3s    ""           Tell: %-3s\r\n"
			"        Mission: %-3s\r\n"
			"`y[`GGame Specifics`y]`n\r\n"
			"     Wimp Level: %-3s\r\n",

			ONOFF(PRF_FLAGGED(ch, PRF_BRIEF)),	ONOFF(!PRF_FLAGGED(ch, PRF_SUMMONABLE)),ONOFF(PRF_FLAGGED(ch, PRF_COMPACT)),
			ONOFF(PRF_FLAGGED(ch, PRF_COLOR)),	YESNO(!PRF_FLAGGED(ch, PRF_NOREPEAT)),	ONOFF(PRF_FLAGGED(ch, PRF_AUTOEXIT)),
			
			ONOFF(!CHN_FLAGGED(ch, Channel::NoChat)),ONOFF(!CHN_FLAGGED(ch, Channel::NoMusic)),	ONOFF(!CHN_FLAGGED(ch, Channel::NoGratz)),
			ONOFF(!CHN_FLAGGED(ch, Channel::NoInfo)),ONOFF(!CHN_FLAGGED(ch, Channel::NoShout)),	ONOFF(!CHN_FLAGGED(ch, Channel::NoTell)),
			YESNO(CHN_FLAGGED(ch, Channel::Mission)),
			
			buf
	);
	
	release_buffer(buf);
}


struct sort_struct {
	SInt32	sort_pos;
	Flags	type;
} *cmd_sort_info = NULL;

int num_of_cmds;

#define TYPE_CMD		(1 << 0)
#define TYPE_SOCIAL		(1 << 1)
#define TYPE_STAFFCMD	(1 << 2)

void sort_commands(void) {
	int a, b, tmp;

	num_of_cmds = 0;

	// first, count commands (num_of_commands is actually one greater than the
	//	number of commands; it inclues the '\n').
	while (*complete_cmd_info[num_of_cmds].command != '\n')
		num_of_cmds++;

	//	check if there was an old sort info.. then free it -- aedit -- M. Scott
	if (cmd_sort_info) free(cmd_sort_info);
	//	create data array
	CREATE(cmd_sort_info, struct sort_struct, num_of_cmds);

	//	initialize it
	for (a = 1; a < num_of_cmds; a++) {
		cmd_sort_info[a].sort_pos = a;
		if (complete_cmd_info[a].command_pointer == do_action)
			cmd_sort_info[a].type = TYPE_SOCIAL;
		if (IS_STAFFCMD(a) || (complete_cmd_info[a].minimum_level >= LVL_IMMORT))
			cmd_sort_info[a].type |= TYPE_STAFFCMD;
		if (!cmd_sort_info[a].type)
			cmd_sort_info[a].type = TYPE_CMD;
	}

	/* the infernal special case */
	cmd_sort_info[find_command("insult")].type = TYPE_SOCIAL;

	/* Sort.  'a' starts at 1, not 0, to remove 'RESERVED' */
	for (a = 1; a < num_of_cmds - 1; a++) {
		for (b = a + 1; b < num_of_cmds; b++) {
			if (strcmp(complete_cmd_info[cmd_sort_info[a].sort_pos].command,
					complete_cmd_info[cmd_sort_info[b].sort_pos].command) > 0) {
				tmp = cmd_sort_info[a].sort_pos;
				cmd_sort_info[a].sort_pos = cmd_sort_info[b].sort_pos;
				cmd_sort_info[b].sort_pos = tmp;
			}
		}
	}
}


ACMD(do_commands) {
	int no, i, cmd_num;
	int wizhelp = 0, socials = 0;
	CharData *vict;
	char *arg = get_buffer(MAX_STRING_LENGTH);
	char *buf;
	Flags	type;

	one_argument(argument, arg);

	if (*arg) {
		if (!(vict = get_char_vis(ch, arg)) || IS_NPC(vict)) {
			send_to_char("Who is that?\r\n", ch);
			release_buffer(arg);
			return;
		}
		if (!IS_STAFF(ch) && (GET_LEVEL(ch) < GET_LEVEL(vict))) {
			send_to_char("You can't see the commands of people above your level.\r\n", ch);
			release_buffer(arg);
			return;
		}
	} else
		vict = ch;

	release_buffer(arg);
	
	if (subcmd == SCMD_SOCIALS)			type = TYPE_SOCIAL;
	else if (subcmd == SCMD_WIZHELP)	type = TYPE_STAFFCMD;
	else								type = TYPE_CMD;

	buf = get_buffer(MAX_STRING_LENGTH);

	sprintf(buf, "The following %s%s are available to %s:\r\n",
			(subcmd == SCMD_WIZHELP) ? "privileged " : "",
			(subcmd == SCMD_SOCIALS) ? "socials" : "commands",
			vict == ch ? "you" : vict->RealName());

	/* cmd_num starts at 1, not 0, to remove 'RESERVED' */
	for (no = 1, cmd_num = 1; cmd_num < num_of_cmds; cmd_num++) {
		i = cmd_sort_info[cmd_num].sort_pos;
		
		if (!IS_SET(cmd_sort_info[i].type, type))
			continue;
		
		if ((GET_LEVEL(vict) < complete_cmd_info[i].minimum_level) /* && !IS_STAFF(vict) */)
			continue;
		
		if (IS_NPCCMD(i) && !IS_NPC(vict))
			continue;
		
		if ((subcmd == SCMD_WIZHELP) && IS_STAFFCMD(i) && !STF_FLAGGED(vict, complete_cmd_info[i].staffcmd))
			continue;
		if ((subcmd == SCMD_WIZHELP) && !IS_STAFFCMD(i) && (complete_cmd_info[i].minimum_level < LVL_IMMORT))
			continue;
		
		sprintf(buf + strlen(buf), "%-11s", complete_cmd_info[i].command);
		if (!(no % 7))
			strcat(buf, "\r\n");
		no++;
	}
	
	strcat(buf, "\r\n");
	
	page_string(ch->desc, buf, true);
	
	release_buffer(buf);
}


ACMD(do_attributes) {
	ch->Send("                 Character attributes for %s\r\n"
			 "\r\n"
			 "Level: %d   Race: %s\r\n"
			 "Age: %d yrs / %d mths  Height: %d inches  Weight: %d lbs\r\n"
			 "Str: %d       Int: %d       Per: %d\r\n"
			 "Agl: %d       Con: %d\r\n"
			 "Armor Absorb: %d\r\n"
			 "Hitroll: %d   Damroll: %d\r\n",
			 
			ch->GetName(),
			GET_LEVEL(ch), RACE_ABBR(ch),
			age(ch).year, age(ch).month, GET_HEIGHT(ch), GET_WEIGHT(ch),
			GET_STR(ch), GET_INT(ch), GET_PER(ch),
			GET_AGI(ch), GET_CON(ch),
			GET_AC(ch),
			(ch)->points.hitroll, (ch)->points.damroll);
}
