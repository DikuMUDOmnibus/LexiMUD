#include "characters.h"
#include "descriptors.h"
#include "event.h"
#include "index.h"
#include "objects.h"
#include "opinion.h"
#include "rooms.h"
#include "comm.h"
#include "handler.h"
#include "interpreter.h"
#include "utils.h"
#include "track.h"


ACMD(do_flee);
ACMD(do_hit);
EVENT(combat_event);
extern int no_specials;

void mprog_random_trigger(CharData * mob);
void mprog_wordlist_check(char *arg, CharData * mob,
	CharData * actor, ObjData * obj, Ptr vo, int type);

/* local functions */
void mobile_activity(CharData *ch);
void clearMemory(CharData * ch);
void check_mobile_activity(UInt32 pulse);
bool clearpath(CharData *ch, RNum room, int dir);
void MobHunt(CharData *ch);
void FoundHated(CharData *ch, CharData *vict);
CharData *AggroScan(CharData *mob, SInt32 range);


bool clearpath(CharData *ch, RNum room, int dir) {
	RoomDirection *exit;

	if (room == -1)	return false;
	
	exit = world[room].dir_option[dir];

	if (!exit || (exit->to_room == NOWHERE))					return false;
	if (EXIT_FLAGGED(exit, EX_NOMOVE | EX_CLOSED))				return false;
	if (IS_NPC(ch) && EXIT_FLAGGED(exit, EX_NOMOB))				return false;
	if (IS_NPC(ch) && ROOM_FLAGGED(exit->to_room, ROOM_NOMOB))	return false;
	if (MOB_FLAGGED(ch, MOB_STAY_ZONE) &&
			world[room].zone != world[exit->to_room].zone)		return false;
	
	return true;
}


CharData *AggroScan(CharData *mob, SInt32 range) {
	SInt32	i, r;
	RNum	room;
	CharData *ch;
	LListIterator<CharData *>	iter;
	
	for (i = 0; i < NUM_OF_DIRS; i++) {
		r = 0;
		room = IN_ROOM(mob);
		while (r < range) {
			if (clearpath(mob, room, i)) {
				room = world[room].dir_option[i]->to_room;
				r++;
				iter.Start(world[room].people);
				while ((ch = iter.Next())) {
					if (mob->CanSee(ch) && mob->mob->hates &&
							mob->mob->hates->InList(GET_ID(ch)))
						return ch;
				}
				iter.Finish();
			} else
				r = range + 1;
		}
	}
	return NULL;
}


void FoundHated(CharData *ch, CharData *vict) {
	if (!ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL))
		combat_event(ch, vict, 0);
}


void MobHunt(CharData *ch) {
	CharData *	vict;
	RNum		dest;
	SInt32		dir;
	
	if (!ch->path)	return;
	
//	if (--ch->persist <= 0) {
//		delete ch->path;
//		ch->path = NULL;
//		return;
//	}
	
	vict = ch->path->victim;
	dest = ch->path->dest;
	
	if ((dir = Track(ch)) != -1) {
		perform_move(ch, dir, 1);
		if (vict) {
			if (IN_ROOM(ch) == IN_ROOM(vict)) {
				//	Determined mobs: they will wait till you leave the room, then
				//	give chase!!!
				if (ch->mob->hates && ch->mob->hates->InList(GET_ID(vict)) &&
						!ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
					FoundHated(ch, vict);
					delete ch->path;
					ch->path = NULL;
				}
			
			}
		} else if (IN_ROOM(ch) == dest) {
			delete ch->path;
			ch->path = NULL;
		}
	}
}


void check_mobile_activity(UInt32 pulse) {
	CharData *	ch;
	UInt32		tick;
	
	tick = (pulse / PULSE_MOBILE) % TICK_WRAP_COUNT;
	
	START_ITER(iter, ch, CharData *, character_list) {
		if (IS_NPC(ch)) {
			// Take care of this now.  We want fast-acting MOBs.
			if((ch->mpactnum > 0) /* && !is_empty(world[IN_ROOM(ch)].zone) */) {
				MPROG_ACT_LIST *tmp_act, *tmp2_act;
				for(tmp_act = ch->mpact; tmp_act != NULL; tmp_act=tmp_act->next) {
					mprog_wordlist_check(tmp_act->buf, ch, tmp_act->ch,
							tmp_act->obj, tmp_act->vo, ACT_PROG);
					FREE(tmp_act->buf);
				}
				for(tmp_act = ch->mpact; tmp_act != NULL; tmp_act=tmp2_act) {
					tmp2_act = tmp_act->next;
					FREE(tmp_act);
				}
				ch->mpactnum = 0;
				ch->mpact = NULL;
				continue;		// Take care of the rest of this NEXT time...
			}

			if (ch->mob->tick == tick)
				mobile_activity(ch);
			else if (ch->path)
				MobHunt(ch);
		}
	} END_ITER(iter);
}


