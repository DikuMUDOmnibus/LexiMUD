/* ************************************************************************
*   File: fight.c                                       Part of CircleMUD *
*  Usage: Combat system                                                   *
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
#include "scripts.h"
#include "handler.h"
#include "find.h"
#include "interpreter.h"
#include "db.h"
#include "spells.h"
#include "event.h"
#include "affects.h"
#include "track.h"


struct msg_type {
	char *			attacker_msg;		//	message to attacker
	char *			victim_msg;			//	message to victim
	char *			room_msg;			//	message to room
};


struct message_type {
	struct msg_type	die_msg;		//	messages when death
	struct msg_type	miss_msg;		//	messages when miss
	struct msg_type	hit_msg;		//	messages when hit
	struct msg_type	god_msg;		//	messages when hit on god
};


struct message_list {
	SInt32			a_type;					//	Attack type
	SInt32			number_of_attacks;		//	How many attack messages to chose from.
	LList<message_type *>msg;				//	List of messages.
};


struct DamMesgType {
	char *			to_room;
	char *			to_char;
	char *			to_vict;
};


/* Structures */
LList<CharData *>	combat_list;


/* External structures */
struct message_list fight_messages[MAX_MESSAGES];
extern int pk_allowed;		/* see config.c */
extern int auto_save;		/* see config.c */
extern char *dirs[];
extern char *from_dir[];
extern int rev_dir[];
extern int max_npc_corpse_time, max_pc_corpse_time;
extern int thaco[NUM_RACES+1][2];
extern char *times_string[];
extern char * dir_text_2[];

/* External procedures */
char *fread_action(FILE * fl, int nr);
ACMD(do_flee);
ACMD(do_get);
void forget(CharData * ch, CharData * victim);
void remember(CharData * ch, CharData * victim);
int ok_damage_shopkeeper(CharData * ch, CharData * victim);
void mprog_hitprcnt_trigger(CharData * mob, CharData * ch);
void mprog_fight_trigger(CharData * mob, CharData * ch);
int combat_delay(CharData *ch);
EVENT(combat_event);
void AlterHit(CharData *ch, SInt32 amount);

/* local functions */
void appear(CharData * ch);
void load_messages(void);
void check_killer(CharData * ch, CharData * vict);
void make_corpse(CharData * ch);
void death_cry(CharData * ch);
void group_gain(CharData * ch, CharData * victim);
char *replace_string(char *str, char *weapon_singular, char *weapon_plural);
void perform_violence(void);
void dam_message(int dam, CharData * ch, CharData * victim, int w_type);
void multi_dam_message (int damage, CharData *ch, CharData *vict, int w_type, int times);
void explosion(CharData * ch, int dice_num, int dice_size, int room, int shrapnel);
void acidblood(CharData *ch, int dam);
void acidspray(int room, int dam);
void announce(char *buf);
int skill_roll(CharData *ch, int skill_num);
int strike_missile(CharData *ch, CharData *tch, 
                   ObjData *missile, int dir, int attacktype);
void miss_missile(CharData *ch, CharData *tch, 
                ObjData *missile, int dir, int attacktype);
void mob_reaction(CharData *ch, CharData *vict, int dir);
int fire_missile(CharData *ch, char *arg1, ObjData *missile, SInt32 pos, SInt32 range, SInt32 dir, SInt32 skill);
UInt32	calc_reward(CharData *ch, CharData *victim);
bool kill_valid(CharData *ch, CharData *vict);
void ExtractNoKeep(ObjData *obj, ObjData *corpse, CharData *ch);


/* Weapon attack texts */
struct attack_hit_type attack_hit_text[] =
{
  {"hit", "hits"},		/* 0 */
  {"sting", "stings"},
  {"whip", "whips"},
  {"slash", "slashes"},
  {"bite", "bites"},
  {"bludgeon", "bludgeons"},	/* 5 */
  {"crush", "crushes"},
  {"pound", "pounds"},
  {"claw", "claws"},
  {"maul", "mauls"},
  {"thrash", "thrashes"},	/* 10 */
  {"pierce", "pierces"},
  {"blast", "blasts"},
  {"punch", "punches"},
  {"stab", "stabs"},
  {"shoot", "shoots"},		/* 15 */
  {"burn", "burns"}
};

#define IS_WEAPON(type) (((type) >= TYPE_HIT) && ((type) < TYPE_SUFFERING))
#define DEDUCT_AMMO(ch, eq) if (--GET_GUN_AMMO(eq) < 1)\
						send_to_char("*CLICK* You're weapon is out of ammo\r\n", ch);


/* The Fight related routines */

void appear(CharData * ch) {
	REMOVE_BIT(AFF_FLAGS(ch), AFF_INVISIBLE | AFF_HIDE);

	if (IS_STAFF(ch))							act("You feel a strange presence as $n appears, seemingly from nowhere.", FALSE, ch, 0, 0, TO_ROOM);
	else if (GET_RACE(ch) == RACE_PREDATOR)		act("A faint shimmer in the air slowly becomes more real as $n appears.", FALSE, ch, 0, 0, TO_ROOM);
	else										act("$n slowly fades into existence.", FALSE, ch, 0, 0, TO_ROOM);
}


void load_messages(void) {
	FILE *fl;
	int i, type;
	struct message_type *messages;
	char *chk = get_buffer(128);

	if (!(fl = fopen(MESS_FILE, "r"))) {
		log("Error reading combat message file %s", MESS_FILE);
		exit(1);
	}
	for (i = 0; i < MAX_MESSAGES; i++) {
		fight_messages[i].a_type = 0;
		fight_messages[i].number_of_attacks = 0;
		fight_messages[i].msg.Erase();
//		memset(&fight_messages[i].msg, 0, sizeof(LList<message_type *>));
	}


	fgets(chk, 128, fl);
	while (!feof(fl) && (*chk == '\n' || *chk == '*'))
		fgets(chk, 128, fl);

	while (*chk == 'M') {
		fgets(chk, 128, fl);
		sscanf(chk, " %d\n", &type);
		for (i = 0; (i < MAX_MESSAGES) && (fight_messages[i].a_type != type) &&
				(fight_messages[i].a_type); i++);
		if (i >= MAX_MESSAGES) {
			log("Too many combat messages.  Increase MAX_MESSAGES and recompile.");
			exit(1);
		}
		CREATE(messages, struct message_type, 1);
		fight_messages[i].number_of_attacks++;
		fight_messages[i].a_type = type;
		fight_messages[i].msg.Add(messages);
//		messages->next = fight_messages[i].msg;
//		fight_messages[i].msg = messages;

		messages->die_msg.attacker_msg = fread_action(fl, i);
		messages->die_msg.victim_msg = fread_action(fl, i);
		messages->die_msg.room_msg = fread_action(fl, i);
		messages->miss_msg.attacker_msg = fread_action(fl, i);
		messages->miss_msg.victim_msg = fread_action(fl, i);
		messages->miss_msg.room_msg = fread_action(fl, i);
		messages->hit_msg.attacker_msg = fread_action(fl, i);
		messages->hit_msg.victim_msg = fread_action(fl, i);
		messages->hit_msg.room_msg = fread_action(fl, i);
		messages->god_msg.attacker_msg = fread_action(fl, i);
		messages->god_msg.victim_msg = fread_action(fl, i);
		messages->god_msg.room_msg = fread_action(fl, i);
		fgets(chk, 128, fl);
		while (!feof(fl) && (*chk == '\n' || *chk == '*'))
			fgets(chk, 128, fl);
	}
	
	release_buffer(chk);
	fclose(fl);
}


