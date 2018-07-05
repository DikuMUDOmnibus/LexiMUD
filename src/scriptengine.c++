/**************************************************************************
*  File: scripts.c                                                        *
*  Usage: contains general functions for using scripts.                   *
**************************************************************************/

 
#include "structs.h"
#include "scripts.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "find.h"
#include "handler.h"
#include "event.h"
#include "olc.h"
#include "clans.h"
#include "buffer.h"


//	External Functions
char *scriptedit_get_class(TrigData *trig);
RNum find_target_room(CharData * ch, char *rawroomstr);
int is_empty(int zone_nr);
void free_varlist(LList<TrigVarData *> &vd);

//	External Variables
extern char *mtrig_types[];
extern char *otrig_types[];
extern char *wtrig_types[];
extern char *item_types[];
extern char *genders[];
extern char *race_types[];
extern char *exit_bits[];
extern char *relations[];


/* function protos from this file */
int script_driver(Ptr go, TrigData *trig, int type, int mode);
SInt32 trgvar_in_room(VNum vnum);

void var_subst(Ptr go, ScriptData *sc, TrigData *trig,
		int type, char *line, char *buf);
void eval_op(char *op, char *lhs, char *rhs, char *result, Ptr go,
		ScriptData *sc, TrigData *trig);
char *matching_quote(char *p);
char *matching_paren(char *p);
void eval_expr(char *line, char *result, Ptr go, ScriptData *sc,
		TrigData *trig, int type);
int eval_lhs_op_rhs(char *expr, char *result, Ptr go, ScriptData *sc,
		TrigData *trig, int type);
int process_if(char *cond, Ptr go, ScriptData *sc,
		TrigData *trig, int type);
CmdlistElement *find_end(CmdlistElement *cl);
CmdlistElement *find_else_end(TrigData *trig, CmdlistElement *cl,
		Ptr go, ScriptData *sc, int type);
CmdlistElement *find_case(TrigData *trig, CmdlistElement *cl,
		Ptr go, ScriptData *sc, int type, char *cond);
CmdlistElement *find_done(CmdlistElement *cl);
void process_wait(Ptr go, TrigData *trig, int type, char *cmd, CmdlistElement *cl);
void process_set(ScriptData *sc, TrigData *trig, char *cmd);
void process_eval(Ptr go, ScriptData *sc, TrigData *trig, int type, char *cmd);
int process_return(TrigData *trig, char *cmd);
void process_unset(ScriptData *sc, TrigData *trig, char *cmd);
void process_global(ScriptData *sc, TrigData *trig, char *cmd);


EVENTFUNC(trig_wait_event);
void find_uid_name(char *uid, char *name);
void script_stat (CharData *ch, ScriptData *sc);
ACMD(do_attach);
int remove_trigger(ScriptData *sc, char *name);
ACMD(do_detach);

void add_var(LList<TrigVarData *> &var_list, char *name, char *value);
int remove_var(LList<TrigVarData *> &var_list, char *name);
void find_replacement(Ptr go, ScriptData *sc, TrigData *trig, int type, char *var, char *the_field, char *str);


/* local structures */
struct wait_event_data {
	TrigData *	trigger;
	Ptr			go;
	UInt32		type;
};


SInt32 trgvar_in_room(VNum vnum) {
	RNum	rnum;
	if ((rnum = real_room(vnum)) == NOWHERE) {
		mudlogf(NRM, LVL_STAFF, FALSE, "SCRIPTERR: people.vnum: world[vnum] does not exist");
		return (-1);
	}

	return world[rnum].people.Count();;
}


/* checks every PLUSE_SCRIPT for random triggers */
void script_trigger_check(void) {
	CharData *ch;
	ObjData *obj;
	RoomData *room;
	int nr;
	ScriptData *sc;


	START_ITER(ch_iter, ch, CharData *, character_list) {
		if ((sc = SCRIPT(ch))) {
			if (IS_SET(SCRIPT_TYPES(sc), WTRIG_RANDOM) &&
					(!is_empty(world[IN_ROOM(ch)].zone) || IS_SET(SCRIPT_TYPES(sc), WTRIG_GLOBAL)))
				random_mtrigger(ch);
		}
	} END_ITER(ch_iter);
  
	START_ITER(obj_iter, obj, ObjData *, object_list) {
		if ((sc = SCRIPT(obj))) {
			if (IS_SET(SCRIPT_TYPES(sc), OTRIG_RANDOM) && (((nr = obj->Room()) == -1) ||
					!is_empty(world[nr].zone) || IS_SET(SCRIPT_TYPES(sc), WTRIG_GLOBAL)))
				random_otrigger(obj);
		}
	} END_ITER(obj_iter);

	for (nr = 0; nr <= top_of_world; nr++) {
		room = &world[nr];
		if ((sc = SCRIPT(room))) {
			if (IS_SET(SCRIPT_TYPES(sc), WTRIG_RANDOM) &&
					(!is_empty(room->zone) || IS_SET(SCRIPT_TYPES(sc), WTRIG_GLOBAL)))
				random_wtrigger(room);
		}
	}
}


EVENTFUNC(trig_wait_event) {
	struct wait_event_data *wait_event_obj = (struct wait_event_data *)event_obj;
	TrigData *	trig;
	Ptr			go;
	UInt32		type;

	trig = wait_event_obj->trigger;
	go = wait_event_obj->go;
	type = wait_event_obj->type;
	
	if (type == WLD_TRIGGER) {
		go = &world[real_room((RNum)go)];
	}
	
	GET_TRIG_WAIT(trig) = NULL;
	
	script_driver(go, trig, type, TRIG_RESTART);
	return 0;
}


void do_stat_trigger(CharData *ch, TrigData *trig) {
	CmdlistElement *cmd_list;
	char *string, *buf, *bitBuf, **trig_types;
	
	if (!trig) {
		log("SYSERR: NULL trigger passed to do_stat_trigger.");
		return;
	}
	
	string = get_buffer(MAX_STRING_LENGTH);
	buf = get_buffer(MAX_STRING_LENGTH);
	bitBuf = get_buffer(MAX_STRING_LENGTH);

	sprintf(string, "Name: '`y%s`n',  VNum: [`g%5d`n], RNum: [%5d]\r\n",
			SSData(GET_TRIG_NAME(trig)), GET_TRIG_VNUM(trig), GET_TRIG_RNUM(trig));

	switch (GET_TRIG_DATA_TYPE(trig)) {
		case WLD_TRIGGER:	trig_types = wtrig_types;	break;
		case OBJ_TRIGGER:	trig_types = otrig_types;	break;
		case MOB_TRIGGER:	trig_types = mtrig_types;	break;
		default:			trig_types = wtrig_types;	break;
	}

	sprintbit(GET_TRIG_TYPE(trig), trig_types, bitBuf);

	sprintf(buf,"Trigger Class: %s\r\n"
				"Trigger Type: %s, N Arg: %d, Arg list: %s\r\n",
			scriptedit_get_class(trig), bitBuf, GET_TRIG_NARG(trig),
			(SSData(GET_TRIG_ARG(trig)) ? SSData(GET_TRIG_ARG(trig)) : "None"));
	strcat(string, buf);
	strcat(string, "Commands:\r\n   ");
	
	release_buffer(buf);
	release_buffer(bitBuf);

//	for (cmd_list = trig->cmndlist; cmd_list; cmd_list = cmd_list->next) { ... }
	cmd_list = trig->cmdlist;
	while (cmd_list) {
		if (cmd_list->cmd) {
			strcat(string, cmd_list->cmd);
			strcat(string, "\r\n   ");
		}
		cmd_list = cmd_list->next;
	}

	page_string(ch->desc, string, 1);
	release_buffer(string);
}


/* find the name of what the uid points to */
void find_uid_name(char *uid, char *name) {
	CharData *ch;
	ObjData *obj;

	if ((ch = get_char(uid)))		strcpy(name, SSData(ch->general.name));
	else if ((obj = get_obj(uid)))	strcpy(name, SSData(obj->name));
	else							sprintf(name, "uid = %s, (not found)", uid + 1);
}


