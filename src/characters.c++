/*************************************************************************
*   File: characters.c++             Part of Aliens vs Predator: The MUD *
*  Usage: Primary code for characters                                    *
*************************************************************************/


#include "structs.h"
#include "utils.h"
#include "scripts.h"
#include "comm.h"
#include "buffer.h"
#include "files.h"
#include "interpreter.h"
#include "affects.h"
#include "db.h"
#include "clans.h"
#include "opinion.h"
#include "alias.h"
#include "event.h"
#include "handler.h"
#include "spells.h"

#include <stdarg.h>

// External variables
extern char *MENU;
extern UInt32	max_id;
extern struct title_type titles[NUM_RACES][16];


// External functions
void mprog_death_trigger(CharData * mob, CharData * killer);
int death_mtrigger(CharData *ch, CharData *actor);
void death_cry(CharData *ch);
void announce(char *string);
void clearMemory(CharData *ch);
void make_corpse(CharData *ch);
void strip_string(char *buffer);
int find_name(char *name);
void tag_argument(char *argument, char *tag);
void mprog_read_programs(FILE *fp, IndexData *pIndex, char * mobfile);
void check_autowiz(CharData * ch);
void CheckRegenRates(CharData *ch);

LList<CharData *>	character_list;		//	global linked list of chars
LList<CharData *>	purged_chars;		//	list of chars to be deleted
IndexData *mob_index;	/* index table for mobile file	 */
UInt32 top_of_mobt = 0;		/* top of mobile index table	 */
struct player_index_element *player_table = NULL;	/* index to plr file	 */
SInt32 top_of_p_table = 0;		/* ref to top of table		 */
PlayerData dummy_player;	/* dummy spec area for mobs	 */
MobData dummy_mob;	/* dummy spec area for players	 */


PlayerData::PlayerData(void) {
	memset(this, 0, sizeof(struct PlayerData));
	this->special.load_room = NOWHERE;
	this->special.imc.listen = str_dup("");
}


PlayerData::~PlayerData(void) {
	Alias *a;
	while ((a = this->aliases.Top())) {
		this->aliases.Remove(a);
		delete a;
	}
	if (this->poofin)		free(this->poofin);
	if (this->poofout)		free(this->poofout);
	if (this->host)			free(this->host);
	if (this->prompt)		free(this->prompt);
	if (this->afk)			free(this->afk);
	
	if (this->special.imc.listen)	free(this->special.imc.listen);
	if (this->special.imc.rreply)	free(this->special.imc.rreply);
	if (this->special.imc.rreply_name)	free(this->special.imc.rreply_name);
}


MobData::MobData(void) {
	this->Init();
}


MobData::MobData(MobData *mob) {
	this->Init();
	this->attack_type = mob->attack_type;
	this->default_pos = mob->default_pos;
	this->dodge = mob->dodge;
	this->damage = mob->damage;
	this->hitdice = mob->hitdice;
	
	if (mob->hates)	this->hates = new Opinion(mob->hates);
	if (mob->fears)	this->fears = new Opinion(mob->fears);
}


void MobData::Init(void) {
	memset(this, 0, sizeof(struct MobData));
}

MobData::~MobData(void) {
	if (this->hates)	delete this->hates;
	if (this->fears)	delete this->fears;
}


CharData::CharData(void) {
	this->Init();
	
	this->general.position = POS_STANDING;
	
	this->points.armor = 0;
	this->points.max_mana = 100;
	this->points.max_move = 10;
}


//	Copy constructor, yay!
CharData::CharData(CharData *ch) {
	Affect *aff, *newAff;
	SInt32	eq;
	ObjData *item;
	
	this->Init();
	
//	*this = *ch;
	GET_MOB_RNUM(this) = GET_MOB_RNUM(ch);
	GET_ID(this) = max_id++;
	
	this->player = &dummy_player;
	this->mob = (!ch->mob || (ch->mob == &dummy_mob)) ? new MobData() : new MobData(ch->mob);
	if (!ch->mob || (ch->mob == &dummy_mob)) {
		this->mob->dodge = GET_SKILL(ch, SKILL_DODGE);
	}
	this->general = ch->general;
	// Share the strings!
	this->general.name = SSShare(ch->general.name);
//	this->general.shortDesc = SSShare(ch->general.shortDesc);
//	this->general.longDesc = SSShare(ch->general.longDesc);
	this->general.description = SSShare(ch->general.description);
	this->general.title = SSShare(ch->general.title);

	if (!IS_NPC(ch)) {
		char *buf = get_buffer(MAX_STRING_LENGTH);
		
		if (IS_STAFF(ch))	sprintf(buf, "%s %s", ch->GetName(), SSData(ch->general.title));
		else				sprintf(buf, "%s %s", SSData(ch->general.title), ch->GetName());
		this->general.longDesc = SSCreate(buf);
		this->general.shortDesc = SSShare(this->general.longDesc);
		
		release_buffer(buf);
	} else {
		this->general.shortDesc = SSShare(ch->general.shortDesc);
		this->general.longDesc = SSShare(ch->general.longDesc);
	}

	FIGHTING(this) = NULL;
	HUNTING(this) = 0;
	
	MOB_FLAGS(this) = IS_NPC(ch) ? MOB_FLAGS(ch) : MOB_ISNPC;	//	Gaurantee - no duping players, lest to make a mob

	this->real_abils = ch->real_abils;
	this->aff_abils = ch->real_abils;
	this->points = ch->points;
	
	START_ITER(affect_iter, aff, Affect *, ch->affected) {
		newAff = new Affect(aff, this);
		this->affected.Append(newAff);
	} END_ITER(affect_iter);
	
	for (eq = 0; eq < NUM_WEARS; eq++) {
		if (ch->equipment[eq])
			this->equipment[eq] = new ObjData(ch->equipment[eq]);
	}
	
	START_ITER(item_iter, item, ObjData *, ch->carrying) {
		this->carrying.Append(new ObjData(item));
	} END_ITER(item_iter);
	
	this->mprog_target = ch->mprog_target;
//	UNIMPLEMENTED:	SCRIPT COPY in CharData Copy Constructor
//	this->script = new ScriptData(ch->script);
}


CharData::~CharData(void) {
	if (this->player && (this->player != &dummy_player)) {
		delete this->player;
		if (IS_NPC(this))
			log("SYSERR: Mob %s (#%d) had PlayerData allocated!", this->RealName(), GET_MOB_VNUM(this));
	}
	if (this->mob && (this->mob != &dummy_mob))	{
		delete this->mob;
		if (!IS_NPC(this))
			log("SYSERR: Player %s had MobData allocated!", this->RealName());
	}
	SSFree(this->general.name);
	SSFree(this->general.title);
	SSFree(this->general.shortDesc);
	SSFree(this->general.longDesc);
	SSFree(this->general.description);

//	while (this->affected)
//		this->affected->Remove(this);
}


/* clear ALL the working variables of a char; do NOT free any space alloc'ed */
void CharData::Init(void) {
	memset(this, 0, sizeof(*this));

	GET_MOB_RNUM(this) = -1;
	IN_ROOM(this) = NOWHERE;
	GET_WAS_IN(this) = NOWHERE;
	GET_PFILEPOS(this) = -1;
	GET_POS(this) = POS_STANDING;
	this->mob = &dummy_mob;
	this->player = &dummy_player;

	HUNTING(this) = NOBODY;
}


const char *CharData::GetName(void) const {
	return this->RealName();
}

