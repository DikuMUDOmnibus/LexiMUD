/* ************************************************************************
*   File: act.comm.c                                    Part of CircleMUD *
*  Usage: Player-level communication commands                             *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */


#include "characters.h"
#include "rooms.h"
#include "objects.h"
#include "descriptors.h"
#include "utils.h"
#include "buffer.h"
#include "comm.h"
#include "interpreter.h"
#include "find.h"
#include "db.h"
#include "boards.h"
#include "clans.h"
#include "staffcmds.h"


void speech_mtrigger(CharData *actor, char *str);
void speech_otrigger(CharData *actor, char *str);
void speech_wtrigger(CharData *actor, char *str);


/* extern variables */
extern int level_can_shout;
extern int holler_move_cost;
extern void mprog_speech_trigger(char *txt, CharData *mob);

/* local functions */
void perform_tell(CharData *ch, CharData *vict, char *arg);
bool is_tell_ok(CharData *ch, CharData *vict);
ACMD(do_say);
ACMD(do_gsay);
ACMD(do_tell);
ACMD(do_reply);
ACMD(do_spec_comm);
ACMD(do_write);
ACMD(do_page);
ACMD(do_gen_comm);
ACMD(do_qcomm);


ACMD(do_say) {
	char *arg;
//	char *buf;
	char *verb, *s;

	skip_spaces(argument);

	if (!*argument)
		ch->Send("Yes, but WHAT do you want to say?\r\n");
	else if (GET_POS(ch) == POS_SLEEPING)
		ch->Send("In your dreams, or what?\r\n");
	else {
		arg = get_buffer(MAX_STRING_LENGTH);
//		buf = get_buffer(MAX_STRING_LENGTH);
		
		strcpy(arg, argument);
		
		s = arg + strlen(arg);
		while (isspace(*(--s)));
		*(s + 1) = '\0';
		
		switch (*s) {
			case '?':	verb = "ask";		break;
			case '!':	verb = "exclaim";	break;
			default:	verb = "say";		break;
		}
		delete_doubledollar(arg);
//		sprintf(buf, "$n %ss, '%s`n'", verb, arg);
//		act(buf, FALSE, ch, 0, 0, TO_ROOM | TO_TRIG);
		if (PRF_FLAGGED(ch, PRF_NOREPEAT))	ch->Send(OK);
//		else								ch->Send("You %s, '%s`n'\r\n", verb, arg);
		else {
//			sprintf(buf, "You %s, '%s`n'", verb, arg);
//			act(buf, FALSE, ch, 0, 0, TO_CHAR);
			MOBTrigger = FALSE;
			act("You $t, '$T'", FALSE, ch, (ObjData *)verb, arg, TO_CHAR | TO_TRIG);
		}
		MOBTrigger = FALSE;
		act("$n $ts, '$T'", FALSE, ch, (ObjData *)verb, arg, TO_ROOM | TO_TRIG);
//		release_buffer(buf);
		mprog_speech_trigger(arg, ch);
		speech_mtrigger(ch, arg);
		speech_otrigger(ch, arg);
		speech_wtrigger(ch, arg);
		release_buffer(arg);
	}
}


ACMD(do_gsay) {
	CharData *k, *follower;

	delete_doubledollar(argument);
	skip_spaces(argument);

	if (!AFF_FLAGGED(ch, AFF_GROUP))
		ch->Send("But you are not the member of a group!\r\n");
	else if (!*argument)
		ch->Send("Yes, but WHAT do you want to group-say?\r\n");
	else {
		k = ch->master ? ch->master : ch;

		if (AFF_FLAGGED(k, AFF_GROUP) && (k != ch))
			act("$n tells the group, '$t'", FALSE, ch, (ObjData *)argument, k, TO_VICT | TO_SLEEP);
		
		LListIterator<CharData *>	iter(k->followers);
		while ((follower = iter.Next())) {
			if (AFF_FLAGGED(follower, AFF_GROUP) && (follower != ch))
				act("$n tells the group, '$t'", FALSE, ch, (ObjData *)argument, follower, TO_VICT | TO_SLEEP);
		}
		
		if (PRF_FLAGGED(ch, PRF_NOREPEAT))
			ch->Send(OK);
		else
			act("You tell the group, '$t'", FALSE, ch, (ObjData *)argument, 0, TO_CHAR | TO_SLEEP);
	}
}


