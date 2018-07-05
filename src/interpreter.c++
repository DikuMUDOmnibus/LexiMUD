/* ************************************************************************
*   File: interpreter.c                                 Part of CircleMUD *
*  Usage: parse user commands, search for specials, call ACMD functions   *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include "structs.h"
#include "alias.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "db.h"
#include "spells.h"
#include "handler.h"
#include "mail.h"
#include "olc.h"
#include "ban.h"
#include "buffer.h"

#include "staffcmds.h"

#include "imc-mercbase.h"
#include "icec-mercbase.h"

extern struct title_type titles[NUM_RACES][16];
extern char *motd;
extern char *imotd;
extern char *background;
extern char *MENU;
extern char *WELC_MESSG;
extern char *START_MESSG;
extern char *GREETINGS;
extern struct player_index_element *player_table;
extern SInt32 circle_restrict;
extern int no_specials;
extern int max_bad_pws;
extern char *race_menu;
extern UInt8 stat_max[NUM_RACES][5];

/* external functions */
void do_start(CharData *ch);
void init_char(CharData *ch);
int parse_race(char arg);
int create_entry(char *name);
bool special(CharData *ch, char *cmd, char *arg);
void oedit_parse(DescriptorData *d, char *arg);
void zedit_parse(DescriptorData *d, char *arg);
void medit_parse(DescriptorData *d, char *arg);
void sedit_parse(DescriptorData *d, char *arg);
void aedit_parse(DescriptorData *d, char *arg);
void hedit_parse(DescriptorData *d, char *arg);
void cedit_parse(DescriptorData *d, char *arg);
void scriptedit_parse(DescriptorData *d, char *arg);
void roll_real_abils(CharData * ch);
int command_mtrigger(CharData *actor, char *cmd, char *argument);
int command_otrigger(CharData *actor, char *cmd, char *argument);
int command_wtrigger(CharData *actor, char *cmd, char *argument);

/* local functions */
Alias *find_alias(LList<Alias *> &alias_list, char *str);
void perform_complex_alias(struct txt_q *input_q, char *orig, Alias *a);
int perform_alias(DescriptorData *d, char *orig);
bool reserved_word(const char *argument);
int find_name(char *name);
int _parse_name(char *arg, char *name);
int perform_dupe_check(DescriptorData *d);
int enter_player_game (DescriptorData *d);


/* prototypes for all do_x functions. */
ACMD(do_afk);
ACMD(do_action);
ACMD(do_advance);
ACMD(do_alias);
ACMD(do_allow);
ACMD(do_assist);
ACMD(do_at);
ACMD(do_attributes);
ACMD(do_backstab);
ACMD(do_ban);
ACMD(do_bash);
ACMD(do_circle);
ACMD(do_commands);
ACMD(do_consider);
ACMD(do_date);
ACMD(do_dc);
ACMD(do_diagnose);
ACMD(do_display);
ACMD(do_drink);
ACMD(do_drop);
ACMD(do_drive);
ACMD(do_eat);
ACMD(do_echo);
ACMD(do_enter);
ACMD(do_equipment);
ACMD(do_examine);
ACMD(do_exits);
ACMD(do_flee);
ACMD(do_follow);
ACMD(do_force);
ACMD(do_gecho);
ACMD(do_gen_comm);
ACMD(do_gen_door);
ACMD(do_gen_ps);
ACMD(do_gen_tog);
ACMD(do_gen_write);
ACMD(do_get);
ACMD(do_give);
ACMD(do_gold);
ACMD(do_goto);
ACMD(do_grab);
ACMD(do_group);
ACMD(do_gsay);
ACMD(do_hcontrol);
ACMD(do_help);
ACMD(do_hide);
ACMD(do_hit);
ACMD(do_house);
ACMD(do_insult);
ACMD(do_inventory);
ACMD(do_invis);
ACMD(do_kick_bite);
ACMD(do_kill);
ACMD(do_last);
ACMD(do_leave);
ACMD(do_levels);
ACMD(do_load);
ACMD(do_look);
ACMD(do_mpasound);
ACMD(do_mpjunk);
ACMD(do_mpecho);
ACMD(do_mpechoat);
ACMD(do_mpechoaround);
ACMD(do_mpkill);
ACMD(do_mpmload);
ACMD(do_mpoload);
ACMD(do_mppurge);
ACMD(do_mpgoto);
ACMD(do_mpat);
ACMD(do_mptransfer);
ACMD(do_mpforce);
ACMD(do_mpdelayed);
ACMD(do_mpremember);
ACMD(do_mpforget);
ACMD(do_mpdelayprog);
ACMD(do_mpcancel);
ACMD(do_mpremove);
ACMD(do_mpgforce);
ACMD(do_mpvforce);
ACMD(do_mpdamage);
ACMD(do_mpassist);
ACMD(do_mpgecho);
ACMD(do_mpzecho);
ACMD(do_move);
ACMD(do_not_here);
ACMD(do_olc);
ACMD(do_order);
ACMD(do_page);
ACMD(do_pkill);
ACMD(do_poofset);
ACMD(do_pour);
ACMD(do_practice);
ACMD(do_purge);
ACMD(do_put);
ACMD(do_qcomm);
ACMD(do_quit);
ACMD(do_reboot);
ACMD(do_reload);
ACMD(do_remove);
ACMD(do_reply);
ACMD(do_report);
ACMD(do_rescue);
ACMD(do_respond);
ACMD(do_rest);
ACMD(do_restore);
ACMD(do_return);
ACMD(do_save);
ACMD(do_say);
ACMD(do_score);
ACMD(do_send);
ACMD(do_set);
ACMD(do_show);
ACMD(do_shoot);
ACMD(do_shutdown);
ACMD(do_sit);
ACMD(do_skillset);
ACMD(do_sleep);
ACMD(do_sneak);
ACMD(do_snoop);
ACMD(do_spec_comm);
ACMD(do_stand);
ACMD(do_stat);
ACMD(do_switch);
ACMD(do_syslog);
ACMD(do_throw);
ACMD(do_pull);
ACMD(do_teleport);
ACMD(do_tell);
ACMD(do_time);
ACMD(do_title);
ACMD(do_toggle);
ACMD(do_track);
ACMD(do_trans);
ACMD(do_unban);
ACMD(do_ungroup);
//ACMD(do_use);
ACMD(do_users);
ACMD(do_visible);
ACMD(do_vnum);
ACMD(do_vstat);
ACMD(do_wake);
ACMD(do_watch);
ACMD(do_wear);
ACMD(do_weather);
ACMD(do_where);
ACMD(do_who);
ACMD(do_wield);
ACMD(do_wimpy);
ACMD(do_wizlock);
ACMD(do_wiznet);
ACMD(do_wizutil);
ACMD(do_write);
ACMD(do_zreset);

ACMD(do_zallow);
ACMD(do_zdeny);
ACMD(do_ostat);
ACMD(do_mstat);
ACMD(do_rstat);
ACMD(do_zstat);
ACMD(do_zlist);
ACMD(do_liblist);

ACMD(do_mpstat);
ACMD(do_mpdump);

ACMD(do_tedit);
  
ACMD(do_push_drag);

ACMD(do_depiss);
ACMD(do_repiss);

ACMD(do_hunt);

ACMD(do_mount);
ACMD(do_unmount);

ACMD(do_vwear);

ACMD(do_promote);

ACMD(do_reward);
ACMD(do_delete);

ACMD(do_copyover);

ACMD(do_buffer);
ACMD(do_overflow);

ACMD(do_attach);
ACMD(do_detach);

// Clan ACMDs
ACMD(do_apply);
ACMD(do_resign);
ACMD(do_enlist);
ACMD(do_boot);
ACMD(do_clanlist);
ACMD(do_clanstat);
ACMD(do_members);
ACMD(do_forceenlist);
ACMD(do_clanwho);
ACMD(do_clanpromote);
ACMD(do_clandemote);
ACMD(do_deposit);

ACMD(do_prompt);

ACMD(do_move_element);

ACMD(do_string);

ACMD(do_massroomsave);

ACMD(do_path);


ACMD(do_wizcall);
ACMD(do_wizassist);


struct command_info *complete_cmd_info;

/* This is the Master Command List(tm).

 * You can put new commands in, take commands out, change the order
 * they appear in, etc.  You can adjust the "priority" of commands
 * simply by changing the order they appear in the command list.
 * For example, if you want "as" to mean "assist" instead of "ask",
 * just put "assist" above "ask" in the Master Command List(tm).
 *
 * In general, utility commands such as "at" should have high priority;
 * infrequently used and dangerously destructive commands should have low
 * priority.
 */

struct command_info cmd_info[] = {
	{ "RESERVED"	, "RESERVED"	, 0, 0, 0, 0 },	/* this must be first -- for specprocs */

	/* directions must come before other commands but after RESERVED */
	{ "north"		, "n"			, POS_STANDING	, do_move		, 0, 0, SCMD_NORTH },
	{ "east"		, "e"			, POS_STANDING	, do_move		, 0, 0, SCMD_EAST },
	{ "south"		, "s"			, POS_STANDING	, do_move		, 0, 0, SCMD_SOUTH },
	{ "west"		, "w"			, POS_STANDING	, do_move		, 0, 0, SCMD_WEST },
	{ "up"			, "u"			, POS_STANDING	, do_move		, 0, 0, SCMD_UP },
	{ "down"		, "d"			, POS_STANDING	, do_move		, 0, 0, SCMD_DOWN },

	/* Communications channels */
	{ "broadcast"	, "broa"		, POS_STUNNED	, do_gen_comm	, STAFF_CMD	, STAFF_SECURITY | STAFF_ADMIN | STAFF_GAME, SCMD_BROADCAST },
	{ "chat"		, "chat"		, POS_STUNNED	, do_gen_comm	, 0, 0, SCMD_GOSSIP },
	{ "clan"		, "clan"		, POS_STUNNED	, do_gen_comm	, 0, 0, SCMD_CLAN },
	{ "gossip"		, "go"			, POS_STUNNED	, do_gen_comm	, 0, 0, SCMD_GOSSIP },
	{ "grats"		, "grats"		, POS_STUNNED	, do_gen_comm	, 0, 0, SCMD_GRATZ },
	{ "msay"		, "msay"		, POS_STUNNED 	, do_gen_comm	, 0, 0, SCMD_MISSION },
	{ "music"		, "mus"			, POS_STUNNED	, do_gen_comm	, 0, 0, SCMD_MUSIC },
	{ "race"		, "rac"			, POS_STUNNED	, do_gen_comm	, 0, 0, SCMD_RACE	},
	{ "shout"		, "shou"		, POS_STUNNED 	, do_gen_comm	, 0, 0, SCMD_SHOUT },

	/* now, the main list */
	{ "assist"		, "as"			, POS_FIGHTING	, do_assist		, 1, 0, 0 },
	{ "afk"			, "afk"			, POS_DEAD		, do_afk		, 0, 0, 0 },
	{ "alias"		, "ali"			, POS_DEAD		, do_alias		, 0, 0, 0 },
	{ "ask"			, "ask"			, POS_RESTING	, do_spec_comm	, 0, 0, SCMD_ASK },
	{ "autoexit"	, "autoex"		, POS_DEAD		, do_gen_tog	, 0, 0, SCMD_AUTOEXIT },
	{ "at"			, "a"			, POS_DEAD		, do_at			, STAFF_CMD	, STAFF_GEN, 0 },
	{ "attributes"	, "attr"		, POS_DEAD		, do_attributes	, 0, 0, 0 },

