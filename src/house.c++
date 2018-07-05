/* ************************************************************************
*   File: house.c                                       Part of CircleMUD *
*  Usage: Handling of player houses                                       *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */





#include "structs.h"
#include "utils.h"
#include "scripts.h"
#include "buffer.h"
#include "comm.h"
#include "handler.h"
#include "find.h"
#include "db.h"
#include "interpreter.h"
#include "house.h"
#include "staffcmds.h"

extern char *dirs[];
extern int rev_dir[];


int House_get_filename(VNum vnum, char *filename);
int House_load(VNum vnum);
void House_save(ObjData * obj, FILE * fp, int locate);
void House_restore_weight(ObjData * obj);
void House_delete_file(VNum vnum);
int find_house(VNum vnum);
void House_save_control(void);
void hcontrol_list_houses(CharData * ch);
void hcontrol_build_house(CharData * ch, char *arg);
void hcontrol_destroy_house(CharData * ch, char *arg);
void hcontrol_pay_house(CharData * ch, char *arg);
ACMD(do_hcontrol);
ACMD(do_house);

struct house_control_rec house_control[MAX_HOUSES];
int num_of_houses = 0;


/* First, the basics: finding the filename; loading/saving objects */

/* Return a filename given a house vnum */
int House_get_filename(VNum vnum, char *filename) {
	if (vnum < 0)
		return 0;

	sprintf(filename, HOUSE_DIR "%d.house", vnum);
	
	return 1;
}


#define MAX_BAG_ROW 5

/* Load all objects for a house */
int House_load(VNum vnum) {
	FILE *	fl;
	char *	fname, *line;
	RNum	rnum;
	bool	read_file = true;
//	bool	result = true;
	ObjData *obj, *obj1;
	LList<ObjData *>	cont_row[MAX_BAG_ROW];
	SInt32	locate, j, obj_vnum;

	if ((rnum = real_room(vnum)) == NOWHERE)
		return 0;
	if (!House_get_filename(vnum, (fname = get_buffer(MAX_STRING_LENGTH)))) {
		release_buffer(fname);
		return 0;
	}
	
	if (!(fl = fopen(fname, "r+b"))) {
		release_buffer(fname);
		return 0;
	}
	
	line = get_buffer(MAX_INPUT_LENGTH);
	
//	for (j = 0;j < MAX_BAG_ROW;j++)
//		cont_row[j] = NULL; /* empty all cont lists (you never know ...) */

	while (get_line(fl, line) && read_file) {
		switch (*line) {
			case '#':
				if (sscanf(line, "#%d %d", &obj_vnum, &locate) < 1) {
					log("House file %s is damaged: no vnum", fname);
					read_file = false;
					if ((obj_vnum == -1) || (real_object(obj_vnum) == -1)) {
						obj = new ObjData();
						obj->name = SSCreate("Empty.");
						obj->shortDesc = SSCreate("Empty.");
						obj->description = SSCreate("Empty.\r\n");
					} else
						obj = read_object(obj_vnum, VIRTUAL);
						
					read_file = obj->load(fl, fname);
					
					for (j = MAX_BAG_ROW-1;j > -locate;j--)
						if (cont_row[j].Count()) { /* no container -> back to ch's inventory */
//							for (;cont_row[j];cont_row[j] = obj1) {
//								obj1 = cont_row[j]->next_content;
//								cont_row[j]->to_room(rnum);
//							}
//							cont_row[j] = NULL;
							while((obj1 = cont_row[j].Top())) {
								cont_row[j].Remove(obj1);
								obj1->to_room(rnum);
							}
						}

					if (j == -locate && cont_row[j].Count()) { /* content list existing */
						if (GET_OBJ_TYPE(obj) == ITEM_CONTAINER) {
							/* take item ; fill ; give to char again */
							obj->from_char();
//							for (;cont_row[j];cont_row[j] = obj1) {
//								obj1 = cont_row[j]->next_content;
//								cont_row[j]->to_obj(obj);
//							}
							while((obj1 = cont_row[j].Top())) {
								cont_row[j].Remove(obj1);
								obj1->to_obj(obj);
							}
							obj->to_room(rnum); /* add to inv first ... */
						} else { /* object isn't container -> empty content list */
//							for (;cont_row[j];cont_row[j] = obj1) {
//								obj1 = cont_row[j]->next_content;
//								cont_row[j]->to_room(rnum);
//							}
//							cont_row[j] = NULL;
							while((obj1 = cont_row[j].Top())) {
								cont_row[j].Remove(obj1);
								obj1->to_room(rnum);
							}
						}
					}

					if (locate < 0 && locate >= -MAX_BAG_ROW) {
						/*	let obj be part of content list but put it at the list's end
						 *	thus having the items
						 *	in the same order as before renting
						 */
						obj->from_char();
//						if ((obj1 = cont_row[-locate-1])) {
//							while (obj1->next_content)
//								obj1 = obj1->next_content;
//							obj1->next_content = obj;
//						} else
//							cont_row[-locate-1] = obj;
						cont_row[-locate-1].Prepend(obj);
					}
				}
				break;
			case '$':
				read_file = false;
				break;
			default:
				log("House file %s is damaged: bad line %s", fname, line);
				read_file = false;
		}
	}

	release_buffer(fname);
	release_buffer(line);
	fclose(fl);
	return 1;
}


