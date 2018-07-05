
#define NOTHING			-1    /* nil reference for objects		*/

/* Item types: used by obj_data.obj_flags.type_flag */
#define ITEM_LIGHT		1		/* Item is a light source	*/
#define ITEM_WEAPON		2		/* Item is a weapon		*/
#define ITEM_FIREWEAPON	3		/* Item is a loadable weapon	*/
#define ITEM_MISSILE	4		/* Item is a loadable weapon's ammo	*/
#define ITEM_THROW		5               /* Item can be thrown as weapon */
#define ITEM_GRENADE	6               /* Item is a grenade      */
#define ITEM_BOW		7               /* shoots arrows */
#define ITEM_ARROW		8
#define ITEM_BOOMERANG	9
#define ITEM_TREASURE   10		/* Item is a treasure, not gold	*/
#define ITEM_ARMOR      11		/* Item is armor		*/
#define ITEM_OTHER		12		/* Misc object			*/
#define ITEM_TRASH		13		/* Trash - shopkeeps won't buy	*/
#define ITEM_CONTAINER	14		/* Item is a container		*/
#define ITEM_NOTE		15		/* Item is note 		*/
#define ITEM_KEY		16		/* Item is a key		*/
#define ITEM_PEN		17		/* Item is money (gold)		*/
#define ITEM_BOAT		18		/* Item is a boat		*/
#define ITEM_FOOD		19
#define ITEM_DRINKCON	20
#define ITEM_FOUNTAIN	21
#define ITEM_VEHICLE	22
#define ITEM_DRONE		23
#define ITEM_V_HATCH	24
#define ITEM_V_CONTROLS	25
#define ITEM_V_WINDOW	26
#define ITEM_V_WEAPON	27
#define ITEM_BED		28
#define ITEM_CHAIR		29
#define ITEM_BOARD		30
#define ITEM_ATTACHMENT	31


/* Take/Wear flags: used by obj_data.obj_flags.wear_flags */
#define ITEM_WEAR_TAKE		(1 << 0)  /* Item can be takes		*/
#define ITEM_WEAR_FINGER	(1 << 1)  /* Can be worn on finger	*/
#define ITEM_WEAR_NECK		(1 << 2)  /* Can be worn around neck 	*/
#define ITEM_WEAR_BODY		(1 << 3)  /* Can be worn on body 	*/
#define ITEM_WEAR_HEAD		(1 << 4)  /* Can be worn on head 	*/
#define ITEM_WEAR_LEGS		(1 << 5)  /* Can be worn on legs	*/
#define ITEM_WEAR_FEET		(1 << 6)  /* Can be worn on feet	*/
#define ITEM_WEAR_HANDS		(1 << 7)  /* Can be worn on hands	*/
#define ITEM_WEAR_ARMS		(1 << 8)  /* Can be worn on arms	*/
#define ITEM_WEAR_SHIELD	(1 << 9)  /* Can be used as a shield	*/
#define ITEM_WEAR_ABOUT		(1 << 10) /* Can be worn about body 	*/
#define ITEM_WEAR_WAIST 	(1 << 11) /* Can be worn around waist 	*/
#define ITEM_WEAR_WRIST		(1 << 12) /* Can be worn on wrist 	*/
#define ITEM_WEAR_WIELD		(1 << 13) /* Can be wielded		*/
#define ITEM_WEAR_HOLD		(1 << 14) /* Can be held		*/
#define ITEM_WEAR_EYES		(1 << 15) /* Can be worn on eyes */


/* Extra object flags: used by obj_data.obj_flags.extra_flags */
#define ITEM_GLOW			(1 << 0)	/* Item is glowing		*/
#define ITEM_HUM			(1 << 1)	/* Item is humming		*/
#define ITEM_NORENT			(1 << 2)	/* Item cannot be rented	*/
#define ITEM_NODONATE		(1 << 3)	/* Item cannot be donated	*/
#define ITEM_NOINVIS		(1 << 4)	/* Item cannot be made invis	*/
#define ITEM_INVISIBLE		(1 << 5)	/* Item is invisible		*/
#define ITEM_NODROP			(1 << 6)	/* Item is cursed: can't drop	*/
#define ITEM_ANTI_HUMAN		(1 << 7)	/* Not usable by humans		*/
#define ITEM_ANTI_SYNTHETIC	(1 << 8)	/* Not usable by synthetics	*/
#define ITEM_ANTI_PREDATOR	(1 << 9)	/* Not usable by predators	*/
#define ITEM_ALIEN			(1 << 10)	/* Usable by Aliens	*/
#define ITEM_NOSELL			(1 << 11)	/* Shopkeepers won't touch it	*/
#define ITEM_NOLOSE			(1 << 12)	/* Don't lose it when you die */
#define ITEM_MOVEABLE		(1 << 13)	/* Can move it */
#define ITEM_MISSION		(1 << 14)	/* Missoin item - I thought this already existed! */
#define ITEM_TWO_HAND		(1 << 15)

#define ITEM_APPROVED		(1 << 30)	/* Item has been APPROVED for auto-loading */
#define ITEM_DELETED		(1 << 31)	/* Item has been DELETED */

/* Container flags - value[1] */
#define CONT_CLOSEABLE      (1 << 0)	/* Container can be closed	*/
#define CONT_PICKPROOF      (1 << 1)	/* Container is pickproof	*/
#define CONT_CLOSED         (1 << 2)	/* Container is closed		*/
#define CONT_LOCKED         (1 << 3)	/* Container is locked		*/


/* Some different kind of liquids for use in values of drink containers */
#define LIQ_WATER      0

#define MAX_OBJ_AFFECT		6	/* Used in obj_file_elem *DO*NOT*CHANGE* */