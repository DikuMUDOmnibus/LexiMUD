
#include "types.h"

#include <dirent.h>

#include "objects.h"
#include "descriptors.h"
#include "characters.h"
#include "rooms.h"
#include "utils.h"
#include "buffer.h"
#include "comm.h"
#include "db.h"
#include "boards.h"
#include "interpreter.h"
#include "handler.h"
#include "find.h"
#include "files.h"

LList<board_info_type *>board_info;

void strip_string(char *buffer);

void Board_respond_message(int board_vnum, CharData *ch, int mnum);
ACMD(do_respond);

void Board_init_boards(void) {
	int n,o, board_vnum;
	struct dirent **namelist;

	n = scandir(BOARD_DIRECTORY, &namelist, 0, alphasort);
	if (n < 0) {
		log("Funny, no board files found.");
		return;
	}
	for (o=0; o != n; o++) {
		if(strcmp("..",namelist[o]->d_name) && strcmp(".",namelist[o]->d_name)) {
			sscanf(namelist[o]->d_name,"%d", &board_vnum);
			create_new_board(board_vnum);
		}
	}
	/* just before we close, lets look at boards huh? */
	look_at_boards(); 
}


struct board_info_type * create_new_board(int board_vnum) {
//	struct board_msg_info *bsmg;
	int boards=0, t_messages=0, error=0, t[4], mnum, poster,timestamp, id /*, sflag */;
	FILE *fl;
	struct board_info_type *temp_board=NULL;
//	struct board_memory_type *memboard, *list;
	ObjData *obj;
	char *filename, *buf;
	
	filename = get_buffer(MAX_INPUT_LENGTH);
	buf = get_buffer(MAX_INPUT_LENGTH);
	
	sprintf(filename, BOARD_DIRECTORY "%d.brd", board_vnum);
	
