/*************************************************************************
*   File: rooms.cp                   Part of Aliens vs Predator: The MUD *
*  Usage: Primary code for rooms                                         *
*************************************************************************/


#include "rooms.h"
#include "characters.h"
#include "descriptors.h"
#include "utils.h"
#include "buffer.h"
#include "db.h"
#include "extradesc.h"
#include "files.h"
#include <stdarg.h>

RoomData *world = NULL;	/* array of rooms		 */
UInt32 top_of_world = 0;		/* ref to top element of world	 */
extern struct zone_data *zone_table;	/* zone table			 */
extern int top_of_zone_table;	/* top element of zone tab	 */
extern bool mini_mud;


RoomData::RoomData(void) { }


RoomData::~RoomData(void) {
	int i;
	ExtraDesc *extradesc, *next;

	if (this->name)				FREE(this->name);
	if (this->description)		FREE(this->description);
//	SSFree(this->name);
//	SSFree(this->description);
	
	//	Free exits.
	for (i = 0; i < NUM_OF_DIRS; i++) {
		if (this->dir_option[i])
			delete this->dir_option[i];
	}

	//	Free extra descriptions.
	for (extradesc = this->ex_description; extradesc; extradesc = next) {
		next = extradesc->next;
		delete extradesc;
	}
}


SInt32 RoomData::Send(const char *messg, ...) {
	CharData *i;
	va_list args;
	char *send_buf;
	SInt32	length = 0;
	DescriptorData *	d;
	
	if (!messg || !*messg || !this || !this->people.Count())
		return 0;
 
 	send_buf = get_buffer(MAX_STRING_LENGTH);
 	
	va_start(args, messg);
	length += vsprintf(send_buf, messg, args);
	va_end(args);
	
	LListIterator<CharData *>	iter(this->people);
	
	while ((i = iter.Next())) {
		d = i->desc;
		if (d && (STATE(d) == CON_PLAYING) && !PLR_FLAGGED(i, PLR_WRITING) && AWAKE(i));
			SEND_TO_Q(send_buf, d);
	}
	
	release_buffer(send_buf);

	return length;
}


RoomDirection::RoomDirection(void) : exit_info(0), key(0), to_room(-1),
		general_description(NULL), keyword(NULL) {
//	this->general_description = NULL;
//	this->keyword = NULL;
//	this->exit_info = 0;
//	this->key = 0;
//	this->to_room = -1;
}


RoomDirection::RoomDirection(RoomDirection *dir) {
	*this = *dir;
	this->general_description = str_dup(dir->general_description);
	this->keyword = str_dup(dir->keyword);
//	this->general_description = SSCreate(SSData(dir->general_description));
//	this->keyword = SSCreate(SSData(dir->keyword));
}


RoomDirection::~RoomDirection(void) {
	if (this->general_description)	FREE(this->general_description);
	if (this->keyword)				FREE(this->keyword);
//	SSFree(this->general_description);
//	SSFree(this->keyword);
}


/* load the rooms */
void RoomFuncs::ParseRoom(FILE * fl, int virtual_nr, char *filename) {
	static int room_nr = 0, zone = 0;
	int t[10], i;
	ExtraDesc *new_descr;
	char	*line = get_buffer(256),
			*flags = get_buffer(128),
			*buf = get_buffer(128);

	sprintf(buf, "room #%d", virtual_nr);

	if (virtual_nr <= (zone ? zone_table[zone - 1].top : -1)) {
		log("Room #%d is below zone %d.", virtual_nr, zone);
		tar_restore_file(filename);
		exit(1);
	}
	while (virtual_nr > zone_table[zone].top)
		if (++zone > top_of_zone_table) {
			log("Room %d is outside of any zone.", virtual_nr);
			tar_restore_file(filename);
			exit(1);
		}
	world[room_nr].zone = zone;
	world[room_nr].number = virtual_nr;
	world[room_nr].name = fread_string(fl, buf, filename);
	world[room_nr].description = fread_string(fl, buf, filename);
//	world[room_nr].name = SSfread(fl, buf, filename);
//	world[room_nr].description = SSfread(fl, buf, filename);
	
	if (!get_line(fl, line) || (sscanf(line, " %d %s %d ", t, flags, t + 2) != 3)) {
		log("Format error in room #%d", virtual_nr);
		tar_restore_file(filename);
		exit(1);
	}
/* t[0] is the zone number; ignored with the zone-file system */
	world[room_nr].flags = asciiflag_conv(flags);
	world[room_nr].sector_type = t[2];

	world[room_nr].func = NULL;
//	world[room_nr].contents = NULL;
//	world[room_nr].people = NULL;
	world[room_nr].light = 0;	/* Zero light sources */
	world[room_nr].id = room_nr + ROOM_ID_BASE;

	for (i = 0; i < NUM_OF_DIRS; i++)
		world[room_nr].dir_option[i] = NULL;

	world[room_nr].ex_description = NULL;


	for (;;) {
		if (!get_line(fl, line)) {
			log("Format error in room #%d (expecting D/E/S)", virtual_nr);
			tar_restore_file(filename);
			exit(1);
		}
		switch (*line) {
			case 'D':
				RoomFuncs::SetupDir(fl, room_nr, atoi(line + 1), filename);
				break;
			case 'E':
				new_descr = new ExtraDesc;
				new_descr->keyword = SSfread(fl, buf, filename);
				new_descr->description = SSfread(fl, buf, filename);
				new_descr->next = world[room_nr].ex_description;
				world[room_nr].ex_description = new_descr;
				break;
			case 'S':			/* end of room */
				top_of_world = room_nr++;
				release_buffer(flags);
				release_buffer(line);
				release_buffer(buf);
				return;
				break;
			default:
				log("Format error in room #%d (expecting D/E/S)", virtual_nr);
				tar_restore_file(filename);
				exit(1);
				break;
		}
	}
	release_buffer(flags);
	release_buffer(line);
	release_buffer(buf);
}


