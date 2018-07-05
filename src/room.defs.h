#define NOWHERE			-1    /* nil reference for room-database	*/

#define NUM_OF_DIRS		6	/* number of directions in a room (nsewud) */

/* The cardinal directions: used as index to room_data.dir_option[] */
#define NORTH			0
#define EAST			1
#define SOUTH			2
#define WEST			3
#define UP				4
#define DOWN			5


/* Room flags: used in room_data.room_flags */
/* WARNING: In the world files, NEVER set the bits marked "R" ("Reserved") */
#define ROOM_DARK			(1 << 0)	//	Dark
#define ROOM_DEATH			(1 << 1)	//	Death trap
#define ROOM_NOMOB			(1 << 2)	//	MOBs not allowed
#define ROOM_INDOORS		(1 << 3)	//	Indoors
#define ROOM_PEACEFUL		(1 << 4)	//	Violence not allowed
#define ROOM_SOUNDPROOF		(1 << 5)	//	Shouts, gossip blocked
#define ROOM_NOTRACK		(1 << 6)	//	Track won't go through
#define ROOM_PARSE			(1 << 7)	//	Use String Parser
#define ROOM_TUNNEL			(1 << 8)	//	room for only 1 pers
#define ROOM_PRIVATE		(1 << 9)	//	Can't teleport in
#define ROOM_GODROOM		(1 << 10)	//	LVL_STAFF+ only allowed
#define ROOM_HOUSE			(1 << 11)	//	(R) Room is a house
#define ROOM_HOUSE_CRASH	(1 << 12)	//	(R) House needs saving
#define ROOM_ATRIUM			(1 << 13)	//	(R) The door to a house
#define ROOM_UNUSED_14		(1 << 14)	//	UNUSED
#define ROOM_BFS_MARK		(1 << 15)	//	(R) breath-first srch mrk
#define ROOM_VEHICLE		(1 << 16)	//	Vehicles allowed
#define ROOM_GRGODROOM		(1 << 17)	//	GRGODs or higher only
#define ROOM_ULTRAPRIVATE	(1 << 18)	//	FearItself only
#define ROOM_GRAVITY		(1 << 19)	//	Room has gravity, players fall down :-)
#define ROOM_VACUUM			(1 << 20)	//	GASP!  No...air...
#define	ROOM_DELETED		(1 << 31)	//	Room is deleted

/* Exit info: used in room_data.dir_option.exit_info */
#define EX_ISDOOR			(1 << 0)   /* Exit is a door		*/
#define EX_CLOSED			(1 << 1)   /* The door is closed	*/
#define EX_LOCKED			(1 << 2)   /* The door is locked	*/
#define EX_PICKPROOF		(1 << 3)   /* Lock can't be picked	*/
#define EX_HIDDEN			(1 << 4)   /* Exit is hidden */
#define EX_NOSHOOT			(1 << 5)
#define EX_NOMOVE			(1 << 6)
#define	EX_NOMOB			(1 << 7)
#define	EX_NOVEHICLES		(1 << 8)


/* Sector types: used in room_data.sector_type */
#define SECT_INSIDE				0			/* Indoors			*/
#define SECT_CITY				1			/* In a city			*/
#define SECT_FIELD				2			/* In a field		*/
#define SECT_FOREST				3			/* In a forest		*/
#define SECT_HILLS				4			/* In the hills		*/
#define SECT_MOUNTAIN			5			/* On a mountain		*/
#define SECT_WATER_SWIM			6			/* Swimmable water		*/
#define SECT_WATER_NOSWIM		7			/* Water - need a boat	*/
#define SECT_UNDERWATER			8			/* Underwater		*/
#define SECT_FLYING				9			/* Wheee!			*/
#define SECT_SPACE				10			/* Guess...			*/
#define SECT_DEEPSPACE			11			/* Guess again!		*/


/* Sun state for sunlight */
#define SUN_DARK	0
#define SUN_RISE	1
#define SUN_LIGHT	2
#define SUN_SET		3


/* Sky conditions for weather */
#define SKY_CLOUDLESS	0
#define SKY_CLOUDY	1
#define SKY_RAINING	2
#define SKY_LIGHTNING	3