void check_killer(CharData * ch, CharData * vict) {
	if (!pk_allowed || (ch == vict) || NO_STAFF_HASSLE(ch))
		return;
	
	if (ch->GetRelation(vict) != RELATION_FRIEND)
		return;
	
	if (IS_NPC(ch)) {
		SET_BIT(MOB_FLAGS(ch), MOB_TRAITOR);
	} else {
		SET_BIT(PLR_FLAGS(ch), PLR_TRAITOR);
		send_to_char("If you want to be a TRAITOR, so be it...\r\n", ch);
	}
}


bool kill_valid(CharData *ch, CharData *vict) {
	return true;		// DEFAULT for now, deactivate later!
	if (IS_NPC(ch) || IS_NPC(vict))
		return true;
	if (!PRF_FLAGGED(ch, PRF_PK) || !PRF_FLAGGED(vict, PRF_PK))
		return false;
}



/* start one char fighting another (yes, it is horrible, I know... )  */
void set_fighting(CharData * ch, CharData * vict) {
	if ((ch == vict) || PURGED(ch))
		return;

	if (FIGHTING(ch)) {
		core_dump();
		return;
	}

	combat_list.Add(ch);
	
	if (AFF_FLAGGED(ch, AFF_SLEEP))
		Affect::FromChar(ch, Affect::Sleep);
	

	FIGHTING(ch) = vict;
	if (ch->sitting_on)
		ch->sitting_on = NULL;
	GET_POS(ch) = POS_FIGHTING;

	/*  if (!pk_allowed)*/
	check_killer(ch, vict);
}



/* remove a char from the list of fighting chars */
void CharData::stop_fighting(void) {
	combat_list.Remove(this);
	FIGHTING(this) = NULL;
	GET_POS(this) = POS_STANDING;
	this->update_pos();
}


void ExtractNoKeep(ObjData *obj, ObjData *corpse, CharData *ch) {
	ObjData *tmp;

	if (NO_LOSE(obj)) {
		if (IS_NPC(ch))		obj->extract();
		else				return;
	}
	
	LListIterator<ObjData *>	iter(obj->contains);
	while ((tmp = iter.Next())) {
		ExtractNoKeep(tmp, corpse, ch);
	}
	if (IS_NPC(ch) || !obj->contains.Count()) {
		if (obj->worn_by)			obj = unequip_char(ch, obj->worn_on);
		if (obj->in_obj)			obj->from_obj();
		else if (obj->carried_by)	obj->from_char();
		obj->to_obj(corpse);
	}
}


void make_corpse(CharData * ch) {
	ObjData *corpse, *o;
	int i;
	char *buf = get_buffer(SMALL_BUFSIZE);
	
	corpse = create_obj();
	
	GET_OBJ_RNUM(corpse) = NOTHING;
	IN_ROOM(corpse) = NOWHERE;
	corpse->name = SSCreate("corpse");
	
	sprintf(buf, "The corpse of %s is lying here.", ch->GetName());
	corpse->description = SSCreate(buf);
	
	sprintf(buf, "the corpse of %s", ch->GetName());
	corpse->shortDesc = SSCreate(buf);
	
	release_buffer(buf);
	
	GET_OBJ_TYPE(corpse)	= ITEM_CONTAINER;
	GET_OBJ_WEAR(corpse)	= (int)NULL;/*ITEM_WEAR_TAKE*/ 
	GET_OBJ_EXTRA(corpse)	= ITEM_NODONATE;
	GET_OBJ_VAL(corpse, 0)	= 0;	/* You can't store stuff in a corpse */
	GET_OBJ_VAL(corpse, 3)	= 1;	/* corpse identifier */
	GET_OBJ_WEIGHT(corpse)	= GET_WEIGHT(ch); /* + IS_CARRYING_W(ch) */
	GET_OBJ_TIMER(corpse)	= (IS_NPC(ch) ? max_npc_corpse_time : max_pc_corpse_time);
	
	/* transfer character's equipment to the corpse */
	for (i = 0; i < NUM_WEARS; i++)
		if (GET_EQ(ch, i))
			ExtractNoKeep(GET_EQ(ch, i), corpse, ch);
	
	/* transfer character's inventory to the corpse */
/*	corpse->contains = ch->carrying;*/
	LListIterator<ObjData *>	iter(ch->carrying);
	while ((o = iter.Next())) {
		ExtractNoKeep(o, corpse, ch);
	}
	
	if (!IS_NPC(ch)) {
		Crash_crashsave(ch);
	}

	corpse->to_room(IN_ROOM(ch));
}


void death_cry(CharData * ch) {
	int door, was_in;

	act("Your blood freezes as you hear $n's death cry.", FALSE, ch, 0, 0, TO_ROOM);
	was_in = IN_ROOM(ch);

	for (door = 0; door < NUM_OF_DIRS; door++) {
		if (CAN_GO(ch, door)) {
/*      IN_ROOM(ch) = world[was_in].dir_option[door]->to_room;
      act("Your blood freezes as you hear someone's death cry.", FALSE, ch, 0, 0, TO_ROOM);
      IN_ROOM(ch) = was_in;*/
			world[EXIT(ch,door)->to_room].Send("Your blood freezes as you hear someone's death cry.\r\n");
		}
	}
}


void announce(char *buf) {
	DescriptorData *pt;
	START_ITER(iter, pt, DescriptorData *, descriptor_list) {
		if ((STATE(pt) == CON_PLAYING) && !CHN_FLAGGED(pt->character, Channel::NoInfo))
			send_to_char(buf, pt->character);
	} END_ITER(iter);
}


UInt32	calc_reward(CharData *ch, CharData *victim) {
	int	value;
	if (IS_NPC(victim))	value = (GET_MAX_HIT(victim) / 500);
	else				value = (GET_LEVEL(victim) / 4);
	value = MAX(1, value);
	return (!IS_NPC(victim) || GET_MAX_HIT(victim) >= 500) ? value : 0;
}


void group_gain(CharData * ch, CharData * victim) {
	CharData *k, *follower;
	SInt32		value;
	k = (ch->master) ? ch->master : ch;
	
	if (!IS_NPC(k) && !IS_TRAITOR(k) && (k->GetRelation(victim) == RELATION_ENEMY)) {
		if ((value = calc_reward(k, victim)))
			GET_MISSION_POINTS(k) += MAX(1,  value / (k->followers.Count() ? 2 : 1));
	}
	START_ITER(iter, follower, CharData *, k->followers) {
		if (AFF_FLAGGED(follower, AFF_GROUP) && IN_ROOM(follower) == IN_ROOM(ch))
			if (!IS_NPC(follower) && !IS_TRAITOR(follower) && (ch->GetRelation(victim) == RELATION_ENEMY)) {
				if ((value = calc_reward(follower, victim)))
					GET_MISSION_POINTS(follower) += MAX(1,  value / 2);
			}
	} END_ITER(iter);
}


