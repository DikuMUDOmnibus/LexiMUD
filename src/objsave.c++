/* ************************************************************************
*   File: objsave.c                                     Part of CircleMUD *
*  Usage: loading/saving player objects for rent and crash-save           *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */




#include "structs.h"
#include "buffer.h"
#include "comm.h"
#include "scripts.h"
#include "handler.h"
#include "db.h"
#include "interpreter.h"
#include "utils.h"
#include "files.h"


/* Extern data */
extern struct player_index_element *player_table;


/* local functions */
void Crash_restore_weight(ObjData * obj);
void Crash_extract_objs(ObjData * obj);
int Crash_is_unrentable(ObjData * obj);
void Crash_extract_norents(ObjData * obj);
void Crash_rentsave(CharData * ch);
void Crash_save(ObjData * obj, FILE * fp, int locate);
void auto_equip(CharData *ch, ObjData *obj, int locate);
void Crash_extract_norents_from_equipped(CharData * ch);
void Crash_crashsave(CharData * ch);


void Crash_listrent(CharData * ch, char *name) {
	FILE *fl;
	char *fname = get_buffer(MAX_INPUT_LENGTH), *buf, *line;
	ObjData *obj;
	SInt32	obj_vnum, location;
	bool	read_file = true;

	if (!get_filename(name, fname)) {
		release_buffer(fname);
		return;
	}
	if (!(fl = fopen(fname, "rb"))) {
		ch->Send("%s has no rent file.\r\n", name);
		release_buffer(fname);
		return;
	}
	
	buf = get_buffer(MAX_STRING_LENGTH);
	line = get_buffer(MAX_INPUT_LENGTH);
	
	sprintf(buf, "%s\r\n", fname);
	
	get_line(fl, line);
	while (get_line(fl, line) && read_file) {
		switch (*line) {
			case '#':
				if (sscanf(line, "#%d %d", &obj_vnum, &location) < 1) {
					log("Player object file %s is damaged: no vnum", fname);
					read_file = false;
				} else {
					if (!(obj = read_object(obj_vnum, VIRTUAL))) {
						obj = new ObjData();
						obj->name = SSCreate("Empty.");
						obj->shortDesc = SSCreate("Empty.");
						obj->description = SSCreate("Empty.");
					}
					if (obj->load(fl, fname)) {
						sprintf(buf + strlen(buf), " [%5d] <%2d> %-20s\r\n", GET_OBJ_VNUM(obj),
								location, SSData(obj->shortDesc));
					} else {
						log("Player object file %s is damaged: failed to load obj #%d", fname, obj_vnum);
						read_file = false;
					}
					obj->extract();
				}
				break;
			case '$':
				read_file = false;
				break;
			default:
				log("Player object file %s is damaged: bad line %s", fname, line);
				read_file = false;
		}
	}
	page_string(ch->desc, buf, true);
	release_buffer(fname);
	release_buffer(buf);
	release_buffer(line);
	fclose(fl);
}


 /* so this is gonna be the auto equip (hopefully) */
void auto_equip(CharData *ch, ObjData *obj, int locate) {
	int j;
 
	if (locate > 0) { /* was worn */
		switch (j = locate-1) {
			case WEAR_FINGER_R:
			case WEAR_FINGER_L:	if (!CAN_WEAR(obj,ITEM_WEAR_FINGER))	locate = 0;		break;
			case WEAR_NECK	:	if (!CAN_WEAR(obj,ITEM_WEAR_NECK))		locate = 0;		break;
			case WEAR_BODY:		if (!CAN_WEAR(obj,ITEM_WEAR_BODY))		locate = 0;		break;
			case WEAR_HEAD:		if (!CAN_WEAR(obj,ITEM_WEAR_HEAD))		locate = 0;		break;
			case WEAR_LEGS:		if (!CAN_WEAR(obj,ITEM_WEAR_LEGS))		locate = 0;		break;
			case WEAR_FEET:		if (!CAN_WEAR(obj,ITEM_WEAR_FEET))		locate = 0;		break;
			case WEAR_HANDS:	if (!CAN_WEAR(obj,ITEM_WEAR_HANDS))		locate = 0;		break;
			case WEAR_ARMS:		if (!CAN_WEAR(obj,ITEM_WEAR_ARMS))		locate = 0;		break;
			case WEAR_ABOUT:	if (!CAN_WEAR(obj,ITEM_WEAR_ABOUT))		locate = 0;		break;
			case WEAR_WAIST:	if (!CAN_WEAR(obj,ITEM_WEAR_WAIST))		locate = 0;		break;
			case WEAR_WRIST_R:
			case WEAR_WRIST_L:	if (!CAN_WEAR(obj,ITEM_WEAR_WRIST))		locate = 0;		break;
			case WEAR_EYES:		if (!CAN_WEAR(obj,ITEM_WEAR_EYES))		locate = 0;		break;
			case WEAR_HAND_R:	
			case WEAR_HAND_L:	if (!CAN_WEAR(obj, ITEM_WEAR_TAKE | ITEM_WEAR_SHIELD | ITEM_WEAR_WIELD))
									locate = 0;
								break;
			default:			locate = 0;
	}
	if (locate > 0)
		if (!GET_EQ(ch,j)) {
			/* check ch's race to prevent $M from being zapped through auto-equip */
			if (!ch->CanUse(obj))
				locate = 0;
			else
				equip_char(ch, obj, j);
		} else  /* oops - saved player with double equipment[j]? */
			locate = 0;
	}
	if (locate <= 0)
		obj->to_char(ch);
}
 
 
 
