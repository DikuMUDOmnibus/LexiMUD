/* ************************************************************************
*   File: act.other.c                                   Part of CircleMUD *
*  Usage: Miscellaneous player-level commands                             *
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
#include "interpreter.h"
#include "handler.h"
#include "find.h"
#include "db.h"
#include "spells.h"
#include "house.h"
#include "affects.h"

#include <sys/stat.h>

/* extern variables */
extern RoomData *world;
extern RoomData *world;
extern struct skill_info_type skill_info[];
extern IndexData *mob_index;
extern int pt_allowed;
extern int nameserver_is_slow;
extern int max_filesize;
extern char *dirs[];
extern char *race_abbrevs[];
extern char * dir_text_2[];

/* extern procedures */
SPECIAL(shop_keeper);
void die(CharData * ch, CharData * killer);
void Crash_rentsave(CharData * ch);
ACMD(do_gen_comm);
void list_skills(CharData * ch);
void appear(CharData * ch);
void perform_immort_vis(CharData *ch);
int command_mtrigger(CharData *actor, char *cmd, char *argument);
int command_otrigger(CharData *actor, char *cmd, char *argument);
int command_wtrigger(CharData *actor, char *cmd, char *argument);

/* prototypes */
int perform_group(CharData *ch, CharData *vict);
void print_group(CharData *ch);
ACMD(do_quit);
ACMD(do_save);
ACMD(do_not_here);
ACMD(do_sneak);
ACMD(do_hide);
ACMD(do_practice);
ACMD(do_visible);
ACMD(do_title);
ACMD(do_group);
ACMD(do_ungroup);
ACMD(do_report);
ACMD(do_use);
ACMD(do_wimpy);
ACMD(do_display);
ACMD(do_gen_write);
ACMD(do_gen_tog);
ACMD(do_afk);
ACMD(do_watch);
ACMD(do_promote);
ACMD(do_prompt);



ACMD(do_quit) {
	VNum save_room;
	DescriptorData *d;

	if (IS_NPC(ch) || !ch->desc)
		return;

	if (GET_POS(ch) == POS_FIGHTING)
		send_to_char("No way!  You're fighting for your life!\r\n", ch);
//	else if (!NO_STAFF_HASSLE(ch) && !ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL))
//		send_to_char("You can't quit in no-man's land.  Get back to a safe place first!\r\n", ch);
	else {
		if (GET_POS(ch) <= POS_STUNNED) {
			send_to_char("You die before your time...\r\n", ch);
			ch->die(NULL);
		}
//		if (!GET_INVIS_LEV(ch))
			act("$n has left the game.", TRUE, ch, 0, 0, TO_ROOM);
		mudlogf( NRM, LVL_STAFF, TRUE,  "%s has quit the game.", ch->GetName());
		send_to_char("Go ahead, return to real life for now... you'll be back soon enough!\r\n", ch);
		/*
		 * kill off all sockets connected to the same player as the one who is
		 * trying to quit.  Helps to maintain sanity as well as prevent duping.
		 */
		START_ITER(iter, d, DescriptorData *, descriptor_list) {
			if (d == ch->desc)
				continue;
			if (d->character && (GET_IDNUM(d->character) == GET_IDNUM(ch)))
				STATE(d) = CON_DISCONNECT;
		} END_ITER(iter);
		save_room = IN_ROOM(ch);
		Crash_rentsave(ch);
		ch->extract();		/* Char is saved in extract char */

		/* If someone is quitting in their house, let them load back here */
		if (ROOM_FLAGGED(save_room, ROOM_HOUSE))
			ch->save(save_room);
	}
}



ACMD(do_save) {
	if (IS_NPC(ch) || !ch->desc)
		return;

	if (command) {
		act("Saving $n.\r\n", FALSE, ch, 0, 0, TO_CHAR);
	}
	ch->save(NOWHERE);
	Crash_crashsave(ch);
	if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_HOUSE_CRASH))
		House_crashsave(world[IN_ROOM(ch)].number);
}


/* generic function for commands which are normally overridden by
   special procedures - i.e., shop commands, mail commands, etc. */