/* Save all objects for a house (recursive; initial call must be followed
   by a call to House_restore_weight)  Assumes file is open already. */
void House_save(ObjData * obj, FILE * fp, int locate) {
	ObjData *tmp;

	if (obj) {
		START_ITER(iter, tmp, ObjData *, obj->contains) {
			House_save(tmp, fp, MIN(0, locate)-1);
		} END_ITER(iter);
		
		obj->save(fp, locate);

		for (tmp = obj->in_obj; tmp; tmp = tmp->in_obj)
			GET_OBJ_WEIGHT(tmp) -= GET_OBJ_WEIGHT(obj);
	}
}


/* restore weight of containers after House_save has changed them for saving */
void House_restore_weight(ObjData * obj) {
	ObjData *tmp;
	if (obj) {
		START_ITER(iter, tmp, ObjData *, obj->contains) {
			House_restore_weight(tmp);
		} END_ITER(iter);
		if (obj->in_obj)
			GET_OBJ_WEIGHT(obj->in_obj) += GET_OBJ_WEIGHT(obj);
	}
}


/* Save all objects in a house */
void House_crashsave(VNum vnum) {
	RNum rnum;
	char *buf;
	FILE *fp;
	ObjData *obj;

	if ((rnum = real_room(vnum)) == NOWHERE)
		return;
	
	buf = get_buffer(MAX_STRING_LENGTH);
	if (House_get_filename(vnum, buf)) {
		if (!(fp = fopen(buf, "wb")))
			perror("SYSERR: Error saving house file");
		else {
			START_ITER(save_iter, obj, ObjData *, world[rnum].contents) {
				House_save(obj, fp, 0);
			} END_ITER(save_iter);
			fclose(fp);
			START_ITER(weight_iter, obj, ObjData *, world[rnum].contents) {
				House_restore_weight(obj);
			} END_ITER(weight_iter);
		}
		REMOVE_BIT(ROOM_FLAGS(rnum), ROOM_HOUSE_CRASH);
	}
	release_buffer(buf);
}


/* Delete a house save file */
void House_delete_file(VNum vnum) {
	char	*buf = get_buffer(MAX_INPUT_LENGTH),
			*fname = get_buffer(MAX_INPUT_LENGTH);
	FILE *fl;

	if (House_get_filename(vnum, fname)) {
		if (!(fl = fopen(fname, "rb"))) {
			if (errno != ENOENT) {
				sprintf(buf, "SYSERR: Error deleting house file #%d. (1)", vnum);
				perror(buf);
			}
		} else {
			fclose(fl);
			if (unlink(fname) < 0) {
				sprintf(buf, "SYSERR: Error deleting house file #%d. (2)", vnum);
				perror(buf);
			}
		}
	}
	release_buffer(buf);
	release_buffer(fname);
}


