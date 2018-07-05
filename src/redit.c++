/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*  _TwyliteMud_ by Rv.                          Based on CircleMud3.0bpl9 *
*    				                                          *
*  OasisOLC - redit.c 		                                          *
*    				                                          *
*  Copyright 1996 Harvey Gilpin.                                          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*. Original author: Levork .*/




#include "structs.h"
#include "comm.h"
#include "utils.h"
#include "scripts.h"
#include "db.h"
#include "boards.h"
#include "olc.h"
#include "buffer.h"
#include "extradesc.h"


/*------------------------------------------------------------------------*/

/*
 * External data structures.
 */
extern char 	*room_bits[];
extern char 	*sector_types[];
extern char 	*exit_bits[];
extern struct zone_data	*zone_table;
extern int		top_of_zone_table;

/*------------------------------------------------------------------------*/

/*
 * Function Prototypes
 */


/*------------------------------------------------------------------------*/

#define  W_EXIT(room, num) (world[(room)].dir_option[(num)])

/*------------------------------------------------------------------------*\
  Utils and exported functions.
\*------------------------------------------------------------------------*/

void REdit::setup_new(DescriptorData *d) {
	CREATE(OLC_ROOM(d), RoomData, 1);
//	OLC_ROOM(d)->name = SSCreate("An unfinished room");
//	OLC_ROOM(d)->description = SSCreate("You are in an unfinished room.\r\n");
	OLC_ROOM(d)->name = str_dup("An unfinished room");
	OLC_ROOM(d)->description = str_dup("You are in an unfinished room.\r\n");
	REdit::disp_menu(d);
	OLC_VAL(d) = 0;
}

/*------------------------------------------------------------------------*/

void REdit::setup_existing(DescriptorData *d, RNum real_num) {
	RoomData *room;
	int counter;
  
	//	uild a copy of the room for editing.
	room = new RoomData;
	*room = world[real_num];
	
//	memset(&room->people, 0, sizeof(LList<CharData *>));
//	memset(&room->contents, 0, sizeof(LList<ObjData *>));

	room->people.Erase();
	room->contents.Erase();
	
	//	Allocate space for all strings.
//	room->name = SSCreate(world[real_num].GetName("undefined"));
//	room->description = SSCreate(world[real_num].GetDesc("undefined"));
	room->name = str_dup(world[real_num].GetName("undefined"));
	room->description = str_dup(world[real_num].GetDesc("undefined"));

	//	Exits - We allocate only if necessary.
	for (counter = 0; counter < NUM_OF_DIRS; counter++)
		if (world[real_num].dir_option[counter])
			room->dir_option[counter] = new RoomDirection(world[real_num].dir_option[counter]);

	//	Extra descriptions, if necessary.
	if (world[real_num].ex_description) {
		ExtraDesc *extradesc, *temp, *temp2;
		temp = new ExtraDesc;

		room->ex_description = temp;
		for (extradesc = world[real_num].ex_description; extradesc; extradesc = extradesc->next) {
			temp->keyword = SSCreate(SSData(extradesc->keyword));
			temp->description = SSCreate(SSData(extradesc->description));
			if (extradesc->next) {
				temp2 = new ExtraDesc;
				temp->next = temp2;
				temp = temp2;
			} else
				temp->next = NULL;
		}
	}

	//	Attatch room copy to players descriptor
	OLC_ROOM(d) = room;
	OLC_VAL(d) = 0;
	REdit::disp_menu(d);
}


/*------------------------------------------------------------------------*/
      
#define ZCMD (zone_table[zone].cmd[cmd_no])

