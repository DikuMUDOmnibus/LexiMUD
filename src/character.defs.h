/*************************************************************************
*   File: character.defs.h           Part of Aliens vs Predator: The MUD *
*  Usage: Header for character-related defines                           *
*************************************************************************/

#ifndef __CHARACTER_DEFS_H__
#define __CHARACTER_DEFS_H__

#define NOBODY				-1    /* nil reference for mobiles		*/

enum Race {
	RACE_UNDEFINED	= -1,
	RACE_HUMAN,
	RACE_SYNTHETIC,
	RACE_PREDATOR,
	RACE_ALIEN,
	RACE_OTHER
};
#define NUM_RACES			5

//	Sex
enum Sex {
	SEX_NEUTRAL = 0,
	SEX_MALE,
	SEX_FEMALE
};


//	Positions
enum Position {
	POS_DEAD = 0,			//	dead
	POS_MORTALLYW,			//	mortally wounded
	POS_INCAP,				//	incapacitated
	POS_STUNNED,			//	stunned
	POS_SLEEPING,			//	sleeping
	POS_RESTING,			//	resting
	POS_SITTING,			//	sitting
	POS_FIGHTING,			//	fighting
	POS_STANDING			//	standing
};



/* Player flags: used by char_data.char_specials.act */
#define PLR_TRAITOR		(1 << 0)   /* Player is a traitor			*/
#define PLR_unused1		(1 << 1)   /* Player is a player-thief		*/
#define PLR_FROZEN		(1 << 2)   /* Player is frozen				*/
#define PLR_DONTSET		(1 << 3)   /* Don't EVER set (ISNPC bit)	*/
#define PLR_WRITING		(1 << 4)   /* Player writing (board/mail/olc)	*/
#define PLR_MAILING		(1 << 5)   /* Player is writing mail		*/
#define PLR_CRASH		(1 << 6)   /* Player needs to be crash-saved	*/
#define PLR_SITEOK		(1 << 7)   /* Player has been site-cleared	*/
#define PLR_NOSHOUT		(1 << 8)   /* Player not allowed to shout/goss	*/
#define PLR_NOTITLE		(1 << 9)   /* Player not allowed to set title	*/
#define PLR_DELETED		(1 << 10)  /* Player deleted - space reusable	*/
#define PLR_LOADROOM	(1 << 11)  /* Player uses nonstandard loadroom	*/
#define PLR_unused12	(1 << 12)  /* Player shouldn't be on wizlist	*/
#define PLR_NODELETE	(1 << 13)  /* Player shouldn't be deleted	*/
#define PLR_INVSTART	(1 << 14)  /* Player should enter game wizinvis	*/


/* Mobile flags: used by char_data.char_specials.act */
#define MOB_SPEC			(1 << 0)  /* Mob has a callable spec-proc	*/
#define MOB_SENTINEL		(1 << 1)  /* Mob should not move		*/
#define MOB_SCAVENGER		(1 << 2)  /* Mob picks up stuff on the ground	*/
#define MOB_ISNPC			(1 << 3)  /* (R) Automatically set on all Mobs	*/
#define MOB_AWARE			(1 << 4)  /* Mob can't be backstabbed		*/
#define MOB_AGGRESSIVE		(1 << 5)  /* Mob hits players in the room	*/
#define MOB_STAY_ZONE		(1 << 6)  /* Mob shouldn't wander out of zone	*/
#define MOB_WIMPY			(1 << 7)  /* Mob flees if severely injured	*/
#define MOB_AGGR_ALL		(1 << 8)  /* auto attack everything		*/
#define MOB_MEMORY			(1 << 9) /* remember attackers if attacked	*/
#define MOB_HELPER			(1 << 10) /* attack PCs fighting other NPCs	*/
#define MOB_NOCHARM			(1 << 11) /* Mob can't be charmed		*/
#define MOB_NOSUMMON		(1 << 12) /* Mob can't be summoned		*/
#define MOB_NOSLEEP			(1 << 13) /* Mob can't be slept		*/
#define MOB_NOBASH			(1 << 14) /* Mob can't be bashed (e.g. trees)	*/
#define MOB_NOBLIND			(1 << 15) /* Mob can't be blinded		*/
#define MOB_ACIDBLOOD		(1 << 16) /* MOB has acid blood (splash) */
#define MOB_PROGMOB			(1 << 17) /* Mob is a Program MOB */

#define MOB_APPROVED		(1 << 20) /* MOB has been APPROVED for Auto-Loading */
#define MOB_TRAITOR			(1 << 30) /* MOB is a traitor */
#define MOB_DELETED			(1 << 31) /* Mob is deleted */

