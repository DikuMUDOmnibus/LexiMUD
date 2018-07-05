/************************************************************************
 * OasisOLC - oedit.c                                              v1.5 *
 * Copyright 1996 Harvey Gilpin.                                        *
 * Original author: Levork                                              *
 ************************************************************************/


#include "characters.h"
#include "descriptors.h"
#include "objects.h"
#include "rooms.h"
#include "comm.h"
#include "spells.h"
#include "utils.h"
#include "scripts.h"
#include "db.h"
#include "boards.h"
#include "shop.h"
#include "olc.h"
#include "interpreter.h"
#include "buffer.h"
#include "extradesc.h"


/*------------------------------------------------------------------------*/

//	External variable declarations.
extern struct zone_data *zone_table;
extern int top_of_zone_table;
extern struct shop_data *shop_index;
extern int top_shop;
extern struct attack_hit_type attack_hit_text[];
extern char *item_types[];
extern char *wear_bits[];
extern char *extra_bits[];
extern char *drinks[];
extern char *apply_types[];
extern char *affected_bits[];
extern char *container_bits[];
extern char *skills[];
extern struct board_info_type *board_info;
extern char * ammo_types[];

void free_intlist(struct int_list *intlist);

/*------------------------------------------------------------------------*/

//	Handy macros.
#define S_PRODUCT(s, i) ((s)->producing[(i)])

/*------------------------------------------------------------------------*/

void oedit_disp_container_flags_menu(DescriptorData *d);
void oedit_disp_extradesc_menu(DescriptorData *d);
void oedit_disp_weapon_menu(DescriptorData *d);
void oedit_disp_val1_menu(DescriptorData *d);
void oedit_disp_val2_menu(DescriptorData *d);
void oedit_disp_val3_menu(DescriptorData *d);
void oedit_disp_val4_menu(DescriptorData *d);
void oedit_disp_val5_menu(DescriptorData *d);
void oedit_disp_val6_menu(DescriptorData *d);
void oedit_disp_val7_menu(DescriptorData *d);
void oedit_disp_val8_menu(DescriptorData *d);
void oedit_disp_type_menu(DescriptorData *d);
void oedit_disp_extra_menu(DescriptorData *d);
void oedit_disp_wear_menu(DescriptorData *d);
void oedit_disp_menu(DescriptorData *d);
void oedit_disp_gun_menu(DescriptorData *d);
void oedit_disp_prompt_apply_menu(DescriptorData *d);
void oedit_disp_apply_menu(DescriptorData *d);
void oedit_disp_shoot_menu(DescriptorData *d);
void oedit_disp_ammo_menu(DescriptorData *d);
void oedit_disp_aff_menu(DescriptorData *d);

void oedit_parse(DescriptorData *d, char *arg);
void oedit_liquid_type(DescriptorData *d);
void oedit_setup_new(DescriptorData *d);
void oedit_setup_existing(DescriptorData *d, RNum real_num);
void oedit_save_to_disk(int zone_num);
void oedit_save_internally(DescriptorData *d);

void oedit_disp_scripts_menu(DescriptorData *d);

/*------------------------------------------------------------------------*\
  Utility and exported functions
\*------------------------------------------------------------------------*/

void oedit_setup_new(DescriptorData *d) {
	OLC_OBJ(d) = new ObjData;
	
	OLC_OBJ(d)->name = SSCreate("unfinished object");
	OLC_OBJ(d)->description = SSCreate("An unfinished object is lying here.");
	OLC_OBJ(d)->shortDesc = SSCreate("an unfinished object");
	GET_OBJ_WEAR(OLC_OBJ(d)) = ITEM_WEAR_TAKE;
	OLC_VAL(d) = 0;
	oedit_disp_menu(d);
}
/*------------------------------------------------------------------------*/

void oedit_setup_existing(DescriptorData *d, RNum real_num) {
	ExtraDesc *extradesc, *temp, *temp2;
	ObjData *obj, *proto;
	struct int_list *i, *inthead;
	
	/* allocate object */
	obj = new ObjData;
	proto = (ObjData *)obj_index[real_num].proto;
	*obj = *proto;

	//	Copy all strings over.
	obj->name = SSCreate(SSData(proto->name) ? SSData(proto->name) : "undefined");
	obj->shortDesc = SSCreate(SSData(proto->shortDesc) ? SSData(proto->shortDesc) : "undefined");
	obj->description = SSCreate(SSData(proto->description) ? SSData(proto->description) : "undefined");
	obj->actionDesc = SSCreate(SSData(proto->actionDesc));

	//	Gun data
	if (proto->gun) {
		obj->gun = new GunData();
		*obj->gun = *proto->gun;
	}

	//	Extra descriptions if necessary.
	if (proto->exDesc) {
		temp = new ExtraDesc;
		obj->exDesc = temp;
		for (extradesc = proto->exDesc; extradesc; extradesc = extradesc->next) {
			if (!SSData(extradesc->keyword) || !SSData(extradesc->description))
				continue;

			temp->keyword = SSCreate(SSData(extradesc->keyword));
			temp->description = SSCreate(SSData(extradesc->description));

			if (extradesc->next) {
				temp2 = new ExtraDesc;
				temp->next = temp2;
				temp = temp2;
			} else
				temp->next = NULL;
		}
	}

	if (obj_index[real_num].triggers)
		CREATE(OLC_INTLIST(d), struct int_list, 1);
	inthead = OLC_INTLIST(d);
	for (i = obj_index[real_num].triggers; i; i = i->next) {
		OLC_INTLIST(d)->i = i->i;
		if (i->next) {
			CREATE(OLC_INTLIST(d)->next, struct int_list, 1);
			OLC_INTLIST(d) = OLC_INTLIST(d)->next;
		}
	}
	OLC_INTLIST(d) = inthead;

	/*. Attatch new obj to players descriptor .*/
	OLC_OBJ(d) = obj;
	OLC_VAL(d) = 0;
	oedit_disp_menu(d);
}
/*------------------------------------------------------------------------*/

#define ZCMD zone_table[zone].cmd[cmd_no]

