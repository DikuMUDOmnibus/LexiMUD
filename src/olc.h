/*************************************************************************
*   File: olc.h                      Part of Aliens vs Predator: The MUD *
*  Usage: Header file for the OLC System                                 *
*************************************************************************/

#ifndef __OLC_H__
#define __OLC_H__

#include "types.h"
#include "characters.h"

class	TrigData;
class	ExtraDesc;

//	Include a short explanation of each field in your zone files,
#undef ZEDIT_HELP_IN_FILE

//	Clear the screen before certain Oasis menus
#define CLEAR_SCREEN	1


//	Macros, defines, structs and globals for the OLC suite.
#define NUM_ROOM_FLAGS 		21
#define NUM_ROOM_SECTORS	12

#define NUM_MOB_FLAGS		18
#define NUM_EXIT_FLAGS		9
#define NUM_AFF_FLAGS		18
#define NUM_ATTACK_TYPES	15
#define NUM_SHOOT_TYPES     2
/*
 * Define this to how many MobProg scripts you have.
 */
#define NUM_PROGS			22

#define NUM_WTRIG_TYPES		8
#define NUM_OTRIG_TYPES		13
#define NUM_MTRIG_TYPES		16

#define NUM_AMMO_TYPES		14

#define NUM_ITEM_TYPES		31
#define NUM_ITEM_FLAGS		16
#define NUM_ITEM_WEARS 		16
#define NUM_APPLIES			19
#define NUM_LIQ_TYPES 		1
#define NUM_POSITIONS		9

#define NUM_GENDERS			3
#define NUM_SHOP_FLAGS 		2
#define NUM_TRADERS 		3

/* aedit permissions magic number */
#define AEDIT_PERMISSION	999
#define HEDIT_PERMISSION	666
#define CEDIT_PERMISSION	888

#define LVL_BUILDER		LVL_STAFF

/*. Limit info .*/
#define MAX_ROOM_NAME	75
#define MAX_MOB_NAME	50
#define MAX_OBJ_NAME	50
#define MAX_ROOM_DESC	1024
#define MAX_EXIT_DESC	256
#define MAX_EXTRA_DESC  512
#define MAX_MOB_DESC	512
#define MAX_OBJ_DESC	512
#define MAX_HELP_KEYWORDS	75
#define MAX_HELP_ENTRY		1024

/*
 * Utilities exported from olc.c.
 */
void strip_string(char *);
void cleanup_olc(DescriptorData *d, UInt8 cleanup_type);
void olc_add_to_save_list(int zone, UInt8 type);
void olc_remove_from_save_list(int zone, UInt8 type);
bool get_zone_perms(CharData * ch, int rnum);
int real_zone(int number);

/*
 * OLC structures.
 */

class HelpElement;

struct olc_data {
	int mode;
	int zone_num;
	int number;
	int value;
	int total_mprogs;
	char *storage;
	CharData *mob;
	RoomData *room;
	ObjData *obj;
	ClanData *clan;
	struct zone_data *zone;
	struct shop_data *shop;
	TrigData *trig;
	struct social_messg *action;
	ExtraDesc *desc;
	HelpElement *help;
	struct mob_prog_data *mprogl;
	struct mob_prog_data *mprog;
	struct int_list *intlist;
	Opinion *opinion;
};

struct olc_save_info {
  int zone;
  char type;
  struct olc_save_info *next;
};


/*. Exported globals .*/
#ifdef _OASIS_OLC_
char *nrm, *grn, *cyn, *yel;
struct olc_save_info *olc_save_list = NULL;
#else
extern char *nrm, *grn, *cyn, *yel;
extern struct olc_save_info *olc_save_list;
#endif


/*
 * Descriptor access macros.
 */
