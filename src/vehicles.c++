/* ************************************************************************
*   File: vehicles.c                                    Part of CircleMUD *
*  Usage: Majority of vehicle-oriented code                               *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Vehicles.c is Copyright (C) 1997 Chris Jacobson                        *
*  CircleMUD is Copyright (C) 1993, 94 by the Trustees of the Johns       *
*  Hopkins University                                                     *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */


#include "characters.h"
#include "objects.h"
#include "rooms.h"
#include "utils.h"
#include "buffer.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "find.h"
#include "event.h"

ACMD(do_drive);
ACMD(do_mount);
ACMD(do_unmount);

extern int rev_dir[];
extern char *dirs[];
extern char *room_bits[];
extern char * dir_text[NUM_OF_DIRS];

ACMD(do_look);
void vehicle_move(ObjData *vehicle, RNum to);
int sit_otrigger(ObjData *obj, CharData *actor);


EVENTFUNC(drive_event);
ACMD(do_turn);
ACMD(do_accelerate);


struct DriveEvent {		//	It keeps the IDs of these, rather than pointers.
	SInt32		ch;
	SInt32		vehicle;
	SInt32		controls;
	SInt8		direction;
	SInt8		speed;
};


#define	VEHICLE_FLAGGED(vehicle, flag)	(OBJVAL_FLAGGED(vehicle, flag))
#define	VEHICLE_GROUND			(1 << 0)
#define VEHICLE_AIR				(1 << 1)
#define VEHICLE_SPACE			(1 << 2)
#define VEHICLE_INTERSTELLAR	(1 << 3)
#define VEHICLE_WATER			(1 << 4)
#define VEHICLE_SUBMERGE		(1 << 5)

#define ROOM_SECT(room)			(world[room].sector_type)

#define DRIVE_NOT		0
#define DRIVE_CONTROL	1
#define DRIVE_MOUNT		2

