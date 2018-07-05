/* ************************************************************************
*   File: modify.c                                      Part of CircleMUD *
*  Usage: Run-time modification of game variables                         *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
**************************************************************************/



#include "structs.h"
#include "utils.h"
#include "buffer.h"
#include "interpreter.h"
#include "scripts.h"
#include "handler.h"
#include "find.h"
#include "db.h"
#include "comm.h"
#include "spells.h"
#include "mail.h"
#include "boards.h"
#include "olc.h"

#include "imc-mercbase.h"

/*
 * Function declarations.
 */
void show_string(DescriptorData *d, char *input);

/*
 * Action modes for parse_action().
 */
#define PARSE_FORMAT		0  
#define PARSE_REPLACE		1 
#define PARSE_HELP			2 
#define PARSE_DELETE		3
#define PARSE_INSERT		4
#define PARSE_LIST_NORM		5
#define PARSE_LIST_NUM		6
#define PARSE_EDIT			7

void string_add(DescriptorData *d, char *str);
char *next_page(char *str);
int count_pages(char *str);
void paginate_string(char *str, DescriptorData *d);
void parse_action(int command, char *string, DescriptorData *d);
ACMD(do_skillset);
extern char *MENU;
extern char *skills[];

char *string_fields[] =
{
  "name",
  "short",
  "long",
  "description",
  "title",
  "delete-description",
  "\n"
};


/*
 * Maximum length for text field x+1
 */
int length[] =
{
  15,
  60,
  256,
  240,
  60
};


/*************************************
 * Modification of malloc'ed strings.*
 *************************************/