	{ "backstab"	, "back"		, POS_STANDING	, do_backstab	, 1, 0, 0 },
	{ "bash"		, "bash"		, POS_FIGHTING	, do_bash		, 1, 0, 0 },
	{ "bite"		, "bite"		, POS_FIGHTING	, do_kick_bite	, 1, 0, SCMD_BITE },
	{ "brief"		, "brief"		, POS_DEAD		, do_gen_tog	, 0, 0, SCMD_BRIEF },
	{ "bug"			, "bug"			, POS_DEAD		, do_gen_write	, 0, 0, SCMD_BUG },

	{ "circle"		, "circ"		, POS_FIGHTING	, do_circle		, 0, 0, 0 },
	{ "clear"		, "clear"		, POS_DEAD		, do_gen_ps		, 0, 0, SCMD_CLEAR },
	{ "close"		, "c"			, POS_SITTING	, do_gen_door	, 0, 0, SCMD_CLOSE },
	{ "cls"			, "cls"			, POS_DEAD		, do_gen_ps		, 0, 0, SCMD_CLEAR },
	{ "consider"	, "co"			, POS_RESTING	, do_consider	, 0, 0, 0},
	{ "color"		, "color"		, POS_DEAD		, do_gen_tog	, 0, 0, SCMD_COLOR },
	{ "commands"	, "comm"		, POS_DEAD		, do_commands	, 0, 0, SCMD_COMMANDS },
	{ "compact"		, "comp"		, POS_DEAD		, do_gen_tog	, 0, 0, SCMD_COMPACT },
	{ "credits"		, "cred"		, POS_DEAD		, do_gen_ps		, 0, 0, SCMD_CREDITS },

	{ "diagnose"	, "diag"		, POS_RESTING	, do_diagnose	, 0, 0, 0 },
	{ "display"		, "disp"		, POS_DEAD		, do_display	, 0, 0, 0 },
	{ "donate"		, "don"			, POS_RESTING	, do_drop		, 0, 0, SCMD_DONATE },
	{ "drop"		, "drop"		, POS_RESTING	, do_drop		, 0, 0, SCMD_DROP },
	{ "drag"		, "drag"		, POS_STANDING	, do_push_drag	, 0, 0, SCMD_DRAG },
	{ "drink"		, "drink"		, POS_RESTING	, do_drink		, 0, 0, SCMD_DRINK },
	{ "drive"		, "dri"			, POS_RESTING	, do_drive		, 0, 0, 0 },

	{ "eat"			, "eat"			, POS_RESTING	, do_eat		, 0, 0, SCMD_EAT },
	{ "emote"		, "em"			, POS_RESTING	, do_echo		, 0, 0, SCMD_EMOTE },
	{ ":"			, ":"			, POS_RESTING	, do_echo		, 0, 0, SCMD_EMOTE },
	{ "enter"		, "enter"		, POS_STANDING	, do_enter		, 0, 0, 0 },
	{ "equipment"	, "eq"			, POS_SLEEPING	, do_equipment	, 0, 0, 0 },
	{ "exits"		, "ex"			, POS_RESTING	, do_exits		, 0, 0, 0 },
	{ "examine"		, "exa"			, POS_SITTING	, do_examine	, 0, 0, 0 },

	{ "fill"		, "fill"		, POS_STANDING	, do_pour		, 0, SCMD_FILL },
	{ "flee"		, "fl"			, POS_FIGHTING	, do_flee		, 0, 0, 0 },
	{ "follow"		, "fol"			, POS_RESTING	, do_follow		, 0, 0, 0 },

	{ "get"			, "g"			, POS_RESTING	, do_get		, 0, 0, 0 },
	{ "give"		, "gi"			, POS_RESTING	, do_give		, 0, 0, 0 },
	{ "gold"		, "gol"			, POS_RESTING	, do_gold		, 0, 0, 0 },
	{ "group"		, "gr"			, POS_RESTING	, do_group		, 0, 0, 0 },
	{ "grab"		, "grab"		, POS_RESTING	, do_grab		, 0, 0, 0 },
	{ "gsay"		, "gs"			, POS_SLEEPING	, do_gsay		, 0, 0, 0 },
	{ "gtell"		, "gt"			, POS_SLEEPING	, do_gsay		, 0, 0, 0 },

	{ "help"		, "h"			, POS_DEAD		, do_help		, 0, 0, 0 },
	{ "hide"		, "hide"		, POS_RESTING	, do_hide		, 0, 0, 0 },
	{ "hit"			, "hit"			, POS_FIGHTING	, do_hit		, 0, 0, SCMD_HIT },
	{ "hold"		, "hold"		, POS_RESTING	, do_grab		, 0, 0, 0 },
	{ "house"		, "house"		, POS_RESTING	, do_house		, 0, 0, 0 },

	{ "inventory"	, "i"			, POS_DEAD		, do_inventory	, 0, 0, 0 },
	{ "idea"		, "idea"		, POS_DEAD		, do_gen_write	, 0, 0, SCMD_IDEA },
	{ "immlist"		, "imm"			, POS_DEAD		, do_gen_ps		, 0, 0, SCMD_IMMLIST },
	{ "info"		, "in"			, POS_SLEEPING	, do_gen_ps		, 0, 0, SCMD_INFO },
	{ "insult"		, "insult"		, POS_RESTING	, do_insult		, 0, 0, 0 },

	{ "kill"		, "ki"			, POS_FIGHTING	, do_kill		, 0, 0, 0 },
	{ "kick"		, "kick"		, POS_FIGHTING	, do_kick_bite	, 0, 0, SCMD_KICK },

	{ "look"		, "l"			, POS_RESTING	, do_look		, 0, 0, SCMD_LOOK },
	{ "leave"		, "le"			, POS_STANDING	, do_leave		, 0, 0, 0 },
	{ "levels"		, "lev"			, POS_DEAD		, do_levels		, 0, 0, 0 },
	{ "lock"		, "loc"			, POS_SITTING	, do_gen_door	, 0, 0, SCMD_LOCK },

	{ "motd"		, "mot"			, POS_DEAD		, do_gen_ps		, 0, 0, SCMD_MOTD },
	{ "mission"		, "mission"		, POS_DEAD		, do_gen_tog	, 0, 0, SCMD_QUEST },
	{ "mount"		, "mou"			, POS_STANDING	, do_mount		, 0, 0, 0 },
	{ "murder"		, "mur"			, POS_FIGHTING	, do_hit		, 0, 0, SCMD_MURDER },

	{ "mpstat"		, "mpstat"		, POS_DEAD		, do_mpstat		, STAFF_CMD, STAFF_OLC, 0 },
	{ "mpdump"		, "mpdump"		, POS_DEAD		, do_mpdump		, STAFF_CMD, STAFF_OLC, 0 },

	{ "mpasound"	, "mpasound"	, POS_DEAD		, do_mpasound	, MOB_CMD, 0, 0 },
	{ "mpjunk"		, "mpjunk"		, POS_DEAD		, do_mpjunk		, MOB_CMD, 0, 0 },
	{ "mpecho"		, "mpecho"		, POS_DEAD		, do_mpecho		, MOB_CMD, 0, 0 },
	{ "mpechoat"	, "mpechoat"	, POS_DEAD		, do_mpechoat	, MOB_CMD, 0, 0 },
	{ "mpechoaround", "mpechoaround", POS_DEAD		, do_mpechoaround, MOB_CMD, 0, 0 },
	{ "mpkill"		, "mpkill"		, POS_DEAD		, do_mpkill		, MOB_CMD, 0, 0 },
	{ "mpmload"		, "mpmload"		, POS_DEAD		, do_mpmload	, MOB_CMD, 0, 0 },
	{ "mpoload"		, "mpoload"		, POS_DEAD		, do_mpoload	, MOB_CMD, 0, 0 },
	{ "mppurge"		, "mppurge"		, POS_DEAD		, do_mppurge	, MOB_CMD, 0, 0 },
	{ "mpgoto"		, "mpgoto"		, POS_DEAD		, do_mpgoto		, MOB_CMD, 0, 0 },
	{ "mpat"		, "mpat"		, POS_DEAD		, do_mpat		, MOB_CMD, 0, 0 },
	{ "mptransfer"	, "mptransfer"	, POS_DEAD		, do_mptransfer	, MOB_CMD, 0, 0 },
	{ "mpforce"		, "mpforce"		, POS_DEAD		, do_mpforce	, MOB_CMD, 0, 0 },
	{ "mpdelayed"	, "mpdelayed"	, POS_DEAD		, do_mpdelayed	, MOB_CMD, 0, 0 },

	{ "mpremember"	, "mprem"		, POS_DEAD		, do_mpremember	, MOB_CMD, 0, 0 },
	{ "mpforget"	, "mpforget"	, POS_DEAD		, do_mpforget	, MOB_CMD, 0, 0 },
	{ "mpdelayprog"	, "mpdelayprog"	, POS_DEAD		, do_mpdelayprog, MOB_CMD, 0, 0 },
	{ "mpcancel"	, "mpcancel"	, POS_DEAD		, do_mpcancel	, MOB_CMD, 0, 0 },
	{ "mpremove"	, "mpremove"	, POS_DEAD		, do_mpremove	, MOB_CMD, 0, 0 },
	{ "mpgforce"	, "mpgforce"	, POS_DEAD		, do_mpgforce	, MOB_CMD, 0, 0 },
	{ "mpvforce"	, "mpvforce"	, POS_DEAD		, do_mpvforce	, MOB_CMD, 0, 0 },
	{ "mpdamage"	, "mpdamage"	, POS_DEAD		, do_mpdamage	, MOB_CMD, 0, 0 },
	{ "mpassist"	, "mpassist"	, POS_DEAD		, do_mpassist	, MOB_CMD, 0, 0 },
	{ "mpgecho"		, "mpgecho"		, POS_DEAD		, do_mpgecho	, MOB_CMD, 0, 0 },
	{ "mpzecho"		, "mpzecho"		, POS_DEAD		, do_mpzecho	, MOB_CMD, 0, 0 },

/*
	{ "mat"			, "mat"			, POS_DEAD		, do_mat		, MOB_CMD, 0, 0 },
	{ "mdamage"		, "mdamage"		, POS_DEAD		, do_mdamage	, MOB_CMD, 0, 0 },
	{ "mecho"		, "mecho"		, POS_DEAD		, do_mecho		, MOB_CMD, 0, 0 },
	{ "mechoaround"	, "mechoaround"	, POS_DEAD		, do_send		, MOB_CMD, 0, SCMD_MECHOAROUND },
	{ "mforce"		, "mforce"		, POS_DEAD		, do_mforce		, MOB_CMD, 0, 0 },
	{ "mgoto"		, "mgoto"		, POS_DEAD		, do_mgoto		, MOB_CMD, 0, 0 },
	{ "mheal"		, "mheal"		, POS_DEAD		, do_mheal		, MOB_CMD, 0, 0 },
	{ "minfo"		, "minfo"		, POS_DEAD		, do_minfo		, MOB_CMD, 0, 0 },
	{ "mjunk"		, "mjunk"		, POS_DEAD		, do_mjunk		, MOB_CMD, 0, 0 },
	{ "mkill"		, "mkill"		, POS_DEAD		, do_mkill		, MOB_CMD, 0, 0 },
	{ "mload"		, "mload"		, POS_DEAD		, do_mload		, MOB_CMD, 0, 0 },
	{ "mmp"			, "mmp"			, POS_DEAD		, do_mmp		, MOB_CMD, 0, 0 },
	{ "mpurge"		, "mpurge"		, POS_DEAD		, do_mpurge		, MOB_CMD, 0, 0 },
	{ "mremove"		, "mremove"		, POS_DEAD		, do_mremove	, MOB_CMD, 0, 0 },
	{ "msend"		, "msend"		, POS_DEAD		, do_msend		, MOB_CMD, 0, SCMD_MSEND },
	{ "mteleport"	, "mteleport"	, POS_DEAD		, do_mteleport	, MOB_CMD, 0, 0 },
	{ "mzoneecho"	, "mzoneecho"	, POS_DEAD		, do_mzoneecho	, MOB_CMD, 0, 0 },
*/

