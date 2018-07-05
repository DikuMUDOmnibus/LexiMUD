/* ************************************************************************
*   File: shop.c                                        Part of CircleMUD *
*  Usage: shopkeepers: loading config files, spec procs.                  *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

/***
 * The entire shop rewrite for Circle 3.0 was done by Jeff Fink.  Thanks Jeff!
 ***/

#define __SHOP_C__


#include "structs.h"
#include "buffer.h"
#include "comm.h"
#include "handler.h"
#include "find.h"
#include "db.h"
#include "interpreter.h"
#include "utils.h"
#include "shop.h"
#include "files.h"


/* External variables */
extern struct TimeInfoData time_info;
extern char *item_types[];
extern char *extra_bits[];
extern char *drinks[];

/* Forward/External function declarations */
ACMD(do_tell);
ACMD(do_action);
ACMD(do_echo);
ACMD(do_say);
char *fname(char *namelist);
void sort_keeper_objs(CharData * keeper, int shop_nr);
int can_take_obj(CharData * ch, ObjData * obj);


/* Local variables */
struct shop_data *shop_index;
int top_shop = 0;
int cmd_say, cmd_tell, cmd_emote, cmd_slap, cmd_puke;


int is_ok_char(CharData * keeper, CharData * ch, int shop_nr);
int is_open(CharData * keeper, int shop_nr, int msg);
int is_ok(CharData * keeper, CharData * ch, int shop_nr);
void push(struct stack_data * stack, int pushval);
int top(struct stack_data * stack);
int pop(struct stack_data * stack);
void evaluate_operation(struct stack_data * ops, struct stack_data * vals);
int find_oper_num(char token);
int evaluate_expression(ObjData * obj, char *expr);
int trade_with(ObjData * item, int shop_nr);
bool same_obj(ObjData * obj1, ObjData * obj2);
bool shop_producing(ObjData * item, int shop_nr);
int transaction_amt(char *arg);
char *times_message(ObjData * obj, char *name, int num);
int buy_price(ObjData * obj, int shop_nr);
int sell_price(CharData * ch, ObjData * obj, int shop_nr);
char *list_object(ObjData * obj, int cnt, int index, int shop_nr);
int ok_shop_room(int shop_nr, int room);
int ok_damage_shopkeeper(CharData * ch, CharData * victim);
int add_to_list(struct shop_buy_data * list, int type, int *len, int *val);
int end_read_list(struct shop_buy_data * list, int len, int error);
void read_line(FILE * shop_f, char *string, Ptr data);
void boot_the_shops(FILE * shop_f, char *filename, int rec_count);
void assign_the_shopkeepers(void);
char *customer_string(int shop_nr, int detailed);
void list_all_shops(CharData * ch);
void handle_detailed_list(char *buf, char *buf1, CharData * ch);
void list_detailed_shop(CharData * ch, int shop_nr);
void show_shops(CharData * ch, char *arg);
SPECIAL(shop_keeper);
ObjData *get_slide_obj_vis(CharData * ch, char *name, LList<ObjData *> &list);
ObjData *get_hash_obj_vis(CharData * ch, char *name, LList<ObjData *> &list);
ObjData *get_purchase_obj(CharData * ch, char *arg, CharData * keeper, int shop_nr, int msg);
void shopping_buy(char *arg, CharData * ch, CharData * keeper, int shop_nr);
ObjData *get_selling_obj(CharData * ch, char *name, CharData * keeper, int shop_nr, int msg);
ObjData *slide_obj(ObjData * obj, CharData * keeper, int shop_nr);
void shopping_sell(char *arg, CharData * ch, CharData * keeper, int shop_nr);
void shopping_value(char *arg, CharData * ch,  CharData * keeper, int shop_nr);
void shopping_list(char *arg, CharData * ch, CharData * keeper, int shop_nr);
int read_list(FILE * shop_f, struct shop_buy_data * list, int new_format, int max, int type);
int read_type_list(FILE * shop_f, struct shop_buy_data * list, int new_format, int max);


const char *operator_str[] = {
	"[({",
	"])}",
	"|+",
	"&*",
	"^'"
} ;
/* Constant list for printing out who we sell to */
char *trade_letters[] = {
	"Human",		/* Then the class based ones */
	"Predator",
	"Alien"
	"\n"
} ;
char *shop_bits[] = {
	"WILL_FIGHT",
	"USES_BANK",
	"\n"
} ;



int is_ok_char(CharData * keeper, CharData * ch, int shop_nr) {
	char *buf;

	if (!keeper->CanSee(ch)) {
		do_say(keeper, MSG_NO_SEE_CHAR, cmd_say, "say", 0);
		return (FALSE);
	}
  
	if (NO_STAFF_HASSLE(ch))
		return (TRUE);

/*  if (IS_NPC(ch) ||)
	return (TRUE);*/

	if ((IS_MARINE(ch) && NOTRADE_HUMAN(shop_nr)) ||
			(IS_PREDATOR(ch) && NOTRADE_PREDATOR(shop_nr)) ||
			(IS_ALIEN(ch) && NOTRADE_ALIEN(shop_nr))) {
		buf = get_buffer(256);
		sprintf(buf, "%s %s", ch->GetName(), MSG_NO_SELL_CLASS);
		do_tell(keeper, buf, cmd_tell, "tell", 0);
		release_buffer(buf);
		return (FALSE);
	}
	return (TRUE);
}


int is_open(CharData * keeper, int shop_nr, int msg) {
	char *buf = get_buffer(256);

	if (SHOP_OPEN1(shop_nr) > time_info.hours)
		strcpy(buf, MSG_NOT_OPEN_YET);
	else if (SHOP_CLOSE1(shop_nr) < time_info.hours)
		if (SHOP_OPEN2(shop_nr) > time_info.hours)
			strcpy(buf, MSG_NOT_REOPEN_YET);
		else if (SHOP_CLOSE2(shop_nr) < time_info.hours)
			strcpy(buf, MSG_CLOSED_FOR_DAY);

	if (!(*buf)) {
		release_buffer(buf);
		return (TRUE);
	}
	if (msg)
		do_say(keeper, buf, cmd_tell, "say", 0);
	release_buffer(buf);
	return (FALSE);
}


