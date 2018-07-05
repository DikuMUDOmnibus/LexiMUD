#include "structs.h"
#include "scripts.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "find.h"
#include "handler.h"
#include "buffer.h"
#include "db.h"
#include "olc.h"
#include "spells.h"
#include "event.h"


#define SCMD_SEND		0
#define SCMD_ECHOAROUND	1


extern char *dirs[];
long asciiflag_conv(char *flag);
void AlterHit(CharData *ch, SInt32 amount);

#define SCMD(name)	void (name)(Ptr go, int type, char *argument, int subcmd)

#define script_log(msg)	script_error(go, type, (msg))

#define GET_TARGET_CHAR(arg)	((type == MOB_TRIGGER) ? get_char_vis((CharData *)go, arg) : \
			((type == OBJ_TRIGGER) ? get_char_by_obj((ObjData *)go, arg) :					 \
			((type == WLD_TRIGGER) ? get_char_by_room((RoomData *)go, arg) : NULL)))
			
#define GET_TARGET_OBJ(arg)		((type == MOB_TRIGGER) ? get_obj_vis((CharData *)go, arg) : \
			((type == OBJ_TRIGGER) ? get_obj_by_obj((ObjData *)go, arg) :					\
			((type == WLD_TRIGGER) ? get_obj_by_room((RoomData *)go, arg) : NULL)))

#define GET_TARGET_ROOM(arg)	((type == MOB_TRIGGER) ? find_target_room_by_char((CharData *)go, arg) : \
			((type == OBJ_TRIGGER) ? find_target_room_by_obj((ObjData *)go, arg) :					\
			((type == WLD_TRIGGER) ? find_target_room_by_room((RoomData *)go, arg) : NOWHERE)))

void script_error(Ptr go, int type, char *msg);
RNum find_target_room_by_room(RoomData *room, char *roomstr);
RNum find_target_room_by_obj(ObjData *obj, char *roomstr);
RNum find_target_room_by_char(CharData * ch, char *rawroomstr);
RNum this_room(Ptr go, int type);
void script_command_interpreter(Ptr go, char *argument, int type);


SCMD(func_asound);
SCMD(func_echo);
SCMD(func_send);
SCMD(func_zoneecho);
SCMD(func_teleport);
SCMD(func_door);
SCMD(func_force);
SCMD(func_mp);
SCMD(func_kill);		//	Mob only
SCMD(func_junk);		//	Mob only
SCMD(func_load);
SCMD(func_purge);
SCMD(func_at);
SCMD(func_damage);
SCMD(func_heal);
SCMD(func_remove);
SCMD(func_goto);
SCMD(func_info);
SCMD(func_copy);


void script_error(Ptr go, int type, char *msg) {
	switch (type) {
		case MOB_TRIGGER:	mudlogf(NRM, LVL_STAFF, FALSE, "SCRIPTERR: Mob (%s, VNum %d): %s",
									((CharData *)go)->RealName(), GET_MOB_VNUM((CharData *)go), msg);
							break;
		case OBJ_TRIGGER:	mudlogf(NRM, LVL_STAFF, FALSE, "SCRIPTERR: Obj (%s, VNum %d): %s",
									SSData(((ObjData *)go)->shortDesc), GET_OBJ_VNUM((ObjData *)go), msg);
							break;
		case WLD_TRIGGER:	mudlogf(NRM, LVL_STAFF, FALSE, "SCRIPTERR: Wld (Room %d): %s",
									((RoomData *)go)->number, msg);
							break;
		default:			mudlogf(NRM, LVL_STAFF, FALSE, "SCRIPTERR: Unknown (Ptr %p): %s", go, msg);
							break;
	}
}


RNum this_room(Ptr go, int type) {
	switch (type) {
		case MOB_TRIGGER:	return IN_ROOM((CharData *)go);
		case OBJ_TRIGGER:	return ((ObjData *)go)->Room();
		//	A hacky way to do it... maybe (room - world) ?  >shrug<
		case WLD_TRIGGER:	return real_room(((RoomData *)go)->number);
	}
	return NOWHERE;
}


