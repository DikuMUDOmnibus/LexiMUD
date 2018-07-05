/*************************************************************************
*   File: rooms.h                    Part of Aliens vs Predator: The MUD *
*  Usage: Header file for room data                                      *
*************************************************************************/

#ifndef __ROOMS_H__
#define __ROOMS_H__

#include "types.h"
#include "room.defs.h"
#include "internal.defs.h"
#include "stl.llist.h"

/*  Room Related Structures  */

class RoomFuncs {
public:
	static void			ParseRoom(FILE * fl, int virtual_nr, char *filename);
	static void			SetupDir(FILE * fl, int room, int dir, char *filename);
};

class REdit {
public:
	static void			parse(DescriptorData * d, char *arg);
	static void			setup_new(DescriptorData *d);
	static void			setup_existing(DescriptorData *d, RNum real_num);
	static void			save_to_disk(int zone_num);
	static void			save_internally(DescriptorData *d);
	static void			free_room(RoomData *room);
	static void			free_world_room(RoomData *room);
	static void			disp_extradesc_menu(DescriptorData * d);
	static void			disp_exit_menu(DescriptorData * d);
	static void			disp_exit_flag_menu(DescriptorData * d);
	static void			disp_flag_menu(DescriptorData * d);
	static void			disp_sector_menu(DescriptorData * d);
	static void			disp_menu(DescriptorData * d);
};

//	Room Exits
class RoomDirection {
public:
	friend class RoomFuncs;
	friend class REdit;
	
						RoomDirection(void);
						RoomDirection(RoomDirection *dir);
						~RoomDirection(void);
	
	inline const char *	GetKeyword(const char *def = NULL) {
							return (this->keyword ? this->keyword : def);
						}
	inline const char *	GetDesc(const char *def = NULL) {
							return (this->general_description ? this->general_description : def);
						}
	
//	SString *			general_description;	//	When look DIR.
//	SString *			keyword;				//	for open/close
//	char *				general_description;	//	When look DIR.
//	char *				keyword;				//	for open/close

	Flags				exit_info;				//	Exit info
	RNum				key;					//	Key's number (-1 for no key)
	RNum				to_room;				//	Where direction leads (NOWHERE)
//protected:
	char *				general_description;
	char *				keyword;
};


//	Class for Rooms
class RoomData {
public:
	friend class RoomFuncs;
	friend class REdit;
	
						RoomData(void);
						~RoomData(void);
	
	inline const char *	GetName(const char *def = NULL) {
							return (this->name ? this->name : def);
						}
	inline const char *	GetDesc(const char *def = NULL) {
							return (this->description ? this->description : def);
						}

	SInt32				Send(const char *messg, ...) __attribute__ ((format (printf, 2, 3)));
		
	VNum				number;					//	Rooms number (vnum)
    SInt32				id;
	ExtraDesc *			ex_description;			//	for examine/look
	
	UInt16				zone;					//	Room zone (for resetting)
	UInt8				sector_type;			//	sector type (move/hide)
	Flags				flags;					//	DEATH,DARK ... etc
	
	RoomDirection *		dir_option[NUM_OF_DIRS];	//	Directions

	UInt8				light;					//	Number of lightsources in room
	SPECIAL(*func);

	LList<ObjData *>	contents;				//	List of items in room
	LList<CharData *>	people;					//	List of characters in room
	
    ScriptData *		script;					//	Script data
//protected:
	char *				name;
	char *				description;
};


//	Global data from rooms.cp
extern RoomData *	world;
extern UInt32		top_of_world;

extern int sunlight;

//	Global data pertaining to rooms
extern RNum		r_mortal_start_room;
extern RNum		r_immort_start_room;
extern RNum		r_frozen_start_room;

extern VNum		mortal_start_room;
extern VNum		immort_start_room;
extern VNum		frozen_start_room;

//	Public functions in rooms.cp
RNum real_room(VNum vnum);
void check_start_rooms(void);
void renum_world(void);


#endif