void parse_action(int command, char *string, DescriptorData *d) {
	int indent = 0, rep_all = 0, flags = 0, total_len, replaced;
	int j = 0;
	int i, line_low, line_high;
	char *s, *t, temp /*, *buf, *buf2 */;
	char *arg, *arg2;
	static char buf[32 * 1024];
	
	switch (command) { 
		case PARSE_HELP:
//			buf = get_buffer(MAX_STRING_LENGTH);
			sprintf(buf, 
					"Editor command formats: /<letter>\r\n\r\n"
					"/a         -  aborts editor\r\n"
					"/c         -  clears buffer\r\n"
					"/d#        -  deletes a line #\r\n"
					"/e# <text> -  changes the line at # with <text>\r\n"
					"/f         -  formats text\r\n"
					"/fi        -  indented formatting of text\r\n"
					"/h         -  list text editor commands\r\n"
					"/i# <text> -  inserts <text> before line #\r\n"
					"/l         -  lists buffer\r\n"
					"/n         -  lists buffer with line numbers\r\n"
					"/r 'a' 'b' -  replace 1st occurance of text <a> in buffer with text <b>\r\n"
					"/ra 'a' 'b'-  replace all occurances of text <a> within buffer with text <b>\r\n"
					"              usage: /r[a] 'pattern' 'replacement'\r\n"
					"/s         -  saves text\r\n");
			SEND_TO_Q(buf, d);
//			release_buffer(buf);
			break;
		case PARSE_FORMAT: 
			while (isalpha(string[j]) && j < 2) {
				switch (string[j]) {
					case 'i':
						if (!indent) {
							indent = TRUE;
							flags += FORMAT_INDENT;
						}             
						break;
					default:
						break;
				 }     
				j++;
			}
			format_text(d->str, flags, d, d->max_str);
//			buf = get_buffer(MAX_INPUT_LENGTH);
			sprintf(buf, "Text formarted with%s indent.\r\n", (indent ? "" : "out")); 
			SEND_TO_Q(buf, d);
//			release_buffer(buf);
			break;
		case PARSE_REPLACE: 
			while (isalpha(string[j]) && j < 2) {
				switch (string[j]) {
					case 'a':
						if (!indent) {
							rep_all = 1;
						}             
						break;
					default:
						break;
				}     
				j++;
			}
			if ((s = strtok(string, "'")) == NULL) {
				SEND_TO_Q("Invalid format.\r\n", d);
				return;
			} else if ((s = strtok(NULL, "'")) == NULL) {
				SEND_TO_Q("Target string must be enclosed in single quotes.\r\n", d);
				return;
			} else if ((t = strtok(NULL, "'")) == NULL) {
				SEND_TO_Q("No replacement string.\r\n", d);
				return;
			} else if ((t = strtok(NULL, "'")) == NULL) {
				SEND_TO_Q("Replacement string must be enclosed in single quotes.\r\n", d);
				return;
			} else if ((total_len = ((strlen(t) - strlen(s)) + strlen(*d->str))) <= d->max_str) {
//				buf = get_buffer(MAX_STRING_LENGTH);
				if ((replaced = replace_str(d->str, s, t, rep_all, d->max_str)) > 0) {
					sprintf(buf, "Replaced %d occurance%sof '%s' with '%s'.\r\n", replaced, ((replaced != 1)?"s ":" "), s, t); 
					SEND_TO_Q(buf, d);
				} else if (replaced == 0) {
					sprintf(buf, "String '%s' not found.\r\n", s); 
					SEND_TO_Q(buf, d);
				} else {
					SEND_TO_Q("ERROR: Replacement string causes buffer overflow, aborted replace.\r\n", d);
				}
//				release_buffer(buf);
			} else
				SEND_TO_Q("Not enough space left in buffer.\r\n", d);
			break;
		case PARSE_DELETE:
			switch (sscanf(string, " %d - %d ", &line_low, &line_high)) {
				case 0:
					SEND_TO_Q("You must specify a line number or range to delete.\r\n", d);
					return;
				case 1:
					line_high = line_low;
					break;
				case 2:
					if (line_high < line_low) {
						SEND_TO_Q("That range is invalid.\r\n", d);
						return;
					}
					break;
			}
      
			i = 1;
			total_len = 1;
			if ((s = *d->str) == NULL) {
				SEND_TO_Q("Buffer is empty.\r\n", d);
				return;
			} else if (line_low > 0) {
				while (s && (i < line_low))
					if ((s = strchr(s, '\n')) != NULL) {
						i++;
						s++;
					}
				if ((i < line_low) || (s == NULL)) {
					SEND_TO_Q("Line(s) out of range; not deleting.\r\n", d);
					return;
				}
	 
				t = s;
				while (s && (i < line_high))
					if ((s = strchr(s, '\n')) != NULL) {
						i++;
						total_len++;
						s++;
					}
				if ((s) && ((s = strchr(s, '\n')) != NULL)) {
					s++;
					while (*s != '\0') *(t++) = *(s++);
				} else total_len--;
				*t = '\0';
				RECREATE(*d->str, char, strlen(*d->str) + 3);
//				buf = get_buffer(128);
				sprintf(buf, "%d line%sdeleted.\r\n", total_len, ((total_len != 1)?"s ":" "));
				SEND_TO_Q(buf, d);
//				release_buffer(buf);
			} else {
				SEND_TO_Q("Invalid line numbers to delete must be higher than 0.\r\n", d);
				return;
			}
			break;
		case PARSE_LIST_NORM:
			*buf = '\0';
			if (*string != '\0')
				switch (sscanf(string, " %d - %d ", &line_low, &line_high)) {
					case 0:
						line_low = 1;
						line_high = 999999;
						break;
					case 1:
						line_high = line_low;
						break;
				} else {
					line_low = 1;
					line_high = 999999;
				}
  
			if (line_low < 1) {
				SEND_TO_Q("Line numbers must be greater than 0.\r\n", d);
				return;
			}
			if (line_high < line_low) {
				SEND_TO_Q("That range is invalid.\r\n", d);
				return;
			}
			
//			buf = get_buffer(32 * 1024);
			*buf = '\0';
			
			if ((line_high < 999999) || (line_low > 1))
				sprintf(buf, "Current buffer range [%d - %d]:\r\n", line_low, line_high);
			i = 1;
			total_len = 0;
			s = *d->str;
			while (s && (i < line_low))
				if ((s = strchr(s, '\n')) != NULL) {
					i++;
					s++;
				}
			if ((i < line_low) || (s == NULL)) {
				SEND_TO_Q("Line(s) out of range; no buffer listing.\r\n", d);
//				release_buffer(buf);
				return;
			}
      
			t = s;
			while (s && (i <= line_high))
				if ((s = strchr(s, '\n')) != NULL) {
					i++;
					total_len++;
					s++;
				}
			if (s)	{
				temp = *s;
				*s = '\0';
				strcat(buf, t);
				*s = temp;
			} else strcat(buf, t);
			/*
			 * This is kind of annoying...but some people like it.
			 */
#if 0
			sprintf(buf + strlen(buf), "\r\n%d line%sshown.\r\n", total_len, ((total_len != 1)?"s ":" ")); 
#endif
			page_string(d, buf, TRUE);
//			release_buffer(buf);
			break;
		case PARSE_LIST_NUM:
			*buf = '\0';
			if (*string != '\0')
			switch (sscanf(string, " %d - %d ", &line_low, &line_high)) {
				case 0:
					line_low = 1;
					line_high = 999999;
					break;
				case 1:
					line_high = line_low;
					break;
			} else {
				line_low = 1;
				line_high = 999999;
			}
      
			if (line_low < 1) {
				SEND_TO_Q("Line numbers must be greater than 0.\r\n", d);
				return;
			}
			if (line_high < line_low) {
				SEND_TO_Q("That range is invalid.\r\n", d);
				return;
			}
			
			*buf = '\0';
			
			i = 1;
			total_len = 0;
			s = *d->str;
			while (s && (i < line_low))
				if ((s = strchr(s, '\n')) != NULL) {
					i++;
					s++;
				}
			if ((i < line_low) || (s == NULL)) {
				SEND_TO_Q("Line(s) out of range; no buffer listing.\r\n", d);
				return;
			}
      
//			buf = get_buffer(32 * 1024);
			
			t = s;
			while (s && (i <= line_high))
				if ((s = strchr(s, '\n')) != NULL) {
					i++;
					total_len++;
					s++;
					temp = *s;
					*s = '\0';
					sprintf(buf, "%s%4d:\r\n", buf, (i-1));
					strcat(buf, t);
					*s = temp;
					t = s;
				}
			if (s && t) {
				temp = *s;
				*s = '\0';
				strcat(buf, t);
				*s = temp;
			} else if (t) strcat(buf, t);
			/*
			 * This is kind of annoying .. seeing as the lines are numbered.
			 */
#if 0
			sprintf(buf + strlen(buf), "\r\n%d numbered line%slisted.\r\n", total_len, ((total_len != 1)?"s ":" ")); 
#endif
			page_string(d, buf, TRUE);
//			release_buffer(buf);
			break;

		case PARSE_INSERT:
			arg = get_buffer(MAX_INPUT_LENGTH);
			arg2 = get_buffer(MAX_INPUT_LENGTH);
			half_chop(string, arg, arg2);
			if (*arg == '\0') {
				SEND_TO_Q("You must specify a line number before which to insert text.\r\n", d);
				release_buffer(arg);
				release_buffer(arg2);
				return;
			}
			line_low = atoi(arg);
			release_buffer(arg);
			strcat(arg2, "\r\n");
			
			i = 1;
			*buf = '\0';
			if ((s = *d->str) == NULL)
				SEND_TO_Q("Buffer is empty, nowhere to insert.\r\n", d);
			else if (line_low > 0) {
				while (s && (i < line_low))
					if ((s = strchr(s, '\n')) != NULL) {
						i++;
						s++;
					}
				if ((i < line_low) || (s == NULL)) {
					SEND_TO_Q("Line number out of range; insert aborted.\r\n", d);
					release_buffer(arg2);
					return;
				}
				temp = *s;
				*s = '\0';
				if ((strlen(*d->str) + strlen(arg2) + strlen(s+1) + 3) > d->max_str) {
					*s = temp;
					SEND_TO_Q("Insert text pushes buffer over maximum size, insert aborted.\r\n", d);
				} else {
//					buf = get_buffer(32 * 1024);
					if (*d->str && (**d->str != '\0')) strcat(buf, *d->str);
					*s = temp;
					strcat(buf, arg2);
					if (s && (*s != '\0')) strcat(buf, s);
					RECREATE(*d->str, char, strlen(buf) + 3);
					strcpy(*d->str, buf);
					SEND_TO_Q("Line inserted.\r\n", d);
//					release_buffer(buf);
				}
			} else
				SEND_TO_Q("Line number must be higher than 0.\r\n", d);
			release_buffer(arg2);
			break;

		case PARSE_EDIT:
			arg = get_buffer(MAX_INPUT_LENGTH);
			arg2 = get_buffer(MAX_INPUT_LENGTH);
			half_chop(string, arg, arg2);
			if (*arg == '\0') {
				SEND_TO_Q("You must specify a line number at which to change text.\r\n", d);
				release_buffer(arg);
				release_buffer(arg2);
				return;
			}
			line_low = atoi(arg);
			release_buffer(arg);
			strcat(arg2, "\r\n");

			i = 1;
			*buf = '\0';
			if ((s = *d->str) == NULL)
				SEND_TO_Q("Buffer is empty, nothing to change.\r\n", d);
			else if (line_low > 0) {
				/* loop through the text counting /n chars till we get to the line */
				while (s && (i < line_low))
					if ((s = strchr(s, '\n')) != NULL) {
						i++;
						s++;
					}
				/*
				 * Make sure that there was a THAT line in the text.
				 */
				if ((i < line_low) || (s == NULL)) {
					SEND_TO_Q("Line number out of range; change aborted.\r\n", d);
					release_buffer(arg2);
					return;
				}
//				buf = get_buffer(32 * 1024);
				/*
				 * If s is the same as *d->str that means I'm at the beginning of the
				 * message text and I don't need to put that into the changed buffer.
				 */
				if (s != *d->str) {
					/* first things first .. we get this part into buf. */
					temp = *s;
					*s = '\0';
					/* put the first 'good' half of the text into storage */
					strcat(buf, *d->str);
					*s = temp;
				}
				/* put the new 'good' line into place. */
				strcat(buf, arg2);
				if ((s = strchr(s, '\n')) != NULL) {
					/* this means that we are at the END of the line we want outta there. */
					/* BUT we want s to point to the beginning of the line AFTER
					 * the line we want edited */
					s++;
					/* now put the last 'good' half of buffer into storage */
					strcat(buf, s);
				}
				/* check for buffer overflow */
				if (strlen(buf) > d->max_str) {
					SEND_TO_Q("Change causes new length to exceed buffer maximum size, aborted.\r\n", d);
				} else {
					/* change the size of the REAL buffer to fit the new text */
					RECREATE(*d->str, char, strlen(buf) + 3);
					strcpy(*d->str, buf);
					SEND_TO_Q("Line changed.\r\n", d);
				}
//				release_buffer(buf);
			} else
				SEND_TO_Q("Line number must be higher than 0.\r\n", d);
			release_buffer(arg2);
			break;
		default:
			SEND_TO_Q("Invalid option.\r\n", d);
			mudlog("SYSERR: invalid command passed to parse_action", BRF, LVL_CODER, TRUE);
			break;
	}
}

