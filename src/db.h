/* ************************************************************************
*   File: db.h                                          Part of CircleMUD *
*  Usage: header file for database handling                               *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#ifndef __DB_H__
#define __DB_H__

#include "types.h"
#include "stl.llist.h"

/* arbitrary constants used by index_boot() (must be unique) */
#define DB_BOOT_WLD	0
#define DB_BOOT_MOB	1
#define DB_BOOT_OBJ	2
#define DB_BOOT_ZON	3
#define DB_BOOT_SHP	4
#define DB_BOOT_HLP	5
/*#ifdef CLANS_PATCH
#define DB_BOOT_CLAN	6
#endif*/
#define DB_BOOT_TRG	7

#define ROOM_ID_BASE	50000

/* names of various files and directories */
#define INDEX_FILE	"index"		/* index of world files		*/
#define MINDEX_FILE	"index.mini"	/* ... and for mini-mud-mode	*/


#ifdef CIRCLE_MAC

#define	SEPERATOR	":"

#define WLD_PREFIX	":world:wld:"	/* room definitions		*/
#define MOB_PREFIX	":world:mob:"	/* monster prototypes		*/
#define OBJ_PREFIX	":world:obj:"	/* object prototypes		*/
#define ZON_PREFIX	":world:zon:"	/* zon defs & command tables	*/
#define SHP_PREFIX	":world:shp:"	/* shop definitions		*/
#define TRG_PREFIX	":world:trg:"
#define HLP_PREFIX	":text:help:"	/* for HELP <keyword>		*/
#define BRD_PREFIX  ":etc:board:"
#define MOB_DIR    	":world:prg:"     /* Mob programs                 */
#define PLR_PREFIX	":pfiles:"	/* player ascii file directory	*/
#define	TXT_PREFIX	":text:"
#define MSC_PREFIX	":misc:"
#define ETC_PREFIX	":etc:"
#define HOUSE_DIR	":house:"
#define PLROBJS_PREFIX	":plrobjs:"
#define BOARD_DIRECTORY ETC_PREFIX "boards:"
#define IMC_DIR		":imc:"

#define FASTBOOT_FILE   ":fastboot"  /* autorun: boot without sleep  */
#define KILLSCRIPT_FILE ":killscript"/* autorun: shut mud down       */
#define PAUSE_FILE      ":pause"      /* autorun: don't restart mud   */

#else

#define SEPERATOR	"/"

#define WLD_PREFIX		"world/wld/"	/* room definitions		*/
#define MOB_PREFIX		"world/mob/"	/* monster prototypes		*/
#define OBJ_PREFIX		"world/obj/"	/* object prototypes		*/
#define ZON_PREFIX		"world/zon/"	/* zon defs & command tables	*/
#define SHP_PREFIX		"world/shp/"	/* shop definitions		*/
#define TRG_PREFIX		"world/trg/"
#define HLP_PREFIX		"text/help/"	/* for HELP <keyword>		*/
#define BRD_PREFIX		"etc/board/"
#define MOB_DIR			"world/prg/"     /* Mob programs                 */
#define PLR_PREFIX		"pfiles/"	/* player ascii file directory	*/
#define TXT_PREFIX		"text/"
#define MSC_PREFIX		"misc/"
#define ETC_PREFIX		"etc/"
#define HOUSE_DIR		"house/"
#define PLROBJS_PREFIX	"plrobjs/"
#define IMC_DIR			"imc/"

#define BOARD_DIRECTORY ETC_PREFIX "boards/"
	
#define FASTBOOT_FILE   "../.fastboot"  /* autorun: boot without sleep  */
#define KILLSCRIPT_FILE "../.killscript"/* autorun: shut mud down       */
#define PAUSE_FILE      "../pause"      /* autorun: don't restart mud   */

#endif

