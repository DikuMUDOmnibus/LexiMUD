/**************************************************************************
*  File: triggers.c                                                       *
*                                                                         *
*  Usage: contains all the trigger functions for scripts.                 *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Death's Gate MUD is based on CircleMUD, Copyright (C) 1993, 94.        *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
*                                                                         *
*  $Author: galion $
*  $Date: 1996/08/05 23:32:08 $
*  $Revision: 3.9 $
**************************************************************************/

#include "structs.h"
#include "scripts.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "find.h"
#include "db.h"
#include "buffer.h"


/* external functions from scripts.c */
void add_var(LList<TrigVarData *> &var_list, char *name, char *value);
int script_driver(Ptr go, TrigData *trig, int type, int mode);
char *matching_quote(char *p);

extern char *dirs[];
extern int rev_dir[];

char *one_phrase(char *arg, char *first_arg);
int is_substring(char *sub, char *string);
int word_check(char *str, char *wordlist);
void bribe_mtrigger(CharData *ch, CharData *actor, int amount);
int greet_mtrigger(CharData *actor, int dir);
int entry_mtrigger(CharData *ch);
int command_mtrigger(CharData *actor, char *cmd, char *argument);
void speech_mtrigger(CharData *actor, char *str);
void act_mtrigger(CharData *ch, char *str, CharData *actor, 
		CharData *victim, ObjData *object, ObjData *target, char *arg);
void fight_mtrigger(CharData *ch);
void hitprcnt_mtrigger(CharData *ch);
int receive_mtrigger(CharData *ch, CharData *actor, ObjData *obj);
int death_mtrigger(CharData *ch, CharData *actor);
int get_otrigger(ObjData *obj, CharData *actor);
int cmd_otrig(ObjData *obj, CharData *actor, char *cmd, char *argument, int type);
int greet_otrigger(CharData *actor, int dir);
int command_otrigger(CharData *actor, char *cmd, char *argument);
int wear_otrigger(ObjData *obj, CharData *actor, int where);
int drop_otrigger(ObjData *obj, CharData *actor);
int give_otrigger(ObjData *obj, CharData *actor, CharData *victim);
int sit_otrigger(ObjData *obj, CharData *actor);
int enter_wtrigger(RoomData *room, CharData *actor, int dir);
int command_wtrigger(CharData *actor, char *cmd, char *argument);
void speech_wtrigger(CharData *actor, char *str);
int drop_wtrigger(ObjData *obj, CharData *actor);
int consume_otrigger(ObjData *obj, CharData *actor);

int leave_mtrigger(CharData *actor, int dir);
int leave_otrigger(CharData *actor, int dir);
int leave_wtrigger(RoomData *room, CharData *actor, int dir);

int door_mtrigger(CharData *actor, int subcmd, int dir);
int door_otrigger(CharData *actor, int subcmd, int dir);
int door_wtrigger(CharData *actor, int subcmd, int dir);

int speech_otrig(ObjData *obj, CharData *actor, char *str, int type);
void speech_otrigger(CharData *actor, char *str);

int is_empty(int zone_nr);

/* mob trigger types */
char *mtrig_types[] = {
	"Global",
	"Random",
	"Command",
	"Speech",
	"Act",
	"Death",
	"Greet",
	"Greet-All",
	"Entry",
	"Receive",
	"Fight",
	"HitPrcnt",
	"Bribe",
	"Leave",
	"Leave-All",
	"Door",
	"\n"
};


/* obj trigger types */
char *otrig_types[] = {
	"Global",
	"Random",
	"Command",
	"Sit",
	"Leave",
	"Greet",
	"Get",
	"Drop",
	"Give",
	"Wear",
	"Door",
	"Speech",
	"Consume",
	"\n"
};


/* wld trigger types */
char *wtrig_types[] = {
	"Global",
	"Random",
	"Command",
	"Speech",
	"Door",
	"Leave",
	"Enter",
	"Drop",
	"\n"
};



char *trig_types[] = {
	"Global",
	"Random",
	"Command",
	"Speech",
	"Greet",
	"Leave",
	"Door",
	"Drop",
	"Get",
	"Act",
	"Death",
	"Fight",
	"HitPrcnt",
	"Sit",
	"Give",
	"Wear",
	"\n"
};


char *door_act[] = {
	"open",
	"close",
	"unlock",
	"lock",
	"pick",
	"\n"
};



/*
 *  General functions used by several triggers
 */


/*
 * Copy first phrase into first_arg, returns rest of string
 */