void perform_tell(CharData *ch, CharData *vict, char *arg) {
	delete_doubledollar(arg);
	act("$n `rtells you, '$t`r'`n", FALSE, ch, (ObjData *)arg, vict, TO_VICT | TO_SLEEP);

	if (PRF_FLAGGED(ch, PRF_NOREPEAT))
		ch->Send(OK);
	else {
		act("`rYou tell $N`r, '$t`r'`n", FALSE, ch, (ObjData *)arg, vict, TO_CHAR | TO_SLEEP);
	}

	if (!IS_NPC(vict))
		GET_LAST_TELL(vict) = GET_ID(ch);
}


bool is_tell_ok(CharData *ch, CharData *vict) {
	if (ch == vict)
		ch->Send("You try to tell yourself something.\r\n");
	else if (CHN_FLAGGED(ch, Channel::NoTell))
		ch->Send("You can't tell other people while you have notell on.\r\n");
	else if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_SOUNDPROOF))
		ch->Send("The walls seem to absorb your words.\r\n");
	else if (!IS_NPC(vict) && !vict->desc)        /* linkless */
		act("$E's linkless at the moment.", FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP);
	else if (PLR_FLAGGED(vict, PLR_WRITING))
		act("$E's writing a message right now; try again later.", FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP);
	else if (CHN_FLAGGED(vict, Channel::NoTell) || ROOM_FLAGGED(IN_ROOM(vict), ROOM_SOUNDPROOF))
		act("$E can't hear you.", FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP);
	else
		return true;
	return false;
}



/*
 * Yes, do_tell probably could be combined with whisper and ask, but
 * called frequently, and should IMHO be kept as tight as possible.
 */
ACMD(do_tell) {
	CharData *vict;
	char *buf = get_buffer(MAX_STRING_LENGTH);

	argument = one_argument(argument, buf);

	if (!*buf || !*argument)
		ch->Send("Who do you wish to tell what??\r\n");
	else if (!(vict = get_char_vis(ch, buf)))
		ch->Send(NOPERSON);
	else if (is_tell_ok(ch, vict)) {
		if (!IS_NPC(vict) && GET_AFK(vict)) {
			act("$N is AFK and may not hear your tell.\r\n"
				"MESSAGE: $t\r\n", TRUE, ch, (ObjData *)GET_AFK(vict), vict, TO_CHAR);
		}
		perform_tell(ch, vict, argument);
	}
	
	release_buffer(buf);
}


ACMD(do_reply) {
	CharData *tch;
	
	if (IS_NPC(ch))
		return;
	
	skip_spaces(argument);

	if (GET_LAST_TELL(ch) == 0)			ch->Send("You have no-one to reply to!\r\n");
	else if (!*argument)				ch->Send("What is your reply?\r\n");
	else if (!(tch = find_char(GET_LAST_TELL(ch)))) {
		ch->Send("They are no longer playing.\r\n");
		GET_LAST_TELL(ch) = NOBODY;
	} else if (is_tell_ok(ch, tch))	{
		perform_tell(ch, tch, argument);
	}
}


ACMD(do_spec_comm) {
	CharData *vict;
	char *action_sing, *action_plur, *action_others;
	char *buf = get_buffer(MAX_STRING_LENGTH);

	if (subcmd == SCMD_WHISPER) {
		action_sing = "whisper to";
		action_plur = "whispers to";
		action_others = "$n whispers something to $N.";
	} else {
		action_sing = "ask";
		action_plur = "asks";
		action_others = "$n asks $N a question.";
	}

	argument = one_argument(argument, buf);

	if (!*buf || !*argument) {
		ch->Send("Whom do you want to %s.. and what??\r\n", action_sing);
	} else if (!(vict = get_char_room_vis(ch, buf)))
		send_to_char(NOPERSON, ch);
	else if (vict == ch)
		send_to_char("You can't get your mouth close enough to your ear...\r\n", ch);
	else {
		sprintf(buf, "$n %s you, '%s'", action_plur, argument);
		act(buf, FALSE, ch, 0, vict, TO_VICT);
		if (PRF_FLAGGED(ch, PRF_NOREPEAT))
			send_to_char(OK, ch);
		else {
			sprintf(buf, "You %s $N, '%s'", action_sing, argument);
			act(buf, FALSE, ch, 0, vict, TO_CHAR);
		}
		act(action_others, FALSE, ch, 0, vict, TO_NOTVICT);
	}
	release_buffer(buf);
}


