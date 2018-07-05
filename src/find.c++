/**************************************************************************
*  File: find.c                                                           *
*  Usage: contains functions for finding mobs and objs in lists           *
**************************************************************************/



#include "structs.h"
#include "utils.h"
#include "find.h"
#include "handler.h"
#include "db.h"
#include "interpreter.h"
#include "buffer.h"


int get_number(char **name) {
	int i;
	char *ppos;
	char *number = get_buffer(MAX_INPUT_LENGTH);

	if ((ppos = strchr(*name, '.'))) {
		*ppos++ = '\0';
		strcpy(number, *name);
		strcpy(*name, ppos);

		for (i = 0; *(number + i); i++)
			if (!isdigit(*(number + i))) {
				release_buffer(number);
				return 0;
			}
		
		i = atoi(number);
		release_buffer(number);
		return i;
	}
	release_buffer(number);
	return 1;
}


CharData *get_char(const char *name) {
	CharData *i = NULL;

	if (*name == UID_CHAR) {
		i = find_char(atoi(name + 1));
		if (i && !GET_INVIS_LEV(i))
			return i;
	} else {
		START_ITER(iter, i, CharData *, character_list) {
			if (silly_isname(name, SSData(i->general.name)) && !GET_INVIS_LEV(i))
				break;
		} END_ITER(iter);
	}
	return i;
}


/* returns the object in the world with name name, or NULL if not found */
ObjData *get_obj(const char *name) {
	ObjData *obj = NULL;
	long id;

	if (*name == UID_CHAR) {
		id = atoi(name + 1);

		START_ITER(iter, obj, ObjData *, object_list) {
			if (id == GET_ID(obj))
				break;
		} END_ITER(iter);
	} else {
		START_ITER(iter, obj, ObjData *, object_list) {
			if (silly_isname(name, SSData(obj->name)))
				break;
		} END_ITER(iter);
	}
	return obj;
}


/* finds room by with name.  returns NULL if not found */
RoomData *get_room(const char *name) {
    int nr;
  
    if (*name == UID_CHAR)								return find_room(atoi(name + 1));
    else if ((nr = real_room(atoi(name))) == NOWHERE)	return NULL;
    else												return &world[nr];
}


CharData *get_player_vis(CharData *ch, const char *name, bool inroom) {
	CharData *i = NULL;
	
	if (*name == UID_CHAR) {
		i = find_char(atoi(name + 1));
		if (i && IS_NPC(i) && (!inroom || IN_ROOM(i) == IN_ROOM(ch)) && ch->CanSee(i))
			return i;
	} else {
		START_ITER(iter, i, CharData *, character_list) {
			if (!IS_NPC(i) && (!inroom || IN_ROOM(i) == IN_ROOM(ch)) && !str_cmp(SSData(i->general.name), name) && ch->CanSee(i))
				break;
		} END_ITER(iter);
	}
	return i;
}


CharData *get_char_vis(CharData * ch, const char *name) {
	CharData *i = NULL;
	int j = 0, number;
	char *tmp;

	/* check the room first */
	if ((i = get_char_room_vis(ch, name)) != NULL)
		return i;

	if (*name == UID_CHAR) {
		i = find_char(atoi(name + 1));
		if (i && ch->CanSee(i))
			return i;
	} else {
		tmp = get_buffer(MAX_INPUT_LENGTH);

		strcpy(tmp, name);
		
		if (!(number = get_number(&tmp))) {
			i = get_player_vis(ch, tmp, false);
		} else {
			START_ITER(iter, i, CharData *, character_list) {
				if (isname(tmp, SSData(i->general.name)) && ch->CanSee(i))
					if (++j == number)
						break;
			} END_ITER(iter);
		}
		release_buffer(tmp);
	}
	return i;
}


/* search a room for a char, and return a pointer if found..  */
CharData *get_char_room(const char *name, RNum room) {
	CharData *i = NULL;
	int j = 0, number;
	char *tmpname, *tmp;

	if (*name == UID_CHAR) {
		i = find_char(atoi(name + 1));
		if (i && (IN_ROOM(i) == room) && !GET_INVIS_LEV(i))
			return i;
	} else {
		tmpname = get_buffer(MAX_INPUT_LENGTH);
		tmp = tmpname;

		strcpy(tmp, name);
		if ((number = get_number(&tmp))) {
			START_ITER(iter, i, CharData *, world[room].people) {
				if (j > number) {
					i = NULL;
					break;
				}
				if (isname(tmp, SSData(i->general.name)))
					if (++j == number)
						break;
			} END_ITER(iter);
		}

		release_buffer(tmpname);
	}
	return i;
}


