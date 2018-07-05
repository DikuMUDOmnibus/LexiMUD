/*************************************************************************
*   File: act.wizard.c++             Part of Aliens vs Predator: The MUD *
*  Usage: Staff commands                                                 *
*************************************************************************/


#include "structs.h"
#include "opinion.h"
#include "scripts.h"
#include "utils.h"
#include "buffer.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "find.h"
#include "db.h"
#include "spells.h"
#include "house.h"
#include "affects.h"
#include "event.h"
#include "olc.h"
#include "staffcmds.h"
#include "extradesc.h"
#include "track.h"
#include "ident.h"

#include "imc.h"

/* extern functions */
void do_start(CharData *ch);
void show_shops(CharData * ch, char *value);
void hcontrol_list_houses(CharData *ch);
void appear(CharData *ch);
int parse_race(char arg);
void reset_zone(int zone);
void roll_real_abils(CharData *ch);
void forget(CharData * ch, CharData * victim);
void remember(CharData * ch, CharData * victim);
void CheckRegenRates(CharData *ch);

/*   external vars  */
extern struct title_type titles[NUM_RACES][16];
extern struct zone_data *zone_table;
extern time_t boot_time;
extern struct attack_hit_type attack_hit_text[];
extern int top_of_zone_table;
extern SInt32 circle_restrict;
extern int circle_shutdown, circle_reboot;
extern int buf_switches, buf_largecount, buf_overflows;
extern IdentServer	* Ident;
extern bool no_external_procs;

/* for objects */
extern char *item_types[];
extern char *wear_bits[];
extern char *extra_bits[];
extern char *container_bits[];
extern char *drinks[];

/* for rooms */
extern char *dirs[];
extern char *room_bits[];
extern char *exit_bits[];
extern char *sector_types[];

/* for chars */
extern char *genders[];
extern char *skills[];
extern char *affected_bits[];
extern char *apply_types[];
extern char *action_bits[];
extern char *player_bits[];
extern char *preference_bits[];
extern char *position_types[];
extern char *connected_types[];
extern char *staff_bits[];
extern char *race_abbrevs[];
extern char *affects[];

/* prototypes */
RNum	find_target_room(CharData * ch, char *rawroomstr);
void do_stat_room(CharData * ch);
void do_stat_object(CharData * ch, ObjData * j);
void do_stat_character(CharData * ch, CharData * k);
void stop_snooping(CharData * ch);
void perform_immort_vis(CharData *ch);
void perform_immort_invis(CharData *ch, int level);
void print_zone_to_buf(char *bufptr, int zone);
void show_deleted(CharData * ch);
void send_to_imms(char *msg);
bool perform_set(CharData *ch, CharData *vict, SInt32 mode, char *val_arg);
ACMD(do_echo);
ACMD(do_send);
ACMD(do_at);
ACMD(do_goto);
ACMD(do_trans);
ACMD(do_teleport);
ACMD(do_vnum);
ACMD(do_stat);
ACMD(do_shutdown);
ACMD(do_snoop);
ACMD(do_switch);
ACMD(do_return);
ACMD(do_load);
ACMD(do_vstat);
ACMD(do_purge);
ACMD(do_advance);
ACMD(do_restore);
ACMD(do_invis);
ACMD(do_gecho);
ACMD(do_poofset);
ACMD(do_dc);
ACMD(do_wizlock);
ACMD(do_date);
ACMD(do_last);
ACMD(do_force);
ACMD(do_wiznet);
ACMD(do_zreset);
ACMD(do_wizutil);
ACMD(do_show);
ACMD(do_set);
ACMD(do_syslog);
ACMD(do_depiss);
ACMD(do_repiss);
ACMD(do_hunt);
ACMD(do_vwear);
ACMD(do_copyover);
ACMD(do_tedit);
ACMD(do_reward);
ACMD(do_string);
ACMD(do_allow);


extern struct sort_struct {
	SInt32	sort_pos;
	Flags	type;
} *cmd_sort_info;

extern int num_of_cmds;

ACMD(do_echo) {
	skip_spaces(argument);

	if (!*argument)
		send_to_char("Yes.. but what?\r\n", ch);
	else {
		char *buf = get_buffer(MAX_STRING_LENGTH);
		if (subcmd == SCMD_EMOTE)
			sprintf(buf, "$n %s", argument);
		else
			strcpy(buf, argument);
		if (PRF_FLAGGED(ch, PRF_NOREPEAT))
			send_to_char(OK, ch);
		else {
			MOBTrigger = FALSE;
			act(buf, FALSE, ch, 0, 0, TO_CHAR);
		}
		MOBTrigger = FALSE;
		act(buf, FALSE, ch, 0, 0, TO_ROOM);
		release_buffer(buf);
	}
}


ACMD(do_send) {
	CharData *vict;
	char	*buf = get_buffer(MAX_INPUT_LENGTH),
			*arg = get_buffer(MAX_INPUT_LENGTH);
			
	half_chop(argument, arg, buf);

	if (!*arg) {
		send_to_char("Send what to who?\r\n", ch);
	} else if (!(vict = get_char_vis(ch, arg))) {
		send_to_char(NOPERSON, ch);
	} else {
		send_to_char(buf, vict);
		send_to_char("\r\n", vict);
		if (PRF_FLAGGED(ch, PRF_NOREPEAT))
			send_to_char("Sent.\r\n", ch);
		else
			act("You send '$t' to $N.\r\n", TRUE, ch, (ObjData *)buf, vict, TO_CHAR);
	}
	release_buffer(buf);
	release_buffer(arg);
}



/* take a string, and return an rnum.. used for goto, at, etc.  -je 4/6/93 */
RNum find_target_room(CharData * ch, char *rawroomstr) {
	RNum location;
	CharData *target_mob;
	ObjData *target_obj;
	char *roomstr = get_buffer(MAX_INPUT_LENGTH);

	one_argument(rawroomstr, roomstr);

	if (!*roomstr) {
		send_to_char("You must supply a room number or name.\r\n", ch);
		release_buffer(roomstr);
		return NOWHERE;
	}
	if (isdigit(*roomstr) && !strchr(roomstr, '.')) {
		if ((location = real_room(atoi(roomstr))) < 0) {
			send_to_char("No room exists with that number.\r\n", ch);
			release_buffer(roomstr);
			return NOWHERE;
		}
	} else if ((target_mob = get_char_vis(ch, roomstr)))
		location = IN_ROOM(target_mob);
	else if ((target_obj = get_obj_vis(ch, roomstr))) {
		if (IN_ROOM(target_obj) != NOWHERE)
			location = IN_ROOM(target_obj);
		else {
			send_to_char("That object is not available.\r\n", ch);
			release_buffer(roomstr);
			return NOWHERE;
		}
	} else {
		send_to_char("No such creature or object around.\r\n", ch);
		release_buffer(roomstr);
		return NOWHERE;
	}
	release_buffer(roomstr);

	if (ROOM_FLAGGED(location, ROOM_GODROOM) && !IS_STAFF(ch)) {
		send_to_char("Only staff are allowed in that room!\r\n", ch);
		return NOWHERE;
	}

	/* a location has been found -- if you're < GRGOD, check restrictions. */
	if (GET_LEVEL(ch) < LVL_SRSTAFF) {
		if (ROOM_FLAGGED(location, ROOM_PRIVATE) && (world[location].people.Count() > 1)) {
			send_to_char("There's a private conversation going on in that room.\r\n", ch);
			return NOWHERE;
		}
		if (ROOM_FLAGGED(location, ROOM_HOUSE) && !House_can_enter(ch, world[location].number)) {
			send_to_char("That's private property -- no trespassing!\r\n", ch);
			return NOWHERE;
		}
		if (ROOM_FLAGGED(location, ROOM_GRGODROOM)) {
			send_to_char("Only Senior Staff are allowed in that room!\r\n", ch);
			return NOWHERE;
		}
	}
	
	if (ROOM_FLAGGED(location, ROOM_ULTRAPRIVATE) && (GET_LEVEL(ch) < LVL_CODER)) {
		send_to_char(	"That room is off limits to absolutely everyone.\r\n"
						"Except FearItself, of course.\r\n", ch);
		return NOWHERE;
	}
	return location;
}



ACMD(do_at) {
	char	*the_command = get_buffer(MAX_INPUT_LENGTH),
			*buf = get_buffer(MAX_INPUT_LENGTH);
	int location, original_loc;

	half_chop(argument, buf, the_command);
	if (!*buf) {
		send_to_char("You must supply a room number or a name.\r\n", ch);
	} else if (!*the_command) {
		send_to_char("What do you want to do there?\r\n", ch);
	} else if ((location = find_target_room(ch, buf)) >= 0) {

		/* a location has been found. */
		original_loc = IN_ROOM(ch);
		ch->from_room();
		ch->to_room(location);
		command_interpreter(ch, the_command);

		/* check if the char is still there */
		if (IN_ROOM(ch) == location) {
			ch->from_room();
			ch->to_room(original_loc);
		}
	}
	release_buffer(the_command);
	release_buffer(buf);
}


ACMD(do_goto) {
	RNum location;
	char * buf;

	if ((location = find_target_room(ch, argument)) < 0)
		return;

	buf = get_buffer(MAX_STRING_LENGTH);

	if (POOFOUT(ch))
		sprintf(buf, "$n %s", POOFOUT(ch));
	else
		strcpy(buf, "$n disappears in a puff of smoke.");

	act(buf, TRUE, ch, 0, 0, TO_ROOM);
	ch->from_room();
	ch->to_room(location);

	if (POOFIN(ch))
		sprintf(buf, "$n %s", POOFIN(ch));
	else
		strcpy(buf, "$n appears with an ear-splitting bang.");

	act(buf, TRUE, ch, 0, 0, TO_ROOM);
	look_at_room(ch, 0);
	release_buffer(buf);
}



ACMD(do_trans) {
	DescriptorData *i;
	CharData *victim;
	char *buf = get_buffer(MAX_INPUT_LENGTH);
	
	one_argument(argument, buf);
	if (!*buf)
		send_to_char("Whom do you wish to transfer?\r\n", ch);
	else if (str_cmp("all", buf)) {
		if (!(victim = get_char_vis(ch, buf)))
			send_to_char(NOPERSON, ch);
		else if (victim == ch)
			send_to_char("That doesn't make much sense, does it?\r\n", ch);
		else if ((IS_STAFF(victim) && !STF_FLAGGED(ch, STAFF_ADMIN)) || STF_FLAGGED(victim, STAFF_ADMIN))
			send_to_char("Go transfer someone your own size.\r\n", ch);
		else {
			act("$n disappears in a mushroom cloud.", FALSE, victim, 0, 0, TO_ROOM);
			victim->from_room();
			victim->to_room(IN_ROOM(ch));
			act("$n arrives from a puff of smoke.", FALSE, victim, 0, 0, TO_ROOM);
			act("$n has transferred you!", FALSE, ch, 0, victim, TO_VICT);
			look_at_room(victim, 0);
		}
	} else {			/* Trans All */
		if (!STF_FLAGGED(ch, STAFF_GAME | STAFF_ADMIN | STAFF_SECURITY)) {
			send_to_char("I think not.\r\n", ch);
			release_buffer(buf);
			return;
		}

		START_ITER(iter, i, DescriptorData *, descriptor_list) {
			if ((STATE(i) == CON_PLAYING) && i->character && (i->character != ch)) {
				victim = i->character;
				if ((IS_STAFF(victim) && !STF_FLAGGED(ch, STAFF_ADMIN)) || STF_FLAGGED(victim, STAFF_ADMIN))
					continue;
				act("$n disappears in a mushroom cloud.", FALSE, victim, 0, 0, TO_ROOM);
				victim->from_room();
				victim->to_room(IN_ROOM(ch));
				act("$n arrives from a puff of smoke.", FALSE, victim, 0, 0, TO_ROOM);
				act("$n has transferred you!", FALSE, ch, 0, victim, TO_VICT);
				look_at_room(victim, 0);
			}
		} END_ITER(iter);
		send_to_char(OK, ch);
	}
	release_buffer(buf);
}



ACMD(do_teleport) {
	CharData *victim;
	SInt16	target;
	char *buf = get_buffer(MAX_INPUT_LENGTH),
		*buf2 = get_buffer(MAX_INPUT_LENGTH);
		
	two_arguments(argument, buf, buf2);

	if (!*buf)
		send_to_char("Whom do you wish to teleport?\r\n", ch);
	else if (!(victim = get_char_vis(ch, buf)))
		send_to_char(NOPERSON, ch);
	else if (victim == ch)
		send_to_char("Use 'goto' to teleport yourself.\r\n", ch);
//	else if (GET_LEVEL(victim) >= GET_LEVEL(ch))
//		send_to_char("Maybe you shouldn't do that.\r\n", ch);
	else if ((IS_STAFF(victim) && !STF_FLAGGED(ch, STAFF_ADMIN)) || STF_FLAGGED(victim, STAFF_ADMIN))
		send_to_char("Maybe you shouldn't do that.\r\n", ch);
	else if (!*buf2)
		send_to_char("Where do you wish to send this person?\r\n", ch);
	else if ((target = find_target_room(ch, buf2)) >= 0) {
		send_to_char(OK, ch);
		act("$n disappears in a puff of smoke.", FALSE, victim, 0, 0, TO_ROOM);
		victim->from_room();
		victim->to_room(target);
		act("$n arrives from a puff of smoke.", FALSE, victim, 0, 0, TO_ROOM);
		act("$n has teleported you!", FALSE, ch, 0, victim, TO_VICT);
		look_at_room(victim, 0);
	}
	release_buffer(buf);
	release_buffer(buf2);
}



ACMD(do_vnum) {
	char *buf = get_buffer(MAX_INPUT_LENGTH),
		*buf2 = get_buffer(MAX_INPUT_LENGTH);
		
	two_arguments(argument, buf, buf2);

	if (!*buf || !*buf2)
		send_to_char("Usage: vnum { obj | mob | trg } <name>\r\n", ch);
	else if (is_abbrev(buf, "mob")) {
		if (!vnum_mobile(buf2, ch))
			send_to_char("No mobiles by that name.\r\n", ch);
	} else if (is_abbrev(buf, "obj")) {
		if (!vnum_object(buf2, ch))
			send_to_char("No objects by that name.\r\n", ch);
	} else if (is_abbrev(buf, "trg")) {
		if (!vnum_trigger(buf2, ch))
			send_to_char("No triggers by that name.\r\n", ch);
	} else
		send_to_char("Usage: vnum { obj | mob | trg } <name>\r\n", ch);

	release_buffer(buf);
	release_buffer(buf2);
}