char *one_phrase(char *arg, char *first_arg) {
	skip_spaces(arg);

	if (!*arg)	*first_arg = '\0';
	else if (*arg == '"') {
		char *p, c;

		p = matching_quote(arg);
		c = *p;
		*p = '\0';
		strcpy(first_arg, arg + 1);
		if (c == '\0')	return p;
		else			return p + 1;
	} else {
		char *s, *p;

		s = first_arg;
		p = arg;
		
		while (*p && !isspace(*p) && *p != '"')
			*s++ = *p++;
			
		*s = '\0';
		return p;
	}
	return arg;
}


int is_substring(char *sub, char *string) {
	char *s;

	if ((s = str_str(string, sub))) {
		int len = strlen(string);
		int sublen = strlen(sub);

		if ((s == string || isspace(*(s - 1)) || ispunct(*(s - 1))) &&	/* check front */
				((s + sublen == string + len) || isspace(s[sublen]) ||	/* check end */
				ispunct(s[sublen])))
			return 1;
	}
	return 0;
}


/*
 * return 1 if str contains a word or phrase from wordlist.
 * phrases are in double quotes (").
 * if wrdlist is NULL, then return 1, if str is NULL, return 0.
 */
int word_check(char *str, char *wordlist) {
	char *words = get_buffer(MAX_INPUT_LENGTH),
		*phrase = get_buffer(MAX_INPUT_LENGTH),
		*s;

	strcpy(words, wordlist);
    
	for (s = one_phrase(words, phrase); *phrase; s = one_phrase(s, phrase)) {
		if (is_substring(phrase, str)) {
			release_buffer(words);
			release_buffer(phrase);
			return 1;
		}
	}
	
	release_buffer(words);
	release_buffer(phrase);
	
	return 0;
}

	

/*
 *  mob triggers
 */

void random_mtrigger(CharData *ch) {
	TrigData *t;

	if (!SCRIPT_CHECK(ch, MTRIG_RANDOM) || AFF_FLAGGED(ch, AFF_CHARM))
		return;

	START_ITER(iter, t, TrigData *, TRIGGERS(SCRIPT(ch))) {
		if (TRIGGER_CHECK(t, MTRIG_RANDOM) && (Number(1, 100) <= GET_TRIG_NARG(t))) {
			if (TRIGGER_CHECK(t, MTRIG_GLOBAL) || !is_empty(world[IN_ROOM(ch)].zone)) {
				script_driver(ch, t, MOB_TRIGGER, TRIG_NEW);
				break;
			}
		}
	} END_ITER(iter);
}

/*
void bribe_mtrigger(CharData *ch, CharData *actor, int amount) {
	TrigData *t;
	char *buf;
  
	if (!SCRIPT_CHECK(ch, MTRIG_BRIBE) || AFF_FLAGGED(ch, AFF_CHARM))
		return;

	for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
		if (TRIGGER_CHECK(t, MTRIG_BRIBE) && (amount >= GET_TRIG_NARG(t))) {

			buf = get_buffer(MAX_INPUT_LENGTH);
			sprintf(buf, "%d", amount);
			add_var(&GET_TRIG_VARS(t), "amount", buf);
			ADD_UID_VAR(buf, t, actor, "actor");
			release_buffer(buf);
			script_driver(ch, t, MOB_TRIGGER, TRIG_NEW);
			break;
		}
	}
}
*/

int greet_mtrigger(CharData *actor, int dir) {
	TrigData *t;
	CharData *ch;
 
	START_ITER(ch_iter, ch, CharData *, world[IN_ROOM(actor)].people) {
		if (!SCRIPT_CHECK(ch, MTRIG_GREET | MTRIG_GREET_ALL) || 
				!AWAKE(ch) || FIGHTING(ch) || (ch == actor) || AFF_FLAGGED(ch, AFF_CHARM))
			continue;
    
//		for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
		START_ITER(trig_iter, t, TrigData *, TRIGGERS(SCRIPT(ch))) {
			if (((IS_SET(GET_TRIG_TYPE(t), MTRIG_GREET) && ch->CanSee(actor)) ||
					IS_SET(GET_TRIG_TYPE(t), MTRIG_GREET_ALL)) && 
					!GET_TRIG_DEPTH(t) && (Number(1, 100) <= GET_TRIG_NARG(t))) {
				add_var(GET_TRIG_VARS(t), "direction", dirs[rev_dir[dir]]);
				ADD_UID_VAR(t, actor, "actor");
				END_ITER(trig_iter);
				END_ITER(ch_iter);
				return script_driver(ch, t, MOB_TRIGGER, TRIG_NEW);
			}
		} END_ITER(trig_iter);
	} END_ITER(ch_iter);
	return 1;
}


