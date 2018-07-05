/*************************************************************************
*   File: objects.cp                 Part of Aliens vs Predator: The MUD *
*  Usage: Primary code for objects                                       *
*************************************************************************/



#include "types.h"
#include "objects.h"
#include "rooms.h"
#include "characters.h"
#include "utils.h"
#include "scripts.h"
#include "comm.h"
#include "buffer.h"
#include "files.h"
#include "event.h"
#include "affects.h"
#include "db.h"
#include "extradesc.h"
#include "handler.h"


// External function declarations
CharData *get_char_on_obj(ObjData *obj);
void tag_argument(char *argument, char *tag);
void strip_string(char *buffer);


// External variables
extern UInt32	max_id;


LList<ObjData *>	object_list;		//	global linked list of objs
LList<ObjData *>	purged_objs;		//	list of objs to be deleted
IndexData *			obj_index;			//	index table for object file
UInt32				top_of_objt = 0;	//	top of object index table


GunData::GunData(void) : attack(0), rate(0), range(0) {
	this->dice.number = 0;
	this->dice.size = 0;
	
	this->ammo.type = 0;
	this->ammo.vnum = -1;
	this->ammo.amount = 0;
}


GunData::~GunData(void) {
}


ObjData::ObjData(void) {
	this->Init();
}


ObjData::ObjData(ObjData *obj) {
	ExtraDesc *extra, **ned, *tmp;
	SInt32		i;
	ObjData		*newObj, *contentObj;
	
	this->Init();
	
	//	This used to copy the entire object, but was only really
	//	safe with protos.  Now we do it the right way, by hand.
	//	*this = *obj;
	
	//	Share SStrings
	this->name = SSShare(obj->name);
	this->description = SSShare(obj->description);
	this->shortDesc = SSShare(obj->shortDesc);
	this->actionDesc = SSShare(obj->actionDesc);

	//	Share other data
	GET_OBJ_RNUM(this) = GET_OBJ_RNUM(obj);
	
	for (i = 0; i < 8; i++)
		GET_OBJ_VAL(this, i) = GET_OBJ_VAL(obj, i);
	GET_OBJ_COST(this) = GET_OBJ_COST(obj);
	GET_OBJ_WEIGHT(this) = GET_OBJ_WEIGHT(obj);
	GET_OBJ_TIMER(this) = GET_OBJ_TIMER(obj);
	GET_OBJ_TYPE(this) = GET_OBJ_TYPE(obj);
	GET_OBJ_WEAR(this) = GET_OBJ_WEAR(obj);
	GET_OBJ_EXTRA(this) = GET_OBJ_EXTRA(obj);
	this->affects = obj->affects;
	GET_OBJ_SPEED(this) = GET_OBJ_SPEED(obj);
	
	for (i = 0; i < MAX_OBJ_AFFECT; i++)
		this->affect[i] = obj->affect[i];
	
	//	Share GUN data
	if (IS_GUN(obj)) {
		this->gun = new GunData();
		*this->gun = *obj->gun;
	}
	
	// Duplicate extra descriptions
	ned = &this->exDesc;
	for(extra = obj->exDesc; extra; extra = extra->next) {
		tmp = new ExtraDesc();
		tmp->keyword = SSShare(extra->keyword);
		tmp->description = SSShare(extra->description);
		*ned = tmp;
		ned = &tmp->next;
	}
	
	//	This doesn't happen on protos - only on copied real objects
	START_ITER(iter, contentObj, ObjData *, obj->contains) {
		newObj = new ObjData(contentObj);
//		newObj->to_obj(this);				This messes up weight...
		this->contains.Append(newObj);	//	Safer, and we can add to end of list
		newObj->in_obj = this;			//	Plus no need for weight correction
	} END_ITER(iter);
}


/* Delete the object */
ObjData::~ObjData(void) {
	ExtraDesc *extra, *nextExtra;

	for (extra = this->exDesc; extra; extra = nextExtra) {
		nextExtra = extra->next;
		delete extra;
	}
	SSFree(this->name);
	SSFree(this->description);
	SSFree(this->shortDesc);
	SSFree(this->actionDesc);

	if (this->gun);		delete (this->gun);

//	CLEAR SCRIPT
	if (SCRIPT(this))	delete (SCRIPT(this));
}



