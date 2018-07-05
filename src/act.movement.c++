/* ************************************************************************
*   File: act.movement.c                                Part of CircleMUD *
*  Usage: movement commands, door handling, & sleep/rest/etc state        *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */



#include "structs.h"
#include "utils.h"
#include "buffer.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "find.h"
#include "db.h"
#include "spells.h"
#include "house.h"
#include "event.h"

/* external vars  */
extern int rev_dir[];
extern char *dirs[];
extern int movement_loss[];
extern char *from_dir[];

/* external functs */
int special(CharData *ch, char *cmd, char *arg);
void death_cry(CharData *ch);
int find_eq_pos(CharData * ch, ObjData * obj, char *arg);
void add_follower(CharData *ch, CharData *leader);
int greet_mtrigger(CharData *actor, int dir);
int entry_mtrigger(CharData *ch);
int enter_wtrigger(RoomData *room, CharData *actor, int dir);
int greet_otrigger(CharData *actor, int dir);
int sit_otrigger(ObjData *obj, CharData *actor);
int leave_mtrigger(CharData *actor, int dir);
int leave_otrigger(CharData *actor, int dir);
int leave_wtrigger(RoomData *room, CharData *actor, int dir);
int door_mtrigger(CharData *actor, int subcmd, int dir);
int door_otrigger(CharData *actor, int subcmd, int dir);
int door_wtrigger(CharData *actor, int subcmd, int dir);

ACMD(do_mount);
EVENTFUNC(gravity);

void AlterMove(CharData *ch, SInt32 amount);

/* local functions */
int has_boat(CharData *ch);
int find_door(CharData *ch, char *type, char *dir, char *cmdname);
bool has_key(CharData *ch, int key);
void do_doorcmd(CharData *ch, ObjData *obj, int door, int scmd);
int ok_pick(CharData *ch, int keynum, int pickproof, int scmd);
int do_enter_vehicle(CharData * ch, char * buf);
ACMD(do_move);
ACMD(do_gen_door);
ACMD(do_enter);
ACMD(do_leave);
ACMD(do_stand);
ACMD(do_sit);
ACMD(do_rest);
ACMD(do_sleep);
ACMD(do_wake);
ACMD(do_follow);
ACMD(do_push_drag);
ACMD(do_recall);
ACMD(do_push_drag_out);		// Expands do_push_drag

void handle_motion_leaving(CharData *ch, int direction);
void handle_motion_entering(CharData *ch, int direction);

void mprog_greet_trigger(CharData * ch);
void mprog_entry_trigger(CharData * mob);
int mprog_exit_trigger(CharData *ch, int dir);

/* simple function to determine if char can walk on water */
int has_boat(CharData *ch)
{
  ObjData *obj;
  int i;
/*
  if (ROOM_IDENTITY(IN_ROOM(ch)) == DEAD_SEA)
    return 1;
*/
  if (AFF_FLAGGED(ch, AFF_WATERWALK) || NO_STAFF_HASSLE(ch))
    return 1;

  /* non-wearable boats in inventory will do it */
  START_ITER(iter, obj, ObjData *, ch->carrying) {
    if (GET_OBJ_TYPE(obj) == ITEM_BOAT && (find_eq_pos(ch, obj, NULL) < 0)) {
      END_ITER(iter);
      return 1;
    }
  } END_ITER(iter);

  /* and any boat you're wearing will do it too */
  for (i = 0; i < NUM_WEARS; i++)
    if (GET_EQ(ch, i) && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_BOAT)
      return 1;

  return 0;
}

  

/* do_simple_move assumes
 *    1. That there is no master and no followers.
 *    2. That the direction exists.
 *
 *   Returns :
 *   1 : If succes.
 *   0 : If fail
 */
 
extern SInt32 base_response[NUM_RACES+1];
int do_simple_move(CharData *ch, int dir, int need_specials_check) {
	int was_in, need_movement;
	was_in = IN_ROOM(ch);

	if (!MOB_FLAGGED(ch, MOB_PROGMOB)) {

//		if (!greet_mtrigger(ch))
//			return 0;
		/*
		 * Check for special routines (North is 1 in command list, but 0 here) Note
		 * -- only check if following; this avoids 'double spec-proc' bug
		 */
		if (need_specials_check && special(ch, complete_cmd_info[dir + 1].command, ""))
			return 0;
		if (!leave_mtrigger(ch, dir))
			return 0;
		if (!leave_otrigger(ch, dir))
			return 0;
		if (!leave_wtrigger(&world[ch->in_room], ch, dir))
			return 0;

		/* charmed? */
		if (AFF_FLAGGED(ch, AFF_CHARM) && ch->master && IN_ROOM(ch) == IN_ROOM(ch->master)) {
			send_to_char("The thought of leaving your master makes you weep.\r\n", ch);
			act("$n bursts into tears.", FALSE, ch, 0, 0, TO_ROOM);
			return 0;
		}

		if (mprog_exit_trigger(ch, dir))
			return 0;
		
		if (IS_NPC(ch) && EXIT_FLAGGED(EXIT(ch, dir), EX_NOMOB))
			return 0;
		
		/* if this room or the one we're going to is air, check for fly */
		if (/* (SECT(IN_ROOM(ch)) == SECT_FLYING) || */ (SECT(EXIT(ch, dir)->to_room) == SECT_FLYING)) {
			if (!AFF_FLAGGED(ch, AFF_FLYING) && !NO_STAFF_HASSLE(ch)) {
				send_to_char("You would have to fly to go there.\r\n", ch);
				return 0;
			}
		}

		/* if this room or the one we're going to needs a boat, check for one */
		if ((SECT(IN_ROOM(ch)) == SECT_WATER_NOSWIM) ||
				(SECT(EXIT(ch, dir)->to_room) == SECT_WATER_NOSWIM)) {
			if (!has_boat(ch)) {
				send_to_char("You need a boat to go there.\r\n", ch);
				return 0;
			}
		}

		/* move points needed is avg. move loss for src and destination sect type */
		need_movement = (movement_loss[SECT(IN_ROOM(ch))] +
				movement_loss[SECT(EXIT(ch, dir)->to_room)]) / 2;

		if ((GET_MOVE(ch) < need_movement) && !IS_NPC(ch) && !NO_STAFF_HASSLE(ch)) {
			if (need_specials_check && ch->master)
				send_to_char("You are too exhausted to follow.\r\n", ch);
			else
				send_to_char("You are too exhausted.\r\n", ch);

			return 0;
		}
		
		if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_ATRIUM)) {
			if (!House_can_enter(ch, world[EXIT(ch, dir)->to_room].number)) {
				send_to_char("That's private property -- no trespassing!\r\n", ch);
				return 0;
			}
		}
		
		if (ROOM_FLAGGED(EXIT(ch, dir)->to_room, ROOM_TUNNEL) &&
				world[EXIT(ch, dir)->to_room].people.Count() > 2) {
			send_to_char("There isn't enough room there for more than one person!\r\n", ch);
			return 0;
		}
		
		if (!NO_STAFF_HASSLE(ch) && !IS_NPC(ch))
			AlterMove(ch, need_movement);

		if (!enter_wtrigger(&world[EXIT(ch, dir)->to_room], ch, dir) || PURGED(ch))
			return 0;
		
		if (IN_ROOM(ch) != was_in)
			return 1;	//	Trick, to make FOLLOW work

		if (!AFF_FLAGGED(ch, AFF_SNEAK)) {
			act("$n leaves $T.", TRUE, ch, 0, dirs[dir], TO_ROOM);
		}

		handle_motion_leaving(ch, dir);
	}
	
	ch->from_room();
	ch->to_room(world[was_in].dir_option[dir]->to_room);
	if (!NO_STAFF_HASSLE(ch))
		WAIT_STATE(ch, MAX(5, ((base_response[GET_RACE(ch)] RL_SEC) / 10) - (GET_AGI(ch) / 10) + 5));
	//	(8 to 12) - (0 to 10) + 5
	//	.3 - 1.7 sec
	
	if (!MOB_FLAGGED(ch, MOB_PROGMOB)) {
		handle_motion_entering(ch, dir);
	  
		if (!AFF_FLAGGED(ch, AFF_SNEAK))
			act("$n has arrived from $T.", TRUE, ch, 0, from_dir[dir], TO_ROOM);
//		else
//			REMOVE_BIT(AFF_FLAGS(ch), AFF_SNEAK);
	  
		if (ch->desc != NULL)
			look_at_room(ch, 0);

		if (!entry_mtrigger(ch))
			return 0;
		
		greet_mtrigger(ch, dir);
		greet_otrigger(ch, dir);

		mprog_entry_trigger(ch);
		mprog_greet_trigger(ch);
	}
	return 1;
}


