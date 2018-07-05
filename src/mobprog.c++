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
*  such installation can be found in INSTALL.  Enjoy...         N'Atas-Ha *
***************************************************************************/
 

#include "structs.h"
#include "utils.h"
#include "interpreter.h"
#include "handler.h"
#include "find.h"
#include "db.h"
#include "clans.h"
#include "olc.h"
#include "event.h"
#include "buffer.h"


extern char *dirs[];
extern char *affected_bits[];
extern char *positions[];
extern char *equipment_types[];
extern char *race_types[];
extern char *action_bits[];

extern void death_cry (CharData *ch);
extern int str_prefix (const char *astr, const char *bstr);
extern int number_percent(void);
extern int number_range(int from, int to);
extern char* medit_get_mprog_type(struct mob_prog_data *mprog);
extern struct TimeInfoData time_info;
//extern char * ifchk[];

/*#define bug(x) mudlogf(NRM, LVL_BUILDER, TRUE, x ": vnum %d, room %d.",\
		mob_index[ch->nr].vnum, world[IN_ROOM(ch)].number)
#define sbug(x, y) mudlogf(NRM, LVL_BUILDER, TRUE, x ": vnum %d, room %d, arg %s.",\
		mob_index[ch->nr].vnum, world[IN_ROOM(ch)].number, (y))
#define dbug(x, y) mudlogf(NRM, LVL_BUILDER, TRUE, x ": vnum %d, room %d, val %d.",\
		mob_index[ch->nr].vnum, world[IN_ROOM(ch)].number, (y))
#define sdbug(x, y, z) mudlogf(NRM, LVL_BUILDER, TRUE, x ": vnum %d, room %d, val %d, arg %s.",\
		mob_index[ch->nr].vnum, world[IN_ROOM(ch)].number, (y), (z)) 
*/
#define bug(x, y) mudlogf(NRM, LVL_STAFF, FALSE, x, y)
#define bug2(x, y, z) mudlogf(NRM, LVL_STAFF, FALSE, x, y, z)
#define bug3(x, y, z, a) mudlogf(NRM, LVL_STAFF, FALSE, x, y, z, a)
#define bug4(x, y, z, a, b) mudlogf(NRM, LVL_STAFF, FALSE, x, y, z, a, b)
/*
 * Local function prototypes
 */

int     mprog_seval		(const char *lhs, int opr, const char *rhs);
int	mprog_veval		(int lhs, int opr, int rhs);
int	mprog_do_ifchck	(char *prog_type, char *line, int check, CharData* mob,
			       CharData* actor, Ptr arg1,
			       Ptr arg2, CharData* rndm);
void mprog_translate(char *buf, const char *format, CharData *mob, CharData *actor,
                    Ptr arg1, Ptr arg2, CharData *rndm);
void	mprog_process_cmnd	(char* cmnd, CharData* mob, 
			       CharData* actor, ObjData* obj,
			       Ptr vo, CharData* rndm);
void	mprog_driver (struct mob_prog_data *mprog, CharData* mob,
			       CharData* actor, Ptr arg1,
			       Ptr arg2, int single_step);


int str_infix(const char *astr, const char *bstr);
void mprog_wordlist_check(char *arg, CharData *mob, CharData *actor,
		  ObjData *obj, Ptr vo, int type);
int mprog_percent_check(CharData *mob, CharData *actor, ObjData *obj,
		 Ptr vo, int type);
void mprog_act_trigger(char *buf, CharData *mob, CharData *ch,
	       ObjData *obj, Ptr vo);
void mprog_bribe_trigger(CharData *mob, CharData *ch, int amount);
void mprog_death_trigger(CharData *mob, CharData *killer);
void mprog_entry_trigger(CharData *mob);
void mprog_fight_trigger(CharData *mob, CharData *ch);
void mprog_give_trigger(CharData *mob, CharData *ch, ObjData *obj);
void mprog_greet_trigger(CharData *ch);
void mprog_hitprcnt_trigger(CharData *mob, CharData *ch);
void mprog_random_trigger(CharData *mob);
void mprog_speech_trigger(char *txt, CharData *mob);
void mprog_script_trigger(CharData *mob);

int get_mob_vnum_room(CharData *ch, int vnum );
int get_obj_vnum_room(CharData *ch, VNum vnum );
int count_people_room(CharData *mob, int iFlag );
int get_order(CharData *ch);
int has_item(CharData *ch, int vnum, int item_type, int fWear );
int item_lookup(char *name);
char *capitalize(char *str);

int mprog_exit_trigger(CharData *ch, int dir);
EVENT(mprog_delay_trigger);

char * ifchk[] = {
	"rand",			/* 0 */
	"ispc",			/* 1 */
	"isnpc",		/* 2 */
	"isfight",		/* 3 */
	"isimmort",		/* 4 */
	"ischarmed",	/* 5 */
	"isfollow",		/* 6 */
	"affected",	    /* 7 */
	"hitprcnt",		/* 8 */
	"inroom",		/* 9 */
	"sex",			/* 10 */
	"position",		/* 11 */
	"level",		/* 12 */
	"race",			/* 13 */
	"mp",			/* 14 */
	"objtype",		/* 15 */
	"objval0",		/* 16 */
	"objval1",		/* 17 */
	"objval2",		/* 18 */
	"objval3",		/* 19 */
	"objval4",		/* 20 */
	"objval5",		/* 21 */
	"objval6",		/* 22 */
	"objval7",		/* 23 */
	"number",		/* 24 */
	"name",			/* 25 */
	"clan",			/* 26 */
	// New
	"mobhere",		/* 27 */
	"objhere",		/* 28 */
	"mobexists",	/* 29 */
	"objexists",	/* 30 */
	"people",		/* 31 */
	"players",		/* 32 */
	"mobs",			/* 33 */
	"clones",		/* 34 */
	"order",		/* 35 */
	"hour",			/* 36 */
	"isactive",		/* 37 */
	"isdelay",		/* 38 */
	"isvisible",	/* 39 */
	"hastarget",	/* 40 */
	"istarget",		/* 41 */
	"exists",		/* 42 */
	"mobflagged",	/* 43 */
	"carries",		/* 44 */
	"wears",		/* 45 */
	"has",			/* 46 */
	"grpsize",		/* 47 */
	"uses",			/* 48 */
	"mobamount",	/* 49 */
	"objamount",	/* 50 */
	"\n"
};


