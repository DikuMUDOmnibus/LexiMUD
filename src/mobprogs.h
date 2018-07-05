/*************************************************************************
*   File: mobprogs.h                 Part of Aliens vs Predator: The MUD *
*  Usage: Header file for mobprogs.  I want to get rid of this...        *
*************************************************************************/

#ifndef __MOBPROGS_H__
#define __MOBPROGS_H__

#define ERROR_PROG        -1		// 1 << 31
#define IN_FILE_PROG       0		// 1 << 0
#define ACT_PROG		(1 << 0)
#define SPEECH_PROG		(1 << 1)
#define RAND_PROG		(1 << 2)
#define FIGHT_PROG		(1 << 3)
#define DEATH_PROG		(1 << 4)
#define HITPRCNT_PROG	(1 << 5)
#define ENTRY_PROG		(1 << 6)
#define GREET_PROG		(1 << 7)
#define ALL_GREET_PROG	(1 << 8)
#define GIVE_PROG		(1 << 9)
#define BRIBE_PROG		(1 << 10)
#define EXIT_PROG		(1 << 11)
#define ALL_EXIT_PROG	(1 << 12)
#define DELAY_PROG		(1 << 13)
#define SCRIPT_PROG		(1 << 14)
#define WEAR_PROG		(1 << 15)
#define REMOVE_PROG		(1 << 16)
#define GET_PROG		(1 << 17)
#define DROP_PROG		(1 << 18)
#define EXAMINE_PROG	(1 << 19)
#define PULL_PROG		(1 << 20)
/* end of MOBProg foo */
#define KILL_PROG		(1 << 31)	// Not implemented yet...
#define SURRENDER_PROG	(1 << 31)	// Not implemented yet...


/* MOBProgram foo */
struct mob_prog_act_list {
	struct mob_prog_act_list *next;
	char *buf;
	CharData *ch;
	ObjData *obj;
	Ptr vo;
};
typedef struct mob_prog_act_list MPROG_ACT_LIST;

struct mob_prog_data {
	struct mob_prog_data *next;
	int type;
	char *arglist;
	char *comlist;
};

typedef struct mob_prog_data MPROG_DATA;

extern int MOBTrigger;

#endif	//	__MOBPROGS_H__