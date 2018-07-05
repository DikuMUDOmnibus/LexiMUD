/***************************************************************************\
[*]    __     __  ___                ___  [*]   LexiMUD: A feature rich   [*]
[*]   / /  ___\ \/ (_) /\/\  /\ /\  /   \ [*]      C++ MUD codebase       [*]
[*]  / /  / _ \\  /| |/    \/ / \ \/ /\ / [*] All rights reserved         [*]
[*] / /__|  __//  \| / /\/\ \ \_/ / /_//  [*] Copyright(C) 1997-1998      [*]
[*] \____/\___/_/\_\_\/    \/\___/___,'   [*] Chris Jacobson (FearItself) [*]
[*]        LexiMUD: Feel the Power        [*] fear@technologist.com       [*]
[-]---------------------------------------+-+-----------------------------[-]
[*] File : clans.c++                                                      [*]
[*] Usage: Primary clan code                                              [*]
\***************************************************************************/


#include "clans.h"
#include "characters.h"
#include "interpreter.h"
#include "descriptors.h"
#include "db.h"
#include "utils.h"
#include "olc.h"
#include "files.h"
#include "comm.h"
#include "find.h"
#include "rooms.h"
#include "buffer.h"


//	Global Data
ClanData *clan_index = NULL;
UInt32 top_of_clant = 0;


//	External Functions
int		count_hash_records(FILE * fl);


//	Local Functions
ACMD(do_apply);
ACMD(do_enlist);
ACMD(do_boot);
ACMD(do_clanlist);
ACMD(do_clanstat);
ACMD(do_members);
ACMD(do_forceenlist);
ACMD(do_resign);
ACMD(do_clanwho);
ACMD(do_clanpromote);
ACMD(do_clandemote);
ACMD(do_deposit);
//ACMD(do_withdraw);


char *clan_ranks[] = {
	"Applicant",
	"Member",
	"Commander",
	"Leader",
	"\n"
};


//	Clan Data
ClanData::ClanData(void)
	: vnum(0), name(NULL), description(NULL), owner(0), room(0), savings(0) {
}


ClanData::~ClanData(void) {

}


bool ClanData::AddMember(UInt32 id) {
	if (this->IsMember(id))
		return false;
	this->members.Add(id);
	
	clans_save_to_disk();
	return true;
}


bool ClanData::RemoveMember(UInt32 id) {
	if (this->IsMember(id)) {
		this->members.Remove(id);
		clans_save_to_disk();
		return true;
	}
	return false;
}


bool ClanData::IsMember(UInt32 id) {
	return (this->members.InList(id));
}


ACMD(do_clanlist) {
	UInt32 clan;
	char *buf = get_buffer(MAX_STRING_LENGTH);
	char *buf2 = get_buffer(MAX_INPUT_LENGTH);
	const char *name;
	
	strcpy(buf, "    #  Clan                   Members   Leader\r\n");
	strcat(buf, "-------------------------------------------------------\r\n");
//				   123. 12345678901234567890     123     123456789012345
	for (clan = 0; clan <= top_of_clant; clan++) {
		name = get_name_by_id(clan_index[clan].owner);
		strcpy(buf2, name ? name : "<NONE>");
		sprintf(buf + strlen(buf), "  %3d. %-20s     %3d     %-15s\r\n", clan_index[clan].vnum,
				clan_index[clan].name, clan_index[clan].members.Count(), CAP(buf2));
	}
	page_string(ch->desc, buf, true);

	release_buffer(buf);
	release_buffer(buf2);
}