CharData *get_char_room_vis(CharData * ch, const char *name) {
	CharData *i = NULL;
	int j = 0, number;
	char *tmp;

	if (*name == UID_CHAR) {
		i = find_char(atoi(name + 1));
		
		if (i && (IN_ROOM(ch) == IN_ROOM(i)) && ch->CanSee(i))
			return i;
	} else {
		if (!str_cmp(name, "self") || !str_cmp(name, "me"))
			return ch;

		tmp = get_buffer(MAX_INPUT_LENGTH);
		/* 0.<name> means PC with name */
		strcpy(tmp, name);
		if (!(number = get_number(&tmp))) {
			i = get_player_vis(ch, tmp, true);
		} else {
			START_ITER(iter, i, CharData *, world[IN_ROOM(ch)].people) {
				if (j > number) {
					i = NULL;
					break;
				}
				if (isname(tmp, SSData(i->general.name)) && ch->CanSee(i) && (++j == number))
					break;
			} END_ITER(iter);
		}
		
		release_buffer(tmp);
	}
	return i;
}



CharData *get_char_by_obj(ObjData *obj, const char *name) {
	CharData *ch = NULL;
	
	if (*name == UID_CHAR) {
		ch = find_char(atoi(name + 1));
		if (ch && !GET_INVIS_LEV(ch))
			return ch;
	} else {
		if (obj->carried_by && silly_isname(name, SSData(obj->carried_by->general.name)) &&
				!GET_INVIS_LEV(obj->carried_by))
			ch = obj->carried_by;
		else if (obj->worn_by && silly_isname(name, SSData(obj->worn_by->general.name)) &&
				!GET_INVIS_LEV(obj->worn_by))
			ch = obj->worn_by;
		else {
			START_ITER(iter, ch, CharData *, character_list) {
				if (silly_isname(name, SSData(ch->general.name)) && !GET_INVIS_LEV(ch))
					break;
			} END_ITER(iter);
		}
	}
	return ch;
}


CharData *get_char_by_room(RoomData *room, const char *name) {
	CharData *ch;
	LListIterator<CharData *>	iter;
	
	if (*name == UID_CHAR) {
		ch = find_char(atoi(name + 1));
		if (ch && !GET_INVIS_LEV(ch))
			return ch;
	} else {
		iter.Start(room->people);
		while ((ch = iter.Next())) {
			if (silly_isname(name, SSData(ch->general.name)) && !GET_INVIS_LEV(ch))
				return ch;
		}
		iter.Restart(character_list);
		while ((ch = iter.Next())) {
			if (silly_isname(name, SSData(ch->general.name)) && !GET_INVIS_LEV(ch))
				return ch;
		}
	}
	return NULL;
}


/************************************************************
 * object searching routines
 ************************************************************/

/* search the entire world for an object, and return a pointer  */
ObjData *get_obj_vis(CharData * ch, const char *name) {
	ObjData *i = NULL;
	int j = 0, number, eq = 0;
	long id;
	char *tmp;

	/* scan items carried */
	if ((i = get_obj_in_list_vis(ch, name, ch->carrying)))
		return i;
	
	if ((i = get_object_in_equip_vis(ch, name, ch->equipment, &eq)))
		return i;
	
	/* scan room */
	if ((i = get_obj_in_list_vis(ch, name, world[IN_ROOM(ch)].contents)))
		return i;

	if (*name == UID_CHAR) {
		id = atoi(name + 1);
		START_ITER(iter, i, ObjData *, object_list) {
			if ((id == GET_ID(i)) && ch->CanSee(i))
				break;
		} END_ITER(iter);
	} else {
		tmp = get_buffer(MAX_INPUT_LENGTH);
		strcpy(tmp, name);
		if ((number = get_number(&tmp))) {
			START_ITER(iter, i, ObjData *, object_list) {
				if (j > number) {
					i = NULL;
					break;
				}
				if (ch->CanSee(i) && (!i->carried_by || ch->CanSee(i->carried_by)) &&
						isname(tmp, SSData(i->name)) && (++j == number))
					break;
			} END_ITER(iter);
		}
		release_buffer(tmp);
	}
	return i;
}


