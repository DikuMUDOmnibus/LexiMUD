

#include "structs.h"
#include "utils.h"
#include "buffer.h"
#include "comm.h"
#include "interpreter.h"
#include "db.h"
#include "event.h"

/*   external vars  */
extern RoomData *world;
extern int rev_dir[];
extern char *dirs[];
extern struct event_info * pending_events;

void ASSIGNROOM(int room, SPECIAL(fname));

void clean_elevator_events(int elevator);
int elevator_pending(int elevator);
void assign_elevators(void);
int find_elevator_level_by_room(int elevator, int room);
int find_elevator_by_level(int room);
int find_elevator_by_room(int room);
SPECIAL(call_elevator);
SPECIAL(tell_elevator);
EVENTFUNC(elevator_move);


#define NUM_ELEVATORS 2

class elevRoom {
public:
	int		room;
	int		button;
};

class elevRec {
public:
	char *		name;
	int			exit;
	elevRoom	room;
	elevRoom	levels[10];
	int			numLevels;
	int			pos;
	int			dir,
				prev;
	struct		{
		int				stop;
		int				start;
		int				move;
	} delay;
	Event *		event;
};

#define EL_UP	 1
#define EL_STOP  0
#define EL_DOWN	-1

#define ELEVATOR_NAME(elev)			elevators[elev].name
#define ELEVATOR_LEVELS(elev)		elevators[elev].numLevels
#define ELEVATOR_ROOM(elev, level)	elevators[elev].levels[level]
#define ELEVATOR(elev)				elevators[elev].room
#define ELEVATOR_EXIT(elev)			elevators[elev].exit
#define ELEVATOR_POS(elev)			elevators[elev].pos
#define ELEVATOR_DIR(elev)			elevators[elev].dir
#define ELEVATOR_PREV_DIR(elev)		elevators[elev].prev
#define ELEVATOR_CUR(elev)			elevators[elev].levels[elevators[elev].pos]
#define ELEVATOR_BUTTON(elev, lev)	elevators[elev].levels[lev].button
#define ELEVATOR_DELAY_MOVE(elev)	elevators[elev].delay.move
#define ELEVATOR_DELAY_START(elev)	elevators[elev].delay.start
#define ELEVATOR_DELAY_STOP(elev)	elevators[elev].delay.stop

elevRec elevators[NUM_ELEVATORS] = {
	{
		"elevator",
		NORTH,
		{8004},
		{	{8003},
			{8008},
			{8018},
		},
		3,
		1,
		EL_UP,
		EL_UP,
		{ 20, 15, 10 }
	},
/*
	{
		"hydraulic lift",
		NORTH,
		{7297},
		{	{7295},
			{7216},
			{7296},
		},
		3,
		1,
		EL_UP,
		EL_UP,
		{ 10, 5, 5 }
	},
*/
	{
		"elevator",
		SOUTH,
		{4235},
		{	{4225},
			{4236},
			{4268},
			{4269},
			{4270},
			{4271},
		},
		6,
		1,
		EL_UP,
		EL_UP,
		{ 10, 5, 5 }
	}
};


typedef struct {
	UInt32	elevator;
} ElevatorEvent;


void assign_elevators(void) {
	int x,y;
	struct ElevatorEvent *el_data;

	for(x=0; x<NUM_ELEVATORS; x++) {
		ASSIGNROOM(ELEVATOR(x).room, tell_elevator);
//		add_event(1, elevator_move, 0, 0, (Ptr)x);
		CREATE(el_data, struct ElevatorEvent, 1);
		el_data->elevator = x;
		elevators[x].event = new Event(elevator_move, el_data, 1);
		for(y = 0; y < ELEVATOR_LEVELS(x); y++)
			ASSIGNROOM(ELEVATOR_ROOM(x, y).room, call_elevator);
	}
}


int find_elevator_level_by_room(int elevator, int room) {
	int y;
	for(y=0; y < ELEVATOR_LEVELS(elevator);y++) {
		if (room == ELEVATOR_ROOM(elevator, y).room)
			return y+1;
	}
	return 0;
}


int find_elevator_by_level(int room) {
	int x;
	for(x=0; x < NUM_ELEVATORS; x++) {
		if(find_elevator_level_by_room(x, room))
			return x+1;
	}
	log("Problem... elevator doesn't exist!");
	return 0;
}


