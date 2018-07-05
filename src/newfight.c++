


#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "handler.h"
#include "interpreter.h"
#include "spells.h"
#include "event.h"

EVENTFUNC(combat);

#define IS_WEAPON(type) (((type) >= TYPE_HIT) && ((type) < TYPE_SUFFERING))

extern RoomData *world;

EVENT(combat_event);
int combat_delay(CharData *ch);
void fight_mtrigger(CharData *ch);
void hitprcnt_mtrigger(CharData *ch);
bool kill_valid(CharData *ch, CharData *vict);


SInt32 base_response[NUM_RACES+1] = {
	12,	//	12 seconds for human
	8,	//	8 seconds for synthetics
	10,	//	10 seconds for predators
	8,	//	8 seconds for aliens
	10	//	10 seconds for animals
};

int combat_delay(CharData *ch) {
	ObjData * weapon;
	int speed;
	if (!ch)	/* bad call... no character. */
		return 0;
	
	speed = base_response[(int)GET_RACE(ch)];	/* Get the base speed */
	weapon = GET_EQ(ch, WEAR_HAND_R);
	if (!weapon || (GET_OBJ_TYPE(weapon) != ITEM_WEAPON))
		weapon = GET_EQ(ch, WEAR_HAND_L);
	
	if (weapon && (GET_OBJ_TYPE(weapon) == ITEM_WEAPON)) {
			speed += GET_OBJ_SPEED(weapon);	/* The speed of the weapon is important */
			speed += 1;			/* Force it to round up by incrementing. */
			speed /= 2;		/* Divide by 2, for average */
    }
	speed -= GET_AGI(ch) / 40;	/* Minor bonuses for dexterity */
	return MAX(1, speed);		/* Minimum of 1 */
}

#define GET_WAIT(ch) (IS_NPC(ch) ? GET_MOB_WAIT(ch) : ((ch)->desc ? (ch)->desc->wait : 0))

EVENT(combat_event) {
	ObjData * weapon;
//	CharData *vict;
	int attackRate = 1;
	int notGun = TRUE;
	ACMD(do_reload);
	
	if (!info && event_pending(CAUSER_CH, combat_event, EVENT_CAUSER))
		return;
	
	if (!CAUSER_CH || !VICTIM_CH || CAUSER_CH == VICTIM_CH)	/* bad event was posted... ignore it */
		return;
	
	if (!kill_valid(CAUSER_CH, VICTIM_CH)) {
		if (!PRF_FLAGGED(CAUSER_CH, PRF_PK)) {
			send_to_char("You must turn on PK first!  Type \"pkill\" for more information.\r\n", CAUSER_CH);
			return;
		}
		if (!PRF_FLAGGED(VICTIM_CH, PRF_PK)) {
			act("You cannot kill $N because $E has not chosen to participate in PK.", FALSE, CAUSER_CH, 0, VICTIM_CH, TO_CHAR);
			return;
		}
	}
	
	if (IN_ROOM(CAUSER_CH) != IN_ROOM(VICTIM_CH)) {
		if (FIGHTING(CAUSER_CH) == VICTIM_CH)
			CAUSER_CH->stop_fighting();
		return;
	}
	
	if (FIGHTING(CAUSER_CH) != VICTIM_CH) {
		if (!FIGHTING(CAUSER_CH)) {
			set_fighting(CAUSER_CH, VICTIM_CH);
		} else if (IN_ROOM(CAUSER_CH) != IN_ROOM(FIGHTING(CAUSER_CH))) {
			CAUSER_CH->stop_fighting();
			set_fighting(CAUSER_CH, VICTIM_CH);
		} else
			return;
	}
	
	if ((GET_WAIT(CAUSER_CH) / PASSES_PER_SEC) > 0) {
		add_event(GET_WAIT(CAUSER_CH) / PASSES_PER_SEC, combat_event, CAUSER_CH, VICTIM_CH, 0);
		return;	// Causer is already waiting.
	}

	if (GET_POS(CAUSER_CH) <= POS_STUNNED) {
		add_event((base_response[(int)GET_RACE(CAUSER_CH)] * 2) - (GET_CON(CAUSER_CH) / 25),
				combat_event, CAUSER_CH, VICTIM_CH, 0);
		return;	// Causer is in no condition to do anything.
	}
	
	if (GET_POS(CAUSER_CH) < POS_FIGHTING) {
		if (IS_NPC(CAUSER_CH)) {
			GET_POS(CAUSER_CH) = POS_FIGHTING;
			if (CAUSER_CH->sitting_on)
				CAUSER_CH->sitting_on = NULL;
			act("$n scrambles to $s feet!", TRUE, CAUSER_CH, 0, 0, TO_ROOM);
			add_event(base_response[(int)GET_RACE(CAUSER_CH)] / 2,
					combat_event, CAUSER_CH, VICTIM_CH, 0);
			return;	// Causer is jumping to feet.
		} else {
			send_to_char("You're on the ground!  Get up, fast!\r\n", CAUSER_CH);
		}
	}
		
	weapon = GET_EQ(CAUSER_CH, WEAR_HAND_R);
	if (!weapon || (GET_OBJ_TYPE(weapon) != ITEM_WEAPON))
		weapon = GET_EQ(CAUSER_CH, WEAR_HAND_L);
	
	if (IS_NPC(CAUSER_CH)) {
		if (weapon && IS_GUN(weapon) && (GET_GUN_AMMO(weapon) <= 0)) {
			do_reload(CAUSER_CH, "", 0, "reload", SCMD_LOAD);
			if (GET_GUN_AMMO(weapon) > 0) {
				add_event(6, combat_event, CAUSER_CH, VICTIM_CH, 0);
				return;	// Causer is reloading.
			}
		}
	}
	
	if (weapon && IS_GUN(weapon) && (GET_GUN_AMMO(weapon) > 0))
		attackRate = GET_GUN_RATE(weapon);
		
	add_event(combat_delay(CAUSER_CH), combat_event, CAUSER_CH, VICTIM_CH, 0);
	
	if (GET_POS(CAUSER_CH) < POS_FIGHTING)
		attackRate /= 2;

	hit(CAUSER_CH, VICTIM_CH, TYPE_UNDEFINED, MAX(1, attackRate));
	
	if (IS_NPC(CAUSER_CH) && !GET_MOB_WAIT(CAUSER_CH) && FIGHTING(CAUSER_CH)) {
		fight_mtrigger(CAUSER_CH);
		hitprcnt_mtrigger(CAUSER_CH);
	}
}