const char *CharData::RealName(void) const {
	return (IS_NPC(this) ? SSData(this->general.shortDesc) : SSData(this->general.name));
}

/* Extract a ch completely from the world, and leave his stuff behind */
void CharData::extract(void) {
	CharData *k;
	DescriptorData *t_desc;
	ObjData *obj;
	int i;
	ACMD(do_return);

	if (PURGED(this))
		return;

	if (!IS_NPC(this) && !this->desc) {
		START_ITER(iter, t_desc, DescriptorData *, descriptor_list) {
			if (t_desc->original == this)
				do_return(t_desc->character, "", 0, "return", 0);
		}
	}
	if (IN_ROOM(this) == NOWHERE) {
		log("SYSERR: NOWHERE extracting char %s in extract_char.", this->RealName());
		core_dump();
//		exit(1);
	}
	if (this->followers.Count() || this->master)
		this->die_follower();

	/* Forget snooping, if applicable */
	if (this->desc) {
		if (this->desc->snooping) {
			this->desc->snooping->snoop_by = NULL;
			this->desc->snooping = NULL;
		}
		if (this->desc->snoop_by) {
			SEND_TO_Q("Your victim is no longer among us.\r\n",
					this->desc->snoop_by);
			this->desc->snoop_by->snooping = NULL;
			this->desc->snoop_by = NULL;
		}
	}
	
	/* transfer objects to room, if any */
	START_ITER(object_iter, obj, ObjData *, this->carrying) {
		if (!OBJ_FLAGGED(obj, ITEM_MISSION)) {
			obj->from_char();
			obj->to_room(IN_ROOM(this));
		} else
			obj->extract();
	}

	/* transfer equipment to room, if any */
	for (i = 0; i < NUM_WEARS; i++) {
		if (GET_EQ(this, i)) {
			if (!OBJ_FLAGGED(GET_EQ(this, i), ITEM_MISSION))
				unequip_char(this, i)->to_room(IN_ROOM(this));
			else
				GET_EQ(this, i)->extract();
		}
	}
			
	if (FIGHTING(this))
		this->stop_fighting();

	START_ITER(combat_iter, k, CharData *, combat_list) {
		if (FIGHTING(k) == this)
			k->stop_fighting();
	} END_ITER(combat_iter);


	if (SCRIPT(this)) {
		SCRIPT(this)->extract();
		SCRIPT(this) = NULL;
	}

	//	pull the char from the lists
	this->from_room();
	this->from_world();
	
	//	remove any pending events for this character.
	this->EventCleanup();
	
	if (this->desc && this->desc->original)
		do_return(this, "", 0, "return", 0);

	if (!IS_NPC(this)) {
		this->save(NOWHERE);
	} else {
		if (GET_MOB_RNUM(this) > -1)		/* if mobile */
			mob_index[GET_MOB_RNUM(this)].number--;
		clearMemory(this);		/* Only NPC's can have memory */
		PURGED(this) = true;
		purged_chars.Add(this);
	}

	if (this->desc != NULL) {
		STATE(this->desc) = CON_MENU;
		SEND_TO_Q(MENU, this->desc);
	} else if (!PURGED(this)) {  /* if a player gets purged from within the game */
		PURGED(this) = true;
		purged_chars.Add(this);
	}
}


/* place a character in a room */
void CharData::to_room(RNum room) {
	EVENTFUNC(gravity);
	if (room < 0 || room > top_of_world)
		log("SYSERR: %s->to_room(RNum room = %d)", this->RealName(), room);
	else if (IN_ROOM(this) != NOWHERE)
		log("SYSERR: in_room == %d on %s->to_room(RNum room = %d)", IN_ROOM(this), this->RealName(), room);
	else if (PURGED(this))
		log("SYSERR: purged == true on %s->to_room(RNum room = %d)", this->RealName(), room);
	else {
		if (world[room].people.InList(this)) {
			log("SYSERR: world[%d].people.InList() == true, %s->in_room == %d on CharData::to_room()",
					room, this->RealName(), IN_ROOM(this));
			return;
		}
		world[room].people.Add(this);
		IN_ROOM(this) = room;

		if (GET_EQ(this, WEAR_HAND_R))
			if (GET_OBJ_TYPE(GET_EQ(this, WEAR_HAND_R)) == ITEM_LIGHT)
				if (GET_OBJ_VAL(GET_EQ(this, WEAR_HAND_R), 2))	/* Light ON */
					world[room].light++;
		if (GET_EQ(this, WEAR_HAND_L))
			if (GET_OBJ_TYPE(GET_EQ(this, WEAR_HAND_L)) == ITEM_LIGHT)
				if (GET_OBJ_VAL(GET_EQ(this, WEAR_HAND_L), 2))	/* Light ON */
					world[room].light++;
		if (AFF_FLAGGED(this, AFF_LIGHT))
			world[room].light++;
			
		this->temp_was_in = NOWHERE;
		
		if (FIGHTING(this) && (IN_ROOM(this) != IN_ROOM(FIGHTING(this)))) {
			FIGHTING(this)->stop_fighting();
			this->stop_fighting();
		}
		
		if (NO_STAFF_HASSLE(this))	// Because the rest of the function is only to kill off morts.
			return;
		
		if (ROOM_FLAGGED(room, ROOM_DEATH)) {
			GET_HIT(this)	= -100;
			this->update_pos();
			return;
		}

		if (ROOM_FLAGGED(room, ROOM_GRAVITY) && !FindEvent(this->events, gravity)) {
//			add_event(3, gravity, this, NULL, NULL);
			struct FallingEvent *FEData;
			CREATE(FEData, FallingEvent, 1);
			FEData->faller = this;
			FEData->previous = 0;
			this->events.Add(new Event(gravity, FEData, 3 RL_SEC));
		}
		
		if (ROOM_FLAGGED(room, ROOM_VACUUM) && !AFF_FLAGGED(this, AFF_SPACESUIT)) {
			act("*Gasp* You can't breath!  Its a vacuum!", TRUE, this, 0, 0, TO_CHAR);
			GET_HIT(this) = -100;
			this->update_pos();
			return;
		}
	}
}


/* move a player out of a room */
void CharData::from_room() {
	if (PURGED(this)) {
		log("SYSERR: purged == TRUE in %s->from_room()", this->RealName());
		core_dump();
		return;
	}

	if (IN_ROOM(this) == NOWHERE) {
		log("SYSERR: in_room == NOWHERE in %s->from_room()", this->RealName());
		return;
	}

	CHAR_WATCHING(this) = 0;

	if (FIGHTING(this))
		this->stop_fighting();

	if (GET_EQ(this, WEAR_HAND_R))
		if (GET_OBJ_TYPE(GET_EQ(this, WEAR_HAND_R)) == ITEM_LIGHT)
			if (GET_OBJ_VAL(GET_EQ(this, WEAR_HAND_R), 2))	/* Light is ON */
				world[IN_ROOM(this)].light--;
	if (GET_EQ(this, WEAR_HAND_L))
		if (GET_OBJ_TYPE(GET_EQ(this, WEAR_HAND_L)) == ITEM_LIGHT)
			if (GET_OBJ_VAL(GET_EQ(this, WEAR_HAND_L), 2))	/* Light is ON */
				world[IN_ROOM(this)].light--;
	if (AFF_FLAGGED(this, AFF_LIGHT))
		world[IN_ROOM(this)].light--;

	world[IN_ROOM(this)].people.Remove(this);
	this->temp_was_in = IN_ROOM(this);
	IN_ROOM(this) = NOWHERE;

	this->sitting_on = NULL;
}


