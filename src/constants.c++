/* ************************************************************************
*   File: constants.c                                   Part of CircleMUD *
*  Usage: Numeric and string contants used by the MUD                     *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */



#include "structs.h"

char circlemud_version[] = {
"Aliens vs Predator: The MUD\r\n"
"Compiled on " __DATE__ " at " __TIME__ "\r\n"};

/* strings corresponding to ordinals/bitvectors in structs.h ***********/


/* (Note: strings for class definitions in class.c instead of here) */


/* cardinal directions */
char *dirs[] = {
	"north",
	"east",
	"south",
	"west",
	"up",
	"down",
	"\n"
};


char *from_dir[] = {
	"the south",
	"the west",
	"the north",
	"the east",
	"below",
	"above",
	"\n"
};


/* ROOM_x */
char *room_bits[] = {
  "DARK",
  "DEATH",
  "!MOB",
  "INDOORS",
  "PEACEFUL",
  "SOUNDPROOF",
  "!TRACK",
  "PARSER",
  "TUNNEL",
  "PRIVATE",
  "STAFF-ONLY",
  "HOUSE",
  "HCRSH",
  "ATRIUM",
  "Unused14",
  "*",				/* BFS MARK */
  "VEHICLES",
  "SRSTAFF-ONLY",
  "IMP-ONLY",
  "GRAVITY",
  "VACUUM",					/* 20 */
  "Unused21",
  "Unused22",
  "Unused23",
  "Unused24",
  "Unused25",				/* 25 */
  "Unused26",
  "Unused27",
  "Unused28",
  "Unused29",
  "Unused30",				/* 30 */
  "DELETED",
  "\n"
};


/* EX_x */
char *exit_bits[] = {
  "DOOR",
  "CLOSED",
  "LOCKED",
  "PICKPROOF",
  "HIDDEN",
  "NO-SHOOT",
  "NO-MOVE",
  "NO-MOB",
  "NO-VEHICLE",
  "\n"
};


/* SECT_ */
char *sector_types[] = {
  "Inside",
  "City",
  "Field",
  "Forest",
  "Hills",
  "Mountains",
  "Water (Swim)",
  "Water (No Swim)",
  "Underwater",
  "In Flight",
  "In Space",
  "Deep Space",
  "\n"
};


/* SEX_x */
char *genders[] =
{
  "Neutral",
  "Male",
  "Female",
  "\n"
};


/* POS_x */
char *position_types[] = {
  "Dead",
  "Mortally wounded",
  "Incapacitated",
  "Stunned",
  "Sleeping",
  "Resting",
  "Sitting",
  "Fighting",
  "Standing",
  "\n"
};


/* POS_x */
char *positions[] = {
  "dead",
  "mortally wounded",
  "incapacitated",
  "stunned",
  "sleeping",
  "resting",
  "sitting",
  "fighting",
  "standing",
  "\n"
};


/* PLR_x */
char *player_bits[] = {
  "TRAITOR",			/* 0 */
  "THIEF",
  "FROZEN",
  "DONTSET",
  "WRITING",
  "MAILING",			/* 5 */
  "CSH",
  "SITEOK",
  "NOSHOUT",
  "NOTITLE",
  "DELETED",			/* 10 */
  "LOADRM",
  "!WIZL",
  "!DEL",
  "INVST",
  "CRYO",				/* 15 */
  "\n"
};


/* MOB_x */
char *action_bits[] = {
  "SPEC",			/* 0 */
  "SENTINEL",
  "SCAVENGER",
  "ISNPC",
  "AWARE",
  "AGGR",			/* 5 */
  "STAY-ZONE",
  "WIMPY",
  "AGGR_ALL",
  "MEMORY",
  "HELPER",			/* 10 */
  "!CHARM",
  "!SUMMN",
  "!SLEEP",
  "!BASH",
  "!BLIND",			/* 15 */
  "ACIDBLOOD",
  "PROG-MOB",
  "Unused18",
  "Unused19",
  "APPROVED",				/* 20 */
  "Unused21",
  "Unused22",
  "Unused23",
  "Unused24",
  "Unused25",				/* 25 */
  "Unused26",
  "Unused27",
  "Unused28",
  "Unused29",
  "TRAITOR",				/* 30 */
  "DELETED",
  "\n"
};


/* PRF_x */
char *preference_bits[] = {
  "BRIEF",			/* 0 */
  "COMPACT",
  "unused",
  "unused",
  "unused",
  "unused",			/* 5 */
  "unused",
  "AUTOEX",
  "!HASS",
  "unused",
  "SUMN",			/* 10 */
  "!REP",
  "LIGHT",
  "COLOR",
  "unused",
  "unused",			/* 15 */
  "LOG1",
  "LOG2",
  "unused",
  "unused",
  "unused",			/* 20 */
  "RMFLG",
  "CLAN",
  "AFK",
  "unused",
  "AUTOSWITCH",		/* 25 */
  "unused",
  "\n"
};


