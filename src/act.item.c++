/* ************************************************************************
*   File: act.item.c                                    Part of CircleMUD *
*  Usage: object handling routines -- get/drop and container handling     *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */


#include "characters.h"
#include "rooms.h"
#include "objects.h"
#include "descriptors.h"
#include "utils.h"
#include "buffer.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "find.h"
#include "db.h"
#include "spells.h"
#include "event.h"
#include "boards.h"
#include "affects.h"


/* extern variables */
extern char *drinks[];
extern int drink_aff[][3];
extern char *drinknames[];
extern VNum donation_room_1;
#if 0
extern VNum donation_room_2;  /* uncomment if needed! */
extern VNum donation_room_3;  /* uncomment if needed! */
#endif

void mprog_give_trigger(CharData * mob, CharData * ch, ObjData * obj);
void mprog_bribe_trigger(CharData * mob, CharData * ch, int amount);

int drop_otrigger(ObjData *obj, CharData *actor);
int drop_wtrigger(ObjData *obj, CharData *actor);
int get_otrigger(ObjData *obj, CharData *actor);
int give_otrigger(ObjData *obj, CharData *actor, CharData *victim);
int receive_mtrigger(CharData *ch, CharData *actor, ObjData *obj);
int wear_otrigger(ObjData *obj, CharData *actor, int where);
int consume_otrigger(ObjData *obj, CharData *actor);

int wear_message_number (ObjData *obj, int where);


/* functions */
int can_take_obj(CharData * ch, ObjData * obj);
void get_check_money(CharData * ch, ObjData * obj);
int perform_get_from_room(CharData * ch, ObjData * obj);
void get_from_room(CharData * ch, char *arg);
CharData *give_find_vict(CharData * ch, char *arg);
void weight_change_object(ObjData * obj, int weight);
void name_from_drinkcon(ObjData * obj);
void name_to_drinkcon(ObjData * obj, int type);
void wear_message(CharData * ch, ObjData * obj, int where);
void perform_wear(CharData * ch, ObjData * obj, int where);
int find_eq_pos(CharData * ch, ObjData * obj, char *arg);
void perform_remove(CharData * ch, int pos);
void perform_put(CharData * ch, ObjData * obj, ObjData * cont);
void perform_get_from_container(CharData * ch, ObjData * obj, ObjData * cont, int mode);
int perform_drop(CharData * ch, ObjData * obj, SInt8 mode, char *sname, RNum RDR);
void perform_give(CharData * ch, CharData * vict, ObjData * obj);
void get_from_container(CharData * ch, ObjData * cont, char *arg, int mode);
bool can_hold(ObjData *obj);
ACMD(do_put);
ACMD(do_get);
ACMD(do_drop);
ACMD(do_give);
ACMD(do_drink);
ACMD(do_eat);
ACMD(do_pour);
ACMD(do_wear);
ACMD(do_wield);
ACMD(do_grab);
ACMD(do_remove);
ACMD(do_pull);
ACMD(do_reload);

void perform_put(CharData * ch, ObjData * obj, ObjData * cont) {
	if (!drop_otrigger(obj, ch))
		return;
	if ((GET_OBJ_WEIGHT(cont) + GET_OBJ_WEIGHT(obj)) > GET_OBJ_VAL(cont, 0))
		act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
	else {
		obj->from_char();
		obj->to_obj(cont);
		act("You put $p in $P.", FALSE, ch, obj, cont, TO_CHAR);
		act("$n puts $p in $P.", TRUE, ch, obj, cont, TO_ROOM);
	}
}


/* The following put modes are supported by the code below:

	1) put <object> <container>
	2) put all.<object> <container>
	3) put all <container>

	<container> must be in inventory or on ground.
	all objects to be put into container must be in inventory.
*/

ACMD(do_put) {
	char *	arg1 = get_buffer(MAX_INPUT_LENGTH),
		 *	arg2 = get_buffer(MAX_INPUT_LENGTH);
	ObjData *obj, *cont;
	CharData *tmp_char;
	int obj_dotmode, cont_dotmode, found = 0;

  two_arguments(argument, arg1, arg2);
  obj_dotmode = find_all_dots(arg1);
  cont_dotmode = find_all_dots(arg2);

  if (!*arg1)
    send_to_char("Put what in what?\r\n", ch);
  else if (cont_dotmode != FIND_INDIV)
    send_to_char("You can only put things into one container at a time.\r\n", ch);
  else if (!*arg2) {
    ch->Send("What do you want to put %s in?\r\n",
	    ((obj_dotmode == FIND_INDIV) ? "it" : "them"));
  } else {
    generic_find(arg2, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP, ch, &tmp_char, &cont);
    if (!cont) {
      ch->Send("You don't see %s %s here.\r\n", AN(arg2), arg2);
    } else if (GET_OBJ_TYPE(cont) != ITEM_CONTAINER)
      act("$p is not a container.", FALSE, ch, cont, 0, TO_CHAR);
    else if (OBJVAL_FLAGGED(cont, CONT_CLOSED))
      send_to_char("You'd better open it first!\r\n", ch);
    else {
      if (obj_dotmode == FIND_INDIV) {	/* put <obj> <container> */
	if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
	  ch->Send("You aren't carrying %s %s.\r\n", AN(arg1), arg1);
	} else if (obj == cont)
	  send_to_char("You attempt to fold it into itself, but fail.\r\n", ch);
	else
	  perform_put(ch, obj, cont);
      } else {
	START_ITER(iter, obj, ObjData *, ch->carrying) {
	  if ((obj != cont) && ch->CanSee(obj) &&
	      (obj_dotmode == FIND_ALL || isname(arg1, SSData(obj->name)))) {
	    found = 1;
	    perform_put(ch, obj, cont);
	  }
	} END_ITER(iter);
	if (!found) {
	  if (obj_dotmode == FIND_ALL)
	    send_to_char("You don't seem to have anything to put in it.\r\n", ch);
	  else {
	    ch->Send("You don't seem to have any %ss.\r\n", arg1);
	  }
	}
      }
    }
  }
	release_buffer(arg1);
	release_buffer(arg2);
}



int can_take_obj(CharData * ch, ObjData * obj) {
	if (!IS_STAFF(ch) && IS_CARRYING_N(ch) >= CAN_CARRY_N(ch))
		act("$p: you can't carry that many items.", FALSE, ch, obj, 0, TO_CHAR);
	else if (!IS_STAFF(ch) && (IS_CARRYING_W(ch) + GET_OBJ_WEIGHT(obj)) > CAN_CARRY_W(ch))
		act("$p: you can't carry that much weight.", FALSE, ch, obj, 0, TO_CHAR);
	else if (!(CAN_WEAR(obj, ITEM_WEAR_TAKE)))
		act("$p: you can't take that!", FALSE, ch, obj, 0, TO_CHAR);
	else if (!ch->CanUse(obj))
		act("$p: you can't figure what it does.", FALSE, ch, obj, 0, TO_CHAR);
	else
		return 1;
	return 0;
}


void perform_get_from_container(CharData * ch, ObjData * obj,
				     ObjData * cont, int mode)
{
  if (mode == FIND_OBJ_INV || can_take_obj(ch, obj)) {
    if (IS_CARRYING_N(ch) >= CAN_CARRY_N(ch))
      act("$p: you can't hold any more items.", FALSE, ch, obj, 0, TO_CHAR);
    else if (get_otrigger(obj, ch)) {
      obj->from_obj();
      obj->to_char(ch);
      act("You get $p from $P.", FALSE, ch, obj, cont, TO_CHAR);
      act("$n gets $p from $P.", TRUE, ch, obj, cont, TO_ROOM);
/*      get_check_money(ch, obj); */
    }
  }
}


