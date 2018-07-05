/* ************************************************************************
*   File: db.c                                          Part of CircleMUD *
*  Usage: Loading/saving chars, booting/resetting world, internal funcs   *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#define __DB_C__


#include "structs.h"
#include "utils.h"
#include "buffer.h"
#include "scripts.h"
#include "db.h"
#include "comm.h"
#include "handler.h"
#include "find.h"
#include "spells.h"
#include "mail.h"
#include "interpreter.h"
#include "house.h"
#include "boards.h"
#include "files.h"
#include "clans.h"
#include "ban.h"
#include "help.h"
#include "extradesc.h"
#include "olc.h"

#include <stdarg.h>

void mprog_read_programs(FILE *fp, IndexData *pIndex, char * mobfile);
char err_buf[MAX_STRING_LENGTH];


int mprog_name_to_type (char *name);
MPROG_DATA* mprog_file_read(char *f, MPROG_DATA *mprg, IndexData *pMobIndex);
IndexData *get_obj_index (int vnum);
IndexData *get_mob_index (int vnum);
void init_char(CharData * ch);
void CheckRegenRates(CharData *ch);

/**************************************************************************
*  declarations of most of the 'global' variables                         *
************************************************************************ */

struct zone_data *zone_table;	/* zone table			 */
SInt32 top_of_zone_table = 0;	/* top element of zone tab	 */

UInt32 top_idnum = 0;		/* highest idnum in use		 */

extern bool no_mail;		//	mail disabled?
bool mini_mud = false;		//	mini-mud mode?
time_t boot_time = 0;		//	time of mud boot
SInt32 circle_restrict = 0;		//	level of game restriction
RNum r_mortal_start_room;	//	rnum of mortal start room
RNum r_immort_start_room;	//	rnum of immort start room
RNum r_frozen_start_room;	//	rnum of frozen start room

char *credits = NULL;		//	game credits
char *news = NULL;			//	mud news
char *motd = NULL;			//	message of the day - mortals
char *imotd = NULL;			//	message of the day - immorts
char *help = NULL;			//	help screen
char *info = NULL;			//	info page
char *wizlist = NULL;		//	list of higher gods
char *immlist = NULL;		//	list of peon gods
char *background = NULL;	//	background story
char *handbook = NULL;		//	handbook for new immortals
char *policies = NULL;		//	policies page

struct TimeInfoData time_info;/* the infomation about the time    */
struct reset_q_type reset_q;	/* queue of zones to be reset	 */

UInt32 max_id = 100000;		/* next unique id number to use  */

/* local functions */
void discrete_load(FILE * fl, int mode, char *filename);
void reboot_wizlists(void);
void boot_world(void);
int count_hash_records(FILE * fl);
void save_etext(CharData * ch);
void get_one_line(FILE *fl, char *buf);
ACMD(do_reboot);
void index_boot(int mode);
void load_zones(FILE * fl, char *zonename);
void assign_mobiles(void);
void assign_objects(void);
void assign_rooms(void);
void assign_the_shopkeepers(void);
void build_player_index(void);
int is_empty(int zone_nr);
void reset_zone(int zone);
int file_to_string(char *name, char *buf);
int file_to_string_alloc(char *name, char **buf);
void renum_zone_table(void);
void log_zone_error(int zone, int cmd_no, char *message);
void reset_time(void);
void free_varlist(TrigVarData *vd);

/* external functions */
void load_messages(void);
void weather_and_time(int mode);
void mag_assign_spells(void);
void boot_social_messages(void);
void create_command_list(void);
void sort_commands(void);
void sort_spells(void);
void boot_the_shops(FILE * shop_f, char *filename, int rec_count);
int find_first_step(RNum src, RNum target);
void strip_string(char *buffer);
struct TimeInfoData mud_time_passed(time_t t2, time_t t1);
int find_name(char *name);
void make_maze(int zone);

/* external vars */
extern int no_specials;

extern char *dirs[];

#define READ_SIZE 256

/*************************************************************************
*  routines for booting the system                                       *
*********************************************************************** */

/* this is necessary for the autowiz system */
void reboot_wizlists(void)
{
  file_to_string_alloc(WIZLIST_FILE, &wizlist);
  file_to_string_alloc(IMMLIST_FILE, &immlist);
}


ACMD(do_reboot) {
	int i;
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	
	one_argument(argument, arg);

	if (!str_cmp(arg, "all") || *arg == '*') {
		file_to_string_alloc(WIZLIST_FILE, &wizlist);
		file_to_string_alloc(IMMLIST_FILE, &immlist);
		file_to_string_alloc(NEWS_FILE, &news);
		file_to_string_alloc(CREDITS_FILE, &credits);
		file_to_string_alloc(MOTD_FILE, &motd);
		file_to_string_alloc(IMOTD_FILE, &imotd);
		file_to_string_alloc(HELP_PAGE_FILE, &help);
		file_to_string_alloc(INFO_FILE, &info);
		file_to_string_alloc(POLICIES_FILE, &policies);
		file_to_string_alloc(HANDBOOK_FILE, &handbook);
		file_to_string_alloc(BACKGROUND_FILE, &background);
	} else if (!str_cmp(arg, "wizlist"))
		file_to_string_alloc(WIZLIST_FILE, &wizlist);
	else if (!str_cmp(arg, "immlist"))
		file_to_string_alloc(IMMLIST_FILE, &immlist);
	else if (!str_cmp(arg, "news"))
		file_to_string_alloc(NEWS_FILE, &news);
	else if (!str_cmp(arg, "credits"))
		file_to_string_alloc(CREDITS_FILE, &credits);
	else if (!str_cmp(arg, "motd"))
		file_to_string_alloc(MOTD_FILE, &motd);
	else if (!str_cmp(arg, "imotd"))
		file_to_string_alloc(IMOTD_FILE, &imotd);
	else if (!str_cmp(arg, "help"))
		file_to_string_alloc(HELP_PAGE_FILE, &help);
	else if (!str_cmp(arg, "info"))
		file_to_string_alloc(INFO_FILE, &info);
	else if (!str_cmp(arg, "policy"))
		file_to_string_alloc(POLICIES_FILE, &policies);
	else if (!str_cmp(arg, "handbook"))
		file_to_string_alloc(HANDBOOK_FILE, &handbook);
	else if (!str_cmp(arg, "background"))
		file_to_string_alloc(BACKGROUND_FILE, &background);
	else if (!str_cmp(arg, "xhelp")) {
		if (help_table) {
			for (i = 0; i <= top_of_helpt; i++) {
				help_table[i].Free();
			}
			FREE(help_table);
		}
		top_of_helpt = 0;
		index_boot(DB_BOOT_HLP);
	} else {
		send_to_char("Unknown reload option.\r\n", ch);
		release_buffer(arg);
		return;
	}

	send_to_char(OK, ch);
	release_buffer(arg);
}


void boot_world(void) {
	log("Loading zone table.");
	index_boot(DB_BOOT_ZON);

	log("Loading rooms.");
	index_boot(DB_BOOT_WLD);

	log("Renumbering rooms.");
	renum_world();

	log("Checking start rooms.");
	check_start_rooms();

	log("Loading mobs and generating index.");
	index_boot(DB_BOOT_MOB);

	log("Loading objs and generating index.");
	index_boot(DB_BOOT_OBJ);

	log("Loading trigs and generating index.");
	index_boot(DB_BOOT_TRG);
	
	log("Loading clans and generating index.");
/*  index_boot(DB_BOOT_CLAN);*/
	load_clans();

	log("Renumbering zone table.");
	renum_zone_table();

	log("Loading boards.");
	Board_init_boards();

	if (!no_specials) {
		log("Loading shops.");
		index_boot(DB_BOOT_SHP);
	}
}

  

/* body of the booting system */
void boot_db(void) {
	int i;

	log("Boot db -- BEGIN.");

	log("Resetting the game time:");
	reset_time();

	log("Reading news, credits, help, bground, info & motds.");
	file_to_string_alloc(NEWS_FILE, &news);
	file_to_string_alloc(CREDITS_FILE, &credits);
	file_to_string_alloc(MOTD_FILE, &motd);
	file_to_string_alloc(IMOTD_FILE, &imotd);
	file_to_string_alloc(HELP_PAGE_FILE, &help);
	file_to_string_alloc(INFO_FILE, &info);
	file_to_string_alloc(WIZLIST_FILE, &wizlist);
	file_to_string_alloc(IMMLIST_FILE, &immlist);
	file_to_string_alloc(POLICIES_FILE, &policies);
	file_to_string_alloc(HANDBOOK_FILE, &handbook);
	file_to_string_alloc(BACKGROUND_FILE, &background);

	log("Generating player index.");
	build_player_index();
	
	boot_world();

	log("Loading help entries.");
	index_boot(DB_BOOT_HLP);


	log("Loading fight messages.");
	load_messages();

	log("Loading social messages.");
	boot_social_messages();
	create_command_list(); /* aedit patch -- M. Scott */
  
	log("Assigning function pointers:");

	if (!no_specials) {
		log("   Mobiles.");
		assign_mobiles();
		log("   Shopkeepers.");
		assign_the_shopkeepers();
		log("   Objects.");
		assign_objects();
		log("   Rooms.");
		assign_rooms();
	}
	log("   Spells.");
	mag_assign_spells();

	log("Assigning spell and skill levels.");
	init_spell_levels();

	log("Sorting command list and spells.");
	sort_commands();
	sort_spells();

	log("Booting mail system.");
	if (!scan_file()) {
		log("    Mail boot failed -- Mail system disabled");
		no_mail = true;
	}
	log("Reading banned site and invalid-name list.");
	load_banned();
	Read_Invalid_List();

	for (i = 0; i <= top_of_zone_table; i++) {
		log("Resetting %s (rooms %d-%d).", zone_table[i].name, (i ? (zone_table[i - 1].top + 1) : 0),
				zone_table[i].top);
		reset_zone(i);
	}

	reset_q.head = reset_q.tail = NULL;

	if (!mini_mud) {
		log("Booting houses.");
		House_boot();
	}
	boot_time = time(0);

	MOBTrigger = TRUE;

	log("Boot db -- DONE.");
}


