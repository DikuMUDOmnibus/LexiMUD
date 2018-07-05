/* ************************************************************************
*   File: interpreter.h                                 Part of CircleMUD *
*  Usage: header file: public procs, macro defs, subcommand defines       *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#ifndef __INTERPRETER_H__
#define __INTERPRETER_H__

#include "types.h"

#define ACMD(name)	void name(CharData *ch, char *argument, SInt32 cmd, char *command, SInt32 subcmd)

#define STAFF_CMD		-1		//	command is usable only by staff (staffcmd1)
#define MOB_CMD			-3		//	command is usable only by mobs

#define CMD_NAME			(complete_cmd_info[cmd].command)
#define CMD_IS(cmd_name)	(!strn_cmp(cmd, cmd_name, MIN(4, strlen(cmd_name))))
#define IS_MOVE(cmdnum)		(cmd_info[cmdnum].command_pointer == do_move)

#define	IS_STAFFCMD(cmd)	(complete_cmd_info[cmd].minimum_level == STAFF_CMD)
#define IS_NPCCMD(cmd)		(complete_cmd_info[cmd].minimum_level == MOB_CMD)

void	command_interpreter(CharData *ch, char *argument);
int		search_block(const char *arg, char **list, bool exact);
int		search_chars(const char arg, const char *list);
char *	one_argument(const char *argument, char *first_arg);
char *	one_word(const char *argument, char *first_arg);
char *	any_one_arg(const char *argument, char *first_arg);
char *	any_one_name(const char *argument, char *first_arg);
char *	two_arguments(const char *argument, char *first_arg, char *second_arg);
char *	three_arguments(const char *argument, char *first_arg, char *second_arg, char *third_arg);
bool	fill_word(const char *argument);
void	half_chop(const char *string, char *arg1, char *arg2);
void	nanny(DescriptorData *d, char *arg);
bool	is_abbrev(const char *arg1, const char *arg2);
bool	is_number(const char *str);
int		find_command(const char *command);
void	skip_spaces(const char *&string);
char *	delete_doubledollar(char *string);


struct command_info {
	char *	command;
	char *	sort_as;
	SInt32	minimum_position;
	void	(*command_pointer) (CharData *ch, char * argument, SInt32 cmd, char *command, SInt32 subcmd);
	SInt32	minimum_level;
	Flags	staffcmd;
	SInt32	subcmd;
};

/* necessary for CMD_IS macro */
extern struct command_info *complete_cmd_info;

/*
 * SUBCOMMANDS
 *   You can define these however you want to, and the definitions of the
 *   subcommands are independent from function to function.
 */

/* directions */
#define SCMD_NORTH	1
#define SCMD_EAST	2
#define SCMD_SOUTH	3
#define SCMD_WEST	4
#define SCMD_UP		5
#define SCMD_DOWN	6

/* do_gen_ps */
#define SCMD_INFO       0
#define SCMD_HANDBOOK   1 
#define SCMD_CREDITS    2
#define SCMD_NEWS       3
#define SCMD_WIZLIST    4
#define SCMD_POLICIES   5
#define SCMD_VERSION    6
#define SCMD_IMMLIST    7
#define SCMD_MOTD	8
#define SCMD_IMOTD	9
#define SCMD_CLEAR	10
#define SCMD_WHOAMI	11

/* do_gen_tog */
#define SCMD_NOSUMMON   0
#define SCMD_NOHASSLE   1
#define SCMD_BRIEF      2
#define SCMD_COMPACT    3
#define SCMD_NOTELL		4
#define SCMD_NOMUSIC	5
#define SCMD_DEAF		6
#define SCMD_NOGOSSIP	7
#define SCMD_NOGRATZ	8
#define SCMD_NOWIZ		9
#define SCMD_QUEST		10
#define SCMD_ROOMFLAGS	11
#define SCMD_NOREPEAT	12
#define SCMD_HOLYLIGHT	13
#define SCMD_SLOWNS		14
#define SCMD_AUTOEXIT	15
#define SCMD_NOINFO		16
#define SCMD_COLOR		17
#define SCMD_NORACE		18