void do_stat_room(CharData * ch) {
	ExtraDesc *desc;
	RoomData *rm = &world[IN_ROOM(ch)];
	int i, found = 0;
	ObjData *j = 0;
	CharData *k = 0;
	char	*buf = get_buffer(MAX_STRING_LENGTH);

	sprinttype(rm->sector_type, sector_types, buf);
	ch->Send(	"Room name     : `c%s`n\r\n"
				"     Zone     : [%3d]      VNum     : [`g%5d`n]\r\n"
				"     Type     : %-10s "   "RNum     : [%5d]\r\n",
			rm->GetName("<ERROR>"),
			zone_table[rm->zone].number, rm->number,
			buf, IN_ROOM(ch));

	sprintbit(rm->flags, room_bits, buf);
	ch->Send(	"     SpecProc : %-10s "   "Script   : %s\r\n"
				"Room Flags    : %s\r\n"
				"[Description]\r\n"
				"%s",
			(rm->func == NULL) ? "None" : "Exists", SCRIPT(rm) ? "Exists" : "None",
			buf,
			rm->GetDesc("  None.\r\n"));

	if (rm->ex_description) {
		send_to_char("Extra descs   :`c", ch);
		for (desc = rm->ex_description; desc; desc = desc->next)
			ch->Send(" %s", SSData(desc->keyword));
		send_to_char("`n\r\n", ch);
	}

	if (rm->people.Count()) {
		*buf = found = 0;
		send_to_char(	"[Chars present]\r\n     `y", ch);
		START_ITER(iter, k, CharData *, rm->people) {
			if (!ch->CanSee(k))	continue;
			sprintf(buf + strlen(buf),	"%s %s(%s)",
					found++ ? "," : "", k->GetName(),
					(!IS_NPC(k) ? "PC" : (!IS_MOB(k) ? "NPC" : "MOB")));
			if (strlen(buf) >= 62) {
				if (iter.Peek())		ch->Send("%s,\r\n", buf);
				else					ch->Send("%s\r\n", buf);
				*buf = found = 0;
			}
		} END_ITER(iter);
		ch->Send("%s%s`n", buf, found ? "\r\n" : "");
	}
	
	if (rm->contents.Count()) {
		*buf = found = 0;
		send_to_char("[Contents]\r\n     `g", ch);
		START_ITER(iter, j, ObjData *, rm->contents) {
			if (!ch->CanSee(j))	continue;
			sprintf(buf + strlen(buf), "%s %s", found++ ? "," : "", SSData(j->shortDesc));
			if (strlen(buf) >= 62) {
				if (iter.Peek())		ch->Send("%s,\r\n", buf);
				else					ch->Send("%s\r\n", buf);
				*buf = found = 0;
			}
		} END_ITER(iter);
		ch->Send("%s%s`n", buf, found ? "\r\n" : "");
	}
	
	for (i = 0; i < NUM_OF_DIRS; i++) {
		if (rm->dir_option[i]) {
			if (rm->dir_option[i]->to_room == NOWHERE)
				strcpy(buf, " `cNONE`n");
			else
				sprintf(buf, "`c%5d`n", world[rm->dir_option[i]->to_room].number);
			ch->Send("Exit `c%-5s`n:  To: [%s], Key: [%5d], Keywrd: %s, ",
					dirs[i], buf, rm->dir_option[i]->key, rm->dir_option[i]->GetKeyword("None"));
			
			sprintbit(rm->dir_option[i]->exit_info, exit_bits, buf);
			ch->Send("Type: %s\r\n"
					 "%s",
					buf,
					rm->dir_option[i]->GetDesc("  No exit description.\r\n"));
		}
	}
	release_buffer(buf);
}



void do_stat_object(CharData * ch, ObjData * j) {
	int i, vnum, found;
	ObjData *j2;
	ExtraDesc *desc;
	extern char * ammo_types[];
	extern struct attack_hit_type attack_hit_text[];
	char	*buf = get_buffer(MAX_STRING_LENGTH);
			
	vnum = GET_OBJ_VNUM(j);
	ch->Send(	"Name   : \"`y%s`n\"\r\n"
				"Aliases: %s\r\n",
			(SSData(j->shortDesc) ? SSData(j->shortDesc) : "<None>"),
			SSData(j->name));
			
	sprinttype(GET_OBJ_TYPE(j), item_types, buf);
	ch->Send(	"     VNum     : [`g%5d`n]    RNum     : [%5d]\r\n"
				"     Type     : %s\r\n"
				"     SpecProc : %-10s\r\n"
				"Scripts\r\n"
				"         Local: %-10s "     "Global   : %s\r\n"
				"L-Des         : %s\r\n",
			vnum, GET_OBJ_RNUM(j),
			buf,
			((GET_OBJ_RNUM(j) >= 0) ? (obj_index[GET_OBJ_RNUM(j)].func ? "Exists" : "None") : "None"),
			SCRIPT(j) ? "Exists" : "None", GET_OBJ_TRGS(j) ? "Exists" : "None",
			(SSData(j->description) ? SSData(j->description) : "None"));

	if (j->exDesc) {
		send_to_char("Extra descs   :`c", ch);
		for (desc = j->exDesc; desc; desc = desc->next) {
			ch->Send(" %s", SSData(desc->keyword));
		}
		send_to_char("`n\r\n", ch);
	}
	
	sprintbit(GET_OBJ_WEAR(j), wear_bits, buf);		ch->Send(	"Can be worn on: %s\r\n", buf);
	sprintbit(j->affects, affected_bits, buf);		ch->Send(	"Set char bits : %s\r\n", buf);
	sprintbit(GET_OBJ_EXTRA(j), extra_bits, buf);	ch->Send(	"Extra flags   : %s\r\n", buf);

	ch->Send(	"     Weight   : %-5d      Value    : %d\r\n"
				"     Timer    : %-5d      Contents : %d\r\n",
			GET_OBJ_WEIGHT(j), GET_OBJ_COST(j),
			GET_OBJ_TIMER(j), j->TotalValue() - GET_OBJ_COST(j));

	if(IN_ROOM(j) > 0)		ch->Send("In room       : %s`n [`c%d`n]\r\n", world[IN_ROOM(j)].GetName("<ERROR>"), world[IN_ROOM(j)].number);
	else if(j->in_obj)		ch->Send("In object     : %s`n\r\n", SSData((j->in_obj)->shortDesc));
	else if(j->carried_by)	ch->Send("Carried by    : %s`n\r\n", j->carried_by->GetName());
	else if(j->worn_by)		ch->Send("Worn by       : %s`n\r\n", j->worn_by->GetName());

	send_to_char("[Values]\r\n", ch);

	switch (GET_OBJ_TYPE(j)) {
		case ITEM_LIGHT:
			if (GET_OBJ_VAL(j, 2) == -1)	ch->Send("     Hours left: Infinite");
			else							ch->Send("     Hours left: %d", GET_OBJ_VAL(j, 2));
			break;
		case ITEM_THROW:
		case ITEM_ARROW:
			ch->Send("     Damage   : %3dd%-3d", GET_OBJ_VAL(j, 2), GET_OBJ_VAL(j, 3));
			break;
		case ITEM_BOOMERANG:
			ch->Send("     Damage   : %3dd%-3d\r\n"
					 "     Range    : %d",
					GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 2), GET_OBJ_VAL(j, 5));
			break;
		case ITEM_GRENADE:
			ch->Send("     Damage   : %3dd%-3d    Timer    : %d",
					GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 2), GET_OBJ_VAL(j, 0));
			break;
		case ITEM_BOW:
			ch->Send("     Range    : %d", GET_OBJ_VAL(j, 1));
			break;
		case ITEM_WEAPON:
			ch->Send("     Damage   : %3dd%-3d    Message  : %s\r\n"
					 "     Speed    : %d",
					GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 2), attack_hit_text[GET_OBJ_VAL(j, 3)].singular,
					GET_OBJ_SPEED(j));
			if (IS_GUN(j)) {
				ch->Send("\r\nGun settings:\r\n"
						 "     Damage   : %3dd%-3d    Rate/Fire: %d\r\n"
						 "     Range    : %-3d        Ammo Type: %s\r\n"
						 "     Attack   : %s",
						GET_GUN_DICE_NUM(j), GET_GUN_DICE_SIZE(j), GET_GUN_RATE(j),
						GET_GUN_RANGE(j), ammo_types[(int)GET_GUN_AMMO_TYPE(j)],
						attack_hit_text[GET_GUN_ATTACK_TYPE(j) + (TYPE_SHOOT-TYPE_HIT)].singular);
				if (GET_GUN_AMMO(j) > 0) {
					ch->Send("\r\n"
							"     Ammo     : %-3d        VNUM     : %d",
							GET_GUN_AMMO(j), GET_GUN_AMMO_VNUM(j));
				}
			}

			break;
		case ITEM_MISSILE:
			ch->Send("     Ammo     : %4d       Ammo Type: %s", GET_OBJ_VAL(j,1), ammo_types[GET_OBJ_VAL(j,0)]);
			break;
		case ITEM_ARMOR:
			ch->Send("     Absorb   : %d", GET_OBJ_VAL(j, 0));
			break;
		case ITEM_CONTAINER:
			sprintbit(GET_OBJ_VAL(j, 1), container_bits, buf);
			ch->Send("     MaxWeight: %-4d       Lock Type: %s\r\n"
					 "     Key Num  : %-5d      Corpse   : %s",
					GET_OBJ_VAL(j, 0), buf,
					GET_OBJ_VAL(j, 2), YESNO(GET_OBJ_VAL(j, 3)));
			break;
		case ITEM_DRINKCON:
		case ITEM_FOUNTAIN:
			sprinttype(GET_OBJ_VAL(j, 2), drinks, buf);
			ch->Send("     Capacity : %5d      Contains : %d\r\n"
					 "     Poisoned : %-3s        Liquid   : %s",
					GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 1),
					YESNO(GET_OBJ_VAL(j, 3)), buf);
			break;
		case ITEM_NOTE:
		case ITEM_PEN:
		case ITEM_KEY:
			send_to_char("[NONE]", ch);
			break;
		case ITEM_FOOD:
			ch->Send("     Fill     : %-5d      Poisoned : %s",
					GET_OBJ_VAL(j, 0), YESNO(GET_OBJ_VAL(j, 3)));
			break;
		case ITEM_BOARD:
			ch->Send("     Levels\r\n"
					 "     Read     : %-5d      Write    : %-5d\r\n"
					 "     Remove   : %-5d",
					GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 1),
					GET_OBJ_VAL(j, 2));
			break;
		case ITEM_VEHICLE:
			ch->Send("     EntryRoom: %-5d      Rooms    : %-5d-%-5d\r\n"
					 "     Flags    : %-5d\r\n"
			         "     Size     : %-5d",
			         GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 2), GET_OBJ_VAL(j, 3),
			         GET_OBJ_VAL(j, 1),
			         GET_OBJ_VAL(j, 4));
			break;
		default:
			ch->Send("     Value 0  : %-10d Value 1  : %d\r\n"
					 "     Value 2  : %-10d Value 3  : %d\r\n"
					 "     Value 4  : %-10d Value 5  : %d\r\n"
					 "     Value 6  : %-10d Value 7  : %d",
					GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 1),
					GET_OBJ_VAL(j, 2), GET_OBJ_VAL(j, 3),
					GET_OBJ_VAL(j, 4), GET_OBJ_VAL(j, 5),
					GET_OBJ_VAL(j, 6), GET_OBJ_VAL(j, 7));
			break;
	}
	send_to_char("\r\n", ch);

	if (j->contains.Count()) {
		strcpy(buf, "\r\nContents:`g");
		*buf = found = 0;
		START_ITER(iter, j2, ObjData *, j->contains) {
			sprintf(buf + strlen(buf), "%s %s", found++ ? "," : "", SSData(j2->shortDesc));
			if (strlen(buf) >= 62) {
				ch->Send("%s%s\r\n", buf, iter.Peek() ? "," : "");
				*buf = found = 0;
			}
		} END_ITER(iter);
		ch->Send("%s%s`n", buf, found ? "\r\n" : "");
	}

/*
	if (j->contains) {
		send_to_char("\r\nContents:`g", ch);
		len = 10;
		for (found = 0, j2 = j->contains; j2; j2 = j2->next_content) {
			ch->Send("%s %s", found++ ? "," : "", SSData(j2->shortDesc));
			len += strlen(SSData(j2->shortDesc)) + 2;
			if (len >= 62) {
				if (j2->next_content)	send_to_char(",\r\n", ch);
				else					send_to_char("\r\n", ch);
				found = 0;
				len = 0;
			}
		}
		if (len)	send_to_char("\r\n", ch);
		send_to_char("`n", ch);
	}
*/	

	found = 0;
	send_to_char("Affections:", ch);
	for (i = 0; i < MAX_OBJ_AFFECT; i++)
		if (j->affect[i].modifier) {
			sprinttype(j->affect[i].location, apply_types, buf);
			ch->Send("%s %+d to %s", found++ ? "," : "", j->affect[i].modifier, buf);
		}
	ch->Send("%s\r\n", found ? "" : " None");
	
	release_buffer(buf);
}