ACMD(do_not_here) {
	send_to_char("Sorry, but you cannot do that here!\r\n", ch);
}



ACMD(do_sneak) {
//	struct affected_type af;
	SInt8 percent;

	send_to_char("Okay, you'll try to move silently for a while.\r\n", ch);
//	if (AFF_FLAGGED(ch, AFF_SNEAK))
//		affect_from_char(ch, Affect_Sneak);

	percent = Number(1, 101);	/* 101% is a complete failure */

	if (percent > GET_SKILL(ch, SKILL_SNEAK) + (GET_AGI(ch) / 4))
		return;

/*
	af.type = SKILL_SNEAK;
	af.duration = GET_LEVEL(ch);
	af.modifier = 0;
	af.location = APPLY_NONE;
	af.bitvector = AFF_SNEAK;
	affect_to_char(ch, &af);
*/
}



ACMD(do_hide) {
	UInt8 percent;

	send_to_char("You attempt to hide yourself.\r\n", ch);

	if (AFF_FLAGGED(ch, AFF_HIDE))
		REMOVE_BIT(AFF_FLAGS(ch), AFF_HIDE);

	percent = Number(1, 101);	/* 101% is a complete failure */

	if (percent > (GET_SKILL(ch, SKILL_HIDE) + (GET_AGI(ch) / 4)))
		return;

	SET_BIT(AFF_FLAGS(ch), AFF_HIDE);
}



ACMD(do_practice) {
	char * arg = get_buffer(MAX_INPUT_LENGTH);
	one_argument(argument, arg);

	if (*arg)
		send_to_char("You can only practice skills in your guild.\r\n", ch);
	else
		list_skills(ch);
	
	release_buffer(arg);
}



ACMD(do_visible) {
	if (IS_STAFF(ch)) {
		perform_immort_vis(ch);
		return;
	}

	if AFF_FLAGGED(ch, AFF_INVISIBLE) {
		appear(ch);
		send_to_char("You become visible.\r\n", ch);
		act("The air shimmers as $n becomes visible.", FALSE, ch, 0, 0, TO_ROOM);
	} else if (IS_PREDATOR(ch)) {
		if (FIGHTING(ch))
			send_to_char("Not while fighting, dufus!\r\n", ch);
		else {
			act("$n slowly fades out of sight.", FALSE, ch, 0, 0, TO_ROOM);
			send_to_char("You fade from vision.\r\n", ch);
			SET_BIT(AFF_FLAGS(ch), AFF_INVISIBLE);
		}
	} else
	send_to_char("You are already visible.\r\n", ch);
}



ACMD(do_title) {
	char *buf = get_buffer(MAX_INPUT_LENGTH);

	skip_spaces(argument);
	strcpy(buf, argument);
	delete_doubledollar(buf);

	if (IS_NPC(ch))
		send_to_char("Your title is fine... go away.\r\n", ch);
	else if (PLR_FLAGGED(ch, PLR_NOTITLE))
		send_to_char("You can't title yourself -- you shouldn't have abused it!\r\n", ch);
	else if (strstr(argument, "(") || strstr(argument, ")"))
		send_to_char("Titles can't contain the ( or ) characters.\r\n", ch);
	else if (strlen(argument) > MAX_TITLE_LENGTH) {
		ch->Send("Sorry, titles can't be longer than %d characters.\r\n", MAX_TITLE_LENGTH);
	} else {
		ch->set_title(buf);
		act("Okay, you're now $n $T.", FALSE, ch, 0, GET_TITLE(ch), TO_CHAR);
	}
	release_buffer(buf);
}


int perform_group(CharData *ch, CharData *vict) {
	if (AFF_FLAGGED(vict, AFF_GROUP) || !ch->CanSee(vict))
		return 0;

	SET_BIT(AFF_FLAGS(vict), AFF_GROUP);
	if (ch != vict)
		act("$N is now a member of your group.", FALSE, ch, 0, vict, TO_CHAR);
	act("You are now a member of $n's group.", FALSE, ch, 0, vict, TO_VICT);
	act("$N is now a member of $n's group.", FALSE, ch, 0, vict, TO_NOTVICT);
	return 1;
}