void get_from_container(CharData * ch, ObjData * cont,
			     char *arg, int mode) {
	ObjData *obj;
	int obj_dotmode, found = 0;

	obj_dotmode = find_all_dots(arg);

	if (OBJVAL_FLAGGED(cont, CONT_CLOSED))
		act("$p is closed.", FALSE, ch, cont, 0, TO_CHAR);
	else if (obj_dotmode == FIND_INDIV) {
		if (!(obj = get_obj_in_list_vis(ch, arg, cont->contains))) {
			char *buf = get_buffer(128);
			sprintf(buf, "There doesn't seem to be %s %s in $p.", AN(arg), arg);
			act(buf, FALSE, ch, cont, 0, TO_CHAR);
			release_buffer(buf);
		} else
			perform_get_from_container(ch, obj, cont, mode);
	} else {
		if (obj_dotmode == FIND_ALLDOT && !*arg) {
			send_to_char("Get all of what?\r\n", ch);
			return;
		}
		START_ITER(iter, obj, ObjData *, cont->contains) {
			if (ch->CanSee(obj) &&
					(obj_dotmode == FIND_ALL || isname(arg, SSData(obj->name)))) {
				found = 1;
				perform_get_from_container(ch, obj, cont, mode);
			}
		} END_ITER(iter);
		if (!found) {
			if (obj_dotmode == FIND_ALL)
				act("$p seems to be empty.", FALSE, ch, cont, 0, TO_CHAR);
			else {
				char *buf = get_buffer(128);
				sprintf(buf, "You can't seem to find any %ss in $p.", arg);
				act(buf, FALSE, ch, cont, 0, TO_CHAR);
				release_buffer(buf);
			}
		}
	}
}


int perform_get_from_room(CharData * ch, ObjData * obj)
{
  if (can_take_obj(ch, obj) && get_otrigger(obj, ch)) {
    obj->from_room();
    obj->to_char(ch);
    act("You get $p.", FALSE, ch, obj, 0, TO_CHAR);
    act("$n gets $p.", TRUE, ch, obj, 0, TO_ROOM);
/*    get_check_money(ch, obj);*/
    return 1;
  }
  return 0;
}


void get_from_room(CharData * ch, char *arg) {
	ObjData *obj;
	int dotmode, found = 0;

	dotmode = find_all_dots(arg);

	if (dotmode == FIND_INDIV) {
		if (!(obj = get_obj_in_list_vis(ch, arg, world[IN_ROOM(ch)].contents))) {
			ch->Send("You don't see %s %s here.\r\n", AN(arg), arg);
		} else
			perform_get_from_room(ch, obj);
	} else {
		if (dotmode == FIND_ALLDOT && !*arg) {
			send_to_char("Get all of what?\r\n", ch);
			return;
		}
		START_ITER(iter, obj, ObjData *, world[IN_ROOM(ch)].contents) {
			if (ch->CanSee(obj) &&
					(dotmode == FIND_ALL || isname(arg, SSData(obj->name)))) {
				found = 1;
				perform_get_from_room(ch, obj);
			}
		} END_ITER(iter);
		if (!found) {
			if (dotmode == FIND_ALL)
				send_to_char("There doesn't seem to be anything here.\r\n", ch);
			else {
				act("You don't see any $ts here.\r\n", FALSE, ch, (ObjData *)arg, 0, TO_CHAR);
			}
		}
	}
}



ACMD(do_get) {
	char *arg1 = get_buffer(MAX_INPUT_LENGTH);
	char *arg2 = get_buffer(MAX_INPUT_LENGTH);

	int cont_dotmode, found = 0, mode;
	ObjData *cont;
	CharData *tmp_char;

	two_arguments(argument, arg1, arg2);

	if (IS_CARRYING_N(ch) >= CAN_CARRY_N(ch))
		send_to_char("Your arms are already full!\r\n", ch);
	else if (!*arg1)
		send_to_char("Get what?\r\n", ch);
	else if (!*arg2)
		get_from_room(ch, arg1);
	else {
		cont_dotmode = find_all_dots(arg2);
		if (cont_dotmode == FIND_INDIV) {
			mode = generic_find(arg2, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP, ch, &tmp_char, &cont);
			if (!cont)
				ch->Send("You don't have %s %s.\r\n", AN(arg2), arg2);
			else if (GET_OBJ_TYPE(cont) != ITEM_CONTAINER)
				act("$p is not a container.", FALSE, ch, cont, 0, TO_CHAR);
			else
				get_from_container(ch, cont, arg1, mode);
		} else if (cont_dotmode == FIND_ALLDOT && !*arg2) {
				send_to_char("Get from all of what?\r\n", ch);
		} else {
			LListIterator<ObjData *>	iter(ch->carrying);
			while ((cont = iter.Next())) {
				if (ch->CanSee(cont) && (cont_dotmode == FIND_ALL || isname(arg2, SSData(cont->name)))) {
					if (GET_OBJ_TYPE(cont) == ITEM_CONTAINER) {
						found = 1;
						get_from_container(ch, cont, arg1, FIND_OBJ_INV);
					} else if (cont_dotmode == FIND_ALLDOT) {
						found = 1;
						act("$p is not a container.", FALSE, ch, cont, 0, TO_CHAR);
					}
				}
			}
			
			iter.Restart(world[IN_ROOM(ch)].contents);
			while ((cont = iter.Next())) {
				if (ch->CanSee(cont) &&
						(cont_dotmode == FIND_ALL || isname(arg2, SSData(cont->name))))
					if (GET_OBJ_TYPE(cont) == ITEM_CONTAINER) {
						get_from_container(ch, cont, arg1, FIND_OBJ_ROOM);
						found = 1;
					} else if (cont_dotmode == FIND_ALLDOT) {
						act("$p is not a container.", FALSE, ch, cont, 0, TO_CHAR);
						found = 1;
					}
			}
								
			if (!found) {
				if (cont_dotmode == FIND_ALL)
					send_to_char("You can't seem to find any containers.\r\n", ch);
				else
					ch->Send("You can't seem to find any %ss here.\r\n", arg2);
			}
		}
	}
	release_buffer(arg1);
	release_buffer(arg2);
}


#define VANISH(mode) ((mode == SCMD_DONATE || mode == SCMD_JUNK) ? \
		      "  It vanishes in a puff of smoke!" : "")

int perform_drop(CharData * ch, ObjData * obj, SInt8 mode, char *sname, RNum RDR) {
  char *buf = get_buffer(SMALL_BUFSIZE);
  int value;

  if (OBJ_FLAGGED(obj, ITEM_NODROP)) {
    sprintf(buf, "You can't %s $p!", sname);
    act(buf, FALSE, ch, obj, 0, TO_CHAR);
    release_buffer(buf);
    return 0;
  }
  if (OBJ_FLAGGED(obj, ITEM_MISSION)) {
    sprintf(buf, "You can't %s $p, it is Mission Equipment.", sname);
    act(buf, FALSE, ch, obj, 0, TO_CHAR);
    release_buffer(buf);
    return 0;
  }
  if (!drop_otrigger(obj, ch))
  	return 0;
  if ((mode == SCMD_DROP) && !drop_wtrigger(obj, ch))
  	return 0;
  	
  sprintf(buf, "You %s $p.%s", sname, VANISH(mode));
  act(buf, FALSE, ch, obj, 0, TO_CHAR);
  sprintf(buf, "$n %ss $p.%s", sname, VANISH(mode));
  act(buf, TRUE, ch, obj, 0, TO_ROOM);
  release_buffer(buf);
  obj->from_char();

  if ((mode == SCMD_DONATE) && OBJ_FLAGGED(obj, ITEM_NODONATE))
    mode = SCMD_JUNK;

  switch (mode) {
  case SCMD_DROP:
    obj->to_room(IN_ROOM(ch));
    return 0;
    break;
  case SCMD_DONATE:
    obj->to_room(RDR);
    act("$p suddenly appears in a puff a smoke!", FALSE, 0, obj, 0, TO_ROOM);
    return 0;
    break;
  case SCMD_JUNK:
    value = MAX(1, MIN(200, GET_OBJ_COST(obj) / 16));
    obj->extract();
    return value;
    break;
  default:
    log("SYSERR: Incorrect argument %d passed to perform_drop", mode);
    break;
  }

  return 0;
}