void do_stat_character(CharData * ch, CharData * k) {
	int i, i2, found = 0;
	Affect *aff;
	CharData *follower;
	char	*buf = get_buffer(MAX_STRING_LENGTH),
			*buf2;


	switch (GET_SEX(k)) {
		case SEX_NEUTRAL:	strcpy(buf, "NEUTRAL-SEX");		break;
		case SEX_MALE:		strcpy(buf, "MALE");			break;
		case SEX_FEMALE:	strcpy(buf, "FEMALE");			break;
		default:			strcpy(buf, "ILLEGAL-SEX!!");	break;
	}

	ch->Send("%s %s '%s'  IDNum: [%5d], In room [%5d]\r\n",
			buf, (!IS_NPC(k) ? "PC" : (!IS_MOB(k) ? "NPC" : "MOB")),
			k->GetName(), GET_IDNUM(k), IN_ROOM(k) != NOWHERE ? world[IN_ROOM(k)].number : -1);

	if (IS_MOB(k))
		ch->Send("Alias: %s, VNum: [%5d], RNum: [%5d]\r\n",
				SSData(k->general.name), GET_MOB_VNUM(k), GET_MOB_RNUM(k));

	ch->Send(	"Title: %s\r\n"
				"L-Des: %s"
				"Race: [`y%s`n], Lev: [`y%2d`n]\r\n",
				GET_TITLE(k) ? GET_TITLE(k) : "<None>",
				SSData(k->general.longDesc) ? SSData(k->general.longDesc) : "<None>\r\n",
				race_abbrevs[(int)GET_RACE(k)], GET_LEVEL(k));

	if (!IS_NPC(k)) {
		buf2 = get_buffer(MAX_STRING_LENGTH);
		strcpy(buf, asctime(localtime(&(k->player->time.birth))));
		strcpy(buf2, asctime(localtime(&(k->player->time.logon))));
		buf[10] = buf2[10] = '\0';

		ch->Send(	"Created: [%s], Last Logon: [%s], Played [%dh %dm], Age [%d]\r\n"
					"Pracs: %d\r\n",
				buf, buf2, k->player->time.played / 3600, ((k->player->time.played % 3600) / 60), age(k).year,
				GET_PRACTICES(k));
		release_buffer(buf2);
	}
	sprinttype(GET_POS(k), position_types, buf);
	ch->Send(	"Str: [`C%d`n]  Int: [`C%d`n]  Per: [`C%d`n]  "
				"Agl: [`C%d`n]  Con: [`C%d`n]\r\n"
				"Hit p.:[`G%d/%d+%d`n]  Move p.:[`G%d/%d+%d`n]\r\n"
				"Mission Points: %d\r\n"
				"AC: [%4d], Hitroll: [%2d], Damroll: [%2d]\r\n"
				"Pos: %s, Fighting: %s",
			GET_STR(k), GET_INT(k), GET_PER(k),
			GET_AGI(k), GET_CON(k),
			GET_HIT(k), GET_MAX_HIT(k), hit_gain(k),
			GET_MOVE(k), GET_MAX_MOVE(k), move_gain(k),
			GET_MISSION_POINTS(k),
			GET_AC(k), k->points.hitroll, k->points.damroll,
			buf, FIGHTING(k) ? FIGHTING(k)->GetName() : "Nobody");

	if (IS_NPC(k))
		ch->Send(", Attack type: %s", attack_hit_text[k->mob->attack_type].singular);

	if (k->desc) {
		sprinttype(STATE(k->desc), connected_types, buf);
		ch->Send(", Connected: %s", buf);
	}

	sprinttype((k->mob->default_pos), position_types, buf);
	ch->Send("\r\nDefault position: %s, Idle Timer (in tics) [%d]\r\n", buf, k->player->timer);

	if (IS_NPC(k)) {
		sprintbit(MOB_FLAGS(k), action_bits, buf);			ch->Send("NPC flags: `c%s`n\r\n", buf);
	} else {
		sprintbit(PLR_FLAGS(k), player_bits, buf);			ch->Send("PLR: `c%s`n\r\n", buf);
		sprintbit(PRF_FLAGS(k), preference_bits, buf);		ch->Send("PRF: `g%s`n\r\n", buf);
		sprintbit(STF_FLAGS(k), staff_bits, buf);			ch->Send("STAFF: `g%s`n\r\n", buf);
	}

	if (IS_MOB(k)) {
		ch->Send(	"Mob Spec-Proc : %s\r\n"
					"Scripts\r\n"
					"         Local: %-10s   ""Global   : %s\r\n"
					"NPC Bare Hand Dam: %dd%d\r\n",
				(mob_index[GET_MOB_RNUM(k)].func ? "Exists" : "None"),
				SCRIPT(k) ? "Exists" : "None", GET_MOB_TRGS(k) ? "Exists" : "None",
				k->mob->damage.number, k->mob->damage.size);
	}
	
	i = k->carrying.Count();
	ch->Send("Carried: weight: %d, items: %d; Items in: inventory: %d, ",
			IS_CARRYING_W(k), IS_CARRYING_N(k), i);

	for (i = 0, i2 = 0; i < NUM_WEARS; i++)
		if (GET_EQ(k, i))
			i2++;
	
	ch->Send(	"eq: %d\r\n"
				"Hunger: %d, Thirst: %d, Drunk: %d\r\n"
				"Master is: %s, Followers are:",
			i2,
			GET_COND(k, FULL), GET_COND(k, THIRST), GET_COND(k, DRUNK),
			((k->master) ? k->master->GetName() : "<none>"));

	if (k->followers.Count()) {
		*buf = found = 0;
		START_ITER(follow_iter, follower, CharData *, k->followers) {
			sprintf(buf + strlen(buf), "%s %s", found++ ? "," : "", PERS(follower, ch));
			if (strlen(buf) >= 62) {
				ch->Send("%s%s\r\n", buf, follow_iter.Peek() ? "," : "");
				*buf = found = 0;
			}
		} END_ITER(follow_iter);

		ch->Send("%s%s`n", buf, found ? "\r\n" : "");
	}
	
	/* Showing the bitvector */
	sprintbit(AFF_FLAGS(k), affected_bits, buf);
	ch->Send("AFF: `y%s`n\r\n", buf);

	if (IS_NPC(k) && k->mob->hates) {
		struct int_list *names;
		CharData *vict;
		send_to_char("Pissed List:\r\n", ch);
		for (names = k->mob->hates->charlist; names; names = names->next) {
			vict = find_char(names->i);
			if (vict)	ch->Send("  %s\r\n", vict->GetName());
		}
		send_to_char("\r\n", ch);
	}

	/* Routine to show what spells a char is affected by */
	if (k->affected.Count()) {
		START_ITER(affect_iter, aff, Affect *, k->affected) {
			ch->Send("SKL: (%3dhr) `c%-21s`n ",
					aff->event ? aff->event->Time() : 0, 
					((aff->type >= Affect::None) && ((SInt32)aff->type <= MAX_AFFECT)) ?
//					(Affect_None <= aff->type <= MAX_AFFECT) ?

							affects[aff->type] : affects[Affect::None]);
			if (aff->modifier)
				ch->Send("%+d to %s", aff->modifier, apply_types[aff->location]);
			if (aff->bitvector) {
				sprintbit(aff->bitvector, affected_bits, buf);
				ch->Send("%ssets %s", aff->modifier ? ", " : "", buf);
			}
			send_to_char("\r\n", ch);
		} END_ITER(affect_iter);
	}
	release_buffer(buf);
}


ACMD(do_stat) {
	CharData *victim = 0;
	ObjData *object = 0;
	char	*buf1 = get_buffer(MAX_INPUT_LENGTH),
			*buf2 = get_buffer(MAX_INPUT_LENGTH);

	half_chop(argument, buf1, buf2);

	if (!*buf1)
		send_to_char("Stats on who or what?\r\n", ch);
	else if (is_abbrev(buf1, "room"))
		if (subcmd == SCMD_STAT)	do_stat_room(ch);
		else						do_sstat_room(ch);
	else if (is_abbrev(buf1, "mob")) {
		if (!*buf2)
			send_to_char("Stats on which mobile?\r\n", ch);
		else if (!(victim = get_char_vis(ch, buf2)))
			send_to_char("No such mobile around.\r\n", ch);
	} else if (is_abbrev(buf1, "player")) {
		if (!*buf2)
			send_to_char("Stats on which player?\r\n", ch);
		else if (!(victim = get_player_vis(ch, buf2, false)))
			send_to_char("No such player around.\r\n", ch);
	} else if (is_abbrev(buf1, "file")) {
		if (!*buf2)
			send_to_char("Stats on which player?\r\n", ch);
		else {
			victim = new CharData();
			if (victim->load(buf2) > -1) {
				if (IS_STAFF(victim) && !STF_FLAGGED(ch, STAFF_SECURITY | STAFF_ADMIN))
					send_to_char("Sorry, you can't do that.\r\n", ch);
				else
					do_stat_character(ch, victim);
			} else
				send_to_char("There is no such player.\r\n", ch);
			delete victim;
			victim = NULL;
		}
	} else if (is_abbrev(buf1, "object")) {
		if (!*buf2)
			send_to_char("Stats on which object?\r\n", ch);
		else if (!(object = get_obj_vis(ch, buf2)))
			send_to_char("No such object around.\r\n", ch);
	} else {
		if ((victim = get_char_vis(ch, buf1)));
		else if ((object = get_obj_vis(ch, buf1)));
		else send_to_char("Nothing around by that name.\r\n", ch);
	}
	if (object) {
		if (subcmd == SCMD_STAT)	do_stat_object(ch, object);
		else						do_sstat_object(ch, object);
	} else if (victim) {
		if (subcmd == SCMD_STAT)	do_stat_character(ch, victim);
		else						do_sstat_character(ch, victim);
	}
	release_buffer(buf1);
	release_buffer(buf2);
}


ACMD(do_shutdown) {
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	one_argument(argument, arg);

	if (!*arg) {
		log("(GC) Shutdown by %s.", ch->RealName());
		send_to_all("Shutting down.\r\n");
		circle_shutdown = 1;
	} else if (!str_cmp(arg, "reboot")) {
		log("(GC) Reboot by %s.", ch->RealName());
		send_to_all("Rebooting.. come back in a minute or two.\r\n");
		touch(FASTBOOT_FILE);
		circle_shutdown = circle_reboot = 1;
	} else if (!str_cmp(arg, "die")) {
		log("(GC) Shutdown by %s.", ch->RealName());
		send_to_all("Shutting down for maintenance.\r\n");
		touch(KILLSCRIPT_FILE);
		circle_shutdown = 1;
	} else if (!str_cmp(arg, "pause")) {
		log("(GC) Shutdown by %s.", ch->RealName());
		send_to_all("Shutting down for maintenance.\r\n");
		touch(PAUSE_FILE);
		circle_shutdown = 1;
	} else
		send_to_char("Unknown shutdown option.\r\n", ch);
	release_buffer(arg);
}


void stop_snooping(CharData * ch) {
	if (!ch->desc->snooping)
		send_to_char("You aren't snooping anyone.\r\n", ch);
	else {
		send_to_char("You stop snooping.\r\n", ch);
		ch->desc->snooping->snoop_by = NULL;
		ch->desc->snooping = NULL;
	}
}


ACMD(do_snoop) {
	CharData *victim, *tch;
	char *arg;
	
	if (!ch->desc)
		return;
		
	arg = get_buffer(MAX_INPUT_LENGTH);
	one_argument(argument, arg);

	if (!*arg)
		stop_snooping(ch);
	else if (!(victim = get_char_vis(ch, arg)))
		send_to_char("No such person around.\r\n", ch);
	else if (!victim->desc)
		send_to_char("There's no link.. nothing to snoop.\r\n", ch);
	else if (victim == ch)
		stop_snooping(ch);
	else if (victim->desc->snoop_by)
		send_to_char("Busy already. \r\n", ch);
	else if (victim->desc->snooping == ch->desc)
		send_to_char("Don't be stupid.\r\n", ch);
	else {
		tch = (victim->desc->original ? victim->desc->original : victim);

		if (IS_STAFF(victim) && !STF_FLAGGED(ch, STAFF_ADMIN))
			send_to_char("You can't.\r\n", ch);
		else {
			send_to_char(OK, ch);

			if (ch->desc->snooping)
				ch->desc->snooping->snoop_by = NULL;

			ch->desc->snooping = victim->desc;
			victim->desc->snoop_by = ch->desc;
		}
	}
	release_buffer(arg);
}



ACMD(do_switch) {
	CharData *victim;
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	
	one_argument(argument, arg);

	if (ch->desc->original)
		send_to_char("You're already switched.\r\n", ch);
	else if (!*arg)
		send_to_char("Switch with who?\r\n", ch);
	else if (!(victim = get_char_vis(ch, arg)))
		send_to_char("No such character.\r\n", ch);
	else if (ch == victim)
		send_to_char("Hee hee... we are jolly funny today, eh?\r\n", ch);
	else if (victim->desc)
		send_to_char("You can't do that, the body is already in use!\r\n", ch);
	else if (!STF_FLAGGED(ch, STAFF_ADMIN | STAFF_SECURITY | STAFF_CHAR) && !IS_NPC(victim))
		send_to_char("You don't have the clearance to use a mortal's body.\r\n", ch);
	else {
		send_to_char(OK, ch);

		ch->desc->character = victim;
		ch->desc->original = ch;

		victim->desc = ch->desc;
		ch->desc = NULL;
	}
	release_buffer(arg);
}


ACMD(do_return) {
	if (ch->desc && ch->desc->original) {
		send_to_char("You return to your original body.\r\n", ch);

		/* JE 2/22/95 */
		/* if someone switched into your original body, disconnect them */
		if (ch->desc->original->desc)
			STATE(ch->desc->original->desc) = CON_DISCONNECT;

		ch->desc->character = ch->desc->original;
		ch->desc->original = NULL;

		ch->desc->character->desc = ch->desc;
		ch->desc = NULL;
	}
}



ACMD(do_load) {
	CharData *mob;
	ObjData *obj;
	int number, r_num;
	char	*buf = get_buffer(MAX_INPUT_LENGTH),
			*buf2 = get_buffer(MAX_INPUT_LENGTH);
			
	two_arguments(argument, buf, buf2);

	if (!*buf || !*buf2 || !isdigit(*buf2))
		send_to_char("Usage: load { obj | mob } <number>\r\n", ch);
	else if ((number = atoi(buf2)) < 0)
		send_to_char("A NEGATIVE number??\r\n", ch);
	else if (is_abbrev(buf, "mob")) {
		if ((r_num = real_mobile(number)) < 0)
			send_to_char("There is no monster with that number.\r\n", ch);
		else {
			mob = read_mobile(r_num, REAL);
			mob->to_room(IN_ROOM(ch));

			act("$n makes a quaint, magical gesture with one hand.", TRUE, ch, 0, 0, TO_ROOM);
			act("$n has created $N!", FALSE, ch, 0, mob, TO_ROOM);
			act("You create $N.", FALSE, ch, 0, mob, TO_CHAR);
		}
	} else if (is_abbrev(buf, "obj")) {
		if ((r_num = real_object(number)) < 0)
			send_to_char("There is no object with that number.\r\n", ch);
		else {
			obj = read_object(r_num, REAL);
			if (obj->wear)
				obj->to_char(ch);
			else 
				obj->to_room(IN_ROOM(ch));
			act("$n makes a strange magical gesture.", TRUE, ch, 0, 0, TO_ROOM);
			act("$n has created $p!", FALSE, ch, obj, 0, TO_ROOM);
			act("You create $p.", FALSE, ch, obj, 0, TO_CHAR);
		}
	} else
		send_to_char("That'll have to be either 'obj' or 'mob'.\r\n", ch);
	release_buffer(buf);
	release_buffer(buf2);
}



ACMD(do_vstat) {
	CharData *mob;
	ObjData *obj;
	TrigData *trig;
	int number, r_num;
	char	*buf = get_buffer(MAX_INPUT_LENGTH),
			*buf2 = get_buffer(MAX_INPUT_LENGTH);

	two_arguments(argument, buf, buf2);

	if (!*buf || !*buf2 || !isdigit(*buf2))
		send_to_char("Usage: vstat { obj | mob | trg } <number>\r\n", ch);
	else if ((number = atoi(buf2)) < 0)
		send_to_char("A NEGATIVE number??\r\n", ch);
	else if (is_abbrev(buf, "mob")) {
		if ((r_num = real_mobile(number)) < 0)
			send_to_char("There is no monster with that number.\r\n", ch);
		else {
			mob = read_mobile(r_num, REAL);
			mob->to_room(0);
			do_stat_character(ch, mob);
			mob->extract();
		}
	} else if (is_abbrev(buf, "obj")) {
		if ((r_num = real_object(number)) < 0)
			send_to_char("There is no object with that number.\r\n", ch);
		else {
			obj = read_object(r_num, REAL);
			do_stat_object(ch, obj);
			obj->extract();
		}
	} else if (is_abbrev(buf, "trg")) {
		if ((r_num = real_trigger(number)) < 0)
			send_to_char("There is no trigger with that number.\r\n", ch);
		else {
			trig = read_trigger(r_num, REAL);
			do_stat_trigger(ch, trig);
			trig->extract();
		}
	} else
		send_to_char("That'll have to be either 'obj' or 'mob'.\r\n", ch);
	release_buffer(buf);
	release_buffer(buf2);
}




/* clean a room of all mobiles and objects */
ACMD(do_purge) {
	CharData *vict;
	ObjData *obj;
	char *buf = get_buffer(MAX_INPUT_LENGTH);
	
	one_argument(argument, buf);

	if (*buf) {			/* argument supplied. destroy single object
						 * or char */
		if ((vict = get_char_room_vis(ch, buf))) {
			if (!IS_NPC(vict) && (!STF_FLAGGED(ch, STAFF_SECURITY | STAFF_ADMIN) ||
					(IS_STAFF(vict) && !STF_FLAGGED(ch, STAFF_ADMIN)))) {
				send_to_char("Fuuuuuuuuu!\r\n", ch);
				release_buffer(buf);
				return;
			}
			act("$n disintegrates $N.", FALSE, ch, 0, vict, TO_NOTVICT);

			if (!IS_NPC(vict)) {
				mudlogf(BRF, LVL_STAFF, TRUE, "(GC) %s has purged %s.", ch->RealName(), vict->RealName());
				if (vict->desc) {
					STATE(vict->desc) = CON_CLOSE;
					vict->desc->character = NULL;
					vict->desc = NULL;
				}
			}
			vict->extract();
		} else if ((obj = get_obj_in_list_vis(ch, buf, world[IN_ROOM(ch)].contents))) {
			act("$n destroys $p.", FALSE, ch, obj, 0, TO_ROOM);
			obj->extract();
		} else {
			send_to_char("Nothing here by that name.\r\n", ch);
			release_buffer(buf);
			return;
		}

		send_to_char(OK, ch);
	} else {			/* no argument. clean out the room */
		act("$n gestures... You are surrounded by scorching flames!", FALSE, ch, 0, 0, TO_ROOM);
		world[IN_ROOM(ch)].Send("The world seems a little cleaner.\r\n");

		START_ITER(ch_iter, vict, CharData *, world[IN_ROOM(ch)].people) {
			if (IS_NPC(vict))
				vict->extract();
		} END_ITER(ch_iter);

		START_ITER(obj_iter, obj, ObjData *, world[IN_ROOM(ch)].contents) {
			obj->extract();
		} END_ITER(obj_iter);
	}
	release_buffer(buf);
}