char *sevals[] = {
    "==",			/* 0 */
    "!=",			/* 1 */
    "/",			/* 2 */
    "!/",			/* 3 */
    "\n"
};

char *vevals[] = {
    "==",			/* 0 */
    "!=",			/* 1 */
    ">",			/* 2 */
    "<",			/* 3 */
    ">=",			/* 4 */
    "<=",			/* 5 */
    "&",			/* 6 */
    "|",			/* 7 */
    "\n"
};



/***************************************************************************
 * Local function code and brief comments.
 */
 
/* Used to get sequential lines of a multi line string (separated by "\r\n")
 * Thus its like one_argument(), but a trifle different. It is destructive
 * to the multi line string argument, and thus clist must not be shared.
 */
 


/*
 * Check if there's a mob with given vnum in the room
 */
int get_mob_vnum_room(CharData *ch, int vnum ) {
    CharData *mob;
    START_ITER(iter, mob, CharData *, world[IN_ROOM(ch)].people) {
		if (IS_NPC(mob) && (GET_MOB_VNUM(mob) == vnum)) {
			END_ITER(iter);
			return TRUE;
		}
	} END_ITER(iter);
    return FALSE;
}
/*
 * Check if there's an object with given vnum in the room
 */
int get_obj_vnum_room(CharData *ch, VNum vnum ) {
    ObjData *obj;
	START_ITER(iter, obj, ObjData *, world[IN_ROOM(ch)].contents) {
		if (GET_OBJ_VNUM(obj) == vnum ) {
			END_ITER(iter);
		    return TRUE;
		}
	} END_ITER(iter);
    return FALSE;
}


/* 
 * How many other players / mobs are there in the room
 * iFlag: 0: all, 1: players, 2: mobiles 3: mobs w/ same vnum 4: same group
 */
int count_people_room(CharData *mob, int iFlag ) {
    CharData *vch;
    int count;
    
    count = 0;
	START_ITER(iter, vch, CharData *, world[IN_ROOM(mob)].people) {
		if ( mob != vch && (iFlag == 0
				|| (iFlag == 1 && !IS_NPC( vch )) 
				|| (iFlag == 2 && IS_NPC( vch ))
				|| (iFlag == 3 && IS_NPC( mob ) && IS_NPC( vch ) 
						&& GET_MOB_VNUM(mob) == GET_MOB_VNUM(vch) )
				|| (iFlag == 4 && is_same_group( mob, vch )) )
						&& mob->CanSee(vch)) 
			count++;
	} END_ITER(iter);
    return (count);
}


int get_order(CharData *ch) {
	CharData *vch;
	int i;

	if ( !IS_NPC(ch) )
		return 0;
	i = 0;
	START_ITER(iter, vch, CharData *, world[IN_ROOM(ch)].people) {
		if ( vch == ch ) {
			END_ITER(iter);
			return i;
		}
		if (IS_NPC(vch) && GET_MOB_VNUM(vch) == GET_MOB_VNUM(ch) )
			i++;
	} END_ITER(iter);
	return 0;
}


int has_item(CharData *ch, int vnum, int item_type, int fWear ) {
	ObjData *obj;
	START_ITER(iter, obj, ObjData *, ch->carrying) {
		if ((vnum < 0 || GET_OBJ_VNUM(obj) == vnum)
				&& (item_type < 0 || GET_OBJ_TYPE(obj) == item_type)
				&& (!fWear || GET_OBJ_WEAR(obj))) {
			END_ITER(iter);
			return TRUE;
		}
	} END_ITER(iter);
	return FALSE;
}


int item_lookup(char *name) {
	int type;
	
	for (type = 0; type < top_of_objt; type++) {
		if (is_abbrev(name, SSData(((ObjData *)obj_index[type].proto)->name)))
			return ((ObjData *)obj_index[type].proto)->type;
	}
	return -1;
}


/*
 * Returns an initial-capped string.
 */
char *capitalize(char *str) {
	int i;
    for (i = 0; str[i]; i++)
		str[i] = LOWER(str[i]);
    str[0] = UPPER(str[0]);
    return str;
}


/* we need str_infix here because strstr is not case insensitive */

int str_infix(const char *astr, const char *bstr)
{
    int sstr1;
    int sstr2;
    int ichar;
    char c0;

    if ((c0 = LOWER(astr[0])) == '\0')
        return FALSE;

    sstr1 = strlen(astr);
    sstr2 = strlen(bstr);

    for (ichar = 0; ichar <= sstr2 - sstr1; ichar++) {
        if (c0 == LOWER(bstr[ichar]) && !str_prefix(astr, bstr + ichar))
            return FALSE;
    }

    return TRUE;
}

/* These two functions do the basic evaluation of ifcheck operators.
*  It is important to note that the string operations are not what
*  you probably expect.  Equality is exact and division is substring.
*  remember that lhs has been stripped of leading space, but can
*  still have trailing spaces so be careful when editing since:
*  "guard" and "guard " are not equal.
*/
int mprog_seval(const char *lhs, int opr, const char *rhs) {
	switch (opr /*search_block(opr, sevals, TRUE)*/) {
		case 0:	return (!str_cmp(lhs, rhs));	// ==
		case 1:	return (str_cmp(lhs, rhs));		// !=
		case 2:	return (!str_infix(rhs, lhs));	// /
		case 3:	return (str_infix(rhs, lhs));	// !/
		default:
			log("Improper MOBProg operator");
			return 0;
	}
	log("SYSERR: Exited switch in mprog_seval");
	return 0;	
/*	if (!str_cmp(opr, "=="))	return (!str_cmp(lhs, rhs));
	if (!str_cmp(opr, "!="))	return (str_cmp(lhs, rhs));
	if (!str_cmp(opr, "/"))		return (!str_infix(rhs, lhs));
	if (!str_cmp(opr, "!/"))	return (str_infix(rhs, lhs));

	log("Improper MOBprog operator");
	return 0;
*/
}

