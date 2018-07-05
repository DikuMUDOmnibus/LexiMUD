/*************************************************************************
*   File: characters.h               Part of Aliens vs Predator: The MUD *
*  Usage: Header file for characters                                     *
*************************************************************************/


#ifndef	__CHARACTERS_H__
#define __CHARACTERS_H__

#include "types.h"
#include "mobprogs.h"
#include "character.defs.h"
#include "room.defs.h"

//	STL
#include "stl.llist.h"

class	Opinion;
class	Affect;
class	Alias;
class	Path;


/* These data contain information about a players time data */
struct time_data {
	time_t	birth;			/* This represents the characters age                */
	time_t	logon;			/* Time of the last logon (used to calculate played) */
	UInt32	played;			/* This is the total accumulated time played in secs */
};


struct title_type {
	char *	title_m;
	char *	title_f;
};


struct MiscData {
	SInt32		watching;
	
	SInt32		carry_weight;
	UInt8		carry_items;
	VNum		clannum;
};


struct GeneralData {
	SString *	name;
	SString *	shortDesc;
	SString *	longDesc;
	SString *	description;
	SString *	title;
	
	Sex			sex;
	Race		race;
	SInt8		level;
	
	Position	position;
	
	UInt16		weight;
	UInt16		height;
	
	Flags		act;
	Flags		affected_by;
	
	SInt8		conditions[3];
	
	CharData *	fighting;
	SInt32		hunting;

	struct MiscData misc;	
};


struct PlayerSpecialData {
	SInt8			skills[MAX_SKILLS+1];
	UInt32			wimp_level;
	VNum			load_room;
	Flags			preferences;
	Flags			channels;
	Flags			staff_flags;
	UInt8			freeze_level;
	UInt8			invis_level;
	UInt8			bad_pws;
	
	SInt8			clanrank;
	
	UInt32			pracs;
	
	UInt32			PKills;
	UInt32			MKills;
	UInt32			PDeaths;
	UInt32			MDeaths;
	
	struct {
		char *		listen;
		char *		rreply;
		char *		rreply_name;
		Flags		deaf;		//	Deaf-to-channel
		Flags		allow;		//	Allow overrides
		Flags		deny;		//	Deny overrides
	} imc;
};


class PlayerData {
public:
					PlayerData(void);
					~PlayerData(void);
	
	struct PlayerSpecialData special;
	char			passwd[MAX_PWD_LENGTH+1];
	char *			afk;
	char *			prompt;
	char *			host;
	char *			poofin;
	char *			poofout;
	LList<Alias *>	aliases;
	time_data		time;
	
	SInt32			last_tell;
	
	SInt32			idnum;
	SInt32			timer;
};


class MobData {
public:
					MobData(void);
					MobData(MobData *mob);
					~MobData(void);
	
	UInt32			attack_type;
	SInt32			wait_state;
	Position		default_pos;
	UInt8			last_direction;
	UInt8			tick;
	UInt8			dodge;
	struct Dice		damage;
	struct Dice		hitdice;

	Opinion *		hates;
	Opinion *		fears;
//	Opinion			likes;
protected:
	void Init(void);
};


/* Char's abilities. */
struct AbilityData {
	UInt8	str;
	UInt8	intel;
	UInt8	per;
	UInt8	agi;
	UInt8	con;
};


struct PointData {
	SInt16	hit;
	SInt16	max_hit;
	SInt16	move;
	SInt16	max_move;
	SInt16	mana;
	SInt16	max_mana;

	SInt32	mp;

	SInt16	armor;

	SInt16	hitroll;
	SInt16	damroll;
};


/* ================== Structure for player/non-player ===================== */
class CharData {
public:
						CharData(void);
						CharData(CharData *ch);
						~CharData(void);
	
//	Accessor Functions
	const char *		GetName(void) const;
	const char *		RealName(void) const;
	
//	Game Functions
	void				to_world(void);
	void				from_world(void);
	
	void				save(RNum load_room = NOWHERE);
	SInt32				load(char *name);
	
//	void				set_name(char *name);
//	void				set_short(char *short_descr = NULL);
	void				set_title(char *title = NULL);
	void				set_level(UInt8	level);
	
	void				extract(void);
	void				EventCleanup(void);
	
	void				from_room(void);
	void				to_room(RNum room);
	
	void				update_pos(void);
	void				update_objects(void);
	void				AffectTotal(void);
	
	
	void				die_follower(void);
	void				stop_follower(void);

	void				stop_fighting(void);
	void				die(CharData * killer);
	
	Relation			GetRelation(CharData *target);
	RNum				StartRoom(void);

//	Utility Classes, mostly former Macros
	bool				LightOk(void);
	bool				InvisOk(CharData *target);
	bool				CanSee(CharData *target);
	bool				CanSee(ObjData *target);
	
	bool				CanUse(ObjData *obj);
	
	SInt32				Send(const char *messg, ...) __attribute__ ((format (printf, 2, 3)));

	bool				purged;				//	this is set for purging
	SInt32				id;					//	ID-system
	
	SInt32				pfilepos;			//	playerfile pos
	RNum				nr;					//	Mob's rnum
	RNum				in_room;			//	Location (real room number)
	RNum				was_in_room;		//	location for linkdead people
	RNum				temp_was_in;		//	For debugging purposes
	
	struct GeneralData	general;			//	Normal data
	struct AbilityData	real_abils;			//	Unmodified Abilities
	struct AbilityData	aff_abils;			//	Current Abililities
	struct PointData	points;				//	Points
	struct PlayerData *	player;				//	PC specials
	struct MobData *	mob;				//	NPC specials
	
	LList<Affect *>		affected;			//	affects
	
	LList<ObjData *>	carrying;			//	Head of list
	ObjData *equipment[NUM_WEARS];			//	Equipment array
	
	DescriptorData *	desc;				//	NULL for mobiles
	
	LList<CharData *>	followers;			//	List of chars followers
	CharData *			master;				//	Who is char following?
	
	LList<Event *>		events;
	Event *				points_event[3];
	
	Path *				path;

// This is all MPROG stuff here.  Yuck.
	MPROG_ACT_LIST *mpact;
	int mpactnum;
	CharData *mprog_target;
	int mprog_delay;
	int	mpscriptpos;

	ObjData *	sitting_on;
	
	ScriptData *script;					//	script info of NPC
protected:
	void Init(void);
};


extern LList<CharData *>	character_list;
extern LList<CharData *>	purged_chars;
extern LList<CharData *>	combat_list;
extern IndexData *	mob_index;
extern UInt32	top_of_mobt;
extern SInt32	top_of_p_table;
extern struct player_index_element *player_table;	/* index to plr file	 */

CharData *read_mobile(VNum nr, UInt8 type);
#define REAL	0
#define VIRTUAL	1
RNum real_mobile(VNum vnum);
void parse_mobile(FILE * mob_f, VNum nr, char *filename);



struct FallingEvent {
	CharData *	faller;
	SInt32		previous;
};

#define GET_POINTS_EVENT(ch, i) ((ch)->points_event[i])

#endif	// __CHARACTERS_H__