char *replace_string(char *str, char *weapon_singular, char *weapon_plural) {
	static char buf[256];
	char *cp;

	cp = buf;

	for (; *str; str++) {
		if (*str == '#') {
			switch (*(++str)) {
				case 'W':
					for (; *weapon_plural; *(cp++) = *(weapon_plural++));
					break;
				case 'w':
					for (; *weapon_singular; *(cp++) = *(weapon_singular++));
					break;
				default:
					*(cp++) = '#';
					break;
			}
		} else
			*(cp++) = *str;

		*cp = 0;
	}

	return (buf);
}


static DamMesgType dam_weapon[] = {
   // use #w for singular (i.e. "slash") and #W for plural (i.e. "slashes")
   { "$n tries to #w $N",					//	0: 0
     "You try to #w $N",
     "$n tries to #w you"},
   { "$n grazes $N with $s #w",				//	1: 1-10
     "You graze $N with your #w",
     "$n grazes you with $s #w" },
   { "$n barely #W $N",						//	2: 11-25
     "You barely #w $N",
     "$n barely #W you" },
/*   { "$n lightly #W $N",					//	2: 26-50
     "You lightly #W $N",
     "$n lightly #W you" },*/
   { "$n #W $N",							//	3: 26-50
     "You #w $N",
     "$n #W you" },
   { "$n #W $N hard",						//	4: 51-75
     "You #w $N hard",
     "$n #W you hard" },
   { "$n #W $N rather hard",				//	5: 76-125
     "You #w $N rather hard",
     "$n #W you rather hard" },
   { "$n #W $N very hard",					//	6: 126-250
     "You #w $N very hard",
     "$n #W you very hard" },
   { "$n #W $N extremely hard",				//	7: 251-500
     "You #w $N extremely hard",
     "$n #W you extremely hard" },
   { "$n massacres $N with $s #wing",		//	8: 501-999
     "You massacre $N with your #wing",
     "$n massacres you with $s #wing" },
   { "$n annihilates $N with $s #wing",		//	9: > 1000
     "You annihilate $N with your #wing",
     "$n annihilates you with $s #wing" }
};


static DamMesgType dam_amount[] = {
   { "$E hardly notices.",                   // 0: 0%-1%
     "$E hardly notices.",
     "you hardly notice." },
   { "barely hurts $M.",                     // 1: 2%
     "barely hurts $M.",
     "barely hurts you." },
   { "slightly wounds $M.",                  // 2: 3%-5%
     "slightly wounds $M.",
     "slightly wounds you." },
   { "wounds $M.",                           // 3: 6%-10%
     "wounds $M.",
     "wounds you." },
   { "severely wounds $M.",                  // 4: 11%-20%
     "severely wounds $M.",
     "severely wounds you." },
   { "maims $M.",                            // 5: 21%-30%
     "maims $M.",
     "maims you." },
   { "seriously maims $M!",                  // 6: 31%-45%
     "seriously maims $M!",
     "seriously maims you!" },
   { "cripples $M!",                         // 7: 46%-65%
     "cripples $M!",
     "cripples you!" },
   { "completely cripples $M!",              // 8: 66%-90%
     "completely cripples $M!",
     "completely cripples you!" },
   { "totally ANNIHILATES $M!!",             // 9: > 90%
     "totally ANNIHILATES $M!!",
     "totally ANNIHILATES you!!" }
};


// message for doing damage with a weapon
void dam_message (int damage, CharData *ch, CharData *vict, int w_type) {
	char *message;
	int amount_index, weapon_index, percent;

	w_type -= TYPE_HIT;          // Change to base of table with text

	percent = (damage * 100 / GET_MAX_HIT (vict));

	if		(percent <= 1)		amount_index = 0;
	else if	(percent <= 2)		amount_index = 1;
	else if	(percent <= 5)		amount_index = 2;
	else if	(percent <= 10)		amount_index = 3;
	else if	(percent <= 20)		amount_index = 4;
	else if	(percent <= 30)		amount_index = 5;
	else if	(percent <= 45)		amount_index = 6;
	else if	(percent <= 65)		amount_index = 7;
	else if	(percent <= 90)		amount_index = 8;
	else						amount_index = 9;

	if		(damage <= 0)		weapon_index = 0;
	else if	(damage <= 10)		weapon_index = 1;
	else if	(damage <= 25)		weapon_index = 2;
	else if	(damage <= 50)		weapon_index = 3;
	else if	(damage <= 75)		weapon_index = 4;
	else if	(damage <= 125)		weapon_index = 5;
	else if	(damage <= 250)		weapon_index = 6;
	else if	(damage <= 500)		weapon_index = 7;
	else if	(damage < 1000)		weapon_index = 8;
	else						weapon_index = 9;

	message = get_buffer(MAX_STRING_LENGTH);

	// message to onlookers
	
	sprintf(message, "(1 %s) %s, %s %s",
			weapon_index > 0 ? "hit" : "miss",
			replace_string (dam_weapon[weapon_index].to_room,
					attack_hit_text[w_type].singular,
					attack_hit_text[w_type].plural),
			weapon_index > 0 ? "which" : "but",
			weapon_index > 0 ? dam_amount[amount_index].to_room : "miss.");
	act (message, FALSE, ch, NULL, vict, TO_NOTVICT);

	// message to damager
	sprintf(message, "(1 %s) %s, %s %s",
			weapon_index > 0 ? "hit" : "miss",
			replace_string (dam_weapon[weapon_index].to_char,
					attack_hit_text[w_type].singular,
					attack_hit_text[w_type].plural),
			weapon_index > 0 ? "which" : "but",
			weapon_index > 0 ? dam_amount[amount_index].to_char : "miss.");
	act (message, FALSE, ch, NULL, vict, TO_CHAR);

	// message to damagee
	sprintf(message, "(1 %s) %s, %s %s",
			weapon_index > 0 ? "hit" : "miss",
			replace_string (dam_weapon[weapon_index].to_vict,
					attack_hit_text[w_type].singular,
					attack_hit_text[w_type].plural),
			weapon_index > 0 ? "which" : "but",
			weapon_index > 0 ? dam_amount[amount_index].to_vict : "miss.");
	act (message, FALSE, ch, NULL, vict, TO_VICT | TO_SLEEP);
	release_buffer(message);
}


