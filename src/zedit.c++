/************************************************************************
 *  OasisOLC - zedit.c                                            v1.5  *
 *                                                                      *
 *  Copyright 1996 Harvey Gilpin.                                       *
 ************************************************************************/

#include "characters.h"
#include "descriptors.h"
#include "objects.h"
#include "rooms.h"
#include "comm.h"
#include "utils.h"
#include "scripts.h"
#include "db.h"
#include "olc.h"
#include "buffer.h"

/*-------------------------------------------------------------------*/
/*. external data areas .*/
extern struct zone_data *zone_table;	/*. db.c	.*/
extern int top_of_zone_table;		/*. db.c	.*/
extern char *equipment_types[];		/*. constants.c	.*/
extern char *dirs[];			/*. constants.c .*/

/*-------------------------------------------------------------------*/
/* function protos */
void zedit_disp_menu(DescriptorData * d);
void zedit_setup(DescriptorData *d, RNum room_num);
void add_cmd_to_list(struct reset_com **list, struct reset_com *newcmd, int pos);
void remove_cmd_from_list(struct reset_com **list, int pos);
void delete_command(DescriptorData *d, int pos);
bool new_command(DescriptorData *d, int pos);
bool start_change_command(DescriptorData *d, int pos);
void zedit_disp_comtype(DescriptorData *d);
void zedit_disp_arg1(DescriptorData *d);
void zedit_disp_arg2(DescriptorData *d);
void zedit_disp_arg3(DescriptorData *d);
void zedit_disp_arg4(DescriptorData *d);
void zedit_save_internally(DescriptorData *d);
void zedit_save_to_disk(int zone_num);
void zedit_create_index(int znum, char *type);
void zedit_new_zone(CharData *ch, int vzone_num);
void zedit_command_menu(DescriptorData * d);
void zedit_disp_repeat(DescriptorData *d);
void zedit_parse(DescriptorData * d, char *arg);
int count_commands(struct reset_com *list);

/*-------------------------------------------------------------------*/
/*. Nasty internal macros to clean up the code .*/

#define ZCMD		(zone_table[OLC_ZNUM(d)].cmd[subcmd])
#define MYCMD		(OLC_ZONE(d)->cmd[subcmd])
#define OLC_CMD(d)	(OLC_ZONE(d)->cmd[OLC_VAL(d)])

/*-------------------------------------------------------------------*\
  utility functions 
\*-------------------------------------------------------------------*/

void zedit_setup(DescriptorData *d, RNum room_num)
{ struct zone_data *zone;
  int subcmd = 0, count = 0, cmd_room = -1;

  /*. Alloc some zone shaped space .*/
  CREATE(zone, struct zone_data, 1);

  /*. Copy in zone header info .*/
  zone->name = str_dup(zone_table[OLC_ZNUM(d)].name);
  zone->lifespan = zone_table[OLC_ZNUM(d)].lifespan;
  zone->top = zone_table[OLC_ZNUM(d)].top;
  zone->reset_mode = zone_table[OLC_ZNUM(d)].reset_mode;
  /*. The remaining fields are used as a 'has been modified' flag .*/
  zone->number = 0;   	/*. Header info has changed .*/
  zone->age = 0;	/*. Commands have changed   .*/
  
  /*. Start the reset command list with a terminator .*/
  CREATE(zone->cmd, struct reset_com, 1);
  zone->cmd[0].command = 'S';

  /*. Add all entried in zone_table that relate to this room .*/
  while(ZCMD.command != 'S')
  { switch(ZCMD.command)
    { case 'M':
      case 'O':
        cmd_room = ZCMD.arg3;
        break;
      case 'D':
      case 'R':
        cmd_room = ZCMD.arg1;
        break;
      case 'T':
        if (ZCMD.arg1 == WLD_TRIGGER)
        	cmd_room = ZCMD.arg4;
        break;
      default:
        break;
    }
    if(cmd_room == room_num)
    { add_cmd_to_list(&(zone->cmd), &ZCMD, count);
      count++;
    }
    subcmd++;
  }

  OLC_ZONE(d) = zone;
  /*. Display main menu .*/
  zedit_disp_menu(d);
}


/*-------------------------------------------------------------------*/
/*. Create a new zone .*/

