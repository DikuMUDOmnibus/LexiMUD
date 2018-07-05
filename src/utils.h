/* ************************************************************************
*   File: utils.h                                       Part of CircleMUD *
*  Usage: header file: utility macros and prototypes of utility funcs     *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

/* external declarations and prototypes **********************************/
#ifndef __UTILS_H__
#define __UTILS_H__

#include "types.h"

#undef log

#define mudlog(a,b,c,d)	mudlogf(b,c,d,a)

/* public functions in utils.c */
char	*str_dup(const char *source);
int		str_cmp(const char *arg1, const char *arg2);
int		strn_cmp(const char *arg1, const char *arg2, int n);
char	*str_str(char *cs, char *ct);
int		touch(char *path);
void	log_death_trap(CharData *ch);
int		Number(int from, int to);
int		dice(int number, int size);
void	sprintbit(UInt32 vektor, char *names[], char *result);
void	sprinttype(int type, char *names[], char *result);
int		get_line(FILE *fl, char *buf);
int		get_filename(const char *orig_name, char *filename);
struct TimeInfoData age(CharData *ch);
int		num_pc_in_room(RoomData *room);
int		replace_str(char **string, char *pattern, char *replacement, int rep_all, int max_size);
void	format_text(char **ptr_string, int mode, DescriptorData *d, int maxlen);
void	core_dump_func(const char *who, SInt16 line);
#define core_dump()	core_dump_func(__FUNCTION__, __LINE__)

/* random functions in random.c */
void circle_srandom(unsigned int initial_seed);
UInt32 circle_random(void);


/* in log.c */
/*
 * The attribute specifies that mudlogf() is formatted just like printf() is.
 * 4,5 means the format string is the fourth parameter and start checking
 * the fifth parameter for the types in the format string.  Not certain
 * if this is a GNU extension so if you want to try it, put #if 1 below.
 */
void	log(const char *format, ...) __attribute__ ((format (printf, 1, 2)));

void    mudlogf(SInt8 type, UInt32 level, bool file, const char *format, ...) __attribute__ ((format (printf, 4, 5)));


/* undefine MAX and MIN so that our functions are used instead */
#ifdef MAX
#undef MAX
#endif

#ifdef MIN
#undef MIN
#endif

int MAX(int a, int b);
int MIN(int a, int b);

/* in magic.c */
int	circle_follow(CharData *ch, CharData * victim);

/* in act.informative.c */
void	look_at_room(CharData *ch, int mode);
void	look_at_rnum(CharData *ch, RNum room, int mode);

/* in act.movmement.c */
int	do_simple_move(CharData *ch, int dir, int following);
int	perform_move(CharData *ch, int dir, int following);

/* in limits.c */
int	hit_limit(CharData *ch);
int	move_limit(CharData *ch);
int	hit_gain(CharData *ch);
int	move_gain(CharData *ch);
void	advance_level(CharData *ch);
void	gain_condition(CharData *ch, int condition, int value);
void	check_idling(CharData *ch);
void	point_update(void);


/* various constants *****************************************************/


/* defines for mudlog() */
#define OFF	0
#define BRF	1
#define NRM	2
#define CMP	3

/* breadth-first searching */
#define BFS_ERROR			-1
#define BFS_ALREADY_THERE	-2
#define BFS_NO_PATH			-3

/* mud-life time */
#define SECS_PER_MUD_HOUR	75
#define SECS_PER_MUD_DAY	(24*SECS_PER_MUD_HOUR)
#define SECS_PER_MUD_MONTH	(30*SECS_PER_MUD_DAY)
#define SECS_PER_MUD_YEAR	(12*SECS_PER_MUD_MONTH)

#define PULSES_PER_MUD_HOUR     (SECS_PER_MUD_HOUR*PASSES_PER_SEC)

