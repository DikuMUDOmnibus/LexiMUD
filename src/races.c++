


#include "structs.h"
#include "utils.h"
#include "spells.h"

/* prototypes */
int parse_race(char arg);
long find_race_bitvector(char arg);
void roll_real_abils(CharData * ch);
void do_start(CharData * ch);
void advance_level(CharData * ch);
void init_spell_levels(void);
void CheckRegenRates(CharData *ch);

/* extern variales */


char *race_abbrevs[] = { 
  "Hum",
  "Syn",
  "Prd",
  "Aln",
  "Oth",
  "\n"
};


char *pc_race_types[] = {
  "Human",
  "Synthetic",
  "Predator",
  "Alien",
  "Other",
  "\n"
};


char *race_types[] = {
  "human",
  "synthetic",
  "predator",
  "alien",
  "other",
  "\n"
};


/* The menu for choosing a race in interpreter.c: */
char *race_menu =
"\r\n"
"Select a Species:\r\n"
"  (H)uman             - The natural soldier\r\n"
"  (S)ynthetic human   - The artificial soldier\r\n"
"  (P)redator          - The natural hunter\r\n"
"  (A)lien             - The natural killer\r\n";

/*
 * The code to interpret a race letter (used in interpreter.c when a
 * new character is selecting a race).
 */
int parse_race(char arg) {
	switch (LOWER(arg)) {
		case 'h':	return RACE_HUMAN;
		case 's':	return RACE_SYNTHETIC;
		case 'p':	return RACE_PREDATOR;
		case 'a':	return RACE_ALIEN;
		default:	return RACE_UNDEFINED;
	}
}

long find_race_bitvector(char arg) {
	switch (LOWER(arg)) {
		case 'h':	return (1 << 0);
		case 's':	return (1 << 1);
		case 'p':	return (1 << 2);
		case 'a':	return (1 << 3);
		default:	return 0;
	}
}

/* #define LEARNED_LEVEL	0  % known which is considered "learned" */
/* #define MAX_PER_PRAC		1  max percent gain in skill per practice */
/* #define MIN_PER_PRAC		2  min percent gain in skill per practice */

int prac_params[3][NUM_RACES] = {
//	HUMAN	SYNTH	PRED	ALIEN
	{90,	95, 	85, 	75},		//	learned level
	{ 6,	 8, 	 5, 	 5},		//	max per prac
	{ 3,	 4, 	 2,  	 3}			//	min per prac
};


/*
 * ...And the appropriate rooms for each guildmaster/guildguard; controls
 * which types of people the various guildguards let through.  i.e., the
 * first line shows that from room 3017, only MAGIC_USERS are allowed
 * to go south.
 */
int guild_info[][3] = {

/* Midgaard */

/* Brass Dragon */

/* New Sparta */

/* this must go last -- add new guards above! */
{-1, -1, -1}
};




// Base Attack Chance {melee, ranged}
int thaco[NUM_RACES+1][2] = {
{50, 60},	//	Human
{60, 75},	//	Synthetic
{70, 45},	//	Predator
{75,  0},	//	Alien
{50, 50}	//	Animal
};


// MIN
UInt8 stat_min[NUM_RACES + 1][5] =
{		/*	STR	INT	PER	AGI	CON */
/* HUM */  { 20, 20, 20, 20, 20}, // 100
/* SYN */  { 30, 80, 40, 50,  1}, // 200
/* PRD */  { 40, 10, 10, 25, 40}, // 125
/* ALN */  { 40,  5, 25, 25, 10}, // 105
/* ANM */  {  1,  1,  1,  1,  1}
};


// MAX
UInt8 stat_max[NUM_RACES + 1][5] =
{		/*	STR	INT	PER	AGI	CON */
/* HUM */  { 80, 80, 80, 80, 80}, // 400
/* SYN */  { 90,100, 90, 95, 25}, // 400
/* PRD */  { 90, 50, 60, 75,100}, // 375
/* ALN */  { 90, 25, 40,100, 40}, // 295
/* ANM */  {100,100,100,100,100}
};

// 500
// 600
// 500
// 400


/*
 * Roll the 6 stats for a character... each stat is made of the sum of
 * the best 3 out of 4 rolls of a 6-sided die.  Each class then decides
 * which priority will be given for the best to worst stats.
 */