int entry_mtrigger(CharData *ch) {
	TrigData *t;

	if (!SCRIPT_CHECK(ch, MTRIG_ENTRY) || AFF_FLAGGED(ch, AFF_CHARM))
		return 1;

	START_ITER(iter, t, TrigData *, TRIGGERS(SCRIPT(ch))) {
		if (TRIGGER_CHECK(t, MTRIG_ENTRY) && (Number(1, 100) <= GET_TRIG_NARG(t))){
			END_ITER(iter);
			return script_driver(ch, t, MOB_TRIGGER, TRIG_NEW);
		}
	} END_ITER(iter);
	return 1;
}


int command_mtrigger(CharData *actor, char *cmd, char *argument) {
	CharData *ch;
	TrigData *t;

	START_ITER(ch_iter, ch, CharData *, world[IN_ROOM(actor)].people) {
		if (SCRIPT_CHECK(ch, MTRIG_COMMAND) && !AFF_FLAGGED(ch, AFF_CHARM)) {
//			for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
			START_ITER(trig_iter, t, TrigData *, TRIGGERS(SCRIPT(ch))) {
				if (TRIGGER_CHECK(t, MTRIG_COMMAND) && SSData(GET_TRIG_ARG(t)) &&
						!str_cmp(SSData(GET_TRIG_ARG(t)), cmd)) {
					ADD_UID_VAR(t, actor, "actor");
					skip_spaces(argument);
					add_var(GET_TRIG_VARS(t), "arg", argument);
					if (script_driver(ch, t, MOB_TRIGGER, TRIG_NEW)) {
						END_ITER(trig_iter);
						END_ITER(ch_iter);
						return 1;
					}
				}
			} END_ITER(trig_iter);
//			}
		}
	} END_ITER(ch_iter);
	return 0;
}


void speech_mtrigger(CharData *actor, char *str) {
	CharData *ch;
	TrigData *t;
	
	START_ITER(ch_iter, ch, CharData *, world[IN_ROOM(actor)].people) {
		if (SCRIPT_CHECK(ch, MTRIG_SPEECH) && AWAKE(ch) && !AFF_FLAGGED(ch, AFF_CHARM) && (actor != ch)) {
//			for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
			START_ITER(trig_iter, t, TrigData *, TRIGGERS(SCRIPT(ch))) {
				if (TRIGGER_CHECK(t, MTRIG_SPEECH) && (!SSData(GET_TRIG_ARG(t)) ||
						(GET_TRIG_NARG(t) ?
						word_check(str, SSData(GET_TRIG_ARG(t))) :
						is_substring(SSData(GET_TRIG_ARG(t)), str)))) {
					ADD_UID_VAR(t, actor, "actor");
					add_var(GET_TRIG_VARS(t), "speech", str);
					script_driver(ch, t, MOB_TRIGGER, TRIG_NEW);
					break;
				}
			} END_ITER(trig_iter);
//			}
		}
	} END_ITER(ch_iter);
}


void act_mtrigger(CharData *ch, char *str, CharData *actor, 
		CharData *victim, ObjData *object, ObjData *target, char *arg) {
	TrigData *t;

	if (SCRIPT_CHECK(ch, MTRIG_ACT) && !AFF_FLAGGED(ch, AFF_CHARM)) {
//		for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) 
		START_ITER(iter, t, TrigData *, TRIGGERS(SCRIPT(ch))) {
//			if (TRIGGER_CHECK(t, MTRIG_ACT) && word_check(str, SSData(GET_TRIG_ARG(t)))) {
			if (TRIGGER_CHECK(t, MTRIG_ACT) && (GET_TRIG_NARG(t) ?
					word_check(str, SSData(GET_TRIG_ARG(t))) :
					is_substring(SSData(GET_TRIG_ARG(t)), str))) {

				if (actor)		ADD_UID_VAR(t, actor, "actor");
				if (victim)		ADD_UID_VAR(t, victim, "victim");
				if (object)		ADD_UID_VAR(t, object, "object");
				if (target)		ADD_UID_VAR(t, target, "target");
				if (arg) {
					skip_spaces(arg);
					add_var(GET_TRIG_VARS(t), "arg", arg);
				}
				script_driver(ch, t, MOB_TRIGGER, TRIG_NEW);
				break;
			}
		} END_ITER(iter);
	}
}