	{ "news"		, "ne"			, POS_SLEEPING	, do_gen_ps		, 0, 0, SCMD_NEWS },
	{ "norace"		, "nora"		, POS_DEAD		, do_gen_tog	, 1, 0, SCMD_NORACE },
	{ "nomusic"		, "nomu"		, POS_DEAD		, do_gen_tog	, 1, 0, SCMD_NOMUSIC },
	{ "nogossip"	, "nogo"		, POS_DEAD		, do_gen_tog	, 1, 0, SCMD_NOGOSSIP },
	{ "nochat"		, "noch"		, POS_DEAD		, do_gen_tog	, 1, 0, SCMD_NOGOSSIP },
	{ "nograts"		, "nogr"		, POS_DEAD		, do_gen_tog	, 1, 0, SCMD_NOGRATZ },
	{ "norepeat"	, "nore"		, POS_DEAD		, do_gen_tog	, 1, 0, SCMD_NOREPEAT },
	{ "noshout"		, "nosh"		, POS_DEAD		, do_gen_tog	, 1, 0, SCMD_DEAF },
	{ "nosummon"	, "nosu"		, POS_DEAD		, do_gen_tog	, 1, 0, SCMD_NOSUMMON },
	{ "notell"		, "note"		, POS_DEAD		, do_gen_tog	, 1, 0, SCMD_NOTELL },
	{ "noinfo"		, "noin"		, POS_DEAD		, do_gen_tog	, 1, 0, SCMD_NOINFO },

	{ "order"		, "ord"			, POS_RESTING	, do_order		, 1, 0, 0 },
	{ "open"		, "o"			, POS_SITTING	, do_gen_door	, 0, 0, SCMD_OPEN },

	{ "pick"		, "pick"		, POS_STANDING	, do_gen_door	, 1, 0, SCMD_PICK },
	{ "pkill"		, "pk"			, POS_DEAD		, do_pkill		, 1, 0, 0 },
	{ "policy"		, "pol"			, POS_DEAD		, do_gen_ps		, 0, 0, SCMD_POLICIES },
	{ "pour"		, "pour"		, POS_STANDING	, do_pour		, 0, 0, SCMD_POUR },
	{ "prompt"		, "prompt"		, POS_DEAD		, do_prompt		, 0, 0, 0 },
	{ "practice"	, "pr"			, POS_RESTING	, do_practice	, 1, 0, 0 },
	{ "promote"		, "promote"		, POS_RESTING	, do_promote	, 1, 0, 0 },
	{ "pull"		, "pull"		, POS_RESTING	, do_pull		, 0, 0, 0 },
	{ "push"		, "push"		, POS_STANDING	, do_push_drag	, 0, 0, SCMD_PUSH },
	{ "put"			, "p"			, POS_RESTING	, do_put		, 0, 0, 0 },

	{ "quit"		, "quit"		, POS_DEAD		, do_quit		, 0, 0, 0 },

	{ "reply"		, "r"			, POS_SLEEPING	, do_reply		, 0, 0, 0 },
	{ "rest"		, "re"			, POS_RESTING	, do_rest		, 0, 0, 0 },
	{ "respond"		, "resp"	    , POS_RESTING	, do_respond	, 0, 0, 0 },
	{ "read"		, "read"		, POS_RESTING	, do_look		, 0, 0, SCMD_READ },
	{ "reload"		, "rel"			, POS_RESTING	, do_reload		, 0, 0, SCMD_LOAD },
	{ "remove"		, "rem"			, POS_RESTING	, do_remove		, 0, 0, 0 },
	{ "report"		, "rep"			, POS_RESTING	, do_report		, 0, 0, 0 },
	{ "rescue"		, "resc"		, POS_FIGHTING	, do_rescue		, 0, 0, 0 },
	{ "return"		, "return"		, POS_DEAD		, do_return		, 0, 0, 0 },

	{ "say"			, "sa"			, POS_STUNNED	, do_say		, 0, 0, 0 },
	{ "'"			, "'"			, POS_STUNNED	, do_say		, 0, 0, 0 },
	{ "save"		, "sav"			, POS_STUNNED	, do_save		, 0, 0, 0 },
	{ "score"		, "sc"			, POS_DEAD		, do_score		, 0, 0, 0 },
	{ "shoot"		, "sh"			, POS_SITTING	, do_shoot		, 0, 0, 0},
	{ "sip"			, "sip"			, POS_RESTING	, do_drink		, 0, 0, SCMD_SIP },
	{ "sit"			, "si"			, POS_RESTING	, do_sit		, 0, 0, 0 },
	{ "sleep"		, "sl"			, POS_SLEEPING	, do_sleep		, 0, 0, 0 },
	{ "sneak"		, "sn"			, POS_STANDING	, do_sneak		, 1, 0, 0 },
	{ "socials"		, "soc"			, POS_DEAD		, do_commands	, 0, 0, SCMD_SOCIALS },
	{ "stand"		, "st"			, POS_RESTING	, do_stand		, 0, 0, 0 },

	{ "tell"		, "t"			, POS_DEAD		, do_tell		, 0, 0, 0 },
	{ "take"		, "ta"			, POS_RESTING	, do_get		, 0, 0, 0 },
	{ "taste"		, "tas"			, POS_RESTING	, do_eat		, 0, 0, SCMD_TASTE },
	{ "throw"		, "th"			, POS_RESTING	, do_throw		, 0, 0, 0 },
	{ "time"		, "ti"			, POS_DEAD		, do_time		, 0, 0, 0 },
	{ "toggle"		, "to"			, POS_DEAD		, do_toggle		, 0, 0, 0 },
	{ "track"		, "tr"			, POS_STANDING	, do_track		, 0, 0, 0 },
	{ "typo"		, "typo"		, POS_DEAD		, do_gen_write	, 0, 0, SCMD_TYPO },

	{ "unload"		, "unloa"		, POS_RESTING	, do_reload		, 0, 0, SCMD_UNLOAD },
	{ "unlock"		, "unl"			, POS_SITTING	, do_gen_door	, 0, 0, SCMD_UNLOCK },
	{ "unmount"		, "unm"			, POS_SITTING	, do_unmount	, 0, 0, 0 },
	{ "ungroup"		, "ung"			, POS_DEAD		, do_ungroup	, 0, 0, 0 },

	{ "version"		, "ver"			, POS_DEAD		, do_gen_ps		, 0, 0, SCMD_VERSION },
	{ "visible"		, "vi"			, POS_RESTING	, do_visible	, 1, 0, 0 },

	{ "wake"		, "wa"			, POS_SLEEPING	, do_wake		, 0, 0, 0 },
	{ "watch"		, "wat"			, POS_RESTING	, do_watch		, 0, 0, 0 },
	{ "wear"		, "wea"			, POS_RESTING	, do_wear		, 0, 0, 0 },
	{ "weather"		, "wea"			, POS_RESTING	, do_weather	, 0, 0, 0 },
	{ "who"			, "wh"			, POS_DEAD		, do_who		, 0, 0, 0 },
	{ "whoami"		, "whoa"		, POS_DEAD		, do_gen_ps		, 0, 0, SCMD_WHOAMI },
	{ "whisper"		, "whi"			, POS_STUNNED	, do_spec_comm	, 0, 0, SCMD_WHISPER },
	{ "wield"		, "wi"			, POS_RESTING	, do_wield		, 0, 0, 0 },
	{ "wimpy"		, "wim"			, POS_DEAD		, do_wimpy		, 0, 0, 0 },
	{ "wizlist"		, "wizl"		, POS_DEAD		, do_gen_ps		, 0, 0, SCMD_WIZLIST },
	{ "write"		, "wr"			, POS_RESTING	, do_write		, 0, 0, 0 },

	{ "?"			, "?"			, POS_DEAD		, do_help		, 0, 0, 0 },

	//	Clan commands
	{ "apply"		, "app"			, POS_DEAD		, do_apply		, 0, 0, 0 },
	{ "resign"		, "resign"		, POS_DEAD		, do_resign		, 0, 0, 0 },
	{ "enlist"		, "enl"			, POS_DEAD		, do_enlist		, 0, 0, 0 },
	{ "boot"		, "boot"		, POS_DEAD		, do_boot		, 0, 0, 0 },
	{ "clans"		, "clans"		, POS_DEAD		, do_clanlist	, 0, 0, 0 },
	{ "members"		, "mem"			, POS_DEAD		, do_members	, 0, 0, 0 },
	{ "cwho"		, "cwho"		, POS_DEAD		, do_clanwho	, 0, 0, 0 },
	{ "cpromote"	, "cpro"		, POS_DEAD		, do_clanpromote, 0, 0, 0 },
	{ "cdemote"		, "cdem"		, POS_DEAD		, do_clandemote	, 0, 0, 0 },
	{ "deposit"		, "depo"		, POS_DEAD		, do_deposit	, 0, 0, 0 },

	//	IMC commands
	{ "rtell"		, "rt"			, POS_SLEEPING	, do_rtell		, 6, 0, 0 },
	{ "rreply"		, "rr"			, POS_SLEEPING	, do_rreply		, 6, 0, 0 },
	{ "rwho"		, "rw"			, POS_SLEEPING	, do_rwho		, 6, 0, 0 },
	{ "rwhois"		, "rwhoi"		, POS_SLEEPING	, do_rwhois		, 6, 0, 0 },
	{ "rquery"		, "rq"			, POS_SLEEPING	, do_rquery		, 6, 0, 0 },
	{ "rfinger"		, "rf"			, POS_SLEEPING	, do_rfinger	, 6, 0, 0 },
	{ "imc"			, "im"			, POS_DEAD		, do_imc		, STAFF_CMD, STAFF_IMC, 0 },
	{ "imclist"		, "imcl"		, POS_DEAD		, do_imclist	, 6, 0, 0 },
	{ "rbeep"		, "rb"			, POS_SLEEPING	, do_rbeep		, 6, 0, 0 },
	{ "istats"		, "ist"			, POS_DEAD		, do_istats		, 6, 0, 0 },
	{ "rchannels"	, "rch"			, POS_DEAD		, do_rchannels	, 6, 0, 0 },

	{ "rinfo"		, "rin"			, POS_DEAD		, do_rinfo		, 6, 0, 0 },
	{ "rsockets"	, "rso"			, POS_DEAD		, do_rsockets	, STAFF_CMD, STAFF_IMC, 0 },
	{ "rconnect"	, "rc"			, POS_DEAD		, do_rconnect	, STAFF_CMD, STAFF_IMC, 0 },
	{ "rdisconnect"	, "rd"			, POS_DEAD		, do_rdisconnect, STAFF_CMD, STAFF_IMC, 0 },
	{ "rignore"		, "rig"			, POS_DEAD		, do_rignore	, STAFF_CMD, STAFF_IMC, 0 },
	{ "rchanset"	, "rchans"		, POS_DEAD		, do_rchanset	, STAFF_CMD, STAFF_IMC, 0 },
	{ "mailqueue"	, "mailq"		, POS_DEAD		, do_mailqueue	, STAFF_CMD, STAFF_IMC, 0 },

	{ "icommand"	, "ico"			, POS_DEAD		, do_icommand	, STAFF_CMD, STAFF_IMC, 0 },
	{ "isetup"		, "ise"			, POS_DEAD		, do_isetup		, STAFF_CMD, STAFF_IMC, 0 },
	{ "ilist"		, "il"			, POS_DEAD		, do_ilist		, 6, 0, 0 },
	{ "ichannels"	, "ich"			, POS_DEAD		, do_ichannels	, 6, 0, 0 },