ACMD(do_drive) {
	int dir, drive_mode = DRIVE_NOT, i;
	ObjData *vehicle, *controls = NULL, *vehicle_in_out;
	char *arg1;
	
	if (ch->sitting_on && IS_MOUNTABLE(ch->sitting_on))
		drive_mode = DRIVE_MOUNT;
	else {
		drive_mode = DRIVE_CONTROL;
		if (!(controls = get_obj_in_list_type(ITEM_V_CONTROLS, world[IN_ROOM(ch)].contents)))
			if (!(controls = get_obj_in_list_type(ITEM_V_CONTROLS, ch->carrying)))
				for (i = 0; i < NUM_WEARS; i++)
					if (GET_EQ(ch, i) && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_V_CONTROLS) {
						controls = GET_EQ(ch, i);
						break;
					}
				
		if (!controls) {
			send_to_char("You can't do that here.\r\n", ch);
			return;
		}
		if (!ch->CanUse(controls)) {
			send_to_char("You can't figure out the controls.\r\n", ch);
			return;
		}
		if (GET_POS(ch) > POS_SITTING) {
			send_to_char("You must be sitting to drive!\r\n", ch);
			return;
		}
	}
	
	if (drive_mode == DRIVE_CONTROL) {
		vehicle = find_vehicle_by_vnum(GET_OBJ_VAL(controls, 0));
		if (!vehicle || (GET_OBJ_TYPE(vehicle) != ITEM_VEHICLE)) {
			send_to_char("The controls seem to be broken.\r\n", ch);
			return;
		}
	} else if (!(vehicle = (ch->sitting_on)) || (IN_ROOM(vehicle) == NOWHERE)) {
		send_to_char("The vehicle exists yet it doesn't.  Sorry, you're screwed...\r\n", ch);
		return;
	}
	if (AFF_FLAGGED(ch, AFF_BLIND)) {
		/* Blind characters can't drive! */
		send_to_char("You can't see the controls!\r\n", ch);
		return;
	}
	
	arg1 = get_buffer(MAX_INPUT_LENGTH);
	argument = one_argument(argument, arg1);
	
	/* Gotta give us a direction... */
	if (!*arg1)
		send_to_char("Drive which direction?\r\n", ch);
	else if (is_abbrev(arg1, "into")) {	// Driving Into another Vehicle
		int is_going_to;
		
		one_argument(argument, arg1);
		
		if (!*arg1)
			send_to_char("Drive into what?\r\n", ch);
		else if (!(vehicle_in_out = get_obj_in_list_vis(ch, arg1, world[IN_ROOM(vehicle)].contents)))
			send_to_char("Nothing here by that name!\r\n", ch);
		else if (GET_OBJ_TYPE(vehicle_in_out) != ITEM_VEHICLE)
			send_to_char("Thats not a vehicle.\r\n", ch);
		else if(vehicle == vehicle_in_out)
			send_to_char("Very funny...\r\n", ch);
		else if (IS_MOUNTABLE(vehicle_in_out))
			send_to_char("Thats a mountable vehicle, not one you can drive into!\r\n", ch);
		else if (GET_OBJ_VAL(vehicle_in_out, 0) == NOWHERE)
			send_to_char("Thats not a vehicle.\r\n", ch);
		else if ((GET_OBJ_VAL(vehicle_in_out, 4) / 2) < GET_OBJ_VAL(vehicle, 4))
			act("$p is too large to drive into $P.", TRUE, ch, vehicle, vehicle_in_out, TO_CHAR);
		else if (((is_going_to = real_room(GET_OBJ_VAL(vehicle_in_out, 0))) == NOWHERE) ||
				!ROOM_FLAGGED(is_going_to, ROOM_VEHICLE))
			send_to_char("That vehicle can't carry other vehicles.\r\n", ch);
		else if(drive_mode == DRIVE_MOUNT) {
			act("$n drives $p into $P.", TRUE, ch, vehicle, vehicle_in_out, TO_ROOM);
			act("You drive $p into $P.", TRUE, ch, vehicle, vehicle_in_out, TO_CHAR);
			
			vehicle_move(vehicle, is_going_to);
			
			act("$n drives $p in.", TRUE, ch, vehicle, 0, TO_ROOM);
			
			look_at_room(ch, 0);
		} else {
			act("You drive into $P", TRUE, ch, 0, vehicle_in_out, TO_CHAR);
			act("$n drives into $P.", TRUE, ch, 0, vehicle_in_out, TO_ROOM);
			act("$p enters $P.\r\n", TRUE, 0, vehicle, vehicle_in_out, TO_ROOM);

			vehicle_move(vehicle, is_going_to);
			
			act("$p enters.", FALSE, 0, vehicle, 0, TO_ROOM);
			
			look_at_rnum(ch, IN_ROOM(vehicle), FALSE);
		}
	} else if (is_abbrev(arg1, "out")) {
		ObjData *hatch;
		
		if (!(hatch = get_obj_in_list_type(ITEM_V_HATCH, world[IN_ROOM(vehicle)].contents)))
			send_to_char("Nowhere to drive out of.\r\n", ch);
		else if (!(vehicle_in_out = find_vehicle_by_vnum(GET_OBJ_VAL(hatch, 0))) ||
				(IN_ROOM(vehicle_in_out) == NOWHERE))
			send_to_char("ERROR!  Vehicle to drive out of doesn't exist!\r\n", ch);
		else if (drive_mode == DRIVE_MOUNT) {
			act("$n drives $p out.", TRUE, ch, vehicle, 0, TO_ROOM);
			act("You drive $p out of $P.", TRUE, ch, vehicle, vehicle_in_out, TO_CHAR);

			vehicle_move(vehicle, IN_ROOM(vehicle_in_out));
			
			act("$n drives $p out of $P.", TRUE, ch, vehicle, vehicle_in_out, TO_ROOM);
			look_at_room(ch, 0);
		} else {
			act("$p exits $P.", TRUE, 0, vehicle, vehicle_in_out, TO_ROOM);

			vehicle_move(vehicle, IN_ROOM(vehicle_in_out));
			
			act("$p drives out of $P.", TRUE, 0, vehicle, vehicle_in_out, TO_ROOM);
			act("$n drives out of $P.", TRUE, ch, 0, vehicle_in_out, TO_ROOM);
			act("You drive $p out of $P.", TRUE, ch, vehicle, vehicle_in_out, TO_CHAR);
			
			look_at_rnum(ch, IN_ROOM(vehicle), FALSE);
		}
	} else if ((dir = search_block(arg1, dirs, FALSE)) >= 0) {	// Drive in a direction...
		/* Ok we found the direction! */
		if (!ch || (dir < 0) || (dir >= NUM_OF_DIRS) || (IN_ROOM(vehicle) == NOWHERE))
			/* But something is invalid */
			send_to_char("Sorry, an internal error has occurred.\r\n", ch);
		else if (!CAN_GO(vehicle, dir))	// But there is no exit that way */
			send_to_char("Alas, you cannot go that way...\r\n", ch);
		else if (EXIT_FLAGGED(EXIT(vehicle, dir), EX_CLOSED)) { // But the door is closed
			if (EXIT(vehicle, dir)->GetKeyword())
				act("The $F seems to be closed.", FALSE, ch, 0, const_cast<char *>(EXIT(vehicle, dir)->GetKeyword()), TO_CHAR);
			else
				act("It seems to be closed.", FALSE, ch, 0, 0, TO_CHAR);
		} else if (!ROOM_FLAGGED(EXIT(vehicle, dir)->to_room, ROOM_VEHICLE) ||
				EXIT_FLAGGED(EXIT(vehicle, dir), EX_NOVEHICLES))	// But the vehicle can't go that way */
			send_to_char("The vehicle can't manage that terrain.\r\n", ch);
		else if (ROOM_SECT(EXIT(vehicle, dir)->to_room) == SECT_FLYING &&
				!VEHICLE_FLAGGED(vehicle, VEHICLE_AIR)) {
			/* This vehicle isn't a flying vehicle */
			if (ROOM_SECT(IN_ROOM(vehicle)) == SECT_SPACE)
				act("The $o can't enter the atmosphere.", FALSE, ch, vehicle, 0, TO_CHAR);
			else
				act("The $o can't fly.", FALSE, ch, vehicle, 0, TO_CHAR);
		} else if ((ROOM_SECT(EXIT(vehicle, dir)->to_room) == SECT_WATER_NOSWIM) &&
				!VEHICLE_FLAGGED(vehicle, VEHICLE_WATER | VEHICLE_SUBMERGE))
			/* This vehicle isn't a water vehicle */
			act("The $o can't go in such deep water!", FALSE, ch, vehicle, 0, TO_CHAR);
		else if ((ROOM_SECT(EXIT(vehicle, dir)->to_room) == SECT_UNDERWATER) &&
				!VEHICLE_FLAGGED(vehicle, VEHICLE_SUBMERGE))
			act("The $o can't go underwater!", FALSE, ch, vehicle, 0, TO_CHAR);
		else if (ROOM_SECT(EXIT(vehicle, dir)->to_room) == SECT_DEEPSPACE &&
					!VEHICLE_FLAGGED(vehicle, VEHICLE_INTERSTELLAR))
			/* This vehicle isn't a space vehicle */
			act("The $o can't go into space!", FALSE, ch, vehicle, 0, TO_CHAR);
		else if (ROOM_SECT(EXIT(vehicle, dir)->to_room) == SECT_SPACE &&
				!VEHICLE_FLAGGED(vehicle, VEHICLE_SPACE))
			act("The $o cannot handle space travel.", FALSE, ch, vehicle, 0, TO_CHAR);
		else if ((ROOM_SECT(EXIT(vehicle, dir)->to_room) > SECT_FIELD) &&
				(ROOM_SECT(EXIT(vehicle, dir)->to_room) <= SECT_MOUNTAIN) &&
				!VEHICLE_FLAGGED(vehicle, VEHICLE_GROUND))
			act("The $o cannot land on that terrain.", FALSE, ch, vehicle, 0, TO_CHAR);
		else {	//	But nothing!  Let's go that way!			
			if(drive_mode == DRIVE_MOUNT)
				act("$n drives $p $T.", TRUE, ch, vehicle, dirs[dir], TO_ROOM);
			else
				act("$p leaves $T.", FALSE, 0, vehicle, dirs[dir], TO_ROOM);

			vehicle_move(vehicle, world[IN_ROOM(vehicle)].dir_option[dir]->to_room);
			
			if (drive_mode == DRIVE_MOUNT) {
				act("You drive $p $T.", TRUE, ch, vehicle, dirs[dir], TO_CHAR);
				act("$n enters from $T on $p.", TRUE, ch, vehicle,
						dir_text[rev_dir[dir]], TO_ROOM);
				if (ch->desc != NULL)
					look_at_room(ch, 0);
			} else {
				act("$p enters from $T.", FALSE, 0, vehicle, dir_text[rev_dir[dir]], TO_ROOM);
				act("$n drives $T.", TRUE, ch, 0, dirs[dir], TO_ROOM);
				act("You drive $T.", TRUE, ch, 0, dirs[dir], TO_CHAR);
				if (ch->desc != NULL)
					look_at_rnum(ch, IN_ROOM(vehicle), FALSE);
			}
		}
	} else
		send_to_char("Thats not a valid direction.\r\n", ch);
	release_buffer(arg1);
}