void fight_mtrigger(CharData *ch) {
	TrigData *t;

	if (!SCRIPT_CHECK(ch, MTRIG_FIGHT) || !FIGHTING(ch) || AFF_FLAGGED(ch, AFF_CHARM))
		return;

	START_ITER(iter, t, TrigData *, TRIGGERS(SCRIPT(ch))) {
		if (TRIGGER_CHECK(t, MTRIG_FIGHT) && (Number(1, 100) <= GET_TRIG_NARG(t))){
			ADD_UID_VAR(t, FIGHTING(ch), "actor");
			script_driver(ch, t, MOB_TRIGGER, TRIG_NEW);
			break;
		}
	} END_ITER(iter);
}


void hitprcnt_mtrigger(CharData *ch) {
	TrigData *t;
  
	if (!SCRIPT_CHECK(ch, MTRIG_HITPRCNT) || !FIGHTING(ch) || AFF_FLAGGED(ch, AFF_CHARM))
		return;

	START_ITER(iter, t, TrigData *, TRIGGERS(SCRIPT(ch))) {
		if (TRIGGER_CHECK(t, MTRIG_HITPRCNT) && GET_MAX_HIT(ch) &&
				(((GET_HIT(ch) * 100) / GET_MAX_HIT(ch)) <= GET_TRIG_NARG(t))) {
			ADD_UID_VAR(t, FIGHTING(ch), "actor");
			script_driver(ch, t, MOB_TRIGGER, TRIG_NEW);
			break;
		}
	} END_ITER(iter);
}


int receive_mtrigger(CharData *ch, CharData *actor, ObjData *obj) {
	TrigData *t;

	if (!SCRIPT_CHECK(ch, MTRIG_RECEIVE) || AFF_FLAGGED(ch, AFF_CHARM))
		return 1;

	START_ITER(iter, t, TrigData *, TRIGGERS(SCRIPT(ch))) {
		if (TRIGGER_CHECK(t, MTRIG_RECEIVE) && (Number(1, 100) <= GET_TRIG_NARG(t))){
			ADD_UID_VAR(t, actor, "actor");
			ADD_UID_VAR(t, obj, "object");
			END_ITER(iter);
			return script_driver(ch, t, MOB_TRIGGER, TRIG_NEW);
		}
	} END_ITER(iter);

	return 1;
}


int death_mtrigger(CharData *ch, CharData *actor) {
	TrigData *t;
  
	if (!SCRIPT_CHECK(ch, MTRIG_DEATH) || AFF_FLAGGED(ch, AFF_CHARM))
		return 1;
  
	START_ITER(iter, t, TrigData *, TRIGGERS(SCRIPT(ch))) {
		if (TRIGGER_CHECK(t, MTRIG_DEATH)) {
			if (actor)	ADD_UID_VAR(t, actor, "actor");
			END_ITER(iter);
			return script_driver(ch, t, MOB_TRIGGER, TRIG_NEW);
		}
	} END_ITER(iter);
	return 1;
}


int leave_mtrigger(CharData *actor, int dir) {
	TrigData *t;
	CharData *ch;
	
	START_ITER(ch_iter, ch, CharData *, world[IN_ROOM(actor)].people) {
		if (!SCRIPT_CHECK(ch, MTRIG_LEAVE | MTRIG_LEAVE_ALL) || !AWAKE(ch) ||
				FIGHTING(ch) || (ch == actor) || AFF_FLAGGED(ch, AFF_CHARM))
			continue;

//		for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
		START_ITER(trig_iter, t, TrigData *, TRIGGERS(SCRIPT(ch))) {
			if (((IS_SET(GET_TRIG_TYPE(t), MTRIG_LEAVE) && ch->CanSee(actor)) ||
					IS_SET(GET_TRIG_TYPE(t), MTRIG_LEAVE_ALL)) &&
					!GET_TRIG_DEPTH(t) && (Number(1, 100) <= GET_TRIG_NARG(t))) {
				add_var(GET_TRIG_VARS(t), "direction", dirs[dir]);
				ADD_UID_VAR(t, actor, "actor");
				END_ITER(trig_iter);
				END_ITER(ch_iter);
				return script_driver(ch, t, MOB_TRIGGER, TRIG_NEW);
			}
		} END_ITER(trig_iter);
//		}
	} END_ITER(ch_iter);
	return 1;
}