char *channel_bits[] = {
	"!SHOUT",
	"!TELL",
	"!CHAT",
	"MISSION",
	"!MUSIC",
	"!GRATZ",
	"!INFO",
	"!WIZ",
	"!RACE",
	"!CLAN",
	"\n"
};


/* AFF_x */
char *affected_bits[] =
{
  "BLIND",
  "INVIS",
  "DET-INVIS",
  "SENSE-LIFE",
  "WATWALK",
  "SANCT",
  "GROUP",
  "FLYING",
  "INFRA",
  "POISON",
  "SLEEP",
  "!TRACK",
  "SNEAK",
  "HIDE",
  "CHARM",
  "TRACKING",
  "VACUUM-SAFE",
  "LIGHT",
  "TRAPPED",
  "IMPREGNATED",
  "\n"
};


/* CON_x */
char *connected_types[] = {
	"Playing",
	"Disconnecting",
	"Get name",
	"Confirm name",
	"Get password",
	"Get new PW",
	"Conf. new PW",
	"Select sex",
	"Select race",
	"Reading MOTD",
	"Main Menu",
	"Get descript.",
	"Chg PW 1",
	"Chg PW 2",
	"Chg PW 3",
	"Self-Delete 1",
	"Self-Delete 2",
	"Object edit",
	"Room edit",
	"Zone edit",
	"Mobile edit",
	"Shop edit",
	"Action edit",
	"Help edit",
	"Clan edit",
	"World edit",
	"Conf. Stats",
	"Text Editor",
	"Script edit",
	"Disconnect",
	"Ident",
	"\n"
};


/* WEAR_x - for eq list */
char *where[] = {
  "<worn on finger>     ",
  "<worn on finger>     ",
  "<worn around neck>   ",
  "<worn on body>       ",
  "<worn on head>       ",
  "<worn on legs>       ",
  "<worn on feet>       ",
  "<worn on hands>      ",
  "<worn on arms>       ",
  "<worn about body>    ",
  "<worn about waist>   ",
  "<worn around wrist>  ",
  "<worn around wrist>  ",
  "<worn over eyes>     ",
  "ERROR - PLEASE REPORT",
  "ERROR - PLEASE REPORT",
  "<wielded two-handed> ",
  "<held two-handed>    ",
  "<wielded>            ",
  "<wielded off-hand>   ",
  "<held as light>      ",
  "<held>               "
};


/* WEAR_x - for stat */
char *equipment_types[] = {
  "Worn on right finger",
  "Worn on left finger",
  "First worn around Neck",
  "Worn on body",
  "Worn on head",
  "Worn on legs",
  "Worn on feet",
  "Worn on hands",
  "Worn on arms",
  "Worn about body",
  "Worn around waist",
  "Worn around right wrist",
  "Worn around left wrist",
  "Worn over eyes",
  "Right Hand",
  "Left Hand",
  "Wielded two-handed",
  "Held two-handed",
  "Wielded",
  "Wielded off-hand",
  "Held as light",
  "Held",
  "\n"
};


/* ITEM_x (ordinal object types) */
char *item_types[] = {
  "UNDEFINED",			/* 0 */
  "LIGHT",
  "WEAPON",
  "FIRE WEAPON",
  "AMMO",
  "THROW",				/* 5 */
  "GRENADE",
  "BOW",
  "ARROW",
  "BOOMERANG",
  "TREASURE",			/* 10 */
  "ARMOR",
  "OTHER",
  "TRASH",
  "CONTAINER",
  "NOTE",				/* 15 */
  "KEY",
  "PEN",
  "BOAT",
  "FOOD",
  "LIQ CONTAINER",		/* 20 */
  "FOUNTAIN",
  "VEHICLE",
  "DRONE VEHICLE",
  "VEHICLE HATCH",
  "VEHICLE CONTROLS",	/* 25 */
  "VEHICLE WINDOW",
  "VEHICLE WEAPON",
  "BED",
  "CHAIR",
  "BOARD",				/* 30 */
  "ATTACHMENT",
  "\n"
};


/* ITEM_WEAR_ (wear bitvector) */
char *wear_bits[] = {
  "TAKE",
  "FINGER",
  "NECK",
  "BODY",
  "HEAD",
  "LEGS",
  "FEET",
  "HANDS",
  "ARMS",
  "SHIELD",
  "ABOUT",
  "WAIST",
  "WRIST",
  "WIELD",
  "HOLD",
  "EYES",
  "\n"
};


/* ITEM_x (extra bits) */
char *extra_bits[] = {
  "GLOW",
  "HUM",
  "!RENT",
  "!DONATE",
  "!INVIS",
  "INVISIBLE",
  "!DROP",
  "!HUMAN",
  "!SYNTHETIC",
  "!PREDATOR",
  "ALIEN",					/* 10 */
  "!SELL",
  "!LOSE",
  "MOVEABLE",
  "MISSION",
  "TWO-HANDS",
  "Unused16",
  "Unused17",
  "Unused18",
  "Unused19",
  "Unused20",				/* 20 */
  "Unused21",
  "Unused22",
  "Unused23",
  "Unused24",
  "Unused25",				/* 25 */
  "Unused26",
  "Unused27",
  "Unused28",
  "Unused29",
  "APPROVED",				/* 30 */
  "DELETED",
  "\n"
};