void zedit_new_zone(CharData *ch, int vzone_num)
{ FILE *fp;
  struct zone_data *new_table;
  char *buf;
  int i, room, found = 0;

  if (vzone_num < 0) {
    send_to_char("You can't make negative zones.\r\n", ch);
    return;
  } else if (vzone_num > 326) {
    send_to_char("326 is the highest zone allowed.\r\n", ch);
    return;
  }

  /*. Check zone does not exist .*/
  room = vzone_num * 100;
  for (i = 0; i <= top_of_zone_table; i++)
    if ((zone_table[i].number * 100 <= room) &&
        (zone_table[i].top >= room))
    { send_to_char("A zone already covers that area.\r\n", ch);
      return;
    }

  /*. Create Zone file .*/
  buf = get_buffer(MAX_INPUT_LENGTH);
  
  sprintf(buf, ZON_PREFIX "%i.zon", vzone_num);
  
  if(!(fp = fopen(buf, "w"))) {
    mudlog("SYSERR: OLC: Can't write new zone file", BRF, LVL_CODER, TRUE);
    send_to_char("Could not write zone file.\r\n", ch);
    release_buffer(buf);
    return;
  }
  fprintf(fp, 
	"#%d\n"
	"New Zone~\n"
	"%d 30 2\n"
	"S\n"
	"$\n", 
	vzone_num,
	(vzone_num * 100) + 99
  );
  fclose(fp);

  /*. Create Rooms file .*/
  sprintf(buf, WLD_PREFIX "%d.wld", vzone_num);
  
  if(!(fp = fopen(buf, "w"))) 
  { mudlog("SYSERR: OLC: Can't write new world file", BRF, LVL_CODER, TRUE);
  send_to_char("Could not write world file.\r\n", ch);
    release_buffer(buf);
    return;
  }
  fprintf(fp, 
	"#%d\n"
    	"The Begining~\n"
	"Not much here.\n"
	"~\n"
	"%d 0 0\n"
	"S\n"
	"$\n",
	vzone_num * 100,
 	vzone_num
  );
  fclose(fp);

  /*. Create Mobiles file .*/
  
  sprintf(buf, MOB_PREFIX "%i.mob", vzone_num);
  
  if(!(fp = fopen(buf, "w")))
  { mudlog("SYSERR: OLC: Can't write new mob file", BRF, LVL_CODER, TRUE);
    release_buffer(buf);
    return;
  }
  fprintf(fp, "$\n");
  fclose(fp);

  /*. Create Objects file .*/
  sprintf(buf, OBJ_PREFIX "%i.obj", vzone_num);
  
  if(!(fp = fopen(buf, "w"))) 
  { mudlog("SYSERR: OLC: Can't write new obj file", BRF, LVL_CODER, TRUE);
    send_to_char("Could not write object file.\r\n", ch);
    release_buffer(buf);
    return;
  }
  fprintf(fp, "$\n");
  fclose(fp);

  /*. Create Shops file .*/
  sprintf(buf, SHP_PREFIX "%i.shp", vzone_num);
  
  if(!(fp = fopen(buf, "w")))
  { mudlog("SYSERR: OLC: Can't write new shop file", BRF, LVL_CODER, TRUE);
    send_to_char("Could not write shop file.\r\n", ch);
    release_buffer(buf);
    return;
  }
  fprintf(fp, "$~\n");
  fclose(fp);
  
  
  /*. Create Triggers file .*/
  sprintf(buf, TRG_PREFIX "%i.trg", vzone_num);
  
  if(!(fp = fopen(buf, "w")))
  { mudlog("SYSERR: OLC: Can't write new script file", BRF, LVL_CODER, TRUE);
    send_to_char("Could not write script file.\r\n", ch);
    release_buffer(buf);
    return;
  }
  fprintf(fp, "$~\n");
  fclose(fp);

  release_buffer(buf);
    
  /*. Update index files .*/
  zedit_create_index(vzone_num, "zon");
  zedit_create_index(vzone_num, "wld");
  zedit_create_index(vzone_num, "mob");
  zedit_create_index(vzone_num, "obj");
  zedit_create_index(vzone_num, "shp");
  zedit_create_index(vzone_num, "trg");

	/*
	 * Make a new zone in memory. This was the source of all the zedit new
	 * crashes reported to the CircleMUD list. It was happily overwriting
	 * the stack.  This new loop by Andrew Helm fixes that problem and is
	 * more understandable at the same time.
	 *
	 * The variable is 'top_of_zone_table_table + 2' because we need record 0
	 * through top_of_zone (top_of_zone_table + 1 items) and a new one which
	 * makes it top_of_zone_table + 2 elements large.
	 */
	CREATE(new_table, struct zone_data, top_of_zone_table + 2);
	new_table[top_of_zone_table + 1].number = 32000;

  if (vzone_num > zone_table[top_of_zone_table].number) {
    /*
     * We're adding to the end of the zone table, copy all of the current
     * top_of_zone_table + 1 items over and set write point to before the
     * the last record for the for() loop below.
     */
    memcpy(new_table, zone_table, (sizeof (struct zone_data) * (top_of_zone_table + 1)));
    i = top_of_zone_table + 1;
  } else
    /*
     * Copy over all the zones that are before this zone.
     */
    for (i = 0; vzone_num > zone_table[i].number; i++)
      new_table[i] = zone_table[i];

  /*
   * Ok, insert the new zone here.
   */  
  new_table[i].name = str_dup("New Zone");
  new_table[i].number = vzone_num;
  new_table[i].top = (vzone_num * 100) + 99;
  new_table[i].lifespan = 30;
  new_table[i].age = 0;
  new_table[i].reset_mode = 2;
  /*
   * No zone commands, just terminate it with an 'S'
   */
  CREATE(new_table[i].cmd, struct reset_com, 1);
  new_table[i].cmd[0].command = 'S';

  /*
   * Copy remaining zones into the table one higher, unless of course we
   * are appending to the end in which case this loop will not be used.
   */
  for (; i <= top_of_zone_table; i++)
    new_table[i + 1] = zone_table[i];

  /*
   * Look Ma, no memory leak!
   */
	FREE(zone_table);
	zone_table = new_table;
	top_of_zone_table++;

  /*
   * Previously, creating a new zone while invisible gave you away.
   * That quirk has been fixed with the MAX() statement.
   */
	mudlogf(BRF, MAX(LVL_BUILDER, GET_INVIS_LEV(ch)), TRUE,  "OLC: %s creates new zone #%d", ch->RealName(), vzone_num);
	send_to_char("Zone created successfully.\r\n", ch);
	return;
}

/*-------------------------------------------------------------------*/

void zedit_create_index(int znum, char *type)
{ FILE *newfile, *oldfile;
  char *new_name, *old_name, *prefix, *buf, *buf1;
  int num, found = FALSE;

  switch(*type)
  { case 'z':
      prefix = ZON_PREFIX;
      break;
    case 'w':
      prefix = WLD_PREFIX;
      break;
    case 'o':
      prefix = OBJ_PREFIX;
      break;
    case 'm':
      prefix = MOB_PREFIX;
      break;
    case 's':
      prefix = SHP_PREFIX;
      break;
    case 't':
      prefix = TRG_PREFIX;
      break;
    default:
      /*. Caller messed up .*/
      return;
  }
  
  new_name = get_buffer(64);
  old_name = get_buffer(64);
  sprintf(old_name, "%sindex", prefix);
  sprintf(new_name, "%snewindex", prefix);

	if(!(oldfile = fopen(old_name, "r"))) {
		mudlogf( BRF, LVL_ADMIN, TRUE,  "SYSERR: OLC: Failed to open %s", old_name);
		release_buffer(new_name);
		release_buffer(old_name);
		return;
	} else if(!(newfile = fopen(new_name, "w"))) {
		mudlogf( BRF, LVL_ADMIN, TRUE,  "SYSERR: OLC: Failed to open %s", new_name);
		fclose(oldfile);
		release_buffer(new_name);
		release_buffer(old_name);
		return;
	}
  /*. Index contents must be in order: search through the old file for
      the right place, insert the new file, then copy the rest over.
      .*/
      
  buf = get_buffer(MAX_INPUT_LENGTH);
  buf1 = get_buffer(MAX_INPUT_LENGTH);
  
  sprintf(buf1, "%d.%s", znum, type);
  while(get_line(oldfile, buf))
  { if (*buf == '$')
    { if (!found)
        fprintf(newfile,"%s\n", buf1);
        fprintf(newfile,"$\n");
      break;
    } else if (!found) {
    sscanf(buf, "%d", &num);
      if (num > znum)
      { found = TRUE;
        fprintf(newfile, "%s\n", buf1);
      }
    }
    fprintf(newfile, "%s\n", buf);
  }
  release_buffer(buf);
  release_buffer(buf1);
  
  fclose(newfile);
  fclose(oldfile);
  /*. Out with the old, in with the new .*/
  remove(old_name);
  rename(new_name, old_name);
  release_buffer(new_name);
  release_buffer(old_name);
}

/*-------------------------------------------------------------------*/

/*
 * Save all the information in the player's temporary buffer back into
 * the current zone table.
 */
void zedit_save_internally(DescriptorData *d)
{ int subcmd = 0, cmd_room = -2, room_num;

  room_num = real_room(OLC_NUM(d));

  /*
   * Delete all entries in zone_table that relate to this room so we
   * can add all the ones we have in their place.
   */
  while(ZCMD.command != 'S')
  { switch(ZCMD.command)
    { case 'M':
      case 'O':
        cmd_room = ZCMD.arg3;
        break;
      case 'D':
      case 'R':
        cmd_room = ZCMD.arg1;
        break;
      case 'T':
        if (ZCMD.arg1 == WLD_TRIGGER)
        	cmd_room = ZCMD.arg4;
      default:
        break;
    }
    if(cmd_room == room_num)
      remove_cmd_from_list(&(zone_table[OLC_ZNUM(d)].cmd), subcmd);
    else
      subcmd++;
  }

	/*. Now add all the entries in the players descriptor list .*/
	subcmd = 0;
	while(MYCMD.command != 'S') {
		add_cmd_to_list(&(zone_table[OLC_ZNUM(d)].cmd), &MYCMD, subcmd);
		subcmd++;
	}

	/*. Finally, if zone headers have been changed, copy over .*/
	if (OLC_ZONE(d)->number) {
		FREE(zone_table[OLC_ZNUM(d)].name);
		zone_table[OLC_ZNUM(d)].name		= str_dup(OLC_ZONE(d)->name);
		zone_table[OLC_ZNUM(d)].top			= OLC_ZONE(d)->top;
		zone_table[OLC_ZNUM(d)].reset_mode	= OLC_ZONE(d)->reset_mode;
		zone_table[OLC_ZNUM(d)].lifespan	= OLC_ZONE(d)->lifespan;
	}
	olc_add_to_save_list(zone_table[OLC_ZNUM(d)].number, OLC_SAVE_ZONE);
}