void CharData::die(CharData * killer) {
	Affect * aff;
	
	if (!IS_NPC(this)) {
		char *buf = get_buffer(MAX_INPUT_LENGTH);
		if (!killer || this == killer)
			sprintf(buf, "`y[`bINFO`y]`n %s killed at %s`n.\r\n", this->GetName(), world[IN_ROOM(this)].GetName());
		else			sprintf(buf, "`y[`bINFO`y]`n %s killed by %s`n at %s`n.\r\n", this->GetName(), killer->GetName(), world[IN_ROOM(this)].GetName());
		announce(buf);
	  	release_buffer(buf);
	}
  	
  	if (killer)
  		WAIT_STATE(killer, 0);
  	
	if (FIGHTING(this)) {
		FIGHTING(this)->stop_fighting();
		this->stop_fighting();
	}
	
	START_ITER(iter, aff, Affect *, this->affected) {
		aff->Remove(this);
	} END_ITER(iter);
	
	if (IS_NPC(this)) {
		if (death_mtrigger(this, killer)) {
			if (killer)	mprog_death_trigger(this, killer);
			else		death_cry(this);
		}
	} else
		death_cry(this);

	make_corpse(this);
	if (killer && (this != killer)) {
		if (IS_NPC(this))	killer->player->special.MKills++;
		else				killer->player->special.PKills++;
		
		if (IS_NPC(killer))	this->player->special.MDeaths++;
		else				this->player->special.PDeaths++;
	}

	if (IS_NPC(this))
		this->extract();
	else {
		REMOVE_BIT(PLR_FLAGS(this), PLR_TRAITOR);
		
		this->EventCleanup();
		
		this->from_room();
		this->to_room(this->desc ? this->StartRoom() : 1);
		
		GET_HIT(this) = (IS_SYNTHETIC(this) ? GET_MAX_HIT(this) : (GET_MAX_HIT(this) / 2));
		GET_MOVE(this) = (IS_SYNTHETIC(this) ? GET_MAX_MOVE(this) : (GET_MAX_MOVE(this) / 2));
		this->update_pos();
		CheckRegenRates(this);
		
		if (this->desc)	this->desc->inbuf[0] = '\0';
		act("$n has returned from the dead.", TRUE, this, 0, 0, TO_ROOM);
		GET_POS(this) = POS_SITTING;
		look_at_room(this, 0);
	}
}


void CharData::EventCleanup(void) {
	SInt32	i;
	Event *	event;
	
	clean_events(this);
	
	while ((event = this->events.Top())) {
		this->events.Remove(event);
		event->Cancel();
	}

	/* cancel point updates */
	for (i = 0; i < 3; i++) {
		if (GET_POINTS_EVENT(this, i)) {
			GET_POINTS_EVENT(this, i)->Cancel();
			GET_POINTS_EVENT(this, i) = NULL;
		}
	}
}


void CharData::update_objects(void) {
	int i, pos;

	for (pos = WEAR_HAND_R; pos <= WEAR_HAND_L; pos++) {
		if (GET_EQ(this, pos)) {
			if (GET_OBJ_TYPE(GET_EQ(this, pos)) == ITEM_LIGHT) {
				if (GET_OBJ_VAL(GET_EQ(this, pos), 2) > 0) {
					i = --GET_OBJ_VAL(GET_EQ(this, pos), 2);
					if (i == 1) {
						act("Your light begins to flicker and fade.", FALSE, this, 0, 0, TO_CHAR);
						act("$n's light begins to flicker and fade.", FALSE, this, 0, 0, TO_ROOM);
					} else if (i == 0) {
						act("Your light sputters out and dies.", FALSE, this, 0, 0, TO_CHAR);
						act("$n's light sputters out and dies.", FALSE, this, 0, 0, TO_ROOM);
						world[IN_ROOM(this)].light--;
					}
				}
			}
		}
	}
//	for (i = 0; i < NUM_WEARS; i++)
//		if (GET_EQ(this, i))
//			GET_EQ(this, i)->update(2);
//
//	if (this->carrying)
//		this->carrying->update(1);
}


//	This updates a character by subtracting everything he is affected by
//	restoring original abilities, and then affecting all again
void CharData::AffectTotal(void) {
	Affect *af;
	int i, j;
	LListIterator<Affect *>		iter(this->affected);

	for (i = 0; i < NUM_WEARS; i++) {
		if (GET_EQ(this, i))
			for (j = 0; j < MAX_OBJ_AFFECT; j++)
				Affect::Modify(this, GET_EQ(this, i)->affect[j].location,
						GET_EQ(this, i)->affect[j].modifier, GET_EQ(this, i)->affects, FALSE);
	}

	while ((af = iter.Next()))
		Affect::Modify(this, af->location, af->modifier, af->bitvector, FALSE);
	
	this->aff_abils = this->real_abils;

	for (i = 0; i < NUM_WEARS; i++) {
		if (GET_EQ(this, i))
			for (j = 0; j < MAX_OBJ_AFFECT; j++)
				Affect::Modify(this, GET_EQ(this, i)->affect[j].location,
						GET_EQ(this, i)->affect[j].modifier, GET_EQ(this, i)->affects, TRUE);
	}

	iter.Reset();
	while ((af = iter.Next()))
		Affect::Modify(this, af->location, af->modifier, af->bitvector, TRUE);
	
	/* Make certain values are between 0..200, not < 0 and not > 200! */
	i = (200);

	GET_STR(this) = MAX(0, MIN(GET_STR(this), i));
	GET_INT(this) = MAX(0, MIN(GET_INT(this), i));
	GET_PER(this) = MAX(0, MIN(GET_PER(this), i));
	GET_AGI(this) = MAX(0, MIN(GET_AGI(this), i));
	GET_CON(this) = MAX(0, MIN(GET_CON(this), i));

	CheckRegenRates(this);
}


/* enter a character in the world (place on lists) */
void CharData::to_world(void) {
	character_list.Add(this);
}


/* remove a character from the world lists */
void CharData::from_world(void) {
	character_list.Remove(this);
}


/*
 * If missing vsnprintf(), remove the 'n' and remove MAX_STRING_LENGTH.
 */
SInt32 CharData::Send(const char *messg, ...) {
	va_list args;
	char *send_buf;
	SInt32	length = 0;

	if (!messg || !*messg)
		return 0;
		
 	send_buf = get_buffer(MAX_STRING_LENGTH);

	va_start(args, messg);
	length += vsprintf(send_buf, messg, args);
	va_end(args);

	if (this->desc)	SEND_TO_Q(send_buf, this->desc);
	else			act(send_buf, FALSE, this, 0, 0, TO_CHAR);
	
	release_buffer(send_buf);
	
	return length;
}