/* Clear the object */
void ObjData::Init(void) {
	memset(this, 0, sizeof(*this));
	
	GET_OBJ_RNUM(this) = NOTHING;
	GET_ID(this) = -1;
	IN_ROOM(this) = NOWHERE;

//	for (counter = 0; counter++; counter < MAX_OBJ_AFFECT) {
//		this->affect[counter].location = 0;
//		this->affect[counter].modifier = 0;
//	}
}


/* Extract an object from the world */
void ObjData::extract(void) {
	ObjData *	temp;
	CharData *	ch;
	
	if (PURGED(this))
		return;
	
	if (this->worn_by != NULL)
		if (unequip_char(this->worn_by, this->worn_on) != this)
			log("SYSERR: Inconsistent worn_by and worn_on pointers!!");

	if (IN_ROOM(this) != NOWHERE)	this->from_room();
	else if (this->carried_by)		this->from_char();
	else if (this->in_obj)			this->from_obj();

	/* Get rid of the contents of the object, as well. */
//	while (this->contains)
//		this->contains->extract();
	START_ITER(iter, temp, ObjData *, this->contains) {
		temp->extract();
	} END_ITER(iter);

	if (IN_ROOM(this) != NOWHERE && IS_SITTABLE(this))
		while ((ch = get_char_on_obj(this)))
			ch->sitting_on = NULL;

	if (SCRIPT(this)) {
		SCRIPT(this)->extract();
		SCRIPT(this) = NULL;
	}
	
	/* remove any pending event for/from this object. */
	clean_events(this);
	
	Event *		event;
	LListIterator<Event *>	eventIter(this->events);
	while ((event = eventIter.Next())) {
		this->events.Remove(event);
		event->Cancel();
	}

	if (GET_OBJ_RNUM(this) >= 0)
		(obj_index[GET_OBJ_RNUM(this)].number)--;
	
	object_list.Remove(this);
	purged_objs.Add(this);
	
	PURGED(this) = TRUE;
}


/* give an object to a char   */
void ObjData::to_char(CharData * ch) {
	if (!ch) {
		log("SYSERR: NULL char passed to ObjData::to_char");
		return;
	} else if (PURGED(this) || PURGED(ch))
		return;
		
	ch->carrying.Add(this);
	this->carried_by = ch;
	IN_ROOM(this) = NOWHERE;
	IS_CARRYING_W(ch) += GET_OBJ_WEIGHT(this);
	IS_CARRYING_N(ch)++;

	/* set flag for crash-save system */
	if (!IS_NPC(this->carried_by))
		SET_BIT(PLR_FLAGS(ch), PLR_CRASH);
}


/* take an object from a char */
void ObjData::from_char() {
	if (PURGED(this))
		return;
	else if (!this->carried_by) {
		log("SYSERR: Object not carried by anyone in call to obj_from_char.");
		return;
	}
	this->carried_by->carrying.Remove(this);
	
	/* set flag for crash-save system */
	if (!IS_NPC(this->carried_by))
		SET_BIT(PLR_FLAGS(this->carried_by), PLR_CRASH);

	IS_CARRYING_W(this->carried_by) -= GET_OBJ_WEIGHT(this);
	IS_CARRYING_N(this->carried_by)--;
	this->carried_by = NULL;
}


/* put an object in a room */
void ObjData::to_room(RNum room) {
	if (room < 0 || room > top_of_world)
		log("SYSERR: Illegal value(s) passed to obj_to_room. (Room #%d/%d, obj %s)", room, top_of_world, SSData(this->shortDesc));
	else if (!PURGED(this)){
		world[room].contents.Add(this);
		IN_ROOM(this) = room;
		this->carried_by = NULL;
		if (ROOM_FLAGGED(room, ROOM_HOUSE))
			SET_BIT(ROOM_FLAGS(room), ROOM_HOUSE_CRASH);
	}
}


/* Take an object from a room */
void ObjData::from_room(void) {
	CharData *tch;

	if (IN_ROOM(this) == NOWHERE) {
		log("SYSERR: Object (%s) not in a room passed to obj_from_room", SSData(this->shortDesc));
		return;
	}
	
	if (PURGED(this))
		return;

	START_ITER(iter, tch, CharData *, world[IN_ROOM(this)].people) {
		if (tch->sitting_on == this)
			tch->sitting_on = NULL;
	} END_ITER(iter);
	
	if (ROOM_FLAGGED(IN_ROOM(this), ROOM_HOUSE))
		SET_BIT(ROOM_FLAGS(IN_ROOM(this)), ROOM_HOUSE_CRASH);
	
	world[IN_ROOM(this)].contents.Remove(this);
	
	IN_ROOM(this) = NOWHERE;
}