// message for doing damage with a weapon
void multi_dam_message (int damage, CharData *ch, CharData *vict, int w_type, int times) {
	char *message;
	int amount_index, weapon_index, percent;

	w_type -= TYPE_HIT;          // Change to base of table with text


	percent = (damage * 100 / GET_MAX_HIT (vict));

	if		(percent <= 1)		amount_index = 0;
	else if	(percent <= 2)		amount_index = 1;
	else if	(percent <= 5)		amount_index = 2;
	else if	(percent <= 10)		amount_index = 3;
	else if	(percent <= 20)		amount_index = 4;
	else if	(percent <= 30)		amount_index = 5;
	else if	(percent <= 45)		amount_index = 6;
	else if	(percent <= 60)		amount_index = 7;
	else if	(percent <= 80)		amount_index = 8;
	else						amount_index = 9;

	if		(damage <= 0)		weapon_index = 0;
	else if	(damage <= 10)		weapon_index = 1;
	else if	(damage <= 25)		weapon_index = 2;
	else if	(damage <= 50)		weapon_index = 3;
	else if	(damage <= 75)		weapon_index = 4;
	else if	(damage <= 125)		weapon_index = 5;
	else if	(damage <= 250)		weapon_index = 6;
	else if	(damage <= 500)		weapon_index = 7;
	else if	(damage < 1000)		weapon_index = 8;
	else						weapon_index = 9;

	message = get_buffer(MAX_STRING_LENGTH);

	// message to onlookers
	sprintf(message, "(%d %s) %s %s, %s %s",
			times,
			weapon_index > 0 ? "hits" : "misses",
			replace_string (dam_weapon[weapon_index].to_room,
					attack_hit_text[w_type].singular,
					attack_hit_text[w_type].plural),
			times_string[times],
			weapon_index > 0 ? "which" : "but",
			weapon_index > 0 ? dam_amount[amount_index].to_room : "miss.");
	act (message, FALSE, ch, NULL, vict, TO_NOTVICT);

	// message to damager
	sprintf(message, "(%d %s) %s %s, %s %s",
			times,
			weapon_index > 0 ? "hits" : "misses",
			replace_string (dam_weapon[weapon_index].to_char,
					attack_hit_text[w_type].singular,
					attack_hit_text[w_type].plural),
			times_string[times],
			weapon_index > 0 ? "which" : "but",
			weapon_index > 0 ? dam_amount[amount_index].to_char : "miss.");
	act (message, FALSE, ch, NULL, vict, TO_CHAR);

	// message to damagee
	sprintf(message, "(%d %s) %s %s, %s %s",
			times,
			weapon_index > 0 ? "hits" : "misses",
			replace_string (dam_weapon[weapon_index].to_vict,
					attack_hit_text[w_type].singular,
					attack_hit_text[w_type].plural),
			times_string[times],
			weapon_index > 0 ? "which" : "but",
			weapon_index > 0 ? dam_amount[amount_index].to_vict : "miss.");
	act (message, FALSE, ch, NULL, vict, TO_VICT | TO_SLEEP);
	release_buffer(message);
}


/*
 * message for doing damage with a spell or skill
 *  C3.0: Also used for weapon damage on miss and death blows
 */
int skill_message(int dam, CharData * ch, CharData * vict, int attacktype) {
	int i, j, nr;
	struct message_type *msg;

	ObjData *weap = GET_EQ(ch, WEAR_HAND_R);

	for (i = 0; i < MAX_MESSAGES; i++) {
		if (fight_messages[i].a_type == attacktype) {
			nr = dice(1, fight_messages[i].number_of_attacks);
//			for (j = 1, msg = fight_messages[i].msg; (j < nr) && msg; j++)
//				msg = msg->next;
			j = 1;
			START_ITER(iter, msg, message_type *, fight_messages[i].msg) {
				if (j >= nr)
					break;
				j++;
			} END_ITER(iter);


			if (NO_STAFF_HASSLE(vict)) {
				act(msg->god_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR);
				act(msg->god_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT);
				act(msg->god_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT);
			} else if (dam != 0) {
				if (GET_POS(vict) == POS_DEAD) {
					send_to_char("`y", ch);
					act(msg->die_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR);
					send_to_char("`n", ch);

					send_to_char("`r", vict);
					act(msg->die_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP);
					send_to_char("`n", vict);

					act(msg->die_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT);
				} else {
					send_to_char("`y", ch);
					act(msg->hit_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR);
					send_to_char("`n", ch);

					send_to_char("`r", vict);
					act(msg->hit_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP);
					send_to_char("`n", vict);

					act(msg->hit_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT);
				}
			} else if (ch != vict) {	/* Dam == 0 */
				send_to_char("`y", ch);
				act(msg->miss_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR);
				send_to_char("`n", ch);

				send_to_char("`r", vict);
				act(msg->miss_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP);
				send_to_char("`n", vict);

				act(msg->miss_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT);
			}
			return 1;
		}
	}
	return 0;
}

