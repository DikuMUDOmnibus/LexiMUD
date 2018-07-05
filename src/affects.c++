/***************************************************************************\
[*]    __     __  ___                ___  [*]   LexiMUD: A feature rich   [*]
[*]   / /  ___\ \/ (_) /\/\  /\ /\  /   \ [*]      C++ MUD codebase       [*]
[*]  / /  / _ \\  /| |/    \/ / \ \/ /\ / [*] All rights reserved         [*]
[*] / /__|  __//  \| / /\/\ \ \_/ / /_//  [*] Copyright(C) 1997-1998      [*]
[*] \____/\___/_/\_\_\/    \/\___/___,'   [*] Chris Jacobson (FearItself) [*]
[*]        LexiMUD: Feel the Power        [*] fear@technologist.com       [*]
[-]---------------------------------------+-+-----------------------------[-]
[*] File : affects.c++                                                    [*]
[*] Usage: Primary code for affections                                    [*]
\***************************************************************************/


#include "characters.h"
#include "objects.h"
#include "utils.h"
#include "comm.h"
#include "affects.h"
#include "event.h"


void CheckRegenRates(CharData *ch);


char *affect_wear_off_msg[] = {
	"RESERVED DB.C",		/* 0 */
	"The whiteness in your vision slowly fades and you can see clearly.",
	"You begin to think for yourself.",
	"You slowly come to.",
	"You don't feel as sick.",
	"",
	""
};


char *affects[] = {
	"UNDEFINED",
	"BLIND",
	"CHARM",
	"SLEEP",
	"POISON",
	"SNEAK",
	"\n"
};


//	Internal to this file
struct AffectEvent {
	Affect *	affect;
	CharData *	ch;
};


EVENTFUNC(affect_event);


Affect::Affect(AffectType type, SInt32 modifier, AffectLoc location, Flags bitvector) {
	this->type = ((type >= Affect::None) && (type <= NUM_AFFECTS)) ? type : Affect::None;
	this->modifier = modifier;
	this->location = location;
	this->bitvector = bitvector;

	event = NULL;
}


//	Copy affect and attach to char - a "type" of copy-constructor
Affect::Affect(Affect *aff, CharData *ch) {
	struct AffectEvent *ae_data;
	
	this->type = aff->type;
	this->modifier = aff->modifier;
	this->location = aff->location;
	this->bitvector = aff->bitvector;
	if (aff->event) {
		CREATE(ae_data, struct AffectEvent, 1);
		ae_data->affect = this;
		ae_data->ch = ch;
		this->event = new Event(affect_event, ae_data, aff->event->Time());
	} else
		this->event = NULL;
}


Affect::~Affect(void) {
	if (this->event)
		this->event->Cancel();
}


//	Insert an Affect in a CharData structure
//	Automatically sets apropriate bits and apply's
void Affect::ToChar(CharData * ch, UInt32 duration) {
	struct AffectEvent *ae_data;

	ch->affected.Add(this);
	
	if (duration > 0) {
		CREATE(ae_data, struct AffectEvent, 1);
		ae_data->affect = this;
		ae_data->ch = ch;
		this->event = new Event(affect_event, ae_data, duration);
	} else
		this->event = NULL;

	Modify(ch, this->location, this->modifier, this->bitvector, TRUE);
	ch->AffectTotal();
}


//	Remove an Affect from a char (called when duration reaches zero).
//	Frees mem and calls affect_modify
void Affect::Remove(CharData *ch) {
	Modify(ch, this->location, this->modifier, this->bitvector, FALSE);
	ch->affected.Remove(this);
	ch->AffectTotal();
	delete this;
}


void Affect::Join(CharData * ch, UInt32 duration, bool add_dur, bool avg_dur, bool add_mod, bool avg_mod) {
	Affect *hjp;
	
	LListIterator<Affect *>		iter(ch->affected);
	while ((hjp = iter.Next())) {
		if ((hjp->type == this->type) && (hjp->location == this->location)) {
			if (add_dur && hjp->event)		duration += hjp->event->Time();
			if (avg_dur)					duration /= 2;
			if (add_mod)					this->modifier += hjp->modifier;
			if (avg_mod)					this->modifier /= 2;

			hjp->Remove(ch);
			this->ToChar(ch, duration);
			return;
		}
	}
	this->ToChar(ch, duration);
}


EVENTFUNC(affect_event) {
	struct AffectEvent *ae_data = (struct AffectEvent *)event_obj;
	
	Affect *other;
	Affect *af =	ae_data->affect;
	CharData *ch =	ae_data->ch;
	SInt32	type =	af->type;
	
	af->event = NULL;
	af->Remove(ch);
	
	if ((type > 0) && (type <= NUM_AFFECTS)) {
		LListIterator<Affect *>		iter(ch->affected);
		while ((other = iter.Next()))
			if ((other->type == type) && other->event)
				return 0;
		if (*affect_wear_off_msg[type] && (IN_ROOM(ch) != NOWHERE))
			act(affect_wear_off_msg[type], FALSE, ch, NULL, NULL, TO_CHAR);
	}
//		if (!af->next || (af->next->type != af->type) || !af->next->event)	// Compound Affects

	return 0;
}