/* put an object in an object (quaint)  */
void ObjData::to_obj(ObjData* obj_to) {
	ObjData *tmp_obj;

	if (!obj_to || this == obj_to) {
		log("SYSERR: NULL object (%p) or same source and target obj passed to obj_to_obj", obj_to);
		return;
	}
	
	if (PURGED(this))
		return;
	
	obj_to->contains.Add(this);
	this->in_obj = obj_to;

	for (tmp_obj = this->in_obj; tmp_obj->in_obj; tmp_obj = tmp_obj->in_obj)
		GET_OBJ_WEIGHT(tmp_obj) += GET_OBJ_WEIGHT(this);

	/* top level object.  Subtract weight from inventory if necessary. */
	GET_OBJ_WEIGHT(tmp_obj) += GET_OBJ_WEIGHT(this);
	if (tmp_obj->carried_by)
		IS_CARRYING_W(tmp_obj->carried_by) += GET_OBJ_WEIGHT(this);
	
	if ((GET_OBJ_TYPE(this) == ITEM_ATTACHMENT) && (GET_OBJ_TYPE(obj_to) != ITEM_CONTAINER)) {
		// DO THE ATTACHMENT
	}
}


/* remove an object from an object */
void ObjData::from_obj() {
	ObjData *temp, *obj_from;

	if (this->in_obj == NULL) {
		log("SYSERR: trying to illegally extract obj from obj");
		return;
	}
	if (PURGED(this))
		return;
		
	obj_from = this->in_obj;
	obj_from->contains.Remove(this);

	/* Subtract weight from containers container */
	for (temp = this->in_obj; temp->in_obj; temp = temp->in_obj)
		GET_OBJ_WEIGHT(temp) -= GET_OBJ_WEIGHT(this);

	/* Subtract weight from char that carries the object */
	GET_OBJ_WEIGHT(temp) -= GET_OBJ_WEIGHT(this);
	if (temp->carried_by)
		IS_CARRYING_W(temp->carried_by) -= GET_OBJ_WEIGHT(this);

	this->in_obj = NULL;

	if ((GET_OBJ_TYPE(this) == ITEM_ATTACHMENT) && (GET_OBJ_TYPE(obj_from) != ITEM_CONTAINER)) {
		// DO THE DETACHMENT
	}
}


/*
void ObjData::unequip() {


}

void ObjData::equip(CharData *ch) {

}
*/


void ObjData::update(UInt32 use) {
	ObjData *	obj;
	if (GET_OBJ_TIMER(this) > 0)		GET_OBJ_TIMER(this) -= use;
	START_ITER(iter, obj, ObjData *, this->contains) {
		obj->update(use);
	} END_ITER(iter);
//	if (this->contains && this->contains != this)			this->contains->update(use);
//	if (this->next_content && this->next_content != this)	this->next_content->update(use);
}


SInt32 ObjData::TotalValue(void) {
	SInt32		value;
	ObjData *	obj;
	
	value = OBJ_FLAGGED(this, ITEM_MISSION) ? GET_OBJ_COST(this) : 0;
	
	START_ITER(iter, obj, ObjData *, this->contains) {
		value += obj->TotalValue();
	} END_ITER(iter);
	
	return value;
}