void oedit_save_internally(DescriptorData *d) {
	int i, shop, zone, cmd_no;
	RNum	robj_num;
	bool	found = false;
	ExtraDesc *extradesc, *next_one;
	ObjData *obj, *proto, *old;
	IndexData *new_obj_index;
	DescriptorData *dsc;

	REMOVE_BIT(GET_OBJ_EXTRA(OLC_OBJ(d)), ITEM_DELETED);
	
	if ((GET_OBJ_TYPE(OLC_OBJ(d)) != ITEM_WEAPON) && IS_GUN(OLC_OBJ(d))) {
		delete GET_GUN_INFO(OLC_OBJ(d));
		GET_GUN_INFO(OLC_OBJ(d)) = NULL;
	}
	
	/* write to internal tables */
	if ((robj_num = real_object(OLC_NUM(d))) >= 0) {
		/* we need to run through each and every object currently in the
		 * game to see which ones are pointing to this prototype */

		/* if object is pointing to this prototype, then we need to replace
		 * with the new one */
		START_ITER(iter, obj, ObjData *, object_list) {
			if (GET_OBJ_RNUM(obj) == robj_num) {
/* Copy all the important data, if the item is the same */
#define SAME(field)	(obj->field == old->field)
#define COPY(field) if (SAME(field))	obj->field = OLC_OBJ(d)->field;
#define SHARE(field) if (SAME(field)) { SSFree(obj->field); obj->field = SSShare(OLC_OBJ(d)->field); }

				old = (ObjData *)obj_index[robj_num].proto;
				
				SHARE(name);
				SHARE(shortDesc);
				SHARE(description);
				SHARE(actionDesc);
				
//				SSFree(obj->name);
//				SSFree(obj->shortDesc);
//				SSFree(obj->description);
//				SSFree(obj->actionDesc);

//				for (extradesc = obj->exDesc; extradesc; extradesc = next_one) {
//					next_one = extradesc->next;
//					delete extradesc;
//				}

//				obj->name = SSShare(OLC_OBJ(d)->name);
//				obj->shortDesc = SSShare(OLC_OBJ(d)->shortDesc);
//				obj->description = SSShare(OLC_OBJ(d)->description);
//				obj->actionDesc = SSShare(OLC_OBJ(d)->actionDesc);
				
//				ned = &obj->exDesc;
//				for(extradesc = OLC_OBJ(d)->exDesc; extradesc; extradesc = extradesc->next) {
//					if (!SSData(extradesc->keyword) || !SSData(extradesc->description))
//						continue;
//					next_one = new ExtraDesc;
//					next_one->keyword = SSShare(extradesc->keyword);
//					next_one->description = SSShare(extradesc->description);
//					*ned = next_one;
//					ned = &next_one->next;
//				}


				COPY(value[0]);
				COPY(value[1]);
				COPY(value[2]);
				COPY(value[3]);
				COPY(value[4]);
				COPY(value[5]);
				COPY(value[6]);
				COPY(value[7]);
				COPY(value[8]);
				COPY(cost);
				COPY(weight);
				COPY(timer);
				COPY(type);
				COPY(wear);
				COPY(extra);
				COPY(affects);
				if (IS_GUN(OLC_OBJ(d))) {
					if (!IS_GUN(obj))
						GET_GUN_INFO(obj) = new GunData();
					
					if (IS_GUN(old)) {
						COPY(gun->attack);
						COPY(gun->rate);
						COPY(gun->range);
						COPY(gun->dice.number);
						COPY(gun->dice.size);
						COPY(gun->ammo.type);
					} else {
						obj->gun->attack = OLC_OBJ(d)->gun->attack;
						obj->gun->rate = OLC_OBJ(d)->gun->rate;
						obj->gun->range = OLC_OBJ(d)->gun->range;
						obj->gun->dice.number = OLC_OBJ(d)->gun->dice.number;
						obj->gun->dice.size = OLC_OBJ(d)->gun->dice.size;
						obj->gun->ammo.type = OLC_OBJ(d)->gun->ammo.type;
					}
				} else if (IS_GUN(obj)) {
					delete GET_GUN_INFO(obj);
					GET_GUN_INFO(obj) = NULL;
				}
			}
		} END_ITER(iter);
		
		proto = (ObjData *)obj_index[robj_num].proto;
		/* now safe to free old proto and write over */
		SSFree(proto->name);
		SSFree(proto->description);
		SSFree(proto->shortDesc);
		SSFree(proto->actionDesc);
		for (extradesc = proto->exDesc; extradesc; extradesc = next_one) {
			next_one = extradesc->next;
			delete extradesc;
		}
		if (proto->gun)
			delete proto->gun;
		
		*proto = *OLC_OBJ(d);
		
		proto->name = SSShare(OLC_OBJ(d)->name);
		proto->description = SSShare(OLC_OBJ(d)->description);
		proto->shortDesc = SSShare(OLC_OBJ(d)->shortDesc);
		proto->actionDesc = SSShare(OLC_OBJ(d)->actionDesc);
		proto->exDesc = OLC_OBJ(d)->exDesc;
		OLC_OBJ(d)->gun = NULL;
		OLC_OBJ(d)->exDesc = NULL;
		
		GET_OBJ_RNUM(proto) = robj_num;
		free_intlist(obj_index[robj_num].triggers);
		obj_index[robj_num].triggers = OLC_INTLIST(d);
		OLC_INTLIST(d) = NULL;
	} else {		/*. It's a new object, we must build new tables to contain it .*/

		CREATE(new_obj_index, IndexData, top_of_objt + 2);
		/* start counting through both tables */
		for (i = 0; i <= top_of_objt; i++) {
			/* if we haven't found it */
			if (!found) {
				/* check if current virtual is bigger than our virtual */
				if (obj_index[i].vnum > OLC_NUM(d)) {
					found = TRUE;
					robj_num = i;
					GET_OBJ_RNUM(OLC_OBJ(d)) = robj_num;
					new_obj_index[robj_num].vnum = OLC_NUM(d);
					new_obj_index[robj_num].number = 0;
					new_obj_index[robj_num].func = NULL;
					new_obj_index[robj_num].triggers = OLC_INTLIST(d);
					OLC_INTLIST(d) = NULL;
					new_obj_index[robj_num].proto = proto = new ObjData(OLC_OBJ(d));
					IN_ROOM(proto) = NOWHERE;
					/*. Copy over the obj that should be here .*/
					new_obj_index[robj_num + 1] = obj_index[robj_num];
					proto = (ObjData *)new_obj_index[robj_num + 1].proto;
					GET_OBJ_RNUM(proto) = robj_num + 1;
				} else {
					/* just copy from old to new, no num change */
					new_obj_index[i] = obj_index[i];
				}
			} else {
				/* we HAVE already found it.. therefore copy to object + 1 */
				new_obj_index[i + 1] = obj_index[i];
				proto = (ObjData *)new_obj_index[i + 1].proto;
				GET_OBJ_RNUM(proto) = i + 1;
			}
		}
		if (!found) {
			robj_num = i;
			GET_OBJ_RNUM(OLC_OBJ(d)) = robj_num;
			new_obj_index[robj_num].vnum = OLC_NUM(d);
			new_obj_index[robj_num].number = 0;
			new_obj_index[robj_num].func = NULL;
			new_obj_index[robj_num].triggers = OLC_INTLIST(d);
			OLC_INTLIST(d) = NULL;
			new_obj_index[robj_num].proto = proto = new ObjData(OLC_OBJ(d));
			IN_ROOM(proto) = NOWHERE;
		}

		/* free and replace old tables */
		FREE(obj_index);
		obj_index = new_obj_index;
		top_of_objt++;
		
		/*. Renumber live objects .*/
		START_ITER(obj_iter, obj, ObjData *, object_list) {
			if (GET_OBJ_RNUM (obj) >= robj_num)
				GET_OBJ_RNUM (obj)++;
		} END_ITER(obj_iter);
		
		/*. Renumber purged objects .*/
		START_ITER(purged_iter, obj, ObjData *, purged_objs) {
			if (GET_OBJ_RNUM (obj) >= robj_num)
				GET_OBJ_RNUM (obj)++;
		} END_ITER(purged_iter);
		
		/*. Renumber zone table .*/
		for (zone = 0; zone <= top_of_zone_table; zone++)
			for (cmd_no = 0; ZCMD.command != 'S'; cmd_no++)
				switch (ZCMD.command) {
					case 'P':
						if(ZCMD.arg3 >= robj_num)	ZCMD.arg3++;
						/*. No break here - drop into next case .*/
					case 'O':
					case 'G':
					case 'E':
						if(ZCMD.arg1 >= robj_num)	ZCMD.arg1++;
						break;
					case 'R':
						if(ZCMD.arg2 >= robj_num) ZCMD.arg2++;
						break;
				}

		/*. Renumber shop produce .*/
		for(shop = 0; shop < top_shop; shop++)
			for(i = 0; SHOP_PRODUCT(shop, i) != -1; i++)
				if (SHOP_PRODUCT(shop, i) >= robj_num)
					SHOP_PRODUCT(shop, i)++;

		/*. Renumber produce in shops being edited .*/
		START_ITER(desc_iter, dsc, DescriptorData *, descriptor_list) {
			if(STATE(dsc) == CON_SEDIT)
				for(i = 0; S_PRODUCT(OLC_SHOP(dsc), i) != -1; i++)
					if (S_PRODUCT(OLC_SHOP(dsc), i) >= robj_num)
						S_PRODUCT(OLC_SHOP(dsc), i)++;
		} END_ITER(desc_iter);
// Now save the board...
		if (GET_OBJ_TYPE(OLC_OBJ(d)) == ITEM_BOARD)
			Board_save_board(GET_OBJ_VNUM(OLC_OBJ(d)));

	}
  
	olc_add_to_save_list(zone_table[OLC_ZNUM(d)].number, OLC_SAVE_OBJ);
}
/*------------------------------------------------------------------------*/