ACMD(do_advance) {
	CharData *victim;
	char	*name = get_buffer(MAX_STRING_LENGTH),
			*level = get_buffer(MAX_STRING_LENGTH);
	int newlevel, oldlevel;

	two_arguments(argument, name, level);

	if (!*name)
		send_to_char("Advance who?\r\n", ch);
	else if (!(victim = get_char_vis(ch, name)))
		send_to_char("That player is not here.\r\n", ch);
	else if (IS_STAFF(victim) && !STF_FLAGGED(ch, STAFF_ADMIN))
		send_to_char("Maybe that's not such a great idea.\r\n", ch);
	else if (IS_NPC(victim))
		send_to_char("NO!  Not on NPC's.\r\n", ch);
	else if (!*level || (newlevel = atoi(level)) <= 0)
		send_to_char("That's not a level!\r\n", ch);
	else if (newlevel > LVL_ADMIN)
		ch->Send("%d is the highest possible level.\r\n", LVL_ADMIN);
//	else if (newlevel == GET_LEVEL(ch))
//		send_to_char("Yeah, right.\r\n", ch);
	else if (newlevel == GET_LEVEL(victim))
		send_to_char("They are already at that level.\r\n", ch);
	else {
		oldlevel = GET_LEVEL(victim);
		if (newlevel < GET_LEVEL(victim)) {
			do_start(victim);
			GET_LEVEL(victim) = newlevel;
			send_to_char("You have been demoted!\r\n", victim);
		} else {
			act("$n has promoted you.\r\n"
				"You put the new rank emblems on.\r\n"
				"You feel more powerful.", FALSE, ch, 0, victim, TO_VICT);
		}

		send_to_char(OK, ch);

		mudlogf(BRF, LVL_STAFF, TRUE, "(GC) %s has advanced %s to level %d (from %d)", ch->RealName(), victim->RealName(), newlevel, oldlevel);
		victim->set_level(newlevel);
		victim->save(NOWHERE);
	}
	release_buffer(name);
	release_buffer(level);
}



ACMD(do_restore) {
	CharData *vict;
	DescriptorData *d;
	int i;
	char *buf = get_buffer(MAX_INPUT_LENGTH);
	
	one_argument(argument, buf);
	
	if (!*buf)
		send_to_char("Whom do you wish to restore?\r\n", ch);
	else if (!str_cmp(buf, "all")) {
		mudlogf(NRM, LVL_STAFF, TRUE, "(GC) %s restores all", ch->RealName());
		START_ITER(iter, d, DescriptorData *, descriptor_list) {
			if ((vict = d->character) && (vict != ch)) {
				GET_HIT(vict) = GET_MAX_HIT(vict);
				GET_MOVE(vict) = GET_MAX_MOVE(vict);

//				if ((GET_LEVEL(ch) >= LVL_ADMIN) && (GET_LEVEL(vict) >= LVL_IMMORT)) {
//					for (i = 1; i <= MAX_SKILLS; i++)
//						SET_SKILL(vict, i, 100);
//
//					if (GET_LEVEL(vict) >= LVL_STAFF) {
//						vict->real_abils.intel = 200;
//						vict->real_abils.per = 200;
//						vict->real_abils.dex = 200;
//						vict->real_abils.str = 200;
//						vict->real_abils.con = 200;
//					}
//					vict->aff_abils = vict->real_abils;
//				}
				vict->update_pos();
				act("You have been fully healed by $N!", FALSE, vict, 0, ch, TO_CHAR);
			}
		} END_ITER(iter);
	} else if (!(vict = get_char_vis(ch, buf)))
		send_to_char(NOPERSON, ch);
	else {
		GET_HIT(vict) = GET_MAX_HIT(vict);
		GET_MOVE(vict) = GET_MAX_MOVE(vict);

		if (IS_STAFF(vict)) {
			for (i = 1; i <= MAX_SKILLS; i++)
				SET_SKILL(vict, i, 100);

			vict->real_abils.intel = 200;
			vict->real_abils.per = 200;
			vict->real_abils.agi = 200;
			vict->real_abils.str = 200;
			vict->real_abils.con = 200;

			vict->aff_abils = vict->real_abils;
		}
		vict->update_pos();
		send_to_char(OK, ch);
		act("You have been fully healed by $N!", FALSE, vict, 0, ch, TO_CHAR);
	}
	release_buffer(buf);
}


void perform_immort_vis(CharData *ch) {
	if (GET_INVIS_LEV(ch) == 0 && !AFF_FLAGGED(ch, AFF_HIDE | AFF_INVISIBLE)) {
		send_to_char("You are already fully visible.\r\n", ch);
		return;
	}

	GET_INVIS_LEV(ch) = 0;
	appear(ch);
	send_to_char("You are now fully visible.\r\n", ch);
}


void perform_immort_invis(CharData *ch, int level) {
	CharData *tch;
	
	if (IS_NPC(ch))
		return;

	START_ITER(iter, tch, CharData *, world[IN_ROOM(ch)].people) {
		if (tch == ch)
			continue;
		if (GET_LEVEL(tch) >= GET_INVIS_LEV(ch) && GET_LEVEL(tch) < level)
			act("You blink and suddenly realize that $n is gone.", FALSE, ch, 0, tch, TO_VICT);
		if (GET_LEVEL(tch) < GET_INVIS_LEV(ch) && GET_LEVEL(tch) >= level)
			act("You suddenly realize that $n is standing beside you.", FALSE, ch, 0, tch, TO_VICT);
	} END_ITER(iter);

	GET_INVIS_LEV(ch) = level;
	ch->Send("Your invisibility level is %d.\r\n", level);
}


ACMD(do_invis) {
	int level;
	char *arg;
	
	if (IS_NPC(ch)) {
		send_to_char("You can't do that!\r\n", ch);
		return;
	}

	arg = get_buffer(MAX_INPUT_LENGTH);
	one_argument(argument, arg);
	if (!*arg) {
		if (GET_INVIS_LEV(ch) > 0)	perform_immort_vis(ch);
		else						perform_immort_invis(ch, 101);
	} else {
		level = atoi(arg);
		if (level > 101)			send_to_char("You can't go invisible above 101.\r\n", ch);
		else if (level < 1)			perform_immort_vis(ch);
		else						perform_immort_invis(ch, level);
	}
	release_buffer(arg);
}


ACMD(do_gecho) {
//	DescriptorData *pt;
	
	skip_spaces(argument);
	delete_doubledollar(argument);

	if (!*argument)
		send_to_char("That must be a mistake...\r\n", ch);
	else {
		send_to_playersf(ch, "%s\r\n", argument);
		if (PRF_FLAGGED(ch, PRF_NOREPEAT))	send_to_char(OK, ch);
		else								ch->Send("%s\r\n", argument);
	}
}


ACMD(do_poofset) {
	char **msg;

	switch (subcmd) {
		case SCMD_POOFIN:    msg = &(POOFIN(ch));    break;
		case SCMD_POOFOUT:   msg = &(POOFOUT(ch));   break;
		default:    return;    break;
	}

	skip_spaces(argument);

	if (*msg)	free(*msg);
	*msg = (*argument) ? str_dup(argument) : NULL;

	send_to_char(OK, ch);
}


ACMD(do_dc) {
	DescriptorData *d;
	int num_to_dc;
	char *arg = get_buffer(MAX_INPUT_LENGTH);

	one_argument(argument, arg);
	if (!(num_to_dc = atoi(arg))) {
		send_to_char("Usage: DC <user number> (type USERS for a list)\r\n", ch);
		release_buffer(arg);
		return;
	}
	release_buffer(arg);
	
	START_ITER(iter, d, DescriptorData *, descriptor_list) {
		if (d->desc_num == num_to_dc)
			break;
	} END_ITER(iter);

	if (!d) {
		send_to_char("No such connection.\r\n", ch);
		return;
	}
	if (d->character && ((IS_STAFF(d->character) && !STF_FLAGGED(ch, STAFF_ADMIN)) || STF_FLAGGED(d->character, STAFF_ADMIN))) {
//		send_to_char("Umm.. maybe that's not such a good idea...\r\n", ch);
		if (!ch->CanSee(d->character))
			send_to_char("No such connection.\r\n", ch);
		else
			send_to_char("Umm.. maybe that's not such a good idea...\r\n", ch);
		return;
	}
	/* We used to just close the socket here using close_socket(), but
	 * various people pointed out this could cause a crash if you're
	 * closing the person below you on the descriptor list.  Just setting
	 * to CON_CLOSE leaves things in a massively inconsistent state so I
	 * had to add this new flag to the descriptor.
	 */
	STATE(d) = CON_DISCONNECT;
	ch->Send("Connection #%d closed.\r\n", num_to_dc);
	log("(GC) Connection closed by %s.", ch->RealName());
}



ACMD(do_wizlock) {
	int value;
	char *when;
	char *buf = get_buffer(MAX_INPUT_LENGTH);

	one_argument(argument, buf);
	if (*buf) {
		value = atoi(buf);
		if (value < 0 || value > 101) {
			send_to_char("Invalid wizlock value.\r\n", ch);
			release_buffer(buf);
			return;
		}
		circle_restrict = value;
		when = "now";
	} else
		when = "currently";

	switch (circle_restrict) {
		case 0:
			sprintf(buf, "The game is %s completely open.\r\n", when);
			break;
		case 1:
			sprintf(buf, "The game is %s closed to new players.\r\n", when);
			break;
		default:
			sprintf(buf, "Only level %d and above may enter the game %s.\r\n", circle_restrict, when);
			break;
	}
	send_to_char(buf, ch);
	release_buffer(buf);
}


ACMD(do_date) {
	char *tmstr;
	time_t mytime;
	int d, h, m;
	
	if (subcmd == SCMD_DATE)
		mytime = time(0);
	else
		mytime = boot_time;

	tmstr = asctime(localtime(&mytime));
	*(tmstr + strlen(tmstr) - 1) = '\0';

	if (subcmd == SCMD_DATE)
		ch->Send("Current machine time: %s\r\n", tmstr);
	else {
		mytime = time(0) - boot_time;
		d = mytime / 86400;
		h = (mytime / 3600) % 24;
		m = (mytime / 60) % 60;

		ch->Send("Up since %s: %d day%s, %d:%02d\r\n", tmstr, d, ((d == 1) ? "" : "s"), h, m);
	}
}



ACMD(do_last) {
	char *buf = get_buffer(MAX_INPUT_LENGTH);
 	CharData *tempchar = new CharData();

	one_argument(argument, buf);
	if (!*buf)
		send_to_char("For whom do you wish to search?\r\n", ch);
	else if (tempchar->load(buf) < 0)
		send_to_char("There is no such player.\r\n", ch);
//	else if ((GET_LEVEL(tempchar) > GET_LEVEL(ch)) && (GET_LEVEL(ch) < LVL_ADMIN))
//		send_to_char("You lack the security for that!\r\n", ch);
	else {
		ch->Send("[%5d] [%2d %s] %-12s : %-18s : %-20s\r\n",
				GET_IDNUM(tempchar), GET_LEVEL(tempchar),
				race_abbrevs[(int) GET_RACE(tempchar)], tempchar->GetName(), tempchar->player->host,
				ctime(&tempchar->player->time.logon));
	}
	delete tempchar;
	release_buffer(buf);
}


ACMD(do_force) {
	DescriptorData *i;
	CharData *vict;
	char	*to_force = get_buffer(MAX_INPUT_LENGTH + 2),
			*arg = get_buffer(MAX_INPUT_LENGTH),
			*buf = get_buffer(SMALL_BUFSIZE),
			*buf1 = get_buffer(SMALL_BUFSIZE);
	
	half_chop(argument, arg, to_force);

	sprintf(buf1, "$n has forced you to '%s'.", to_force);

	if (!*arg || !*to_force)
		send_to_char("Whom do you wish to force do what?\r\n", ch);
	else if ((str_cmp("all", arg) && str_cmp("room", arg))) {
		if (!(vict = get_char_vis(ch, arg)))
			send_to_char(NOPERSON, ch);
		else if ((IS_STAFF(vict) && !STF_FLAGGED(ch, STAFF_ADMIN)) || STF_FLAGGED(vict, STAFF_ADMIN))
			send_to_char("No, no, no!\r\n", ch);
		else {
			send_to_char(OK, ch);
			act(buf1, TRUE, ch, NULL, vict, TO_VICT);
			mudlogf( NRM, LVL_STAFF, TRUE,  "(GC) %s forced %s to %s", ch->RealName(), vict->RealName(), to_force);
			command_interpreter(vict, to_force);
		}
	} else if (!str_cmp("room", arg)) {
		send_to_char(OK, ch);
		mudlogf( NRM, LVL_STAFF, TRUE,  "(GC) %s forced room %d to %s", ch->RealName(), world[IN_ROOM(ch)].number, to_force);

		START_ITER(iter, vict, CharData *, world[IN_ROOM(ch)].people) {
			if ((IS_STAFF(vict) && !STF_FLAGGED(ch, STAFF_ADMIN)) || STF_FLAGGED(vict, STAFF_ADMIN))
				continue;
			act(buf1, TRUE, ch, NULL, vict, TO_VICT);
			command_interpreter(vict, to_force);
		} END_ITER(iter);
	} else { /* force all */
		send_to_char(OK, ch);
		mudlogf( NRM, LVL_STAFF, TRUE,  "(GC) %s forced all to %s", ch->RealName(), to_force);

		START_ITER(iter, i, DescriptorData *, descriptor_list) {
			if ((STATE(i) != CON_PLAYING) || !(vict = i->character) || (vict == ch) || STF_FLAGGED(vict, STAFF_ADMIN))
				continue;
			if (IS_STAFF(vict) && !STF_FLAGGED(ch, STAFF_ADMIN))
				continue;
			act(buf1, TRUE, ch, NULL, vict, TO_VICT);
			command_interpreter(vict, to_force);
		} END_ITER(iter);
	}
	release_buffer(to_force);
	release_buffer(buf);
	release_buffer(buf1);
	release_buffer(arg);
}