	{ "rping"		, "rp"			, POS_DEAD		, do_rping		, STAFF_CMD, STAFF_IMC, 0 },
	
	//	Assistance commands
	{ "wizcall"		, "wizc"		, POS_DEAD		, do_wizcall	, 0, 0, 0 },
	{ "wizassist"	, "wiza"		, POS_DEAD		, do_wizassist	, STAFF_CMD, STAFF_GEN, 0 },
  
  /* Staff Commands */
	{ "allow"		, "allow"		, POS_DEAD		, do_allow		, LVL_IMMORT, 0, SCMD_ALLOW },
	{ "deny"		, "deny"		, POS_DEAD		, do_allow		, LVL_IMMORT, 0, SCMD_DENY },

	{ "date"		, "date"		, POS_DEAD		, do_date		, LVL_IMMORT, 0, SCMD_DATE },
	{ "echo"		, "echo"		, POS_SLEEPING	, do_echo		, STAFF_CMD	, STAFF_GEN, SCMD_ECHO },
	{ "force"		, "f"			, POS_SLEEPING	, do_force		, STAFF_CMD	, STAFF_SECURITY | STAFF_GAME | STAFF_OLC, 0 },
	{ "handbook"	, "hand"		, POS_DEAD		, do_gen_ps		, LVL_IMMORT, 0, SCMD_HANDBOOK },
	{ "imotd"		, "im"			, POS_DEAD		, do_gen_ps		, LVL_IMMORT, 0, SCMD_IMOTD },
	{ "goto"		, "got"			, POS_SLEEPING	, do_goto		, STAFF_CMD	, STAFF_GEN, 0 },
	{ "holylight"	, "holy"		, POS_DEAD		, do_gen_tog	, STAFF_CMD	, STAFF_GEN, SCMD_HOLYLIGHT },
	{ "last"		, "last"		, POS_DEAD		, do_last		, STAFF_CMD	, STAFF_GEN, 0 },
	{ "mecho"		, "mecho"		, POS_DEAD		, do_qcomm		, STAFF_CMD	, STAFF_CHAR, SCMD_QECHO },
	{ "nohassle"	, "nohass"		, POS_DEAD		, do_gen_tog	, STAFF_CMD	, STAFF_GEN, SCMD_NOHASSLE },
	{ "nowiz"		, "nowiz"		, POS_DEAD		, do_gen_tog	, STAFF_CMD	, STAFF_GEN, SCMD_NOWIZ },
	{ "poofin"		, "poofi"		, POS_DEAD		, do_poofset	, STAFF_CMD	, STAFF_GEN, SCMD_POOFIN },
	{ "poofout"		, "poofo"		, POS_DEAD		, do_poofset	, STAFF_CMD	, STAFF_GEN, SCMD_POOFOUT },
	{ "send"		, "send"		, POS_SLEEPING	, do_send		, STAFF_CMD	, STAFF_GEN, 0 },
	{ "show"		, "show"		, POS_DEAD		, do_show		, STAFF_CMD	, STAFF_GEN, 0 },
	{ "title"		, "title"		, POS_DEAD		, do_title		, LVL_IMMORT, 0, 0 },
	{ "teleport"	, "tele"		, POS_DEAD		, do_teleport	, STAFF_CMD	, STAFF_GEN, 0 },
	{ "transfer"	, "tran"		, POS_SLEEPING	, do_trans		, STAFF_CMD	, STAFF_GEN, 0 },
	{ "uptime"		, "upt"			, POS_DEAD		, do_date		, LVL_IMMORT, 0, SCMD_UPTIME },
	{ "where"		, "where"		, POS_RESTING	, do_where		, LVL_IMMORT, 0, 0 },
	{ "wizhelp"		, "wizhelp"		, POS_SLEEPING	, do_commands	, LVL_IMMORT, 0, SCMD_WIZHELP },
	{ "wiznet"		, "wiz"			, POS_DEAD		, do_wiznet		, STAFF_CMD	, STAFF_GEN, 0 },
	{ ";"			, ";"			, POS_DEAD		, do_wiznet		, STAFF_CMD	, STAFF_GEN, 0 },

	{ "load"		, "load"		, POS_DEAD		, do_load		, STAFF_CMD	, STAFF_GEN, 0 },
	{ "invis"		, "invis"		, POS_DEAD		, do_invis		, STAFF_CMD	, STAFF_GEN, 0 },
	{ "mute"		, "mu"			, POS_DEAD		, do_wizutil	, STAFF_CMD	, STAFF_SECURITY, SCMD_SQUELCH },
	{ "page"		, "pa"			, POS_DEAD		, do_page		, STAFF_CMD	, STAFF_GEN, 0 },
	{ "purge"		, "purge"		, POS_DEAD		, do_purge		, STAFF_CMD	, STAFF_SECURITY | STAFF_OLC, 0 },
	{ "restore"		, "restore"		, POS_DEAD		, do_restore	, STAFF_CMD	, STAFF_CHAR, 0 },
	{ "reward"		, "reward"		, POS_DEAD		, do_reward		, STAFF_CMD	, STAFF_CHAR, 0 },
	{ "roomflags"	, "roomfl"		, POS_DEAD		, do_gen_tog	, STAFF_CMD	, STAFF_GEN, SCMD_ROOMFLAGS },
	{ "set"			, "set"			, POS_DEAD		, do_set		, STAFF_CMD	, STAFF_CHAR, 0 },
	{ "snoop"		, "snoop"		, POS_DEAD		, do_snoop		, STAFF_CMD	, STAFF_SECURITY | STAFF_ADMIN, 0 },
	{ "stat"		, "stat"		, POS_DEAD		, do_stat		, STAFF_CMD	, STAFF_GEN, SCMD_STAT },
	{ "sstat"		, "sstat"		, POS_DEAD		, do_stat		, STAFF_CMD	, STAFF_GEN, SCMD_SSTAT },
	{ "string"		, "string"		, POS_DEAD		, do_string		, STAFF_CMD	, STAFF_CHAR, 0 },
	{ "syslog"		, "sys"			, POS_DEAD		, do_syslog		, STAFF_CMD	, STAFF_GEN, 0 },
	{ "unaffect"	, "unaff"		, POS_DEAD		, do_wizutil	, STAFF_CMD	, STAFF_CHAR, SCMD_UNAFFECT },
	{ "users"		, "us"			, POS_DEAD		, do_users		, STAFF_CMD	, STAFF_GEN, 0 },
	{ "vnum"		, "vn"			, POS_DEAD		, do_vnum		, STAFF_CMD	, STAFF_GEN, 0 },
	{ "vstat"		, "vst"			, POS_DEAD		, do_vstat		, STAFF_CMD	, STAFF_GEN, 0 },
	{ "vwear"		, "vwear"		, POS_DEAD		, do_vwear		, STAFF_CMD	, STAFF_GEN, 0 },
	{ "zreset"		, "zreset"		, POS_DEAD		, do_zreset		, STAFF_CMD	, STAFF_GEN, 0 },

	{ "attach"		, "attach"		, POS_DEAD		, do_attach		, STAFF_CMD	, STAFF_SCRIPT, 0 },
	{ "detach"		, "detach"		, POS_DEAD		, do_detach		, STAFF_CMD	, STAFF_SCRIPT, 0 },

	{ "ban"			, "ban"			, POS_DEAD		, do_ban		, STAFF_CMD	, STAFF_SECURITY, 0 },
	{ "dc"			, "dc"			, POS_DEAD		, do_dc			, STAFF_CMD	, STAFF_SECURITY, 0 },
	{ "depiss"		, "depiss"		, POS_DEAD		, do_depiss		, STAFF_CMD	, STAFF_GAME, 0 },
	{ "gecho"		, "gecho"		, POS_DEAD		, do_gecho		, STAFF_CMD	, STAFF_GAME, 0 },
	{ "hunt"		, "hunt"		, POS_DEAD		, do_hunt		, STAFF_CMD	, STAFF_GAME, 0 },
	{ "notitle"		, "notitle"		, POS_DEAD		, do_wizutil	, STAFF_CMD	, STAFF_SECURITY, SCMD_NOTITLE },
	{ "pardon"		, "pard"		, POS_DEAD		, do_wizutil	, STAFF_CMD	, STAFF_CHAR, SCMD_PARDON },
	{ "repiss"		, "repiss"		, POS_DEAD		, do_repiss		, STAFF_CMD	, STAFF_GAME, 0 },
	{ "reroll"		, "reroll"		, POS_DEAD		, do_wizutil	, STAFF_CMD	, STAFF_CHAR, SCMD_REROLL },
	{ "skillset"	, "skillset"	, POS_DEAD		, do_skillset	, STAFF_CMD	, STAFF_CHAR, 0 },
	{ "switch"		, "swit"		, POS_DEAD		, do_switch		, STAFF_CMD	, STAFF_GAME, 0 },
	{ "unban"		, "unban"		, POS_DEAD		, do_unban		, STAFF_CMD	, STAFF_SECURITY | STAFF_ADMIN, 0 },

	{ "advance"		, "ad"			, POS_DEAD		, do_advance	, STAFF_CMD	, STAFF_CHAR | STAFF_ADMIN, 0 },
	{ "copyover"	, "copyover"	, POS_DEAD		, do_copyover	, STAFF_CMD	, STAFF_ADMIN, 0 },
	{ "hcontrol"	, "hcontrol"	, POS_DEAD		, do_hcontrol	, STAFF_CMD	, STAFF_HOUSES, 0 },
	{ "shutdown"	, "shutdown"	, POS_DEAD		, do_shutdown	, STAFF_CMD	, STAFF_ADMIN, 0 },
	{ "slowns"		, "slowns"		, POS_DEAD		, do_gen_tog	, STAFF_CMD	, STAFF_ADMIN, SCMD_SLOWNS },
	{ "reboot"		, "reboot"		, POS_DEAD		, do_reboot		, STAFF_CMD	, STAFF_ADMIN, 0 },
	{ "wizlock"		, "wizlock"		, POS_DEAD		, do_wizlock	, STAFF_CMD	, STAFF_ADMIN, 0 },
	{ "zallow"		, "zallow"		, POS_DEAD		, do_zallow		, STAFF_CMD	, STAFF_OLCADMIN, 0 },
	{ "zdeny"		, "zdeny"		, POS_DEAD		, do_zdeny		, STAFF_CMD	, STAFF_OLCADMIN, 0 },

	{ "buffer"		, "buffer"		, POS_DEAD		, do_buffer		, LVL_CODER	, 0, 0 },
	{ "crash"		, "crash"		, POS_DEAD		, do_overflow	, LVL_CODER	, 0, 0 },
	
	{ "path"		, "path"		, POS_DEAD		, do_path		, LVL_IMMORT, 0, 0 },

	{ "freeze"		, "fr"			, POS_DEAD		, do_wizutil	, STAFF_CMD	, STAFF_SECURITY, SCMD_FREEZE },
	{ "thaw"		, "thaw"		, POS_DEAD		, do_wizutil	, STAFF_CMD	, STAFF_SECURITY, SCMD_THAW },


	//	OLC Commands
	{ "olc"			, "olc"			, POS_DEAD		, do_olc		, STAFF_CMD	, STAFF_OLC, SCMD_OLC_SAVEINFO },