/*-------------------------------------------------------------------*/
/*. Save all the zone_table for this zone to disk.  Yes, this could
    automatically comment what it saves out, but why bother when you
    have an OLC as cool as this ? :> .*/
#undef ZCMD
#define ZCMD		(zone_table[zone_num].cmd[subcmd])

void zedit_save_to_disk(int zone_num) {
	int subcmd, arg1 = -1, arg2 = -1, arg3 = -1, arg4 = -1;
	char	*buf, *fname = get_buffer(64), *bldr;
	FILE *zfile;
	const char *comment;

	sprintf(fname, ZON_PREFIX "%i.new", zone_table[zone_num].number);

	if(!(zfile = fopen(fname, "w"))) {
		mudlogf(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: zedit_save_to_disk:  Can't write zone %d.", 
				zone_table[zone_num].number);
		release_buffer(fname);
		return;
	}
  
	//	Print zone header to file
	fprintf(zfile, 
			"#%d\n"
			"%s~\n"
			"%d %d %d\n",
			zone_table[zone_num].number,
			zone_table[zone_num].name ? zone_table[zone_num].name : "undefined",
			zone_table[zone_num].top, zone_table[zone_num].lifespan, zone_table[zone_num].reset_mode
	);
#if defined(ZEDIT_HELP_IN_FILE)
	fprintf(zfile,
		"* Field #1    Field #3   Field #4  Field #5   FIELD #6\n"
		"* M (Mobile)  Mob-Vnum   Wld-Max   Room-Vnum\n"
		"* O (Object)  Obj-Vnum   Wld-Max   Room-Vnum\n"
		"* G (Give)    Obj-Vnum   Wld-Max   Unused\n"
		"* E (Equip)   Obj-Vnum   Wld-Max   EQ-Position\n"
		"* P (Put)     Obj-Vnum   Wld-Max   Target-Obj-Vnum\n"
		"* D (Door)    Room-Vnum  Door-Dir  Door-State\n"
		"* R (Remove)  Room-Vnum  Obj-Vnum  Unused\n"
		"* C (Command) String...\n"
		"* Z (Maze)    Unused     Unused    Unused\n"
		"* T (Trigger) Trig Type  Trg-VNum  Wld-Max    [Room-VNum]\n"
	);
#endif
  
	LListIterator<char *>	iter(zone_table[zone_num].builders);
	while ((bldr = iter.Next()))
		if(bldr && (*bldr != '*'))
			fprintf(zfile, "* Builder %s\n", bldr);

	for(subcmd = 0; ZCMD.command != 'S'; subcmd++) {
		arg1 = -1;
		arg2 = -1;
		arg3 = -1;
		arg4 = -1;
		switch (ZCMD.command) {
			case 'M':
				arg1 = mob_index[ZCMD.arg1].vnum;
				arg2 = ZCMD.arg2;
				arg3 = world[ZCMD.arg3].number;
				comment = SSData(((CharData *)mob_index[ZCMD.arg1].proto)->general.shortDesc);
				break;
			case 'O':
				arg1 = obj_index[ZCMD.arg1].vnum;
				arg2 = ZCMD.arg2;
				arg3 = world[ZCMD.arg3].number;
				comment = SSData(((ObjData *)obj_index[ZCMD.arg1].proto)->shortDesc);
				break;
			case 'G':
			 	arg1 = obj_index[ZCMD.arg1].vnum;
				arg2 = ZCMD.arg2;
				arg3 = -1;
				comment = SSData(((ObjData *)obj_index[ZCMD.arg1].proto)->shortDesc);
				break;
			case 'E':
				arg1 = obj_index[ZCMD.arg1].vnum;
				arg2 = ZCMD.arg2;
				arg3 = ZCMD.arg3;
				comment = SSData(((ObjData *)obj_index[ZCMD.arg1].proto)->shortDesc);
				break;
			case 'P':
				arg1 = obj_index[ZCMD.arg1].vnum;
				arg2 = ZCMD.arg2;
				arg3 = obj_index[ZCMD.arg3].vnum;
				comment = SSData(((ObjData *)obj_index[ZCMD.arg1].proto)->shortDesc);
				break;
			case 'D':
				arg1 = world[ZCMD.arg1].number;
				arg2 = ZCMD.arg2;
				arg3 = ZCMD.arg3;
				comment = world[ZCMD.arg1].GetName("<ERROR>");
				break;
			case 'R':
				arg1 = world[ZCMD.arg1].number;
				arg2 = obj_index[ZCMD.arg2].vnum;
				arg3 = -1;
				comment = SSData(((ObjData *)obj_index[ZCMD.arg2].proto)->shortDesc);
				break;
			case 'Z':
				arg1 = ZCMD.arg1;
				arg2 = ZCMD.arg2;
				arg3 = ZCMD.arg3;
				comment = "Automagically Generated Maze.";
				break;
			case 'T':
				arg1 = ZCMD.arg1;
				arg2 = trig_index[ZCMD.arg2].vnum;
				arg3 = ZCMD.arg3;
				arg4 = (ZCMD.arg1 == WLD_TRIGGER) ? world[ZCMD.arg4].number : 0;
				comment = SSData(((TrigData *)trig_index[ZCMD.arg2].proto)->name);
				break;
			case 'C':
				break;
			case '*':
				//	Invalid commands are replaced with '*' - Ignore them
				continue;
			default:
				mudlogf(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: z_save_to_disk(): Unknown cmd '%c' - NOT saving", ZCMD.command);
				continue;
		}
#if defined(ZEDIT_HELP_IN_FILE)
		switch(ZCMD.command) {
//			case 'G':
//			case 'R':
//				fprintf(zfile, "%c %d %d %d %d \t(%s)\n", ZCMD.command, ZCMD.if_flag, arg1, arg2, ZCMD.repeat, comment);
//				break;
//			case 'O':
//			case 'P':
//				fprintf(zfile, "%c %d %d %d %d %d \t(%s)\n", ZCMD.command, ZCMD.if_flag, arg1, arg2, arg3, ZCMD.repeat, comment);
//				break;
			case 'C':
				fprintf(zfile, "%c %d %s\n", ZCMD.command, ZCMD.if_flag, ZCMD.command_string);
				break;
//			case 'D':
//			case 'E':
//			case 'M':
//			case 'T':
//			case 'Z':
			default:
				fprintf(zfile, "%c %d %d %d %d %d %d \t(%s)\n", ZCMD.command, ZCMD.if_flag, arg1, arg2, arg3, arg4, ZCMD.repeat, comment);
				break;				
		}
#else
		switch(ZCMD.command) {
//			case 'G':
//			case 'R':
//				fprintf(zfile, "%c %d %d %d %d\n", ZCMD.command, ZCMD.if_flag, arg1, arg2, ZCMD.repeat);
//				break;
//			case 'O':
//			case 'P':
//				fprintf(zfile, "%c %d %d %d %d %d\n", ZCMD.command, ZCMD.if_flag, arg1, arg2, arg3, ZCMD.repeat);
//				break;
			case 'C':
				fprintf(zfile, "%c %d %s\n", ZCMD.command, ZCMD.if_flag, ZCMD.command_string);
				break;
//			case 'D':
//			case 'E':
//			case 'M':
//			case 'T':
//			case 'Z':
			default:
				fprintf(zfile, "%c %d %d %d %d %d %d\n", ZCMD.command, ZCMD.if_flag, arg1, arg2, arg3, arg4, ZCMD.repeat);
				break;				
		}
#endif
	}
	fprintf(zfile, "S\n$\n");
	fclose(zfile);

	buf = get_buffer(MAX_STRING_LENGTH);
	sprintf(buf, ZON_PREFIX "%d.zon", zone_table[zone_num].number);
	/*
	* We're fubar'd if we crash between the two lines below.
	*/
	remove(buf);
	rename(fname, buf);

	release_buffer(fname);
	release_buffer(buf);
	olc_remove_from_save_list(zone_table[zone_num].number, OLC_SAVE_ZONE);
}

/*-------------------------------------------------------------------*/

/*
 * Some common code to count the number of comands in the list.
 */
int count_commands(struct reset_com *list) {
	int count = 0;
	while (list[count].command != 'S')	count++;
	return count;
}

/*-------------------------------------------------------------------*/

/*. Adds a new reset command into a list.  Takes a pointer to the list
    so that it may play with the memory locations.*/

void add_cmd_to_list(struct reset_com **list, struct reset_com *newcmd, int pos) {
	int count = 0, i, l;
	struct reset_com *newlist;

	//	Count number of commands (not including terminator).
	count = count_commands(*list);

	CREATE(newlist, struct reset_com, count + 2);

	//	Tight loop to copy old list and insert new command
	l = 0;
	for(i = 0; i <= count;i++)
		newlist[i] = ((i == pos) ? *newcmd : (*list)[l++]);

	//	Add terminator then insert new list
	newlist[count+1].command = 'S';
	FREE(*list);
	*list = newlist;
}

/*-------------------------------------------------------------------*/
//	Remove a reset command from a list.  Takes a pointer to the list
//	so that it may play with the memory locations

void remove_cmd_from_list(struct reset_com **list, int pos) {
	int count = 0, i, l;
	struct reset_com *newlist;

	//	Count number of commands (not including terminator)  
	count = count_commands(*list);

	CREATE(newlist, struct reset_com, count);

	//	Tight loop to copy old list and skip unwanted command
	l = 0;
	for(i = 0; i < count; i++)
		if(i != pos)
			newlist[l++] = (*list)[i];

	if(((*list)[i].command == 'C') && (*list)[i].command_string)
		FREE((*list)[i].command_string);			// Can't use the FREE macro here.

	//	Add terminator then insert new list
	newlist[count-1].command = 'S';
	FREE(*list);
	*list = newlist;
}

/*-------------------------------------------------------------------*/
//	Error check user input and then add new (blank) command

bool new_command(DescriptorData *d, int pos) {
	int subcmd = 0;
	struct reset_com *new_com;

	//	Error check to ensure users hasn't given too large an index
	while(MYCMD.command != 'S')	subcmd++;

	if ((pos > subcmd) || (pos < 0))
		return false;

	//	Ok, let's add a new (blank) command.
	CREATE(new_com, struct reset_com, 1);
	new_com->command = 'N'; 
	add_cmd_to_list(&OLC_ZONE(d)->cmd, new_com, pos);
	return true;
}


/*-------------------------------------------------------------------*/
//	Error check user input and then remove command

void delete_command(DescriptorData *d, int pos) {
	int subcmd = 0;

	//	Error check to ensure users hasn't given too large an index
	while(MYCMD.command != 'S')	subcmd++;

	if ((pos >= subcmd) || (pos < 0))
		return;

	//	Ok, let's zap it
	remove_cmd_from_list(&OLC_ZONE(d)->cmd, pos);
}

/*-------------------------------------------------------------------*/
//	Error check user input and then setup change

bool start_change_command(DescriptorData *d, int pos) {
	int subcmd = 0;

	//	Error check to ensure users hasn't given too large an index
	while(MYCMD.command != 'S')	subcmd++;

	if ((pos >= subcmd) || (pos < 0))
		return false;

	//	Ok, let's get editing
	OLC_VAL(d) = pos;
	return true;
}

/**************************************************************************
 Menu functions 
 **************************************************************************/

void zedit_command_menu(DescriptorData * d) {
	char *cmdtype;
	
	switch(OLC_CMD(d).command) {
		case 'M':
			cmdtype = "Load Mobile";
			break;
		case 'G':
			cmdtype = "Give Object to Mobile";
			break;
		case 'C':
			cmdtype = "Command Mobile";
			break;
		case 'O':
			cmdtype = "Load Object";
			break;
		case 'E':
			cmdtype = "Equip Mobile with Object";
			break;
		case 'P':
			cmdtype = "Put Object in Object";
			break;
		case 'R':
			cmdtype = "Remove Object";
			break;
		case 'D':
			cmdtype = "Set Door";
			break;
		case 'T':
			cmdtype = "Trigger";
			break;
		case 'Z':
			cmdtype = "Randomized Zone";
			break;
		case '*':
			cmdtype = "<Comment>";
			break;
		default:
			cmdtype = "<Unknown>";
	}
	d->character->Send(
				"\x1B[H\x1B[J"
				"`GC`n) Command   : %s\r\n"
				"`GD`n) Dependent : %s\r\n",
				cmdtype,
				OLC_CMD(d).if_flag ? "YES" : "NO");
		
	/* arg1 */
	switch(OLC_CMD(d).command) {
		case 'M':
			d->character->Send("`G1`n) Mobile    : `y%s [`c%d`y]\r\n",
					SSData(((CharData *)mob_index[OLC_CMD(d).arg1].proto)->general.shortDesc),
					mob_index[OLC_CMD(d).arg1].vnum);
			break;
		case 'G':
		case 'O':
		case 'E':
		case 'P':
			d->character->Send("`G1`n) Object    : `y%s [`c%d`y]\r\n",
					SSData(((ObjData *)obj_index[OLC_CMD(d).arg1].proto)->shortDesc),
					obj_index[OLC_CMD(d).arg1].vnum);
			break;
		case 'C':
			d->character->Send("`G1`n) Command   : `y%s\r\n", OLC_CMD(d).command_string);
			break;
	}
	
	/* arg2 */
	switch(OLC_CMD(d).command) {
		case 'M':
		case 'G':
		case 'O':
		case 'E':
		case 'P':
			d->character->Send("`G2`n) Maximum   : `c%d\r\n",
					OLC_CMD(d).arg2);
			break;
		case 'R':
			d->character->Send("`G2`n) Object    : `y%s [`c%d`y]\r\n",
					SSData(((ObjData *)obj_index[OLC_CMD(d).arg2].proto)->shortDesc),
					obj_index[OLC_CMD(d).arg2].vnum);
			break;
		case 'D':
			d->character->Send("`G2`n) Door      : `c%s\r\n",
					dirs[OLC_CMD(d).arg2]);
			break;
		case 'T':
			d->character->Send("`G2`n) Script    : `y%s [`c%d`y]\r\n",
					SSData(((TrigData *)trig_index[OLC_CMD(d).arg2].proto)->name),
					trig_index[OLC_CMD(d).arg2].vnum);
			break;
	}
	
	/* arg3 */
	switch(OLC_CMD(d).command) {
		case 'E':
			d->character->Send("`G3`n) Location  : `y%s\r\n",
				equipment_types[OLC_CMD(d).arg3]);
			break;
		case 'P':
			d->character->Send("`G3`n) Container : `y%s [`c%d`y]\r\n",
				SSData(((ObjData *)obj_index[OLC_CMD(d).arg3].proto)->shortDesc),
				obj_index[OLC_CMD(d).arg3].vnum);
			break;
		case 'D':
			d->character->Send("`G3`n) Door state: `y%s\r\n",
					OLC_CMD(d).arg3 ? (OLC_CMD(d).arg3 == 1 ? "Closed" : "Locked") : "Open");
			break;
		case 'T':
			d->character->Send("`G3`n) Maximum   : `c%d\r\n",
					OLC_CMD(d).arg3);
			break;
	}
	
 	/* arg4 */

	switch(OLC_CMD(d).command) {
		case 'M':
			d->character->Send("`G4`n) Max/zone  : `c%d\r\n",
					OLC_CMD(d).arg4);
			break;
	}

	if (strchr("GORP", OLC_CMD(d).command) != NULL)
		d->character->Send("`GR`n) Repeat    : `c%d\r\n", OLC_CMD(d).repeat);
	send_to_char(
			"`GQ`n) Quit\r\n\n"
			"`CEnter your choice :`n ",
			d->character);

	OLC_MODE(d) = ZEDIT_COMMAND_MENU;
}


/* the main menu */
void zedit_disp_menu(DescriptorData * d) {
	int subcmd = 0, room, counter = 0;
	char *buf = get_buffer(MAX_STRING_LENGTH);
  
	room = real_room(OLC_NUM(d));

  /*. Menu header .*/
	d->character->Send(
#if defined(CLEAR_SCREEN)
			"\x1B[H\x1B[J"
#endif
			"Room number: `c%d`n		Room zone: `c%d\r\n"
			"`GZ`n) Zone name   : `y%s\r\n"
			"`GL`n) Lifespan    : `y%d minutes\r\n"
			"`GT`n) Top of zone : `y%d\r\n"
			"`GR`n) Reset Mode  : `y%s\r\n"
			"`n[Command list]\r\n",

			OLC_NUM(d), zone_table[OLC_ZNUM(d)].number,
			OLC_ZONE(d)->name ? OLC_ZONE(d)->name : "<NONE!>",
			OLC_ZONE(d)->lifespan,
			OLC_ZONE(d)->top,
			OLC_ZONE(d)->reset_mode ? ((OLC_ZONE(d)->reset_mode == 1) ?
				"Reset when no players are in zone." : 
				"Normal reset.") :
				"Never reset");

	/*. Print the commands for this room into display buffer .*/
	while(MYCMD.command != 'S') {	// Translate what the command means
		if (strchr("GORP", MYCMD.command) != NULL && MYCMD.repeat > 1)
			sprintf(buf, "(x%2d) ", MYCMD.repeat);
		else
			strcpy(buf, "");
			
		switch(MYCMD.command) {
			case'M':
				sprintf(buf, "Load %s [`c%d`y]",
						SSData(((CharData *)mob_index[MYCMD.arg1].proto)->general.shortDesc),
						mob_index[MYCMD.arg1].vnum);
				break;
			case'G':
				sprintf(buf + strlen(buf), "Give it %s [`c%d`y]",
						SSData(((ObjData *)obj_index[MYCMD.arg1].proto)->shortDesc),
						obj_index[MYCMD.arg1].vnum);
				break;
			case'C':
				sprintf(buf, "Do command: `c%s`y", MYCMD.command_string);
				break;
			case'O':
				sprintf(buf + strlen(buf), "Load %s [`c%d`y]",
						SSData(((ObjData *)obj_index[MYCMD.arg1].proto)->shortDesc),
					obj_index[MYCMD.arg1].vnum);
				break;
			case'E':
				sprintf(buf, "Equip with %s [`c%d`y], %s",
						SSData(((ObjData *)obj_index[MYCMD.arg1].proto)->shortDesc),
						obj_index[MYCMD.arg1].vnum,
						equipment_types[MYCMD.arg3]);
				break;
			case'P':
				sprintf(buf + strlen(buf), "Put %s [`c%d`y] in %s [`c%d`y]",
						SSData(((ObjData *)obj_index[MYCMD.arg1].proto)->shortDesc),
						obj_index[MYCMD.arg1].vnum,
						SSData(((ObjData *)obj_index[MYCMD.arg3].proto)->shortDesc),
						obj_index[MYCMD.arg3].vnum);
				break;
			case'R':
				sprintf(buf + strlen(buf), "Remove %s [`c%d`y] from room.",
						SSData(((ObjData *)obj_index[MYCMD.arg2].proto)->shortDesc),
						obj_index[MYCMD.arg2].vnum);
				break;
			case'D':
				sprintf(buf, "Set door %s as %s.",
						dirs[MYCMD.arg2],
						MYCMD.arg3 ? ((MYCMD.arg3 == 1) ? "closed" : "locked") : "open");
				break;
			case 'T':
				sprintf(buf, "Attach script %s [`c%d`y]",
						SSData(((TrigData *)trig_index[MYCMD.arg2].proto)->name),
						MYCMD.arg2);
				break;
			case 'Z':
				strcpy(buf, "Zone is Randomized");
				break;
			case '*':
				strcpy(buf, "<Comment>");
				break;
			default:
				strcpy(buf, "<Unknown Command>");
				break;
		}
		/* Build the display buffer for this command */
		d->character->Send("`n%-2d - `y%s%s",
				counter++,
				MYCMD.if_flag ? " then " : "",
				buf);
		if (strchr("EGMOP", MYCMD.command) != NULL && MYCMD.arg2 > 0)
			d->character->Send(", Max : %d", MYCMD.arg2);
		else if (MYCMD.command == 'T' && MYCMD.arg3 > 0)
			d->character->Send(", Max : %d", MYCMD.arg3);
		send_to_char("\r\n", d->character);
		subcmd++;
	}
	/* Finish off menu */
	d->character->Send(
			"`n%-2d - <END OF LIST>\r\n"
			"`GN`y) New command.\r\n"
			"`GE`y) Edit a command.\r\n"
			"`GD`y) Delete a command.\r\n"
			"`GQ`y) Quit\r\n"
			"`CEnter your choice:`n ",
			counter);

	OLC_MODE(d) = ZEDIT_MAIN_MENU;
	release_buffer(buf);
}


/*-------------------------------------------------------------------*/
//	Print the command type menu and setup response catch.

void zedit_disp_comtype(DescriptorData *d) {
#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	send_to_char(
			"`gM`n) Load Mobile to room             `gO`n) Load Object to room\r\n"
			"`gE`n) Equip mobile with object        `gG`n) Give an object to a mobile\r\n"
			"`gP`n) Put object in another object    `gD`n) Open/Close/Lock a Door\r\n"
			"`gR`n) Remove an object from the room  `gC`n) Force mobile to do command\r\n"
			"`gT`n) Attach Script\r\n"
			"What sort of command will this be? : ",
			d->character);
	OLC_MODE(d) = ZEDIT_COMMAND_TYPE;
}