ACMD(do_clanstat) {
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	const char *name;
	RNum	clan, room;
	
	one_argument(argument, arg);
	
	clan = real_clan(GET_CLAN(ch));
	
	if (!*arg && (clan == -1))
		send_to_char("Stats on which clan?\r\n", ch);
	else if (*arg && ((clan = real_clan(atoi(arg))) == -1))
		send_to_char("Clan does not exist.\r\n", ch);
	else {
		room = real_room(clan_index[clan].room);
		name = get_name_by_id(clan_index[clan].owner);
		strcpy(arg, name ? name : "<NONE>");
		ch->Send("Clan       : %d\r\n"
				 "Name       : %s\r\n"
				 "Description:\r\n"
				 "%s"
				 "Owner      : [%5d] %s\r\n"
				 "Room       : [%5d] %20s\r\n"
				 "Savings    : %d MP\r\n"
				 "Members    : %d\r\n",
				clan_index[clan].vnum,
				clan_index[clan].name,
				clan_index[clan].description ? clan_index[clan].description : "<NONE>\r\n",
				clan_index[clan].owner, CAP(arg),
				clan_index[clan].room, (room != NOWHERE) ? world[room].GetName() : "<Not Created>",
				clan_index[clan].savings,
				clan_index[clan].members.Count());
	}
	release_buffer(arg);
}


ACMD(do_members) {
	SInt32	counter, columns = 0;
	UInt32	member;
	CharData *applicant;
	DescriptorData *d;
	RNum	clan;
	char	*buf;
	const char *name;
	bool	found = false;
	
	clan = real_clan(IS_STAFF(ch) ? atoi(argument) : GET_CLAN(ch));
	
	if ((clan == -1) || ((GET_CLAN_LEVEL(ch) < CLAN_MEMBER) && !IS_STAFF(ch))) {
		if (IS_STAFF(ch))	send_to_char("Clan does not exist.\r\n", ch);
		else				send_to_char("You aren't even in a clan!\r\n", ch);
		return;
	}
	
	buf = get_buffer(MAX_STRING_LENGTH);

	sprintf(buf, "Members of %s:\r\n", clan_index[clan].name);
	columns = counter = 0;
	LListIterator<SInt32>	member_iter(clan_index[clan].members);
	while ((member = member_iter.Next())) {
		if ((name = get_name_by_id(member))) {
			found = true;
			sprintf(buf + strlen(buf), "  %2d] %c%-19.19s %s", ++counter, UPPER(*name), name + 1,
					!(++columns % 2) ? "\r\n" : "");
		}
	}
	if (columns % 2)	strcat(buf, "\r\n");
	if (!found)			strcat(buf, "  None.\r\n");
	columns = 0;
	found = false;
	LListIterator<DescriptorData *>	desc_iter(descriptor_list);
	while ((d = desc_iter.Next())) {
		if ((applicant = d->Original()) && !IS_NPC(applicant) &&
				(GET_CLAN(applicant) == clan_index[clan].vnum) &&
				(GET_CLAN_LEVEL(applicant) == CLAN_APPLY)) {
			if (!found) {
				strcat(buf, "Applicants Currently Online:\r\n");
				found = true;
			}
			sprintf(buf + strlen(buf), "  %2d] %-20.20s %s", ++counter, applicant->RealName(),
					!(++columns % 2) ? "\r\n" : "");
		}
	}
	if (columns % 2)	strcat(buf, "\r\n");
	sprintf(buf + strlen(buf), "\r\n%d players listed.\r\n", counter);
	page_string(ch->desc, buf, true);

	release_buffer(buf);
}