void oedit_save_to_disk(int zone_num) {
	int counter, counter2, realcounter;
	FILE *fp;
	ObjData *obj;
	ExtraDesc *ex_desc;
	char	*bitList,
			*bitList2;
	char *buf, *buf2;
	char *fname = get_buffer(MAX_STRING_LENGTH);
	
	sprintf(fname, OBJ_PREFIX "%d.new", zone_table[zone_num].number);

	if (!(fp = fopen(fname, "w+"))) {
		mudlogf(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: Cannot open objects file %s!", fname);
//		send_to_char("Cannot open the object file for writing!\r\n", d->character);
		release_buffer(fname);
		return;
	}
	
	buf = get_buffer(MAX_STRING_LENGTH);
	buf2 = get_buffer(MAX_STRING_LENGTH);
	bitList = get_buffer(32);
	bitList2 = get_buffer(32);
	
	/* start running through all objects in this zone */
	for (counter = zone_table[zone_num].number * 100;
				counter <= zone_table[zone_num].top;
				counter++) {
		/* write object to disk */
		if ((realcounter = real_object(counter)) >= 0) {
			obj = (ObjData *)obj_index[realcounter].proto;

			if (OBJ_FLAGGED(obj, ITEM_DELETED))
				continue;
			
			if (SSData(obj->actionDesc)) {
				strcpy(buf, SSData(obj->actionDesc));
				strip_string(buf);
			} else
				*buf = '\0';
			
			if (GET_OBJ_EXTRA(obj))		sprintbits(GET_OBJ_EXTRA(obj), bitList);
			else						strcpy(bitList, "0");
			if (GET_OBJ_WEAR(obj))		sprintbits(GET_OBJ_WEAR(obj), bitList2);
			else						strcpy(bitList2, "0");
			
			fprintf(fp,	"#%d\n"
						"%s~\n"
						"%s~\n"
						"%s~\n"
						"%s~\n"
						"%d %s %s\n"
						"%d %d %d %d %d %d %d %d\n"
						"%d %d %d\n",

					GET_OBJ_VNUM(obj),
					SSData(obj->name) ? SSData(obj->name) : "undefined",
					SSData(obj->shortDesc) ? SSData(obj->shortDesc) : "undefined",
					SSData(obj->description) ? SSData(obj->description) : "undefined",
					buf,
					GET_OBJ_TYPE(obj), bitList, bitList2,
					GET_OBJ_VAL(obj, 0), GET_OBJ_VAL(obj, 1), GET_OBJ_VAL(obj, 2),
					GET_OBJ_VAL(obj, 3), GET_OBJ_VAL(obj, 4), GET_OBJ_VAL(obj, 5),
					GET_OBJ_VAL(obj, 6), GET_OBJ_VAL(obj, 7),
					GET_OBJ_WEIGHT(obj), OBJ_FLAGGED(obj, ITEM_MISSION) ? GET_OBJ_COST(obj) : 0, GET_OBJ_SPEED(obj));

			/* Do we have extra descriptions? */
			if (obj->exDesc) { /*. Yep, save them too .*/
				for (ex_desc = obj->exDesc; ex_desc; ex_desc = ex_desc->next) {
					/*. Sanity check to prevent nasty protection faults .*/
					if (!SSData(ex_desc->keyword) || !SSData(ex_desc->description)) {
						mudlog("SYSERR: OLC: oedit_save_to_disk: Corrupt ex_desc!", BRF, LVL_BUILDER, TRUE);
						continue;
					}
					strcpy(buf, SSData(ex_desc->description));
					strip_string(buf);
					fprintf(fp,	"E\n"
								"%s~\n"
								"%s~\n",
							SSData(ex_desc->keyword),
							buf);
				}
			}

			/* Do we have object flags? */
			if (obj->affects) {
				sprintbits(obj->affects, bitList);
				fprintf(fp,	"B\n"
							" %s\n",
						bitList);
			}
          
			/* Do we have affects? */
			for (counter2 = 0; counter2 < MAX_OBJ_AFFECT; counter2++)
				if (obj->affect[counter2].modifier) 
					fprintf(fp,	"A\n"
								"%d %d\n", 
							obj->affect[counter2].location,
							obj->affect[counter2].modifier);

			if (GET_OBJ_TYPE(obj) == ITEM_WEAPON && IS_GUN(obj)) {
				/*. its a gun, save that... .*/
				fprintf(fp,	"G\n"
							"%d %d %d %d %d %d\n",
						GET_GUN_DICE_NUM(obj),
						GET_GUN_DICE_SIZE(obj),
						GET_GUN_RATE(obj),
						GET_GUN_AMMO_TYPE(obj),
						GET_GUN_ATTACK_TYPE(obj),
						GET_GUN_RANGE(obj));
			}
			if (obj_index[(obj)->number].triggers) {
				fprintf(fp, "T\n");
				fprint_triglist(fp, obj_index[(obj)->number].triggers);
				fprintf(fp, "~\n");
			}
		}
	}
	/* write final line, close */
	fprintf(fp, "$~\n");
	fclose(fp);
	sprintf(buf, "%s/%d.obj", OBJ_PREFIX, zone_table[zone_num].number);
	/*
	 * We're fubar'd if we crash between the two lines below.
	 */
	remove(buf);
	rename(fname, buf);
	olc_remove_from_save_list(zone_table[zone_num].number, OLC_SAVE_OBJ);
	
	release_buffer(buf);
	release_buffer(buf2);
	release_buffer(fname);
	release_buffer(bitList);
	release_buffer(bitList2);
}

/**************************************************************************
 Menu functions 
 **************************************************************************/

/* For container flags */
void oedit_disp_container_flags_menu(DescriptorData * d) {
	char *buf = get_buffer(MAX_STRING_LENGTH);


	sprintbit(GET_OBJ_VAL(OLC_OBJ(d), 1), container_bits, buf);
	d->character->Send(
#if defined(CLEAR_SCREEN)
			"\x1B[H\x1B[J"
#endif
			"`g1`n) CLOSEABLE\r\n"
			"`g2`n) PICKPROOF\r\n"
			"`g3`n) CLOSED\r\n"
			"`g4`n) LOCKED\r\n"
			"Container flags: `c%s`n\r\n"
			"Enter flag, 0 to quit : ",
			buf);
	release_buffer(buf);
}

/* For extra descriptions */
void oedit_disp_extradesc_menu(DescriptorData * d) {
	ExtraDesc *extra_desc = OLC_DESC(d);

	d->character->Send(
#if defined(CLEAR_SCREEN)
			"\x1B[H\x1B[J"
#endif
			"-- Extra desc menu\r\n"
			"`g1`n) Keyword: `y%s\r\n"
			"`g2`n) Description:\r\n`y%s\r\n"
			"`g3`n) Goto next description: %s\r\n"
			"`g0`n) Quit\r\n"
			"Enter choice : ",
			SSData(extra_desc->keyword) ? SSData(extra_desc->keyword) : "<NONE>",
			SSData(extra_desc->description) ? SSData(extra_desc->description) : "<NONE>",
			(extra_desc->next ? "Set." : "<Not set>"));
	OLC_MODE(d) = OEDIT_EXTRADESC_MENU;
}

/* For guns */
void oedit_disp_gun_menu(DescriptorData * d) {
	ObjData *obj;
	
	obj = OLC_OBJ(d);

	d->character->Send(
#if defined(CLEAR_SCREEN)
			"\x1B[H\x1B[J"
#endif
			"-- Gun menu\r\n"
			"`g1`n) Damage       : `c%dd%d\r\n"
			"`g2`n) Rate of Fire : `c%d\r\n"
			"`g3`n) Range        : `c%d\r\n"
			"`g4`n) Ammo Type    : `y%s\r\n"
			"`g5`n) Attack Type  : `y%s\r\n"
			"`g6`n) Purge Gun Status\r\n"
			"\r\n"
			"`g0`n) Quit Gun Setup\r\n"
			"Enter choice : ",
			GET_GUN_DICE_NUM(obj), GET_GUN_DICE_SIZE(obj),
			GET_GUN_RATE(obj),
			GET_GUN_RANGE(obj),
			ammo_types[(int)GET_GUN_AMMO_TYPE(obj)],
			attack_hit_text[GET_GUN_ATTACK_TYPE(obj)+TYPE_SHOOT-TYPE_HIT].singular);

	OLC_MODE(d) = OEDIT_GUN_MENU;
}

/* Ask for *which* apply to edit */
void oedit_disp_prompt_apply_menu(DescriptorData * d) {
	int counter;
	char *buf = get_buffer(256);
	
#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	for (counter = 0; counter < MAX_OBJ_AFFECT; counter++) {
		if (OLC_OBJ(d)->affect[counter].modifier) {
			sprinttype(OLC_OBJ(d)->affect[counter].location, apply_types, buf);
			d->character->Send(" `g%d`n) `y%+d `nto `y%s\r\n",
					counter + 1, OLC_OBJ(d)->affect[counter].modifier, buf);
		} else {
			d->character->Send(" `g%d`n) `cNone.\r\n", counter + 1);
		}
	}
	release_buffer(buf);
	
	send_to_char("\r\n`nEnter affection to modify (0 to quit): ", d->character);
	
	OLC_MODE(d) = OEDIT_PROMPT_APPLY;
}

