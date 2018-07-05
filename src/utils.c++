/* ************************************************************************
*   File: utils.c                                       Part of CircleMUD *
*  Usage: various internal functions of a utility nature                  *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */


#include "structs.h"
#include "utils.h"
#include "buffer.h"
#include "comm.h"
#include "db.h"

int sunlight;

void tag_argument(char *argument, char *tag);

/* local functions */
struct TimeInfoData real_time_passed(time_t t2, time_t t1);
struct TimeInfoData mud_time_passed(time_t t2, time_t t1);
void die_follower(CharData * ch);
void add_follower(CharData * ch, CharData * leader);
int wear_message_number (ObjData *obj, int where);


int str_prefix(const char *astr, const char *bstr);


/* creates a random number in interval [from;to] */
int Number(int from, int to)
{
  /* error checking in case people call Number() incorrectly */
  if (from > to) {
    int tmp = from;
    from = to;
    to = tmp;
//    log("SYSERR: Number() should be called with lowest, then highest. Number(%d, %d), not Number(%d, %d).", from, to, to, from);
  }

  return ((circle_random() % (to - from + 1)) + from);
}


/* simulates dice roll */
int dice(int number, int size) {
	int sum = 0;

	if (size <= 0 || number <= 0)
		return 0;

	while (number-- > 0)
		sum += ((circle_random() % size) + 1);

	return sum;
}


int MIN(int a, int b) {
	return a < b ? a : b;
}


int MAX(int a, int b) {
	return a > b ? a : b;
}



/* Create a duplicate of a string */
char *str_dup(const char *source) {
	char *new_s;
	
	if (!source)
		return NULL;
	
	CREATE(new_s, char, strlen(source) + 1);
	return (strcpy(new_s, source));
}



/* str_cmp: a case-insensitive version of strcmp */
/* returns: 0 if equal, 1 if arg1 > arg2, -1 if arg1 < arg2  */
/* scan 'till found different or end of both                 */
int str_cmp(const char *arg1, const char *arg2) {
	int chk, i;

	for (i = 0; *(arg1 + i) || *(arg2 + i); i++)
		if ((chk = LOWER(*(arg1 + i)) - LOWER(*(arg2 + i)))) {
			if (chk < 0)	return -1;
			else			return 1;
		}
//	SInt32	chk;
//	while (*arg1 || *arg2)
//		if ((chk = LOWER(*(arg1++)) - LOWER(*(arg2++))))
//			if (chk < 0)	return -1;
//			else			return 1;
	return 0;
}


/* strn_cmp: a case-insensitive version of strncmp */
/* returns: 0 if equal, 1 if arg1 > arg2, -1 if arg1 < arg2  */
/* scan 'till found different, end of both, or n reached     */
int strn_cmp(const char *arg1, const char *arg2, int n) {
	int chk, i;
	for (i = 0; (*(arg1 + i) || *(arg2 + i)) && (n > 0); i++, n--)
		if ((chk = LOWER(*(arg1 + i)) - LOWER(*(arg2 + i)))) {
			if (chk < 0)	return -1;
			else			return 1;
		}
//	SInt32	chk;
//	while (*arg1 || *arg2)
//		if ((chk = LOWER(*(arg1++)) - LOWER(*(arg2++))))
//			if (chk < 0)	return -1;
//			else			return 1;
//		else if (n-- <= 0)
//			break;
	return 0;
}


/* Return pointer to first occurrence in string ct in */
/* cs, or NULL if not present.  Case insensitive */
char *str_str(char *cs, char *ct) {
	char *s, *t;

	if (!cs || !ct)
		return NULL;

	while (*cs) {
		t = ct;

		while (*cs && (LOWER(*cs) != LOWER(*t)))
			cs++;

		s = cs;

		while (*t && *cs && (LOWER(*cs) == LOWER(*t))) {
			t++;
			cs++;
		}
    
		if (!*t)
			return s;
	}

	return NULL;
}


/* log a death trap hit */
void log_death_trap(CharData * ch)
{
  mudlogf(BRF, LVL_IMMORT, TRUE, "%s hit death trap #%d (%s)", ch->RealName(),
	  world[IN_ROOM(ch)].number, world[IN_ROOM(ch)].GetName("<ERROR>"));
}


/* the "touch" command, essentially. */
int touch(char *path)
{
  FILE *fl;

  if (!(fl = fopen(path, "a"))) {
    perror(path);
    return -1;
  } else {
    fclose(fl);
    return 0;
  }
}


