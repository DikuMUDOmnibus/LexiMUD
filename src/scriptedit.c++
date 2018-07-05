/***************************************************************************\
[*]    __     __  ___                ___  [*]   LexiMUD: A feature rich   [*]
[*]   / /  ___\ \/ (_) /\/\  /\ /\  /   \ [*]      C++ MUD codebase       [*]
[*]  / /  / _ \\  /| |/    \/ / \ \/ /\ / [*] All rights reserved         [*]
[*] / /__|  __//  \| / /\/\ \ \_/ / /_//  [*] Copyright(C) 1997-1998      [*]
[*] \____/\___/_/\_\_\/    \/\___/___,'   [*] Chris Jacobson (FearItself) [*]
[*]        LexiMUD: Feel the Power        [*] fear@technologist.com       [*]
[-]---------------------------------------+-+-----------------------------[-]
[*] File : scriptedit.c++                                                 [*]
[*] Usage: ScriptEdit - OLC Editor for Scripts                            [*]
\***************************************************************************/


#include "scripts.h"
#include "characters.h"
#include "descriptors.h"
#include "buffer.h"
#include "comm.h"
#include "utils.h"
#include "db.h"
#include "olc.h"
#include "event.h"


void free_varlist(LList<TrigVarData *> &vd);
extern struct zone_data *zone_table;
extern int top_of_zone_table;
extern char *mtrig_types[];
extern char *otrig_types[];
extern char *wtrig_types[];

/* function protos */
void scriptedit_disp_menu(DescriptorData *d);
void scriptedit_parse(DescriptorData *d, char *arg);
void scriptedit_setup_new(DescriptorData *d);
void scriptedit_setup_existing(DescriptorData *d, RNum real_num);
void scriptedit_save_to_disk(int zone_num);
void scriptedit_save_internally(DescriptorData *d);
char *scriptedit_get_class(TrigData *trig);
char *scriptedit_get_type(DescriptorData *d);
void scriptedit_display_types(DescriptorData *d);
void scriptedit_display_classes(DescriptorData *d);

/*
 * Utils and exported functions.
 */

void scriptedit_setup_new(DescriptorData *d) {
	TrigData *trig;
	
	trig = new TrigData;
	
	GET_TRIG_RNUM(trig) = -1;
	GET_TRIG_DATA_TYPE(trig) = -1;
	GET_TRIG_NAME(trig) = SSCreate("new script");
	GET_TRIG_ARG(trig) = NULL;
	
	OLC_STORAGE(d) = str_dup("* Empty script\r\n");

	OLC_TRIG(d) = trig;
	
	OLC_VAL(d) = 0;
	scriptedit_disp_menu(d);
}

/*------------------------------------------------------------------------*/

void scriptedit_setup_existing(DescriptorData *d, RNum real_num) {
	TrigData *trig, *proto = (TrigData *)trig_index[real_num].proto;
	char *buf = get_buffer(MAX_STRING_LENGTH);
	CmdlistElement *cmd;
	
	trig = new TrigData;
	*trig = *proto;
	
	GET_TRIG_NAME(trig) = SSCreate(SSData(GET_TRIG_NAME(proto)));
	GET_TRIG_ARG(trig) = SSCreate(SSData(GET_TRIG_ARG(proto)));
	
	for(cmd = proto->cmdlist; cmd; cmd = cmd->next) {
		if (cmd->cmd)
			strcat(buf, cmd->cmd);
		strcat(buf, "\r\n");
	}
	
	OLC_STORAGE(d) = str_dup(buf);

	release_buffer(buf);
	
	OLC_TRIG(d) = trig;
	
	OLC_VAL(d) = 0;
	scriptedit_disp_menu(d);
}

#define ZCMD zone_table[zone].cmd[cmd_no]