ACMD(do_drop) {
	ObjData		*obj;
	RNum		RDR = 0;
	SInt8		mode = SCMD_DROP;
	int			dotmode, amount = 0;
	char		*sname, *arg;

	switch (subcmd) {
		case SCMD_JUNK:
			sname = "junk";
			mode = SCMD_JUNK;
			break;
		case SCMD_DONATE:
			sname = "donate";
			mode = SCMD_DONATE;
			switch (Number(0, 2)) {
				case 0:
					mode = SCMD_JUNK;
					break;
				case 1:
				case 2:
					RDR = real_room(donation_room_1);
					break;
//				case 3: RDR = real_room(donation_room_2); break;
//				case 4: RDR = real_room(donation_room_3); break;
			}
			if (RDR == NOWHERE) {
				send_to_char("Sorry, you can't donate anything right now.\r\n", ch);
				return;
			}
			break;
		default:
			sname = "drop";
			break;
	}
	arg = get_buffer(MAX_INPUT_LENGTH);

	argument = one_argument(argument, arg);

	if (!*arg)
		ch->Send("What do you want to %s?\r\n", sname);
	else if (is_number(arg)) {
		amount = atoi(arg);
		argument = one_argument(argument, arg);
//		if (!str_cmp("coins", arg) || !str_cmp("coin", arg))
//			perform_drop_gold(ch, amount, mode, RDR);
//		else {
			//	code to drop multiple items.  anyone want to write it? -je
			send_to_char("Sorry, you can't do that to more than one item at a time.\r\n", ch);
//		}
	} else {
		dotmode = find_all_dots(arg);

		/* Can't junk or donate all */
		if ((dotmode == FIND_ALL) && (subcmd == SCMD_JUNK || subcmd == SCMD_DONATE)) {
			if (subcmd == SCMD_JUNK)
				send_to_char("Go to the dump if you want to junk EVERYTHING!\r\n", ch);
			else
				send_to_char("Go do the donation room if you want to donate EVERYTHING!\r\n", ch);
		} else if (dotmode == FIND_ALL) {
			if (!ch->carrying.Count())
				send_to_char("You don't seem to be carrying anything.\r\n", ch);
			else {
				START_ITER(iter, obj, ObjData *, ch->carrying) {
					if (ch->CanSee(obj))
						amount += perform_drop(ch, obj, mode, sname, RDR);
				} END_ITER(iter);
			}
		} else if (dotmode == FIND_ALLDOT) {
			if (!*arg)
				ch->Send("What do you want to %s all of?\r\n", sname);
			else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying)))
				ch->Send("You don't seem to have any %ss.\r\n", arg);
			else {
				START_ITER(iter, obj, ObjData *, ch->carrying) {
					if (ch->CanSee(obj) && isname(arg, SSData(obj->name)))
						amount += perform_drop(ch, obj, mode, sname, RDR);
				} END_ITER(iter);
			}
		} else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying)))
			ch->Send("You don't seem to have %s %s.\r\n", AN(arg), arg);
		else
			amount += perform_drop(ch, obj, mode, sname, RDR);
	}

//	if (amount && (subcmd == SCMD_JUNK)) {
//		act("You trash $p.", TRUE, ch, obj, 0, TO_CHAR);
//		act("$n trashes $p.", TRUE, ch, obj, 0, TO_ROOM);
//		send_to_char("You have been rewarded by the gods!\r\n", ch);
//		act("$n has been rewarded by the gods!", TRUE, ch, 0, 0, TO_ROOM);
//		GET_MISSION_POINTS(ch) += amount;
//	}
	release_buffer(arg);
}


void perform_give(CharData * ch, CharData * vict,  ObjData * obj) {
	if (OBJ_FLAGGED(obj, ITEM_NODROP))
		act("You can't let go of $p!!  Yeech!", FALSE, ch, obj, 0, TO_CHAR);
	else if (IS_CARRYING_N(vict) >= CAN_CARRY_N(vict))
		act("$N seems to have $S hands full.", FALSE, ch, 0, vict, TO_CHAR);
	else if (GET_OBJ_WEIGHT(obj) + IS_CARRYING_W(vict) > CAN_CARRY_W(vict))
		act("$E can't carry that much weight.", FALSE, ch, 0, vict, TO_CHAR);
	else if (!vict->CanUse(obj))
		act("$E can't figure what $p does, so you keep it.", FALSE, ch, obj, vict, TO_CHAR);
	else if (give_otrigger(obj, ch, vict) && receive_mtrigger(vict, ch, obj)) {  	
		obj->from_char();
		obj->to_char(vict);
		MOBTrigger = FALSE;
		act("You give $p to $N.", FALSE, ch, obj, vict, TO_CHAR);
		MOBTrigger = FALSE;
		act("$n gives you $p.", FALSE, ch, obj, vict, TO_VICT);
		MOBTrigger = FALSE;
		act("$n gives $p to $N.", TRUE, ch, obj, vict, TO_NOTVICT);
		mprog_give_trigger(vict, ch, obj);
	}
}

/* utility function for give */
CharData *give_find_vict(CharData * ch, char *arg)
{
  CharData *vict;

  if (!*arg) {
    send_to_char("To who?\r\n", ch);
    return NULL;
  } else if (!(vict = get_char_room_vis(ch, arg))) {
    send_to_char(NOPERSON, ch);
    return NULL;
  } else if (vict == ch) {
    send_to_char("What's the point of that?\r\n", ch);
    return NULL;
  } else
    return vict;
}


ACMD(do_give) {
	int amount, dotmode;
	CharData *vict;
	ObjData *obj;
	char	*	arg = get_buffer(MAX_INPUT_LENGTH),
			*	buf1 = get_buffer(MAX_INPUT_LENGTH);

	argument = one_argument(argument, arg);

	if (!*arg)
		send_to_char("Give what to who?\r\n", ch);
	else if (is_number(arg)) {
		amount = atoi(arg);
		argument = one_argument(argument, arg);
	/* code to give multiple items.  anyone want to write it? -je */
		send_to_char("You can't give more than one item at a time.\r\n", ch);
	} else {
		one_argument(argument, buf1);
		if ((vict = give_find_vict(ch, buf1))) {
			dotmode = find_all_dots(arg);
			if (dotmode == FIND_INDIV) {
				if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying)))
					ch->Send("You don't seem to have %s %s.\r\n", AN(arg), arg);
				else
					perform_give(ch, vict, obj);
			} else if ((dotmode == FIND_ALLDOT) && !*arg)
				send_to_char("All of what?\r\n", ch);
			else if (!ch->carrying.Count())
				send_to_char("You don't seem to be holding anything.\r\n", ch);
			else {
				START_ITER(iter, obj, ObjData *, ch->carrying) {
					if (ch->CanSee(obj) && ((dotmode == FIND_ALL || isname(arg, SSData(obj->name)))))
						perform_give(ch, vict, obj);
				} END_ITER(iter);
			}
		}
	}
	release_buffer(arg);
	release_buffer(buf1);
}



