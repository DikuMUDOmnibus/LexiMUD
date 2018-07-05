/*************************************************************************
*   File: affects.h                  Part of Aliens vs Predator: The MUD *
*  Usage: Header for affects                                             *
*************************************************************************/

#ifndef __AFFECTS_H__
#define __AFFECTS_H__

#include "types.h"

#define NUM_AFFECTS	5

//	Modifier constants used with obj affects ('A' fields)
enum AffectLoc {
	APPLY_NONE = 0,			//	No effect
	APPLY_STR,				//	Apply to strength
	APPLY_DEX,				//	Apply to dexterity
	APPLY_INT,				//	Apply to constitution
	APPLY_PER,				//	Apply to wisdom
	APPLY_CON,				//	Apply to constitution

	APPLY_AGE = 9,			//	Apply to age
	APPLY_WEIGHT,			//	Apply to weight
	APPLY_HEIGHT,			//	Apply to height
	APPLY_HIT,				//	Apply to max hit points
	APPLY_MOVE,				//	Apply to max move points

	APPLY_AC = 16,			//	Apply to Armor Class
	APPLY_HITROLL,			//	Apply to hitroll
	APPLY_DAMROLL			//	Apply to damage roll
};


class Affect {
public:
	enum AffectType { None, Blind, Charm, Sleep, Poison, Sneak };	//	0-5
	
					Affect(AffectType type, SInt32 modifier, AffectLoc location, Flags bitvector);
					Affect(Affect *aff, CharData *ch);
					~Affect(void);

	void			Remove(CharData *ch);
	void			ToChar(CharData * ch, UInt32 duration);
	void			Join(CharData * ch, UInt32 duration, bool add_dur, bool avg_dur, bool add_mod, bool avg_mod);

	static void		FromChar(CharData * ch, AffectType type);
	static void		Modify(CharData * ch, SInt8 loc, SInt8 mod, Flags bitv, bool add);
	static bool		AffectedBy(CharData * ch, AffectType type);

	AffectType		type;			//	The type of spell that caused this
	AffectLoc		location;		//	Tells which ability to change(APPLY_XXX)
	SInt32			modifier;		//	This is added to apropriate ability
	Flags			bitvector;		//	Tells which bits to set (AFF_XXX)
	
	Event *	event;
};


//void	affects(int level, CharData * ch, CharData * victim, int affectnum /*, int savetype*/);

#endif