#define OLC_MODE(d)		((d)->olc->mode)		/* Parse input mode		*/
#define OLC_NUM(d)		((d)->olc->number)		/* Room/Obj VNUM		*/
#define OLC_VAL(d)		((d)->olc->value)		/* Scratch variable		*/
#define OLC_ZNUM(d)		((d)->olc->zone_num) 	/* Real zone number		*/
#define OLC_STORAGE(d)	((d)->olc->storage)		/* For command storage	*/
#define OLC_ROOM(d)		((d)->olc->room)		/* Room structure		*/
#define OLC_OBJ(d)		((d)->olc->obj)			/* Object structure		*/
#define OLC_ZONE(d)		((d)->olc->zone)		/* Zone structure		*/
#define OLC_MOB(d)		((d)->olc->mob)			/* Mob structure		*/
#define OLC_SHOP(d)		((d)->olc->shop)		/* Shop structure		*/
#define OLC_ACTION(d)	((d)->olc->action)		/* Action structure		*/
#define OLC_DESC(d)		((d)->olc->desc)		/* Extra description	*/
#define OLC_MPROGL(d)	((d)->olc->mprogl)		/* Temporary MobProg	*/
#define OLC_MPROG(d)	((d)->olc->mprog)		/* MobProg list			*/
#define OLC_MTOTAL(d)	((d)->olc->total_mprogs)/* Total mprog number	*/
#define OLC_HELP(d)		((d)->olc->help)		/* Help structure		*/
#define OLC_TRIG(d)		((d)->olc->trig)		/* Trig structure		*/
#define OLC_INTLIST(d)	((d)->olc->intlist)		/* Intlist				*/
#define OLC_CLAN(d)		((d)->olc->clan)		/* Clan list			*/

/*
 * Other macros.
 */
#define OLC_EXIT(d)		(OLC_ROOM(d)->dir_option[OLC_VAL(d)])
#define OLC_OPINION(d)	((d)->olc->opinion)
#define GET_OLC_ZONE(c)	((c)->player_specials->saved.olc_zone)

/*
 * Cleanup types.
 */
#define CLEANUP_ALL			1	/* Free the whole lot  */
#define CLEANUP_STRUCTS		2	/* Don't free strings  */

/*
 * Add/Remove save list types.
 */
#define OLC_SAVE_ROOM			0
#define OLC_SAVE_OBJ			1
#define OLC_SAVE_ZONE			2
#define OLC_SAVE_MOB			3
#define OLC_SAVE_SHOP			4
#define OLC_SAVE_ACTION			5
#define OLC_SAVE_HELP			6
#define OLC_SAVE_TRIGGER		7
#define OLC_SAVE_CLAN			8


/*
 * Submodes of OEDIT connectedness.
 */
#define OEDIT_MAIN_MENU              	1
#define OEDIT_EDIT_NAMELIST          	2
#define OEDIT_SHORTDESC              	3
#define OEDIT_LONGDESC               	4
#define OEDIT_ACTDESC                	5
#define OEDIT_TYPE                   	6
#define OEDIT_EXTRAS                 	7
#define OEDIT_WEAR                  	8
#define OEDIT_WEIGHT                	9
#define OEDIT_COST                  	10
#define OEDIT_SPEED		            	11
#define OEDIT_TIMER                 	12
#define OEDIT_VALUE_1               	13
#define OEDIT_VALUE_2               	14
#define OEDIT_VALUE_3               	15
#define OEDIT_VALUE_4               	16
#define OEDIT_VALUE_5               	17
#define OEDIT_VALUE_6               	18
#define OEDIT_VALUE_7               	19
#define OEDIT_VALUE_8               	20
#define OEDIT_APPLY                 	21
#define OEDIT_APPLYMOD              	22
#define OEDIT_EXTRADESC_KEY         	23
#define OEDIT_CONFIRM_SAVEDB        	24
#define OEDIT_CONFIRM_SAVESTRING    	25
#define OEDIT_PROMPT_APPLY          	26
#define OEDIT_EXTRADESC_DESCRIPTION 	27
#define OEDIT_EXTRADESC_MENU        	28
#define OEDIT_LEVEL                 	29
#define OEDIT_GUN_MENU                  30
#define OEDIT_GUN_DICE_NUM				31
#define OEDIT_GUN_DICE_SIZE				32
#define OEDIT_GUN_RATE					33
#define OEDIT_GUN_RANGE					34
#define OEDIT_GUN_AMMO_TYPE				35
#define OEDIT_GUN_ATTACK_TYPE			36
#define OEDIT_AFFECTS					37
#define OEDIT_TRIGGERS					38
#define OEDIT_TRIGGER_ADD				39
#define OEDIT_TRIGGER_PURGE				40