ACMD(do_clanwho) {
	DescriptorData *d;
	CharData *		wch;
	RNum			clan;
	UInt32			players = 0;
	char			*buf;

	buf = get_buffer(MAX_STRING_LENGTH);
	
	one_argument(argument, buf);

	clan = real_clan(IS_STAFF(ch) ? atoi(buf) : GET_CLAN(ch));
	
	if ((clan == -1) || ((GET_CLAN_LEVEL(ch) < CLAN_MEMBER) && !IS_STAFF(ch))) {
		if (IS_STAFF(ch))	send_to_char("Clan does not exist.\r\n", ch);
		else				send_to_char("You aren't even in a clan!\r\n", ch);
		release_buffer(buf);
		return;
	}
	
	strcpy(buf,	"Clan Members currently online\r\n"
				"-----------------------------\r\n");

	START_ITER(iter, d, DescriptorData *, descriptor_list) {
		if (!(wch = d->Original()))							continue;
		if ((STATE(d) != CON_PLAYING) || !ch->CanSee(wch))	continue;
		 
		if ((GET_CLAN(wch) != clan_index[clan].vnum) || (GET_CLAN_LEVEL(wch) < CLAN_MEMBER))
			continue;
		
		sprintf(buf + strlen(buf), "`n[%-9s] %s %s`n", clan_ranks[GET_CLAN_LEVEL(wch)],
			GET_TITLE(wch), wch->GetName());
		players++;
		
		if (PLR_FLAGGED(wch, PLR_MAILING))			strcat(buf, " (`bmailing`n)");
		else if (PLR_FLAGGED(wch, PLR_WRITING))		strcat(buf, " (`rwriting`n)");

		if (CHN_FLAGGED(wch, Channel::NoShout))			strcat(buf, " (`gdeaf`n)");
		if (CHN_FLAGGED(wch, Channel::NoTell))			strcat(buf, " (`gnotell`n)");
		if (CHN_FLAGGED(wch, Channel::Mission))			strcat(buf, " (`mmission`n)");
		if (PLR_FLAGGED(wch, PLR_TRAITOR))			strcat(buf, " (`RTRAITOR`n)");
		if (GET_AFK(wch))							strcat(buf, " `c[AFK]`n");
		strcat(buf, "\r\n");
	} END_ITER(iter);
	
	if (players == 0)	strcat(buf, "\r\nNo clan members are currently visible to you.\r\n");
	else				sprintf(buf + strlen(buf), "\r\nThere %s %d visible clan member%s\r\n.", (players == 1 ? "is" : "are"), players, (players == 1 ? "" : "s"));
	
	page_string(ch->desc, buf, 1);
	
	release_buffer(buf);
}


ACMD(do_apply) {
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	RNum	clan;
	VNum	vnum;

	one_argument(argument, arg);
	
	if (IS_NPC(ch) || IS_STAFF(ch))
		send_to_char("You can't join a clan you dolt!\r\n", ch);
	else if ((real_clan(GET_CLAN(ch)) != -1) && (GET_CLAN_LEVEL(ch) > CLAN_APPLY))
		send_to_char("You are already a member of a clan!\r\n", ch);
	else if (!*arg || ((clan = real_clan(vnum = atoi(arg))) < 0))
		send_to_char("Apply to WHAT clan?", ch);
	else {
		GET_CLAN(ch) = vnum;
		GET_CLAN_LEVEL(ch) = CLAN_APPLY;
		ch->Send("You have applied to \"%s\".", clan_index[clan].name);
	}
	release_buffer(arg);
}


ACMD(do_resign) {
	RNum	clan;
	
	if (IS_NPC(ch))
		send_to_char("Very funny.\r\n", ch);
	else if ((clan = real_clan(GET_CLAN(ch))) == -1)
		send_to_char("You aren't even in a clan!\r\n", ch);
	else {
		if (GET_CLAN_LEVEL(ch) > CLAN_APPLY) {
			if (!clan_index[clan].RemoveMember(GET_IDNUM(ch)))
				mudlogf(BRF, LVL_STAFF, TRUE, "SYSERR: CLAN: %s's attempt to resign from clan \"%s\" failed.",
						ch->RealName(), clan_index[clan].name);
			ch->Send("You resign from \"%s\".\r\n", clan_index[clan].name);
		} else
			ch->Send("You remove your application to \"%s\".\r\n", clan_index[clan].name);
		GET_CLAN(ch) = 0;
		GET_CLAN_LEVEL(ch) = 0;
	}
}