void roll_real_abils(CharData * ch) {
	ch->real_abils.str	= Number(stat_min[GET_RACE(ch)][0],stat_max[GET_RACE(ch)][0]);
	ch->real_abils.intel= Number(stat_min[GET_RACE(ch)][1],stat_max[GET_RACE(ch)][1]);
	ch->real_abils.per	= Number(stat_min[GET_RACE(ch)][2],stat_max[GET_RACE(ch)][2]);
	ch->real_abils.agi	= Number(stat_min[GET_RACE(ch)][3],stat_max[GET_RACE(ch)][3]);
	ch->real_abils.con	= Number(stat_min[GET_RACE(ch)][4],stat_max[GET_RACE(ch)][4]);
	
	ch->aff_abils = ch->real_abils;
}


/* Some initializations for characters, including initial skills */
void do_start(CharData * ch) {
	GET_LEVEL(ch) = 1;

	ch->set_title(NULL);

	switch (GET_RACE(ch)) {
		case RACE_HUMAN:
			ch->points.max_hit = 1000;
			SET_SKILL(ch, SKILL_PUNCH, 10);
			SET_SKILL(ch, SKILL_DODGE, 25);
			SET_SKILL(ch, SKILL_SHOOT, 25);
			break;
		case RACE_SYNTHETIC:
			ch->points.max_hit = 1250;
			SET_SKILL(ch, SKILL_PUNCH, 15);
			SET_SKILL(ch, SKILL_DODGE, 20);
			SET_SKILL(ch, SKILL_SHOOT, 25);
			break;
		case RACE_PREDATOR:
			ch->points.max_hit = 1500;
			SET_SKILL(ch, SKILL_PUNCH, 25);
			SET_SKILL(ch, SKILL_DODGE, 25);
			SET_SKILL(ch, SKILL_SHOOT, 10);
			break;
		case RACE_ALIEN:
			ch->points.max_hit = 1250;
			SET_SKILL(ch, SKILL_PUNCH, 35);
			SET_SKILL(ch, SKILL_DODGE, 25);
			break;
		default:
			ch->points.max_hit = 1;
			log("Serious Bug: hit DEFAULT on GET_RACE(ch) during do_start(ch)");
	}

	advance_level(ch);
	
	GET_HIT(ch) = GET_MAX_HIT(ch);
	GET_MOVE(ch) = GET_MAX_MOVE(ch);

	GET_COND(ch, THIRST) = -1;
	GET_COND(ch, FULL) = -1;
	GET_COND(ch, DRUNK) = -1;

	ch->player->time.played = 0;
	ch->player->time.logon = time(0);
}



/*
 * This function controls the change to maxmove, and maxhp for
 * each race every time they gain a level.
 */
void advance_level(CharData * ch) {
	int add_hp = 0, add_move = 0, i;

	if (IS_STAFF(ch)) {
		for (i = 0; i < 3; i++)
			GET_COND(ch, i) = (char) -1;
		SET_BIT(PRF_FLAGS(ch), PRF_HOLYLIGHT);
	}
	if (GET_RACE(ch) == RACE_ALIEN)
		GET_MAX_HIT(ch) += GET_CON(ch);
//	i = MAX(1, ((GET_PER(ch) + GET_INT(ch)) / 25));
//	i += MAX(1, i * (GET_LEVEL(ch) / 10));
//	GET_PRACTICES(ch) += Number(MAX(1, i / 2), i);
	i = MAX(1, (GET_INT(ch) * 2) / 25);
	i += MAX(1, i * (GET_LEVEL(ch) / 10));
	GET_PRACTICES(ch) += Number(MAX(1, i / 2), i);

	CheckRegenRates(ch);

	ch->save(NOWHERE);

//	mudlogf(CMP, LVL_STAFF, FALSE,  "%s advanced to level %d", ch->RealName(), GET_LEVEL(ch));
}



/*
 * SPELLS AND SKILLS.  This area defines which spells are assigned to
 * which classes, and the minimum level the character must be to use
 * the spell or skill.
 */