#define MAX_BAG_ROW 5
/* should be enough - who would carry a bag in a bag in a bag in a bag in a bag in a bag ?!? */


int Crash_load(CharData * ch) {
/* return values:
	0 - successful load, keep char in rent room.
	1 - load failure or load of crash items -- put char in temple.
*/
	FILE *fl;
	char *fname = get_buffer(MAX_STRING_LENGTH);
	ObjData *obj, *obj1;
	LList<ObjData *>	cont_row[MAX_BAG_ROW];
	SInt32	locate, j, renttime, obj_vnum;
	bool	read_file = true;
//	bool	result = true;
	char *line;

	if (!get_filename(SSData(ch->general.name), fname)) {
		release_buffer(fname);
		return 1;
	}
	if (!(fl = fopen(fname, "r+b"))) {
		if (errno != ENOENT) {	/* if it fails, NOT because of no file */
			log("SYSERR: READING OBJECT FILE %s (5)", fname);
			send_to_char("\r\n********************* NOTICE *********************\r\n"
							"There was a problem loading your objects from disk.\r\n"
							"Contact Staff for assistance.\r\n", ch);
		}
		mudlogf(NRM, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE,  "%s entering game with no equipment.", ch->RealName());
		release_buffer(fname);
		return 1;
	}
	
	line = get_buffer(MAX_INPUT_LENGTH);
	if (get_line(fl, line))
		renttime = atoi(line);

	mudlogf( NRM, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE,  "%s entering game.", ch->RealName());

//	for (j = 0;j < MAX_BAG_ROW;j++)
//		cont_row[j] = NULL; /* empty all cont lists (you never know ...) */

	while (get_line(fl, line) && read_file) {
		switch (*line) {
			case '#':
				locate = 0;
				if (sscanf(line, "#%d %d", &obj_vnum, &locate) < 1) {
					log("Player object file %s is damaged: no vnum", fname);
					read_file = false;
				} else {
					if ((obj_vnum == -1) || (real_object(obj_vnum) == -1)) {
						obj = new ObjData();
						obj->name = SSCreate("Empty.");
						obj->shortDesc = SSCreate("Empty.");
						obj->description = SSCreate("Empty.\r\n");
					} else
						obj = read_object(obj_vnum, VIRTUAL);
					
					read_file = obj->load(fl, fname);
					auto_equip(ch, obj, locate);

					if (locate > 0) { /* item equipped */
						for (j = MAX_BAG_ROW-1;j > 0;j--)
							if (cont_row[j].Count()) { /* no container -> back to ch's inventory */
//								for (;cont_row[j];cont_row[j] = obj1) {
//									obj1 = cont_row[j]->next_content;
//									cont_row[j]->to_char(ch);
//								}
//								cont_row[j] = NULL;
								while((obj1 = cont_row[j].Top())) {
									cont_row[j].Remove(obj1);
									obj1->to_char(ch);
								}
							}
						if (cont_row[0].Count()) {		//	content list existing
							if (GET_OBJ_TYPE(obj) == ITEM_CONTAINER) {
								/* rem item ; fill ; equip again */
								obj = unequip_char(ch, locate-1);
//								for (;cont_row[0];cont_row[0] = obj1) {
//									obj1 = cont_row[0]->next_content;
//									cont_row[0]->to_obj(obj);
//								}
								while((obj1 = cont_row[0].Top())) {
									cont_row[0].Remove(obj1);
									obj1->to_obj(obj);
								}
								equip_char(ch, obj, locate-1);
							} else { /* object isn't container -> empty content list */
//								for (;cont_row[0];cont_row[0] = obj1) {
//									obj1 = cont_row[0]->next_content;
//									cont_row[0]->to_char(ch);
//								}
//								cont_row[0] = NULL;
								while((obj1 = cont_row[0].Top())) {
									cont_row[0].Remove(obj1);
									obj1->to_char(ch);
								}
							}
						}
					} else { /* locate <= 0 */
						for (j = MAX_BAG_ROW-1;j > -locate;j--)
							if (cont_row[j].Count()) { /* no container -> back to ch's inventory */
//								for (;cont_row[j];cont_row[j] = obj1) {
//									obj1 = cont_row[j]->next_content;
//									cont_row[j]->to_char(ch);
//								}
//								cont_row[j] = NULL;
								while((obj1 = cont_row[j].Top())) {
									cont_row[j].Remove(obj1);
									obj1->to_char(ch);
								}
							}

						if (j == -locate && cont_row[j].Count()) { /* content list existing */
							if (GET_OBJ_TYPE(obj) == ITEM_CONTAINER) {
								/* take item ; fill ; give to char again */
								obj->from_char();
//								for (;cont_row[j];cont_row[j] = obj1) {
//									obj1 = cont_row[j]->next_content;
//									cont_row[j]->to_obj(obj);
//								}
								while ((obj1 = cont_row[j].Top())) {
									cont_row[j].Remove(obj1);
									obj1->to_obj(obj);
								}
								obj->to_char(ch); /* add to inv first ... */
							} else { /* object isn't container -> empty content list */
//								for (;cont_row[j];cont_row[j] = obj1) {
//									obj1 = cont_row[j]->next_content;
//									cont_row[j]->to_char(ch);
//								}
//								cont_row[j] = NULL;
								while ((obj1 = cont_row[j].Top())) {
									cont_row[j].Remove(obj1);
									obj1->to_char(ch);
								}
							}
						}

						if (locate < 0 && locate >= -MAX_BAG_ROW) {
							/*	let obj be part of content list but put it at the list's end
							 *	thus having the items
							 *	in the same order as before renting
							 */
							obj->from_char();
//							if ((obj1 = cont_row[-locate-1].Top())) {
//								while (obj1->next_content)
//									obj1 = obj1->next_content;
//								obj1->next_content = obj;
//							} else
//								cont_row[-locate-1] = obj;
							cont_row[-locate-1].Prepend(obj);
						}
					}
				}
				break;
			case '$':
				read_file = false;
				break;
			default:
				log("Player object file %s is damaged: bad line %s", fname, line);
				read_file = false;
				fclose(fl);
				return 1;
		}
	}
	fclose(fl);

	release_buffer(fname);
	release_buffer(line);
	
	return 0;
}