	{ "aedit"		, "aed"			, POS_DEAD		, do_olc		, STAFF_CMD	, STAFF_SOCIALS, SCMD_OLC_AEDIT},
	{ "cedit"		, "ced"			, POS_DEAD		, do_olc		, STAFF_CMD	, STAFF_CLANS, SCMD_OLC_CEDIT},
	{ "hedit"		, "hed"			, POS_DEAD		, do_olc		, STAFF_CMD	, STAFF_HELP, SCMD_OLC_HEDIT},
	{ "medit"		, "med"			, POS_DEAD		, do_olc		, STAFF_CMD	, STAFF_OLC, SCMD_OLC_MEDIT},
	{ "oedit"		, "oed"			, POS_DEAD		, do_olc		, STAFF_CMD	, STAFF_OLC, SCMD_OLC_OEDIT},
	{ "redit"		, "red"			, POS_DEAD		, do_olc		, STAFF_CMD	, STAFF_OLC, SCMD_OLC_REDIT},
	{ "sedit"		, "sed"			, POS_DEAD		, do_olc		, STAFF_CMD	, STAFF_SHOPS, SCMD_OLC_SEDIT},
	{ "tedit"		, "ted"			, POS_DEAD		, do_tedit		, STAFF_CMD	, STAFF_ADMIN, 0 },
	{ "zedit"		, "zed"			, POS_DEAD		, do_olc		, STAFF_CMD	, STAFF_OLC, SCMD_OLC_ZEDIT},
	{ "scriptedit"	, "scriptedit"	, POS_DEAD		, do_olc		, STAFF_CMD	, STAFF_SCRIPT, SCMD_OLC_SCRIPTEDIT},
	
	{ "mstat"		, "mstat"		, POS_DEAD		, do_mstat		, STAFF_CMD	, STAFF_GEN, 0 },
	{ "ostat"		, "ostat"		, POS_DEAD		, do_ostat		, STAFF_CMD	, STAFF_GEN, 0 },
	{ "rstat"		, "rstat"		, POS_DEAD		, do_rstat		, STAFF_CMD	, STAFF_GEN, 0 },
	{ "zstat"		, "zstat"		, POS_DEAD		, do_zstat		, STAFF_CMD	, STAFF_GEN, 0 },
	{ "zlist"		, "zlist"		, POS_DEAD		, do_zlist		, STAFF_CMD	, STAFF_GEN, 0 },
	{ "mlist"		, "mlist"		, POS_DEAD		, do_liblist	, STAFF_CMD	, STAFF_GEN, SCMD_MLIST },
	{ "olist"		, "olist"		, POS_DEAD		, do_liblist	, STAFF_CMD	, STAFF_GEN, SCMD_OLIST },
	{ "rlist"		, "rlist"		, POS_DEAD		, do_liblist	, STAFF_CMD	, STAFF_GEN, SCMD_RLIST },
	{ "tlist"		, "tlist"		, POS_DEAD		, do_liblist	, STAFF_CMD	, STAFF_GEN, SCMD_TLIST },
	{ "mmove"		, "mmove"		, POS_DEAD		, do_move_element, STAFF_CMD, STAFF_OLC, SCMD_MMOVE },
	{ "omove"		, "omove"		, POS_DEAD		, do_move_element, STAFF_CMD, STAFF_OLC, SCMD_OMOVE },
	{ "rmove"		, "rmove"		, POS_DEAD		, do_move_element, STAFF_CMD, STAFF_OLC, SCMD_RMOVE },
	{ "tmove"		, "tmove"		, POS_DEAD		, do_move_element, STAFF_CMD, STAFF_OLC, SCMD_TMOVE },

	{ "delete"		, "delete"		, POS_DEAD		, do_delete		, STAFF_CMD	, STAFF_OLC, SCMD_DELETE },
	{ "undelete"	, "undelete"	, POS_DEAD		, do_delete		, STAFF_CMD	, STAFF_OLC, SCMD_UNDELETE },

	{ "clanstat"	, "clanstat"	, POS_DEAD		, do_clanstat	, STAFF_CMD	, STAFF_CLANS, 0 },
	{ "forceenlist"	, "forceen"		, POS_DEAD		, do_forceenlist, STAFF_CMD	, STAFF_CLANS, 0 },

	{ "massroomsave", "massroomsave", POS_DEAD		, do_massroomsave, LVL_CODER, 0, 0 },
	
	{ "\n"			, "zzzzzzz"	,0, 0, 0, 0 }	//	This must be last
};


char *fill[] =
{
  "in"	,
  "from",
  "with",
  "the"	,
  "on"	,
  "at"	,
  "to"	,
  "\n"
};

char *reserved[] =
{
  "a",
  "an",
  "self",
  "me",
  "all",
  "room",
  "someone",
  "something",
  "\n"
};

/*
 * This is the actual command interpreter called from game_loop() in comm.c
 * It makes sure you are the proper level and position to execute the command,
 * then calls the appropriate function.
 */
void command_interpreter(CharData *ch, char *argument) {
	int cmd, length;
	char *line, *arg;

	REMOVE_BIT(AFF_FLAGS(ch), AFF_HIDE);

	skip_spaces(argument);
	
	if (!*argument || PURGED(ch))
		return;

	/*
	 * special case to handle one-character, non-alphanumeric commands;
	 * requested by many people so "'hi" or ";godnet test" is possible.
	 * Patch sent by Eric Green and Stefan Wasilewski.
	 */
	arg = get_buffer(MAX_INPUT_LENGTH);
	if (!isalpha(*argument)) {
		arg[0] = argument[0];
		arg[1] = '\0';
		line = argument + 1;
	} else
		line = any_one_arg(argument, arg);

	if (command_wtrigger(ch, arg, line) || command_mtrigger(ch, arg, line) || command_otrigger(ch, arg, line)) {
		release_buffer(arg);
		return; /* command trigger took over */
	}
	
	if (!no_specials && special(ch, arg, line)) {
		release_buffer(arg);
		return;
	}
	
	/* otherwise, find the command */
	for (length = strlen(arg), cmd = 0; *complete_cmd_info[cmd].command != '\n'; cmd++)
		if (!strncmp(complete_cmd_info[cmd].command, arg, length) && (length >= strlen(complete_cmd_info[cmd].sort_as)))
			if ((GET_LEVEL(ch) >= complete_cmd_info[cmd].minimum_level) && 
					(!IS_STAFFCMD(cmd) || (complete_cmd_info[cmd].staffcmd & STF_FLAGS(ch))) &&
					(!IS_NPCCMD(cmd) || IS_NPC(ch)))
				break;

	if (*complete_cmd_info[cmd].command == '\n') {
		if (!icec_command_hook(ch, arg, line))
			send_to_char("Huh?!?\r\n", ch);
	} else if (PLR_FLAGGED(ch, PLR_FROZEN) && (GET_LEVEL(ch) < LVL_CODER))
		send_to_char("You try, but the mind-numbing cold prevents you...\r\n", ch);
	else if (complete_cmd_info[cmd].command_pointer == NULL)
		send_to_char("Sorry, that command hasn't been implemented yet.\r\n", ch);
	else if ((!ch->desc || ch->desc->original) && IS_STAFFCMD(cmd))
		send_to_char("You can't use staff commands while switched.\r\n", ch);
//	else if (IS_NPC(ch) && complete_cmd_info[cmd].minimum_level >= LVL_IMMORT)
//		send_to_char("You can't use immortal commands while switched.\r\n", ch);
	else if (GET_POS(ch) < complete_cmd_info[cmd].minimum_position) {
		switch (GET_POS(ch)) {
			case POS_DEAD:
				send_to_char("Lie still; you are DEAD!!!\r\n", ch);
				break;
			case POS_INCAP:
			case POS_MORTALLYW:
				send_to_char("You are in a pretty bad shape, unable to do anything!\r\n", ch);
				break;
			case POS_STUNNED:
				send_to_char("All you can do right now is think about the stars!\r\n", ch);
				break;
			case POS_SLEEPING:
				send_to_char("In your dreams, or what?\r\n", ch);
				break;
			case POS_RESTING:
				send_to_char("Nah... You feel too relaxed to do that..\r\n", ch);
				break;
			case POS_SITTING:
				send_to_char("Maybe you should get on your feet first?\r\n", ch);
				break;
			case POS_FIGHTING:
				send_to_char("No way!  You're fighting for your life!\r\n", ch);
				break;
			default:
				break;	//	POS_STANDING... shouldn't reach here.
		}
	} else
		(*complete_cmd_info[cmd].command_pointer) (ch, line, cmd, arg, complete_cmd_info[cmd].subcmd);
	release_buffer(arg);
}

/**************************************************************************
 * Routines to handle aliasing                                             *
  **************************************************************************/


Alias *find_alias(LList<Alias *> &alias_list, char *str) {
//	while (alias_list) {
	Alias *a;
	START_ITER(iter, a, Alias *, alias_list) {
		if (*str == *a->command)
			if (!strcmp(str, a->command)) {
				END_ITER(iter);
				return a;
			}
	} END_ITER(iter);
//		alias_list = alias_list->next;
//	}
	return NULL;
}


Alias::Alias(char *arg, char *repl) : command(str_dup(arg)), replacement(str_dup(repl)){
//	this->command = str_dup(arg);
//	this->replacement = str_dup(repl);
	this->type = (strchr(repl, ALIAS_SEP_CHAR) || strchr(repl, ALIAS_VAR_CHAR)) ?
		ALIAS_COMPLEX : ALIAS_SIMPLE;
}


Alias::~Alias(void) {
	if (this->command)		FREE(this->command);
	if (this->replacement)	FREE(this->replacement);
}


/* The interface to the outside world: do_alias */
ACMD(do_alias) {
	char *repl, *arg;
	Alias *a;

	if (IS_NPC(ch))
		return;

	arg = get_buffer(MAX_INPUT_LENGTH);
	repl = any_one_arg(argument, arg);

	if (!*arg) {			/* no argument specified -- list currently defined aliases */
		send_to_char("Currently defined aliases:\r\n", ch);
		if (GET_ALIASES(ch).Count() == 0)
			send_to_char(" None.\r\n", ch);
		else {
//			char *buf = get_buffer(SMALL_BUFSIZE);
//			while (a) {
//				sprintf(buf, "%-15s %s\r\n", a->command, a->replacement);
//				send_to_char(buf, ch);
			START_ITER(iter, a, Alias *, GET_ALIASES(ch)) {
				ch->Send("%-15s %s\r\n", a->command, a->replacement);
			} END_ITER(iter);
//				a = a->next;
//			}
//	release_buffer(buf);
		}
	} else {			/* otherwise, add or remove aliases */
		/* is this an alias we've already defined? */
		if ((a = find_alias(GET_ALIASES(ch), arg)) != NULL) {
			GET_ALIASES(ch).Remove(a);
			delete a;
		}
		/* if no replacement string is specified, assume we want to delete */
		if (!*repl) {
			if (!a)	send_to_char("No such alias.\r\n", ch);
			else	send_to_char("Alias deleted.\r\n", ch);
		} else  if (!str_cmp(arg, "alias"))	/* otherwise, either add or redefine an alias */
			send_to_char("You can't alias 'alias'.\r\n", ch);
		else {
			delete_doubledollar(repl);
			a = new Alias(arg, repl);
			GET_ALIASES(ch).Add(a);
			send_to_char("Alias added.\r\n", ch);
		}
	}
	release_buffer(arg);
}

/*
 * Valid numeric replacements are only $1 .. $9 (makes parsing a little
 * easier, and it's not that much of a limitation anyway.)  Also valid
 * is "$*", which stands for the entire original line after the alias.
 * ";" is used to delimit commands.
 */
#define NUM_TOKENS       9