void Affect::Modify(CharData * ch, SInt8 loc, SInt8 mod, Flags bitv, bool add) {
	if (add) {
		SET_BIT(AFF_FLAGS(ch), bitv);
	} else {
		REMOVE_BIT(AFF_FLAGS(ch), bitv);
		mod = -mod;
	}

#define RANGE(var, low, high)	MAX((low), MIN((high), (var)))

	switch (loc) {
		case APPLY_NONE:														break;
		case APPLY_STR:		GET_STR(ch) = RANGE(GET_STR(ch) + mod, 0, 200);		break;
		case APPLY_DEX:		GET_AGI(ch) = RANGE(GET_AGI(ch) + mod, 0, 200);		break;
		case APPLY_INT:		GET_INT(ch) = RANGE(GET_INT(ch) + mod, 0, 200);		break;
		case APPLY_PER:		GET_PER(ch) = RANGE(GET_PER(ch) + mod, 0, 200);		break;
		case APPLY_CON:		GET_CON(ch) = RANGE(GET_CON(ch) + mod, 0, 200);		break;
		case APPLY_AGE:		ch->player->time.birth -= (mod * SECS_PER_MUD_YEAR);	break;
		case APPLY_WEIGHT:	GET_WEIGHT(ch) = MAX(1, GET_WEIGHT(ch) + mod);		break;
		case APPLY_HEIGHT:	GET_HEIGHT(ch) = MAX(1, GET_HEIGHT(ch) + mod);		break;
		case APPLY_HIT:
			GET_MAX_HIT(ch) += mod;
			GET_HIT(ch) = MIN(GET_HIT(ch), GET_MAX_HIT(ch));
			break;
		case APPLY_MOVE:
			GET_MAX_MOVE(ch) += mod;
			GET_MOVE(ch) = MIN(GET_MOVE(ch), GET_MAX_MOVE(ch));
			break;
		case APPLY_AC:		GET_AC(ch) = RANGE(GET_AC(ch) + mod, 0, 32000);		break;
		case APPLY_HITROLL:	GET_HITROLL(ch) += mod;		break;
		case APPLY_DAMROLL:	GET_DAMROLL(ch) += mod;		break;
		default:
//			log("SYSERR: affect_modify: Unknown apply adjust %d attempt", loc);
			break;
	}
}





/* Call affect_remove with every spell of spelltype "skill" */
void Affect::FromChar(CharData * ch, AffectType type) {
	Affect *hjp;
	
	LListIterator<Affect *>		iter(ch->affected);
	while ((hjp = iter.Next()))
		if (hjp->type == type)
			hjp->Remove(ch);
}



/*
 * Return if a char is affected by a spell (SPELL_XXX), NULL indicates
 * not affected
 */
bool Affect::AffectedBy(CharData * ch, AffectType type) {
	Affect *hjp;

	LListIterator<Affect *>		iter(ch->affected);
	while ((hjp = iter.Next()))
		if (hjp->type == type)
			return true;

	return false;
}