void print_group(CharData *ch) {
	CharData *k, *follower;

	if (!AFF_FLAGGED(ch, AFF_GROUP))
		send_to_char("But you are not the member of a group!\r\n", ch);
	else {
		char *buf = get_buffer(MAX_STRING_LENGTH);
		send_to_char("Your group consists of:\r\n", ch);

		k = (ch->master ? ch->master : ch);

		if (AFF_FLAGGED(k, AFF_GROUP)) {
			sprintf(buf, "     [%3dH %3dV] [%2d %s] $N (Head of group)",
					GET_HIT(k), GET_MOVE(k), GET_LEVEL(k), RACE_ABBR(k));
			act(buf, FALSE, ch, 0, k, TO_CHAR);
		}

		START_ITER(iter, follower, CharData *, k->followers) {
			if (!AFF_FLAGGED(follower, AFF_GROUP))
				continue;

			sprintf(buf, "     [%3dH %3dV] [%2d %s] $N", GET_HIT(follower),
					GET_MOVE(follower), GET_LEVEL(follower), RACE_ABBR(follower));
			act(buf, FALSE, ch, 0, follower, TO_CHAR);
		} END_ITER(iter);
		release_buffer(buf);
	}
}



ACMD(do_group) {
	CharData *vict, *follower;
	int found = 0;
	char *buf = get_buffer(MAX_INPUT_LENGTH);
	one_argument(argument, buf);

	if (!*buf)
		print_group(ch);
	else if (ch->master)
		act("You can not enroll group members without being head of a group.", FALSE, ch, 0, 0, TO_CHAR);
	else if (!str_cmp(buf, "all")) {
		perform_group(ch, ch);
		START_ITER(iter, follower, CharData *, ch->followers) {
			found += perform_group(ch, follower);
		} END_ITER(iter);
		if (!found)
			send_to_char("Everyone following you is already in your group.\r\n", ch);
	} else if (!(vict = get_char_room_vis(ch, buf)))
		send_to_char(NOPERSON, ch);
	else if ((vict->master != ch) && (vict != ch))
		act("$N must follow you to enter your group.", FALSE, ch, 0, vict, TO_CHAR);
	else if (!AFF_FLAGGED(vict, AFF_GROUP))
		perform_group(ch, vict);
	else if (ch == vict) {
		act("You disband the group.", FALSE, ch, 0, 0, TO_CHAR);
		act("$n disbands the group.", FALSE, ch, 0, 0, TO_ROOM);
		ch->die_follower();
		REMOVE_BIT(AFF_FLAGS(ch), AFF_GROUP);
	} else {
		act("$N is no longer a member of your group.", FALSE, ch, 0, vict, TO_CHAR);
		act("You have been kicked out of $n's group!", FALSE, ch, 0, vict, TO_VICT);
		act("$N has been kicked out of $n's group!", FALSE, ch, 0, vict, TO_NOTVICT);
		REMOVE_BIT(AFF_FLAGS(vict), AFF_GROUP);
	}
	release_buffer(buf);
}



ACMD(do_ungroup) {
	CharData *tch, *follower;
	char *buf = get_buffer(MAX_INPUT_LENGTH);

	one_argument(argument, buf);

	if (!*buf) {
		if (ch->master || !(AFF_FLAGGED(ch, AFF_GROUP))) {
			send_to_char("But you lead no group!\r\n", ch);
		} else {
			sprintf(buf, "%s has disbanded the group.\r\n", ch->GetName());
			START_ITER(iter, follower, CharData *, ch->followers) {
				if (AFF_FLAGGED(follower, AFF_GROUP)) {
					REMOVE_BIT(AFF_FLAGS(follower), AFF_GROUP);
					send_to_char(buf, follower);
					if (!AFF_FLAGGED(follower, AFF_CHARM))
						follower->stop_follower();
				}
			} END_ITER(iter);

			REMOVE_BIT(AFF_FLAGS(ch), AFF_GROUP);
			send_to_char("You disband the group.\r\n", ch);
		}
	} else if (!(tch = get_char_room_vis(ch, buf)))
		send_to_char("There is no such person!\r\n", ch);
	else if (tch->master != ch)
		send_to_char("That person is not following you!\r\n", ch);
	else if (!AFF_FLAGGED(tch, AFF_GROUP))
		send_to_char("That person isn't in your group.\r\n", ch);
	else {
		REMOVE_BIT(AFF_FLAGS(tch), AFF_GROUP);

		act("$N is no longer a member of your group.", FALSE, ch, 0, tch, TO_CHAR);
		act("You have been kicked out of $n's group!", FALSE, ch, 0, tch, TO_VICT);
		act("$N has been kicked out of $n's group!", FALSE, ch, 0, tch, TO_NOTVICT);

		if (!AFF_FLAGGED(tch, AFF_CHARM))
			tch->stop_follower();
	}
	release_buffer(buf);
}