#define MAX_NOTE_LENGTH 1000	/* arbitrary */

ACMD(do_write) {
	ObjData *paper = NULL, *pen = NULL, *obj, *held;
	char *papername, *penname;
	int temp = 0;

	if (!ch->desc)
		return;

	/* before we do anything, lets see if there's a board involved. */
	if(!(obj = get_obj_in_list_type(ITEM_BOARD, ch->carrying)))
		obj = get_obj_in_list_type(ITEM_BOARD, world[IN_ROOM(ch)].contents);
	
	if(obj)                /* then there IS a board! */
		Board_write_message(GET_OBJ_VNUM(obj), ch, argument);
	else {
		char *buf1 = get_buffer(MAX_INPUT_LENGTH);
		char *buf2 = get_buffer(MAX_INPUT_LENGTH);
		
		papername = buf1;
		penname = buf2;

		two_arguments(argument, papername, penname);

		if (!*papername)		/* nothing was delivered */
			send_to_char("Write?  With what?  ON what?  What are you trying to do?!?\r\n", ch);
		else if (*penname) {		/* there were two arguments */
			if (!(paper = get_obj_in_list_vis(ch, papername, ch->carrying)))
				ch->Send("You have no %s.\r\n", papername);
			else if (!(pen = get_obj_in_list_vis(ch, penname, ch->carrying)))
				ch->Send("You have no %s.\r\n", penname);
		} else {		/* there was one arg.. let's see what we can find */
			if (!(paper = get_obj_in_list_vis(ch, papername, ch->carrying)))
				ch->Send("There is no %s in your inventory.\r\n", papername);
			else if (GET_OBJ_TYPE(paper) == ITEM_PEN) {	/* oops, a pen.. */
				pen = paper;
				paper = NULL;
			} else if (GET_OBJ_TYPE(paper) != ITEM_NOTE) {
				send_to_char("That thing has nothing to do with writing.\r\n", ch);
				release_buffer(buf1);
				release_buffer(buf2);
				return;
			}
			/* One object was found.. now for the other one. */
			held = GET_EQ(ch, WEAR_HAND_R);
			if (!held || (GET_OBJ_TYPE(held) != (pen ? ITEM_NOTE : ITEM_PEN))) {
				held = GET_EQ(ch, WEAR_HAND_L);
				if (!held || (GET_OBJ_TYPE(held) != (pen ? ITEM_NOTE : ITEM_PEN))) {
					ch->Send("You can't write with %s %s alone.\r\n", AN(papername), papername);
					pen = NULL;
				}
			}
			if (held && !ch->CanSee(held))
				send_to_char("The stuff in your hand is invisible!  Yeech!!\r\n", ch);
			else if (pen)	paper = held;
			else			pen = held;
		}
		
		if (paper && pen) {
			/* ok.. now let's see what kind of stuff we've found */
			if (GET_OBJ_TYPE(pen) != ITEM_PEN)
				act("$p is no good for writing with.", FALSE, ch, pen, 0, TO_CHAR);
			else if (GET_OBJ_TYPE(paper) != ITEM_NOTE)
				act("You can't write on $p.", FALSE, ch, paper, 0, TO_CHAR);
			else if (paper->actionDesc)
				send_to_char("There's something written on it already.\r\n", ch);
			else {
				ch->desc->backstr = NULL;
				send_to_char("Write your note.  (/s saves /h for help)\r\n", ch);
				if (paper->actionDesc) {
					if (SSData(paper->actionDesc)) {
						ch->desc->backstr = str_dup(SSData(paper->actionDesc));
						send_to_char(SSData(paper->actionDesc), ch);
					}
				} else
					paper->actionDesc = SSCreate("");
				act("$n begins to jot down a note.", TRUE, ch, 0, 0, TO_ROOM);
				ch->desc->str = &paper->actionDesc->str;
				ch->desc->max_str = MAX_NOTE_LENGTH;
			}
		}
		release_buffer(buf1);
		release_buffer(buf2);
	}
}