int door_mtrigger(CharData *actor, int subcmd, int dir) {
	TrigData *t;
	CharData *ch;
	
	START_ITER(ch_iter, ch, CharData *, world[IN_ROOM(actor)].people) {
		if (!SCRIPT_CHECK(ch, MTRIG_DOOR) || !AWAKE(ch) || FIGHTING(ch) ||
				(ch == actor) || AFF_FLAGGED(ch, AFF_CHARM))
			continue;

//		for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
		START_ITER(trig_iter, t, TrigData *, TRIGGERS(SCRIPT(ch))) {
			if (TRIGGER_CHECK(t, MTRIG_DOOR) && ch->CanSee(actor) &&
					(Number(1, 100) <= GET_TRIG_NARG(t))) {
				add_var(GET_TRIG_VARS(t), "cmd", door_act[subcmd]);
				add_var(GET_TRIG_VARS(t), "direction", dirs[dir]);
				ADD_UID_VAR(t, actor, "actor");
				END_ITER(trig_iter);
				END_ITER(ch_iter);
				return script_driver(ch, t, MOB_TRIGGER, TRIG_NEW);
			}
		} END_ITER(trig_iter);
//		}
	} END_ITER(ch_iter);
	return 1;
}


/*
 *  object triggers
 */

void random_otrigger(ObjData *obj) {
	TrigData *t;
	RNum	room;

	if (!SCRIPT_CHECK(obj, OTRIG_RANDOM))
		return;

	START_ITER(iter, t, TrigData *, TRIGGERS(SCRIPT(obj))) {
		if (TRIGGER_CHECK(t, OTRIG_RANDOM) && (Number(1, 100) <= GET_TRIG_NARG(t))) {
			if (((room = obj->Room()) != NOWHERE) && (TRIGGER_CHECK(t, OTRIG_GLOBAL) ||
					!is_empty(world[room].zone))) {
				script_driver(obj, t, OBJ_TRIGGER, TRIG_NEW);
				break;
			}
		}
	}
	END_ITER(iter);
}


int get_otrigger(ObjData *obj, CharData *actor) {
	TrigData *t;
	
	if (!SCRIPT_CHECK(obj, OTRIG_GET))
		return 1;

	START_ITER(iter, t, TrigData *, TRIGGERS(SCRIPT(obj))) {
		if (TRIGGER_CHECK(t, OTRIG_GET) && (Number(1, 100) <= GET_TRIG_NARG(t))) {
			ADD_UID_VAR(t, actor, "actor");
			END_ITER(iter);
			return script_driver(obj, t, OBJ_TRIGGER, TRIG_NEW);
		}
	} END_ITER(iter);
	return 1;
}


/* checks for command trigger on specific object */
int cmd_otrig(ObjData *obj, CharData *actor, char *cmd, char *argument, int type) {
	TrigData *t;
  
	if (obj && SCRIPT_CHECK(obj, OTRIG_COMMAND)) {
		LListIterator<TrigData *>	iter(TRIGGERS(SCRIPT(obj)));
		while ((t = iter.Next())) {
			if (TRIGGER_CHECK(t, OTRIG_COMMAND) && IS_SET(GET_TRIG_NARG(t), type) &&
					SSData(GET_TRIG_ARG(t)) && !str_cmp(SSData(GET_TRIG_ARG(t)), cmd)) {
				ADD_UID_VAR(t, actor, "actor");
				skip_spaces(argument);
				add_var(GET_TRIG_VARS(t), "arg", argument);
	
				if (script_driver(obj, t, OBJ_TRIGGER, TRIG_NEW))
					return 1;
			}
		}
	}

	return 0;
}


int command_otrigger(CharData *actor, char *cmd, char *argument) {
	ObjData *obj;
	int i;

	for (i = 0; i < NUM_WEARS; i++)
		if (cmd_otrig(GET_EQ(actor, i), actor, cmd, argument, OCMD_EQUIP))
			return 1;
  
  	LListIterator<ObjData *>	ch_iter(actor->carrying);
  	while ((obj = ch_iter.Next()))
		if (cmd_otrig(obj, actor, cmd, argument, OCMD_INVEN))
			return 1;
		
  	LListIterator<ObjData *>	rm_iter(world[IN_ROOM(actor)].contents);
  	while ((obj = rm_iter.Next()))
		if (cmd_otrig(obj, actor, cmd, argument, OCMD_ROOM))
			return 1;

	return 0;
}