/*. Ask for liquid type .*/
void oedit_liquid_type(DescriptorData * d) {
	int counter, columns = 0;
	
#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	for (counter = 0; counter < NUM_LIQ_TYPES; counter++) {
		d->character->Send(" `g%2d`n) `y%-20.20s %s", 
				counter, drinks[counter], !(++columns % 2) ? "\r\n" : "");
	}
	send_to_char("\r\n`nEnter drink type: ", d->character);
	
	OLC_MODE(d) = OEDIT_VALUE_3;
}

/* The actual apply to set */
void oedit_disp_apply_menu(DescriptorData * d) {
	int counter, columns = 0;
	
#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	for (counter = 0; counter < NUM_APPLIES; counter++) {
		d->character->Send("`g%2d`n) `c%-20.20s %s", counter, apply_types[counter],
					!(++columns % 2) ? "\r\n" : "");
	}
	send_to_char("\r\n`nEnter apply type (0 is no apply): ", d->character);
	
	OLC_MODE(d) = OEDIT_APPLY;
}


/* weapon type */
void oedit_disp_weapon_menu(DescriptorData * d) {
	int counter, columns = 0;
	
#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	for (counter = 0; counter < NUM_ATTACK_TYPES; counter++) {
		d->character->Send("`g%2d`n) `c%-20.20s %s", counter, attack_hit_text[counter].singular,
				!(++columns % 2) ? "\r\n" : "");
	}
	send_to_char("\r\n`nEnter weapon type: ", d->character);
}

void oedit_disp_shoot_menu(DescriptorData * d) {
	int counter, columns = 0;
	
#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	for (counter = 0; counter < NUM_SHOOT_TYPES; counter++) {
		d->character->Send("`g%2d`n) `c%-20.20s %s", counter, attack_hit_text[counter+TYPE_SHOOT-TYPE_HIT].singular,
				!(++columns % 2) ? "\r\n" : "");
	}
	send_to_char("\r\n`nEnter shoot type: ", d->character);
	
	OLC_MODE(d) = OEDIT_GUN_ATTACK_TYPE;
}


void oedit_disp_ammo_menu(DescriptorData * d) {
	int counter, columns = 0;
	char *buf = get_buffer(256);
	
#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	for (counter = 0; counter < NUM_AMMO_TYPES; counter++) {
		sprintf(buf, "`g%2d`n) `c%-20.20s %s", counter, ammo_types[counter],
				!(++columns % 2) ? "\r\n" : "");
		send_to_char(buf, d->character);
	}
	release_buffer(buf);
	send_to_char("\r\n`nEnter ammo type: ", d->character);
}

/* object value 1 */
void oedit_disp_val1_menu(DescriptorData * d) {
	OLC_MODE(d) = OEDIT_VALUE_1;
	switch (GET_OBJ_TYPE(OLC_OBJ(d))) {
		case ITEM_VEHICLE:	send_to_char("Vehicle entryway room number : ", d->character);		break;
		case ITEM_V_CONTROLS:
		case ITEM_V_WINDOW:
		case ITEM_V_WEAPON:
		case ITEM_V_HATCH:	send_to_char("Vehicle VNUM : ", d->character);						break;
		case ITEM_BED:
		case ITEM_CHAIR:	send_to_char("Capacity of sittable object : ", d->character);		break;
		case ITEM_LIGHT:	oedit_disp_val3_menu(d);	/* values 0-1 are unused.. go to 2 */	break;
		case ITEM_BOW:		send_to_char("Range (1 - 3) = ", d->character);						break;
		case ITEM_ARROW:
		case ITEM_THROW:
		case ITEM_BOOMERANG:oedit_disp_val2_menu(d);											break;
		case ITEM_MISSILE:	oedit_disp_ammo_menu(d);											break;
		case ITEM_WEAPON:	send_to_char("Modifier to Hitroll : ", d->character);				break;
		case ITEM_ARMOR:	send_to_char("Apply to AC : ", d->character);						break;
		case ITEM_CONTAINER:send_to_char("Max weight to contain : ", d->character);				break;
		case ITEM_DRINKCON:
		case ITEM_FOUNTAIN:	send_to_char("Max drink units : ", d->character);					break;
		case ITEM_FOOD:		send_to_char("Hours to fill stomach : ", d->character);				break;
		case ITEM_GRENADE:	send_to_char("Ticks to countdown to explosion : ", d->character);	break;
		case ITEM_BOARD:	send_to_char("Enter the minimum level to read this board: ",d->character);break;
		default:			oedit_disp_menu(d);													break;
	}
}

/* object value 2 */
void oedit_disp_val2_menu(DescriptorData * d) {
	OLC_MODE(d) = OEDIT_VALUE_2;
	switch (GET_OBJ_TYPE(OLC_OBJ(d))) {
		case ITEM_VEHICLE:	send_to_char(
				"Add the proper flag numbers together for final value.\r\n"
				" 1) Ground (rough terrain)\r\n"
				" 2) Air (flight)\r\n"
				" 4) Space (upper atmosphere)\r\n"
				" 8) Deep space (interstellar space)\r\n"
				"16) Water Surface\r\n"
				"32) Underwater\r\n"
				"Vehicle travel value: ", d->character);										break;
		case ITEM_ARROW:
		case ITEM_THROW:
		case ITEM_BOOMERANG:
		case ITEM_GRENADE:
		case ITEM_WEAPON:	send_to_char("Number of damage dice : ", d->character);				break;
		case ITEM_FOOD:		oedit_disp_val4_menu(d);	/* values 2-3 are unused, go to 4. */	break;
		case ITEM_CONTAINER:oedit_disp_container_flags_menu(d);									break;
			/* ^^^ these are flags, needs a bit of special handling ^^^*/
		case ITEM_DRINKCON:
		case ITEM_FOUNTAIN:	send_to_char("Initial drink units : ", d->character);				break;
		case ITEM_MISSILE:	send_to_char("Ammo provided = ", d->character);						break;
		case ITEM_BOARD:	send_to_char("Minimum level to write: ",d->character);				break;
		default:			oedit_disp_menu(d);													break;
	}
}


/* object value 3 */
void oedit_disp_val3_menu(DescriptorData * d) {
	OLC_MODE(d) = OEDIT_VALUE_3;
	switch (GET_OBJ_TYPE(OLC_OBJ(d))) {
		case ITEM_VEHICLE:	send_to_char("Lowest vehicle room number: ", d->character);
							break;
		case ITEM_LIGHT:	send_to_char("Number of hours (0 = burnt, -1 is infinite) : ", d->character);
							break;
		case ITEM_ARROW:
		case ITEM_THROW:
		case ITEM_BOOMERANG:
		case ITEM_GRENADE:
		case ITEM_WEAPON:	send_to_char("Size of damage dice : ", d->character);				break;
		case ITEM_CONTAINER:send_to_char("Vnum of key to open container (-1 for no key) : ", d->character);
							break;
		case ITEM_DRINKCON:
		case ITEM_FOUNTAIN:	oedit_liquid_type(d);												break;
		case ITEM_BOARD:	send_to_char("Minimum level to remove messages: ",d->character);	break;
		default:			oedit_disp_menu(d);													break;
	}
}


/* object value 4 */
void oedit_disp_val4_menu(DescriptorData * d) {
	OLC_MODE(d) = OEDIT_VALUE_4;
	switch (GET_OBJ_TYPE(OLC_OBJ(d))) {
		case ITEM_VEHICLE:	send_to_char("Highest vehicle room number: ", d->character);
							break;
		case ITEM_BOOMERANG:oedit_disp_val6_menu(d);											break;
		case ITEM_WEAPON:	oedit_disp_weapon_menu(d);											break;
		case ITEM_DRINKCON:
		case ITEM_FOUNTAIN:
		case ITEM_FOOD:		send_to_char("Poisoned (0 = not poison) : ", d->character);			break;
//		case ITEM_BOARD:	Board_save_board(GET_OBJ_VNUM(OLC_OBJ(d)));
		default:			oedit_disp_menu(d);													break;
	}
}


