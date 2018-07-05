/* ************************************************************************
*   File: spec_assign.c                                 Part of CircleMUD *
*  Usage: Functions to assign function pointers to objs/mobs/rooms        *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */


#include "structs.h"
#include "buffer.h"
#include "db.h"
#include "interpreter.h"
#include "utils.h"

extern bool mini_mud;
extern int dts_are_dumps;

void assign_elevators(void);

/* local functions */
void ASSIGNMOB(int mob, SPECIAL(fname));
void ASSIGNOBJ(int obj, SPECIAL(fname));
void ASSIGNROOM(int room, SPECIAL(fname));
void assign_mobiles(void);
void assign_objects(void);
void assign_rooms(void);


/* functions to perform assignments */

void ASSIGNMOB(int mob, SPECIAL(fname)) {
	int rnum;
	if ((rnum = real_mobile(mob)) >= 0)
		mob_index[rnum].func = fname;
	else if (!mini_mud)
		log("SYSERR: Attempt to assign spec to non-existant mob #%d", mob);
}

void ASSIGNOBJ(int obj, SPECIAL(fname)) {
	int rnum;
  if ((rnum = real_object(obj)) >= 0)
    obj_index[rnum].func = fname;
  else if (!mini_mud)
    log("SYSERR: Attempt to assign spec to non-existant obj #%d", obj);
}

void ASSIGNROOM(int room, SPECIAL(fname))
{
	int rnum;
  if ((rnum = real_room(room)) >= 0)
    world[rnum].func = fname;
  else if (!mini_mud)
    log("SYSERR: Attempt to assign spec to non-existant rm. #%d", room);
}


/* ********************************************************************
*  Assignments                                                        *
******************************************************************** */

/* assign special procedures to mobiles */
void assign_mobiles(void)
{
  SPECIAL(puff);
  SPECIAL(postmaster);
  SPECIAL(temp_guild);
/*  
  SPECIAL(cityguard);
  SPECIAL(receptionist);
  SPECIAL(cryogenicist);
  SPECIAL(guild_guard);
  SPECIAL(guild);
  SPECIAL(fido);
  SPECIAL(janitor);
  SPECIAL(mayor);
  SPECIAL(snake);
  SPECIAL(thief);
  SPECIAL(magic_user); */
/*  void assign_kings_castle(void); */

/*  assign_kings_castle(); */

  ASSIGNMOB(1, puff);
  ASSIGNMOB(1201, postmaster);
//  ASSIGNMOB(8005, temp_guild);
//  ASSIGNMOB(7005, temp_guild);
  
  ASSIGNMOB(8020, temp_guild);		//	Marine trainer
  ASSIGNMOB(688, temp_guild);		//	Yautja Trainer
  ASSIGNMOB(25000, temp_guild);		//	Alien trainer
  ASSIGNMOB(8898, temp_guild);
//  ASSIGNMOB(9002, temp_guild);
  
#if 0
  /* Immortal Zone */
  ASSIGNMOB(1200, receptionist);
  ASSIGNMOB(1201, postmaster);
  ASSIGNMOB(1202, janitor);

  /* Midgaard */
  ASSIGNMOB(3005, receptionist);
  ASSIGNMOB(3010, postmaster);
  ASSIGNMOB(3020, guild);
  ASSIGNMOB(3021, guild);
  ASSIGNMOB(3022, guild);
  ASSIGNMOB(3023, guild);
  ASSIGNMOB(3024, guild_guard);
  ASSIGNMOB(3025, guild_guard);
  ASSIGNMOB(3026, guild_guard);
  ASSIGNMOB(3027, guild_guard);
  ASSIGNMOB(3059, cityguard);
  ASSIGNMOB(3060, cityguard);
  ASSIGNMOB(3061, janitor);
  ASSIGNMOB(3062, fido);
  ASSIGNMOB(3066, fido);
  ASSIGNMOB(3067, cityguard);
  ASSIGNMOB(3068, janitor);
  ASSIGNMOB(3095, cryogenicist);
#endif
}



/* assign special procedures to objects */
void assign_objects(void)
{
}

/* assign special procedures to rooms */
void assign_rooms(void)
{
  int i;
  SPECIAL(dump);

  assign_elevators();
  
  if (dts_are_dumps)
    for (i = 0; i < top_of_world; i++)
      if (ROOM_FLAGGED(i, ROOM_DEATH))
	world[i].func = dump;
}