/* general function to display stats on script sc */
void script_stat (CharData *ch, ScriptData *sc) {
	TrigVarData *tv;
	TrigData *t;
	char **trig_types;
	char *name = get_buffer(MAX_INPUT_LENGTH),
		*bitBuf = get_buffer(MAX_INPUT_LENGTH);
		
	LListIterator<TrigVarData *>	var_iter(sc->global_vars);
	LListIterator<TrigData *>		trig_iter(TRIGGERS(sc));
	
	ch->Send("Global Variables: %s\r\n", sc->global_vars.Count() ? "" : "None");
	
	while ((tv = var_iter.Next())) {
		if (*(tv->value) == UID_CHAR) {
			find_uid_name(tv->value, name);
			ch->Send("    %15s:  %s\r\n", tv->name, name);
		} else 
			ch->Send("    %15s:  %s\r\n", tv->name, tv->value);
	}
	
	while ((t = trig_iter.Next())) {
		ch->Send("\r\n"
				 "  Trigger: `y%s`n, VNum: [`g%5d`n], RNum: [%5d]\r\n",
				SSData(GET_TRIG_NAME(t)), GET_TRIG_VNUM(t), GET_TRIG_RNUM(t));

		switch (GET_TRIG_DATA_TYPE(t)) {
			case WLD_TRIGGER:	trig_types = wtrig_types;	break;
			case OBJ_TRIGGER:	trig_types = otrig_types;	break;
			case MOB_TRIGGER:	trig_types = mtrig_types;	break;
			default:			trig_types = wtrig_types;	break;
		}
		sprintbit(GET_TRIG_TYPE(t), trig_types, bitBuf);

		ch->Send("Script Type: %s\r\n"
				 "  Trigger Type: %s, N Arg: %d, Arg list: %s\r\n", 
				scriptedit_get_class(t), bitBuf, GET_TRIG_NARG(t), 
				(SSData(GET_TRIG_ARG(t)) ? SSData(GET_TRIG_ARG(t)) : "None"));

		if (GET_TRIG_WAIT(t)) {
			ch->Send("    Wait: %d, Current line: %s\r\n",
			GET_TRIG_WAIT(t)->Time(), t->curr_state->cmd);

			ch->Send("  Variables: %s\r\n", GET_TRIG_VARS(t).Count() ? "" : "None");
			
			var_iter.Restart(GET_TRIG_VARS(t));
			while ((tv = var_iter.Next())) {
				if (*(tv->value) == UID_CHAR) {
					find_uid_name(tv->value, name);
					ch->Send("    %15s:  %s\r\n", tv->name, name);
				} else 
					ch->Send("    %15s:  %s\r\n", tv->name, tv->value);
			}
		}
	}
	release_buffer(bitBuf);
	release_buffer(name);
}


void do_sstat_room(CharData * ch) {
	RoomData *rm = &world[IN_ROOM(ch)];

	if (!SCRIPT(rm)) {
		act("This room does not have a script.", FALSE, ch, NULL, NULL, TO_CHAR);
		return;
	}

	ch->Send("Room name: `c%s`n\r\n"
			 "Zone: [%3d], VNum: [`g%5d`n]\r\n",
			 rm->GetName("<ERROR>"),
			 rm->zone, rm->number);

	script_stat(ch, SCRIPT(rm));
}


void do_sstat_object(CharData *ch, ObjData *j) {
	char *bitBuf;
	if (!SCRIPT(j)) {
		act("$p does not have a script.", FALSE, ch, j, NULL, TO_CHAR);
		return;
	}
	bitBuf = get_buffer(MAX_STRING_LENGTH);

	sprinttype(GET_OBJ_TYPE(j), item_types, bitBuf);
	ch->Send("Name: '`y%s`n', Aliases: %s\r\n"
			 "VNum: [`g%5d`n], Type: %s\r\n",
		(SSData(j->shortDesc) ? SSData(j->shortDesc) : "<None>"), SSData(j->name),
		GET_OBJ_VNUM(j), bitBuf);

	script_stat(ch, SCRIPT(j));
	release_buffer(bitBuf);
}


void do_sstat_character(CharData *ch, CharData *k) {
	if (!SCRIPT(k)) {
		act("$N does not have a script.", FALSE, ch, NULL, k, TO_CHAR);
		return;
	}
	
	ch->Send("Name: '`y%s`n', Alias: %s\r\n"
			 "VNum: [`g%5d`n], In room [%5d]\r\n",
			SSData(k->general.shortDesc), SSData(k->general.name),
			GET_MOB_VNUM(k), world[IN_ROOM(k)].number);
	
	script_stat(ch, SCRIPT(k));
}


/*
 * adds the trigger t to script sc in in location loc.  loc = -1 means
 * add to the end, loc = 0 means add before all other triggers.
 */
void add_trigger(ScriptData *sc, TrigData *t) /*, int loc) */ {
//	TrigData *i;
//	int n;

//	for (n = loc, i = TRIGGERS(sc); i && i->next && (n != 0); n--, i = i->next);

/*	if (!loc) {
		t->next = TRIGGERS(sc);
		TRIGGERS(sc) = t;
	} else if (!i)
		TRIGGERS(sc) = t;
	else {
		t->next = i->next;
		i->next = t;
	}
*/
	TRIGGERS(sc).Add(t);

	SCRIPT_TYPES(sc) |= GET_TRIG_TYPE(t);
}


ACMD(do_attach)  {
	CharData *victim;
	ObjData *object;
	TrigData *trig;
	char *targ_name, *trig_name, *loc_name, *arg;
	int loc, room, tn;

	trig_name = get_buffer(MAX_INPUT_LENGTH);
	targ_name = get_buffer(MAX_INPUT_LENGTH);
	loc_name = get_buffer(MAX_INPUT_LENGTH);
	arg = get_buffer(MAX_STRING_LENGTH);
	
	argument = two_arguments(argument, arg, trig_name);
	two_arguments(argument, targ_name, loc_name);

	if (!*arg || !*targ_name || !*trig_name)
		send_to_char("Usage: attach { mtr | otr | wtr } { trigger } { name } [ location ]\r\n", ch);
	else {  
		tn = atoi(trig_name);
		loc = (*loc_name) ? atoi(loc_name) : -1;
  
		if (is_abbrev(arg, "mtr")) {
			if ((victim = get_char_vis(ch, targ_name))) {
				if (IS_NPC(victim))  {

					/* have a valid mob, now get trigger */
					if ((trig = read_trigger(tn, VIRTUAL))) {

						if (!SCRIPT(victim))
							SCRIPT(victim) = new ScriptData;
						add_trigger(SCRIPT(victim), trig); //, loc);

						ch->Send("Trigger %d (%s) attached to %s.\r\n",
								tn, SSData(GET_TRIG_NAME(trig)), victim->RealName());
					} else
						send_to_char("That trigger does not exist.\r\n", ch);
				} else
					send_to_char("Players can't have scripts.\r\n", ch);
			} else
				send_to_char("That mob does not exist.\r\n", ch);
		} else if (is_abbrev(arg, "otr")) {
			if ((object = get_obj_vis(ch, targ_name))) {
				
				/* have a valid obj, now get trigger */
				if ((trig = read_trigger(tn, VIRTUAL))) {
	
					if (!SCRIPT(object))
						SCRIPT(object) = new ScriptData;
					add_trigger(SCRIPT(object), trig); //, loc);
	  
					ch->Send("Trigger %d (%s) attached to %s.\r\n", tn, SSData(GET_TRIG_NAME(trig)), 
							(object->shortDesc ? SSData(object->shortDesc) : SSData(object->name)));
				} else
					send_to_char("That trigger does not exist.\r\n", ch);
			} else
				send_to_char("That object does not exist.\r\n", ch);
		} else if (is_abbrev(arg, "wtr")) {
			if (isdigit(*targ_name) && !strchr(targ_name, '.')) {
				if ((room = find_target_room(ch, targ_name)) != NOWHERE) {
	
					/* have a valid room, now get trigger */
					if ((trig = read_trigger(tn, VIRTUAL))) {

						if (!(world[room].script))
							world[room].script = new ScriptData;
						add_trigger(world[room].script, trig); //, loc);
  
						ch->Send("Trigger %d (%s) attached to room %d.\r\n",
								tn, SSData(GET_TRIG_NAME(trig)), world[room].number);
					} else
						send_to_char("That trigger does not exist.\r\n", ch);
				}
	    	} else
				send_to_char("You need to supply a room number.\r\n", ch);
		} else
			send_to_char("Please specify 'mtr', otr', or 'wtr'.\r\n", ch);
    }
    release_buffer(targ_name);
    release_buffer(trig_name);
    release_buffer(loc_name);
    release_buffer(arg);
}