void init_spell_levels(void) {
	int x, y;
	int myArray[][NUM_RACES + 1] = {
	//					H	S	P	A
	{ SKILL_DODGE,		1,  1,  1,  1 },
	{ SKILL_SHOOT,		1,  1,  1,101 },
	{ SKILL_THROW,		1,  1,  1,101 },
	{ SKILL_PUNCH,		1,  1,  1,  1 },

	{ SKILL_BACKSTAB,  31, 61, 61, 31 },
	{ SKILL_BASH,	   11, 51, 21, 21 },
	{ SKILL_HIDE,	   31, 21, 41, 21 },
	{ SKILL_KICK,	   11, 51, 11,101 },
	{ SKILL_PICK_LOCK, 51, 11, 21, 51 },
	{ SKILL_RESCUE,	   11, 21, 51, 51 },
	{ SKILL_SNEAK,	   21, 11, 11, 11 },
	{ SKILL_TRACK,	   71, 31, 21, 11 },
	{ SKILL_WATCH,	   11, 11, 31, 21 },
	{ SKILL_CIRCLE,	   41, 71, 31, 61 },
	{ SKILL_BITE,	  101,101,101, 31 },
	{ SKILL_TRAIN,     61, 41, 51, 21 },
	{ SKILL_COCOON,	  101,101,101, 31 },
	{ SKILL_IDENTIFY,  51, 21, 41, 51 },
	{ SKILL_CRITICAL,  81, 81, 81, 81 },
	{ 0,				0,	0,	0,	0 },
	};

	for (x = 0; myArray[x][0]; x++)
		for (y = 0; y < (NUM_RACES - 1); y++)
			spell_level(myArray[x][0], y, myArray[x][y+1]);
}


/* NEW SYSTEM */
/* Names of class/levels and exp required for each level */

struct title_type titles[NUM_RACES][16] = {
		/* Humans */
	{	{"Civilian", "Civilian"},							/* 0 */
		{"Private", "Private"},
		{"Corporal", "Corporal"},
		{"Sergeant", "Sergeant"},
		{"Sergeant Major", "Sergeant Major"},
		{"Warrant Officer", "Warrant Officer"},				/* 5 */
		{"Lieutenant", "Lieutenant"},
		{"Captain", "Captain"},
		{"Major", "Major"},
		{"Colonel", "Colonel"},
		{"General", "General"},								/* 10 */
		{"Veteran", "Veteran"},
		{"the Dishonorably Discharged", "the Dishonorably Discharged"},
		{"the Court Marshalled", "the Court Marshalled"},
		{"is M.I.A.", "is M.I.A."},
		{"the Implementor", "Implementor"}					/* 15 */
	},
		/* Synthetics */
	{	{"Civilian", "Civilian"},							/* 0 */
		{"Private", "Private"},
		{"Corporal", "Corporal"},
		{"Sergeant", "Sergeant"},
		{"Sergeant Major", "Sergeant Major"},
		{"Warrant Officer", "Warrant Officer"},				/* 5 */
		{"Lieutenant", "Lieutenant"},
		{"Captain", "Captain"},
		{"Major", "Major"},
		{"Colonel", "Colonel"},
		{"General", "General"},								/* 10 */
		{"Veteran", "Veteran"},
		{"the Dishonorably Discharged", "the Dishonorably Discharged"},
		{"the Court Marshalled", "the Court Marshalled"},
		{"is M.I.A.", "is M.I.A."},
		{"the Implementor", "the Implementress"}			/* 15 */
	},
		/* PREDATORS */
	{	{"Suckling", "Suckling"},							/* 0 */
		{"Uninitiated Yautja", "Uninitiated Yautja"},
		{"Initiate", "Initiate"},
		{"Unblooded", "Unblooded"},
		{"Blooded", "Blooded"},
		{"Student", "Student"},								/* 5 */
		{"Lesser Warrior", "Lesser Warrior"},
		{"Warrior", "Warrior"},
		{"Champion", "Champion"},
		{"Elder", "Elder"},
		{"Leader", "Leader"},								/* 10 */
		{"Veteran", "Veteran"},
		{"the Outcast", "the Outcast"},
		{"the Chickenshit", "the Chickenshit"},
		{"the Fang-Face", "the Fang-Face"},
		{"the Implementor", "the Implementress"}			/* 15 */
	},
		/* ALIENS */
	{	{"Egg", "Egg"},										/* 0 */
		{"Facehugger", "Facehugger"},
		{"Chestburster", "Chestburster"},
		{"Alien", "Alien"},
		{"Scout", "Scout"},
		{"Drone", "Drone"},									/* 5 */
		{"Sentry", "Sentry"},
		{"Hunter", "Hunter"},
		{"Warrior", "Warrior"},
		{"Royal Guard", "Royal Guard"},
		{"King", "Queen"},									/* 10 */
		{"Immortal", "Immortal"},
		{"the Squashed Bug", "the Squashed Bug"},
		{"the Trophy", "the Trophy"},
		{"the Rogue", "the Bitch"},
		{"the Implementor", "the Implementress"}			/* 15 */
	}
};