/* real-life time (remember Real Life?) */
#define SECS_PER_REAL_MIN	60
#define SECS_PER_REAL_HOUR	(60*SECS_PER_REAL_MIN)
#define SECS_PER_REAL_DAY	(24*SECS_PER_REAL_HOUR)
#define SECS_PER_REAL_YEAR	(365*SECS_PER_REAL_DAY)


/* string utils **********************************************************/


#define YESNO(a) ((a) ? "YES" : "NO")
#define ONOFF(a) ((a) ? "ON" : "OFF")

// IS_UPPER and IS_LOWER added by dkoepke
#define IS_UPPER(c) ((c) >= 'A' && (c) <= 'Z')
#define IS_LOWER(c) ((c) >= 'a' && (c) <= 'z')

#define LOWER(c)   (IS_UPPER(c) ? ((c)+('a'-'A')) : (c))
#define UPPER(c)   (IS_LOWER(c) ? ((c)+('A'-'a')) : (c))

#define ISNEWL(ch) ((ch) == '\n' || (ch) == '\r') 
#define IF_STR(st) ((st) ? (st) : "\0")
#define CAP(st)  (*(st) = UPPER(*(st)), st)

#define AN(string) (strchr("aeiouAEIOU", *string) ? "an" : "a")

#define END_OF(buffer)		((buffer) + strlen((buffer)))

/* memory utils **********************************************************/


#if !defined(__STRING)
#define __STRING(x)	#x
#endif

#if BUFFER_MEMORY

#define CREATE(result, type, number)  do {\
	if (!((result) = (type *) debug_calloc ((number), sizeof(type), __STRING(result), __FUNCTION__, __LINE__)))\
		{ perror("malloc failure"); abort(); } } while(0)

#define RECREATE(result,type,number) do {\
	if (!((result) = (type *) debug_realloc ((result), sizeof(type) * (number), __STRING(result), __FUNCTION__, __LINE__)))\
		{ perror("realloc failure"); abort(); } } while(0)

#define free(variable)	debug_free((variable), __FUNCTION__, __LINE__)

#define str_dup(variable)	debug_str_dup((variable), __STRING(variable), __FUNCTION__, __LINE__)

#else

#define CREATE(result, type, number)  do {\
	if ((number * sizeof(type)) <= 0) {\
	mudlogf(BRF, 102, TRUE, "CODEERR: Attempt to alloc 0 at %s:%d", __FUNCTION__, __LINE__);\
	}\
	if (!((result) = (type *) ALLOC ((number ? number : 1), sizeof(type))))\
		{ perror("malloc failure"); abort(); } } while(0)

#define RECREATE(result,type,number) do {\
  if (!((result) = (type *) realloc ((result), sizeof(type) * (number))))\
		{ perror("realloc failure"); abort(); } } while(0)

#endif
/*
 * the source previously used the same code in many places to remove an item
 * from a list: if it's the list head, change the head, else traverse the
 * list looking for the item before the one to be removed.  Now, we have a
 * macro to do this.  To use, just make sure that there is a variable 'temp'
 * declared as the same type as the list to be manipulated.  BTW, this is
 * a great application for C++ templates but, alas, this is not C++.  Maybe
 * CircleMUD 4.0 will be...
 */
#define REMOVE_FROM_LIST(item, head, next)	\
	if ((item) == (head))		\
		head = (item)->next;		\
	else {				\
		temp = head;			\
		while (temp && (temp->next != (item))) \
			temp = temp->next;		\
		if (temp)				\
			temp->next = (item)->next;	\
	}					\


/* basic bitvector utils *************************************************/


#define IS_SET(flag,bit)  ((flag) & (bit))
#define SET_BIT(var,bit)  ((var) |= (bit))
#define REMOVE_BIT(var,bit)  ((var) &= ~(bit))
#define TOGGLE_BIT(var,bit) ((var) = (var) ^ (bit))

