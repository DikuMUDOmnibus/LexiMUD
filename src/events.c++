


#include "structs.h"
#include "utils.h"
#include "find.h"
#include "comm.h"
#include "handler.h"
#include "event.h"
#include "spells.h"

/* external vars  */
extern int rev_dir[];
extern char *dirs[];
extern int movement_loss[];

void explosion(CharData *ch, int dice_num, int dice_size, int room, int shrapnel);
int greet_mtrigger(CharData *actor, int dir);
int entry_mtrigger(CharData *ch);
int enter_wtrigger(RoomData *room, CharData *actor, int dir);
int greet_otrigger(CharData *actor, int dir);
int leave_mtrigger(CharData *actor, int dir);
int leave_otrigger(CharData *actor, int dir);
int leave_wtrigger(RoomData *room, CharData *actor, int dir);

EVENT(grenade_explosion);
EVENTFUNC(gravity);
EVENT(acid_burn);


EVENT(grenade_explosion) {
	int t, old_room;
	
	/*	grenades are activated by pulling the pin - ie, setting the
		one of the extra flag bits. After the pin is pulled the grenade
		starts counting down. once it reaches zero, it explodes. */

	t = CAUSER_OBJ->Room();
	
	/* then truly this grenade is nowhere?!? */
	if (t <= 0) {
		log("Serious problem, grenade truly in nowhere");
	} else { /* ok we have a room to blow up */
		old_room = IN_ROOM(CAUSER_OBJ);
		IN_ROOM(CAUSER_OBJ) = t;
		if (ROOM_FLAGGED(t, ROOM_PEACEFUL)) {		/* peaceful rooms */
			act("You hear $p explode harmlessly, with a loud POP!",
					FALSE, 0, CAUSER_OBJ, 0, TO_ROOM);
		} else {
			act("You hear a loud explosion as $p explodes!\r\n",
					FALSE, 0, CAUSER_OBJ, 0, TO_ROOM);

			explosion((CharData *)info, CAUSER_OBJ->value[1],
					CAUSER_OBJ->value[2], t, TRUE);
		}
		IN_ROOM(CAUSER_OBJ) = old_room;
	}
	CAUSER_OBJ->extract();
	return;
}


EVENTFUNC(gravity) {
	FallingEvent *	FEData = (FallingEvent *)event_obj;
	SInt32			dam;
	CharData *		faller = FEData->faller;
	RNum			was_in;

	if (!faller)
		return 0;	// Problemo...
	
	was_in = IN_ROOM(faller);

	faller->events.Remove(event);
	
	/* Character is falling... hehe */
	if (NO_STAFF_HASSLE(faller) || (IN_ROOM(faller) == NOWHERE) || !CAN_GO(faller, DOWN) ||
			EXIT_FLAGGED(EXIT(faller, DOWN), EX_NOMOVE) || AFF_FLAGGED(faller, AFF_FLYING) ||
			!ROOM_FLAGGED(IN_ROOM(faller), ROOM_GRAVITY)) {
		return 0;
	}
	
	if (!leave_mtrigger(faller, DOWN) || !leave_otrigger(faller, DOWN) || !leave_wtrigger(&world[IN_ROOM(faller)], faller, DOWN) ||
			!enter_wtrigger(&world[EXIT(faller, DOWN)->to_room], faller, DOWN)) {
		return 0;
	}
	
	if (FEData->previous++)	act("You are falling!\r\n", TRUE, faller, 0, 0, TO_CHAR);
	else					act("You fall!\r\n", TRUE, faller, 0, 0, TO_CHAR);
	
	act("$n falls down!\r\n", TRUE, faller, 0, 0, TO_ROOM);
	
	faller->from_room();
	faller->to_room(EXITN(was_in, DOWN)->to_room);
	
	look_at_room(faller, 0);
	
	act("$n falls from above!", TRUE, faller, 0, 0, TO_ROOM);
	
	if ((IN_ROOM(faller) == was_in) || !ROOM_FLAGGED(IN_ROOM(faller), ROOM_GRAVITY) ||
			!CAN_GO(faller, DOWN) /*|| IS_SET(EXIT(faller, DOWN)->exit_info, EX_NOMOVE)*/) {
		was_in = IN_ROOM(faller);
		act("You hit the ground with a $T.", TRUE, faller, 0, (FEData->previous <= 2) ? "thud" : "splat", TO_CHAR);
		act("$n hits the ground with a $T.", TRUE, faller, 0, (FEData->previous <= 2) ? "thud" : "splat", TO_ROOM);
		dam = dice(FEData->previous * 100, 10);
		damage(NULL, faller, (dam <= (GET_HIT(faller) + 99)) ? dam : GET_HIT(faller) + 99, TYPE_GRAVITY, 1);

		if (!PURGED(faller)) {
			if (GET_POS(faller) == POS_MORTALLYW)
				act("Damn... you broke your neck... you can't move... all you can do is...\r\nLie here until you die.",
						TRUE, faller, 0, 0, TO_CHAR);
			greet_mtrigger(faller, DOWN);
			greet_otrigger(faller, DOWN);
		}
		
		return 0;
	}
	
	faller->events.Add(event);
	
	greet_mtrigger(faller, DOWN);
	greet_otrigger(faller, DOWN);
	return (3 RL_SEC);
}


EVENT(acid_burn) {
	info = dice(2, info / 2);
	
	act("$n is being `Yburned`n by acid!", FALSE, CAUSER_CH, 0, 0, TO_ROOM);
	act("`RYou are being `Yburned`R by acid!`n", FALSE, CAUSER_CH, 0, 0, TO_CHAR);
/*	if ((int)info > 25)
		add_event(ACID_BURN_TIME, acid_burn, CAUSER_CH, NULL, (Ptr)info);*/
	damage(NULL, CAUSER_CH, info, TYPE_ACIDBLOOD, 1);
}

/*
EVENT(room_fire) {
	CharData *tch, next_tch;
	int room = info;
	
	if (!world[room].people)
		return;
	
	act("The room is on fire!", FALSE, world[room].people, 0, 0, TO_ROOM);
	
	for (tch = world[room].people; tch; tch = tch->next_tch) {
		next_tch = tch->next_in_room;
	}
}

*/