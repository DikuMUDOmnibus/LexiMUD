/**************************************************************************
*  File: scripts.h                                                        *
*  Usage: header file for script structures and contstants, and           *
*         function prototypes for scripts.c                               *
*                                                                         *
*                                                                         *
*  $Author: egreen $
*  $Date: 1996/09/24 03:48:42 $
*  $Revision: 3.6 $
**************************************************************************/


#ifndef __SCRIPTS_H__
#define __SCRIPTS_H__

#include "types.h"
#include "index.h"
#include "stl.llist.h"

#define MOB_TRIGGER				0
#define OBJ_TRIGGER				1
#define WLD_TRIGGER				2


#define TRIG_GLOBAL				(1 << 0)	//	MOW	- Random when nobody in zone, *-all,
#define TRIG_RANDOM				(1 << 1)	//	MOW
#define TRIG_COMMAND			(1 << 2)	//	MOW
#define TRIG_SPEECH				(1 << 3)	//	MOW
#define TRIG_GREET				(1 << 4)	//	MOW
#define TRIG_LEAVE				(1 << 5)	//	MOW
#define TRIG_DOOR				(1 << 6)	//	MOW
#define TRIG_DROP				(1 << 7)	//	 OW
#define TRIG_GET				(1 << 8)	//	MO	(MTRIG_RECEIVE, OTRIG_GET)
#define TRIG_ACT				(1 << 9)	//	M
#define TRIG_DEATH				(1 << 10)	//	M	(OW later)
#define TRIG_FIGHT				(1 << 11)	//	M	(OW later)
#define TRIG_HITPRCNT			(1 << 12)	//	M	(O later)
#define TRIG_SIT				(1 << 13)	//	 O
#define	TRIG_GIVE				(1 << 14)	//	 O
#define TRIG_WEAR				(1 << 15)	//	 O


/* mob trigger types */
#define MTRIG_GLOBAL			(1 << 0)		/* check even if zone empty   */
#define MTRIG_RANDOM			(1 << 1)		/* checked randomly           */
#define MTRIG_COMMAND			(1 << 2)		/* character types a command  */
#define MTRIG_SPEECH			(1 << 3)		/* a char says a word/phrase  */
#define MTRIG_ACT				(1 << 4)		/* word or phrase sent to act */
#define MTRIG_DEATH				(1 << 5)		/* character dies             */
#define MTRIG_GREET				(1 << 6)		/* something enters room seen */
#define MTRIG_GREET_ALL			(1 << 7)		/* anything enters room       */
#define MTRIG_ENTRY				(1 << 8)		/* the mob enters a room      */
#define MTRIG_RECEIVE			(1 << 9)		/* character is given obj     */
#define MTRIG_FIGHT				(1 << 10)		/* each pulse while fighting  */
#define MTRIG_HITPRCNT			(1 << 11)		/* fighting and below some hp */
#define MTRIG_BRIBE				(1 << 12)		/* coins are given to mob     */
#define MTRIG_LEAVE				(1 << 13)
#define MTRIG_LEAVE_ALL			(1 << 14)
#define MTRIG_DOOR				(1 << 15)

/* obj trigger types */
#define OTRIG_GLOBAL			(1 << 0)		// unused
#define OTRIG_RANDOM			(1 << 1)		//	Checked randomly
#define OTRIG_COMMAND			(1 << 2)		//	Character types a command
#define OTRIG_SIT				(1 << 3)		//	Character tries to sit on the object
#define OTRIG_LEAVE				(1 << 4)		//	Character leaves room
#define OTRIG_GREET				(1 << 5)		//	Character enters room
#define OTRIG_GET				(1 << 6)		//	Character tries to get obj
#define OTRIG_DROP				(1 << 7)		//	Character tries to drop obj
#define OTRIG_GIVE				(1 << 8)		//	Character tries to give obj
#define OTRIG_WEAR				(1 << 9)		//	character tries to wear obj
#define OTRIG_DOOR				(1 << 10)
#define OTRIG_SPEECH			(1 << 11)		//	Speech check
#define OTRIG_CONSUME			(1 << 12)		//	Eat/Drink obj


/* wld trigger types */
#define WTRIG_GLOBAL			(1 << 0)		// check even if zone empty
#define WTRIG_RANDOM			(1 << 1)		// checked randomly
#define WTRIG_COMMAND			(1 << 2)		// character types a command
#define WTRIG_SPEECH			(1 << 3)		// a char says word/phrase
#define WTRIG_DOOR				(1 << 4)
#define WTRIG_LEAVE				(1 << 5)		// character leaves room
#define WTRIG_ENTER				(1 << 6)		// character enters room
#define WTRIG_DROP				(1 << 7)		// something dropped in room


#define TRIG_DELETED			(1 << 31)

/* obj command trigger types */
#define OCMD_EQUIP				(1 << 0)		/* obj must be in char's equip */
#define OCMD_INVEN				(1 << 1)		/* obj must be in char's inven */
#define OCMD_ROOM				(1 << 2)		/* obj must be in char's room  */