int is_ok(CharData * keeper, CharData * ch, int shop_nr) {
	if (is_open(keeper, shop_nr, TRUE))		return (is_ok_char(keeper, ch, shop_nr));
	else									return (FALSE);
}


void push(struct stack_data * stack, int pushval)
{
  S_DATA(stack, S_LEN(stack)++) = pushval;
}


int top(struct stack_data * stack)
{
  if (S_LEN(stack) > 0)
    return (S_DATA(stack, S_LEN(stack) - 1));
  else
    return (NOTHING);
}


int pop(struct stack_data * stack)
{
  if (S_LEN(stack) > 0)
    return (S_DATA(stack, --S_LEN(stack)));
  else {
    log("Illegal expression %d in shop keyword list", S_LEN(stack));
    return (0);
  }
}


void evaluate_operation(struct stack_data * ops, struct stack_data * vals)
{
  int oper;

  if ((oper = pop(ops)) == OPER_NOT)
    push(vals, !pop(vals));
  else if (oper == OPER_AND)
    push(vals, pop(vals) && pop(vals));
  else if (oper == OPER_OR)
    push(vals, pop(vals) || pop(vals));
}


int find_oper_num(char token)
{
  int index;

  for (index = 0; index <= MAX_OPER; index++)
    if (strchr(operator_str[index], token))
      return (index);
  return (NOTHING);
}


int evaluate_expression(ObjData * obj, char *expr)
{
  struct stack_data ops, vals;
  char *ptr, *end, *name;
  int temp, index;

	if (!expr)
		return TRUE;

	if (!isalpha(*expr))
		return TRUE;

	ops.len = vals.len = 0;
	ptr = expr;
	name = get_buffer(256);
	while (*ptr) {
		if (isspace(*ptr))
			ptr++;
		else {
			if ((temp = find_oper_num(*ptr)) == NOTHING) {
				end = ptr;
				while (*ptr && !isspace(*ptr) && (find_oper_num(*ptr) == NOTHING))
					ptr++;
				strncpy(name, end, ptr - end);
				name[ptr - end] = 0;
				for (index = 0; *extra_bits[index] != '\n'; index++)
					if (!str_cmp(name, extra_bits[index])) {
						push(&vals, OBJ_FLAGGED(obj, 1 << index));
						break;
					}
				if (*extra_bits[index] == '\n')
					push(&vals, isname(name, SSData(obj->name)));
			} else {
				if (temp != OPER_OPEN_PAREN)
					while (top(&ops) > temp)
						evaluate_operation(&ops, &vals);

				if (temp == OPER_CLOSE_PAREN) {
					if ((temp = pop(&ops)) != OPER_OPEN_PAREN) {
						log("Illegal parenthesis in shop keyword expression.");
						release_buffer(name);
						return (FALSE);
					}
				} else
					push(&ops, temp);
				ptr++;
			}
		}
	}
	release_buffer(name);
	while (top(&ops) != NOTHING)
		evaluate_operation(&ops, &vals);
	temp = pop(&vals);
	if (top(&vals) != NOTHING) {
		log("Extra operands left on shop keyword expression stack.");
		return (FALSE);
	}
	return (temp);
}


int trade_with(ObjData * item, int shop_nr) {
	SInt32		counter;
//	SInt32		contentTest;
	ObjData *	content;

	//	Is it worth anything?  --  get_selling_obj needs this check
	if (item->TotalValue() < 1)
		return (OBJECT_NOVAL);

	//	IS it a no-sell item?
	if (OBJ_FLAGGED(item, ITEM_NOSELL))
		return (OBJECT_NOTOK);

	//	Makes sure it doesn't contain a no-sell item
	//	Moved up so that it can't be overridden by a buyword, because
	//	if it is, it would either return OBJECT_CONT_NOTOK, or OBJECT_NOTOK
	//	which is pretty useless.
	LListIterator<ObjData *>	iter(item->contains);
	while ((content = iter.Next()))
		if (trade_with(content, shop_nr) != OBJECT_OK)
			return OBJECT_CONT_NOTOK;
	
	for (counter = 0; SHOP_BUYTYPE(shop_nr, counter); counter++)
		if (SHOP_BUYTYPE(shop_nr, counter) == GET_OBJ_TYPE(item))
			if (evaluate_expression(item, SHOP_BUYWORD(shop_nr, counter)))
				return (OBJECT_OK);


	return (OBJECT_NOTOK);
}


bool same_obj(ObjData * obj1, ObjData * obj2) {
	int index;

	if (!obj1 || !obj2)
		return (obj1 == obj2);

	if (GET_OBJ_RNUM(obj1) != GET_OBJ_RNUM(obj2))
		return false;

	if (GET_OBJ_COST(obj1) != GET_OBJ_COST(obj2))
		return false;

	if (GET_OBJ_EXTRA(obj1) != GET_OBJ_EXTRA(obj2))
		return false;

	if (GET_OBJ_WEIGHT(obj1) != GET_OBJ_WEIGHT(obj2))
		return false;
	
	for (index = 0; index < 8; index++)
		if (GET_OBJ_VAL(obj1, index) != GET_OBJ_VAL(obj2, index))
			return false;
	
	if (str_cmp(SSData(obj1->name), SSData(obj2->name)))
		return false;
	
	if (str_cmp(SSData(obj1->shortDesc), SSData(obj2->shortDesc)))
		return false;
	
	if (str_cmp(SSData(obj1->description), SSData(obj2->description)))
		return false;
	
//	if (str_cmp(SSData(obj1->actionDesc), SSData(obj2->actionDesc)))
//		return false;
	
	
	for (index = 0; index < MAX_OBJ_AFFECT; index++)
		if ((obj1->affect[index].location != obj2->affect[index].location) ||
				(obj1->affect[index].modifier != obj2->affect[index].modifier))
			return false;

	return true;
}


bool shop_producing(ObjData * item, int shop_nr) {
	int counter;

	if (GET_OBJ_RNUM(item) < 0)
		return false;

	for (counter = 0; SHOP_PRODUCT(shop_nr, counter) != NOTHING; counter++)
		if (same_obj(item, (ObjData *)obj_index[SHOP_PRODUCT(shop_nr, counter)].proto))
			return true;

	return false;
}