void Crash_save(ObjData * obj, FILE * fp, int locate) {
	ObjData *tmp;

	START_ITER(iter, tmp, ObjData *, obj->contains) {
		Crash_save(tmp, fp, MIN(0,locate)-1);
	} END_ITER(iter);
	obj->save(fp, locate);
	for (tmp = obj->in_obj; tmp; tmp = tmp->in_obj)
		GET_OBJ_WEIGHT(tmp) -= GET_OBJ_WEIGHT(obj);
}


void Crash_restore_weight(ObjData * obj) {
	ObjData *tmp;
	START_ITER(iter, tmp, ObjData *, obj->contains) {
		Crash_restore_weight(tmp);
	} END_ITER(iter);
	if (obj->in_obj)
		GET_OBJ_WEIGHT(obj->in_obj) += GET_OBJ_WEIGHT(obj);
}



void Crash_extract_objs(ObjData * obj) {
	ObjData *tmp;
	START_ITER(iter, tmp, ObjData *, obj->contains) {
		Crash_extract_objs(tmp);
	} END_ITER(iter);
	obj->extract();
}


int Crash_is_unrentable(ObjData * obj) {
	if (!obj)							return 0;
	if (OBJ_FLAGGED(obj, ITEM_NORENT))	return 1;
	return 0;
}


