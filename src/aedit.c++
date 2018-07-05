/*
 * File: aedit.c
 * Comment: OLC for MUDs -- this one edits socials
 * by Michael Scott <scottm@workcomm.net> -- 06/10/96
 * for use with OasisOLC
 * ftpable from ftp.circlemud.org:/pub/CircleMUD/contrib/code
 */



#include "characters.h"
#include "descriptors.h"
#include "interpreter.h"
#include "comm.h"
#include "utils.h"
#include "db.h"
#include "olc.h"
#include "socials.h"

extern char			*position_types[];

/* WARNING: if you have added diagonal directions and have them at the
 * beginning of the command list.. change this value to 11 or 15 (depending) */
/* reserve these commands to come straight from the cmd list then start
 * sorting */
#define RESERVE_CMDS		7

/* external functs */
void sort_commands(void); /* aedit patch -- M. Scott */
void create_command_list(void);
void free_action(struct social_messg *action);


/* function protos */
void aedit_disp_menu(DescriptorData * d);
void aedit_disp_positions_menu(DescriptorData * d);
void aedit_parse(DescriptorData * d, char *arg);
void aedit_setup_new(DescriptorData *d);
void aedit_setup_existing(DescriptorData *d, int real_num);
void aedit_save_to_disk(int zone_num);
void aedit_save_internally(DescriptorData *d);



/*
 * Utils and exported functions.
 */

void aedit_setup_new(DescriptorData *d) {
	CREATE(OLC_ACTION(d), struct social_messg, 1);
	OLC_ACTION(d)->command = str_dup(OLC_STORAGE(d));
	OLC_ACTION(d)->sort_as = str_dup(OLC_STORAGE(d));
	OLC_ACTION(d)->hide    = 0;
	OLC_ACTION(d)->min_victim_position = POS_STANDING;
	OLC_ACTION(d)->min_char_position   = POS_STANDING;
	OLC_ACTION(d)->min_level_char      = 0;
	OLC_ACTION(d)->char_no_arg = str_dup("This action is unfinished.");
	OLC_ACTION(d)->others_no_arg = str_dup("This action is unfinished.");
	OLC_ACTION(d)->char_found = NULL;
	OLC_ACTION(d)->others_found = NULL;
	OLC_ACTION(d)->vict_found = NULL;
	OLC_ACTION(d)->not_found = NULL;
	OLC_ACTION(d)->char_auto = NULL;
	OLC_ACTION(d)->others_auto = NULL;
	OLC_ACTION(d)->char_body_found = NULL;
	OLC_ACTION(d)->others_body_found = NULL;
	OLC_ACTION(d)->vict_body_found = NULL;
	OLC_ACTION(d)->char_obj_found = NULL;
	OLC_ACTION(d)->others_obj_found = NULL;
	aedit_disp_menu(d);
	OLC_VAL(d) = 0;
}

/*------------------------------------------------------------------------*/