int transaction_amt(char *arg) {
	int num;
	char *buf = get_buffer(MAX_INPUT_LENGTH);
	
	one_argument(arg, buf);
	if (*buf)
		if ((is_number(buf))) {
			num = atoi(buf);
			strcpy(arg, arg + strlen(buf) + 1);
			release_buffer(buf);
			return (num);
		}
	release_buffer(buf);
	return (1);
}


char *times_message(ObjData * obj, char *name, int num)
{
	static char buf[256];
	char *ptr;

	if (obj)
		strcpy(buf, SSData(obj->shortDesc));
	else {
		if ((ptr = strchr(name, '.')) == NULL)	ptr = name;
		else									ptr++;
		sprintf(buf, "%s %s", AN(ptr), ptr);
	}

	if (num > 1)
		sprintf(END_OF(buf), " (x %d)", num);

	return (buf);
}


ObjData *get_slide_obj_vis(CharData * ch, char *name, LList<ObjData *> &list) {
  ObjData *i, *last_match = 0;
  int j, number;
  char *tmpname = get_buffer(MAX_INPUT_LENGTH);
  char *tmp;

  strcpy(tmpname, name);
  tmp = tmpname;
  if (!(number = get_number(&tmp))) {
    release_buffer(tmpname);
    return NULL;
  }
  release_buffer(tmpname);

	j = 1;
	START_ITER(iter, i, ObjData *, list) {
		if (j > number)
			break;
		if (ch->CanSee(i) && isname(tmp, SSData(i->name)) && !same_obj(last_match, i)) {
			if (j == number) {
				END_ITER(iter);
				return i;
			}
			last_match = i;
			j++;
		}
	} END_ITER(iter);
	return NULL;
}


ObjData *get_hash_obj_vis(CharData * ch, char *name, LList<ObjData *> &list) {
	ObjData *loop, *last_obj = 0;
	int index;

	if ((is_number(name + 1)))		index = atoi(name + 1);
	else							return NULL;

	START_ITER(iter, loop, ObjData *, list) {
		if (ch->CanSee(loop) && (loop->TotalValue() > 0) && !same_obj(last_obj, loop)) {
			if (--index == 0) {
				END_ITER(iter);
				return loop;
			}
			last_obj = loop;
		}
	} END_ITER(iter);
	return NULL;
}


ObjData *get_purchase_obj(CharData * ch, char *arg, CharData * keeper, int shop_nr, int msg) {
	char	*buf = get_buffer(MAX_STRING_LENGTH),
			*name = get_buffer(MAX_INPUT_LENGTH);
	ObjData *obj;

	one_argument(arg, name);
	do {
		if (*name == '#')	obj = get_hash_obj_vis(ch, name, keeper->carrying);
		else				obj = get_slide_obj_vis(ch, name, keeper->carrying);
		if (!obj) {
			if (msg) {
				sprintf(buf, shop_index[shop_nr].no_such_item1, ch->GetName());
				do_tell(keeper, buf, cmd_tell, "tell", 0);
			}
			release_buffer(buf);
			release_buffer(name);
			return NULL;
		}
		if (obj->TotalValue() <= 0) {
			obj->extract();
			obj = NULL;
		}
	} while (!obj);
	release_buffer(buf);
	release_buffer(name);
	return (obj);
}


int buy_price(ObjData * obj, int shop_nr) {
	return ((int) (obj->TotalValue() * SHOP_BUYPROFIT(shop_nr)));
}


void shopping_buy(char *arg, CharData * ch, CharData * keeper, int shop_nr) {
	char	*tempstr, *buf;
	ObjData *obj, *last_obj = NULL;
	int goldamt = 0, buynum, bought = 0;

	if (!(is_ok(keeper, ch, shop_nr)))
		return;

	if (SHOP_SORT(shop_nr) < IS_CARRYING_N(keeper))
		sort_keeper_objs(keeper, shop_nr);

	buf = get_buffer(MAX_STRING_LENGTH);

	if ((buynum = transaction_amt(arg)) < 0) {
		sprintf(buf, "%s A negative amount?  Try selling me something.", ch->GetName());
		do_tell(keeper, buf, cmd_tell, "tell", 0);
		release_buffer(buf);
		return;
	}
	if (!(*arg) || !(buynum)) {
		sprintf(buf, "%s What do you want to buy??", ch->GetName());
		do_tell(keeper, buf, cmd_tell, "tell", 0);
		release_buffer(buf);
		return;
	}
	if (!(obj = get_purchase_obj(ch, arg, keeper, shop_nr, TRUE))) {
		release_buffer(buf);
		return;
	}  


	if ((buy_price(obj, shop_nr) > GET_MISSION_POINTS(ch)) && !NO_STAFF_HASSLE(ch)) {
		sprintf(buf, shop_index[shop_nr].missing_cash2, ch->GetName());
		do_tell(keeper, buf, cmd_tell, "tell", 0);
		release_buffer(buf);

		switch (SHOP_BROKE_TEMPER(shop_nr)) {
			case 0:
				do_action(keeper, const_cast<char *>(ch->GetName()), cmd_puke, "puke", 0);
				break;
			case 1:
				do_echo(keeper, "smokes on his joint.", cmd_emote, "emote", SCMD_EMOTE);
				break;
			default:
				break;
		}
		return;
	}
/*
  if ((IS_CARRYING_N(ch) + 1 > CAN_CARRY_N(ch))) {
    sprintf(buf, "%s: You can't carry any more items.\r\n", fname(SSData(obj->name)));
    send_to_char(buf, ch);
    release_buffer(buf);
    return;
  }
  if ((IS_CARRYING_W(ch) + GET_OBJ_WEIGHT(obj)) > CAN_CARRY_W(ch)) {
    sprintf(buf, "%s: You can't carry that much weight.\r\n", fname(SSData(obj->name)));
    send_to_char(buf, ch);
    release_buffer(buf);
    return;
  }
*/
	if (!can_take_obj(ch, obj)) {
		release_buffer(buf);
		return;
	}
	while ((obj) && ((GET_MISSION_POINTS(ch) >= buy_price(obj, shop_nr)) || NO_STAFF_HASSLE(ch))
			&& (bought < buynum)) {
		bought++;
		/* Test if producing shop ! */
		if (shop_producing(obj, shop_nr))
			obj = new ObjData(obj);
//			obj = read_object(GET_OBJ_RNUM(obj), REAL);
		else {
			obj->from_char();
			SHOP_SORT(shop_nr)--;
		}
		obj->to_char(ch);

		goldamt += buy_price(obj, shop_nr);
		if (!NO_STAFF_HASSLE(ch))
			GET_MISSION_POINTS(ch) -= buy_price(obj, shop_nr);

		last_obj = obj;
		obj = get_purchase_obj(ch, arg, keeper, shop_nr, FALSE);
		if (!same_obj(obj, last_obj))
			break;
	}

	if (bought < buynum) {
		if (!obj || !same_obj(last_obj, obj))
			sprintf(buf, "%s I only have %d to sell you.", ch->GetName(), bought);
		else if (GET_MISSION_POINTS(ch) < buy_price(obj, shop_nr))
			sprintf(buf, "%s You can only afford %d.", ch->GetName(), bought);
		else
			sprintf(buf, "%s Something screwy only gave you %d.", ch->GetName(), bought);
		do_tell(keeper, buf, cmd_tell, "tell", 0);
	}
//  if (!NO_STAFF_HASSLE(ch))
//    GET_MISSION_POINTS(keeper) += goldamt;
	tempstr = get_buffer(200),

	sprintf(tempstr, times_message(ch->carrying.Top(), 0, bought));
	sprintf(buf, "$n buys %s.", tempstr);
	act(buf, FALSE, ch, obj, 0, TO_ROOM);

	sprintf(buf, shop_index[shop_nr].message_buy, ch->GetName(), goldamt);
	do_tell(keeper, buf, cmd_tell, "tell", 0);
	sprintf(buf, "You now have %s.\r\n", tempstr);
	send_to_char(buf, ch);

//  if (SHOP_USES_BANK(shop_nr))
//    if (GET_MISSION_POINTS(keeper) > MAX_OUTSIDE_BANK) {
//      SHOP_BANK(shop_nr) += (GET_MISSION_POINTS(keeper) - MAX_OUTSIDE_BANK);
//      GET_MISSION_POINTS(keeper) = MAX_OUTSIDE_BANK;
//    }
	release_buffer(buf);
	release_buffer(tempstr);
}