RNum find_target_room_by_char(CharData *ch, char *roomstr) {
	RNum location = NOWHERE;
	CharData *target_mob;
	ObjData *target_obj;
	
	if (isdigit(*roomstr) && !strchr(roomstr, '.'))
		location = real_room(atoi(roomstr));
	else if ((target_mob = get_char_vis(ch, roomstr)))
		location = IN_ROOM(target_mob);
	else if ((target_obj = get_obj_vis(ch, roomstr)))
		location = IN_ROOM(target_obj);

	return location;
}


RNum find_target_room_by_room(RoomData *room, char *roomstr) {
	RNum location = NOWHERE;
	CharData *target_mob;
	ObjData *target_obj;
	
	if (isdigit(*roomstr) && !strchr(roomstr, '.'))
		location = real_room(atoi(roomstr));
	else if ((target_mob = get_char_by_room(room, roomstr)))
		location = IN_ROOM(target_mob);
	else if ((target_obj = get_obj_by_room(room, roomstr)))
		location = IN_ROOM(target_obj);

	return location;
}


RNum find_target_room_by_obj(ObjData *obj, char *roomstr) {
	RNum location = NOWHERE;
	CharData *target_mob;
	ObjData *target_obj;
	
	if (isdigit(*roomstr) && !strchr(roomstr, '.'))
		location = real_room(atoi(roomstr));
	else if ((target_mob = get_char_by_obj(obj, roomstr)))
		location = IN_ROOM(target_mob);
	else if ((target_obj = get_obj_by_obj(obj, roomstr)))
		location = IN_ROOM(target_obj);

	return location;
}

SCMD(func_asound) {
	RNum 	source_room;
	int  	door;

	if (!*argument) {
		script_log("asound called with no argument");
		return;
	}

	skip_spaces(argument);
	
	source_room = this_room(go, type);

	for (door = 0; door < NUM_OF_DIRS; door++) {
		struct RoomDirection *exit;

		if ((exit = EXITN(source_room, door)) &&
				exit->to_room != NOWHERE && exit->to_room != source_room) {
			world[source_room].Send(argument);
		}
	}
}


SCMD(func_echo) {
	RNum	room;

	skip_spaces(argument);

	if (!*argument) {
		script_log("echo called with no args");
		return;
	}
	
	room = this_room(go, type);

	if (room != NOWHERE) {
//		if (world[room].people.Count())
			world[room].Send(argument);
//			act(argument, TRUE, world[room].people.Top(), 0, 0, TO_ROOM | TO_CHAR);
	} else
		script_log("echo called from NOWHERE");
}


SCMD(func_send) {
	char *msg,
		*buf = get_buffer(MAX_INPUT_LENGTH);
	CharData *ch;

	msg = any_one_arg(argument, buf);

	if (!*buf)
		script_log("send called with no args");
	else {
		skip_spaces(msg);

		if (!*msg)
			script_log("send called without a message");
		else {
			ch = GET_TARGET_CHAR(buf);

			if (ch) {
				act(msg, TRUE, ch, 0, 0, (subcmd == SCMD_SEND) ? TO_CHAR : TO_ROOM);
			} else
				script_log((subcmd == SCMD_SEND) ? "no target found for send" : "no target found for echoaround");
		}
	}
	release_buffer(buf);
}


SCMD(func_zoneecho) {
	int zone;
	char *msg,
		*zone_name = get_buffer(MAX_INPUT_LENGTH),
		*buf = get_buffer(MAX_INPUT_LENGTH);

	msg = any_one_arg(argument, zone_name);
	skip_spaces(msg);

	if (!*zone_name || !*msg)
		script_log("zoneecho called with too few args");
	else if ((zone = real_zone(atoi(zone_name) * 100)) < 0)
		script_log("zoneecho called for nonexistant zone");
	else {
		sprintf(buf, "%s\r\n", msg);
		send_to_zone(buf, zone);
//		act(buf, TRUE, 0, 0, 0, TO_ZONE);
	}
	release_buffer(zone_name);
	release_buffer(buf);
}