// NEW: return 1 if you killed them or if the call should not be repeated.
// NEW: Multiple hits in one string!
int damage(CharData * ch, CharData * victim, int dam, int attacktype, int times) {
	int room;
	int missile = FALSE;
//	int count;
//	ObjData *weapon;

	if (times < 0 || times > 12) {
		log("SYSERR: times too %s in damage().  (times = %d)", times < 0 ? "low" : "great", times);
		return 1;
	}
	
	if (GET_POS(victim) <= POS_DEAD) {
		log("SYSERR: Attempt to damage a corpse '%s' in room #%d by '%s'", victim->RealName(),
				world[IN_ROOM(victim)].number, ch ? ch->RealName() : "<nobody>");
		victim->die(NULL);
		return 1;
	}

	if (ch) {
		//	Non-PK-flagged players going at it...
		if (!kill_valid(ch, victim)) {
			if (FIGHTING(ch) == victim)
				ch->stop_fighting();
			return 1;
		}

		if (IN_ROOM(ch) != IN_ROOM(victim))
			missile = TRUE;

		/* peaceful rooms */
		if ((ch != victim) && ROOM_FLAGGED(IN_ROOM(victim), ROOM_PEACEFUL) &&
				(attacktype < TYPE_SUFFERING) && !NO_STAFF_HASSLE(ch)) {
			send_to_char("This room just has such a peaceful, easy feeling...\r\n", ch);
			if (FIGHTING(ch) && (FIGHTING(ch) == victim))
				ch->stop_fighting();
			return 1;
		}

	  /* shopkeeper protection */
		if (!ok_damage_shopkeeper(ch, victim))
			return 1;

	/* Daniel Houghton's missile modification */
		if (victim != ch && !missile) {
			/* Start the attacker fighting the victim */
			if (GET_POS(ch) > POS_STUNNED && !FIGHTING(ch) && 
					(IS_NPC(ch) || attacktype < TYPE_SUFFERING))
				set_fighting(ch, victim);
				
			/* Start the victim fighting the attacker */
			if (GET_POS(victim) > POS_STUNNED && !FIGHTING(victim) &&
					attacktype < TYPE_SUFFERING) {
				set_fighting(victim, ch);
				if (MOB_FLAGGED(victim, MOB_MEMORY) && !IS_NPC(ch))
					remember(victim, ch);
			}
		}
		/* If you attack a pet, it hates your guts */
		if (victim->master == ch)
			victim->stop_follower();

		/* If the attacker is invisible, he becomes visible */
		if (attacktype < TYPE_SUFFERING && AFF_FLAGGED(ch, AFF_INVISIBLE | AFF_HIDE))
			appear(ch);
	}

	/* peaceful rooms */
	if (ROOM_FLAGGED(IN_ROOM(victim), ROOM_PEACEFUL) && ch && !NO_STAFF_HASSLE(ch))
		return 1;

	if (dam && attacktype != TYPE_SUFFERING) {
		if (attacktype >= TYPE_SHOOT && attacktype < TYPE_SUFFERING)
			dam -= Number(GET_AC(victim) / 8, GET_AC(victim) / 2);
		else
			dam -= Number(GET_AC(victim) / 4, GET_AC(victim));
							/* Damage is between 1/4th victim's AC and their whole AC */

		if (AFF_FLAGGED(victim, AFF_SANCTUARY))
			dam = dam / 2;		/* 1/2 damage when sanctuary */

		dam = MAX(dam, 1);
	}
	
	if (NO_STAFF_HASSLE(victim))
		dam = 0;

	AlterHit(victim, dam);	
//	GET_HIT(victim) -= dam;
		
/*	if (ch && ch != victim)
		gain_exp(ch, GET_LEVEL(victim) * dam);*/
	
	victim->update_pos();

  /*
   * skill_message sends a message from the messages file in lib/misc.
   * dam_message just sends a generic "You hit $n extremely hard.".
   * skill_message is preferable to dam_message because it is more
   * descriptive.
   * 
   * If we are _not_ attacking with a weapon (i.e. a spell), always use
   * skill_message. If we are attacking with a weapon: If this is a miss or a
   * death blow, send a skill_message if one exists; if not, default to a
   * dam_message. Otherwise, always send a dam_message.
   */
    if (!ch || (attacktype > TYPE_SUFFERING)) {
    	/* Don't say a THING!  It's being handled. */
	} else if (!IS_WEAPON(attacktype) || missile)
		skill_message(dam, ch, victim, attacktype);
	else {
		if (GET_POS(victim) == POS_DEAD || dam == 0) {
			if (times > 1)	multi_dam_message(dam, ch, victim, attacktype, times);
			else if (!skill_message(dam, ch, victim, attacktype))
				dam_message(dam, ch, victim, attacktype);
		} else
			if (times > 1)	multi_dam_message(dam, ch, victim, attacktype, times);
			else			dam_message(dam, ch, victim, attacktype);
	}
	
//	if (ch && (attacktype >= TYPE_SHOOT) && (attacktype < TYPE_SUFFERING)) {
//		weapon = GET_EQ(ch, WEAR_HAND_R);
//		if (!weapon || !IS_GUN(weapon))
//			weapon = GET_EQ(ch, WEAR_HAND_L);
		
//		if (weapon && IS_GUN(weapon)) {
//			for (count = 0; count < times; count++) {
//				if (GET_GUN_AMMO(weapon) > 0)
//					DEDUCT_AMMO(ch, weapon);
//			}
//		}
//	}
  /* Use send_to_char -- act() doesn't send message if you are DEAD. */
	switch (GET_POS(victim)) {
		case POS_MORTALLYW:
			act("$n is mortally wounded, and will die soon, if not aided.", TRUE, victim, 0, 0, TO_ROOM);
			send_to_char("You are mortally wounded, and will die soon, if not aided.\r\n", victim);
			break;
		case POS_INCAP:
			act("$n is incapacitated and will slowly die, if not aided.", TRUE, victim, 0, 0, TO_ROOM);
			send_to_char("You are incapacitated an will slowly die, if not aided.\r\n", victim);
			break;
		case POS_STUNNED:
			act("$n is stunned, but will probably regain consciousness again.", TRUE, victim, 0, 0, TO_ROOM);
			send_to_char("You're stunned, but will probably regain consciousness again.\r\n", victim);
			break;
		case POS_DEAD:
			act("$n is dead!  R.I.P.", FALSE, victim, 0, 0, TO_ROOM);
			send_to_char("You are dead!  Sorry...\r\n", victim);
			break;

		default:			/* >= POSITION SLEEPING */
			if (dam > (GET_MAX_HIT(victim) / 4))
				act("That really did HURT!", FALSE, victim, 0, 0, TO_CHAR);

			if (GET_HIT(victim) < (GET_MAX_HIT(victim) / 4)) {
				send_to_char("`RYou wish that your wounds would stop BLEEDING so much!`n\r\n", victim);
				if (ch && MOB_FLAGGED(victim, MOB_WIMPY) &&
						!MOB_FLAGGED(victim, MOB_SENTINEL) && (ch != victim))
					do_flee(victim, "", 0, "flee", 0);
			}
			if (ch && !IS_NPC(victim) && GET_WIMP_LEV(victim) && (victim != ch) &&
					GET_HIT(victim) < GET_WIMP_LEV(victim)) {
				send_to_char("You wimp out, and attempt to flee!\r\n", victim);
				do_flee(victim, "", 0, "flee", 0);
			}
			break;
	}
	
	if (((dam/times) >= 150) && (MOB_FLAGGED(victim, MOB_ACIDBLOOD) || (!IS_NPC(victim) && IS_ALIEN(victim))))
		acidblood(victim, dam);

//	if (ch && !IS_NPC(victim) && !(victim->desc)) {
//		do_flee(victim, "", 0, "flee", 0);
//	}

	/* stop someone from fighting if they're stunned or worse */
	if ((GET_POS(victim) <= POS_STUNNED) && (FIGHTING(victim) != NULL))
	victim->stop_fighting();

	/* Uh oh.  Victim died. */
	if (GET_POS(victim) == POS_DEAD) {
		if (ch && ch != victim) {
			if (AFF_FLAGGED(ch, AFF_GROUP))
				group_gain(ch, victim);
			else if (!IS_NPC(ch) && !IS_TRAITOR(ch) && (ch->GetRelation(victim) == RELATION_ENEMY))
				GET_MISSION_POINTS(ch) += calc_reward(ch, victim);
			if (MOB_FLAGGED(ch, MOB_MEMORY))
				forget(ch, victim);
		}
		room = IN_ROOM(victim);
		victim->die(ch);
		return 1;
	}
	return 0;
}


#define RANGE(input, low, high) (MAX((low), MIN((high), (input))))


const int charDam[NUM_RACES+1][2] = {
{20,50},
{20,80},
{40,80},
{25,100},
{20,50}
};

const int charDodge[NUM_RACES+1] = {
40,
50,
60,
30,
40
};

				/* A  D */
const int hitchart[2][2] = {
/*	Defender					*/
/*	No		Yes		Attacker	*/
{	50	,	10	},/*No			*/
{	90	,	50	} /*Yes			*/
};