void perform_complex_alias(struct txt_q *input_q, char *orig, Alias *a) {
	struct txt_q temp_queue;
	char *tokens[NUM_TOKENS], *temp, *write_point,
		*buf2 = get_buffer(MAX_STRING_LENGTH),
		*buf = get_buffer(MAX_STRING_LENGTH);
	int num_of_tokens = 0, num;

	/* First, parse the original string */
	temp = strtok(strcpy(buf2, orig), " ");
	while (temp && (num_of_tokens < NUM_TOKENS)) {
		tokens[num_of_tokens++] = temp;
		temp = strtok(NULL, " ");
	}

	/* initialize */
	write_point = buf;
	temp_queue.head = temp_queue.tail = NULL;

	/* now parse the alias */
	for (temp = a->replacement; *temp; temp++) {
		if (*temp == ALIAS_SEP_CHAR) {
			*write_point = '\0';
			buf[MAX_INPUT_LENGTH - 1] = '\0';
			write_to_q(buf, &temp_queue, 1);
			write_point = buf;
		} else if (*temp == ALIAS_VAR_CHAR) {
			temp++;
			if ((num = *temp - '1') < num_of_tokens && num >= 0) {
				strcpy(write_point, tokens[num]);
				write_point += strlen(tokens[num]);
			} else if (*temp == ALIAS_GLOB_CHAR) {
				strcpy(write_point, orig);
				write_point += strlen(orig);
			} else if ((*(write_point++) = *temp) == '$')	/* redouble $ for act safety */
				*(write_point++) = '$';
		} else
			*(write_point++) = *temp;
	}

	*write_point = '\0';
	buf[MAX_INPUT_LENGTH - 1] = '\0';
	write_to_q(buf, &temp_queue, 1);

	/* push our temp_queue on to the _front_ of the input queue */
	if (input_q->head) {
		temp_queue.tail->next = input_q->head;
		input_q->head = temp_queue.head;
	} else
		*input_q = temp_queue;
		
	release_buffer(buf);
	release_buffer(buf2);
}


/*
 * Given a character and a string, perform alias replacement on it.
 *
 * Return values:
 *   0: String was modified in place; call command_interpreter immediately.
 *   1: String was _not_ modified in place; rather, the expanded aliases
 *      have been placed at the front of the character's input queue.
 */
int perform_alias(DescriptorData *d, char *orig) {
	char *first_arg, *ptr;
	Alias *a;
	
	/* bail out immediately if the guy doesn't have any aliases */
	if (IS_NPC(d->character) || !GET_ALIASES(d->character).Count())
		return 0;
    
	first_arg = get_buffer(MAX_INPUT_LENGTH);

	/* find the alias we're supposed to match */
	ptr = any_one_arg(orig, first_arg);

	/* bail out if it's null */
	if (!*first_arg) {
		release_buffer(first_arg);
		return 0;
	}

	/* if the first arg is not an alias, return without doing anything */
	if ((a = find_alias(GET_ALIASES(d->character), first_arg)) == NULL) {
		release_buffer(first_arg);
		return 0;
	}
	release_buffer(first_arg);

	if (a->type == ALIAS_SIMPLE) {
		strcpy(orig, a->replacement);
		return 0;
	} else {
		perform_complex_alias(&d->input, ptr, a);
		return 1;
	}
}



/***************************************************************************
 * Various other parsing utilities                                         *
 **************************************************************************/

/*
 * searches an array of strings for a target string.  "exact" can be
 * 0 or non-0, depending on whether or not the match must be exact for
 * it to be returned.  Returns -1 if not found; 0..n otherwise.  Array
 * must be terminated with a '\n' so it knows to stop searching.
 */
int search_block(const char *arg, char **list, bool exact) {
	SInt32		i, l;
	
	//	Make into lower case, and get length of string
//	for (l = 0; *(arg + l); l++)
//		*(arg + l) = LOWER(*(arg + l));
	l = strlen(arg);
	
	if (exact) {
		for (i = 0; **(list + i) != '\n'; i++)
			if (!str_cmp(arg, *(list + i)))
				return (i);
	} else {
		if (!l)
			l = 1;	//	Avoid "" to match the first available string
		for (i = 0; **(list + i) != '\n'; i++)
			if (!strn_cmp(arg, *(list + i), l))
				return (i);
	}
	
	return -1;
}


int search_chars(char arg, const char *list) {
	int i;

	for (i = 0; list[i] != '\n'; i++)
		if (arg == list[i])
			return (i);
	return -1;
}


/*
bool is_number(char *str) {
	if (!str || !*str)		return false;
	if (*str == '-')		str++;		
	while (*str)
		if (!isdigit(*(str++)))
			return false;
	return true;
}
*/
bool is_number(const char *str) {
	if (!str || !*str)				return false;
	if (*str == '-' || *str == '+')	str++;
	while (*str && isdigit(*str))	str++;
	if (!*str || isspace(*str))		return true;
	else							return false;
}


void skip_spaces(const char *&string) {
	while (*string && isspace(*string))	string++;
//	for (; *string && isspace(*string); string++);
}


char *delete_doubledollar(char *string) {
	char *read, *write;

	if ((write = strchr(string, '$')) == NULL)
		return string;

	read = write;

	while (*read)
		if ((*(write++) = *(read++)) == '$')
			if (*read == '$')
				read++;

	*write = '\0';

	return string;
}


bool fill_word(const char *argument) {
	return (search_block(argument, fill, TRUE) >= 0);
}


bool reserved_word(const char *argument) {
	return (search_block(argument, reserved, TRUE) >= 0);
}


/*
 * copy the first non-fill-word, space-delimited argument of 'argument'
 * to 'first_arg'; return a pointer to the remainder of the string.
 */
char *one_argument(const char *argument, char *first_arg) {
	char *begin = first_arg;
	
	if (!argument) {
		log("SYSERR: one_argument received a NULL pointer!");
		*first_arg = '\0';
		return NULL;
	}
	
	do {
		skip_spaces(argument);

		first_arg = begin;
		while (*argument && !isspace(*argument)) {
			*(first_arg++) = LOWER(*argument);
			argument++;
		}

		*first_arg = '\0';
	} while (fill_word(begin));

	skip_spaces(argument);
	
	return const_cast<char *>(argument);
}


/*
 * one_word is like one_argument, except that words in quotes ("") are
 * considered one word.
 */
char *one_word(const char *argument, char *first_arg)
{
  char *begin = first_arg;

	if (!argument) {
		log("SYSERR: one_argument received a NULL pointer.");
		*first_arg = '\0';
		return NULL;
	}

  do {
    skip_spaces(argument);

    first_arg = begin;

    if (*argument == '\"') {
      argument++;
      while (*argument && *argument != '\"') {
        *(first_arg++) = LOWER(*argument);
        argument++;
      }
      argument++;
    } else {
      while (*argument && !isspace(*argument)) {
        *(first_arg++) = LOWER(*argument);
        argument++;
      }
    }

    *first_arg = '\0';
  } while (fill_word(begin));

  return const_cast<char *>(argument);
}


/* same as one_argument except that it doesn't ignore fill words */
char *any_one_arg(const char *argument, char *first_arg)
{
  skip_spaces(argument);

  while (*argument && !isspace(*argument)) {
    *(first_arg++) = LOWER(*argument);
    argument++;
  }

  *first_arg = '\0';

  return const_cast<char *>(argument);
}



/* same as any_one_arg except that it stops at punctuation */
char *any_one_name(const char *argument, char *first_arg) {
    char* arg;
  
    /* Find first non blank */
//    while(isspace(*argument))
//		argument++;
	skip_spaces(argument);	
	  
    /* Find length of first word */
    for(arg = first_arg ;
			*argument && !isspace(*argument) &&
	  		(!ispunct(*argument) || *argument == '#' || *argument == '-');
			arg++, argument++)
		*arg = LOWER(*argument);
    *arg = '\0';

    return const_cast<char *>(argument);
}


/*
 * Same as one_argument except that it takes two args and returns the rest;
 * ignores fill words
 */
char *two_arguments(const char *argument, char *first_arg, char *second_arg) {
	return one_argument(one_argument(argument, first_arg), second_arg);
}


char *three_arguments(const char *argument, char *first_arg, char *second_arg, char *third_arg) {
	return one_argument(two_arguments(argument, first_arg, second_arg), third_arg);  
}


/*
 * determine if a given string is an abbreviation of another
 * returns 1 if arg1 is an abbreviation of arg2
 */
bool is_abbrev(const char *arg1, const char *arg2) {
	if (!*arg1 || !*arg2)
		return false;

	for (; *arg1 && *arg2; arg1++, arg2++)
		if (LOWER(*arg1) != LOWER(*arg2))
			return false;

	if (!*arg1)	return true;
	else		return false;
}

/* return first space-delimited token in arg1; remainder of string in arg2 */
void half_chop(const char *string, char *arg1, char *arg2)
{
  char *temp;

  temp = any_one_arg(string, arg1);
  skip_spaces(temp);
  strcpy(arg2, temp);
}



/* Used in specprocs, mostly.  (Exactly) matches "command" to cmd number */
int find_command(const char *command) {
	int cmd;

	for (cmd = 0; *complete_cmd_info[cmd].command != '\n'; cmd++)
		if (!strcmp(complete_cmd_info[cmd].command, command))
			return cmd;

	return -1;
}


bool special(CharData *ch, char *cmd, char *arg) {
	ObjData *i;
	CharData *k;
	int j;

	//	Room
	if (GET_ROOM_SPEC(IN_ROOM(ch)) != NULL)
		if (GET_ROOM_SPEC(IN_ROOM(ch)) (ch, world + IN_ROOM(ch), cmd, arg))
			return true;

	//	In EQ List
	for (j = 0; j < NUM_WEARS; j++)
		if (GET_EQ(ch, j) && GET_OBJ_SPEC(GET_EQ(ch, j)) != NULL)
			if (GET_OBJ_SPEC(GET_EQ(ch, j)) (ch, GET_EQ(ch, j), cmd, arg))
				return true;

	//	In Inventory
	START_ITER(obj_iter, i, ObjData *, ch->carrying) {
		if (GET_OBJ_SPEC(i) != NULL)
			if (GET_OBJ_SPEC(i) (ch, i, cmd, arg)) {
				END_ITER(obj_iter);
				return true;
			}
	} END_ITER(obj_iter);

	//	In Mobile
	START_ITER(mob_iter, k, CharData *, world[IN_ROOM(ch)].people) {
		if (GET_MOB_SPEC(k) != NULL)
			if (GET_MOB_SPEC(k) (ch, k, cmd, arg)) {
				END_ITER(mob_iter);
				return true;
			}
	} END_ITER(mob_iter);

	//	In Object present
	START_ITER(world_iter, i, ObjData *, world[IN_ROOM(ch)].contents) {
		if (GET_OBJ_SPEC(i) != NULL)
			if (GET_OBJ_SPEC(i) (ch, i, cmd, arg)) {
				END_ITER(world_iter);
				return true;
			}
	} END_ITER(world_iter);

	return false;
}



/* *************************************************************************
*  Stuff for controlling the non-playing sockets (get name, pwd etc)       *
************************************************************************* */


/* locate entry in p_table with entry->name == name. -1 mrks failed search */
int find_name(char *name)
{
  int i;

  for (i = 0; i <= top_of_p_table; i++) {
    if (!str_cmp((player_table + i)->name, name))
      return i;
  }

  return -1;
}


int _parse_name(char *arg, char *name)
{
  int i;

  /* skip whitespaces */
  for (; isspace(*arg); arg++);

  for (i = 0; (*name = *arg); arg++, i++, name++)
    if (!isalpha(*arg))
      return 1;

  if (!i)
    return 1;

  return 0;
}


#define RECON		1
#define USURP		2
#define UNSWITCH	3