void weight_change_object(ObjData * obj, int weight)
{
  ObjData *tmp_obj;
  CharData *tmp_ch;

  if (IN_ROOM(obj) != NOWHERE) {
    GET_OBJ_WEIGHT(obj) += weight;
  } else if ((tmp_ch = obj->carried_by)) {
    obj->from_char();
    GET_OBJ_WEIGHT(obj) += weight;
    obj->to_char(tmp_ch);
  } else if ((tmp_obj = obj->in_obj)) {
    obj->from_obj();
    GET_OBJ_WEIGHT(obj) += weight;
    obj->to_obj(tmp_obj);
  } else {
    log("SYSERR: Unknown attempt to subtract weight from an object.");
  }
}



void name_from_drinkcon(ObjData * obj) {
	int i;
	char *new_name;

	for (i = 0; (*(SSData(obj->name) + i) != ' ') && (*(SSData(obj->name) + i) != '\0'); i++);

	if (*(SSData(obj->name) + i) == ' ') {
		new_name = str_dup(SSData(obj->name) + i + 1);
		SSFree(obj->name);
		obj->name = SSCreate(new_name);
		free(new_name);
	}
}



void name_to_drinkcon(ObjData * obj, int type) {
	char *new_name;

	CREATE(new_name, char, strlen(SSData(obj->name)) + strlen(drinknames[type]) + 2);
	sprintf(new_name, "%s %s", drinknames[type], SSData(obj->name));
	SSFree(obj->name);
	obj->name = SSCreate(new_name);
	free(new_name);
}



ACMD(do_drink) {
	ObjData *temp;
	Affect *af;
	int amount, weight;
	int on_ground = 0;
	char *arg = get_buffer(MAX_INPUT_LENGTH);

	one_argument(argument, arg);

	if (!*arg) {
		send_to_char("Drink from what?\r\n", ch);
		release_buffer(arg);
		return;
	}
	if (!(temp = get_obj_in_list_vis(ch, arg, ch->carrying))) {
		if (!(temp = get_obj_in_list_vis(ch, arg, world[IN_ROOM(ch)].contents))) {
			act("You can't find it!", FALSE, ch, 0, 0, TO_CHAR);
			release_buffer(arg);
			return;
		} else
			on_ground = 1;
	}
	release_buffer(arg);
	if ((GET_OBJ_TYPE(temp) != ITEM_DRINKCON) &&
			(GET_OBJ_TYPE(temp) != ITEM_FOUNTAIN)) {
		send_to_char("You can't drink from that!\r\n", ch);
		return;
	}
	if (on_ground && (GET_OBJ_TYPE(temp) == ITEM_DRINKCON)) {
		send_to_char("You have to be holding that to drink from it.\r\n", ch);
		return;
	}
	if ((GET_COND(ch, DRUNK) > 10) && (GET_COND(ch, THIRST) > 0)) {
		/* The pig is drunk */
		send_to_char("You can't seem to get close enough to your mouth.\r\n", ch);
		act("$n tries to drink but misses $s mouth!", TRUE, ch, 0, 0, TO_ROOM);
		return;
	}
	if (!GET_OBJ_VAL(temp, 1)) {
		send_to_char("It's empty.\r\n", ch);
		return;
	}
	
	if (!consume_otrigger(temp, ch))
		return;
	
	if ((GET_COND(ch, FULL) > 20) && (GET_COND(ch, THIRST) > 0)) {
		send_to_char("Your stomach can't contain anymore!\r\n", ch);
		return;
	}
		
	if (subcmd == SCMD_DRINK) {
		act("$n drinks $T from $p.", TRUE, ch, temp, drinks[GET_OBJ_VAL(temp, 2)], TO_ROOM);

		act("You drink the $T.\r\n", FALSE, ch, 0, drinks[GET_OBJ_VAL(temp, 2)], TO_CHAR);

		if (drink_aff[GET_OBJ_VAL(temp, 2)][DRUNK] > 0)
			amount = (25 - GET_COND(ch, THIRST)) / drink_aff[GET_OBJ_VAL(temp, 2)][DRUNK];
		else
			amount = Number(3, 10);

	} else {
		act("$n sips from $p.", TRUE, ch, temp, 0, TO_ROOM);
		act("It tastes like $t.\r\n", FALSE, ch, (ObjData *)drinks[GET_OBJ_VAL(temp, 2)], 0, TO_CHAR);
		amount = 1;
	}

	amount = MIN(amount, GET_OBJ_VAL(temp, 1));

	/* You can't subtract more than the object weighs */
	weight = MIN(amount, GET_OBJ_WEIGHT(temp));

	weight_change_object(temp, -weight);	/* Subtract amount */

	if (GET_RACE(ch) != RACE_SYNTHETIC) {
		gain_condition(ch, DRUNK,
				(int) ((int) drink_aff[GET_OBJ_VAL(temp, 2)][DRUNK] * amount) / 4 );

		gain_condition(ch, FULL,
				(int) ((int) drink_aff[GET_OBJ_VAL(temp, 2)][FULL] * amount) / 4 );

		gain_condition(ch, THIRST,
				(int) ((int) drink_aff[GET_OBJ_VAL(temp, 2)][THIRST] * amount) / 4 );

		if (GET_COND(ch, DRUNK) > 10)
			send_to_char("You feel drunk.\r\n", ch);

		if (GET_COND(ch, THIRST) > 20)
			send_to_char("You don't feel thirsty any more.\r\n", ch);

		if (GET_COND(ch, FULL) > 20)
			send_to_char("You are full.\r\n", ch);

		if (GET_OBJ_VAL(temp, 3)) {	/* The shit was poisoned ! */
			send_to_char("Oops, it tasted rather strange!\r\n", ch);
			act("$n chokes and utters some strange sounds.", TRUE, ch, 0, 0, TO_ROOM);

		/*    af.type = SPELL_POISON;*/
/*			af.duration = amount * 3;
			af.modifier = 0;
			af.location = APPLY_NONE;
			af.bitvector = AFF_POISON;
			affect_join(ch, &af, FALSE, FALSE, FALSE, FALSE);*/
			af = new Affect(Affect::Poison, 0, APPLY_NONE, AFF_POISON);
			af->Join(ch, amount * 3, false, false, false, false);
		}
	}
	/* empty the container, and no longer poison. */
	GET_OBJ_VAL(temp, 1) -= amount;
	if (!GET_OBJ_VAL(temp, 1)) {	/* The last bit */
		GET_OBJ_VAL(temp, 2) = 0;
		GET_OBJ_VAL(temp, 3) = 0;
		name_from_drinkcon(temp);
	}
	return;
}



