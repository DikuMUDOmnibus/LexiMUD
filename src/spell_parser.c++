/* ************************************************************************
*   File: spell_parser.c                                Part of CircleMUD *
*  Usage: top-level magic routines; outside points of entry to magic sys. *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */


#include "structs.h"
#include "utils.h"
#include "buffer.h"
#include "interpreter.h"
#include "spells.h"
#include "handler.h"
#include "find.h"
#include "comm.h"
#include "db.h"

struct skill_info_type skill_info[TOP_SPELL_DEFINE + 1];

#define SINFO skill_info[spellnum]

extern RoomData *world;

void skillo(int spl);
void unused_spell(int spl);
void mag_assign_spells(void);

/*
 * This arrangement is pretty stupid, but the number of skills is limited by
 * the playerfile.  We can arbitrarily increase the number of skills by
 * increasing the space in the playerfile. Meanwhile, this should provide
 * ample slots for skills.
 */

char *skills[] =
{
  "!RESERVED!",			/* 0 - reserved */

  /* SKILLS */

  "backstab",			/* 1 */
  "bash",
  "hide",
  "kick",
  "pick lock",			/* 5 */
  "melee",
  "rescue",
  "sneak",
  "circle",
  "track",				/* 10 */
  "throw", 
  "shoot",
  "dodge",
  "watch",
  "bite",				/* 15 */
  "train",
  "cocoon",
  "identify",
  "critical hit",
  "!UNUSED!",			/* 20 */
  "!UNUSED!", "!UNUSED!", "!UNUSED!", "!UNUSED!", "!UNUSED!",
  "!UNUSED!", "!UNUSED!", "!UNUSED!", "!UNUSED!", "!UNUSED!", /* 30 */
  "!UNUSED!", "!UNUSED!", "!UNUSED!", "!UNUSED!", "!UNUSED!",
  "!UNUSED!", "!UNUSED!", "!UNUSED!", "!UNUSED!", "!UNUSED!", /* 40 */
  "!UNUSED!", "!UNUSED!", "!UNUSED!", "!UNUSED!", "!UNUSED!",
  "!UNUSED!", "!UNUSED!", "!UNUSED!", "!UNUSED!", "!UNUSED!", /* 50 */
  "\n"				/* the end */
};


char *skill_name(int num)
{
  int i = 0;

  if (num <= 0) {
    if (num == -1)
      return "UNUSED";
    else
      return "UNDEFINED";
  }

  while (num && *skills[i] != '\n') {
    num--;
    i++;
  }

  if (*skills[i] != '\n')
    return skills[i];
  else
    return "UNDEFINED";
}

	 
int find_skill_num(char *name)
{
  int index = 0, ok;
  char *temp, *temp2;
  char *first, *first2;
  
  first = get_buffer(256);
  first2 = get_buffer(256);
  
  while (*skills[++index] != '\n') {
    if (is_abbrev(name, skills[index])) {
      release_buffer(first);
      release_buffer(first2);
      return index;
    }

    ok = 1;
    temp = any_one_arg(skills[index], first);
    temp2 = any_one_arg(name, first2);
    while (*first && *first2 && ok) {
      if (!is_abbrev(first2, first))
	ok = 0;
      temp = any_one_arg(temp, first);
      temp2 = any_one_arg(temp2, first2);
    }

    if (ok && !*first2) {
      release_buffer(first);
      release_buffer(first2);
      return index;
    }
  }

  release_buffer(first);
  release_buffer(first2);
  
  return -1;
}

void spell_level(int spell, int race, int level) {
	int bad = 0;

	if (spell < 0 || spell > TOP_SPELL_DEFINE)
		log("SYSERR: attempting assign to illegal spellnum %d", spell);
	else if (race < 0 || race >= NUM_RACES)
		log("SYSERR: assigning '%s' to illegal race %d", skill_name(spell), race);
	else if (level < 1 || level > LVL_CODER)
		log("SYSERR: assigning '%s' to illegal level %d", skill_name(spell), level);
	else
		skill_info[spell].min_level[race] = level;
}


/* Assign the spells on boot up */
void skillo(int spl)
{
  int i;

  for (i = 0; i < NUM_RACES; i++)
    skill_info[spl].min_level[i] = LVL_IMMORT;
}


void unused_spell(int spl)
{
  int i;

  for (i = 0; i < NUM_RACES; i++)
    skill_info[spl].min_level[i] = LVL_CODER + 1;
}



void mag_assign_spells(void)
{
  int i;

  /* Do not change the loop below */
  for (i = 1; i <= TOP_SPELL_DEFINE; i++)
    unused_spell(i);
  /* Do not change the loop above */
  /*
   * Declaration of skills - this actually doesn't do anything except
   * set it up so that immortals can use these skills by default.  The
   * min level to use the skill for other classes is set up in class.c.
   */

  skillo(SKILL_BACKSTAB);
  skillo(SKILL_BASH);
  skillo(SKILL_HIDE);
  skillo(SKILL_KICK);
  skillo(SKILL_PICK_LOCK);
  skillo(SKILL_PUNCH);
  skillo(SKILL_RESCUE);
  skillo(SKILL_SNEAK);
  skillo(SKILL_TRACK);
  skillo(SKILL_THROW);
  skillo(SKILL_SHOOT);
  skillo(SKILL_DODGE);
  skillo(SKILL_WATCH);
  skillo(SKILL_CIRCLE);
  skillo(SKILL_BITE);
  skillo(SKILL_TRAIN);
  skillo(SKILL_COCOON);
  skillo(SKILL_IDENTIFY);
  skillo(SKILL_CRITICAL);
}