ACMD(do_wiznet) {
	DescriptorData *d;
	char emote = FALSE, any = FALSE, *buf1, *buf2;
	int level = LVL_IMMORT;
	LListIterator<DescriptorData *>	iter(descriptor_list);
	
	skip_spaces(argument);
	delete_doubledollar(argument);

	if (!*argument) {
		send_to_char(	"Usage: wiznet <text> | #<level> <text> | *<emotetext> |\r\n "
						"       wiznet @<level> *<emotetext> | wiz @\r\n", ch);
		return;
	}
	
	switch (*argument) {
		case '*':
			emote = TRUE;
		case '#':
			buf1 = get_buffer(MAX_INPUT_LENGTH);
			one_argument(argument + 1, buf1);
			if (is_number(buf1)) {
				half_chop(argument + 1, buf1, argument);
				level = MAX(atoi(buf1), LVL_IMMORT);
				if (level > GET_LEVEL(ch)) {
					send_to_char("You can't wizline above your own level.\r\n", ch);
					release_buffer(buf1);
					return;
				}
			} else if (emote)
				argument++;
			release_buffer(buf1);
			break;
		case '@':
			while ((d = iter.Next())) {
				if ((STATE(d) == CON_PLAYING) && IS_STAFF(d->character) &&
						!CHN_FLAGGED(d->character, Channel::NoWiz) &&
						(ch->CanSee(d->character) || GET_LEVEL(ch) == LVL_CODER)) {
					if (!any) {
						ch->Send("Gods online:\r\n");
						any = TRUE;
					}
					ch->Send("  %s", d->character->GetName());
					if (PLR_FLAGGED(d->character, PLR_WRITING))			ch->Send(" (Writing)");
					else if (PLR_FLAGGED(d->character, PLR_MAILING))	ch->Send(" (Writing mail)");
					ch->Send("\r\n");
				}
			}
			any = FALSE;
			iter.Reset();
			while ((d = iter.Next())) {
				if ((STATE(d) == CON_PLAYING) && IS_STAFF(d->character) &&
						CHN_FLAGGED(d->character, Channel::NoWiz) &&
						ch->CanSee(d->character)) {
					if (!any) {
						ch->Send("Gods offline:\r\n");
						any = TRUE;
					}
					ch->Send("  %s\r\n", d->character->GetName());
				}
			}
			return;
		case '\\':
			++argument;
			break;
		default:
			break;
	}
	if (CHN_FLAGGED(ch, Channel::NoWiz)) {
		send_to_char("You are offline!\r\n", ch);
		return;
	}
	skip_spaces(argument);

	if (!*argument) {
		send_to_char("Don't bother the gods like that!\r\n", ch);
		return;
	}
		
	buf1 = get_buffer(MAX_STRING_LENGTH);
	buf2 = get_buffer(MAX_STRING_LENGTH);
	
	if (level > LVL_IMMORT) {
		sprintf(buf1, "%s: <%d> %s%s\r\n", ch->GetName(), level, emote ? "<--- " : "", argument);
		sprintf(buf2, "Someone: <%d> %s%s\r\n", level, emote ? "<--- " : "", argument);
	} else {
		sprintf(buf1, "%s: %s%s\r\n", ch->GetName(), emote ? "<--- " : "", argument);
		sprintf(buf2, "Someone: %s%s\r\n", emote ? "<--- " : "", argument);
	}

	while ((d = iter.Next())) {
		if ((STATE(d) == CON_PLAYING) && (GET_LEVEL(d->character) >= level) &&
				!CHN_FLAGGED(d->character, Channel::NoWiz) &&
				!PLR_FLAGGED(d->character, PLR_WRITING | PLR_MAILING)
				&& ((d != ch->desc) || !PRF_FLAGGED(d->character, PRF_NOREPEAT))) {
			send_to_char("`c", d->character);
			if (d->character->CanSee(ch))
				send_to_char(buf1, d->character);
			else
				send_to_char(buf2, d->character);
			send_to_char("`n", d->character);
		}
	}

	if (PRF_FLAGGED(ch, PRF_NOREPEAT))
		send_to_char(OK, ch);

	release_buffer(buf1);
	release_buffer(buf2);
}



ACMD(do_zreset) {
	int i, j;
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	
	one_argument(argument, arg);
	if (!*arg) {
		send_to_char("You must specify a zone.\r\n", ch);
		release_buffer(arg);
		return;
	}
	if (*arg == '*') {
		for (i = 0; i <= top_of_zone_table; i++)
			reset_zone(i);
			send_to_char("Reset world.\r\n", ch);
			mudlogf( NRM, LVL_IMMORT, TRUE,  "(GC) %s reset entire world.", ch->RealName());
			release_buffer(arg);
			return;
	} else if (*arg == '.')
		i = world[IN_ROOM(ch)].zone;
	else {
		j = atoi(arg);
		for (i = 0; i <= top_of_zone_table; i++)
			if (zone_table[i].number == j)
				break;
	}
	if (i >= 0 && i <= top_of_zone_table) {
		reset_zone(i);
		ch->Send("Reset zone %d (#%d): %s.\r\n", i, zone_table[i].number, zone_table[i].name);
		mudlogf( NRM, LVL_STAFF, TRUE,  "(GC) %s reset zone %d (%s)", ch->RealName(), zone_table[i].number, zone_table[i].name);
	} else
		send_to_char("Invalid zone number.\r\n", ch);
	
	release_buffer(arg);
}


/*
 *  General fn for wizcommands of the sort: cmd <player>
 */

ACMD(do_wizutil) {
	CharData *vict;
	SInt32 result;
	char *arg = get_buffer(MAX_INPUT_LENGTH);

	one_argument(argument, arg);

	if (!*arg)
		send_to_char("Yes, but for whom?!?\r\n", ch);
	else if (!(vict = get_char_vis(ch, arg)))
		send_to_char("There is no such player.\r\n", ch);
	else if (IS_NPC(vict))
		send_to_char("You can't do that to a mob!\r\n", ch);
	else if ((IS_STAFF(vict) && !STF_FLAGGED(ch, STAFF_ADMIN)) || STF_FLAGGED(vict, STAFF_ADMIN))
		send_to_char("Hmmm...you'd better not.\r\n", ch);
	else {
		switch (subcmd) {
			case SCMD_REROLL:
				send_to_char("Rerolled...\r\n", ch);
				roll_real_abils(vict);
				log("(GC) %s has rerolled %s.", ch->RealName(), vict->RealName());
				ch->Send("New stats: Str %d, Int %d, Per %d, Agl %d, Con %d\r\n",
						GET_STR(vict), GET_INT(vict), GET_PER(vict),
						GET_AGI(vict), GET_CON(vict));
				break;
			case SCMD_PARDON:
				if (!PLR_FLAGGED(vict, PLR_TRAITOR)) {
					send_to_char("Your victim is not flagged.\r\n", ch);
					release_buffer(arg);
					return;
				}
				REMOVE_BIT(PLR_FLAGS(vict), PLR_TRAITOR);
				send_to_char("Pardoned.\r\n", ch);
				send_to_char("You have been pardoned by the Gods!\r\n", vict);
				mudlogf( BRF, LVL_STAFF, TRUE,  "(GC) %s pardoned by %s", vict->RealName(), ch->RealName());
				break;
			case SCMD_NOTITLE:
				result = PLR_TOG_CHK(vict, PLR_NOTITLE);
				sprintf(arg, "(GC) Notitle %s for %s by %s.", ONOFF(result), vict->RealName(), ch->RealName());
				mudlog(arg, NRM, LVL_STAFF, TRUE);
				strcat(arg, "\r\n");
				send_to_char(arg, ch);
				break;
			case SCMD_SQUELCH:
				result = PLR_TOG_CHK(vict, PLR_NOSHOUT);
				mudlogf(BRF, LVL_STAFF, TRUE, "(GC) Squelch %s for %s by %s.", ONOFF(result), vict->RealName(), ch->RealName());
				ch->Send("(GC) Squelch %s for %s by %s\r\n.", ONOFF(result), vict->RealName(), ch->RealName());
				break;
			case SCMD_FREEZE:
				if (ch == vict) {
					send_to_char("Oh, yeah, THAT'S real smart...\r\n", ch);
					release_buffer(arg);
					return;
				}
				if (PLR_FLAGGED(vict, PLR_FROZEN)) {
					send_to_char("Your victim is already pretty cold.\r\n", ch);
					release_buffer(arg);
					return;
				}
				SET_BIT(PLR_FLAGS(vict), PLR_FROZEN);
//				GET_FREEZE_LEV(vict) = GET_LEVEL(ch);
				send_to_char("A bitter wind suddenly rises and drains every bit of heat from your body!\r\nYou feel frozen!\r\n", vict);
				send_to_char("Frozen.\r\n", ch);
				act("A sudden cold wind conjured from nowhere freezes $n!", FALSE, vict, 0, 0, TO_ROOM);
				mudlogf(BRF, LVL_STAFF, TRUE,  "(GC) %s frozen by %s.", vict->RealName(), ch->RealName());
				break;
			case SCMD_THAW:
				if (!PLR_FLAGGED(vict, PLR_FROZEN)) {
					send_to_char("Sorry, your victim is not morbidly encased in ice at the moment.\r\n", ch);
					release_buffer(arg);
					return;
				}
//				if (GET_FREEZE_LEV(vict) > GET_LEVEL(ch)) {
//					ch->Send("Sorry, a level %d God froze %s... you can't unfreeze %s.\r\n",
//							GET_FREEZE_LEV(vict), vict->RealName(), HMHR(vict));
//					release_buffer(arg);
//					return;
//				}
				mudlogf(BRF, LVL_STAFF, TRUE,  "(GC) %s un-frozen by %s.", vict->RealName(), ch->RealName());
				REMOVE_BIT(PLR_FLAGS(vict), PLR_FROZEN);
				send_to_char("Thawed.\r\n", ch);
				act("$N turns up the heat, thawing you out.", FALSE, vict, 0, ch, TO_CHAR);
				act("A sudden fireball conjured from nowhere thaws $n!", FALSE, vict, 0, 0, TO_ROOM);
				break;
			case SCMD_UNAFFECT:
//				if (vict->affected) {
				{
				Affect *aff;
				START_ITER(iter, aff, Affect *, vict->affected) {	
					aff->Remove(vict);
				} END_ITER(iter);
				}
				send_to_char(	"There is a brief flash of light!\r\n"
								"You feel slightly different.\r\n", vict);
				send_to_char("All spells removed.\r\n", ch);
				CheckRegenRates(vict);
//				} else {
//					send_to_char("Your victim does not have any affections!\r\n", ch);
//					release_buffer(arg);
//					return;
//				}
				break;
			default:
				log("SYSERR: Unknown subcmd %d passed to do_wizutil (" __FILE__ ")", subcmd);
				break;
		}
		vict->save(NOWHERE);
	}
	release_buffer(arg);
}


/* single zone printing fn used by "show zone" so it's not repeated in the
   code 3 times ... -je, 4/6/93 */

void print_zone_to_buf(char *bufptr, int zone) {
	sprintf(bufptr + strlen(bufptr), "%3d %-30.30s Age: %3d; Reset: %3d (%1d); Top: %5d\r\n",
			zone_table[zone].number, zone_table[zone].name,
			zone_table[zone].age, zone_table[zone].lifespan,
			zone_table[zone].reset_mode, zone_table[zone].top);
}


void show_deleted(CharData * ch) {
	UInt32	found, x;
	char	*buf = get_buffer(MAX_STRING_LENGTH);
	
	found = 0;
	for (x = 0; x <= top_of_mobt; x++) {		/* List mobs */
		if (MOB_FLAGGED((CharData *)mob_index[x].proto, MOB_DELETED)) {
			if (found == 0)
				strcat(buf, "Deleted Mobiles:\r\n");
			sprintf(buf + strlen(buf), "`c%3d.  `n[`y%5d`n] %s\r\n", ++found, mob_index[x].vnum, SSData(((CharData *)mob_index[x].proto)->general.shortDesc));
		}
	}
	
	found = 0;
	for (x = 0; x <= top_of_objt; x++) {		/* List objs */
		if (OBJ_FLAGGED((ObjData *)obj_index[x].proto, ITEM_DELETED)) {
			if (found == 0)
				strcat(buf, "\r\nDeleted Objects:\r\n");
			sprintf(buf + strlen(buf), "`c%3d.  `n[`y%5d`n] %s\r\n", ++found, obj_index[x].vnum, SSData(((ObjData *)obj_index[x].proto)->shortDesc));
		}
	}
	
	found = 0;
	for (x = 0; x <= top_of_world; x++) {		/* List rooms */
		if (ROOM_FLAGGED(x, ROOM_DELETED)) {
			if (found == 0)
				strcat(buf, "\r\nDeleted Rooms:\r\n");
			sprintf(buf + strlen(buf), "`c%3d.  `n[`y%5d`n] %s\r\n", ++found, world[x].number, world[x].GetName("<ERROR>"));
		}
	}
//	strcat(buf, "\r\n");
	page_string(ch->desc, buf, 1);
	
	release_buffer(buf);
}