int perform_dupe_check(DescriptorData *d) {
	DescriptorData *k; //, *next_k;
	CharData *target = NULL, *ch;
	int mode = 0;

	int id = GET_IDNUM(d->character);

	/*
	 * Now that this descriptor has successfully logged in, disconnect all
	 * other descriptors controlling a character with the same ID number.
	 */

	START_ITER(desc_iter, k, DescriptorData *, descriptor_list) {
		if (k == d)
			continue;

		if (k->original && (GET_IDNUM(k->original) == id)) {    /* switched char */
			SEND_TO_Q("\r\nMultiple login detected -- disconnecting.\r\n", k);
			STATE(k) = CON_CLOSE;
			if (!target) {
				target = k->original;
				mode = UNSWITCH;
			}
			if (k->character)
				k->character->desc = NULL;
			k->character = NULL;
			k->original = NULL;
		} else if (k->character && (GET_IDNUM(k->character) == id)) {
			if (!target && (STATE(k) == CON_PLAYING)) {
				SEND_TO_Q("\r\nThis body has been usurped!\r\n", k);
				target = k->character;
				mode = USURP;
			}
			k->character->desc = NULL;
			k->character = NULL;
			k->original = NULL;
			SEND_TO_Q("\r\nMultiple login detected -- disconnecting.\r\n", k);
			STATE(k) = CON_CLOSE;
		}
	} END_ITER(desc_iter);

	/*
	 * now, go through the character list, deleting all characters that
	 * are not already marked for deletion from the above step (i.e., in the
	 * CON_HANGUP state), and have not already been selected as a target for
	 * switching into.  In addition, if we haven't already found a target,
	 * choose one if one is available (while still deleting the other
	 * duplicates, though theoretically none should be able to exist).
	 */
	START_ITER(ch_iter, ch, CharData *, character_list) {
		if (IS_NPC(ch))				continue;
		if (GET_IDNUM(ch) != id)	continue;

		//	ignore chars with descriptors (already handled by above step)
		if (ch->desc)				continue;

		//	don't extract the target char we've found one already
		if (ch == target)			continue;

		//	we don't already have a target and found a candidate for switching
		if (!target) {
			target = ch;
			mode = RECON;
			continue;
		}

		//	we've found a duplicate - blow him away, dumping his eq in limbo.
		if (IN_ROOM(ch) != NOWHERE)
			ch->from_room();
		ch->to_room(1);
		ch->extract();
	} END_ITER(ch_iter);

	//	no target for swicthing into was found - allow login to continue
	if (!target)
		return 0;

	//	Okay, we've found a target.  Connect d to target.
	delete d->character; /* get rid of the old char */
	d->character = target;
	d->character->desc = d;
	d->original = NULL;
	d->character->player->timer = 0;
	REMOVE_BIT(PLR_FLAGS(d->character), PLR_MAILING | PLR_WRITING);
	STATE(d) = CON_PLAYING;

	switch (mode) {
		case RECON:
			SEND_TO_Q("Reconnecting.\r\n", d);
			act("$n has reconnected.", TRUE, d->character, 0, 0, TO_ROOM);
			mudlogf( NRM, LVL_IMMORT, TRUE,  "%s [%s] has reconnected.", d->character->RealName(), d->host);
			break;
		case USURP:
			SEND_TO_Q("You take over your own body, already in use!\r\n", d);
			act("$n suddenly keels over in pain, surrounded by a white aura...\r\n"
				"$n's body has been taken over by a new spirit!",
					TRUE, d->character, 0, 0, TO_ROOM);
			mudlogf(NRM, LVL_IMMORT, TRUE,
					"%s has re-logged in ... disconnecting old socket.",
					d->character->RealName());
			break;
	case UNSWITCH:
			SEND_TO_Q("Reconnecting to unswitched char.", d);
			mudlogf( NRM, LVL_IMMORT, TRUE,  "%s [%s] has reconnected.", d->character->RealName(), d->host);
			break;
	}

	return 1;
}


/* load the player, put them in the right room - used by copyover_recover too */
int enter_player_game (DescriptorData *d) {
	VNum load_room;
	int load_result;

	reset_char(d->character);
	if (PLR_FLAGGED(d->character, PLR_INVSTART))
		GET_INVIS_LEV(d->character) = 101;
	load_room = d->character->StartRoom();
	d->character->to_room(load_room);
	load_result = Crash_load(d->character);

	GET_ID(d->character) = GET_IDNUM(d->character);

	d->character->to_world();
	d->character->save(NOWHERE);
	return load_result;
}