/* List all objects in a house file */
void House_listrent(CharData * ch, VNum vnum) {
	FILE *fl;
	char *fname = get_buffer(MAX_STRING_LENGTH), *buf;
	ObjData *obj = NULL;
	char *line;
	bool read_file = true;
	SInt32	obj_vnum;

	if (!House_get_filename(vnum, fname)) {
		release_buffer(fname);
		return;
	}
	fl = fopen(fname, "rb");
	if (!fl) {
		ch->Send("No objects on file for house #%d.\r\n", vnum);
		release_buffer(fname);
		return;
	}
	
	buf = get_buffer(MAX_STRING_LENGTH);
	line = get_buffer(MAX_INPUT_LENGTH);
	while (get_line(fl, line) && read_file) {
		switch (*line) {
			case '#':
				if (sscanf(line, "#%d", &obj_vnum) < 1) {
					log("House file %s is damaged: no vnum", fname);
					read_file = false;
				} else {
					if (!(obj = read_object(obj_vnum, VIRTUAL))) {
						obj = new ObjData();
						obj->name = SSCreate("Empty.");
						obj->shortDesc = SSCreate("Empty.");
						obj->description = SSCreate("Empty.");
					}
					if (obj->load(fl, fname)) {
						sprintf(buf + strlen(buf), " [%5d] %-20s\r\n", GET_OBJ_VNUM(obj),
								SSData(obj->shortDesc));
					} else {
						log("House file %s is damaged: failed to load obj #%d", fname, obj_vnum);
						read_file = false;
					}
					obj->extract();
				}
				break;
			case '$':
				read_file = false;
				break;
			default:
				log("Player object file %s is damaged: bad line %s", fname, line);
				read_file = false;
		}
	}

	send_to_char(buf, ch);
	release_buffer(buf);
	release_buffer(line);
	release_buffer(fname);
	fclose(fl);
}




/******************************************************************
 *  Functions for house administration (creation, deletion, etc.  *
 *****************************************************************/

int find_house(VNum vnum) {
	int i;

	for (i = 0; i < num_of_houses; i++)
		if (house_control[i].vnum == vnum)
			return i;

	return -1;
}



/* Save the house control information */
void House_save_control(void)
{
  FILE *fl;

  if (!(fl = fopen(HCONTROL_FILE, "wb"))) {
    perror("SYSERR: Unable to open house control file");
    return;
  }
  /* write all the house control recs in one fell swoop.  Pretty nifty, eh? */
  fwrite(house_control, sizeof(struct house_control_rec), num_of_houses, fl);

  fclose(fl);
}


/* call from boot_db - will load control recs, load objs, set atrium bits */
/* should do sanity checks on vnums & remove invalid records */
void House_boot(void)
{
  struct house_control_rec temp_house;
  RNum real_house, real_atrium;
  FILE *fl;

  memset((char *)house_control,0,sizeof(struct house_control_rec)*MAX_HOUSES);

  if (!(fl = fopen(HCONTROL_FILE, "rb"))) {
    log("House control file " HCONTROL_FILE " does not exist.");
    return;
  }
  while (!feof(fl) && num_of_houses < MAX_HOUSES) {
    fread(&temp_house, sizeof(struct house_control_rec), 1, fl);

    if (feof(fl))
      break;

    if (get_name_by_id(temp_house.owner) == NULL)
      continue;			/* owner no longer exists -- skip */

    if ((real_house = real_room(temp_house.vnum)) < 0)
      continue;			/* this vnum doesn't exist -- skip */

    if ((find_house(temp_house.vnum)) >= 0)
      continue;			/* this vnum is already a hosue -- skip */

    if ((real_atrium = real_room(temp_house.atrium)) < 0)
      continue;			/* house doesn't have an atrium -- skip */

    if (temp_house.exit_num < 0 || temp_house.exit_num >= NUM_OF_DIRS)
      continue;			/* invalid exit num -- skip */

    if (TOROOM(real_house, temp_house.exit_num) != real_atrium)
      continue;			/* exit num mismatch -- skip */

    house_control[num_of_houses++] = temp_house;

    SET_BIT(ROOM_FLAGS(real_house), ROOM_HOUSE | ROOM_PRIVATE);
    SET_BIT(ROOM_FLAGS(real_atrium), ROOM_ATRIUM);
    House_load(temp_house.vnum);
  }

  fclose(fl);
  House_save_control();
}