/* object value 5 */
void oedit_disp_val5_menu(DescriptorData * d) {
	OLC_MODE(d) = OEDIT_VALUE_5;
	switch (GET_OBJ_TYPE(OLC_OBJ(d))) {
		case ITEM_VEHICLE:	send_to_char(
				"Note: Vehicles can hold vehicles of 1/2 their size\r\n"
				"      However, there is currently no maximum.\r\n"
				"Guide: (size is roughly cubic meters or storage space)\r\n"
				"      APCs: 10        Dropships: 20\r\n"
				"      Small spaceships: 60\r\n"
				"      \"Capital\" ships: 100\r\n"
				"\r\n"
				"Vehicle size: ", d->character);
		default:			oedit_disp_menu(d);													break;
	}
}


/* object value 6 */
void oedit_disp_val6_menu(DescriptorData * d) {
	OLC_MODE(d) = OEDIT_VALUE_6;
	switch (GET_OBJ_TYPE(OLC_OBJ(d))) {
		case ITEM_BOOMERANG:send_to_char("Range: ", d->character);								break;
		default:			oedit_disp_menu(d);													break;
	}
}

/* object value 7 */
void oedit_disp_val7_menu(DescriptorData * d) {
	OLC_MODE(d) = OEDIT_VALUE_7;
	switch (GET_OBJ_TYPE(OLC_OBJ(d))) {
		default:			oedit_disp_menu(d);													break;
	}
}

/* object value 8 */
void oedit_disp_val8_menu(DescriptorData * d) {
	OLC_MODE(d) = OEDIT_VALUE_8;
	switch (GET_OBJ_TYPE(OLC_OBJ(d))) {
		default:			oedit_disp_menu(d);													break;
	}
}

/* object type */
void oedit_disp_type_menu(DescriptorData * d) {
	int counter, columns = 0;
	
#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	for (counter = 0; counter < NUM_ITEM_TYPES; counter++) {
		d->character->Send("`g%2d`n) `c%-20.20s %s", counter, item_types[counter],
				!(++columns % 2) ? "\r\n" : "");
	}
	send_to_char("\r\n`nEnter object type: ", d->character);
}

/* object extra flags */
void oedit_disp_extra_menu(DescriptorData * d) {
	int counter, columns = 0;
	char *buf1 = get_buffer(256);
	
#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	for (counter = 0; counter < NUM_ITEM_FLAGS; counter++) {
		d->character->Send("`g%2d`n) `c%-20.20s %s", counter + 1, extra_bits[counter],
				!(++columns % 2) ? "\r\n" : "");
	}
	sprintbit(GET_OBJ_EXTRA(OLC_OBJ(d)), extra_bits, buf1);
	d->character->Send("\r\n`nObject flags: `c%s\r\n"
				"`nEnter object extra flag (0 to quit): ", buf1);
	release_buffer(buf1);
}

/* object wear flags */
void oedit_disp_wear_menu(DescriptorData * d) {
	int counter, columns = 0;
	char *buf1 = get_buffer(256);
	
#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	for (counter = 0; counter < NUM_ITEM_WEARS; counter++) {
		d->character->Send("`g%2d`n) `c%-20.20s %s", counter + 1, wear_bits[counter],
				!(++columns % 2) ? "\r\n" : "");
	}
	sprintbit(GET_OBJ_WEAR(OLC_OBJ(d)), wear_bits, buf1);
	d->character->Send("\r\n`nWear flags: `c%s\r\n"
				"`nEnter wear flag, 0 to quit: ", buf1);
	release_buffer(buf1);
}


/* display main menu */
void oedit_disp_menu(DescriptorData * d) {
	extern char *ammo_types[];
	ObjData *obj;
	char *buf1 = get_buffer(256);
	char *buf2 = get_buffer(256);
	struct int_list *i;
	
	obj = OLC_OBJ(d);

	/*. Build buffers for first part of menu .*/
	sprinttype(GET_OBJ_TYPE(obj), item_types, buf1);
	sprintbit(GET_OBJ_EXTRA(obj), extra_bits, buf2);

	/*. Build first hallf of menu .*/
	d->character->Send(
#if defined(CLEAR_SCREEN)
				"\x1B[H\x1B[J"
#endif
				"-- Item number : [`c%d`n]\r\n"
				"`G1`n) Namelist : `y%s\r\n"
				"`G2`n) S-Desc   : `y%s\r\n"
				"`G3`n) L-Desc   :-\r\n`y%s\r\n"
				"`G4`n) A-Desc   :-\r\n`y%s\r\n"
				"`G5`n) Type        : `c%s\r\n"
				"`G6`n) Extra flags : `c%s\r\n",
		OLC_NUM(d), SSData(obj->name), SSData(obj->shortDesc), SSData(obj->description),
		SSData(obj->actionDesc) ?  SSData(obj->actionDesc) : "<not set>\r\n",
		buf1, buf2);
	release_buffer(buf2);
	/*. Build second half of menu .*/
	sprintbit(GET_OBJ_WEAR(obj), wear_bits, buf1);
	d->character->Send(
				"`G7`n) Wear flags  : `c%s\r\n"
				"`G8`n) Weight      : `c%d\r\n"
				"`G9`n) Cost        : `c%d\r\n",
			buf1, GET_OBJ_WEIGHT(obj), GET_OBJ_COST(obj));
 
	if (GET_OBJ_TYPE(obj) == ITEM_WEAPON) {
		d->character->Send("`GA`n) Speed       : `c%d\r\n", GET_OBJ_SPEED(obj));
	}
 
	sprintbit(obj->affects, affected_bits, buf1);
	d->character->Send(
				"`GB`n) Timer       : `c%d\r\n"
				"`GC`n) Affects     : `c%s\r\n"
				"`GD`n) Values      : `c%d %d %d %d %d %d %d %d\r\n"
				"`GE`g) Applies menu\r\n"
				"`GF`g) Extra descriptions menu\r\n",
			GET_OBJ_TIMER(obj),
			buf1,
			GET_OBJ_VAL(obj, 0), 
			GET_OBJ_VAL(obj, 1), 
			GET_OBJ_VAL(obj, 2),
			GET_OBJ_VAL(obj, 3),
			GET_OBJ_VAL(obj, 4),
			GET_OBJ_VAL(obj, 5),
			GET_OBJ_VAL(obj, 6),
			GET_OBJ_VAL(obj, 7));
  	
	if (IS_GUN(obj)) {
		d->character->Send("`GG`n) Gun setup   : %d`gd`n%d `gx`n %d `g@`n %d `g:`y %s\r\n"
					"   `nAmmo        : `y%s\r\n",
				GET_GUN_DICE_NUM(obj), GET_GUN_DICE_SIZE(obj),
				GET_GUN_RATE(obj), GET_GUN_RANGE(obj),
				attack_hit_text[GET_GUN_ATTACK_TYPE(obj)+TYPE_SHOOT-TYPE_HIT].singular,
				ammo_types[(int)GET_GUN_AMMO_TYPE(obj)]);
	}
	buf1[0] = '\0';
	for (i = OLC_INTLIST(d); i; i = i->next) {
		sprintf(buf1 + strlen(buf1), "%d ", i->i);
	}
	d->character->Send("`GS`n) Scripts     : `c%s\r\n"
				 "`GQ`n) Quit\r\n"
				 "Enter choice : ",
			*buf1 ? buf1 : "None");

  	release_buffer(buf1);
  	
	OLC_MODE(d) = OEDIT_MAIN_MENU;
}


/*-------------------------------------------------------------------*/
/*. Display aff-flags menu .*/

void oedit_disp_aff_menu(DescriptorData *d) {
	int counter, columns = 0;
	char	*buf = get_buffer(256);
			
#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	for (counter = 0; counter < NUM_AFF_FLAGS; counter++) {
		d->character->Send("`g%2d`n) %-20.20s  %s", counter + 1, affected_bits[counter], !(++columns % 2) ? "\r\n" : "");
	}
	sprintbit(OLC_OBJ(d)->affects, affected_bits, buf);
	d->character->Send("\r\nAffect flags : `c%s`n\r\n"
				"Enter object affect flag (0 to quit) : ",
			buf);
	release_buffer(buf);
}

/*-------------------------------------------------------------------*/
/*. Display scripts attached .*/