void REdit::save_internally(DescriptorData *d) {
	int i, j, room_num, found = 0, zone, cmd_no;
	RoomData *new_world;
	CharData *temp_ch;
	ObjData *temp_obj;
	DescriptorData *dsc;

	REMOVE_BIT(OLC_ROOM(d)->flags, ROOM_DELETED);
	
	room_num = real_room(OLC_NUM(d));
	if (room_num > -1) {
		//	Room exists: move contents over then free and replace it.
		OLC_ROOM(d)->people = world[room_num].people;
		OLC_ROOM(d)->contents = world[room_num].contents;
		REdit::free_world_room(world + room_num);
		world[room_num] = *OLC_ROOM(d);
//		memset(&OLC_ROOM(d)->people, 0, sizeof(LList<CharData *>));
//		memset(&OLC_ROOM(d)->contents, 0, sizeof(LList<ObjData *>));
		OLC_ROOM(d)->people.Erase();
		OLC_ROOM(d)->contents.Erase();
		
		// Update iters?  Seems safe.
//		Is this really necessary?  They point to nothing really
//		and the address stays the same!
//		OLC_ROOM(d)->people.UpdateIters();
//		OLC_ROOM(d)->contents.UpdateIters();
	} else {	//	Room doesn't exist, hafta add it
		CREATE(new_world, RoomData, top_of_world + 2);

		//	Count through world tables.
		for (i = 0; i <= top_of_world; i++) {
			if (!found) {	//	Is this the place? 
				if (world[i].number > OLC_NUM(d)) {
					found = 1;
					new_world[i] = *(OLC_ROOM(d));
					new_world[i].number = OLC_NUM(d);
					new_world[i].func = NULL;
					room_num  = i;

					new_world[i + 1] = world[i];	//	Copy from world to new_world + 1.
					new_world[i + 1].people.UpdateIters();
					new_world[i + 1].contents.UpdateIters();
					
					//	People in this room must have their numbers moved up one.
					START_ITER(ch_iter, temp_ch, CharData *, world[i].people) {
						if (IN_ROOM(temp_ch) != NOWHERE)
							IN_ROOM(temp_ch) = i + 1;
					} END_ITER(ch_iter);
					//	Move objects up one room.
					START_ITER(obj_iter, temp_obj, ObjData *, world[i].contents) {
						if (IN_ROOM(temp_obj) != NOWHERE)
							IN_ROOM(temp_obj) = i + 1;
					} END_ITER(obj_iter);
				} else {	//	Not yet placed, copy straight over.
					new_world[i] = world[i];
					new_world[i].people.UpdateIters();
					new_world[i].contents.UpdateIters();
				}
			} else {	//	Already been found.
				//	People in this room must have their in_rooms moved.
				START_ITER(ch_iter, temp_ch, CharData *, world[i].people) {
					if (IN_ROOM(temp_ch) != NOWHERE)
						IN_ROOM(temp_ch) = i + 1;
				} END_ITER(ch_iter);
				
				//	Move objects too.
				START_ITER(obj_iter, temp_obj, ObjData *, world[i].contents) {
					if (IN_ROOM(temp_obj) != NOWHERE)
						IN_ROOM(temp_obj) = i + 1;
				} END_ITER(obj_iter);

				new_world[i + 1] = world[i];
				new_world[i + 1].people.UpdateIters();
				new_world[i + 1].contents.UpdateIters();
			}
		}
		if (!found) {	//	Still not found, insert at top of table
			new_world[i] = *(OLC_ROOM(d));
			new_world[i].number = OLC_NUM(d);
			new_world[i].func = NULL;
			room_num = i;
		}

		//	Copy world table over to new one.
		free(world);
		world = new_world;
		top_of_world++;

		//	Update zone table
		for (zone = 0; zone <= top_of_zone_table; zone++)
			for (cmd_no = 0; ZCMD.command != 'S'; cmd_no++)
			switch (ZCMD.command) {
				case 'M':
				case 'O':
					if (ZCMD.arg3 >= room_num)
						ZCMD.arg3++;
					break;
				case 'D':
				case 'R':
					if (ZCMD.arg1 >= room_num)
						ZCMD.arg1++;
					break;
				case 'T':
					if ((ZCMD.arg1 == WLD_TRIGGER) && (ZCMD.arg4 >= room_num))
						ZCMD.arg4++;
					break;
				case 'G':
				case 'P':
				case 'E':
				case '*':
				case 'C':
				case 'Z':
					break;
				default:
					mudlogf(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: REdit::save_internally: Unknown case in zone update '%c'", ZCMD.command);
        }
		/*
		 * Update load rooms, to fix creeping load room problem.
		 */
		if (room_num <= r_mortal_start_room)	r_mortal_start_room++;
		if (room_num <= r_immort_start_room)	r_immort_start_room++;
		if (room_num <= r_frozen_start_room)	r_frozen_start_room++;

		/*
		 * Update world exits.
		 */
		for (i = 0; i < top_of_world + 1; i++)
			for (j = 0; j < NUM_OF_DIRS; j++)
				if (W_EXIT(i, j))
					if (W_EXIT(i, j)->to_room >= room_num)
						W_EXIT(i, j)->to_room++;
		/*
		 * Update any rooms being edited.
		 */
		START_ITER(desc_iter, dsc, DescriptorData *, descriptor_list) {
			if (STATE(dsc) == CON_REDIT)
				for (j = 0; j < NUM_OF_DIRS; j++)
					if (OLC_ROOM(dsc)->dir_option[j])
						if (OLC_ROOM(dsc)->dir_option[j]->to_room >= room_num)
							OLC_ROOM(dsc)->dir_option[j]->to_room++;
		} END_ITER(desc_iter);
	}
	olc_add_to_save_list(zone_table[OLC_ZNUM(d)].number, OLC_SAVE_ROOM);
}


/*------------------------------------------------------------------------*/

void REdit::save_to_disk(int zone_num) {
	int counter, counter2, realcounter;
	FILE *fp;
	RoomData *room;
	ExtraDesc *ex_desc;
	char *bitList;
	char *fname = get_buffer(MAX_STRING_LENGTH);
	char *buf1;

	sprintf(fname, WLD_PREFIX "%d.new", zone_table[zone_num].number);

	if (!(fp = fopen(fname, "w+"))) {
		mudlogf(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: Cannot open room file %s!", fname);
		release_buffer(fname);
		return;
	}

	bitList = get_buffer(32);
	buf1 = get_buffer(MAX_STRING_LENGTH);

	for (counter = zone_table[zone_num].number * 100; counter <= zone_table[zone_num].top; counter++) {
		realcounter = real_room(counter);
		if (realcounter >= 0) {
			if (ROOM_FLAGGED(realcounter, ROOM_DELETED))
				continue;

			room = (world + realcounter);
			//	Remove the '\r\n' sequences from description.
			strcpy(buf1, room->GetDesc("Empty"));
			strip_string(buf1);

			if (room->flags)		sprintbits(room->flags, bitList);
			else					strcpy(bitList, "0");

			fprintf(fp,	"#%d\n"
	      				"%s~\n"
	      				"%s~\n"
	      				"%d %s %d\n",
					counter,
	      			room->GetName("undefined"),
	      			buf1,
	      			zone_table[room->zone].number, bitList, room->sector_type);

			//	Handle exits.
			for (counter2 = 0; counter2 < NUM_OF_DIRS; counter2++) {
				if (room->dir_option[counter2]) {
					int temp_door_flag;

					if (room->dir_option[counter2]->to_room == -1 &&
							!room->dir_option[counter2]->GetDesc())
						continue;
					
					//	Again, strip out the garbage.
					if (room->dir_option[counter2]->GetDesc()) {
						strcpy(buf1, room->dir_option[counter2]->GetDesc());
						strip_string(buf1);
					} else
						*buf1 = 0;

					//	Figure out door flag - mask out run-time-only flags.
					temp_door_flag = room->dir_option[counter2]->exit_info & ~(EX_CLOSED | EX_LOCKED);

					//	Ok, now wrote output to file.
					fprintf(fp, "D%d\n"
								"%s~\n"
								"%s~\n"
								"%d %d %d\n",
							counter2,
//							*buf1 ? buf1 : "Looks like your average exit.",
							buf1,
							room->dir_option[counter2]->GetKeyword("door"),
							temp_door_flag, room->dir_option[counter2]->key,
							room->dir_option[counter2]->to_room != -1 ?
							world[room->dir_option[counter2]->to_room].number : -1);
				}
			}
			//	Home straight, just deal with extra descriptions.
			for (ex_desc = room->ex_description; ex_desc; ex_desc = ex_desc->next) {
				if (SSData(ex_desc->description) && SSData(ex_desc->keyword)) {
					strcpy(buf1, SSData(ex_desc->description));
					strip_string(buf1);
 					fprintf(fp,	"E\n"
								"%s~\n"
								"%s~\n",
							SSData(ex_desc->keyword),
							buf1);
				}
			}
			fprintf(fp, "S\n");
		}
	}
	release_buffer(bitList);
	//	Write final line and close.
	fprintf(fp, "$~\n");
	fclose(fp);
	sprintf(buf1, "%s/%d.wld", WLD_PREFIX, zone_table[zone_num].number);
	
	//	We're fubar'd if we crash between the two lines below.
	remove(buf1);
	rename(fname, buf1);
	
	release_buffer(buf1);
	release_buffer(fname);
	olc_remove_from_save_list(zone_table[zone_num].number, OLC_SAVE_ROOM);
}


/*------------------------------------------------------------------------*/

void REdit::free_room(RoomData *room) {
	SInt32	i;
	
//	SSFree(room->name);
//	SSFree(room->description);
	room->name = NULL;
	room->description = NULL;
	for (i = 0; i < NUM_OF_DIRS; i++)
		room->dir_option[i] = NULL;
	room->ex_description = NULL;
//	memset(&room->people, 0, sizeof(LList<RoomData *>));
//	memset(&room->contents, 0, sizeof(LList<RoomData *>));
	room->people.Erase();
	room->contents.Erase();
	delete room;
}


void REdit::free_world_room(RoomData *room) {
	SInt32 i;
	ExtraDesc *extradesc, *next;
	
//	SSFree(room->name);
//	SSFree(room->description);
	if (room->name)			free(room->name);
	if (room->description)	free(room->description);
	for (i = 0; i < NUM_OF_DIRS; i++)
		if (room->dir_option[i])
			delete room->dir_option[i];
	for (extradesc = room->ex_description; extradesc; extradesc = next) {
		next = extradesc->next;
		delete extradesc;
	}
}


/**************************************************************************
 Menu functions 
 **************************************************************************/

/*
 * For extra descriptions.
 */
void REdit::disp_extradesc_menu(DescriptorData * d) {
	ExtraDesc *extra_desc = OLC_DESC(d);
	
	d->character->Send(
#if defined(CLEAR_SCREEN)
				"\x1B[H\x1B[J"
#endif
				"`g1`n) Keyword: `y%s\r\n"
				"`g2`n) Description:\r\n`y%s\r\n"
				"`g3`n) Goto next description: %s\r\n"
				"Enter choice (0 to quit) : ",
				SSData(extra_desc->keyword) ? SSData(extra_desc->keyword) : "<NONE>",
				SSData(extra_desc->description) ? SSData(extra_desc->description) : "<NONE>",
				!extra_desc->next ? "<NOT SET>\r\n" : "Set.\r\n");
				
	OLC_MODE(d) = REDIT_EXTRADESC_MENU;
}


/*
 * For exits.
 */
void REdit::disp_exit_menu(DescriptorData * d) {
	char	*buf = get_buffer(MAX_STRING_LENGTH);

	/*
	 * if exit doesn't exist, alloc/create it 
	 */
	if(!OLC_EXIT(d))	OLC_EXIT(d) = new RoomDirection;

	sprintbit(OLC_EXIT(d)->exit_info, exit_bits, buf);
  
	d->character->Send(
#if defined(CLEAR_SCREEN)
				"\x1B[H\x1B[J"
#endif
				"`g1`n) Exit to     : `c%d\r\n"
				"`g2`n) Description :-\r\n`y%s\r\n"
				"`g3`n) Door name   : `y%s\r\n"
				"`g4`n) Key         : `c%d\r\n"
				"`g5`n) Door flags  : `c%s\r\n"
				"`g6`n) Purge exit.\r\n"
				"Enter choice, 0 to quit : ",
			OLC_EXIT(d)->to_room != -1 ? world[OLC_EXIT(d)->to_room].number : -1,
			OLC_EXIT(d)->GetDesc("<NONE>"),
			OLC_EXIT(d)->GetKeyword("<NONE>"),
			OLC_EXIT(d)->key,
			*buf ? buf : "None");
	release_buffer(buf);
	
	OLC_MODE(d) = REDIT_EXIT_MENU;
}


/*
 * For exit flags.
 */
void REdit::disp_exit_flag_menu(DescriptorData * d) {
	int counter, columns = 0;
	char	*buf = get_buffer(MAX_STRING_LENGTH);

#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	for (counter = 0; counter < NUM_EXIT_FLAGS; counter++) {
		d->character->Send("`g%2d`n) %-20.20s %s", counter + 1, exit_bits[counter],
				!(++columns % 2) ? "\r\n" : "");
	}
	sprintbit(OLC_EXIT(d)->exit_info, exit_bits, buf);
	d->character->Send(
			"\r\nExit flags: `c%s`n\r\n"
			"Enter room flags, 0 to quit : ",
			buf);
	release_buffer(buf);
	OLC_MODE(d) = REDIT_EXIT_DOORFLAGS;
}


/*
 * For room flags.
 */
void REdit::disp_flag_menu(DescriptorData * d) {
	int counter, columns = 0;
	char	*buf = get_buffer(MAX_STRING_LENGTH);

#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	for (counter = 0; counter < NUM_ROOM_FLAGS; counter++) {
		d->character->Send("`g%2d`n) %-20.20s %s", counter + 1, room_bits[counter],
				!(++columns % 2) ? "\r\n" : "");
	}
	sprintbit(OLC_ROOM(d)->flags, room_bits, buf);
	d->character->Send(
			"\r\nRoom flags: `c%s`n\r\n"
			"Enter room flags, 0 to quit : ",
			buf);
	release_buffer(buf);
	OLC_MODE(d) = REDIT_FLAGS;
}