bool ObjData::load(FILE *fl, char *filename) {
	char *line, *tag, *buf;
	SInt32	num, t[3];
	UInt8	currentAffect = 0;
	bool	result = true;
//	ExtraDesc *new_descr;
	TrigData *trig;
	
	line = get_buffer(MAX_INPUT_LENGTH);
	tag = get_buffer(6);
	buf = get_buffer(MAX_INPUT_LENGTH);

	sprintf(buf, "plr obj file %s", filename);

	while(get_line(fl, line)) {
		if (*line == '$')	break;	// Done reading object
		tag_argument(line, tag);
		num = atoi(line);
		if (!strcmp(tag, "Actn")) {
			SSFree(this->actionDesc);
			this->actionDesc = SSfread(fl, buf, filename);
		} else if (!strcmp(tag, "Aff ")) {
//			sscanf(line, "%d %d %d", t, t + 1, t + 2);
//			this->affect[t[0]].location = t[1];
//			this->affect[t[0]].modifier = t[2];
		} else if (!strcmp(tag, "Affs"))	this->affects = asciiflag_conv(line);
		else if (!strcmp(tag, "Ammo")) {
			if (!IS_GUN(this))	GET_GUN_INFO(this) = new GunData();
			sscanf(line, "%d %d", t, t + 1);
			GET_GUN_AMMO_VNUM(this) = t[0];
			GET_GUN_AMMO(this) = t[1];
		} else if (!strcmp(tag, "Cost"))	this->cost = num;
		else if (!strcmp(tag, "Desc")) {
			SSFree(this->description);
			this->description = SSCreate(line);
		} else if (!strcmp(tag, "GAtk")) {
			if (!IS_GUN(this))	GET_GUN_INFO(this) = new GunData();
			GET_GUN_ATTACK_TYPE(this) = num;
		} else if (!strcmp(tag, "GATp")) {
			if (!IS_GUN(this))	GET_GUN_INFO(this) = new GunData();
			GET_GUN_AMMO_TYPE(this) = num;
		} else if (!strcmp(tag, "GDic")) {
			if (!IS_GUN(this))	GET_GUN_INFO(this) = new GunData();
			sscanf(line, "%d %d", t, t + 1);
			GET_GUN_DICE_NUM(this) = t[0];
			GET_GUN_DICE_SIZE(this) = t[1];
		} else if (!strcmp(tag, "GRat")) {
			if (!IS_GUN(this))	GET_GUN_INFO(this) = new GunData();
			GET_GUN_RATE(this) = num;
		} else if (!strcmp(tag, "GRng")) {
			if (!IS_GUN(this))	GET_GUN_INFO(this) = new GunData();
			GET_GUN_RANGE(this) = num;
		} else if (!strcmp(tag, "Name")) {
			SSFree(this->name);
			this->name = SSCreate(line);
		} else if (!strcmp(tag, "Shrt")) {
			SSFree(this->shortDesc);
			this->shortDesc = SSCreate(line);
		} else if (!strcmp(tag, "Sped"))	this->speed = num;
		else if (!strcmp(tag, "Time"))		this->timer = num;
		else if (!strcmp(tag, "Trig")) {
			if ((trig = read_trigger(num, VIRTUAL))) {
				if (!SCRIPT(this))	SCRIPT(this) = new ScriptData;
				add_trigger(SCRIPT(this), trig); //, 0);
			}
		} else if (!strcmp(tag, "Type"))	this->type = num;
		else if (!strcmp(tag, "XDsc")) {
//			new_descr = new ExtraDesc();
//			new_descr->keyword = SSCreate(line);
//			new_descr->description = SSfread(fl, buf, filename);
//			new_descr->next = this->exDesc;
//			this->exDesc = new_descr;
		} else if (!strcmp(tag, "Val0"))	this->value[0] = num;
		else if (!strcmp(tag, "Val1"))		this->value[1] = num;
		else if (!strcmp(tag, "Val2"))		this->value[2] = num;
		else if (!strcmp(tag, "Val3"))		this->value[3] = num;
		else if (!strcmp(tag, "Val4"))		this->value[4] = num;
		else if (!strcmp(tag, "Val5"))		this->value[5] = num;
		else if (!strcmp(tag, "Val6"))		this->value[6] = num;
		else if (!strcmp(tag, "Val7"))		this->value[7] = num;
		else if (!strcmp(tag, "Wate"))		this->weight = num;
		else if (!strcmp(tag, "Wear"))		this->wear = asciiflag_conv(line);
		else if (!strcmp(tag, "Xtra"))		this->extra = asciiflag_conv(line);
		else {
			log("SYSERR: PlrObj file %s has unknown tag %s (rest of line: %s)", filename, tag, line);
			result = false;
		}
	}
	release_buffer(line);
	release_buffer(tag);
	release_buffer(buf);
	return result;
}


#define NOT_SAME(field)		((this->number == -1) || (this->field != ((ObjData *)obj_index[this->number].proto)->field))