/* ************************************************************************
*  modification of malloc'ed strings                                      *
************************************************************************ */

/*
 * Add user input to the 'current' string (as defined by d->str).
 */
void string_add(DescriptorData *d, char *str) {
	FILE *fl;
	int terminator = 0, action = 0;
	int i = 2, j = 0;
	char *actions = get_buffer(MAX_INPUT_LENGTH);
	char *tilde;
	char *tmstr;
	time_t mytime;
	
	delete_doubledollar(str);
	
	while ((tilde = strchr(str, '~')))
		*tilde = ' ';
	
	if ((action = (*str == '/'))) {
		while (str[i] != '\0') {
			actions[j] = str[i];              
			i++;
			j++;
		}
		actions[j] = '\0';
		*str = '\0';
		switch (str[1]) {
			case 'a':
				terminator = 2; /* working on an abort message */
				break;
			case 'c':
				if (*(d->str)) {
					FREE(*d->str);
					*(d->str) = NULL;
					SEND_TO_Q("Current buffer cleared.\r\n", d);
				} else
					SEND_TO_Q("Current buffer empty.\r\n", d);
				break;
			case 'd':
				parse_action(PARSE_DELETE, actions, d);
				break;
			case 'e':
				parse_action(PARSE_EDIT, actions, d);
				break;
			case 'f': 
				if (*(d->str))	parse_action(PARSE_FORMAT, actions, d);
				else			SEND_TO_Q("Current buffer empty.\r\n", d);
				break;
			case 'i':
				if (*(d->str))	parse_action(PARSE_INSERT, actions, d);
				else			SEND_TO_Q("Current buffer empty.\r\n", d);
				break;
			case 'h': 
				parse_action(PARSE_HELP, actions, d);
				break;
			case 'l':
				if (*d->str)	parse_action(PARSE_LIST_NORM, actions, d);
				else			SEND_TO_Q("Current buffer empty.\r\n", d);
				break;
			case 'n':
				if (*d->str)	parse_action(PARSE_LIST_NUM, actions, d);
				else			SEND_TO_Q("Current buffer empty.\r\n", d);
				break;
			case 'r':   
				parse_action(PARSE_REPLACE, actions, d);
				break;
			case 's':
				terminator = 1;
				*str = '\0';
				break;
			default:
				SEND_TO_Q("Invalid option.\r\n", d);
				break;
		}
	}
	
	release_buffer(actions);

	if (terminator) {
		//	OLC Edits
		extern void oedit_disp_menu(DescriptorData *d);
		extern void oedit_disp_extradesc_menu(DescriptorData *d);
		extern void medit_disp_menu(DescriptorData *d);
		extern void hedit_disp_menu(DescriptorData *d);
		extern void cedit_disp_menu(DescriptorData *d);
		extern void medit_change_mprog(DescriptorData *d);
		extern void scriptedit_disp_menu(DescriptorData *d);
		
		
		//	Here we check for the abort option and reset the pointers.
		if ((terminator == 2) &&
				((STATE(d) == CON_REDIT) ||
				(STATE(d) == CON_MEDIT) ||
				(STATE(d) == CON_OEDIT) ||
				(STATE(d) == CON_HEDIT) ||
				(STATE(d) == CON_CEDIT) ||
				(STATE(d) == CON_TEXTED) ||
				(STATE(d) == CON_SCRIPTEDIT) ||
				(STATE(d) == CON_EXDESC))) {
			if (*d->str)	free(*d->str);
			if (d->backstr)	*d->str = d->backstr;
			else			*d->str = NULL;
			d->backstr = NULL;
			d->str = NULL;
		} else if (d->str && *d->str && (**d->str == '\0')) {
			free(*d->str);
			*d->str = NULL;
		}
     
		if (STATE(d) == CON_MEDIT) {
			switch (OLC_MODE(d)) {
				case MEDIT_D_DESC:					medit_disp_menu(d);				break;
				case MEDIT_MPROG_COMLIST:			medit_change_mprog(d);			break;
			}
		} else if (STATE(d) == CON_OEDIT) {
			switch (OLC_MODE(d)) {
				case OEDIT_ACTDESC:					oedit_disp_menu(d);				break;
				case OEDIT_EXTRADESC_DESCRIPTION:	oedit_disp_extradesc_menu(d);	break;
			}
		} else if (STATE(d) == CON_REDIT) {
			switch (OLC_MODE(d)) {
				case REDIT_DESC:					REdit::disp_menu(d);				break;
				case REDIT_EXIT_DESCRIPTION:		REdit::disp_exit_menu(d);		break;
				case REDIT_EXTRADESC_DESCRIPTION:	REdit::disp_extradesc_menu(d);	break;
			}
		}else if (STATE(d) == CON_SCRIPTEDIT) {
			switch (OLC_MODE(d)) {
				case SCRIPTEDIT_SCRIPT:				scriptedit_disp_menu(d);		break;
			}
		} else if (STATE(d) == CON_HEDIT)
			hedit_disp_menu(d);
		else if (STATE(d) == CON_CEDIT)
			cedit_disp_menu(d);
		else if ((STATE(d) == CON_PLAYING) && (PLR_FLAGGED(d->character, PLR_MAILING))) {
			if ((terminator == 1) && *d->str) {
				if (d->mail_to != -1)
					store_mail(d->mail_to, GET_IDNUM(d->character), *d->str);
				else {
					mytime = time(0);
					tmstr = asctime(localtime(&mytime));
					*(tmstr + strlen(tmstr) - 1) = '\0';
					imc_post_mail(d->character, d->character->RealName(),
							d->imc_mail_to, tmstr, "MudMail", *d->str);
				}
				SEND_TO_Q("Message sent!\r\n", d);
			} else
				SEND_TO_Q("Mail aborted.\r\n", d);
			d->mail_to = 0;
			if (*d->str)		FREE(*d->str);
			if (d->imc_mail_to)	FREE(d->imc_mail_to);
			FREE(d->str);
		} else if (d->mail_to >= BOARD_MAGIC) {
			Board_save_board(d->mail_to - BOARD_MAGIC);
			if (terminator == 2)
				SEND_TO_Q("Post not aborted, use REMOVE <post #>.\r\n", d);
			d->mail_to = 0;
		} else if (STATE(d) == CON_EXDESC) {
			if (terminator != 1)
				SEND_TO_Q("Description aborted.\r\n", d);
			SEND_TO_Q(MENU, d);
			STATE(d) = CON_MENU;
		} else if (STATE(d) == CON_TEXTED) {
			if (terminator == 1) {
				if (!(fl = fopen(d->storage, "w"))) {
					mudlogf(BRF, LVL_CODER, TRUE, "SYSERR: Can't write file '%s'.", d->storage);
				} else {
					if (*d->str) {
						char *buf1 = get_buffer(d->max_str);
						strcpy(buf1, *d->str);
						strip_string(buf1);
						fputs(buf1, fl);
						release_buffer(buf1);
					}
					fclose(fl);
					mudlogf(NRM, LVL_BUILDER, TRUE, "OLC: %s saves '%s'.", d->character->RealName(), d->storage);
					SEND_TO_Q("Saved.\r\n", d);
				}
			} else {
				SEND_TO_Q("Edit aborted.\r\n", d);
			}
			FREE(d->storage);
			d->storage = NULL;
			STATE(d) = CON_PLAYING;
			act("$n stops working on a PDA.", TRUE, d->character, 0, 0, TO_ROOM);
		} else if ((STATE(d) == CON_PLAYING) && d->character && !IS_NPC(d->character)) {
			if (terminator == 1) {
				if (strlen(*d->str) == 0) {
					FREE(*d->str);
					*d->str = NULL;
				}
			} else {
				FREE(*d->str);
				if (d->backstr)		*d->str = d->backstr;
				else				*d->str = NULL;
				d->backstr = NULL;
				SEND_TO_Q("Message aborted.\r\n", d);
			}
		}
    
		if (d->character && !IS_NPC(d->character))
			REMOVE_BIT(PLR_FLAGS(d->character), PLR_WRITING | PLR_MAILING);
		if (d->backstr)	FREE(d->backstr);
		d->backstr = NULL;
		d->str = NULL;
	} else if (!action) {
		if (!(*d->str)) {
			if (strlen(str) > (d->max_str - 3)) {
				send_to_char("String too long - Truncated.\r\n", d->character);
				*(str + d->max_str - 2) = '\0';
			}
			CREATE(*d->str, char, strlen(str) + 3);
			strcpy(*d->str, str);
			strcat(*d->str, "\r\n");
		} else if ((strlen(str) + strlen(*d->str)) > (d->max_str - 3)) {
			send_to_char("String too long - Truncated.\r\n", d->character);
			*(str + d->max_str - strlen(*d->str) - 2) = '\0';
			if (!(*d->str = (char *)realloc(*d->str, strlen(*d->str) + strlen(str) + 3))) {
				perror("string_add");
				exit(1);
			}
			strcat(*d->str, str);
			strcat(*d->str, "\r\n");
		} else if (!(*d->str = (char *)realloc(*d->str, strlen(*d->str) + strlen(str) + 3))) {
			perror("string_add");
			exit(1);
		} else {
			strcat(*d->str, str);
			strcat(*d->str, "\r\n");
		}
	}
}