/*-------------------------------------------------------------------*/
//	Print the appropriate message for the command type for arg1 and set
//	up the input catch clause

void zedit_disp_arg1(DescriptorData *d) {
	switch(OLC_CMD(d).command) {
		case 'M':
			send_to_char("Input mob's vnum : ", d->character);
			OLC_MODE(d) = ZEDIT_ARG1;
			break;
		case 'O':
		case 'E':
		case 'P':
		case 'G':
			send_to_char("Input object vnum : ", d->character);
			OLC_MODE(d) = ZEDIT_ARG1;
			break;
		case 'D':
		case 'R':
			/*. Arg1 for these is the room number, skip to arg2 .*/
			OLC_CMD(d).arg1 = real_room(OLC_NUM(d));
			zedit_disp_arg2(d);
			break;
		case 'C':
			send_to_char("Command : ", d->character);
			OLC_MODE(d) = ZEDIT_ARG1;
			break;
		default:
			//	We should never get here
			//	cleanup_olc(d, CLEANUP_ALL);
			mudlog("SYSERR: OLC: zedit_disp_arg1(): Help!", BRF, LVL_BUILDER, TRUE);
			send_to_char("Oops...\r\n", d->character);
	}
}

    

/*-------------------------------------------------------------------*/
/*. Print the appropriate message for the command type for arg2 and set
    up the input catch clause .*/