void ObjData::save(FILE *fl, SInt32 location) {
	char *	buf = get_buffer(MAX_STRING_LENGTH);
//	ExtraDesc *extradesc, *extradesc2;
//	bool	found;
//	UInt32	x;
	
	fprintf(fl, "#%d %d\n", GET_OBJ_VNUM(this), location);
	if (NOT_SAME(name))			fprintf(fl, "Name: %s\n", SSData(this->name));
	if (NOT_SAME(shortDesc))	fprintf(fl, "Shrt: %s\n", SSData(this->shortDesc));
	if (NOT_SAME(description))	fprintf(fl, "Desc: %s\n", SSData(this->description));
	if (this->actionDesc && NOT_SAME(actionDesc))	{
		strcpy(buf, SSData(this->actionDesc));
		strip_string(buf);
		fprintf(fl, "Actn:\n%s~\n", buf);
	}
/*
	for (extradesc = this->exDesc; extradesc; extradesc = extradesc->next) {
		found = false;
		if (this->number > -1) {
			for (extradesc2 = ((ObjData *)obj_index[this->number].proto)->exDesc;
					extradesc2 && !found; extradesc2 = extradesc2->next) {
				if (!strcmp(SSData(extradesc->keyword), SSData(extradesc2->keyword)))
					if (!strcmp(SSData(extradesc->description
			}
		}
	}
*/
	if (NOT_SAME(value[0]))		fprintf(fl, "Val0: %d\n", this->value[0]);
	if (NOT_SAME(value[1]))		fprintf(fl, "Val1: %d\n", this->value[1]);
	if (NOT_SAME(value[2]))		fprintf(fl, "Val2: %d\n", this->value[2]);
	if (NOT_SAME(value[3]))		fprintf(fl, "Val3: %d\n", this->value[3]);
	if (NOT_SAME(value[4]))		fprintf(fl, "Val4: %d\n", this->value[4]);
	if (NOT_SAME(value[5]))		fprintf(fl, "Val5: %d\n", this->value[5]);
	if (NOT_SAME(value[6]))		fprintf(fl, "Val6: %d\n", this->value[6]);
	if (NOT_SAME(value[7]))		fprintf(fl, "Val7: %d\n", this->value[7]);
	
	if (NOT_SAME(cost))			fprintf(fl, "Cost: %d\n", this->cost);
	if (NOT_SAME(weight))		fprintf(fl, "Wate: %d\n", this->weight);
	if (NOT_SAME(timer))		fprintf(fl, "Time: %d\n", this->timer);
	if (NOT_SAME(type))			fprintf(fl, "Type: %d\n", this->type);
	if (NOT_SAME(wear)) {
		sprintbits(this->wear, buf);
		fprintf(fl, "Wear: %s\n", buf);
	}
	if (NOT_SAME(extra)) {
		sprintbits(this->extra, buf);
		fprintf(fl, "Xtra: %s\n", buf);
	}
	if (NOT_SAME(affects)) {
		sprintbits(this->affects, buf);
		fprintf(fl, "Affs: %s\n", buf);
	}
	if (NOT_SAME(speed))		fprintf(fl, "Sped: %d\n", this->speed);
	if (this->gun && this->type == ITEM_WEAPON) {
		if (NOT_SAME(gun->attack))		fprintf(fl, "GAtk: %d\n", this->gun->attack);
		if (NOT_SAME(gun->rate))		fprintf(fl, "GRat: %d\n", this->gun->rate);
		if (NOT_SAME(gun->range))		fprintf(fl, "GRng: %d\n", this->gun->range);
		if (NOT_SAME(gun->dice.number) || NOT_SAME(gun->dice.size))
			fprintf(fl, "GDic: %d %d\n", this->gun->dice.number, this->gun->dice.size);
		if (NOT_SAME(gun->ammo.type))	fprintf(fl, "GATp: %d\n", this->gun->ammo.type);
		if (this->gun->ammo.amount)
			fprintf(fl, "Ammo: %d %d\n", this->gun->ammo.vnum, this->gun->ammo.amount);
	}
//	for (x = 0; x < MAX_OBJ_AFFECT; x++) {
//		if (NOT_SAME(affect[x].modifier))
//			fprintf(fl, "Aff : %d %d %d\n", x, this->affect[x].location, this->affect[x].modifier);
//	}
	
	fprintf(fl, "$\n");
	
	release_buffer(buf);
	
}