ACMD(do_eat)
{
	ObjData *food;
	Affect *af;
	int amount;
	char *arg = get_buffer(MAX_INPUT_LENGTH);

	one_argument(argument, arg);

	if (!*arg) {
		send_to_char("Eat what?\r\n", ch);
		release_buffer(arg);
		return;
	}
	if (!(food = get_obj_in_list_vis(ch, arg, ch->carrying))) {
		ch->Send("You don't seem to have %s %s.\r\n", AN(arg), arg);
		release_buffer(arg);
		return;
	}
	release_buffer(arg);
	if (subcmd == SCMD_TASTE && ((GET_OBJ_TYPE(food) == ITEM_DRINKCON) ||
			(GET_OBJ_TYPE(food) == ITEM_FOUNTAIN))) {
		do_drink(ch, argument, 0, "drink", SCMD_SIP);
		return;
	}
	if ((GET_OBJ_TYPE(food) != ITEM_FOOD) && !IS_STAFF(ch)) {
		send_to_char("You can't eat THAT!\r\n", ch);
		return;
	}
	
	if (!consume_otrigger(food, ch))
		return;
		
	if (GET_COND(ch, FULL) > 20) {/* Stomach full */
		act("You are too full to eat more!", FALSE, ch, 0, 0, TO_CHAR);
		return;
	}
	if (subcmd == SCMD_EAT) {
		act("You eat the $o.", FALSE, ch, food, 0, TO_CHAR);
		act("$n eats $p.", TRUE, ch, food, 0, TO_ROOM);
	} else {
		act("You nibble a little bit of the $o.", FALSE, ch, food, 0, TO_CHAR);
		act("$n tastes a little bit of $p.", TRUE, ch, food, 0, TO_ROOM);
	}

	amount = (subcmd == SCMD_EAT ? GET_OBJ_VAL(food, 0) : 1);

	if (GET_RACE(ch) != RACE_SYNTHETIC) {
		gain_condition(ch, FULL, amount);

		if (GET_COND(ch, FULL) > 20)
			act("You are full.", FALSE, ch, 0, 0, TO_CHAR);

		if (GET_OBJ_VAL(food, 3) && !IS_STAFF(ch)) {
	/* The shit was poisoned ! */
			send_to_char("Oops, that tasted rather strange!\r\n", ch);
			act("$n coughs and utters some strange sounds.", FALSE, ch, 0, 0, TO_ROOM);
			
			af = new Affect(Affect::Poison, 0, APPLY_NONE, AFF_POISON);
			af->Join(ch, amount * 2, FALSE, FALSE, FALSE, FALSE);
		}
	}
	if (subcmd == SCMD_EAT)
		food->extract();
	else if (!(--GET_OBJ_VAL(food, 0))) {
		send_to_char("There's nothing left now.\r\n", ch);
		food->extract();
	}
}