int perform_move(CharData *ch, int dir, int need_specials_check) {
	RNum		was_in;
	CharData *	follower;

	if (ch == NULL || dir < 0 || dir >= NUM_OF_DIRS || PURGED(ch) || FIGHTING(ch))
		return 0;
	else if (!EXIT(ch, dir) || EXIT(ch, dir)->to_room == NOWHERE)
		send_to_char("Alas, you cannot go that way...\r\n", ch);
	else if (EXIT_FLAGGED(EXIT(ch, dir), EX_CLOSED)) {
		if (EXIT(ch, dir)->GetKeyword())
			act("The $T seems to be closed.", FALSE, ch, 0, fname(EXIT(ch, dir)->GetKeyword()), TO_CHAR);
		else
			send_to_char("It seems to be closed.\r\n", ch);
	} else if (EXIT_FLAGGED(EXIT(ch, dir), EX_NOMOVE)) {
		if (EXIT(ch, dir)->GetKeyword())
			act("The $T won't let you through.", FALSE, ch, 0, fname(EXIT(ch, dir)->GetKeyword()), TO_CHAR);
		else
			send_to_char("It won't let you through.\r\n", ch);
	} else if (IS_NPC(ch) && EXIT_FLAGGED(EXIT(ch, dir), EX_NOMOB))
		send_to_char("Alas, you cannot go that way...\r\n", ch);
	else {
		if (!ch->followers.Count())
			return (do_simple_move(ch, dir, need_specials_check));

		was_in = IN_ROOM(ch);
		if (!do_simple_move(ch, dir, need_specials_check))
			return 0;

		START_ITER(iter, follower, CharData *, ch->followers) {
			if ((IN_ROOM(follower) == was_in) && (GET_POS(follower) >= POS_STANDING)) {
				act("You follow $N.\r\n", FALSE, follower, 0, ch, TO_CHAR);
				perform_move(follower, dir, 1);
			}
		} END_ITER(iter);
		return 1;
	}
	return 0;
}


ACMD(do_move) {
	/*
	* This is basically a mapping of cmd numbers to perform_move indices.
	* It cannot be done in perform_move because perform_move is called
	* by other functions which do not require the remapping.
	*/
//	if (event_pending(ch, gravity, EVENT_CAUSER))
//		send_to_char("You are falling, and can't grab anything!\r\n", ch);
//	else
//		perform_move(ch, subcmd - 1, 0);
	if (FindEvent(ch->events, gravity))
		send_to_char("You are falling, and can't grab anything!\r\n", ch);
	else
		perform_move(ch, subcmd - 1, 0);
}


int find_door(CharData *ch, char *type, char *dir, char *cmdname) {
  int door;

	if (*dir) {		//	a direction was specified
		if ((door = search_block(dir, dirs, FALSE)) == -1)	//	Partial Match
			send_to_char("That's not a direction.\r\n", ch);
		else if (EXIT(ch, door)) {
			if (EXIT(ch, door)->GetKeyword()) {
				if (isname(type, EXIT(ch, door)->GetKeyword()))
					return door;
				else
					ch->Send("I see no %s there.", type);
			} else
				return door;
		} else	send_to_char("I really don't see how you can close anything there.\r\n", ch);
	} else {		//	try to locate the keyword
		if (!*type)	ch->Send("What is it you want to %s?\r\n", cmdname);
		else {
			for (door = 0; door < NUM_OF_DIRS; door++)
				if (EXIT(ch, door) && EXIT(ch, door)->GetKeyword() &&
						isname(type, EXIT(ch, door)->GetKeyword()))
					return door;

//			if (((door = search_block(dir, dirs, FALSE)) != -1) && EXIT(ch, door)) {	//	Partial Match
//				return door;
//				send_to_char("There is no exit in that direction.\r\n", ch);
//			} else
			if (((door = search_block(type, dirs, FALSE)) != -1) && EXIT(ch, door))
				return door;
			else
				ch->Send("There doesn't seem to be %s %s here.\r\n", AN(type), type);
		}
	}
	return -1;
}


bool has_key(CharData *ch, int key) {
	ObjData *o;

	if (key < 0)
		return false;

	START_ITER(iter, o, ObjData *, ch->carrying) {
		if (GET_OBJ_VNUM(o) == key) {
			END_ITER(iter);
			return true;
		}
	} END_ITER(iter);

	if (GET_EQ(ch, WEAR_HAND_R) && (GET_OBJ_VNUM(GET_EQ(ch, WEAR_HAND_R)) == key))
		return true;
	if (GET_EQ(ch, WEAR_HAND_L) && (GET_OBJ_VNUM(GET_EQ(ch, WEAR_HAND_L)) == key))
		return true;

	return false;
}



#define NEED_OPEN	1
#define NEED_CLOSED	2
#define NEED_UNLOCKED	4
#define NEED_LOCKED	8

char *cmd_door[] =
{
  "open",
  "close",
  "unlock",
  "lock",
  "pick"
};


const int flags_door[] =
{
  NEED_CLOSED | NEED_UNLOCKED,
  NEED_OPEN,
  NEED_CLOSED | NEED_LOCKED,
  NEED_CLOSED | NEED_UNLOCKED,
  NEED_CLOSED | NEED_LOCKED
};


#define OPEN_DOOR(room, obj, door)	((obj) ?\
		(REMOVE_BIT(GET_OBJ_VAL(obj, 1), CONT_CLOSED)) :\
		(REMOVE_BIT(EXITN(room, door)->exit_info, EX_CLOSED)))
#define CLOSE_DOOR(room, obj, door)	((obj) ?\
		(SET_BIT(GET_OBJ_VAL(obj, 1), CONT_CLOSED)) :\
		(SET_BIT(EXITN(room, door)->exit_info, EX_CLOSED)))
#define LOCK_DOOR(room, obj, door)	((obj) ?\
		(SET_BIT(GET_OBJ_VAL(obj, 1), CONT_LOCKED)) :\
		(SET_BIT(EXITN(room, door)->exit_info, EX_LOCKED)))
#define UNLOCK_DOOR(room, obj, door)	((obj) ?\
		(REMOVE_BIT(GET_OBJ_VAL(obj, 1), CONT_LOCKED)) :\
		(REMOVE_BIT(EXITN(room, door)->exit_info, EX_LOCKED)))

