/* ************************************************************************
*   File: boards.h                                      Part of CircleMUD *
*  Usage: header file for bulletin boards                                 *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Response and threading code copyright Trevor Man 1997                  *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */


#include "types.h"
#include "stl.llist.h"


#define MAX_MESSAGE_LENGTH	4096	/* arbitrary -- change if needed */

#define BOARD_MAGIC	1048575	/* arbitrary number - see modify.c */

struct board_msg_info {
	UInt32	id;
	SInt32	poster;
	time_t	timestamp;
	char *	subject;
	char *	data;
};

/* sorry everyone, but inorder to allow for board memory, I had to make
   this in the following nasty way.  Sorri. */
//struct board_memory_type {
//	int timestamp;
//	int reader;
//	struct board_memory_type *next;
//};


struct board_info_type {
	SInt32		read_lvl;	/* min level to read messages on this board */
	SInt32		write_lvl;	/* min level to write messages on this board */
	SInt32		remove_lvl;	/* min level to remove messages from this board */
	SInt32		vnum;
	LList<board_msg_info *>	messages;
	UInt32	last_id;
//	struct board_memory_type *memory[301];
};

#define READ_LVL(i)			(i->read_lvl)
#define WRITE_LVL(i)		(i->write_lvl)
#define REMOVE_LVL(i)		(i->remove_lvl)
#define BOARD_VNUM(i)		(i->vnum)
#define BOARD_MESSAGES(i)	(i->messages)
#define BOARD_MEMORY(i,j)	(i->memory[j])
#define BOARD_LASTID(i)		(i->last_id)

#define MESG_POSTER(i)		(i->poster)
#define MESG_TIMESTAMP(i)	(i->timestamp)
#define MESG_SUBJECT(i)		(i->subject)
#define MESG_DATA(i)		(i->data)
#define MESG_ID(i)			(i->id)

//#define MEMORY_TIMESTAMP(i) (i->timestamp)
//#define MEMORY_READER(i) (i->reader)
//#define MEMORY_NEXT(i) (i->next)

void Board_init_boards(void);
struct board_info_type * create_new_board(int board_vnum);
void parse_message( FILE *fl, struct board_info_type *t_board, char*filename, UInt32 id);
void look_at_boards(void);
int Board_show_board(int board_vnum, CharData *ch);
int Board_display_msg(int board_vnum, CharData * ch, int arg);
int Board_remove_msg(int board_vnum, CharData * ch, int arg);
int Board_save_board(int board_vnum);
void Board_write_message(int board_vnum, CharData *ch, char *arg);
