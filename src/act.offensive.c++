/* ************************************************************************
*   File: act.offensive.c                               Part of CircleMUD *
*  Usage: player-level commands of an offensive nature                    *
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
#include "help.h"
#include "event.h"

/* extern variables */
extern int pk_allowed;
extern char *dirs[];
extern int rev_dir[];

/* extern functions */
int perform_move(CharData *ch, int dir, int specials_check);
EVENT(combat_event);
EVENTFUNC(gravity);

/* Daniel Houghton's revised external functions */
int skill_roll(CharData *ch, int skill_num);
void strike_missile(CharData *ch, CharData *tch, 
                   ObjData *missile, int dir, int attacktype);
void miss_missile(CharData *ch, CharData *tch, 
                ObjData *missile, int dir, int attacktype);
void mob_reaction(CharData *ch, CharData *vict, int dir);
int fire_missile(CharData *ch, char *arg1, ObjData *missile, SInt32 pos, SInt32 range, SInt32 dir, SInt32 skill);

/* prototypes */
ACMD(do_assist);
ACMD(do_hit);
ACMD(do_kill);
ACMD(do_backstab);
ACMD(do_order);
ACMD(do_flee);
ACMD(do_bash);
ACMD(do_rescue);
ACMD(do_kick_bite);
ACMD(do_shoot);
ACMD(do_throw);
ACMD(do_circle);
ACMD(do_pkill);


ACMD(do_assist) {
	CharData *helpee, *opponent;
	char *arg;
	
	if (FIGHTING(ch)) {
		send_to_char("You're already fighting!  How can you assist someone else?\r\n", ch);
		return;
	}
	
	arg = get_buffer(MAX_INPUT_LENGTH);
	
	one_argument(argument, arg);

	if (!*arg)
		send_to_char("Whom do you wish to assist?\r\n", ch);
	else if (!(helpee = get_char_room_vis(ch, arg)))
		send_to_char(NOPERSON, ch);
	else if (helpee == ch)
		send_to_char("You can't help yourself any more than this!\r\n", ch);
	else {
		START_ITER(iter, opponent, CharData *, world[IN_ROOM(ch)].people) {
			if (FIGHTING(opponent) == helpee)
				break;
		} END_ITER(iter);

		if (!opponent)
			act("But nobody is fighting $M!", FALSE, ch, 0, helpee, TO_CHAR);
		else if (!ch->CanSee(opponent))
			act("You can't see who is fighting $M!", FALSE, ch, 0, helpee, TO_CHAR);
		else {
			send_to_char("You join the fight!\r\n", ch);
			act("$N assists you!", 0, helpee, 0, ch, TO_CHAR);
			act("$n assists $N.", FALSE, ch, 0, helpee, TO_NOTVICT);
//			if (!FIGHTING(ch))
//				set_fighting(ch, opponent);
			combat_event(ch, opponent, 0);
//			hit(ch, opponent, TYPE_UNDEFINED, 1);
		}
	}
	release_buffer(arg);
}


ACMD(do_hit) {
	CharData *vict;
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	
	one_argument(argument, arg);

	if (!*arg)
		send_to_char("Hit who?\r\n", ch);
	else if (!(vict = get_char_room_vis(ch, arg)))
		send_to_char("They don't seem to be here.\r\n", ch);
	else if (vict == ch) {
		send_to_char("You hit yourself...OUCH!.\r\n", ch);
		act("$n hits $mself, and says OUCH!", FALSE, ch, 0, vict, TO_ROOM);
	} else if (AFF_FLAGGED(ch, AFF_CHARM) && (ch->master == vict))
		act("$N is just such a good friend, you simply can't hit $M.", FALSE, ch, 0, vict, TO_CHAR);
	else {
		if ((GET_POS(ch) == POS_STANDING) && (vict != FIGHTING(ch))) {
/*      hit(ch, vict, TYPE_UNDEFINED);*/
//			set_fighting(ch, vict);
			combat_event(ch, vict, 0);
//			WAIT_STATE(ch, PULSE_VIOLENCE * 2);
		} else
			send_to_char("You do the best you can!\r\n", ch);
	}
	release_buffer(arg);
}