void equip_char(CharData * ch, ObjData * obj, UInt8 pos) {
	int j;

	if (pos > NUM_WEARS) {
		core_dump();
		return;
	}

	if (PURGED(obj))
		return;

	if (GET_EQ(ch, pos)) {
//		log("SYSERR: Char is already equipped: %s, %s", ch->RealName(), SSData(obj->shortDesc));
		return;
	}
	if (obj->carried_by) {
		log("SYSERR: EQUIP: Obj is carried_by when equip.");
		return;
	}
	if (IN_ROOM(obj) != NOWHERE) {
		log("SYSERR: EQUIP: Obj is in_room when equip.");
		return;
	}

	if (!ch->CanUse(obj)) {
		act("You can't figure out how to use $p.", FALSE, ch, obj, 0, TO_CHAR);
		act("$n tries to use $p, but can't figure it out.", FALSE, ch, obj, 0, TO_ROOM);
		obj->to_char(ch);	/* changed to drop in inventory instead of ground */
		return;
	}

	GET_EQ(ch, pos) = obj;
	obj->worn_by = ch;
	obj->worn_on = pos;

	if (GET_OBJ_TYPE(obj) == ITEM_ARMOR)
		GET_AC(ch) += GET_OBJ_VAL(GET_EQ(ch, pos), 0);

	if (IN_ROOM(ch) != NOWHERE) {
		if (((pos == WEAR_HAND_R) || (pos == WEAR_HAND_L)) && GET_OBJ_TYPE(obj) == ITEM_LIGHT)
			if (GET_OBJ_VAL(obj, 2))	/* if light is ON */
				world[IN_ROOM(ch)].light++;
		if (IS_SET(obj->affects, AFF_LIGHT) && !AFF_FLAGGED(ch, AFF_LIGHT))
			world[IN_ROOM(ch)].light++;
	} else
		log("SYSERR: IN_ROOM(ch) = NOWHERE when equipping char %s.", ch->RealName());

	for (j = 0; j < MAX_OBJ_AFFECT; j++)
		Affect::Modify(ch, obj->affect[j].location, obj->affect[j].modifier, obj->affects, TRUE);

	ch->AffectTotal();
}



ObjData *unequip_char(CharData * ch, UInt8 pos) {
	int j;
	ObjData *obj;

	if ((pos >= NUM_WEARS) || !GET_EQ(ch, pos)) {
		core_dump();
		return NULL;
	}

//	if(pos > NUM_WEARS) {
//		log("SYSERR: pos passed to unequip_char is invalid.  Pos = %d", pos);
//		return read_object(0, REAL);
//	}
//	if (!GET_EQ(ch, pos)) {
//		log("SYSERR: no EQ at position %d on character %s", pos, ch->RealName());
//		return read_object(0, REAL);
//	}

	obj = GET_EQ(ch, pos);
	obj->worn_by = NULL;
	obj->worn_on = -1;

	if (GET_OBJ_TYPE(obj) == ITEM_ARMOR)
		GET_AC(ch) -= GET_OBJ_VAL(GET_EQ(ch, pos), 0);

	if (IN_ROOM(ch) != NOWHERE) {
		if (((pos == WEAR_HAND_R) || (pos == WEAR_HAND_L)) && GET_OBJ_TYPE(obj) == ITEM_LIGHT)
			if (GET_OBJ_VAL(obj, 2))	/* if light is ON */
				world[IN_ROOM(ch)].light--;
		if (IS_SET(obj->affects, AFF_LIGHT))
			world[IN_ROOM(ch)].light--;
	} else
		log("SYSERR: IN_ROOM(ch) = NOWHERE when unequipping char %s.", ch->RealName());

	GET_EQ(ch, pos) = NULL;

	for (j = 0; j < MAX_OBJ_AFFECT; j++)
		Affect::Modify(ch, obj->affect[j].location, obj->affect[j].modifier,
				obj->affects, FALSE);

	ch->AffectTotal();
	
	if ((IN_ROOM(ch) != NOWHERE) && IS_SET(obj->affects, AFF_LIGHT) && AFF_FLAGGED(ch, AFF_LIGHT))
		world[IN_ROOM(ch)].light++;
	
	return (obj);
}