#define MOB_FLAGS(ch)	((ch)->general.act)
#define PLR_FLAGS(ch)	((ch)->general.act)
#define PRF_FLAGS(ch)	((ch)->player->special.preferences)
#define AFF_FLAGS(ch)	((ch)->general.affected_by)
#define CHN_FLAGS(ch)	((ch)->player->special.channels)
#define ROOM_FLAGS(loc)	(world[(loc)].flags)
#define STF_FLAGS(ch)	((ch)->player->special.staff_flags)

#define IS_NPC(ch)		(IS_SET(MOB_FLAGS(ch), MOB_ISNPC))
#define IS_MOB(ch)		(IS_NPC(ch) && ((ch)->nr >-1))

#define MOB_FLAGGED(ch, flag)		(IS_NPC(ch) && IS_SET(MOB_FLAGS(ch), (flag)))
#define PLR_FLAGGED(ch, flag)		(!IS_NPC(ch) && IS_SET(PLR_FLAGS(ch), (flag)))
#define AFF_FLAGGED(ch, flag)		(IS_SET(AFF_FLAGS(ch), (flag)))
#define PRF_FLAGGED(ch, flag)		(!IS_NPC(ch) && IS_SET(PRF_FLAGS(ch), (flag)))
#define CHN_FLAGGED(ch, flag)		(!IS_NPC(ch) && IS_SET(CHN_FLAGS(ch), (flag)))
#define ROOM_FLAGGED(loc, flag)		(IS_SET(ROOM_FLAGS(loc), (flag)))
#define EXIT_FLAGGED(exit, flag)	(IS_SET((exit)->exit_info, (flag)))
#define OBJVAL_FLAGGED(obj, flag)	(IS_SET(GET_OBJ_VAL((obj), 1), (flag)))
#define OBJWEAR_FLAGGED(obj, flag)	(IS_SET((obj)->wear, (flag)))
#define OBJ_FLAGGED(obj, flag)		(IS_SET(GET_OBJ_EXTRA(obj), (flag)))
#define STF_FLAGGED(ch, flag)		(!IS_NPC(ch) && IS_SET(STF_FLAGS(ch), (flag)))

#define PLR_TOG_CHK(ch,flag)	((TOGGLE_BIT(PLR_FLAGS(ch), (flag))) & (flag))
#define PRF_TOG_CHK(ch,flag)	((TOGGLE_BIT(PRF_FLAGS(ch), (flag))) & (flag))


/* room utils ************************************************************/


#define SECT(room)	(world[(room)].sector_type)

#define IS_DARK(room)	(!world[room].light && \
						(ROOM_FLAGGED(room, ROOM_DARK) || \
						( ( SECT(room) != SECT_INSIDE && \
						SECT(room) != SECT_CITY && !ROOM_FLAGGED(room, ROOM_INDOORS)) && \
						(sunlight == SUN_SET || sunlight == SUN_DARK)) ) )

#define IS_LIGHT(room)  (!IS_DARK(room))

#define GET_ROOM_SPEC(room) ((room) >= 0 ? world[(room)].func : NULL)


/* char utils ************************************************************/

#define IN_ROOM(ch)			((ch)->in_room)
#define GET_WAS_IN(ch)		((ch)->was_in_room)
#define GET_AGE(ch)			(age(ch).year)

//#define GET_NAME(ch)
//	(IS_NPC(ch) ? SSData((ch)->general.shortDesc) : SSData((ch)->general.name))
//#define GET_NAME(ch)		((ch)->GetName())
#define GET_TITLE(ch)		(SSData((ch)->general.title))
#define GET_LEVEL(ch)		((ch)->general.level)
#define GET_PASSWD(ch)		((ch)->player->passwd)
#define GET_PROMPT(ch)		((ch)->player->prompt)
#define GET_PFILEPOS(ch)	((ch)->pfilepos)

/*
 * I wonder if this definition of GET_REAL_LEVEL should be the definition
 * of GET_LEVEL?  JE
 */