ACMD(do_pour) {
	char *arg1 = get_buffer(MAX_INPUT_LENGTH);
	char *arg2 = get_buffer(MAX_INPUT_LENGTH);
	ObjData *from_obj = NULL, *to_obj = NULL;
	int amount = TRUE;

	two_arguments(argument, arg1, arg2);

	if (subcmd == SCMD_POUR) {
		if (!*arg1) {		/* No arguments */
			act("From what do you want to pour?", FALSE, ch, 0, 0, TO_CHAR);
			amount = FALSE;
		} else if (!(from_obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
			act("You can't find it!", FALSE, ch, 0, 0, TO_CHAR);
			amount = FALSE;
		} else if (GET_OBJ_TYPE(from_obj) != ITEM_DRINKCON) {
			act("You can't pour from that!", FALSE, ch, 0, 0, TO_CHAR);
			amount = FALSE;
		}
	} else if (subcmd == SCMD_FILL) {
		if (!*arg1) {		/* no arguments */
			send_to_char("What do you want to fill?  And what are you filling it from?\r\n", ch);
			amount = FALSE;
		} else if (!(to_obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
			send_to_char("You can't find it!", ch);
			amount = FALSE;
		} else if (GET_OBJ_TYPE(to_obj) != ITEM_DRINKCON) {
			act("You can't fill $p!", FALSE, ch, to_obj, 0, TO_CHAR);
			amount = FALSE;
		} else if (!*arg2) {		/* no 2nd argument */
			act("What do you want to fill $p from?", FALSE, ch, to_obj, 0, TO_CHAR);
			amount = FALSE;
		} else if (!(from_obj = get_obj_in_list_vis(ch, arg2, world[IN_ROOM(ch)].contents))) {
			ch->Send("There doesn't seem to be %s %s here.\r\n", AN(arg2), arg2);
			amount = FALSE;
		} else if (GET_OBJ_TYPE(from_obj) != ITEM_FOUNTAIN) {
			act("You can't fill something from $p.", FALSE, ch, from_obj, 0, TO_CHAR);
			amount = FALSE;
		}
	}
	if (!amount) {
		release_buffer(arg1);
		release_buffer(arg2);
		return;
	}
	if (GET_OBJ_VAL(from_obj, 1) == 0) {
		act("The $p is empty.", FALSE, ch, from_obj, 0, TO_CHAR);
		release_buffer(arg1);
		release_buffer(arg2);
		return;
	}
	
	if (subcmd == SCMD_POUR) {	/* pour */
		if (!*arg2) {
			act("Where do you want it?  Out or in what?", FALSE, ch, 0, 0, TO_CHAR);
			amount = FALSE;
		} else if (!str_cmp(arg2, "out")) {
			act("$n empties $p.", TRUE, ch, from_obj, 0, TO_ROOM);
			act("You empty $p.", FALSE, ch, from_obj, 0, TO_CHAR);

			weight_change_object(from_obj, -GET_OBJ_VAL(from_obj, 1)); /* Empty */

			GET_OBJ_VAL(from_obj, 1) = 0;
			GET_OBJ_VAL(from_obj, 2) = 0;
			GET_OBJ_VAL(from_obj, 3) = 0;
			name_from_drinkcon(from_obj);

			amount = FALSE;
		} else if (!(to_obj = get_obj_in_list_vis(ch, arg2, ch->carrying))) {
			act("You can't find it!", FALSE, ch, 0, 0, TO_CHAR);
			amount = FALSE;
		} else if ((GET_OBJ_TYPE(to_obj) != ITEM_DRINKCON) &&
				(GET_OBJ_TYPE(to_obj) != ITEM_FOUNTAIN)) {
			act("You can't pour anything into that.", FALSE, ch, 0, 0, TO_CHAR);
			amount = FALSE;
		}
	}
	if (!amount) {
		release_buffer(arg1);
		release_buffer(arg2);
		return;
	}
	
	if (to_obj == from_obj) {
		act("A most unproductive effort.", FALSE, ch, 0, 0, TO_CHAR);
		amount = FALSE;
	} else if ((GET_OBJ_VAL(to_obj, 1) != 0) &&
			(GET_OBJ_VAL(to_obj, 2) != GET_OBJ_VAL(from_obj, 2))) {
		act("There is already another liquid in it!", FALSE, ch, 0, 0, TO_CHAR);
		amount = FALSE;
	} else if (!(GET_OBJ_VAL(to_obj, 1) < GET_OBJ_VAL(to_obj, 0))) {
		act("There is no room for more.", FALSE, ch, 0, 0, TO_CHAR);
		amount = FALSE;
	}
	if (!amount) {
		release_buffer(arg1);
		release_buffer(arg2);
		return;
	}
	
	if (subcmd == SCMD_POUR) {
		act("You pour the $T into the $p.", FALSE, ch, to_obj,
				drinks[GET_OBJ_VAL(from_obj, 2)], TO_CHAR);
	} else if (subcmd == SCMD_FILL) {
		act("You gently fill $p from $P.", FALSE, ch, to_obj, from_obj, TO_CHAR);
		act("$n gently fills $p from $P.", TRUE, ch, to_obj, from_obj, TO_ROOM);
	}
	/* New alias */
	if (GET_OBJ_VAL(to_obj, 1) == 0)
		name_to_drinkcon(to_obj, GET_OBJ_VAL(from_obj, 2));

	/* First same type liq. */
	GET_OBJ_VAL(to_obj, 2) = GET_OBJ_VAL(from_obj, 2);

	/* Then how much to pour */
	GET_OBJ_VAL(from_obj, 1) -= (amount = (GET_OBJ_VAL(to_obj, 0) - GET_OBJ_VAL(to_obj, 1)));

	GET_OBJ_VAL(to_obj, 1) = GET_OBJ_VAL(to_obj, 0);

	if (GET_OBJ_VAL(from_obj, 1) < 0) {	/* There was too little */
		GET_OBJ_VAL(to_obj, 1) += GET_OBJ_VAL(from_obj, 1);
		amount += GET_OBJ_VAL(from_obj, 1);
		GET_OBJ_VAL(from_obj, 1) = 0;
		GET_OBJ_VAL(from_obj, 2) = 0;
		GET_OBJ_VAL(from_obj, 3) = 0;
		name_from_drinkcon(from_obj);
	}
	/* Then the poison boogie */
	GET_OBJ_VAL(to_obj, 3) = (GET_OBJ_VAL(to_obj, 3) || GET_OBJ_VAL(from_obj, 3));

	/* And the weight boogie */
	weight_change_object(from_obj, -amount);
	weight_change_object(to_obj, amount);	/* Add weight */

	release_buffer(arg1);
	release_buffer(arg2);
}



void wear_message(CharData * ch, ObjData * obj, int where)
{
  char *wear_messages[][2] = {
    {"$n slides $p on to $s right ring finger.",
    "You slide $p on to your right ring finger."},

    {"$n slides $p on to $s left ring finger.",
    "You slide $p on to your left ring finger."},

    {"$n wears $p around $s neck.",
    "You wear $p around your neck."},

    {"$n wears $p on $s body.",
    "You wear $p on your body.",},

    {"$n wears $p on $s head.",
    "You wear $p on your head."},

    {"$n puts $p on $s legs.",
    "You put $p on your legs."},

    {"$n wears $p on $s feet.",
    "You wear $p on your feet."},

    {"$n puts $p on $s hands.",
    "You put $p on your hands."},

    {"$n wears $p on $s arms.",
    "You wear $p on your arms."},

    {"$n wears $p about $s body.",
    "You wear $p around your body."},

    {"$n wears $p around $s waist.",
    "You wear $p around your waist."},

    {"$n puts $p on around $s right wrist.",
    "You put $p on around your right wrist."},

    {"$n puts $p on around $s left wrist.",
    "You put $p on around your left wrist."},

    {"$n wears $p over $s eyes.",
    "You wear $p over your eyes."},

	{"ERROR, PLEASE REPORT",
	"ERROR, PLEASE REPORT"},

	{"ERROR, PLEASE REPORT",
	"ERROR, PLEASE REPORT"},

    {"$n wields $p with both $s hands.",
    "You wield $p with both your hands."},

    {"$n grabs $p with both $s hands.",
    "You grab $p with both your hands."}, 
    
    {"$n wields $p.",
    "You wield $p."},

    {"$n wields $p in $s off hand.",
    "You wield $p in your off hand."},

    {"$n lights $p and holds it.",
    "You light $p and hold it."},

    {"$n grabs $p.",
    "You grab $p."}
  };

  act(wear_messages[wear_message_number(obj, where)][0], TRUE, ch, obj, 0, TO_ROOM);
  act(wear_messages[wear_message_number(obj, where)][1], FALSE, ch, obj, 0, TO_CHAR);
}



void perform_wear(CharData * ch, ObjData * obj, int where) {
	//	ITEM_WEAR_TAKE is used for objects that do not require special bits
	//	to be put into that position (e.g. you can hold any object, not just
	//	an object with a HOLD bit.)
	ObjData *	obj2;
	
	int wear_bitvectors[] = {
		ITEM_WEAR_FINGER, ITEM_WEAR_FINGER, ITEM_WEAR_NECK,
		ITEM_WEAR_BODY, ITEM_WEAR_HEAD, ITEM_WEAR_LEGS,
		ITEM_WEAR_FEET, ITEM_WEAR_HANDS, ITEM_WEAR_ARMS,
		ITEM_WEAR_ABOUT, ITEM_WEAR_WAIST, ITEM_WEAR_WRIST, ITEM_WEAR_WRIST,
		ITEM_WEAR_EYES,
		(ITEM_WEAR_TAKE | ITEM_WEAR_SHIELD | ITEM_WEAR_WIELD),
		(ITEM_WEAR_TAKE | ITEM_WEAR_SHIELD | ITEM_WEAR_WIELD)};

	char *already_wearing[] = {
		"YOU SHOULD NEVER SEE THIS MESSAGE.  PLEASE REPORT.\r\n",
		"You're already wearing something on both of your ring fingers.\r\n",
		"You're already wearing something around your neck.\r\n",
		"You're already wearing something on your body.\r\n",
		"You're already wearing something on your head.\r\n",
		"You're already wearing something on your legs.\r\n",
		"You're already wearing something on your feet.\r\n",
		"You're already wearing something on your hands.\r\n",
		"You're already wearing something on your arms.\r\n",
		"You're already wearing something about your body.\r\n",
		"You already have something around your waist.\r\n",
		"YOU SHOULD NEVER SEE THIS MESSAGE.  PLEASE REPORT.\r\n",
		"You're already wearing something around both of your wrists.\r\n",
		"You're already wearing something over your eyes.\r\n",
		"You're already holding something in your right hand.\r\n",
		"You're already holding something in your left hand.\r\n"
	};

	if (!ch->CanUse(obj)) {
		act("You can't figure out how to use $p.", FALSE, ch, obj, 0, TO_CHAR);
		return;
	}
  
	/* first, make sure that the wear position is valid. */
	if (!CAN_WEAR(obj, wear_bitvectors[where])) {
		act("You can't wear $p there.", FALSE, ch, obj, 0, TO_CHAR);
		return;
	}
  
  
	/* for neck, finger, and wrist, try pos 2 if pos 1 is already full */
	if ((where == WEAR_FINGER_R) || (where == WEAR_WRIST_R))
		if (GET_EQ(ch, where))
			where++;

	if (!wear_otrigger(obj, ch, where))
		return;

	if (GET_EQ(ch, where)) {
		send_to_char(already_wearing[where], ch);
		return;
	} else if (where == WEAR_HAND_R || where == WEAR_HAND_L) {
		obj2 = GET_EQ(ch, where == WEAR_HAND_R ? WEAR_HAND_L : WEAR_HAND_R);
		if (obj2 && OBJ_FLAGGED(obj, ITEM_TWO_HAND)) {
			send_to_char("Your hands are full!", ch);
			return;
		}
	} 
	
	wear_message(ch, obj, where);
	obj->from_char();
	equip_char(ch, obj, where);
}



int find_eq_pos(CharData * ch, ObjData * obj, char *arg) {
	int where = -1;

	static char *keywords[] = {
		"finger",
		"!RESERVED!",
		"neck",
		"body",
		"head",
		"legs",
		"feet",
		"hands",
		"arms",
		"about",
		"waist",
		"wrist",
		"!RESERVED!",
		"eyes",
		"right",
		"left",
		"\n"
	};

	if (!arg || !*arg) {
		if (CAN_WEAR(obj, ITEM_WEAR_FINGER))      where = WEAR_FINGER_R;
		if (CAN_WEAR(obj, ITEM_WEAR_NECK))        where = WEAR_NECK;
		if (CAN_WEAR(obj, ITEM_WEAR_BODY))        where = WEAR_BODY;
		if (CAN_WEAR(obj, ITEM_WEAR_HEAD))        where = WEAR_HEAD;
		if (CAN_WEAR(obj, ITEM_WEAR_LEGS))        where = WEAR_LEGS;
		if (CAN_WEAR(obj, ITEM_WEAR_FEET))        where = WEAR_FEET;
		if (CAN_WEAR(obj, ITEM_WEAR_HANDS))       where = WEAR_HANDS;
		if (CAN_WEAR(obj, ITEM_WEAR_ARMS))        where = WEAR_ARMS;
		if (CAN_WEAR(obj, ITEM_WEAR_ABOUT))       where = WEAR_ABOUT;
		if (CAN_WEAR(obj, ITEM_WEAR_WAIST))       where = WEAR_WAIST;
		if (CAN_WEAR(obj, ITEM_WEAR_WRIST))       where = WEAR_WRIST_R;
		if (CAN_WEAR(obj, ITEM_WEAR_EYES))		  where = WEAR_EYES;
	} else {
		if ((where = search_block(arg, keywords, FALSE)) < 0) {
			act("'$T'?  What part of your body is THAT?\r\n", FALSE, ch, 0, arg, TO_CHAR);
		}
	}

	return where;
}


bool can_hold(ObjData *obj) {
	if (CAN_WEAR(obj, ITEM_WEAR_HOLD | ITEM_WEAR_SHIELD) || GET_OBJ_TYPE(obj) == ITEM_LIGHT)
		return true;
	return false;
}


ACMD(do_wear) {
	char *arg1 = get_buffer(MAX_INPUT_LENGTH);
	char *arg2 = get_buffer(MAX_INPUT_LENGTH);
	ObjData *obj;
	int where = -1, dotmode, items_worn = 0;
	
	two_arguments(argument, arg1, arg2);

	if (!*arg1)
		send_to_char("Wear what?\r\n", ch);
	else {
		LListIterator<ObjData *>	iter(ch->carrying);
		dotmode = find_all_dots(arg1);
	
		if (*arg2 && (dotmode != FIND_INDIV))
			send_to_char("You can't specify the same body location for more than one item!\r\n", ch);
		else if (dotmode == FIND_ALL) {
			while((obj = iter.Next())) {
				if (!ch->CanSee(obj))
					continue;
				if ((where = find_eq_pos(ch, obj, 0)) >= 0) {
					items_worn++;
					perform_wear(ch, obj, where);
				} else if (CAN_WEAR(obj, ITEM_WEAR_WIELD)) {
					sprintf(arg1, "%c%d", UID_CHAR, GET_ID(obj));
					do_wield(ch, arg1, 0, "wield", 0);
				} else if (can_hold(obj)) {
					sprintf(arg1, "%c%d", UID_CHAR, GET_ID(obj));
					do_grab(ch, arg1, 0, "grab", 0);
				}
			}
			if (!items_worn)
				send_to_char("You don't seem to have anything wearable.\r\n", ch);
		} else if (dotmode == FIND_ALLDOT) {
			if (!*arg1)
				send_to_char("Wear all of what?\r\n", ch);
			else if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying)))
				act("You don't seem to have any $ts.\r\n", FALSE, ch, (ObjData *)arg1, 0, TO_CHAR);
			else {
				while ((obj = iter.Next())) {
					if (!ch->CanSee(obj) || !isname(arg1, SSData(obj->name)))
						continue;
					if ((where = find_eq_pos(ch, obj, 0)) >= 0)
						perform_wear(ch, obj, where);
					else if (CAN_WEAR(obj, ITEM_WEAR_WIELD)) {
						sprintf(arg1, "%c%d", UID_CHAR, GET_ID(obj));
						do_wield(ch, arg1, 0, "wield", 0);
					} else if (can_hold(obj)) {
						sprintf(arg1, "%c%d", UID_CHAR, GET_ID(obj));
						do_grab(ch, arg1, 0, "grab", 0);
					} else
						act("You can't wear $p.", FALSE, ch, obj, 0, TO_CHAR);
				}
			}
		} else if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying)))
			ch->Send("You don't seem to have %s %s.\r\n", AN(arg1), arg1);
		else if ((where = find_eq_pos(ch, obj, arg2)) >= 0)
			perform_wear(ch, obj, where);
		else if (CAN_WEAR(obj, ITEM_WEAR_WIELD)) {
			sprintf(arg1, "%c%d", UID_CHAR, GET_ID(obj));
			do_wield(ch, arg1, 0, "wield", 0);
		} else if (can_hold(obj)) {
			sprintf(arg1, "%c%d", UID_CHAR, GET_ID(obj));
			do_grab(ch, arg1, 0, "grab", 0);
		} else if (!*arg2)
			act("You can't wear $p.", FALSE, ch, obj, 0, TO_CHAR);
	}
	release_buffer(arg1);
	release_buffer(arg2);
}