ACMD(do_mount) {
	ObjData *vehicle;
	CharData *tch;
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	
	one_argument(argument, arg);

	if (!*arg)
		act("Mount what?", TRUE, ch, 0, 0, TO_CHAR);
	else if (!(vehicle = get_obj_in_list_vis(ch, arg, world[IN_ROOM(ch)].contents)))
		act("You don't see that here.", TRUE, ch, 0, 0, TO_CHAR);
	else if ((tch = get_char_on_obj(vehicle)))
		act("$N is already mounted on $p.", TRUE, ch, vehicle, tch, TO_CHAR);
	else if (!IS_MOUNTABLE(vehicle))
		act("You can't mount $p.", TRUE, ch, vehicle, 0, TO_CHAR);
	else if (sit_otrigger(vehicle, ch)) {
		act("You mount $p.", TRUE, ch, vehicle, 0, TO_CHAR);
		act("$n mounts $p.", TRUE, ch, vehicle, 0, TO_ROOM);
		GET_POS(ch) = POS_SITTING;
		ch->sitting_on = vehicle;
	}
	release_buffer(arg);
}


ACMD(do_unmount) {
	if (!ch->sitting_on)
		act("You aren't sitting on anything.", TRUE, ch, 0, 0, TO_CHAR);
	else if (!IS_MOUNTABLE(ch->sitting_on))
		act("You aren't sitting on a vehicle.", TRUE, ch, 0, 0, TO_CHAR);
	else {
		GET_POS(ch) = POS_STANDING;
		act("You get off $p.", TRUE, ch, ch->sitting_on, 0, TO_CHAR);
		act("$n gets off $p.", TRUE, ch, ch->sitting_on, 0, TO_ROOM);
		ch->sitting_on = NULL;
	}
}