/* create a new object from a prototype */
ObjData *read_object(VNum nr, UInt8 type) {
	ObjData *obj;
	int i;
	struct int_list *trn;
	TrigData *trig;
	int t;


	if (nr < 0) {
		log("SYSERR: trying to create obj with negative number %d!", nr);
		return NULL;
	}
	if (type == VIRTUAL) {
		if ((i = real_object(nr)) < 0) {
			log("SYSERR: Object (V) %d does not exist in database.", nr);
			return NULL;
		}
	} else
		i = nr;

	obj = new ObjData((ObjData *)obj_index[i].proto);
	object_list.Add(obj);
	GET_ID(obj) = max_id++;

	obj_index[i].number++;
	
	/* add triggers */
	for (trn = obj_index[i].triggers; trn; trn = trn->next) {
		t = real_trigger(trn->i);
		if (t == -1)
			log("SYSERR: Invalid trigger %d assigned to object %d", trn->i, obj_index[i].vnum);
		else {
			trig = read_trigger(t, REAL);

			if (!SCRIPT(obj))
				SCRIPT(obj) = new ScriptData;
			add_trigger(SCRIPT(obj), trig); //, 0);
		}
	}

	return obj;
}



RNum real_object(VNum vnum) {
	int bot, top, mid, nr, found = NOWHERE;

	/* First binary search. */
	bot = 0;
	top = top_of_objt;

	for (;;) {
		mid = (bot + top) / 2;

		if ((obj_index + mid)->vnum == vnum)
			return mid;
		if (bot >= top)
			break;
		if ((obj_index + mid)->vnum > vnum)
			top = mid - 1;
		else
			bot = mid + 1;
	}

	/* If not found - use linear on the "new" parts. */
	for(nr = 0; nr <= top_of_objt; nr++) {
		if(obj_index[nr].vnum == vnum) {
			found = nr;
			break;
		}
	}
	return(found);
}