#define TRIG_NEW				0				/* trigger starts from top  */
#define TRIG_RESTART			1				/* trigger restarting       */

#define MAX_SCRIPT_DEPTH	10	/* maximum depth triggers can recurse into each other */


/* one line of the trigger */
class CmdlistElement {
public:
	CmdlistElement(void);
	CmdlistElement(char *string);
	~CmdlistElement(void);
	
	char *	cmd;				/* one line of a trigger */
	CmdlistElement *original;
	CmdlistElement *next;
protected:
	void	Init(void);
};


class TrigVarData {
public:
	TrigVarData(void);
	~TrigVarData(void);

	char *name;				/* name of variable  */
	char *value;				/* value of variable */
};


/* structure for triggers */
class TrigData {
public:
	TrigData(void);
	TrigData(const TrigData *trig);
	~TrigData(void);
	
	void	extract(void);
	
	RNum				nr;					//	trigger's rnum
	bool				purged;				//	trigger is set to be purged
	
	SInt8				data_type;			//	type of game_data for trig
	
	SString *			name;				//	name of trigger
	Flags				trigger_type;		//	type of trigger (for bitvector)
	CmdlistElement *	cmdlist;			//	top of command list
	CmdlistElement *	curr_state;			//	ptr to current line of trigger
	SInt32				narg;				//	numerical argument
	SString *			arglist;			//	argument list
	UInt32				depth;				//	depth into nest ifs/whiles/etc
	UInt32				loops;
	Event *				wait_event;			//	event to pause the trigger
	LList<TrigVarData *>var_list;			//	list of local vars for trigger
	
protected:
	void	Init(void);
};


/* a complete script (composed of several triggers) */
class ScriptData {
public:
						ScriptData(void);
						~ScriptData(void);
	
	void				extract(void);
	
	bool				purged;				//	script is set to be purged
	
	Flags				types;				//	bitvector of trigger types
	LList<TrigData *>	trig_list;			//	list of triggers
	LList<TrigVarData *>global_vars;		//	list of global variables
};


extern LList<TrigData *>	trig_list;
extern LList<TrigData *>	purged_trigs;
extern LList<ScriptData *>	purged_scripts;
extern IndexData *	trig_index;
extern UInt32		top_of_trigt;


RNum real_trigger(VNum vnum);
TrigData *read_trigger(VNum nr, UInt8 type);
#define REAL	0
#define VIRTUAL	1
void parse_trigger(FILE *trig_f, RNum nr, char *filename);


/* function prototypes from triggers.c */
   /* mob triggers */
void random_mtrigger(CharData *ch);

   /* object triggers */
void random_otrigger(ObjData *obj);

   /* world triggers */
void random_wtrigger(RoomData *ch);


/* function prototypes from scripts.c */
void script_trigger_check(void);
void add_trigger(ScriptData *sc, TrigData *t); //, int loc);

void do_stat_trigger(CharData *ch, TrigData *trig);
void do_sstat_room(CharData * ch);
void do_sstat_object(CharData * ch, ObjData * j);
void do_sstat_character(CharData * ch, CharData * k);

void script_log(char *msg);


void fprint_triglist(FILE *stream, struct int_list *list);


/* Macros for scripts */

#define GET_TRIG_NAME(t)		((t)->name)
#define GET_TRIG_RNUM(t)		((t)->nr)
#define GET_TRIG_VNUM(t)		((GET_TRIG_RNUM(t) >= 0) ? trig_index[GET_TRIG_RNUM(t)].vnum : -1)
#define GET_TRIG_TYPE(t)		((t)->trigger_type)
#define GET_TRIG_DATA_TYPE(t)	((t)->data_type)
#define GET_TRIG_NARG(t)		((t)->narg)
#define GET_TRIG_ARG(t)			((t)->arglist)
#define GET_TRIG_VARS(t)		((t)->var_list)
#define GET_TRIG_WAIT(t)		((t)->wait_event)
#define GET_TRIG_DEPTH(t)		((t)->depth)
#define GET_TRIG_LOOPS(t)		((t)->loops)


#define SCRIPT(o)				((o)->script)

#define SCRIPT_TYPES(s)			((s)->types)
#define TRIGGERS(s)				((s)->trig_list)


#define SCRIPT_CHECK(go, type)	(SCRIPT(go) && IS_SET(SCRIPT_TYPES(SCRIPT(go)), type))
#define TRIGGER_CHECK(t, type)	(IS_SET(GET_TRIG_TYPE(t), type) && !GET_TRIG_DEPTH(t))

#define ADD_UID_VAR(trig, go, name) { \
		char *buf = get_buffer(MAX_INPUT_LENGTH); \
		sprintf(buf, "%c%d", UID_CHAR, GET_ID(go)); \
		add_var(GET_TRIG_VARS(trig), name, buf); \
		release_buffer(buf); \
		}

#endif	// __SCRIPTS_H__