/* **********************************************************************
*  Modification of character skills                                     *
********************************************************************** */

ACMD(do_skillset) {
	CharData *vict;
	char *name = get_buffer(128), *buf, *help;
	int skill, value, i, qend;
	
	argument = one_argument(argument, name);

	if (!*name) {			/* no arguments. print an informative text */
		release_buffer(name);
		help = get_buffer(MAX_STRING_LENGTH);
		send_to_char("Syntax: skillset <name> '<skill>' <value>\r\n", ch);
		strcpy(help, "Skill being one of the following:\r\n");
		for (i = 0; *skills[i] != '\n'; i++) {
			if (*skills[i] == '!')
				continue;
			sprintf(help + strlen(help), "%18s", skills[i]);
			if (i % 4 == 3) {
				strcat(help, "\r\n");
				send_to_char(help, ch);
				*help = '\0';
			}
		}
		if (*help)
			send_to_char(help, ch);
		send_to_char("\r\n", ch);
		release_buffer(help);
		return;
	}
	if (!(vict = get_char_vis(ch, name))) {
		send_to_char(NOPERSON, ch);
		release_buffer(name);
		return;
	}
	release_buffer(name);
	
	skip_spaces(argument);

	/* If there is no chars in argument */
	if (!*argument) {
		send_to_char("Skill name expected.\r\n", ch);
		return;
	}
	if (*argument != '\'') {
		send_to_char("Skill must be enclosed in: ''\r\n", ch);
		return;
	}
	/* Locate the last quote && lowercase the magic words (if any) */

	for (qend = 1; *(argument + qend) && (*(argument + qend) != '\''); qend++)
		*(argument + qend) = LOWER(*(argument + qend));

	if (*(argument + qend) != '\'') {
		send_to_char("Skill must be enclosed in: ''\r\n", ch);
		return;
	}
	help = get_buffer(MAX_STRING_LENGTH);
	strcpy(help, (argument + 1));
	help[qend - 1] = '\0';
	if ((skill = find_skill_num(help)) <= 0) {
		send_to_char("Unrecognized skill.\r\n", ch);
		release_buffer(help);
		return;
	}
	release_buffer(help);
	argument += qend + 1;		/* skip to next parameter */
	buf = get_buffer(MAX_INPUT_LENGTH);
	argument = one_argument(argument, buf);

	if (!*buf) {
		send_to_char("Learned value expected.\r\n", ch);
		release_buffer(buf);
		return;
	}
	value = atoi(buf);
	release_buffer(buf);
	if (value < 0) {
		send_to_char("Minimum value for learned is 0.\r\n", ch);
		return;
	}
	if (value > 100) {
		send_to_char("Max value for learned is 100.\r\n", ch);
		return;
	}
	if (IS_NPC(vict)) {
		send_to_char("You can't set NPC skills.\r\n", ch);
		return;
	}
	mudlogf(BRF, LVL_STAFF, TRUE, "%s changed %s's %s to %d.", ch->RealName(), vict->RealName(), skills[skill], value);

	SET_SKILL(vict, skill, value);
	
	buf = get_buffer(MAX_INPUT_LENGTH);
	sprintf(buf, "You change %s's %s to %d.\r\n", vict->RealName(), skills[skill], value);
	send_to_char(buf, ch);
	release_buffer(buf);
}