void sprintbit(UInt32 bitvector, char *names[], char *result)
{
  long nr;
  
  *result = '\0';

  for (nr = 0; bitvector; bitvector >>= 1) {
    if (IS_SET(bitvector, 1)) {
      if (*names[nr] != '\n') {
	strcat(result, names[nr]);
	strcat(result, " ");
      } else
	strcat(result, "UNDEFINED ");
    }
    if (*names[nr] != '\n')
      nr++;
  }

  if (!*result)
    strcpy(result, "NOBITS ");
}



void sprinttype(int type, char *names[], char *result)
{
  int nr = 0;

  while (type && *names[nr] != '\n') {
    type--;
    nr++;
  }

  if (*names[nr] != '\n')
    strcpy(result, names[nr]);
  else
    strcpy(result, "UNDEFINED");
}


/* Calculate the REAL time passed over the last t2-t1 centuries (secs) */
struct TimeInfoData real_time_passed(time_t t2, time_t t1)
{
  long secs;
  struct TimeInfoData now;

  secs = (long) (t2 - t1);

  now.hours = (secs / SECS_PER_REAL_HOUR) % 24;	/* 0..23 hours */
  secs -= SECS_PER_REAL_HOUR * now.hours;

  now.day = (secs / SECS_PER_REAL_DAY);	/* 0..29 days  */
  secs -= SECS_PER_REAL_DAY * now.day;

  now.month = -1;
  now.year = -1;

  return now;
}



/* Calculate the MUD time passed over the last t2-t1 centuries (secs) */
struct TimeInfoData mud_time_passed(time_t t2, time_t t1)
{
  long secs;
  struct TimeInfoData now;

  secs = (long) (t2 - t1);

  now.hours = (secs / SECS_PER_MUD_HOUR) % 24;	/* 0..23 hours */
  secs -= SECS_PER_MUD_HOUR * now.hours;

  now.day = (secs / SECS_PER_MUD_DAY) % 30;	/* 0..29 days  */
  secs -= SECS_PER_MUD_DAY * now.day;

  now.month = (secs / SECS_PER_MUD_MONTH) % 12;	/* 0..11 months */
  secs -= SECS_PER_MUD_MONTH * now.month;

  now.year = (secs / SECS_PER_MUD_YEAR);	/* 0..XX? years */

  return now;
}



struct TimeInfoData age(CharData * ch)
{
  struct TimeInfoData player_age;

  player_age = mud_time_passed(time(0), ch->player->time.birth);

  player_age.year += 17;	/* All players start at 17 */

  return player_age;
}


/* Check if making CH follow VICTIM will create an illegal */
/* Follow "Loop/circle"                                    */
int circle_follow(CharData * ch, CharData * victim)
{
  CharData *k;

  for (k = victim; k; k = k->master) {
    if (k == ch)
      return TRUE;
  }

  return FALSE;
}



/* Do NOT call this before having checked if a circle of followers */
/* will arise. CH will follow leader                               */
void add_follower(CharData * ch, CharData * leader) {
	if (ch->master) {
		core_dump();
		return;
	}

	ch->master = leader;

	leader->followers.Add(ch);
	
	act("You now follow $N.", FALSE, ch, 0, leader, TO_CHAR);
	if (leader->CanSee(ch))
		act("$n starts following you.", TRUE, ch, 0, leader, TO_VICT);
	act("$n starts to follow $N.", TRUE, ch, 0, leader, TO_NOTVICT);
}

/*
 * get_line reads the next non-blank line off of the input stream.
 * The newline character is removed from the input.  Lines which begin
 * with '*' are considered to be comments.
 *
 * Returns the number of lines advanced in the file.
 */
int get_line(FILE * fl, char *buf) {
	char *temp = get_buffer(256);
	int lines = 0;

	do {
		lines++;
		fgets(temp, 256, fl);
		if (*temp)
			temp[strlen(temp) - 1] = '\0';
	} while (!feof(fl) && (!*temp));

	if (feof(fl)) {
		*buf = '\0';
		lines = 0;
	} else
		strcpy(buf, temp);
	
	release_buffer(temp);
	return lines;
}


int get_filename(const char *orig_name, char *filename) {
	char	*ptr, *name;

	if (!*orig_name)
		return 0;
    
	name = get_buffer(64);
	strcpy(name, orig_name);
	for (ptr = name; *ptr; ptr++)
		*ptr = LOWER(*ptr);

	sprintf(filename, PLR_PREFIX "%c" SEPERATOR "%s.objs", *name, name);
  
	release_buffer(name);
	return 1;
}


int num_pc_in_room(RoomData *room) {
	int i = 0;
	CharData *ch;

	START_ITER(iter, ch, CharData *, room->people) {
		if (!IS_NPC(ch))
			i++;
	} END_ITER(iter);

	return i;
}