void do_doorcmd(CharData *ch, ObjData *obj, int door, int scmd) {
	int other_room = 0;
	struct RoomDirection *back = NULL;
	char *buf;

	if (!door_mtrigger(ch, scmd, door))	return;
	if (!door_otrigger(ch, scmd, door))	return;
	if (!door_wtrigger(ch, scmd, door))	return;
	
	buf = get_buffer(MAX_STRING_LENGTH);
	sprintf(buf, "$n %ss ", cmd_door[scmd]);
	if (!obj && ((other_room = EXIT(ch, door)->to_room) != NOWHERE))
		if ((back = world[other_room].dir_option[rev_dir[door]]))
			if (back->to_room != IN_ROOM(ch))
				back = NULL;

	switch (scmd) {
		case SCMD_OPEN:
			OPEN_DOOR(IN_ROOM(ch), obj, door);
			if (back)	OPEN_DOOR(other_room, obj, rev_dir[door]);
//			send_to_char(OK, ch);
			break;
		case SCMD_CLOSE:
			CLOSE_DOOR(IN_ROOM(ch), obj, door);
			if (back)	CLOSE_DOOR(other_room, obj, rev_dir[door]);
//			send_to_char(OK, ch);
			break;
		case SCMD_UNLOCK:
			UNLOCK_DOOR(IN_ROOM(ch), obj, door);
			if (back)	UNLOCK_DOOR(other_room, obj, rev_dir[door]);
//			send_to_char("*Click*\r\n", ch);
			break;
		case SCMD_LOCK:
			LOCK_DOOR(IN_ROOM(ch), obj, door);
			if (back)	LOCK_DOOR(other_room, obj, rev_dir[door]);
//			send_to_char("*Click*\r\n", ch);
			break;
		case SCMD_PICK:
			UNLOCK_DOOR(IN_ROOM(ch), obj, door);
			if (back)	UNLOCK_DOOR(other_room, obj, rev_dir[door]);
			send_to_char("The lock quickly yields to your skills.\r\n", ch);
			strcpy(buf, "$n skillfully picks the lock on ");
			break;
	}
	if (scmd != SCMD_PICK)
		act(obj ? "You $t $P." : "You $t the $F.", FALSE, ch, (ObjData *)cmd_door[scmd],
				obj ? obj : (void *)EXIT(ch, door)->GetKeyword("door"), TO_CHAR);

	/* Notify the room */
	sprintf(buf + strlen(buf), "%s%s.", ((obj) ? "" : "the "), (obj) ? "$p" :
			(EXIT(ch, door)->GetKeyword() ? "$F" : "door"));
	if (!obj || (IN_ROOM(obj) != NOWHERE))
		act(buf, FALSE, ch, obj, obj ? 0 : const_cast<char *>(EXIT(ch, door)->GetKeyword()), TO_ROOM);

	/* Notify the other room */
	if ((scmd == SCMD_OPEN || scmd == SCMD_CLOSE) && back && world[EXIT(ch, door)->to_room].people.Count()) {
		sprintf(buf, "The %s is %s%s from the other side.\r\n",
				fname(back->GetKeyword("door")), cmd_door[scmd],
//				(back->keyword ? fname(back->keyword) : "door"), cmd_door[scmd],
				(scmd == SCMD_CLOSE) ? "d" : "ed");
		act(buf, FALSE, world[EXIT(ch, door)->to_room].people.Top(), 0, 0, TO_ROOM | TO_CHAR);
//		world[EXIT(ch, door)->to_room].Send("%s", buf);
	}
	release_buffer(buf);
}


int ok_pick(CharData *ch, int keynum, int pickproof, int scmd) {
	int percent;

	percent = Number(1, 101);

	if (scmd == SCMD_PICK) {
		if (keynum < 0)		send_to_char("Odd - you can't seem to find a keyhole.\r\n", ch);
		else if (pickproof)	send_to_char("It resists your attempts to pick it.\r\n", ch);
		else if (percent > GET_SKILL(ch, SKILL_PICK_LOCK))
			send_to_char("You failed to pick the lock.\r\n", ch);
		else				return 1;
		return 0;
	}
	return 1;
}


#define DOOR_IS_OPENABLE(ch, obj, door)	((obj) ? \
			((GET_OBJ_TYPE(obj) == ITEM_CONTAINER) && \
			(OBJVAL_FLAGGED(obj, CONT_CLOSEABLE))) :\
			(EXIT_FLAGGED(EXIT(ch, door), EX_ISDOOR)))
#define DOOR_IS_OPEN(ch, obj, door)	((obj) ? \
			(!OBJVAL_FLAGGED(obj, CONT_CLOSED)) :\
			(!EXIT_FLAGGED(EXIT(ch, door), EX_CLOSED)))
#define DOOR_IS_UNLOCKED(ch, obj, door)	((obj) ? \
			(!OBJVAL_FLAGGED(obj, CONT_LOCKED)) :\
			(!EXIT_FLAGGED(EXIT(ch, door), EX_LOCKED)))
#define DOOR_IS_PICKPROOF(ch, obj, door) ((obj) ? \
			(OBJVAL_FLAGGED(obj, CONT_PICKPROOF)) : \
			(EXIT_FLAGGED(EXIT(ch, door), EX_PICKPROOF)))

#define DOOR_IS_CLOSED(ch, obj, door)	(!(DOOR_IS_OPEN(ch, obj, door)))
#define DOOR_IS_LOCKED(ch, obj, door)	(!(DOOR_IS_UNLOCKED(ch, obj, door)))
#define DOOR_KEY(ch, obj, door)		((obj) ? (GET_OBJ_VAL(obj, 2)) : \
					(EXIT(ch, door)->key))
#define DOOR_LOCK(ch, obj, door)	((obj) ? (GET_OBJ_VAL(obj, 1)) : \
					(EXIT(ch, door)->exit_info))

ACMD(do_gen_door) {
	int door = -1, keynum;
	char *type, *dir;
	ObjData *obj = NULL;
	CharData *victim = NULL;

	skip_spaces(argument);
	if (!*argument) {
		ch->Send("%s what?\r\n", cmd_door[subcmd]);
		return;
	}
	
	type = get_buffer(MAX_INPUT_LENGTH);
	dir = get_buffer(MAX_INPUT_LENGTH);
	two_arguments(argument, type, dir);
	
	if (!generic_find(type, FIND_OBJ_INV | FIND_OBJ_ROOM, ch, &victim, &obj))
		door = find_door(ch, type, dir, cmd_door[subcmd]);

	if ((obj) || (door >= 0)) {
		keynum = DOOR_KEY(ch, obj, door);
		if (!(DOOR_IS_OPENABLE(ch, obj, door)))
			act("You can't $F that!", FALSE, ch, 0, cmd_door[subcmd], TO_CHAR);
		else if (!DOOR_IS_OPEN(ch, obj, door) && IS_SET(flags_door[subcmd], NEED_OPEN))
			send_to_char("But it's already closed!\r\n", ch);
		else if (!DOOR_IS_CLOSED(ch, obj, door) && IS_SET(flags_door[subcmd], NEED_CLOSED))
			send_to_char("But it's currently open!\r\n", ch);
		else if (!(DOOR_IS_LOCKED(ch, obj, door)) && IS_SET(flags_door[subcmd], NEED_LOCKED))
			send_to_char("Oh.. it wasn't locked, after all..\r\n", ch);
		else if (!(DOOR_IS_UNLOCKED(ch, obj, door)) && IS_SET(flags_door[subcmd], NEED_UNLOCKED))
			send_to_char("It seems to be locked.\r\n", ch);
		else if (!has_key(ch, keynum) && !IS_STAFF(ch) &&
				((subcmd == SCMD_LOCK) || (subcmd == SCMD_UNLOCK)))
			send_to_char("You don't seem to have the proper key.\r\n", ch);
		else if (ok_pick(ch, keynum, DOOR_IS_PICKPROOF(ch, obj, door), subcmd))
			do_doorcmd(ch, obj, door, subcmd);
	}
	release_buffer(type);
	release_buffer(dir);
	return;
}