ACMD(do_report) {
	CharData *k, *follower;
	char *buf;
	
	if (!AFF_FLAGGED(ch, AFF_GROUP)) {
		send_to_char("But you are not a member of any group!\r\n", ch);
		return;
	}
	
	buf = get_buffer(128);
	
	sprintf(buf, "%s reports: %d/%dH, %d/%dV\r\n",
			ch->GetName(), GET_HIT(ch), GET_MAX_HIT(ch),
			GET_MOVE(ch), GET_MAX_MOVE(ch));

	CAP(buf);

	k = (ch->master ? ch->master : ch);

	START_ITER(iter, follower, CharData *, k->followers) {
		if (AFF_FLAGGED(follower, AFF_GROUP) && (follower != ch))
			send_to_char(buf, follower);
	} END_ITER(iter);
	if (k != ch)
		send_to_char(buf, k);
	send_to_char("You report to the group.\r\n", ch);
	release_buffer(buf);
}


#if 0
ACMD(do_use) {
	ObjData *mag_item;
	char *arg	=	get_buffer(MAX_INPUT_LENGTH),
		*buf = get_buffer(MAX_INPUT_LENGTH);
		
	half_chop(argument, arg, buf);
	if (!*arg) {
		ch->Send("What do you want to %s?\r\n", CMD_NAME);
		release_buffer(arg);
		release_buffer(buf);
		return;
	}
	mag_item = GET_EQ(ch, WEAR_HOLD);

	if (!mag_item || !isname(arg, mag_item->name)) {
		switch (subcmd) {
			case SCMD_USE:
				ch->Send("You don't seem to be holding %s %s.\r\n", AN(arg), arg);
				release_buffer(arg);
				release_buffer(buf);
				return;
			default:
				log("SYSERR: Unknown subcmd %d passed to do_use", subcmd);
				release_buffer(arg);
				release_buffer(buf);
				return;
		}
  	}
	
	switch (subcmd) {
		case SCMD_USE:
			send_to_char("You can't seem to figure out how to use it.\r\n", ch);
			break;
	}
	release_buffer(arg);
	release_buffer(buf);
}
#endif


ACMD(do_wimpy) {
	int wimp_lev;
	char * arg;
	
	if (IS_NPC(ch))
		return;
	
	arg = get_buffer(MAX_INPUT_LENGTH);
	
	one_argument(argument, arg);

	if (!*arg) {
		if (GET_WIMP_LEV(ch))
			ch->Send("Your current wimp level is %d hit points.\r\n", GET_WIMP_LEV(ch));
		else
			ch->Send("At the moment, you're not a wimp.  (sure, sure...)\r\n");
	} else if (isdigit(*arg)) {
		if ((wimp_lev = atoi(arg))) {
			if (wimp_lev < 0)
				send_to_char("Heh, heh, heh.. we are jolly funny today, eh?\r\n", ch);
			else if (wimp_lev > GET_MAX_HIT(ch))
				send_to_char("That doesn't make much sense, now does it?\r\n", ch);
			else if (wimp_lev > (GET_MAX_HIT(ch) / 2))
				send_to_char("You can't set your wimp level above half your hit points.\r\n", ch);
			else {
				ch->Send("Okay, you'll wimp out if you drop below %d hit points.\r\n", wimp_lev);
				GET_WIMP_LEV(ch) = wimp_lev;
			}
		} else {
			ch->Send("Okay, you'll now tough out fights to the bitter end.\r\n");
			GET_WIMP_LEV(ch) = 0;
		}
	} else
		ch->Send("Specify at how many hit points you want to wimp out at.  (0 to disable)\r\n");
	release_buffer(arg);
}