ObjData *get_object_in_equip(CharData * ch, const char *arg, ObjData * equipment[], int *j) {
	long id;
	char *tmp, *tmpname;
	int n = 0, number;
	ObjData *obj = NULL;
	
	if (*arg == UID_CHAR) {
		id = atoi(arg + 1);
		for ((*j) = 0; (*j) < NUM_WEARS; (*j)++) {
			if ((obj = equipment[(*j)]) && (id == GET_ID(obj)))
				break;
		}
	} else {
		tmpname = get_buffer(MAX_INPUT_LENGTH);
		tmp = tmpname;
		
		strcpy(tmp, arg);
		
		if ((number = get_number(&tmp))) {
			for ((*j) = 0; (*j) < NUM_WEARS; (*j)++) {
				if ((obj = equipment[(*j)]) && isname(arg, SSData(obj->name)) &&
						(++n == number))
					break;
			}
		}
		release_buffer(tmpname);
	}
	return (*j < NUM_WEARS) ? obj : NULL;
}


ObjData *get_object_in_equip_vis(CharData * ch, const char *arg, ObjData * equipment[], int *j) {
	long id;
	ObjData *obj = NULL;
	char *tmp, *tmpname;
	int n = 0, number;
	
	if (*arg == UID_CHAR) {
		id = atoi(arg + 1);
		for ((*j) = 0; (*j) < NUM_WEARS; (*j)++) {
			if ((obj = equipment[(*j)]) && ch->CanSee(obj) &&
					(id == GET_ID(obj)))
				break;
		}
	} else {
		tmpname = get_buffer(MAX_INPUT_LENGTH);
		tmp = tmpname;
		
		strcpy(tmp, arg);
		
		if ((number = get_number(&tmp))) {			
			for ((*j) = 0; (*j) < NUM_WEARS; (*j)++) {
				if ((obj = equipment[(*j)]) && ch->CanSee(obj) &&
						isname(arg, SSData(obj->name)) && (++n == number))
					break;
			}
		}
		release_buffer(tmpname);
	}
	return (*j < NUM_WEARS) ? obj : NULL;
}


ObjData *get_obj_in_list(const char *name, LList<ObjData *> &list) {
	ObjData *i = NULL;
	long id;
	char *tmp;
	int j = 0, number;
	
	if (*name == UID_CHAR) {
		id = atoi(name +1);
		START_ITER(iter, i, ObjData *, list) {
			if (id == GET_ID(i))
				break;
		} END_ITER(iter);
	} else {
		tmp = get_buffer(MAX_INPUT_LENGTH);
		strcpy(tmp, name);
		
		if ((number = get_number(&tmp))) {
			START_ITER(iter, i, ObjData *, list) {
				if (j > number) {
					i = NULL;
					break;
				}
				if (isname(tmp, SSData(i->name)))
					if (++j == number) {
						release_buffer(tmp);
						return i;
					}
			} END_ITER(iter);
		}
		release_buffer(tmp);
	}
	return i;
}


ObjData *get_obj_in_list_vis(CharData * ch, const char *name, LList<ObjData *> &list) {
	ObjData *i = NULL;
	int j = 0, number;
	char *tmp;
	long id;
	LListIterator<ObjData *>	iter(list);
	
	if (*name == UID_CHAR) {
		id = atoi(name + 1);
		while ((i = iter.Next())) {
			if ((id == GET_ID(i)) && ch->CanSee(i))
				break;
		}
	} else {
		tmp = get_buffer(MAX_INPUT_LENGTH);
		strcpy(tmp, name);
		if ((number = get_number(&tmp))) {
			while ((i = iter.Next())) {
				if (j > number) {
					i = NULL;
					break;
				}
				if (ch->CanSee(i) && isname(tmp, SSData(i->name)))
					if (++j == number)
						break;
			}
		}
		release_buffer(tmp);
	}
	return i;
}


/* Search the given list for an object type, and return a ptr to that obj */
ObjData *get_obj_in_list_type(int type, LList<ObjData *> &list) {
	ObjData * i;
	
	START_ITER(iter, i, ObjData *, list) {
		if (GET_OBJ_TYPE(i) == type)
			break;
	} END_ITER(iter);
	return i;
}


/* Search the given list for an object that has 'flag' set, and return a ptr to that obj */
ObjData *get_obj_in_list_flagged(int flag, LList<ObjData *> &list) {
	ObjData * i = NULL;
	
	START_ITER(iter, i, ObjData *, list) {
		if (OBJ_FLAGGED(i, flag))
			break;
	} END_ITER(iter);
	return i;
}