ACMD(do_kill) {
	CharData *vict;
	char *arg;
	
	if (!NO_STAFF_HASSLE(ch) || IS_NPC(ch)) {
		do_hit(ch, argument, 0, "hit", subcmd);
		return;
	}
	
	arg = get_buffer(MAX_INPUT_LENGTH);
	one_argument(argument, arg);

	if (!*arg) {
		send_to_char("Kill who?\r\n", ch);
	} else {
		if (!(vict = get_char_room_vis(ch, arg)))
			send_to_char("They aren't here.\r\n", ch);
		else if (ch == vict)
			send_to_char("Your mother would be so sad...\r\n", ch);
		else {
			act("You chop $M to pieces!  Ah!  The blood!", FALSE, ch, 0, vict, TO_CHAR);
			act("$N chops you to pieces!", FALSE, vict, 0, ch, TO_CHAR);
			act("$n brutally slays $N!", FALSE, ch, 0, vict, TO_NOTVICT);
			vict->die(ch);
		}
	}
	release_buffer(arg);
}



ACMD(do_backstab) {
	CharData *vict;
	int percent, prob;
	char *buf = get_buffer(MAX_INPUT_LENGTH);
	
	one_argument(argument, buf);

	if (!(vict = get_char_room_vis(ch, buf))) {
		send_to_char("Backstab who?\r\n", ch);
		release_buffer(buf);
		return;
	}
	release_buffer(buf);
	if (vict == ch) {
		send_to_char("How can you sneak up on yourself?\r\n", ch);
		return;
	}
	if (!GET_EQ(ch, WEAR_HAND_R) || GET_OBJ_TYPE(GET_EQ(ch, WEAR_HAND_R)) != ITEM_WEAPON) {
		send_to_char("You need to wield a weapon in your good hand to make it a success.\r\n", ch);
		return;
	}
	if (GET_OBJ_VAL(GET_EQ(ch, WEAR_HAND_R), 3) != TYPE_PIERCE - TYPE_HIT) {
		send_to_char("Only piercing weapons can be used for backstabbing.\r\n", ch);
		return;
	}
	if (FIGHTING(vict)) {
		send_to_char("You can't backstab a fighting person -- they're too alert!\r\n", ch);
		return;
	}
	if (MOB_FLAGGED(vict, MOB_AWARE) && !FIGHTING(vict)) {
		act("You notice $N lunging at you!", FALSE, vict, 0, ch, TO_CHAR);
		act("$e notices you lunging at $m!", FALSE, vict, 0, ch, TO_VICT);
		act("$n notices $N lunging at $m!", FALSE, vict, 0, ch, TO_NOTVICT);
//		if (!FIGHTING(vict))
//			set_fighting(vict, ch);
		combat_event(vict, ch, 0);
//		hit(vict, ch, TYPE_UNDEFINED, 1);
		WAIT_STATE(ch, 10 RL_SEC);
		return;
	}

	percent = Number(1, 101);	/* 101% is a complete failure */
//	prob = GET_SKILL(ch, SKILL_BACKSTAB);
	if (IS_NPC(ch)) {
		buf = get_buffer(MAX_INPUT_LENGTH);
		one_argument(argument, buf);
		prob = atoi(buf);
		release_buffer(buf);
	} else
		prob = GET_SKILL(ch, SKILL_BACKSTAB);

	if (AWAKE(vict) && (percent > prob))
		damage(ch, vict, 0, SKILL_BACKSTAB, 1);
	else
		hit(ch, vict, SKILL_BACKSTAB, 1);
	WAIT_STATE(ch, 10 RL_SEC);
}



ACMD(do_order) {
	char	*name = get_buffer(MAX_INPUT_LENGTH),
			*message = get_buffer(MAX_INPUT_LENGTH),
			*buf = get_buffer(MAX_INPUT_LENGTH);
	int found = FALSE;
	int org_room;
	CharData *vict, *follower;

	half_chop(argument, name, message);

	if (!*name || !*message)
		send_to_char("Order who to do what?\r\n", ch);
	else if (!(vict = get_char_room_vis(ch, name)) && !is_abbrev(name, "followers"))
		send_to_char("That person isn't here.\r\n", ch);
	else if (ch == vict)
		send_to_char("You obviously suffer from skitzofrenia.\r\n", ch);

	else {
		if (AFF_FLAGGED(ch, AFF_CHARM)) {
			send_to_char("Your superior would not aprove of you giving orders.\r\n", ch);
		} else if (vict) {
			act("$N orders you to '$t'", FALSE, vict, (ObjData *)message, ch, TO_CHAR);
			act("$n gives $N an order.", FALSE, ch, 0, vict, TO_NOTVICT);

			if ((vict->master != ch) || !AFF_FLAGGED(vict, AFF_CHARM))
				act("$n has an indifferent look.", FALSE, vict, 0, 0, TO_ROOM);
			else {
				send_to_char(OK, ch);
				command_interpreter(vict, message);
			}
		} else {			/* This is order "followers" */
			act("$n issues the order '$t'.", FALSE, ch, (ObjData *)message, vict, TO_ROOM);

			org_room = IN_ROOM(ch);

			START_ITER(iter, follower, CharData *, ch->followers) {
				if ((org_room == IN_ROOM(follower)) && AFF_FLAGGED(follower, AFF_CHARM)) {
					found = TRUE;
					command_interpreter(follower, message);
				}
			} END_ITER(iter);
			if (found)	send_to_char(OK, ch);
			else		send_to_char("Nobody here is a loyal subject of yours!\r\n", ch);
		}
	}
	release_buffer(name);
	release_buffer(message);
	release_buffer(buf);
}