ACMD(do_enlist) {
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	CharData *applicant;
	RNum	clan;
	
	one_argument(argument, arg);
	
	if (((clan = real_clan(GET_CLAN(ch))) == -1) || (GET_CLAN_LEVEL(ch) < CLAN_MEMBER))
		send_to_char("You aren't even in a clan!\r\n", ch);
	else if (GET_CLAN_LEVEL(ch) < CLAN_COMMANDER)
		send_to_char("You are of insufficient clan rank to do that.\r\n", ch);
	else if (!*arg)
		send_to_char("Enlist who?\r\n", ch);
	else if (!(applicant = get_player_vis(ch, arg, false)))
		send_to_char(NOPERSON, ch);
	else if (GET_CLAN(applicant) != GET_CLAN(ch))
		send_to_char("They aren't applying to this clan!", ch);
	else if (GET_CLAN_LEVEL(applicant) > CLAN_APPLY)
		send_to_char("They are already in your clan!\r\n", ch);
	else {
		if (!clan_index[clan].AddMember(GET_IDNUM(applicant))) {
			send_to_char("Problem adding player to clan, contact Staff.", ch);
			mudlogf(BRF, LVL_STAFF, TRUE, "SYSERR: CLAN: %s's attempt to enlist %s to clan \"%s\" failed.",
					ch->RealName(), applicant->RealName(), clan_index[clan].name);
		} else {
//			applicant->Send("You have been accepted into %s!", clan_index[clan].name);
//			ch->Send("You have enlisted %s into %s!", applicant->RealName(), clan_index[clan].name);
			act("You have been accepted into $t!", TRUE, 0, (ObjData *)clan_index[clan].name, applicant, TO_VICT);
			act("You have enlisted $N into $t!", TRUE, ch, (ObjData *)clan_index[clan].name, applicant, TO_CHAR);
			GET_CLAN_LEVEL(applicant) = CLAN_MEMBER;
		}
	}
	
	release_buffer(arg);
}


ACMD(do_clanpromote) {
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	CharData *victim;
	RNum	clan;
	
	one_argument(argument, arg);
	
	if (((clan = real_clan(GET_CLAN(ch))) == -1) || (GET_CLAN_LEVEL(ch) < CLAN_MEMBER))
		send_to_char("You aren't even in a clan!\r\n", ch);
	else if (GET_CLAN_LEVEL(ch) < CLAN_LEADER)
		send_to_char("You are of insufficient clan rank to do that.\r\n", ch);
	else if (!*arg)
		send_to_char("Promote who?\r\n", ch);
	else if (!(victim = get_player_vis(ch, arg, false)))
		send_to_char(NOPERSON, ch);
	else if (GET_CLAN(victim) != GET_CLAN(ch))
		send_to_char("They aren't even in this clan!\r\n", ch);
	else if (ch == victim)
		send_to_char("To yourself?!?  Dumbass.\r\n", ch);
	else if (GET_CLAN_LEVEL(victim) != CLAN_MEMBER)
		send_to_char("You can only promote clan members of rank MEMBER.\r\n", ch);
	else {
		GET_CLAN_LEVEL(victim) = CLAN_COMMANDER;
		victim->save(NOWHERE);
		act("You have been promoted to a clan Commander!", TRUE, 0, 0, victim, TO_VICT);
		act("You have promoted $N to a clan Commander!", TRUE, ch, 0, victim, TO_CHAR);
	}
	
	release_buffer(arg);
}