ObjData *get_selling_obj(CharData * ch, char *name, CharData * keeper, int shop_nr, int msg) {
	char		*buf;
	ObjData		*obj;
	int			result;

	if (!(obj = get_obj_in_list_vis(ch, name, ch->carrying))) {
		if (msg) {
			buf = get_buffer(MAX_STRING_LENGTH);
			sprintf(buf, shop_index[shop_nr].no_such_item2, ch->GetName());
			do_tell(keeper, buf, cmd_tell, "tell", 0);
			release_buffer(buf);
		}
		return NULL;
	}
	if ((result = trade_with(obj, shop_nr)) == OBJECT_OK)
		return (obj);

	buf = get_buffer(SMALL_BUFSIZE);
	switch (result) {
		case OBJECT_NOVAL:
			sprintf(buf, "%c%d You've got to be kidding, that thing is worthless!", UID_CHAR, GET_ID(ch));
			break;
		case OBJECT_NOTOK:
			sprintf(buf, shop_index[shop_nr].do_not_buy, ch->GetName());
			break;
		case OBJECT_DEAD:
			sprintf(buf, "%c%d %s", UID_CHAR, GET_ID(ch), MSG_NO_USED_WANDSTAFF);
			break;
		case OBJECT_CONT_NOTOK:
			sprintf(buf, "%c%d I won't buy the contents of it!", UID_CHAR, GET_ID(ch));
			break;
		default:
			log("Illegal return value of %d from trade_with() (" __FILE__ ")", result);
			sprintf(buf, "%c%d An error has occurred.", UID_CHAR, GET_ID(ch));
			break;
	}
	if (msg)	do_tell(keeper, buf, cmd_tell, "tell", 0);
	release_buffer(buf);
	return NULL;
}


int sell_price(CharData * ch, ObjData * obj, int shop_nr) {
	return ((int) (obj->TotalValue() * SHOP_SELLPROFIT(shop_nr)));
}


//	Finally rewrote to sort.  With the InsertBefore() function added to
//	the LList template, it became MUCH easier!
ObjData *slide_obj(ObjData * obj, CharData * keeper, int shop_nr) {
	int temp;
	ObjData *loop;

	if (SHOP_SORT(shop_nr) < IS_CARRYING_N(keeper))
		sort_keeper_objs(keeper, shop_nr);

	/* Extract the object if it is identical to one produced */
	if (shop_producing(obj, shop_nr)) {
		temp = GET_OBJ_RNUM(obj);
		obj->extract();
		return (ObjData *)obj_index[temp].proto;
	}
	SHOP_SORT(shop_nr)++;
	obj->to_char(keeper);			//	Move it to the keeper...
	keeper->carrying.Remove(obj);	//	But we want to re-order it

	LListIterator<ObjData *>	iter(keeper->carrying);
	
	while((loop = iter.Next())) {
		if (same_obj(obj, loop)) {
			keeper->carrying.InsertBefore(obj, loop);
			return obj;
		}
	}
	keeper->carrying.Prepend(obj);
	return obj;
}


void sort_keeper_objs(CharData * keeper, int shop_nr) {
	ObjData				*temp;
	LList<ObjData *>	list;
	
	while (SHOP_SORT(shop_nr) < IS_CARRYING_N(keeper)) {
		temp = keeper->carrying.Top();
		temp->from_char();
		list.Prepend(temp);
	}

	while (list.Count()) {
		temp = list.Top();
		list.Remove(temp);
		if ((shop_producing(temp, shop_nr)) && !(get_obj_in_list_num(GET_OBJ_RNUM(temp), keeper->carrying))) {
			temp->to_char(keeper);
			SHOP_SORT(shop_nr)++;
		} else
			slide_obj(temp, keeper, shop_nr);
	}
}


