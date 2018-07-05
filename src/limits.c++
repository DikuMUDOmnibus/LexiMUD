/* ************************************************************************
*   File: limits.c                                      Part of CircleMUD *
*  Usage: limits & gain funcs for HMV, exp, hunger/thirst, idle time      *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */



#include "structs.h"
#include "utils.h"
#include "buffer.h"
#include "spells.h"
#include "comm.h"
#include "scripts.h"
#include "db.h"
#include "handler.h"
#include "find.h"
#include "event.h"


extern int use_autowiz;
extern int min_wizlist_lev;
void Crash_rentsave(CharData *ch);

void hour_update(void);
int graf(int age, int p0, int p1, int p2, int p3, int p4, int p5, int p6);
void check_autowiz(CharData * ch);

EVENTFUNC(regen_event);
void AlterHit(CharData *ch, SInt32 amount);
void AlterMove(CharData *ch, SInt32 amount);
void CheckRegenRates(CharData *ch);


/* When age < 15 return the value p0 */
/* When age in 15..29 calculate the line between p1 & p2 */
/* When age in 30..44 calculate the line between p2 & p3 */
/* When age in 45..59 calculate the line between p3 & p4 */
/* When age in 60..79 calculate the line between p4 & p5 */
/* When age >= 80 return the value p6 */
int graf(int age, int p0, int p1, int p2, int p3, int p4, int p5, int p6) {
	if (age < 15)
		return (p0);		/* < 15   */
	else if (age <= 29)
		return (int) (p1 + (((age - 15) * (p2 - p1)) / 15));	/* 15..29 */
	else if (age <= 44)
		return (int) (p2 + (((age - 30) * (p3 - p2)) / 15));	/* 30..44 */
	else if (age <= 59)
		return (int) (p3 + (((age - 45) * (p4 - p3)) / 15));	/* 45..59 */
	else if (age <= 79)
		return (int) (p4 + (((age - 60) * (p5 - p4)) / 20));	/* 60..79 */
	else
		return (p6);		/* >= 80 */
}


//	Player point types for events
#define REGEN_HIT      0
#define REGEN_MANA     1
#define REGEN_MOVE     2


struct RegenEvent {
	CharData *	ch;
	SInt32		type;
};


EVENTFUNC(regen_event) {
	struct RegenEvent *regen = (struct RegenEvent *)event_obj;
	CharData *ch = regen->ch;
	SInt32	gain;
	
	if (!ch)
		return 0;
	
	if (GET_POS(ch) >= POS_STUNNED) {	//	No help for the dying
		switch (regen->type) {
			case REGEN_HIT:
				if (IS_SYNTHETIC(ch))
					break;
				
				if (!FIGHTING(ch))
					GET_HIT(ch) = MIN(GET_HIT(ch) + 1, GET_MAX_HIT(ch));
				
				if (GET_POS(ch) <= POS_STUNNED)
					ch->update_pos();
				
				if (GET_HIT(ch) < GET_MAX_HIT(ch)) {
					gain = hit_gain(ch);
					return (/* PULSES_PER_MUD_HOUR / */ (gain ? gain : 1));
				}
				break;
//			case REGEN_MANA:
//				if (!FIGHTING(ch))
//					GET_MANA(ch) = MIN(GET_MANA(ch) + 1, GET_MAX_MANA(ch));
//				
//				if (GET_MANA(ch) < GET_MAX_MANA(ch)) {
//					gain = mana_gain(ch);
//					return (/*PULSES_PER_MUD_HOUR / */(gain ? gain : 1));
//				}
//				break;
			case REGEN_MOVE:
				if (!FIGHTING(ch))
					GET_MOVE(ch) = MIN(GET_MOVE(ch) + 1, GET_MAX_MOVE(ch));
				
				if (GET_MOVE(ch) < GET_MAX_MOVE(ch)) {
					gain = move_gain(ch);
					return (/* PULSES_PER_MUD_HOUR / */(gain ? gain : 1));
				}
				break;
			default:
				log("SYSERR:  Unknown points event type %d", regen->type);
		}
	}
	GET_POINTS_EVENT(ch, regen->type) = NULL;
	return 0;
}