ACMD(do_clandemote) {
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	CharData *victim;
	RNum	clan;
	
	one_argument(argument, arg);
	
	if (((clan = real_clan(GET_CLAN(ch))) == -1) || (GET_CLAN_LEVEL(ch) < CLAN_MEMBER))
		send_to_char("You aren't even in a clan!\r\n", ch);
	else if (GET_CLAN_LEVEL(ch) < CLAN_LEADER)
		send_to_char("You are of insufficient clan rank to do that.\r\n", ch);
	else if (!*arg)
		send_to_char("Demote who?\r\n", ch);
	else if (!(victim = get_player_vis(ch, arg, false)))
		send_to_char(NOPERSON, ch);
	else if (GET_CLAN(victim) != GET_CLAN(ch))
		send_to_char("They aren't even in this clan!\r\n", ch);
	else if (ch == victim)
		send_to_char("To yourself?!?  Dumbass.\r\n", ch);
	else if (GET_CLAN_LEVEL(victim) != CLAN_COMMANDER)
		send_to_char("You can only demote clan members of rank COMMANDER.\r\n", ch);
	else {
		GET_CLAN_LEVEL(victim) = CLAN_MEMBER;
		victim->save(NOWHERE);
		act("You have been promoted to a clan Commander!", TRUE, 0, 0, victim, TO_VICT);
		act("You have promoted $N to a clan Commander!", TRUE, ch, 0, victim, TO_CHAR);
	}
	
	release_buffer(arg);
}


ACMD(do_forceenlist) {
	char *arg = get_buffer(MAX_INPUT_LENGTH);
	CharData *applicant;
	RNum	clan;
	
	one_argument(argument, arg);
	
	if (!*arg)
		send_to_char("Force-Enlist who?\r\n", ch);
	else if (!(applicant = get_player_vis(ch, arg, false)))
		send_to_char(NOPERSON, ch);
	else if ((clan = real_clan(GET_CLAN(applicant))) < 0)
		send_to_char("They aren't applying to a clan.\r\n", ch);
	else if (GET_CLAN_LEVEL(applicant) > CLAN_APPLY)
		send_to_char("They are already a member of a clan.\r\n", ch);
	else {
		if (!clan_index[clan].AddMember(GET_IDNUM(applicant))) {
			send_to_char("Problem adding player to clan.", ch);
			mudlogf(BRF, LVL_STAFF, TRUE, "SYSERR: CLAN: %s's attempt to force-enlist %s to clan \"%s\" failed.",
					ch->RealName(), applicant->RealName(), clan_index[clan].name);
		} else {
//			applicant->Send("You have been accepted into %s!", clan_index[clan].name);
//			ch->Send("You have force-enlisted %s into %s", applicant->RealName(), clan_index[clan].name);
			act("You have been accepted into $t!", TRUE, 0, (ObjData *)clan_index[clan].name, applicant, TO_VICT);
			act("You have force-enlisted $N into $t!", TRUE, ch, (ObjData *)clan_index[clan].name, applicant, TO_CHAR);
			if (GET_IDNUM(applicant) == clan_index[clan].owner)
				GET_CLAN_LEVEL(applicant) = CLAN_LEADER;
			else
				GET_CLAN_LEVEL(applicant) = CLAN_MEMBER;
		}
	}
	release_buffer(arg);
}