/* Preference flags: used by char_data.player_specials.pref */
#define PRF_BRIEF		(1 << 0)		//	Room descs won't normally be shown
#define PRF_COMPACT		(1 << 1)		//	No extra CRLF pair before prompts
#define PRF_PK			(1 << 2)		//	Can PK/be PK'd
//#define PRF_NOTELL		(1 << 3)  /* Can't receive tells		*/
//#define PRF_NORACE		(1 << 4)  /* Display hit points in prompt	*/
//#define PRF_NOCLAN		(1 << 5)  /* Display mana points in prompt	*/
//#define PRF_unused6		(1 << 6)  /* Display move points in prompt	*/
#define PRF_AUTOEXIT	(1 << 7)		//	Display exits in a room
#define PRF_NOHASSLE	(1 << 8)		//	Staff are immortal
//#define PRF_MISSION		(1 << 9)  /* On quest				*/
#define PRF_SUMMONABLE	(1 << 10) /* Can be summoned			*/
#define PRF_NOREPEAT	(1 << 11) /* No repetition of comm commands	*/
#define PRF_HOLYLIGHT	(1 << 12) /* Can see in dark			*/
#define PRF_COLOR		(1 << 13) /* Color (low bit)			*/
//#define PRF_unused14	(1 << 14) /* Color (high bit)			*/
//#define PRF_NOWIZ		(1 << 15) /* Can't hear wizline			*/
#define PRF_LOG1		(1 << 16) /* On-line System Log (low bit)	*/
#define PRF_LOG2		(1 << 17) /* On-line System Log (high bit)	*/
//#define PRF_NOMUSIC		(1 << 18) /* Can't hear music channel		*/
//#define PRF_NOGOSS		(1 << 19) /* Can't hear gossip channel		*/
//#define PRF_NOGRATZ		(1 << 20) /* Can't hear grats channel		*/
#define PRF_ROOMFLAGS	(1 << 21) /* Can see room flags (ROOM_x)	*/
//#define PRF_unused22	(1 << 22) /* Can hear clan talk */
//#define PRF_AFK			(1 << 23)
#define PRF_AUTOSWITCH	(1 << 25)
//#define PRF_NOINFO		(1 << 26)


class Channel {
public:
	enum {
		NoShout		= (1 << 0),
		NoTell		= (1 << 1),
		NoChat		= (1 << 2),
		Mission		= (1 << 3),
		NoMusic		= (1 << 4),
		NoGratz		= (1 << 5),
		NoInfo		= (1 << 6),
		NoWiz		= (1 << 7),
		NoRace		= (1 << 8),
		NoClan		= (1 << 9)
	};
};


/* Affect bits: used in char_data.char_specials.saved.affected_by */
/* WARNING: In the world files, NEVER set the bits marked "R" ("Reserved") */
#define AFF_BLIND				(1 << 0)		//	(R) Char is blind
#define AFF_INVISIBLE			(1 << 1)		//	Char is invisible
#define AFF_DETECT_INVIS		(1 << 2)		//	Char can see invis chars
#define AFF_SENSE_LIFE			(1 << 3)		//	Char can sense hidden life
#define AFF_WATERWALK			(1 << 4)		//	Char can walk on water
#define AFF_SANCTUARY			(1 << 5)		//	Char protected by sanct.
#define AFF_GROUP				(1 << 6)		//	(R) Char is grouped
#define AFF_FLYING				(1 << 7)		//	Char is flying
#define AFF_INFRAVISION   	    (1 << 8)		//	Char can see in dark
#define AFF_POISON				(1 << 9)		//	(R) Char is poisoned
#define AFF_SLEEP				(1 << 10)		//	(R) Char unconscious
#define AFF_NOTRACK				(1 << 11)		//	Char can't be tracked
#define AFF_SNEAK				(1 << 12)		//	Char can move quietly
#define AFF_HIDE				(1 << 13)		//	Char is hidden
#define AFF_CHARM				(1 << 14)		//	Char is charmed
#define AFF_MOTIONTRACKING		(1 << 15)		//	Char detects motion
#define AFF_SPACESUIT			(1 << 16)		//	Char is vacuum-safe.  Phwooosh!
#define AFF_LIGHT				(1 << 17)		//	Gives off light
#define AFF_TRAPPED				(1 << 18)		//	Character is stunned/cocooned!
#define AFF_IMPREGNATED			(1 << 19)		//	Character is impregnated...


/* Character equipment positions: used as index for char_data.equipment[] */
/* NOTE: Don't confuse these constants with the ITEM_ bitvectors
   which control the valid places you can wear a piece of equipment */
#define WEAR_FINGER_R   0
#define WEAR_FINGER_L   1
#define WEAR_NECK		2
#define WEAR_BODY		3
#define WEAR_HEAD		4
#define WEAR_LEGS		5
#define WEAR_FEET		6
#define WEAR_HANDS		7
#define WEAR_ARMS		8
#define WEAR_ABOUT		9
#define WEAR_WAIST		10
#define WEAR_WRIST_R	11
#define WEAR_WRIST_L	12
#define WEAR_EYES		13
#define WEAR_HAND_R		14
#define WEAR_HAND_L		15

#define NUM_WEARS		16	/* This must be the # of eq positions!! */

/* position of messages in the list for WEAR_HAND_x, depending on object */
#define POS_WIELD_TWO	16
#define POS_HOLD_TWO	17
#define POS_WIELD		18
#define POS_WIELD_OFF	19
#define POS_LIGHT		20
#define POS_HOLD		21


enum {
	LVL_IMMORT	= 101,
	LVL_STAFF	= 102,
	LVL_SRSTAFF	= 103,
	LVL_ADMIN	= 104,
	LVL_CODER	= 105,

	LVL_FREEZE	= LVL_ADMIN
};



//	Player conditions
enum { DRUNK, FULL, THIRST };

const SInt32	MAX_NAME_LENGTH		= 12;
const SInt32	MAX_PWD_LENGTH		= 10;
const SInt32	MAX_TITLE_LENGTH	= 80;
const SInt32	HOST_LENGTH			= 30;
const SInt32	EXDSCR_LENGTH		= 480;
const SInt32	MAX_SKILLS			= 25;
const SInt32	MAX_AFFECT			= 32;
const SInt32	MAX_ICE_LENGTH		= 160;


enum Relation {
	RELATION_NONE = -1,
	RELATION_FRIEND,
	RELATION_NEUTRAL,
	RELATION_ENEMY
};


#endif