ACMD(do_flee) {
	int i, attempt;

	if (GET_POS(ch) < POS_FIGHTING) {
		send_to_char("You are in pretty bad shape, unable to flee!\r\n", ch);
		return;
	}
	
	if (FindEvent(ch->events, gravity)) {
		send_to_char("You are falling, and can't grab anything!\r\n", ch);
		return;
	}
	
	WAIT_STATE(ch, (100 - GET_AGI(ch)) / 20);

	for (i = 0; i < 6; i++) {
		attempt = Number(0, NUM_OF_DIRS - 1);	/* Select a random direction */
		if (CAN_GO(ch, attempt) &&
/*	!IS_SET(ROOM_FLAGS(EXIT(ch, attempt)->to_room), ROOM_DEATH)*/
				!EXIT_FLAGGED(EXIT(ch, attempt), EX_NOMOVE)) {
			act("$n panics, and attempts to flee!", TRUE, ch, 0, 0, TO_ROOM);
			if (do_simple_move(ch, attempt, TRUE)) {
				send_to_char("You flee head over heels.\r\n", ch);
				if (FIGHTING(ch)) {
					if (FIGHTING(FIGHTING(ch)) == ch)
						FIGHTING(ch)->stop_fighting();
					ch->stop_fighting();
				}
			} else {
				act("$n tries to flee, but can't!", TRUE, ch, 0, 0, TO_ROOM);
			}
			return;
		}
	}
	send_to_char("PANIC!  You couldn't escape!\r\n", ch);
}


ACMD(do_bash) {
	CharData *vict;
	int percent, prob;
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	
	one_argument(argument, arg);

	if (!(vict = get_char_room_vis(ch, arg))) {
		if (FIGHTING(ch) && (IN_ROOM(ch) == IN_ROOM(FIGHTING(ch)))) {
			vict = FIGHTING(ch);
		} else {
			send_to_char("Bash who?\r\n", ch);
			release_buffer(arg);
			return;
		}
	}
	
	release_buffer(arg);
	
	if (vict == ch) {
		send_to_char("Aren't we funny today...\r\n", ch);
		return;
	}
	if ((!GET_EQ(ch, WEAR_HAND_R) || GET_OBJ_TYPE(GET_EQ(ch, WEAR_HAND_R)) != ITEM_WEAPON) &&
		(!GET_EQ(ch, WEAR_HAND_L) || GET_OBJ_TYPE(GET_EQ(ch, WEAR_HAND_L)) != ITEM_WEAPON)) {
		send_to_char("You need to wield a weapon to make it a success.\r\n", ch);
		return;
	}
	percent = Number(1, 101);	/* 101% is a complete failure */
//	prob = GET_SKILL(ch, SKILL_BASH);
	if (IS_NPC(ch)) {
		arg = get_buffer(MAX_INPUT_LENGTH);
		one_argument(argument, arg);
		prob = atoi(arg);
		release_buffer(arg);
	} else
		prob = GET_SKILL(ch, SKILL_BASH);

	if (MOB_FLAGGED(vict, MOB_NOBASH))
		percent = 101;

	if (percent > prob) {
		damage(ch, vict, 0, SKILL_BASH, 1);
		GET_POS(ch) = POS_SITTING;
	} else {
		//	There's a bug in the two lines below.  We could fail bash and they sit.
		damage(ch, vict, 1, SKILL_BASH, 1);
		GET_POS(vict) = POS_SITTING;
		WAIT_STATE(vict, PULSE_VIOLENCE);
	}
	WAIT_STATE(ch, PULSE_VIOLENCE * 2);
}