//	Subtract amount of HP from ch's current and start points event
void AlterHit(CharData *ch, SInt32 amount) {
	struct RegenEvent *	regen;
	SInt32				time;
	SInt32				gain;
	
	GET_HIT(ch) = MIN(GET_HIT(ch) - amount, GET_MAX_HIT(ch));
	
	ch->update_pos();
	
	if ((GET_POS(ch) < POS_INCAP)  || (GET_RACE(ch) == RACE_SYNTHETIC))
		return;
	
	if ((GET_HIT(ch) < GET_MAX_HIT(ch)) && !GET_POINTS_EVENT(ch, REGEN_HIT)) {
		CREATE(regen, struct RegenEvent, 1);
		regen->ch = ch;
		regen->type = REGEN_HIT;
		gain = hit_gain(ch);
		time = /* PULSES_PER_MUD_HOUR / */ (gain ? gain : 1);
		GET_POINTS_EVENT(ch, REGEN_HIT) = new Event(regen_event, regen, time);
	}
	
	if (amount >= 0) {
		ch->update_pos();
	}
}

/* 
//	Subtracts amount of mana from ch's current and starts points event
void AlterMana(CharData *ch, SInt32 amount) {
	struct RegenEvent *regen;
	SInt32	time;
	SInt32	gain;

	GET_MANA(ch) = MIN(GET_MANA(ch) - amount, GET_MAX_MANA(ch));
	
	if ((GET_MANA(ch) < GET_MAX_MANA(ch)) && !GET_POINTS_EVENT(ch, REGEN_MANA)) {
		if (GET_POS(ch) >= POS_STUNNED) {
			CREATE(regen, struct RegenEvent, 1);
			regen->ch = ch;
			regen->type = REGEN_MANA;
			gain = mana_gain(ch);
			time = PULSES_PER_MUD_HOUR / (gain ? gain : 1);
			GET_POINTS_EVENT(ch, REGEN_MANA) = new Event(regen_event, regen, time, NULL);
		}
	}
}
*/


void AlterMove(CharData *ch, SInt32 amount) {
	struct RegenEvent *regen;
	SInt32	time;
	SInt32	gain;

	GET_MOVE(ch) = MIN(GET_MOVE(ch) - amount, GET_MAX_MOVE(ch));
	
	if ((GET_MOVE(ch) < GET_MAX_MOVE(ch)) && !GET_POINTS_EVENT(ch, REGEN_MOVE)) {
		if (GET_POS(ch) >= POS_STUNNED) {
			CREATE(regen, struct RegenEvent, 1);
			regen->ch = ch;
			regen->type = REGEN_MOVE;
			gain = move_gain(ch);
			time = /* PULSES_PER_MUD_HOUR / */(gain ? gain : 1);
			GET_POINTS_EVENT(ch, REGEN_MOVE) = new Event(regen_event, regen, time);
		}
	}
}


//	The higher, the faster
int hit_gain(CharData * ch) {
	//	Hitpoint gain pr. game hour
	float gain;

//	if (IS_NPC(ch)) {
//		gain = 3;
//	} else {
		//	5 - 500 pulses
		gain = 500 / (GET_CON(ch) ? GET_CON(ch) : 1); //	3 * graf(age(ch).year, 8, 12, 20, 32, 16, 10, 4);
		
		//	Race/Level calculations
//		if (IS_ALIEN(ch))	gain /= 1.5;

		//	Skill/Spell calculations

		//	Position calculations
		switch (GET_POS(ch)) {
			case POS_SLEEPING:	gain /= 3;			break;		//	Triple
			case POS_RESTING:	gain /= 2;			break;		//	Double
			case POS_SITTING:	gain /= 1.5;		break;		//	One and a half
			default:								break;
		}
//	}

	if (AFF_FLAGGED(ch, AFF_POISON))		//	Half
		gain *= 2;

//	Disabled until food is more common.
//	if ((GET_COND(ch, FULL) == 0) || (GET_COND(ch, THIRST) == 0))
//		gain *= 2;							//	Half yet again

	return (int)gain;
}