/* reset the time in the game from file */
void reset_time(void) {
/* This should yield about equal times */
#ifdef CIRCLE_MAC
	SInt32 beginning_of_time = -1561789232;
#else
	SInt32 beginning_of_time = 650336715;
#endif

	struct TimeInfoData mud_time_passed(time_t t2, time_t t1);

	time_info = mud_time_passed(time(0), beginning_of_time);

	time_info.year += 2000;

	if (time_info.hours <= 4)			sunlight = SUN_DARK;
	else if (time_info.hours == 5)		sunlight = SUN_RISE;
	else if (time_info.hours <= 20)		sunlight = SUN_LIGHT;
	else if (time_info.hours == 21)		sunlight = SUN_SET;
	else								sunlight = SUN_DARK;

	log("   Current Gametime: %dH %dD %dM %dY.", time_info.hours, time_info.day, time_info.month, time_info.year);
/*
	weather_info.pressure = 960;
	if ((time_info.month >= 7) && (time_info.month <= 12))
		weather_info.pressure += dice(1, 50);
	else
		weather_info.pressure += dice(1, 80);

	weather_info.change = 0;

	if (weather_info.pressure <= 980)			weather_info.sky = SKY_LIGHTNING;
	else if (weather_info.pressure <= 1000)		weather_info.sky = SKY_RAINING;
	else if (weather_info.pressure <= 1020)		weather_info.sky = SKY_CLOUDY;
	else										weather_info.sky = SKY_CLOUDLESS;
*/
}


/* new version to build the player index for the ascii pfiles */
/* generate index table for the player file */
void build_player_index(void) {
	int rec_count = 0, i; //, lowerCounter;
	FILE *plr_index;
	char	*index_name = get_buffer(40);
	char	*line, *arg1, *arg2;

	sprintf(index_name, "%s", PLR_INDEX_FILE);
	if(!(plr_index = fopen(index_name, "r"))) {
		if (errno != ENOENT) {
			perror("SYSERR: Error opening player index file pfiles/plr_index");
			system("touch ../.killscript");
			exit(1);
		} else {
			log("No player index file. Creating a new one.");
			touch(PLR_INDEX_FILE);
			if (!(plr_index = fopen(index_name, "r"))) {
				perror("SYSERR: Error opening player index file pfiles/plr_index");
				system("touch ../.killscript");
				exit(1);
			}
		}
	}
	
	release_buffer(index_name);
	line = get_buffer(256);
	
/* count the number of players in the index */
	while(get_line(plr_index, line))
		rec_count++;
	rewind(plr_index);

	if(rec_count == 0) {
		player_table = NULL;
		top_of_p_table = -1;
	} else {
		arg1 = get_buffer(40);
		arg2 = get_buffer(80);
  
		CREATE(player_table, struct player_index_element, rec_count);
		for(i = 0; i < rec_count; i++) {
			get_line(plr_index, line);
			two_arguments(line, arg1, arg2);
//			for (lowerCounter = 0; arg2[lowerCounter]; lowerCounter++)
//				arg2[lowerCounter] = LOWER(arg2[lowerCounter]);
//			CREATE(player_table[i].name, char, strlen(arg2) + 1);
//			strcpy(player_table[i].name, arg2);
			player_table[i].name = str_dup(arg2);
			player_table[i].id = atoi(arg1);
			top_idnum = MAX(top_idnum, player_table[i].id);
		}
		top_of_p_table = i - 1;
  
		log("   %d players in database.", rec_count);
		release_buffer(arg1);
		release_buffer(arg2);
	}
	release_buffer(line);
}


/* function to count how many hash-mark delimited records exist in a file */
int count_hash_records(FILE * fl) {
	char *buf = get_buffer(128);
	int count = 0;

	while (fgets(buf, 128, fl))
		if (*buf == '#')
			count++;

	release_buffer(buf);
	return count;
}


void index_boot(int mode) {
	char *index_filename, *prefix;
	FILE *index, *db_file;
	int rec_count = 0;
	char *buf1;
	char *filename;

/*
 * XXX: Just in case, although probably not necessary.
 */
//	release_all_buffers();

	filename = get_buffer(256);
	buf1 = get_buffer(256);
	
	switch (mode) {
		case DB_BOOT_WLD:	prefix = WLD_PREFIX;	break;
		case DB_BOOT_MOB:	prefix = MOB_PREFIX;	break;
		case DB_BOOT_OBJ:	prefix = OBJ_PREFIX;	break;
		case DB_BOOT_ZON:	prefix = ZON_PREFIX;	break;
		case DB_BOOT_SHP:	prefix = SHP_PREFIX;	break;
		case DB_BOOT_HLP:	prefix = HLP_PREFIX;	break;
		case DB_BOOT_TRG:	prefix = TRG_PREFIX;	break;
/*#ifdef CLANS_PATCH
		case DB_BOOT_CLAN:	prefix = CLAN_PREFIX;	break;
#endif*/
		default:
			log("SYSERR: Unknown subcommand %d to index_boot!", mode);
			exit(1);
			break;
	}

	if (mini_mud)
		index_filename = MINDEX_FILE;
	else
		index_filename = INDEX_FILE;
	
	sprintf(filename, "%s%s", prefix, index_filename);

	if (!(index = fopen(filename, "r"))) {
		sprintf(buf1, "SYSERR: Error opening index file '%s'", filename);
		perror(buf1);
		tar_restore_file(filename);
		exit(1);
	}

/* first, count the number of records in the file so we can malloc */
	fscanf(index, "%s\n", buf1);
	while (*buf1 != '$') {
		sprintf(filename, "%s%s", prefix, buf1);
		
		if (!(db_file = fopen(filename, "r"))) {
			perror(filename);
			log("SYSERR: File %s listed in %s/%s not found", filename, prefix, index_filename);
			tar_restore_file(filename);
			exit(1);
		} else {
			if (mode == DB_BOOT_ZON)
				rec_count++;
			else
				rec_count += count_hash_records(db_file);
		}

		fclose(db_file);
		fscanf(index, "%s\n", buf1);
	}

	/* Exit if 0 records, unless this is shops */
	if (!rec_count) {
		release_buffer(buf1);
		release_buffer(filename);
		if (mode == DB_BOOT_SHP) {
			return;
		}
		log("SYSERR: boot error - 0 records counted in %s/%s", prefix, index_filename);
		exit(1);
	}

	rec_count++;

	switch (mode) {
		case DB_BOOT_WLD:
			CREATE(world, RoomData, rec_count);
			break;
		case DB_BOOT_MOB:
			CREATE(mob_index, IndexData, rec_count);
			break;
		case DB_BOOT_OBJ:
			CREATE(obj_index, IndexData, rec_count);
			break;
		case DB_BOOT_ZON:
			CREATE(zone_table, struct zone_data, rec_count);
			break;
		case DB_BOOT_HLP:
			CREATE(help_table, HelpElement, rec_count);
			break;
/*#ifdef CLANS_PATCH
		case DB_BOOT_CLAN:
			CREATE(clan_index, struct clan_info, rec_count + 10);
			clan_top = rec_count - 1;
			break;
#endif*/
		case DB_BOOT_TRG:
			CREATE(trig_index, IndexData, rec_count);
	}

	rewind(index);
	fscanf(index, "%s\n", buf1);
	while (*buf1 != '$') {
		sprintf(filename, "%s%s", prefix, buf1);
		
		if (!(db_file = fopen(filename, "r"))) {
			perror(filename);
			tar_restore_file(filename);
			exit(1);
		}
		switch (mode) {
			case DB_BOOT_WLD:
			case DB_BOOT_OBJ:
			case DB_BOOT_MOB:
/*#ifdef CLANS_PATCH
			case DB_BOOT_CLAN:
#endif*/
			case DB_BOOT_TRG:
				discrete_load(db_file, mode, filename);
				break;
			case DB_BOOT_ZON:
				load_zones(db_file, filename);
				break;
			case DB_BOOT_HLP:
				load_help(db_file);
				break;
			case DB_BOOT_SHP:
				boot_the_shops(db_file, filename, rec_count);
				break;
		}

		fclose(db_file);
		fscanf(index, "%s\n", buf1);
	}
	fclose(index);

	release_buffer(buf1);
	release_buffer(filename);
}


void discrete_load(FILE * fl, int mode, char *filename) {
	int nr = -1, last = 0;
	char *line = get_buffer(256);

/*#ifdef CLANS_PATCH
	char *modes[] = {"world", "mob", "obj", "clan"};
#else*/
	char *modes[] = {"world", "mob", "obj", "ERROR", "ERROR", "ERROR", "clan", "trigger"};
/*#endif*/

	for (;;) {
		/*
		 * we have to do special processing with the obj files because they have
		 * no end-of-record marker :(
		 */
		if (mode != DB_BOOT_OBJ || nr < 0)
			if (!get_line(fl, line)) {
				log("SYSERR: Format error after %s #%d", modes[mode], nr);
				tar_restore_file(filename);
				exit(1);
			}

		if (*line == '$') {
			release_buffer(line);
			return;
		}

		if (*line == '#') {
			last = nr;
			if (sscanf(line, "#%d", &nr) != 1) {
				log("SYSERR: Format error after %s #%d", modes[mode], last);
				tar_restore_file(filename);
				exit(1);
			}
			if (nr >= 99999) {
				release_buffer(line);
				return;
			} else
				switch (mode) {
					case DB_BOOT_WLD:	RoomFuncs::ParseRoom(fl, nr, filename);			break;
					case DB_BOOT_MOB:	parse_mobile(fl, nr, filename);					break;
					case DB_BOOT_OBJ:	strcpy(line, parse_object(fl, nr, filename));	break;
					case DB_BOOT_TRG:	parse_trigger(fl, nr, filename);	break;
/*#ifdef CLANS_PATCH
					case DB_BOOT_CLAN:	parse_clan(fl, nr);					break;
#endif*/
				}
		} else {
			log("SYSERR: Format error in %s file near %s #%d", modes[mode], modes[mode], nr);
			log("SYSERR: Offending line: '%s'", line);
			tar_restore_file(filename);
			exit(1);
		}
	}
	release_buffer(line);
}


#define ZCMD zone_table[zone].cmd[cmd_no]