	if(!(fl = fopen(filename, "r"))) {
		/* this is for boards which exist as an object, but don't have a file yet */
		/* Ie, new boards */

		if(!(fl = fopen(filename,"w"))) {
			log("Hm. Error while creating new board file '%s'", filename);
		} else {
			obj = read_object(board_vnum, VIRTUAL);
			
			if (!obj) {
				fclose(fl);
				remove(filename);
				release_buffer(filename);
				release_buffer(buf);
				return NULL;
			}
			
			CREATE(temp_board, struct board_info_type, 1);
			READ_LVL(temp_board)=GET_OBJ_VAL(obj, 0);
			WRITE_LVL(temp_board)=GET_OBJ_VAL(obj, 1);
			REMOVE_LVL(temp_board)=GET_OBJ_VAL(obj, 2);
			BOARD_VNUM(temp_board)=board_vnum;
			BOARD_LASTID(temp_board)=0;
			obj->extract();
			fprintf(fl,"Board File\n%d %d %d %d\n",READ_LVL(temp_board),
					WRITE_LVL(temp_board), REMOVE_LVL(temp_board), BOARD_MESSAGES(temp_board).Count());
			fclose(fl);

			board_info.Add(temp_board);
			release_buffer(buf);
			release_buffer(filename);
			return temp_board;
		}
	} else {
		get_line(fl,buf);
		
		if(!(str_cmp("Board File", buf))) {
			boards++;
			CREATE(temp_board, struct board_info_type, 1);
			temp_board->vnum = board_vnum;
			get_line(fl, buf); 
			if (sscanf(buf,"%d", t) != 1)
				error=1;
			if (real_object(board_vnum) < 0) {
				log("Erasing non-existant board file: %d", board_vnum);
				remove(filename);
				unlink(filename);
				if (temp_board)
					free(temp_board);
				release_buffer(buf);
				release_buffer(filename);
				return NULL;    /* realize why I do this yes? */
			}
			obj = read_object(board_vnum, VIRTUAL);

			READ_LVL(temp_board)=GET_OBJ_VAL(obj, 0);
			WRITE_LVL(temp_board)=GET_OBJ_VAL(obj, 1);
			REMOVE_LVL(temp_board)=GET_OBJ_VAL(obj, 2);	
			obj->extract();
			if (error) {
				log("Parse error in board %d - attempting to continue", BOARD_VNUM(temp_board));
				/* attempting board ressurection */

				if(t[0] < 0 || t[0] > LVL_CODER)		READ_LVL(temp_board) = LVL_IMMORT;
				if(t[1] < 0 || t[1] > LVL_CODER)		WRITE_LVL(temp_board) = LVL_IMMORT;
				if(t[2] < 0 || t[2] > LVL_CODER)		REMOVE_LVL(temp_board) = LVL_ADMIN;
			}
//			list = NULL;	/* or BOARD_MEMORY(temp_board) but this should be null anyway...*/
			while(get_line(fl,buf)) {
				/* this is the loop that reads the messages and also the message read info stuff */
				if (*buf == 'S') {
					sscanf(buf,"S %d %d %d ", &mnum, &poster, &timestamp);
//					CREATE(memboard, struct board_memory_type, 1);
//					MEMORY_READER(memboard)=poster;
//					MEMORY_TIMESTAMP(memboard)=timestamp;
//					bmsg = BOARD_MESSAGES(temp_board);
//					sflag = 0;
					//	Check and see if memory is still valid
//					while (bmsg && !sflag) {
//						if (MESG_TIMESTAMP(bmsg) == MEMORY_TIMESTAMP(memboard) &&
//								(mnum == ((MESG_TIMESTAMP(bmsg) % 301 + MESG_POSTER(bmsg) %301) % 301))) {
//							sflag = 1;
							/* probably ought to put a flag saying that
							   memory should be deleted because the reader
							   no longer exists on the mud, but it will
							   figure it out in a few iterations. Let it be
							   happy, and let me be lazy */
//						}
//						bmsg = MESG_NEXT(bmsg);
//					}
//					if (sflag) {
//						if (BOARD_MEMORY(temp_board, mnum)) {
//							list = BOARD_MEMORY(temp_board, mnum);
//							BOARD_MEMORY(temp_board, mnum) = memboard;
//							MEMORY_NEXT(memboard) = list;
//						} else {
//							BOARD_MEMORY(temp_board, mnum) = memboard;
//							MEMORY_NEXT(memboard) = NULL;
//						}
//					} else {
//						FREE(memboard);
//					}
//					if(BOARD_MEMORY(temp_board,mnum)) {
//						list=BOARD_MEMORY(temp_board,mnum);
//						BOARD_MEMORY(temp_board,mnum)=memboard;
//						MEMORY_NEXT(memboard)=list;
//					} else {
//						BOARD_MEMORY(temp_board,mnum)=memboard;
//						MEMORY_NEXT(memboard)=NULL;
//					}
				} else if (*buf == '#') {
					id = atoi(buf + 1);
					parse_message(fl, temp_board, filename, id);
					if (id > BOARD_LASTID(temp_board))
						BOARD_LASTID(temp_board) = id;
				}
			}
			
			/* add it to the list. */

			board_info.Add(temp_board);
		}
		fclose(fl);
	}

	/*	hold on. Lets think about this:
		Board messages are added in the folowing way - 1,2,3,4,5...etc
		However, when they're read in, its fifo, 5,4,3,2,1, so between
		each reboot, the existing order is reversed.  hm. */

//	reverse_message_order(temp_board);

	release_buffer(buf);
	release_buffer(filename);
	return temp_board;
}