ACMD(do_show) {
	int j, k, l, con;
	UInt32	i;
	char self = 0;
	CharData *vict;
	char *field, *value, *birth, *arg, *buf;
//	extern LList<SString *>	SStrings;

	struct show_struct {
		char *cmd;
		char level;
	} fields[] = {
		{ "nothing"	, 0				},	//	0
		{ "zones"	, LVL_IMMORT	},	//	1
		{ "player"	, LVL_STAFF		},
		{ "rent"	, LVL_STAFF		},
		{ "stats"	, LVL_IMMORT	},
		{ "errors"	, LVL_STAFF		},	//	5
		{ "death"	, LVL_STAFF		},
		{ "godrooms", LVL_STAFF		},
		{ "shops"	, LVL_IMMORT	},
		{ "houses"	, LVL_STAFF		},
		{ "deleted"	, LVL_STAFF		},	//	10
		{ "buffers"	, LVL_STAFF		},
		{ "\n", 0 }
	};

	skip_spaces(argument);

	if (!*argument) {
		ch->Send("Show options:\r\n");
		for (j = 0, i = 1; fields[i].level; i++)
			if (fields[i].level <= GET_LEVEL(ch))
				ch->Send("%-15s%s", fields[i].cmd, (!(++j % 5) ? "\r\n" : ""));
		ch->Send("\r\n");
		return;
	}
	
	arg = get_buffer(MAX_INPUT_LENGTH);
	field = get_buffer(MAX_INPUT_LENGTH);
	value = get_buffer(MAX_INPUT_LENGTH);

	strcpy(arg, two_arguments(argument, field, value));

	for (l = 0; *(fields[l].cmd) != '\n'; l++)
		if (!strncmp(field, fields[l].cmd, strlen(field)))
			break;

	if (GET_LEVEL(ch) < fields[l].level) {
		send_to_char("You are not godly enough for that!\r\n", ch);
		release_buffer(field);
		release_buffer(value);
		release_buffer(arg);
		return;
	}
	if (!strcmp(value, "."))
		self = 1;

	switch (l) {
		case 1:			/* zone */
			buf = get_buffer(MAX_STRING_LENGTH);
			/* tightened up by JE 4/6/93 */
			if (self)
				print_zone_to_buf(buf, world[IN_ROOM(ch)].zone);
			else if (*value && is_number(value)) {
				for (j = atoi(value), i = 0; zone_table[i].number != j && i <= top_of_zone_table; i++);
					if (i <= top_of_zone_table)
						print_zone_to_buf(buf, i);
					else {
						send_to_char("That is not a valid zone.\r\n", ch);
						release_buffer(buf);
						break;
					}
			} else
				for (i = 0; i <= top_of_zone_table; i++)
					print_zone_to_buf(buf, i);
			send_to_char(buf, ch);
			release_buffer(buf);
			break;
		case 2:			/* player */
			vict = new CharData();
			if (!*value)
				send_to_char("A name would help.\r\n", ch);
			else if (vict->load(value) < 0)
				send_to_char("There is no such player.\r\n", ch);
			else {
				ch->Send(	"Player: %-12s (%s) [%2d %s]\r\n"
							"MP: %-8d  Pracs: %-3d\r\n",
						vict->RealName(), genders[GET_SEX(vict)], GET_LEVEL(vict), race_abbrevs[GET_RACE(vict)],
						GET_MISSION_POINTS(vict), GET_PRACTICES(vict));
				birth = get_buffer(80);
				strcpy(birth, ctime(&vict->player->time.birth));
				ch->Send("Started: %-20.16s  Last: %-20.16s  Played: %3dh %2dm\r\n",
						birth, ctime(&vict->player->time.logon), (int) (vict->player->time.played / 3600),
						(int) (vict->player->time.played / 60 % 60));
				release_buffer(birth);
			}
			delete vict;
			break;
		case 3:
			Crash_listrent(ch, value);
			break;
		case 4:
			i = 0;
			j = 0;
			con = 0;
			{
			START_ITER(iter, vict, CharData *, character_list) {
				if (IS_NPC(vict))
					j++;
				else if (ch->CanSee(vict)) {
					i++;
					if (vict->desc)
						con++;
				}
			} END_ITER(iter);
			}
			ch->Send(	"Current stats:\r\n"
						"  %5d players in game  %5d connected\r\n"
						"  %5d registered\r\n"
						"  %5d mobiles          %5d prototypes\r\n"
						"  %5d objects          %5d prototypes\r\n"
						"  %5d triggers         %5d prototypes\r\n"
						"  %5d rooms            %5d zones\r\n"
//						"  %5d shared strings\r\n"
						"  %5d large bufs\r\n"
						"  %5d buf switches     %5d overflows\r\n"
						"  %5d buf cache hits   %5d buf cache misses\r\n",

					i, con,
					top_of_p_table + 1,
					j, top_of_mobt + 1,
					object_list.Count(), top_of_objt + 1,
					trig_list.Count(), top_of_trigt + 1,
					top_of_world + 1, top_of_zone_table + 1,
//					SStrings.Count(),
					buf_largecount,
					buf_switches, buf_overflows,
					buffer_cache_stat[BUFFER_CACHE_HITS],
					buffer_cache_stat[BUFFER_CACHE_MISSES]);
			break;
		case 5:
			send_to_char("Errant Rooms\r\n------------\r\n", ch);
			for (i = 0, k = 0; i <= top_of_world; i++)
				for (j = 0; j < NUM_OF_DIRS; j++)
					if (world[i].dir_option[j] && world[i].dir_option[j]->to_room == 0)
						ch->Send("%2d: [%5d] %s\r\n", ++k, world[i].number, world[i].GetName("<ERROR>"));
			break;
		case 6:
			send_to_char("Death Traps\r\n-----------\r\n", ch);
			for (i = 0, j = 0; i <= top_of_world; i++)
				if (ROOM_FLAGGED(i, ROOM_DEATH))
					ch->Send("%2d: [%5d] %s\r\n", ++j, world[i].number, world[i].GetName("<ERROR>"));
			break;
		case 7:
			send_to_char("Godrooms\r\n--------------------------\r\n", ch);
			for (i = 0, j = 0; i < top_of_world; i++)
				if (ROOM_FLAGGED(i, ROOM_GODROOM) || ROOM_FLAGGED(i, ROOM_GRGODROOM))
					ch->Send("%2d: [%5d] %s\r\n", j++, world[i].number, world[i].GetName("<ERROR>"));
			break;
		case 8:
			show_shops(ch, value);
			break;
		case 9:
			hcontrol_list_houses(ch);
			break;
		case 10:
			show_deleted(ch);
			break;
		case 11:
			show_buffers(ch, -1, 1);
			show_buffers(ch, -1, 2);
			show_buffers(ch, -1, 0);
			break;
		default:
			send_to_char("Sorry, I don't understand that.\r\n", ch);
			break;
	}
	release_buffer(field);
	release_buffer(value);
	release_buffer(arg);
}


#define PC   1
#define NPC  2
#define BOTH 3

#define MISC	0
#define BINARY	1
#define NUMBER	2

#define SET_OR_REMOVE(flagset, flags) { \
	if (on) SET_BIT(flagset, flags); \
	else if (off) REMOVE_BIT(flagset, flags); }

#define RANGE(low, high) (value = MAX((low), MIN((high), (value))))


struct set_struct {
	char *cmd;
	char level;
	char pcnpc;
	char type;
} set_fields[] = {
	{ "brief",		LVL_STAFF, 	PC, 	BINARY },	/* 0 */
	{ "invstart", 	LVL_STAFF, 	PC, 	BINARY },	/* 1 */
	{ "title",		LVL_STAFF, 	PC, 	MISC },
	{ "nosummon", 	LVL_STAFF, 	PC, 	BINARY },
	{ "maxhit",		LVL_SRSTAFF, 	BOTH, 	NUMBER },
	{ "nodelete", 	LVL_ADMIN, 	PC, 	BINARY },	/* 5 */
	{ "maxmove", 	LVL_SRSTAFF, 	BOTH, 	NUMBER },
	{ "hit", 		LVL_SRSTAFF, 	BOTH, 	NUMBER },
	{ "mission",	LVL_STAFF, 	PC, 	BINARY },
	{ "move",		LVL_STAFF, 	BOTH, 	NUMBER },
	{ "race",		LVL_ADMIN,	BOTH,	MISC },		/* 10 */
	{ "str",		LVL_SRSTAFF, 	BOTH, 	NUMBER },
	{ "passwd",		LVL_ADMIN, 	PC, 	MISC },
	{ "int", 		LVL_SRSTAFF, 	BOTH, 	NUMBER },
	{ "per", 		LVL_SRSTAFF, 	BOTH, 	NUMBER },
	{ "agi", 		LVL_SRSTAFF, 	BOTH, 	NUMBER },	/* 15 */
	{ "con", 		LVL_SRSTAFF, 	BOTH, 	NUMBER },
	{ "sex", 		LVL_ADMIN, 	BOTH, 	MISC },
	{ "ac", 		LVL_SRSTAFF, 	BOTH, 	NUMBER },
	{ "mp",			LVL_SRSTAFF, 	BOTH, 	NUMBER },
	{ "loadroom", 	LVL_SRSTAFF, 	PC, 	MISC },		/* 20 */
	{ "color",		LVL_STAFF, 	PC, 	BINARY },
	{ "hitroll", 	LVL_SRSTAFF,	BOTH, 	NUMBER },
	{ "damroll", 	LVL_SRSTAFF,	BOTH, 	NUMBER },
	{ "invis",		LVL_SRSTAFF, 	PC, 	NUMBER },
	{ "nohassle", 	LVL_ADMIN, 	PC, 	BINARY },	/* 25 */
	{ "frozen",		LVL_ADMIN,	PC, 	BINARY },
	{ "practices", 	LVL_SRSTAFF,	PC, 	NUMBER },
	{ "lessons", 	LVL_SRSTAFF,	PC, 	NUMBER },
	{ "drunk",		LVL_STAFF, 	BOTH, 	MISC },
	{ "hunger",		LVL_STAFF, 	BOTH, 	MISC },		/* 30 */
	{ "thirst",		LVL_STAFF, 	BOTH, 	MISC },
	{ "traitor",	LVL_SRSTAFF, 	PC, 	BINARY },
	{ "idnum",		LVL_CODER, 	PC, 	NUMBER },
	{ "level",		LVL_ADMIN, 	BOTH, 	NUMBER },
	{ "room",		LVL_ADMIN, 	BOTH, 	NUMBER },	/* 35 */
	{ "roomflag", 	LVL_STAFF, 	PC, 	BINARY },
	{ "siteok",		LVL_SRSTAFF, 	PC, 	BINARY },
	{ "deleted", 	LVL_SRSTAFF, 	PC, 	BINARY },
	{ "\n", 0, BOTH, MISC }
};


bool perform_set(CharData *ch, CharData *vict, SInt32 mode, char *val_arg) {
	SInt32	i, value = 0;
	bool	on = false, off = false;
	char *output;
	
	if (GET_LEVEL(ch) != LVL_ADMIN) {
		if (((IS_STAFF(vict) && !STF_FLAGGED(ch, STAFF_ADMIN)) || STF_FLAGGED(vict, STAFF_ADMIN)) && (vict != ch)) {
			send_to_char("Maybe that's not such a great idea...\r\n", ch);
			return 0;
		}
	}
	if (GET_LEVEL(ch) < set_fields[mode].level) {
		send_to_char("You do not have authority to do that.\r\n", ch);
		return 0;
	}
	
	//	Make sure the PC/NPC is correct
	if (IS_NPC(vict) && !(set_fields[mode].pcnpc & NPC)) {
		send_to_char("You can't do that to a mob!", ch);
		return 0;
	} else if (!IS_NPC(vict) && !(set_fields[mode].pcnpc & PC)) {
		send_to_char("You can only do that to a mob!", ch);		
		return 0;
	}
	
	output = get_buffer(MAX_STRING_LENGTH);
	
	if (set_fields[mode].type == BINARY) {
		if (!strcmp(val_arg, "on") || !strcmp(val_arg, "yes"))
			on = true;
		else if (!strcmp(val_arg, "off") || !strcmp(val_arg, "no"))
			off = true;
		if (!(on || off)) {
			send_to_char("Value must be 'on' or 'off'.\r\n", ch);
			release_buffer(output);
			return 0;
		}
		sprintf(output, "%s %s for %s.", set_fields[mode].cmd, ONOFF(on), vict->RealName());
	} else if (set_fields[mode].type == NUMBER) {
		value = atoi(val_arg);
		sprintf(output, "%s's %s set to %d.", vict->RealName(), set_fields[mode].cmd, value);
	} else {
		strcpy(output, "Okay.");
	}
	
	switch (mode) {
		case 0:
			SET_OR_REMOVE(PRF_FLAGS(vict), PRF_BRIEF);
			break;
		case 1:
			SET_OR_REMOVE(PLR_FLAGS(vict), PLR_INVSTART);
			break;
		case 2:
			vict->set_title(val_arg);
			sprintf(output, "%s's title is now: %s", vict->RealName(), GET_TITLE(vict));
			vict->Send("%s set your title to \"%s\".\r\n", ch->RealName(), GET_TITLE(vict));
			break;
		case 3:
			SET_OR_REMOVE(PRF_FLAGS(vict), PRF_SUMMONABLE);
			sprintf(output, "Nosummon %s for %s.\r\n", ONOFF(!on), vict->RealName());
			break;
		case 4:
			vict->points.max_hit = RANGE(1, 5000);
			vict->AffectTotal();
			break;
		case 5:
			SET_OR_REMOVE(PLR_FLAGS(vict), PLR_NODELETE);
			break;
		case 6:
			vict->points.max_move = RANGE(1, 5000);
			vict->AffectTotal();
			break;
		case 7:
			vict->points.hit = RANGE(-9, vict->points.max_hit);
			vict->AffectTotal();
			break;
		case 8:
			SET_OR_REMOVE(CHN_FLAGS(vict), Channel::Mission);
			break;
		case 9:
			vict->points.move = RANGE(0, vict->points.max_move);
			vict->AffectTotal();
			break;
		case 10:
			if ((i = parse_race(*val_arg)) == RACE_UNDEFINED) {
				send_to_char("That is not a race.\r\n", ch);
				release_buffer(output);
				return false;
			}
			GET_RACE(vict) = (Race)i;
			break;
		case 11:
			RANGE(1, 200);
			vict->real_abils.str = value;
			vict->AffectTotal();
			break;
		case 12:
			if (GET_IDNUM(ch) > 1) {
				send_to_char("Please don't use this command, yet.\r\n", ch);
				release_buffer(output);
				return false;
			} else if (GET_LEVEL(vict) >= LVL_ADMIN) {
				send_to_char("You cannot change that.\r\n", ch);
				release_buffer(output);
				return false;
			} else {
				strncpy(vict->player->passwd, CRYPT(val_arg, tmp_store.name), MAX_PWD_LENGTH);
				vict->player->passwd[MAX_PWD_LENGTH] = '\0';
				sprintf(output, "Password changed to '%s'.", val_arg);
			}
			break;
		case 13:
			RANGE(1, 200);
			vict->real_abils.intel = value;
			vict->AffectTotal();
			break;
		case 14:
			RANGE(1, 200);
			vict->real_abils.per = value;
			vict->AffectTotal();
			break;
		case 15:
			RANGE(1, 200);
			vict->real_abils.agi = value;
			vict->AffectTotal();
			break;
		case 16:
			RANGE(1, 200);
			vict->real_abils.con = value;
			vict->AffectTotal();
			break;
		case 17:
			if (!str_cmp(val_arg, "male"))
				vict->general.sex = SEX_MALE;
			else if (!str_cmp(val_arg, "female"))
				vict->general.sex = SEX_FEMALE;
			else if (!str_cmp(val_arg, "neutral"))
				vict->general.sex = SEX_NEUTRAL;
			else {
				send_to_char("Must be 'male', 'female', or 'neutral'.\r\n", ch);
				release_buffer(output);
				return false;
			}
			break;
		case 18:
			vict->points.armor = RANGE(/* -100, 100 */0, 1000);
			vict->AffectTotal();
			break;
		case 19:
			GET_MISSION_POINTS(vict) = RANGE(0, 100000000);
			break;
		case 20:
			if (!str_cmp(val_arg, "off"))
				REMOVE_BIT(PLR_FLAGS(vict), PLR_LOADROOM);
			else if (is_number(val_arg)) {
				value = atoi(val_arg);
				if (real_room(value) == NOWHERE) {
					send_to_char("That room does not exist!", ch);
					release_buffer(output);
					return false;
				}
				SET_BIT(PLR_FLAGS(vict), PLR_LOADROOM);
				GET_LOADROOM(vict) = value;
				sprintf(output, "%s will enter at room #%d.", vict->RealName(), GET_LOADROOM(vict));
			} else {
				send_to_char("Must be 'off' or a room's virtual number.\r\n", ch);
				release_buffer(output);
				return false;
			}
			break;
		case 21:
			SET_OR_REMOVE(PRF_FLAGS(vict), PRF_COLOR);
			break;
		case 22:
			vict->points.hitroll = RANGE(-20, 20);
			vict->AffectTotal();
			break;
		case 23:
			vict->points.damroll = RANGE(-20, 20);
			vict->AffectTotal();
			break;
		case 24:
//			if (IS_STAFF(ch) < LVL_ADMIN && ch != vict) {
//				send_to_char("You aren't godly enough for that!\r\n", ch);
//				release_buffer(output);
//				return false;
//			}
			GET_INVIS_LEV(vict) = RANGE(0, 101);
			break;
		case 25:
//			if (GET_LEVEL(ch) < LVL_ADMIN && ch != vict) {
//				send_to_char("You aren't godly enough for that!\r\n", ch);
//				release_buffer(output);
//				return false;
//			}
			SET_OR_REMOVE(PRF_FLAGS(vict), PRF_NOHASSLE);
			break;
		case 26:
			if (ch == vict) {
				send_to_char("Better not -- could be a long winter!\r\n", ch);
				release_buffer(output);
				return false;
			}
			SET_OR_REMOVE(PLR_FLAGS(vict), PLR_FROZEN);
			break;
		case 27:
		case 28:
			GET_PRACTICES(vict) = RANGE(0, 100);
			break;
		case 29:
		case 30:
		case 31:
			if (!str_cmp(val_arg, "off")) {
				GET_COND(vict, (mode - 29)) = (char) -1;
				sprintf(output, "%s's %s now off.", vict->RealName(), set_fields[mode].cmd);
			} else if (is_number(val_arg)) {
				value = atoi(val_arg);
				RANGE(0, 24);
				GET_COND(vict, (mode - 29)) = (char) value;
				sprintf(output, "%s's %s set to %d.", vict->RealName(), set_fields[mode].cmd, value);
			} else {
				send_to_char("Must be 'off' or a value from 0 to 24.\r\n", ch);
				release_buffer(output);
				return false;
			}
//			CheckRegenRates(vict);
			break;
		case 32:
			SET_OR_REMOVE(PLR_FLAGS(vict), PLR_TRAITOR);
			break;
		case 33:
			if (GET_IDNUM(ch) != 1 || !IS_NPC(vict)) {
				send_to_char("Nuh-uh.", ch);
				release_buffer(output);
				return false;
			}
			GET_IDNUM(vict) = value;
			break;
		case 34:
			if (value > GET_LEVEL(ch) || value > LVL_ADMIN) {
				send_to_char("You can't do that.\r\n", ch);
				release_buffer(output);
				return false;
			}
			RANGE(0, LVL_CODER);
			GET_LEVEL(vict) = value;
			break;
		case 35:
			if ((i = real_room(value)) == NOWHERE) {
				send_to_char("No room exists with that number.\r\n", ch);
				release_buffer(output);
				return false;
			}
			if (IN_ROOM(vict) != NOWHERE)
				vict->from_room();
			vict->to_room(i);
			break;
		case 36:
			SET_OR_REMOVE(PRF_FLAGS(vict), PRF_ROOMFLAGS);
			break;
		case 37:
			SET_OR_REMOVE(PLR_FLAGS(vict), PLR_SITEOK);
			break;
		case 38:
			SET_OR_REMOVE(PLR_FLAGS(vict), PLR_DELETED);
			break;
		default:
			send_to_char("Can't set that!", ch);
			release_buffer(output);
			return false;
	}
	ch->Send("%s\r\n", CAP(output));
	release_buffer(output);
	return true;
}