ACMD(do_display) {
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	int i, x;

	const char *def_prompts[][2] = {
		{ "Stock Circle"				, "`w%hhp %vmv>`n"					},
		{ "Colorized Standard Circle"	, "`R%h`rhp `G%v`gmv`K>`n"			},
		{ "Standard"					, "`R%ph`rhp `G%pv`gmv`K>`n"		},
		{ "Full Featured"				,
				"`cOpponent`K: `B%o `W/ `cTank`K: `B%t%_"
				"`r%h`K(`R%H`K)`whitp `g%v`K(`G%V`K)`wmove`n>"				},
		{ "\n"							, "\n"								}
	};

	one_argument(argument, arg);

	if (!arg || !*arg) {
		send_to_char("The following pre-set prompts are availible...\r\n", ch);
		for (i = 0; *def_prompts[i][0] != '\n'; i++)
			ch->Send("  %d. %-25s  %s\r\n", i, def_prompts[i][0], def_prompts[i][1]);
		send_to_char(	"Usage: display <number>\r\n"
						"To create your own prompt, use \"prompt <str>\".\r\n", ch);
	} else if (!isdigit(*arg))
		send_to_char(	"Usage: display <number>\r\n"
						"Type \"display\" without arguments for a list of preset prompts.\r\n", ch);
	else {

		i = atoi(arg);
	
	
		if (i < 0) {
			send_to_char("The number cannot be negative.\r\n", ch);
		} else {
			for (x = 0; *def_prompts[x][0] != '\n'; x++);

			if (i >= x) {
				ch->Send("The range for the prompt number is 0-%d.\r\n", x);
			} else {
				if (GET_PROMPT(ch))		free(GET_PROMPT(ch));
				GET_PROMPT(ch) = str_dup(def_prompts[i][1]);

				ch->Send("Set your prompt to the %s preset prompt.\r\n", def_prompts[i][0]);
			}
		}
	}
	release_buffer(arg);
}


ACMD(do_prompt) {
	skip_spaces(argument);
	
	if (!*argument) {
		ch->Send("Your prompt is currently: %s\r\n", (GET_PROMPT(ch) ? GET_PROMPT(ch) : "n/a"));
	} else {
		if (GET_PROMPT(ch))		free(GET_PROMPT(ch));
		GET_PROMPT(ch) = str_dup(argument);
		delete_doubledollar(GET_PROMPT(ch));

		ch->Send("Okay, set your prompt to: %s\r\n", GET_PROMPT(ch));
	}
}
  


ACMD(do_gen_write) {
	FILE *fl;
	char *tmp, *filename, *buf;
	struct stat fbuf;
	time_t ct;

	switch (subcmd) {
		case SCMD_BUG:
			filename = BUG_FILE;
			break;
		case SCMD_TYPO:
			filename = TYPO_FILE;
			break;
		case SCMD_IDEA:
			filename = IDEA_FILE;
			break;
		default:
			return;
	}

	ct = time(0);
	tmp = asctime(localtime(&ct));

	if (IS_NPC(ch)) {
		send_to_char("Monsters can't have ideas - Go away.\r\n", ch);
		return;
	}

	skip_spaces(argument);

	if (!*argument) {
		send_to_char("That must be a mistake...\r\n", ch);
		return;
	}
	
	buf = get_buffer(MAX_INPUT_LENGTH);
	strcpy(buf, argument);
	delete_doubledollar(buf);
	
	mudlogf(NRM, LVL_IMMORT, FALSE,  "%s %s: %s", ch->GetName(), CMD_NAME, buf);

	if (stat(filename, &fbuf) < 0)
		perror("Error statting file");
	else if (fbuf.st_size >= max_filesize)
		send_to_char("Sorry, the file is full right now.. try again later.\r\n", ch);
	else if (!(fl = fopen(filename, "a"))) {
		perror("do_gen_write");
		send_to_char("Could not open the file.  Sorry.\r\n", ch);
	} else {
		fprintf(fl, "%-8s (%6.6s) [%5d] %s\n", ch->GetName(), (tmp + 4),
				world[IN_ROOM(ch)].number, buf);
		fclose(fl);
		send_to_char("Okay.  Thanks!\r\n", ch);
	}
	release_buffer(buf);
}