#define GET_REAL_LEVEL(ch) \
	(ch->desc && ch->desc->original ? GET_LEVEL(ch->desc->original) : GET_LEVEL(ch))

#define GET_RACE(ch)		((ch)->general.race)  
#define GET_HEIGHT(ch)		((ch)->general.height)
#define GET_WEIGHT(ch)		((ch)->general.weight)
#define GET_SEX(ch)			((ch)->general.sex)

#define GET_STR(ch)			((ch)->aff_abils.str)
#define GET_AGI(ch)			((ch)->aff_abils.agi)
#define GET_INT(ch)			((ch)->aff_abils.intel)
#define GET_PER(ch)			((ch)->aff_abils.per)
#define GET_CON(ch)			((ch)->aff_abils.con)

#define GET_AC(ch)			((ch)->points.armor)
#define GET_HIT(ch)			((ch)->points.hit)
#define GET_MAX_HIT(ch)		((ch)->points.max_hit)
#define GET_MOVE(ch)		((ch)->points.move)
#define GET_MAX_MOVE(ch)	((ch)->points.max_move)
#define GET_MISSION_POINTS(ch)	((ch)->points.mp)
#define GET_HITROLL(ch)		((ch)->points.hitroll)
#define GET_DAMROLL(ch)		((ch)->points.damroll)

#define GET_POS(ch)			((ch)->general.position)
#define GET_IDNUM(ch)		((ch)->player->idnum)
#define IS_CARRYING_W(ch)	((ch)->general.misc.carry_weight)
#define IS_CARRYING_N(ch)	((ch)->general.misc.carry_items)
#define FIGHTING(ch)		((ch)->general.fighting)
#define HUNTING(ch)			((ch)->general.hunting)

#define GET_COND(ch, i)		((ch)->general.conditions[(i)])
#define GET_LOADROOM(ch)	((ch)->player->special.load_room)
#define GET_PRACTICES(ch)	((ch)->player->special.pracs)
#define GET_INVIS_LEV(ch)	((ch)->player->special.invis_level)
#define GET_WIMP_LEV(ch)	((ch)->player->special.wimp_level)
//#define GET_FREEZE_LEV(ch)	((ch)->player->special.freeze_level)
#define GET_BAD_PWS(ch)		((ch)->player->special.bad_pws)
#define POOFIN(ch)			((ch)->player->poofin)
#define POOFOUT(ch)			((ch)->player->poofout)
#define GET_AFK(ch)			((ch)->player->afk)

#define GET_ALIASES(ch)		((ch)->player->aliases)
#define GET_LAST_TELL(ch)	((ch)->player->last_tell)

#define GET_SKILL(ch, i)	((ch)->player->special.skills[i])
#define SET_SKILL(ch, i, pct)	{ (ch)->player->special.skills[i] = pct; }

#define GET_EQ(ch, i)		((ch)->equipment[i])

#define GET_MOB_SPEC(ch)	(IS_MOB(ch) ? (mob_index[(ch->nr)].func) : NULL)
#define GET_MOB_TRGS(ch)	(IS_MOB(ch) ? mob_index[(ch)->nr].triggers : NULL)
#define GET_MOB_RNUM(mob)	((mob)->nr)
#define GET_MOB_VNUM(mob)	(IS_MOB(mob) ? mob_index[GET_MOB_RNUM(mob)].vnum : -1)

#define GET_MOB_WAIT(ch)	((ch)->mob->wait_state)
#define GET_MOB_DODGE(ch)	((ch)->mob->dodge)

#define GET_CLAN(ch)		((ch)->general.misc.clannum)

#define GET_DEFAULT_POS(ch)	((ch)->mob->default_pos)
#define MEMORY(ch)			((ch)->mob->memory)