/* read direction data */
void RoomFuncs::SetupDir(FILE * fl, int room, int dir, char *filename) {
	SInt32	t[3];
	char	*line = get_buffer(256),
			*buf = get_buffer(128);

	sprintf(buf, "room #%d, direction D%d", world[room].number, dir);

	world[room].dir_option[dir] = new RoomDirection;
	EXITN(room, dir)->general_description = fread_string(fl, buf, filename);
	EXITN(room, dir)->keyword = fread_string(fl, buf, filename);
//	EXITN(room, dir)->general_description = SSfread(fl, buf, filename);
//	EXITN(room, dir)->keyword = SSfread(fl, buf, filename);

	if (!get_line(fl, line) || (sscanf(line, " %d %d %d ", t, t + 1, t + 2) != 3)) {
		log("Format error, %s", buf);
		tar_restore_file(filename);
		exit(1);
	}

  	EXITN(room, dir)->exit_info = t[0];
	EXITN(room, dir)->key = t[1];
	EXITN(room, dir)->to_room = t[2];
	
	release_buffer(line);
	release_buffer(buf);
}


/* make sure the start rooms exist & resolve their vnums to rnums */
void check_start_rooms(void) {
	if ((r_mortal_start_room = real_room(mortal_start_room)) < 0) {
		log("SYSERR:  Mortal start room does not exist.  Change in config.c.");
		exit(1);
	}
	if ((r_immort_start_room = real_room(immort_start_room)) < 0) {
		if (!mini_mud)
			log("SYSERR:  Warning: Immort start room does not exist.  Change in config.c.");
		r_immort_start_room = r_mortal_start_room;
	}
	if ((r_frozen_start_room = real_room(frozen_start_room)) < 0) {
		if (!mini_mud)
			log("SYSERR:  Warning: Frozen start room does not exist.  Change in config.c.");
		r_frozen_start_room = r_mortal_start_room;
	}
}


/* resolve all vnums into rnums in the world */
void renum_world(void) {
	RNum	room;
	SInt32	door;

	for (room = 0; room <= top_of_world; room++)
		for (door = 0; door < NUM_OF_DIRS; door++)
			if (world[room].dir_option[door])
				if (world[room].dir_option[door]->to_room != NOWHERE)
					world[room].dir_option[door]->to_room =
							real_room(world[room].dir_option[door]->to_room);
}




/* returns the real number of the object with given virtual number */
/* reditmod - changed binary search to incremental                 */
/* -JEE- 0.4 - changed to binary & incremental                     */
/* thanks to Johan Eenfeldt for the idea and the code :)  *.06*    */
RNum real_room(VNum vnum) {
	int bot, top, mid, nr, found = NOWHERE;

	/* First binary search. */
	bot = 0;
	top = top_of_world;

	for (;;) {
		mid = (bot + top) / 2;
		
		if (mid == -1) {
			mudlogf(BRF, LVL_STAFF, TRUE, "SYSERR: real_room mid == -1!  (vnum == %d)", vnum);
			break;
		}
		if ((world + mid)->number == vnum)
			return mid;
		if (bot >= top)
			break;
		if ((world + mid)->number > vnum)
			top = mid - 1;
		else
			bot = mid + 1;
	}

	/* If not found - use linear on the "new" parts. */
	for(nr = 0; nr <= top_of_world; nr++) {
		if(world[nr].number == vnum) {
			found = nr;
			break;
		}
	}
	return(found);
}