int do_enter_vehicle(CharData * ch, char *buf) {
	ObjData * vehicle = get_obj_in_list_vis(ch, buf, world[IN_ROOM(ch)].contents);
	RNum	vehicle_room;
	
	if (vehicle && (GET_OBJ_TYPE(vehicle) == ITEM_VEHICLE)) {
		if (IS_MOUNTABLE(vehicle))
			act("You can ride $p, but not get into it.", TRUE, ch, vehicle, 0, TO_CHAR);
		else if ((vehicle_room = real_room(GET_OBJ_VAL(vehicle, 0))) == NOWHERE)
			send_to_char("Serious problem: Vehicle has no insides!", ch);
		else {
			act("You climb into $p.", TRUE, ch, vehicle, 0, TO_CHAR);
			act("$n climbs into $p", TRUE, ch, vehicle, 0, TO_ROOM);
			ch->from_room();
			ch->to_room(vehicle_room);
			act("$n climbs in.", TRUE, ch, 0, 0, TO_ROOM);
			if (ch->desc != NULL)
				look_at_room(ch, 0);
		}
		return 1;
	}
	return 0;
}


ACMD(do_enter) {
	int door;
	char *buf = get_buffer(MAX_INPUT_LENGTH);
	
	one_argument(argument, buf);

	if (*buf) {			/* an argument was supplied, search for door keyword */
		for (door = 0; door < NUM_OF_DIRS; door++)
			if (EXIT(ch, door) && EXIT(ch, door)->GetKeyword())
				if (!str_cmp(EXIT(ch, door)->GetKeyword(), buf))
//			if (EXIT(ch, door) && SSData(EXIT(ch, door)->keyword))
//				if (!str_cmp(SSData(EXIT(ch, door)->keyword), buf))
				{
					perform_move(ch, door, 1);
					release_buffer(buf);
					return;
				}
		//	No door was found.  Search for a vehicle
		if (!do_enter_vehicle(ch, buf))
			ch->Send("There is no %s here.\r\n", buf);
	} else if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_INDOORS))
		send_to_char("You are already indoors.", ch);
	else {	//	try to locate an entrance
		for (door = 0; door < NUM_OF_DIRS; door++)
			if (EXIT(ch, door))
				if (EXIT(ch, door)->to_room != NOWHERE)
					if (!EXIT_FLAGGED(EXIT(ch, door), EX_CLOSED) &&
							ROOM_FLAGGED(EXIT(ch, door)->to_room, ROOM_INDOORS)) {
						perform_move(ch, door, 1);
						release_buffer(buf);
						return;
					}
		//	No door was found.  Search for a vehicle
		if (!do_enter_vehicle(ch, buf))
			send_to_char("You can't seem to find anything to enter.\r\n", ch);
	}
	release_buffer(buf);
}


ACMD(do_leave) {
	int door;
	ObjData * hatch, * vehicle;

	/* FIRST see if there is a hatch in the room */
	hatch = get_obj_in_list_type(ITEM_V_HATCH, world[IN_ROOM(ch)].contents);
	if (hatch) {
		vehicle = find_vehicle_by_vnum(GET_OBJ_VAL(hatch, 0));
		if (!vehicle || IN_ROOM(vehicle) == NOWHERE) {
			send_to_char("The hatch seems to lead to nowhere...\r\n", ch);
			return;
		}
		act("$n leaves $p.", TRUE, ch, vehicle, 0, TO_ROOM);
		act("You climb out of $p.", TRUE, ch, vehicle, 0, TO_CHAR);
		ch->from_room();
		ch->to_room(IN_ROOM(vehicle));
		act("$n climbs out of $p.", TRUE, ch, vehicle, 0, TO_ROOM);
		
		/* Now show them the room */
		if (ch->desc != NULL)
			look_at_room(ch, 0);
			
		return;
	}
	if (!ROOM_FLAGGED(IN_ROOM(ch), ROOM_INDOORS))
		send_to_char("You are outside.. where do you want to go?\r\n", ch);
	else {
		for (door = 0; door < NUM_OF_DIRS; door++)
			if (EXIT(ch, door))
				if (EXIT(ch, door)->to_room != NOWHERE)
					if (!EXIT_FLAGGED(EXIT(ch, door), EX_CLOSED) &&
							!ROOM_FLAGGED(EXIT(ch, door)->to_room, ROOM_INDOORS)) {
						perform_move(ch, door, 1);
						return;
					}
		send_to_char("I see no obvious exits to the outside.\r\n", ch);
	}
}