/*
 *  removes the trigger specified by name, and the script of o if
 *  it removes the last trigger.  name can either be a number, or
 *  a 'silly' name for the trigger, including things like 2.beggar-death.
 *  returns 0 if did not find the trigger, otherwise 1.  If it matters,
 *  you might need to check to see if all the triggers were removed after
 *  this function returns, in order to remove the script.
 */
int remove_trigger(ScriptData *sc, char *name) {
	TrigData *i;
	int num = 0, string = FALSE, n;
	char *cname;
  
	if (!sc)
		return 0;

	if ((cname = strstr(name,".")) || (!isdigit(*name)) ) {
		string = TRUE;
		if (cname) {
			*cname = '\0';
			num = atoi(name);
			name = ++cname;
		}
	} else
		num = atoi(name);
  
	n = 0;
	START_ITER(iter, i, TrigData *, TRIGGERS(sc)) {
		if (string) {
			if (silly_isname(name, SSData(GET_TRIG_NAME(i))))
				if (++n >= num)
					break;
		} else if (++n >= num)
			break;
	}

	if (i) {
//		if (j) {
//			j->next = i->next;
//			i->extract();
//		} else {	/* this was the first trigger */
//			TRIGGERS(sc) = i->next;
//			i->extract();
//		}
		TRIGGERS(sc).Remove(i);
		
		/* update the script type bitvector */
		SCRIPT_TYPES(sc) = 0;
//		for (i = TRIGGERS(sc); i; i = i->next)
		START_ITER(type_iter, i, TrigData *, TRIGGERS(sc)) {
			SCRIPT_TYPES(sc) |= GET_TRIG_TYPE(i);
		} END_ITER(type_iter);

		return 1;
	} else
		return 0;
}


ACMD(do_detach)  {
	CharData *victim = NULL;
	ObjData *object = NULL;
	RoomData *room;
	char *arg1, *arg2, *arg3;
	char *trigger = 0;
	int tmp;

	arg1 = get_buffer(MAX_INPUT_LENGTH);
	arg2 = get_buffer(MAX_INPUT_LENGTH);
	arg3 = get_buffer(MAX_INPUT_LENGTH);
	
	argument = two_arguments(argument, arg1, arg2);
	one_argument(argument, arg3);

	if (!*arg1 || !*arg2) {
		send_to_char("Usage: detach [ mob | object ] { target } { trigger | 'all' }\r\n", ch);
	} else {
		if (!str_cmp(arg1, "room")) {
			room = &world[IN_ROOM(ch)];
			if (!SCRIPT(room)) 
				send_to_char("This room does not have any triggers.\r\n", ch);
			else if (!str_cmp(arg2, "all")) {
				SCRIPT(room)->extract();
				SCRIPT(room) = NULL;
				send_to_char("All triggers removed from room.\r\n", ch);
			} else if (remove_trigger(SCRIPT(room), arg2)) {
				send_to_char("Trigger removed.\r\n", ch);
				if (!TRIGGERS(SCRIPT(room)).Top()) {
					SCRIPT(room)->extract();
					SCRIPT(room) = NULL;
				}
			} else
				send_to_char("That trigger was not found.\r\n", ch);
		} else {
			if (is_abbrev(arg1, "mob")) {
				if (!(victim = get_char_vis(ch, arg2)))
					send_to_char("No such mobile around.\r\n", ch);
				else if (!*arg3)
					send_to_char("You must specify a trigger to remove.\r\n", ch);
				else
					trigger = arg3;
			} else if (is_abbrev(arg1, "object")) {
				if (!(object = get_obj_vis(ch, arg2)))
					send_to_char("No such object around.\r\n", ch);
				else if (!*arg3)
					send_to_char("You must specify a trigger to remove.\r\n", ch);
				else
					trigger = arg3;
			} else  {
				if ((object = get_object_in_equip_vis(ch, arg1, ch->equipment, &tmp)));
				else if ((object = get_obj_in_list_vis(ch, arg1, ch->carrying)));
				else if ((victim = get_char_room_vis(ch, arg1)));
				else if ((object = get_obj_in_list_vis(ch, arg1, world[IN_ROOM(ch)].contents)));
				else if ((victim = get_char_vis(ch, arg1)));
				else if ((object = get_obj_vis(ch, arg1)));
				else send_to_char("Nothing around by that name.\r\n", ch);
				trigger = arg2;
			}

			if (victim) {
				if (!IS_NPC(victim))
					send_to_char("Players don't have triggers.\r\n", ch);
				else if (!SCRIPT(victim))
					send_to_char("That mob doesn't have any triggers.\r\n", ch);
				else if (!str_cmp(arg2, "all")) {
					SCRIPT(victim)->extract();
					SCRIPT(victim) = NULL;
					act("All triggers removed from $N.", TRUE, ch, 0, victim, TO_CHAR);
				} else if (remove_trigger(SCRIPT(victim), trigger)) {
					send_to_char("Trigger removed.\r\n", ch);
					if (!TRIGGERS(SCRIPT(victim)).Top()) {
						SCRIPT(victim)->extract();
						SCRIPT(victim) = NULL;
					}
				} else
					send_to_char("That trigger was not found.\r\n", ch);
			} else if (object) {
				if (!SCRIPT(object))
					send_to_char("That object doesn't have any triggers.\r\n", ch);
				else if (!str_cmp(arg2, "all")) {
					SCRIPT(object)->extract();
					SCRIPT(object) = NULL;
					act("All triggers removed from $p.", TRUE, ch, 0, object, TO_CHAR);
				} else if (remove_trigger(SCRIPT(object), trigger)) {
					send_to_char("Trigger removed.\r\n", ch);
					if (!TRIGGERS(SCRIPT(object)).Top()) {
						SCRIPT(object)->extract();
						SCRIPT(object) = NULL;
					}
				} else
					send_to_char("That trigger was not found.\r\n", ch);
			}
		}
	}
	release_buffer(arg1);
	release_buffer(arg2);
	release_buffer(arg3);
}


/* adds a variable with given name and value to trigger */
void add_var(LList<TrigVarData *> &var_list, char *name, char *value) {
	TrigVarData *vd;
	
	LListIterator<TrigVarData *>	iter(var_list);
	while ((vd = iter.Next())) {
		if (!str_cmp(vd->name, name))
			break;
	}
	
	if (vd) {
		if (vd->value)	FREE(vd->value);
		vd->value = str_dup(value);
	} else {
		vd = new TrigVarData;
		vd->name = str_dup(name);
		vd->value = str_dup(value);

		var_list.Add(vd);
	}
}


/*
 * remove var name from var_list
 * returns 1 if found, else 0
 */
int remove_var(LList<TrigVarData *> &var_list, char *name) {
	TrigVarData *i;

	LListIterator<TrigVarData *>	iter(var_list);
	while ((i = iter.Next())) {
		if (!str_cmp(name, i->name))
			break;
	}
	
	if (i) {
		var_list.Remove(i);
		delete i;
		return 1;
	}
	return 0;
}