/* resulve vnums into rnums in the zone reset tables */
void renum_zone_table(void) {
	char *buf = get_buffer(MAX_INPUT_LENGTH);
	int zone, cmd_no, a, b, c, d, olda, oldb, oldc, oldd;

	for (zone = 0; zone <= top_of_zone_table; zone++)
		for (cmd_no = 0; ZCMD.command != 'S'; cmd_no++) {
			a = b = c = d = 0;
			olda = ZCMD.arg1;
			oldb = ZCMD.arg2;
			oldc = ZCMD.arg3;
			oldd = ZCMD.arg4;
			switch (ZCMD.command) {
				case 'M':
					a = ZCMD.arg1 = real_mobile((VNum)ZCMD.arg1);
					c = ZCMD.arg3 = real_room((VNum)ZCMD.arg3);
					break;
				case 'O':
					a = ZCMD.arg1 = real_object((VNum)ZCMD.arg1);
					if (ZCMD.arg3 != NOWHERE)
						c = ZCMD.arg3 = real_room((VNum)ZCMD.arg3);
					break;
				case 'G':
					a = ZCMD.arg1 = real_object((VNum)ZCMD.arg1);
					break;
				case 'E':
					a = ZCMD.arg1 = real_object((VNum)ZCMD.arg1);
					break;
				case 'P':
					a = ZCMD.arg1 = real_object((VNum)ZCMD.arg1);
					c = ZCMD.arg3 = real_object((VNum)ZCMD.arg3);
					break;
				case 'D':
					a = ZCMD.arg1 = real_room((VNum)ZCMD.arg1);
					break;
				case 'R': /* rem obj from room */
					a = ZCMD.arg1 = real_room((VNum)ZCMD.arg1);
					b = ZCMD.arg2 = real_object((VNum)ZCMD.arg2);
					break;
				case 'T':
					b = ZCMD.arg2 = real_trigger((VNum)ZCMD.arg2);
					switch (ZCMD.arg1) {
						case MOB_TRIGGER:
						case OBJ_TRIGGER:
							break;
						case WLD_TRIGGER:
							ZCMD.arg4 = real_room((VNum)ZCMD.arg4);
							break;
						default:
							a = -1;
							break;
					}
			}
			if (a < 0 || b < 0 || c < 0 || d < 0) {
				if (!mini_mud)
					sprintf(buf,  "Invalid vnum %d, cmd disabled",
							(a < 0) ? olda : ((b < 0) ? oldb : oldc));
					log_zone_error(zone, cmd_no, buf);
				ZCMD.command = '*';
			}
		}
	release_buffer(buf);
}


#define Z	zone_table[zone]

/* load the zone table and command tables */
void load_zones(FILE * fl, char *zonename) {
	static int zone = 0;
	int cmd_no = 0, num_of_cmds = 0, line_num = 0, tmp, error, num[6];
	char	*ptr,
			*buf = get_buffer(256),
			*zname = get_buffer(256),
			*buf2 = get_buffer(128),
			*builder;

	strcpy(zname, zonename);

	while (get_line(fl, buf))
		num_of_cmds++;		/* this should be correct within 3 or so */
	rewind(fl);

	if (num_of_cmds == 0) {
		log("SYSERR: %s is empty!", zname);
		exit(0);
	} else
		CREATE(Z.cmd, struct reset_com, num_of_cmds);

	line_num += get_line(fl, buf);

	if (sscanf(buf, "#%d", &Z.number) != 1) {
		log("SYSERR: Format error in %s, line %d", zname, line_num);
		exit(1);
	}
	sprintf(buf2, "beginning of zone #%d", Z.number);

	line_num += get_line(fl, buf);
	if ((ptr = strchr(buf, '~')) != NULL)	/* take off the '~' if it's there */
		*ptr = '\0';
	Z.name = str_dup(buf);

	line_num += get_line(fl, buf);
	if (sscanf(buf, " %d %d %d ", &Z.top, &Z.lifespan, &Z.reset_mode) != 3) {
		log("SYSERR: Format error in 3-constant line of %s", zname);
		exit(1);
	}

	Z.pressure = 960;
	if ((time_info.month >= 7) && (time_info.month <= 12))	Z.pressure += dice(1, 50);
	else 													Z.pressure += dice(1, 80);
	Z.change = 0;
	if (Z.pressure <= 980)			Z.sky = SKY_LIGHTNING;
	else if (Z.pressure <= 1000)	Z.sky = SKY_RAINING;
	else if (Z.pressure <= 1020)	Z.sky = SKY_CLOUDY;
	else							Z.sky = SKY_CLOUDLESS;

	cmd_no = 0;

	for (;;) {
		if (!(tmp = get_line(fl, buf))) {
			log("SYSERR: Format error in %s - premature end of file", zname);
			exit(1);
		}
		line_num += tmp;
		ptr = buf;
		skip_spaces(ptr);
		
		ZCMD.command = *ptr;
		
		ptr++;
		
		if (ZCMD.command == '*') {
			char * arg1 = get_buffer(MAX_INPUT_LENGTH);
			char * arg2 = get_buffer(MAX_INPUT_LENGTH);
			half_chop(ptr, arg1, arg2);
			if (*arg1 && *arg2) {				
				if (is_abbrev(arg1, "Builder")) {
//					CREATE(tbldr, struct builder_list, 1);
//					tbldr->name = str_dup(arg2);
//					tbldr->next = bldr;
//					bldr = tbldr;
					builder = str_dup(arg2);
					Z.builders.Append(builder);
//					if (!bldr) {
//						CREATE(bldr, struct builder_list, 1);
//						bldr->name = str_dup(arg2);
//						bldr->next = NULL;
//					} else {
//					}
				}
			}
			release_buffer(arg1);
			release_buffer(arg2);
			continue;
		}


		if (ZCMD.command == 'S' || ZCMD.command == '$') {
			ZCMD.command = 'S';
			break;
		}
		error = 0;

		sscanf(ptr, " %d %d %d %d %d %d", num, num + 1, num + 2, num + 3, num + 4, num + 5);

		ZCMD.if_flag = num[0] ? 1 : 0;
		ZCMD.arg1 = num[1];
		ZCMD.arg2 = num[2];
		ZCMD.arg3 = num[3];
		ZCMD.arg4 = num[4];
		ZCMD.repeat = (num[5] > 0) ? num[5] : 1;
		switch (ZCMD.command) {
			case 'C':
				skip_spaces(ptr);
				if (*ptr) {
//					ZCMD.if_flag = *ptr ? 1 : 0;
					ptr++;
					skip_spaces(ptr);
					ZCMD.command_string = str_dup(ptr);
				}
//				ZCMD.command_string = str_dup(ptr);
				break;
			case 'M':
//				if (sscanf(ptr, " %d %d %d %d %d ", &tmp, &ZCMD.arg1, &ZCMD.arg2, &ZCMD.arg3, &ZCMD.arg4) < 4)
//					error = 1;
//				break;
				ZCMD.repeat = 1;
				break;
			case 'E':
			case 'D':
			case 'Z':
//				if (sscanf(ptr, " %d %d %d %d ", &tmp, &ZCMD.arg1, &ZCMD.arg2, &ZCMD.arg3) != 4)
//					error = 1;
//				break;
				ZCMD.arg4 = -1;
				ZCMD.repeat = 1;
				break;
			case 'O':
			case 'P':
//				if (sscanf(ptr, " %d %d %d %d %d ", &tmp, &ZCMD.arg1, &ZCMD.arg2, &ZCMD.arg3, &ZCMD.repeat) != 5)
//					error = 1;
//				break;
				ZCMD.arg4 = -1;
				break;
			case 'G':
			case 'R':
//				if (sscanf(ptr, " %d %d %d %d ", &tmp, &ZCMD.arg1, &ZCMD.arg2, &ZCMD.repeat) != 4)
//					error = 1;
//				break;
				ZCMD.arg3 = -1;
				ZCMD.arg4 = -1;
				break;
			case 'T':
//				if (sscanf(ptr, " %d %d %d %d %d ", &tmp, &ZCMD.arg1, &ZCMD.arg2, &ZCMD.arg3, &ZCMD.arg4) != 5)
//					error = 1;
//				break;
				ZCMD.repeat = 1;
				break;
			default:
//				if (sscanf(ptr, " %d %d %d ", &tmp, &ZCMD.arg1, &ZCMD.arg2) != 3)
//				error = 1;
//				break;
				ZCMD.arg3 = -1;
				ZCMD.arg4 = -1;
				ZCMD.repeat = 1;
				break;
		}
//		ZCMD.if_flag = tmp;

//		if (error) {
//			log("SYSERR: Format error in %s, line %d: '%s'", zname, line_num, buf);
//			exit(1);
//		}
		ZCMD.line = line_num;
		cmd_no++;
	}
	
	top_of_zone_table = zone++;
	
	release_buffer(buf);
	release_buffer(zname);
	release_buffer(buf2);
}

#undef Z


void get_one_line(FILE *fl, char *buf) {
	if (fgets(buf, READ_SIZE, fl) == NULL) {
		log("error reading help file: not terminated with $?");
		exit(1);
	}

	buf[strlen(buf) - 1] = '\0'; /* take off the trailing \n */
}


/*************************************************************************
*  procedures for resetting, both play-time and boot-time	 	 *
*********************************************************************** */



int vnum_mobile(char *searchname, CharData * ch) {
	int nr, found = 0;
	char *buf = get_buffer(SMALL_BUFSIZE);
	
	for (nr = 0; nr <= top_of_mobt; nr++) {
		if (isname(searchname, SSData(((CharData *)mob_index[nr].proto)->general.name))) {
			sprintf(buf, "%3d. [%5d] %s\r\n", ++found, mob_index[nr].vnum,
					SSData(((CharData *)mob_index[nr].proto)->general.shortDesc));
			send_to_char(buf, ch);
		}
	}
	release_buffer(buf);
	return (found);
}



int vnum_object(char *searchname, CharData * ch) {
	int nr, found = 0;

	for (nr = 0; nr <= top_of_objt; nr++) {
		if (isname(searchname, SSData(((ObjData *)obj_index[nr].proto)->name))) {
			ch->Send("%3d. [%5d] %s\r\n", ++found, obj_index[nr].vnum,
					SSData(((ObjData *)obj_index[nr].proto)->shortDesc));
		}
	}
	return (found);
}


int vnum_trigger(char *searchname, CharData * ch) {
	int nr, found = 0;
	char *buf = get_buffer(SMALL_BUFSIZE);

	for (nr = 0; nr <= top_of_trigt; nr++) {
		if (isname(searchname, SSData(((TrigData *)trig_index[nr].proto)->name))) {
			sprintf(buf, "%3d. [%5d] %s\r\n", ++found, trig_index[nr].vnum, SSData(((TrigData *)trig_index[nr].proto)->name));
			send_to_char(buf, ch);
		}
	}
	release_buffer(buf);
	return (found);
}