/* "House Control" functions */

char *HCONTROL_FORMAT =
"Usage: hcontrol build <house vnum> <exit direction> <player name>\r\n"
"       hcontrol destroy <house vnum>\r\n"
"       hcontrol pay <house vnum>\r\n"
"       hcontrol show\r\n";

#define NAME(x) ((temp = get_name_by_id(x)) ? temp : "<UNDEF>")

void hcontrol_list_houses(CharData * ch) {
	int i, j;
	char *timestr, *built_on, *last_pay, *own_name, *buf;
	const char *temp;
	
	if (!num_of_houses) {
		send_to_char("No houses have been defined.\r\n", ch);
		return;
	}
	built_on = get_buffer(128);
	last_pay = get_buffer(128);
	own_name = get_buffer(128);
	buf = get_buffer(MAX_STRING_LENGTH);

	strcpy(buf, "Address  Atrium  Build Date  Guests  Owner        Last Paymt\r\n"
				"-------  ------  ----------  ------  ------------ ----------\r\n");

	for (i = 0; i < num_of_houses; i++) {
		if (house_control[i].built_on) {
			timestr = asctime(localtime(&(house_control[i].built_on)));
			*(timestr + 10) = 0;
			strcpy(built_on, timestr);
		} else
			strcpy(built_on, "Unknown");

		if (house_control[i].last_payment) {
			timestr = asctime(localtime(&(house_control[i].last_payment)));
			*(timestr + 10) = 0;
			strcpy(last_pay, timestr);
		} else
			strcpy(last_pay, "None");

		strcpy(own_name, NAME(house_control[i].owner));

		sprintf(buf + strlen(buf), "%7d %7d  %-10s    %2d    %-12s %s\r\n",
				house_control[i].vnum, house_control[i].atrium, built_on,
				house_control[i].num_of_guests, CAP(own_name), last_pay);

		if (house_control[i].num_of_guests) {
			strcat(buf, "     Guests: ");
			for (j = 0; j < house_control[i].num_of_guests; j++) {
				sprintf(own_name, "%s ", NAME(house_control[i].guests[j]));
				strcat(buf, CAP(own_name));
			}
			strcat(buf, "\r\n");
		}
	}
	send_to_char(buf, ch);
	release_buffer(buf);
	release_buffer(built_on);
	release_buffer(last_pay);
	release_buffer(own_name);
}