#define CREDITS_FILE	TXT_PREFIX "credits"	/* for the 'credits' command	*/
#define NEWS_FILE		TXT_PREFIX "news"	/* for the 'news' command	*/
#define MOTD_FILE		TXT_PREFIX "motd"	/* messages of the day / mortal	*/
#define IMOTD_FILE		TXT_PREFIX "imotd"	/* messages of the day / immort	*/
#define HELP_PAGE_FILE	HLP_PREFIX "screen" /* for HELP <CR>		*/
#define INFO_FILE		TXT_PREFIX "info"	/* for INFO			*/
#define WIZLIST_FILE	TXT_PREFIX "wizlist"	/* for WIZLIST			*/
#define IMMLIST_FILE	TXT_PREFIX "immlist"	/* for IMMLIST			*/
#define BACKGROUND_FILE	TXT_PREFIX "background" /* for the background story	*/
#define POLICIES_FILE	TXT_PREFIX "policies"	/* player policies/rules	*/
#define HANDBOOK_FILE	TXT_PREFIX "handbook"	/* handbook for new immorts	*/
#define PLR_INDEX_FILE	PLR_PREFIX "plr_index" /* player index file		*/
#define HELP_FILE		HLP_PREFIX "help.hlp"

#define IDEA_FILE		MSC_PREFIX "ideas"	/* for the 'idea'-command	*/
#define TYPO_FILE		MSC_PREFIX "typos"	/*         'typo'		*/
#define BUG_FILE		MSC_PREFIX "bugs"	/*         'bug'		*/
#define MESS_FILE		MSC_PREFIX "messages"	/* damage messages		*/
#define SOCMESS_FILE	MSC_PREFIX "socials"	/* messgs for social acts	*/
#define XNAME_FILE		MSC_PREFIX "xnames"	/* invalid name substrings	*/

#define MAIL_FILE		ETC_PREFIX "plrmail"	/* for the mudmail system	*/
#define BAN_FILE		ETC_PREFIX "badsites"	/* for the siteban system	*/
#define CLAN_FILE		ETC_PREFIX "clans"		/* For clan info		*/
#define HCONTROL_FILE	ETC_PREFIX "hcontrol"  /* for the house system		*/

/* public procedures in db.c */
void	boot_db(void);
int	create_entry(const char *name);
void	zone_update(void);
SInt32	get_id_by_name(const char *name);
const char	*get_name_by_id(SInt32 id);

int	vnum_mobile(char *searchname, CharData *ch);
void	reset_char(CharData *ch);
void	save_player_index(void);

int	vnum_object(char *searchname, CharData *ch);
void sprintbits(UInt32 vektor,char *outstring);

int vnum_trigger(char *searchname, CharData * ch);
void fprint_triglist(FILE *stream, struct int_list *list);

void vwear_object(int wearpos, CharData * ch);

#define REAL 0
#define VIRTUAL 1

/* structure for the reset commands */
struct reset_com {
	char	command;	/* current command							*/

	bool	if_flag;	/* if TRUE: exe only if preceding exe'd		*/
	SInt32	repeat;		/* Number of times to repeat this command	*/
	SInt32	arg1;		/*											*/
	SInt32	arg2;		/* Arguments to the command					*/
	SInt32	arg3;		/*											*/
	SInt32	arg4;
	UInt32	line;		/* line number this command appears on		*/

	char *command_string;
	/* 
	 *  Commands:              *
	 *  'M': Read a mobile     *
	 *  'O': Read an object    *
	 *  'G': Give obj to mob   *
	 *  'P': Put obj in obj    *
	 *  'G': Obj to char       *
	 *  'E': Obj to char equip *
	 *  'D': Set state of door *
	 */
};


/* zone definition structure. for the 'zone-table'   */
struct zone_data {
	char			*name;			//	name of this zone
	LList<char *>	builders;		//	zone builders
	int				lifespan;		//	how long between resets (minutes)
	int				age;			//	current age of this zone (minutes)
	int				top;			//	upper limit for rooms in this zone

	int				reset_mode;		//	conditions for reset (see below)
	int				number;			//	virtual number of this zone
	struct reset_com *cmd;			//	command table for reset
	
	//	Weather system
	int				pressure;		//	How is the pressure ( Mb )
	int				change;			//	How fast and what way does it change.
	int				sky;			//	How is the sky.
	/*
	 *  Reset mode:									*
	 *  0: Don't reset, and don't update age.		*
	 *  1: Reset if no PC's are located in zone.	*
	 *  2: Just reset.								*
	 */
};


/* for queueing zones for update   */
struct reset_q_element {
   int	zone_to_reset;            /* ref to zone_data */
   struct reset_q_element *next;
};


/* structure for the update queue     */
struct reset_q_type {
   struct reset_q_element *head;
   struct reset_q_element *tail;
};


struct player_index_element {
	char *	name;
	SInt32	id;
};

extern char	*OK;
extern char	*NOPERSON;
extern char	*NOEFFECT;

#endif