#define ZO_DEAD  999

/* update zone ages, queue for reset if necessary, and dequeue when possible */
void zone_update(void)
{
	int i;
	struct reset_q_element *update_u, *temp;
	static int timer = 0;
	char *buf = get_buffer(128);

	/* jelson 10/22/92 */
	if (((++timer * PULSE_ZONE) / PASSES_PER_SEC) >= 60) {
		/* one minute has passed */
		/*
		 * NOT accurate unless PULSE_ZONE is a multiple of PASSES_PER_SEC or a
		 * factor of 60
		 */

		timer = 0;

		/* since one minute has passed, increment zone ages */
		for (i = 0; i <= top_of_zone_table; i++) {
			if (zone_table[i].age < zone_table[i].lifespan && zone_table[i].reset_mode)
				(zone_table[i].age)++;

			if (zone_table[i].age >= zone_table[i].lifespan &&
					zone_table[i].age < ZO_DEAD && zone_table[i].reset_mode) {
				/* enqueue zone */

				CREATE(update_u, struct reset_q_element, 1);

				update_u->zone_to_reset = i;
				update_u->next = 0;

				if (!reset_q.head)
					reset_q.head = reset_q.tail = update_u;
				else {
					reset_q.tail->next = update_u;
					reset_q.tail = update_u;
				}

				zone_table[i].age = ZO_DEAD;
			}
		}
	}	/* end - one minute has passed */


	/* dequeue zones (if possible) and reset */
	/* this code is executed every 10 seconds (i.e. PULSE_ZONE) */
	for (update_u = reset_q.head; update_u; update_u = update_u->next)
		if (zone_table[update_u->zone_to_reset].reset_mode == 2 || is_empty(update_u->zone_to_reset)) {
			reset_zone(update_u->zone_to_reset);
			mudlogf(CMP, LVL_STAFF, FALSE, "Auto zone reset: %s", zone_table[update_u->zone_to_reset].name);
			/* dequeue */
			if (update_u == reset_q.head)
				reset_q.head = reset_q.head->next;
			else {
				for (temp = reset_q.head; temp->next != update_u;
					temp = temp->next);

				if (!update_u->next)
					reset_q.tail = temp;

				temp->next = update_u->next;
			}

			FREE(update_u);
			break;
		}
	release_buffer(buf);
}

void log_zone_error(int zone, int cmd_no, char *message) {
	mudlogf(NRM, LVL_STAFF, FALSE,	"ZONEERR: zone file: %s", message);
	mudlogf(NRM, LVL_STAFF, FALSE,	"ZONEERR: ...offending cmd: '%c' cmd in zone #%d, line %d",
			ZCMD.command, zone_table[zone].number, ZCMD.line);
}

#define ZONE_ERROR(message) \
	{ log_zone_error(zone, cmd_no, message); last_cmd = 0; }

void make_maze(int zone)
{
  int card[400], temp, x, y, dir, room;
  int num, next_room = 0, test, r_back;
  int vnum = zone_table[zone].number;
  extern int rev_dir[];
  
  for (test = 0; test < 400; test++) {
    card[test] = test;
    temp = test;
    dir = temp / 100;
    temp = temp - (dir * 100);
    x = temp / 10;
    temp = temp - (x * 10);
    y = temp;
    room = (vnum * 100) + (x * 10) + y;
    room = real_room(room);
    if ((x == 0) && (dir == 0))
      continue;
    if ((y == 9) && (dir == 1))
      continue;
    if ((x == 9) && (dir == 2))
      continue;
    if ((y == 0) && (dir == 3))
      continue;
    world[room].dir_option[dir]->to_room = -1;
    REMOVE_BIT(ROOM_FLAGS(room), ROOM_NOTRACK);
  }
  for (x = 0; x < 399; x++) {
    y = Number(0, 399);
    temp = card[y];
    card[y] = card[x];
    card[x] = temp;
  }

  for (num = 0; num < 400; num++) {
    temp = card[num];
    dir = temp / 100;
    temp = temp - (dir * 100);
    x = temp / 10;
    temp = temp - (x * 10);
    y = temp;
    room = (vnum * 100) + (x * 10) + y;
    r_back = room;
    room = real_room(room);
    if ((x == 0) && (dir == 0))
      continue;
    if ((y == 9) && (dir == 1))
      continue;
    if ((x == 9) && (dir == 2))
      continue;
    if ((y == 0) && (dir == 3))
      continue;
    if (world[room].dir_option[dir]->to_room != -1)
      continue;
    switch(dir) {
      case 0:
        next_room = r_back - 10;
        break;
      case 1:
        next_room = r_back + 1;
        break;
      case 2:
        next_room = r_back + 10;
        break;
      case 3:
        next_room = r_back - 1;
        break;
    }
    next_room = real_room(next_room);
    test = find_first_step(room, next_room);
    switch (test) {
      case BFS_ERROR:
        log("Maze making error.");
        break;
      case BFS_ALREADY_THERE:
        log("Maze making error.");
        break;
      case BFS_NO_PATH:

        world[room].dir_option[dir]->to_room = next_room;
        world[next_room].dir_option[(int) rev_dir[dir]]->to_room = room;
        break;
    }
  }
  for (num = 0;num < 100;num++) {
    room = (vnum * 100) + num;
    room = real_room(room);
/* Remove the next line if you want to be able to track your way through 
the maze */
/*    SET_BIT(ROOM_FLAGS(room), ROOM_NOTRACK); */

    REMOVE_BIT(ROOM_FLAGS(room), ROOM_BFS_MARK);
  }
}

/* execute the reset command table of a given zone */
void reset_zone(int zone) {
	int cmd_no, repeat_no, last_cmd = 0, maze_made = 0;
	CharData *mob = NULL;
	ObjData *obj = NULL, *obj_to;
	TrigData *trig;
	RoomData *room;

	for (cmd_no = 0; ZCMD.command != 'S'; cmd_no++) {

		if ((ZCMD.if_flag || (ZCMD.command == 'C')) && !last_cmd)
			continue;
		
		if (ZCMD.repeat <= 0)
			ZCMD.repeat = 1;

		for (repeat_no = 0; repeat_no < ZCMD.repeat; repeat_no++) {
			switch (ZCMD.command) {
				case '*':			/* ignore command */
					last_cmd = 0;
					break;

				case 'M':			/* read a mobile */
					if ((!ZCMD.arg2 || ((MOB_FLAGGED((CharData *)mob_index[ZCMD.arg1].proto, MOB_SENTINEL) ?
							(count_mobs_in_room(ZCMD.arg1, ZCMD.arg3)) :
							(mob_index[ZCMD.arg1].number)) < ZCMD.arg2)) &&
							((ZCMD.arg4 <= 0) ||
							(count_mobs_in_zone(ZCMD.arg4, world[ZCMD.arg3].zone) < ZCMD.arg4))) {
						if (ZCMD.arg3 >= 0) {
							mob = read_mobile(ZCMD.arg1, REAL);
							mob->to_room(ZCMD.arg3);
							last_cmd = 1;
						}
					} else
						last_cmd = 0;
					break;

				case 'O':			/* read an object */
					if (!ZCMD.arg2 || obj_index[ZCMD.arg1].number < ZCMD.arg2) {
						if (ZCMD.arg3 >= 0) {
							obj = read_object(ZCMD.arg1, REAL);
							obj->to_room(ZCMD.arg3);
							last_cmd = 1;
						}
					} else
						last_cmd = 0;
					break;

				case 'P':			/* object to object */
					if (!ZCMD.arg2 || obj_index[ZCMD.arg1].number < ZCMD.arg2) {
						if (!(obj_to = get_obj_num(ZCMD.arg3))) {
							ZONE_ERROR("target obj not found");
							break;
						}
						obj = read_object(ZCMD.arg1, REAL);
						obj->to_obj(obj_to);
						last_cmd = 1;
					} else
						last_cmd = 0;
					break;

				case 'G':			/* obj_to_char */
					if (!mob) {
						ZONE_ERROR("attempt to give obj to non-existant mob");
						break;
					}
					if (!ZCMD.arg2 || obj_index[ZCMD.arg1].number < ZCMD.arg2) {
						obj = read_object(ZCMD.arg1, REAL);
						obj->to_char(mob);
						last_cmd = 1;
					} else
						last_cmd = 0;
					break;

				case 'E':			/* object to equipment list */
					if (!mob) {
						ZONE_ERROR("trying to equip non-existant mob");
						break;
					}
					if (!ZCMD.arg2 || obj_index[ZCMD.arg1].number < ZCMD.arg2) {
						if (ZCMD.arg3 < 0 || ZCMD.arg3 >= NUM_WEARS) {
							switch (ZCMD.arg3) {
								case POS_WIELD_TWO:
								case POS_HOLD_TWO:
								case POS_WIELD:
									if (!mob->equipment[ZCMD.arg3]) {
										obj = read_object(ZCMD.arg1, REAL);
										equip_char(mob, obj, WEAR_HAND_R);
										last_cmd = 1;
									}
									break;
								case POS_WIELD_OFF:
								case POS_LIGHT:
								case POS_HOLD:
									if (!mob->equipment[ZCMD.arg3]) {
										obj = read_object(ZCMD.arg1, REAL);
										equip_char(mob, obj, WEAR_HAND_L);
										last_cmd = 1;
									}
									break;
								default:
									ZONE_ERROR("invalid equipment pos number");
							}
						} else if (!mob->equipment[ZCMD.arg3]) {
							obj = read_object(ZCMD.arg1, REAL);
							equip_char(mob, obj, ZCMD.arg3);
							last_cmd = 1;
						}
					} else
						last_cmd = 0;
					break;

				case 'R': /* rem obj from room */
					if ((obj = get_obj_in_list_num(ZCMD.arg2, world[ZCMD.arg1].contents)) != NULL) {
						obj->from_room();
						obj->extract();
					}
					last_cmd = 1;
					break;
					
				case 'C':			/* command mob to do something */
					if(!mob || !ZCMD.command_string) {
						if (!mob)						ZONE_ERROR("trying to command nonexistant mob")
						else if (!ZCMD.command_string)	ZONE_ERROR("trying to command mob with no command")
					} else {
						command_interpreter(mob, ZCMD.command_string);
						last_cmd = 1;
					}
					break;
					
				case 'D':			/* set state of door */
					if (ZCMD.arg2 < 0 || ZCMD.arg2 >= NUM_OF_DIRS || ZCMD.arg1 == NOWHERE || 
							(world[ZCMD.arg1].dir_option[ZCMD.arg2] == NULL)) {
						char *buf = get_buffer(MAX_STRING_LENGTH);
						sprintf(buf, "door %s does not exist in room %d", dirs[ZCMD.arg2], world[ZCMD.arg1].number);
						ZONE_ERROR(buf);
						release_buffer(buf);
					} else {
						switch (ZCMD.arg3) {
							case 0:
								REMOVE_BIT(world[ZCMD.arg1].dir_option[ZCMD.arg2]->exit_info,
										EX_LOCKED);
								REMOVE_BIT(world[ZCMD.arg1].dir_option[ZCMD.arg2]->exit_info,
										EX_CLOSED);
								break;
							case 1:
								SET_BIT(world[ZCMD.arg1].dir_option[ZCMD.arg2]->exit_info,
										EX_CLOSED);
								REMOVE_BIT(world[ZCMD.arg1].dir_option[ZCMD.arg2]->exit_info,
										EX_LOCKED);
								break;
							case 2:
								SET_BIT(world[ZCMD.arg1].dir_option[ZCMD.arg2]->exit_info,
										EX_LOCKED);
								SET_BIT(world[ZCMD.arg1].dir_option[ZCMD.arg2]->exit_info,
										EX_CLOSED);
								break;
						}
					}
					last_cmd = 1;
					break;
					
				case 'Z':			/* This zone is a MAZE zone */
					if (!maze_made) {
						maze_made = 1;
						make_maze(zone);
					}
					last_cmd = 1;
					break;
					
				case 'T':
					last_cmd = 0;
					switch (ZCMD.arg1) {
						case MOB_TRIGGER:
							if (!mob) {
								ZONE_ERROR("attempt to assign trigger to non-existant mob");
								continue;
							}
							if (trig_index[ZCMD.arg2].number < ZCMD.arg3) {
								trig = read_trigger(ZCMD.arg2, REAL);
								if (!SCRIPT(mob))
									SCRIPT(mob) = new ScriptData;
								add_trigger(SCRIPT(mob), trig); //, -1);
								last_cmd = 1;
							}
							break;
						case OBJ_TRIGGER:
							if (!obj) {
								ZONE_ERROR("attempt to assign trigger to non-existant object");
								continue;
							}
							if (trig_index[ZCMD.arg2].number < ZCMD.arg3) {
								trig = read_trigger(ZCMD.arg2, REAL);
								
								if (!SCRIPT(obj))
									SCRIPT(obj) = new ScriptData;
								add_trigger(SCRIPT(obj), trig); //, -1);
								last_cmd = 1;
							}
							break;
						case WLD_TRIGGER:
							if (ZCMD.arg4 == -1) {
								ZONE_ERROR("attempt to assign trigger to non-existant room");
								continue;
							}
							room = &world[ZCMD.arg4];
							if (trig_index[ZCMD.arg2].number < ZCMD.arg3) {
								trig = read_trigger(ZCMD.arg2, REAL);

								if (!SCRIPT(room))
									SCRIPT(room) = new ScriptData;
								add_trigger(SCRIPT(room), trig); //, -1);
								last_cmd = 1;
							}
							break;
						default:
							ZONE_ERROR("unknown trigger type");
							ZCMD.command = '*';
							break;
					}
					break;
					
				default:
					ZONE_ERROR("unknown cmd in reset table; cmd disabled");
					ZCMD.command = '*';
					break;
			}
		}
	}
	zone_table[zone].age = 0;
}