SCMD(func_teleport) {
	CharData *ch;
	RNum target, room = NOWHERE;
	char *arg1 = get_buffer(MAX_INPUT_LENGTH),
		*arg2 = get_buffer(MAX_INPUT_LENGTH);

	two_arguments(argument, arg1, arg2);

	if (!*arg1 || !*arg2)
		script_log("teleport called with too few args");
	else if ((target = GET_TARGET_ROOM(arg2)) == NOWHERE)
		script_log("teleport target is an invalid room");
	else if (!str_cmp(arg1, "all")) {
		room = this_room(go, type);
		
		if (room == NOWHERE)	script_log("teleport called from NOWHERE");
		if (target == room)		script_log("teleport target is itself");

		START_ITER(iter, ch, CharData *, world[room].people) {
			ch->from_room();
			ch->to_room(target);
		} END_ITER(iter);
	} else if ((ch = GET_TARGET_CHAR(arg1))) {
		ch->from_room();
		ch->to_room(target);
	} else
		script_log("teleport: no target found");
	release_buffer(arg1);
	release_buffer(arg2);
}


SCMD(func_door) {
	char *value;
	RoomData *rm;
	struct RoomDirection *exit;
	int dir, fd, to_room;
	char 	*target = get_buffer(MAX_INPUT_LENGTH),
			*direction = get_buffer(MAX_INPUT_LENGTH),
			*field = get_buffer(MAX_INPUT_LENGTH);
//			*buf;
	
	char *door_field[] = {
	"purge",
	"description",
	"flags",
	"key",
	"name",
	"room",
	"\n"
	};
	
	
	
	argument = two_arguments(argument, target, direction);
	value = one_argument(argument, field);
	skip_spaces(value);
	
	if (!*target || !*direction || !*field)
		script_log("door called with too few args");
	else if ((rm = get_room(target)) == NULL)
		script_log("door: invalid target");
	else if ((dir = search_block(direction, dirs, FALSE)) == -1)
		script_log("door: invalid direction");
	else if ((fd = search_block(field, door_field, FALSE)) == -1)
		script_log("door: invalid field");
	else {
		exit = rm->dir_option[dir];

		/* purge exit */
		if (fd == 0) {
			if (exit) {
//				if (exit->general_description)	free(exit->general_description);
//				if (exit->keyword)				free(exit->keyword);
//				SSFree(exit->general_description);
//				SSFree(exit->keyword);
//				free(exit);
				delete exit;
				rm->dir_option[dir] = NULL;
			}
		} else {
			if (!exit) {
				exit = new RoomDirection;
				rm->dir_option[dir] = exit;
			}

			switch (fd) {
				case 1:  /* description */
//					if (exit->general_description)	free(exit->general_description);
//					SSFree(exit->general_description);
//					buf = get_buffer(MAX_STRING_LENGTH);
//					strcpy(buf, value);
//					strcat(buf, "\r\n");
//					exit->general_description = SSCreate(buf);
//					release_buffer(buf);
//					CREATE(exit->general_description, char, strlen(value) + 3);
//					strcpy(exit->general_description, value);
//					strcat(exit->general_description, "\r\n");
					break;
				case 2:  /* flags       */
					exit->exit_info = asciiflag_conv(value);
					break;
				case 3:  /* key         */
					exit->key = atoi(value);
					break;
				case 4:  /* name        */
//					if (exit->keyword)				free(exit->keyword);
//					exit->keyword = str_dup(value);
//					SSFree(exit->keyword);
//					exit->keyword = SSCreate(value);
					break;
				case 5:  /* room        */
					if ((to_room = real_room(atoi(value))) != NOWHERE)
						exit->to_room = to_room;
					else
						script_log("door: invalid door target");
					break;
			}
		}
	}
	release_buffer(target);
	release_buffer(direction);
	release_buffer(field);
}