ACMD(do_stand)
{
  ACMD(do_unmount);
  if (ch->sitting_on) {
  	if (IN_ROOM(ch->sitting_on) == IN_ROOM(ch)) {
  	  if(IS_MOUNTABLE(ch->sitting_on)) {
  	  	do_unmount(ch, "", 0, "unmount", 0);
  	  	return;
  	  }
  	  switch (GET_POS(ch)) {
		  case POS_SITTING:
		    act("You stop sitting on $p and stand up.",
		    		FALSE, ch, ch->sitting_on, 0, TO_CHAR);
		    act("$n stops sitting on $p and clambers to $s feet.",
		    		FALSE, ch, ch->sitting_on, 0, TO_ROOM);
		    GET_POS(ch) = POS_STANDING;
			ch->sitting_on = NULL;
		    return;
		  case POS_RESTING:
		    act("You stop resting on $p and stand up.",
		    		FALSE, ch, ch->sitting_on, 0, TO_CHAR);
		    act("$n stops resting on $p and clambers to $s feet.",
		    		FALSE, ch, ch->sitting_on, 0, TO_ROOM);
		    GET_POS(ch) = POS_STANDING;
			ch->sitting_on = NULL;
		    return;
		  default:
		    break;
  	  }
	}
  }
  switch (GET_POS(ch)) {
  case POS_STANDING:
    act("You are already standing.", FALSE, ch, 0, 0, TO_CHAR);
    break;
  case POS_SITTING:
    act("You stand up.", FALSE, ch, 0, 0, TO_CHAR);
    act("$n clambers to $s feet.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_STANDING;
    break;
  case POS_RESTING:
    act("You stop resting, and stand up.", FALSE, ch, 0, 0, TO_CHAR);
    act("$n stops resting, and clambers on $s feet.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_STANDING;
    break;
  case POS_SLEEPING:
    act("You have to wake up first!", FALSE, ch, 0, 0, TO_CHAR);
    break;
  case POS_FIGHTING:
    act("Do you not consider fighting as standing?", FALSE, ch, 0, 0, TO_CHAR);
    break;
  default:
    act("You stop floating around, and put your feet on the ground.",
	FALSE, ch, 0, 0, TO_CHAR);
    act("$n stops floating around, and puts $s feet on the ground.",
	TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_STANDING;
    break;
  }
}


ACMD(do_sit)
{
  ObjData *obj = NULL;
  char *arg	= get_buffer(MAX_INPUT_LENGTH);
  
  one_argument(argument, arg);
  
  if (*arg) {
	obj = get_obj_in_list_vis(ch, arg, world[IN_ROOM(ch)].contents);
	release_buffer(arg);
	if ((obj) && (GET_POS(ch) >= POS_STANDING))
		if (IS_MOUNTABLE(obj)) {
			do_mount(ch, argument, 0, "mount", 0);
			return;
		} else if (IS_SITTABLE(obj)) {
			if (get_num_chars_on_obj(obj) >= GET_OBJ_VAL(obj, 0)) {
				act("There is no more room on $p.", FALSE, ch, obj, 0, TO_CHAR);
				return;
			} else if (!sit_otrigger(obj, ch))
				return;
		} else {
			act("You can't sit on $p.", FALSE, ch, obj, 0, TO_CHAR);
				return;
		}
	} else {
		release_buffer(arg);
	}
  
	switch (GET_POS(ch)) {
		case POS_STANDING:
			if (obj) {
				act("You sit on $p.", FALSE, ch, obj, 0, TO_CHAR);
				act("$n sits on $p.", FALSE, ch, obj, 0, TO_ROOM);
				ch->sitting_on = obj;
			} else {
				act("You sit down.", FALSE, ch, 0, 0, TO_CHAR);
				act("$n sits down.", FALSE, ch, 0, 0, TO_ROOM);
			}
			GET_POS(ch) = POS_SITTING;
			break;
		case POS_SITTING:
			send_to_char("You're sitting already.\r\n", ch);
			break;
		case POS_RESTING:
			act("You stop resting, and sit up.", FALSE, ch, 0, 0, TO_CHAR);
			act("$n stops resting.", TRUE, ch, 0, 0, TO_ROOM);
			GET_POS(ch) = POS_SITTING;
			break;
		case POS_SLEEPING:
			act("You have to wake up first.", FALSE, ch, 0, 0, TO_CHAR);
			break;
		case POS_FIGHTING:
			act("Sit down while fighting? are you MAD?", FALSE, ch, 0, 0, TO_CHAR);
			break;
		default:
			if (obj) {
				act("You stop floating around, and sit on $p.", FALSE, ch, obj, 0, TO_CHAR);
				act("$n stops floating around, and sits on $p.", TRUE, ch, obj, 0, TO_ROOM);
				ch->sitting_on = obj;
			} else {
				act("You stop floating around, and sit down.", FALSE, ch, 0, 0, TO_CHAR);
				act("$n stops floating around, and sits down.", TRUE, ch, 0, 0, TO_ROOM);
			}
			GET_POS(ch) = POS_SITTING;
			break;
	}
}


ACMD(do_rest) {
  ObjData *obj = NULL;
  char * arg = get_buffer(MAX_INPUT_LENGTH);
  
  one_argument(argument, arg);
  
  if ((*arg) && (GET_POS(ch) != POS_SITTING)) {
	obj = get_obj_in_list_vis(ch, arg, world[IN_ROOM(ch)].contents);
	release_buffer(arg);
	if ((obj) && (GET_POS(ch) >= POS_STANDING))
		if (IS_SITTABLE(obj)) {
			if (get_num_chars_on_obj(obj) >= GET_OBJ_VAL(obj, 0)) {
					act("There is no more room on $p.", FALSE, ch, obj, 0, TO_CHAR);
					return;
			} else if (!sit_otrigger(obj, ch))
				return;
		} else {
			act("You can't rest on $p.", FALSE, ch, obj, 0, TO_CHAR);
			return;
		}
  } else
  	release_buffer(arg);
  
  switch (GET_POS(ch)) {
  case POS_STANDING:
  	if (obj) {
	    act("You sit down on $p and rest your tired bones.", FALSE, ch, obj, 0, TO_CHAR);
	    act("$n sits down on $p and rests.", TRUE, ch, obj, 0, TO_ROOM);
		ch->sitting_on = obj;
  	} else {
	    act("You sit down and rest your tired bones.", FALSE, ch, 0, 0, TO_CHAR);
	    act("$n sits down and rests.", TRUE, ch, 0, 0, TO_ROOM);
    }
    GET_POS(ch) = POS_RESTING;
    break;
  case POS_SITTING:
    if (ch->sitting_on)
    	if(IS_MOUNTABLE(ch->sitting_on)) {
    		act("You can't rest on $p.", TRUE, ch, ch->sitting_on, 0, TO_CHAR);
    		return;
    	}
    act("You rest your tired bones.", FALSE, ch, 0, 0, TO_CHAR);
    act("$n rests.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_RESTING;
    break;
  case POS_RESTING:
    act("You are already resting.", FALSE, ch, 0, 0, TO_CHAR);
    break;
  case POS_SLEEPING:
    act("You have to wake up first.", FALSE, ch, 0, 0, TO_CHAR);
    break;
  case POS_FIGHTING:
    act("Rest while fighting?  Are you MAD?", FALSE, ch, 0, 0, TO_CHAR);
    break;
  default:
  	if (obj) {
	    act("You stop floating around, and stop to rest your tired bones on $p.",
				FALSE, ch, obj, 0, TO_CHAR);
	    act("$n stops floating around, and rests on $p.", FALSE, ch, obj, 0, TO_ROOM);
		ch->sitting_on = obj;
  	} else {
	    act("You stop floating around, and stop to rest your tired bones.",
				FALSE, ch, 0, 0, TO_CHAR);
	    act("$n stops floating around, and rests.", FALSE, ch, 0, 0, TO_ROOM);
    }
    GET_POS(ch) = POS_SITTING;
    break;
  }
}


ACMD(do_sleep) {
	ObjData *obj = NULL;
	char * arg = get_buffer(MAX_INPUT_LENGTH);

	one_argument(argument, arg);

	if ((ch->sitting_on) && (IN_ROOM(ch) == IN_ROOM(ch->sitting_on)) && !IS_SLEEPABLE(ch->sitting_on)) {
		act("You can't sleep on $p.", FALSE, ch, ch->sitting_on, 0, TO_CHAR);
		release_buffer(arg);
		return;
	}
	
	if (*arg) {
		obj = get_obj_in_list_vis(ch, arg, world[IN_ROOM(ch)].contents);
		release_buffer(arg);
		if (obj && !IS_SLEEPABLE(obj)) {
			act("You won't be able to sleep on $p.", FALSE, ch, obj, 0, TO_CHAR);
			return;
		}
		if (obj && (GET_POS(ch) >= POS_STANDING)) {
			if (GET_OBJ_TYPE(obj) != ITEM_BED) {
				act("You can't sleep on $p.", FALSE, ch, obj, 0, TO_CHAR);
				return;
			} else if (get_num_chars_on_obj(obj) >= GET_OBJ_VAL(obj, 0)) {
				act("There is no more room on $p.", FALSE, ch, obj, 0, TO_CHAR);
				return;
			} else if (!sit_otrigger(obj, ch) || PURGED(ch))
				return;
			else if (PURGED(obj))
				obj = NULL;		//	In case the script purged the object, but we still let the
								//	player sleep
		}
	} else
		release_buffer(arg);
	

	switch (GET_POS(ch)) {
		case POS_SITTING:
		case POS_RESTING:
			if (obj && ch->sitting_on && (IN_ROOM(ch->sitting_on) == IN_ROOM(ch)) && (ch->sitting_on != obj)) {
				act("You are already $T on $p.", FALSE, ch, ch->sitting_on,
						(GET_POS(ch) == POS_SITTING) ? "sitting" : "resting", TO_CHAR);
				return;
			}
		case POS_STANDING:
			if (obj) {
				act("You lie down on $p and go to sleep.",
				FALSE, ch, obj, 0, TO_CHAR);
				act("$n lies down on $p and falls asleep.", FALSE, ch, obj, 0, TO_ROOM);
				ch->sitting_on = obj;
			} else {
				send_to_char("You go to sleep.\r\n", ch);
				act("$n lies down and falls asleep.", TRUE, ch, 0, 0, TO_ROOM);
			}
			GET_POS(ch) = POS_SLEEPING;
			break;
		case POS_SLEEPING:
			send_to_char("You are already sound asleep.\r\n", ch);
			break;
		case POS_FIGHTING:
			send_to_char("Sleep while fighting?  Are you MAD?\r\n", ch);
			break;
		default:
			if (obj) {
				act("You stop floating around, and fall asleep on $p.",
						FALSE, ch, obj, 0, TO_CHAR);
				act("$n stops floating around, and falls asleep on $p.", FALSE, ch, obj, 0, TO_ROOM);
				ch->sitting_on = obj;
			} else {
				act("You stop floating around, and lie down to sleep.",
						FALSE, ch, 0, 0, TO_CHAR);
				act("$n stops floating around, and lie down to sleep.",
						TRUE, ch, 0, 0, TO_ROOM);
			}
			GET_POS(ch) = POS_SLEEPING;
			break;
	}
}


ACMD(do_wake) {
	CharData *vict;
	int self = 0;
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	
	one_argument(argument, arg);
	
	if (*arg) {
		if (GET_POS(ch) == POS_SLEEPING)
			send_to_char("Maybe you should wake yourself up first.\r\n", ch);
		else if ((vict = get_char_room_vis(ch, arg)) == NULL)
			send_to_char(NOPERSON, ch);
		else if (vict == ch)
			self = 1;
		else if (GET_POS(vict) > POS_SLEEPING)
			act("$E is already awake.", FALSE, ch, 0, vict, TO_CHAR);
		else if (AFF_FLAGGED(vict, AFF_SLEEP))
			act("You can't wake $M up!", FALSE, ch, 0, vict, TO_CHAR);
		else if (GET_POS(vict) < POS_SLEEPING)
			act("$E's in pretty bad shape!", FALSE, ch, 0, vict, TO_CHAR);
		else {
			act("You wake $M up.", FALSE, ch, 0, vict, TO_CHAR);
			act("You are awakened by $n.", FALSE, ch, 0, vict, TO_VICT | TO_SLEEP);
			act("$n wakes $N up.", FALSE, ch, 0, vict, TO_NOTVICT);
			GET_POS(vict) = POS_SITTING;
		}
		if (!self) {
			release_buffer(arg);
			return;
		}
	}
	
	release_buffer(arg);
	
	if (AFF_FLAGGED(ch, AFF_SLEEP))			send_to_char("You can't wake up!\r\n", ch);
	else if (GET_POS(ch) > POS_SLEEPING)	send_to_char("You are already awake...\r\n", ch);
	else {
		send_to_char("You awaken, and sit up.\r\n", ch);
		act("$n awakens.", TRUE, ch, 0, 0, TO_ROOM);
		GET_POS(ch) = POS_SITTING;
	}
}


ACMD(do_follow) {
	CharData *leader;
	char *buf = get_buffer(MAX_INPUT_LENGTH);

	one_argument(argument, buf);

	if (!*buf)
		send_to_char("Whom do you wish to follow?\r\n", ch);
	else if (!(leader = get_char_room_vis(ch, buf)))
		send_to_char(NOPERSON, ch);
	else if (ch->master == leader)
		act("You are already following $M.", FALSE, ch, 0, leader, TO_CHAR);
	else if (AFF_FLAGGED(ch, AFF_CHARM) && (ch->master))
		act("But you only feel like following $N!", FALSE, ch, 0, ch->master, TO_CHAR);
	else if (leader == ch) {			/* Not Charmed follow person */
		if (!ch->master)	send_to_char("You are already following yourself.\r\n", ch);
		else				ch->stop_follower();
	} else if (circle_follow(ch, leader))
		act("Sorry, but following in loops is not allowed.", FALSE, ch, 0, 0, TO_CHAR);
	else {
		if (ch->master)
			ch->stop_follower();
		REMOVE_BIT(AFF_FLAGS(ch), AFF_GROUP);
		add_follower(ch, leader);
	}
	release_buffer(buf);
}


#define CAN_DRAG_CHAR(ch, vict)	((CAN_CARRY_W(ch)) >= (vict->general.weight + IS_CARRYING_W(vict)))
#define CAN_PUSH_CHAR(ch, vict)	((CAN_CARRY_W(ch) /** 2*/) >= (vict->general.weight + IS_CARRYING_W(vict)))
#define CAN_DRAG_OBJ(ch, obj)	((CAN_CARRY_W(ch)) >= GET_OBJ_WEIGHT(obj))
#define CAN_PUSH_OBJ(ch, obj)	((CAN_CARRY_W(ch) /* * 2*/) >= GET_OBJ_WEIGHT(obj))

ACMD(do_push_drag) {
	ObjData *	obj = NULL;
	CharData *	vict;
	int					mode, dir;
	RNum				dest;
	extern char *		dir_text[];
	char	*arg, *arg2;

	if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL) && !NO_STAFF_HASSLE(ch)) {
		act("But its so peaceful here...", FALSE, ch, 0, 0, TO_CHAR);
		return;
	}
	
	arg = get_buffer(MAX_INPUT_LENGTH);
	arg2 = get_buffer(MAX_INPUT_LENGTH);
		
	two_arguments(argument, arg, arg2);

	if (!*arg)
		act("Move what?", FALSE, ch, 0, 0, TO_CHAR);
	else if (!*arg2)
		act("Move it in what direction?", FALSE, ch, 0, 0, TO_CHAR);
	else if (is_abbrev(arg2, "out"))
		do_push_drag_out(ch, arg, cmd, command, subcmd);
	else if ((dir = search_block(arg2, dirs, FALSE)) < 0 || dir > NUM_OF_DIRS)
		act("That's not a valid direction.", FALSE, ch, 0, 0, TO_CHAR);
	else if (!CAN_GO(ch, dir) || EXIT_FLAGGED(EXIT(ch, dir), EX_NOMOVE))
		act("You can't go that way.", FALSE, ch, 0, 0, TO_CHAR);
	else if (dir == UP)
		act("No pushing people up, lamah.", FALSE, ch, 0, 0, TO_CHAR);
	else {
		dest = EXIT(ch, dir)->to_room;
		
		release_buffer(arg2);

		if (!(vict = get_char_room_vis(ch, arg))) {
			if (!(obj = get_obj_in_list_vis(ch, arg, world[IN_ROOM(ch)].contents))) {
				act("There is nothing like that here.", FALSE, ch, 0, 0, TO_CHAR);
				release_buffer(arg);
				return;
			} else
				mode = 0;
		} else if (vict == ch) {
			act("Very funny...", FALSE, ch, 0, 0, TO_CHAR);
			release_buffer(arg);
			return;
		} else
			mode = 1;
	
		release_buffer(arg);
	
		if (!mode) {
			if (!OBJ_FLAGGED(obj, ITEM_MOVEABLE) ||
					(subcmd == SCMD_PUSH ? !CAN_PUSH_OBJ(ch, obj) : !CAN_DRAG_OBJ(ch, obj))) {
				act("You try to $T $p, but it won't move.", FALSE, ch, obj,
						(subcmd == SCMD_PUSH) ? "push" : "drag", TO_CHAR);
				act("$n tries in vain to move $p.", TRUE, ch,obj, 0, TO_ROOM);
				return;
			}
		} else {
			if (GET_POS(vict) < POS_FIGHTING && subcmd == SCMD_PUSH) {
				act("Trying to push people who are on the ground won't work.", FALSE, ch, 0, 0, TO_CHAR);
				return;
			}
			if (!NO_STAFF_HASSLE(ch)) {
				if (IS_NPC(vict)) {
					if (MOB_FLAGGED(vict, MOB_NOBASH)) {
						act("You try to move $N, but fail.", FALSE, ch, 0, vict, TO_CHAR);
						act("$n tries to move $N, but fails.", FALSE, ch, 0, vict, TO_ROOM);
						return;
					}
					if (MOB_FLAGGED(vict, MOB_SENTINEL)) {
						act("$N won't budge.", FALSE, ch, 0, vict, TO_CHAR);
						act("$n tries to move $N, who won't budge.", FALSE, ch, 0, vict, TO_ROOM);
						return;
					}
					if (EXIT_FLAGGED(EXIT(ch, dir), EX_NOMOB)) {
						act("$N can't go that way.", FALSE, ch, 0, vict, TO_CHAR);
						act("$n tries to move $N, who won't budge.", FALSE, ch, 0, vict, TO_ROOM);
						return;
					}
					act("Due to recent abuse of the PUSH and DRAG commands, you can no longer\r\n"
						"do that to MOBs.", FALSE, ch, 0, 0, TO_CHAR);
					return;
				}
				if (NO_STAFF_HASSLE(vict)) {
					act("You can't push staff around!", FALSE, ch, 0, vict, TO_CHAR);
					act("$n is trying to push you around.", FALSE, ch, 0, vict, TO_VICT);
					return;
				}
				if ((subcmd == SCMD_PUSH) ? !CAN_PUSH_CHAR(ch, vict) : !CAN_DRAG_CHAR(ch, vict)) {
					if (subcmd == SCMD_PUSH) {
						act("You try to push $N $t, but fail.", FALSE, ch, (ObjData *)dirs[dir], vict, TO_CHAR);
						act("$n tries to push $N $t, but fails.", FALSE, ch, (ObjData *)dirs[dir], vict, TO_NOTVICT);
						act("$n tried to push you $t!", FALSE, ch, (ObjData *)dirs[dir], vict, TO_VICT);
					} else {
						act("You try to push $N $t, but fail.", FALSE, ch, (ObjData *)dirs[dir], vict, TO_CHAR);
						act("$n tries to push $N $t, but fails.", FALSE, ch, (ObjData *)dirs[dir], vict, TO_NOTVICT);
						act("$n grabs on to you and tries to drag you $T, but can't seem to move you!", FALSE, ch, (ObjData *)dirs[dir], vict, TO_VICT);
					}
					return;
				}
			}
		}
	

		if (subcmd == SCMD_DRAG) {
			switch (mode) {
				case 0:
					act("You drag $p $T.", FALSE, ch, obj, dirs[dir], TO_CHAR );
					act("$n drags $p $T.", FALSE, ch, obj, dirs[dir], TO_ROOM );
					ch->from_room();
					ch->to_room(dest);
					obj->from_room();
					obj->to_room(dest);
					act("$n drags $p into the room from $T.", FALSE, ch, obj, dir_text[rev_dir[dir]], TO_ROOM );
					break;
				case 1:
					act("You drag $N $t.", FALSE, ch, (ObjData *)dirs[dir], vict, TO_CHAR );
					act("$n drags $N $t.", FALSE, ch, (ObjData *)dirs[dir], vict, TO_NOTVICT);
					act("$n drags you $t!", FALSE, ch, (ObjData *)dirs[dir], vict, TO_VICT);
					ch->from_room();
					ch->to_room(dest);
					vict->from_room();
					vict->to_room(dest);
					act("$n drags $N into the room from $t.", FALSE, ch, (ObjData *)dir_text[rev_dir[dir]], vict, TO_NOTVICT );
					look_at_room(vict, 0);
					break;
			}
			look_at_room(ch, 0);
		} else {
			switch (mode) {
				case 0:
					act("You push $p $T.", FALSE, ch, obj, dirs[dir], TO_CHAR );
					act("$n pushes $p $T.", FALSE, ch, obj, dirs[dir], TO_ROOM );
					obj->from_room();
					obj->to_room(dest);
					act("$p is pushed into the room from $T.", FALSE, 0, obj,
							dir_text[rev_dir[dir]], TO_ROOM);
					break;
				case 1:
					act("You push $N $t.", FALSE, ch, (ObjData *)dirs[dir], vict, TO_CHAR );
					act("$n pushes $N $t.", FALSE, ch, (ObjData *)dirs[dir], vict, TO_NOTVICT );
					act("$n pushes you $t!", FALSE, ch, (ObjData *)dirs[dir], vict, TO_VICT);
					vict->from_room();
					vict->to_room(dest);
					if (GET_POS(vict) > POS_SITTING) GET_POS(vict) = POS_SITTING;
					look_at_room(vict, 0);
					act("$n is pushed into the room from $T.", FALSE, vict, 0, dir_text[rev_dir[dir]], TO_ROOM );
					break;
			}
		}
		WAIT_STATE(ch, 10 RL_SEC);
		return;
	}
	release_buffer(arg);
	release_buffer(arg2);
}


ACMD(do_push_drag_out) {
	ObjData * hatch, *vehicle, *obj;
	CharData *vict = NULL;
	int dest, mode;
	char *arg;
	
	if (!(hatch = get_obj_in_list_type(ITEM_V_HATCH, world[IN_ROOM(ch)].contents))) {
		act("What?  There is no vehicle exit here.", FALSE, ch, 0, 0, TO_CHAR);
		return;
	}
	
	if (!(vehicle = find_vehicle_by_vnum(GET_OBJ_VAL(hatch, 0)))) {
		act("The hatch seems to lead to nowhere...", FALSE, ch, 0, 0, TO_CHAR);
		return;
	}

	dest = IN_ROOM(vehicle);
	arg = get_buffer(MAX_INPUT_LENGTH);
	one_argument(argument, arg);
	
	if (!(obj = get_obj_in_list_vis(ch, arg, world[IN_ROOM(ch)].contents))) {
		if (!(vict = get_char_room_vis(ch, arg))) {
			act("There is nothing like that here.", FALSE, ch, 0, 0, TO_CHAR);
			release_buffer(arg);
			return;
		} else if (vict == ch) {
			act("Very funny...", FALSE, ch, 0, 0, TO_CHAR);
			release_buffer(arg);
			return;
		} else
			mode = 1;
		if (!NO_STAFF_HASSLE(ch) && !IS_NPC(ch) && !IS_NPC(vict)) {	// Mort pushing mort out
			act("Shya, as if.  Now everyone knows you are a loser.", FALSE, ch, 0, vict, TO_CHAR);
			act("$n tried to push $N OUT...  What a loser!", FALSE, ch, 0, vict, TO_NOTVICT);
			act("$n tried to push you OUT, because $e is such a loser!", FALSE, ch, 0, vict, TO_VICT);
			release_buffer(arg);
			return;
		}
	} else
		mode = 0;

	release_buffer(arg);
	
	if (!mode) {
		if (!OBJ_FLAGGED(obj, ITEM_MOVEABLE) ||
					(subcmd == SCMD_PUSH ? !CAN_PUSH_OBJ(ch, obj) : !CAN_DRAG_OBJ(ch, obj))) {
			act("You try to $T $p, but it won't move.", FALSE, ch, obj,
					(subcmd == SCMD_PUSH) ? "push" : "drag", TO_CHAR);
			act("$n tries in vain to move $p.", TRUE, ch,obj, 0, TO_ROOM);
			return;
		}
	} else {
		if ((GET_POS(vict) < POS_FIGHTING) && (subcmd == SCMD_PUSH)) {
			act("Trying to push people who are on the ground won't work.", FALSE, ch, 0, 0, TO_CHAR);
			return;
		}
		if (!NO_STAFF_HASSLE(ch)) {
			if (IS_NPC(vict)) {
				if (MOB_FLAGGED(vict, MOB_NOBASH)) {
					act("You try to move $N, but fail.", FALSE, ch, 0, vict, TO_CHAR);
					act("$n tries to move $N, but fails.", FALSE, ch, 0, vict, TO_ROOM);
					return;
				}
				if (MOB_FLAGGED(vict, MOB_SENTINEL)) {
					act("$N won't budge.", FALSE, ch, 0, vict, TO_CHAR);
					act("$n tries to move $N, who won't budge.", FALSE, ch, 0, vict, TO_ROOM);
					return;
				}
	/*			if (IS_SET(EXIT(ch, dir)->exit_info, EX_NOMOB)) {
					act("$N can't go that way.", FALSE, ch, 0, vict, TO_CHAR);
					act("$n tries to move $N, who won't budge.", FALSE, ch, 0, vict, TO_ROOM);
					return;
				}*/
			}
			if (NO_STAFF_HASSLE(vict)) {
				act("You can't push staff around!", FALSE, ch, 0, vict, TO_CHAR);
				act("$n is trying to push you around.", FALSE, ch, 0, vict, TO_VICT);
				return;
			}
			if ((subcmd == SCMD_PUSH) ? !CAN_PUSH_CHAR(ch, vict) : !CAN_DRAG_CHAR(ch, vict)) {
				if (subcmd == SCMD_PUSH) {
					act("You try to push $N out, but fail.", FALSE, ch, 0, vict, TO_CHAR);
					act("$n tries to push $N out, but fails.", FALSE, ch, 0, vict, TO_NOTVICT);
					act("$n tried to push you out!", FALSE, ch, 0, vict, TO_VICT);
				} else {
					act("You try to push $N out, but fail.", FALSE, ch, 0, vict, TO_CHAR);
					act("$n tries to push $N out, but fails.", FALSE, ch, 0, vict, TO_NOTVICT);
					act("$n grabs on to you and tries to drag you out, but can't seem to move you!", FALSE, ch, 0, vict, TO_VICT);
				}
				return;
			}
		}
	}
	

	if (subcmd == SCMD_DRAG) {
		if (mode) {
			act("You drag $N out.", FALSE, ch, 0, vict, TO_CHAR );
			act("$n drags $N out.", FALSE, ch, 0, vict, TO_NOTVICT);
			act("$n drags you out!", FALSE, ch, 0, vict, TO_VICT);
			ch->from_room();
			ch->to_room(dest);
			vict->from_room();
			vict->to_room(dest);
			act("$n drags $N out of $p.", FALSE, ch, vehicle, vict, TO_NOTVICT );
			look_at_room(vict, 0);
			look_at_room(ch, 0);
		} else {
			act("You drag $p out.", FALSE, ch, obj, 0, TO_CHAR );
			act("$n drags $p out.", FALSE, ch, obj, 0, TO_ROOM );
			ch->from_room();
			ch->to_room(dest);
			obj->from_room();
			obj->to_room(dest);
			act("$n drags $p out of $P.", FALSE, ch, obj, vehicle, TO_ROOM );
			look_at_room(ch, 0);
		}
		look_at_room(ch, 0);
	} else {
		if (mode) {
			act("You push $N out.", FALSE, ch, 0, vict, TO_CHAR );
			act("$n pushes $N out.", FALSE, ch, 0, vict, TO_NOTVICT );
			act("$n pushes you out!", FALSE, ch, 0, vict, TO_VICT);
			vict->from_room();
			vict->to_room(dest);
			if (GET_POS(vict) > POS_SITTING) GET_POS(vict) = POS_SITTING;
			look_at_room(vict, 0);
			act("$n is pushed out of $p.", FALSE, vict, vehicle, 0, TO_ROOM );
		} else {
			act("You push $p out.", FALSE, ch, obj, 0, TO_CHAR );
			act("$n pushes $p out.", FALSE, ch, obj, 0, TO_ROOM );
			obj->from_room();
			obj->to_room(dest);
			act("$p is pushed out of $P.", FALSE, 0, obj, vehicle, TO_ROOM);
		}
	}
	WAIT_STATE(ch, 10 RL_SEC);
	return;
}


#define MAX_DISTANCE 6

const char * distance_text[MAX_DISTANCE + 1] = {
	"SHOULD NOT SEE!  REPORT IMMEDIATELY!",
	"Immediately",
	"Nearby",
	"A little ways away",
	"A ways",
	"Off in the distance",
	"Far away"
};
const char * distance_text_2[MAX_DISTANCE + 1] = {
	"SHOULD NOT SEE!  REPORT IMMEDIATELY!",
	"immediately",
	"nearby",
	"a little ways away",
	"a ways",
	"off in the distance",
	"far away"
};


void handle_motion_leaving(CharData *ch, int direction) {
	int door, nextroom, distance;
	CharData *vict;
	extern char * dirs[];
	extern char * dir_text_2[];
	extern int rev_dir[];
	char *buf = get_buffer(MAX_INPUT_LENGTH);
	
	for (door = 0; door < NUM_OF_DIRS; door++) {
		if (!CAN_GO(ch, door))
			continue;
		nextroom = IN_ROOM(ch);		/* Get the next room */


		for (distance = 1; (distance < MAX_DISTANCE + 1); distance++) {
			if (!CAN_GO2(nextroom, door))
				break;
			nextroom = EXITN(nextroom, door)->to_room;
			
			LListIterator<CharData *>	iter(world[nextroom].people);
			
			while ((vict = iter.Next())) {
				if (!AWAKE(vict))
					continue;
				if (AFF_FLAGGED(vict, AFF_MOTIONTRACKING)) {
					vict->Send("You detect motion %s %s you.", distance_text_2[distance],
							dir_text_2[rev_dir[door]] );
				}
				if (rev_dir[CHAR_WATCHING(vict) - 1] == door && vict->CanSee(ch)) {
					if (((distance == MAX_DISTANCE) &&						/* Moving out	*/
								(CHAR_WATCHING(vict) - 1 == direction)) ||
								((door != direction) && (door != rev_dir[direction]))) {	/* Moving out of view */
						sprintf(buf, "%s %s you see $N move out of view.", distance_text[distance],
								dir_text_2[CHAR_WATCHING(vict) - 1]);
						act(buf, TRUE, vict, NULL, ch, TO_CHAR);
					} else if ((door != direction) || (distance > 1)) {
						sprintf(buf, "%s %s you see $N move %s you.", distance_text[distance],
								dirs[CHAR_WATCHING(vict) - 1],
								(direction == ( CHAR_WATCHING(vict) - 1 )) ? "away from" : "towards");
						act(buf, TRUE, vict, NULL, ch, TO_CHAR);
					}
				}	/* IF ...watching... */
			}
		}	/* FOR distance */
	}	/* FOR door */
	release_buffer(buf);
}


void handle_motion_entering(CharData *ch, int direction) {
	int door, nextroom, distance;
	CharData *vict;
	extern char * dir_text_2[];
	extern int rev_dir[];
	char *buf = get_buffer(MAX_INPUT_LENGTH);

	direction = rev_dir[direction];

	for (door = 0; door < NUM_OF_DIRS; door++) {
		if (!CAN_GO(ch, door))	continue;
		nextroom = IN_ROOM(ch);		/* Get the next room */

		for (distance = 1; (distance < MAX_DISTANCE + 1); distance++) {
			if (!CAN_GO2(nextroom, door))
				break;
			nextroom = EXITN(nextroom, door)->to_room;
			LListIterator<CharData *>	iter(world[nextroom].people);
			
			while ((vict = iter.Next())) {
				if (!AWAKE(vict))
					continue;
				if (AFF_FLAGGED(vict, AFF_MOTIONTRACKING) &&
						(((distance == MAX_DISTANCE) && (direction == rev_dir[door])) ||
						((door != direction) && (door != rev_dir[direction])))) {
					vict->Send("You detect motion %s %s you.", distance_text_2[distance],
							dir_text_2[rev_dir[door]] );
				}
				if (rev_dir[CHAR_WATCHING(vict) - 1] == door && vict->CanSee(ch)) {
					if (((distance == MAX_DISTANCE) && (CHAR_WATCHING(vict) - 1 == direction)) ||
								((door != direction) && (door != rev_dir[direction]))) {	/* Moving into view */
						sprintf(buf, "%s %s you see $N move into view.", distance_text[distance],
								dir_text_2[CHAR_WATCHING(vict) - 1]);
						act(buf, TRUE, vict, 0, ch, TO_CHAR);
					}
				}	/* IF ...watching... */
			}
		}	/* FOR distance */
		
		
	}	/* FOR door */
	release_buffer(buf);
}


ACMD(do_recall) {
	if (FindEvent(ch->events, gravity)) {
		send_to_char("For some reason you can't recall!\r\n", ch);
		return;
	}
	act("$n disappears in a bright flash of light.", TRUE, ch, 0, 0, TO_ROOM);
	
/*	clean_special_events(ch, gravity, EVENT_BOTH);*/
	
	ch->from_room();
	ch->to_room(ch->StartRoom());
 	
	act("$n appears in a bright flash of light.", TRUE, ch, 0, 0, TO_ROOM);
	look_at_room(ch, 0);
}