/* for use in reset_zone; return TRUE if zone 'nr' is free of PC's  */
int is_empty(int zone_nr) {
	DescriptorData *i;

	START_ITER(iter, i, DescriptorData *, descriptor_list) {
		if ((STATE(i) == CON_PLAYING) && (world[IN_ROOM(i->character)].zone == zone_nr)) {
			END_ITER(iter);
			return 0;
		}		
	} END_ITER(iter);
	
	return 1;
}





/*************************************************************************
*  stuff related to the save/load player system				 *
*********************************************************************** */


SInt32 get_id_by_name(const char *name) {
	int i;
	char *arg = get_buffer(MAX_INPUT_LENGTH);

	one_argument(name, arg);
	for (i = 0; i <= top_of_p_table; i++)
		if (!str_cmp((player_table + i)->name, arg)) {
			release_buffer(arg);
			return ((player_table + i)->id);
		}
	
	release_buffer(arg);
	return -1;
}


const char *get_name_by_id(SInt32 id) {
	int i;

	for (i = 0; i <= top_of_p_table; i++)
		if ((player_table + i)->id == id)
			return ((player_table + i)->name);

	return NULL;
}


/* create a new entry in the in-memory index table for the player file */
int create_entry(const char *name) {
	int i;

	if (top_of_p_table == -1) {
		CREATE(player_table, struct player_index_element, 1);
		top_of_p_table = 0;
	} else {
		RECREATE(player_table, struct player_index_element, (++top_of_p_table + 1));
	}
	
	CREATE(player_table[top_of_p_table].name, char, strlen(name) + 1);

	//	copy lowercase equivalent of name to table field
	for (i = 0; (*(player_table[top_of_p_table].name + i) = LOWER(*(name + i))); i++);

	return (top_of_p_table);
}



/************************************************************************
*  funcs of a (more or less) general utility nature			*
********************************************************************** */


/* release memory allocated for a char struct */


/* read contents of a text file, alloc space, point buf to it */
int file_to_string_alloc(char *name, char **buf) {
	char *temp = get_buffer(MAX_STRING_LENGTH);
	
	if (*buf)	FREE(*buf);

	if (file_to_string(name, temp) < 0) {
		*buf = "";
		release_buffer(temp);
		return -1;
	} else {
		*buf = str_dup(temp);
		release_buffer(temp);
		return 0;
	}
}


/* read contents of a text file, and place in buf */
int file_to_string(char *name, char *buf) {
	FILE *fl;
	char *tmp = get_buffer(READ_SIZE+3);

	*buf = '\0';

	if (!(fl = fopen(name, "r"))) {
		sprintf(tmp, "SYSERR: Error reading %s", name);
		perror(tmp);
		release_buffer(tmp);
		release_buffer(tmp);
		return (-1);
	}
	do {
		fgets(tmp, READ_SIZE, fl);
		tmp[strlen(tmp) - 1] = '\0'; /* take off the trailing \n */
		strcat(tmp, "\r\n");

		if (!feof(fl)) {
			if (strlen(buf) + strlen(tmp) + 1 > MAX_STRING_LENGTH) {
				log("SYSERR: %s: string too big (%d max)", name, MAX_STRING_LENGTH);
				*buf = '\0';
				release_buffer(tmp);
				return -1;
			}
			strcat(buf, tmp);
		}
	} while (!feof(fl));

	fclose(fl);

	release_buffer(tmp);
	return (0);
}



/* clear some of the the working variables of a char */
void reset_char(CharData * ch) {
	int i;

	for (i = 0; i < NUM_WEARS; i++)
		GET_EQ(ch, i) = NULL;

	ch->followers.Clear();
	ch->master = NULL;
	IN_ROOM(ch) = NOWHERE;
	ch->carrying.Clear();
	FIGHTING(ch) = NULL;
	ch->general.position = POS_STANDING;

	ch->general.misc.carry_weight = 0;
	ch->general.misc.carry_items = 0;

	if (GET_HIT(ch) <= 0)	GET_HIT(ch) = 1;
	if (GET_MOVE(ch) <= 0)	GET_MOVE(ch) = 1;

	CheckRegenRates(ch);

	GET_LAST_TELL(ch) = NOBODY;
}


/* initialize a new character only if class is set */
void init_char(CharData * ch) {
	int i;

	if (GET_PFILEPOS(ch) < 0)
		GET_PFILEPOS(ch) = create_entry(ch->RealName());

	//	create a player_special structure
	if (ch->player == NULL)
		ch->player = new PlayerData;

	//	if this is our first player --- he be God
	if (top_of_p_table == 0) {
		GET_LEVEL(ch) = LVL_CODER;

		ch->points.max_hit = 5000;
		ch->points.max_move = 800;
		SET_BIT(STF_FLAGS(ch), 0xFFFFFFFF);
	}
	ch->set_title(NULL);

	ch->general.shortDesc = NULL;
	ch->general.longDesc = NULL;
	ch->general.description = NULL;

	ch->player->time.birth = time(0);
	ch->player->time.played = 0;
	ch->player->time.logon = time(0);

	/* make favors for sex */
	if (ch->general.sex == SEX_MALE) {
		ch->general.weight = Number(180, 250);
		ch->general.height = Number(66, 78);
	} else {
		ch->general.weight = Number(120, 180);
		ch->general.height = Number(60, 72);
	}

	ch->points.hit = GET_MAX_HIT(ch);
	ch->points.max_move = 82;
	ch->points.move = GET_MAX_MOVE(ch);
	ch->points.armor = 0 /*100*/;

	GET_ID(ch) = player_table[top_of_p_table].id = GET_IDNUM(ch) = ++top_idnum;

	for (i = 1; i <= MAX_SKILLS; i++) {
		SET_SKILL(ch, i, !IS_STAFF(ch) ? 0 : 100);
	}

	ch->general.affected_by = 0;

//	ch->real_abils.intel = 100;
//	ch->real_abils.per = 100;
//	ch->real_abils.agi = 100;
//	ch->real_abils.str = 100;
//	ch->real_abils.con = 100;

	for (i = 0; i < 3; i++)
		GET_COND(ch, i) = (IS_STAFF(ch) ? -1 : 24);

	GET_LOADROOM(ch) = NOWHERE;
	
	ch->save(NOWHERE);
	save_player_index();
}


/* MOBProg functions */

/* This routine transfers between alpha and numeric forms of the
 * mob_prog bitvector types.  This allows the use of the words in the
 * mob/script files.
 */