/* new save_char that writes an ascii pfile */
void CharData::save(RNum load_room) {
	FILE *outfile;
	char *outname, *bits, *buf, *temp;
	int i;
	Affect *aff;
	ObjData *char_eq[NUM_WEARS];
	Alias *a;
	SInt32 hit, move;
//	SInt32 mana;


	if (IS_NPC(this) || GET_PFILEPOS(this) < 0)
		return;

	/*
	 * save current values, because unequip_char will call
	 * affect_total(), which will reset points to lower values
	 * if player is wearing +points items.  We will restore old
	 * points level after reequiping.
	 */
	hit = GET_HIT(this);
//	mana = GET_MANA(this);
	move = GET_MOVE(this);


	outname = get_buffer(MAX_INPUT_LENGTH);
	bits = get_buffer(MAX_INPUT_LENGTH);

	{
		temp = SSData(this->general.name);
		for (i = 0; (*(bits + i) = LOWER(*(temp + i))); i++);
		
		sprintf(outname, PLR_PREFIX "%c" SEPERATOR "%s", *bits, bits);

		if (!(outfile = fopen(outname, "w"))) {
			perror(outname);
			release_buffer(outname);
			release_buffer(bits);
			return;
		}
		/* Unequip everything a character can be affected by */
		for (i = 0; i < NUM_WEARS; i++) {
			if (GET_EQ(this, i))	char_eq[i] = unequip_char(this, i);
			else					char_eq[i] = NULL;
		}

		if(this->RealName())	fprintf(outfile, "Name: %s\n", this->RealName());
		if(GET_PASSWD(this))	fprintf(outfile, "Pass: %s\n", GET_PASSWD(this));
		if(GET_TITLE(this))		fprintf(outfile, "Titl: %s\n", GET_TITLE(this));
		if(SSData(this->general.description)) {
			buf = get_buffer(MAX_STRING_LENGTH);
			strcpy(buf, SSData(this->general.description));
			strip_string(buf);
			fprintf(outfile, "Desc:\n%s~\n", buf);
			release_buffer(buf);
		} 
		if (GET_PROMPT(this))	fprintf(outfile, "Prmp: %s\n", GET_PROMPT(this));
		fprintf(outfile, "Sex : %d\n", GET_SEX(this));
		fprintf(outfile, "Race: %d\n", GET_RACE(this));
		fprintf(outfile, "Levl: %d\n", GET_LEVEL(this));
		fprintf(outfile, "Brth: %ld\n", this->player->time.birth);
		fprintf(outfile, "Plyd: %ld\n", this->player->time.played + (time(0) - this->player->time.logon));
		fprintf(outfile, "Last: %ld\n", this->player->time.logon);
		fprintf(outfile, "Host: %s\n", (this->desc) ? this->desc->host : this->player->host);
		fprintf(outfile, "Hite: %d\n", GET_HEIGHT(this));
		fprintf(outfile, "Wate: %d\n", GET_WEIGHT(this));

		fprintf(outfile, "Id  : %d\n", this->player->idnum);
		if(this->general.act) {
			sprintbits(this->general.act, bits);
			fprintf(outfile, "Act : %s\n", bits);
//			fprintf(outfile, "Act : %d\n", this->general.act);
		}
		if(this->general.affected_by) {
			int new_affected_by = this->general.affected_by;
			new_affected_by &= ~(AFF_GROUP);		// Must mask out some stuff.
			sprintbits(new_affected_by, bits);
			fprintf(outfile, "Aff : %s\n", bits);
		}

		if (!PLR_FLAGGED(this, PLR_LOADROOM)) {
			if (load_room != NOWHERE)
				load_room = world[load_room].number;
		} else
			load_room = this->player->special.load_room;
		
		if(!IS_STAFF(this)) {
			fprintf(outfile, "Skil:\n");
			for(i = 1; i <= MAX_SKILLS; i++) {
				if(GET_SKILL(this, i))
					fprintf(outfile, "%d %d\n", i, GET_SKILL(this, i));
			}
			fprintf(outfile, "0 0\n");
		}
		if(this->player->special.wimp_level)		fprintf(outfile, "Wimp: %d\n", this->player->special.wimp_level);
		if(this->player->special.freeze_level)		fprintf(outfile, "Frez: %d\n", this->player->special.freeze_level);
		if(this->player->special.invis_level)		fprintf(outfile, "Invs: %d\n", this->player->special.invis_level);
		if(load_room != NOWHERE)	fprintf(outfile, "Room: %d\n", load_room);
		if(this->player->special.preferences) {
			sprintbits(this->player->special.preferences, bits);
			fprintf(outfile, "Pref: %s\n", bits);
		}
		if(this->player->special.channels) {
			sprintbits(this->player->special.channels, bits);
			fprintf(outfile, "Chan: %s\n", bits);
		}
		if(this->player->special.bad_pws)			fprintf(outfile, "Badp: %d\n", this->player->special.bad_pws);
		if(!IS_STAFF(this)) {
			if(this->general.conditions[0])			fprintf(outfile, "Hung: %d\n", this->general.conditions[0]);
			if(this->general.conditions[1])			fprintf(outfile, "Thir: %d\n", this->general.conditions[1]);
			if(this->general.conditions[2])			fprintf(outfile, "Drnk: %d\n", this->general.conditions[2]);
		}
		if(this->player->special.pracs)				fprintf(outfile, "Lern: %d\n", this->player->special.pracs);
		if(real_clan(this->general.misc.clannum) != -1)	fprintf(outfile, "Clan: %d %d\n", this->general.misc.clannum, this->player->special.clanrank);
		fprintf(outfile,   "Kill: %d %d %d %d\n",	this->player->special.PKills, this->player->special.MKills,
				this->player->special.PDeaths, this->player->special.MDeaths);

		fprintf(outfile, "Str : %d\n", this->real_abils.str);
		fprintf(outfile, "Int : %d\n", this->real_abils.intel);
		fprintf(outfile, "Per : %d\n", this->real_abils.per);
		fprintf(outfile, "Agi : %d\n", this->real_abils.agi);
		fprintf(outfile, "Con : %d\n", this->real_abils.con);

		fprintf(outfile, "Hit : %d/%d\n", this->points.hit, this->points.max_hit);
		fprintf(outfile, "Move: %d/%d\n", this->points.move, this->points.max_move);
		fprintf(outfile, "Ac  : %d\n", this->points.armor);
		fprintf(outfile, "MP  : %d\n", this->points.mp);
		fprintf(outfile, "Hrol: %d\n", this->points.hitroll);
		fprintf(outfile, "Drol: %d\n", this->points.damroll);
		
		if (this->player->poofin)	fprintf(outfile, "Pfin: %s\n", this->player->poofin);
		if (this->player->poofout)	fprintf(outfile, "Pfou: %s\n", this->player->poofout);

		if (this->player->special.imc.listen) {
			fprintf(outfile, "ILsn: %s\n", this->player->special.imc.listen);
		}
		
		if (this->player->special.imc.deaf) {
			sprintbits(this->player->special.imc.deaf, bits);
			fprintf(outfile, "IDef: %s\n", bits);
		}
		
		if (this->player->special.imc.allow) {
			sprintbits(this->player->special.imc.allow, bits);
			fprintf(outfile, "IAlw: %s\n", bits);
		}
		
		if (this->player->special.imc.deny) {
			sprintbits(this->player->special.imc.deny, bits);
			fprintf(outfile, "IDny: %s\n", bits);
		}
		
		if(this->player->special.staff_flags) {
			sprintbits(this->player->special.staff_flags, bits);
			fprintf(outfile, "Stf : %s\n", bits);
		}

		//	save aliases
//		for (a = this->player->aliases; a; a = a->next) {
		START_ITER(alias_iter, a, Alias *, this->player->aliases) {
			fprintf(outfile, "Alis: %s %s\n", a->command, a->replacement);
		} END_ITER(alias_iter);
//		}

/* affected_type */
		/*
		 * remove the affections so that the raw values are stored; otherwise the
		 * effects are doubled when the char logs back in.
		 */
//		while (ch->affected)
//			affect_remove(ch, ch->affected);

		fprintf(outfile, "Affs:\n");
//		for(i = 0, aff = this->affected; i < MAX_AFFECT && aff; i++, aff = aff->next) {

		START_ITER(affect_iter, aff, Affect *, this->affected) {
			if(aff->type)
				fprintf(outfile, "%d %d %d %d %d\n", aff->type, aff->event ? aff->event->Time() : 0,
						aff->modifier, aff->location, (int)aff->bitvector);
		} END_ITER(affect_iter);
		fprintf(outfile, "0 0 0 0 0\n");
		  /* add spell and eq affections back in now */
//  for (i = 0; i < MAX_AFFECT; i++) {
  //  if (st->affected[i].type)
 //     affect_to_char(ch, &st->affected[i]);
//  }

//		Re-equip
		for (i = 0; i < NUM_WEARS; i++) {
			if (char_eq[i])
				equip_char(this, char_eq[i], i);
		}

		/* restore our points to previous level */
		GET_HIT(this) = hit;
//		GET_MANA(this) = mana;
		GET_MOVE(this) = move;

		fclose(outfile);
	}
	release_buffer(outname);
	release_buffer(bits);
}


  /* Load a char, TRUE if loaded, FALSE if not */