/* Submodes of REDIT connectedness */
#define REDIT_MAIN_MENU					1
#define REDIT_NAME						2
#define REDIT_DESC						3
#define REDIT_FLAGS						4
#define REDIT_SECTOR					5
#define REDIT_EXIT_MENU					6
#define REDIT_CONFIRM_SAVEDB			7
#define REDIT_CONFIRM_SAVESTRING		8
#define REDIT_EXIT_NUMBER				9
#define REDIT_EXIT_DESCRIPTION			10
#define REDIT_EXIT_KEYWORD				11
#define REDIT_EXIT_KEY					12
#define REDIT_EXIT_DOORFLAGS			13
#define REDIT_EXTRADESC_MENU			14
#define REDIT_EXTRADESC_KEY				15
#define REDIT_EXTRADESC_DESCRIPTION		16


//	Submodes of ZEDIT connectedness
#define ZEDIT_MAIN_MENU					0
#define ZEDIT_DELETE_ENTRY				1
#define ZEDIT_NEW_ENTRY					2
#define ZEDIT_CHANGE_ENTRY				3
#define ZEDIT_COMMAND_TYPE				4
#define ZEDIT_IF_FLAG					5
#define ZEDIT_ARG1						6
#define ZEDIT_ARG2						7
#define ZEDIT_ARG3						8
#define ZEDIT_ARG4						9
#define ZEDIT_ZONE_NAME					10
#define ZEDIT_ZONE_LIFE					11
#define ZEDIT_ZONE_TOP					12
#define ZEDIT_ZONE_RESET				13
#define ZEDIT_CONFIRM_SAVESTRING		14
#define ZEDIT_REPEAT					15
#define ZEDIT_COMMAND_MENU				16


//	Submodes of MEDIT connectedness
#define MEDIT_MAIN_MENU					0
#define MEDIT_ALIAS						1
#define MEDIT_S_DESC					2
#define MEDIT_L_DESC					3
#define MEDIT_D_DESC					4
#define MEDIT_NPC_FLAGS					5
#define MEDIT_AFF_FLAGS					6
#define MEDIT_CONFIRM_SAVESTRING		7
#define MEDIT_TRIGGERS					8
#define MEDIT_ATTRIBUTES				9
#define MEDIT_MPROG_COMLIST				20		// Never-Reach Case
#define MEDIT_MPROG_ARGS				21
//	Numerical responses
#define MEDIT_NUMERICAL_RESPONSE		30
#define MEDIT_SEX						31
#define MEDIT_HITROLL					32
#define MEDIT_DAMROLL					33
#define MEDIT_NDD						34
#define MEDIT_SDD						35
#define MEDIT_NUM_HP_DICE				36
#define MEDIT_SIZE_HP_DICE				37
#define MEDIT_ADD_HP					38
#define MEDIT_AC						39
#define MEDIT_POS						40
#define MEDIT_DEFAULT_POS				41
#define MEDIT_ATTACK					42
#define MEDIT_LEVEL						43
#define MEDIT_RACE						44
#define MEDIT_MPROG						45
#define MEDIT_PURGE_MPROG				46
#define MEDIT_CHANGE_MPROG				50
#define MEDIT_MPROG_TYPE				51
#define MEDIT_TRIGGER_ADD				54
#define MEDIT_TRIGGER_PURGE				55
#define MEDIT_CLAN						56
#define MEDIT_ATTR_STR					60
#define MEDIT_ATTR_INT					61
#define MEDIT_ATTR_PER					62
#define MEDIT_ATTR_AGI					63
#define MEDIT_ATTR_CON					64
#define MEDIT_OPINIONS					70
#define MEDIT_OPINIONS_HATES_SEX		71
#define MEDIT_OPINIONS_HATES_RACE		72
#define MEDIT_OPINIONS_HATES_VNUM		73
#define MEDIT_OPINIONS_FEARS_SEX		74
#define MEDIT_OPINIONS_FEARS_RACE		75
#define MEDIT_OPINIONS_FEARS_VNUM		76