int mprog_name_to_type (char *name)
{
    if (!str_cmp(name, "in_file_prog"	))	return IN_FILE_PROG;
    if (!str_cmp(name, "act_prog"		))	return ACT_PROG;
    if (!str_cmp(name, "speech_prog"	))	return SPEECH_PROG;
    if (!str_cmp(name, "rand_prog"		))	return RAND_PROG;
    if (!str_cmp(name, "fight_prog"		))	return FIGHT_PROG;
    if (!str_cmp(name, "hitprcnt_prog"	))	return HITPRCNT_PROG;
    if (!str_cmp(name, "death_prog"		))	return DEATH_PROG;
    if (!str_cmp(name, "entry_prog"		))	return ENTRY_PROG;
    if (!str_cmp(name, "greet_prog"		))	return GREET_PROG;
    if (!str_cmp(name, "all_greet_prog"	))	return ALL_GREET_PROG;
    if (!str_cmp(name, "give_prog"		))	return GIVE_PROG;
    if (!str_cmp(name, "bribe_prog"		))	return BRIBE_PROG;
    if (!str_cmp(name, "exit_prog"		))	return EXIT_PROG;
    if (!str_cmp(name, "all_exit_prog"	))	return ALL_EXIT_PROG;
    if (!str_cmp(name, "delay_prog"		))	return DELAY_PROG;
	if (!str_cmp(name, "script_prog"	))	return SCRIPT_PROG;
	if (!str_cmp(name, "wear_prog"		))	return WEAR_PROG;
	if (!str_cmp(name, "remove_prog"	))	return REMOVE_PROG;
	if (!str_cmp(name, "get_prog"		))	return GET_PROG;
	if (!str_cmp(name, "drop_prog"		))	return DROP_PROG;
	if (!str_cmp(name, "examine_prog"	))	return EXAMINE_PROG;
	if (!str_cmp(name, "pull_prog"		))	return PULL_PROG;
    return(ERROR_PROG);
}


  /* This routine reads in scripts of MOBprograms from a file */

MPROG_DATA* mprog_file_read(char *f, MPROG_DATA *mprg, IndexData *pIndex) {
	char	*MOBProgfile = get_buffer(MAX_INPUT_LENGTH),
			*buf = get_buffer(MAX_INPUT_LENGTH);
	MPROG_DATA	*mprg2;
	FILE		*progfile;
	char		letter;
	int			done = FALSE;

	sprintf(MOBProgfile, MOB_DIR "%s", f);

	progfile = fopen(MOBProgfile, "r");
	if (!progfile) {
		log("Mob: %d couldnt open mobprog file", pIndex->vnum);
		tar_restore_file(MOBProgfile);
		exit(1);
	}

	mprg2 = mprg;
	switch (letter = fread_letter(progfile)) {
		case '>':
			break;
		case '|':
			log("empty mobprog file.");
			tar_restore_file(MOBProgfile);
			exit(1);
			break;
		default:
			log("in mobprog file syntax error.");
			tar_restore_file(MOBProgfile);
			exit(1);
			break;
	}

	while (!done) {
		mprg2->type = mprog_name_to_type(fread_word(progfile));
		switch (mprg2->type) {
		case ERROR_PROG:
			log("mobprog file type error");
			tar_restore_file(MOBProgfile);
			exit(1);
			break;
		case IN_FILE_PROG:
			log("mprog file contains a call to file.");
			tar_restore_file(MOBProgfile);
			exit(1);
			break;
		default:
			sprintf(buf, "Error in file %s", f);
			pIndex->progtypes		= pIndex->progtypes | mprg2->type;
			mprg2->arglist			= fread_string(progfile, buf, MOBProgfile);
			mprg2->comlist			= fread_string(progfile, buf, MOBProgfile);
			switch (letter = fread_letter(progfile)) {
				case '>':
					CREATE(mprg2->next, MPROG_DATA, 1);
					mprg2       = mprg2->next;
					mprg2->next = NULL;
					break;
				case '|':
					done = TRUE;
					break;
				default:
					log("in mobprog file %s syntax error.", f);
					tar_restore_file(MOBProgfile);
					exit(1);
			}
			break;
		}
	}
	fclose(progfile);
	release_buffer(buf);
	release_buffer(MOBProgfile);
	return mprg2;
}


IndexData *get_obj_index (int vnum) {
	int nr;
	for(nr = 0; nr <= top_of_objt; nr++) {
		if(obj_index[nr].vnum == vnum) return &obj_index[nr];
	}
	return NULL;
}

IndexData *get_mob_index (int vnum) {
	int nr;
	for(nr = 0; nr <= top_of_mobt; nr++) {
		if(mob_index[nr].vnum == vnum) return &mob_index[nr];
	}
	return NULL;
}


/* This procedure is responsible for reading any in_file MOBprograms. */

void mprog_read_programs(FILE *fp, IndexData *pIndex, char * mobfile) {
	MPROG_DATA *mprg;
	char        letter;
	int        done = FALSE;
	char * buf = get_buffer(MAX_INPUT_LENGTH);

    if ((letter = fread_letter(fp)) != '>') {
        log("Load_mobiles: vnum %d MOBPROG char", pIndex->vnum);
        exit(1);
    }
//	pIndex->mobprogs = (MPROG_DATA *)malloc(sizeof(MPROG_DATA));
	CREATE(pIndex->mobprogs, MPROG_DATA, 1);
	mprg = pIndex->mobprogs;

	while (!done) {
		mprg->type = mprog_name_to_type(fread_word(fp));
		switch (mprg->type) {
			case ERROR_PROG:
				log("Load_mobiles: vnum %d MOBPROG type.", pIndex->vnum);
				tar_restore_file(mobfile);
				exit(1);
				break;
			case IN_FILE_PROG:
//				sprintf(buf, "Mobprog for mob #%d", pMobIndex->vnum);
				mprg = mprog_file_read(fread_word(fp), mprg, pIndex);
				fread_to_eol(fp);   /* need to strip off that silly ~*/
				switch (letter = fread_letter(fp)) {
					case '>':
						CREATE(mprg->next, MPROG_DATA, 1);
//						mprg->next = (MPROG_DATA *)malloc(sizeof(MPROG_DATA));
						mprg       = mprg->next;
						mprg->next = NULL;
						break;
					case '|':
						mprg->next = NULL;
						fread_to_eol(fp);
						done = TRUE;
						break;
					default:
						log("Load_mobiles: vnum %d bad MOBPROG.", pIndex->vnum);
						tar_restore_file(mobfile);
						exit(1);
						break;
				}
				break;
			default:
				sprintf(buf, "Mobprog for mob #%d", pIndex->vnum);
				pIndex->progtypes	= pIndex->progtypes | mprg->type;
				mprg->arglist			= fread_string(fp, buf, mobfile);
				mprg->comlist			= fread_string(fp, buf, mobfile);
				switch (letter = fread_letter(fp)) {
					case '>':
						CREATE(mprg->next, MPROG_DATA, 1);
//						mprg->next = (MPROG_DATA *)malloc(sizeof(MPROG_DATA));
						mprg       = mprg->next;
						mprg->next = NULL;
						break;
					case '|':
						mprg->next = NULL;
						fread_to_eol(fp);
						done = TRUE;
						break;
					default:
						log("Load_mobiles: vnum %d bad MOBPROG (%c).", pIndex->vnum, letter);
						tar_restore_file(mobfile);
						exit(1);
						break;
				}
				break;
			}
		}
	release_buffer(buf);
	return;
}

void vwear_object(int wearpos, CharData * ch) {
	int nr, found = 0;
	
	for (nr = 0; nr <= top_of_objt; nr++)
		if (CAN_WEAR((ObjData *)obj_index[nr].proto, wearpos))
			ch->Send("%3d. [%5d] %s\r\n", ++found, obj_index[nr].vnum, SSData(((ObjData *)obj_index[nr].proto)->shortDesc));
}


void save_player_index(void) {
	int i;
	FILE *index_file;

	if(!(index_file = fopen(PLR_INDEX_FILE, "w"))) {
		log("SYSERR:  Could not write player index file");
		return;
	}

	for(i = 0; i <= top_of_p_table; i++)
		fprintf(index_file, "%d	%s\n", (int)player_table[i].id, player_table[i].name);
//		fprintf(index_file, "%d	%c%s\n", (int)player_table[i].id, LOWER(*player_table[i].name), player_table[i].name + 1);

	fclose(index_file);
}

void free_purged_lists(void);

#if 0
void FreeAllMemory(void);
void FreeAllMemory(void) {
	CharData *	ch;
	ObjData *	obj;
	TrigData *	trig; //, *next_trig;
	CmdlistElement *cmd, *next_cmd;
	ExtraDesc *	exdesc, *next_exdesc;
	struct builder_list *builder, *next_builder;
	struct reset_q_element *rqelement, *next_rqelement;
	SInt32		i, dir, x;
	
	log("Freeing memory for shutdown:");

	if (help_table) {
		log("  - Help");
		for (i = 0; i <= top_of_helpt; i++) {
			if (help_table[i].keyword)								free(help_table[i].keyword);
			if (help_table[i].entry && !help_table[i].duplicate)	free(help_table[i].entry);
		}
		FREE(help_table);
	}
	
	log("  - Loaded lists:");
	log("    - Mobs");
	START_ITER(ch_iter, ch, CharData *, character_list) {
		ch->extract();
	} END_ITER(ch_iter);
	
	log("    - Objs");
	START_ITER(obj_iter, obj, ObjData *, object_list) {
		obj->extract();
	} END_ITER(obj_iter);
	
	log("    - Scripts");
	for (i = 0; i <= top_of_world; i++) {	// World scripts first
		if (world[i].script)	delete world[i].script;
	}
	START_ITER(trig_iter, trig, TrigData *, trig_list) {
		trig->extract();
	}
	free_purged_lists();
	
	log("  - Prototypes:");
	log("    - Mobs");
	for (i = 0; i <= top_of_mobt; i++) {
		if (mob_index[i].proto) {
			delete (CharData *)mob_index[i].proto;
		}
	}
	FREE(mob_index);
	
	log("    - Objs");
	for (i = 0; i <= top_of_objt; i++) {
		if (obj_index[i].proto) {
			delete (ObjData *)obj_index[i].proto;
		}
	}
	FREE(obj_index);
	
	log("    - Scripts");
	for (i = 0; i <= top_of_trigt; i++) {
		if (trig_index[i].proto) {
			for (cmd = ((TrigData *)trig_index[i].proto)->cmdlist; cmd; cmd = next_cmd) {
				next_cmd = cmd->next;
				delete cmd;
			}
			delete (TrigData *)trig_index[i].proto;
		}
	}
	FREE(trig_index);
	
	log("    - Rooms");
	for (i = 0; i <= top_of_world; i++) {
		SSFree(world[i].name);
		SSFree(world[i].description);
		for (dir = 0; dir < NUM_OF_DIRS; dir++) {
			if (world[i].dir_option[dir])	delete world[i].dir_option[dir];
		}
		for (exdesc = world[i].ex_description; exdesc; exdesc = next_exdesc) {
			next_exdesc = exdesc->next;
			delete exdesc;
		}
	}
	FREE(world);
	
	log("  - Zone data");
	for (i = 0; i <= top_of_zone_table; i++) {
		if (zone_table[i].name)		free(zone_table[i].name);
		for (builder = zone_table[i].builders; builder; builder = next_builder) {
			next_builder = builder->next;
			if (builder->name)	free(builder->name);
			free(builder);
		}
		for (x = 0; zone_table[i].cmd[x].command != 'S'; x++) {
			if (zone_table[i].cmd[x].command == 'C')
				free(zone_table[i].cmd[x].command_string);
		}
		free(zone_table[i].cmd);
	}
	log("  - Zone Resets");
	for (rqelement = reset_q.head; rqelement; rqelement = next_rqelement) {
		next_rqelement = rqelement->next;
		free(rqelement);
	}
	log("  - Player index");
	for (i = 0; i <= top_of_p_table; i++) {
		if (player_table[i].name)	free(player_table[i].name);
	}
	free(player_table);char *credits = NULL;		/* game credits			 */
	
	log("  - Texts");
	if (credits)	free(credits);
	if (news)		free(news);
	if (motd)		free(motd);
	if (imotd)		free(imotd);
	if (help)		free(help);
	if (info)		free(info);
	if (wizlist)	free(wizlist);
	if (immlist)	free(immlist);
	if (background)	free(background);
	if (handbook)	free(handbook);
	if (policies)	free(policies);
}
#endif