void aedit_setup_existing(DescriptorData *d, int real_num) {
	CREATE(OLC_ACTION(d), struct social_messg, 1);
	OLC_ACTION(d)->command = str_dup(soc_mess_list[real_num].command);
	OLC_ACTION(d)->sort_as = str_dup(soc_mess_list[real_num].sort_as);
	OLC_ACTION(d)->hide    = soc_mess_list[real_num].hide;
	OLC_ACTION(d)->min_victim_position = soc_mess_list[real_num].min_victim_position;
	OLC_ACTION(d)->min_char_position   = soc_mess_list[real_num].min_char_position;
	OLC_ACTION(d)->min_level_char      = soc_mess_list[real_num].min_level_char;
	if (soc_mess_list[real_num].char_no_arg)
		OLC_ACTION(d)->char_no_arg = str_dup(soc_mess_list[real_num].char_no_arg);
	if (soc_mess_list[real_num].others_no_arg)
		OLC_ACTION(d)->others_no_arg = str_dup(soc_mess_list[real_num].others_no_arg);
	if (soc_mess_list[real_num].char_found)
		OLC_ACTION(d)->char_found = str_dup(soc_mess_list[real_num].char_found);
	if (soc_mess_list[real_num].others_found)
		OLC_ACTION(d)->others_found = str_dup(soc_mess_list[real_num].others_found);
	if (soc_mess_list[real_num].vict_found)
		OLC_ACTION(d)->vict_found = str_dup(soc_mess_list[real_num].vict_found);
	if (soc_mess_list[real_num].not_found)
		OLC_ACTION(d)->not_found = str_dup(soc_mess_list[real_num].not_found);
	if (soc_mess_list[real_num].char_auto)
		OLC_ACTION(d)->char_auto = str_dup(soc_mess_list[real_num].char_auto);
	if (soc_mess_list[real_num].others_auto)
		OLC_ACTION(d)->others_auto = str_dup(soc_mess_list[real_num].others_auto);
	if (soc_mess_list[real_num].char_body_found)
		OLC_ACTION(d)->char_body_found = str_dup(soc_mess_list[real_num].char_body_found);
	if (soc_mess_list[real_num].others_body_found)
		OLC_ACTION(d)->others_body_found = str_dup(soc_mess_list[real_num].others_body_found);
	if (soc_mess_list[real_num].vict_body_found)
		OLC_ACTION(d)->vict_body_found = str_dup(soc_mess_list[real_num].vict_body_found);
	if (soc_mess_list[real_num].char_obj_found)
		OLC_ACTION(d)->char_obj_found = str_dup(soc_mess_list[real_num].char_obj_found);
	if (soc_mess_list[real_num].others_obj_found)
		OLC_ACTION(d)->others_obj_found = str_dup(soc_mess_list[real_num].others_obj_found);
	OLC_VAL(d) = 0;
	aedit_disp_menu(d);
}


      
void aedit_save_internally(DescriptorData *d) {
	struct social_messg *new_soc_mess_list = NULL;
	int i;

	/* add a new social into the list */
	if (OLC_ZNUM(d) > top_of_socialt)  {
		CREATE(new_soc_mess_list, struct social_messg, top_of_socialt + 2);
		for (i = 0; i <= top_of_socialt; i++)
			new_soc_mess_list[i] = soc_mess_list[i];
		new_soc_mess_list[++top_of_socialt] = *OLC_ACTION(d);
		FREE(soc_mess_list);
		soc_mess_list = new_soc_mess_list;
		create_command_list();
		sort_commands();
	}
	/* pass the editted action back to the list - no need to add */
	else {
		i = find_command(OLC_ACTION(d)->command);
		OLC_ACTION(d)->act_nr = soc_mess_list[OLC_ZNUM(d)].act_nr;
		/* why did i do this..? hrm */
		free_action(soc_mess_list + OLC_ZNUM(d));
		soc_mess_list[OLC_ZNUM(d)] = *OLC_ACTION(d);
		if (i > -1) {
			complete_cmd_info[i].command = soc_mess_list[OLC_ZNUM(d)].command;
			complete_cmd_info[i].sort_as = soc_mess_list[OLC_ZNUM(d)].sort_as;
			complete_cmd_info[i].minimum_position = soc_mess_list[OLC_ZNUM(d)].min_char_position;
			complete_cmd_info[i].minimum_level	   = soc_mess_list[OLC_ZNUM(d)].min_level_char;
		}
	}
	olc_add_to_save_list(AEDIT_PERMISSION, OLC_SAVE_ACTION);
}


/*------------------------------------------------------------------------*/