ACMD(do_page) {
	DescriptorData *d;
	CharData *vict;
	char *arg1 = get_buffer(MAX_INPUT_LENGTH);

	argument = one_argument(argument, arg1);

	if (IS_NPC(ch))
		send_to_char("Monsters can't page.. go away.\r\n", ch);
	else if (!*arg1)
		send_to_char("Whom do you wish to page?\r\n", ch);
	else {
		if (!str_cmp(arg1, "all")) {
			if (STF_FLAGGED(ch, STAFF_ADMIN | STAFF_GAME | STAFF_SECURITY)) {
				START_ITER(iter, d, DescriptorData *, descriptor_list) {
					if ((STATE(d) == CON_PLAYING) && d->character)
						act("`*`**$n* $t\r\n", FALSE, ch, (ObjData *)argument, d->character, TO_VICT);
				} END_ITER(iter);
			} else
				send_to_char("You will never be privileged enough to do that!\r\n", ch);
		} else if ((vict = get_char_vis(ch, arg1)) != NULL) {
			act("`*`**$n* $t\r\n", FALSE, ch, (ObjData *)argument, vict, TO_VICT);
			if (PRF_FLAGGED(ch, PRF_NOREPEAT))
				send_to_char(OK, ch);
			else
				act("`*`*-$N-> $t\r\n", FALSE, ch, (ObjData *)argument, vict, TO_CHAR);
		} else
			send_to_char("There is no such player!\r\n", ch);
	}
	release_buffer(arg1);
}


/**********************************************************************
 * generalized communication func, originally by Fred C. Merkel (Torg) *
  *********************************************************************/