#if 0
void reverse_message_order(struct board_info_type *t_board) {
	struct board_msg_info *old, *newmsg, *store;

//	old = BOARD_MESSAGES(t_board);
	if(BOARD_MESSAGES(t_board).Count() < 2)
		return;

	store = newmsg = NULL;

	/* yes, there's a better way for doing this, but I think I had some
	problems with second thoughts after accidently deleting stuff */

	while(old) {
		store = newmsg;
		CREATE(newmsg, struct board_msg_info, 1);
		MESG_POSTER(newmsg)=MESG_POSTER(old);
		MESG_TIMESTAMP(newmsg)=MESG_TIMESTAMP(old);
		MESG_SUBJECT(newmsg)=MESG_SUBJECT(old);
		MESG_DATA(newmsg)=MESG_DATA(old);
		MESG_ID(newmsg) = MESG_ID(old);
		MESG_NEXT(newmsg)=store;
		store=old;
		old=MESG_NEXT(old);
		FREE(store);
	}
	BOARD_MESSAGES(t_board) = newmsg;
}
#endif


void parse_message( FILE *fl, struct board_info_type *t_board, char *filename, UInt32 id) {
	struct board_msg_info *tmsg;
	char *subject = get_buffer(MAX_INPUT_LENGTH);
	char *buf = get_buffer(MAX_STRING_LENGTH);

	CREATE(tmsg, struct board_msg_info, 1);
	
	MESG_ID(tmsg) = id;
	
	fscanf(fl, "%d\n", &(MESG_POSTER(tmsg)));
	fscanf(fl, "%ld\n", &(MESG_TIMESTAMP(tmsg)));

	get_line(fl,subject);

	MESG_SUBJECT(tmsg)=str_dup(subject);
	MESG_DATA(tmsg)=fread_string(fl,buf, filename);

	BOARD_MESSAGES(t_board).Append(tmsg);

	release_buffer(subject);
	release_buffer(buf);
}


void look_at_boards() {
	SInt32	messages = 0;
	struct board_info_type *tboard;
 
	START_ITER(iter, tboard, board_info_type *, board_info) {
		messages += BOARD_MESSAGES(tboard).Count();
	} END_ITER(iter);
	
	log("There are %d boards located; %d messages", board_info.Count(), messages);
}


int Board_show_board(int board_vnum, CharData *ch) {
	struct board_info_type *thisboard;
	struct board_msg_info *message;
	char *tmstr;
	int msgcount=0, yesno = 0;
	char *buf, *buf2, *buf3;
	const char *name;

	/* board locate */
	START_ITER(board_iter, thisboard, board_info_type *, board_info) {
		if (BOARD_VNUM(thisboard) == board_vnum)
			break;
	} END_ITER(board_iter);
	if (!thisboard) {
		log("Creating new board - board #%d", board_vnum);
		thisboard = create_new_board(board_vnum);
	}
	if (GET_LEVEL(ch) < READ_LVL(thisboard)) {
		send_to_char("*** Security code invalid. ***\r\n",ch);
		return 1;
	}
	/* send the standard board boilerplate */

	buf = get_buffer(MAX_STRING_LENGTH);
	buf2 = get_buffer(MAX_STRING_LENGTH);

	sprintf(buf,"This is a bulletin board.\r\n"
			"Usage: READ/REMOVE <messg #>, WRITE <header>.\r\n");
	if (!BOARD_MESSAGES(thisboard).Count()) {
		strcat(buf, "The board is empty.\r\n");
		send_to_char(buf, ch);
		release_buffer(buf);
		release_buffer(buf2);
		return 1;
	} else {
		sprintf(buf + strlen(buf), "There %s %d message%s on the board.\r\n",
				(BOARD_MESSAGES(thisboard).Count() == 1) ? "is" : "are",
				BOARD_MESSAGES(thisboard).Count(),
				(BOARD_MESSAGES(thisboard).Count() == 1) ? "" : "s");
	}
	
	buf3 = get_buffer(MAX_STRING_LENGTH);
	
	START_ITER(message_iter, message, board_msg_info *, BOARD_MESSAGES(thisboard)) {
		tmstr = asctime(localtime( &MESG_TIMESTAMP(message)));
		*(tmstr + strlen(tmstr) - 1) = '\0';
		
		name = get_name_by_id(MESG_POSTER(message));
		strcpy(buf3, name ? name : "<UNKNOWN>");
		sprintf(buf2,"(%s)", CAP(buf3));
		
		sprintf(buf + strlen(buf), "[%5d] : %6.10s %-23s :: %s\r\n", /* yesno ? 'X' : ' ', */
				MESG_ID(message), tmstr, buf2, MESG_SUBJECT(message) ? MESG_SUBJECT(message) : "No Subject");
	} END_ITER(message_iter);
	release_buffer(buf3);
	release_buffer(buf2);
	page_string(ch->desc, buf, 1);
	release_buffer(buf);
	return 1;
}