// NEW: return 1 if you killed them or if the call should not be repeated.
int hit(CharData * ch, CharData * victim, int type, int times) {
	ObjData *wielded = GET_EQ(ch, WEAR_HAND_R);
	int w_type, calc_thaco, dam, diceroll;
	int hitskill, dodgeskill;
	SInt32	hits = 0,
			misses = 0,
			result = 0,
			total_dam = 0,
			hit_counter;
	bool	critical = FALSE;

	if (IN_ROOM(ch) != IN_ROOM(victim)) {
		if (FIGHTING(ch) && FIGHTING(ch) == victim)
			ch->stop_fighting();
		return 1;
	}

	if (!(wielded = GET_EQ(ch, WEAR_HAND_R)) || (GET_OBJ_TYPE(wielded) != ITEM_WEAPON))
		wielded = GET_EQ(ch, WEAR_HAND_L);

	if (type == TYPE_UNDEFINED) {
		if (wielded && GET_OBJ_TYPE(wielded) == ITEM_WEAPON) {
			if (IS_GUN(wielded) && (GET_GUN_AMMO(wielded) > 0))
				w_type = GET_GUN_ATTACK_TYPE(wielded) + TYPE_SHOOT;
			else
				w_type = GET_OBJ_VAL(wielded, 3) + TYPE_HIT;
		} else {
			if (IS_NPC(ch) && (ch->mob->attack_type != 0))
				w_type = ch->mob->attack_type + TYPE_HIT;
			else
				w_type = (IS_ALIEN(ch) ? TYPE_CLAW : TYPE_PUNCH);
		}
	} else
		w_type = type;
	
	for (hit_counter = 0; hit_counter < times; hit_counter++) {
		if ((w_type >= TYPE_SHOOT) && (hit_counter >= GET_GUN_AMMO(wielded)))
			break;
		/* Calculate to-hit */
		hitskill = IS_NPC(ch) ? thaco[(int)GET_RACE(ch)][w_type>=TYPE_SHOOT] :
				GET_SKILL(ch, ((w_type < TYPE_SHOOT) ? SKILL_PUNCH : SKILL_SHOOT));

		dodgeskill = IS_NPC(victim) ?
				(GET_MOB_DODGE(victim) ?
						GET_MOB_DODGE(victim) :
						charDodge[(int)GET_RACE(victim)] + GET_LEVEL(victim)) :
				GET_SKILL(victim, SKILL_DODGE);

		calc_thaco = hitchart[Number(0, 101) < hitskill][Number(0, 101) < dodgeskill];
		calc_thaco += (w_type < TYPE_SHOOT) ? GET_STR(ch) / 4 : GET_AGI(ch) / 4;
		calc_thaco += GET_HITROLL(ch) / 10;

		if (AWAKE(victim))
			calc_thaco -= GET_AGI(victim) / 4;
/*		calc_thaco += (int) ((GET_INT(ch) - 13) / 1.5);	*//* Intelligence helps! */
/*		calc_thaco += (int) ((GET_PER(ch) - 13) / 1.5);	*//* So does wisdom */
		calc_thaco = MAX(5, MIN(95, calc_thaco));
  
		diceroll = Number(1, 100);


		/* decide whether this is a hit or a miss */
		if ((diceroll == 1) || (calc_thaco < diceroll)) {
			misses++;
		} else {
			/* okay, we know the guy has been hit.  now calculate damage. */
			dam = (SInt32)(w_type < TYPE_SHOOT) ? (IS_ALIEN(ch) ?
                                        (GET_STR(ch) * 3) :
                                        (IS_PREDATOR(ch) ? (GET_STR(ch) * 5 / 2) :
						(GET_STR(ch) * 2))) :
					(IS_SYNTHETIC(ch) ?
                                        (GET_AGI(ch) * 3 / 2) :
                                        (GET_AGI(ch) * 2));
			//	dam = dam * GET_LEVEL(ch) / 25;
/*(SInt32)((w_type < TYPE_SHOOT) ? (IS_ALIEN(ch) ?
					(((float)GET_STR(ch) / 2) * (GET_LEVEL(ch) / 5)) :	//	Alien H2H
					(((float)GET_STR(ch) / 5) * (GET_LEVEL(ch) / 8))) :	//	Other H2H
					(IS_MARINE(ch) ?
					(((float)GET_AGI(ch) / 4) * (GET_LEVEL(ch) / 6)) :	//	Ranged Marine
					(((float)GET_AGI(ch) / 5) * (GET_LEVEL(ch) / 8))));	//	Ranged Other
			*/dam += GET_DAMROLL(ch);
			
			if (wielded) {
				if (IS_GUN(wielded) && GET_GUN_AMMO(wielded) > 0)
					dam += GET_GUN_DICE(wielded);
				else
					dam += dice(GET_OBJ_VAL(wielded, 1), GET_OBJ_VAL(wielded, 2));
			} else if (IS_NPC(ch))
				dam += dice(ch->mob->damage.number, ch->mob->damage.size);
			else
				dam += Number(charDam[(int)GET_RACE(ch)][0],charDam[(int)GET_RACE(ch)][1]);
			
			if (GET_POS(victim) < POS_FIGHTING)
				dam *= 1 + ((POS_FIGHTING - GET_POS(victim)) / 3);
			/* Position  sitting  x 1.33 */
			/* Position  resting  x 1.66 */
			/* Position  sleeping x 2.00 */
			/* Position  stunned  x 2.33 */
			/* Position  incap    x 2.66 */
			/* Position  mortally x 3.00 */

			dam = MAX(Number(1, 5), dam);		/* at least 1-5 hp damage min per hit */
			
			if (GET_SKILL(ch, SKILL_CRITICAL) && (GET_SKILL(ch, SKILL_CRITICAL) > (Number(1,100) + GET_AGI(victim) - GET_AGI(ch)))) {
				dam *= (GET_SKILL(ch, SKILL_CRITICAL) / 25) + 1;
				if (!critical) {
					send_to_char("`B*CRITICAL STRIKE*`n ", ch);
					send_to_char("`R*CRITICAL STRIKE*`n ", victim);
					critical = TRUE;
				}
			}
			
/*			check_killer(ch, victim);*/
			
			total_dam += dam;
			hits++;
		}
	}
	
#define BACKSTAB(ch)	(GET_AGI(ch) / 20)

	if (type == SKILL_BACKSTAB) {
		total_dam *= BACKSTAB(ch);
		result = damage(ch, victim, total_dam, SKILL_BACKSTAB, 1);
	} else {
		if (hits)			result = damage(ch, victim, total_dam, w_type, hits);
		else if (misses)	result = damage(ch, victim, 0, w_type, misses);
	}
	
	if (w_type >= TYPE_SHOOT) {
		GET_GUN_AMMO(wielded) -= hit_counter;
		if (GET_GUN_AMMO(wielded) <= 0)
			send_to_char("*CLICK* You're weapon is out of ammo\r\n", ch);
	}

	return result;
}



/* control the fights going on.  Called every 2 seconds from comm.c. */
void perform_violence(void)
{
	CharData *ch;
	
	START_ITER(iter, ch, CharData *, combat_list) {
		if (FIGHTING(ch) == NULL || (IN_ROOM(ch) != IN_ROOM(FIGHTING(ch)))) {
			ch->stop_fighting();
			continue;
		}
		
		if (IS_NPC(ch)) {
			if (GET_MOB_WAIT(ch) > 0) {
				GET_MOB_WAIT(ch) -= PULSE_VIOLENCE;
				continue;
			}
			GET_MOB_WAIT(ch) = 0;
		} else if (CHECK_WAIT(ch))
				continue;
		
		mprog_hitprcnt_trigger(ch, FIGHTING(ch));
		mprog_fight_trigger(ch, FIGHTING(ch));

	    ch->update_pos();
	    
		if ((GET_POS(ch) > POS_STUNNED) && !event_pending(ch, combat_event, EVENT_CAUSER)) {
			add_event(combat_delay(ch), combat_event, ch, FIGHTING(ch), 0);
		}

		if (MOB_FLAGGED(ch, MOB_SPEC) && mob_index[GET_MOB_RNUM(ch)].func != NULL)
			(mob_index[GET_MOB_RNUM(ch)].func) (ch, ch, 0, "");
	} END_ITER(iter);
}



int skill_roll(CharData *ch, int skill_num) 
{
  if (Number(0, 101) > GET_SKILL(ch, skill_num))
    return FALSE;
  else
    return TRUE;
}