/* move gain pr. game hour */
int move_gain(CharData * ch) {
	float gain;

//	if (IS_NPC(ch)) {
//		gain = 5;
//	} else {
		//	10 - 1000 pulses
		gain = 1000 / (GET_CON(ch) ? GET_CON(ch) : 1); //	graf(age(ch).year, 16, 20, 24, 20, 16, 12, 10);
		
		//	Class/Level calculations
		if (IS_SYNTHETIC(ch))	gain = 5;

		//	Skill/Spell calculations

		//	Position calculations
		switch (GET_POS(ch)) {
			case POS_SLEEPING:	gain /= 3;			break;		//	Triple
			case POS_RESTING:	gain /= 2;			break;		//	Double
			case POS_SITTING:	gain /= 1.5;		break;		//	One and a half
			default:								break;
		}

		if (!IS_SYNTHETIC(ch)) {
			if (AFF_FLAGGED(ch, AFF_POISON))
				gain *= 2;
//			if ((GET_COND(ch, FULL) == 0) || (GET_COND(ch, THIRST) == 0))
//				gain *= 2;
		}
//	}
	return (int)gain;
}


void CheckRegenRates(CharData *ch) {
	struct RegenEvent *	regen;
	SInt32				gain, time, type;
	
	if (GET_POS(ch) <= POS_INCAP)
		return;
	
	for (type = REGEN_HIT; type <= REGEN_MOVE; type++) {
		switch (type) {
			case REGEN_HIT:
				if (GET_HIT(ch) >= GET_MAX_HIT(ch))		continue;
				else 									gain = hit_gain(ch);
				break;
//			case REGEN_MANA:
//				if (GET_MANA(ch) >= GET_MAX_MANA(ch))	continue;
//				else									gain = mana_gain(ch);
//				break;
			case REGEN_MOVE:
				if (GET_MOVE(ch) >= GET_MAX_MOVE(ch))	continue;
				else									gain = move_gain(ch);
				break;
			default:
				continue;
		}
		time = /* PULSES_PER_MUD_HOUR / */ (gain ? gain : 1);
	
		if (!GET_POINTS_EVENT(ch, type) || (time < GET_POINTS_EVENT(ch, type)->Time())) {
			if (GET_POINTS_EVENT(ch, type))	GET_POINTS_EVENT(ch, type)->Cancel();
				
			CREATE(regen, struct RegenEvent, 1);
			regen->ch = ch;
			regen->type = type;
			GET_POINTS_EVENT(ch, type) = new Event(regen_event, regen, time);
		}
	}
}


void check_autowiz(CharData * ch) {
#ifndef CIRCLE_UNIX
	return;
#else
	char *buf;

	if (use_autowiz && IS_STAFF(ch)) {
		buf = get_buffer(128);
		sprintf(buf, "nice ../bin/autowiz %d %s %d %s %d &", min_wizlist_lev,
				WIZLIST_FILE, LVL_IMMORT, IMMLIST_FILE, (int) getpid());
		mudlog("Initiating autowiz.", CMP, LVL_IMMORT, FALSE);
		system(buf);
		release_buffer(buf);
	}
#endif
}


#define READ_TITLE(ch, lev) (GET_SEX(ch) == SEX_MALE ?   \
	titles[(int)GET_RACE(ch)][lev].title_m :  \
	titles[(int)GET_RACE(ch)][lev].title_f)


void gain_condition(CharData * ch, int condition, int value) {
	int intoxicated;

	if (GET_COND(ch, condition) == -1)	/* No change */
		return;

	intoxicated = (GET_COND(ch, DRUNK) > 0);

	GET_COND(ch, condition) += value;

	/* update regen rates if we were just on empty */
	if ((condition != DRUNK) && (value > 0) && (GET_COND(ch, condition) == value))
		CheckRegenRates(ch);
		
	GET_COND(ch, condition) = MAX(0, GET_COND(ch, condition));
	GET_COND(ch, condition) = MIN(24, GET_COND(ch, condition));

	if (GET_COND(ch, condition) || PLR_FLAGGED(ch, PLR_WRITING))
		return;

	switch (condition) {
		case FULL:
			send_to_char("You are hungry.\r\n", ch);
			return;
		case THIRST:
			send_to_char("You are thirsty.\r\n", ch);
			return;
		case DRUNK:
			if (intoxicated)
				send_to_char("You are now sober.\r\n", ch);
			return;
		default:
			break;
	}
}