void shopping_sell(char *arg, CharData * ch, CharData * keeper, int shop_nr) {
	char	*tempstr, *buf, *name;
	ObjData *obj, *tag = 0;
	int sellnum, sold = 0, goldamt = 0;

	if (!(is_ok(keeper, ch, shop_nr)))
		return;

	buf = get_buffer(SMALL_BUFSIZE);
	if ((sellnum = transaction_amt(arg)) < 0) {
		sprintf(buf, "%s A negative amount?  Try buying something.", ch->GetName());
		do_tell(keeper, buf, cmd_tell, "tell", 0);
		release_buffer(buf);
		return;
	}
	if (!*arg || !sellnum) {
		sprintf(buf, "%s What do you want to sell??", ch->GetName());
		do_tell(keeper, buf, cmd_tell, "tell", 0);
		release_buffer(buf);
		return;
	}
	name = get_buffer(MAX_INPUT_LENGTH);
	one_argument(arg, name);

	obj = get_selling_obj(ch, name, keeper, shop_nr, TRUE);
	if (!obj) {
		release_buffer(buf);
		release_buffer(name);
		return;
	}
	
//	if (GET_MISSION_POINTS(keeper) + SHOP_BANK(shop_nr) < sell_price(ch, obj, shop_nr)) {
//		sprintf(buf, shop_index[shop_nr].missing_cash1, ch->GetName());
//		do_tell(keeper, buf, cmd_tell, 0);
//		release_buffer(name);
//		release_buffer(buf);
//		return;
//	}
	while ((obj) && /* (GET_MISSION_POINTS(keeper) + SHOP_BANK(shop_nr) >=
			sell_price(ch, obj, shop_nr)) && */ (sold < sellnum)) {
		sold++;

		goldamt += sell_price(ch, obj, shop_nr);
//		GET_MISSION_POINTS(keeper) -= sell_price(ch, obj, shop_nr);

		obj->from_char();
		tag = slide_obj(obj, keeper, shop_nr);
		obj = get_selling_obj(ch, name, keeper, shop_nr, FALSE);
	}

	if (sold < sellnum) {
		if (!obj)
			sprintf(buf, "%s You only have %d of those.", ch->GetName(), sold);
//		else if (GET_MISSION_POINTS(keeper) + SHOP_BANK(shop_nr) < sell_price(ch, obj, shop_nr))
//			sprintf(buf, "%s I can only afford to buy %d of those.", ch->GetName(), sold);
		else
			sprintf(buf, "%s Something really screwy made me buy %d.", ch->GetName(), sold);

		do_tell(keeper, buf, cmd_tell, "tell", 0);
	}
	tempstr = get_buffer(200);

	GET_MISSION_POINTS(ch) += goldamt;
	strcpy(tempstr, times_message(0, name, sold));
	sprintf(buf, "$n sells %s.", tempstr);
	act(buf, FALSE, ch, obj, 0, TO_ROOM);

	sprintf(buf, shop_index[shop_nr].message_sell, ch->GetName(), goldamt);
	do_tell(keeper, buf, cmd_tell, "tell", 0);
	sprintf(buf, "The shopkeeper now has %s.\r\n", tempstr);
	send_to_char(buf, ch);

	release_buffer(buf);
	release_buffer(tempstr);
	release_buffer(name);
  
//	if (GET_MISSION_POINTS(keeper) < MIN_OUTSIDE_BANK) {
//		goldamt = MIN(MAX_OUTSIDE_BANK - GET_MISSION_POINTS(keeper), SHOP_BANK(shop_nr));
//		SHOP_BANK(shop_nr) -= goldamt;
//		GET_MISSION_POINTS(keeper) += goldamt;
//	}
}


void shopping_value(char *arg, CharData * ch,
		         CharData * keeper, int shop_nr)
{
  char *buf, *name;
  ObjData *obj;

  if (!(is_ok(keeper, ch, shop_nr)))
    return;

  if (!(*arg)) {
    buf = get_buffer(MAX_INPUT_LENGTH);
    sprintf(buf, "%s What do you want me to valuate??", ch->GetName());
    do_tell(keeper, buf, cmd_tell, "tell", 0);
    release_buffer(buf);
    return;
  }
  name = get_buffer(MAX_INPUT_LENGTH);
  one_argument(arg, name);
  obj = get_selling_obj(ch, name, keeper, shop_nr, TRUE);
  release_buffer(name);
  if (!obj)
    return;

  buf = get_buffer(MAX_INPUT_LENGTH);
  sprintf(buf, "%s I'll give you %d MP for that!", ch->GetName(),
	  sell_price(ch, obj, shop_nr));
  do_tell(keeper, buf, cmd_tell, "tell", 0);
  release_buffer(buf);

  return;
}


char *list_object(ObjData * obj, int cnt, int index, int shop_nr) {
	static char buf[256];
	char		*buf2 = get_buffer(300),
				*buf3 = get_buffer(200);
	SInt32		contentCount;

	if (shop_producing(obj, shop_nr))	strcpy(buf2, "Unlimited   ");
	else								sprintf(buf2, "%5d       ", cnt);
	sprintf(buf, " %2d)  %s", index, buf2);

	/* Compile object name and information */
	strcpy(buf3, SSData(obj->shortDesc));
	if ((GET_OBJ_TYPE(obj) == ITEM_DRINKCON) && (GET_OBJ_VAL(obj, 1)))
		sprintf(END_OF(buf3), " of %s", drinks[GET_OBJ_VAL(obj, 2)]);
	else if ((contentCount = obj->contains.Count()))
		sprintf(END_OF(buf3), " (%d content%s)", contentCount, ((contentCount > 1) ? "s" : ""));

	/* FUTURE: */
	/* Add glow/hum/etc */

	sprintf(buf2, "%-48s %6d\r\n", buf3, buy_price(obj, shop_nr));
	strcat(buf, CAP(buf2));
	release_buffer(buf2);
	release_buffer(buf3);

	return (buf);
}