void aedit_save_to_disk(int zone_num) {
	FILE *fp;
	int i;
	
	if (!(fp = fopen(SOCMESS_FILE, "w+")))  {
		log("SYSERR: Can't open socials file '%s'", SOCMESS_FILE);
		exit(1);
	}

	for (i = 0; i <= top_of_socialt; i++)  {
		fprintf(fp, "~%s %s %d %d %d %d\n",
				soc_mess_list[i].command,
				soc_mess_list[i].sort_as,
				soc_mess_list[i].hide,
				soc_mess_list[i].min_char_position,
				soc_mess_list[i].min_victim_position,
				soc_mess_list[i].min_level_char);
//		fputs(buf, fp);
		fprintf(fp, "%s\n%s\n%s\n%s\n",
				((soc_mess_list[i].char_no_arg)?soc_mess_list[i].char_no_arg:"#"),
				((soc_mess_list[i].others_no_arg)?soc_mess_list[i].others_no_arg:"#"),
				((soc_mess_list[i].char_found)?soc_mess_list[i].char_found:"#"),
				((soc_mess_list[i].others_found)?soc_mess_list[i].others_found:"#"));
//		fputs(buf, fp);
		fprintf(fp, "%s\n%s\n%s\n%s\n",
				((soc_mess_list[i].vict_found)?soc_mess_list[i].vict_found:"#"),
				((soc_mess_list[i].not_found)?soc_mess_list[i].not_found:"#"),
				((soc_mess_list[i].char_auto)?soc_mess_list[i].char_auto:"#"),
				((soc_mess_list[i].others_auto)?soc_mess_list[i].others_auto:"#"));
//		fputs(buf, fp);
		fprintf(fp, "%s\n%s\n%s\n",
				((soc_mess_list[i].char_body_found)?soc_mess_list[i].char_body_found:"#"),
				((soc_mess_list[i].others_body_found)?soc_mess_list[i].others_body_found:"#"),
				((soc_mess_list[i].vict_body_found)?soc_mess_list[i].vict_body_found:"#"));
//		fputs(buf, fp);
		fprintf(fp, "%s\n%s\n\n",
				((soc_mess_list[i].char_obj_found)?soc_mess_list[i].char_obj_found:"#"),
				((soc_mess_list[i].others_obj_found)?soc_mess_list[i].others_obj_found:"#"));
//		fputs(buf, fp);
	}
   
	fprintf(fp, "$\n");
	fclose(fp);
	olc_remove_from_save_list(AEDIT_PERMISSION, OLC_SAVE_ACTION);
}

/*------------------------------------------------------------------------*/

/* Menu functions */

/* the main menu */
void aedit_disp_menu(DescriptorData * d) {
	struct social_messg *action = OLC_ACTION(d);
	CharData *ch        = d->character;
	
	d->character->Send("\x1B[H\x1B[J"
			"`n-- Action editor\r\n\r\n"
			"`gn`n) Command         : `y%-15.15s `g1`n) Sort as Command  : `y%-15.15s\r\n"
			"`g2`n) Min Position[CH]: `c%-8.8s        `g3`n) Min Position [VT]: `c%-8.8s\r\n"
			"`g4`n) Min Level   [CH]: `c%-3d             `g5`n) Show if Invisible: `c%s\r\n"
			"`ga`n) Char    [NO ARG]: `c%s\r\n"
			"`gb`n) Others  [NO ARG]: `c%s\r\n"
			"`gc`n) Char [NOT FOUND]: `c%s\r\n"
			"`gd`n) Char  [ARG SELF]: `c%s\r\n"
			"`ge`n) Others[ARG SELF]: `c%s\r\n"
			"`gf`n) Char      [VICT]: `c%s\r\n"
			"`gg`n) Others    [VICT]: `c%s\r\n"
			"`gh`n) Victim    [VICT]: `c%s\r\n"
			"`gi`n) Char  [BODY PRT]: `c%s\r\n"
			"`gj`n) Others[BODY PRT]: `c%s\r\n"
			"`gk`n) Victim[BODY PRT]: `c%s\r\n"
			"`gl`n) Char       [OBJ]: `c%s\r\n"
			"`gm`n) Others     [OBJ]: `c%s\r\n"
			"`gq`n) Quit\r\n",
			action->command, action->sort_as,
			position_types[action->min_char_position], position_types[action->min_victim_position],
			action->min_level_char, (action->hide?"HIDDEN" : "NOT HIDDEN"),
			action->char_no_arg ? action->char_no_arg : "<Null>",
			action->others_no_arg ? action->others_no_arg : "<Null>",
			action->not_found ? action->not_found : "<Null>",
			action->char_auto ? action->char_auto : "<Null>",
			action->others_auto ? action->others_auto : "<Null>",
			action->char_found ? action->char_found : "<Null>",
			action->others_found ? action->others_found : "<Null>",
			action->vict_found ? action->vict_found : "<Null>",
			action->char_body_found ? action->char_body_found : "<Null>",
			action->others_body_found ? action->others_body_found : "<Null>",
			action->vict_body_found ? action->vict_body_found : "<Null>",
			action->char_obj_found ? action->char_obj_found : "<Null>",
			action->others_obj_found ? action->others_obj_found : "<Null>");

	d->character->Send("Enter choice: ");

	OLC_MODE(d) = AEDIT_MAIN_MENU;
}