void zedit_disp_arg2(DescriptorData *d) {
	int i = 0;

	switch(OLC_CMD(d).command) {
		case 'M':
		case 'O':
		case 'E':
		case 'P':
		case 'G':
			send_to_char("Input the maximum number that can exist: ", d->character);
			break;
		case 'D':
			while(*dirs[i] != '\n') {
				d->character->Send("%d) Exit %s.\r\n", i, dirs[i]);
				i++;
			}
			send_to_char("Enter exit number for door : ", d->character);
			break;
		case 'R':
			send_to_char("Input object's vnum : ", d->character);
			break;
		case 'T':
			send_to_char("Input script's vnum : ", d->character);
			break;
		default:
			cleanup_olc(d, CLEANUP_ALL);
			mudlog("SYSERR: OLC: zedit_disp_arg2(): Help!", BRF, LVL_BUILDER, TRUE);
			return;
	}
	OLC_MODE(d) = ZEDIT_ARG2;
}


/*-------------------------------------------------------------------*/
/*. Print the appropriate message for the command type for arg3 and set
    up the input catch clause .*/

void zedit_disp_arg3(DescriptorData *d) {
	int i = 0;
	
	switch(OLC_CMD(d).command) { 
		case 'E':
			while(*equipment_types[i] != '\n') {
				d->character->Send("%2d) %26.26s %2d) %26.26s\r\n", i, 
						equipment_types[i], i+1, (*equipment_types[i+1] != '\n') ?
						equipment_types[i+1] : "");
				if(*equipment_types[i+1] != '\n')
					i+=2;
				else
					break;
			}
			send_to_char("Input location to equip : ", d->character);
			break;
		case 'P':
			send_to_char("Input the vnum of the container : ", d->character);
			break;
		case 'D':
			send_to_char(
					"0)  Door open\r\n"
					"1)  Door closed\r\n"
					"2)  Door locked\r\n" 
					"Enter state of the door : ", d->character);
			break;
		case 'T':
			send_to_char("Input the maximum number that can exist on the mud : ", d->character);
			break;
		default:
			cleanup_olc(d, CLEANUP_ALL);
			mudlog("SYSERR: OLC: zedit_disp_arg3(): Help!", BRF, LVL_BUILDER, TRUE);
			return;
	}
	OLC_MODE(d) = ZEDIT_ARG3;
}

    
void zedit_disp_arg4(DescriptorData *d) {
	int i = 0;

	switch(OLC_CMD(d).command) { 
		case 'M':
			send_to_char("Input the maximum number that can exist in the zone : ", d->character);
			break;
		default:
			cleanup_olc(d, CLEANUP_ALL);
			mudlog("SYSERR: OLC: zedit_disp_arg3(): Help!", BRF, LVL_BUILDER, TRUE);
			return;
	}
	OLC_MODE(d) = ZEDIT_ARG4;
}