void parser_error(char *format, ...);

char *parser_matching_quote(char *p);
char *parser_matching_bracket(char *p);
char *parser_get_keyword(char *input, char *keyword);
char *parser_read_string(char *input, char *string);
char *parser_read_block(char *input, char *block);
char *parser(char *input, char *keyword);
char *parser_flag_parser(char *input, Flags *result, char **bits, const char *what);
void exdesc_parser(char *input, ExtraDesc **exdesc);
void exdesc_sub_parser(char *input, ExtraDesc *exdesc);
void room_parser(char *input, RoomData *room);
void exit_parser(char *input, RoomData *room);
void direction_parser(char *input, RoomDirection *direction);
void mob_parser(char *input, CharData *ch);
void obj_parser(char *input, ObjData *obj);
//void shop_parser(char *input, shop_data *shop);


SInt32 parse_number;
char *parse_file = NULL;

char *parser_matching_quote(char *p) {
	for (p++; *p && (*p != '"'); p++) {
		if (*p == '\\' && *(p+1))	p++;
	}
	if (!*p)	p--;
	return p;
}

/*
 * p points to the first paren.  returns a pointer to the
 * matching closing paren, or the last non-null char in p.
 */
char *parser_matching_bracket(char *p) {
	int i;
	for (p++, i = 1; *p && i; p++) {
		if (*p == '{')			i++;
		else if (*p == '}')		i--;
		else if (*p == '"')		p = parser_matching_quote(p);
	}
	return --p;
}


char *parser_get_keyword(char *input, char *keyword) {
	SInt32 counter;
	for (counter = 0;*input && isalnum(*input) && (counter < 255);
			*(keyword++) = LOWER(*(input++)), counter++);
	*keyword = '\0';
	skip_spaces(input);
	return input;
}


char *parser_read_string(char *input, char *string) {
	while (*input && (*input != '"')) {				//	Find start
		//	Maybe also detect stray tokens, but be careful
		//	not to count every character!
		input++;
	}
	while (*input && (*input != '"')) {				//	Then read till end
		if (*input == '\\') {						//	Allow cntrl characters
			input++;	//	Move to next char
			switch (*input) {
				case 'n':		//	Newline
					*(string++) = '\r';
					*(string++) = '\n';
					break;
				case '"':		//	"
					*(string++) = '"';
					break;
				case 'r':		//	Nothing - for the \r\n'rs
					break;
				default:		//	Maybe should somehow escape-translate?
					*(string++) = *(input++);
			}
		} else
			*(string++) = *(input++);
	}
	*string = '\0';
	return input;
}


char *parser_read_block(char *input, char *block) {
	char *end_block, *next_input;
	
	while (*input && *input != '{')	input++;
	if (*input) {
		next_input = end_block = parser_matching_bracket(input);
		input++;
		skip_spaces(input);
		while (*end_block && isspace(*end_block)) end_block--;
		if (end_block > input) {
			strncpy(block, input, end_block - input);
			block[end_block - input] = '\0';
		} else
			*block = '\0';
		input = next_input + 1;
		skip_spaces(input);
	} else {
		parser_error("End of String when reading block");
		//	ERROR HANDLER ? - EOS when attempt to read block
	}
	return input;
}


char *parser_flag_parser(char *input, Flags *result, char **bits, const char *what) {
	SInt32	i;
	char *keyword = get_buffer(MAX_INPUT_LENGTH);
	
	skip_spaces(input);
	if (*input == '{') {	//	List
		char *key2 = get_buffer(MAX_INPUT_LENGTH);
		char *keyPtr;
		
		input = parser_read_block(input, keyword);
		keyPtr = one_word(keyword, key2);
		while (*key2) {
			if ((i = search_block(key2, bits, false)) != -1) {
				SET_BIT(*result, (1 << i));
			} else {
				//	ERROR HANDLER - Unknown Flag
				parser_error("%s: Unknown flag \"%s\"", what, key2);
			}
			keyPtr = one_word(keyPtr, key2);
		}
		release_buffer(key2);
	} else if (is_number(input)) {	//	Compressed-save: #
		input = one_word(input, keyword);
		*result = atoi(keyword);
	} else {
		parser_error("Bad %s", what);
		//	ERROR HANDLER - Non-number/List
	}
	release_buffer(keyword);
	return input;
}


void parser_error(char *format, ...) {
	va_list args;
	char *buf = get_buffer(MAX_STRING_LENGTH);
	
	va_start(args, format);
	vsprintf(buf, format, args);
	va_end(args);
	
	log("Parse Error: %s:#%d - %s", parse_file ? parse_file : "<ERROR>", parse_number,
			*buf ? buf : "<ERROR>");
			
	release_buffer(buf);
}


char *parser(char *input, char *keyword) {
//	skip_spaces(input);
	if (!*input)	return input;						//	End of parsing
	while (*input) {									//	Find last keyword before '='
		if ((*input == '/') && (*(input++) == '*')) {	//	Handle comments!
			input++;
			while (*input && (*input != '*') && (*(input+1) != '/'))	input++;
			if (*input)	input += 2;		//	End of comment found, now skip to end
		}
		skip_spaces(input);
		//	Note: I do the (redundant) check here for speed purposes,
		//	even though parser_get_keyword and sub-functions can handle
		//	*input == '\0' and exit appropriately
		if (*input)	input = parser_get_keyword(input, keyword);
		if (*input != '=') {
			//	ERROR HANDLER - stray keyword
			parser_error("Expected \"=\", but got something else");
			break;
		} else {
			input++;
			break;
		}
	}
	skip_spaces(input);
	return input;
}


char *room_keywords[] = {
	"name",
	"desc",
	"sector",
	"flags",
	"exits",
	"exdesc",
	"\n"
};


extern char *sector_types[];
extern char *room_bits[];
void room_parser(char *input, RoomData *room) {
	char *	keyword = get_buffer(MAX_STRING_LENGTH);
	SInt32	key, i;
	
	for (;;) {
		input = parser(input, keyword);
		
		if (!*input || !*keyword)	break;
		
		key = search_block(keyword, room_keywords, true);
		switch (key) {
			case 0:		//	Name
				input = parser_read_string(input, keyword);
				room->name = *keyword ? str_dup(keyword) : NULL;
				break;
			case 1:		//	Desc
				input = parser_read_string(input, keyword);
				room->description = *keyword ? str_dup(keyword) : NULL;
				break;
			case 2:		//	Sector
				input = one_word(input, keyword);
				if (is_number(keyword))	room->sector_type = MAX(0, MIN(NUM_ROOM_SECTORS - 1, atoi(keyword)));
				else if ((i = search_block(keyword, sector_types, false)) != -1)
					room->sector_type = i;
				else {
					room->sector_type = 0;	//	Default value
					//	ERROR HANDLER - Bad sector name
					parser_error("Bad sector name \"%s\"", keyword);
				}
				break;
			case 3:		//	Flags
				input = parser_flag_parser(input, &room->flags, room_bits, "Room Flags");
				break;
			case 4:		//	Exits
				input = parser_read_block(input, keyword);
				exit_parser(keyword, room);
				break;
			case 5:		//	ExDesc
				input = parser_read_block(input, keyword);
				exdesc_parser(keyword, &room->ex_description);
				break;
			default:
				//	ERROR HANDLER - Unknown keyword
				parser_error("Unknown keyword \"%s\"", keyword);
		}
		while (*input && (*input != ';'))	input++;
		skip_spaces(input);
		if (!*input || *input == '}')		break;
	}
	release_buffer(keyword);
}


void exit_parser(char *input, RoomData *room) {
	char *	keyword = get_buffer(MAX_STRING_LENGTH);
	SInt32	key;
	
	for (;;) {
		input = parser(input, keyword);
		
		if (!*input || !*keyword)	break;
		
		key = search_block(keyword, dirs, true);
		if (key != -1) {
			if (!room->dir_option[key]) {
				room->dir_option[key] = new RoomDirection();
				input = parser_read_block(input, keyword);
				direction_parser(keyword, room->dir_option[key]);
			} else {
				//	ERROR HANDLER - Duplicate direction
				parser_error("Duplicate direction %s", keyword);
			}
		} else {
			//	ERROR HANDLER - Uknown direction
			parser_error("Unknown direction %s", keyword);
		}
		while (*input && (*input != ','))	input++;
		skip_spaces(input);
		if (!*input)				break;
	}
}


char *direction_keywords[] = {
	"keyword",
	"desc",
	"flags",
	"key",
	"room",
	"\n"
};