/*********************************************************************
* New Pagination Code
* Michael Buselli submitted the following code for an enhanced pager
* for CircleMUD.  All functions below are his.  --JE 8 Mar 96
*********************************************************************/

#define PAGE_LENGTH     22
#define PAGE_WIDTH      80

/*
 * Traverse down the string until the begining of the next page has been
 * reached.  Return NULL if this is the last page of the string.
 */
char *next_page(char *str) {
	SInt32	col = 1,
			line = 1;
	bool	spec_code = false;

	for (;; str++) {
		//	If end of string, return NULL
		if (*str == '\0')				return NULL;
		//	If we're at the start of the next page, return this fact.
		else if (line > PAGE_LENGTH)	return str;
		//	Check for the begining of an ANSI color code block.
		else if (*str == '\x1B' && !spec_code)
			spec_code = true;
		//	Check for the end of an ANSI color code block.
		else if (*str == 'm' && spec_code)
			spec_code = false;
		//	Check for everything else.
		else if (!spec_code) {
			//	Handle internal color coding :-)
			if (*str == '`') {
				str++;
				if (!*str)	str--;
				else		str++;
			}
			//	Carriage return puts us in column one.
			if (*str == '\r')	col = 1;
			//	Newline puts us on the next line.
			else if (*str == '\n')	line++;
			//	We need to check here and see if we are over the page width,
			//	and if so, compensate by going to the begining of the next line.
			else if (col++ > PAGE_WIDTH) {
				col = 1;
				line++;
			}
		}
	}
}