/* read all objects from obj file; generate index and prototypes */
char *parse_object(FILE * obj_f, VNum nr, char *filename) {
	static int i = 0, retval;
	static char line[256];
	SInt32	t[10], j;
	char	*tmpptr,
			*f1 = get_buffer(256),
			*f2 = get_buffer(256),
			*buf = get_buffer(128),
			*triggers, *s;
	struct int_list *tr_list;
	ObjData *obj;
	ExtraDesc *	new_descr;

	obj_index[i].vnum = nr;
	obj_index[i].number = 0;
	obj_index[i].func = NULL;

	obj = new ObjData();
	obj_index[i].proto = obj;
	obj->number = i;
	IN_ROOM(obj) = NOWHERE;

	sprintf(buf, "object #%d", nr);

	/* *** string data *** */
	if ((obj->name = SSfread(obj_f, buf, filename)) == NULL) {
		log("SYSERR: Null obj name or format error at or near %s", buf);
		tar_restore_file(filename);
		exit(1);
	}
	
	tmpptr = fread_string(obj_f, buf, filename);
	if (tmpptr && *tmpptr)
		if (!str_cmp(fname(tmpptr), "a") || !str_cmp(fname(tmpptr), "an") ||
				!str_cmp(fname(tmpptr), "the"))
			*tmpptr = LOWER(*tmpptr);
	obj->shortDesc = SSCreate(tmpptr);
	if (tmpptr) FREE(tmpptr);
	
//	tmpptr = fread_string(obj_f, buf, filename);
//	if (tmpptr && *tmpptr)
//		*tmpptr = UPPER(*tmpptr);
//	obj->description = SSCreate(tmpptr);
//	if (tmpptr) FREE(tmpptr);
	obj->description = SSfread(obj_f, buf, filename);
	obj->actionDesc = SSfread(obj_f, buf, filename);
  
/* *** numeric data *** */
	if (!get_line(obj_f, line) || (retval = sscanf(line, " %d %s %s", t, f1, f2)) != 3) {
		log("SYSERR: Format error in first numeric line (expecting 3 args, got %d), %s\n", retval, buf);
		tar_restore_file(filename);
		exit(1);
	}
	obj->type	= t[0];
	obj->extra	= asciiflag_conv(f1);
	obj->wear	= asciiflag_conv(f2);

	if (!get_line(obj_f, line) || (retval = sscanf(line, "%d %d %d %d %d %d %d %d", t, t + 1, t + 2, t + 3, t + 4, t + 5, t + 6, t + 7)) != 8) {
		log("SYSERR: Format error in second numeric line (expecting 8 args, got %d), %s\n", retval, buf);
		tar_restore_file(filename);
		exit(1);
	}
	obj->value[0] = t[0];
	obj->value[1] = t[1];
	obj->value[2] = t[2];
	obj->value[3] = t[3];
	obj->value[4] = t[4];
	obj->value[5] = t[5];
	obj->value[6] = t[6];
	obj->value[7] = t[7];

	if (!get_line(obj_f, line) || (retval = sscanf(line, "%d %d %d %d", t, t + 1, t + 2, t + 3)) < 3) {
		log("SYSERR: Format error in third numeric line (expecting 3 args, got %d), %s\n", retval, buf);
		tar_restore_file(filename);
		exit(1);
	}
	obj->weight	= t[0];
	obj->cost	= t[1];
	obj->speed	= t[2];
	if (retval == 4)
		obj->timer = t[3];

/* check to make sure that weight of containers exceeds curr. quantity */
	if (obj->type == ITEM_DRINKCON || obj->type == ITEM_FOUNTAIN) {
		if (obj->weight < obj->value[1])
			obj->weight = obj->value[1] + 5;
	}

/* *** extra descriptions and affect fields *** */

	for (j = 0; j < MAX_OBJ_AFFECT; j++) {
		obj->affect[j].location = APPLY_NONE;
		obj->affect[j].modifier = 0;
	}

	strcat(buf, ", after numeric constants\n...expecting 'E', 'A', 'G', 'B', 'T', '$', or next object number");
	j = 0;

	for (;;) {
		if (!get_line(obj_f, line)) {
			log("SYSERR: Format error in %s", buf);
			tar_restore_file(filename);
			exit(1);
		}
		switch (*line) {
			case 'E':
				new_descr = new ExtraDesc();
				new_descr->keyword = SSfread(obj_f, buf, filename);
				new_descr->description = SSfread(obj_f, buf, filename);
				new_descr->next = obj->exDesc;
				obj->exDesc = new_descr;
				break;
			case 'A':
				if (j >= MAX_OBJ_AFFECT) {
					log("SYSERR: Too many A fields (%d max), %s\n", MAX_OBJ_AFFECT, buf);
					tar_restore_file(filename);
					exit(1);
				}
				get_line(obj_f, line);
				sscanf(line, " %d %d ", t, t + 1);
				obj->affect[j].location = t[0];
				obj->affect[j].modifier = t[1];
				j++;
				break;
			case 'G':
				if (!IS_GUN(obj)) {
					GET_GUN_INFO(obj) = new GunData();
					get_line(obj_f, line);
					sscanf(line, "%d %d %d %d %d %d\n", t, t+1, t+2, t+3, t+4, t+5);
					GET_GUN_DICE_NUM(obj)		= t[0];
					GET_GUN_DICE_SIZE(obj)		= t[1];
					GET_GUN_RATE(obj)			= t[2];
					GET_GUN_AMMO_TYPE(obj)		= t[3];
					GET_GUN_ATTACK_TYPE(obj)	= t[4];
					GET_GUN_RANGE(obj)			= t[5];
				} else
					log("SYSERR: Too many G fields (1 max)\n");
				break;
			case 'B':
				get_line(obj_f, line);
				obj->affects = asciiflag_conv(line);
				break;
			case 'T': /* triggers */
				sprintf(buf, "obj vnum %d, section P\n", nr);
				if (obj_index[i].triggers) {
					log("More than one list of triggers found on object, %s", buf);
					break;
				}
				triggers = fread_string(obj_f, buf, filename);
				for (s = strtok(triggers, " \r\n"); s; s = strtok(NULL, " \r\n")) {
					CREATE(tr_list, struct int_list, 1);
					tr_list->i = atoi(s);
					tr_list->next = obj_index[i].triggers;
					obj_index[i].triggers = tr_list;
				}
				FREE(triggers);
				break;

			case '$':
			case '#':
				top_of_objt = i++;
				release_buffer(f1);
				release_buffer(f2);
				release_buffer(buf);
				return line;
				break;
			default:
				log("SYSERR: Format error in %s", buf);
				tar_restore_file(filename);
				exit(1);
				break;
		}
	}
}


/* create an object, and add it to the object list */
ObjData *create_obj(void) {
	ObjData *obj;

	obj = new ObjData;
	object_list.Add(obj);
	GET_ID(obj) = max_id++;

	return obj;
}


/* returns the real room number that the object or object's carrier is in */
RNum ObjData::Room(void) {
	if (IN_ROOM(this) != NOWHERE)		return IN_ROOM(this);
	else if (this->carried_by)			return IN_ROOM(this->carried_by);
	else if (this->worn_by)				return IN_ROOM(this->worn_by);
	else if (this->in_obj)				return this->in_obj->Room();
	else								return NOWHERE;
}