extern char *exit_bits[];
void direction_parser(char *input, RoomDirection *direction) {
	char *	keyword = get_buffer(MAX_STRING_LENGTH);
	SInt32	key;
	
	for (;;) {
		input = parser(input, keyword);
		
		if (!*input || !*keyword)	break;
		
		key = search_block(keyword, room_keywords, true);
		switch (key) {
			case 0:		//	Keyword
				input = parser_read_string(input, keyword);
				direction->keyword = *keyword ? str_dup(keyword) : NULL;
				break;
			case 1:		//	Desc
				input = parser_read_string(input, keyword);
				direction->general_description = *keyword ? str_dup(keyword) : NULL;
				break;
			case 2:		//	Flags
				input = parser_flag_parser(input, &direction->exit_info, exit_bits, "Exit Flags");
				break;
			case 3:		//	Key
				input = one_word(input, keyword);
				if (is_number(keyword))	direction->key = atoi(keyword);
				else {
					//	ERROR HANDLER - Non-number
					parser_error("Expected \"key = #\", got \"%s\"", keyword);
				}
				break;
			case 4:		//	Room
				input = one_word(input, keyword);
				if (is_number(keyword))	direction->to_room = atoi(keyword);
				else {
					//	ERROR HANDLER - Non-number
					parser_error("Expected \"room = #\", got \"%s\"", keyword);
				}
				break;
			default:
				//	ERROR HANDLER - Unknown field
				parser_error("Unknown room field \"%s\"", keyword);
		}
		while (*input && (*input != ';'))	input++;
		skip_spaces(input);
		if (!*input)				break;
	}
	release_buffer(keyword);
}


char *mob_keywords[] = {
	"name",			//	0
	"short",
	"long",
	"desc",
	"sex",
	"race",			//	5
	"level",
	"armor",
	"hp",
	"damage",
	"attack",		//	10
	"position",
	"defposition",
	"flags",
	"affects",
	"points",		//	15
	"dodge",
	"clan",
	"opinions",
	"triggers",
	"mobprog",		//	20
	"\n"
};


extern char *genders[];
extern char *race_types[];
extern char *positions[];
extern char *action_bits[];
extern char *affected_bits[];
char *attack_types[] = {
	"hit",			/* 0 */
	"sting",
	"whip",
	"slash",
	"bite",
	"bludgeon",		/* 5 */
	"crush",
	"pound",
	"claw",
	"maul",
	"thrash",		/* 10 */
	"pierce",
	"blast",
	"punch",
	"stab",
	"shoot",		/* 15 */
	"burn",
	"\n"
};

void mob_parser(char *input, CharData *ch) {
	char *	keyword = get_buffer(MAX_STRING_LENGTH);
	SInt32	key, i[5];
	
	for (;;) {
		input = parser(input, keyword);
		
		if (!*input || !*keyword)	break;
		
		key = search_block(keyword, room_keywords, true);
		switch (key) {
			case 0:		//	Name
				input = parser_read_string(input, keyword);
				ch->general.name = SSCreate(keyword);
				break;
			case 1:		//	Short
				input = parser_read_string(input, keyword);
				ch->general.shortDesc = SSCreate(keyword);
				break;
			case 2:		//	Long
				input = parser_read_string(input, keyword);
				ch->general.longDesc = SSCreate(keyword);
				break;
			case 3:		//	Desc
				input = parser_read_string(input, keyword);
				ch->general.description = SSCreate(keyword);
				break;
			case 4:		//	Sex
				input = one_word(input, keyword);
				if (is_number(keyword))	ch->general.sex = (Sex)MAX(0, MIN(NUM_GENDERS - 1, atoi(keyword)));
				else if ((ch->general.sex = (Sex)search_block(keyword, genders, false)) == -1) {
					ch->general.sex = SEX_MALE;	//	Default value
					//	ERROR HANDLER - Bad gender
					parser_error("Bad gender \"%s\"", keyword);
				}
				break;
			case 5:		//	Race
				input = one_word(input, keyword);
				if (is_number(keyword))	ch->general.race = (Race)MAX(0, MIN(NUM_RACES - 1, atoi(keyword)));
				else if ((ch->general.race = (Race)search_block(keyword, race_types, false)) == -1) {
					ch->general.race = RACE_OTHER;	//	Default value
					//	ERROR HANDLER - Bad race
					parser_error("Bad race \"%s\"", keyword);
				}
				break;
			case 6:		//	Level
				input = one_word(input, keyword);
				if (is_number(keyword))	ch->general.level = MAX(0, MIN(100, atoi(keyword)));
				else {
					//	ERROR HANDLER - Word instead of level #
					parser_error("Expected \"level = #\", got \"%s\"", keyword);
				}
				break;
			case 7:		//	Armor
				input = one_word(input, keyword);
				if (is_number(keyword))	ch->points.armor = MAX(0, MIN(999, atoi(keyword)));
				else {
					//	ERROR HANDLER - Word instead of armor #
					parser_error("Expected \"armor = #\", got \"%s\"", keyword);
				}
				break;
			case 8:		//	HP
				input = one_word(input, keyword);
				if (sscanf(keyword, "%dd%d+%d", i, i+1, i+2) == 3) {
					ch->points.hit = i[0];
					ch->points.mana = i[1];
					ch->points.move = i[2];
				} else {
					ch->points.hit = 0;
					ch->points.mana = 0;
					ch->points.move = 1;
					//	ERROR HANDLER - Bad format hitpoints
					parser_error("Expected \"hp = #d#+#\", got \"%s\"", keyword);
				}
				break;
			case 9:		//	Damage
				input = one_word(input, keyword);
				if (sscanf(keyword, "%dd%d+%d", i, i+1, i+2) == 3) {
					ch->mob->damage.number = i[0];
					ch->mob->damage.size = i[1];
					ch->points.damroll = i[2];
				} else {
					ch->mob->damage.number = 0;
					ch->mob->damage.size = 0;
					ch->points.damroll = 1;
					//	ERROR HANDLER - Bad format damage
					parser_error("Expected \"damage = #d#+#\", got \"%s\"", keyword);
				}
				break;
			case 10:	//	Attack
				input = one_word(input, keyword);
				if (is_number(keyword))	ch->mob->attack_type = MAX(0, MIN(NUM_ATTACK_TYPES - 1, atoi(keyword)));
				else if ((ch->mob->attack_type = search_block(keyword, attack_types, false)) == -1) {
					ch->mob->attack_type = TYPE_HIT;
					//	ERROR HANDLER - Bad attack type
					parser_error("Bad attack type \"%s\"", keyword);
				}
				break;
				break;
			case 11:	//	Position
				input = one_word(input, keyword);
				if (is_number(keyword))	ch->general.position = (Position)MAX(0, MIN(NUM_POSITIONS - 1, atoi(keyword)));
				else if ((ch->general.position = (Position)search_block(keyword, positions, false)) == -1) {
					ch->general.position = POS_STANDING;
					//	ERROR HANDLER - Bad Position
					parser_error("Bad position \"%s\"", keyword);
				}
				break;
			case 12:	//	DefPosition
				input = one_word(input, keyword);
				if (is_number(keyword))	ch->mob->default_pos = (Position)MAX(0, MIN(NUM_POSITIONS - 1, atoi(keyword)));
				else if ((ch->mob->default_pos = (Position)search_block(keyword, positions, false)) == -1) {
					ch->mob->default_pos = POS_STANDING;
					//	ERROR HANDLER - Bad Position
					parser_error("Bad default position \"%s\"", keyword);
				}
				break;
			case 13:	//	Flags
				input = parser_flag_parser(input, &ch->general.act, action_bits, "Mob Flags");
				break;
			case 14:	//	Affects
				input = parser_flag_parser(input, &ch->general.affected_by, affected_bits, "Mob Affect Flags");
				break;
			case 15:	//	Points
			case 16:	//	Dodge
				input = one_word(input, keyword);
				if (is_number(keyword))	ch->mob->dodge = MAX(0, MIN(100, atoi(keyword)));
				else {
					//	ERROR HANDLER - Word instead of dodge #
					parser_error("Expected \"dodge = #\", got \"%s\"", keyword);
				}
				break;
			case 17:	//	Clan
				input = one_word(input, keyword);
				if (is_number(keyword))	ch->general.misc.clannum = MAX(0, atoi(keyword));
				else {
					//	ERROR HANDLER - Word instead of clan #
					parser_error("Expected \"clan = #\", got \"%s\"", keyword);
				}
				break;
			case 18:	//	Opinions
			case 19:	//	Triggers
			case 20:	//	Mobprog
				break;
			default:
				//	ERROR HANDLER - Unknown keyword
				parser_error("Unknown keyword \"%s\"", keyword);
		}
		while (*input && (*input != ';'))	input++;
		skip_spaces(input);
		if (!*input || *input == '}')		break;
	}
	release_buffer(keyword);
}



void exdesc_parser(char *input, ExtraDesc **exdesc) {
	char *	keyword = get_buffer(MAX_STRING_LENGTH);
	ExtraDesc *ed;
	
	for (;;) {
		input = parser_read_block(input, keyword);
		
		if (*keyword) {
			ed = new ExtraDesc();
			exdesc_sub_parser(keyword, ed);
			if (SSData(ed->keyword) && SSData(ed->description)) {
				ed->next = (*exdesc)->next;
				*exdesc = ed;
			} else {
				//	ERROR HANDLER - Bad exdesc
				parser_error("Bad exdesc");
				delete ed;
			}
		}
		while (*input && (*input != ','))	input++;
		skip_spaces(input);
		if (!*input)				break;
	}
}


char *exdesc_keywords[] = {
	"keyword",
	"desc",
	"\n"
};
void exdesc_sub_parser(char *input, ExtraDesc *exdesc) {
	char *	keyword = get_buffer(MAX_STRING_LENGTH);
	SInt32	key;
	
	for (;;) {
		input = parser_read_block(input, keyword);

		input = parser(input, keyword);
		
		if (!*input || !*keyword)	break;
		
		key = search_block(keyword, room_keywords, true);
		switch (key) {
			case 0:		//	Keyword
				input = parser_read_string(input, keyword);
				exdesc->keyword = SSCreate(keyword);
				break;
			case 1:		//	Desc
				input = parser_read_string(input, keyword);
				exdesc->description = SSCreate(keyword);
				break;
			default:
				//	ERROR HANDLER - Unknown field
				parser_error("Unknown exdesc field \"%s\"", keyword);
		}
		while (*input && (*input != ';'))	input++;
		skip_spaces(input);
		if (!*input)				break;
	}
	release_buffer(keyword);
}