ACMD(do_gen_comm) {
	DescriptorData *i;
	char *buf;

	/* Array of flags which must _not_ be set in order for comm to be heard */
	static UInt32 channels[] = {
		Channel::NoShout,
		Channel::NoChat,
		Channel::NoMusic,
		Channel::NoGratz,
		0,
		Channel::NoClan,
		Channel::NoRace,
		0,
		0
	};

	/*
	 * com_msgs: [0] Message if you can't perform the action because of noshout
	 *           [1] name of the action
	 *           [2] message if you're not on the channel
	 *           [3] a color string.
	 */
	static char *com_msgs[][5] = {
		{	"You cannot shout!!\r\n",
			"shout",
			"Turn off your noshout flag first!\r\n",
			"$n `yshouts '%s`y'`n",
			"`yYou shout `y'%s`y'`n",
		},

		{	"You cannot chat!!\r\n",
			"chat",
			"You aren't even on the channel!\r\n",
			"`y[`bCHAT`y] $n`y: %s`n",
			"`y[`bCHAT`y] -> `y%s`n",
		},

		{	"You cannot sing!!\r\n",
			"sing",
			"You aren't even on the channel!\r\n",
			"`y[`mMUSIC by $n`y] `m%s`n",
			"`y[`mMUSIC`y] -> `w%s`n",
		},

		{	"You cannot congratulate!\r\n",
			"congrat",
			"You aren't even on the channel!\r\n",
			"`y[`bCONGRATS`y] $n`g: %s`n",
			"`y[`bCONGRATS`y] -> `g%s`n",
		},
		{	"You cannot broadcast info!\r\n",
			"broadcast",
			"",
			"`y[`bINFO`y] $n`w: %s`n",
			"`y[`bINFO`y] -> `w%s`n",
		},
		{	"You cannot converse with your clan!\r\n",
			"clanchat",
			"You aren't even on the channel!\r\n",
			"`y[`bCLAN`y] $n`r: %s`n",
			"`y[`bCLAN`y] -> `r%s`n",
		},
		{	"You cannot converse with your race!\r\n",
			"racechat",
			"You aren't even on the channel!\r\n",
			"`y[`bRACE`y] $n`r: %s`n",
			"`y[`bRACE`y] -> `r%s`n",
		},
		{
			"You cannot chat with others on the mission!\r\n",
			"mission",
			"",
			"`y[`bMISSION`y] $n`g:%s`n",
			"`y[`bMISSION`y] -> `g%s`n"
		}
	};

	if (PLR_FLAGGED(ch, PLR_NOSHOUT)) {
		send_to_char(com_msgs[subcmd][0], ch);
		return;
	}
	if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_SOUNDPROOF) && !NO_STAFF_HASSLE(ch)) {
		send_to_char("The walls seem to absorb your words.\r\n", ch);
		return;
	}
	/* level_can_shout defined in config.c */
	if (GET_LEVEL(ch) < level_can_shout) {
		char *buf1 = get_buffer(128);
		ch->Send("You must be at least level %d before you can %s.\r\n",
				level_can_shout, com_msgs[subcmd][1]);
		release_buffer(buf1);
		return;
	}
	/* make sure the char is on the channel */
	if (CHN_FLAGGED(ch, channels[subcmd])) {
		send_to_char(com_msgs[subcmd][2], ch);
		return;
	}
	if (subcmd == SCMD_MISSION && !CHN_FLAGGED(ch, Channel::Mission)) {
		send_to_char("You aren't even part of the mission!\r\n", ch);
		return;
	}
	if (subcmd == SCMD_CLAN && (real_clan(GET_CLAN(ch)) == -1)) {
		send_to_char("You aren't even a clan member!\r\n", ch);
		return;
	}

	/* skip leading spaces */
	skip_spaces(argument);

	/* make sure that there is something there to say! */
	if (!*argument) {
		act("Yes, $t, fine, $t we must, but WHAT???\r\n", FALSE, ch, (ObjData *)com_msgs[subcmd][1], 0, TO_CHAR);
		return;
	}
	
	buf = get_buffer(MAX_STRING_LENGTH);
	
	/* first, set up strings to be given to the communicator */
	if (PRF_FLAGGED(ch, PRF_NOREPEAT))
		send_to_char(OK, ch);
	else {
		sprintf(buf, com_msgs[subcmd][4], argument);
		act(buf, FALSE, ch, 0, 0, TO_CHAR | TO_SLEEP);
	}

	sprintf(buf, com_msgs[subcmd][3], argument);

	/* now send all the strings out */
	LListIterator<DescriptorData *>	iter(descriptor_list);
	while ((i = iter.Next())) {
		if ((STATE(i) == CON_PLAYING) && (i != ch->desc) && i->character &&
				!CHN_FLAGGED(i->character, channels[subcmd]) &&
				!PLR_FLAGGED(i->character, PLR_WRITING) &&
				!ROOM_FLAGGED(IN_ROOM(i->character), ROOM_SOUNDPROOF)) {

			if ((subcmd == SCMD_SHOUT) &&
					((world[IN_ROOM(ch)].zone != world[IN_ROOM(i->character)].zone) ||
					GET_POS(i->character) < POS_RESTING))
				continue;
			if ((subcmd == SCMD_CLAN) && 
					((GET_CLAN(ch) != GET_CLAN(i->character)) || 
					(GET_CLAN_LEVEL(i->character) < CLAN_MEMBER) ||
					IS_TRAITOR(i->character)))
				continue;
			if ((subcmd == SCMD_RACE) && ((GET_RACE(ch) > RACE_SYNTHETIC) ?
					(GET_RACE(ch) != GET_RACE(i->character)) :
					(GET_RACE(i->character) > RACE_SYNTHETIC)))
				continue;
			if ((subcmd == SCMD_MISSION) && !CHN_FLAGGED(i->character, Channel::Mission))
				continue;
			
			act(buf, FALSE, ch, 0, i->character, TO_VICT | TO_SLEEP);
		}
	}
	release_buffer(buf);
}


ACMD(do_qcomm) {
	DescriptorData *i;
	
	skip_spaces(argument);
	
	if (!CHN_FLAGGED(ch, Channel::Mission)) {
		send_to_char("You aren't even on the mission channel!\r\n", ch);
	} else if (!*argument) {
		send_to_char("MEcho?  Yes, fine, %s we must, but WHAT??\r\n", ch);
	} else {
		if (PRF_FLAGGED(ch, PRF_NOREPEAT))
			send_to_char(OK, ch);
		else
			act(argument, FALSE, ch, 0, 0, TO_CHAR);

		LListIterator<DescriptorData *>	iter(descriptor_list);
		while ((i = iter.Next())) {
			if ((STATE(i) == CON_PLAYING) && i != ch->desc && CHN_FLAGGED(i->character, Channel::Mission))
				act(argument, 0, ch, 0, i->character, TO_VICT | TO_SLEEP);
		}
	}
}