int Board_display_msg(int board_vnum, CharData * ch, int arg) {
	struct board_info_type *thisboard;
	struct board_msg_info *message;
	char *tmstr;
	char *buf, *buf2;
	const char *name;
//	int msgcount,mem,sflag;
//	struct board_memory_type *mboard_type, *list;
 
	/* guess we'll have to locate the board now in the list */
 
	START_ITER(board_iter, thisboard, board_info_type *, board_info) {
		if (BOARD_VNUM(thisboard) == board_vnum)
			break;
	} END_ITER(board_iter);
 
	if (!thisboard) {
		log("Creating new board - board #%d", board_vnum);
		thisboard = create_new_board(board_vnum);
	} 
 
	if (GET_LEVEL(ch) < READ_LVL(thisboard)) {
		send_to_char("*** Security code invalid. ***\r\n", ch);
		return 1;
	}
    
	if (!BOARD_MESSAGES(thisboard).Count()) {
		send_to_char("The board is empty!\r\n", ch);
		return 1;
	}
	
	/* now we locate the message.*/  
	if (arg < 1) {
		send_to_char("You must specify the (positive) number of the message to be read!\r\n",ch);
		return 1;
	}
	
	START_ITER(message_iter, message, board_msg_info *, BOARD_MESSAGES(thisboard)) {
		if (MESG_ID(message) == arg)
			break;
	} END_ITER(message_iter);
	
	if(!message) {
		send_to_char("That message exists only in your imagination.\r\n", ch);
		return 1;
	}
	/* Have message, lets add the fact that this player read the mesg */
	
//	mem = ((MESG_TIMESTAMP(message)%301 + MESG_POSTER(message)%301)%301);

	/*make the new node */
//	CREATE(mboard_type, struct board_memory_type, 1);
//	MEMORY_READER(mboard_type)=GET_IDNUM(ch);
//	MEMORY_TIMESTAMP(mboard_type)=MESG_TIMESTAMP(message);
//	MEMORY_NEXT(mboard_type)=NULL;

	/* make sure that the slot is open, free */
//	list=BOARD_MEMORY(thisboard,mem);
//	sflag = 1;
//	while (list && sflag) {
//		if ((MEMORY_READER(list) == MEMORY_READER(mboard_type)) &&
//				(MEMORY_TIMESTAMP(list) == MEMORY_TIMESTAMP(mboard_type))
//			sflag = 0;
//		list = MEMORY_NEXT(list);
//	}
//	if (sflag) {
//		list = BOARD_MEMORY(thisboard, mem);
//		BOARD_MEMORY(thisboard,mem) = mboard_type;
//		MEMORY_NEXT(mboard_type)=list;
//	} else if (mboard_type)
//		FREE(mboard_type);

	/* before we print out the message, we may as well restore a human readable timestamp. */
	tmstr = asctime(localtime(&MESG_TIMESTAMP(message)));
	*(tmstr + strlen(tmstr) - 1) = '\0';

	buf = get_buffer(MAX_STRING_LENGTH);
	buf2 = get_buffer(MAX_STRING_LENGTH);
	name = get_name_by_id(MESG_POSTER(message));
	strcpy(buf2, name ? name : "<UNKNOWN>");
	sprintf(buf,"Message %d : %6.10s (%s) :: %s\r\n\r\n%s\r\n",
			MESG_ID(message), tmstr, CAP(buf2),
			MESG_SUBJECT(message) ? MESG_SUBJECT(message) : "No Subject",
			MESG_DATA(message) ? MESG_DATA(message) : "Looks like this message is empty.");
	page_string(ch->desc, buf, 1);
	Board_save_board(BOARD_VNUM(thisboard));
	release_buffer(buf);
	release_buffer(buf2);
	return 1;
}
 