#define CAN_CARRY_W(ch)		(GET_STR(ch) * 1.5)
#define CAN_CARRY_N(ch)		(5 + (GET_AGI(ch) / 5))
#define AWAKE(ch)			(GET_POS(ch) > POS_SLEEPING)
#define CAN_SEE_IN_DARK(ch) \
   (AFF_FLAGGED(ch, AFF_INFRAVISION) || PRF_FLAGGED(ch, PRF_HOLYLIGHT) || GET_RACE(ch) == RACE_ALIEN)

#define CHAR_WATCHING(ch)	((ch)->general.misc.watching)


#define NO_STAFF_HASSLE(ch)	(IS_STAFF(ch) && PRF_FLAGGED(ch, PRF_NOHASSLE))

/* descriptor-based utils ************************************************/


#define WAIT_STATE(ch, cycle) { \
	if ((ch)->desc) (ch)->desc->wait = (cycle); \
	else if (IS_NPC(ch)) GET_MOB_WAIT(ch) = (cycle); }

#define CHECK_WAIT(ch)	(((ch)->desc) ? ((ch)->desc->wait > 1) : 0)
#define STATE(d)	((d)->connected)


/* object utils **********************************************************/


#define GET_OBJ_TYPE(obj)	((obj)->type)
#define GET_OBJ_COST(obj)	((obj)->cost)
#define GET_OBJ_EXTRA(obj)	((obj)->extra)
#define GET_OBJ_WEAR(obj)	((obj)->wear)
#define GET_OBJ_VAL(obj, val)	((obj)->value[(val)])
#define GET_OBJ_WEIGHT(obj)	((obj)->weight)
#define GET_OBJ_TIMER(obj)	((obj)->timer)
#define GET_OBJ_RNUM(obj)	((obj)->number)
#define GET_OBJ_VNUM(obj)	(GET_OBJ_RNUM(obj) >= 0 ? obj_index[GET_OBJ_RNUM(obj)].vnum : -1)

#define GET_OBJ_SPEED(obj)	((obj)->speed)

#define IS_GUN(obj)				((obj)->gun)
#define GET_GUN_INFO(obj)		((obj)->gun)
#define GET_GUN_DICE_NUM(obj)	(GET_GUN_INFO(obj)->dice.number)
#define GET_GUN_DICE_SIZE(obj)	(GET_GUN_INFO(obj)->dice.size)
#define GET_GUN_DICE(obj)		(dice(GET_GUN_DICE_NUM(obj), GET_GUN_DICE_SIZE(obj)))
#define GET_GUN_RATE(obj)		(GET_GUN_INFO(obj)->rate)
#define GET_GUN_RANGE(obj)		(GET_GUN_INFO(obj)->range)
#define GET_GUN_AMMO(obj)		(GET_GUN_INFO(obj)->ammo.amount)
#define GET_GUN_AMMO_TYPE(obj)	(GET_GUN_INFO(obj)->ammo.type)
#define GET_GUN_AMMO_VNUM(obj)	(GET_GUN_INFO(obj)->ammo.vnum)
#define GET_GUN_ATTACK_TYPE(obj)	(GET_GUN_INFO(obj)->attack)

#define GET_OBJ_SPEC(obj) (GET_OBJ_RNUM(obj) >= 0 ? (obj_index[GET_OBJ_RNUM(obj)].func) : NULL)

#define CAN_WEAR(obj, part) (IS_SET((obj)->wear, (part)))

#define IS_SITTABLE(obj)	((GET_OBJ_TYPE(obj) == ITEM_CHAIR) || (GET_OBJ_TYPE(obj) == ITEM_BED))
#define IS_SLEEPABLE(obj)	((GET_OBJ_TYPE(obj) == ITEM_BED))

#define IS_MOUNTABLE(obj)	((GET_OBJ_TYPE(obj) == ITEM_VEHICLE) && (GET_OBJ_VAL(obj, 0) == -1))

#define NO_LOSE(obj)		(OBJ_FLAGGED(obj, ITEM_NOLOSE | ITEM_MISSION))
#define NO_DROP(obj)		(OBJ_FLAGGED(obj, ITEM_NODROP | ITEM_MISSION))