void hcontrol_build_house(CharData * ch, char *arg) {
	char *arg1;
	struct house_control_rec temp_house;
	RNum virt_house, real_house, real_atrium, virt_atrium, exit_num;
	long owner;

	if (num_of_houses >= MAX_HOUSES) {
		send_to_char("Max houses already defined.\r\n", ch);
		return;
	}

	arg1 = get_buffer(MAX_INPUT_LENGTH);
	/* first arg: house's vnum */
	arg = one_argument(arg, arg1);
	if (!*arg1) {
		send_to_char(HCONTROL_FORMAT, ch);
		release_buffer(arg1);
		return;
	}
	virt_house = atoi(arg1);
	release_buffer(arg1);
	if ((real_house = real_room(virt_house)) < 0) {
		send_to_char("No such room exists.\r\n", ch);
		return;
	}
	if ((find_house(virt_house)) >= 0) {
		send_to_char("House already exists.\r\n", ch);
		return;
	}

	arg1 = get_buffer(MAX_INPUT_LENGTH);
	/* second arg: direction of house's exit */
	arg = one_argument(arg, arg1);
	if (!*arg1) {
		send_to_char(HCONTROL_FORMAT, ch);
		release_buffer(arg1);
		return;
	}
	if ((exit_num = search_block(arg1, dirs, FALSE)) < 0) {
		char *buf = get_buffer(SMALL_BUFSIZE);
		sprintf(buf, "'%s' is not a valid direction.\r\n", arg1);
		send_to_char(buf, ch);
		release_buffer(buf);
		release_buffer(arg1);
		return;
	}
	release_buffer(arg1);
	if (TOROOM(real_house, exit_num) == NOWHERE) {
		char *buf = get_buffer(SMALL_BUFSIZE);
		sprintf(buf, "There is no exit %s from room %d.\r\n", dirs[exit_num], virt_house);
		send_to_char(buf, ch);
		release_buffer(buf);
		return;
	}

	real_atrium = TOROOM(real_house, exit_num);
	virt_atrium = world[real_atrium].number;

	if (TOROOM(real_atrium, rev_dir[exit_num]) != real_house) {
		send_to_char("A house's exit must be a two-way door.\r\n", ch);
		return;
	}

	arg1 = get_buffer(MAX_INPUT_LENGTH);
	/* third arg: player's name */
	arg = one_argument(arg, arg1);
	if (!*arg1) {
		send_to_char(HCONTROL_FORMAT, ch);
		release_buffer(arg1);
		return;
	}
	if ((owner = get_id_by_name(arg1)) < 0) {
		char *buf = get_buffer(SMALL_BUFSIZE);
		sprintf(buf, "Unknown player '%s'.\r\n", arg1);
		send_to_char(buf, ch);
		release_buffer(buf);
		release_buffer(arg1);
		return;
	}
	release_buffer(arg1);

	temp_house.mode = HOUSE_PRIVATE;
	temp_house.vnum = virt_house;
	temp_house.atrium = virt_atrium;
	temp_house.exit_num = exit_num;
	temp_house.built_on = time(0);
	temp_house.last_payment = 0;
	temp_house.owner = owner;
	temp_house.num_of_guests = 0;

	house_control[num_of_houses++] = temp_house;

	SET_BIT(ROOM_FLAGS(real_house), ROOM_HOUSE | ROOM_PRIVATE);
	SET_BIT(ROOM_FLAGS(real_atrium), ROOM_ATRIUM);
	House_crashsave(virt_house);

	send_to_char("House built.  Mazel tov!\r\n", ch);
	House_save_control();
}



void hcontrol_destroy_house(CharData * ch, char *arg)
{
  int i, j;
  RNum real_atrium, real_house;

  if (!*arg) {
    send_to_char(HCONTROL_FORMAT, ch);
    return;
  }
  if ((i = find_house(atoi(arg))) < 0) {
    send_to_char("Unknown house.\r\n", ch);
    return;
  }
  if ((real_atrium = real_room(house_control[i].atrium)) < 0)
    log("SYSERR: House %d had invalid atrium %d!", atoi(arg), house_control[i].atrium);
  else
    REMOVE_BIT(ROOM_FLAGS(real_atrium), ROOM_ATRIUM);

  if ((real_house = real_room(house_control[i].vnum)) < 0)
    log("SYSERR: House %d had invalid vnum %d!", atoi(arg), house_control[i].vnum);
  else
    REMOVE_BIT(ROOM_FLAGS(real_house),
	       ROOM_HOUSE | ROOM_PRIVATE | ROOM_HOUSE_CRASH);

  House_delete_file(house_control[i].vnum);

  for (j = i; j < num_of_houses - 1; j++)
    house_control[j] = house_control[j + 1];

  num_of_houses--;

  send_to_char("House deleted.\r\n", ch);
  House_save_control();

  /*
   * Now, reset the ROOM_ATRIUM flag on all existing houses' atriums,
   * just in case the house we just deleted shared an atrium with another
   * house.  --JE 9/19/94
   */
  for (i = 0; i < num_of_houses; i++)
    if ((real_atrium = real_room(house_control[i].atrium)) >= 0)
      SET_BIT(ROOM_FLAGS(real_atrium), ROOM_ATRIUM);
}


void hcontrol_pay_house(CharData * ch, char *arg)
{
  int i;

  if (!*arg)
    send_to_char(HCONTROL_FORMAT, ch);
  else if ((i = find_house(atoi(arg))) < 0)
    send_to_char("Unknown house.\r\n", ch);
  else {
    mudlogf( NRM, LVL_IMMORT, TRUE,  "Payment for house %s collected by %s.", arg, ch->RealName());

    house_control[i].last_payment = time(0);
    House_save_control();
    send_to_char("Payment recorded.\r\n", ch);
  }
}