int Board_remove_msg(int board_vnum, CharData * ch, int arg) {
	struct board_info_type *thisboard;
	struct board_msg_info *message;
	DescriptorData *d;
	char *buf;
	
	
	START_ITER(board_iter, thisboard, board_info_type *, board_info) {
		if (BOARD_VNUM(thisboard) == board_vnum)
			break;
	} END_ITER(board_iter);
	if (!thisboard) {
		send_to_char("Error: Your board could not be found. Please report.\n",ch);
		log("Error in Board_remove_msg - board #%d", board_vnum);
		return 0;
	}
	
	if (arg < 1) {
		send_to_char("You must specify the (positive) number of the message to be read!\r\n",ch);
		return 1;
	}
	
	START_ITER(message_iter, message, board_msg_info *, BOARD_MESSAGES(thisboard)) {
		if (MESG_ID(message) == arg)
			break;
	} END_ITER(message_iter);

	if(!message) {
		send_to_char("That message exists only in your imagination.\r\n", ch);
		return 1;
	}

	if(GET_IDNUM(ch) != MESG_POSTER(message) && GET_LEVEL(ch) < REMOVE_LVL(thisboard)) {
		send_to_char("*** Security code invalid. ***\r\n",ch);
		return 1;
	}

	/* perform check for mesg in creation */
	START_ITER(desc_iter, d, DescriptorData *, descriptor_list) {
		if ((STATE(d) == CON_PLAYING) && d->str == &(MESG_DATA(message))) {
			send_to_char("At least wait until the author is finished before removing it!\r\n", ch);
			END_ITER(desc_iter);
			return 1;
		}
	} END_ITER(desc_iter);

	/* everything else is peachy, kill the message */
	BOARD_MESSAGES(thisboard).Remove(message);
	
	if (MESG_SUBJECT(message))	free(MESG_SUBJECT(message));
	if (MESG_DATA(message))		free(MESG_DATA(message));
	free(message);

	send_to_char("Message removed.\r\n", ch);
	buf = get_buffer(MAX_INPUT_LENGTH);
	sprintf(buf, "$n just removed message %d.", arg);
	act(buf, FALSE, ch, 0, 0, TO_ROOM);
	release_buffer(buf);
	Board_save_board(BOARD_VNUM(thisboard));
	return 1; 
}
 
int Board_save_board(int board_vnum) {
	struct board_info_type *thisboard;
	struct board_msg_info *message;
//	struct board_memory_type *memboard;
	ObjData *obj;
	FILE *fl;
	int i=1, counter = 0;
	char *buf = get_buffer(MAX_STRING_LENGTH);

	
	START_ITER(board_iter, thisboard, board_info_type *, board_info) {
		if (BOARD_VNUM(thisboard) == board_vnum)
			break;
	} END_ITER(board_iter);
	
	if(!thisboard) {
		log("Creating new board - board #%d", board_vnum);
		thisboard = create_new_board(board_vnum);
	}

	/* before we save to the file, lets make sure that our board is updated */
	sprintf(buf, BOARD_DIRECTORY "%d.brd", board_vnum);

	obj = read_object(board_vnum, VIRTUAL);
	if (!obj) {
		remove(buf);
		unlink(buf);
		release_buffer(buf);
		return 0;
	}
	READ_LVL(thisboard)=GET_OBJ_VAL(obj, 0);
	WRITE_LVL(thisboard)=GET_OBJ_VAL(obj, 1);
	REMOVE_LVL(thisboard)=GET_OBJ_VAL(obj, 2);
	obj->extract();


	if (!(fl = fopen(buf, "wb"))) {
		perror("Error writing board");
		release_buffer(buf);
		return 0;
	}
	/* now we write out the basic stats of the board */

	fprintf(fl,"Board File\n%d\n", BOARD_MESSAGES(thisboard).Count());

	/* now write out the saved info.. */
	START_ITER(message_iter, message, board_msg_info *, BOARD_MESSAGES(thisboard)) {
		strcpy(buf, MESG_DATA(message) && *MESG_DATA(message) ? MESG_DATA(message) : "Empty!");
		strip_string(buf);
		fprintf(fl,	"#%d\n"
					"%d\n"
					"%ld\n"
					"%s\n"
					"%s~\n",
				MESG_ID(message), MESG_POSTER(message), MESG_TIMESTAMP(message),
				MESG_SUBJECT(message) && *MESG_SUBJECT(message)? MESG_SUBJECT(message) : "Empty", buf);
//		for (counter = 0; counter != 301; counter++) {
//		memboard = BOARD_MEMORY(thisboard, counter);
//		while (memboard) {
//			fprintf(fl, "S %d %d %d\n", counter, MEMORY_READER(memboard), MEMORY_TIMESTAMP(memboard));
//			memboard = MEMORY_NEXT(memboard);
//		}
	} END_ITER(message_iter);
	fclose(fl);
	release_buffer(buf);
	return 1;
}