SInt32 CharData::load(char *name) {
	int id, num = 0, num2 = 0, num3 = 0, num4 = 0, num5 = 0, i;
	FILE *fl;
	char *filename;
	char *buf, *line, *tag;
	char *buf2 = NULL;
	Affect *aff;
	char *cmd, *repl;
	Alias *a;
	RNum	clan;

	if (!this->player || (this->player == &dummy_player))
		this->player = new PlayerData;
	
	if((id = find_name(name)) < 0)		return (-1);
	else {
		filename = get_buffer(40);
		buf = get_buffer(128);
		strcpy(buf, player_table[id].name);
		for (i = 0; (buf[i] = LOWER(buf[i])); i++);
		
		sprintf(filename, PLR_PREFIX "%c" SEPERATOR "%s", *buf, buf);
		
		if(!(fl = fopen(filename, "r"))) {
			mudlogf( NRM, LVL_STAFF, TRUE,  "SYSERR: Couldn't open player file %s", filename);
			release_buffer(filename);
			release_buffer(buf);
			return (-1);
		}
		line = get_buffer(MAX_INPUT_LENGTH+1);
		tag = get_buffer(6);

		while(get_line(fl, line)) {
			tag_argument(line, tag);
			num = atoi(line);
			/* A */
			if (!strcmp(tag, "Ac  "))		GET_AC(this) = num;
			else if (!strcmp(tag, "Act "))	this->general.act = asciiflag_conv(line);
			else if (!strcmp(tag, "Aff "))	this->general.affected_by = asciiflag_conv(line);
			else if (!strcmp(tag, "Affs")) {
				i = 0;
				do {
					get_line(fl, line);
					sscanf(line, "%d %d %d %d %d", &num, &num2, &num3, &num4, &num5);
					if (num != 0) {
						aff = new Affect((Affect::AffectType)num, num3, (AffectLoc)num4, num5);
						if (aff->type)	aff->ToChar(this, num2);
						else			delete aff;
					}
				} while (num != 0 && i < MAX_AFFECT);
			} else if (!strcmp(tag, "Agi "))	this->real_abils.agi = num;
			else if (!strcmp(tag, "Alis")) {
				cmd = get_buffer(MAX_INPUT_LENGTH);
				repl = get_buffer(MAX_INPUT_LENGTH);
				half_chop(line, cmd, repl);
				a = new Alias(cmd, repl);
				this->player->aliases.Add(a);
				release_buffer(cmd);
				release_buffer(repl);
			}
			/* B */
			else if (!strcmp(tag, "Badp"))	this->player->special.bad_pws = num;
			else if (!strcmp(tag, "Brth"))	this->player->time.birth = num;
			/* C */
			else if (!strcmp(tag, "Chan"))	this->player->special.channels = asciiflag_conv(line);
			else if (!strcmp(tag, "Clan")) {
				sscanf(line, "%d %d", &num, &num2);
				if (((clan = real_clan(num)) != -1) && (clan_index[clan].IsMember(GET_IDNUM(this)))) {
					this->general.misc.clannum = num;
					this->player->special.clanrank = num2;
				}
			} else if (!strcmp(tag, "Con "))	this->real_abils.con = num;
			/* D */
			else if (!strcmp(tag, "Desc"))	this->general.description = SSfread(fl, line, filename);
			else if (!strcmp(tag, "Drnk"))	this->general.conditions[2] = num;
			else if (!strcmp(tag, "Drol"))	this->points.damroll = num;
			/* F */
			else if (!strcmp(tag, "Frez"))	this->player->special.freeze_level = num;
			/* H */
			else if (!strcmp(tag, "Hit ")) {
				sscanf(line, "%d/%d", &num, &num2);
				this->points.hit = num;
				this->points.max_hit = num2;
			} else if (!strcmp(tag, "Hite"))	GET_HEIGHT(this) = num;
			else if (!strcmp(tag, "Host"))	this->player->host = str_dup(line);
			else if (!strcmp(tag, "Hrol"))	this->points.hitroll = num;
			else if (!strcmp(tag, "Hung"))	this->general.conditions[0] = num;
			/* I */
			else if (!strcmp(tag, "Id  "))	this->player->idnum = num;
			else if (!strcmp(tag, "ILsn"))	this->player->special.imc.listen = str_dup(line);
			else if (!strcmp(tag, "IAlw"))	this->player->special.imc.allow = asciiflag_conv(line);
			else if (!strcmp(tag, "IDef"))	this->player->special.imc.deaf = asciiflag_conv(line);
			else if (!strcmp(tag, "IDny"))	this->player->special.imc.deny = asciiflag_conv(line);
			else if (!strcmp(tag, "Int "))	this->real_abils.intel = num;
			else if (!strcmp(tag, "Invs"))	this->player->special.invis_level = num;
			/* K */
			else if (!strcmp(tag, "Kill")) {
				sscanf(line, "%d %d %d %d", &num, &num2, &num3, &num4);
				this->player->special.PKills = num;
				this->player->special.MKills = num2;
				this->player->special.PDeaths = num3;
				this->player->special.MDeaths = num4;
			}
			/* L */
			else if (!strcmp(tag, "Last"))	this->player->time.logon = num;
			else if (!strcmp(tag, "Lern"))	this->player->special.pracs = num;
			else if (!strcmp(tag, "Levl"))	GET_LEVEL(this) = num;
			/* M */
			else if (!strcmp(tag, "Move")) {
				sscanf(line, "%d/%d", &num, &num2);
				this->points.move = num;
				this->points.max_move = num2;
			} else if (!strcmp(tag, "MP  "))	this->points.mp = num;
			/* N */
			else if (!strcmp(tag, "Name"))	this->general.name = SSCreate(line);
			/* P */
			else if (!strcmp(tag, "Pass")) {
				strncpy(this->player->passwd, line, MAX_PWD_LENGTH);
				this->player->passwd[MAX_PWD_LENGTH] = '\0';
			} else if (!strcmp(tag, "Per "))	this->real_abils.per = num;
			else if (!strcmp(tag, "Pfin"))	this->player->poofin = str_dup(line);
			else if (!strcmp(tag, "Pfou"))	this->player->poofout = str_dup(line);
			else if (!strcmp(tag, "Plyd"))	this->player->time.played = num;
			else if (!strcmp(tag, "Pref"))	this->player->special.preferences = asciiflag_conv(line);
			else if (!strcmp(tag, "Prmp"))	this->player->prompt = str_dup(line);
			/* R */
			else if (!strcmp(tag, "Race"))	GET_RACE(this) = (Race)num;
			else if (!strcmp(tag, "Room"))	this->player->special.load_room = num;
			/* S */
			else if (!strcmp(tag, "Sex "))	GET_SEX(this) = (Sex)num;
			else if (!strcmp(tag, "Skil")) {
				do {
					get_line(fl, line);
					sscanf(line, "%d %d", &num, &num2);
					if(num != 0)
						GET_SKILL(this, num) = num2;
				} while (num != 0);
			} else if (!strcmp(tag, "Stf "))	this->player->special.staff_flags = asciiflag_conv(line);
			else if (!strcmp(tag, "Str "))	this->real_abils.str = num;
			/* T */
			else if (!strcmp(tag, "Thir"))	this->general.conditions[1] = num;
			else if (!strcmp(tag, "Titl"))	this->general.title = SSCreate(line);
			/* W */
			else if (!strcmp(tag, "Wate"))	GET_WEIGHT(this) = num;
			else if (!strcmp(tag, "Wimp"))	this->player->special.wimp_level = num;
			/* Default */
			else	log("SYSERR: Unknown tag %s in pfile %s", tag, name);
		}
	}
	fclose(fl);
	
	// Final data initialization
	this->aff_abils = this->real_abils;	// Copy abil scores
	this->general.misc.carry_weight = 0;
	this->general.misc.carry_items = 0;
	
	/* initialization for imms */
	if(IS_STAFF(this)) {
		for(i = 1; i <= MAX_SKILLS; i++)
			SET_SKILL(this, i, 100);
		this->general.conditions[0] = -1;
		this->general.conditions[1] = -1;
		this->general.conditions[2] = -1;
	}
	/*
	 * If you're not poisioned and you've been away for more than an hour of
	 * real time, we'll set your HMV back to full
	 */
	if (!AFF_FLAGGED(this, AFF_POISON) && ((time(0) - this->player->time.logon) >= SECS_PER_REAL_HOUR)) {
		GET_HIT(this) = GET_MAX_HIT(this);
		GET_MOVE(this) = GET_MAX_MOVE(this);
	}
	
	GET_ID(this) = GET_IDNUM(this);

	release_buffer(buf);
	release_buffer(line);
	release_buffer(filename);
	release_buffer(tag);
	
	if (!SSData(this->general.name) || (GET_LEVEL(this) < 1))
		return (-1);
	
	if (IS_NPC(this))
		this->general.act = 0;
	
	return (id);
}