#define TOG_OFF 0
#define TOG_ON  1

#define PRF_TOG_CHK(ch,flag) ((TOGGLE_BIT(PRF_FLAGS(ch), (flag))) & (flag))
#define CHN_TOG_CHK(ch,flag) ((TOGGLE_BIT(CHN_FLAGS(ch), (flag))) & (flag))

ACMD(do_gen_tog) {
	Flags	result;

	char *tog_messages[][2] = {
		{"You are now safe from summoning by other players.\r\n",
		"You may now be summoned by other players.\r\n"},
		{"Nohassle disabled.\r\n",
		"Nohassle enabled.\r\n"},
		{"Brief mode off.\r\n",
		"Brief mode on.\r\n"},
		{"Compact mode off.\r\n",
		"Compact mode on.\r\n"},
		{"You can now hear tells.\r\n",
		"You are now deaf to tells.\r\n"},
		{"You can now hear music.\r\n",
		"You are now deaf to music.\r\n"},
		{"You can now hear shouts.\r\n",
		"You are now deaf to shouts.\r\n"},
		{"You can now hear chat.\r\n",
		"You are now deaf to chat.\r\n"},
		{"You can now hear the congratulation messages.\r\n",
		"You are now deaf to the congratulation messages.\r\n"},
		{"You can now hear the Wiz-channel.\r\n",
		"You are now deaf to the Wiz-channel.\r\n"},
		{"You are no longer part of the Mission.\r\n",
		"Okay, you are part of the Mission!\r\n"},
		{"You will no longer see the room flags.\r\n",
		"You will now see the room flags.\r\n"},
		{"You will now have your communication repeated.\r\n",
		"You will no longer have your communication repeated.\r\n"},
		{"HolyLight mode off.\r\n",
		"HolyLight mode on.\r\n"},
		{"Nameserver_is_slow changed to NO; IP addresses will now be resolved.\r\n",
		"Nameserver_is_slow changed to YES; sitenames will no longer be resolved.\r\n"},
		{"Autoexits disabled.\r\n",
		"Autoexits enabled.\r\n"},
		{"You will now receive game broadcasts.\r\n",
		"You will no longer receive game broadcasts.\r\n"},
		{"You will no longer see color.\r\n",
		"You will now see color.\r\n"},
		{"You can now hear racial communications.\r\n",
		"You are now deaf to racial communications.\r\n"}
	};


	if (IS_NPC(ch))
		return;

	switch (subcmd) {
		case SCMD_NOSUMMON:	result = PRF_TOG_CHK(ch, PRF_SUMMONABLE);	break;
		case SCMD_NOHASSLE:	result = PRF_TOG_CHK(ch, PRF_NOHASSLE);		break;
		case SCMD_BRIEF:	result = PRF_TOG_CHK(ch, PRF_BRIEF);		break;
		case SCMD_COMPACT:	result = PRF_TOG_CHK(ch, PRF_COMPACT);		break;
		case SCMD_NOTELL:	result = CHN_TOG_CHK(ch, Channel::NoTell);	break;
		case SCMD_NOMUSIC:	result = CHN_TOG_CHK(ch, Channel::NoMusic);	break;
		case SCMD_DEAF:		result = CHN_TOG_CHK(ch, Channel::NoShout);	break;
		case SCMD_NOGOSSIP:	result = CHN_TOG_CHK(ch, Channel::NoChat);	break;
		case SCMD_NOGRATZ:	result = CHN_TOG_CHK(ch, Channel::NoGratz);	break;
		case SCMD_NOWIZ:	result = CHN_TOG_CHK(ch, Channel::NoWiz);	break;
		case SCMD_QUEST:	result = CHN_TOG_CHK(ch, Channel::Mission);	break;
		case SCMD_ROOMFLAGS:result = PRF_TOG_CHK(ch, PRF_ROOMFLAGS);	break;
		case SCMD_NOREPEAT:	result = PRF_TOG_CHK(ch, PRF_NOREPEAT);		break;
		case SCMD_HOLYLIGHT:result = PRF_TOG_CHK(ch, PRF_HOLYLIGHT);	break;
		case SCMD_SLOWNS:	result = (nameserver_is_slow = !nameserver_is_slow);	break;
		case SCMD_AUTOEXIT:	result = PRF_TOG_CHK(ch, PRF_AUTOEXIT);		break;
		case SCMD_NOINFO:	result = CHN_TOG_CHK(ch, Channel::NoInfo);	break;
		case SCMD_COLOR:	result = PRF_TOG_CHK(ch, PRF_COLOR);		break;
		case SCMD_NORACE:	result = CHN_TOG_CHK(ch, Channel::NoRace);	break;
		default:	log("SYSERR: Unknown subcmd %d in do_gen_toggle", subcmd);	return;
	}

	if (result)		send_to_char(tog_messages[subcmd][TOG_ON], ch);
	else			send_to_char(tog_messages[subcmd][TOG_OFF], ch);

	return;
}