void scriptedit_save_internally(DescriptorData *d) {
	int trig_rnum, i, found = FALSE, zone, cmd_no;
	char *s;
	TrigData *proto;
	TrigData *trig = OLC_TRIG(d);
	TrigData *live_trig;
	CmdlistElement *cmd, *next_cmd;
	IndexData *new_index;
	DescriptorData *dsc;
	
	REMOVE_BIT(GET_TRIG_TYPE(OLC_TRIG(d)), TRIG_DELETED);
	
	if ((trig_rnum = real_trigger(OLC_NUM(d))) != -1) {
		proto = (TrigData *)trig_index[trig_rnum].proto;
		/* Erase the old command list */
		for (cmd = proto->cmdlist; cmd; cmd = next_cmd) {
			next_cmd = cmd->next;
			delete cmd;
		}
		
		SSFree(proto->arglist);
		SSFree(proto->name);
		
		/* Recompile the command list from the new script */
		if (!OLC_STORAGE(d))
			OLC_STORAGE(d) = str_dup("* Empty script\r\n");
			
		s = OLC_STORAGE(d);
		
		trig->cmdlist = new CmdlistElement(strtok(s, "\n\r"));
		cmd = trig->cmdlist;

		while ((s = strtok(NULL, "\n\r"))) {
			cmd->next = new CmdlistElement(s);
			cmd = cmd->next;
		}
		
		/* Ok now thats setup, copy it all over! */
		
		*proto = *trig;
		proto->arglist = SSShare(trig->arglist);
		proto->name = SSShare(trig->name);
		
		/* HERE IT HAS TO GO THROUGH AND FIX ALL SCRIPTS/TRIGS OF THIS KIND */
		START_ITER(trig_iter, live_trig, TrigData *, trig_list) {
			if (GET_TRIG_RNUM(live_trig) == trig_rnum) {
				SSFree(live_trig->arglist);
				SSFree(live_trig->name);
				
				live_trig->arglist = SSShare(proto->arglist);
				live_trig->name = SSShare(proto->name);

				live_trig->cmdlist = proto->cmdlist;
				live_trig->curr_state = live_trig->cmdlist;
				live_trig->trigger_type = proto->trigger_type;
				live_trig->narg = proto->narg;
				live_trig->data_type = proto->data_type;
				live_trig->depth = 0;
				if (GET_TRIG_WAIT(live_trig))
					GET_TRIG_WAIT(live_trig)->Cancel();
				GET_TRIG_WAIT(live_trig) = NULL;
				
				free_varlist(live_trig->var_list);
			}
		} END_ITER(trig_iter);
	} else {
		CREATE(new_index, IndexData, top_of_trigt + 2);

		/* Recompile the command list from the new script */
		s = OLC_STORAGE(d);
		
		trig->cmdlist = new CmdlistElement(strtok(s, "\n\r"));
		cmd = trig->cmdlist;

		while ((s = strtok(NULL, "\n\r"))) {
			cmd->next = new CmdlistElement(s);
			cmd = cmd->next;
		}
		
		for (i = 0; i <= top_of_trigt; i++) {
			if (!found) {
				if (trig_index[i].vnum > OLC_NUM(d)) {
					found = TRUE;
					trig_rnum = i;
					
					GET_TRIG_RNUM(OLC_TRIG(d)) = trig_rnum;
					new_index[trig_rnum].vnum = OLC_NUM(d);
					new_index[trig_rnum].number = 0;
					new_index[trig_rnum].func = NULL;
					new_index[trig_rnum].proto = proto = new TrigData;
					*proto = *trig;
					proto->name = SSShare(trig->name);
					proto->arglist = SSShare(trig->arglist);
					
					new_index[trig_rnum + 1] = trig_index[trig_rnum];
					proto = (TrigData *)trig_index[trig_rnum].proto;
					proto->nr = trig_rnum + 1;
				} else {
					new_index[i] = trig_index[i];
					new_index[i].proto = trig_index[i].proto;
				}
			} else {
				new_index[i + 1] = trig_index[i];
				proto = (TrigData *)trig_index[i].proto;
				proto->nr = i + 1;
			}
		}
		
		if (!found) {
			trig_rnum = i;
			GET_TRIG_RNUM(OLC_TRIG(d)) = trig_rnum;
			new_index[trig_rnum].vnum = OLC_NUM(d);
			new_index[trig_rnum].number = 0;
			new_index[trig_rnum].func = NULL;
			
			new_index[trig_rnum].proto = proto = new TrigData;
			*proto = *trig;
			proto->name = SSShare(trig->name);
			proto->arglist = SSShare(trig->arglist);
		}
		
		FREE(trig_index);
		
		trig_index = new_index;
		top_of_trigt++;
		
		/* HERE IT HAS TO GO THROUGH AND FIX ALL SCRIPTS/TRIGS OF HIGHER RNUM */
		START_ITER(trig_iter, live_trig, TrigData *, trig_list) {
			if (GET_TRIG_RNUM(live_trig) > trig_rnum)
				GET_TRIG_RNUM(live_trig)++;
		} END_ITER(trig_iter);
				
		/* HERE IT HAS TO GO THROUGH AND FIX ALL PURGED TRIGS OF HIGHER RNUM */
		START_ITER(purged_iter, live_trig, TrigData *, purged_trigs) {
			if (GET_TRIG_RNUM(live_trig) > trig_rnum)
				GET_TRIG_RNUM(live_trig)++;
		} END_ITER(purged_iter);

		/*
		 * Update zone table.
		 */
		for (zone = 0; zone <= top_of_zone_table; zone++)
			for (cmd_no = 0; ZCMD.command != 'S'; cmd_no++) 
				if (ZCMD.command == 'T')
					if (ZCMD.arg2 >= trig_rnum)
						ZCMD.arg2++;

		/*
		 * Update other trigs being edited.
		 */
		START_ITER(desc_iter, dsc, DescriptorData *, descriptor_list) {
			if (STATE(dsc) == CON_SCRIPTEDIT)
				if(GET_TRIG_RNUM(OLC_TRIG(dsc)) >= trig_rnum)
					GET_TRIG_RNUM(OLC_TRIG(dsc))++;
		} END_ITER(desc_iter);
	}
	
	olc_add_to_save_list(zone_table[OLC_ZNUM(d)].number, OLC_SAVE_TRIGGER);
}