void Board_write_message(int board_vnum, CharData *ch, char *arg) {
	struct board_info_type *thisboard;
	struct board_msg_info *message;

	if(IS_NPC(ch)) {
		send_to_char("Orwellian police thwart your attempt at free speech.\r\n",ch);
		return;
	}

	/* board locate */
	
	START_ITER(iter, thisboard, board_info_type *, board_info) {
		if (BOARD_VNUM(thisboard) == board_vnum)
			break;
	} END_ITER(iter);
	
	if (!thisboard) {
		send_to_char("Error: Your board could not be found. Please report.\n",ch);
		log("Error in Board_display_msg - board #%d", board_vnum);
		return;
	}

	if (GET_LEVEL(ch) < WRITE_LVL(thisboard)) {
		send_to_char("*** Security code invalid. ***\r\n",ch);
		return;
	}

	if (!*arg)
		strcpy(arg, "No Subject");
	else {
		skip_spaces(arg);
		delete_doubledollar(arg);
		arg[81] = '\0';
	}

//	if (!*arg) {
//		send_to_char("We must have a headline!\r\n", ch);
//		return;
//	}

	CREATE(message, struct board_msg_info, 1);
	MESG_ID(message) = ++BOARD_LASTID(thisboard);
	MESG_POSTER(message)=GET_IDNUM(ch);
	MESG_TIMESTAMP(message)=time(0);
	MESG_SUBJECT(message) = str_dup(arg);
	MESG_DATA(message)=NULL;

	BOARD_MESSAGES(thisboard).Add(message);

	send_to_char("Write your message.  (/s saves /h for help)\r\n\r\n", ch);
	act("$n starts to write a message.", TRUE, ch, 0, 0, TO_ROOM);

	if (!IS_NPC(ch))
		SET_BIT(PLR_FLAGS(ch), PLR_WRITING);

	ch->desc->str = &(MESG_DATA(message));
	ch->desc->max_str = MAX_MESSAGE_LENGTH;
	ch->desc->mail_to = board_vnum + BOARD_MAGIC;

	return;
}


