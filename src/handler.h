/* ************************************************************************
*   File: handler.h                                     Part of CircleMUD *
*  Usage: header file: prototypes of handling and utility functions       *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */


/* utility */
int			isname(const char *str, const char *namelist);
char *		fname(const char *namelist);
int			get_number(char **name);
int			silly_isname(const char *str, const char *namelist);
int			split_string(char *str, char *sep, char **argv);

/* ******* characters ********* */

int is_same_group(CharData *ach, CharData *bch );


/* prototypes from crash save system */

void	Crash_listrent(CharData *ch, char *name);
int	Crash_load(CharData *ch);
void	Crash_crashsave(CharData *ch);
void	Crash_save_all(void);

/* prototypes from fight.c */
void	set_fighting(CharData *ch, CharData *victim);
int		hit(CharData *ch, CharData *victim, int type, int times);
void	forget(CharData *ch, CharData *victim);
void	remember(CharData *ch, CharData *victim);
int damage(CharData * ch, CharData * victim, int dam, int attacktype, int times);
int	skill_message(int dam, CharData *ch, CharData *vict,
		      int attacktype);