ACMD(do_rescue) {
	CharData *vict, *tmp_ch;
	int percent, prob;
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	
	one_argument(argument, arg);

	if (!(vict = get_char_room_vis(ch, arg))) {
		send_to_char("Whom do you want to rescue?\r\n", ch);
		release_buffer(arg);
		return;
	}
	release_buffer(arg);
	if (vict == ch) {
		send_to_char("What about fleeing instead?\r\n", ch);
		return;
	}
	if (FIGHTING(ch) == vict) {
		send_to_char("How can you rescue someone you are trying to kill?\r\n", ch);
		return;
	}

	START_ITER(iter, tmp_ch, CharData *, world[IN_ROOM(ch)].people) {
		if (FIGHTING(tmp_ch) == vict)
			break;
	} END_ITER(iter);

	if (!tmp_ch) {
		act("But nobody is fighting $M!", FALSE, ch, 0, vict, TO_CHAR);
		return;
	}
	percent = Number(1, 101);	/* 101% is a complete failure */
//	prob = GET_SKILL(ch, SKILL_RESCUE);
	if (IS_NPC(ch)) {
		arg = get_buffer(MAX_INPUT_LENGTH);
		one_argument(argument, arg);
		prob = atoi(arg);
		release_buffer(arg);
	} else
		prob = GET_SKILL(ch, SKILL_RESCUE);

	if (percent > prob) {
		send_to_char("You fail the rescue!\r\n", ch);
		return;
	}
	send_to_char("Banzai!  To the rescue...\r\n", ch);
	act("You are rescued by $N, you are confused!", FALSE, vict, 0, ch, TO_CHAR);
	act("$n heroically rescues $N!", FALSE, ch, 0, vict, TO_NOTVICT);

	if (FIGHTING(vict) == tmp_ch)
		vict->stop_fighting();
	if (FIGHTING(tmp_ch))
		tmp_ch->stop_fighting();
	if (FIGHTING(ch))
		ch->stop_fighting();

	set_fighting(ch, tmp_ch);
	set_fighting(tmp_ch, ch);

	WAIT_STATE(vict, PULSE_VIOLENCE * 2);
}


ACMD(do_kick_bite) {
	CharData *vict;
	int percent, prob;
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	
	one_argument(argument, arg);

	if (!(vict = get_char_room_vis(ch, arg))) {
		if (FIGHTING(ch)) {
			vict = FIGHTING(ch);
		} else {
			if (subcmd == SCMD_KICK)
				send_to_char("Kick who?\r\n", ch);
			else
				send_to_char("Bite who?\r\n", ch);
			release_buffer(arg);
			return;
		}
	}
	release_buffer(arg);
	
	if (vict == ch) {
		send_to_char("Aren't we funny today...\r\n", ch);
		return;
	}
	percent = ((10 - /*(GET_AC(vict) / 10)*/ 5) * 2) + Number(1, 101);
	/* 101% is a complete failure */

	if (IS_NPC(ch)) {
		arg = get_buffer(MAX_INPUT_LENGTH);
		one_argument(argument, arg);
		prob = atoi(arg);
		release_buffer(arg);
	} else
		prob = GET_SKILL(ch, subcmd);
//	prob = GET_SKILL(ch, subcmd);

	if (percent > prob) {
		damage(ch, vict, 0, subcmd, 1);
	} else
		damage(ch, vict, (GET_LEVEL(ch) * 5) + 100, subcmd, 1);
	WAIT_STATE(ch, 6 RL_SEC);
}