/* Called when a character that follows/is followed dies */
void CharData::die_follower(void) {
	CharData *		follower;

	if (this->master)
		this->stop_follower();

	START_ITER(iter, follower, CharData *, this->followers) {
		follower->stop_follower();
	} END_ITER(iter);
}


/* Called when stop following persons, or stopping charm */
/* This will NOT do if a character quits/dies!!          */
void CharData::stop_follower(void) {
	if (!this->master) {
		core_dump();
		return;
	}

	if (AFF_FLAGGED(this, AFF_CHARM)) {
		act("You realize that $N is a jerk!", FALSE, this, 0, this->master, TO_CHAR);
		act("$n realizes that $N is a jerk!", FALSE, this, 0, this->master, TO_NOTVICT);
		act("$n hates your guts!", FALSE, this, 0, this->master, TO_VICT);
		if (Affect::AffectedBy(this, Affect::Charm))
			Affect::FromChar(this, Affect::Charm);
	} else {
		act("You stop following $N.", FALSE, this, 0, this->master, TO_CHAR);
		act("$n stops following $N.", TRUE, this, 0, this->master, TO_NOTVICT);
		act("$n stops following you.", TRUE, this, 0, this->master, TO_VICT);
	}

//	if (this->master->followers->follower == this) {	/* Head of follower-list? */
//		k = this->master->followers;
//		this->master->followers = k->next;
//		FREE(k);
//	} else {			/* locate follower who is not head of list */
//		for (k = this->master->followers; k->next->follower != this; k = k->next);
//
//		j = k->next;
//		k->next = j->next;
//		FREE(j);
//	}
	this->master->followers.Remove(this);
	this->master = NULL;
	REMOVE_BIT(AFF_FLAGS(this), AFF_CHARM | AFF_GROUP);
}



#define READ_TITLE(ch, lev) (GET_SEX(ch) != SEX_FEMALE ?   \
	titles[(int)GET_RACE(ch)][lev].title_m :  \
	titles[(int)GET_RACE(ch)][lev].title_f)


void CharData::set_title(char *title) {
	int tempLev;
	if (title == NULL) {
		if (IS_STAFF(this))	tempLev = (GET_LEVEL(this) - LVL_IMMORT) + 11;
		else				tempLev = ((GET_LEVEL(this) - 1) / 10) + 1;

		title = READ_TITLE(this, tempLev);
	}


	if (strlen(title) > MAX_TITLE_LENGTH)
		title[MAX_TITLE_LENGTH] = '\0';

	if (GET_TITLE(this))	SSFree(this->general.title);

	this->general.title = SSCreate(title);
}


void CharData::set_level(UInt8 level) {
	void do_start(CharData *ch);
	int is_altered = FALSE;
	int num_levels = 0;
	char *buf;

	if (IS_NPC(this))
		return;
	
	if (level < 1)
		level = 1;
	if (level == GET_LEVEL(this))
		return;

	if (level < GET_LEVEL(this))
		do_start(this);

	while ((GET_LEVEL(this) < LVL_CODER) && (GET_LEVEL(this) < level)) {
		GET_LEVEL(this) += 1;
		num_levels++;
		advance_level(this);
	}

	if (num_levels == 1)
		send_to_char("You rise a level!\r\n", this);
	else {
		buf = get_buffer(128);
		sprintf(buf, "You rise %d levels!\r\n", num_levels);
		send_to_char(buf, this);
		release_buffer(buf);
	}
	this->set_title(NULL);
	check_autowiz(this);
}


void CharData::update_pos(void) {

	if ((GET_HIT(this) > 0) && (GET_POS(this) > POS_STUNNED))	return;
	else if (GET_HIT(this) > 0) {
		if (GET_POS(this) > POS_STUNNED)			GET_POS(this) = POS_STANDING;
		else if (!AFF_FLAGGED(this, AFF_TRAPPED))	GET_POS(this) = POS_SITTING;
	}
	else if (GET_HIT(this) <= -101)			GET_POS(this) = POS_DEAD;
	else if (GET_HIT(this) <= -75)			GET_POS(this) = POS_MORTALLYW;
	else if (GET_HIT(this) <= -50)			GET_POS(this) = POS_INCAP;
	else									GET_POS(this) = POS_STUNNED;
}


SInt32 start_rooms[NUM_RACES] = {
	1200,		//	Human (Marine)
	1200,		//	Synth (Marine)
	1200,		//	Yautja
	1200,		//	Alien
};