int mprog_veval(int lhs, int opr, int rhs) {
	switch (opr /*search_block(opr, vevals, TRUE)*/) {
		case 0:	return (lhs == rhs);	// ==
		case 1:	return (lhs != rhs);	// !=
		case 2:	return (lhs > rhs);		// >
		case 3:	return (lhs < rhs);		// <
		case 4:	return (lhs >= rhs);	// >=
		case 5:	return (lhs <= rhs);	// <=
		case 6:	return (lhs & rhs);		// &
		case 7:	return (lhs | rhs);		// |
		default:
			log("Improper MOBProg operator");
			return 0;
	}
	log("SYSERR: Exited switch in mprog_veval");
	return 0;
/*	if (!str_cmp(opr, "=="))	return (lhs == rhs);
	if (!str_cmp(opr, "!="))	return (lhs != rhs);
	if (!str_cmp(opr, ">"))		return (lhs > rhs);
	if (!str_cmp(opr, "<"))		return (lhs < rhs);
	if (!str_cmp(opr, "<="))	return (lhs <= rhs);
	if (!str_cmp(opr, ">="))	return (lhs >= rhs);
	if (!str_cmp(opr, "&"))		return (lhs & rhs);
	if (!str_cmp(opr, "|"))		return (lhs | rhs);

	log("Improper MOBProg operator");
	return 0;
*/
}

/* This function performs the evaluation of the if checks.  It is
 * here that you can add any ifchecks which you so desire. Hopefully
 * it is clear from what follows how one would go about adding your
 * own. The syntax for an if check is: ifchck (arg) [opr val]
 * where the parenthesis are required and the opr and val fields are
 * optional but if one is there then both must be. The spaces are all
 * optional. The evaluation of the opr expressions is farmed out
 * to reduce the redundancy of the mammoth if statement list.
 * If there are errors, then return -1 otherwise return boolean 1,0
 */