/* deal with newcomers and other non-playing sockets */
void nanny(DescriptorData *d, char *arg) {
	char *buf, *tmp_name;
	int player_i, load_result;

	skip_spaces(arg);

	switch (STATE(d)) {
  
  /*. OLC states .*/
		case CON_OEDIT: 
			oedit_parse(d, arg);
			break;
		case CON_REDIT: 
			REdit::parse(d, arg);
			break;
		case CON_ZEDIT: 
			zedit_parse(d, arg);
			break;
		case CON_MEDIT: 
			medit_parse(d, arg);
			break;
		case CON_SEDIT: 
			sedit_parse(d, arg);
			break;
		case CON_AEDIT:
			aedit_parse(d, arg);
			break;
		case CON_HEDIT:
			hedit_parse(d, arg);
			break;
		case CON_SCRIPTEDIT:
			scriptedit_parse(d, arg);
			break;
		case CON_CEDIT:		/* Editing clans */
			cedit_parse(d, arg);
			break;
/*. End of OLC states .*/
		
		case CON_GET_NAME:		/* wait for input of name */
			if (!d->character) {
				d->character = new CharData;
				d->character->player = new PlayerData;
				d->character->desc = d;
			}
			if (!*arg)
				STATE(d) = CON_CLOSE;
			else {
				buf = get_buffer(128);
				tmp_name = get_buffer(MAX_INPUT_LENGTH);

				if ((_parse_name(arg, tmp_name)) || strlen(tmp_name) < 2 ||
						strlen(tmp_name) > MAX_NAME_LENGTH || // !Valid_Name(tmp_name) ||
						fill_word(strcpy(buf, tmp_name)) || reserved_word(buf))
					;	//	Bad name breaker
				else if ((player_i = d->character->load(tmp_name)) > -1) {
					//	Character loaded
					GET_PFILEPOS(d->character) = player_i;

					if (PLR_FLAGGED(d->character, PLR_DELETED)) {
						/* We get false positive from the original deleted name. */
						get_filename(d->character->RealName(), buf);
						remove(buf);
						
						SSFree(d->character->general.name);
						d->character->general.name = NULL;
						/* Check for multiple creations... */
						if (Valid_Name(tmp_name)) {
							delete d->character;
							d->character = new CharData;
							d->character->player = new PlayerData;
							d->character->desc = d;
							d->character->general.name = SSCreate(CAP(tmp_name));
							GET_PFILEPOS(d->character) = player_i;

							STATE(d) = CON_NAME_CNFRM;
						}
					} else {
/* undo it just in case they are set */
						REMOVE_BIT(PLR_FLAGS(d->character), PLR_WRITING | PLR_MAILING);

						SEND_TO_Q("Password: ", d);
						echo_off(d);
						d->idle_tics = 0;
						STATE(d) = CON_PASSWORD;
					}
				} else {	//	Player unknown... new characters
					SSFree(d->character->general.name);
					d->character->general.name = NULL;
					if (Valid_Name(tmp_name)) {
						d->character->general.name = SSCreate(CAP(tmp_name));
						STATE(d) = CON_NAME_CNFRM;
					}
				}
				
				if (STATE(d) == CON_GET_NAME)
						SEND_TO_Q("We won't accept that name, private!\r\n"
								  "Give me a better one: ", d);
				else if (STATE(d) == CON_NAME_CNFRM) {
					sprintf(buf, "Did I hear you correctly, '%s' (Y/N)? ", tmp_name);
					SEND_TO_Q(buf, d);
				}
				release_buffer(buf);
				release_buffer(tmp_name);
			}
			break;
			
		case CON_NAME_CNFRM:		/* wait for conf. of new name    */
			if (UPPER(*arg) == 'Y') {
				if (isbanned(d->host) >= BAN_NEW) {
//					SEND_TO_Q("Sorry, new characters are not allowed from your site!\r\n", d);
					SEND_TO_Q("We aren't accepting greenies from your neck of the woods!\r\n"
							  "(No new characters allowed from your site)", d);
					mudlogf(NRM, LVL_STAFF, TRUE, "Request for new char %s denied from [%s] (siteban)",
							d->character->RealName(), d->host);
					STATE(d) = CON_CLOSE;
					return;
				}
				if (circle_restrict) {
//					SEND_TO_Q("Sorry, new players can't be created at the moment.\r\n", d);
					SEND_TO_Q("We aren't accepting greenies at the moment!  Try again later.\r\n"
							  "(No new players can be created at the moment)", d);
					mudlogf(NRM, LVL_STAFF, TRUE, "Request for new char %s denied from [%s] (wizlock)",
							d->character->RealName(), d->host);
					STATE(d) = CON_CLOSE;
					return;
				}
				SEND_TO_Q("Welcome to corps, private!\r\n", d);

				buf = get_buffer(128);
				sprintf(buf, "Now give me a password, '%s': ", d->character->RealName());
				SEND_TO_Q(buf, d);
				release_buffer(buf);
				
				echo_off(d);
				STATE(d) = CON_NEWPASSWD;
			} else if (*arg == 'n' || *arg == 'N') {
				SEND_TO_Q("Okay, what IS it, then? ", d);
				SSFree(d->character->general.name);
				d->character->general.name = NULL;
				STATE(d) = CON_GET_NAME;
			} else {
				SEND_TO_Q("I won't take that for an answer!\r\n"
						  "Give me a 'Yes' or a 'No': ", d);
			}
			break;
		case CON_PASSWORD:		/* get pwd for known player      */
		/*
		 * To really prevent duping correctly, the player's record should
		 * be reloaded from disk at this point (after the password has been
		 * typed).  However I'm afraid that trying to load a character over
		 * an already loaded character is going to cause some problem down the
		 * road that I can't see at the moment.  So to compensate, I'm going to
		 * (1) add a 15 or 20-second time limit for entering a password, and (2)
		 * re-add the code to cut off duplicates when a player quits.  JE 6 Feb 96
		 */

			echo_on(d);    /* turn echo back on */

			if (!*arg)
				STATE(d) = CON_CLOSE;
			else {
				if (strncmp(CRYPT(arg, GET_PASSWD(d->character)), GET_PASSWD(d->character), MAX_PWD_LENGTH)) {
					mudlogf( BRF, LVL_STAFF, TRUE,  "Bad PW: %s [%s]", d->character->RealName(), d->host);
					GET_BAD_PWS(d->character)++;
					d->character->save(NOWHERE);
					if (++(d->bad_pws) >= max_bad_pws) {	/* 3 strikes and you're out. */
						SEND_TO_Q("Wrong password... disconnecting.\r\n", d);
						STATE(d) = CON_CLOSE;
					} else {
						SEND_TO_Q("Wrong password.\r\nPassword: ", d);
						echo_off(d);
					}
					return;
				}
				load_result = GET_BAD_PWS(d->character);
				GET_BAD_PWS(d->character) = 0;

				if (isbanned(d->host) == BAN_SELECT && !PLR_FLAGGED(d->character, PLR_SITEOK)) {
					SEND_TO_Q("Sorry, this char has not been cleared for login from your site!\r\n", d);
					STATE(d) = CON_CLOSE;
					mudlogf(NRM, LVL_STAFF, TRUE, "Connection attempt for %s denied from %s",
							d->character->RealName(), d->host);
					return;
				}
				if (GET_LEVEL(d->character) < circle_restrict) {
					SEND_TO_Q("The game is temporarily restricted.. try again later.\r\n", d);
					STATE(d) = CON_CLOSE;
					mudlogf(NRM, LVL_STAFF, TRUE, "Request for login denied for %s [%s] (wizlock)",
							d->character->RealName(), d->host);
					return;
				}
      /* check and make sure no other copies of this player are logged in */
				if (perform_dupe_check(d))
					return;

				d->character->player->time.logon = time(0);	// Set the last logon time to now.

				if (IS_STAFF(d->character))		SEND_TO_Q(imotd, d);
				else							SEND_TO_Q(motd, d);

				mudlogf( BRF, LVL_IMMORT, TRUE, 
						"%s [%s] has connected.", d->character->RealName(), d->host);

				if (load_result) {
					buf = get_buffer(128);
					sprintf(buf, "\r\n\r\n\007\007\007"
							"`R%d LOGIN FAILURE%s SINCE LAST SUCCESSFUL LOGIN.`n\r\n",
							load_result, (load_result > 1) ? "S" : "");
					SEND_TO_Q(buf, d);
					release_buffer(buf);

					GET_BAD_PWS(d->character) = 0;
				}
				SEND_TO_Q("\r\n\n*** FIRE WHEN READY: ", d);
				STATE(d) = CON_RMOTD;
			}
			break;

		case CON_NEWPASSWD:
		case CON_CHPWD_GETNEW:
			if (!*arg || strlen(arg) > MAX_PWD_LENGTH || strlen(arg) < 3 || !str_cmp(arg, d->character->RealName())) {
				SEND_TO_Q("\r\nIllegal password.\r\n", d);
				SEND_TO_Q("Password: ", d);
				break;
			}
			strncpy(GET_PASSWD(d->character), CRYPT(arg, d->character->RealName()), MAX_PWD_LENGTH);
			*(GET_PASSWD(d->character) + MAX_PWD_LENGTH) = '\0';

			SEND_TO_Q("\r\nPlease retype password: ", d);
			
			STATE(d) = (STATE(d) == CON_NEWPASSWD) ? CON_CNFPASSWD : CON_CHPWD_VRFY;

			break;

		case CON_CNFPASSWD:
		case CON_CHPWD_VRFY:
			if (strncmp(CRYPT(arg, GET_PASSWD(d->character)), GET_PASSWD(d->character), MAX_PWD_LENGTH)) {
				SEND_TO_Q("\r\nPasswords don't match... start over.\r\n", d);
				SEND_TO_Q("Password: ", d);
				STATE(d) = (STATE(d) == CON_CNFPASSWD) ? CON_NEWPASSWD : CON_CHPWD_GETNEW;
			}
			echo_on(d);

			if (STATE(d) == CON_CNFPASSWD) {
				SEND_TO_Q(race_menu, d);
				SEND_TO_Q("Race: ", d);
				STATE(d) = CON_QRACE;
			} else {
				d->character->save(NOWHERE);
//				echo_on(d);		//	Duplicate
				SEND_TO_Q("\r\nDone.\r\n", d);
				SEND_TO_Q(MENU, d);
				STATE(d) = CON_MENU;
			}

			break;

		case CON_QSEX:		/* query sex of new user         */
			switch (*arg) {
				case 'm':
				case 'M':
					d->character->general.sex = SEX_MALE;
					break;
				case 'f':
				case 'F':
					d->character->general.sex = SEX_FEMALE;
					break;
				case 'n':
				case 'N':
					if (d->character->general.race == RACE_ALIEN) {
						d->character->general.sex = SEX_NEUTRAL;
						break;
					}
				default:
					SEND_TO_Q("That is NOT a sex..\r\n"
						"Try again: ", d);
					return;
			}
			
//			if (d->character->general.sex == SEX_NEUTRAL)
//				return;
				
			roll_real_abils(d->character);
			
			buf = get_buffer(MAX_INPUT_LENGTH);
			sprintf(buf,"\r\n\nYour stats are:\r\n"
					"      Current  Racial\r\n"
					"         Stat  Max\r\n"
					"     Str: %3d  %3d\r\n"
					"     Int: %3d  %3d\r\n"
					"     Per: %3d  %3d\r\n"
					"     Agi: %3d  %3d\r\n"
					"     Con: %3d  %3d\r\n"
					"Is this acceptable? ",
					GET_STR(d->character), stat_max[GET_RACE(d->character)][0],
					GET_INT(d->character), stat_max[GET_RACE(d->character)][1],
					GET_PER(d->character), stat_max[GET_RACE(d->character)][2],
					GET_AGI(d->character), stat_max[GET_RACE(d->character)][3],
					GET_CON(d->character), stat_max[GET_RACE(d->character)][4]);
			SEND_TO_Q(buf, d);
			release_buffer(buf);
			
			STATE(d) = CON_STAT_CNFRM;
			break;
			
		case CON_QRACE:
			load_result = parse_race(*arg);
			if (load_result == RACE_UNDEFINED) {
				SEND_TO_Q("\r\nThat's not a race.\r\nRace: ", d);
				break;
			}
			GET_RACE(d->character) = (Race)load_result;

			if (GET_RACE(d->character) == RACE_ALIEN)
				SEND_TO_Q("What is your sex (M/F/N)? ", d);
			else
				SEND_TO_Q("What is your sex (M/F)? ", d);
			STATE(d) = CON_QSEX;
			break;


		case CON_STAT_CNFRM:
			switch(*arg) {
				case 'Y':
				case 'y':
					init_char(d->character);

//					d->character->save(NOWHERE);
					SEND_TO_Q(motd, d);
					SEND_TO_Q("\r\n\n*** FIRE WHEN READY: ", d);
					STATE(d) = CON_RMOTD;

					mudlogf( NRM, LVL_IMMORT, TRUE,  "%s [%s] new player.", d->character->RealName(), d->host);
					break;
				default:
					roll_real_abils(d->character);
					
					buf = get_buffer(MAX_INPUT_LENGTH);
					sprintf(buf,"\r\n\nYour stats are:\r\n"
							"      Current  Racial\r\n"
							"         Stat  Max\r\n"
							"     Str: %3d  %3d\r\n"
							"     Int: %3d  %3d\r\n"
							"     Per: %3d  %3d\r\n"
							"     Agi: %3d  %3d\r\n"
							"     Con: %3d  %3d\r\n"
							"Is this acceptable? ",
							GET_STR(d->character), stat_max[GET_RACE(d->character)][0],
							GET_INT(d->character), stat_max[GET_RACE(d->character)][1],
							GET_PER(d->character), stat_max[GET_RACE(d->character)][2],
							GET_AGI(d->character), stat_max[GET_RACE(d->character)][3],
							GET_CON(d->character), stat_max[GET_RACE(d->character)][4]);
					SEND_TO_Q(buf, d);
					release_buffer(buf);
			}
			break;

		case CON_RMOTD:		/* read CR after printing motd   */
			SEND_TO_Q(MENU, d);
			STATE(d) = CON_MENU;
			break;

		case CON_MENU:		/* get selection from main menu  */
			switch (*arg) {
				case '0':
					SEND_TO_Q("Goodbye.\r\n", d);
					STATE(d) = CON_CLOSE;
					break;

				case '1':
					load_result = enter_player_game(d);
					send_to_char(WELC_MESSG, d->character);

					act("$n has entered the game.", TRUE, d->character, 0, 0, TO_ROOM);

					STATE(d) = CON_PLAYING;
					if (!GET_LEVEL(d->character)) {
						do_start(d->character);
						send_to_char(START_MESSG, d->character);
					}
					look_at_room(d->character, 0);
					if (has_mail(GET_IDNUM(d->character)))
						send_to_char("You have mail waiting.\r\n", d->character);
					if (load_result == 2) {	/* rented items lost */
						send_to_char(	"\r\n\007You could not afford your rent!\r\n"
										"Your possesions have been donated to the Salvation Army!\r\n",
								d->character);
						}
					d->has_prompt = 0;
					break;

				case '2':
					SEND_TO_Q("Enter the text you'd like others to see when they look at you.\r\n", d);
					SEND_TO_Q("(/s saves /h for help)\r\n", d);
					if (SSData(d->character->general.description)) {
						SEND_TO_Q("Current description:\r\n", d);
						SEND_TO_Q(SSData(d->character->general.description), d);
						d->backstr = str_dup(SSData(d->character->general.description));
					} else {
						d->character->general.description = SSCreate("");
					}
					d->str = &d->character->general.description->str;
					d->max_str = EXDSCR_LENGTH;
					STATE(d) = CON_EXDESC;
					break;

				case '3':
					page_string(d, background, 0);
					STATE(d) = CON_RMOTD;
					break;

				case '4':
					SEND_TO_Q("\r\nEnter your old password: ", d);
					echo_off(d);
					STATE(d) = CON_CHPWD_GETOLD;
					break;

				case '5':
					SEND_TO_Q("\r\nEnter your password for verification: ", d);
					echo_off(d);
					STATE(d) = CON_DELCNF1;
					break;

				default:
					SEND_TO_Q("\r\nThat's not a menu choice!\r\n", d);
					SEND_TO_Q(MENU, d);
					break;
			}

			break;

		case CON_CHPWD_GETOLD:
			if (strncmp(CRYPT(arg, GET_PASSWD(d->character)), GET_PASSWD(d->character), MAX_PWD_LENGTH)) {
				echo_on(d);
				SEND_TO_Q("\r\nIncorrect password.\r\n", d);
				SEND_TO_Q(MENU, d);
				STATE(d) = CON_MENU;
			} else {
				SEND_TO_Q("\r\nEnter a new password: ", d);
				STATE(d) = CON_CHPWD_GETNEW;
			}
			break;

		case CON_DELCNF1:
			echo_on(d);
			if (strncmp(CRYPT(arg, GET_PASSWD(d->character)), GET_PASSWD(d->character), MAX_PWD_LENGTH)) {
				SEND_TO_Q("\r\nIncorrect password.\r\n", d);
				SEND_TO_Q(MENU, d);
				STATE(d) = CON_MENU;
			} else {
				SEND_TO_Q(	"\r\nYOU ARE ABOUT TO DELETE THIS CHARACTER PERMANENTLY.\r\n"
							"ARE YOU ABSOLUTELY SURE?\r\n\r\n"
							"Please type \"yes\" to confirm: ", d);
				STATE(d) = CON_DELCNF2;
			}
			break;

		case CON_DELCNF2:
			if (!str_cmp(arg, "yes")) {
				if (PLR_FLAGGED(d->character, PLR_FROZEN)) {
					SEND_TO_Q("You try to kill yourself, but the ice stops you.\r\n", d);
					SEND_TO_Q("Character not deleted.\r\n\r\n", d);
					STATE(d) = CON_CLOSE;
				} else {
					if (GET_LEVEL(d->character) < LVL_ADMIN)
						SET_BIT(PLR_FLAGS(d->character), PLR_DELETED);
					d->character->save(NOWHERE);
					
					buf = get_buffer(128);
					sprintf(buf,	"Character '%s' deleted!\r\n"
									"Goodbye.\r\n", d->character->RealName());
					SEND_TO_Q(buf, d);
					
					get_filename(d->character->RealName(), buf);
					remove(buf);
					
					release_buffer(buf);
					
					mudlogf(NRM, LVL_STAFF, TRUE, "%s (lev %d) has self-deleted.",
							d->character->RealName(), GET_LEVEL(d->character));
					STATE(d) = CON_CLOSE;
				}
			} else {
				SEND_TO_Q("\r\nCharacter not deleted.\r\n", d);
				SEND_TO_Q(MENU, d);
				STATE(d) = CON_MENU;
			}
			break;
			
// Taken care of in game_loop()
		case CON_CLOSE:
//			close_socket(d);
			break;
			
		case CON_IDENT:
			//	Idle on the player while we lookup their IP.  No input accepted.  Hah!
			SEND_TO_Q("Your domain is being looked up, please be patient.\r\n", d);
			break;

		default:
			log("SYSERR: Nanny: illegal state of con'ness (%d) for '%s'; closing connection.", STATE(d), d->character ? d->character->RealName() : "<unknown>");
			STATE(d) = CON_DISCONNECT;
			break;
	}
}