/* Get the start room number */
RNum CharData::StartRoom(void) {
	VNum	load_room;
	RNum	clan;
	
	if ((load_room = GET_LOADROOM(this)) != NOWHERE)
		load_room = real_room(load_room);

	if ((load_room == NOWHERE) && (GET_CLAN(this) != -1) && 
			((clan = real_clan(GET_CLAN(this))) > -1) && (GET_CLAN_LEVEL(this) > CLAN_APPLY))
		load_room = real_room(clan_index[clan].room);
	
	/* If char was saved with NOWHERE, or real_room above failed... */
	if (load_room == NOWHERE)
		load_room = IS_STAFF(this) ? r_immort_start_room : real_room(start_rooms[GET_RACE(this)]);

	if (load_room == NOWHERE)
		load_room = r_mortal_start_room;
	
	if (!PLR_FLAGGED(this, PLR_LOADROOM))
		GET_LOADROOM(this) = NOWHERE;

	if (PLR_FLAGGED(this, PLR_FROZEN))
		load_room = r_frozen_start_room;

	return load_room;
}


Relation CharData::GetRelation(CharData *target) {
	RNum	chClan, tarClan;
	
	if (NO_STAFF_HASSLE(this) || NO_STAFF_HASSLE(target))
		return RELATION_FRIEND;						//	One is a Staff
	if (IS_TRAITOR(this) ^ IS_TRAITOR(target))
		return RELATION_ENEMY;						//	One is a traitor, other is not
	
	chClan = (GET_CLAN_LEVEL(this) >= CLAN_MEMBER) ? real_clan(GET_CLAN(this)) : -1;
	tarClan = (GET_CLAN_LEVEL(target) >= CLAN_MEMBER) ? real_clan(GET_CLAN(target)) : -1;
	
	if ((GET_RACE(this) > RACE_SYNTHETIC) ?
			(GET_RACE(this) != GET_RACE(target)) :
			(GET_RACE(target) > RACE_SYNTHETIC)) {
		if ((chClan != -1) && (chClan == tarClan))
			return RELATION_FRIEND;					//	Seperate Race, Same Clan
		return RELATION_ENEMY;						//	Seperate Race
	}
	if ((chClan != -1) && (tarClan != -1) && (chClan != tarClan))
		return RELATION_NEUTRAL;					//	Both clanned, seperate clans
	return RELATION_FRIEND;							//	Friendly
}


bool CharData::LightOk(void) {
	if (AFF_FLAGGED(this, AFF_BLIND))
		return false;
	
	if (!IS_LIGHT(IN_ROOM(this)) && !AFF_FLAGGED(this, AFF_INFRAVISION) &&
			!(GET_RACE(this) == RACE_ALIEN))
		return false;
	
	return true;
}


bool CharData::InvisOk(CharData *target) {
	if (AFF_FLAGGED(target, AFF_INVISIBLE) && !AFF_FLAGGED(this, AFF_DETECT_INVIS))
		return false;
	
	if (AFF_FLAGGED(target, AFF_HIDE) && !AFF_FLAGGED(this, AFF_SENSE_LIFE))
		return false;
	
	return true;
}


bool CharData::CanSee(CharData *target) {
	if (!target)								return false;
	if (SELF(this, target))						return true;	//	this == target

	if (!PRF_FLAGGED(this, PRF_HOLYLIGHT)) {
		if (MOB_FLAGGED(target, MOB_PROGMOB))	return false;	//	ProgMob check
		if (!this->LightOk())					return false;	//	Enough light to see
		if (!this->InvisOk(target))				return false;	//	Invis check
	}
	
	if (GET_REAL_LEVEL(this) < GET_INVIS_LEV(target))
		return false;		//	Invisibility level
	
	return true;
}


bool CharData::CanSee(ObjData *target) {
	if (!target)
		return false;
	
	if (PRF_FLAGGED(this, PRF_HOLYLIGHT))
		return true;
	
	if (!this->LightOk())
		return false;
	
	if (OBJ_FLAGGED(target, ITEM_INVISIBLE) && !AFF_FLAGGED(this, AFF_DETECT_INVIS))
		return false;
	
	if (target->in_obj && !this->CanSee(target->in_obj))
		return false;
	
	if (target->carried_by && !this->CanSee(target->carried_by))
		return false;
	
	return true;
}


bool CharData::CanUse(ObjData *obj) {
	if (NO_STAFF_HASSLE(this))	return true;
	return !((OBJ_FLAGGED(obj, ITEM_ANTI_HUMAN) && IS_HUMAN(this)) ||
			(OBJ_FLAGGED(obj, ITEM_ANTI_SYNTHETIC) && IS_SYNTHETIC(this)) ||
			(OBJ_FLAGGED(obj, ITEM_ANTI_PREDATOR) && IS_PREDATOR(this)) ||
			(!OBJ_FLAGGED(obj, ITEM_ALIEN) && IS_ALIEN(this)));
}


RNum real_mobile(VNum vnum) {
	int bot, top, mid, nr, found = NOWHERE;

	bot = 0;
	top = top_of_mobt;

	//	First binary search
	for (;;) {
		mid = (bot + top) / 2;

		if ((mob_index + mid)->vnum == vnum)
			return mid;
		if (bot >= top)
			break;
		if ((mob_index + mid)->vnum > vnum)
			top = mid - 1;
		else
			bot = mid + 1;
	}

	//	Then, if not found, linear
	for(nr = 0; nr <= top_of_mobt; nr++) {
		if(mob_index[nr].vnum == vnum)
			return nr;
	}
	return -1;
}


/* create a new mobile from a prototype */
CharData *read_mobile(VNum nr, UInt8 type) {
	static UInt32 mob_tick_count = 0;
	int i, t;
	CharData *mob;
	TrigData *trig;
	struct int_list *trn;

	if (type == VIRTUAL) {
		if ((i = real_mobile(nr)) < 0) {
			log("Mobile (V) %d does not exist in database.", nr);
			return NULL;
		}
	} else
		i = nr;

	mob = new CharData((CharData *)mob_index[i].proto);
	
	if (!mob->points.max_hit) {
		mob->points.max_hit = dice(mob->points.hit, mob->points.mana) + mob->points.move;
	} else
		mob->points.max_hit = Number(mob->points.hit, mob->points.mana);

	mob->points.hit = mob->points.max_hit;
	mob->points.mana = mob->points.max_mana;
	mob->points.move = mob->points.max_move;

	mob->mob->tick = mob_tick_count++;

	if (mob_tick_count == TICK_WRAP_COUNT)
		mob_tick_count=0;
	
//	GET_ID(mob) = max_id++;
	mob->to_world();

	mob_index[i].number++;

	/* add triggers */
	for (trn = mob_index[i].triggers; trn; trn = trn->next) {
		t = real_trigger(trn->i);
		if (t == -1)
			log("SYSERR: Invalid trigger %d assigned to mob %d", trn->i, mob_index[i].vnum);
		else {
			trig = read_trigger(t, REAL);
			if (!SCRIPT(mob))
				SCRIPT(mob) = new ScriptData;
			add_trigger(SCRIPT(mob), trig); //, 0);
		}
	}

	return mob;
}