int find_elevator_by_room(int room) {
	int x;
	for(x=0; x < NUM_ELEVATORS; x++) {
		if(room == ELEVATOR(x).room)
			return x+1;
	}
	log("Problem... elevator doesn't exist!");
	return 0;
}


SPECIAL(call_elevator) {
	int RNum = world[IN_ROOM(ch)].number;
	int elevator, level;
	char *arg;
	
	
	if (CMD_IS("press")) {
		arg = get_buffer(MAX_INPUT_LENGTH);
		one_argument(argument, arg);
		if (!is_abbrev(arg, "button")) {
			release_buffer(arg);
			return 0;
		}
		release_buffer(arg);
		if(!(elevator = find_elevator_by_level(RNum)))
			return 0;
		elevator--;
		if(!(level = find_elevator_level_by_room(elevator, RNum)))
			return 0;
		level--;
		ELEVATOR_BUTTON(elevator, level) = 1;
 		act("You press the button.", TRUE, ch, 0, 0, TO_CHAR);
 		act("$n presses the button.", FALSE, ch, 0, 0, TO_ROOM);
//		if (!elevator_pending(elevator))
//			add_event(ELEVATOR_DELAY_MOVE(elevator), elevator_move, 0, 0, (Ptr)elevator);
 		return 1;
	}
	return 0;
}

SPECIAL(tell_elevator) {
	int elevator = NUM_ELEVATORS, y;
	int VNum = world[IN_ROOM(ch)].number, RNum = IN_ROOM(ch);
	char *arg1, *arg2, *buf;
	
	if (CMD_IS("press")) {
		if(!(elevator = find_elevator_by_room(VNum)))
			return 0;
		elevator--;
		arg1 = get_buffer(MAX_INPUT_LENGTH);
		arg2 = get_buffer(MAX_INPUT_LENGTH);
		two_arguments(argument, arg1, arg2);
		if (!*arg2)
			y = atoi(arg1);
		else if (is_abbrev(arg1, "button"))
			y = atoi(arg2);
		else {
			release_buffer(arg1);
			release_buffer(arg2);
			return 0;
		}
		release_buffer(arg1);
		release_buffer(arg2);
		buf = get_buffer(128);
		if ((y <= 0) || (y > ELEVATOR_LEVELS(elevator))) {
			sprintf(buf, "There is no button with the number %d.\r\n", y);
			send_to_char(buf, ch);
			release_buffer(buf);
			return 1;
		}
		ELEVATOR_BUTTON(elevator, y-1) = 1;
		sprintf(buf, "You press button %d.", y);
		act(buf, TRUE, ch, 0, 0, TO_CHAR);
		sprintf(buf, "$n presses button %d.", y);
		act(buf, TRUE, ch, 0, 0, TO_ROOM);
		release_buffer(buf);
		if (!EXIT_FLAGGED(EXITN(RNum, ELEVATOR_EXIT(elevator)), EX_CLOSED)) {
			struct ElevatorEvent *el_data;
			elevators[elevator].event->Cancel();
			CREATE(el_data, struct ElevatorEvent, 1);
			el_data->elevator = elevator;
			elevators[elevator].event = new Event(elevator_move, el_data, 1);
//			clean_elevator_events(elevator);
//			add_event(1, elevator_move, 0, 0, (Ptr)elevator);
		}
//		} else if (!elevator_pending(elevator))
//			add_event(ELEVATOR_DELAY_MOVE(elevator), elevator_move, 0, 0, (Ptr)elevator);
		return 1;
	} else if (CMD_IS("close")) {
		if(!(elevator = find_elevator_by_room(VNum)))
			return 0;
		elevator--;
		act("You press the 'close' button.", TRUE, ch, 0, 0, TO_CHAR);
		act("$n presses the 'close' button.", FALSE, ch, 0, 0, TO_ROOM);
		if (!EXIT_FLAGGED(EXITN(RNum, ELEVATOR_EXIT(elevator)), EX_CLOSED)) {
			struct ElevatorEvent *el_data;
			elevators[elevator].event->Cancel();
			CREATE(el_data, struct ElevatorEvent, 1);
			el_data->elevator = elevator;
			elevators[elevator].event = new Event(elevator_move, el_data, 1);
//			clean_elevator_events(elevator);
//			add_event(1, elevator_move, 0, 0, (Ptr)elevator);
		} else
			act("Nothing happens.", TRUE, ch, 0, 0, TO_CHAR);
		return 1;
	} else if (CMD_IS("open")) {
		if(!(elevator = find_elevator_by_room(VNum)))
			return 0;
		elevator--;
		if (EXIT_FLAGGED(EXITN(RNum, ELEVATOR_EXIT(elevator)), EX_CLOSED)) {
			act("Try as you might, the $T doors won't open, unless it has come to a stop at a floor.",
					TRUE, ch, 0, ELEVATOR_NAME(elevator), TO_CHAR);
			act("$n tries in vain to open the doors, until $e realizes that the $T is still moving.",
					TRUE, ch, 0, ELEVATOR_NAME(elevator), TO_ROOM);
		} else {
			act("The $T doors are already open.",
					TRUE, ch, 0, ELEVATOR_NAME(elevator), TO_CHAR);
		}
		return 1;
	}
	return 0;
}