/* The hcontrol command itself, used by imms to create/destroy houses */
ACMD(do_hcontrol) {
	char	*arg1 = get_buffer(MAX_INPUT_LENGTH),
			*arg2 = get_buffer(MAX_INPUT_LENGTH);

	half_chop(argument, arg1, arg2);

	if (is_abbrev(arg1, "build"))			hcontrol_build_house(ch, arg2);
	else if (is_abbrev(arg1, "destroy"))	hcontrol_destroy_house(ch, arg2);
	else if (is_abbrev(arg1, "pay"))		hcontrol_pay_house(ch, arg2);
	else if (is_abbrev(arg1, "show"))		hcontrol_list_houses(ch);
	else									send_to_char(HCONTROL_FORMAT, ch);
	
	release_buffer(arg1);
	release_buffer(arg2);
}


/* The house command, used by mortal house owners to assign guests */
ACMD(do_house) {
	int i, j;
	SInt32	id;
	const char	*temp;
	char	*arg = get_buffer(MAX_INPUT_LENGTH),
			*buf = get_buffer(MAX_STRING_LENGTH);


	one_argument(argument, arg);

	if (!ROOM_FLAGGED(IN_ROOM(ch), ROOM_HOUSE))
		send_to_char("You must be in your house to set guests.\r\n", ch);
	else if ((i = find_house(world[IN_ROOM(ch)].number)) < 0)
		send_to_char("Um.. this house seems to be screwed up.\r\n", ch);
	else if (GET_IDNUM(ch) != house_control[i].owner)
		send_to_char("Only the primary owner can set guests.\r\n", ch);
	else if (!*arg) {
		send_to_char("Guests of your house:\r\n", ch);
		if (house_control[i].num_of_guests == 0)
			send_to_char("  None.\r\n", ch);
		else
			for (j = 0; j < house_control[i].num_of_guests; j++) {
				strcpy(buf, NAME(house_control[i].guests[j]));
				send_to_char(strcat(CAP(buf), "\r\n"), ch);
			}
	} else if ((id = get_id_by_name(arg)) < 0)
		send_to_char("No such player.\r\n", ch);
	else {
		for (j = 0; j < house_control[i].num_of_guests; j++)
			if (house_control[i].guests[j] == id) {
				for (; j < house_control[i].num_of_guests; j++)
					house_control[i].guests[j] = house_control[i].guests[j + 1];
				house_control[i].num_of_guests--;
				House_save_control();
				send_to_char("Guest deleted.\r\n", ch);
				release_buffer(buf);
				release_buffer(arg);
				return;
			}
		j = house_control[i].num_of_guests++;
		house_control[i].guests[j] = id;
		House_save_control();
		send_to_char("Guest added.\r\n", ch);
	}
	release_buffer(buf);
	release_buffer(arg);
}



/* Misc. administrative functions */


/* crash-save all the houses */
void House_save_all(void)
{
  int i;
  RNum real_house;

  for (i = 0; i < num_of_houses; i++)
    if ((real_house = real_room(house_control[i].vnum)) != NOWHERE)
      if (ROOM_FLAGGED(real_house, ROOM_HOUSE_CRASH))
	House_crashsave(house_control[i].vnum);
}


/* note: arg passed must be house vnum, so there. */
bool House_can_enter(CharData * ch, VNum house)
{
  int i, j;

  if (STF_FLAGGED(ch, STAFF_ADMIN | STAFF_SECURITY) || (i = find_house(house)) < 0)
    return 1;

  switch (house_control[i].mode) {
  case HOUSE_PRIVATE:
    if (GET_IDNUM(ch) == house_control[i].owner)
      return 1;
    for (j = 0; j < house_control[i].num_of_guests; j++)
      if (GET_IDNUM(ch) == house_control[i].guests[j])
	return 1;
    return 0;
    break;
  }

  return 0;
}