void aedit_disp_positions_menu(DescriptorData * d) {
	int counter;
	
	for (counter = 0; counter <= POS_STANDING; counter++)
		d->character->Send("`g%2d`n) %-20.20s \r\n", counter, position_types[counter]);
}

/*
 * The main loop
 */

void aedit_parse(DescriptorData * d, char *arg) {
	int i;

	switch (OLC_MODE(d)) {
		case AEDIT_CONFIRM_SAVESTRING:
			switch (*arg) {
				case 'y': case 'Y':
					aedit_save_internally(d);
					mudlogf(NRM, LVL_SRSTAFF, TRUE, "OLC: %s edits action %s", d->character->RealName(),
							OLC_ACTION(d)->command);
					/* do not free the strings.. just the structure */
					cleanup_olc(d, CLEANUP_STRUCTS);
					send_to_char("Action saved to memory.\r\n", d->character);
					break;
				case 'n': case 'N':
					/* free everything up, including strings etc */
					cleanup_olc(d, CLEANUP_ALL);
					break;
				default:
					send_to_char("Invalid choice!\r\nDo you wish to save this action internally? ", d->character);
					break;
			}
			return; /* end of AEDIT_CONFIRM_SAVESTRING */

		case AEDIT_CONFIRM_EDIT:
			switch (*arg)  {
				case 'y': case 'Y':
					aedit_setup_existing(d, OLC_ZNUM(d));
					break;
				case 'q': case 'Q':
					cleanup_olc(d, CLEANUP_ALL);
					break;
				case 'n': case 'N':
					OLC_ZNUM(d)++;
					for (;(OLC_ZNUM(d) <= top_of_socialt); OLC_ZNUM(d)++)
						if (is_abbrev(OLC_STORAGE(d), soc_mess_list[OLC_ZNUM(d)].command)) break;
					if (OLC_ZNUM(d) > top_of_socialt) {
						if (find_command(OLC_STORAGE(d)) > -1)  {
							cleanup_olc(d, CLEANUP_ALL);
							break;
						}
						d->character->Send("Do you wish to add the '%s' action? ", OLC_STORAGE(d));
						OLC_MODE(d) = AEDIT_CONFIRM_ADD;
					} else  {
						d->character->Send("Do you wish to edit the '%s' action? ", soc_mess_list[OLC_ZNUM(d)].command);
						OLC_MODE(d) = AEDIT_CONFIRM_EDIT;
					}
					break;
				default:
					d->character->Send("Invalid choice!\r\nDo you wish to edit the '%s' action? ", soc_mess_list[OLC_ZNUM(d)].command);
					break;
			}
			return;

		case AEDIT_CONFIRM_ADD:
			switch (*arg)  {
				case 'y': case 'Y':
					aedit_setup_new(d);
					break;
				case 'n': case 'N': case 'q': case 'Q':
					cleanup_olc(d, CLEANUP_ALL);
					break;
				default:
					d->character->Send("Invalid choice!\r\nDo you wish to add the '%s' action? ", OLC_STORAGE(d));
					break;
			}
			return;

		case AEDIT_MAIN_MENU:
			switch (*arg) {
				case 'q': case 'Q':
					if (OLC_VAL(d))  { /* Something was modified */
						send_to_char("Do you wish to save this action internally? ", d->character);
						OLC_MODE(d) = AEDIT_CONFIRM_SAVESTRING;
					} else
						cleanup_olc(d, CLEANUP_ALL);
					break;
				case 'n':
					send_to_char("Enter action name: ", d->character);
					OLC_MODE(d) = AEDIT_ACTION_NAME;
					break;
				case '1':
					send_to_char("Enter sort info for this action (for the command listing): ", d->character);
					OLC_MODE(d) = AEDIT_SORT_AS;
					break;
				case '2':
					aedit_disp_positions_menu(d);
					send_to_char("Enter the minimum position the Character has to be in to activate social [0 - 8]: ", d->character);
					OLC_MODE(d) = AEDIT_MIN_CHAR_POS;
					break;
				case '3':
					aedit_disp_positions_menu(d);
					send_to_char("Enter the minimum position the Victim has to be in to activate social [0 - 8]: ", d->character);
					OLC_MODE(d) = AEDIT_MIN_VICT_POS;
					break;
				case '4':
					send_to_char("Enter new minimum level for social: ", d->character);
					OLC_MODE(d) = AEDIT_MIN_CHAR_LEVEL;
					break;
				case '5':
					OLC_ACTION(d)->hide = !OLC_ACTION(d)->hide;
					aedit_disp_menu(d);
					OLC_VAL(d) = 1;
					break;
				case 'a': case 'A':
					d->character->Send("Enter social shown to the Character when there is no argument supplied.\r\n[OLD]: %s\r\n[NEW]: ",
							((OLC_ACTION(d)->char_no_arg)?OLC_ACTION(d)->char_no_arg:"NULL"));
					OLC_MODE(d) = AEDIT_NOVICT_CHAR;
					break;
				case 'b': case 'B':
					d->character->Send("Enter social shown to Others when there is no argument supplied.\r\n[OLD]: %s\r\n[NEW]: ",
							((OLC_ACTION(d)->others_no_arg)?OLC_ACTION(d)->others_no_arg:"NULL"));
					OLC_MODE(d) = AEDIT_NOVICT_OTHERS;
					break;
				case 'c': case 'C':
					d->character->Send("Enter text shown to the Character when his victim isnt found.\r\n[OLD]: %s\r\n[NEW]: ",
							((OLC_ACTION(d)->not_found)?OLC_ACTION(d)->not_found:"NULL"));
					OLC_MODE(d) = AEDIT_VICT_NOT_FOUND;
					break;
				case 'd': case 'D':
					d->character->Send("Enter social shown to the Character when it is its own victim.\r\n[OLD]: %s\r\n[NEW]: ",
							((OLC_ACTION(d)->char_auto)?OLC_ACTION(d)->char_auto:"NULL"));
					OLC_MODE(d) = AEDIT_SELF_CHAR;
					break;
				case 'e': case 'E':
					d->character->Send("Enter social shown to Others when the Char is its own victim.\r\n[OLD]: %s\r\n[NEW]: ",
							((OLC_ACTION(d)->others_auto)?OLC_ACTION(d)->others_auto:"NULL"));
					OLC_MODE(d) = AEDIT_SELF_OTHERS;
					break;
				case 'f': case 'F':
					d->character->Send("Enter normal social shown to the Character when the victim is found.\r\n[OLD]: %s\r\n[NEW]: ",
							((OLC_ACTION(d)->char_found)?OLC_ACTION(d)->char_found:"NULL"));
					OLC_MODE(d) = AEDIT_VICT_CHAR_FOUND;
					break;
				case 'g': case 'G':
					d->character->Send("Enter normal social shown to Others when the victim is found.\r\n[OLD]: %s\r\n[NEW]: ",
							((OLC_ACTION(d)->others_found)?OLC_ACTION(d)->others_found:"NULL"));
					OLC_MODE(d) = AEDIT_VICT_OTHERS_FOUND;
					break;
				case 'h': case 'H':
					d->character->Send("Enter normal social shown to the Victim when the victim is found.\r\n[OLD]: %s\r\n[NEW]: ",
							((OLC_ACTION(d)->vict_found)?OLC_ACTION(d)->vict_found:"NULL"));
					OLC_MODE(d) = AEDIT_VICT_VICT_FOUND;
					break;
				case 'i': case 'I':
					d->character->Send("Enter 'body part' social shown to the Character when the victim is found.\r\n[OLD]: %s\r\n[NEW]: ",
							((OLC_ACTION(d)->char_body_found)?OLC_ACTION(d)->char_body_found:"NULL"));
					OLC_MODE(d) = AEDIT_VICT_CHAR_BODY_FOUND;
					break;
				case 'j': case 'J':
					d->character->Send("Enter 'body part' social shown to Others when the victim is found.\r\n[OLD]: %s\r\n[NEW]: ",
							((OLC_ACTION(d)->others_body_found)?OLC_ACTION(d)->others_body_found:"NULL"));
					OLC_MODE(d) = AEDIT_VICT_OTHERS_BODY_FOUND;
					break;
				case 'k': case 'K':
					d->character->Send("Enter 'body part' social shown to the Victim when the victim is found.\r\n[OLD]: %s\r\n[NEW]: ",
							((OLC_ACTION(d)->vict_body_found)?OLC_ACTION(d)->vict_body_found:"NULL"));
					OLC_MODE(d) = AEDIT_VICT_VICT_BODY_FOUND;
					break;
				case 'l': case 'L':
					d->character->Send("Enter 'object' social shown to the Character when the object is found.\r\n[OLD]: %s\r\n[NEW]: ",
							((OLC_ACTION(d)->char_obj_found)?OLC_ACTION(d)->char_obj_found:"NULL"));
					OLC_MODE(d) = AEDIT_OBJ_CHAR_FOUND;
					break;
				case 'm': case 'M':
					d->character->Send("Enter 'object' social shown to the Room when the object is found.\r\n[OLD]: %s\r\n[NEW]: ",
							((OLC_ACTION(d)->others_obj_found)?OLC_ACTION(d)->others_obj_found:"NULL"));
					OLC_MODE(d) = AEDIT_OBJ_OTHERS_FOUND;
					break;
				default:
					aedit_disp_menu(d);
					break;
			}
			return;

		case AEDIT_ACTION_NAME:
			if (*arg) {
				if (strchr(arg,' ')) {
					aedit_disp_menu(d);
					return;
				} else  {
					if (OLC_ACTION(d)->command)
						free(OLC_ACTION(d)->command);
					OLC_ACTION(d)->command = str_dup(arg);
				}
			} else {
				aedit_disp_menu(d);
				return;
			}
			break;

		case AEDIT_SORT_AS:
			if (*arg) {
				if (strchr(arg,' ')) {
					aedit_disp_menu(d);
					return;
				} else  {
					if (OLC_ACTION(d)->sort_as)
						free(OLC_ACTION(d)->sort_as);
					OLC_ACTION(d)->sort_as = str_dup(arg);
				}
			} else {
				aedit_disp_menu(d);
				return;
			}
			break;

		case AEDIT_MIN_CHAR_POS:
		case AEDIT_MIN_VICT_POS:
			if (*arg)  {
				i = atoi(arg);
				if ((i < 0) && (i > POS_STANDING))  {
					aedit_disp_menu(d);
					return;
				} else {
					if (OLC_MODE(d) == AEDIT_MIN_CHAR_POS)
						OLC_ACTION(d)->min_char_position = i;
					else OLC_ACTION(d)->min_victim_position = i;
				}
			} else  {
				aedit_disp_menu(d);
				return;
			}
			break;

		case AEDIT_MIN_CHAR_LEVEL:
			if (*arg)  {
				i = atoi(arg);
				if ((i < 0) || (i > LVL_CODER))  {
					aedit_disp_menu(d);
					return;
				} else
					OLC_ACTION(d)->min_level_char = i;
			} else  {
				aedit_disp_menu(d);
				return;
			}
			break;

		case AEDIT_NOVICT_CHAR:
			if (OLC_ACTION(d)->char_no_arg)
				free(OLC_ACTION(d)->char_no_arg);
			if (*arg)	{
				delete_doubledollar(arg);
				OLC_ACTION(d)->char_no_arg = str_dup(arg);
			} else
				OLC_ACTION(d)->char_no_arg = NULL;
			break;

		case AEDIT_NOVICT_OTHERS:
			if (OLC_ACTION(d)->others_no_arg)
				free(OLC_ACTION(d)->others_no_arg);
			if (*arg)	{
				delete_doubledollar(arg);
				OLC_ACTION(d)->others_no_arg = str_dup(arg);
			} else
				OLC_ACTION(d)->others_no_arg = NULL;
			break;

		case AEDIT_VICT_CHAR_FOUND:
			if (OLC_ACTION(d)->char_found)
				free(OLC_ACTION(d)->char_found);
			if (*arg)	{
				delete_doubledollar(arg);
				OLC_ACTION(d)->char_found = str_dup(arg);
			} else
				OLC_ACTION(d)->char_found = NULL;
			break;

		case AEDIT_VICT_OTHERS_FOUND:
			if (OLC_ACTION(d)->others_found)
				free(OLC_ACTION(d)->others_found);
			if (*arg)	{
				delete_doubledollar(arg);
				OLC_ACTION(d)->others_found = str_dup(arg);
			} else
				OLC_ACTION(d)->others_found = NULL;
			break;

		case AEDIT_VICT_VICT_FOUND:
			if (OLC_ACTION(d)->vict_found)
				free(OLC_ACTION(d)->vict_found);
			if (*arg)	{
				delete_doubledollar(arg);
				OLC_ACTION(d)->vict_found = str_dup(arg);
			} else
				OLC_ACTION(d)->vict_found = NULL;
			break;

		case AEDIT_VICT_NOT_FOUND:
			if (OLC_ACTION(d)->not_found)
				free(OLC_ACTION(d)->not_found);
			if (*arg) {
				delete_doubledollar(arg);
				OLC_ACTION(d)->not_found = str_dup(arg);
			} else
				OLC_ACTION(d)->not_found = NULL;
			break;

		case AEDIT_SELF_CHAR:
			if (OLC_ACTION(d)->char_auto)
				free(OLC_ACTION(d)->char_auto);
			if (*arg)	{
				delete_doubledollar(arg);
				OLC_ACTION(d)->char_auto = str_dup(arg);
			} else
				OLC_ACTION(d)->char_auto = NULL;
			break;

		case AEDIT_SELF_OTHERS:
			if (OLC_ACTION(d)->others_auto)
				free(OLC_ACTION(d)->others_auto);
			if (*arg)	{
				delete_doubledollar(arg);
				OLC_ACTION(d)->others_auto = str_dup(arg);
			} else
				OLC_ACTION(d)->others_auto = NULL;
			break;

		case AEDIT_VICT_CHAR_BODY_FOUND:
			if (OLC_ACTION(d)->char_body_found)
				free(OLC_ACTION(d)->char_body_found);
			if (*arg)	{
				delete_doubledollar(arg);
				OLC_ACTION(d)->char_body_found = str_dup(arg);
			} else
				OLC_ACTION(d)->char_body_found = NULL;
			break;

		case AEDIT_VICT_OTHERS_BODY_FOUND:
			if (OLC_ACTION(d)->others_body_found)
				free(OLC_ACTION(d)->others_body_found);
			if (*arg)	{
				delete_doubledollar(arg);
				OLC_ACTION(d)->others_body_found = str_dup(arg);
			} else
				OLC_ACTION(d)->others_body_found = NULL;
			break;

		case AEDIT_VICT_VICT_BODY_FOUND:
			if (OLC_ACTION(d)->vict_body_found)
				free(OLC_ACTION(d)->vict_body_found);
			if (*arg)	{
				delete_doubledollar(arg);
				OLC_ACTION(d)->vict_body_found = str_dup(arg);
			} else
				OLC_ACTION(d)->vict_body_found = NULL;
			break;

		case AEDIT_OBJ_CHAR_FOUND:
			if (OLC_ACTION(d)->char_obj_found)
				free(OLC_ACTION(d)->char_obj_found);
			if (*arg)	{
				delete_doubledollar(arg);
				OLC_ACTION(d)->char_obj_found = str_dup(arg);
			} else
				OLC_ACTION(d)->char_obj_found = NULL;
			break;

		case AEDIT_OBJ_OTHERS_FOUND:
			if (OLC_ACTION(d)->others_obj_found)
				free(OLC_ACTION(d)->others_obj_found);
			if (*arg)	{
				delete_doubledollar(arg);
				OLC_ACTION(d)->others_obj_found = str_dup(arg);
			} else
				OLC_ACTION(d)->others_obj_found = NULL;
			break;

		default:
			/* we should never get here */
			break;
	}
	OLC_VAL(d) = 1;
	aedit_disp_menu(d);
}