/* sets str to be the value of var.field */
void find_replacement(Ptr go, ScriptData *sc, TrigData *trig, int type,
		char *var, char *the_field, char *str) {
	TrigVarData *vd;
	CharData *ch, *c = NULL, *rndm;
	ObjData *obj, *o = NULL;
	RoomData *room, *r = NULL;
	char *name;
	int num, count, i;
	char *field = get_buffer(MAX_STRING_LENGTH);
	
	strcpy(field, the_field);
	
	*str = '\0';
	
	LListIterator<TrigVarData *>	iter(GET_TRIG_VARS(trig));
	while ((vd = iter.Next()))
		if (!str_cmp(vd->name, var))
			break;

	if (!vd) {
		iter.Restart(sc->global_vars);
		while ((vd = iter.Next()))
			if (!str_cmp(vd->name, var))
				break;
	}
	
	if (!*field) {
		if (vd)								strcpy(str, vd->value);
		else if (!str_cmp(var, "self")) {
			switch (type) {
				case MOB_TRIGGER:	i = GET_ID((CharData *) go);	break;
				case OBJ_TRIGGER:	i = GET_ID((ObjData *) go);		break;
				case WLD_TRIGGER:	i = GET_ID((RoomData *) go);	break;
			}
			sprintf(str, "%c%d", UID_CHAR, i);
//			strcpy(str, "self");
		} else								*str = '\0';
	} else {
		if (vd) {

			name = vd->value;
			
			switch (type) {
				case MOB_TRIGGER:
					ch = (CharData *) go;
					i = 0;
					if ((o = get_object_in_equip(ch, name, ch->equipment, &i)));
					else if ((o = get_obj_in_list(name, ch->carrying)));
					else if ((c = get_char_room(name, IN_ROOM(ch))));
					else if ((o = get_obj_in_list(name, world[IN_ROOM(ch)].contents)));
					else if ((c = get_char(name)));
					else if ((o = get_obj(name)));
					else if ((r = get_room(name))) {}

					break;
				case OBJ_TRIGGER:
					obj = (ObjData *) go;

					if ((c = get_char_by_obj(obj, name)));
					else if ((o = get_obj_by_obj(obj, name)));
					else if ((r = get_room(name))) {}

					break;
				case WLD_TRIGGER:
					room = (RoomData *) go;

					if ((c = get_char_by_room(room, name)));
					else if ((o = get_obj_by_room(room, name)));
					else if ((r = get_room(name))) {}

					break;
			}
		} else {
			if (!str_cmp(var, "self")) {
				switch (type) {
					case MOB_TRIGGER:	c = (CharData *) go;	break;
					case OBJ_TRIGGER:	o = (ObjData *) go;		break;
					case WLD_TRIGGER:	r = (RoomData *) go;	break;
				}
			} else if (!str_cmp(var, "people")) {
				sprintf(str,"%d",((num = atoi(field)) > 0) ? trgvar_in_room(num) : 0);
				release_buffer(field);
				return;
			} else if (!str_cmp(var, "random")) {
				if (!str_cmp(field, "char")) {
					rndm = NULL;
					count = 0;

					if (type == MOB_TRIGGER) {
						ch = (CharData *) go;
						START_ITER(iter, c, CharData *, world[IN_ROOM(ch)].people) {
							if (!PRF_FLAGGED(c, PRF_NOHASSLE) && (c != ch) && ch->CanSee(c))
								if (!Number(0, count++))	rndm = c;
						} END_ITER(iter);
					} else if (type == OBJ_TRIGGER) {
						obj = (ObjData *) go;
						START_ITER(iter, c, CharData *, world[obj->Room()].people) {
							if (!PRF_FLAGGED(c, PRF_NOHASSLE) && !GET_INVIS_LEV(c))
								if (!Number(0, count++))	rndm = c;
						} END_ITER(iter);
					} else if (type == WLD_TRIGGER) {
						room = (RoomData *) go;
						START_ITER(iter, c, CharData *, room->people) {
							if (!PRF_FLAGGED(c, PRF_NOHASSLE) && !GET_INVIS_LEV(c))
								if (!Number(0, count++))	rndm = c;
						} END_ITER(iter);
					}
					if (rndm)	sprintf(str, "%c%d", UID_CHAR, GET_ID(rndm));
					else		*str = '\0';
				} else
					sprintf(str, "%d", ((num = atoi(field)) > 0) ? Number(1, num) : 0);
				release_buffer(field);
				return;
			}
		}

		if (vd && (num = atoi(field))) {
			// GET WORD - recycle field buffer!
			name = vd->value;
			for (count = 0; (count < num) && (*name); count++)
				name = one_argument(name, field);
			if (count == num)	strcpy(str, field);
		} else if (c) {
			if (!str_cmp(field, "name"))		strcpy(str, c->GetName());
			else if (!str_cmp(field, "alias"))	strcpy(str, SSData(c->general.name));
			else if (!str_cmp(field, "level"))	sprintf(str, "%d", GET_LEVEL(c));
			else if (!str_cmp(field, "mp"))		sprintf(str, "%d", GET_MISSION_POINTS(c));
			else if (!str_cmp(field, "sex"))	strcpy(str, genders[(int)GET_SEX(c)]);
			else if (!str_cmp(field, "canbeseen"))
				strcpy(str, ((type == MOB_TRIGGER) && !((CharData *) go)->CanSee(c)) ? "0" : "1");
//			else if (!str_cmp(field, "class"))	sprinttype(GET_CLASS(c), class_types, str);
			else if (!str_cmp(field, "race"))	sprinttype(GET_RACE(c), race_types, str);
			else if (!str_cmp(field, "vnum"))	sprintf(str, "%d", GET_MOB_VNUM(c));
			else if (!str_cmp(field, "str"))	sprintf(str, "%d", GET_STR(c));
			else if (!str_cmp(field, "per"))	sprintf(str, "%d", GET_PER(c));
			else if (!str_cmp(field, "int"))	sprintf(str, "%d", GET_INT(c));
			else if (!str_cmp(field, "dex"))	sprintf(str, "%d", GET_AGI(c));
			else if (!str_cmp(field, "con"))	sprintf(str, "%d", GET_CON(c));
			else if (!str_cmp(field, "room"))	sprintf(str, "%d", world[IN_ROOM(c)].number);
			else if (!str_cmp(field, "hp"))		sprintf(str, "%d", GET_HIT(c));
			else if (!str_cmp(field, "hitpercent"))	sprintf(str, "%d", (GET_HIT(c) / GET_MAX_HIT(c)) * 100);
			else if (!str_cmp(field, "npc"))	strcpy(str, IS_NPC(c) ? "1" : "0");
			else if (!str_cmp(field, "clan")) {
				if (GET_CLAN_LEVEL(c) > CLAN_APPLY)
					sprintf(str, "%d", GET_CLAN(c));
			} else if (!str_cmp(field, "varclass"))	strcpy(str, "character");
			else if (!str_cmp(field, "staff"))	strcpy(str, NO_STAFF_HASSLE(c) ? "1" : "0");
			else if (!str_cmp(field, "relation"))
				strcpy(str, (type == MOB_TRIGGER) ? relations[c->GetRelation((CharData *)go)] : "");
			else if (!str_cmp(field, "friend"))
				strcpy(str, ((type == MOB_TRIGGER) && (c->GetRelation((CharData *)go) == RELATION_FRIEND)) ? "1" : "0");
			else if (!str_cmp(field, "neutral"))
				strcpy(str, ((type == MOB_TRIGGER) && (c->GetRelation((CharData *)go) == RELATION_NEUTRAL)) ? "1" : "0");
			else if (!str_cmp(field, "enemy"))
				strcpy(str, ((type == MOB_TRIGGER) && (c->GetRelation((CharData *)go) == RELATION_ENEMY)) ? "1" : "0");
			else if (!str_cmp(field, "traitor"))	strcpy(str, IS_TRAITOR(c) ? "1" : "0");
			else if (!str_cmp(field, "weight"))	sprintf(str, "%d", GET_WEIGHT(c));
			else if (!str_cmp(field, "height"))	sprintf(str, "%d", GET_HEIGHT(c));
			else if (!str_cmp(field, "age"))	sprintf(str, "%d", GET_AGE(c));
			else if (!str_cmp(field, "master")) {
				if (!c->master)			strcpy(str, "");
				else					sprintf(str, "%c%d", UID_CHAR, GET_ID(c->master));
			}
			// DISABLED these for now for "safety" reasons
/*			else if (!str_cmp(field, "nextinroom")) {
				if (!c->next_in_room)	strcpy(str, "");
				else					sprintf(str, "%c%d", UID_CHAR, GET_ID(c->next_in_room));
			} else if (!str_cmp(field, "nextinworld")) {
				if (!c->next)			strcpy(str, "");
				else					sprintf(str, "%c%d", UID_CHAR, GET_ID(c->next));
			}
*/
			else if(!str_cmp(field, "hisher"))	strcpy(str, HSHR(c));
			else if(!str_cmp(field, "heshe"))	strcpy(str, HSHR(c));
			else if(!str_cmp(field, "hmhr"))	strcpy(str, HSHR(c));
			else if(!str_cmp(field, "fighting")) {
				if (!FIGHTING(c))		strcpy(str, "");
				else					sprintf(str, "%c%d", UID_CHAR, GET_ID(FIGHTING(c)));
			} else
				mudlogf(NRM, LVL_STAFF, FALSE, "SCRIPTERR: Trigger: %s, VNum %d. unknown char field: '%s'",
						SSData(GET_TRIG_NAME(trig)), GET_TRIG_VNUM(trig), field);
		} else if (o) {
			if (!str_cmp(field, "name"))		strcpy(str, SSData(o->name));
			else if (!str_cmp(field, "shortdesc"))	strcpy(str, SSData(o->shortDesc));
			else if (!str_cmp(field, "vnum"))	sprintf(str, "%d", GET_OBJ_VNUM(o));
			else if (!str_cmp(field, "type"))	sprinttype(GET_OBJ_TYPE(o), item_types, str);
			else if (!str_cmp(field, "room"))	sprintf(str, "%d", world[o->Room()].number);
			else if (!str_cmp(field, "val0"))	sprintf(str, "%d", GET_OBJ_VAL(o, 0));
			else if (!str_cmp(field, "val1"))	sprintf(str, "%d", GET_OBJ_VAL(o, 1));
			else if (!str_cmp(field, "val2"))	sprintf(str, "%d", GET_OBJ_VAL(o, 2));
			else if (!str_cmp(field, "val3"))	sprintf(str, "%d", GET_OBJ_VAL(o, 3));
			else if (!str_cmp(field, "val4"))	sprintf(str, "%d", GET_OBJ_VAL(o, 0));
			else if (!str_cmp(field, "val5"))	sprintf(str, "%d", GET_OBJ_VAL(o, 1));
			else if (!str_cmp(field, "val6"))	sprintf(str, "%d", GET_OBJ_VAL(o, 2));
			else if (!str_cmp(field, "val7"))	sprintf(str, "%d", GET_OBJ_VAL(o, 3));
			else if (!str_cmp(field, "varclass"))	strcpy(str, "object");
			else if (!str_cmp(field, "weight"))	sprintf(str, "%d", GET_OBJ_WEIGHT(o));
			else if (!str_cmp(field, "cost"))	sprintf(str, "%d", GET_OBJ_COST(o));
			else if (!str_cmp(field, "owner"))	{
				if (o->carried_by)				sprintf(str, "%d", GET_ID(o->carried_by));
				else if (o->worn_by)			sprintf(str, "%d", GET_ID(o->worn_by));
			} else if (!str_cmp(field, "contents")) {
				ObjData *content;
				LListIterator<ObjData *>	iter(o->contains);
				while ((content = iter.Next()))
					sprintf(str + strlen(str), "%c%d", UID_CHAR, GET_ID(content));
			} else
				mudlogf(NRM, LVL_STAFF, FALSE, "SCRIPTERR: Trigger: %s, VNum %d, type: %d. unknown object field: '%s'",
						SSData(GET_TRIG_NAME(trig)), GET_TRIG_VNUM(trig), type, field);
		} else if (r) {
			if (!str_cmp(field, "name"))		strcpy(str, r->GetName(""));
			else if (!str_cmp(field, "vnum"))	sprintf(str, "%d", r->number);
			else if (!str_cmp(field, "varclass"))	strcpy(str, "room");
			else if (!str_cmp(field, "north")) {
				if (r->dir_option[NORTH]) {
					sprintf(str, "%c%d ", UID_CHAR, GET_ID(r));
					sprintbit(r->dir_option[NORTH]->exit_info, exit_bits, str + strlen(str));
				}
			} else if (!str_cmp(field, "east")) {
				if (r->dir_option[EAST]) {
					sprintf(str, "%c%d ", UID_CHAR, GET_ID(r));
					sprintbit(r->dir_option[EAST]->exit_info, exit_bits, str + strlen(str));
				}
			} else if (!str_cmp(field, "south")) {
				if (r->dir_option[SOUTH]) {
					sprintf(str, "%c%d ", UID_CHAR, GET_ID(r));
					sprintbit(r->dir_option[SOUTH]->exit_info, exit_bits, str + strlen(str));
				}
			} else if (!str_cmp(field, "west")) {
				if (r->dir_option[WEST]) {
					sprintf(str, "%c%d ", UID_CHAR, GET_ID(r));
					sprintbit(r->dir_option[WEST]->exit_info, exit_bits, str + strlen(str));
				}
			} else if (!str_cmp(field, "up")) {
				if (r->dir_option[UP]) {
					sprintf(str, "%c%d ", UID_CHAR, GET_ID(r));
					sprintbit(r->dir_option[UP]->exit_info, exit_bits, str + strlen(str));
				}
			} else if (!str_cmp(field, "down")) {
				if (r->dir_option[DOWN]) {
					sprintf(str, "%c%d ", UID_CHAR, GET_ID(r));
					sprintbit(r->dir_option[DOWN]->exit_info,exit_bits, str + strlen(str));
				}
			} else
				mudlogf(NRM, LVL_STAFF, FALSE, "SCRIPTERR: Trigger: %s, VNum %d, type: %d. unknown room field: '%s'",
					SSData(GET_TRIG_NAME(trig)), GET_TRIG_VNUM(trig), type, field);
		} else
			mudlogf(NRM, LVL_STAFF, FALSE, "SCRIPTERR: Trigger: %s, VNum %d. unknown arg field: '%s'",
					SSData(GET_TRIG_NAME(trig)), GET_TRIG_VNUM(trig), field);
	}
	release_buffer(field);
}