void zedit_disp_repeat(DescriptorData *d) {
	switch(OLC_CMD(d).command) { 
		case 'O':
		case 'G':
		case 'P':
		case 'R':
			send_to_char("Enter number of times to repeat (0 or 1 to do once) : ", d->character);
			break;
		default:
			zedit_disp_menu(d);
			return;
	}
	OLC_MODE(d) = ZEDIT_REPEAT;
}


/**************************************************************************
  The GARGANTAUN event handler
 **************************************************************************/

void zedit_parse(DescriptorData * d, char *arg) {
	int pos, i = 0;
	switch (OLC_MODE(d)) {
/*-------------------------------------------------------------------*/
		case ZEDIT_CONFIRM_SAVESTRING:
			switch (*arg) {
				case 'y':
				case 'Y':
					//	Save the zone in memory
					send_to_char("Saving zone info in memory.\r\n", d->character);
					zedit_save_internally(d);
					mudlogf(NRM, LVL_BUILDER, TRUE, "OLC: %s edits zone info for room %d",
							d->character->RealName(), OLC_NUM(d));
					// FALL THROUGH
				case 'n':
				case 'N':
					cleanup_olc(d, CLEANUP_ALL);
					break;
				default:
					send_to_char("Invalid choice!\r\n", d->character);
					send_to_char("Do you wish to save the zone info? : ", d->character);
					break;
			}
			break; /* end of ZEDIT_CONFIRM_SAVESTRING */

/*-------------------------------------------------------------------*/
		case ZEDIT_MAIN_MENU:
			switch (*arg) {
				case 'q':
				case 'Q':
					if(OLC_ZONE(d)->age || OLC_ZONE(d)->number) {
						send_to_char("Do you wish to save the changes to the zone info? (y/n) : ", d->character);
						OLC_MODE(d) = ZEDIT_CONFIRM_SAVESTRING;
					} else {
						send_to_char("No changes made.\r\n", d->character);
						cleanup_olc(d, CLEANUP_ALL);
					}
					break;
				case 'n':	//	New entry
				case 'N':
					send_to_char("What number in the list should the new command be? : ", d->character);
					OLC_MODE(d) = ZEDIT_NEW_ENTRY;
					break;
				case 'e':	//	Change an entry 
				case 'E':
					send_to_char("Which command do you wish to change? : ", d->character);
					OLC_MODE(d) = ZEDIT_CHANGE_ENTRY;
					break;
				case 'd':	//	Delete an entry
				case 'D':
					send_to_char("Which command do you wish to delete? : ", d->character);
					OLC_MODE(d) = ZEDIT_DELETE_ENTRY;
					break;
				case 'z':	//	Edit zone name
				case 'Z':
					send_to_char("Enter new zone name : ", d->character);
					OLC_MODE(d) = ZEDIT_ZONE_NAME;
					break;
				case 't':	//	Edit zone top
				case 'T':
					if(GET_LEVEL(d->character) < LVL_SRSTAFF)
						zedit_disp_menu(d);
					else {
						send_to_char("Enter new top of zone : ", d->character);
						OLC_MODE(d) = ZEDIT_ZONE_TOP;
					}
					break;
				case 'l':	//	Edit zone lifespan
				case 'L':
					send_to_char("Enter new zone lifespan : ", d->character);
					OLC_MODE(d) = ZEDIT_ZONE_LIFE;
					break;
				case 'r':	//	Edit zone reset mode
				case 'R':
					send_to_char(
							"\r\n"
							"0) Never reset\r\n"
							"1) Reset only when no players in zone\r\n"
							"2) Normal reset\r\n"
							"Enter new zone reset type : ", d->character);
					OLC_MODE(d) = ZEDIT_ZONE_RESET;
					break;
				default:
					zedit_disp_menu(d);
					break;
			}
			break;

/*-------------------------------------------------------------------*/
		case ZEDIT_NEW_ENTRY:
			/*. Get the line number and insert the new line .*/
			pos = atoi(arg);
			if (isdigit(*arg) && new_command(d, pos)) {
				if (start_change_command(d, pos)) {
					zedit_disp_comtype(d);
					OLC_ZONE(d)->age = 1;
				}
			} else
				zedit_disp_menu(d);
			break;

/*-------------------------------------------------------------------*/
		case ZEDIT_DELETE_ENTRY:
			/*. Get the line number and delete the line .*/
			pos = atoi(arg);
			if(isdigit(*arg)) {
				delete_command(d, pos);
				OLC_ZONE(d)->age = 1;
			}
			zedit_disp_menu(d);
			break;

/*-------------------------------------------------------------------*/
		case ZEDIT_CHANGE_ENTRY:
			//	Parse the input for which line to edit, and goto next quiz
			pos = atoi(arg);
			if(isdigit(*arg) && start_change_command(d, pos))
				zedit_command_menu(d);
			else
				zedit_disp_menu(d);
			break;

/*-------------------------------------------------------------------*/
		case ZEDIT_COMMAND_TYPE:
			//	Parse the input for which type of command this is, and goto next quiz
			OLC_CMD(d).command = toupper(*arg);
			if (!OLC_CMD(d).command || (strchr("MOPEDGTRC", OLC_CMD(d).command) == NULL))
				send_to_char("Invalid choice, try again : ", d->character);
			else {
				OLC_CMD(d).arg1 = 0;
				OLC_CMD(d).arg2 = 0;
				OLC_CMD(d).arg3 = 0;
				OLC_CMD(d).arg4 = 0;
				switch (OLC_CMD(d).command) {
					case 'C':
						OLC_CMD(d).command_string = str_dup("say I need a command!");
						break;
					case 'O':
					case 'M':	OLC_CMD(d).arg3 = real_room(OLC_NUM(d));	break;
					case 'R':
					case 'D':	OLC_CMD(d).arg1 = real_room(OLC_NUM(d));	break;
					case 'T':	OLC_CMD(d).arg4 = real_room(OLC_NUM(d));	break;
				}
				zedit_command_menu(d);
			}
			break;

/*-------------------------------------------------------------------*/
		case ZEDIT_IF_FLAG:
			//	Parse the input for the if flag, and goto next quiz
			switch(*arg) {
				case 'y':
				case 'Y':	OLC_CMD(d).if_flag = 1;							break;
				case 'n':
				case 'N':	OLC_CMD(d).if_flag = 0;							break;
				default:	send_to_char("Try again : ", d->character);		return;
			}
			zedit_command_menu(d);
			break;


/*-------------------------------------------------------------------*/
		case ZEDIT_ARG1:
			//	Parse the input for arg1, and goto next quiz
			if (OLC_CMD(d).command == 'C') {
				OLC_CMD(d).command_string = str_dup(arg);
				zedit_command_menu(d);
				break;
			}
			if  (!isdigit(*arg)) {
				send_to_char("Must be a numeric value, try again : ", d->character);
				return;
			}
			switch(OLC_CMD(d).command) {
				case 'M':
					if ((pos = real_mobile(atoi(arg))) >= 0) {
						OLC_CMD(d).arg1 = pos;
						zedit_command_menu(d);
					} else
						send_to_char("That mobile does not exist, try again : ", d->character);
					break;
				case 'O':
				case 'P':
				case 'E':
				case 'G':
					if ((pos = real_object(atoi(arg))) >= 0) {
						OLC_CMD(d).arg1 = pos;
						zedit_command_menu(d);
					} else
						send_to_char("That object does not exist, try again : ", d->character);
					break;
				case 'D':
				case 'R':
				case 'T':
				case 'Z':
				default:
					//	We should never get here
					//	cleanup_olc(d, CLEANUP_ALL);
					mudlog("SYSERR: OLC: zedit_parse(): case ARG1: Ack!", BRF, LVL_BUILDER, TRUE);
					send_to_char("Oops...\r\n", d->character);
					break;
			}
			break;


/*-------------------------------------------------------------------*/
		case ZEDIT_ARG2:
			//	Parse the input for arg2, and goto next quiz
			if  (!isdigit(*arg)) {
				send_to_char("Must be a numeric value, try again : ", d->character);
				return;
			}
			switch(OLC_CMD(d).command) {
				case 'M':
					OLC_CMD(d).arg2 = atoi(arg);
					zedit_command_menu(d);
					break;
				case 'O':
				case 'G':
				case 'P':
				case 'E':
					OLC_CMD(d).arg2 = atoi(arg);
					zedit_command_menu(d);
					break;
				case 'D':
					pos = atoi(arg);
					while(*dirs[i] != '\n') i++;		//	Count dirs
					if ((pos < 0) || (pos > i))
						send_to_char("Try again : ", d->character);
					else {
						OLC_CMD(d).arg2 = pos;
						zedit_command_menu(d);
					}
					break;
				case 'R':
					if ((pos = real_object(atoi(arg))) >= 0) {
						OLC_CMD(d).arg2 = pos;
						zedit_command_menu(d);
					} else
						send_to_char("That object does not exist, try again : ", d->character);
					break;
				case 'T':
					if ((pos = real_trigger(atoi(arg))) >= 0) {
						OLC_CMD(d).arg2 = pos;
						OLC_CMD(d).arg1 = GET_TRIG_DATA_TYPE((TrigData *)trig_index[pos].proto);
						zedit_command_menu(d);
					} else 
						send_to_char("That trigger does not exist, try again : ", d->character);
					break;
				default:
					//	We should never get here
					//	cleanup_olc(d, CLEANUP_ALL);
					mudlog("SYSERR: OLC: zedit_parse(): case ARG2: Ack!", BRF, LVL_BUILDER, TRUE);
					send_to_char("Oops...\r\n", d->character);
					break;
			}
			break;

/*-------------------------------------------------------------------*/
		case ZEDIT_ARG3:
			//	Parse the input for arg3, and go back to main menu.
			if  (!isdigit(*arg)) {
				send_to_char("Must be a numeric value, try again : ", d->character);
				return;
			}
			switch(OLC_CMD(d).command) {
				case 'E':
					pos = atoi(arg);
					//	Count number of wear positions (could use NUM_WEARS,
					//	this is more reliable)
					while(*equipment_types[i] != '\n')
						i++;
					if ((pos < 0) || (pos > i))
						send_to_char("Try again : ", d->character);
					else {
						OLC_CMD(d).arg3 = pos;
						zedit_command_menu(d);
					}
					break;
				case 'P':
					if ((pos = real_object(atoi(arg))) >= 0) {
						OLC_CMD(d).arg3 = pos;
						zedit_command_menu(d);
					} else
						send_to_char("That object does not exist, try again : ", d->character);
					break;
				case 'D':
					pos = atoi(arg);
					if ((pos < 0) || (pos > 2))
						send_to_char("Try again : ", d->character);
					else {
						OLC_CMD(d).arg3 = pos;
						zedit_command_menu(d);
					}
					break;
				case 'T':
					OLC_CMD(d).arg3 = atoi(arg);
					zedit_command_menu(d);
					break;
				default:
					//	We should never get here
					//	cleanup_olc(d, CLEANUP_ALL);
					mudlog("SYSERR: OLC: zedit_parse(): case ARG3: Ack!", BRF, LVL_BUILDER, TRUE);
					send_to_char("Oops...\r\n", d->character);
					break;
			}
			break;

/*-------------------------------------------------------------------*/
		case ZEDIT_ARG4:
			//	Parse the input for arg4, and go back to main menu
			if  (!isdigit(*arg)) {
				send_to_char("Must be a numeric value, try again : ", d->character);
				return;
			}
			switch(OLC_CMD(d).command) { 
				case 'M':
					OLC_CMD(d).arg4 = atoi(arg);
					zedit_command_menu(d);
					break;
				default:
					//	We should never get here
					//	cleanup_olc(d, CLEANUP_ALL);
					mudlog("SYSERR: OLC: zedit_parse(): case ARG3: Ack!", BRF, LVL_BUILDER, TRUE);
					send_to_char("Oops...\r\n", d->character);
					break;
			}
			break;

/*-------------------------------------------------------------------*/
		case ZEDIT_REPEAT:
			//	Repeat for several times
			pos = atoi(arg);
			if (pos > 0)	OLC_CMD(d).repeat = pos;
			else			OLC_CMD(d).repeat = 1;
			zedit_command_menu(d);
			break;
  
/*-------------------------------------------------------------------*/
		case ZEDIT_ZONE_NAME:
			//	Add new name and return to main menu
			FREE(OLC_ZONE(d)->name);
			OLC_ZONE(d)->name = str_dup(arg);
			OLC_ZONE(d)->number = 1;
			zedit_disp_menu(d);
			break;

/*-------------------------------------------------------------------*/
		case ZEDIT_ZONE_RESET:
			//	Parse and add new reset_mode and return to main menu
			pos = atoi(arg);
			if (!isdigit(*arg) || (pos < 0) || (pos > 2))
				send_to_char("Try again (0-2) : ", d->character);
			else {
				OLC_ZONE(d)->reset_mode = pos;
				OLC_ZONE(d)->number = 1;
				zedit_disp_menu(d);
			}
			break; 

/*-------------------------------------------------------------------*/
		case ZEDIT_ZONE_LIFE:
			//	Parse and add new lifespan and return to main menu
			pos = atoi(arg);
			if (!isdigit(*arg) || (pos < 0) || (pos > 240))
				send_to_char("Try again (0-240) : ", d->character);
			else {
				OLC_ZONE(d)->lifespan = pos;
				OLC_ZONE(d)->number = 1;
				zedit_disp_menu(d);
			}
			break; 

/*-------------------------------------------------------------------*/
		case ZEDIT_ZONE_TOP:
			//	Parse and add new top room in zone and return to main menu
			if (OLC_ZNUM(d) == top_of_zone_table)
				OLC_ZONE(d)->top = MAX(OLC_ZNUM(d) * 100, MIN(32000, atoi(arg)));
			else
				OLC_ZONE(d)->top = MAX(OLC_ZNUM(d) * 100, MIN(zone_table[OLC_ZNUM(d) + 1].number * 100, atoi(arg)));
			OLC_ZONE(d)->number = 1;
			zedit_disp_menu(d);
			break; 

/*-------------------------------------------------------------------*/
		case ZEDIT_COMMAND_MENU:
			switch(*arg) {
				case 'C':
				case 'c':
					zedit_disp_comtype(d);
					OLC_ZONE(d)->age = 1;
					break;
				case 'D':
				case 'd':
					if(!strchr("Z", OLC_CMD(d).command) && OLC_VAL(d)) { /*. If there was a previous command .*/
						send_to_char("Is this command dependent on the success of the previous one? (y/n)\r\n", d->character);
						OLC_MODE(d) = ZEDIT_IF_FLAG;
						OLC_ZONE(d)->age = 1;
					} else
						zedit_command_menu(d);
					break;
				case '1':
					if(!strchr("RDZT", OLC_CMD(d).command)) {
						zedit_disp_arg1(d);
						OLC_ZONE(d)->age = 1;
					} else
						zedit_command_menu(d);
					break;
				case '2':
					if (!strchr("ZC", OLC_CMD(d).command)) {
						zedit_disp_arg2(d);
						OLC_ZONE(d)->age = 1;
					} else
						zedit_command_menu(d);
					break;
				case '3':
					if (strchr("EPDT", OLC_CMD(d).command)) {
						zedit_disp_arg3(d);
						OLC_ZONE(d)->age = 1;
					} else
						zedit_command_menu(d);
					break;
				case '4':
					if (strchr("M", OLC_CMD(d).command)) {
						zedit_disp_arg4(d);
						OLC_ZONE(d)->age = 1;
					} else
						zedit_command_menu(d);
					break;
				case 'R':
				case 'r':
					if (strchr("GORP", OLC_CMD(d).command)) {
						zedit_disp_repeat(d);
						OLC_ZONE(d)->age = 1;
					} else
						zedit_command_menu(d);
					break;
				case 'Q':
				case 'q':
					zedit_disp_menu(d);
					break;
				default:
					zedit_command_menu(d);
			}
			break;
  	
		default:
			//	We should never get here
			//	cleanup_olc(d, CLEANUP_ALL);
			mudlog("SYSERR: OLC: zedit_parse(): Reached default case!",BRF,LVL_BUILDER,TRUE);
			send_to_char("Oops...\r\n", d->character);
			break;
	}
}
/*. End of parse_zedit() .*/