int mprog_do_ifchck(char * prog_type, char *line, int check, CharData *mob, CharData *actor,
		Ptr arg1, Ptr arg2, CharData *rndm) {

	CharData *lval_char = mob;
	CharData *vict = (CharData *)arg2;
	ObjData *obj1 = (ObjData *)arg1;
	ObjData *obj2 = (ObjData *)arg2;
	ObjData *lval_obj = NULL;
	
	char *original = line;
	char buf[MAX_INPUT_LENGTH];
	char code;
	int lval = 0, oper = 0, rval = -1;
	int temp_num;
	
	line = one_argument(line, buf);
	
	if (!*buf || !mob)
		return FALSE;

	/* No target?  Assume the victim is it. */
	if (!TARGET(mob))
		TARGET(mob) = actor;
	
	switch (check) {	//	Case 1: keyword and value
		case 0:		// rand
			return (Number(0, 100) <= atoi(buf));
		case 27:	// mobhere
			if (is_number(buf))	return(get_mob_vnum_room(mob, atoi(buf)));
			else				return(get_char_room_vis(mob, buf) != NULL);
		case 28:	// objhere
			if (is_number(buf))	return(get_obj_vnum_room(mob, atoi(buf)));
			else				return(IN_ROOM(mob) != NOWHERE && get_obj_in_list_vis(mob, buf, world[IN_ROOM(mob)].contents) != NULL);
		case 29:	// mobexists
			return (get_char_vis(mob, buf) != NULL);
		case 30:	// objexists
			return (get_obj_vis(mob, buf) != NULL);
/* Case 2 begins here... sneakily use rval to indicate need for numeric eval... */
		case 31:	// people
			rval = count_people_room(mob, 0);	break;
		case 32:	// players
			rval = count_people_room(mob, 1);	break;
		case 33:	// mobs
			rval = count_people_room(mob, 2);	break;
		case 34:	// clones
			rval = count_people_room(mob, 3);	break;
		case 35:	// order
			rval = get_order(mob);	break;
		case 36:	// hour
			rval = time_info.hours;	break;
		case 49:
			if (is_number(buf))
				temp_num = real_mobile(atoi(buf));
			else {
				for (temp_num = 0; temp_num <= top_of_mobt; temp_num++) {
					if (isname(buf, SSData(((CharData *)mob_index[temp_num].proto)->general.name)))
						break;
				}
				if (temp_num > top_of_mobt)
					temp_num = -1;
			}
			if (temp_num > -1)
				rval = mob_index[temp_num].number;
		case 50:
			if (is_number(buf))
				temp_num = real_object(atoi(buf));
			else {
				for (temp_num = 0; temp_num <= top_of_objt; temp_num++) {
					if (isname(buf, SSData(((ObjData *)obj_index[temp_num].proto)->shortDesc)))
						break;
				}
				if (temp_num > top_of_objt)
					temp_num = -1;
			}
			if (temp_num > -1)
				rval = obj_index[temp_num].number;
	}
	
	/* Case 2 continued: evauluate expression */
	if (rval >= 0) {
		if ((oper = search_block(buf, vevals, TRUE)) < 0) {
			bug3("MOBProg: mob %d prog %s syntax error 2 '%s'", GET_MOB_VNUM(mob), prog_type, original);
			return FALSE;
		}
		one_argument(line, buf);
		lval = rval;
		rval = atoi(buf);
		return(mprog_veval(lval, oper, rval));
	}
	
	/* Case 3, 4, 5: Grab actors from $* codes */
	if (buf[0] != '$' || !buf[1]) {
		bug3("MOBProg: mob %d prog %s syntax error 3 '%s'", GET_MOB_VNUM(mob), prog_type, original);
		return FALSE;
	} else
		code = buf[1];
		
	switch(code) {
		case 'i':	lval_char = mob;	break;
		case 'n':	lval_char = actor;	break;
		case 't':	lval_char = vict;	break;
		case 'r':	lval_char = (rndm == NULL) ? actor : rndm; break;
		case 'o':	lval_obj = obj1;	break;
		case 'p':	lval_obj = obj2;	break;
		case 'q':	lval_char = TARGET(mob);	break;
		default:
			bug4("MOBProg: mob %d prog %s syntax error 4 (Invalid $ code %c) '%s'", GET_MOB_VNUM(mob), prog_type, code, original);
			return FALSE;
	}
	/* From now on we need an actor, so if none was found, bail out */
	if (lval_char == NULL && lval_obj == NULL)
		return FALSE;
	
	/* Case 3: Keyword, comparison and value */
	switch (check) {
		case 1:		// ispc
			return (lval_char && !IS_NPC(lval_char));
		case 2:		// isnpc
			return (lval_char && IS_NPC(lval_char));
		case 3:		// isfight
			return (lval_char && FIGHTING(lval_char));
		case 4:		// isimmort
			return (lval_char && NO_STAFF_HASSLE(lval_char));
		case 5:		// ischarmed
			return (lval_char && AFF_FLAGGED(lval_char, AFF_CHARM));
		case 6:		// isfollow
			return (lval_char && lval_char->master && (IN_ROOM(lval_char->master) == IN_ROOM(lval_char)));
		case 37:	// isactive
			return (lval_char && (GET_POS(lval_char) > POS_SLEEPING));
		case 38:	// isdelay
			return (lval_char && (lval_char->mprog_delay > 0));
		case 39:	// isvisible
			switch(code) {
				default:
				case 'i':
				case 'n':
				case 't':
				case 'r':
				case 'q':
					return (lval_char && mob->CanSee(lval_char));
				case 'o':
				case 'p':
					return (lval_obj && mob->CanSee(lval_obj));
			}
		case 40:	// hastarget
			return (lval_char && TARGET(lval_char) && (IN_ROOM(TARGET(lval_char)) == IN_ROOM(lval_char)));
		case 41:	// istarget
			return (lval_char && (TARGET(mob) == lval_char));
	}
	
	/* Case 4: Keyword, actor, and value */
	line = one_argument(line, buf);
	switch (check) {
		case 7:		// isaffected
			return (lval_char && ((temp_num = search_block(buf, affected_bits, TRUE)) >= 0) && AFF_FLAGGED(lval_char, 1 << temp_num));
		case 15:	// objtype
			return (lval_obj && ((temp_num = search_block(buf, equipment_types, TRUE)) >= 0) && (GET_OBJ_TYPE(lval_obj) == temp_num));
		case 43:	// mobflagged
			return (lval_char && ((temp_num = search_block(buf, action_bits, TRUE)) >= 0) && MOB_FLAGGED(lval_char, 1 << temp_num));
		case 44:	// carries
			if (is_number(buf))	return (lval_char && has_item(lval_char, atoi(buf), -1, FALSE));
			else				return (lval_char && get_obj_in_list_vis(mob, buf, lval_char->carrying));
		case 45:	// wears
			if (is_number(buf))	return (lval_char && has_item(lval_char, atoi(buf), -1, TRUE));
			else {
				int j = 0;
				return (lval_char && get_object_in_equip_vis(mob, buf, lval_char->equipment, &j));
			}
		case 46:	// has
			return (lval_char && has_item(lval_char, -1, item_lookup(buf), FALSE));
		case 48:	// uses
			return (lval_char && has_item(lval_char, -1, item_lookup(buf), TRUE));
	}
	
	/* Case 5: Keyword, actor, comparison and value */
	if (((oper = search_block(buf, vevals, TRUE)) < 0) && ((oper = search_block(buf, sevals, TRUE)) < 0)) {
		bug3("MOBProg: mob %d prog %s syntax error 2 '%s'", GET_MOB_VNUM(mob), prog_type, original);
		return FALSE;
	}
	one_argument(line, buf);
	rval = atoi(buf);
	
	switch(check) {
		case 8:		// hitprcnt
			if (lval_char)	lval = (GET_HIT(lval_char) * 100)/GET_MAX_HIT(lval_char);
			break;
		case 9:		// inroom
			if (lval_char && IN_ROOM(lval_char) != NOWHERE)
				lval = world[IN_ROOM(lval_char)].number;
			break;
		case 10:	// sex
			if (lval_char)	lval = GET_SEX(lval_char);					break;
		case 11:	// position
			if (lval_char)	{
				if (!is_number(buf))
					rval = search_block(buf, positions, TRUE);
				lval = GET_POS(lval_char);
			}
			break;
//	Previously a Case 4 check
//			return (lval_char && ((temp_num = search_block(buf, position_types, TRUE)) >= 0) && (GET_POS(lval_char) == temp_num));
		case 12:	// level
			if (lval_char)	lval = GET_LEVEL(lval_char);				break;
		case 13:	// race
			if (lval_char)	{
				if (!is_number(buf))
					rval = search_block(buf, race_types, TRUE);
				lval = GET_RACE(lval_char);
			}
			break;
//	Previously a Case 4 check
//			return (lval_char && ((temp_num = search_block(buf, race_types, TRUE)) >= 0) && (GET_RACE(lval_char) == temp_num));
		case 14:	// mp
			if (lval_char)	lval = GET_MISSION_POINTS(lval_char);		break;
		case 16:	// objval0
		case 17:	// objval1
		case 18:	// objval2
		case 19:	// objval3
		case 20:	// objval4
		case 21:	// objval5
		case 22:	// objval6
		case 23:	// objval7
			if (lval_obj)	lval = GET_OBJ_VAL(lval_obj, check - 16);	break;
		case 24:	// number
			switch (code) {
				default:
				case 'i':
				case 'n':
				case 't':
				case 'r':
				case 'q':
					if (lval_char && IS_NPC(lval_char))
						lval = GET_MOB_VNUM(lval_char);
					break;
				case 'o':
				case 'p':
					if (lval_obj)
						lval = GET_OBJ_VNUM(lval_obj);
					break;
			}
			break;
		case 25:	// name
			switch (code) {
				default:
				case 'i':
				case 'n':
				case 't':
				case 'r':
				case 'q':
					if (lval_char)
						return (mprog_seval(buf, oper, lval_char->RealName()));
					else return 0;
				case 'o':
				case 'p':
					if (lval_obj)
						return (mprog_seval(buf, oper, SSData(lval_obj->name)));
					else return 0;
			}
		case 26:	// clan
			if (lval_char)	lval = GET_CLAN(lval_char);					break;
//	Previously a Case 4 check
//			return (lval_char && (GET_CLAN(lval_char) == atoi(buf)));
		case 42:	// exists
		case 47:	// grpsize
			if (lval_char)	lval = count_people_room(lval_char, 4);		break;
	}
	return (mprog_veval(lval, oper, rval));

}