//	For sector type.
void REdit::disp_sector_menu(DescriptorData * d) {
	int counter, columns = 0;
#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	for (counter = 0; counter < NUM_ROOM_SECTORS; counter++) {
		d->character->Send("`g%2d`n) %-20.20s %s", counter, sector_types[counter],
				!(++columns % 2) ? "\r\n" : "");
	}
	send_to_char("\r\nEnter sector type : ", d->character);
	OLC_MODE(d) = REDIT_SECTOR;
}


//	The main menu.
void REdit::disp_menu(DescriptorData * d) {
	RoomData *room;
	char	*buf1 = get_buffer(256),
			*buf2 = get_buffer(256);
	room = OLC_ROOM(d);

	sprintbit((long)room->flags, room_bits, buf1);
	sprinttype(room->sector_type, sector_types, buf2);
	d->character->Send(
#if defined(CLEAR_SCREEN)
				"\x1B[H\x1B[J"
#endif
				"-- Room number : [`c%d`n]  	Room zone: [`c%d`n]\r\n"
				"`G1`n) Name        : `y%s\r\n"
				"`G2`n) Description :\r\n`y%s"
				"`G3`n) Room flags  : `c%s\r\n"
				"`G4`n) Sector type : `c%s\r\n"
				"`G5`n) Exit north  : `c%d\r\n"
				"`G6`n) Exit east   : `c%d\r\n"
				"`G7`n) Exit south  : `c%d\r\n"
				"`G8`n) Exit west   : `c%d\r\n"
				"`G9`n) Exit up     : `c%d\r\n"
				"`GA`n) Exit down   : `c%d\r\n"
				"`GB`n) Extra descriptions menu\r\n"
				"`GQ`n) Quit\r\n"
				"Enter choice : ",
			OLC_NUM(d), zone_table[OLC_ZNUM(d)].number,
			room->GetName("<ERROR>"),
			room->GetDesc("<ERROR>"),
			buf1,
			buf2,
			room->dir_option[NORTH]	&& room->dir_option[NORTH]->to_room != -1	?	world[room->dir_option[NORTH]->to_room].number	: NOWHERE,
			room->dir_option[EAST]	&& room->dir_option[EAST]->to_room != -1	?	world[room->dir_option[EAST]->to_room].number	: NOWHERE,
			room->dir_option[SOUTH]	&& room->dir_option[SOUTH]->to_room != -1	?	world[room->dir_option[SOUTH]->to_room].number	: NOWHERE,
			room->dir_option[WEST]	&& room->dir_option[WEST]->to_room != -1	?	world[room->dir_option[WEST]->to_room].number	: NOWHERE,
			room->dir_option[UP]	&& room->dir_option[UP]->to_room != -1		?	world[room->dir_option[UP]->to_room].number		: NOWHERE,
			room->dir_option[DOWN]	&& room->dir_option[DOWN]->to_room != -1	?	world[room->dir_option[DOWN]->to_room].number	: NOWHERE);
	release_buffer(buf1);
	release_buffer(buf2);
	OLC_MODE(d) = REDIT_MAIN_MENU;
}