void Board_respond_message(int board_vnum, CharData *ch, int mnum) {
	struct board_info_type *thisboard;
	struct board_msg_info *message, *other;
	char *buf;
	int gcount=0;

	/* board locate */
	
	START_ITER(iter, thisboard, board_info_type *, board_info) {
		if (BOARD_VNUM(thisboard) == board_vnum)
			break;
	} END_ITER(iter);
	
	if (!thisboard) {
		send_to_char("Error: Your board could not be found. Please report.\n",ch);
		log("Error in Board_display_msg - board #%d", board_vnum);
		return;
	}

	if ((GET_LEVEL(ch) < WRITE_LVL(thisboard)) || (GET_LEVEL(ch) < READ_LVL(thisboard))) {
		send_to_char("*** Security code invalid. ***\r\n", ch);
		return;
	}

	//	locate message to be repsponded to
	START_ITER(message_iter, other, board_msg_info *, BOARD_MESSAGES(thisboard)) {
		if (MESG_ID(other) == mnum)
			break;
	} END_ITER(message_iter);
	
	if(!other) {
		send_to_char("That message exists only in your imagination.\r\n", ch);
		return;
	}
	
	buf = get_buffer(MAX_STRING_LENGTH);
	
	CREATE(message, struct board_msg_info, 1);
	MESG_ID(message) = ++BOARD_LASTID(thisboard);
	MESG_POSTER(message) = GET_IDNUM(ch);
	MESG_TIMESTAMP(message) = time(0);
	sprintf(buf, "Re: %s", MESG_SUBJECT(other));
	MESG_SUBJECT(message) = str_dup(buf);
	MESG_DATA(message) = NULL;

	BOARD_MESSAGES(thisboard).Add(message);
	
	send_to_char("Write your message.  (/s saves /h for help)\r\n\r\n", ch);
	act("$n starts to write a message.", TRUE, ch, 0, 0, TO_ROOM);

	if (!IS_NPC(ch))
		SET_BIT(PLR_FLAGS(ch), PLR_WRITING);

	sprintf(buf,"\t------- Quoted message -------\r\n"
				"%s"
				"\t------- End Quote -------\r\n",
			MESG_DATA(other));

	MESG_DATA(message) = str_dup(buf);

	ch->desc->backstr = str_dup(buf);
	send_to_char(buf, ch);

	ch->desc->str = &(MESG_DATA(message));
	ch->desc->max_str = MAX_MESSAGE_LENGTH;
	ch->desc->mail_to = board_vnum + BOARD_MAGIC;
	
	release_buffer(buf);
	return;
}


/*
int mesglookup(struct board_msg_info *message, CharData *ch, struct board_info_type *board) {
	int mem;
	struct board_memory_type *mboard_type;
 
	mem = ((MESG_TIMESTAMP(message)%301 + MESG_POSTER(message)%301)%301);

	// now, we check the mem slot. If its null, we return no, er.. 0..
	// if its full, we double check against the timestamp and reader - 
	// mislabled as poster, but who cares... if they're not true, we
	// go to the linked next slot, and repeat
	mboard_type=BOARD_MEMORY(board,mem);
	while(mboard_type) {
		if(MEMORY_READER(mboard_type)==GET_IDNUM(ch) &&
				MEMORY_TIMESTAMP(mboard_type)==MESG_TIMESTAMP(message)) {
			return 1;
		} else {
			mboard_type=MEMORY_NEXT(mboard_type);
		}
	}
	return 0;
}
*/


ACMD(do_respond) {
	int found=0,mnum=0;
	ObjData *obj;
	char *arg;

	if(IS_NPC(ch)) {
		send_to_char("As a mob, you never bothered to learn to read or write.\r\n",ch);
		return;
	}
	
	
	if(!(obj = get_obj_in_list_type(ITEM_BOARD, ch->carrying)))
		obj = get_obj_in_list_type(ITEM_BOARD, world[IN_ROOM(ch)].contents);

	if (obj) {
		arg = get_buffer(MAX_INPUT_LENGTH);
		argument = one_argument(argument, arg);
		if (!*arg)
			send_to_char("Respond to what?\r\n",ch);
		else if (isname(arg, SSData(obj->name)))
			send_to_char("You cannot reply to an entire board!\r\n",ch);
		else if (!isdigit(*arg) || (!(mnum = atoi(arg))))
			send_to_char("You must reply to a message number.\r\n",ch);
		else
			Board_respond_message(GET_OBJ_VNUM(obj), ch, mnum);
		release_buffer(arg);
	} else
		send_to_char("There is no board here!\r\n", ch);
}