/* This routine handles the variables for command expansion.
 * If you want to add any go right ahead, it should be fairly
 * clear how it is done and they are quite easy to do, so you
 * can be as creative as you want. The only catch is to check
 * that your variables exist before you use them. At the moment,
 * using $t when the secondary target refers to an object 
 * i.e. >prog_act drops~<nl>if ispc($t)<nl>sigh<nl>endif<nl>~<nl>
 * probably makes the mud crash (vice versa as well) The cure
 * would be to change act() so that vo becomes vict & v_obj.
 * but this would require a lot of small changes all over the code.
 */
void mprog_translate(char *buf, const char *format, CharData *mob, CharData *actor,
                    Ptr arg1, Ptr arg2, CharData *rndm) {
    const char *someone = "someone";
    const char *something = "something";
    const char *someones = "someone's";
    
    char *fname;
	CharData	*vict	= (CharData *) arg2;
	ObjData		*obj1	= (ObjData  *) arg1;
	ObjData		*obj2	= (ObjData  *) arg2;
	const char *str;
	const char *i;
	char *point;

	if (!format || !*format)
		return;
		
	point	= buf;
	str		= format;
	fname	= get_buffer(MAX_STRING_LENGTH);
	
	while (*str != '\0') {
		if (*str != '$') {
			*point++ = *str++;
			continue;
		}
		++str;
		switch (*str) {
			case 'i':	one_argument(SSData(mob->general.name), fname);	i = fname;	break;
			case 'I':	i = SSData(mob->general.shortDesc);	break;
			case 'n':	i = someone;
				if (actor && mob->CanSee(actor)) {
					one_argument(SSData(actor->general.name), fname);
					i = capitalize(fname);
				}	break;
			case 'N':	i = PERS(mob, actor);	break;
			case 't':	i = someone;
				if (vict && mob->CanSee(vict)) {
					one_argument(SSData(vict->general.name), fname);
					i = capitalize(fname);
				}	break;
			case 'T':	i = (vict) ? PERS(mob, vict) : someone;	break;
			case 'r':	i = someone;
				if (rndm) {
					one_argument(SSData(rndm->general.name), fname);
					i = capitalize(fname);
				}	break;
			case 'R':	i = (rndm) ? PERS(mob, rndm) : someone; break;
			case 'q':	i = someone;
				if (TARGET(mob) && mob->CanSee(TARGET(mob))) {
					one_argument(SSData(TARGET(mob)->general.name), fname);
					i = capitalize(fname);
				}	break;
			case 'Q':	i = TARGET(mob) ? PERS(mob, TARGET(mob)) : someone;	break;
			case 'e':	i = (actor && mob->CanSee(actor)) ? HSSH(actor) : someone;	break;
			case 'E':	i = (vict && mob->CanSee(vict)) ? HSSH(vict) : someone;	break;
			case 'j':	i = HSSH(mob);	break;
			case 'J':	i = (rndm && mob->CanSee(rndm)) ? HSSH(rndm) : someone;	break;
			case 'X':	i = (TARGET(mob) && mob->CanSee(TARGET(mob))) ? HSSH(TARGET(mob)): someone;	break;
			case 'k':	i = (actor && mob->CanSee(actor)) ? HMHR(actor) : someone;	break;
			case 'K':	i = (vict && mob->CanSee(vict)) ? HMHR(vict) : someone;	break;
			case 'm':	i = HMHR(mob);	break;
			case 'M':	i = (rndm && mob->CanSee(rndm)) ? HMHR(rndm) : someone;	break;
			case 'Y':	i = (TARGET(mob) && mob->CanSee(TARGET(mob))) ? HMHR(TARGET(mob)): someone;	break;
			case 'l':	i = (actor && mob->CanSee(actor)) ? HSHR(actor) : someone;	break;
			case 'L':	i = (vict && mob->CanSee(vict)) ? HSHR(vict) : someone;	break;
			case 's':	i = HSHR(mob);	break;
			case 'S':	i = (rndm && mob->CanSee(rndm)) ? HSHR(rndm) : someone;	break;
			case 'Z':	i = (TARGET(mob) && mob->CanSee(TARGET(mob))) ? HSHR(TARGET(mob)): someone;	break;
			case 'o':	i = something;
				if (obj1 && mob->CanSee(obj1)) {
					one_argument(SSData(obj1->name), fname);
					i = fname;
				}	break;
			case 'O':	i = (obj1 && mob->CanSee(obj1)) ? SSData(obj1->shortDesc) : something;	break;
			case 'p':	i = something;
				if (obj2 && mob->CanSee(obj2)) {
					one_argument(SSData(obj2->name), fname);
					i = fname;
				}	break;
			case 'P':	i = (obj2 && mob->CanSee(obj2)) ? SSData(obj2->shortDesc) : something;	break;
			default:
				bug2("MOBProg: Mob %d has bad expand argument code %c.", GET_MOB_VNUM(mob), *str);
				i = " <@@@> ";
				break;
		}
		++str;
		while ((*point = *i) != '\0')
			++point, ++i;
	}
	*point = '\0';
	release_buffer(fname);
	return;
}



#define MAX_NESTED_LEVEL 12 /* Maximum nested if-else-endif's (stack size) */
#define BEGIN_BLOCK       0 /* Flag: Begin of if-else-endif block */
#define IN_BLOCK         -1 /* Flag: Executable statements */
#define END_BLOCK        -2 /* Flag: End of if-else-endif block */
#define IF_RUN			  1
#define IF_NOTRUN		  2
#define MAX_CALL_LEVEL    5 /* Maximum nested calls */
/* The main focus of the MOBprograms.  This routine is called 
 *  whenever a trigger is successful.  It is responsible for parsing
 *  the command list and figuring out what to do. However, like all
 *  complex procedures, everything is farmed out to the other guys.
 */