SCMD(func_force) {
	CharData *ch;
	char *line, *arg1;
	RNum room;

	room = this_room(go, type);
	
	if (room == NOWHERE) {
		script_log("force called from NOWHERE");
		return;
	}
	
	arg1 = get_buffer(MAX_INPUT_LENGTH);
	line = one_argument(argument, arg1);
	
	if (!*arg1 || !*line) {
		script_log("force called with too few args");
	} else if (!str_cmp(arg1, "all")) {
		START_ITER(iter, ch, CharData *, world[room].people) {
			if (!IS_STAFF(ch))	command_interpreter(ch, line);
		} END_ITER(iter);
	} else if ((ch = GET_TARGET_CHAR(arg1))) {
		if (!IS_STAFF(ch))		command_interpreter(ch, line);
	} else
		script_log("force: no target found");
	release_buffer(arg1);
}


//	Reward mission points
//	NEEDS MODIFICATION: subtract
SCMD(func_mp) {
	CharData *ch;
	char *name = get_buffer(MAX_INPUT_LENGTH),
		*amount = get_buffer(MAX_INPUT_LENGTH);

	two_arguments(argument, name, amount);

	if (!*name || !*amount)
		script_log("mp: too few arguments");
	else if ((ch = GET_TARGET_CHAR(name))) 
		GET_MISSION_POINTS(ch) += atoi(amount);
	else
		script_log("mp: target not found");
	
	release_buffer(name);
	release_buffer(amount);
}

EVENT(combat_event);

SCMD(func_kill) {
	char *arg;
	CharData *victim;

	if (type != MOB_TRIGGER) {
		return;
	}

	arg = get_buffer(MAX_INPUT_LENGTH);

	one_argument(argument, arg);

	if (!*arg)
		script_log("kill called with no argument");
	else if (!(victim = get_char_room_vis((CharData *)go, arg)))
		script_log("kill: victim no in room");
	else if (victim == (CharData *)go)
		script_log("kill: victim is self");
	else if (AFF_FLAGGED((CharData *)go, AFF_CHARM) && ((CharData *)go)->master == victim )
		script_log("kill: charmed mob attacking master");
	else if (FIGHTING((CharData *)go))
		script_log("kill: already fighting");
	else
		combat_event((CharData *)go, victim, 0);
	release_buffer(arg);
}


SCMD(func_junk) {
	char *arg;
	int pos;
	ObjData *obj;

	if (type != MOB_TRIGGER) {
		return;
	}

	arg = get_buffer(MAX_INPUT_LENGTH);
	one_argument(argument, arg);

	if (!*arg)
		script_log("mjunk called with no argument");
	else if (find_all_dots(arg) != FIND_INDIV) {
		if ((obj=get_object_in_equip_vis((CharData *)go,arg,((CharData *)go)->equipment,&pos))!= NULL) {
			unequip_char((CharData *)go, pos);
			obj->extract();
		} else if ((obj = get_obj_in_list_vis((CharData *)go, arg, ((CharData *)go)->carrying)) != NULL )
			obj->extract();
	} else {
		START_ITER(iter, obj, ObjData *, ((CharData *)go)->carrying) {
			if (arg[3] == '\0' || isname(arg+4, SSData(obj->name)))
				obj->extract();
		} END_ITER(iter);
		while ((obj=get_object_in_equip_vis((CharData *)go, arg, ((CharData *)go)->equipment, &pos))) {
			unequip_char((CharData *)go, pos);
			obj->extract();
		}   
	}
	release_buffer(arg);
}


SCMD(func_load) {
	char *arg1, *arg2;
	int 		number = 0;
	CharData *	mob;
	ObjData *	object;
	RNum		room;

	room = this_room(go, type);

	arg1 = get_buffer(MAX_INPUT_LENGTH);
	arg2 = get_buffer(MAX_INPUT_LENGTH);

	two_arguments(argument, arg1, arg2);

	if (!*arg1 || !*arg2 || !is_number(arg2) || ((number = atoi(arg2)) < 0))
		script_log("load: bad syntax");
	else if (is_abbrev(arg1, "mob")) {
		if ((mob = read_mobile(number, VIRTUAL)) == NULL)
			script_log("load: bad mob vnum");
		else
			mob->to_room(room);
	} else if (is_abbrev(arg1, "obj")) {
		if ((object = read_object(number, VIRTUAL)) == NULL)
			script_log("load: bad object vnum");
		else if (CAN_WEAR(object, ITEM_WEAR_TAKE) && (type == MOB_TRIGGER))
			object->to_char((CharData *)go);
		else
			object->to_room(room);
	} else
		script_log("load: bad type");
	release_buffer(arg1);
	release_buffer(arg2);
}