ACMD(do_wield) {
	ObjData *obj;
	char *arg1 = get_buffer(MAX_INPUT_LENGTH);
	char *arg2 = get_buffer(MAX_INPUT_LENGTH);
	
	two_arguments(argument, arg1, arg2);

	if (!*arg1)
		send_to_char("Wield what?\r\n", ch);
	else if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying)))
		ch->Send("You don't seem to have %s %s.\r\n", AN(arg1), arg1);
	else if (!CAN_WEAR(obj, ITEM_WEAR_WIELD))
		send_to_char("You can't wield that.\r\n", ch);
	else if (OBJ_FLAGGED(obj, ITEM_TWO_HAND)) {
		if (GET_EQ(ch, WEAR_HAND_R) || GET_EQ(ch, WEAR_HAND_L))
			send_to_char("You need both hands free to wield this.\r\n", ch);
		else if (GET_OBJ_WEIGHT(obj) > (GET_STR(ch) / 2))		//	1/2
			send_to_char("It's too heavy for you to use.\r\n", ch);
		else
			perform_wear(ch, obj, WEAR_HAND_R);
	} else if (GET_EQ(ch, WEAR_HAND_R) && GET_EQ(ch, WEAR_HAND_L))
		send_to_char("Your hands are full!", ch);
	else if (GET_OBJ_WEIGHT(obj) > (GET_STR(ch) / 4))			//	1/4
		send_to_char("It's too heavy for you to use.\r\n", ch);
	else if (!strn_cmp(arg2, "right", strlen(arg2)))
		perform_wear(ch, obj, WEAR_HAND_R);
	else if (!strn_cmp(arg2, "left", strlen(arg2)))
		perform_wear(ch, obj, WEAR_HAND_L);
	else
		send_to_char("You only have two hands, right and left.\r\n", ch);
	release_buffer(arg1);
	release_buffer(arg2);
}


ACMD(do_grab) {
	ObjData *obj;
	char * arg1 = get_buffer(MAX_INPUT_LENGTH);
	char * arg2 = get_buffer(MAX_INPUT_LENGTH);

	two_arguments(argument, arg1, arg2);

	if (!*arg1)
		send_to_char("Hold what?\r\n", ch);
	else if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying)))
		ch->Send("You don't seem to have %s %s.\r\n", AN(arg1), arg1);
	else if (!can_hold(obj))
		send_to_char("You can't hold that.\r\n", ch);
	else if (OBJ_FLAGGED(obj, ITEM_TWO_HAND)) {
		if (GET_EQ(ch, WEAR_HAND_R) || GET_EQ(ch, WEAR_HAND_L))
			send_to_char("You need both hands free to hold this.\r\n", ch);
		else
			perform_wear(ch, obj, WEAR_HAND_R);
	} else if (GET_EQ(ch, WEAR_HAND_R) && OBJ_FLAGGED(GET_EQ(ch, WEAR_HAND_R), ITEM_TWO_HAND))
		send_to_char("You're already holding something in both of your hands.\r\n", ch);
	else if (!strn_cmp(arg2, "left", strlen(arg2)))
		perform_wear(ch, obj, WEAR_HAND_L);
	else if (!strn_cmp(arg2, "right", strlen(arg2)))
		perform_wear(ch, obj, WEAR_HAND_R);
	else
		send_to_char("You only have two hands, right and left.\r\n", ch);
	release_buffer(arg1);
	release_buffer(arg2);
}