void Crash_extract_norents(ObjData * obj) {
	ObjData *tmp;
	START_ITER(iter, tmp, ObjData *, obj->contains) {
		Crash_extract_norents(tmp);
	} END_ITER(iter);
	if (Crash_is_unrentable(obj))
		obj->extract();
}


/* get norent items from eq to inventory and extract norents out of worn containers */
void Crash_extract_norents_from_equipped(CharData * ch) {
	int j;

	for (j = 0;j < NUM_WEARS;j++) {
		if (GET_EQ(ch,j)) {
			if (Crash_is_unrentable(GET_EQ(ch, j)))
				unequip_char(ch,j)->to_char(ch);
			else
				Crash_extract_norents(GET_EQ(ch,j));
		}
	}
}


void Crash_crashsave(CharData * ch) {
	char *buf;
	int j;
	FILE *fp;
	ObjData *tmp;

	if (IS_NPC(ch))
		return;
	buf = get_buffer(MAX_INPUT_LENGTH);
	if (!get_filename(SSData(ch->general.name), buf)) {
		release_buffer(buf);
		return;
	}
	if (!(fp = fopen(buf, "w"))) {
		release_buffer(buf);
		return;
	}
	
	release_buffer(buf);
	
	fprintf(fp, "%ld\n", time(0));

	for (j = 0; j < NUM_WEARS; j++)
		if (GET_EQ(ch,j)) {
			Crash_save(GET_EQ(ch,j), fp, j+1);
			Crash_restore_weight(GET_EQ(ch, j));
		}
	
	LListIterator<ObjData *>	iter(ch->carrying);
	
	while ((tmp = iter.Next())) {
		Crash_save(tmp, fp, 0);
		Crash_restore_weight(tmp);
	}
	
//	START_ITER(save_iter, tmp, ObjData *, ch->carrying) {
//		Crash_save(tmp, fp, 0);
//	} END_ITER(save_iter);
//	START_ITER(weight_iter, tmp, ObjData *, ch->carrying) {
//		Crash_restore_weight(tmp);
//	} END_ITER(weight_iter);

	fclose(fp);
	REMOVE_BIT(PLR_FLAGS(ch), PLR_CRASH);
}


void Crash_rentsave(CharData * ch) {
	char *buf;
	int j;
	FILE *fp;
	ObjData *tmp;

	if (IS_NPC(ch))
		return;
    
	buf = get_buffer(MAX_INPUT_LENGTH);
	if (!get_filename(SSData(ch->general.name), buf)) {
		release_buffer(buf);
		return;
	}
	if (!(fp = fopen(buf, "wb"))) {
		release_buffer(buf);
		return;
	}
	release_buffer(buf);
  
	Crash_extract_norents_from_equipped(ch);

	LListIterator<ObjData *>	iter(ch->carrying);

	while ((tmp = iter.Next())) {
		Crash_extract_norents(tmp);
	}

	fprintf(fp, "%ld\n", time(0));

	for (j = 0; j < NUM_WEARS; j++)
		if (GET_EQ(ch,j)) {
			Crash_save(GET_EQ(ch,j), fp, j+1);
			Crash_restore_weight(GET_EQ(ch,j));
			Crash_extract_objs(GET_EQ(ch,j));
		}
	
	iter.Reset();
	while((tmp = iter.Next())) {
		Crash_save(tmp, fp, 0);
		Crash_extract_objs(tmp);
	}
	
	fclose(fp);
	
//	START_ITER(ext_iter, tmp, ObjData *, ch->carrying) {
//	} END_ITER(ext_iter);
}


void Crash_save_all(void) {
	DescriptorData *d;
	START_ITER(iter, d, DescriptorData *, descriptor_list) {
		if ((STATE(d) == CON_PLAYING) && !IS_NPC(d->character)) {
			if (PLR_FLAGGED(d->character, PLR_CRASH)) {
				Crash_crashsave(d->character);
				d->character->save(NOWHERE);
				REMOVE_BIT(PLR_FLAGS(d->character), PLR_CRASH);
			}
		}
	} END_ITER(iter);
}