ObjData *get_obj_by_obj(ObjData *obj, const char *name) {
	ObjData *i = NULL;
	int rm, j = 0;
	long id;
	
	if (!str_cmp(name, "self") || !str_cmp(name, "me"))
		return obj;

	if ((i = get_obj_in_list(name, obj->contains)))
		return i;
	
	if (obj->in_obj) {
		if (*name == UID_CHAR) {
			id = atoi(name + 1);
			if (id == GET_ID(obj->in_obj))
				return obj->in_obj;
		} else if (silly_isname(name, SSData(obj->in_obj->name)))
			return obj->in_obj;
	} else if (obj->worn_by && (i = get_object_in_equip(obj->worn_by, name, obj->worn_by->equipment, &j)))
		return i;
	else if (obj->carried_by && (i = get_obj_in_list(name, obj->carried_by->carrying)))
		return i;
	else if (((rm = obj->Room()) != NOWHERE) && (i = get_obj_in_list(name, world[rm].contents)))
		return i;
		
	if (*name == UID_CHAR) {
		id = atoi(name + 1);
		START_ITER(iter, i, ObjData *, object_list) {
			if (id == GET_ID(i))
				break;
		} END_ITER(iter);
	} else {
		START_ITER(iter, i, ObjData *, object_list) {
			if (silly_isname(name, SSData(i->name)))
				break;
		} END_ITER(iter);
	}
	return i;
}


ObjData *get_obj_by_room(RoomData *room, const char *name) {
	ObjData *obj = NULL;
	long id;
	
	if (*name == UID_CHAR) {
		id = atoi(name + 1);
		START_ITER(room_iter, obj, ObjData *, room->contents) {
			if (id == GET_ID(obj))
				break;
		} END_ITER(room_iter);
		
		if (!obj) {
			START_ITER(obj_iter, obj, ObjData *, object_list) {
				if (id == GET_ID(obj))
					break;
			} END_ITER(obj_iter);
		}
	} else {
		START_ITER(room_iter, obj, ObjData *, room->contents) {
			if (silly_isname(name, SSData(obj->name)))
				break;
		} END_ITER(room_iter);
		
		if (!obj) {
			START_ITER(obj_iter, obj, ObjData *, object_list) {
				if (silly_isname(name, SSData(obj->name)))
					break;
			} END_ITER(obj_iter);
		}
	}
	return obj;
}


/************************************************************
 * search by number routines
 ************************************************************/

/* search all over the world for a char num, and return a pointer if found */
CharData *get_char_num(VNum nr) {
	CharData *i = NULL;

	START_ITER(iter, i, CharData *, character_list) {
		if (GET_MOB_RNUM(i) == nr)
			break;
	} END_ITER(iter);
	
	return i;
}


/* search the entire world for an object number, and return a pointer  */
ObjData *get_obj_num(VNum nr) {
	ObjData *i = NULL;

	START_ITER(iter, i, ObjData *, object_list) {
		if (GET_OBJ_RNUM(i) == nr)
			break;
	} END_ITER(iter);

	return i;
}


/* Search a given list for an object number, and return a ptr to that obj */
ObjData *get_obj_in_list_num(int num, LList<ObjData *> &list) {
	ObjData *i = NULL;

	START_ITER(iter, i, ObjData *, list) {
		if (GET_OBJ_RNUM(i) == num)
			break;
	} END_ITER(iter);

	return i;
}


CharData *find_char(int n) {
	CharData *ch = NULL;
	
	START_ITER(iter, ch, CharData *, character_list) {
		if (n == GET_ID(ch))
			break;
	} END_ITER(iter);

	return ch;
}


ObjData *find_obj(int n) {
	ObjData *i;
	
	START_ITER(iter, i, ObjData *, object_list) {
		if (n == GET_ID(i))
			break;
	} END_ITER(iter);
	return i;
}


/* return room with UID n */
RoomData *find_room(int n) {
    n -= ROOM_ID_BASE;

    if ((n >= 0) && (n <= top_of_world))
		return &world[n];

    return NULL;
}


/* Generic Find, designed to find any object/character                    */
/* Calling :                                                              */
/*  *arg     is the sting containing the string to be searched for.       */
/*           This string doesn't have to be a single word, the routine    */
/*           extracts the next word itself.                               */
/*  bitv..   All those bits that you want to "search through".            */
/*           Bit found will be result of the function                     */
/*  *ch      This is the person that is trying to "find"                  */
/*  **tar_ch Will be NULL if no character was found, otherwise points     */
/* **tar_obj Will be NULL if no object was found, otherwise points        */
/*                                                                        */
/* The routine returns a pointer to the next word in *arg (just like the  */
/* one_argument routine).                                                 */