int strike_missile(CharData *ch, CharData *tch, 
                   ObjData *missile, int dir, int attacktype) {
	int dam = 0;

	if (GET_OBJ_TYPE(missile) != ITEM_WEAPON || !IS_GUN(missile))
		dam = GET_STR(ch) / 2;
	else if (IS_GUN(missile))
		dam = GET_GUN_DICE(missile);
	dam += GET_DAMROLL(ch);

	send_to_char("You hit!\r\n", ch);
	act("Something flies in from $t and strikes $n.", FALSE, tch, (ObjData *)dir_text_2[rev_dir[dir]], 0, TO_ROOM);
	act("`RSomething flies in from $t and hits YOU!`n", FALSE, tch, (ObjData *)dir_text_2[rev_dir[dir]], 0, TO_CHAR);
	return damage(ch, tch, dam, attacktype, 1);
}



                
void miss_missile(CharData *ch, CharData *tch, ObjData *missile,
		int dir, int attacktype) {
  act("Something flies in from $t and hits the ground!", FALSE, tch, (ObjData *)dir_text_2[rev_dir[dir]], 0, TO_ROOM);
  act("Something flies in from $t and hits the ground!", FALSE, tch, (ObjData *)dir_text_2[rev_dir[dir]], 0, TO_CHAR);
  send_to_char("You missed!\r\n", ch);
}


                
void mob_reaction(CharData *ch, CharData *vict, int dir) {
	if (!IS_NPC(vict) || FIGHTING(vict) || GET_POS(vict) <= POS_STUNNED)
		return;

	/* can remember so charge! */
	if (MOB_FLAGGED(vict, MOB_MEMORY))
		remember(vict, ch);

//	act("$n bellows in pain!", FALSE, vict, 0, 0, TO_ROOM); 
//	if (GET_POS(vict) == POS_STANDING) {
//			if (!perform_move(vict, rev_dir[dir], 1))
//				act("$n stumbles while trying to run!", FALSE, vict, 0, 0, TO_ROOM);
//		} else
//			GET_POS(vict) = POS_STANDING;
      
		/* can't remember so try to run away */
//	}
	if (MOB_FLAGGED(vict, MOB_WIMPY))
		do_flee(vict, "", 0, "flee", 0);
	else {
		if (GET_POS(vict) != POS_STANDING)
			GET_POS(vict) = POS_STANDING;
		vict->path = Path2Char(IN_ROOM(vict), ch, 10, HUNT_GLOBAL);
	}
}



int fire_missile(CharData *ch, char *arg1, ObjData *missile, SInt32 pos, SInt32 range, SInt32 dir, SInt32 skill) {
	bool found = false;
	int shot = FALSE;
	int attacktype, x;
	int room, nextroom, distance;
	CharData *vict;
    
	if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL) && !NO_STAFF_HASSLE(ch)) {
		send_to_char("This room just has such a peaceful, easy feeling...\r\n", ch);
		return 1;
	}

	room = IN_ROOM(ch);

	if (CAN_GO2(room, dir) && !EXIT_FLAGGED(EXITN(room, dir), EX_NOSHOOT))
		nextroom = EXIT2(room, dir)->to_room;
	else
		nextroom = NOWHERE;
 
	if (GET_OBJ_TYPE(missile) == ITEM_GRENADE) {
		act("You throw $p $T!\r\n", FALSE, ch, missile, dirs[dir], TO_CHAR);
		act("$n throws $p $T.", FALSE, ch, missile, dirs[dir], TO_ROOM);
		for (distance = 0; distance < range; distance++) {
			if (CAN_GO2(room, dir) && !EXIT_FLAGGED(EXITN(room, dir), EX_NOSHOOT)) {
				if ((skill / (distance + 1)) < Number(1, 101)) {
					if (distance < 1) {
						act("The $p slips out of your hand and falls to the floor!", FALSE, ch, missile, 0, TO_CHAR);
						act("$n accidentally drops $p at his feet!", FALSE, ch, missile, 0, TO_ROOM);
					}
					break;
				} else if (distance && world[room].people.Count())
					act("$p flies by from the $T!", FALSE, world[room].people.Top(), missile, from_dir[rev_dir[dir]], TO_CHAR | TO_ROOM);
				room = EXIT2(room, dir)->to_room;
			} else
				break;
		}
		unequip_char(ch, pos)->to_room(room);
		if (distance > 0)
			act("$p flies in from the $T.", FALSE, 0, missile, from_dir[rev_dir[dir]], TO_ROOM);
		return 1;
	}

	for (distance = 1; ((nextroom != NOWHERE) && (distance <= range)); distance++) {
		START_ITER(iter, vict, CharData *, world[nextroom].people) {
			if (isname(arg1, SSData(vict->general.name)) && ch->CanSee(vict)) {
				found = true;
				break;
			}
		} END_ITER(iter);

		if (found) {
			/* Daniel Houghton's missile modification */
			if (missile && ROOM_FLAGGED(IN_ROOM(vict), ROOM_PEACEFUL) && !NO_STAFF_HASSLE(ch)) {
				send_to_char("Nah.  Leave them in peace.\r\n", ch);
				return 1;
			}
			
			check_killer(ch, vict);
			
			switch(GET_OBJ_TYPE(missile)) {
				case ITEM_BOOMERANG:
				case ITEM_THROW:
					act("You throw $o at $N!", TRUE, ch, missile, vict, TO_CHAR);
					act("$n throws $o $T", FALSE, ch, missile, dirs[dir], TO_ROOM);
					attacktype = SKILL_THROW;
					break;
/*
				case ITEM_ARROW:
					act("$n aims and fires!", TRUE, ch, 0, 0, TO_ROOM);
					send_to_char("You aim and fire!\r\n", ch);
					attacktype = SKILL_BOW;
					break;
				case ITEM_ROCK:
					act("$n aims and fires!", TRUE, ch, 0, 0, TO_ROOM);
					send_to_char("You aim and fire!\r\n", ch);
					attacktype = SKILL_SLING;
					break;
				case ITEM_BOLT:
					act("$n aims and fires!", TRUE, ch, 0, 0, TO_ROOM);
					send_to_char("You aim and fire!\r\n", ch);
					attacktype = SKILL_CROSSBOW;
					break;
*/
				case ITEM_WEAPON:
					act("$n aims and fires to the $T!", TRUE, ch, 0, dirs[dir], TO_ROOM);
					act("You aim and fire at $N!", TRUE, ch, 0, vict, TO_CHAR);
					attacktype = SKILL_SHOOT;
					break;
				default:
					attacktype = TYPE_UNDEFINED;
					break;
			}


			for (x = 0; (IS_GUN(missile) ? ((x < GET_GUN_RATE(missile)) && (GET_GUN_AMMO(missile) > 0)) : x < 1) &&
					(IN_ROOM(vict) == nextroom); x++) {
				if ((attacktype != TYPE_UNDEFINED) ? skill_roll(ch, attacktype) : FALSE) {		/* HIT */
					if (strike_missile(ch, vict, missile, dir, attacktype)) {
						if (IS_GUN(missile))
							DEDUCT_AMMO(ch, missile);
						break;
					}
				} else { 				/* MISS */
					if (IS_GUN(missile)) {
						if (GET_OBJ_TYPE(missile) == ITEM_BOOMERANG) {
							act("Something zooms past you from the $T!", TRUE, vict, 0, dirs[rev_dir[dir]], TO_CHAR);
							act("Something zooms past $n from the $T!", TRUE, vict, 0, dirs[rev_dir[dir]], TO_ROOM);
							act("You missed!\r\n", TRUE, ch, 0, 0, TO_CHAR);
						} else
							miss_missile(ch, vict, missile, dir, attacktype);
					}
				}
				if (IS_GUN(missile))
					DEDUCT_AMMO(ch, missile);
			}
			
			if ((GET_OBJ_TYPE(missile) == ITEM_ARROW) || (GET_OBJ_TYPE(missile) == ITEM_THROW))
				unequip_char(ch, pos)->to_room(IN_ROOM(vict));
			/* either way mob remembers */
			mob_reaction(ch, vict, dir);
			WAIT_STATE(ch, GET_OBJ_SPEED(missile) ? GET_OBJ_SPEED(missile) : 2);
			return 1;
		}

		room = nextroom;
		if (CAN_GO2(room, dir) && !EXIT_FLAGGED(EXITN(room, dir), EX_NOSHOOT))
			nextroom = EXIT2(room, dir)->to_room;
		else
			nextroom = NOWHERE;
	}	//	FOR search for target
	return 0;
}