ACMD(do_shoot) {
	ObjData *gun;
	char *arg2;
	char *arg1;
	int range, dir = -1, fired;
	
	if (GET_POS(ch) == POS_FIGHTING) {
		send_to_char("You are too busy fighting!\r\n", ch);
		return;
	}
	
	if (IN_ROOM(ch) == -1 || ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
		send_to_char("But it is so peaceful here...\r\n", ch);
		return;
	}
		
	if (!(gun = GET_EQ(ch, WEAR_HAND_R)) || !IS_GUN(gun))
		gun = GET_EQ(ch, WEAR_HAND_L);

	if (!gun || !IS_GUN(gun)) {
		send_to_char("You aren't wielding a fireable weapon!\r\n", ch);
		return;
	}

	if (GET_GUN_AMMO(gun) <= 0) {
		send_to_char("*CLICK* Weapon isn't loaded!\r\n", ch);
		return;
	}

	if (!(range = GET_GUN_RANGE(gun))) {
		send_to_char("This is not a ranged weapon!", ch);
		return;
	}

	arg2 = get_buffer(MAX_INPUT_LENGTH);
	arg1 = get_buffer(MAX_INPUT_LENGTH);
	
	argument = two_arguments(argument, arg1, arg2);

	if (!*arg1)
		send_to_char("You should try: shoot <someone> [<direction>]\r\n", ch);
	else if (IS_DARK(IN_ROOM(ch)))
		send_to_char("You can't see that far.\r\n", ch);
	else if (*arg2 && (dir = search_block(arg2, dirs, FALSE)) < 0)
		send_to_char("What direction?\r\n", ch);
	else if (*arg2 && !CAN_GO(ch, dir))
		send_to_char("Something blocks the way!\r\n", ch);
	else if (*arg2 && EXIT_FLAGGED(EXIT(ch, dir), EX_NOSHOOT))
		send_to_char("It seems as if something is in the way!\r\n", ch);
	else {
		if (range > 5)
			range = 5;
		if (range < 1)
			range = 1;
		
		if (dir != -1)
			fired = fire_missile(ch, arg1, gun, 0, range, dir, 0);	//	NOTE: no skill used
		else {
			for (dir = 0; dir < NUM_OF_DIRS; dir++) {
				if ((fired = fire_missile(ch, arg1, gun, 0, range, dir, 0)))	//	NOTE: no skill used
					break;
			}
		}
		if (!fired)		send_to_char("Can't find your target!\r\n", ch);
		else			WAIT_STATE(ch, GET_OBJ_SPEED(gun) RL_SEC / 2);
	}
	release_buffer(arg1);
	release_buffer(arg2);
}


ACMD(do_throw) {

/* sample format: throw monkey east
   this would throw a throwable or grenade object wielded
   into the room 1 east of the pc's current room. The chance
   to hit the monkey would be calculated based on the pc's skill.
   if the wielded object is a grenade then it does not 'hit' for
   damage, it is merely dropped into the room. (the timer is set
   with the 'pull pin' command.) */

	ObjData *missile;
	int dir, range = 1;
	char *arg2;
	char *arg1;
	char *searchArg = NULL;
	
/* only two types of throwing objects: 
   ITEM_THROW - knives, stones, etc
   ITEM_GRENADE - calls tick_grenades.c . */

	missile = GET_EQ(ch, WEAR_HAND_L);
	if (!missile || !((GET_OBJ_TYPE(missile) == ITEM_THROW) ||
			(GET_OBJ_TYPE(missile) == ITEM_GRENADE) ||
			(GET_OBJ_TYPE(missile) == ITEM_BOOMERANG)))
		missile = GET_EQ(ch, WEAR_HAND_R);

	if (!missile || !((GET_OBJ_TYPE(missile) == ITEM_THROW) ||
			(GET_OBJ_TYPE(missile) == ITEM_GRENADE) ||
			(GET_OBJ_TYPE(missile) == ITEM_BOOMERANG))) { 
		send_to_char("You should hold a throwing weapon first!\r\n", ch);
		return;
	}

	arg1 = get_buffer(MAX_INPUT_LENGTH);
	arg2 = get_buffer(MAX_INPUT_LENGTH);
	
	two_arguments(argument, arg1, arg2);
	
	if (!*arg1)
		send_to_char("You should try: throw [<someone>] <direction> [<distance>]\r\n", ch);
	else if (GET_OBJ_TYPE(missile) == ITEM_GRENADE) {
		searchArg = arg1;
		range = *arg2 ? atoi(arg2) : 1;
	} else if (!*arg2)
		send_to_char("You should try: throw <direction> <someone>\r\n", ch);
	else
		searchArg = arg2;
	
	if (searchArg) {
		if ((dir = search_block(searchArg, dirs, FALSE)) < 0) {
			send_to_char("What direction?\r\n", ch);
		} else if (!CAN_GO(ch, dir)) {		/* make sure we can go in the direction throwing. */
			send_to_char("Something blocks the way!\r\n", ch);
		} else if (EXIT_FLAGGED(EXIT(ch, dir), EX_NOSHOOT)) {
			send_to_char("It seems as if something is in the way!\r\n", ch);
		} else {
			if (GET_OBJ_TYPE(missile) == ITEM_BOOMERANG)
				range = GET_OBJ_VAL(missile, 5);

			fire_missile(ch, arg1, missile, (GET_EQ(ch, WEAR_HAND_R) == missile) ? WEAR_HAND_R : WEAR_HAND_L,
					range, dir, IS_NPC(ch) ? GET_AGI(ch) : GET_SKILL(ch, SKILL_THROW));
		}
	}
	
	release_buffer(arg1);
	release_buffer(arg2);
}