/*------------------------------------------------------------------------*/

void scriptedit_save_to_disk(int zone_num) {
	FILE *trig_file;
	int i, zone, top, rtrig_num;
	char *buf;
	char *bitBuf;
	char *fname = get_buffer(MAX_INPUT_LENGTH);
	TrigData *trig;
	CmdlistElement *cmd;
	
	zone = zone_table[zone_num].number;
	top = zone_table[zone_num].top;
	
	sprintf(fname, TRG_PREFIX "%i.new", zone);
	
	if (!(trig_file = fopen(fname, "w"))) {
		mudlogf(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: Can't open trig file \"%s\"", fname);
		release_buffer(fname);
		return;
	}
	
	buf = get_buffer(MAX_STRING_LENGTH);
	bitBuf = get_buffer(MAX_INPUT_LENGTH);
	
	for (i = zone * 100; i <= top; i++) {
		if ((rtrig_num = real_trigger(i)) != -1) {
			trig = (TrigData *)trig_index[rtrig_num].proto;
			
			if (IS_SET(GET_TRIG_TYPE(trig), TRIG_DELETED))
				continue;
		
			if (fprintf(trig_file, "#%d\n", i) < 0) {
				mudlogf(BRF, LVL_STAFF, TRUE, "SYSERR: OLC: Can't write trig file!");
				fclose(trig_file);
				release_buffer(fname);
				release_buffer(buf);
				release_buffer(bitBuf);
				return;
			}
			sprintbits(GET_TRIG_TYPE(trig), bitBuf);

			/* Build the text for the script */
			*buf = '\0';
			for(cmd = trig->cmdlist; cmd; cmd = cmd->next) {
				if (cmd->cmd)
					strcat(buf, cmd->cmd);
				strcat(buf, "\r\n");
			}
			strip_string(buf);

			fprintf(trig_file,	"%s~\n"
								"%s %d %d\n"
								"%s~\n"
								"%s~\n",
					SSData(GET_TRIG_NAME(trig)) ? SSData(GET_TRIG_NAME(trig)) : "unknown trigger",
					bitBuf, GET_TRIG_NARG(trig), GET_TRIG_DATA_TYPE(trig),
					SSData(GET_TRIG_ARG(trig)) ? SSData(GET_TRIG_ARG(trig)) : "",
					*buf ? buf : "* Empty script\r\n");
		}
	}
	release_buffer(bitBuf);
	
	fprintf(trig_file, "$~\n");
	fclose(trig_file);
	
	sprintf(buf, TRG_PREFIX "%d.trg", zone);
	
	remove(buf);
	rename(fname, buf);
	
	release_buffer(fname);
	release_buffer(buf);
	
	olc_remove_from_save_list(zone_table[zone_num].number, OLC_SAVE_TRIGGER);
}


/*------------------------------------------------------------------------*/

char *scriptedit_get_class(TrigData *trig) {
	switch(GET_TRIG_DATA_TYPE(trig)) {
		case MOB_TRIGGER:
			return "Mob Script";
		case OBJ_TRIGGER:
			return "Object Script";
		case WLD_TRIGGER:
			return "Room Script";
		default:
			return "Unknown";
	
	}

}


void scriptedit_display_classes(DescriptorData *d) {
#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	send_to_char(	"1) Mob script\r\n"
					"2) Object script\r\n"
					"3) Room script\r\n"
					"Enter script type [0 to exit]: ", d->character);
	OLC_MODE(d) = SCRIPTEDIT_CLASS;
}