ACMD(do_boot) {
	char *	arg = get_buffer(MAX_INPUT_LENGTH);
	CharData *member;
	RNum	clan;
	SInt32	id;
	
	one_argument(argument, arg);
	
	if ((clan = real_clan(GET_CLAN(ch))) == -1)
		send_to_char("You aren't even in a clan!\r\n", ch);
	else if (GET_CLAN_LEVEL(ch) < CLAN_COMMANDER)
		send_to_char("You are of insufficient clan rank to do that.\r\n", ch);
	else if (!*arg)
		send_to_char("Enlist who?\r\n", ch);
	else if ((member = get_player_vis(ch, arg, false))) {
		if (GET_CLAN(ch) != GET_CLAN(member))
			act("$N isn't in your clan!", TRUE, ch, 0, member, TO_CHAR);
//			ch->Send("%s isn't in your clan!\r\n", member->RealName());
		else if (GET_CLAN_LEVEL(ch) <= GET_CLAN_LEVEL(member))
			act("$N is not below you in rank!", TRUE, ch, 0, member, TO_CHAR);
//			ch->Send("%s is not below you in clan rank!\r\n", member->RealName());
		else {
			if (!clan_index[clan].RemoveMember(GET_IDNUM(member))) {
				send_to_char("Problem booting player from clan, contact Sr. Staff.", ch);
				mudlogf(BRF, LVL_STAFF, TRUE, "SYSERR: CLAN: %s's attempt to boot %s from clan \"%s\" failed.",
						ch->RealName(), member->RealName(), clan_index[clan].name);
			} else {
				member->Send("You have been booted from %s by %s!", clan_index[clan].name, ch->RealName());
				ch->Send("You have booted %s from %s!", member->RealName(), clan_index[clan].name);
				GET_CLAN(member) = 0;
				GET_CLAN_LEVEL(member) = 0;
			}
		}
	} else if ((id = get_id_by_name(arg))) {
		if (clan_index[clan].IsMember(id)) {
			if (!clan_index[clan].RemoveMember(id)) {
				send_to_char("Problem booting player from clan, contact Sr. Staff.", ch);
				mudlogf(BRF, LVL_STAFF, TRUE, "SYSERR: CLAN: %s's attempt to boot %s from clan \"%s\" failed.",
						ch->RealName(), member->RealName(), clan_index[clan].name);
			}
			ch->Send("You have booted %s from %s!", CAP(arg), clan_index[clan].name);
		} else 
			send_to_char("They are not a member of your clan!", ch);
	} else
		send_to_char(NOPERSON, ch);
	
	release_buffer(arg);
}


ACMD(do_deposit) {
	char *	arg = get_buffer(MAX_INPUT_LENGTH);
	UInt32	amount;
	RNum	clan;
	
	one_argument(argument, arg);
	
	if ((clan = real_clan(GET_CLAN(ch))) == -1)
		send_to_char("You aren't even in a clan!\r\n", ch);
	else if (!*arg || !is_number(arg))
		send_to_char("Deposit how much?\r\n", ch);
	else if ((amount = atoi(arg)) <= 0)
		send_to_char("Deposit something more than nothing, please.\r\n", ch);
	else if (amount > GET_MISSION_POINTS(ch))
		send_to_char("You don't have that many mission points!\r\n", ch);
	else {
		GET_MISSION_POINTS(ch) -= amount;
		clan_index[clan].savings += amount;
		ch->Send("You deposit %d mission points into clan savings.\r\n", amount);
	}
	release_buffer(arg);
}

/*
ACMD(do_withdraw) {
	char *	arg = get_buffer(MAX_INPUT_LENGTH);
	UInt32	amount;
	RNum	clan;
	
	one_argument(argument, arg);
	
	if ((clan = real_clan(GET_CLAN(ch))) == -1)
		send_to_char("You aren't even in a clan!\r\n", ch);
	else if (!*arg || !is_number(arg))
		send_to_char("Withdraw how much?\r\n", ch);
	else if ((amount = atoi(arg)) <= 0)
		send_to_char("Withdraw something more than nothing, please.\r\n", ch);
	else if (amount > clan_index[clan].savings)
		send_to_char("You're clan doesn't have that many mission points!\r\n", ch);
	else {
		GET_MISSION_POINTS(ch) += amount;
		clan_index[clan].savings -= amount;
		ch->Send("You withdraw %d mission points from clan savings.\r\n", amount);
	}
}
*/

void clans_save_to_disk(void) {
	RNum	clan;
	SInt32	member;
	FILE *fl;
	char *buf;
	
	if (!(fl = fopen(CLAN_FILE, "w"))) {
		mudlogf(NRM, LVL_STAFF, TRUE, "SYSERR: Cannot write clans to disk!");
		return;
	}
	
	buf = get_buffer(MAX_STRING_LENGTH);
	
	for (clan = 0; clan <= top_of_clant; clan++) {
		strcpy(buf, clan_index[clan].description ? clan_index[clan].description : "Undefined\r\n");
		strip_string(buf);
		
		fprintf(fl,	"#%d\n"
					"%s~\n"
					"%s~\n"
					"%d %d %d\n",
				clan_index[clan].vnum,
				clan_index[clan].name ? clan_index[clan].name : "Undefined",
				buf,
				clan_index[clan].owner, clan_index[clan].room, clan_index[clan].savings);
		
		LListIterator<SInt32>	iter(clan_index[clan].members);
		while ((member = iter.Next()))
			fprintf(fl, "%d\n", member);
		fprintf(fl, "~\n");
	}
	fputs("$~\n", fl);
	fclose(fl);
	
	release_buffer(buf);
	
	olc_remove_from_save_list(CEDIT_PERMISSION, OLC_SAVE_CLAN);
}


