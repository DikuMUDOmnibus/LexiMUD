/*
 * File: hedit.c
 * Comment: OLC for MUDs -- this one edits help files
 * By Steve Wolfe
 * for use with OasisOLC
 */



#include "help.h"
#include "descriptors.h"
#include "interpreter.h"
#include "comm.h"
#include "utils.h"
#include "db.h"
#include "olc.h"
#include "buffer.h"
#include "handler.h"


void free_help(HelpElement *help);

/* function protos */
void hedit_disp_menu(DescriptorData *d);
void hedit_parse(DescriptorData *d, char *arg);
void hedit_setup_new(DescriptorData *d, char *arg);
void hedit_setup_existing(DescriptorData *d);
void hedit_save_to_disk(DescriptorData *d);
void hedit_save_internally(DescriptorData *d);
void index_boot(int mode);
RNum find_help_rnum(char *keyword);


/*
 * Utils and exported functions.
 */
void hedit_setup_new(DescriptorData *d, char *arg) {
//	CREATE(OLC_HELP(d), HelpElement, 1);
	OLC_HELP(d) = new HelpElement;
	
	OLC_HELP(d)->keywords = str_dup(arg);
	OLC_HELP(d)->entry = str_dup("This help entry is unfinished.\r\n");
	OLC_HELP(d)->min_level = 0;
	hedit_disp_menu(d);
	OLC_VAL(d) = 0;
}

/*------------------------------------------------------------------------*/

void hedit_setup_existing(DescriptorData *d) {
//	CREATE(OLC_HELP(d), HelpElement, 1);
	RNum	real_num = OLC_ZNUM(d);
	OLC_HELP(d) = new HelpElement;
	OLC_HELP(d)->keywords = str_dup(help_table[real_num].keywords);
	OLC_HELP(d)->entry = str_dup(help_table[real_num].entry);
	OLC_HELP(d)->min_level = help_table[real_num].min_level;
	OLC_VAL(d) = 0;
	hedit_disp_menu(d);
}


void hedit_save_internally(DescriptorData *d) {
	HelpElement *new_help_table = NULL;
	char *temp = NULL;
	int i;
	RNum	rnum;
	
	rnum = OLC_ZNUM(d);
	
	/* add a new help element into the list */
	if (rnum > 0) {
		help_table[rnum].Free();
		help_table[rnum] = *OLC_HELP(d);
	} else {
		CREATE(new_help_table, HelpElement, top_of_helpt + 2);
		for (i = 0; i <= top_of_helpt; i++)
			new_help_table[i] = help_table[i];
		new_help_table[++top_of_helpt] = *OLC_HELP(d);
		free(help_table);
		help_table = new_help_table;
	}
	olc_add_to_save_list(HEDIT_PERMISSION, OLC_SAVE_HELP);
}


/*------------------------------------------------------------------------*/

void hedit_save_to_disk(DescriptorData *d) {
	FILE *fp;
	char *buf;
	int i;
	HelpElement *	help;
	
	if (!(fp = fopen(HELP_FILE ".new", "w+"))) {
		log("SYSERR: Can't open help file '%s'", HELP_FILE);
		exit(1);
	}
	
	buf = get_buffer(32384);
	
	for (i = 0; i <= top_of_helpt; i++) {
		help = (help_table + i);
		
		if (!help->keywords || !*help->keywords ||
			help->entry || !*help->entry)	continue;
		
		/*. Remove the '\r\n' sequences from entry . */
		strcpy(buf, help->entry);	strip_string(buf);
		
		fprintf(fp, "%s\n"
					"%s#%d\n",
				help->keywords,
				buf, help_table[i].min_level);
	}
	release_buffer(buf);

	fprintf(fp, "$\n");
	fclose(fp);
	
	remove(HELP_FILE);
	rename(HELP_FILE ".new", HELP_FILE);
	
	olc_remove_from_save_list(HEDIT_PERMISSION, OLC_SAVE_HELP);
}

/*------------------------------------------------------------------------*/

/* Menu functions */