/* string manipulation fucntion originally by Darren Wilson */
/* (wilson@shark.cc.cc.ca.us) improved and bug fixed by Chris (zero@cnw.com) */
/* completely re-written again by M. Scott 10/15/96 (scottm@workcommn.net), */
/* substitute appearances of 'pattern' with 'replacement' in string */
/* and return the # of replacements */
int replace_str(char **string, char *pattern, char *replacement, int rep_all,
		int max_size) {
   char *replace_buffer = NULL;
   char *flow, *jetsam, temp;
   int len, i;
   
   if ((strlen(*string) - strlen(pattern)) + strlen(replacement) > max_size)
     return -1;
   
   CREATE(replace_buffer, char, max_size);
   i = 0;
   jetsam = *string;
   flow = *string;
   *replace_buffer = '\0';
   if (rep_all) {
      while ((flow = strstr(flow, pattern)) != NULL) {
	 i++;
	 temp = *flow;
	 *flow = '\0';
	 if ((strlen(replace_buffer) + strlen(jetsam) + strlen(replacement)) > max_size) {
	    i = -1;
	    break;
	 }
	 strcat(replace_buffer, jetsam);
	 strcat(replace_buffer, replacement);
	 *flow = temp;
	 flow += strlen(pattern);
	 jetsam = flow;
      }
      strcat(replace_buffer, jetsam);
   }
   else {
      if ((flow = strstr(*string, pattern)) != NULL) {
	 i++;
	 flow += strlen(pattern);  
	 len = (flow - *string) - strlen(pattern);
   
	 strncpy(replace_buffer, *string, len);
	 replace_buffer[len] = '\0';
	 strcat(replace_buffer, replacement);
	 strcat(replace_buffer, flow);
      }
   }
   if (i == 0) return 0;
   if (i > 0) {
      RECREATE(*string, char, strlen(replace_buffer) + 3);
      strcpy(*string, replace_buffer);
   }
   FREE(replace_buffer);
   return i;
}


/* re-formats message type formatted char * */
/* (for strings edited with d->str) (mostly olc and mail)     */
void format_text(char **ptr_string, int mode, DescriptorData *d, int maxlen) {
   int total_chars, cap_next = TRUE, cap_next_next = FALSE;
   char *flow, *start = NULL, temp;
   /* warning: do not edit messages with max_str's of over this value */
   char *formated;
   
   flow   = *ptr_string;
   if (!flow) return;

  formated = get_buffer(MAX_STRING_LENGTH);
   if (IS_SET(mode, FORMAT_INDENT)) {
      strcpy(formated, "   ");
      total_chars = 3;
   }
   else {
      *formated = '\0';
      total_chars = 0;
   } 

   while (*flow != '\0') {
      while ((*flow == '\n') ||
	     (*flow == '\r') ||
	     (*flow == '\f') ||
	     (*flow == '\t') ||
	     (*flow == '\v') ||
	     (*flow == ' ')) flow++;

      if (*flow != '\0') {

	 start = flow++;
	 while ((*flow != '\0') &&
		(*flow != '\n') &&
		(*flow != '\r') &&
		(*flow != '\f') &&
		(*flow != '\t') &&
		(*flow != '\v') &&
		(*flow != ' ') &&
		(*flow != '.') &&
		(*flow != '?') &&
		(*flow != '!')) flow++;

	 if (cap_next_next) {
	    cap_next_next = FALSE;
	    cap_next = TRUE;
	 }

	 /* this is so that if we stopped on a sentance .. we move off the sentance delim. */
	 while ((*flow == '.') || (*flow == '!') || (*flow == '?')) {
	    cap_next_next = TRUE;
	    flow++;
	 }
	 
	 temp = *flow;
	 *flow = '\0';

	 if ((total_chars + strlen(start) + 1) > 79) {
	    strcat(formated, "\r\n");
	    total_chars = 0;
	 }

	 if (!cap_next) {
	    if (total_chars > 0) {
	       strcat(formated, " ");
	       total_chars++;
	    }
	 }
	 else {
	    cap_next = FALSE;
	    *start = UPPER(*start);
	 }

	 total_chars += strlen(start);
	 strcat(formated, start);

	 *flow = temp;
      }

      if (cap_next_next) {
	 if ((total_chars + 3) > 79) {
	    strcat(formated, "\r\n");
	    total_chars = 0;
	 }
	 else {
	    strcat(formated, "  ");
	    total_chars += 2;
	 }
      }
   }
   strcat(formated, "\r\n");

   if (strlen(formated) > maxlen) formated[maxlen] = '\0';
   RECREATE(*ptr_string, char, MIN(maxlen, strlen(formated)+3));
   strcpy(*ptr_string, formated);
   release_buffer(formated);
}