/* substitutes any variables into line and returns it as buf */
void var_subst(Ptr go, ScriptData *sc, TrigData *trig, int type, char *line, char *buf) {
	char *tmp, *repl_str, *var, *field, *p;
	int left, len;
	int	paren_count = 0;

	if (!strchr(line, '%')) {
		strcpy(buf, line);
		return;
	}
	
	// SET UP BUFS

	tmp = get_buffer(MAX_INPUT_LENGTH);
	repl_str = get_buffer(MAX_INPUT_LENGTH);

	p = strcpy(tmp, line);

	left = MAX_INPUT_LENGTH - 1;

	while (*p && (left > 0)) {
		while (*p && (*p != '%') && (left > 0)) {
			*(buf++) = *(p++);
			left--;
		}

		*buf = '\0';

		/* double % */
		if (*p && (*(++p) == '%') && (left > 0)) {
			*(buf++) = *(p++);
			*buf = '\0';
			left--;
			continue;
		} else if (*p && (left > 0)) {
			for (var = p; *p && (*p != '%') && (*p != '.'); p++);

			field = p;
			if (*p == '.') {
				*(p++) = '\0';
//				for (field = p; *p && (*p != '%'); p++);
				for (field = p; *p && ((*p != '%') || (paren_count)); p++) {
					if (*p == '(')		paren_count++;
					else if (*p == ')')	paren_count--;
				}
			}

			*(p++) = '\0';

			find_replacement(go, sc, trig, type, var, field, repl_str);

			strncat(buf, repl_str, left);
			len = strlen(repl_str);
			buf += len;
			left -= len;
		}
	}
	release_buffer(tmp);
	release_buffer(repl_str);
}