/* the main menu */
void hedit_disp_menu(DescriptorData *d) {
	HelpElement *help = OLC_HELP(d);

	d->character->Send(
#if defined(CLEAR_SCREEN)
			"\x1B[H\x1B[J"
#endif
			"`g1`n) Keywords    : `y%s\r\n"
			"`g2`n) Entry       :\r\n`y%s"
			"`g3`n) Min Level   : `y%d\r\n"
			"`gQ`n) Quit\r\n"
			"Enter choice : ",
			
			help->keywords,
			help->entry,
			help->min_level);

	OLC_MODE(d) = HEDIT_MAIN_MENU;
}

/*
 * The main loop
 */

void hedit_parse(DescriptorData *d, char *arg) {
	int i;

	switch (OLC_MODE(d)) {
		case HEDIT_CONFIRM_SAVESTRING:
			switch (*arg) {
				case 'y':
				case 'Y':
					hedit_save_internally(d);
					mudlogf(CMP, LVL_STAFF, TRUE, "OLC: %s edits help.", d->character->RealName());

					/* do not free the strings.. just the structure */
					cleanup_olc(d, CLEANUP_STRUCTS);
					send_to_char("Help saved to memory.\r\n", d->character);
					break;
				case 'n':
				case 'N':
					/* free everything up, including strings etc */
					cleanup_olc(d, CLEANUP_ALL);
					break;
				default:
					send_to_char("Invalid choice!\r\nDo you wish to save this help internally? ", d->character);
					break;
			}
			return;			/* end of HEDIT_CONFIRM_SAVESTRING */

		case HEDIT_MAIN_MENU:
			switch (*arg) {
				case 'q':
				case 'Q':
					if (OLC_VAL(d)) {		/* Something was modified */
						send_to_char("Do you wish to save this help internally? ", d->character);
						OLC_MODE(d) = HEDIT_CONFIRM_SAVESTRING;
					} else
						cleanup_olc(d, CLEANUP_ALL);
					break;
				case '1':
					send_to_char("Enter keywords:-\r\n| ", d->character);
					OLC_MODE(d) = HEDIT_KEYWORDS;
					break;
				case '2':
					OLC_MODE(d) = HEDIT_ENTRY;
					SEND_TO_Q("\x1B[H\x1B[J", d);
					SEND_TO_Q("Enter the help info: (/s saves /h for help)\r\n\r\n", d);
					d->backstr = NULL;
					if (OLC_HELP(d)->entry) {
						SEND_TO_Q(OLC_HELP(d)->entry, d);
						d->backstr = str_dup(OLC_HELP(d)->entry);
					}
					d->str = &OLC_HELP(d)->entry;
					d->max_str = MAX_STRING_LENGTH;
					d->mail_to = 0;
					OLC_VAL(d) = 1;
					break;
				case '3':
					send_to_char("Enter the minimum level: ", d->character);
					OLC_MODE(d) = HEDIT_MIN_LEVEL;
					return;
				default:
					hedit_disp_menu(d);
					break;
			}
			return;
		
		case HEDIT_KEYWORDS:
			if (strlen(arg) > MAX_HELP_KEYWORDS)	arg[MAX_HELP_KEYWORDS - 1] = '\0';
			if (OLC_HELP(d)->keywords);				free(OLC_HELP(d)->keywords);
			OLC_HELP(d)->keywords = str_dup(*arg ? arg : "UNDEFINED");
			break;
			
		case HEDIT_ENTRY:
			/* should never get here */
			mudlog("SYSERR: Reached HEDIT_ENTRY in hedit_parse", BRF, LVL_BUILDER, TRUE);
			break;

		case HEDIT_MIN_LEVEL:
			if (*arg) {
				i = atoi(arg);
				if ((i < 0) || (i > LVL_CODER)) {
					hedit_disp_menu(d);
					return;
				} else
					OLC_HELP(d)->min_level = i;
			} else {
				hedit_disp_menu(d);
				return;
			}
			break;
		default:
			/* we should never get here */
			break;
	}
	OLC_VAL(d) = 1;
	hedit_disp_menu(d);
}


RNum find_help_rnum(char *keyword) {
	int i;

	for (i = 0; i < top_of_helpt; i++)
		if (isname(keyword, help_table[i].keywords))
			return i;

	return -1;
}


void free_help(HelpElement *help) {
	help->keywords = NULL;
	help->entry = NULL;
	delete help;
}