void shopping_list(char *arg, CharData * ch, CharData * keeper, int shop_nr) {
	char *buf, *name;
	ObjData *obj, *last_obj = 0;
	int cnt = 0, index = 0;

	if (!(is_ok(keeper, ch, shop_nr)))
		return;
    

	if (SHOP_SORT(shop_nr) < IS_CARRYING_N(keeper))
		sort_keeper_objs(keeper, shop_nr);

	buf = get_buffer(MAX_STRING_LENGTH);
	name = get_buffer(200);
  
	one_argument(arg, name);
  
	strcpy(buf, " ##   Available   Item                                               Cost\r\n");
	strcat(buf, "-------------------------------------------------------------------------\r\n");
	START_ITER(iter, obj, ObjData *, keeper->carrying) {
		if (ch->CanSee(obj) && (obj->TotalValue() > 0)) {
			if (!last_obj) {
				last_obj = obj;
				cnt = 1;
			} else if (same_obj(last_obj, obj))
				cnt++;
			else {
				index++;
				if (!(*name) || isname(name, SSData(last_obj->name)))
					strcat(buf, list_object(last_obj, cnt, index, shop_nr));
				cnt = 1;
				last_obj = obj;
			}
		}
	} END_ITER(iter);
	index++;
	if (!last_obj) {
		if (*name)	strcpy(buf, "Presently, none of those are for sale.\r\n");
		else		strcpy(buf, "Currently, there is nothing for sale.\r\n");
	} else if (!(*name) || isname(name, SSData(last_obj->name)))
		strcat(buf, list_object(last_obj, cnt, index, shop_nr));

	release_buffer(name);
	page_string(ch->desc, buf, 1);
	release_buffer(buf);
}


int ok_shop_room(int shop_nr, int room)
{
  int index;

  for (index = 0; SHOP_ROOM(shop_nr, index) != NOWHERE; index++)
    if (SHOP_ROOM(shop_nr, index) == room)
      return (TRUE);
  return (FALSE);
}


SPECIAL(shop_keeper)
{
  CharData *keeper = (CharData *) me;
  int shop_nr;

  for (shop_nr = 0; shop_nr < top_shop; shop_nr++)
    if (SHOP_KEEPER(shop_nr) == keeper->nr)
      break;

  if (shop_nr >= top_shop)
    return (FALSE);

  if (SHOP_FUNC(shop_nr))	/* Check secondary function */
    if ((SHOP_FUNC(shop_nr)) (ch, me, cmd, NULL))
      return (TRUE);

  if (keeper == ch) {
    if (cmd)
      SHOP_SORT(shop_nr) = 0;	/* Safety in case "drop all" */
    return (FALSE);
  }
  if (!ok_shop_room(shop_nr, world[IN_ROOM(ch)].number))
    return (0);

  if (!AWAKE(keeper))
    return (FALSE);

  if (CMD_IS("buy")) {
    shopping_buy(argument, ch, keeper, shop_nr);
    return (TRUE);
  } else if (CMD_IS("sell")) {
    shopping_sell(argument, ch, keeper, shop_nr);
    return (TRUE);
  } else if (CMD_IS("value")) {
    shopping_value(argument, ch, keeper, shop_nr);
    return (TRUE);
  } else if (CMD_IS("list")) {
    shopping_list(argument, ch, keeper, shop_nr);
    return (TRUE);
  }
  return (FALSE);
}


int ok_damage_shopkeeper(CharData * ch, CharData * victim) {
	if (IS_NPC(victim) && (mob_index[GET_MOB_RNUM(victim)].func == shop_keeper))
		return (FALSE);
	return (TRUE);
}


int add_to_list(struct shop_buy_data * list, int type, int *len, int *val)
{
  if (*val >= 0)
    if (*len < MAX_SHOP_OBJ) {
      if (type == LIST_PRODUCE)
	*val = real_object(*val);
      if (*val >= 0) {
	BUY_TYPE(list[*len]) = *val;
	BUY_WORD(list[(*len)++]) = 0;
      } else
	*val = 0;
      return (FALSE);
    } else
      return (TRUE);
  return (FALSE);
}


int end_read_list(struct shop_buy_data * list, int len, int error) {
	if (error)
		log("Raise MAX_SHOP_OBJ constant in shop.h to %d", len + error);
	BUY_WORD(list[len]) = 0;
	BUY_TYPE(list[len++]) = NOTHING;
	return (len);
}


void read_line(FILE * shop_f, char *string, Ptr data)
{
  char *buf = get_buffer(MAX_STRING_LENGTH);
  if (!get_line(shop_f, buf) || !sscanf(buf, string, data)) {
    log("Error in shop #%d", SHOP_NUM(top_shop));
    exit(1);
  }
  release_buffer(buf);
}


int read_list(FILE * shop_f, struct shop_buy_data * list, int new_format,
	          int max, int type)
{
  int count, temp, len = 0, error = 0;

  if (new_format) {
    do {
      read_line(shop_f, "%d", &temp);
      error += add_to_list(list, type, &len, &temp);
    } while (temp >= 0);
  } else
    for (count = 0; count < max; count++) {
      read_line(shop_f, "%d", &temp);
      error += add_to_list(list, type, &len, &temp);
    }
  return (end_read_list(list, len, error));
}


int read_type_list(FILE * shop_f, struct shop_buy_data * list,
		       int new_format, int max)
{
  int index, num, len = 0, error = 0;
  char *ptr, *buf;

  if (!new_format)
    return (read_list(shop_f, list, 0, max, LIST_TRADE));
  buf = get_buffer(MAX_STRING_LENGTH);
  do {
    fgets(buf, MAX_STRING_LENGTH - 1, shop_f);
    if ((ptr = strchr(buf, ';')) != NULL)
      *ptr = 0;
    else
      *(END_OF(buf) - 1) = 0;
    for (index = 0, num = NOTHING; *item_types[index] != '\n'; index++)
      if (!strn_cmp(item_types[index], buf, strlen(item_types[index]))) {
	num = index;
	strcpy(buf, buf + strlen(item_types[index]));
	break;
      }
    ptr = buf;
    if (num == NOTHING) {
      sscanf(buf, "%d", &num);
      while (!isdigit(*ptr))
	ptr++;
      while (isdigit(*ptr))
	ptr++;
    }
    while (isspace(*ptr))
      ptr++;
    while (isspace(*(END_OF(ptr) - 1)))
      *(END_OF(ptr) - 1) = 0;
    error += add_to_list(list, LIST_TRADE, &len, &num);
    if (*ptr)
      BUY_WORD(list[len - 1]) = str_dup(ptr);
  } while (num >= 0);
  release_buffer(buf);
  return (end_read_list(list, len, error));
}