ACMD(do_afk) {
	if (IS_NPC(ch))
		return;

	skip_spaces(argument);

	if (!GET_AFK(ch)) {
		GET_AFK(ch) = str_dup(*argument ? argument : "I am AFK!");
		act("$n has just gone AFK: $t\r\n", TRUE, ch, (ObjData *)GET_AFK(ch), 0, TO_ROOM);
	} else {
		free(GET_AFK(ch));
		GET_AFK(ch) = NULL;
		act("$n is back from AFK!\r\n", TRUE, ch, 0, 0, TO_ROOM);
	}

	return;
}


ACMD(do_watch) {
	int look_type, skill_level;
	char *arg;
	
	if (!(skill_level = IS_NPC(ch) ? GET_LEVEL(ch) * 3 : GET_SKILL(ch, SKILL_WATCH))) {
		send_to_char("Huh?!?\r\n", ch);
		return;
	}
	
	arg = get_buffer(MAX_INPUT_LENGTH);
	
	one_argument(argument, arg);
	
	if (!*arg) {
		CHAR_WATCHING(ch) = 0;
		ch->Send("You stop watching.\r\n");
	} else if ((look_type = search_block(arg, dirs, FALSE)) >= 0) {
		CHAR_WATCHING(ch) = look_type + 1;
		ch->Send("You begin watching %s you.\r\n", dir_text_2[look_type]);
	} else
		send_to_char("That's not a valid direction...\r\n", ch);
	release_buffer(arg);
}


#define LEVEL_COST(ch)	(GET_LEVEL(ch) * GET_LEVEL(ch) /* * 5 */)

ACMD(do_promote) {
	int cost = LEVEL_COST(ch), myArg;
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	one_argument(argument, arg);
	
	if (IS_STAFF(ch))
		send_to_char("Very funny.\r\n", ch);
	else if (GET_LEVEL(ch) == LVL_IMMORT - 1)
		send_to_char("You are already at the maximum level possible.\r\n", ch);
	else if (!*arg || ((myArg = atoi(arg)) != GET_LEVEL(ch) + 1)) {
		ch->Send("You need %d MP to advance to level %d.\r\n"
						 "When you have enough mission points, type \"promote %d\".\r\n", cost,
					GET_LEVEL(ch) + 1, GET_LEVEL(ch) + 1);
	} else if (GET_MISSION_POINTS(ch) < cost)
		send_to_char("You don't have enough mission points to level.\r\n", ch);
	else {
		GET_MISSION_POINTS(ch) -= cost;
		ch->set_level(myArg);
	}
	release_buffer(arg);
}
