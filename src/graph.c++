/* ************************************************************************
*   File: graph.c                                       Part of CircleMUD *
*  Usage: various graph algorithms                                        *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */


/*
 * You can define or not define TRACK_THOUGH_DOORS, depending on whether
 * or not you want track to find paths which lead through closed or
 * hidden doors.
 */
#define TRACK_THROUGH_DOORS	0




#include "structs.h"
#include "scripts.h"
#include "utils.h"
#include "buffer.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "find.h"
#include "db.h"
#include "spells.h"


/* Externals */
extern char *dirs[];

/* Locals */
void bfs_enqueue(RNum room, int dir);
void bfs_dequeue(void);
void bfs_clear_queue(void);
int find_first_step(RNum src, char *target);
ACMD(do_track);


struct bfs_queue_struct {
  RNum room;
  char dir;
  struct bfs_queue_struct *next;
};

static struct bfs_queue_struct *queue_head = 0, *queue_tail = 0;

/* Utility macros */
#define MARK(room) (SET_BIT(ROOM_FLAGS(room), ROOM_BFS_MARK))
#define UNMARK(room) (REMOVE_BIT(ROOM_FLAGS(room), ROOM_BFS_MARK))
#define IS_MARKED(room) (ROOM_FLAGGED(room, ROOM_BFS_MARK))
#define TOROOM(x, y) (EXITN((x),(y))->to_room)
#define IS_CLOSED(x, y) (EXIT_FLAGGED(EXITN((x),(y)), EX_CLOSED))

#ifdef TRACK_THROUGH_DOORS
#define VALID_EDGE(x, y) (world[(x)].dir_option[(y)] && \
			  (TOROOM(x, y) != NOWHERE) &&	\
			  (!ROOM_FLAGGED(TOROOM(x, y), ROOM_NOTRACK)) && \
			  (!IS_MARKED(TOROOM(x, y))))
#else
#define VALID_EDGE(x, y) (world[(x)].dir_option[(y)] && \
			  (TOROOM(x, y) != NOWHERE) &&	\
			  (!IS_CLOSED(x, y)) &&		\
			  (!ROOM_FLAGGED(TOROOM(x, y), ROOM_NOTRACK)) && \
			  (!IS_MARKED(TOROOM(x, y))))
#endif

void bfs_enqueue(RNum room, int dir) {
	struct bfs_queue_struct *curr;

	CREATE(curr, struct bfs_queue_struct, 1);
	curr->room = room;
	curr->dir = dir;
	curr->next = 0;

	if (queue_tail) {
		queue_tail->next = curr;
		queue_tail = curr;
	} else
		queue_head = queue_tail = curr;
}


void bfs_dequeue(void) {
	struct bfs_queue_struct *curr;

	curr = queue_head;

	if (!(queue_head = queue_head->next))
		queue_tail = 0;
	FREE(curr);
}


void bfs_clear_queue(void) {
	while (queue_head)
		bfs_dequeue();
}


/************************************************************************
*  Functions and Commands which use the above fns		        *
************************************************************************/

ACMD(do_track) {
	CharData *vict;
	int dir;
	char *arg;
	
	/* The character must have the track skill. */
	if (!GET_SKILL(ch, SKILL_TRACK)) {
		send_to_char("You have no idea how.\r\n", ch);
		return;
	}

	arg = get_buffer(MAX_INPUT_LENGTH);
	one_argument(argument, arg);
	
	if (!*arg)
		send_to_char("Whom are you trying to track?\r\n", ch);
//	else if (AFF_FLAGGED(vict, AFF_NOTRACK))
//		send_to_char("You sense no trail.\r\n", ch);
	else if (Number(0, 101) >= GET_SKILL(ch, SKILL_TRACK)) {
		/* 101 is a complete failure, no matter what the proficiency. */
		/* Find a random direction. */
		int x, found = 0;
		for (x=0; x < NUM_OF_DIRS && !found; x++)
			found = CAN_GO(ch, x);
		if (!found)
			send_to_char("You are in a closed room.\r\n", ch);
		else {
			do {
				dir = Number(0, NUM_OF_DIRS - 1);
			} while (!CAN_GO(ch, dir));
			ch->Send("You sense a trail %s from here!\r\n", dirs[dir]);
		}
	} else if ((dir = find_first_step(IN_ROOM(ch), arg)) == -1) {
		if (!(vict = get_char_vis(ch, arg)))
			send_to_char("No one is around by that name.\r\n", ch);
		else if (IN_ROOM(ch) == IN_ROOM(vict))
			send_to_char("You're already in the same room!!\r\n", ch);
		else
			act("You can't sense a trail to $M from here.\r\n", TRUE, ch, 0, vict, TO_CHAR);
	} else
		ch->Send("You sense a trail %s from here!\r\n", dirs[dir]);
	release_buffer(arg);
}