void perform_remove(CharData * ch, int pos)
{
	ObjData *obj;

	if (!(obj = GET_EQ(ch, pos)))
		log("Error in perform_remove: bad pos %d passed.", pos);
	else if (IS_CARRYING_N(ch) >= CAN_CARRY_N(ch))
		act("$p: you can't carry that many items!", FALSE, ch, obj, 0, TO_CHAR);
//	else if (OBJ_FLAGGED(obj, ITEM_NODROP))
//		act("You can't remove $p, it must be CURSED!", FALSE, ch, obj, 0, TO_CHAR);
	else {
		unequip_char(ch, pos)->to_char(ch);
		act("You stop using $p.", FALSE, ch, obj, 0, TO_CHAR);
		act("$n stops using $p.", TRUE, ch, obj, 0, TO_ROOM);
	}
}



ACMD(do_remove) {
	ObjData *obj = NULL;
	int		i, dotmode, msg;
	char	*arg = get_buffer(MAX_INPUT_LENGTH);
	bool	found = false;
	
	one_argument(argument, arg);

	if (!*arg) {
		send_to_char("Remove what?\r\n", ch);
		release_buffer(arg);
		return;
	}
	
	if (!(obj = get_obj_in_list_type(ITEM_BOARD, ch->carrying)))
		obj = get_obj_in_list_type(ITEM_BOARD, world[IN_ROOM(ch)].contents);
	
	if (obj && isdigit(*arg) && (msg = atoi(arg)))
		Board_remove_msg(GET_OBJ_VNUM(obj), ch, msg);
	else {
		dotmode = find_all_dots(arg);

		if (dotmode == FIND_ALL) {
			for (i = 0; i < NUM_WEARS; i++)
				if (GET_EQ(ch, i)) {
					perform_remove(ch, i);
					found = true;
				}
			if (!found)
				send_to_char("You're not using anything.\r\n", ch);
		} else if (dotmode == FIND_ALLDOT) {
			if (!*arg)
				send_to_char("Remove all of what?\r\n", ch);
			else {
				for (i = 0; i < NUM_WEARS; i++)
					if (GET_EQ(ch, i) && ch->CanSee(GET_EQ(ch, i)) &&
							isname(arg, SSData(GET_EQ(ch, i)->name))) {
						perform_remove(ch, i);
						found = true;
					}
				if (!found)
					ch->Send("You don't seem to be using any %ss.\r\n", arg);
			}
		} else if (!(obj = get_object_in_equip_vis(ch, arg, ch->equipment, &i)))
			ch->Send("You don't seem to be using %s %s.\r\n", AN(arg), arg);
		else
			perform_remove(ch, i);
	}
	release_buffer(arg);
}


ACMD(do_pull) {
	char * arg = get_buffer(MAX_INPUT_LENGTH);
	ObjData * obj;
	EVENT(grenade_explosion);

	one_argument(argument, arg);
	
	if (!*arg) {
		send_to_char("Pull what?\r\n", ch);
	} else if(!str_cmp(arg,"pin")) {
		obj = GET_EQ(ch, WEAR_HAND_L);
		if (!obj || (GET_OBJ_TYPE(obj) != ITEM_GRENADE))
			obj = GET_EQ(ch, WEAR_HAND_R);
		if(obj) {
			if(GET_OBJ_TYPE(obj) == ITEM_GRENADE) {
			    act("You pull the pin on $p.  Its activated!", FALSE, ch, obj, 0, TO_CHAR);
			    act("$n pulls the pin on $p.", TRUE, ch, obj, 0, TO_ROOM);
				add_event(GET_OBJ_VAL(obj, 0), grenade_explosion, obj, 0, ch);
			} else
				send_to_char("That's NOT a grenade!\r\n", ch);
		} else
			send_to_char("You aren't holding a grenade!\r\n", ch);
	}

	//	put other 'pull' options here later
	release_buffer(arg);
}


ACMD(do_reload) {
	ObjData *missile;
	ObjData *weapon;
	int ammo;
	
	if (!(weapon = GET_EQ(ch, WEAR_HAND_R)) || !IS_GUN(weapon))
		weapon = GET_EQ(ch, WEAR_HAND_L);
		
	//	Now check to see if its a loadable weapon
	if (!weapon || !IS_GUN(weapon)) {
		act("You aren't wielding a loadable weapon.", FALSE, ch, weapon, 0, TO_CHAR);
		return;
	}
	
	switch (subcmd) {
		case SCMD_LOAD:
			if (GET_GUN_AMMO(weapon) > 0) {
				send_to_char("The weapon is already loaded!", ch);
				return;
			}

			ammo = GET_GUN_AMMO_TYPE(weapon);
			{
			START_ITER(iter, missile, ObjData *, ch->carrying) {
				if (ch->CanSee(missile) &&
						(GET_OBJ_TYPE(missile) == ITEM_MISSILE) &&
						(GET_OBJ_VAL(missile, 0) == ammo) &&
						(GET_OBJ_VAL(missile, 1) > 0)) {
					act("You load $P into $p.", TRUE, ch, weapon, missile, TO_CHAR);
					act("$n loads $P into $p.", TRUE, ch, weapon, missile, TO_ROOM);
					GET_GUN_AMMO(weapon) = GET_OBJ_VAL(missile, 1);
					GET_GUN_AMMO_VNUM(weapon) = (GET_OBJ_VNUM(missile));
					missile->extract();
					END_ITER(iter);
					return;
				}
			} END_ITER(iter);
			}
			send_to_char("No ammo for the weapon.", ch);
			break;
		case SCMD_UNLOAD:
			if (GET_GUN_AMMO(weapon) <= 0) {
				act("The $p is empty.", FALSE, ch, weapon, 0, TO_CHAR);
				return;
			}

		    missile = read_object(GET_GUN_AMMO_VNUM(weapon), VIRTUAL);
		    if (!missile)
		    	act("Problem: the missile object couldn't be created.\r\n",
		    			TRUE, ch, 0, 0, TO_CHAR);
		    else if (GET_OBJ_TYPE(missile) != ITEM_MISSILE)
		    	act("Problem: the VNUM listed by the gun was not of type ITEM_MISSILE.\r\n",
		    			TRUE, ch, 0, 0, TO_CHAR);
		    else if	(GET_OBJ_VAL(missile, 0) != GET_GUN_AMMO_TYPE(weapon))
		    	act("Problem: the ammo VNUM of gun is not same type as the gun.\r\n",
		    			TRUE, ch, 0, 0, TO_CHAR);
		    else {
			    missile->to_char(ch);

				act("You unload a $P from $p.", TRUE, ch, weapon, missile, TO_CHAR);
				act("$n unloads a $P from $p.", TRUE, ch, weapon, missile, TO_ROOM);
				GET_OBJ_VAL(missile, 1) = GET_GUN_AMMO(weapon);
				GET_GUN_AMMO(weapon) = 0;
			}
			break;
		default:
			log("SYSERR: Unknown subcmd %d in do_reload.", subcmd);
	}
}
