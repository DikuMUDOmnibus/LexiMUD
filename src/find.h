/**************************************************************************
*  File: find.h                                                           *
*  Usage: contains prototypes of functions for finding mobs and objs      *
*         in lists                                                        *
*                                                                         *
**************************************************************************/

#include "types.h"
#include "stl.llist.h"

ObjData *	get_obj(const char *name);
ObjData *	get_obj_num(VNum nr);
ObjData *	get_obj_vis(CharData *ch, const char *name);
ObjData *	get_obj_in_list(const char *name, LList<ObjData *> &list);
ObjData *	get_obj_in_list_vis(CharData *ch, const char *name, LList<ObjData *> &list);
ObjData *	get_obj_in_list_num(int num, LList<ObjData *> &list);
ObjData *	get_obj_in_list_type(int type, LList<ObjData *> &list);
ObjData *	get_obj_in_list_flagged(int flag, LList<ObjData *> &list);
ObjData *	get_object_in_equip(CharData * ch, const char *arg, ObjData * equipment[], int *j);
ObjData *	get_object_in_equip_vis(CharData * ch, const char *arg, ObjData * equipment[], int *j);
ObjData *	get_obj_by_obj(ObjData *obj, const char *name);
ObjData *	get_obj_by_room(RoomData *room, const char *name);
ObjData *	find_vehicle_by_vnum(int vnum);
ObjData *	find_obj(int n);

int	get_num_chars_on_obj(ObjData * obj);
CharData *	get_char_on_obj(ObjData *obj);

CharData *	get_char(const char *name);
CharData *	get_char_room(const char *name, RNum room);
CharData *	get_char_num(RNum nr);
CharData *	get_char_by_obj(ObjData *obj, const char *name);

/* find if character can see */
CharData *	get_char_room_vis(CharData *ch, const char *name);
CharData *	get_player_vis(CharData *ch, const char *name, bool inroom);
CharData *	get_char_vis(CharData *ch, const char *name);

CharData *find_char(int n);
CharData *get_char_by_room(RoomData *room, const char *name);

RoomData *get_room(const char *name);
RoomData *find_room(int n);

RNum find_the_room(const char *roomstr);

int count_mobs_in_room(RNum num, RNum room);
int count_mobs_in_zone(RNum num, int zone);

/* find all dots */

int	find_all_dots(char *arg);

#define FIND_INDIV	0
#define FIND_ALL	1
#define FIND_ALLDOT	2


/* Generic Find */

int	generic_find(const char *arg, int bitvector, CharData *ch, CharData **tar_ch, ObjData **tar_obj);

#define FIND_CHAR_ROOM     1
#define FIND_CHAR_WORLD    2
#define FIND_OBJ_INV       4
#define FIND_OBJ_ROOM      8
#define FIND_OBJ_WORLD    16
#define FIND_OBJ_EQUIP    32


/*
 * Escape character, which indicates that the name is
 * a unique id number rather than a standard name.
 * Should be nonprintable so players can't enter it.
 */
const char UID_CHAR = '\e';