void boot_the_shops(FILE * shop_f, char *filename, int rec_count)
{
  char *buf, *buf2 = get_buffer(150);
  int temp, count, new_format = 0;
  struct shop_buy_data list[MAX_SHOP_OBJ + 1];
  int done = 0;

  sprintf(buf2, "beginning of shop file %s", filename);

  while (!done) {
    buf = fread_string(shop_f, buf2, filename);
    if (*buf == '#') {		/* New shop */
      sscanf(buf, "#%d\n", &temp);
      sprintf(buf2, "shop #%d in shop file %s", temp, filename);
      FREE(buf);		/* Plug memory leak! */
      if (!top_shop)
	CREATE(shop_index, struct shop_data, rec_count);

      SHOP_NUM(top_shop) = temp;
      temp = read_list(shop_f, list, new_format, MAX_PROD, LIST_PRODUCE);
      CREATE(shop_index[top_shop].producing, int, temp);
      for (count = 0; count < temp; count++)
	SHOP_PRODUCT(top_shop, count) = BUY_TYPE(list[count]);

      read_line(shop_f, "%f", &SHOP_BUYPROFIT(top_shop));
      read_line(shop_f, "%f", &SHOP_SELLPROFIT(top_shop));

      temp = read_type_list(shop_f, list, new_format, MAX_TRADE);
      CREATE(shop_index[top_shop].type, struct shop_buy_data, temp);
      for (count = 0; count < temp; count++) {
	SHOP_BUYTYPE(top_shop, count) = BUY_TYPE(list[count]);
	SHOP_BUYWORD(top_shop, count) = BUY_WORD(list[count]);
      }

      shop_index[top_shop].no_such_item1 = fread_string(shop_f, buf2, filename);
      shop_index[top_shop].no_such_item2 = fread_string(shop_f, buf2, filename);
      shop_index[top_shop].do_not_buy = fread_string(shop_f, buf2, filename);
      shop_index[top_shop].missing_cash1 = fread_string(shop_f, buf2, filename);
      shop_index[top_shop].missing_cash2 = fread_string(shop_f, buf2, filename);
      shop_index[top_shop].message_buy = fread_string(shop_f, buf2, filename);
      shop_index[top_shop].message_sell = fread_string(shop_f, buf2, filename);
      read_line(shop_f, "%d", &SHOP_BROKE_TEMPER(top_shop));
      read_line(shop_f, "%d", &SHOP_BITVECTOR(top_shop));
      read_line(shop_f, "%d", &SHOP_KEEPER(top_shop));

      SHOP_KEEPER(top_shop) = real_mobile(SHOP_KEEPER(top_shop));
      read_line(shop_f, "%d", &SHOP_TRADE_WITH(top_shop));

      temp = read_list(shop_f, list, new_format, 1, LIST_ROOM);
      CREATE(shop_index[top_shop].in_room, int, temp);
      for (count = 0; count < temp; count++)
	SHOP_ROOM(top_shop, count) = BUY_TYPE(list[count]);

      read_line(shop_f, "%d", &SHOP_OPEN1(top_shop));
      read_line(shop_f, "%d", &SHOP_CLOSE1(top_shop));
      read_line(shop_f, "%d", &SHOP_OPEN2(top_shop));
      read_line(shop_f, "%d", &SHOP_CLOSE2(top_shop));

      SHOP_BANK(top_shop) = 0;
      SHOP_SORT(top_shop) = 0;
      SHOP_FUNC(top_shop) = 0;
      top_shop++;
    } else {
      if (*buf == '$')		/* EOF */
	done = TRUE;
      else if (strstr(buf, VERSION3_TAG))	/* New format marker */
	new_format = 1;
      FREE(buf);		/* Plug memory leak! */
    }
  }
  release_buffer(buf2);
}


void assign_the_shopkeepers(void)
{
  int index;

  cmd_say = find_command("say");
  cmd_tell = find_command("tell");
  cmd_emote = find_command("emote");
  cmd_slap = find_command("slap");
  cmd_puke = find_command("puke");
  for (index = 0; index < top_shop; index++) {
    if (SHOP_KEEPER(index) == NOBODY)
      continue;
    if (mob_index[SHOP_KEEPER(index)].func)
      SHOP_FUNC(index) = mob_index[SHOP_KEEPER(index)].func;
    mob_index[SHOP_KEEPER(index)].func = shop_keeper;
  }
}


char *customer_string(int shop_nr, int detailed) {
	int index, cnt = 1;
	static char buf[256];

	*buf = '\0';
	for (index = 0; *trade_letters[index] != '\n'; index++, cnt *= 2)
		if (!(SHOP_TRADE_WITH(shop_nr) & cnt))
			if (detailed) {
				if (*buf)
					strcat(buf, ", ");
				strcat(buf, trade_letters[index]);
			} else
				sprintf(END_OF(buf), "%c", *trade_letters[index]);
		else if (!detailed)
			strcat(buf, "_");

	return (buf);
}


void list_all_shops(CharData * ch)
{
  int shop_nr;
  char	*buf = get_buffer(MAX_STRING_LENGTH),
	*buf1 = get_buffer(MAX_STRING_LENGTH),
	*buf2 = get_buffer(MAX_STRING_LENGTH);

  strcpy(buf, "\r\n");
  for (shop_nr = 0; shop_nr < top_shop; shop_nr++) {
    if (!(shop_nr % 19)) {
      strcat(buf, " ##   Virtual   Where    Keeper    Buy   Sell   Customers\r\n");
      strcat(buf, "---------------------------------------------------------\r\n");
    }
    sprintf(buf2, "%3d   %6d   %6d    ", shop_nr + 1, SHOP_NUM(shop_nr),
	    SHOP_ROOM(shop_nr, 0));
    if (SHOP_KEEPER(shop_nr) < 0)
      strcpy(buf1, "<NONE>");
    else
      sprintf(buf1, "%6d", mob_index[SHOP_KEEPER(shop_nr)].vnum);
    sprintf(END_OF(buf2), "%s   %3.2f   %3.2f    ", buf1,
	    SHOP_SELLPROFIT(shop_nr), SHOP_BUYPROFIT(shop_nr));
    strcat(buf2, customer_string(shop_nr, FALSE));
    sprintf(END_OF(buf), "%s\r\n", buf2);
  }
  /*
   * There is a reason for putting these releases above the page_string.
   * If page_string needs memory, we can use these instead of causing
   * it to potentially have to create another buffer.
   */
  release_buffer(buf1);
  release_buffer(buf2);
  page_string(ch->desc, buf, 1);
  release_buffer(buf);
}