ACMD(do_circle) {
	CharData *victim = NULL;
	char *arg, *arg2;
	int prob, percent = 0;

	arg = get_buffer(MAX_INPUT_LENGTH);
	argument = one_argument(argument, arg);
	
	if (IS_NPC(ch)) {
		arg2 = get_buffer(MAX_INPUT_LENGTH);
		one_argument(argument, arg2);
		prob = atoi(arg2);
		release_buffer(arg2);
	} else
		prob = GET_SKILL(ch, SKILL_CIRCLE);
	
	if (!(victim = get_char_room_vis(ch, arg))) {
		if (FIGHTING(ch))
			victim = FIGHTING(ch);
	}
	
	if (!prob)
		send_to_char("You aren't quite skilled enough to pull off a stunt like that...", ch);
	else if (victim == ch)
		send_to_char("Are you stupid or something?", ch);
	else if (victim) {
		percent = Number(1, 100);
		percent += GET_PER(victim);		// PER to notice
		percent += GET_AGI(victim);		// DEX to react
		percent += GET_INT(victim) / 2;	// INT to not fall for it
		percent -= GET_AGI(ch);			// DEX to try
		percent -= GET_INT(victim) / 2;	// INT to outwit
		if (percent <= prob) {
//			act("$n circles around $N, confusing the hell out of $M.", FALSE, ch, 0, victim, TO_NOTVICT);
//			act("You circle behind $N, confusing $M, and getting a chance to strike!", FALSE, ch, 0, victim, TO_CHAR);
//			act("$n leaps to the $t, and you lose track of $m.", FALSE, ch, (ObjData *)(Number(0,1) ? "right" : "left"), victim, TO_VICT);
			hit(ch, victim, SKILL_CIRCLE, 2);
//			hit(ch, victim, SKILL_CIRCLE, 1);
			if (percent < 0)	GET_POS(victim) = POS_STUNNED;
		} else if (percent >= 100 && (Number(1,100) > GET_AGI(ch))) {
			act("$n leaps to the side of $N, stumbles, and falls flat on $s face!", FALSE, ch, 0, victim, TO_NOTVICT);
			act("You try to circle behind $N, but stumble and fall flat on your face!", FALSE, ch, 0, victim, TO_CHAR);
			act("$n manages to fall flat on $s face when $e leaps at you.", FALSE, ch, 0, victim, TO_VICT);			
			GET_POS(victim) = POS_STUNNED;
		} else {
			act("$n tries to circle around $N, but $N tracks $s moves.", FALSE, ch, 0, victim, TO_NOTVICT);
			act("You try to circle behind $N, but $E manages to keep up with you.", FALSE, ch, 0, victim, TO_CHAR);
			act("$n tries to circle around you, but you manage to follow $m.", FALSE, ch, 0, victim, TO_VICT);			
		}
		WAIT_STATE(ch, 10 RL_SEC);
	} else
		act("Circle whom?", TRUE, ch, 0, 0, TO_CHAR);
	release_buffer(arg);
}


ACMD(do_pkill) {
	char *arg, *extra;
	HelpElement *	el;
	
	if (!(el = find_help("pkill")))
		return;
	
	if (IS_NPC(ch)) {
		send_to_char("You're a MOB, you can already kill players!", ch);
		return;
	}
	
	arg = get_buffer(MAX_STRING_LENGTH);
	one_argument(argument, arg);
	
	if (!*arg || !str_cmp(arg, "yes")) {
		extra = "`bTo begin pkilling, type \"`RPKILL YES`b\".\r\n"
				"Remember, there is no turning back!`n";
	} else {
		SET_BIT(PRF_FLAGS(ch), PRF_PK);
		extra = "`RYou are now a player killer!`n";
	}
	strcpy(arg, el->entry);
	strcat(arg, extra);
	page_string(ch->desc, arg, true);
}