#define GET_OBJ_TRGS(obj)	(GET_OBJ_RNUM(obj) >= 0 ? obj_index[GET_OBJ_RNUM(obj)].triggers : NULL)

/* compound utilities and other macros **********************************/


#define HSHR(ch) (GET_SEX(ch) ? (GET_SEX(ch)==SEX_MALE ? "his":"her") :"its")
#define HSSH(ch) (GET_SEX(ch) ? (GET_SEX(ch)==SEX_MALE ? "he" :"she") : "it")
#define HMHR(ch) (GET_SEX(ch) ? (GET_SEX(ch)==SEX_MALE ? "him":"her") : "it")

#define ANA(obj) (strchr("aeiouyAEIOUY", *SSData((obj)->name)) ? "An" : "A")
#define SANA(obj) (strchr("aeiouyAEIOUY", *SSData((obj)->name)) ? "an" : "a")


/* Various macros building up to CAN_SEE */

/*
#define LIGHT_OK(sub)	(!AFF_FLAGGED(sub, AFF_BLIND) && \
   (IS_LIGHT((sub)->in_room) || AFF_FLAGGED((sub), AFF_INFRAVISION) || \
   (GET_RACE(sub) == RACE_ALIEN)))

#define INVIS_OK(sub, obj) \
 ((!AFF_FLAGGED((obj),AFF_INVISIBLE) || AFF_FLAGGED(sub,AFF_DETECT_INVIS)) && \
 (!AFF_FLAGGED((obj), AFF_HIDE) || AFF_FLAGGED(sub, AFF_SENSE_LIFE)))

#define MORT_CAN_SEE(sub, obj) (LIGHT_OK(sub) && INVIS_OK(sub, obj) && \
	(!MOB_FLAGGED(obj, MOB_PROGMOB) || IS_STAFF(sub)))

#define IMM_CAN_SEE(sub, obj) \
   (MORT_CAN_SEE(sub, obj) || PRF_FLAGGED(sub, PRF_HOLYLIGHT))
*/

#define SELF(sub, obj)  ((sub) == (obj))

/* Can subject see character "obj"? */
/*
#define CAN_SEE(sub, obj) (SELF(sub, obj) || \
   ((GET_REAL_LEVEL(sub) >= GET_INVIS_LEV(obj)) && IMM_CAN_SEE(sub, obj)))
*/
/* End of CAN_SEE */

/*
#define INVIS_OK_OBJ(sub, obj) \
  (!IS_OBJ_STAT((obj), ITEM_INVISIBLE) || AFF_FLAGGED((sub), AFF_DETECT_INVIS))

#define MORT_CAN_SEE_OBJ(sub, obj) ((sub)->LightOk() && INVIS_OK_OBJ(sub, obj))

#define CAN_SEE_OBJ(sub, obj) \
   (MORT_CAN_SEE_OBJ(sub, obj) || PRF_FLAGGED((sub), PRF_HOLYLIGHT))
*/
#define CAN_CARRY_OBJ(ch,obj)  \
   (((IS_CARRYING_W(ch) + GET_OBJ_WEIGHT(obj)) <= CAN_CARRY_W(ch)) &&   \
    ((IS_CARRYING_N(ch) + 1) <= CAN_CARRY_N(ch)))

#define CAN_GET_OBJ(ch, obj)   \
   (CAN_WEAR((obj), ITEM_WEAR_TAKE) && CAN_CARRY_OBJ((ch),(obj)) && \
    (ch)->CanSee(obj))


#define PERS(ch, vict)   ((vict)->CanSee(ch) ? (ch)->GetName() : "someone")

#define OBJS(obj, vict) ((vict)->CanSee(obj) ? \
	SSData((obj)->shortDesc)  : "something")

#define OBJN(obj, vict) ((vict)->CanSee(obj) ? \
	fname(SSData((obj)->name)) : "something")