struct CombatEvent {
	CharData *		ch;
};

EVENTFUNC(combat) {
	CombatEvent *	CEData = (CombatEvent *)event_obj;
	CharData		*ch, *vict;
	ObjData			*weapon;
	SInt32			attackRate = 1;
	ACMD(do_reload);
	
	if (!CEData || !(ch = CEData->ch))			//	Something fucked up... this shouldn't be!
		return 0;
	
	//	They stopped fighting already.
	if (!(vict = FIGHTING(ch)) || (IN_ROOM(vict) != IN_ROOM(ch)))	{
		if (vict)	ch->stop_fighting();
		vict = NULL;
		//	Eventually a routine will be here to find another combatant
		//	But for now, it doesn't matter since this code isn't even used yet
		
		if (vict)	set_fighting(ch, vict);
		else		return 0;
	}
	
	if (!kill_valid(ch, vict)) {	//	For wussy PK-optional MUDs
		if (!PRF_FLAGGED(ch, PRF_PK))
			send_to_char("You must turn on PK first!  Type \"pkill\" for more information.\r\n", ch);
		else if (!PRF_FLAGGED(vict, PRF_PK))
			act("You cannot kill $N because $E has not chosen to participate in PK.", FALSE, ch, 0, vict, TO_CHAR);
		return 0;
	}
	
	if ((GET_WAIT(ch) / PASSES_PER_SEC) > 0) {
		return GET_WAIT(ch);
	}
	
	if (GET_POS(ch) < POS_FIGHTING) {
		if (GET_POS(ch) <= POS_STUNNED)
			return (((base_response[GET_RACE(ch)] * 2) - (GET_CON(ch) / 25)) RL_SEC);
		else if (IS_NPC(ch)) {
			GET_POS(ch) = POS_FIGHTING;
			if (ch->sitting_on)	ch->sitting_on = NULL;
			act("$n scrambles to $s feet!", TRUE, ch, 0, 0, TO_ROOM);
			return ((base_response[GET_RACE(ch)] * 2) RL_SEC);
		} else
			send_to_char("You're on the ground!  Get up, fast!\r\n", ch);
	}
	
	if (!(weapon = GET_EQ(ch, WEAR_HAND_R)) || (GET_OBJ_TYPE(weapon) != ITEM_WEAPON))
		weapon = GET_EQ(ch, WEAR_HAND_L);
	
	if (IS_NPC(ch) && weapon && IS_GUN(weapon) && (GET_GUN_AMMO(weapon) <= 0)) {
		do_reload(ch, "", 0, "reload", SCMD_LOAD);
		if (GET_GUN_AMMO(weapon) > 0)
			return (6 RL_SEC);
	}
	
	if (weapon && IS_GUN(weapon) && (GET_GUN_AMMO(weapon) > 0))
		attackRate = GET_GUN_RATE(weapon);
	
	if (GET_POS(ch) < POS_FIGHTING)
		attackRate /= 2;
	
	hit(ch, vict, TYPE_UNDEFINED, MAX(1, attackRate));
	
	if (!PURGED(ch) && IS_NPC(ch) && !GET_MOB_WAIT(ch) && FIGHTING(ch)) {
		fight_mtrigger(ch);
		hitprcnt_mtrigger(ch);
	}
	
	return (PURGED(ch) ? 0 : (combat_delay(ch) RL_SEC));
}