void load_clans(void) {
	FILE *fl;
	char *filename = CLAN_FILE;
	char *line = get_buffer(MAX_INPUT_LENGTH);
	SInt32	clan_count, nr = -1, last = 0;
	
	
	if (!(fl = fopen(filename, "r"))) {
		log("Clan file %s not found.", filename);
		tar_restore_file(filename);
		exit(1);
	}
	
	clan_count = count_hash_records(fl);
	rewind(fl);
	
	clan_count++;

	CREATE(clan_index, ClanData, clan_count);
	
	for (;;) {
		if (!get_line(fl, line)) {
			log("Format error after clan #%d", nr);
			tar_restore_file(filename);
			exit(1);
		}
		
		if (*line == '$')
			break;
		else if (*line == '#') {
			last = nr;
			if (sscanf(line, "#%d", &nr) != 1) {
				log("Format error after clan #%d", nr);
				tar_restore_file(filename);
				exit(1);
			}
			parse_clan(fl, nr, filename);
		} else {
			log("Format error after clan #%d", nr);
			log("Offending line: '%s'", line);
			tar_restore_file(filename);
			exit(1);
		}
	}
	fclose(fl);
	release_buffer(line);
}


void parse_clan(FILE *fl, int virtual_nr, char *filename) {
	static SInt32	i = 0;
	char 	*line = get_buffer(MAX_INPUT_LENGTH),
			*buf = get_buffer(MAX_INPUT_LENGTH),
			*members,
			*s;
	SInt32	t[3], member_num;
			
	sprintf(buf, "Parsing clan #%d", virtual_nr);
	
	clan_index[i].vnum			= virtual_nr;
	clan_index[i].name			= fread_string(fl, buf, filename);
	clan_index[i].description	= fread_string(fl, buf, filename);
	if (!get_line(fl, line) || (sscanf(line, "%d %d %d", t, t + 1, t + 2) != 3)) {
		log("Format error in clan #%d", virtual_nr);
		tar_restore_file(filename);
		exit(1);
	}
	clan_index[i].owner		= t[0];
	clan_index[i].room		= t[1];
	clan_index[i].savings	= t[2];
	
	if ((members = fread_string(fl, buf, filename))) {
		for (s = strtok(members, " \r\n"); s; s = strtok(NULL, " \r\n")) {
			member_num = atoi(s);
			if (!get_name_by_id(member_num))	// Make sure the characters still exist
				continue;
			clan_index[i].members.Add(member_num);
		}
		FREE(members);
	}
	top_of_clant = i++;
	
	release_buffer(line);
	release_buffer(buf);
}


RNum real_clan(VNum vnum) {
	int bot, top, mid, nr, found = NOWHERE;

	/* First binary search. */
	bot = 0;
	top = top_of_clant;

	for (;;) {
		mid = (bot + top) >> 1;

		if (clan_index[mid].vnum == vnum)
			return mid;
		if (bot >= top)
			break;
		if (clan_index[mid].vnum > vnum)
			top = mid - 1;
		else
			bot = mid + 1;
	}

	/* If not found - use linear on the "new" parts. */
	for(nr = 0; nr <= top_of_clant; nr++) {
		if(clan_index[nr].vnum == vnum) {
			found = nr;
			break;
		}
	}
	return(found);
}