void mobile_activity(CharData *ch) {
	CharData	*temp;
	ObjData		*obj, *best_obj;
	SInt32		max, door;
	Relation	relation;
	
	if (IN_ROOM(ch) == -1) {
		log("Char %s in NOWHERE", ch->RealName());
		ch->to_room(0);
	}
	
	//	SPECPROC
	if (MOB_FLAGGED(ch, MOB_SPEC) && !no_specials) {
		if (mob_index[GET_MOB_RNUM(ch)].func == NULL) {
			log("SYSERR: %s (#%d) - Attempting to call non-existing mob func", ch->RealName(), GET_MOB_VNUM(ch));
			REMOVE_BIT(MOB_FLAGS(ch), MOB_SPEC);
		} else if ((mob_index[GET_MOB_RNUM(ch)].func) (ch, ch, 0, ""))
			return;		/* go to next char */
	}

	if (AWAKE(ch) && !FIGHTING(ch)) {
		//	ASSIST
		if (!ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL) && !MOB_FLAGGED(ch, MOB_PROGMOB)) {
			START_ITER(iter, temp, CharData *, world[IN_ROOM(ch)].people) {
				if ((ch == temp) || !ch->CanSee(temp) || !FIGHTING(temp) || (FIGHTING(temp) == ch))
					continue;
				relation = ch->GetRelation(temp);
				if ((relation != RELATION_ENEMY) && (MOB_FLAGGED(ch, MOB_HELPER) ||
						(relation == RELATION_FRIEND && !MOB_FLAGGED(ch, MOB_WIMPY)))) {
					act("$n jumps to the aid of $N!", FALSE, ch, 0, temp, TO_ROOM);
					combat_event(ch, FIGHTING(temp), 0);
					END_ITER(iter);
					return;
				}
			} END_ITER(iter);
		}
		
		//	SCAVENGE
		if (MOB_FLAGGED(ch, MOB_SCAVENGER) && world[IN_ROOM(ch)].contents.Count() && !Number(0, 10)) {
			max = 1;
			best_obj = NULL;
			START_ITER(iter, obj, ObjData *, world[IN_ROOM(ch)].contents) {
				if (CAN_GET_OBJ(ch, obj) && GET_OBJ_COST(obj) > max) {
					best_obj = obj;
					max = GET_OBJ_COST(obj);
				}
			} END_ITER(iter);
			if (best_obj) {
				best_obj->from_room();
				best_obj->to_char(ch);
				act("$n gets $p.", FALSE, ch, best_obj, 0, TO_ROOM);
				return;
			}
		}
			
		//	WANDER
		if (!MOB_FLAGGED(ch, MOB_SENTINEL) && (GET_POS(ch) == POS_STANDING) &&
				((door = Number(0, 18)) < NUM_OF_DIRS) && CAN_GO(ch, door) &&
				!EXIT_FLAGGED(EXIT(ch, door), EX_NOMOVE | EX_NOMOB) &&
				!ROOM_FLAGGED(EXIT(ch, door)->to_room, ROOM_NOMOB | ROOM_DEATH) &&
				(!MOB_FLAGGED(ch, MOB_STAY_ZONE) ||
				(world[EXIT(ch, door)->to_room].zone == world[IN_ROOM(ch)].zone))) {
			perform_move(ch, door, 1);
			return;		// You've done enough for now...
		}
		
		
		//	It's debatable why I have this check against peacefulness...
		//	The primary reason is because mobs should be smart enuf to know
		//	When they are safe from other mobs
		if (!MOB_FLAGGED(ch, MOB_PROGMOB) && !ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
			//	FEAR (normal)
			if ((GET_HIT(ch) > (GET_MAX_HIT(ch) / 2)) && ch->mob->fears && (temp = ch->mob->fears->FindTarget(ch))) {
				do_flee(ch, "", 0, "flee", 0);
				return;
			}
			
			//	HATE
			if (ch->mob->hates && (temp = ch->mob->hates->FindTarget(ch))) {
				FoundHated(ch, temp);
				return;
			}
			
			//	FEAR (desperate non-haters)
			if (ch->mob->fears && (temp = ch->mob->fears->FindTarget(ch))) {
				do_flee(ch, "", 0, "flee", 0);
				return;
			}
		}


		//	AGGR
		if (MOB_FLAGGED(ch, MOB_AGGRESSIVE | MOB_AGGR_ALL)) {
			START_ITER(iter, temp, CharData *, world[IN_ROOM(ch)].people) {
				if (ch == temp)
					continue;
				if (!ch->CanSee(temp) || PRF_FLAGGED(temp, PRF_NOHASSLE))
					continue;
				if (MOB_FLAGGED(ch, MOB_WIMPY) && AWAKE(temp))
					continue;
				relation = ch->GetRelation(temp);
				if ((MOB_FLAGGED(ch, MOB_AGGR_ALL) && (relation != RELATION_FRIEND))
						|| (relation == RELATION_ENEMY)) {
					combat_event(ch, temp, 0);
					END_ITER(iter);
					return;
				}
			} END_ITER(iter);
		}
		
		if (!MOB_FLAGGED(ch, MOB_PROGMOB) && ch->mob->hates) {
			if ((temp = AggroScan(ch, 5))) {
				ch->path = Path2Char(IN_ROOM(ch), temp, 5, HUNT_GLOBAL | HUNT_THRU_DOORS);
			}
		}
		
		//	MPROGS
		mprog_random_trigger(ch);
	}
	//	SOUNDS ?
	
}




void remember(CharData * ch, CharData * victim) {
	if (!IS_NPC(ch) || NO_STAFF_HASSLE(victim))
		return;
	
	if (!ch->mob->hates)
		ch->mob->hates = new Opinion;
	
	ch->mob->hates->AddChar(GET_ID(victim));
}


void forget(CharData * ch, CharData * victim) {
	if (!IS_NPC(ch) || !ch->mob->hates)
		return;
	
	ch->mob->hates->RemChar(GET_ID(victim));
}


void clearMemory(CharData * ch) {
	if (!IS_NPC(ch) || !ch->mob->hates)
		return;
	ch->mob->hates->Clear();
}