/* evaluates 'lhs op rhs', and copies to result */
void eval_op(char *op, char *lhs, char *rhs, char *result, Ptr go, ScriptData *sc, TrigData *trig) {
	char *p;
	int n;

	/* strip off extra spaces at begin and end */
	while (*lhs && isspace(*lhs))	lhs++;
	while (*rhs && isspace(*rhs))	rhs++;

	for (p = lhs; *p; p++);
	for (--p; isspace(*p) && (p > lhs); *p-- = '\0');
	for (p = rhs; *p; p++);
	for (--p; isspace(*p) && (p > rhs); *p-- = '\0');

	/* find the op, and figure out the value */
	if (!strcmp("||", op)) {
		if ((!*lhs || (*lhs == '0')) && (!*rhs || (*rhs == '0')))
			strcpy(result, "0");
		else
			strcpy(result, "1");
	} else if (!strcmp("&&", op)) {
		if (!*lhs || (*lhs == '0') || !*rhs || (*rhs == '0'))
			strcpy (result, "0");
		else
			strcpy (result, "1");
	} else if (!strcmp("==", op)) {
		if (is_number(lhs) && is_number(rhs))
			sprintf(result, "%d", atoi(lhs) == atoi(rhs));
		else
			sprintf(result, "%d", !str_cmp(lhs, rhs));
	} else if (!strcmp("!=", op)) {
		if (is_number(lhs) && is_number(rhs))
			sprintf(result, "%d", atoi(lhs) != atoi(rhs));
		else
			sprintf(result, "%d", str_cmp(lhs, rhs));
	} else if (!strcmp("<=", op)) {
		if (is_number(lhs) && is_number(rhs))
			sprintf(result, "%d", atoi(lhs) <= atoi(rhs));
		else
			sprintf(result, "%d", str_cmp(lhs, rhs) <= 0);
	} else if (!strcmp(">=", op)) {
		if (is_number(lhs) && is_number(rhs))
			sprintf(result, "%d", atoi(lhs) >= atoi(rhs));
		else
			sprintf(result, "%d", str_cmp(lhs, rhs) <= 0);
	} else if (!strcmp("<", op)) {
		if (is_number(lhs) && is_number(rhs))	sprintf(result, "%d", atoi(lhs) < atoi(rhs));
		else									sprintf(result, "%d", str_cmp(lhs, rhs) < 0);
	} else if (!strcmp(">", op)) {
		if (is_number(lhs) && is_number(rhs))	sprintf(result, "%d", atoi(lhs) > atoi(rhs));
		else									sprintf(result, "%d", str_cmp(lhs, rhs) > 0);
	} else if (!strcmp("/=", op))
		sprintf(result, "%c", str_str(lhs, rhs) ? '1' : '0');
	else if (!strcmp("*", op))
		sprintf(result, "%d", atoi(lhs) * atoi(rhs));
	else if (!strcmp("/", op))
		sprintf(result, "%d", (n = atoi(rhs)) ? (atoi(lhs) / n) : 0);
	else if (!strcmp("+", op)) 
		sprintf(result, "%d", atoi(lhs) + atoi(rhs));
	else if (!strcmp("-", op))
		sprintf(result, "%d", atoi(lhs) - atoi(rhs));
//	else if (!strcmp("@", op))
//		sprintf(result, "%d", atoi(lhs) % atoi(rhs));
	else if (!strcmp("!", op)) {
		if (is_number(rhs))	sprintf(result, "%d", !atoi(rhs));
		else				sprintf(result, "%d", !*rhs);
	}
}


/*
 * p points to the first quote, returns the matching
 * end quote, or the last non-null char in p.
*/
char *matching_quote(char *p) {
	for (p++; *p && (*p != '"'); p++) {
		if (*p == '\\' && *(p+1))	p++;
	}
	if (!*p)	p--;
	return p;
}

/*
 * p points to the first paren.  returns a pointer to the
 * matching closing paren, or the last non-null char in p.
 */
char *matching_paren(char *p) {
	int i;
	for (p++, i = 1; *p && i; p++) {
		if (*p == '(')			i++;
		else if (*p == ')')		i--;
		else if (*p == '"')		p = matching_quote(p);
	}
	return --p;
}


/* evaluates line, and returns answer in result */
void eval_expr(char *line, char *result, Ptr go, ScriptData *sc, TrigData *trig, int type) {
	char *p, *expr = get_buffer(MAX_INPUT_LENGTH);

//	while (*line && isspace(*line))
//		line++;
	skip_spaces(line);
	
	if (eval_lhs_op_rhs(line, result, go, sc, trig, type));
	else if (*line == '(') {
		p = strcpy(expr, line);
		p = matching_paren(expr);
		*p = '\0';
		eval_expr(expr + 1, result, go, sc, trig, type);
	} else
		var_subst(go, sc, trig, type, line, result);
	release_buffer(expr);
}


/*
 * evaluates expr if it is in the form lhs op rhs, and copies
 * answer in result.  returns 1 if expr is evaluated, else 0
 */
int eval_lhs_op_rhs(char *expr, char *result, Ptr go, ScriptData *sc, TrigData *trig, int type) {
	char *p, *tokens[256];
	char *line, *lhr, *rhr;
	int i, j, rslt = 0;

	/*
	 * valid operands, in order of priority
	 * each must also be defined in eval_op()
	 */
	static char *ops[] = {
		"||",
		"&&",
		"==",
		"!=",
		"<=",
		">=",
		"<",
		">",
		"/=",
		"-",
		"+",
		"/",
		"*",
//		"@",
		"!",
		"\n"
	};
	
	line = get_buffer(MAX_STRING_LENGTH);
	lhr = get_buffer(MAX_STRING_LENGTH);
	rhr = get_buffer(MAX_STRING_LENGTH);
	
	p = strcpy(line, expr);

	/*
	 * initialize tokens, an array of pointers to locations
	 * in line where the ops could possibly occur.
	 */
	for (j = 0; *p; j++) {
		tokens[j] = p;
		if (*p == '(')			p = matching_paren(p) + 1;
		else if (*p == '"')		p = matching_quote(p) + 1;
		else if (isalnum(*p))	for (p++; *p && (isalnum(*p) || isspace(*p)); p++);
		else					p++;
	}
	tokens[j] = NULL;

	for (i = 0; (*ops[i] != '\n') && !rslt; i++)
		for (j = 0; tokens[j] && !rslt; j++)
			if (!strn_cmp(ops[i], tokens[j], strlen(ops[i]))) {
				*tokens[j] = '\0';
				p = tokens[j] + strlen(ops[i]);

				eval_expr(line, lhr, go, sc, trig, type);
				eval_expr(p, rhr, go, sc, trig, type);
				eval_op(ops[i], lhr, rhr, result, go, sc, trig);

				rslt = 1;
			}
			
	release_buffer(lhr);
	release_buffer(rhr);
	release_buffer(line);

	return rslt;
}


/* returns 1 if cond is true, else 0 */
int process_if(char *cond, Ptr go, ScriptData *sc, TrigData *trig, int type) {
	char *p, *resultString = get_buffer(MAX_STRING_LENGTH);
	int result;
	
	eval_expr(cond, resultString, go, sc, trig, type);
	  
	p = resultString;
	skip_spaces(p);

	if (!*p || *p == '0')	result = 0;
	else					result = 1;
	release_buffer(resultString);
	return result;
}


/*
 * scans for a case/default instance
 * returns the line containg the correct case instance, or the last
 * line of the trigger if not found.
 */
