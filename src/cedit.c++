/*************************************************************************
*   File: cedit.c++                  Part of Aliens vs Predator: The MUD *
*  Usage: Primary code for characters                                    *
*************************************************************************/



#include "clans.h"
#include "descriptors.h"
#include "characters.h"
#include "olc.h"
#include "utils.h"
#include "comm.h"
#include "buffer.h"
#include "rooms.h"
#include "db.h"


void cedit_parse(DescriptorData *d, char *arg);
void cedit_setup_new(DescriptorData *d);
void cedit_setup_existing(DescriptorData *d, RNum real_num);
void cedit_save_internally(DescriptorData *d);
void free_clan(ClanData *clan);
void cedit_disp_menu(DescriptorData *d);

void cedit_setup_new(DescriptorData *d) {
	OLC_CLAN(d) = new ClanData;
	
	OLC_CLAN(d)->name = str_dup("New Clan");
	OLC_CLAN(d)->description = str_dup("A new clan!\r\n");
	OLC_CLAN(d)->owner = -1;
	OLC_CLAN(d)->room = -1;
	OLC_CLAN(d)->vnum = OLC_NUM(d);
	OLC_CLAN(d)->savings = 0;
	OLC_VAL(d) = 0;
	
	cedit_disp_menu(d);
}


void cedit_setup_existing(DescriptorData *d, RNum real_num) {
	ClanData *clan;
	
	clan = new ClanData;
	*clan = clan_index[real_num];
	clan->name = str_dup(clan_index[real_num].name);
	clan->description = str_dup(clan_index[real_num].description);
	
	OLC_CLAN(d) = clan;
	OLC_VAL(d) = 0;
	cedit_disp_menu(d);
}


void cedit_save_internally(DescriptorData *d) {
	RNum	rnum;
	ClanData *new_index;
	bool	found = false;
	
	if (get_name_by_id(OLC_CLAN(d)->owner))
		OLC_CLAN(d)->AddMember(OLC_CLAN(d)->owner);
	
	if ((rnum = real_clan(OLC_NUM(d))) >= 0) {
		if (clan_index[rnum].name)			free(clan_index[rnum].name);
		if (clan_index[rnum].description)	free(clan_index[rnum].description);
		clan_index[rnum] = *OLC_CLAN(d);
		clan_index[rnum].name = str_dup(OLC_CLAN(d)->name);
		clan_index[rnum].description = str_dup(OLC_CLAN(d)->description);
	} else {
		CREATE(new_index, ClanData, top_of_clant + 2);
		for (rnum = 0; rnum <= top_of_clant; rnum++) {
			if (!found) {
				if (clan_index[rnum].vnum > OLC_NUM(d)) {
					found = true;
					new_index[rnum] = *OLC_CLAN(d);
					new_index[rnum].vnum = OLC_NUM(d);
					new_index[rnum].name = str_dup(OLC_CLAN(d)->name);
					new_index[rnum].description = str_dup(OLC_CLAN(d)->description);
					
					new_index[rnum + 1] = clan_index[rnum];
				} else
					new_index[rnum] = clan_index[rnum];
			} else
				new_index[rnum + 1] = clan_index[rnum];
		}
		if (!found) {
			new_index[rnum] = *OLC_CLAN(d);
			new_index[rnum].vnum = OLC_NUM(d);
			new_index[rnum].name = str_dup(OLC_CLAN(d)->name);
			new_index[rnum].description = str_dup(OLC_CLAN(d)->description);
		}
		FREE(clan_index);
		clan_index = new_index;
		top_of_clant++;
	}
	olc_add_to_save_list(CEDIT_PERMISSION, OLC_SAVE_CLAN);
}


void free_clan(ClanData *clan) {
	FREE(clan->name);
	FREE(clan->description);
	clan->members.Erase();
	delete clan;
}