/* checks for speech trigger on specific object  */
int speech_otrig(ObjData *obj, CharData *actor, char *str, int type) {
	TrigData *t;

	if (obj && SCRIPT_CHECK(obj, OTRIG_SPEECH)) {
//		for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
		START_ITER(iter, t, TrigData *, TRIGGERS(SCRIPT(obj))) {
			if (TRIGGER_CHECK(t, OTRIG_SPEECH) && IS_SET(GET_TRIG_NARG(t), type) &&
					(!SSData(GET_TRIG_ARG(t)) ||
					(IS_SET(GET_TRIG_NARG(t), 8) ? word_check(str, SSData(GET_TRIG_ARG(t))) :
					is_substring(SSData(GET_TRIG_ARG(t)), str)))) {
				ADD_UID_VAR(t, actor, "actor");
				add_var(GET_TRIG_VARS(t), "speech", str);
	
				if (script_driver(obj, t, OBJ_TRIGGER, TRIG_NEW)) {
					END_ITER(iter);
					return 1;
				}
			}
		} END_ITER(iter);
//		}
	}
	return 0;
}


void speech_otrigger(CharData *actor, char *str) {
	ObjData *obj;
	int i;

	for (i = 0; i < NUM_WEARS; i++)
		if (speech_otrig(GET_EQ(actor, i), actor, str, OCMD_EQUIP))
			return;
  
	START_ITER(ch_iter, obj, ObjData *, actor->carrying) {
		if (speech_otrig(obj, actor, str, OCMD_INVEN)) {
			END_ITER(ch_iter);
			return;
		}
	} END_ITER(ch_iter);

	START_ITER(obj_iter, obj, ObjData *, world[IN_ROOM(actor)].contents) {
		if (speech_otrig(obj, actor, str, OCMD_ROOM)) {
			END_ITER(obj_iter);
			return;
		}
	} END_ITER(obj_iter);
}


int greet_otrigger(CharData *actor, int dir) {
	TrigData *t;
	ObjData *obj;
	
	START_ITER(obj_iter, obj, ObjData *, world[IN_ROOM(actor)].contents) {
		if (!SCRIPT_CHECK(obj, OTRIG_GREET))
			continue;

//		for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
		START_ITER(trig_iter, t, TrigData *, TRIGGERS(SCRIPT(obj))) {
			if (TRIGGER_CHECK(t, OTRIG_GREET) && (Number(1, 100) <= GET_TRIG_NARG(t))) {
				add_var(GET_TRIG_VARS(t), "direction", dirs[rev_dir[dir]]);
				ADD_UID_VAR(t, actor, "actor");
				END_ITER(trig_iter);
				END_ITER(obj_iter);
				return script_driver(obj, t, OBJ_TRIGGER, TRIG_NEW);
			}
		} END_ITER(trig_iter);
//		}
	} END_ITER(obj_iter);
	
	return 1;
}


int wear_otrigger(ObjData *obj, CharData *actor, int where) {
	TrigData *t;

	if (!SCRIPT_CHECK(obj, OTRIG_WEAR))
		return 1;

	START_ITER(iter, t, TrigData *, TRIGGERS(SCRIPT(obj))) {
		if (TRIGGER_CHECK(t, OTRIG_WEAR)) {
			ADD_UID_VAR(t, actor, "actor");
			END_ITER(iter);
			return script_driver(obj, t, OBJ_TRIGGER, TRIG_NEW);
		}
	} END_ITER(iter);
	return 1;
}


int consume_otrigger(ObjData *obj, CharData *actor) {
	TrigData *t;

	if (!SCRIPT_CHECK(obj, OTRIG_CONSUME))
		return 1;

	LListIterator<TrigData *>	iter(TRIGGERS(SCRIPT(obj)));
	while ((t = iter.Next())) {
		if (TRIGGER_CHECK(t, OTRIG_CONSUME) && (Number(1, 100) <= GET_TRIG_NARG(t))) {
			ADD_UID_VAR(t, actor, "actor");
			return script_driver(obj, t, OBJ_TRIGGER, TRIG_NEW);
		}
	}
	return 1;
}


int drop_otrigger(ObjData *obj, CharData *actor) {
	TrigData *t;

	if (!SCRIPT_CHECK(obj, OTRIG_DROP))
		return 1;

	LListIterator<TrigData *>	iter(TRIGGERS(SCRIPT(obj)));
	while ((t = iter.Next())) {
		if (TRIGGER_CHECK(t, OTRIG_DROP) && (Number(1, 100) <= GET_TRIG_NARG(t))) {
			ADD_UID_VAR(t, actor, "actor");
			return script_driver(obj, t, OBJ_TRIGGER, TRIG_NEW);
		}
	}

	return 1;
}