void parse_mobile(FILE * mob_f, VNum nr, char *filename) {
	static int i = 0;
	int j, t[10], num;
	char	*line, *tag,
			*buf2 = get_buffer(128),
			*tmpptr,
			*triggers,
			*s;
	struct int_list *tr_list;
	CharData *mob;
	
	mob_index[i].vnum = nr;
	mob_index[i].number = 0;
	mob_index[i].func = NULL;

	mob = new CharData();
	mob->mob = new MobData();
	
	mob_index[i].proto = mob;
	
	sprintf(buf2, "mob vnum %d", nr);

	line = get_buffer(MAX_INPUT_LENGTH + 1);
	tag = get_buffer(6);
	
	//	Initializations
	mob->real_abils.str = 60;
	mob->real_abils.intel = 60;
	mob->real_abils.per = 60;
	mob->real_abils.agi = 60;
	mob->real_abils.con = 60;
	mob->general.weight = 72;
	mob->general.height = 198;
	
	for (j = 0; j < 3; j++)
		GET_COND(mob, j) = -1;
	
	mob->general.title = NULL;
	
	while (1) {
		if (!get_line(mob_f, line)) {
			log("SYSERR: Format error in mob #%d: Unexpected EOF", nr);
			tar_restore_file(filename);
			exit(1);
		} else if (*line == '#') {
			log("SYSERR: Format error in mob #%d: Unterminated MOB", nr);
			tar_restore_file(filename);
			exit(1);
		} else if (*line == '$')
			break;
		
		tag_argument(line, tag);
		num = atoi(line);
		
		if		(!strcmp(tag, "Name"))		mob->general.name = SSCreate(line);
		else if (!strcmp(tag, "Shrt"))	{
			mob->general.shortDesc = SSCreate(line);
			if (mob->general.shortDesc && *(tmpptr = SSData(mob->general.shortDesc)))
				if (!str_cmp(fname(tmpptr), "a") || !str_cmp(fname(tmpptr), "an") || !str_cmp(fname(tmpptr), "the"))
					*tmpptr = LOWER(*tmpptr);
		}
		else if (!strcmp(tag, "Long"))	mob->general.longDesc = SSfread(mob_f, buf2, filename);
		else if (!strcmp(tag, "Desc"))	mob->general.description = SSfread(mob_f, buf2, filename);
		else if (!strcmp(tag, "Race"))	mob->general.race = (Race)num;
		else if (!strcmp(tag, "Levl"))	mob->general.level = num;
		else if (!strcmp(tag, "Hitr"))	mob->points.hitroll = num;
		else if (!strcmp(tag, "Armr"))	mob->points.armor = num;
		else if (!strcmp(tag, "HP  ")) {
			sscanf(line, "%dd%d+%d", t, t + 1, t + 2);
			mob->points.hit = t[0];
			mob->points.mana = t[1];
			mob->points.move = t[2];
		}
		else if (!strcmp(tag, "Dmg ")) {
			sscanf(line, "%dd%d+%d", t, t + 1, t + 2);
			mob->mob->damage.number = t[0];
			mob->mob->damage.size = t[1];
			mob->points.damroll = t[2];
		}
		else if (!strcmp(tag, "Pos "))	GET_POS(mob) = (Position)num;
		else if (!strcmp(tag, "DPos"))	GET_DEFAULT_POS(mob) = (Position)num;
		else if (!strcmp(tag, "Sex "))	GET_SEX(mob) = (Sex)num;
		else if (!strcmp(tag, "Act "))	MOB_FLAGS(mob) = asciiflag_conv(line) & ~MOB_SPEC;
		else if (!strcmp(tag, "Aff "))	AFF_FLAGS(mob) = asciiflag_conv(line);
		else if (!strcmp(tag, "Attk"))	mob->mob->attack_type = num;
		else if (!strcmp(tag, "Str "))	mob->real_abils.str = num;
		else if (!strcmp(tag, "Agi "))	mob->real_abils.agi = num;
		else if (!strcmp(tag, "Int "))	mob->real_abils.intel = num;
		else if (!strcmp(tag, "Per "))	mob->real_abils.per = num;
		else if (!strcmp(tag, "Con "))	mob->real_abils.con = num;
		else if (!strcmp(tag, "Dodg"))	mob->mob->dodge = num;
		else if (!strcmp(tag, "Clan"))	mob->general.misc.clannum = num;
		else if (!strcmp(tag, "Hate")) {
			char *sex = get_buffer(MAX_INPUT_LENGTH);
			char *race = get_buffer(MAX_INPUT_LENGTH);
			sscanf(line, "%s %s %d", sex, race, t);
			mob->mob->hates = new Opinion;
			mob->mob->hates->sex = asciiflag_conv(sex);
			mob->mob->hates->race = asciiflag_conv(race);
			mob->mob->hates->vnum = t[0];
			release_buffer(sex);
			release_buffer(race);
		}
		else if (!strcmp(tag, "Fear")) {
			char *sex = get_buffer(MAX_INPUT_LENGTH);
			char *race = get_buffer(MAX_INPUT_LENGTH);
			sscanf(line, "%s %s %d", sex, race, t);
			mob->mob->fears = new Opinion;
			mob->mob->fears->sex = asciiflag_conv(sex);
			mob->mob->fears->race = asciiflag_conv(race);
			mob->mob->fears->vnum = t[0];
			release_buffer(sex);
			release_buffer(race);
		}
		else if (!strcmp(tag, "Trig"))	{
			sprintf(buf2, "mob vnum %d, section Trig\n", nr);
			if (mob_index[i].triggers) {
				log("SYSERR: More than one list of triggers found on mob, %s", buf2);
				continue;
			}
			triggers = fread_string(mob_f, buf2, filename);
			for (s = strtok(triggers, " \r\n"); s; s = strtok(NULL, " \r\n")) {
				CREATE(tr_list, struct int_list, 1);
				tr_list->i = atoi(s);
				tr_list->next = mob_index[i].triggers;
				mob_index[i].triggers = tr_list;
			}
			FREE(triggers);
		}
		else if (!strcmp(tag, "Prog"))	mprog_read_programs(mob_f, &mob_index[i], filename);
		else if (!strcmp(tag, "DONE"))	break;
		else log("SYSERR: Warning: unrecognized keyword %s in mob #%d", tag, nr);
	}
	if (!mob->general.name)			mob->general.name = SSCreate("error");
	if (!mob->general.shortDesc)	mob->general.shortDesc = SSCreate("an erroneous mob");
	if (!mob->general.longDesc)		mob->general.longDesc = SSCreate("A very buggy mob!\r\n");
	if (!mob->general.description)	mob->general.description = SSCreate("ERROR MOB!!!\r\n");

	SET_BIT(MOB_FLAGS(mob), MOB_ISNPC);	//	Done AFTER loading, because MOB_FLAGS can be replaced

	mob->aff_abils = mob->real_abils;

//	for (j = 0; j < NUM_WEARS; j++)
//		mob->equipment[j] = NULL;

	mob->nr = i;
//	mob->desc = NULL;

	top_of_mobt = i++;
	
	release_buffer(line);
	release_buffer(buf2);
	release_buffer(tag);
}




//	NEW CODE - v3 Mob Format

/*	New MOB format (tentative)

 .- Identifier (optional?)
 |
MOB <#vnum> "<name>" {
	short <short>~
	long <long>~
	desc { <desc>~ }
	race <race>
	sex <sex>
	level <#level>
	position = <pos>, <dpos>;		//	Debatable
	points {
		str <#str>
		int <#int>
		agi <#agi>
		per <#per>
		con <#con>
	}
	clan <#vnum>
	trig <#vnum>					//	Multiple instances allowed
	hp <#numDice>d<#sizeDice>+<#bonus>
	dam <#numDice>d<#sizeDice>+<#damroll>
	hitroll <#hitroll>
	armor <#armor>
	attack <attack>
	dodge <dodge>
	flags { <comma-seperated list> }
	affects { <comma-seperated list> }
	opinion ["fear"|"hate"] {
		race { <comma-seperated list> }
		sex { <comma-seperated list> }
		vnum <#vnum>
	}
	prog {
<mprog parse-out>
	}
}


*/