int generic_find(const char *arg, int bitvector, CharData * ch,
		     CharData ** tar_ch, ObjData ** tar_obj) {
	int i, found;
	char *name = get_buffer(256);

	one_argument(arg, name);

	if (!*name) {
		release_buffer(name);
		return (0);
	}

	*tar_ch = NULL;
	*tar_obj = NULL;

	if (IS_SET(bitvector, FIND_CHAR_ROOM)) {	/* Find person in room */
		if ((*tar_ch = get_char_room_vis(ch, name))) {
			release_buffer(name);
			return (FIND_CHAR_ROOM);
		}
	}
	if (IS_SET(bitvector, FIND_CHAR_WORLD)) {
		if ((*tar_ch = get_char_vis(ch, name))) {
			release_buffer(name);
			return (FIND_CHAR_WORLD);
		}
	}
	if (IS_SET(bitvector, FIND_OBJ_EQUIP)) {
		for (found = FALSE, i = 0; i < NUM_WEARS && !found; i++)
			if (GET_EQ(ch, i) && isname(name, SSData(GET_EQ(ch, i)->name))) {
				*tar_obj = GET_EQ(ch, i);
				found = TRUE;
			}
		if (found) {
			release_buffer(name);
			return (FIND_OBJ_EQUIP);
		}
	}
	if (IS_SET(bitvector, FIND_OBJ_INV)) {
		if ((*tar_obj = get_obj_in_list_vis(ch, name, ch->carrying))) {
			release_buffer(name);
			return (FIND_OBJ_INV);
		}
	}
	if (IS_SET(bitvector, FIND_OBJ_ROOM)) {
		if ((*tar_obj = get_obj_in_list_vis(ch, name, world[IN_ROOM(ch)].contents))) {
			release_buffer(name);
			return (FIND_OBJ_ROOM);
		}
	}
	if (IS_SET(bitvector, FIND_OBJ_WORLD)) {
		if ((*tar_obj = get_obj_vis(ch, name))) {
			release_buffer(name);
			return (FIND_OBJ_WORLD);
		}
	}
	release_buffer(name);
	return (0);
}


/* a function to scan for "all" or "all.x" */
int find_all_dots(char *arg) {
	if (!strcmp(arg, "all"))
		return FIND_ALL;
	else if (!strncmp(arg, "all.", 4)) {
		strcpy(arg, arg + 4);
		return FIND_ALLDOT;
	} else
		return FIND_INDIV;
}



int	get_num_chars_on_obj(ObjData * obj) {
	CharData * ch;
	int temp = 0;
	
	START_ITER(iter, ch, CharData *, world[IN_ROOM(obj)].people) {
		if (obj == ch->sitting_on) {
			if (GET_POS(ch) <= POS_SITTING)		temp++;
			else								ch->sitting_on = NULL;
		}
	} END_ITER(iter);
	return temp;
}


CharData *get_char_on_obj(ObjData *obj) {
	CharData * ch;
	START_ITER(iter, ch, CharData *, world[IN_ROOM(obj)].people) {
		if(obj == ch->sitting_on)
			break;
	} END_ITER(iter);
	return ch;
}


int count_mobs_in_room(RNum num, RNum room) {
	CharData * current;
	int counter;
	
	counter = 0;
	
	if (room < 0)
		return -1;
	START_ITER(iter, current, CharData *, world[room].people) {
		if (IS_NPC(current) && current->nr == num)
			counter++;
	} END_ITER(iter);
	return counter;
}


int count_mobs_in_zone(RNum num, int zone) {
	CharData * current;
	int counter;
	
	counter = 0;
	
	if (zone < 0)
		return -1;

	START_ITER(iter, current, CharData *, character_list) {
		if (IS_NPC(current) && current->nr == num && (IN_ROOM(current) != -1))
			if (world[IN_ROOM(current)].zone == zone)
				counter++;
	} END_ITER(iter);
	
	return counter;
}


/* Find a vehicle by VNUM */
ObjData *find_vehicle_by_vnum(int vnum) {
	ObjData * i = NULL;
	
	START_ITER(iter, i, ObjData *, object_list) {
		if (GET_OBJ_VNUM(i) == vnum)
			break;
	} END_ITER(iter);
	return i;
}


RNum find_the_room(const char *roomstr) {
	int tmp;
	RNum location;
	CharData *target_mob;
	ObjData *target_obj;
	
	if (isdigit(*roomstr) && !strchr(roomstr, '.')) {
		tmp = atoi(roomstr);
		location = real_room(tmp);
	} else if ((target_mob = get_char(roomstr)))
		location = IN_ROOM(target_mob);
	else if ((target_obj = get_obj(roomstr)))
		location = IN_ROOM(target_obj);
	else
		return NOWHERE;
	return location;
}