/**************************************************************************
 * The main loop
 **************************************************************************/

void REdit::parse(DescriptorData * d, char *arg) {
	int number;

	switch (OLC_MODE(d)) {
		case REDIT_CONFIRM_SAVESTRING:
			switch (*arg) {
				case 'y':
				case 'Y':
					REdit::save_internally(d);
					mudlogf(NRM, LVL_BUILDER, TRUE,  "OLC: %s edits room %d.", d->character->RealName(), OLC_NUM(d));
					/*
					 * Do NOT free strings! Just the room structure. 
					 */
					cleanup_olc(d, CLEANUP_STRUCTS);
					send_to_char("Room saved to memory.\r\n", d->character);
					break;
				case 'n':
				case 'N':
					/*
					 * Free everything up, including strings, etc.
					*/
					cleanup_olc(d, CLEANUP_ALL);
					break;
				default:
					send_to_char("Invalid choice!\r\n"
								 "Do you wish to save this room internally? : ", d->character);
					break;
			}
			return;
		case REDIT_MAIN_MENU:
			switch (*arg) {
				case 'q':
				case 'Q':
					if (OLC_VAL(d)) { /* Something has been modified. */
						send_to_char("Do you wish to save this room internally? : ", d->character);
						OLC_MODE(d) = REDIT_CONFIRM_SAVESTRING;
					} else
						cleanup_olc(d, CLEANUP_ALL);
					break;
				case '1':
					send_to_char("Enter room name:-\r\n| ", d->character);
					OLC_MODE(d) = REDIT_NAME;
					break;
				case '2':
					OLC_MODE(d) = REDIT_DESC;
#if defined(CLEAR_SCREEN)
					SEND_TO_Q("\x1B[H\x1B[J", d);
#endif
					SEND_TO_Q("Enter room description: (/s saves /h for help)\r\n\r\n", d);
					d->backstr = NULL;
#if 1
					if (OLC_ROOM(d)->description) {
						SEND_TO_Q(OLC_ROOM(d)->description, d);
						d->backstr = str_dup(OLC_ROOM(d)->description);
					}
					d->str = &OLC_ROOM(d)->description;
#else
					if (OLC_ROOM(d)->description) {
						if (SSData(OLC_ROOM(d)->description)) {
							SEND_TO_Q(SSData(OLC_ROOM(d)->description), d);
							d->backstr = str_dup(SSData(OLC_ROOM(d)->description));
						}
					} else
						OLC_ROOM(d)->description = SSCreate("");
					d->str = &OLC_ROOM(d)->description->str;
#endif
					d->max_str = MAX_ROOM_DESC;
					d->mail_to = 0;
					OLC_VAL(d) = 1;
					break;
				case '3':
					REdit::disp_flag_menu(d);
					break;
				case '4':
					REdit::disp_sector_menu(d);
					break;
				case '5':
					OLC_VAL(d) = NORTH;
					REdit::disp_exit_menu(d);
					break;
				case '6':
					OLC_VAL(d) = EAST;
					REdit::disp_exit_menu(d);
					break;
				case '7':
					OLC_VAL(d) = SOUTH;
					REdit::disp_exit_menu(d);
					break;
				case '8':
					OLC_VAL(d) = WEST;
					REdit::disp_exit_menu(d);
					break;
				case '9':
					OLC_VAL(d) = UP;
					REdit::disp_exit_menu(d);
					break;
				case 'a':
				case 'A':
					OLC_VAL(d) = DOWN;
					REdit::disp_exit_menu(d);
					break;
				case 'b':
				case 'B':
					/*
					 * If the extra description doesn't exist.
					 */
					if (!OLC_ROOM(d)->ex_description) {
						OLC_ROOM(d)->ex_description = new ExtraDesc;
						OLC_ROOM(d)->ex_description->next = NULL;
					}
					OLC_DESC(d) = OLC_ROOM(d)->ex_description;
					REdit::disp_extradesc_menu(d);
					break;
				default:
					send_to_char("Invalid choice!", d->character);
					REdit::disp_menu(d);
					break;
			}
			return;

		case REDIT_NAME:
			if (strlen(arg) > MAX_ROOM_NAME)	arg[MAX_ROOM_NAME - 1] = 0;
//			SSFree(OLC_ROOM(d)->name);
//			OLC_ROOM(d)->name = SSCreate(*arg ? arg : "undefined");
			if (OLC_ROOM(d)->name)				free(OLC_ROOM(d)->name);
			OLC_ROOM(d)->name = str_dup(*arg ? arg : "undefined");
			break;
		case REDIT_DESC:
			/*
			 * We will NEVER get here, we hope.
			 */
			mudlogf(BRF, LVL_BUILDER, TRUE, "SYSERR: %s reached REDIT_DESC case in parse_redit", d->character->RealName());
			break;

		case REDIT_FLAGS:
			number = atoi(arg);
			if ((number < 0) || (number > NUM_ROOM_FLAGS))
				send_to_char("That is not a valid choice!\r\n", d->character);
			else if (number == 0)
				break;
			else /* Toggle the bit. */
				TOGGLE_BIT(OLC_ROOM(d)->flags, 1 << (number - 1));
			REdit::disp_flag_menu(d);
			return;

		case REDIT_SECTOR:
			number = atoi(arg);
			if (number < 0 || number >= NUM_ROOM_SECTORS) {
				send_to_char("Invalid choice!", d->character);
				REdit::disp_sector_menu(d);
				return;
			} else
				OLC_ROOM(d)->sector_type = number;
			break;

		case REDIT_EXIT_MENU:
			switch (*arg) {
				case '0':
					break;
				case '1':
					OLC_MODE(d) = REDIT_EXIT_NUMBER;
					send_to_char("Exit to room number : ", d->character);
					return;
				case '2':
					OLC_MODE(d) = REDIT_EXIT_DESCRIPTION;
					SEND_TO_Q("Enter exit description: (/s saves /h for help)\r\n\r\n", d);
					d->backstr = NULL;
#if 1
					if (OLC_EXIT(d)->general_description) {
						SEND_TO_Q(OLC_EXIT(d)->general_description, d);
						d->backstr = str_dup(OLC_EXIT(d)->general_description);
					}
					d->str = &OLC_EXIT(d)->general_description;
#else
					if (OLC_EXIT(d)->general_description) {
						if (SSData(OLC_EXIT(d)->general_description)) {
							SEND_TO_Q(SSData(OLC_EXIT(d)->general_description), d);
							d->backstr = str_dup(SSData(OLC_DESC(d)->description));
						}
					} else
						OLC_EXIT(d)->general_description = SSCreate("");
					d->str = &OLC_EXIT(d)->general_description->str;
#endif
					d->max_str = MAX_EXIT_DESC;
					d->mail_to = 0;
					return;
				case '3':
					OLC_MODE(d) = REDIT_EXIT_KEYWORD;
					send_to_char("Enter keywords : ", d->character);
					return;
				case '4':
					OLC_MODE(d) = REDIT_EXIT_KEY;
					send_to_char("Enter key number : ", d->character);
					return;
				case '5':
					REdit::disp_exit_flag_menu(d);
					OLC_MODE(d) = REDIT_EXIT_DOORFLAGS;
					return;
				case '6':
					/*
					 * Delete an exit.
					 */
//					if (OLC_EXIT(d)->keyword)				FREE(OLC_EXIT(d)->keyword);
//					if (OLC_EXIT(d)->general_description)	FREE(OLC_EXIT(d)->general_description);
//					FREE(OLC_EXIT(d));
					delete OLC_EXIT(d);
					OLC_EXIT(d) = NULL;
					break;
				default:
					send_to_char("Try again : ", d->character);
					return;
			}
			break;

		case REDIT_EXIT_NUMBER:
			if ((number = atoi(arg)) != NOWHERE) {
				if ((number = real_room(number)) < 0) {
					send_to_char("That room does not exist, try again : ", d->character);
					return;
				}
			}
			OLC_EXIT(d)->to_room = number;
			REdit::disp_exit_menu(d);
			return;

		case REDIT_EXIT_DESCRIPTION:
			/*
			 * We should NEVER get here, hopefully.
			 */
			mudlogf(BRF, LVL_BUILDER, TRUE, "SYSERR: %s reached REDIT_EXIT_DESC case in parse_redit", d->character->RealName());
			break;

		case REDIT_EXIT_KEYWORD:
//			SSFree(OLC_EXIT(d)->keyword);
//			OLC_EXIT(d)->keyword = (*arg ? SSCreate(arg) : NULL);
			if (OLC_EXIT(d)->keyword)			free(OLC_EXIT(d)->keyword);
			OLC_EXIT(d)->keyword = (*arg ? str_dup(arg) : NULL);
			REdit::disp_exit_menu(d);
			return;

		case REDIT_EXIT_KEY:
			number = atoi(arg);
			OLC_EXIT(d)->key = number;
			REdit::disp_exit_menu(d);
			return;

		case REDIT_EXIT_DOORFLAGS:
			number = atoi(arg);
			if ((number < 0) || (number > NUM_EXIT_FLAGS)) {
				send_to_char("That's not a valid choice!\r\n"
							 "Enter room flags (0 to quit) : ", d->character);
				return;
			} else if (number == 0) {
				REdit::disp_exit_menu(d);
				return;
			} else if (EXIT_FLAGGED(OLC_EXIT(d), 1 << (number - 1))) {
				/* Toggle bits. */
				switch (number) {
					case 2:		/* Closed 	*/
					case 3:		/* Locked 	*/
						break;
					case 1:		/* Door 	*/
						REMOVE_BIT(OLC_EXIT(d)->exit_info, EX_CLOSED | EX_LOCKED | EX_PICKPROOF);
					default:
						REMOVE_BIT(OLC_EXIT(d)->exit_info, 1 << (number - 1));
				}
			} else {
				switch (number) {
					case 2:		/* Closed 	*/
					case 3:		/* Locked 	*/
						break;
					case 4:		/* Pickproof*/
						if (!EXIT_FLAGGED(OLC_EXIT(d), EX_ISDOOR))
							break;
					default:
						SET_BIT(OLC_EXIT(d)->exit_info, 1 << (number - 1));
				}
			}
			REdit::disp_exit_flag_menu(d);
			return;

		case REDIT_EXTRADESC_KEY:
			SSFree(OLC_DESC(d)->keyword);
			if (*arg)	OLC_DESC(d)->keyword = SSCreate(arg);
			else		OLC_DESC(d)->keyword = NULL;
			REdit::disp_extradesc_menu(d);
			return;

		case REDIT_EXTRADESC_MENU:
			switch ((number = atoi(arg))) {
				case 0:
					/*
					 * If something got left out, delete the extra description
					 * when backing out to the menu.
					 */
					if (!SSData(OLC_DESC(d)->keyword) || !SSData(OLC_DESC(d)->description)) {
						ExtraDesc *temp;
						REMOVE_FROM_LIST(OLC_DESC(d), OLC_ROOM(d)->ex_description, next);
						delete OLC_DESC(d);
						OLC_DESC(d) = NULL;
					}

/*					if (!SSData(OLC_DESC(d)->keyword) || !SSData(OLC_DESC(d)->description)) {
						ExtraDesc **tmp_desc;

						for(tmp_desc = &(OLC_ROOM(d)->ex_description); *tmp_desc; tmp_desc = &((*tmp_desc)->next))
							if(*tmp_desc == OLC_DESC(d)) {
								*tmp_desc = NULL;
								break;
							}
						delete OLC_DESC(d);
						OLC_DESC(d) = NULL;
					}
*/
					break;
				case 1:
					OLC_MODE(d) = REDIT_EXTRADESC_KEY;
					send_to_char("Enter keywords, separated by spaces : ", d->character);
					return;
				case 2:
					OLC_MODE(d) = REDIT_EXTRADESC_DESCRIPTION;
					SEND_TO_Q("Enter extra description: (/s saves /h for help)\r\n\r\n", d);
					d->backstr = NULL;
					if (OLC_DESC(d)->description) {
						if (SSData(OLC_DESC(d)->description)) {
							SEND_TO_Q(SSData(OLC_DESC(d)->description), d);
							d->backstr = str_dup(SSData(OLC_DESC(d)->description));
						}
					} else {
						OLC_DESC(d)->description = SSCreate("");
					}
					d->str = &OLC_DESC(d)->description->str;
					d->max_str = MAX_MESSAGE_LENGTH;
					d->mail_to = 0;
					return;

				case 3:
					if (!OLC_DESC(d)->keyword || !OLC_DESC(d)->description)
						send_to_char("You can't edit the next extra desc without completing this one.\r\n", d->character);
					else {
						ExtraDesc *new_extra;

						if (OLC_DESC(d)->next)
							OLC_DESC(d) = OLC_DESC(d)->next;
						else {	/* Make new extra description and attach at end. */
							new_extra = new ExtraDesc;
							OLC_DESC(d)->next = new_extra;
							OLC_DESC(d) = new_extra;
						}
					}
					REdit::disp_extradesc_menu(d);
					return;
			}
			break;

		default:
			/* we should never get here */
			mudlogf(BRF, LVL_BUILDER, TRUE, "SYSERR: %s reached default case in parse_redit", d->character->RealName());
			break;
	}
	/*
	 * If we get this far, something has be changed
	 */
	OLC_VAL(d) = 1;
	REdit::disp_menu(d);
}