/*
#define MAX_CHAR_AFFECTS	10

void affects(int level, CharData * ch, CharData * victim, int affectnum) {
	Affect af[MAX_AFFECTS];
	int accum_affect = FALSE, accum_duration = FALSE;
	char *to_vict = NULL, *to_room = NULL;
	int i;


	if (!victim || !ch)
		return;

  for (i = 0; i < MAX_AFFECTS; i++) {
    af[i].type = affectnum;
    af[i].bitvector = 0;
    af[i].modifier = 0;
    af[i].location = APPLY_NONE;
  }

  switch (affectnum) {
  case AFFECT_BLIND:
    if (IS_NPC(victim) && MOB_FLAGGED(victim,MOB_NOBLIND))
      return;

    af[0].location = APPLY_HITROLL;
    af[0].modifier = -4;
    af[0].duration = 2;
    af[0].bitvector = AFF_BLIND;

    af[1].location = APPLY_AC;
    af[1].modifier = 40;
    af[1].duration = 2;
    af[1].bitvector = AFF_BLIND;

    to_room = "$n seems to be blinded!";
    to_vict = "You have been blinded!";
    break;

#if 0
  case SPELL_CURSE:
    if (mag_savingthrow(victim, savetype)) {
      send_to_char(NOEFFECT, ch);
      return;
    }

    af[0].location = APPLY_HITROLL;
    af[0].duration = 1 + (GET_LEVEL(ch) >> 1);
    af[0].modifier = -1;
    af[0].bitvector = AFF_CURSE;

    af[1].location = APPLY_DAMROLL;
    af[1].duration = 1 + (GET_LEVEL(ch) >> 1);
    af[1].modifier = -1;
    af[1].bitvector = AFF_CURSE;

    accum_duration = TRUE;
    accum_affect = TRUE;
    to_room = "$n briefly glows red!";
    to_vict = "You feel very uncomfortable.";
    break;

  case SPELL_DETECT_ALIGN:
    af[0].duration = 12 + level;
    af[0].bitvector = AFF_DETECT_ALIGN;
    accum_duration = TRUE;
    to_vict = "Your eyes tingle.";
    break;

  case SPELL_DETECT_INVIS:
    af[0].duration = 12 + level;
    af[0].bitvector = AFF_DETECT_INVIS;
    accum_duration = TRUE;
    to_vict = "Your eyes tingle.";
    break;

  case SPELL_DETECT_MAGIC:
    af[0].duration = 12 + level;
    af[0].bitvector = AFF_DETECT_MAGIC;
    accum_duration = TRUE;
    to_vict = "Your eyes tingle.";
    break;

  case SPELL_INFRAVISION:
    af[0].duration = 12 + level;
    af[0].bitvector = AFF_INFRAVISION;
    accum_duration = TRUE;
    to_vict = "Your eyes glow red.";
    to_room = "$n's eyes glow red.";
    break;

  case SPELL_INVISIBLE:
    if (!victim)
      victim = ch;

    af[0].duration = 12 + (GET_LEVEL(ch) >> 2);
    af[0].modifier = -40;
    af[0].location = APPLY_AC;
    af[0].bitvector = AFF_INVISIBLE;
    accum_duration = TRUE;
    to_vict = "You vanish.";
    to_room = "$n slowly fades out of existence.";
    break;

  case SPELL_POISON:
    if (mag_savingthrow(victim, savetype)) {
      send_to_char(NOEFFECT, ch);
      return;
    }

    af[0].location = APPLY_STR;
    af[0].duration = GET_LEVEL(ch);
    af[0].modifier = -2;
    af[0].bitvector = AFF_POISON;
    to_vict = "You feel very sick.";
    to_room = "$n gets violently ill!";
    break;

  case SPELL_PROT_FROM_EVIL:
    af[0].duration = 24;
    af[0].bitvector = AFF_PROTECT_EVIL;
    accum_duration = TRUE;
    to_vict = "You feel invulnerable!";
    break;

  case SPELL_SANCTUARY:
    af[0].duration = 4;
    af[0].bitvector = AFF_SANCTUARY;

    accum_duration = TRUE;
    to_vict = "A white aura momentarily surrounds you.";
    to_room = "$n is surrounded by a white aura.";
    break;

  case SPELL_SLEEP:
    if (!pk_allowed && !IS_NPC(ch) && !IS_NPC(victim))
      return;
    if (MOB_FLAGGED(victim, MOB_NOSLEEP))
      return;
    if (mag_savingthrow(victim, savetype))
      return;

    af[0].duration = 4 + (GET_LEVEL(ch) >> 2);
    af[0].bitvector = AFF_SLEEP;

    if (GET_POS(victim) > POS_SLEEPING) {
      act("You feel very sleepy...  Zzzz......", FALSE, victim, 0, 0, TO_CHAR);
      act("$n goes to sleep.", TRUE, victim, 0, 0, TO_ROOM);
      GET_POS(victim) = POS_SLEEPING;
    }
    break;

  case SPELL_STRENGTH:
    af[0].location = APPLY_STR;
    af[0].duration = (GET_LEVEL(ch) >> 1) + 4;
    af[0].modifier = 1 + (level > 18);
    accum_duration = TRUE;
    accum_affect = TRUE;
    to_vict = "You feel stronger!";
    break;

  case SPELL_SENSE_LIFE:
    to_vict = "Your feel your awareness improve.";
    af[0].duration = GET_LEVEL(ch);
    af[0].bitvector = AFF_SENSE_LIFE;
    accum_duration = TRUE;
    break;

  case SPELL_WATERWALK:
    af[0].duration = 24;
    af[0].bitvector = AFF_WATERWALK;
    accum_duration = TRUE;
    to_vict = "You feel webbing between your toes.";
    break;
#endif
  }

   // If this is a mob that has this affect set in its mob file, do not
   // perform the affect.  This prevents people from un-sancting mobs
   // by sancting them and waiting for it to fade, for example.
   if (IS_NPC(victim) && AFF_FLAGGED(victim, af[0].bitvector|af[1].bitvector) &&
       !affected_by(victim, affectnum)) {
	send_to_char(NOEFFECT, ch);
	return;
   }

  // If the victim is already affected by this spell, and the spell does
   // not have an accumulative effect, then fail the spell.
  if (affected_by(victim,affectnum) && !(accum_duration||accum_affect)) {
    send_to_char(NOEFFECT, ch);
    return;
  }

  for (i = 0; i < MAX_AFFECTS; i++)
    if (af[i].bitvector || (af[i].location != APPLY_NONE))
      affect_join(victim, af+i, accum_duration, FALSE, accum_affect, FALSE);

  if (to_vict != NULL)
    act(to_vict, FALSE, victim, 0, ch, TO_CHAR);
  if (to_room != NULL)
    act(to_room, TRUE, victim, 0, ch, TO_ROOM);
}
*/