int give_otrigger(ObjData *obj, CharData *actor, CharData *victim) {
	TrigData *t;

	if (!SCRIPT_CHECK(obj, OTRIG_GIVE))
		return 1;

	LListIterator<TrigData *>	iter(TRIGGERS(SCRIPT(obj)));
	while ((t = iter.Next())) {
		if (TRIGGER_CHECK(t, OTRIG_GIVE) && (Number(1, 100) <= GET_TRIG_NARG(t))) {
			ADD_UID_VAR(t, actor, "actor");
			ADD_UID_VAR(t, victim, "victim");
			return script_driver(obj, t, OBJ_TRIGGER, TRIG_NEW);
		}
	}
	
	return 1;
}


int sit_otrigger(ObjData *obj, CharData *actor) {
	TrigData *t;

	if (!SCRIPT_CHECK(obj, OTRIG_SIT))
		return 1;

	LListIterator<TrigData *>	iter(TRIGGERS(SCRIPT(obj)));
	while ((t = iter.Next())) {
		if (TRIGGER_CHECK(t, OTRIG_SIT) && (Number(1, 100) <= GET_TRIG_NARG(t))) {
			ADD_UID_VAR(t, actor, "actor");
			return script_driver(obj, t, OBJ_TRIGGER, TRIG_NEW);
		}
	}
	
	return 1;
}


int leave_otrigger(CharData *actor, int dir) {
	TrigData *t;
	ObjData *obj;
	
	LListIterator<ObjData *>	obj_iter(world[IN_ROOM(actor)].contents);
	while ((obj = obj_iter.Next())) {
		if (!SCRIPT_CHECK(obj, OTRIG_LEAVE))
			continue;

		LListIterator<TrigData *>	iter(TRIGGERS(SCRIPT(obj)));
		while ((t = iter.Next())) {
			if (TRIGGER_CHECK(t, OTRIG_LEAVE) && (Number(1, 100) <= GET_TRIG_NARG(t))) {
				add_var(GET_TRIG_VARS(t), "direction", dirs[rev_dir[dir]]);
				ADD_UID_VAR(t, actor, "actor");
				return script_driver(obj, t, OBJ_TRIGGER, TRIG_NEW);
			}
		}
	}
	
	return 1;
}


int door_otrigger(CharData *actor, int subcmd, int dir) {
	TrigData *t;
	ObjData *obj;

	START_ITER(obj_iter, obj, ObjData *, world[IN_ROOM(actor)].contents) {
		if (!SCRIPT_CHECK(obj, MTRIG_DOOR))
			continue;

//		for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
		START_ITER(trig_iter, t, TrigData *, TRIGGERS(SCRIPT(obj))) {
			if (TRIGGER_CHECK(t, OTRIG_DOOR) && (Number(1, 100) <= GET_TRIG_NARG(t))) {
				add_var(GET_TRIG_VARS(t), "cmd", door_act[subcmd]);
				add_var(GET_TRIG_VARS(t), "direction", dirs[dir]);
				ADD_UID_VAR(t, actor, "actor");
				END_ITER(trig_iter);
				END_ITER(obj_iter);
				return script_driver(obj, t, OBJ_TRIGGER, TRIG_NEW);
			}
		} END_ITER(trig_iter);
//		}
	} END_ITER(obj_iter);
	return 1;
}


/*
 *  world triggers
 */

void random_wtrigger(RoomData *room) {
	TrigData *t;

	if (!SCRIPT_CHECK(room, WTRIG_RANDOM))
		return;

	LListIterator<TrigData *>	iter(TRIGGERS(SCRIPT(room)));
	while ((t = iter.Next())) {
		if (TRIGGER_CHECK(t, WTRIG_RANDOM) && (Number(1, 100) <= GET_TRIG_NARG(t))) {
			if (TRIGGER_CHECK(t, MTRIG_GLOBAL) || !is_empty(room->zone)) {
				script_driver(room, t, WLD_TRIGGER, TRIG_NEW);
				break;
			}
		}
	}
}


int enter_wtrigger(RoomData *room, CharData *actor, int dir) {
	TrigData *t;

	if (!SCRIPT_CHECK(room, WTRIG_ENTER))
		return 1;
	
	LListIterator<TrigData *>	iter(TRIGGERS(SCRIPT(room)));
	while ((t = iter.Next())) {
		if (TRIGGER_CHECK(t, WTRIG_ENTER) && (Number(1, 100) <= GET_TRIG_NARG(t))) {
			add_var(GET_TRIG_VARS(t), "direction", dirs[rev_dir[dir]]);
			ADD_UID_VAR(t, actor, "actor");
			return script_driver(room, t, WLD_TRIGGER, TRIG_NEW);
		}
	}
	return 1;
}