CmdlistElement *find_case(TrigData *trig, CmdlistElement *cl, Ptr go, ScriptData *sc, int type, char *cond) {
	CmdlistElement *c;
	char *p, *buf;

	if (!(cl->next))
		return cl;

	for (c = cl->next; c && c->next; c = c->next) {
		for (p = c->cmd; *p && isspace(*p); p++);

		if (!strn_cmp("while ", p, 6) || !strn_cmp("switch", p, 6))
			c = find_done(c);
		else if (!strn_cmp("case ", p, 5)) {
			buf = get_buffer(MAX_STRING_LENGTH);
			eval_op("==", cond, p + 5, buf, go, sc, trig);
			if (*buf && *buf != '0') {
				release_buffer(buf);
				return c;
			}
			release_buffer(buf);
		} else if (!strn_cmp("default", p, 7))	return c;
		else if (!strn_cmp("done", p, 3))		return c;
		if (!c->next)							return c;
	}
	return c;
}



/*
 * scans for end of while/switch-blocks.
 * returns the line containg 'end', or the last
 * line of the trigger if not found.
 */
CmdlistElement *find_done(CmdlistElement *cl) {
	CmdlistElement *c;
	char *p;

	if (!(cl->next))
		return cl;

	for (c = cl->next; c && c->next; c = c->next) {
		for (p = c->cmd; *p && isspace(*p); p++);

		if (!strn_cmp("while ", p, 6) || !strn_cmp("switch ", p, 7))
			c = find_done(c);
		else if (!strn_cmp("done", p, 3))	return c;
		if (!c->next)						return c;
	}
	return c;
}


/*
 * scans for end of if-block.
 * returns the line containg 'end', or the last
 * line of the trigger if not found.
 */
CmdlistElement *find_end(CmdlistElement *cl) {
	CmdlistElement *c;
	char *p;

	if (!(cl->next))
		return cl;

	for (c = cl->next; c->next; c = c->next) {
		for (p = c->cmd; *p && isspace(*p); p++);

		if (!strn_cmp("if ", p, 3))			c = find_end(c);
		else if (!strn_cmp("end", p, 3))	return c;
		if (!c->next)						return c;
	}
	return c;
}


/*
 * searches for valid elseif, else, or end to continue execution at.
 * returns line of elseif, else, or end if found, or last line of trigger.
 */
CmdlistElement *find_else_end(TrigData *trig, CmdlistElement *cl,
		Ptr go, ScriptData *sc, int type) {
	CmdlistElement *c;
	char *p;

	if (!(cl->next))	return cl;

	for (c = cl->next; c->next; c = c->next) {
		for (p = c->cmd; *p && isspace(*p); p++);

		if (!strn_cmp("if ", p, 3))				c = find_end(c);
		else if (!strn_cmp("elseif ", p, 7)) {
			if (process_if(p + 7, go, sc, trig, type)) {
				GET_TRIG_DEPTH(trig)++;
				return c;
			}
		} else if (!strn_cmp("else", p, 4)) {
			GET_TRIG_DEPTH(trig)++;
			return c;
		} else if (!strn_cmp("end", p, 3))		return c;
		if (!c->next)							return c;
	}
	return c;
}


/* processes any 'wait' commands in a trigger */
void process_wait(Ptr go, TrigData *trig, int type, char *cmd, CmdlistElement *cl) {
//	char *buf, *arg;
	char *arg;
	struct wait_event_data *wait_event_obj;
	SInt32	time, hr, min, ntime;
	char c = '\0';

	extern struct TimeInfoData time_info;
	extern long pulse;

//	buf = get_buffer(MAX_INPUT_LENGTH);
	
//	arg = any_one_arg(cmd, buf);
	arg = cmd;
	skip_spaces(arg);

	if (!*arg)
		mudlogf(NRM, LVL_STAFF, FALSE, "SCRIPTERR: Trigger: %s, VNum %d. wait w/o an arg: 'wait %s'",
				SSData(GET_TRIG_NAME(trig)), GET_TRIG_VNUM(trig), cmd);
	else if (!strn_cmp(arg, "until ", 6)) {
		/* valid forms of time are 14:30 and 1430 */
		if (sscanf(arg, "until %d:%d", &hr, &min) == 2)
			min += (hr * 60);
		else
			min = (hr % 100) + ((hr / 100) * 60);

		/* calculate the pulse of the day of "until" time */
		ntime = (min * SECS_PER_MUD_HOUR * PASSES_PER_SEC) / 60;

		/* calculate pulse of day of current time */
		time = (pulse % (SECS_PER_MUD_HOUR * PASSES_PER_SEC)) +
				(time_info.hours * SECS_PER_MUD_HOUR * PASSES_PER_SEC);

		/* adjust for next day */
		if (time >= ntime)	time = (SECS_PER_MUD_DAY * PASSES_PER_SEC) - time + ntime;
		else				time = ntime - time;
	} else if (sscanf(arg, "%d %c", &time, &c) == 2) {
		if (c == 'd')		time *= 24 * SECS_PER_MUD_HOUR * PASSES_PER_SEC;
		else if (c == 'h')	time *= SECS_PER_REAL_HOUR * PASSES_PER_SEC;
		else if (c == 't')	time *= SECS_PER_MUD_HOUR * PASSES_PER_SEC;	//	't': ticks
//		else if (c == 's')	time *= PASSES_PER_SEC;
		else if (c == 'p')	;											//	'p': pulses
		else 				time *= PASSES_PER_SEC;						//	Default: seconds
	} else					time *= PASSES_PER_SEC;
	
//	release_buffer(buf);

	CREATE(wait_event_obj, struct wait_event_data, 1);
	wait_event_obj->trigger = trig;
	wait_event_obj->type = type;
	wait_event_obj->go = (type == WLD_TRIGGER) ? (Ptr)((RoomData *)go)->number : go;

	GET_TRIG_WAIT(trig) = new Event(trig_wait_event, wait_event_obj, time);
	trig->curr_state = cl->next;
}


//	processes a script set command
void process_set(ScriptData *sc, TrigData *trig, char *cmd) {
//	char 	*arg = get_buffer(MAX_INPUT_LENGTH),
	char	*name = get_buffer(MAX_INPUT_LENGTH),
			*value;

//	value = two_arguments(cmd, arg, name);
	value = one_argument(cmd, name);
	
	skip_spaces(value);

	if (!*name)
		mudlogf(NRM, LVL_STAFF, FALSE, "SCRIPTERR: Trigger: %s, VNum %d. set w/o an arg: 'set %s'",
			SSData(GET_TRIG_NAME(trig)), GET_TRIG_VNUM(trig), cmd);
	else
		add_var(GET_TRIG_VARS(trig), name, value);
//	release_buffer(arg);
	release_buffer(name);
}


//	processes a script eval command
void process_eval(Ptr go, ScriptData *sc, TrigData *trig, int type, char *cmd) {
//	char	*arg = get_buffer(MAX_INPUT_LENGTH),
	char	*name = get_buffer(MAX_INPUT_LENGTH),
			*result = get_buffer(MAX_INPUT_LENGTH),
			*expr;

//	expr = two_arguments(cmd, arg, name);
	expr = one_argument(cmd, name);

	skip_spaces(expr);

	if (!*name)
		mudlogf(NRM, LVL_STAFF, FALSE, "SCRIPTERR: Trigger: %s, VNum %d. eval w/o an arg: 'eval %s'",
				SSData(GET_TRIG_NAME(trig)), GET_TRIG_VNUM(trig), cmd);
	else {
		eval_expr(expr, result, go, sc, trig, type);
		add_var(GET_TRIG_VARS(trig), name, result);
	}
//	release_buffer(arg);
	release_buffer(name);
	release_buffer(result);
}


//	processes a script return command.
//	returns the new value for the script to return.
int process_return(TrigData *trig, char *cmd) {
//	char	*arg1 = get_buffer(MAX_INPUT_LENGTH),
//			*arg2 = get_buffer(MAX_INPUT_LENGTH);
	char *arg;
	int result;
	
//	two_arguments(cmd, arg1, arg2);
	skip_spaces(arg);
	
	if (!*arg) {
		mudlogf(NRM, LVL_STAFF, FALSE, "SCRIPTERR: Trigger: %s, VNum %d. return w/o an arg: 'return %s'",
				SSData(GET_TRIG_NAME(trig)), GET_TRIG_VNUM(trig), cmd);
		result = 1;
	} else
		result = atoi(arg);
//	release_buffer(arg1);
//	release_buffer(arg2);
	return result;
}