void Free_Error(char * file, int line) {
	mudlogf( BRF, LVL_SRSTAFF, TRUE,  "CODEERR: NULL pointer in file %s:%d", file, line);
}


/*thanks to Luis Carvalho for sending this my way..it's probably a
  lot shorter than the one I would have made :)  */
void sprintbits(UInt32 vektor,char *outstring) {
	int i;
	const char flags[53]="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

	*outstring = '\0';
	for (i=0;i<32;i++) {
		if (vektor & 1) {
			*outstring=flags[i];
			outstring++;
		}
		vektor>>=1;
	}
	*outstring=0;
}


void tag_argument(char *argument, char *tag)
{
  char *tmp = argument, *ttag = tag, *wrt = argument;
  int i;

  for(i = 0; i < 4; i++)
    *(ttag++) = *(tmp++);
  *ttag = '\0';
  
  while(*tmp == ':' || *tmp == ' ')
    tmp++;

  while(*tmp)
    *(wrt++) = *(tmp++);
  *wrt = '\0';
}


/* returns the message number to display for where[] and equipment_types[] */
int wear_message_number (ObjData *obj, int where) {
	if (!obj) {
		log("SYSERR:  NULL passed for obj to wear_message_number");
		return where;
	}
	if (where < WEAR_HAND_R)						return where;
	else if (OBJ_FLAGGED(obj, ITEM_TWO_HAND)) {
		if (GET_OBJ_TYPE(obj) == ITEM_WEAPON)		return POS_WIELD_TWO;
		else										return POS_HOLD_TWO;
	} else if (GET_OBJ_TYPE(obj) == ITEM_WEAPON) {
		if (where == WEAR_HAND_R)					return POS_WIELD;
		else										return POS_WIELD_OFF;
	} else if (GET_OBJ_TYPE(obj) == ITEM_LIGHT)		return POS_LIGHT;
	else											return POS_HOLD;
}


RNum real_number(VNum vnum, IndexData *the_index, int the_top);
RNum real_number(VNum vnum, IndexData *the_index, int the_top) {
	int bot, top, mid, nr, found = -1;

	/* First binary search. */
	bot = 0;
	top = the_top;

	for (;;) {
		mid = (bot + top) >> 1;

		if (mid == -1) {
			mudlogf(BRF, LVL_STAFF, TRUE, "SYSERR: real_number mid == -1!  (vnum == %d, top == %d)", vnum, the_top);
			break;
		}
		if ((the_index + mid)->vnum == vnum)
			return mid;
		if (bot >= top)
			break;
		if ((the_index + mid)->vnum > vnum)
			top = mid - 1;
		else
			bot = mid + 1;
	}

	/* If not found - use linear on the "new" parts. */
	for(nr = 0; nr <= the_top; nr++) {
		if(the_index[nr].vnum == vnum) {
			found = nr;
			break;
		}
	}
	return(found);
}


/* string prefix routine */
int str_prefix(const char *astr, const char *bstr) {
	if (!astr) {
		log("SYSERR: str_prefix: null astr.");
		return TRUE;
	}
	if (!bstr) {
		log("SYSERR: str_prefix: null bstr.");
		return TRUE;
	}
	for(; *astr; astr++, bstr++) {
		if(LOWER(*astr) != LOWER(*bstr)) return TRUE;
	}
	return FALSE;
}


#if defined(CIRCLE_UNIX)
/*
 * This function (derived from basic fork(); abort(); idea by Erwin S.
 * Andreasen) causes your MUD to dump core (assuming you can) but
 * continue running.  The core dump will allow post-mortem debugging
 * that is less severe than assert();  Don't call this directly as
 * core_dump_unix() but as simply 'core_dump()' so that it will be
 * excluded from systems not supporting them. (e.g. Windows '95).
 *
 * XXX: Wonder if flushing streams includes sockets?
 */
void core_dump_func(const char *who, SInt16 line) {
	log("SYSERR: *** Dumping core from %s:%d. ***", who, line);

	//	These would be duplicated otherwise...
	fflush(stdout);
	fflush(stderr);
	fflush(logfile);

	//	 Kill the child so the debugger or script doesn't think the MUD
	//	crashed.  The 'autorun' script would otherwise run it again.
	if (!fork())
		abort();
}
#else

//	Any other OS should just note the failure and (attempt to) cope.
void core_dump_func(const char *who, SInt16 line) {
	log("SYSERR: *** Assertion failed at %s:%d. ***", who, line);
	//	Some people may wish to die here.  Try 'assert(FALSE);' if so.
}

#endif