int command_wtrigger(CharData *actor, char *cmd, char *argument) {
	RoomData *room;
	TrigData *t;

	if (!actor || !SCRIPT_CHECK((&world[IN_ROOM(actor)]), WTRIG_COMMAND))
		return 0;

	room = &world[IN_ROOM(actor)];
	LListIterator<TrigData *>	iter(TRIGGERS(SCRIPT(room)));
	while ((t = iter.Next())) {
		if (TRIGGER_CHECK(t, WTRIG_COMMAND) && SSData(GET_TRIG_ARG(t)) &&
				!str_cmp(SSData(GET_TRIG_ARG(t)), cmd)) {
			ADD_UID_VAR(t, actor, "actor");
			skip_spaces(argument);
			add_var(GET_TRIG_VARS(t), "arg", argument);
			return script_driver(room, t, WLD_TRIGGER, TRIG_NEW);
		}
	}

	return 0;
}


void speech_wtrigger(CharData *actor, char *str) {
	RoomData *room;
	TrigData *t;

	if (!actor || (IN_ROOM(actor) == NOWHERE) || !SCRIPT_CHECK((&world[IN_ROOM(actor)]), WTRIG_SPEECH))
		return;

	room = &world[IN_ROOM(actor)];
	LListIterator<TrigData *>	iter(TRIGGERS(SCRIPT(room)));
	while ((t = iter.Next())) {
		if (TRIGGER_CHECK(t, WTRIG_SPEECH) && (!SSData(GET_TRIG_ARG(t)) ||
				(GET_TRIG_NARG(t) ?
				word_check(str, SSData(GET_TRIG_ARG(t))) :
				is_substring(SSData(GET_TRIG_ARG(t)), str)))) {
			ADD_UID_VAR(t, actor, "actor");
			script_driver(room, t, WLD_TRIGGER, TRIG_NEW);
			break;
		}
	}
}


int drop_wtrigger(ObjData *obj, CharData *actor) {
	RoomData *room;
	TrigData *t;

	if (!actor || !SCRIPT_CHECK((&world[IN_ROOM(actor)]), WTRIG_DROP))
		return 1;

	room = &world[IN_ROOM(actor)];
	LListIterator<TrigData *>	iter(TRIGGERS(SCRIPT(room)));
	while ((t = iter.Next())) {
		if (TRIGGER_CHECK(t, WTRIG_DROP) && (Number(1, 100) <= GET_TRIG_NARG(t))) {	
			ADD_UID_VAR(t, actor, "actor");
			ADD_UID_VAR(t, obj, "object");
			return script_driver(room, t, WLD_TRIGGER, TRIG_NEW);
		}	
	}
	
	return 1;
}


int leave_wtrigger(RoomData *room, CharData *actor, int dir) {
	TrigData *t;

	if (!SCRIPT_CHECK(room, WTRIG_LEAVE))
		return 1;

	LListIterator<TrigData *>	iter(TRIGGERS(SCRIPT(room)));
	while ((t = iter.Next())) {
		if (TRIGGER_CHECK(t, WTRIG_LEAVE) && (Number(1, 100) <= GET_TRIG_NARG(t))) {
			add_var(GET_TRIG_VARS(t), "direction", dirs[dir]);
			ADD_UID_VAR(t, actor, "actor");
			return script_driver(room, t, WLD_TRIGGER, TRIG_NEW);
		}
	}

	return 1;
}


int door_wtrigger(CharData *actor, int subcmd, int dir) {
	RoomData *room;
	TrigData *t;

	if (!actor || !SCRIPT_CHECK(&world[IN_ROOM(actor)], WTRIG_DOOR))
		return 1;

	room = &world[IN_ROOM(actor)];
	LListIterator<TrigData *>	iter(TRIGGERS(SCRIPT(room)));
	while ((t = iter.Next())) {
		if (TRIGGER_CHECK(t, WTRIG_DOOR) && (Number(1, 100) <= GET_TRIG_NARG(t))) {
			add_var(GET_TRIG_VARS(t), "cmd", door_act[subcmd]);
			add_var(GET_TRIG_VARS(t), "direction", dirs[dir]);
			ADD_UID_VAR(t, actor, "actor");
			return script_driver(room, t, WLD_TRIGGER, TRIG_NEW);
		}
	}

	return 1;
}