//	Submodes of SEDIT connectedness
#define SEDIT_MAIN_MENU					0
#define SEDIT_CONFIRM_SAVESTRING		1
#define SEDIT_NOITEM1					2
#define SEDIT_NOITEM2					3
#define SEDIT_NOCASH1					4
#define SEDIT_NOCASH2					5
#define SEDIT_NOBUY						6
#define SEDIT_BUY						7
#define SEDIT_SELL						8
#define SEDIT_PRODUCTS_MENU				11
#define SEDIT_ROOMS_MENU				12
#define SEDIT_NAMELIST_MENU				13
#define SEDIT_NAMELIST					14
//	Numerical responses
#define SEDIT_NUMERICAL_RESPONSE		20
#define SEDIT_OPEN1						21
#define SEDIT_OPEN2						22
#define SEDIT_CLOSE1					23
#define SEDIT_CLOSE2					24
#define SEDIT_KEEPER					25
#define SEDIT_BUY_PROFIT				26
#define SEDIT_SELL_PROFIT				27
#define SEDIT_TYPE_MENU					29
#define SEDIT_DELETE_TYPE				30
#define SEDIT_DELETE_PRODUCT			31
#define SEDIT_NEW_PRODUCT				32
#define SEDIT_DELETE_ROOM				33
#define SEDIT_NEW_ROOM					34
#define SEDIT_SHOP_FLAGS				35
#define SEDIT_NOTRADE					36


//	Submodes of AEDIT connectedness
#define AEDIT_CONFIRM_SAVESTRING		0
#define AEDIT_CONFIRM_EDIT				1
#define AEDIT_CONFIRM_ADD				2
#define AEDIT_MAIN_MENU					3
#define AEDIT_ACTION_NAME				4
#define AEDIT_SORT_AS					5
#define AEDIT_MIN_CHAR_POS				6
#define AEDIT_MIN_VICT_POS				7
#define AEDIT_HIDDEN_FLAG				8
#define AEDIT_MIN_CHAR_LEVEL			9
#define AEDIT_NOVICT_CHAR				10
#define AEDIT_NOVICT_OTHERS				11
#define AEDIT_VICT_CHAR_FOUND			12
#define AEDIT_VICT_OTHERS_FOUND			13
#define AEDIT_VICT_VICT_FOUND			14
#define AEDIT_VICT_NOT_FOUND			15
#define AEDIT_SELF_CHAR					16
#define AEDIT_SELF_OTHERS				17
#define AEDIT_VICT_CHAR_BODY_FOUND     	18
#define AEDIT_VICT_OTHERS_BODY_FOUND   	19
#define AEDIT_VICT_VICT_BODY_FOUND     	20
#define AEDIT_OBJ_CHAR_FOUND			21
#define AEDIT_OBJ_OTHERS_FOUND			22


//	Submodes of HEDIT connectedness
#define HEDIT_CONFIRM_SAVESTRING		0
#define HEDIT_MAIN_MENU					1
#define HEDIT_ENTRY						2
#define HEDIT_MIN_LEVEL					3
#define HEDIT_KEYWORDS					4


//	Submodes of SCRIPTEDIT connectedness
#define SCRIPTEDIT_CONFIRM_SAVESTRING	0
#define SCRIPTEDIT_CONFIRM_EDIT			1
#define SCRIPTEDIT_CONFIRM_ADD			2
#define SCRIPTEDIT_MAIN_MENU			3
#define SCRIPTEDIT_NAME					4
#define SCRIPTEDIT_CLASS				5
#define SCRIPTEDIT_TYPE					6
#define SCRIPTEDIT_ARG					7
#define SCRIPTEDIT_NARG					8
#define SCRIPTEDIT_SCRIPT				9


//	Submodes of CEDIT connectedness
#define CEDIT_CONFIRM_SAVESTRING		0
#define CEDIT_MAIN_MENU					3
#define CEDIT_NAME						4
#define CEDIT_DESCRIPTION				5
#define CEDIT_OWNER						6
#define CEDIT_ROOM						7
#define CEDIT_SAVINGS					8

#endif