//	removes a variable from the global vars of sc,
//	or the local vars of trig if not found in global list.
void process_unset(ScriptData *sc, TrigData *trig, char *cmd) {
	char *arg = get_buffer(MAX_INPUT_LENGTH), *var;
//	char *var;
	var = any_one_arg(cmd, arg);
	
	var = arg;
	
//	skip_spaces(var);

	if (!*var)
		mudlogf(NRM, LVL_STAFF, FALSE, "SCRIPTERR: Trigger: %s, VNum %d. unset w/o an arg: 'unset %s'",
				SSData(GET_TRIG_NAME(trig)), GET_TRIG_VNUM(trig), cmd);
	else if (!remove_var(sc->global_vars, var))
		remove_var(GET_TRIG_VARS(trig), var);
		
	release_buffer(arg);
}


//	makes a local variable into a global variable
void process_global(ScriptData *sc, TrigData *trig, char *cmd) {
	TrigVarData *vd;
//	char *arg = get_buffer(MAX_INPUT_LENGTH), *var;
	char *var;
	
	var = cmd;
//	var = any_one_arg(cmd, arg);

	skip_spaces(var);

	if (!*var)
	mudlogf(NRM, LVL_STAFF, FALSE, "SCRIPTERR: Trigger: %s, VNum %d. global w/o an arg: 'global %s'",
			SSData(GET_TRIG_NAME(trig)), GET_TRIG_VNUM(trig), cmd);
	else {
		LListIterator<TrigVarData *>	iter(GET_TRIG_VARS(trig));

		while ((vd = iter.Next()))
			if (!str_cmp(vd->name, var))
				break;

		if (!vd)
			mudlogf(NRM, LVL_STAFF, FALSE, "SCRIPTERR: Trigger: %s, VNum %d. local var 'global %s' not found in global call",
					SSData(GET_TRIG_NAME(trig)), GET_TRIG_VNUM(trig), var);
		else { 
			add_var(sc->global_vars, vd->name, vd->value);
			remove_var(GET_TRIG_VARS(trig), vd->name);
		}
	}
//	release_buffer(arg);
}


/*  This is the core driver for scripts. */
int script_driver(Ptr go, TrigData *trig, int type, int mode) {
	static int depth = 0;
	int ret_val = 1;
	CmdlistElement *cl, *temp;
	char *cmd, *p;
	ScriptData *sc = NULL;
//	UInt32	loops = 0;

//	void obj_command_interpreter(ObjData *obj, char *argument);
//	void wld_command_interpreter(RoomData *room, char *argument);
	void script_command_interpreter(Ptr go, char *argument, int type);

	if (depth > MAX_SCRIPT_DEPTH) {
		mudlogf(NRM, LVL_STAFF, FALSE, "SCRIPTERR: Triggers recursed beyond maximum allowed depth.");
		return ret_val;
	}

	switch (type) {
		case MOB_TRIGGER:
			if (PURGED((CharData *)go))	return 0;
			sc = SCRIPT((CharData *) go);
			break;
		case OBJ_TRIGGER:
			if (PURGED((ObjData *)go))	return 0;
			sc = SCRIPT((ObjData *) go);
			break;
		case WLD_TRIGGER:
			sc = SCRIPT((RoomData *) go);
			break;
		default:
			return 0;
	}

	depth++;
	cmd = get_buffer(MAX_INPUT_LENGTH);

	if (mode == TRIG_NEW) {
		GET_TRIG_LOOPS(trig) = 0;
		GET_TRIG_DEPTH(trig) = 1;
	}
	
	for (cl = (mode == TRIG_NEW) ? trig->cmdlist : trig->curr_state; cl && GET_TRIG_DEPTH(trig); cl = cl->next) {
/*		It purges the trig if the owner is purged, right?  If so, no need for this check
		switch (type) {
			case MOB_TRIGGER:
				if (PURGED((CharData *)go))	return 0;
				sc = SCRIPT((CharData *) go);
				break;
			case OBJ_TRIGGER:
				if (PURGED((ObjData *)go))	return 0;
				sc = SCRIPT((ObjData *) go);
				break;
			case WLD_TRIGGER:
				sc = SCRIPT((RoomData *) go);
				break;
			default:
				return 0;
		}
*/
		if (PURGED(trig))	break;
		if (PURGED(sc))		break;
		
		for (p = cl->cmd; *p && isspace(*p); p++);
		
		if (*p == '*')
			continue;
		else if (!strn_cmp(p, "if ", 3)) {
			if (process_if(p + 3, go, sc, trig, type))	GET_TRIG_DEPTH(trig)++;
			else										cl = find_else_end(trig, cl, go, sc, type);
		} else if (!strn_cmp("elseif ", p, 7) || !strn_cmp("else", p, 4)) {
			cl = find_end(cl);
			GET_TRIG_DEPTH(trig)--;
		} else if (!strn_cmp("while ", p, 6)) {
			temp = find_done(cl);
			if (process_if(p + 6, go, sc, trig, type))		temp->original = cl;
			else											cl = temp;
		} else if (!strn_cmp("switch ", p, 7)) {
			char *buf = get_buffer(MAX_STRING_LENGTH);
			eval_expr(p + 7, buf, go, sc, trig, type);
			cl = find_case(trig, cl, go, sc, type, buf);
			release_buffer(buf);
		} else if (!strn_cmp("end", p, 3)) {
			GET_TRIG_DEPTH(trig)--;
		} else if (!strn_cmp("done", p, 4)) {
			if (cl->original && process_if(cl->original->cmd + 6, go, sc, trig, type)) {
				cl = cl->original;
				GET_TRIG_LOOPS(trig)++;
				if (!(GET_TRIG_LOOPS(trig) % 500)) {
					mudlogf(NRM, LVL_STAFF, TRUE, "SCRIPTERR: Trigger #%d halted.", GET_TRIG_VNUM(trig));
					break;
				} else if (!(GET_TRIG_LOOPS(trig) % 100))
					mudlogf(NRM, LVL_STAFF, TRUE, "SCRIPTERR: Trigger #%d has looped %d times!!!", GET_TRIG_VNUM(trig), GET_TRIG_LOOPS(trig));
				if (!(GET_TRIG_LOOPS(trig) % 20)) {
					process_wait(go, trig, type, "5 p", cl);	//	5 Pulses - 1/2 second
					depth--;
					release_buffer(cmd);
					return ret_val;
				}
			}
		} else if (!strn_cmp("break", p, 5)) {
			cl = find_done(cl);
		} else if (!strn_cmp("case", p, 4)) {
			;	// Do nothing, this allows multiple cases to a single instance
		} else {
			var_subst(go, sc, trig, type, p, cmd);
			if (!strn_cmp(cmd, "eval ", 5))			process_eval(go, sc, trig, type, cmd + 5);
			else if (!strn_cmp(cmd, "halt", 4))		break;
			else if (!strn_cmp(cmd, "global ", 7))	process_global(sc, trig, cmd + 7);
			else if (!strn_cmp(cmd, "return ", 7))	ret_val = process_return(trig, cmd + 7);
			else if (!strn_cmp(cmd, "set ", 4))		process_set(sc, trig, cmd + 4);
			else if (!strn_cmp(cmd, "unset ", 6))	process_unset(sc, trig, cmd + 6);
			else if (!strn_cmp(cmd, "wait ", 5)) {
				process_wait(go, trig, type, cmd + 5, cl);
				depth--;
				release_buffer(cmd);
				return ret_val;
			} else script_command_interpreter(go, cmd, type);
		}
	}
	release_buffer(cmd);
	
	free_varlist(GET_TRIG_VARS(trig));
	GET_TRIG_DEPTH(trig) = 0;

	depth--;
	return ret_val;
}