void vehicle_move(ObjData *vehicle, RNum to) {
	RNum	prev_in;
	CharData *tch;
	LListIterator<CharData *>	iter;
	
	prev_in = IN_ROOM(vehicle);
	
	// First things first... move all the riders to the room
	// Otherwise, vehicle->from_room will set everyone who was
	// sitting on the object, as sitting on NULL
	iter.Start(world[prev_in].people);
	while ((tch = iter.Next())) {
		if (tch->sitting_on == vehicle) {
			tch->from_room();	// This forgets the sitting object
			tch->to_room(to);
			tch->sitting_on = vehicle;
		}
	}
	
	vehicle->from_room();
	vehicle->to_room(to);
	
	iter.Restart(world[to].people);
	while ((tch = iter.Next())) {
		if (tch->sitting_on == vehicle)
			look_at_rnum(tch, to, FALSE);
	}
}


UInt8 turn_dir[4][2] = {
//  L R 
{WEST,EAST},	 //	NORTH
{NORTH,SOUTH},	//	EAST
{EAST,WEST},	//	SOUTH
{SOUTH,NORTH}	//	WEST
};

int temp;
#define GET_DRIVE_DIR(ch)	(temp)
#define GET_DRIVE_SPEED(ch)	(temp)

char *turn_dir_names[] = {
"north",
"east",
"south",
"west",
"up",
"down",
"left",
"right"
};

#define LEFT	6
#define	RIGHT	7