/* purge all objects an npcs in room, or specified object or mob */
SCMD(func_purge) {
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	CharData *ch;
	ObjData *obj;
	RNum	room;

	one_argument(argument, arg);

	if (!*arg) {
		room = this_room(go, type);
		
		START_ITER(ch_iter, ch, CharData *, world[room].people) {
			if (IS_NPC(ch) && (ch != (CharData *)go))
				ch->extract();
		} END_ITER(ch_iter);
		START_ITER(obj_iter, obj, ObjData *, world[room].contents) {
			if (obj != (ObjData *)go)
				obj->extract();
		} END_ITER(obj_iter);
	} else if ((ch = GET_TARGET_CHAR(arg))) {
		if (!IS_NPC(ch))
			script_log("purge: purging a PC");
		else
			ch->extract();
	} else if ((obj = GET_TARGET_OBJ(arg)))
		obj->extract();
	else
		script_log("purge: bad argument");
	release_buffer(arg);
}


SCMD(func_at) {
	char *location = get_buffer(MAX_INPUT_LENGTH),
		*arg2 = get_buffer(MAX_INPUT_LENGTH);
	RNum	rnum = 0, old_room;

	half_chop(argument, location, arg2);

	if (!*location || !*arg2 || (!isdigit(*location) && !(*location == UID_CHAR)))
		script_log("at: bad syntax");
	else {
		if (*location == UID_CHAR) {
			CharData *ch;
			ObjData *obj;
			SInt32	id = atoi(location+1);
			
			if (id >= ROOM_ID_BASE && id < 100000)	rnum = id - ROOM_ID_BASE;
			else if ((ch = find_char(id)))			rnum = IN_ROOM(ch);
			else if ((obj = find_obj(id)))			rnum = IN_ROOM(obj);
			else									script_log("at: id not found");
		} else
			rnum = GET_TARGET_ROOM(location);
			
		if (rnum == NOWHERE)
			script_log("at: location not found");
		else {
			switch (type) {
				case MOB_TRIGGER:
					old_room = IN_ROOM((CharData *)go);
					((CharData *)go)->from_room();
					((CharData *)go)->to_room(rnum);
					script_command_interpreter(go, arg2, type);
					if (IN_ROOM(((CharData *)go)) == rnum) {
						((CharData *)go)->from_room();
						((CharData *)go)->to_room(old_room);
					}
					break;
				case OBJ_TRIGGER:
					old_room = IN_ROOM((ObjData *)go);
					IN_ROOM((ObjData *)go) = rnum;
					script_command_interpreter(go, arg2, type);
					IN_ROOM((ObjData *)go) = old_room;
					break;
				case WLD_TRIGGER:
					script_command_interpreter((Ptr)&world[rnum], arg2, type);
					break;
			}
		}
	}
	release_buffer(location);
	release_buffer(arg2);
}


SCMD(func_damage) {
	CharData *victim;
	char *name, *amount;
	RNum room;
	
	name = get_buffer(MAX_INPUT_LENGTH);
	amount = get_buffer(MAX_INPUT_LENGTH);
	
	two_arguments(argument, name, amount);

	if (!*name || !*amount)
		script_log("damage: too few arguments");
	else if (!is_number(amount))
		script_log("damage: bad syntax");
	else if (!str_cmp(name, "all")) {
		room = this_room(go, type);
		if ((room != NOWHERE) && !ROOM_FLAGGED(room, ROOM_PEACEFUL)) {
			START_ITER(iter, victim, CharData *, world[room].people) {
				if (victim == (CharData *)go)
					continue;
				if (NO_STAFF_HASSLE(victim))
					send_to_char("Being the cool staff member you are, you sidestep a trap, obviously placed to kill you.", victim);
				else
					damage(((type == MOB_TRIGGER) ? (CharData *)go : NULL), victim, atoi(amount), TYPE_MISC, 1);
			} END_ITER(iter);
		}
	} else if ((victim = GET_TARGET_CHAR(name))) {
		if (NO_STAFF_HASSLE(victim))
			send_to_char("Being the cool staff member you are, you sidestep a trap, obviously placed to kill you.", victim);
		else
			damage(((type == MOB_TRIGGER) ? (CharData *)go : NULL), victim, atoi(amount), TYPE_MISC, 1);
	} else
		script_log("damage: target not found");
	release_buffer(name);
	release_buffer(amount);
}