char *scriptedit_get_type(DescriptorData *d) {
	static char buf[256];
	if (!GET_TRIG_TYPE(OLC_TRIG(d))) {
		strcpy(buf, "Undefined");
	} else {
		switch(GET_TRIG_DATA_TYPE(OLC_TRIG(d))) {
			case MOB_TRIGGER:
				sprintbit(GET_TRIG_TYPE(OLC_TRIG(d)), mtrig_types, buf);
				break;
			case OBJ_TRIGGER:
				sprintbit(GET_TRIG_TYPE(OLC_TRIG(d)), otrig_types, buf);
				break;
			case WLD_TRIGGER:
				sprintbit(GET_TRIG_TYPE(OLC_TRIG(d)), wtrig_types, buf);
				break;
			default:
				strcpy(buf, "Undefined");
				break;
		}
	}
	return buf;
}


void scriptedit_display_types(DescriptorData *d) {
	int num_trig_types, i, columns = 0;
	char **trig_types;
	char *bitBuf;
	
	switch (GET_TRIG_DATA_TYPE(OLC_TRIG(d))) {
		case WLD_TRIGGER:	trig_types = wtrig_types;	num_trig_types = NUM_WTRIG_TYPES;	break;
		case OBJ_TRIGGER:	trig_types = otrig_types;	num_trig_types = NUM_OTRIG_TYPES;	break;
		case MOB_TRIGGER:	trig_types = mtrig_types;	num_trig_types = NUM_MTRIG_TYPES;	break;
		default:
			send_to_char(	"Invalid script class - set class first.\r\n"
							"Enter choice : ", d->character);
			return;
	}
	bitBuf = get_buffer(256);
	
#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	for (i = 0; i < num_trig_types; i++) {
		d->character->Send("`g%2d`n) `c %-20.20s  %s", i + 1, trig_types[i],
			!(++columns % 2) ? "\r\n" : "");
	}
	sprintbit(GET_TRIG_TYPE(OLC_TRIG(d)), trig_types, bitBuf);
	d->character->Send(
				"\r\n"
				"`nCurrent types   : `c%s\r\n"
				"`nEnter types (0 to quit): ",
				bitBuf);
	release_buffer(bitBuf);
	OLC_MODE(d) = SCRIPTEDIT_TYPE;
}


/* Menu functions */

#define PAGE_LENGTH	78
/* the main menu */
void scriptedit_disp_menu(DescriptorData *d) {
	char *buf = get_buffer(MAX_STRING_LENGTH);
	char *str, *bufptr;
	UInt8	x;

	bufptr = buf;
	x = 0;
	if (OLC_STORAGE(d)) {
		for (str = OLC_STORAGE(d); *str; str++) {
			if (*str == '\n')	x++;	// Newline puts us on the next line.
			*bufptr++ = *str;
			if (x == 10)
				break;
		}
	}
	*bufptr = '\0';
	
	d->character->Send(
#if defined(CLEAR_SCREEN)
			"\x1B[H\x1B[J"
#endif
			"-- Trig Number:  [`c%d`n]\r\n"
			"`g1`n) Script Name : `c%s\r\n"
			"`g2`n) Script Class: `y%s\r\n"
			"`g3`n) Triggers    : `y%s\r\n"
			"`g4`n) Arg         : `c%s\r\n"
			"`g5`n) Num Arg     : %d\r\n"
			"`g6`n) Script (First 10 lines)\r\n"
			"   `b-----------------------`n\r\n"
			"%s\r\n"
			"`gQ`n) Quit\r\n"
			"Enter choice : ",
			
			OLC_NUM(d),
			SSData(GET_TRIG_NAME(OLC_TRIG(d))),
			scriptedit_get_class(OLC_TRIG(d)),
			scriptedit_get_type(d),
			SSData(GET_TRIG_ARG(OLC_TRIG(d))) ? SSData(GET_TRIG_ARG(OLC_TRIG(d))) : "<NONE>",
			GET_TRIG_NARG(OLC_TRIG(d)),
			buf);

//	page_string(d, buf, true);

	OLC_MODE(d) = SCRIPTEDIT_MAIN_MENU;
	release_buffer(buf);
}