/* do_wizutil */
#define SCMD_REROLL	0
#define SCMD_PARDON     1
#define SCMD_NOTITLE    2
#define SCMD_SQUELCH    3
#define SCMD_FREEZE	4
#define SCMD_THAW	5
#define SCMD_UNAFFECT	6

/* do_spec_com */
#define SCMD_WHISPER	0
#define SCMD_ASK	1

/* do_gen_com */
#define SCMD_SHOUT		0
#define SCMD_GOSSIP		1
#define SCMD_MUSIC		2
#define SCMD_GRATZ		3
#define SCMD_BROADCAST	4
#define SCMD_CLAN		5
#define SCMD_RACE		6
#define SCMD_MISSION	7

/* do_date */
#define SCMD_DATE	0
#define SCMD_UPTIME	1

/* do_commands */
#define SCMD_COMMANDS	0
#define SCMD_SOCIALS	1
#define SCMD_WIZHELP	2

/* do_drop */
#define SCMD_DROP	0
#define SCMD_JUNK	1
#define SCMD_DONATE	2

/* do_gen_write */
#define SCMD_BUG	0
#define SCMD_TYPO	1
#define SCMD_IDEA	2

/* do_look */
#define SCMD_LOOK	0
#define SCMD_READ	1

/* do_qcomm */
#define SCMD_QSAY	0
#define SCMD_QECHO	1

/* do_pour */
#define SCMD_POUR	0
#define SCMD_FILL	1

/* do_poof */
#define SCMD_POOFIN	0
#define SCMD_POOFOUT	1

/* do_hit */
#define SCMD_HIT	0
#define SCMD_MURDER	1

/* do_eat */
#define SCMD_EAT	0
#define SCMD_TASTE	1
#define SCMD_DRINK	2
#define SCMD_SIP	3

/* do_use */
#define SCMD_USE	0

/* do_echo */
#define SCMD_ECHO	0
#define SCMD_EMOTE	1

/* do_gen_door */
#define SCMD_OPEN       0
#define SCMD_CLOSE      1
#define SCMD_UNLOCK     2
#define SCMD_LOCK       3
#define SCMD_PICK       4

/*. do_olc .*/
#define SCMD_OLC_REDIT		0
#define SCMD_OLC_OEDIT		1
#define SCMD_OLC_ZEDIT		2
#define SCMD_OLC_MEDIT		3
#define SCMD_OLC_SEDIT		4
#define SCMD_OLC_AEDIT		5
#define SCMD_OLC_HEDIT		6
#define SCMD_OLC_SCRIPTEDIT	7
#define SCMD_OLC_CEDIT		8
#define SCMD_OLC_SAVEINFO  20

/* do_reload */
#define SCMD_LOAD		0
#define SCMD_UNLOAD		1

/* do_reload */
#define SCMD_PUSH		0
#define SCMD_DRAG		1

/* do_delete */
#define SCMD_DELETE		0
#define SCMD_UNDELETE	1

/* do_liblist */
#define SCMD_OLIST 	0
#define SCMD_MLIST 	1
#define SCMD_RLIST 	2
#define SCMD_TLIST	3

/* do_stat */
#define SCMD_STAT	0
#define SCMD_SSTAT	1

/* do_move_element */
#define SCMD_RMOVE	0
#define SCMD_OMOVE	1
#define SCMD_MMOVE	2
#define SCMD_TMOVE	3

/* do_msend */
#define SCMD_MSEND			0
#define SCMD_MECHOAROUND	1

/* do_kick_bite */
#define SCMD_KICK	SKILL_KICK
#define SCMD_BITE	SKILL_BITE

/* do_allow */
#define SCMD_ALLOW	0
#define SCMD_DENY	1

#endif