void explosion(CharData *ch, int dice_num, int dice_size, int room, int shrapnel) {
	CharData *tch;
	int door, dam = dice_num * dice_size, exit_room;
	extern char *dir_text[NUM_OF_DIRS];
	LListIterator<CharData *>	iter;

	if (room == NOWHERE)
		return;
	
	iter.Start(world[room].people);
	while ((tch = iter.Next())) {
		/* You can't damage an immortal! */
		if (NO_STAFF_HASSLE(tch))
			continue;
			
/*			act("$n is blasted!", TRUE, tch, 0, 0, TO_ROOM);*/
		act("You are caught in the blast!", TRUE, tch, 0, 0, TO_CHAR);
		damage(ch, tch, dice(dice_num, dice_size), TYPE_EXPLOSION, 1);
	}
	
	for (door = 0; door < NUM_OF_DIRS; door++) {
		if (!CAN_GO2(room, door) || ((exit_room = EXITN(room,door)->to_room) == room))
			continue;

		world[exit_room].Send("You hear a loud explosion come from %s!\r\n", dir_text[rev_dir[door]]);
		
		/* Extended explosion sound */
		if ((dam >= 250) && CAN_GO2(exit_room, door) && (EXITN(exit_room, door)->to_room != room))
			world[EXITN(exit_room, door)->to_room].Send("You hear a loud explosion come from %s!\r\n", dir_text[rev_dir[door]]);
		/* End Extended sound */
			
		if (!shrapnel || ROOM_FLAGGED(exit_room, ROOM_PEACEFUL) ||
				(dam < 250) || (dam < 500 && Number(0,1)))
			continue;
			
		world[exit_room].Send("Shrapnel flies into the room from %s!\r\n", dir_text[rev_dir[door]]);
		
		iter.Restart(world[exit_room].people);
		while ((tch = iter.Next())) {
			/* You can't damage an immortal! */
			if (NO_STAFF_HASSLE(tch) || ((dam < 750) && Number(0,1)))
				continue;
				
			act("You are hit by shrapnel!", TRUE, tch, 0, 0, TO_CHAR);
			damage(ch, tch, dice(dice_num / 2, dice_size / 2), TYPE_EXPLOSION, 1);
		}
	}
}

void acidblood(CharData *ch, int dam) {
	EVENT(acid_burn);
	CharData *tch;
//	struct event_info *temp_event;
	int room = IN_ROOM(ch), acid_dam /*, message_sent = 0*/;
	
	if (room == NOWHERE)
		return;

		act("Acid blood sprays from $n's wound!", FALSE, ch, 0, 0, TO_ROOM);
		act("Acid blood sprays from your wound!", FALSE, ch, 0, 0, TO_CHAR);
	
	START_ITER(iter, tch, CharData *, world[room].people) {
		/* You can't damage an immortal, alien, or acidblood character, or yourself. */
		/* Plus if the damage is less than 200, there is a 2 in 3 chance it will be ignored. */
		if (NO_STAFF_HASSLE(tch) || tch == ch || IS_ALIEN(tch) || MOB_FLAGGED(tch, MOB_ACIDBLOOD))
			continue;
		
		if ((dam < 200)) {		// if damage is less than 200
			if (Number(0,2))	// then only a 1 in 3 chance of splashing onto char
				continue;
		} else if ((dam < 300) && Number(0,1))	// otherwise, if damage is less than 300
			continue;			// then 1 in 2 chance of splashing

		act("You are sprayed with $n's acid blood!", FALSE, ch, 0, tch, TO_VICT);
		/* Acid Blood handler goes here. */
		acid_dam = dice(8, dam / 16);
		//if ((temp_event = get_event(tch, acid_burn, EVENT_CAUSER))) {
		//	temp_event->info = (Ptr)((int)temp_event->info + acid_dam);
		//} else {
		if (acid_dam > (GET_HIT(ch)) + 99)
			acid_dam = (GET_HIT(ch) + 99);
			
		damage(NULL, tch, acid_dam, TYPE_ACIDBLOOD, 1);
//		update_pos(tch);
		//	add_event(ACID_BURN_TIME, acid_burn, tch, 0, (Ptr)acid_dam);
		//}
	} END_ITER(iter);
}


void acidspray(int room, int dam) {
	EVENT(acid_burn);
	CharData *tch;
//	struct event_info *temp_event;
	int acid_dam;
	
	if (room == NOWHERE)
		return;
		
	START_ITER(iter, tch, CharData *, world[room].people) {
		/* You can't damage an immortal, alien, or acidblood character, or yourself. */
		/* Plus if the damage is less than 200, there is a 2 in 3 chance it will be ignored. */
		if (NO_STAFF_HASSLE(tch) || IS_ALIEN(tch) || MOB_FLAGGED(tch, MOB_ACIDBLOOD) ||
				(dam < 200 && Number(0,2)))
			continue;
		
		if ((dam > 300) || Number(0,1)) {
/*			if (!message_sent) {
				act("Acid blood sprays from $n's wound!", FALSE, ch, 0, 0, TO_ROOM);
				act("Acid blood sprays from your wound!", FALSE, ch, 0, 0, TO_CHAR);
				message_sent = 1;
			}*/
			act("$n is sprayed with acid blood!", FALSE, tch, 0, 0, TO_ROOM);
			act("You are sprayed with acid blood!", FALSE, tch, 0, 0, TO_CHAR);
			/* Acid Blood handler goes here. */
			/* acid damage 8D(dam>>3) */
			acid_dam = dice(8, dam / 8);
			//if ((temp_event = get_event(tch, acid_burn, EVENT_CAUSER))) {
			//	temp_event->info = (Ptr)((int)temp_event->info + acid_dam);
			//} else {
				damage(NULL, tch, acid_dam, TYPE_ACIDBLOOD, 1);
			//	add_event(ACID_BURN_TIME, acid_burn, tch, 0, (Ptr)acid_dam);
			//}
		}
	} END_ITER(iter);
}