//	The main loop
void scriptedit_parse(DescriptorData *d, char *arg) {
	int i, num_trig_types;

	switch (OLC_MODE(d)) {
		case SCRIPTEDIT_CONFIRM_SAVESTRING:
			switch (*arg) {
				case 'y':
				case 'Y':
					send_to_char("Saving script to memory.\r\n", d->character);
					scriptedit_save_internally(d);
					mudlogf(NRM, LVL_BUILDER, TRUE, "OLC: %s edits script %d", d->character->RealName(), OLC_NUM(d));
				case 'n':
				case 'N':
					cleanup_olc(d, CLEANUP_ALL);
					break;
				default:
					send_to_char("Invalid choice!\r\n"
								"Do you wish to save the script? ", d->character);
					break;
			}
			return;			/* end of SCRIPTEDIT_CONFIRM_SAVESTRING */

			case SCRIPTEDIT_MAIN_MENU:
				switch (*arg) {
					case 'q':
					case 'Q':
						if (OLC_VAL(d)) {		/* Something was modified */
							send_to_char("Do you wish to save the changes to this script? (y/n) : ", d->character);
							OLC_MODE(d) = SCRIPTEDIT_CONFIRM_SAVESTRING;
						} else
							cleanup_olc(d, CLEANUP_ALL);
						break;
					case '1':
						send_to_char("Enter the name of this script: ", d->character);
						OLC_MODE(d) = SCRIPTEDIT_NAME;
						return;
					case '2':
						scriptedit_display_classes(d);
						return;
					case '3':
						scriptedit_display_types(d);
						return;
					case '4':
						send_to_char("Enter the argument for this script: ", d->character);
						OLC_MODE(d) = SCRIPTEDIT_ARG;
						return;
					case '5':
						send_to_char("Enter the number argument for this script: ", d->character);
						OLC_MODE(d) = SCRIPTEDIT_NARG;
						return;
					case '6':
						OLC_MODE(d) = SCRIPTEDIT_SCRIPT;
						SEND_TO_Q("\x1B[H\x1B[J", d);
						SEND_TO_Q("Edit the script: (/s saves /h for help)\r\n\r\n", d);
						d->backstr = NULL;
						if (OLC_STORAGE(d)) {
							SEND_TO_Q(OLC_STORAGE(d), d);
							d->backstr = str_dup(OLC_STORAGE(d));
						}
						d->str = &OLC_STORAGE(d);
						d->max_str = MAX_STRING_LENGTH;
						d->mail_to = 0;
						OLC_VAL(d) = 1;
						break;
					default:
						scriptedit_disp_menu(d);
						break;
				}
				return;
			case SCRIPTEDIT_NAME:
				SSFree(GET_TRIG_NAME(OLC_TRIG(d)));
				GET_TRIG_NAME(OLC_TRIG(d)) = SSCreate(*arg ? arg : "un-named script");
				break;
			case SCRIPTEDIT_CLASS:
				i = atoi(arg);
				if (i > 0) {
					i--;
					if ((i >= MOB_TRIGGER) && (i <= WLD_TRIGGER))
						GET_TRIG_DATA_TYPE(OLC_TRIG(d)) = i;
					else
						send_to_char("Invalid class.\r\n"
									 "Enter script type [0 to exit]: ", d->character);
				}
				break;
			case SCRIPTEDIT_TYPE:
				if ((i = atoi(arg)) == 0) {
					scriptedit_disp_menu(d);
					return;
				} else {
					switch (GET_TRIG_DATA_TYPE(OLC_TRIG(d))) {
						case MOB_TRIGGER:	num_trig_types = NUM_MTRIG_TYPES;	break;
						case OBJ_TRIGGER:	num_trig_types = NUM_OTRIG_TYPES;	break;
						case WLD_TRIGGER:	num_trig_types = NUM_WTRIG_TYPES;	break;
						default:			num_trig_types = NUM_WTRIG_TYPES;	break;
					}
					if (!((i < 0) || (i > num_trig_types)))
						TOGGLE_BIT(GET_TRIG_TYPE(OLC_TRIG(d)), 1 << (i - 1));
					scriptedit_display_types(d);
					OLC_VAL(d) = 1;
					return;
				}
			case SCRIPTEDIT_ARG:
				SSFree(GET_TRIG_ARG(OLC_TRIG(d)));
				GET_TRIG_ARG(OLC_TRIG(d)) = SSCreate(*arg ? arg : NULL);
				break;
			case SCRIPTEDIT_NARG:
				GET_TRIG_NARG(OLC_TRIG(d)) = atoi(arg);
				break;
			case SCRIPTEDIT_SCRIPT:
				mudlog("SYSERR: Reached SCRIPTEDIT_ENTRY in scriptedit_parse", BRF, LVL_BUILDER, TRUE);
				break;
			default:
				/* we should never get here */
				break;
	}
	OLC_VAL(d) = 1;
	scriptedit_disp_menu(d);
}