/* APPLY_x */
char *apply_types[] = {
  "NONE",
  "STR",
  "AGL",
  "INT",
  "PER",
  "CON",
  "UNUSED",
  "CLASS",
  "UNUSED",
  "AGE",
  "CHAR_WEIGHT",
  "CHAR_HEIGHT",
  "MAXHIT",
  "MAXMOVE",
  "GOLD",
  "EXP",
  "ARMOR",
  "HITROLL",
  "DAMROLL",
  "\n"
};


char *ammo_types[] ={
	"10mm Caseless",			/* 1 */
	"Energy Pack",
	"9mm Magazine",
	".50 Caliber",
	"Incinerator Tank",			/* 5 */
	"Spear Tips",
	".357 Caliber",
	".45 Caliber",
	"9mm Black Talon",
	"30-30",					/* 10 */
	"30-06",
	"12-Gauge",
	"20-Gauge",
	"20mm Exp. Bolts",
	"\n"
};

/* CONT_x */
char *container_bits[] = {
  "CLOSEABLE",
  "PICKPROOF",
  "CLOSED",
  "LOCKED",
  "\n",
};


/* LIQ_x */
char *drinks[] =
{
  "water",
  "\n"
};


/* other constants for liquids ******************************************/


/* one-word alias for each drink */
char *drinknames[] =
{
  "water",
  "\n"
};


/* effect of drinks on hunger, thirst, and drunkenness -- see values.doc */
int drink_aff[][3] = {
  {0, 1, 10},
};


/* color of the various drinks */
char *color_liquid[] =
{
  "clear",
  "\n"
};


/* level of fullness for drink containers */
char *fullness[] =
{
  "less than half ",
  "about half ",
  "more than half ",
  ""
};


int rev_dir[] = {
	2,
	3,
	0,
	1,
	5,
	4
};


int movement_loss[] =
{
  1,	/* Inside     */
  1,	/* City       */
  2,	/* Field      */
  3,	/* Forest     */
  4,	/* Hills      */
  6,	/* Mountains  */
  4,	/* Swimming   */
  1,	/* Unswimable */
  10000,/* Flying     */
  5,    /* Underwater */
  0,	/* Space      */
  0,    /* Deep space */
};


char *weekdays[] = {
	"Sunday",
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday"
};


char *month_name[] = {
	"January",		/* 0 */
	"February",
	"March",
	"April",
	"May",
	"June",
	"July",			/* 6 */
	"August",
	"September",
	"October",
	"November",
	"December",		/* 11 */
	"Month of the Dark Shades",
	"Month of the Shadows",
	"Month of the Long Shadows",
	"Month of the Ancient Darkness",
	"Month of the Great Evil"
};


char * level_string[LVL_CODER-LVL_IMMORT+1] = {
	"           ",
	"Terraformer",
	"Security/PR",
	"   Admin   ",
	"   Coder   "
};


char * dir_text[NUM_OF_DIRS] = {
	"the north",
	"the east",
	"the south",
	"the west",
	"above",
	"below",
};


char *mobprog_types[] = {
//"IN_FILE",
"ACT",
"SPEECH",
"RAND",
"FIGHT",
"DEATH",
"HITPRCNT",
"ENTRY",
"GREET",
"ALL_GREET",
"GIVE",
"BRIBE",
"EXIT",
"ALL_EXIT",
"DELAY",
"SCRIPT",
"WEAR",
"REMOVE",
"GET",
"DROP",
"EXAMINE",
"PULL",
"\n"
};


char * dir_text_2[NUM_OF_DIRS] = {
	"to the north of",
	"to the east of",
	"to the south of",
	"to the west of",
	"above",
	"below"
};


char *relation_colors = "nyr";


char *times_string[] = {
	"not at all",					//	0
	"once",							//	1
	"a couple times",				//	2
	"a few times",					//	3
	"several times",				//	4
	"five times",					//	5
	"six times",					//	6
	"seven times",					//	7
	"eight times",					//	8
	"nine times",					//	9
	"ten times",					//	10
	"over and over",				//	11
	"so much you can't remember",	//	12
	"\n"
};

char *relations[] = {
	"friend",
	"neutral",
	"enemy",
	"\n"
};


char *staff_bits[] = {
	"GENERAL",
	"ADMIN",
	"SECURITY",
	"GAME",
	"HOUSES",
	"CHARACTERS",
	"CLANS",
	"OLC",
	"OLCADMIN",
	"SOCIALS",
	"HELP",
	"SHOPS",
	"SCRIPTS",
	"IMC",
	"\n"
};