SCMD(func_heal) {
	CharData *victim;
	char *name, *amount;
	RNum room;
	
	name = get_buffer(MAX_INPUT_LENGTH);
	amount = get_buffer(MAX_INPUT_LENGTH);
	
	two_arguments(argument, name, amount);

	if (!*name || !*amount)
		script_log("heal: too few arguments");
	else if (!is_number(amount))
		script_log("heal: bad syntax");
	else if (!str_cmp(name, "all")) {
		room = this_room(go, type);
		START_ITER(iter, victim, CharData *, world[room].people) {
			if (NO_STAFF_HASSLE(victim))
				send_to_char("Being the cool staff member you are, you don't need to be healed!", victim);
			else
				AlterHit(victim, -atoi(amount));
		} END_ITER(iter);
	} else if ((victim = GET_TARGET_CHAR(name))) {
		if (NO_STAFF_HASSLE(victim))
			send_to_char("Being the cool staff member you are, you don't need to be healed!", victim);
		else
			AlterHit(victim, -atoi(amount));
	} else
		script_log("heal: target not found");
	release_buffer(name);
	release_buffer(amount);
}


SCMD(func_remove) {
	CharData *victim;
	ObjData *the_obj;
	int		x;
	bool	fAll = false;
	VNum	vnum = 0;
	char *vict = get_buffer(MAX_INPUT_LENGTH),
		 *what = get_buffer(MAX_INPUT_LENGTH);
	
	two_arguments(argument, vict, what);
	
	if (!*vict || !*what || (!is_number(what) && str_cmp(what, "all")))
		script_log("remove: bad syntax");
	else if ((victim = GET_TARGET_CHAR(vict))) {
		if (!str_cmp(what, "all"))	fAll = true;
		else 						vnum = atoi(what);
		START_ITER(iter, the_obj, ObjData *, victim->carrying) {
			if (fAll ? !NO_LOSE(the_obj) : (GET_OBJ_VNUM(the_obj) == vnum))
				the_obj->extract();
		} END_ITER(iter);
		for (x = 0; x < NUM_WEARS; x++) {
			the_obj = GET_EQ(victim, x);
			if (the_obj && (fAll ? !NO_LOSE(the_obj) : (GET_OBJ_VNUM(the_obj) == vnum)))
				the_obj->extract();
		}
	}
	release_buffer(vict);
	release_buffer(what);
}


/* lets the mobile goto any location it wishes that is not private */
SCMD(func_goto) {
	char *arg;
	RNum location;

	if (type != MOB_TRIGGER) {
		return;
	}

	arg = get_buffer(MAX_INPUT_LENGTH);

	one_argument(argument, arg);

	if (!*arg)
		script_log("goto called with no argument");
	else if ((location = find_target_room_by_char((CharData *)go, arg)) == NOWHERE)
		script_log("goto: invalid location");
	else {
		if (FIGHTING((CharData *)go))
			((CharData *)go)->stop_fighting();

		if (IN_ROOM((CharData *)go) != NOWHERE)
			((CharData *)go)->from_room();
		((CharData *)go)->to_room(location);
	}
	release_buffer(arg);
}


