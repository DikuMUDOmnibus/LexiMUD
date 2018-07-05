/* ************************************************************************
*   File: handler.c                                     Part of CircleMUD *
*  Usage: internal funcs: moving and finding chars/objs                   *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */




#include "structs.h"
#include "utils.h"
#include "scripts.h"
#include "buffer.h"
#include "comm.h"
#include "db.h"
#include "handler.h"
#include "interpreter.h"
#include "spells.h"
#include "event.h"
#include "find.h"

/* external vars */
//extern char *MENU;
//extern TrigData *purged_trigs;		   /* list of mtrigs to be deleted  */
//extern ScriptData *purged_scripts; /* list of mob scripts to be del */

/* external functions */
ACMD(do_return);
void remove_follower(CharData * ch);
void clearMemory(CharData * ch);

void free_purged_lists(void);

int count_mobs_in_room(int num, int room);

char *fname(const char *namelist) {
	static char holder[256];
	char *point;

	for (point = holder; isalpha(*namelist); namelist++, point++)
		*point = *namelist;

	*point = '\0';

	return (holder);
}

#define WHITESPACE " \t"

int isname(const char *str, const char *namelist) {
	char *newlist;
	char *curtok;

	newlist = str_dup(namelist); /* make a copy since strtok 'modifies' strings */

	for(curtok = strtok(newlist, WHITESPACE); curtok; curtok = strtok(NULL, WHITESPACE))
		if(curtok && is_abbrev(str, curtok)) {
			FREE(newlist);
			return 1;
		}
	FREE(newlist);
	return 0;
}


int silly_isname(const char *str, const char *namelist)
{
  char  *argv[MAX_INPUT_LENGTH], *xargv[MAX_INPUT_LENGTH];
  int   argc, xargc, i, j, exact;
  static char   buf[MAX_INPUT_LENGTH], names[MAX_INPUT_LENGTH], *s;

  strcpy(buf, str);
  argc = split_string(buf, "- \t\n\r,", argv);

  strcpy(names, namelist);
  xargc = split_string(names, "- \t\n\r,", xargv);

  s = argv[argc-1];
  s += strlen(s);
  if (*(--s) == '.') {
    exact = 1;
    *s = 0;
  } else {
    exact = 0;
  }
  /* the string has now been split into separate words with the '-'
     replaced by string terminators.  pointers to the beginning of
     each word are in argv */

  if (exact && argc != xargc)
    return 0;

  for (i = 0; i < argc; i++) {
    for (j = 0; j < xargc; j++) {
      if (xargv[j] && !str_cmp(argv[i], xargv[j])) {
        xargv[j] = NULL;
        break;
      }
    }
    if (j >= xargc)
      return 0;
  }

  return 1;
}

int split_string(char *str, char *sep, char **argv)
     /* str must be writable */
{
  char  *s;
  int   argc=0;

  s = strtok(str, sep);
  if (s)
    argv[argc++] = s;
  else {
    *argv = str;
    return(1);
  }

  while ((s = strtok(NULL, sep)))
    argv[argc++] = s;

  argv[argc] = '\0';

  return(argc);
}



int is_same_group(CharData *ach, CharData *bch ) {
	if (ach == NULL || bch == NULL)
		return FALSE;

	if ( ach->master != NULL ) ach = ach->master;
	if ( bch->master != NULL ) bch = bch->master;
	return ach == bch;
}


/* cleans out the purge lists */
void free_purged_lists() {
	CharData *ch;
	ObjData *obj;
	TrigData *trig;
	ScriptData *sc;
	
	START_ITER(ch_iter, ch, CharData *, purged_chars) {
		purged_chars.Remove(ch);
		delete ch;
	} END_ITER(ch_iter);

	START_ITER(obj_iter, obj, ObjData *, purged_objs) {
		purged_objs.Remove(obj);
		delete obj;
	} END_ITER(obj_iter);

	START_ITER(trig_iter, trig, TrigData *, purged_trigs) {
		purged_trigs.Remove(trig);
		delete trig;
	} END_ITER(trig_iter);

	START_ITER(script_iter, sc, ScriptData *, purged_scripts) {
		purged_scripts.Remove(sc);
		delete sc;
	} END_ITER(script_iter);
}