#define EXIT(ch, door)  (world[(ch)->in_room].dir_option[door])
#define EXITN(room, door)		(world[room].dir_option[door])

#define EXIT2(roomnum, door) (world[(roomnum)].dir_option[door])
#define CAN_GO2(roomnum, door) (EXIT2(roomnum, door) && \
                       (EXIT2(roomnum, door)->to_room != NOWHERE) && \
                       !IS_SET(EXIT2(roomnum,door)->exit_info, EX_CLOSED))

#define CAN_GO(ch, door) ((ch->in_room != NOWHERE) && EXIT(ch,door) && \
			 (EXIT(ch,door)->to_room != NOWHERE) && \
			 !IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED))

#define IS_HIDDEN_EXIT(ch, door)		(IS_SET(EXIT(ch, door)->exit_info, EX_HIDDEN))
#define EXIT_IS_HIDDEN(ch, door)		(IS_HIDDEN_EXIT(ch, door) &&\
										(!IS_SET(EXIT(ch, door)->exit_info, EX_ISDOOR) ||\
										IS_SET(EXIT(ch,door)->exit_info, EX_CLOSED)) &&\
										(GET_LEVEL(ch) < LVL_IMMORT))
#define IS_HIDDEN_EXITN(room, door)		(IS_SET(EXITN(room, door)->exit_info, EX_HIDDEN))
#define EXITN_IS_HIDDEN(room, door, ch)	(IS_HIDDEN_EXITN(room, door) &&\
										(!IS_SET(EXITN(room, door)->exit_info, EX_ISDOOR) ||\
										IS_SET(EXITN(room, door)->exit_info, EX_CLOSED)) &&\
										(GET_LEVEL(ch) < LVL_IMMORT))

#define RACE_ABBR(ch) (IS_NPC(ch) ? "--" : race_abbrevs[(int)GET_RACE(ch)])

#define IS_HUMAN(ch)		(GET_RACE(ch) == RACE_HUMAN)
#define IS_SYNTHETIC(ch)	(GET_RACE(ch) == RACE_SYNTHETIC)
#define IS_PREDATOR(ch)		(GET_RACE(ch) == RACE_PREDATOR)
#define IS_ALIEN(ch)		(GET_RACE(ch) == RACE_ALIEN)

#define IS_MARINE(ch)	(GET_RACE(ch) <= RACE_SYNTHETIC)

#define IS_TRAITOR(ch)	(IS_NPC(ch) ? IS_SET(MOB_FLAGS(ch), MOB_TRAITOR) \
						: IS_SET(PLR_FLAGS(ch), PLR_TRAITOR))

#define IS_STAFF(ch)		(!IS_NPC(ch) && (GET_LEVEL(ch) >= LVL_IMMORT))

#define OUTSIDE(ch) (!ROOM_FLAGGED((ch)->in_room, ROOM_INDOORS))


#define PURGED(i)	((i)->purged)
#define GET_ID(i)	((i)->id)

/* OS compatibility ******************************************************/


/* there could be some strange OS which doesn't have NULL... */
#ifndef NULL
#define NULL (Ptr)0
#endif

#if !defined(FALSE)
#define FALSE 0
#endif

#if !defined(TRUE)
#define TRUE  (!FALSE)
#endif

/* defines for fseek */
#ifndef SEEK_SET
#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2
#endif

//#if defined(NOCRYPT) || !defined(HAVE_CRYPT)
#define CRYPT(a,b) (a)
//#else
//#define CRYPT(a,b) ((char *) crypt((a),(b)))
//#endif

/* Special enhancements macros */

//char freeBuf[200];
//void Free_Error(char * file, int line);

/*
#define FREE(ptr)		if (ptr == NULL) {					\
							Free_Error(__FILE__, __LINE__);	\
						} else {							\
							free(ptr);						\
							ptr = NULL;						\
						}
*/
/* MOBProg Macros */
#define TARGET(mob)		(mob->mprog_target)

#endif