EVENTFUNC(elevator_move) {
	SInt32	elevator;
	RNum	eRoom, tRoom;
	RNum	eDoor, tDoor;
	SInt32	newTimer;
	RNum	rnum;
	
	if (!event_obj) {
		log("SYSERR: bad elevator_move: no event_obj");
		return 0;
	}
	
	elevator = ((ElevatorEvent *)event_obj)->elevator;
	
	if (ELEVATOR_DIR(elevator)) {
		if ((rnum = real_room(ELEVATOR(elevator).room)) == NOWHERE) {
			log("SYSERR: bad elevator %d - No room", elevator);
			elevators[elevator].event = NULL;
			return 0;
		}
		
		ELEVATOR_POS(elevator) += ELEVATOR_DIR(elevator);
		world[rnum].Send("The %s arrives at level %d.\r\n", ELEVATOR_NAME(elevator), ELEVATOR_POS(elevator)+1);

		if (ELEVATOR_POS(elevator) <= 0)								ELEVATOR_DIR(elevator) = EL_UP;
		if (ELEVATOR_POS(elevator) >= ELEVATOR_LEVELS(elevator) - 1)	ELEVATOR_DIR(elevator) = EL_DOWN;
	}
	
	eRoom = real_room(ELEVATOR(elevator).room);
	tRoom = real_room(ELEVATOR_CUR(elevator).room);
	if (eRoom == NOWHERE || tRoom == NOWHERE) {
		log("SYSERR: Elevator error: elevator room OR target room is NOWHERE");
		elevators[elevator].event = NULL;
		return 0;
	}
	
	eDoor = ELEVATOR_EXIT(elevator);
	tDoor = rev_dir[eDoor];
	
	if (!ELEVATOR_DIR(elevator)) {
		world[eRoom].Send("The doors slide closed, and the %s starts moving.\r\n", ELEVATOR_NAME(elevator));
		world[tRoom].Send("The %s doors slide closed.\r\n", ELEVATOR_NAME(elevator));
	
		world[eRoom].dir_option[eDoor]->to_room = tRoom;
		SET_BIT(world[eRoom].dir_option[eDoor]->exit_info, EX_CLOSED);
		SET_BIT(world[tRoom].dir_option[tDoor]->exit_info, EX_CLOSED);
		ELEVATOR_DIR(elevator) = ELEVATOR_PREV_DIR(elevator);
		newTimer = ELEVATOR_DELAY_START(elevator);
	} else {
		if ( ELEVATOR_BUTTON(elevator, ELEVATOR_POS(elevator)) ) {
			ELEVATOR_BUTTON(elevator, ELEVATOR_POS(elevator)) = 0;
			if(world[eRoom].people.Count())
				world[eRoom].Send("The %s stops moving, and the doors slide open.\r\n", ELEVATOR_NAME(elevator));
			if(world[tRoom].people.Count())
				world[tRoom].Send("The %s arrives, and the doors slide open.\r\n", ELEVATOR_NAME(elevator));
				
			
			world[eRoom].dir_option[eDoor]->to_room = tRoom;
			REMOVE_BIT(world[eRoom].dir_option[eDoor]->exit_info, EX_CLOSED);
			REMOVE_BIT(world[tRoom].dir_option[tDoor]->exit_info, EX_CLOSED);
			
			ELEVATOR_PREV_DIR(elevator) = ELEVATOR_DIR(elevator);
			ELEVATOR_DIR(elevator) = EL_STOP;
			newTimer = ELEVATOR_DELAY_STOP(elevator);
		} else
			newTimer = ELEVATOR_DELAY_MOVE(elevator);
	}

	return ((newTimer> 0 ? newTimer : 5) RL_SEC);
}