void mprog_driver (struct mob_prog_data *mprog, CharData *mob, CharData *actor,
		Ptr arg1, Ptr arg2, int single_step) {
	
	char *code = mprog->comlist;
	char *buf, *control, *data;
	char *line;
	char *prog_type = medit_get_mprog_type(mprog);
	CharData *rndm	= NULL;
	CharData *vict	= NULL;
	static int call_level;	// Keep track of nested MPCALLs
	int count = 0;
    int level, eval, check;
    int state[MAX_NESTED_LEVEL], /* Block state (BEGIN,IN,END) */
		cond[MAX_NESTED_LEVEL],  /* Boolean value based on the last if-check */
		elseState[MAX_NESTED_LEVEL];
	int mob_num = mob_index[mob->nr].vnum;
	
	if (AFF_FLAGGED(mob, AFF_CHARM))
		return;

	if (!code || !*code) {
		bug3("MOBProg: Mob %d has %s command list in mprog %s", mob_num, !code? "nonexistant" : "empty", prog_type);
		return;
	}
	
    if (++call_level > MAX_CALL_LEVEL) {
		bug( "MOBProg: MAX_CALL_LEVEL exceeded, mob %d", mob_num );
		call_level--;
		return;
    }


    /*
     * Reset "stack"
     */
    for (level = 0; level < MAX_NESTED_LEVEL; level++) {
    	state[level] = IN_BLOCK;
        cond[level]  = TRUE;
        elseState[level] = IF_NOTRUN;
    }
    level = 0;

	/* get a random visable mortal player who is in the room with the mob */
	START_ITER(iter, vict, CharData *, world[IN_ROOM(mob)].people) {
		if (!IS_NPC(vict) && vict->general.level < LVL_IMMORT && mob->CanSee(vict)) {
			if (Number(0, count) == 0)
				rndm = vict;
				count++;
		}
	} END_ITER(iter);

//	tmpcmndlst = get_buffer(MAX_STRING_LENGTH);
	buf		= get_buffer(MAX_STRING_LENGTH);
	control	= get_buffer(MAX_STRING_LENGTH);
	data	= get_buffer(MAX_STRING_LENGTH);

	while (*code) {
		int first_arg = TRUE;
		char *b = buf, *c = control, *d = data;
		/*
	 	 * Get a command line. We sneakily get both the control word
	 	 * (if/and/or) and the rest of the line in one pass.
		 */
		while (isspace(*code) && *code) code ++;
		while (*code) {
			if (*code == '\r' || *code == '\n')
				break;
			else if (isspace(*code)) {
				if (first_arg)	first_arg = FALSE;
				else			*d++ = *code;
			} else {
				if (first_arg)	*c++ = *code;
				else			*d++ = *code;
			}
			*b++ = *code++;
		}
		*b = *c = *d = '\0';
		if (buf[0] == '\0')
			break;
		if (buf[0] == '*')
			continue;
			
		line = data;
		if (!str_cmp(control, "if")) {
			if (state[level] == BEGIN_BLOCK) {
				bug2("MOBProg: misplaced if statement: mob %d, prog %s", mob_num, prog_type);				
				call_level--;
				release_buffer(buf);
				release_buffer(control);
				release_buffer(data);
				return;
			}
			state[level] = BEGIN_BLOCK;
			if (++level >= MAX_NESTED_LEVEL) {
				bug2("MOBProg: Max nested level exceeded, mob %d, prog %s", mob_num, prog_type);
				call_level--;
				release_buffer(buf);
				release_buffer(control);
				release_buffer(data);
				return;
			}
			if (level && cond[level-1] == FALSE) {
				cond[level] = FALSE;
				continue;
			}
			line = one_argument(line, control);
			if ((check = search_block(control, ifchk, TRUE)) >= 0)
				cond[level] = mprog_do_ifchck(prog_type, line, check, mob, actor, arg1, arg2, rndm);
			else {
				bug2("MOBProg: invalid IF check, mob %d prog %s", mob_num, prog_type);
				call_level--;
				release_buffer(buf);
				release_buffer(control);
				release_buffer(data);
				return;
			}
			if (cond[level])	elseState[level] = IF_RUN;
			state[level] = END_BLOCK;
		} else if (!str_cmp(control, "or")) {
			if (!level || state[level-1] != BEGIN_BLOCK) {
				bug2("MOBProg: OR without IF, mob %d, prog %s", mob_num, prog_type);
				call_level--;
				release_buffer(buf);
				release_buffer(control);
				release_buffer(data);
				return;
			}
			if (cond[level-1] == FALSE)
				continue;
			line = one_argument(line, control);
			if ((check = search_block(control, ifchk, TRUE)) >= 0)
				eval = mprog_do_ifchck(prog_type, line, check, mob, actor, arg1, arg2, rndm);
			else {
				bug2("MOBProg: invalid IF check in OR, mob %d prog %s", mob_num, prog_type);
				call_level--;
				release_buffer(buf);
				release_buffer(control);
				release_buffer(data);
				return;
			}
			cond[level] = (eval == TRUE) ? TRUE : cond[level];
			if (cond[level])	elseState[level] = IF_RUN;
			else				elseState[level] = IF_NOTRUN;
		} else if (!str_cmp(control, "and")) {
			if (!level || state[level-1] != BEGIN_BLOCK) {
				bug2("MOBProg: AND without IF, mob %d, prog %s", mob_num, prog_type);
				call_level--;
				release_buffer(buf);
				release_buffer(control);
				release_buffer(data);
				return;
			}
			if (cond[level-1] == FALSE)
				continue;
			line = one_argument(line, control);
			if ((check = search_block(control, ifchk, TRUE)) >= 0)
				eval = mprog_do_ifchck(prog_type, line, check, mob, actor, arg1, arg2, rndm);
			else {
				bug2("MOBProg: invalid IF check in AND, mob %d prog %s", mob_num, prog_type);
				call_level--;
				release_buffer(buf);
				release_buffer(control);
				release_buffer(data);
				return;
			}
			cond[level] = (cond[level] == TRUE) && (eval == TRUE);
			if (cond[level])	elseState[level] = IF_RUN;
			else				elseState[level] = IF_NOTRUN;
		} else if (!str_cmp(control, "elseif")) {
			if (!level || state[level-1] != BEGIN_BLOCK) {
				bug2("MOBProg: ELSEIF without IF, mob %d, prog %s", mob_num, prog_type);	
				call_level--;
				release_buffer(buf);
				release_buffer(control);
				release_buffer(data);
				return;
			}
			if ((cond[level-1] == FALSE) || (elseState[level] == IF_RUN)) {
				cond[level] = FALSE;
				continue;
			}
			line = one_argument(line, control);
			if ((check = search_block(control, ifchk, TRUE)) >= 0)
				cond[level] = mprog_do_ifchck(prog_type, line, check, mob, actor, arg1, arg2, rndm);
			else {
				bug2("MOBProg: invalid ELSEIF check, mob %d prog %s", mob_num, prog_type);
				call_level--;
				release_buffer(buf);
				release_buffer(control);
				release_buffer(data);
				return;
			}
			state[level] = IN_BLOCK;
//			 = (cond[level] == FALSE) && (eval == TRUE);
			if (cond[level])	elseState[level] = IF_RUN;
		} else if (!str_cmp(control, "else")) {
			if (!level || state[level-1] != BEGIN_BLOCK) {
				bug2("MOBProg: ELSE without IF, mob %d, prog %s", mob_num, prog_type);	
				call_level--;
				release_buffer(buf);
				release_buffer(control);
				release_buffer(data);
				return;
			}
			if ((cond[level-1] == FALSE) || (elseState[level] == IF_RUN)) {
				cond[level] = FALSE;
				continue;
			}
			state[level] = IN_BLOCK;
			cond[level] = TRUE;
//			cond[level] = (cond[level] == TRUE) ? FALSE : TRUE;
			if (cond[level])	elseState[level] = IF_RUN;
		} else if (!str_cmp(control, "endif")) {
			if (!level || state[level-1] != BEGIN_BLOCK) {
				bug2("MOBProg: ENDIF without IF, mob %d, prog %s", mob_num, prog_type);	
				call_level--;
				release_buffer(buf);
				release_buffer(control);
				release_buffer(data);
				return;
			}
			cond[level] = TRUE;
			state[level] = IN_BLOCK;
			elseState[level] = IF_NOTRUN;
			state[--level] = END_BLOCK;
		} else if (cond[level] == TRUE && (!str_cmp(control, "break"))) {
			cond[level] = FALSE;
			if (level > 0)
				cond[level - 1] = FALSE;
		} else if (cond[level] == TRUE && (!str_cmp(control, "end"))) {
			call_level--;
			release_buffer(buf);
			release_buffer(control);
			release_buffer(data);
			return;
		} else if ((!level || cond[level] == TRUE) && buf[0] != '\0') {
			state[level] = IN_BLOCK;
			mprog_translate(data, buf, mob, actor, arg1, arg2, rndm);
			command_interpreter(mob, data);
		}
	}
	call_level--;

	release_buffer(buf);
	release_buffer(control);
	release_buffer(data);
	return;
}