ACMD(do_set) {
	CharData *vict = NULL, *cbuf = NULL;
	char	*field = get_buffer(MAX_INPUT_LENGTH),
			*name = get_buffer(MAX_INPUT_LENGTH),
			*buf = get_buffer(MAX_INPUT_LENGTH);
	SInt32	mode = -1, len = 0, player_i = 0;
	bool	is_file = false, is_mob = false, is_player = false, retval;

	half_chop(argument, name, buf);
	if (!strcmp(name, "file")) {
		is_file = true;
		half_chop(buf, name, buf);
	} else if (!str_cmp(name, "player")) {
		is_player = true;
		half_chop(buf, name, buf);
	} else if (!str_cmp(name, "mob")) {
		is_mob = true;
		half_chop(buf, name, buf);
	}
	half_chop(buf, field, buf);

	if (!*name || !*field) {
		send_to_char("Usage: set <victim> <field> <value>\r\n", ch);
	}
	
	if (!is_file) {
		if (is_player) {
			if (!(vict = get_player_vis(ch, name, false))) {
				send_to_char("There is no such player.\r\n", ch);
				release_buffer(field);
				release_buffer(name);
				release_buffer(buf);
				return;
			}
		} else {
			if (!(vict = get_char_vis(ch, name))) {
				send_to_char("There is no such creature.\r\n", ch);
				release_buffer(field);
				release_buffer(name);
				release_buffer(buf);
				return;
			}
		}
	} else {
		cbuf = new CharData();
		if ((player_i = cbuf->load(name)) > -1) {
			GET_PFILEPOS(cbuf) = player_i;
			if ((IS_STAFF(cbuf) && !STF_FLAGGED(ch, STAFF_ADMIN)) || STF_FLAGGED(cbuf, STAFF_ADMIN)) {
				delete cbuf;
				send_to_char("Sorry, you can't do that.\r\n", ch);
				release_buffer(field);
				release_buffer(name);
				release_buffer(buf);
				return;
			}
			vict = cbuf;
		} else {
			send_to_char("There is no such player.\r\n", ch);
			delete cbuf;
			release_buffer(field);
			release_buffer(name);
			release_buffer(buf);
			return;
		}
	}
	
	len = strlen(field);
	for (mode = 0; *(set_fields[mode].cmd) != '\n'; mode++)
		if (!strncmp(field, set_fields[mode].cmd, len))
			break;

	retval = perform_set(ch, vict, mode, buf);

	if (retval) {
		if (!IS_NPC(vict)) {
			vict->save(NOWHERE);
			if (is_file)
				send_to_char("Saved in file.\r\n", ch);
		}
	}
	
	if (is_file)
		delete cbuf;
	
	release_buffer(field);
	release_buffer(name);
	release_buffer(buf);
}


static char *logtypes[] = {"off", "brief", "normal", "complete", "\n"};

ACMD(do_syslog) {
	int tp;
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	
	one_argument(argument, arg);

	if (!*arg) {
		tp = ((PRF_FLAGGED(ch, PRF_LOG1) ? 1 : 0) + (PRF_FLAGGED(ch, PRF_LOG2) ? 2 : 0));
		ch->Send("Your syslog is currently %s.\r\n", logtypes[tp]);
	} else if (((tp = search_block(arg, logtypes, FALSE)) == -1))
		send_to_char("Usage: syslog { Off | Brief | Normal | Complete }\r\n", ch);
	else {
		REMOVE_BIT(PRF_FLAGS(ch), PRF_LOG1 | PRF_LOG2);
		SET_BIT(PRF_FLAGS(ch), (PRF_LOG1 * (tp & 1)) | (PRF_LOG2 * (tp & 2) >> 1));

		ch->Send("Your syslog is now %s.\r\n", logtypes[tp]);
	}
	release_buffer(arg);
}



ACMD(do_depiss) {
	char	*arg1 = get_buffer(MAX_INPUT_LENGTH),
			*arg2 = get_buffer(MAX_INPUT_LENGTH);
	CharData *vic = 0;
	CharData *mob  = 0;
 
	two_arguments(argument, arg1, arg2);

	if (arg2)
		send_to_char("Usage: depiss <victim> <mobile>\r\n", ch);
	else if (((vic = get_char_vis(ch, arg1))) /* && !IS_NPC(vic) */) {
		if (((mob = get_char_vis(ch, arg2))) && (IS_NPC(mob))) {
			if (mob->mob->hates && mob->mob->hates->InList(GET_ID(vic))) {
				ch->Send("%s no longer hates %s.\r\n", mob->RealName(), vic->RealName());
				forget(mob, vic);
			} else
				send_to_char("Mobile does not hate the victim!\r\n", ch);
		} else
			send_to_char("Sorry, Mobile Not Found!\r\n", ch);
	} else
		send_to_char("Sorry, Victim Not Found!\r\n", ch);
	release_buffer(arg1);
	release_buffer(arg2);
}


ACMD(do_repiss) {
	char	*arg1 = get_buffer(MAX_INPUT_LENGTH),
			*arg2 = get_buffer(MAX_INPUT_LENGTH);
	CharData *vic = 0;
	CharData *mob  = 0;
 
	two_arguments(argument, arg1, arg2);

	if (!*arg1 || !*arg2)
		send_to_char("Usage: repiss <victim> <mobile>\r\n", ch);
	else if (((vic = get_char_vis(ch, arg1))) /* && !IS_NPC(vic) */) {
		if (((mob = get_char_vis(ch, arg2))) && (IS_NPC(mob))) {
			ch->Send("%s now hates %s.\r\n", mob->RealName(), vic->RealName());
			remember(mob, vic);
		} else
			send_to_char("Sorry, Hunter Not Found!\r\n", ch);
	} else
		send_to_char("Sorry, Victim Not Found!\r\n", ch);
	release_buffer(arg1);
	release_buffer(arg2);
}


ACMD(do_hunt) {
	char	*arg1 = get_buffer(MAX_INPUT_LENGTH),
			*arg2 = get_buffer(MAX_INPUT_LENGTH);
	CharData *vic = NULL;
	CharData *mob  = NULL;
 
	two_arguments(argument, arg1, arg2);

	if (!*arg1 || !*arg2)
		send_to_char("Usage: hunt <victim> <hunter>\r\n", ch);
	else if ((vic = get_char_vis(ch, arg1))) {
		if ((mob = get_char_vis(ch, arg2)) && IS_NPC(mob)) {
			mob->path = Path2Char(IN_ROOM(mob), vic, 200, HUNT_GLOBAL | HUNT_THRU_DOORS);
			if (mob->path)	ch->Send("%s is now hunted by %s.\r\n", vic->RealName(), mob->RealName());
			else			ch->Send("%s can't reach %s.\r\n", mob->RealName(), vic->RealName());
		} else				send_to_char("Hunter not found.\r\n", ch);
	} else					send_to_char("Victim not found.\r\n", ch);
	release_buffer(arg1);
	release_buffer(arg2);
}


void send_to_imms(char *msg) {
	DescriptorData *pt;

	START_ITER(iter, pt, DescriptorData *, descriptor_list) {
		if ((STATE(pt) == CON_PLAYING) && pt->character && IS_STAFF(pt->character))
			send_to_char(msg, pt->character);
	} END_ITER(iter);
}


ACMD(do_vwear) {
	char *arg = get_buffer(MAX_INPUT_LENGTH);

	one_argument(argument, arg);

	if (!*arg) send_to_char(	"Usage: vwear <wear position>\r\n"
								"Possible positions are:\r\n"
								"finger    neck    body    head    legs    feet    hands\r\n"
								"shield    arms    about   waist   wrist   wield   hold   eyes\r\n", ch);
	else if (is_abbrev(arg, "finger"))	vwear_object(ITEM_WEAR_FINGER, ch);
	else if (is_abbrev(arg, "neck"))		vwear_object(ITEM_WEAR_NECK, ch);
	else if (is_abbrev(arg, "body"))		vwear_object(ITEM_WEAR_BODY, ch);
	else if (is_abbrev(arg, "head"))		vwear_object(ITEM_WEAR_HEAD, ch);
	else if (is_abbrev(arg, "legs"))		vwear_object(ITEM_WEAR_LEGS, ch);
	else if (is_abbrev(arg, "feet"))		vwear_object(ITEM_WEAR_FEET, ch);
	else if (is_abbrev(arg, "hands"))		vwear_object(ITEM_WEAR_HANDS, ch);
	else if (is_abbrev(arg, "arms"))		vwear_object(ITEM_WEAR_ARMS, ch);
	else if (is_abbrev(arg, "shield"))		vwear_object(ITEM_WEAR_SHIELD, ch);
	else if (is_abbrev(arg, "about body"))	vwear_object(ITEM_WEAR_ABOUT, ch);
	else if (is_abbrev(arg, "waist"))		vwear_object(ITEM_WEAR_WAIST, ch);
	else if (is_abbrev(arg, "wrist"))		vwear_object(ITEM_WEAR_WRIST, ch);
	else if (is_abbrev(arg, "wield"))		vwear_object(ITEM_WEAR_WIELD, ch);
	else if (is_abbrev(arg, "hold"))		vwear_object(ITEM_WEAR_HOLD, ch);
	else if (is_abbrev(arg, "eyes"))		vwear_object(ITEM_WEAR_EYES, ch);
	else send_to_char(	"Possible positions are:\r\n"
						"finger    neck    body    head    legs    feet    hands\r\n"
						"shield    arms    about   waist   wrist   wield   hold   eyes\r\n", ch);
	release_buffer(arg);
}


extern int mother_desc, port;
//extern FILE *player_fl;
void Crash_rentsave(CharData * ch);

#define EXE_FILE "bin/circle" /* maybe use argv[0] but it's not reliable */

/* (c) 1996-97 Erwin S. Andreasen <erwin@pip.dknet.dk> */
ACMD(do_copyover) {
	FILE *fp;
	DescriptorData *d; //, *d_next;
	char buf[256], buf2[64];

	fp = fopen (COPYOVER_FILE, "w");

	if (!fp) {
		send_to_char ("Copyover file not writeable, aborted.\r\n",ch);
		return;
	}

	/* Consider changing all saved areas here, if you use OLC */
	sprintf (buf, "\r\n *** COPYOVER by %s - please remain seated!\r\n", ch->RealName());
	log("COPYOVER by %s", ch->RealName());
	
	/* For each playing descriptor, save its state */
	LListIterator<DescriptorData *>	iter(descriptor_list);
	while ((d = iter.Next())) {
		CharData * och = d->Original();

		if (!och || (STATE(d) != CON_PLAYING)) { /* drop those logging on */
			write_to_descriptor (d->descriptor, "\r\nSorry, we are rebooting. Come back in a few minutes.\r\n");
			close_socket (d); /* throw'em out */
		} else {
			fprintf (fp, "%d %s %s\n", d->descriptor, och->RealName(), d->host);

			/* save och */
			Crash_rentsave(och);
			och->save(IN_ROOM(och));
			write_to_descriptor (d->descriptor, buf);
		}
	}

	fprintf (fp, "-1\n");
	fclose (fp);

	imc_shutdown();
	
	if (no_external_procs)
		delete Ident;		//	We don't control Ident here, unless no external procs
	
	/* Close reserve and other always-open files and release other resources */

	/* exec - descriptors are inherited */

	sprintf (buf, "%d", port);
	sprintf (buf2, "-C%d", mother_desc);

	/* Ugh, seems it is expected we are 1 step above lib - this may be dangerous! */
	chdir ("..");

	exit_buffers();
	
#ifndef CIRCLE_MAC
	execl (EXE_FILE, "circle", buf2, buf, NULL);
#endif
	/* Failed - sucessful exec will not return */

	perror ("do_copyover: execl");
	send_to_char ("Copyover FAILED!\r\n",ch);
	
	exit (1); /* too much trouble to try to recover! */
}



