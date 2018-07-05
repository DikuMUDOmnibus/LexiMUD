/*************************************************************************
*   File: objects.h                  Part of Aliens vs Predator: The MUD *
*  Usage: Header file for objects                                        *
*************************************************************************/


#ifndef	__OBJECTS_H__
#define __OBJECTS_H__

#include "types.h"
#include "index.h"
#include "object.defs.h"
#include "stl.llist.h"


struct ObjAffectedType {
	UInt8	location;					/* Which ability to change (APPLY_XXX)	*/
	SInt8	modifier;					/* How much it changes by				*/
};

//	For Guns
struct GunData {
						GunData(void);
						~GunData(void);
						
	Type				attack;					// Attack type
	UInt8				rate;					// Rate of attack
	UInt8				range;					// Range of gun
	Dice				dice;
	struct {
		Type			type;					// Type of ammo used
		VNum			vnum;					// VNUM of ammo currently loaded
		UInt32			amount;					// Amount of ammo currently loaded
	} ammo;
};


class ObjData {
public:
						ObjData(void);
						ObjData(ObjData *proto);
						~ObjData(void);
	
	void				extract(void);
	void				to_char(CharData *ch);
	void				from_char(void);
	void				to_room(RNum room);
	void				from_room(void);
	void				to_obj(ObjData *obj_to);
	void				from_obj(void);
	void				unequip(void);
	void				equip(CharData *ch);
	
	void				update(UInt32 use);
	
	bool				load(FILE *fl, char *filename);
	void				save(FILE *fl, SInt32 location);
	
	RNum				Room(void);
	
	SInt32				TotalValue(void);
	
//	UNIMPLEMENTED:	Replace bool same_obj(ObjData *, ObjData *) with this
//	bool				operator==(const ObjData &) const;
	
	RNum				number;					// Where in data-base
	RNum				in_room;				// In what room -1 when contained/carried
	SInt32				id;
	
	bool				purged;

	SString *			name;					// Title of object: get, etc
	SString *			description;			// When in room
	SString *			shortDesc;				// when worn/carry/in cont.
	SString *			actionDesc;				// What to write when used
	ExtraDesc *			exDesc;					// extra descriptions

	CharData *			carried_by;				// Carried by - NULL in room/container
	CharData *			worn_by;				// Worn by?
	SInt8				worn_on;				// Worn where?
	ObjData *			in_obj;					// In what object NULL when none
	LList<ObjData *>	contains;				// Contains objects
	
	ScriptData *		script;					// script info for the object

	LList<Event *>		events;					//	List of events

	//	Object information
	SInt32				value[8];				// Values of the item
	UInt32				cost;					// Cost of the item
	SInt32				weight;					// Weight of the item
	UInt32				timer;					// Timer
	Type				type;					// Type of item
	Flags				wear;					// Wear flags
	Flags				extra;					// Extra flags (glow, hum, etc)
	Flags				affects;				// Affect flags
	
	//	For Weapons
	UInt32				speed;					// Speed of weapon
	
	struct GunData *	gun;					//	Ranged weapon data
	
	ObjAffectedType		affect[MAX_OBJ_AFFECT];  /* affects */
	
protected:
	void				Init(void);
};

extern LList<ObjData *>	object_list;
extern LList<ObjData *>	purged_objs;
extern IndexData *	obj_index;
extern UInt32		top_of_objt;

RNum			real_object(VNum vnum);
ObjData *		read_object(VNum nr, UInt8 type);
#define REAL	0
#define VIRTUAL	1
char *parse_object(FILE * obj_f, VNum nr, char *filename);

void equip_char(CharData * ch, ObjData * obj, UInt8 pos);
ObjData *unequip_char(CharData * ch, UInt8 pos);

ObjData *create_obj(void);

#endif	// __OBJECTS_H__
