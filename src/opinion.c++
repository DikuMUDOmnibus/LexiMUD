/*************************************************************************
*   File: opinion.c++                Part of Aliens vs Predator: The MUD *
*  Usage: Code file for opinion system                                   *
*************************************************************************/

#include "opinion.h"
#include "characters.h"
#include "descriptors.h"
#include "rooms.h"
#include "index.h"
#include "utils.h"

void free_intlist(struct int_list *intlist);


Opinion::Opinion(void) {
	this->charlist = NULL;
	this->sex = 0;
	this->race = 0;
	this->vnum = -1;
	this->active = 0;
}


Opinion::Opinion(Opinion *old) {
	this->charlist = NULL;
	this->sex = old->sex;
	this->race = old->race;
	this->vnum = old->vnum;
	this->active = old->active;
}


Opinion::~Opinion(void) {
	free_intlist(this->charlist);
}


void Opinion::Clear(void) {
	free_intlist(this->charlist);
	this->charlist = NULL;
}


bool Opinion::RemChar(SInt32 id) {
	struct int_list *old, *temp;
	
	for (old = this->charlist; old; old = old->next) {
		if (old->i == id)
			break;
	}
	if (old) {
		REMOVE_FROM_LIST(old, this->charlist, next);
		free(old);
		if (!this->charlist)
			REMOVE_BIT(this->active, OP_CHAR);
		return true;
	}
	return false;
}


void Opinion::RemOther(UInt32 type) {
	switch (type) {
		case OP_CHAR:
			break;
		case OP_SEX:
			REMOVE_BIT(this->active, OP_SEX);
			this->sex = 0;
			break;
		case OP_RACE:
			REMOVE_BIT(this->active, OP_RACE);
			this->race = 0;
			break;
		case OP_VNUM:
			REMOVE_BIT(this->active, OP_VNUM);
			this->vnum = -1;
			break;
		default:
			mudlogf(NRM, LVL_STAFF, TRUE, "Bad type (%d) passed to Opinion::RemOther.", type);
	}
}


bool Opinion::AddChar(SInt32 id) {
	struct int_list *newchar;
	
	if (this->InList(id))
		return false;
	
	CREATE(newchar, struct int_list, 1);
	newchar->i = id;

	newchar->next = this->charlist;
	this->charlist = newchar;
	
	SET_BIT(this->active, OP_CHAR);
	
	return true;
}


void Opinion::AddOther(UInt32 type, UInt32 param) {
	switch (type) {
		case OP_CHAR:
			this->AddChar(param);
			break;
		case OP_SEX:
			SET_BIT(this->active, OP_SEX);
			this->sex = param;
			break;
		case OP_RACE:
			SET_BIT(this->active, OP_RACE);
			this->race = param;
			break;
		case OP_VNUM:
			SET_BIT(this->active, OP_VNUM);
			this->vnum = param;
			break;
		default:
			mudlogf(NRM, LVL_STAFF, TRUE, "Bad type (%d) passed to Opinion::AddOther.", type);
	}
}


bool Opinion::InList(SInt32 id) {
	struct int_list *i;
	for (i = this->charlist; i; i = i->next)
		if (i->i == id)	return true;
	return false;
}


bool Opinion::IsTrue(CharData *ch) {
	struct int_list *i;
	
	if (this->race && (ch->general.race != RACE_UNDEFINED)) {
		if ((1 << GET_RACE(ch)) & this->race)	return true;
	}
	if (this->sex) {
		if ((GET_SEX(ch) == SEX_MALE) && IS_SET(this->sex, OP_MALE))		return true;
		if ((GET_SEX(ch) == SEX_FEMALE) && IS_SET(this->sex, OP_FEMALE))	return true;
		if ((GET_SEX(ch) == SEX_NEUTRAL) && IS_SET(this->sex, OP_NEUTRAL))	return true;
	}
	if (this->vnum != -1) {
		if (IS_NPC(ch) && (GET_MOB_VNUM(ch) == this->vnum))	return true;
	}
	for (i = this->charlist; i; i = i->next) {
		if (i->i == GET_ID(ch))	return true;
	}
	return false;
}


CharData *Opinion::FindTarget(CharData *ch) {
	CharData *temp;
	
	if (IN_ROOM(ch) == NOWHERE)
		return NULL;
	
	START_ITER(iter, temp, CharData *, character_list) {	
		if ((ch != temp) && ch->CanSee(temp) && IsTrue(temp)) {
			END_ITER(iter);
			return temp;
		}
	} END_ITER(iter);
	return NULL;
}