/***************************************************************************
 * Global function code and brief comments.
 */

/* The next two routines are the basic trigger types. Either trigger
 *  on a certain percent, or trigger on a keyword or word phrase.
 *  To see how this works, look at the various trigger routines..
 */
int is_substring(char *sub, char *string);
int word_check(char *str, char *wordlist);
void mprog_wordlist_check(char *arg, CharData *mob, CharData *actor, ObjData *obj, Ptr vo, int type) {
	MPROG_DATA *mprg;
	char *temp;

	for (mprg = mob_index[mob->nr].mobprogs; mprg; mprg = mprg->next) {
		if (mprg->type & type) {
			temp = mprg->arglist;
			skip_spaces(temp);
			if (*temp == 'p' && *(temp+1) == ' ') {
				if (is_substring(temp + 2, arg)) {
					mprog_driver(mprg, mob, actor, obj, vo, FALSE);
					return;
				}
			} else {
				if (word_check(arg, temp)) {
					mprog_driver(mprg, mob, actor, obj, vo, FALSE);
					return;
				}
			}
		}
	}
	return;
}

int mprog_percent_check(CharData *mob, CharData *actor, ObjData *obj, Ptr vo, int type) {
	MPROG_DATA * mprg;
	int	result = 0;
	
	for (mprg = mob_index[mob->nr].mobprogs; mprg != NULL; mprg = mprg->next)
		if ((mprg->type & type) && (Number(0,100) < atoi(mprg->arglist))) {
			mprog_driver(mprg, mob, actor, obj, vo, FALSE);
			result = 1;
//			if (type != GREET_PROG && type != ALL_GREET_PROG)
			break;
		}
	return result;
}

/* The triggers.. These are really basic, and since most appear only
 * once in the code (hmm. i think they all do) it would be more efficient
 * to substitute the code in and make the mprog_xxx_check routines global.
 * However, they are all here in one nice place at the moment to make it
 * easier to see what they look like. If you do substitute them back in,
 * make sure you remember to modify the variable names to the ones in the
 * trigger calls.
*/
void mprog_act_trigger(char *buf, CharData *mob, CharData *ch,
		ObjData *obj, Ptr vo) {
	MPROG_ACT_LIST * tmp_act;

	if (IS_NPC(mob) && (mob_index[mob->nr].progtypes & ACT_PROG) && (mob != ch)) {
		tmp_act = (MPROG_ACT_LIST *) malloc(sizeof(MPROG_ACT_LIST));
//		CREATE(tmp_act, MPROG_ACT_LIST, 1);
		if (mob->mpactnum > 0)	tmp_act->next = mob->mpact;
		else					tmp_act->next = NULL;
		mob->mpact      = tmp_act;
		mob->mpact->buf = str_dup(buf);
		mob->mpact->ch  = ch; 
		mob->mpact->obj = obj; 
		mob->mpact->vo  = vo; 
		mob->mpactnum++;
	}
/*	if (mob != ch && IS_NPC(mob) && (mob_index[mob->nr].progtypes & ACT_PROG))
		mprog_wordlist_check(buf, mob, ch, obj, vo, ACT_PROG);
*/
	return;
}

void mprog_bribe_trigger(CharData *mob, CharData *ch, int amount) {
	MPROG_DATA *mprg;

	if (IS_NPC(mob) && (mob_index[mob->nr].progtypes & BRIBE_PROG)) {
		for (mprg = mob_index[mob->nr].mobprogs; mprg != NULL; mprg = mprg->next)
			if ((mprg->type & BRIBE_PROG) && (amount >= atoi(mprg->arglist))) {
				mprog_driver(mprg, mob, ch, NULL , NULL, FALSE);
				break;
			}
	}
	return;
}