SCMD(func_info) {
	ACMD(do_gen_comm);
	void announce(char *info);
	char *buf;
		
	skip_spaces(argument);
	
	if (!*argument)
		script_log("info called with no argument");
//	else if (type == MOB_TRIGGER)	do_gen_comm((CharData *)go, argument, 0, "broadcast", SCMD_BROADCAST);
	else {
		buf = get_buffer(MAX_STRING_LENGTH);
		sprintf(buf, "`y[`bINFO`y]`n %s", argument);
		announce(buf);
		release_buffer(buf);
	}
}


SCMD(func_copy) {
	char *arg1, *arg2;
	int 		number = 0;
	CharData *	mob;
	ObjData *	obj;
	RNum		room;

	room = this_room(go, type);

	arg1 = get_buffer(MAX_INPUT_LENGTH);
	arg2 = get_buffer(MAX_INPUT_LENGTH);

	two_arguments(argument, arg1, arg2);

	if (!*arg1 || !*arg2)
		script_log("copy: bad syntax");
	else if (is_abbrev(arg1, "mob")) {
		if (!(mob = GET_TARGET_CHAR(arg2)))
			script_log("copy: mob not found");
		else if (!(mob = new CharData(mob)))
			script_log("copy: error in \"new CharData(mob)\"");
		else
			mob->to_room(room);
	} else if (is_abbrev(arg1, "obj")) {
		if (!(obj = GET_TARGET_OBJ(arg2)))
			script_log("copy: mob not found");
		else if (!(obj = new ObjData(obj)))
			script_log("copy: error in \"new ObjData(obj)\"");
		else if (CAN_WEAR(obj, ITEM_WEAR_TAKE) && (type == MOB_TRIGGER))
			obj->to_char((CharData *)go);
		else
			obj->to_room(room);
	} else
		script_log("copy: bad type");
	release_buffer(arg1);
	release_buffer(arg2);
}


struct script_command_info {
	char *command;
	void (*command_pointer)	(Ptr go, int type, char *argument, int subcmd);
	int	subcmd;
};


const struct script_command_info script_cmd_info[] = {
	{ "RESERVED", 0, 0 },/* this must be first -- for specprocs */

	{ "asound"		, func_asound	, 0 },
	{ "at"			, func_at		, 0 },
	{ "copy"		, func_copy		, 0 },
	{ "damage"		, func_damage	, 0 },
	{ "door"		, func_door		, 0 },
	{ "echo"		, func_echo		, 0 },
	{ "echoaround"	, func_send		, SCMD_ECHOAROUND },
	{ "force"		, func_force	, 0 },
	{ "goto"		, func_goto		, 0 },
	{ "heal"		, func_heal		, 0 },
	{ "info"		, func_info		, 0 },
	{ "junk"		, func_junk		, 0 },
	{ "kill"		, func_kill		, 0 },
	{ "load"		, func_load		, 0 },
	{ "mp"			, func_mp		, 0 },
	{ "purge"		, func_purge	, 0 },
	{ "remove"		, func_remove	, 0 },
	{ "send"		, func_send		, SCMD_SEND },
	{ "teleport"	, func_teleport	, 0 },
	{ "zoneecho"	, func_zoneecho	, 0 },

	{ "\n", 0, 0 }	/* this must be last */
};


/*
 *  This is the command interpreter used by rooms, called by script_driver.
 */
void script_command_interpreter(Ptr go, char *argument, int type) {
	int cmd, length;
	char *line, *arg;

	skip_spaces(argument);

	/* just drop to next line for hitting CR */
	if (!*argument)
		return;

	arg = get_buffer(MAX_INPUT_LENGTH);

	line = any_one_arg(argument, arg);

	/* find the command */
	for (length = strlen(arg), cmd = 0; *script_cmd_info[cmd].command != '\n'; cmd++)
		if (!strncmp(script_cmd_info[cmd].command, arg, length))
			break;

	if (*script_cmd_info[cmd].command == '\n') {
		if (type == MOB_TRIGGER) {
			command_interpreter((CharData *)go, argument);
		} else {
			sprintf(arg, "Unknown script cmd: '%s'", argument);
			script_log(arg);
		}
	} else
		((*script_cmd_info[cmd].command_pointer) (go, type, line, script_cmd_info[cmd].subcmd));
	release_buffer(arg);
}