ACMD(do_tedit) {
	int l, i;
	extern char *credits;
	extern char *news;
	extern char *motd;
	extern char *imotd;
	extern char *help;
	extern char *info;
	extern char *background;
	extern char *handbook;
	extern char *policies;
	char	*field, *buf;
			
	struct editor_struct {
		char *cmd;
		char level;
		char **buffer;
		int  size;
		char *filename;
	} fields[] = {
		/* edit the lvls to your own needs */
		{ "credits",	LVL_CODER,		&credits,	2400,	CREDITS_FILE},
		{ "news",		LVL_STAFF,		&news,		8192,	NEWS_FILE},
		{ "motd",		LVL_STAFF,		&motd,		2400,	MOTD_FILE},
		{ "imotd",		LVL_STAFF,		&imotd,		2400,	IMOTD_FILE},
		{ "help",       LVL_STAFF,		&help,		2400,	HELP_PAGE_FILE},
		{ "info",		LVL_ADMIN,		&info,		8192,	INFO_FILE},
		{ "background",	LVL_ADMIN,		&background,8192,	BACKGROUND_FILE},
		{ "handbook",   LVL_CODER,		&handbook,	8192,   HANDBOOK_FILE},
		{ "policies",	LVL_STAFF,		&policies,	8192,	POLICIES_FILE},
		{ "file",		LVL_STAFF,		NULL,		8192,	NULL},
		{ "\n",			0,				NULL,		0,		NULL }
	};

	if (ch->desc == NULL) {
		send_to_char("Get outta here you linkdead head!\r\n", ch);
		return;
	}

//	if (GET_LEVEL(ch) < LVL_STAFF) {
//		send_to_char("You do not have text editor permissions.\r\n", ch);
//		return;
//	}
	
   	field = get_buffer(MAX_INPUT_LENGTH);
	buf = get_buffer(MAX_INPUT_LENGTH);

	half_chop(argument, field, buf);

	if (!*field) {
		strcpy(buf, "Files available to be edited:\r\n");
		i = 1;
		for (l = 0; *fields[l].cmd != '\n'; l++) {
			if (GET_LEVEL(ch) >= fields[l].level) {
				sprintf(buf + strlen(buf), "%-11.11s", fields[l].cmd);
				if (!(i % 7)) strcat(buf, "\r\n");
				i++;
			}
		}
		if (--i % 7) strcat(buf, "\r\n");
		if (i == 0) strcat(buf, "None.\r\n");
		send_to_char(buf, ch);
		release_buffer(field);
		release_buffer(buf);
		return;
	}
	for (l = 0; *(fields[l].cmd) != '\n'; l++)
		if (!strncmp(field, fields[l].cmd, strlen(field)))
			break;

	if (*fields[l].cmd == '\n') {
		send_to_char("Invalid text editor option.\r\n", ch);
		release_buffer(field);
		release_buffer(buf);
		return;
	}

	if (GET_LEVEL(ch) < fields[l].level) {
		send_to_char("You are not godly enough for that!\r\n", ch);
		release_buffer(field);
		release_buffer(buf);
		return;
	}

	switch (l) {
		case 0: ch->desc->str = &credits; break;
		case 1: ch->desc->str = &news; break;
		case 2: ch->desc->str = &motd; break;
		case 3: ch->desc->str = &imotd; break;
		case 4: ch->desc->str = &help; break;
		case 5: ch->desc->str = &info; break;
		case 6: ch->desc->str = &background; break;
		case 7: ch->desc->str = &handbook; break;
		case 8: ch->desc->str = &policies; break;
		case 9:	//	break;
		default:
			send_to_char("Invalid text editor option.\r\n", ch);
			release_buffer(field);
			release_buffer(buf);
			return;
	}
   
	/* set up editor stats */
	send_to_char("\x1B[H\x1B[J", ch);
	send_to_char("Edit file below: (/s saves /h for help)\r\n", ch);
	ch->desc->backstr = NULL;
	if (fields[l].buffer && *(fields[l].buffer)) {
		send_to_char(*(fields[l].buffer), ch);
		ch->desc->backstr = str_dup(*(fields[l].buffer));
	}
	ch->desc->max_str = fields[l].size;
	ch->desc->mail_to = 0;
	ch->desc->storage = str_dup(fields[l].filename);
	act("$n begins using a PDA.", TRUE, ch, 0, 0, TO_ROOM);
	SET_BIT(PLR_FLAGS(ch), PLR_WRITING);
	STATE(ch->desc) = CON_TEXTED;
	release_buffer(field);
	release_buffer(buf);
}


ACMD(do_reward) {
	CharData * victim;
	int amount;
	char	*arg1 = get_buffer(MAX_INPUT_LENGTH),
			*arg2 = get_buffer(MAX_INPUT_LENGTH);
	
	half_chop(argument, arg1, arg2);
	
	if (!*arg1 || !*arg2 || (amount = atoi(arg2)) == 0)
		send_to_char("Usage: reward <player> <amount>\r\n", ch);
	else if (!(victim = get_char_vis(ch, arg1)))
		send_to_char(NOPERSON, ch);
	else if (IS_NPC(victim))
		send_to_char("Not on NPCs, you dolt!\r\n", ch);
	else {
		if ((amount < 0) && (amount < (0-GET_MISSION_POINTS(victim)))) {
			amount = 0 - (GET_MISSION_POINTS(victim));
			ch->Send("%s only has %d MP... taking all of them away.\r\n", victim->RealName(), GET_MISSION_POINTS(victim));
		}
		
		if (amount == 0)
			send_to_char("Zero MP, eh?", ch);
		else {			
			GET_MISSION_POINTS(victim) += amount;
			
			ch->Send("You %s %d MP %s %s.\r\n",
					(amount > 0) ? "give" : "take away",
					abs(amount),
					(amount > 0) ? "to" : "from",
					victim->RealName());
			
			victim->Send("%s %s %d MP %s you.\r\n",
					PERS(ch, victim),
					(amount > 0) ? "gives" : "takes away",
					abs(amount),
					(amount > 0) ? "to" : "from");
			
			victim->save(NOWHERE);
		}
	}
	release_buffer(arg1);
	release_buffer(arg2);
}

/*
Usage: string <type> <name> <field> [<string> | <keyword>]

For changing the text-strings associated with objects and characters.  The
format is:

Type is either 'obj' or 'char'.
Name                  (the call-name of an obj/char - kill giant)
Short                 (for inventory lists (obj's) and actions (char's))
Long                  (for when obj/character is seen in room)
Title                 (for players)
Description           (For look at.  For obj's, must be followed by a keyword)
Delete-description    (only for obj's. Must be followed by keyword)
*/
ACMD(do_string) {
	char *type, *name, *field, *string;
	CharData *vict = NULL;
	ObjData *obj = NULL;
	
	type = get_buffer(MAX_INPUT_LENGTH);
	name = get_buffer(MAX_INPUT_LENGTH);
	field = get_buffer(MAX_INPUT_LENGTH);
	string = get_buffer(MAX_INPUT_LENGTH);
	
	argument = two_arguments(argument, type, name);
	half_chop(argument, field, string);
	
	if (!*type || !*name || !*field || !*string) {
		send_to_char("Usage: string [character|object] <name> <field> [<string> | <keyword>]\r\n", ch);
	} else {
		if (is_abbrev(type, "character"))	vict = get_char_vis(ch, name);
		else if (is_abbrev(type, "object"))	obj = get_obj_vis(ch, name);
			
		if (!vict && !obj)
			send_to_char("Usage: string [character|object] <name> <field> [<string> | <keyword>]\r\n", ch);
		else {
			if (is_abbrev(field, "name")) {
				if (vict) {
					if (IS_NPC(vict)) {
						ch->Send("%s's name restrung to '%s'.\r\n", vict->RealName(), string);
						SSFree(vict->general.name);
						vict->general.name = SSCreate(string);
					} else
						send_to_char("You can't re-string player's names.\r\n", ch);
				} else if (obj) {
					ch->Send("%s's name restrung to '%s'.\r\n", SSData(obj->shortDesc), string);
					SSFree(obj->name);
					obj->name = SSCreate(string);
				}
			} else if (is_abbrev(field, "short")) {
				if (vict) {
					if (IS_NPC(vict)) {
						ch->Send("%s's short desc restrung to '%s'.\r\n", vict->RealName(), string);
						SSFree(vict->general.shortDesc);
						vict->general.shortDesc = SSCreate(string);
					} else send_to_char("Players don't have short descs.\r\n", ch);
				} else if (obj) {
					ch->Send("%s's short desc restrung to '%s'.\r\n", SSData(obj->shortDesc), string);
					SSFree(obj->shortDesc);
					obj->shortDesc = SSCreate(string);
				}
			} else if (is_abbrev(field, "long")) {
				strcat(string, "\r\n");
				if (vict) {
					if (IS_NPC(vict)) {
						ch->Send("%s's long desc restrung to '%s'.\r\n", vict->RealName(), string);
						SSFree(vict->general.longDesc);
						vict->general.longDesc = SSCreate(string);
					} else send_to_char("Players don't have long descs.\r\n", ch);
				} else if (obj) {
					ch->Send("%s's long desc restrung to '%s'.\r\n", SSData(obj->shortDesc), string);
					SSFree(obj->description);
					obj->description = SSCreate(string);
				}
			} else if (is_abbrev(field, "title")) {
				if (vict) {
					if (!IS_NPC(vict)) {
						if ((GET_LEVEL(vict) < LVL_STAFF) || (GET_LEVEL(ch) == LVL_CODER)) {
							ch->Send("%s's title restrung to '%s'.\r\n", vict->RealName(), string);
							SSFree(vict->general.title);
							vict->general.title = SSCreate(string);
						} else
							send_to_char("You can't restring the title of staff members!", ch);
					} else send_to_char("Mobs don't have titles.\r\n", ch);
				} else if (obj) send_to_char("Objs don't have titles.\r\n", ch);
			} else if (is_abbrev(field, "description")) {
				send_to_char("Feature not implemented yet.\r\n", ch);
			} else if (is_abbrev(field, "delete-description")) {
				if (vict) send_to_char("Only for objects.\r\n", ch);
				else if (obj) {
					ExtraDesc *extra, *temp;
					
					for (extra = obj->exDesc; extra; extra = extra->next)
						if (!str_cmp(SSData(extra->keyword), string))
							break;
					if (extra) {
						ch->Send("%s's extra description '%s' deleted.\r\n", SSData(obj->shortDesc), SSData(extra->keyword));
						REMOVE_FROM_LIST(extra, obj->exDesc, next);
						delete extra;
					} else
						send_to_char("Extra description not found.\r\n", ch);
				}
			} else
				send_to_char("No such field.\r\n", ch);
		}
	}
	release_buffer(type);
	release_buffer(name);
	release_buffer(field);
	release_buffer(string);
}


struct {
	char *	field;
	Flags	permission;
} StaffPermissions[] = {
	{ "admin",		STAFF_ADMIN		},
	{ "characters",	STAFF_CHAR		},
	{ "clans",		STAFF_CLANS		},
	{ "game",		STAFF_GAME		},
	{ "general",	STAFF_GEN		},
	{ "help",		STAFF_HELP		},
	{ "houses",		STAFF_HOUSES	},
	{ "olc",		STAFF_OLC		},
	{ "olcadmin",	STAFF_OLCADMIN	},
	{ "script",		STAFF_SCRIPT	},
	{ "security",	STAFF_SECURITY	},
	{ "shops",		STAFF_SHOPS		},
	{ "socials",	STAFF_SOCIALS	},
	{ "imc",		STAFF_IMC		},
	{ "\n", 0 }
};

ACMD(do_allow) {
	char *arg1, *arg2;
	SInt32	i, l, the_cmd, cmd_num;
	CharData *vict;
	
	if (IS_NPC(ch)) {
		send_to_char("Nope.\r\n", ch);
		return;
	}
	
	arg1 = get_buffer(MAX_INPUT_LENGTH);
	arg2 = get_buffer(MAX_INPUT_LENGTH);
	
	two_arguments(argument, arg1, arg2);
	
	if (!*arg1 || !*arg2 || !STF_FLAGGED(ch, STAFF_ADMIN)) {
		ch->Send("Usage: %s <name> <permission>\r\n"
				 "Permissions available:\r\n",
				subcmd == SCMD_ALLOW ? "allow" : "deny");
		for (l = 0; *StaffPermissions[l].field != '\n'; l++) {
			i = 1;
			ch->Send("%-9.9s: ", StaffPermissions[l].field);
			for (cmd_num = 0; cmd_num < num_of_cmds; cmd_num++) {
				the_cmd = cmd_sort_info[cmd_num].sort_pos;
				if (IS_SET(complete_cmd_info[the_cmd].staffcmd, StaffPermissions[l].permission)) {
					if (((i % 6) == 1) && (i != 1)) send_to_char("           ", ch);
					ch->Send("%-11.11s", complete_cmd_info[the_cmd].command);
					if (!(i % 6)) send_to_char("\r\n", ch);
					i++;
				}
			}
			if (--i % 6) send_to_char("\r\n", ch);
			if (i == 0) send_to_char("None.\r\n", ch);
		}
	} else if (!(vict = get_player_vis(ch, arg1, false))) {
		send_to_char("Player not found.", ch);
	} else {
		for (i = 0; *StaffPermissions[i].field != '\n'; i++)
			if (is_abbrev(arg2, StaffPermissions[i].field))
				break;
		if (*StaffPermissions[i].field == '\n')
			send_to_char("Invalid choice.\r\n", ch);
		else {
			if (subcmd == SCMD_ALLOW)	SET_BIT(STF_FLAGS(vict), StaffPermissions[i].permission);
			else						REMOVE_BIT(STF_FLAGS(vict), StaffPermissions[i].permission);
			
			mudlogf(BRF, LVL_STAFF, TRUE, "(GC) %s %s %s access to %s commands.", ch->RealName(),
					subcmd == SCMD_ALLOW ? "allowed" : "denied", vict->RealName(), StaffPermissions[i].field);
			
			ch->Send("You %s %s access to %s commands.\r\n", subcmd == SCMD_ALLOW ? "allowed" : "denied",
					vict->RealName(), StaffPermissions[i].field);
			vict->Send("%s %s you access to %s commands.\r\n", ch->RealName(),
					subcmd == SCMD_ALLOW ? "allowed" : "denied", StaffPermissions[i].field);
		}
	}
	release_buffer(arg1);
	release_buffer(arg2);
}



ACMD(do_wizcall);
ACMD(do_wizassist);


LList<SInt32> wizcallers;

ACMD(do_wizcall) {
	if (wizcallers.InList(GET_ID(ch))) {
		wizcallers.Remove(GET_ID(ch));
		send_to_char("You remove yourself from the help queue.\r\n", ch);
		mudlogf(BRF, LVL_STAFF, FALSE, "ASSIST: %s no longer needs assistance.", ch->RealName());
	} else {
		wizcallers.Add(GET_ID(ch));
		send_to_char("A staff member will be with you as soon as possible.\r\n", ch);
		mudlogf(BRF, LVL_STAFF, FALSE, "ASSIST: %s calls for assistance.", ch->RealName());
	}
}


ACMD(do_wizassist) {
	LListIterator<SInt32>	iter(wizcallers);
	char *	arg;
	SInt32	which, i;
	CharData *who = NULL;
	
	arg = get_buffer(MAX_INPUT_LENGTH);
	one_argument(argument, arg);
	
	if (!wizcallers.Count()) {
		send_to_char("Nobody needs assistance right now.\r\n", ch);
	} else if (!*arg || !(which = atoi(arg))) {
		which = 0;
		send_to_char("Players who need assistance\r\n"
					 "---------------------------\r\n", ch);
		while ((i = iter.Next(0))) {
			if ((who = find_char(i)) && (IN_ROOM(who) != NOWHERE))
				ch->Send("%2d. %s\r\n", ++which, who->RealName());
			else
				wizcallers.Remove(i);
		}
		
		if (!which)
			send_to_char("Nobody needs assistance right now.\r\n", ch);
	} else if ((which < 0) || (which > wizcallers.Count()))
		send_to_char("That number doesn't exist in the help queue.\r\n", ch);
	else {
		while ((i = iter.Next(0))) {
			if ((who = find_char(i)) && (IN_ROOM(who) != NOWHERE))
				which--;
			else
				wizcallers.Remove(i);
			if (!which)
				break;
		}
		
		if (i && who && (ch != who)) {
			mudlogf(BRF, LVL_STAFF, FALSE, "ASSIST: %s goes to assist %s.", ch->RealName(),
					who->RealName());
			ch->Send("You go to assist %s.\r\n", who->RealName());
			ch->from_room();
			ch->to_room(IN_ROOM(who));
			act("$n appears in the room, to assist $N.", TRUE, ch, 0, who, TO_NOTVICT);
			act("$n appears in the room, to assist you.", TRUE, ch, 0, who, TO_VICT);
			
			wizcallers.Remove(i);
		} else if (ch == who)
			send_to_char("You can't help yourself.  You're helpless!\r\n", ch);
		else
			send_to_char("That number doesn't exist in the help queue. [ERROR - Report]\r\n", ch);
	}
	release_buffer(arg);
}