ACMD(do_turn) {
	UInt32	dir;
	char *arg;
	ObjData *controls = NULL, *vehicle = NULL;
//	UInt32	i;
	struct event_info *event;
	
/*
	if (!ch->sitting_on || !IS_MOUNTABLE(ch->sitting_on)) {
		if (!(controls = get_obj_in_list_type(ITEM_V_CONTROLS, world[IN_ROOM(ch)].contents)))
			if (!(controls = get_obj_in_list_type(ITEM_V_CONTROLS, ch->carrying)))
				for (i = 0; i < NUM_WEARS; i++)
					if (GET_EQ(ch, i) && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_V_CONTROLS) {
						controls = GET_EQ(ch, i);
						break;
					}
		if (controls)
			vehicle = find_vehicle_by_vnum(GET_OBJ_VAL(controls, 0));
		else {
			send_to_char("Turn what?\r\n", ch);
			return;
		}
	} else
		vehicle = ch->sitting_on;

	if (!vehicle) {
		send_to_char("The vehicle mysteriously has disappeared...\r\n", ch);
		return;
	}
*/

	if (!(event = get_event(ch, drive_event, EVENT_CAUSER))) {
		send_to_char("You aren't driving anything.\r\n", ch);
		return;
	}

	arg = get_buffer(MAX_INPUT_LENGTH);
	
	one_argument(argument, arg);

	if (!*arg)
		send_to_char("Turn which way?\r\n", ch);
	else if ((dir = search_block(arg, turn_dir_names, FALSE)) < 0)
		send_to_char("Turn WHICH way?", ch);
	else if (GET_DRIVE_DIR(ch) == UP) {
		if (dir > WEST)
			send_to_char("You're going UP, you can't turn like that.\r\n", ch);
		else {
			// TURN AND LEVEL OUT IN THE DIRECTION
			act("You turn the $p to the $T.", TRUE, ch, vehicle, turn_dir_names[dir], TO_CHAR);
			act("$n turns the $p to the $T.", TRUE, ch, vehicle, turn_dir_names[dir], TO_ROOM);
			GET_DRIVE_DIR(ch) = dir;
		}	
	} else if (GET_DRIVE_DIR(ch) == DOWN) {
		if (dir > WEST)
			send_to_char("You're going DOWN, you can't turn like that.\r\n", ch);
		else {
			// TURN AND LEVEL OUT IN THE DIRECTION
			act("You turn the $p to the $T.", TRUE, ch, vehicle, turn_dir_names[dir], TO_CHAR);
			act("$n turns the $p to the $T.", TRUE, ch, vehicle, turn_dir_names[dir], TO_ROOM);
			GET_DRIVE_DIR(ch) = dir;
		}
	} else {
		if ((dir < LEFT) && (dir != turn_dir[GET_DRIVE_DIR(ch)][0]) &&
				(dir != turn_dir[GET_DRIVE_DIR(ch)][1]))
			send_to_char("You can't turn more than 90 degrees at one time!\r\n", ch);
		else {
			act("You turn the $p to the $T.", TRUE, ch, vehicle, turn_dir_names[dir], TO_CHAR);
			act("$n turns the $p to the $T.", TRUE, ch, vehicle, turn_dir_names[dir], TO_ROOM);
			GET_DRIVE_DIR(ch) = dir;
		}
	}
	release_buffer(arg);
}


ACMD(do_accelerate) {
	
}


EVENTFUNC(drive_event) {
	DriveEvent *DEData = (DriveEvent *)event_obj;
	CharData *ch = find_char(DEData->ch);
	ObjData *controls = find_obj(DEData->controls);
	ObjData *vehicle = NULL; // = find_obj(((DriveEvent *)event_obj)->vehicle);
	SInt32	drive_speed;
	Event *temp;
//	DriveEvent *eventObj;
	
	if (!ch)
		return 0;
	
	if (!controls || !vehicle) {	//	Event died.
		return 0;
	}
		
	if (IN_ROOM(ch) != IN_ROOM(controls)) {
		return 0;	//	They left, the loser.
	}
		
	if (IS_MOUNTABLE(controls))
		vehicle = controls;
	
	if (!vehicle && !(vehicle = find_vehicle_by_vnum(GET_OBJ_VAL(controls, 0)))) {
		return 0;	//	Vehicle disappeared!
	}
	
	if (EXIT_FLAGGED(EXIT(vehicle, DEData->direction), EX_CLOSED | EX_NOVEHICLES)) {
		//	CRASH
		act("`RThe $p SLAMS into the wall, throwing you all around!`n", false, ch, vehicle, 0, TO_ROOM);
		act("`RThe $p SLAMS into the wall!`n", false, 0, vehicle, 0, TO_ROOM);
		GET_DRIVE_SPEED(ch) = 0;
		return 0;
	}
	
	// DO THE MOVE
	do_drive(ch, dirs[DEData->direction], 0, "drive", 0);
	
	if (EXIT_FLAGGED(EXIT(vehicle, DEData->direction), EX_CLOSED | EX_NOVEHICLES))
		send_to_char("`WYou are about to crash!`n\r\n", ch);		//	WARN OF VERY NEAR DOOM
	else if (!EXITN(EXIT(vehicle, DEData->direction)->to_room, DEData->direction) ||
			EXIT_FLAGGED(EXITN(EXIT(vehicle, DEData->direction)->to_room, DEData->direction), EX_CLOSED | EX_NOVEHICLES))
		send_to_char("You are approaching a dead end!`n\r\n", ch);	//	WARN OF IMPENDING DOOM
	
	drive_speed = 5;

//	CREATE(eventObj, DriveEvent, 1);
//	eventObj->ch = GET_ID(ch);
//	eventObj->vehicle = vehicle ? GET_ID(vehicle) : 0;
//	eventObj->controls = GET_ID(controls);
//	eventObj->direction = ((DriveEvent *)event_obj)->direction;
//	eventObj->speed = ((DriveEvent *)event_obj)->speed;
	
//	ch->events = new Event(drive_event, eventObj, drive_speed, ch->events);
	return (drive_speed RL_SEC);
}