//	Function that returns the number of pages in the string.
int count_pages(char *str) {
	int pages;

	for (pages = 1; (str = next_page(str)); pages++);
	return pages;
}


//	This function assigns all the pointers for showstr_vector for the
//	page_string function, after showstr_vector has been allocated and
//	showstr_count set.
void paginate_string(char *str, DescriptorData *d) {
	int i;

	if (d->showstr_count)	*(d->showstr_vector) = str;

	for (i = 1; i < d->showstr_count && str; i++)
		str = d->showstr_vector[i] = next_page(str);

	d->showstr_page = 0;
}


//	The call that gets the paging ball rolling...
void page_string(DescriptorData *d, char *str, int keep_internal) {
	if (!d)	return;

	if (!str || !*str) {
/*		send_to_char("", d->character); - ?? */
		return;
	}
	d->showstr_count = count_pages(str);
	CREATE(d->showstr_vector, char *, d->showstr_count);

	if (keep_internal) {
		d->showstr_head = str_dup(str);
		paginate_string(d->showstr_head, d);
	} else
		paginate_string(str, d);

	show_string(d, "");
}


//	The call that displays the next page.
void show_string(DescriptorData *d, char *input) {
	char *buf = get_buffer(MAX_STRING_LENGTH);
	int diff;

	any_one_arg(input, buf);


	if (LOWER(*buf) == 'q') {		//	Q = quit
		FREE(d->showstr_vector);
		d->showstr_count = 0;
		if (d->showstr_head) {
			FREE(d->showstr_head);
			d->showstr_head = 0;
		}
		release_buffer(buf);
		return;
	} else if (LOWER(*buf) == 'r')	//	R = refresh, back up one page
		d->showstr_page = MAX(0, d->showstr_page - 1);
	else if (LOWER(*buf) == 'b')	//	B = back, back up two pages
		d->showstr_page = MAX(0, d->showstr_page - 2);
	else if (isdigit(*buf))			//	Feature to 'goto' a page
		d->showstr_page = MAX(0, MIN(atoi(buf) - 1, d->showstr_count - 1));
	else if (*buf) {
		send_to_char("Valid commands while paging are RETURN, Q, R, B, or a numeric value.\r\n", d->character);
		release_buffer(buf);
		return;
	}
	
	//	If we're displaying the last page, just send it to the character, and
	//	then free up the space we used.
	if (d->showstr_page + 1 >= d->showstr_count) {
		send_to_char(d->showstr_vector[d->showstr_page], d->character);
		FREE(d->showstr_vector);
		d->showstr_count = 0;
		if (d->showstr_head) {
			free(d->showstr_head);
			d->showstr_head = NULL;
		}
	} else { /* Or if we have more to show.... */
		strncpy(buf, d->showstr_vector[d->showstr_page],
				diff = ((int) d->showstr_vector[d->showstr_page + 1])
				- ((int) d->showstr_vector[d->showstr_page]));
		buf[diff] = '\0';
		send_to_char(buf, d->character);
		d->showstr_page++;
	}
	release_buffer(buf);
}