void mprog_death_trigger(CharData *mob, CharData *killer) {
	Position old_pos = GET_POS(mob);

	if (IS_NPC(mob) && (mob_index[mob->nr].progtypes & DEATH_PROG)) {
		GET_POS(mob) = POS_STANDING;
		mprog_percent_check(mob, killer, NULL, NULL, DEATH_PROG);
		GET_POS(mob) = old_pos;
	} else if (IS_NPC(mob))
		death_cry(mob);
	return;
}

void mprog_entry_trigger(CharData *mob) {
	if (IS_NPC(mob) && (mob_index[mob->nr].progtypes & ENTRY_PROG))
		mprog_percent_check(mob, NULL, NULL, NULL, ENTRY_PROG);
	return;
}


void mprog_fight_trigger(CharData *mob, CharData *ch) {
	if (IS_NPC(mob) && (mob_index[mob->nr].progtypes & FIGHT_PROG))
		mprog_percent_check(mob, ch, NULL, NULL, FIGHT_PROG);
	return;
}


void mprog_give_trigger(CharData *mob, CharData *ch, ObjData *obj) {
	char *buf = get_buffer(MAX_INPUT_LENGTH);
	MPROG_DATA *mprg;

	if (IS_NPC(mob) && (mob_index[mob->nr].progtypes & GIVE_PROG))
		for (mprg = mob_index[mob->nr].mobprogs; mprg != NULL; mprg = mprg->next) {
			one_argument(mprg->arglist, buf);
			if ((mprg->type & GIVE_PROG) &&
					((!str_infix(SSData(obj->name), mprg->arglist)) ||
					(!str_cmp("all", buf)))) {
				mprog_driver(mprg, mob, ch, obj, NULL, FALSE);
				break;
			}
		}
	release_buffer(buf);
	return;
}


void mprog_greet_trigger(CharData *ch) {
	CharData *vmob;
	
	START_ITER(iter, vmob, CharData *, world[IN_ROOM(ch)].people) {
		if (IS_NPC(vmob)
				&& AWAKE(vmob)
				&& (FIGHTING(vmob) == NULL)
				&& ch != vmob) {
			if (vmob->CanSee(ch) && (mob_index[vmob->nr].progtypes & GREET_PROG))
				mprog_percent_check(vmob, ch, NULL, NULL, GREET_PROG);
			else if (mob_index[vmob->nr].progtypes & ALL_GREET_PROG)
				mprog_percent_check(vmob, ch, NULL, NULL, ALL_GREET_PROG);
		}
	} END_ITER(iter);
	return;
}


int mprog_exit_trigger(CharData *ch, int dir) {
	MPROG_DATA *mprg;
	CharData *vmob;
	int result = 0;
	int exit;
	char *buf = get_buffer(MAX_INPUT_LENGTH);
	
	START_ITER(iter, vmob, CharData *, world[IN_ROOM(ch)].people) {
		if (IS_NPC(vmob)
				&& AWAKE(vmob)
				&& (FIGHTING(vmob) == NULL)
				&& ch != vmob
				&& (mob_index[vmob->nr].progtypes & (ALL_EXIT_PROG | EXIT_PROG))) {
			for (mprg = mob_index[vmob->nr].mobprogs; mprg != NULL; mprg = mprg->next) {
				if ((mprg->type & ALL_EXIT_PROG) || vmob->CanSee(ch)) {
					one_argument(mprg->arglist, buf);
					if (is_number(buf))	exit = atoi(buf);
					else				exit = search_block(buf, dirs, TRUE);
					if (exit == dir) {
						mprog_driver(mprg, vmob, ch, NULL, NULL, FALSE);
						result++;
						break;
					}
				}
			}
		}
	} END_ITER(iter);
	
	release_buffer(buf);
	return result;
}


void mprog_hitprcnt_trigger(CharData *mob, CharData *ch) {
	MPROG_DATA *mprg;

	if (IS_NPC(mob) && (mob_index[mob->nr].progtypes & HITPRCNT_PROG))
		for (mprg = mob_index[mob->nr].mobprogs; mprg != NULL; mprg = mprg->next)
			if ((mprg->type & HITPRCNT_PROG)
					&& ((100*mob->points.hit / mob->points.max_hit) < atoi(mprg->arglist))) {
				mprog_driver(mprg, mob, ch, NULL, NULL, FALSE);
				break;
			} 
	return;
}


void mprog_random_trigger(CharData *mob) {
	if (mob_index[mob->nr].progtypes & RAND_PROG)
		mprog_percent_check(mob,NULL,NULL,NULL,RAND_PROG);
	return;
}


void mprog_speech_trigger(char *txt, CharData *mob) {
	CharData *vmob;
	
	START_ITER(iter, vmob, CharData *, world[IN_ROOM(mob)].people) {
		if ((mob != vmob) && IS_NPC(vmob) && (mob_index[vmob->nr].progtypes & SPEECH_PROG))
			mprog_wordlist_check(txt, vmob, mob, NULL, NULL, SPEECH_PROG);
	} END_ITER(iter);
	return;
}


void mprog_script_trigger(CharData *mob) {
	MPROG_DATA * mprg;

	if (mob_index[mob->nr].progtypes & SCRIPT_PROG)
		for (mprg = mob_index[mob->nr].mobprogs; mprg; mprg = mprg->next)
			if ((mprg->type & SCRIPT_PROG)) {
				if ( mprg->arglist[0] == '\0' ||
						mob->mpscriptpos != 0 ||
						atoi( mprg->arglist ) == time_info.hours)
					mprog_driver(mprg, mob, NULL, NULL, NULL, TRUE);
	}
    return;
}


EVENT(mprog_delay_trigger) {
	CAUSER_CH->mprog_delay = -1;
	if (IS_NPC(CAUSER_CH) && mob_index[CAUSER_CH->nr].progtypes & DELAY_PROG)
		mprog_percent_check(CAUSER_CH, NULL, NULL, NULL, DELAY_PROG);
}