void check_idling(CharData * ch) {
	if (++(ch->player->timer) > 8)
		if (GET_WAS_IN(ch) == NOWHERE && IN_ROOM(ch) != NOWHERE) {
			GET_WAS_IN(ch) = IN_ROOM(ch);
			if (FIGHTING(ch)) {
				FIGHTING(ch)->stop_fighting();
				ch->stop_fighting();
			}
			act("$n disappears into the void.", TRUE, ch, 0, 0, TO_ROOM);
			send_to_char("You have been idle, and are pulled into a void.\r\n", ch);
			ch->save(NOWHERE);
			Crash_crashsave(ch);
			ch->from_room();
			ch->to_room(1);
		} else if (ch->player->timer > 48) {
			if (IN_ROOM(ch) != NOWHERE)
				ch->from_room();
			else
				mudlogf(BRF, LVL_STAFF, TRUE, "%s in NOWHERE when idle-extracted.", ch->RealName());
			ch->to_room(3);
			if (ch->desc) {
				STATE(ch->desc) = CON_DISCONNECT;
				ch->desc->character = NULL;
				ch->desc = NULL;
			}
			Crash_rentsave(ch);
			mudlogf(CMP, LVL_STAFF, TRUE, "%s idle-extracted.", ch->RealName());
			ch->extract();
		}
}


void hour_update(void) {
	CharData *i;
	ObjData *j, *jj;

	/* characters */
	START_ITER(iter, i, CharData *, character_list) {
		if (GET_RACE(i) != RACE_SYNTHETIC) {
			gain_condition(i, FULL, -1);
			gain_condition(i, DRUNK, -1);
			gain_condition(i, THIRST, -1);
		}
		if (!IS_NPC(i)) {
			if (!IS_STAFF(i))
				check_idling(i);
			i->update_objects();
		}
	} END_ITER(iter);
	
	/* objects */
	START_ITER(obj_iter, j, ObjData *, object_list) {
		if ((IN_ROOM(j) == NOWHERE) && !j->in_obj && !j->carried_by && !j->worn_by) {
			j->extract();
			continue;
		}
		/* If this is a temporary object */
		if (GET_OBJ_TIMER(j) > 0) {
			GET_OBJ_TIMER(j)--;

			if (!GET_OBJ_TIMER(j)) {
				if (j->carried_by)
					act("$p decays in your hands.", FALSE, j->carried_by, j, 0, TO_CHAR);
				else if (j->worn_by)
					act("$p decays.", FALSE, j->worn_by, j, 0, TO_CHAR);
				else if (IN_ROOM(j) != NOWHERE)
					act("$p falls apart.", FALSE, 0, j, 0, TO_ROOM);
				if ((GET_OBJ_TYPE(j) == ITEM_CONTAINER) && !GET_OBJ_VAL(j, 3)) {
					START_ITER(obj2_iter, jj, ObjData *, j->contains) {
						jj->from_obj();

						if (j->in_obj)					jj->to_obj(j->in_obj);
						else if (j->carried_by)			jj->to_char(j->carried_by);
						else if (IN_ROOM(j) != NOWHERE)	jj->to_room(IN_ROOM(j));
						else if (j->worn_by)			jj->to_char(j->worn_by);
						else							core_dump();
					} END_ITER(obj2_iter);
				}
				j->extract();
			}
		}
	} END_ITER(obj_iter);
}


/* Update PCs, NPCs, and objects */
void point_update(void) {
	CharData *i;

	/* characters */
	START_ITER(iter, i, CharData *, character_list) {
		if (GET_POS(i) >= POS_STUNNED) {
/*			if (AFF_FLAGGED(i, AFF_POISON))
				damage(i, i, 2, SPELL_POISON);*/
			if (GET_POS(i) <= POS_STUNNED)
				i->update_pos();
		} else if (GET_POS(i) == POS_INCAP)
			damage(NULL, i, 20, TYPE_SUFFERING, 1);
		else if (GET_POS(i) == POS_MORTALLYW)
			damage(NULL, i, 40, TYPE_SUFFERING, 1);
	} END_ITER(iter);
}