void oedit_disp_scripts_menu(DescriptorData *d) {
	int counter;
	int real_trig;
	struct int_list *i;
	
	send_to_char(	"-- Scripts :\r\n", d->character);
	for (i = OLC_INTLIST(d), counter = 1; i; i = i->next, counter++) {
		real_trig = real_trigger(i->i);
		d->character->Send("`g%2d`n) [`c%5d`n] %s\r\n", counter, i->i, (real_trig != -1) ?
				(SSData(((TrigData *)trig_index[real_trig].proto)->name) ? SSData(((TrigData *)trig_index[real_trig].proto)->name) : "(Unnamed)") :
				"(Uncreated)");
	}
	if (!OLC_INTLIST(d))
		send_to_char("No scripts attached.\r\n", d->character);
	send_to_char(	"`GA`n) Add Script\r\n"
					"`GR`n) Remove Script\r\n"
					"`GQ`n) Exit Menu\r\n"
					"Enter choice:  ", d->character);
	OLC_MODE(d) = OEDIT_TRIGGERS;
}


/***************************************************************************
 main loop (of sorts).. basically interpreter throws all input to here
 ***************************************************************************/


void oedit_parse(DescriptorData * d, char *arg) {
	int number, max_val, min_val, i;
	struct int_list *intlist, *temp;
	
	switch (OLC_MODE(d)) {
		case OEDIT_CONFIRM_SAVESTRING:
			switch (*arg) {
				case 'y':
				case 'Y':
					send_to_char("Saving object to memory.\r\n", d->character);
					oedit_save_internally(d);
					mudlogf(NRM, LVL_BUILDER, TRUE,  "OLC: %s edits obj %d", d->character->RealName(), OLC_NUM(d));
					cleanup_olc(d, CLEANUP_STRUCTS);
					return;
				case 'n':
				case 'N':
					/*. Cleanup all .*/
					cleanup_olc(d, CLEANUP_ALL);
					return;
				default:
					send_to_char("Invalid choice!\r\n", d->character);
					send_to_char("Do you wish to save this object internally?\r\n", d->character);
					return;
			}
			return;
		case OEDIT_MAIN_MENU:
			/* throw us out to whichever edit mode based on user input */
			switch (*arg) {
				case 'q':
				case 'Q':
					if (OLC_VAL(d)) { /*. Something has been modified .*/
						send_to_char("Do you wish to save this object internally? : ", d->character);
						OLC_MODE(d) = OEDIT_CONFIRM_SAVESTRING;
					} else 
						cleanup_olc(d, CLEANUP_ALL);
					return;
				case '1':
					send_to_char("Enter namelist : ", d->character);
					OLC_MODE(d) = OEDIT_EDIT_NAMELIST;
					break;
				case '2':
					send_to_char("Enter short desc : ", d->character);
					OLC_MODE(d) = OEDIT_SHORTDESC;
					break;
				case '3':
					send_to_char("Enter long desc :-\r\n| ", d->character);
					OLC_MODE(d) = OEDIT_LONGDESC;
					break;
				case '4':
					OLC_MODE(d) = OEDIT_ACTDESC;
					SEND_TO_Q("Enter action description: (/s saves /h for help)\r\n\r\n", d);
					d->backstr = NULL;
					if (OLC_OBJ(d)->actionDesc) {
						if (SSData(OLC_OBJ(d)->actionDesc)) {
							SEND_TO_Q(SSData(OLC_OBJ(d)->actionDesc), d);
							d->backstr = str_dup(SSData(OLC_OBJ(d)->actionDesc));
						}
					} else {
						OLC_OBJ(d)->actionDesc = SSCreate("");
					}
					d->str = &OLC_OBJ(d)->actionDesc->str;
					d->max_str = MAX_MESSAGE_LENGTH;
					d->mail_to = 0;
					OLC_VAL(d) = 1;
					break;
				case '5':
					oedit_disp_type_menu(d);
					OLC_MODE(d) = OEDIT_TYPE;
					break;
				case '6':
					oedit_disp_extra_menu(d);
					OLC_MODE(d) = OEDIT_EXTRAS;
					break;
				case '7':
					oedit_disp_wear_menu(d);
					OLC_MODE(d) = OEDIT_WEAR;
					break;
				case '8':
					send_to_char("Enter weight : ", d->character);
					OLC_MODE(d) = OEDIT_WEIGHT;
					break;
				case '9':
					send_to_char("Enter cost : ", d->character);
					OLC_MODE(d) = OEDIT_COST;
					break;
				case 'a':
				case 'A':
					send_to_char("Enter hit delay : ", d->character);
					OLC_MODE(d) = OEDIT_SPEED;
					break;
				case 'b':
				case 'B':
					send_to_char("Enter timer : ", d->character);
					OLC_MODE(d) = OEDIT_TIMER;
					break;
				case 'c':
				case 'C':
					oedit_disp_aff_menu(d);
					OLC_MODE(d) = OEDIT_AFFECTS;
			/*. Object level flags in my mud...
			send_to_char("Enter level : ", d->character);
			OLC_MODE(d) = OEDIT_LEVEL;
			.*/
					break;
				case 'd':
				case 'D':
					/*. Clear any old values .*/
					GET_OBJ_VAL(OLC_OBJ(d), 0) = 0;
					GET_OBJ_VAL(OLC_OBJ(d), 1) = 0;
					GET_OBJ_VAL(OLC_OBJ(d), 2) = 0;
					GET_OBJ_VAL(OLC_OBJ(d), 3) = 0;
					GET_OBJ_VAL(OLC_OBJ(d), 4) = 0;
					GET_OBJ_VAL(OLC_OBJ(d), 5) = 0;
					GET_OBJ_VAL(OLC_OBJ(d), 6) = 0;
					GET_OBJ_VAL(OLC_OBJ(d), 7) = 0;
					oedit_disp_val1_menu(d);
					break;
				case 'e':
				case 'E':
					oedit_disp_prompt_apply_menu(d);
					break;
				case 'f':
				case 'F':
					/* if extra desc doesn't exist . */
					if (!OLC_OBJ(d)->exDesc) {
						OLC_OBJ(d)->exDesc = new ExtraDesc;
						OLC_OBJ(d)->exDesc->next = NULL;
					}
					OLC_DESC(d) = OLC_OBJ(d)->exDesc;
					oedit_disp_extradesc_menu(d);
					break;
				case 'g':
				case 'G':
					if (GET_OBJ_TYPE(OLC_OBJ(d)) == ITEM_WEAPON) {
						if (!OLC_OBJ(d)->gun)
							OLC_OBJ(d)->gun = new GunData();
						oedit_disp_gun_menu(d);
					}
					break;
				case 's':
				case 'S':
					oedit_disp_scripts_menu(d);
					break;
				default:
					oedit_disp_menu(d);
					break;
			}
			return;			/* end of OEDIT_MAIN_MENU */

		case OEDIT_EDIT_NAMELIST:
			SSFree(OLC_OBJ(d)->name);
			OLC_OBJ(d)->name = SSCreate(arg);
			break;

		case OEDIT_SHORTDESC:
			SSFree(OLC_OBJ(d)->shortDesc);
			OLC_OBJ(d)->shortDesc = SSCreate(arg);
			break;

		case OEDIT_LONGDESC:
			SSFree(OLC_OBJ(d)->description);
			OLC_OBJ(d)->description = SSCreate(arg);
			break;

		case OEDIT_TYPE:
			number = atoi(arg);
			if ((number < 1) || (number >= NUM_ITEM_TYPES)) {
				send_to_char("Invalid choice, try again : ", d->character);
				return;
			} else
				GET_OBJ_TYPE(OLC_OBJ(d)) = number;
			break;

		case OEDIT_EXTRAS:
			number = atoi(arg);
			if ((number < 0) || (number > NUM_ITEM_FLAGS)) {
				oedit_disp_extra_menu(d);
				return;
			} else {
				/* if 0, quit */
				if (number == 0)
					break;
				else { /* if already set.. remove */
					if (OBJ_FLAGGED(OLC_OBJ(d), 1 << (number - 1)))  
						REMOVE_BIT(GET_OBJ_EXTRA(OLC_OBJ(d)), 1 << (number - 1));
					else /* set */
						SET_BIT(GET_OBJ_EXTRA(OLC_OBJ(d)), 1 << (number - 1));
					oedit_disp_extra_menu(d);
					return;
				}
			}

		case OEDIT_AFFECTS:
			number = atoi(arg);
			if ((number < 0) || (number > NUM_AFF_FLAGS)) {
				oedit_disp_aff_menu(d);
				return;
			} else {
				/* if 0, quit */
				if (number == 0)
					break;
				else {
					/* if already set.. remove */
					if (IS_SET(OLC_OBJ(d)->affects, 1 << (number - 1)))  
						REMOVE_BIT(OLC_OBJ(d)->affects, 1 << (number - 1));
					else /* set */
						SET_BIT(OLC_OBJ(d)->affects, 1 << (number - 1));
					oedit_disp_aff_menu(d);
					return;
				}
			}

		case OEDIT_WEAR:
			number = atoi(arg);
			if ((number < 0) || (number > NUM_ITEM_WEARS)) {
				send_to_char("That's not a valid choice!\r\n", d->character);
				oedit_disp_wear_menu(d);
				return;
			} else { /* if 0, quit */
				if (number == 0)
					break;
				else { /* if already set.. remove */
					if (OBJWEAR_FLAGGED(OLC_OBJ(d), 1 << (number - 1)))
						REMOVE_BIT(GET_OBJ_WEAR(OLC_OBJ(d)), 1 << (number - 1));
					else
						SET_BIT(GET_OBJ_WEAR(OLC_OBJ(d)), 1 << (number - 1));
					oedit_disp_wear_menu(d);
					return;
				}
			}

		case OEDIT_WEIGHT:
			number = atoi(arg);
			GET_OBJ_WEIGHT(OLC_OBJ(d)) = number;
			break;

		case OEDIT_COST:
			number = atoi(arg);
			GET_OBJ_COST(OLC_OBJ(d)) = number;
			break;

		case OEDIT_SPEED:
			number = atoi(arg);
			GET_OBJ_SPEED(OLC_OBJ(d)) = number;
			break;

		case OEDIT_TIMER:
			number = atoi(arg);
			GET_OBJ_TIMER(OLC_OBJ(d)) = number;
			break;

		case OEDIT_LEVEL:
			/*. Object level flags on my mud...
			number = atoi(arg);
			GET_OBJ_LEVEL(OLC_OBJ(d)) = number;
			.*/
			break;

		case OEDIT_GUN_MENU:
			switch (*arg) {
				case '0':
					break;
				case '1':
					send_to_char("Number of damage dice: ", d->character);
					OLC_MODE(d) = OEDIT_GUN_DICE_NUM;
					return;
				case '2':
					send_to_char("Rate of Fire: ", d->character);
					OLC_MODE(d) = OEDIT_GUN_RATE;
					return;
				case '3':
					send_to_char("Range of weapon: ", d->character);
					OLC_MODE(d) = OEDIT_GUN_RANGE;
					return;
				case '4':
					oedit_disp_ammo_menu(d);
					OLC_MODE(d) = OEDIT_GUN_AMMO_TYPE;
					return;
				case '5':
					oedit_disp_shoot_menu(d);
					return;
				case '6':
					send_to_char("Gun status of object purged.", d->character);
					GET_GUN_DICE_NUM(OLC_OBJ(d)) = 0;
					GET_GUN_DICE_SIZE(OLC_OBJ(d)) = 0;
					GET_GUN_RANGE(OLC_OBJ(d)) = 0;
					GET_GUN_RATE(OLC_OBJ(d)) = 0;
					GET_GUN_AMMO_TYPE(OLC_OBJ(d)) = 0;
					GET_GUN_ATTACK_TYPE(OLC_OBJ(d)) = 0;
					oedit_disp_gun_menu(d);
					return;
				default:
					oedit_disp_gun_menu(d);
					return;
			}
			break;

		case OEDIT_GUN_DICE_NUM:
			number = atoi(arg);
			GET_GUN_DICE_NUM(OLC_OBJ(d)) = number;
			send_to_char("Size of damage dice: ", d->character);
			OLC_MODE(d) = OEDIT_GUN_DICE_SIZE;
			return;

		case OEDIT_GUN_DICE_SIZE:
			number = atoi(arg);
			GET_GUN_DICE_SIZE(OLC_OBJ(d)) = number;
			oedit_disp_gun_menu(d);
			return;

		case OEDIT_GUN_AMMO_TYPE:
			number = atoi(arg);
			GET_GUN_AMMO_TYPE(OLC_OBJ(d)) = MAX(0, MIN(number, NUM_AMMO_TYPES - 1));
			oedit_disp_gun_menu(d);
			return;

		case OEDIT_GUN_ATTACK_TYPE:
			number = atoi(arg);
			GET_GUN_ATTACK_TYPE(OLC_OBJ(d)) = MAX(0, MIN(number, NUM_SHOOT_TYPES - 1));
			oedit_disp_gun_menu(d);
			return;

		case OEDIT_GUN_RANGE:
			number = atoi(arg);
			GET_GUN_RANGE(OLC_OBJ(d)) = MAX(0, MIN(number, 3));
			oedit_disp_gun_menu(d);
			return;

		case OEDIT_GUN_RATE:
			number = atoi(arg);
			GET_GUN_RATE(OLC_OBJ(d)) = MAX(1, MIN(number, 10));
			oedit_disp_gun_menu(d);
			return;

		case OEDIT_VALUE_1:
			/* lucky, I don't need to check any of these for outofrange values */
			/*. Hmm, I'm not so sure - Rv .*/
			number = atoi(arg);
			switch (GET_OBJ_TYPE(OLC_OBJ(d))) {
				case ITEM_CHAIR:
				case ITEM_BED:
					min_val = 1;
					max_val = 5;
					break;
				default:
					min_val = -32000;
					max_val = 32000;
			}
			GET_OBJ_VAL(OLC_OBJ(d), 0) = MAX(min_val, MIN(number, max_val));
			/* proceed to menu 2 */
			OLC_VAL(d) = 1; /*. Has changed flag .*/
			oedit_disp_val2_menu(d);
			return;
		case OEDIT_VALUE_2:
			/* here, I do need to check for outofrange values */
			number = atoi(arg);
			switch (GET_OBJ_TYPE(OLC_OBJ(d))) {
				case ITEM_CONTAINER:
					/* needs some special handling since we are dealing with flag values
					 * here */
					number = atoi(arg);
					if (number < 0 || number > 4)
						oedit_disp_container_flags_menu(d);
					else { /* if 0, quit */
						if (number != 0) {
							number = 1 << (number - 1);
							if (OBJVAL_FLAGGED(OLC_OBJ(d), number))
								REMOVE_BIT(GET_OBJ_VAL(OLC_OBJ(d), 1), number);
							else
								SET_BIT(GET_OBJ_VAL(OLC_OBJ(d), 1), number);
							oedit_disp_val2_menu(d);
					} else
						oedit_disp_val3_menu(d);
					}
					break;
				default:
					switch (GET_OBJ_TYPE(OLC_OBJ(d))) {
						case ITEM_VEHICLE:
							min_val = 0;
							max_val = 31;
							break;
						default:
							min_val = -32000;
							max_val = 32000;
					}
					GET_OBJ_VAL(OLC_OBJ(d), 1) = MAX(min_val, MIN(number, max_val));
					oedit_disp_val3_menu(d);
			}
			return;

		case OEDIT_VALUE_3:
			number = atoi(arg);
			/*. Quick'n'easy error checking .*/
			switch (GET_OBJ_TYPE(OLC_OBJ(d))) {
				case ITEM_WEAPON:
					min_val = 1;
					max_val = 50;
					break;
				case ITEM_DRINKCON:
				case ITEM_FOUNTAIN:
					min_val = 0;
					max_val = NUM_LIQ_TYPES -1;
					break;
				case ITEM_VEHICLE:
					min_val = 0;
					max_val = world[top_of_world-1].number;
					break;
				default:
					min_val = -32000;
					max_val = 32000;
			}
			GET_OBJ_VAL(OLC_OBJ(d), 2) = MAX(min_val, MIN(number, max_val));
			oedit_disp_val4_menu(d);
			return;

		case OEDIT_VALUE_4:
			number = atoi(arg);
			switch (GET_OBJ_TYPE(OLC_OBJ(d))) {
				case ITEM_WEAPON:
					min_val = 0;
					max_val = NUM_ATTACK_TYPES - 1;
					break;
				case ITEM_VEHICLE:
					min_val = GET_OBJ_VAL(OLC_OBJ(d), 2) + 1;
					max_val = world[top_of_world].number;
					break;
				default:
					min_val = -32000;
					max_val = 32000;
					break;
			}
			GET_OBJ_VAL(OLC_OBJ(d), 3) = MAX(min_val, MIN(number, max_val));
			oedit_disp_val5_menu(d);
			return;

		case OEDIT_VALUE_5:
			number = atoi(arg);
			switch (GET_OBJ_TYPE(OLC_OBJ(d))) {
				case ITEM_VEHICLE:
					min_val = 1;
					max_val = 32000;
				default:
					min_val = -32000;
					max_val = 32000;
				break;
			}
			GET_OBJ_VAL(OLC_OBJ(d), 4) = MAX(min_val, MIN(number, max_val));
			oedit_disp_val6_menu(d);
			return;

		case OEDIT_VALUE_6:
			number = atoi(arg);
			switch (GET_OBJ_TYPE(OLC_OBJ(d))) {
				case ITEM_BOOMERANG:
					min_val = 0;
					max_val = 3;
					break;
				default:
					min_val = -32000;
					max_val = 32000;
					break;
			}
			GET_OBJ_VAL(OLC_OBJ(d), 5) = MAX(min_val, MIN(number, max_val));
			oedit_disp_val7_menu(d);
			return;

		case OEDIT_VALUE_7:
			number = atoi(arg);
			switch (GET_OBJ_TYPE(OLC_OBJ(d))) {
				default:
					min_val = -32000;
					max_val = 32000;
					break;
			}
			GET_OBJ_VAL(OLC_OBJ(d), 6) = MAX(min_val, MIN(number, max_val));
			oedit_disp_val8_menu(d);
			return;

		case OEDIT_VALUE_8:
			number = atoi(arg);
			switch (GET_OBJ_TYPE(OLC_OBJ(d))) {
				default:
					min_val = -32000;
					max_val = 32000;
					break;
			}
			GET_OBJ_VAL(OLC_OBJ(d), 7) = MAX(min_val, MIN(number, max_val));
			oedit_disp_menu(d);
			break;

		case OEDIT_PROMPT_APPLY:
			number = atoi(arg);
			if (number == 0)
				break;
			else if (number < 0 || number > MAX_OBJ_AFFECT)
				oedit_disp_prompt_apply_menu(d);
			else {
				OLC_VAL(d) = number - 1;
				OLC_MODE(d) = OEDIT_APPLY;
				oedit_disp_apply_menu(d);
			}
			return;

		case OEDIT_APPLY:
			number = atoi(arg);
			if (number == 0) {
				OLC_OBJ(d)->affect[OLC_VAL(d)].location = 0;
				OLC_OBJ(d)->affect[OLC_VAL(d)].modifier = 0;
				oedit_disp_prompt_apply_menu(d);
			} else if (number < 0 || number >= NUM_APPLIES)
				oedit_disp_apply_menu(d);
			else {
				OLC_OBJ(d)->affect[OLC_VAL(d)].location = number;
				send_to_char("Enter value: ", d->character);
				OLC_MODE(d) = OEDIT_APPLYMOD;
			}
			return;

		case OEDIT_APPLYMOD:
			number = atoi(arg);
			OLC_OBJ(d)->affect[OLC_VAL(d)].modifier = number;
			oedit_disp_prompt_apply_menu(d);
			return;

		case OEDIT_EXTRADESC_KEY:
			SSFree(OLC_DESC(d)->keyword);
			if (*arg)	OLC_DESC(d)->keyword = SSCreate(arg);
			else		OLC_DESC(d)->keyword = NULL;
			oedit_disp_extradesc_menu(d);
			return;

		case OEDIT_EXTRADESC_MENU:
			number = atoi(arg);
			switch (number) {
				case 0:	/* if something got left out */
					if (!SSData(OLC_DESC(d)->keyword) || !SSData(OLC_DESC(d)->description)) {
						ExtraDesc *temp;
						REMOVE_FROM_LIST(OLC_DESC(d), OLC_OBJ(d)->exDesc, next);
						delete OLC_DESC(d);
						OLC_DESC(d) = NULL;
					}
/*
					if (!SSData(OLC_DESC(d)->keyword) || !SSData(OLC_DESC(d)->description)) {
						ExtraDesc **tmp_desc;

//						SSFree(OLC_DESC(d)->keyword);
//						SSFree(OLC_DESC(d)->description);

						for(tmp_desc = &(OLC_OBJ(d)->exDesc); *tmp_desc; tmp_desc = &((*tmp_desc)->next))
							if(*tmp_desc == OLC_DESC(d)) {
								*tmp_desc = NULL;
								break;
							}
						delete OLC_DESC(d);
						OLC_DESC(d) = NULL;
					}
*/
					break;

				case 1:
					OLC_MODE(d) = OEDIT_EXTRADESC_KEY;
					send_to_char("Enter keywords, separated by spaces :-\r\n| ", d->character);
					return;

				case 2:
					OLC_MODE(d) = OEDIT_EXTRADESC_DESCRIPTION;
					SEND_TO_Q("Enter the extra description: (/s saves /h for help)\r\n\r\n", d);
					d->backstr = NULL;
					if (OLC_DESC(d)->description) {
						if (SSData(OLC_DESC(d)->description)) {
							SEND_TO_Q(SSData(OLC_DESC(d)->description), d);
							d->backstr = str_dup(SSData(OLC_DESC(d)->description));
						}
					} else {
						OLC_DESC(d)->description = SSCreate("");
					}
					d->str = &OLC_DESC(d)->description->str;
					d->max_str = MAX_MESSAGE_LENGTH;
					d->mail_to = 0;
					OLC_VAL(d) = 1;
					return;

				case 3:
					/*. Only go to the next descr if this one is finished .*/
					if (SSData(OLC_DESC(d)->keyword) && SSData(OLC_DESC(d)->description)) {
						ExtraDesc *new_extra;

						if (OLC_DESC(d)->next)
							OLC_DESC(d) = OLC_DESC(d)->next;
						else { /* make new extra, attach at end */
							new_extra = new ExtraDesc;
							OLC_DESC(d)->next = new_extra;
							OLC_DESC(d) = OLC_DESC(d)->next;
						}
					}
				/*. No break - drop into default case .*/
				default:
					oedit_disp_extradesc_menu(d);
					return;
			}
			break;
		case OEDIT_TRIGGERS:
			switch (*arg) {
				case 'a':
				case 'A':
					send_to_char("Enter the VNUM of script to attach: ", d->character);
					OLC_MODE(d) = OEDIT_TRIGGER_ADD;
					return;
				case 'r':
				case 'R':
					send_to_char("Enter script in list to detach: ", d->character);
					OLC_MODE(d) = OEDIT_TRIGGER_PURGE;
					return;
				case 'q':
				case 'Q':
					oedit_disp_menu(d);
					return;
				default:
					oedit_disp_scripts_menu(d);
					return;
			}
			break;
		case OEDIT_TRIGGER_ADD:
			if (is_number(arg)) {
				number = atoi(arg);
				CREATE(intlist, struct int_list, 1);
				intlist->i = number;
				intlist->next = OLC_INTLIST(d);
				OLC_INTLIST(d) = intlist;
				send_to_char("Script attached.\r\n", d->character);
			} else
				send_to_char("Numbers only, please.\r\n", d->character);
			OLC_VAL(d) = 1;
			oedit_disp_scripts_menu(d);
			return;
		case OEDIT_TRIGGER_PURGE:
			if (is_number(arg)) {
				number = atoi(arg);
				for (i = 1, intlist = OLC_INTLIST(d); intlist && i < number; intlist = intlist->next)
					i++;
				
				if (intlist) {
					REMOVE_FROM_LIST(intlist, OLC_INTLIST(d), next);
					FREE(intlist);
					OLC_VAL(d) = 1;
					send_to_char("Script detached.\r\n", d->character);
				} else
					send_to_char("Invalid number.\r\n", d->character);
			} else
				send_to_char("Numbers only, please.\r\n", d->character);
			oedit_disp_scripts_menu(d);
			return;
		default:
			mudlogf(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: Reached default case in oedit_parse()!  Case: (%d)", OLC_MODE(d));
			send_to_char("Oops...\r\n", d->character);
			break;
	}

	/*. If we get here, we have changed something .*/
	OLC_VAL(d) = 1; /*. Has changed flag .*/
	oedit_disp_menu(d);
}
