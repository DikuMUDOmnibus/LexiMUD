/* ************************************************************************
*   File: spells.h                                      Part of CircleMUD *
*  Usage: header file: constants and fn prototypes for spell system       *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#define TYPE_UNDEFINED               -1
#define SPELL_RESERVED_DBC            0  /* SKILL NUMBER ZERO -- RESERVED */

/* PLAYER SKILLS - Numbered from 1 to MAX_SKILLS */
#define SKILL_BACKSTAB              1 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_BASH                  2 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_HIDE                  3 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_KICK                  4 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_PICK_LOCK             5 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_PUNCH                 6 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_RESCUE                7 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_SNEAK                 8 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_CIRCLE                9 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_TRACK		 		   10 /* Reserved Skill[] DO NOT CHANGE */
/* New skills may be added here up to MAX_SKILLS (25) */
#define SKILL_THROW					11 /* new */
#define SKILL_SHOOT					12
#define SKILL_DODGE					13
#define SKILL_WATCH					14
#define SKILL_BITE					15
#define SKILL_TRAIN					16
#define SKILL_COCOON				17
#define SKILL_IDENTIFY				18
#define SKILL_CRITICAL				19
/* #define NUM_SKILLS 12 */

/*
 *  NON-PLAYER AND OBJECT SPELLS AND SKILLS
 *  The practice levels for the spells and skills below are _not_ recorded
 *  in the playerfile; therefore, the intended use is for spells and skills
 *  associated with objects (such as SPELL_IDENTIFY used with scrolls of
 *  identify) or non-players (such as NPC-only spells).
 */

#define TOP_SPELL_DEFINE	     299
/* NEW NPC/OBJECT SPELLS can be inserted here up to 299 */


/* WEAPON ATTACK TYPES */

#define TYPE_HIT                     300
#define TYPE_STING                   301
#define TYPE_WHIP                    302
#define TYPE_SLASH                   303
#define TYPE_BITE                    304
#define TYPE_BLUDGEON                305
#define TYPE_CRUSH                   306
#define TYPE_POUND                   307
#define TYPE_CLAW                    308
#define TYPE_MAUL                    309
#define TYPE_THRASH                  310
#define TYPE_PIERCE                  311
#define TYPE_BLAST				     312
#define TYPE_PUNCH				     313
#define TYPE_STAB				     314

#define TYPE_SHOOT					 315
#define TYPE_BURN					 316
/* new attack types can be added here - up to TYPE_SUFFERING */
#define TYPE_SUFFERING		    	 399

#define TYPE_EXPLOSION				 400
#define TYPE_GRAVITY				 401
#define TYPE_ACIDBLOOD				 402
#define TYPE_MISC					 403

#define ACID_BURN_TIME		4


#define TAR_IGNORE        1
#define TAR_CHAR_ROOM     2
#define TAR_CHAR_WORLD    4
#define TAR_FIGHT_SELF    8
#define TAR_FIGHT_VICT   16
#define TAR_SELF_ONLY    32 /* Only a check, use with i.e. TAR_CHAR_ROOM */
#define TAR_NOT_SELF     64 /* Only a check, use with i.e. TAR_CHAR_ROOM */
#define TAR_OBJ_INV     128
#define TAR_OBJ_ROOM    256
#define TAR_OBJ_WORLD   512
#define TAR_OBJ_EQUIP  1024

struct skill_info_type {
   int min_level[NUM_RACES];
};

/* Possible Targets:

   bit 0 : IGNORE TARGET
   bit 1 : PC/NPC in room
   bit 2 : PC/NPC in world
   bit 3 : Object held
   bit 4 : Object in inventory
   bit 5 : Object in room
   bit 6 : Object in world
   bit 7 : If fighting, and no argument, select tar_char as self
   bit 8 : If fighting, and no argument, select tar_char as victim (fighting)
   bit 9 : If no argument, select self, if argument check that it IS self.

*/

/* Attacktypes with grammar */

struct attack_hit_type {
   char	*singular;
   char	*plural;
};
/* basic magic calling functions */

int find_skill_num(char *name);

/* other prototypes */
void spell_level(int spell, int race, int level);
void init_spell_levels(void);
char *skill_name(int num);