void handle_detailed_list(char *buf, char *buf1, CharData * ch)
{
  if ((strlen(buf1) + strlen(buf) < 78) || (strlen(buf) < 20))
    strcat(buf, buf1);
  else {
    strcat(buf, "\r\n");
    send_to_char(buf, ch);
    sprintf(buf, "            %s", buf1);
  }
}


void list_detailed_shop(CharData * ch, int shop_nr)
{
  ObjData *obj;
  CharData *k;
  int index, temp;
  char	*buf = get_buffer(MAX_STRING_LENGTH),
	*buf1 = get_buffer(MAX_STRING_LENGTH);

  sprintf(buf, "Vnum:       [%5d], Rnum: [%5d]\r\n", SHOP_NUM(shop_nr),
	  shop_nr + 1);
  send_to_char(buf, ch);

  strcpy(buf, "Rooms:      ");
  for (index = 0; SHOP_ROOM(shop_nr, index) != NOWHERE; index++) {
    if (index)
      strcat(buf, ", ");
    if ((temp = real_room(SHOP_ROOM(shop_nr, index))) != NOWHERE)
      sprintf(buf1, "%s (#%d)", world[temp].GetName("<ERROR>"), world[temp].number);
    else
      sprintf(buf1, "<UNKNOWN> (#%d)", SHOP_ROOM(shop_nr, index));
    handle_detailed_list(buf, buf1, ch);
  }
  if (!index)
    send_to_char("Rooms:      None!\r\n", ch);
  else {
    strcat(buf, "\r\n");
    send_to_char(buf, ch);
  }

  strcpy(buf, "Shopkeeper: ");
  if (SHOP_KEEPER(shop_nr) >= 0) {
    sprintf(END_OF(buf), "%s (#%d), Special Function: %s\r\n",
	    ((CharData *)mob_index[SHOP_KEEPER(shop_nr)].proto)->RealName(),
	mob_index[SHOP_KEEPER(shop_nr)].vnum, YESNO(SHOP_FUNC(shop_nr)));
    if ((k = get_char_num(SHOP_KEEPER(shop_nr)))) {
      send_to_char(buf, ch);
      sprintf(buf, "Coins:      [%9d], Bank: [%9d] (Total: %d)\r\n",
	 GET_MISSION_POINTS(k), SHOP_BANK(shop_nr), GET_MISSION_POINTS(k) + SHOP_BANK(shop_nr));
    }
  } else
    strcat(buf, "<NONE>\r\n");
  send_to_char(buf, ch);

  strcpy(buf1, customer_string(shop_nr, TRUE));
  sprintf(buf, "Customers:  %s\r\n", (*buf1) ? buf1 : "None");
  send_to_char(buf, ch);

  strcpy(buf, "Produces:   ");
  for (index = 0; SHOP_PRODUCT(shop_nr, index) != NOTHING; index++) {
    obj = (ObjData *)obj_index[SHOP_PRODUCT(shop_nr, index)].proto;
    if (index)
      strcat(buf, ", ");
    sprintf(buf1, "%s (#%d)", SSData(obj->shortDesc), obj_index[SHOP_PRODUCT(shop_nr, index)].vnum);
    handle_detailed_list(buf, buf1, ch);
  }
  if (!index)
    send_to_char("Produces:   Nothing!\r\n", ch);
  else {
    strcat(buf, "\r\n");
    send_to_char(buf, ch);
  }

  strcpy(buf, "Buys:       ");
  for (index = 0; SHOP_BUYTYPE(shop_nr, index) != NOTHING; index++) {
    if (index)
      strcat(buf, ", ");
    sprintf(buf1, "%s (#%d) ", item_types[SHOP_BUYTYPE(shop_nr, index)],
	    SHOP_BUYTYPE(shop_nr, index));
    if (SHOP_BUYWORD(shop_nr, index))
      sprintf(END_OF(buf1), "[%s]", SHOP_BUYWORD(shop_nr, index));
    else
      strcat(buf1, "[all]");
    handle_detailed_list(buf, buf1, ch);
  }
  if (!index)
    send_to_char("Buys:       Nothing!\r\n", ch);
  else {
    strcat(buf, "\r\n");
    send_to_char(buf, ch);
  }

  sprintf(buf, "Buy at:     [%4.2f], Sell at: [%4.2f], Open: [%d-%d, %d-%d]%s",
     SHOP_SELLPROFIT(shop_nr), SHOP_BUYPROFIT(shop_nr), SHOP_OPEN1(shop_nr),
   SHOP_CLOSE1(shop_nr), SHOP_OPEN2(shop_nr), SHOP_CLOSE2(shop_nr), "\r\n");

  send_to_char(buf, ch);

  sprintbit((long) SHOP_BITVECTOR(shop_nr), shop_bits, buf1);
  sprintf(buf, "Bits:       %s\r\n", buf1);
  send_to_char(buf, ch);
  release_buffer(buf1);
  release_buffer(buf);
}


void show_shops(CharData * ch, char *arg)
{
  int shop_nr;

  if (!*arg)
    list_all_shops(ch);
  else {
    if (!str_cmp(arg, ".")) {
      for (shop_nr = 0; shop_nr < top_shop; shop_nr++)
	if (ok_shop_room(shop_nr, world[IN_ROOM(ch)].number))
	  break;

      if (shop_nr == top_shop) {
	send_to_char("This isn't a shop!\r\n", ch);
	return;
      }
    } else if (is_number(arg))
      shop_nr = atoi(arg) - 1;
    else
      shop_nr = -1;

    if ((shop_nr < 0) || (shop_nr >= top_shop)) {
      send_to_char("Illegal shop number.\r\n", ch);
      return;
    }
    list_detailed_shop(ch, shop_nr);
  }
}
