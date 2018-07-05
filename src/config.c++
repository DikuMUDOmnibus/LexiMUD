/* ************************************************************************
*   File: config.c                                      Part of CircleMUD *
*  Usage: Configuration of various aspects of CircleMUD operation         *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#define __CONFIG_C__



#include "structs.h"

//#define TRUE	1
#define YES		1
#define FALSE	0
#define NO		0

/*
 * Below are several constants which you can change to alter certain aspects
 * of the way CircleMUD acts.  Since this is a .c file, all you have to do
 * to change one of the constants (assuming you keep your object files around)
 * is change the constant in this file and type 'make'.  Make will recompile
 * this file and relink; you don't have to wait for the whole thing to
 * recompile as you do if you change a header file.
 *
 * I realize that it would be slightly more efficient to have lots of
 * #defines strewn about, so that, for example, the autowiz code isn't
 * compiled at all if you don't want to use autowiz.  However, the actual
 * code for the various options is quite small, as is the computational time
 * in checking the option you've selected at run-time, so I've decided the
 * convenience of having all your options in this one file outweighs the
 * efficency of doing it the other way.
 *
 */

/****************************************************************************/
/****************************************************************************/


/* GAME PLAY OPTIONS */

/*
 * pk_allowed sets the tone of the entire game.  If pk_allowed is set to
 * NO, then players will not be allowed to kill, summon, charm, or sleep
 * other players, as well as a variety of other "asshole player" protections.
 * However, if you decide you want to have an all-out knock-down drag-out
 * PK Mud, just set pk_allowed to YES - and anything goes.
 */
int pk_allowed = YES;
int mobprogs = YES;
int system_b = YES;


/* is playerthieving allowed? */
int pt_allowed = YES;

/* minimum level a player must be to shout/holler/gossip/music */
int level_can_shout = 1;

/* number of movement points it costs to holler */
int holler_move_cost = 0;

/* number of tics (usually 75 seconds) before PC/NPC corpses decompose */
int max_npc_corpse_time = 6;
int max_pc_corpse_time = 12;

/* should items in death traps automatically be junked? */
int dts_are_dumps = YES;

/* "okay" etc. */
char *OK = "Okay.\r\n";
char *NOPERSON = "No-one by that name here.\r\n";
char *NOEFFECT = "Nothing seems to happen.\r\n";

/****************************************************************************/
/****************************************************************************/


/* RENT/CRASHSAVE OPTIONS */

/*
 * Should the game automatically save people?  (i.e., save player data
 * every 4 kills (on average), and Crash-save as defined below.
 */
int auto_save = YES;

/*
 * if auto_save (above) is yes, how often (in minutes) should the MUD
 * Crash-save people's objects?   Also, this number indicates how often
 * the MUD will Crash-save players' houses.
 */
UInt32 autosave_time = 5;

/* Lifetime of crashfiles and forced-rent (idlesave) files in days */
int crash_file_timeout = 10;

/* Lifetime of normal rent files in days */
int rent_file_timeout = 30;


/****************************************************************************/
/****************************************************************************/


/* ROOM NUMBERS */

/* virtual number of room that mortals should enter at */
VNum mortal_start_room = 1204;

/* virtual number of room that immorts should enter at by default */
VNum immort_start_room = 1204;

/* virtual number of room that frozen players should enter at */
VNum frozen_start_room = 1202;

/*
 * virtual numbers of donation rooms.  note: you must change code in
 * do_drop of act.item.c if you change the number of non-NOWHERE
 * donation rooms.
 */
VNum donation_room_1 = NOWHERE;
VNum donation_room_2 = NOWHERE;	/* unused - room for expansion */
VNum donation_room_3 = NOWHERE;	/* unused - room for expansion */


/****************************************************************************/
/****************************************************************************/


/* GAME OPERATION OPTIONS */

/*
 * This is the default port the game should run on if no port is given on
 * the command-line.  NOTE WELL: If you're using the 'autorun' script, the
 * port number there will override this setting.  Change the PORT= line in
 * instead of (or in addition to) changing this.
 */
int DFLT_PORT = 4000;

/* default directory to use as data directory */
char *DFLT_DIR = "lib";

/* What file to log messages to (ex: "log/syslog").  "" == stderr */
char *LOGFILE = "";

/* maximum number of players allowed before game starts to turn people away */
int MAX_PLAYERS = 300;

/* maximum size of bug, typo and idea files in bytes (to prevent bombing) */
int max_filesize = 50000;

/* maximum number of password attempts before disconnection */
int max_bad_pws = 3;

/*
 * Some nameservers are very slow and cause the game to lag terribly every 
 * time someone logs in.  The lag is caused by the gethostbyaddr() function
 * which is responsible for resolving numeric IP addresses to alphabetic names.
 * Sometimes, nameservers can be so slow that the incredible lag caused by
 * gethostbyaddr() isn't worth the luxury of having names instead of numbers
 * for players' sitenames.
 *
 * If your nameserver is fast, set the variable below to NO.  If your
 * nameserver is slow, of it you would simply prefer to have numbers
 * instead of names for some other reason, set the variable to YES.
 *
 * You can experiment with the setting of nameserver_is_slow on-line using
 * the SLOWNS command from within the MUD.
 */

int nameserver_is_slow = NO;


char *MENU =
"\r\n"
"Welcome to LexiMUD Alpha!\r\n"
"0) Exit from LexiMUD Alpha.\r\n"
"1) Enter the game.\r\n"
"2) Enter description.\r\n"
"3) Read the background story.\r\n"
"4) Change password.\r\n"
"5) Delete this character.\r\n"
"\r\n"
"   Make your choice: ";



char *GREETINGS =
"\r\n"
"                          LexiMUD Alpha\r\n"
"\r\n"
"\r\n"
"  Based on LexiMUD,  a creation of Chris Jacobson and the AvP Team,  with code\r\n"
"  created by, or based on code by, George Greer, Eric Green, Stefan Wasilewski\r\n"
"  David Berry, Jushua Mentzer,  Harvey Gilpin, Brian Williams,  Daniel Muller.\r\n"
"  A derivative of CircleMUD 3.0, by Jeremy Elson;  and DikuMUD, by Hans Henrik\r\n"
"  Staerfeldt, Katja Nyboe,  Tom Madsen, Michael Seifert,  and Sebastian Hammer\r\n"
"\r\n\r\n"
"On your feet soldier!  Name? ";

char *WELC_MESSG =
"\r\n"
"Welcome to LexiMUD\r\n"
"\r\n\r\n";

char *START_MESSG =
"Welcome!  This is your new LexiMUD character!  You can now\r\n"
"carry out missions, find weapons and equipment, and much more -- while\r\n"
"killing people from around the world!\r\n";

/****************************************************************************/
/****************************************************************************/


/* AUTOWIZ OPTIONS */

/*
 * Should the game automatically create a new wizlist/immlist every time
 * someone immorts, or is promoted to a higher (or lower) god level?
 * NOTE: this only works under UNIX systems.
 */
int use_autowiz = NO;

/* If yes, what is the lowest level which should be on the wizlist?  (All
   immort levels below the level you specify will go on the immlist instead.) */
int min_wizlist_lev = LVL_STAFF;