void cedit_disp_menu(DescriptorData *d) {
	SInt32	room;
	const char *	name;
	char *	buf = get_buffer(MAX_INPUT_LENGTH);
	
	room = real_room(OLC_CLAN(d)->room);
	
	name = get_name_by_id(OLC_CLAN(d)->owner);
	strcpy(buf, name ? name : "<NONE>");
	CAP(buf);
	
	d->character->Send(
#if defined(CLEAR_SCREEN)
				"\x1B[H\x1B[J"
#endif
				"-- Clan Number:  [`c%d`n]\r\n"
				"`G1`n) Name       : `y%s\r\n"
				"`G2`n) Description:-\r\n`y%s"
				"`G3`n) Owner      : `y%s\r\n"
				"`G4`n) Room       : `y%s `n[`c%5d`n]\r\n"
				"`G5`n) Savings    : `y%d\r\n"
				"`GQ`n) Quit\r\n"
				"Enter choice : ",
				OLC_NUM(d),
				OLC_CLAN(d)->name,
				OLC_CLAN(d)->description,
				buf,
				room != NOWHERE ? world[room].GetName("<ERROR>") : "<Not Created>" , OLC_CLAN(d)->room,
				OLC_CLAN(d)->savings);

	OLC_MODE(d) = CEDIT_MAIN_MENU;
	release_buffer(buf);
}


void cedit_parse(DescriptorData *d, char *arg) {
	switch (OLC_MODE(d)) {
		case CEDIT_CONFIRM_SAVESTRING:
			switch (*arg) {
				case 'y':
				case 'Y':
					send_to_char("Saving clan to memory.\r\n", d->character);
					cedit_save_internally(d);
					mudlogf(NRM, LVL_BUILDER, TRUE, "OLC: %s edits clan %d", d->character->RealName(), OLC_NUM(d));
					// Fall through
				case 'n':
				case 'N':
					cleanup_olc(d, CLEANUP_ALL);
					break;
				default:
				send_to_char("Invalid choice!\r\n"
							 "Do you wish to save the clan? ", d->character);
			}
			return;
		case CEDIT_MAIN_MENU:
			switch (*arg) {
				case 'q':
				case 'Q':
					if (OLC_VAL(d)) {
						send_to_char("Do you wish to save the changes to the clan? (y/n) : ", d->character);
						OLC_MODE(d) = CEDIT_CONFIRM_SAVESTRING;
					} else
						cleanup_olc(d, CLEANUP_ALL);
					return;
				case '1':
					OLC_MODE(d) = CEDIT_NAME;
					send_to_char("Enter new name:\r\n] ", d->character);
					return;
				case '2':
					OLC_MODE(d) = CEDIT_DESCRIPTION;
					OLC_VAL(d) = 1;
					SEND_TO_Q("Enter clan description: (/s saves /h for help)\r\n\r\n", d);
					if (OLC_CLAN(d)->description) {
						SEND_TO_Q(OLC_CLAN(d)->description, d);
						d->backstr = str_dup(OLC_CLAN(d)->description);
					}
					d->str = &OLC_CLAN(d)->description;
					d->max_str = MAX_INPUT_LENGTH;
					d->mail_to = 0;
					return;
				case '3':
					OLC_MODE(d) = CEDIT_OWNER;
					send_to_char("Enter owners name: ", d->character);
					return;
				case '4':
					OLC_MODE(d) = CEDIT_ROOM;
					send_to_char("Enter clan room vnum: ", d->character);
					return;
				case '5':
					OLC_MODE(d) = CEDIT_SAVINGS;
					send_to_char("Enter clan's total savings: ", d->character);
					return;
				default:
					cedit_disp_menu(d);
					return;
			}
			break;
		case CEDIT_NAME:
			if (*arg)	OLC_CLAN(d)->name = str_dup(arg);
			break;
		case CEDIT_DESCRIPTION:
			mudlog("SYSERR: OLC: cedit_parse(): Reached DESCRIPTION case!", BRF, LVL_BUILDER, TRUE);
			send_to_char("Oops...\r\n", d->character);
			break;
		case CEDIT_OWNER:
			OLC_CLAN(d)->owner = get_id_by_name(arg);
			break;
		case CEDIT_ROOM:
			OLC_CLAN(d)->room = atoi(arg);
			break;
		case CEDIT_SAVINGS:
			OLC_CLAN(d)->savings = atoi(arg);
			break;
		default:
			mudlogf(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: cedit_parse(): Reached default case!  Case: %d", OLC_MODE(d));
			send_to_char("Oops...\r\n", d->character);
			break;
	}
	OLC_VAL(d) = 1;
	cedit_disp_menu(d);
}