/************************************************************************
 * OasisOLC - medit.c                                              v1.5 *
 * Copyright 1996 Harvey Gilpin.                                        *
 ************************************************************************/


#include "characters.h"
#include "descriptors.h"
#include "comm.h"
#include "spells.h"
#include "utils.h"
#include "scripts.h"
#include "db.h"
#include "shop.h"
#include "olc.h"
#include "interpreter.h"
#include "clans.h"
#include "buffer.h"
#include "opinion.h"


/*
 * External variable declarations.
 */
extern struct zone_data *zone_table;
extern int top_of_zone_table;
extern PlayerData dummy_player;
extern struct attack_hit_type attack_hit_text[];
extern char *action_bits[];
extern char *affected_bits[];
extern char *position_types[];
extern char *genders[];
extern int top_shop;
extern struct shop_data *shop_index;
extern char *pc_race_types[];
extern char *mobprog_types[];
extern char *race_abbrevs[];
extern UInt8 stat_min[NUM_RACES + 1][5];
extern UInt8 stat_max[NUM_RACES + 1][5];
extern char *race_types[];

void free_intlist(struct int_list *intlist);


/*
 * Handy internal macros.
 */
#define GET_NDD(themob)			((themob)->mob->damage.number)
#define GET_SDD(themob)			((themob)->mob->damage.size)
#define GET_ALIAS(themob)		((themob)->general.name)
#define GET_SDESC(themob)		((themob)->general.shortDesc)
#define GET_LDESC(themob)		((themob)->general.longDesc)
#define GET_DDESC(themob)		((themob)->general.description)
#define GET_ATTACK(themob)		((themob)->mob->attack_type)
#define S_KEEPER(shop)			((shop)->keeper)
#define GET_MPROG(themob)		(mob_index[(themob)->nr].mobprogs)
#define GET_MPROG_TYPE(themob)	(mob_index[(themob)->nr].progtypes)

/*
 * Function prototypes.
 */
void medit_parse(DescriptorData * d, char *arg);
void medit_disp_menu(DescriptorData * d);
void medit_setup_new(DescriptorData *d);
void medit_setup_existing(DescriptorData *d, RNum rmob_num);
void medit_save_internally(DescriptorData *d);
void medit_save_to_disk(int zone_num);
void init_mobile(CharData *mob);
void copy_mobile(CharData *tmob, CharData *fmob);
void medit_disp_positions(DescriptorData *d);
void medit_disp_mob_flags(DescriptorData *d);
void medit_disp_aff_flags(DescriptorData *d);
void medit_disp_attack_types(DescriptorData *d);
void medit_disp_sex(DescriptorData *d);
void medit_disp_race(DescriptorData *d);
void medit_disp_mprog(DescriptorData *d);
void medit_change_mprog(DescriptorData *d);
char *medit_get_mprog_type(struct mob_prog_data *mprog);
void medit_disp_mprog_types(DescriptorData *d);
void medit_disp_scripts_menu(DescriptorData *d);
void medit_disp_attributes_menu(DescriptorData *d);
void medit_disp_opinion(Opinion *op, SInt32 num, DescriptorData *d);
void medit_disp_opinions_menu(DescriptorData *d);
void medit_disp_opinions_genders(DescriptorData *d);
void medit_disp_opinions_races(DescriptorData *d);


/*-------------------------------------------------------------------*\
  utility functions 
\*-------------------------------------------------------------------*/


void medit_setup_new(DescriptorData *d) {
	CharData *mob;

	/*
	 * Allocate a scratch mobile structure.  
	 */
	mob = new CharData;
	
	init_mobile(mob);
	
	GET_MOB_RNUM(mob) = -1;

	/*
	 * Set up some default strings.
	 */
	GET_ALIAS(mob) = SSCreate("mob unfinished");
	GET_SDESC(mob) = SSCreate("the unfinished mob");
	GET_LDESC(mob) = SSCreate("An unfinished mob stands here.\r\n");
	GET_DDESC(mob) = SSCreate("It looks unfinished.\r\n");
	OLC_MPROGL(d) = NULL;
	OLC_MPROG(d) = NULL;

	OLC_MOB(d) = mob;
	OLC_VAL(d) = 0;		/* Has changed flag. (It hasn't so far, we just made it.) */
  
	medit_disp_menu(d);
}

/*-------------------------------------------------------------------*/

void medit_setup_existing(DescriptorData *d, RNum rmob_num) {
	CharData *mob;
	MPROG_DATA *temp;
	MPROG_DATA *head;
	struct int_list *i, *inthead;
	/*
	 * Allocate a scratch mobile structure. 
	 */
	mob = new CharData;
	
	copy_mobile(mob, (CharData *)mob_index[rmob_num].proto);

	/*
	 * I think there needs to be a brace from the if statement to the #endif
	 * according to the way the original patch was indented.  If this crashes,
	 * try it with the braces and report to greerga@van.ml.org on if that works.
	 */
	if (GET_MPROG(mob))
		CREATE(OLC_MPROGL(d), MPROG_DATA, 1);
	head = OLC_MPROGL(d);
	for (temp = GET_MPROG(mob); temp;temp = temp->next) {
		OLC_MPROGL(d)->type = temp->type;
		OLC_MPROGL(d)->arglist = str_dup(temp->arglist);
		OLC_MPROGL(d)->comlist = str_dup(temp->comlist);
		if (temp->next) {
			CREATE(OLC_MPROGL(d)->next, MPROG_DATA, 1);
			OLC_MPROGL(d) = OLC_MPROGL(d)->next;
		}
	}
	OLC_MPROGL(d) = head;
	OLC_MPROG(d) = OLC_MPROGL(d);
	
	if (mob_index[rmob_num].triggers)
		CREATE(OLC_INTLIST(d), struct int_list, 1);
	inthead = OLC_INTLIST(d);
	for (i = mob_index[rmob_num].triggers; i; i = i->next) {
		OLC_INTLIST(d)->i = i->i;
		if (i->next) {
			CREATE(OLC_INTLIST(d)->next, struct int_list, 1);
			OLC_INTLIST(d) = OLC_INTLIST(d)->next;
		}
	}
	OLC_INTLIST(d) = inthead;

	OLC_MOB(d) = mob;
	medit_disp_menu(d);
}

/*-------------------------------------------------------------------*/
/*
 * Copy one mob struct to another.
 */
 
void copy_mobile(CharData *tmob, CharData *fmob) {
//	MobData *temp;

	//	Free up all strings.
	SSFree(GET_ALIAS(tmob));
	SSFree(GET_SDESC(tmob));
	SSFree(GET_LDESC(tmob));
	SSFree(GET_DDESC(tmob));

	//	Copy mob over.
	
//	temp = tmob->mob;
	
	*tmob = *fmob;
	tmob->mob = new MobData(fmob->mob);
	
//	if (temp)
//		delete temp;
	 
	//	Re-share strings.
	GET_ALIAS(tmob) = SSCreate(SSData(GET_ALIAS(fmob)) ? SSData(GET_ALIAS(fmob)) : "undefined");
	GET_SDESC(tmob) = SSCreate(SSData(GET_SDESC(fmob)) ? SSData(GET_SDESC(fmob)) : "undefined");
	GET_LDESC(tmob) = SSCreate(SSData(GET_LDESC(fmob)) ? SSData(GET_LDESC(fmob)) : "undefined");
	GET_DDESC(tmob) = SSCreate(SSData(GET_DDESC(fmob)) ? SSData(GET_DDESC(fmob)) : "undefined");
}


/*
 * Ideally, this function should be in db.c, but I'll put it here for
 * portability.
 */
#define GET_MANA(ch) ch->points.mana
#define GET_MAX_MANA(ch) ch->points.max_mana

void init_mobile(CharData *mob) {
	mob->player = &dummy_player;
	mob->mob = new MobData;
	
	GET_HIT(mob) = GET_MANA(mob) = 1;
	GET_MAX_MANA(mob) = GET_MAX_MOVE(mob) = 100;
	GET_NDD(mob) = GET_SDD(mob) = 1;
	GET_WEIGHT(mob) = 200;
	GET_HEIGHT(mob) = 72;

	mob->real_abils.str = mob->real_abils.intel = mob->real_abils.per = 60;
	mob->real_abils.agi = mob->real_abils.con = 60;
	mob->aff_abils = mob->real_abils;

	SET_BIT(MOB_FLAGS(mob), MOB_ISNPC);
}

/*-------------------------------------------------------------------*/

#define ZCMD zone_table[zone].cmd[cmd_no]

/*
 * Save new/edited mob to memory.
 */
void medit_save_internally(DescriptorData *d) {
	int rmob_num, found = 0, new_mob_num = 0, zone, cmd_no, shop;
	IndexData *new_index;
	CharData *live_mob, *proto;
	DescriptorData *dsc;
	LListIterator<CharData *>	iter(character_list);
	
	REMOVE_BIT(MOB_FLAGS(OLC_MOB(d)), MOB_DELETED);
	
	/*
	 * Mob exists? Just update it.
	 */
	if ((rmob_num = real_mobile(OLC_NUM(d))) != -1) {
		proto = (CharData *)mob_index[rmob_num].proto;
		copy_mobile(proto, OLC_MOB(d));
		free_intlist(mob_index[rmob_num].triggers);
		mob_index[rmob_num].triggers = OLC_INTLIST(d);
		OLC_INTLIST(d) = NULL;
		/*
		 * Update live mobiles.
		 */

		while ((live_mob = iter.Next())) {
			if(IS_MOB(live_mob) && GET_MOB_RNUM(live_mob) == rmob_num) {
				/*
				 * Only really need to update the strings, since other stuff
				 * changing could interfere with the game.
				 */
				SSFree(GET_ALIAS(live_mob));
				SSFree(GET_SDESC(live_mob));
				SSFree(GET_LDESC(live_mob));
				SSFree(GET_DDESC(live_mob));
				GET_ALIAS(live_mob) = SSShare(GET_ALIAS(proto));
				GET_SDESC(live_mob) = SSShare(GET_SDESC(proto));
				GET_LDESC(live_mob) = SSShare(GET_LDESC(proto));
				GET_DDESC(live_mob) = SSShare(GET_DDESC(proto));
			}
		}
	} else {
		/*
		 * Mob does not exist, we have to add it.
		 */
		CREATE(new_index, IndexData, top_of_mobt + 2);

		for (rmob_num = 0; rmob_num <= top_of_mobt; rmob_num++) {
			if (!found) { /*. Is this the place?  .*/
//				if ((rmob_num > top_of_mobt) || (mob_index[rmob_num].vnum > OLC_NUM(d)))
				if (mob_index[rmob_num].vnum > OLC_NUM(d)) {
					/*
					 * Yep, stick it here.
					 */
					found = TRUE;
#if defined(DEBUG)
					log("Inserted: rmob_num: %d", rmob_num);
#endif
					new_index[rmob_num].vnum = OLC_NUM(d);
					new_index[rmob_num].number = 0;
					new_index[rmob_num].func = NULL;
					new_index[rmob_num].triggers = OLC_INTLIST(d);
					OLC_INTLIST(d) = NULL;
					new_mob_num = rmob_num;
					GET_MOB_RNUM(OLC_MOB(d)) = rmob_num;
					new_index[rmob_num].proto = proto = new CharData;
					copy_mobile(proto, OLC_MOB(d));
					/*
					 * Copy the mob that should be here on top.
					 */
					new_index[rmob_num + 1] = mob_index[rmob_num];
					proto = (CharData *)new_index[rmob_num + 1].proto;
					GET_MOB_RNUM(proto) = rmob_num + 1;
				} else { /*. Nope, copy over as normal.*/
					new_index[rmob_num] = mob_index[rmob_num];
				}
			} else { /*. We've already found it, copy the rest over .*/
				new_index[rmob_num + 1] = mob_index[rmob_num];
				proto = (CharData *)new_index[rmob_num + 1].proto;
				GET_MOB_RNUM(proto) = rmob_num + 1;
			}
		}
#if defined(DEBUG)
		log("rmob_num: %d, top_of_mobt: %d, array size: 0-%d (%d)",
				rmob_num, top_of_mobt, top_of_mobt + 1, top_of_mobt + 2);
#endif
		if (!found) { /* Still not found, must add it to the top of the table. */
#if defined(DEBUG)
			log("Append.");
#endif
			new_index[rmob_num].vnum = OLC_NUM(d);
			new_index[rmob_num].number = 0;
			new_index[rmob_num].func = NULL;
			new_index[rmob_num].triggers = OLC_INTLIST(d);
			OLC_INTLIST(d) = NULL;
			new_mob_num = rmob_num;
			GET_MOB_RNUM(OLC_MOB(d)) = rmob_num;
			new_index[rmob_num].proto = proto = new CharData;
			copy_mobile(proto, OLC_MOB(d));
		}

		/*
		 * Replace tables.
		 */
#if defined(DEBUG)
		log("Attempted free.");
#endif
		/*
		 * I have noticed that if you attempt to free a block of memory and you
		 * give FREE() a pointer other than it was originally given, the program
		 * will crash.  It's a theory for the two FREE()'s below.
		 */
		FREE(mob_index);
    	mob_index = new_index;
	    top_of_mobt++;

		/*
		 * Update live mobile rnums.
		 */
		while ((live_mob = iter.Next())) {
			if(GET_MOB_RNUM(live_mob) > new_mob_num)
				GET_MOB_RNUM(live_mob)++;
		}

		/*
		 * Update live mobile rnums.
		 */
		iter.Restart(purged_chars);
		while ((live_mob = iter.Next())) {
			if(GET_MOB_RNUM(live_mob) > new_mob_num)
				GET_MOB_RNUM(live_mob)++;
		}

		/*
		 * Update zone table.
		 */
		for (zone = 0; zone <= top_of_zone_table; zone++)
			for (cmd_no = 0; ZCMD.command != 'S'; cmd_no++) 
				if (ZCMD.command == 'M')
					if (ZCMD.arg1 >= new_mob_num)
						ZCMD.arg1++;

		/*
		 * Update shop keepers.
		 */
		for(shop = 0; shop < top_shop; shop++)
			if(SHOP_KEEPER(shop) >= new_mob_num)
				SHOP_KEEPER(shop)++;

		/*
		 * Update keepers in shops being edited and other mobs being edited.
		 */
		START_ITER(desc_iter, dsc, DescriptorData *, descriptor_list) {
			if(STATE(dsc) == CON_SEDIT) {
				if(S_KEEPER(OLC_SHOP(dsc)) >= new_mob_num)
					S_KEEPER(OLC_SHOP(dsc))++;
			} else if (STATE(dsc) == CON_MEDIT) {
				if (GET_MOB_RNUM(OLC_MOB(dsc)) >= new_mob_num)
					GET_MOB_RNUM(OLC_MOB(dsc))++;
			}
		} END_ITER(desc_iter);
	}

	GET_MPROG(OLC_MOB(d)) = OLC_MPROGL(d);
	GET_MPROG_TYPE(OLC_MOB(d)) = (OLC_MPROGL(d) ? OLC_MPROGL(d)->type : 0);
	while (OLC_MPROGL(d)) {
		GET_MPROG_TYPE(OLC_MOB(d)) |= OLC_MPROGL(d)->type;
		OLC_MPROGL(d) = OLC_MPROGL(d)->next;
	}
	OLC_MPROGL(d) = NULL;
	olc_add_to_save_list(zone_table[OLC_ZNUM(d)].number, OLC_SAVE_MOB);
}


/*
 * Save ALL mobiles for a zone to their .mob file, mobs are all 
 * saved in Extended format, regardless of whether they have any
 * extended fields.  Thanks to Samedi for ideas on this bit of code 
 */
void medit_save_to_disk(int zone_num) { 
	int i, rmob_num, zone, top;
	FILE *mob_file;
	char *fname = get_buffer(64);
	char *buf1, *buf2, *temp;
	CharData *mob;
	MPROG_DATA *mob_prog = NULL;

	zone = zone_table[zone_num].number;
	top = zone_table[zone_num].top;

	sprintf(fname, MOB_PREFIX "%i.new", zone);
	
	if(!(mob_file = fopen(fname, "w"))) {
		mudlogf(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: Cannot open mob file \"%s\"!", fname);
		release_buffer(fname);
		return;
	}
	
	buf1 = get_buffer(MAX_STRING_LENGTH);
	buf2 = get_buffer(MAX_STRING_LENGTH);
	

	//	Seach the database for mobs in this zone and save them.
	for(i = zone * 100; i <= top; i++) {
		if ((rmob_num = real_mobile(i)) != -1) {
			mob = (CharData *)mob_index[rmob_num].proto;
			
			if (MOB_FLAGGED(mob, MOB_DELETED))
				continue;
			
			if (fprintf(mob_file, "#%d\n", i) < 0) {
				mudlog("SYSERR: OLC: Cannot write mob file!", BRF, LVL_BUILDER, TRUE);
				fclose(mob_file);
				release_buffer(buf1);
				release_buffer(buf2);
				release_buffer(fname);
				return;
			}
			
			fprintf(mob_file, "Name: %s\n", SSData(GET_ALIAS(mob)) ? SSData(GET_ALIAS(mob)) : "undefined");
			fprintf(mob_file, "Shrt: %s\n", SSData(GET_SDESC(mob)) ? SSData(GET_SDESC(mob)) : "undefined");

			strcpy(buf1, SSData(GET_LDESC(mob)) ? SSData(GET_LDESC(mob)) : "undefined");
			strip_string(buf1);
			fprintf(mob_file, "Long:\n%s~\n", buf1);
			
			strcpy(buf1, SSData(GET_DDESC(mob)) ? SSData(GET_DDESC(mob)) : "undefined");
			strip_string(buf1);
			fprintf(mob_file, "Desc:\n%s~\n", buf1);

			fprintf(mob_file, "Race: %d\n", GET_RACE(mob));
			fprintf(mob_file, "Levl: %d\n", GET_LEVEL(mob));
			fprintf(mob_file, "Hitr: %d\n", GET_HITROLL(mob));
			fprintf(mob_file, "Armr: %d\n", GET_AC(mob));
			fprintf(mob_file, "HP  : %dd%d+%d\n", GET_HIT(mob), GET_MANA(mob), GET_MOVE(mob));
			fprintf(mob_file, "Dmg : %dd%d+%d\n", GET_NDD(mob), GET_SDD(mob), GET_DAMROLL(mob));
			fprintf(mob_file, "Pos : %d\n", GET_POS(mob));
			fprintf(mob_file, "DPos: %d\n", GET_DEFAULT_POS(mob));
			fprintf(mob_file, "Sex : %d\n", GET_SEX(mob));
			
			if (MOB_FLAGS(mob)) {
				sprintbits(MOB_FLAGS(mob), buf1);
				fprintf(mob_file, "Act : %s\n", buf1);
			}
			if (AFF_FLAGS(mob)) {
				sprintbits(AFF_FLAGS(mob), buf1);
				fprintf(mob_file, "Aff : %s\n", buf1);
			}
			
			/*
			 * Deal with Extra stats in case they are there.
			 */
			if (GET_ATTACK(mob))		fprintf(mob_file, "Attk: %d\n", GET_ATTACK(mob));
			if (GET_STR(mob) != 60)		fprintf(mob_file, "Str : %d\n", GET_STR(mob));
			if (GET_AGI(mob) != 60)		fprintf(mob_file, "Agi : %d\n", GET_AGI(mob));
			if (GET_INT(mob) != 60)		fprintf(mob_file, "Int : %d\n", GET_INT(mob));
			if (GET_PER(mob) != 60)		fprintf(mob_file, "Per : %d\n", GET_PER(mob));
			if (GET_CON(mob) != 60)		fprintf(mob_file, "Con : %d\n", GET_CON(mob));
			if (GET_MOB_DODGE(mob))		fprintf(mob_file, "Dodg: %d\n", GET_MOB_DODGE(mob));
			if (real_clan(GET_CLAN(mob)) != -1)		fprintf(mob_file, "Clan: %d\n", GET_CLAN(mob));

			if (mob->mob->hates) {
				sprintbits(mob->mob->hates->sex, buf1);
				sprintbits(mob->mob->hates->race, buf2);
				fprintf(mob_file, "Hate: %s %s %d\n", *buf1 ? buf1 : "0", *buf2 ? buf2 : "0",
						mob->mob->hates->vnum);
			}
			if (mob->mob->fears) {
				sprintbits(mob->mob->fears->sex, buf1);
				sprintbits(mob->mob->fears->race, buf2);
				fprintf(mob_file, "Fear: %s %s %d\n", *buf1 ? buf1 : "0", *buf2 ? buf2 : "0",
						mob->mob->fears->vnum);
			}

			if (mob_index[(mob)->nr].triggers) {
				fprintf(mob_file, "Trig:\n");
				fprint_triglist(mob_file, mob_index[(mob)->nr].triggers);
				fprintf(mob_file, "~\n");
			}

			/*
			 * Write out the MobProgs.
			 */
			mob_prog = GET_MPROG(mob); //mob_index[rmob_num].mobprogs;
			if (mob_prog)				fprintf(mob_file, "Prog:\n");
			while(mob_prog) {
				if (mob_prog->arglist && *mob_prog->arglist &&
						mob_prog->comlist && *mob_prog->comlist) {
				    strcpy(buf1, mob_prog->arglist);
				    temp = buf1;
				    strip_string(temp);
				    skip_spaces(temp);
				    strcpy(buf2, mob_prog->comlist);
				    strip_string(buf2);
					fprintf(mob_file, "%s %s~\n%s", medit_get_mprog_type(mob_prog), temp, buf2);
				}
				mob_prog=mob_prog->next;
				fprintf(mob_file, "~\n%s", (!mob_prog ? "|\n" : ""));
			}
			fprintf(mob_file, "DONE\n");
		}
	}
	release_buffer(buf1);
  
	fprintf(mob_file, "$\n");
	fclose(mob_file);

	sprintf(buf2, MOB_PREFIX "%d.mob", zone);
	
	//	We're fubar'd if we crash between the two lines below.
	remove(buf2);
	rename(fname, buf2);
	
	release_buffer(fname);
	release_buffer(buf2);
	
	olc_remove_from_save_list(zone_table[zone_num].number, OLC_SAVE_MOB);
}


/**************************************************************************
 Menu functions 
 **************************************************************************/

/*
 * Get Mob Prog Type
 */
char* medit_get_mprog_type(struct mob_prog_data *mprog) {
	switch (mprog->type) {
		case IN_FILE_PROG:	return ">in_file_prog";
		case ACT_PROG:		return ">act_prog";
		case SPEECH_PROG:	return ">speech_prog";
		case RAND_PROG:		return ">rand_prog";
		case FIGHT_PROG:	return ">fight_prog";
		case HITPRCNT_PROG:	return ">hitprcnt_prog";
		case DEATH_PROG:	return ">death_prog";
		case ENTRY_PROG:	return ">entry_prog";
		case GREET_PROG:	return ">greet_prog";
		case ALL_GREET_PROG:return ">all_greet_prog";
		case GIVE_PROG:		return ">give_prog";
		case BRIBE_PROG:	return ">bribe_prog";
		case EXIT_PROG:		return ">exit_prog";
		case ALL_EXIT_PROG:	return ">all_exit_prog";
		case DELAY_PROG:	return ">delay_prog";
		case SCRIPT_PROG:	return ">script_prog";
		case WEAR_PROG:		return ">wear_prog";
		case REMOVE_PROG:	return ">remove_prog";
		case GET_PROG:		return ">get_prog";
		case DROP_PROG:		return ">drop_prog";
		case EXAMINE_PROG:	return ">examine_prog";
		case PULL_PROG:		return ">pull_prog";
	}
	return ">ERROR_PROG";
}

/*-------------------------------------------------------------------*/

/*
 * Display Mob Progs
 */
void medit_disp_mprog(DescriptorData *d) {
	struct mob_prog_data *mprog = OLC_MPROGL(d);
	char *buf = get_buffer(256);
  
	OLC_MTOTAL(d) = 1;
#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	while (mprog) {
		sprintf (buf, "%d) %s %s\r\n",
				OLC_MTOTAL(d), medit_get_mprog_type(mprog), (mprog->arglist ? mprog->arglist : "NONE"));
		send_to_char(buf, d->character);
		OLC_MTOTAL(d)++;
		mprog=mprog->next;
	}
	sprintf(buf,"%d) Create New Mob Prog\r\n"
				"%d) Purge Mob Prog\r\n"
				"Enter number to edit [0 to exit]:  ",
			OLC_MTOTAL(d), OLC_MTOTAL(d) + 1);
	send_to_char(buf, d->character);

	OLC_MODE(d) = MEDIT_MPROG;
	release_buffer(buf);
}

/*-------------------------------------------------------------------*/

/*
 * Change Mob Progs
 */
void medit_change_mprog(DescriptorData *d) {
	char *buf = get_buffer(LARGE_BUFSIZE);
#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	sprintf(buf,"1) Type: %s\r\n"
				"2) Args: %s\r\n"
				"3) Commands:\r\n%s\r\n\r\n"
				"Enter number to edit [0 to exit]: ",
				medit_get_mprog_type(OLC_MPROG(d)),
				(OLC_MPROG(d)->arglist ? OLC_MPROG(d)->arglist : "NONE"),
				(OLC_MPROG(d)->comlist ? OLC_MPROG(d)->comlist : "NONE"));
	send_to_char(buf, d->character);
	OLC_MODE(d) = MEDIT_CHANGE_MPROG;
	release_buffer(buf);
}

/*-------------------------------------------------------------------*/

/*
 * Change the MobProg type.
 */
void medit_disp_mprog_types(DescriptorData *d) {
	int i;
	char *buf = get_buffer(256);
	
	send_to_char("\x1B[H\x1B[J", d->character);
	for (i = 0; i < NUM_PROGS-1; i++) {
		sprintf(buf, "`g%2d`n) `c%s\r\n", i, mobprog_types[i]);
		send_to_char(buf, d->character);
	}
	send_to_char("`nEnter mob prog type: ", d->character);
	OLC_MODE(d) = MEDIT_MPROG_TYPE;
	release_buffer(buf);
}

/*-------------------------------------------------------------------*/

/*
 * Display positions. (sitting, standing, etc)
 */
void medit_disp_positions(DescriptorData *d) {
	int i;
	char *buf = get_buffer(256);

#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	for (i = 0; *position_types[i] != '\n'; i++) {
		sprintf(buf, "`g%2d`n) `c%s\r\n", i, position_types[i]);
		send_to_char(buf, d->character);
	}
	release_buffer(buf);
	send_to_char("`nEnter position number: ", d->character);
}

/*-------------------------------------------------------------------*/

/*
 * Display the gender of the mobile.
 */
void medit_disp_sex(DescriptorData *d) {
	int i;
	char *buf = get_buffer(256);
	
#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	for (i = 0; i < NUM_GENDERS; i++) {
		sprintf(buf, "`g%2d`n) `c%s\r\n", i, genders[i]);
		send_to_char(buf, d->character);
	}
	send_to_char("`nEnter gender number: ", d->character);
	release_buffer(buf);
}

/*-------------------------------------------------------------------*/

/*
 * Display attack types menu.
 */
void medit_disp_attack_types(DescriptorData *d) {
	int i;
 	char *buf = get_buffer(256);

#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	for (i = 0; i < NUM_ATTACK_TYPES; i++) {
		sprintf(buf, "`g%2d`n) `c%s\r\n", i, attack_hit_text[i].singular);
		send_to_char(buf, d->character);
	}
	send_to_char("`nEnter attack type: ", d->character);
	release_buffer(buf);
}
 
/*-------------------------------------------------------------------*/

/*
 * Display races menu.
 */
void medit_disp_race(DescriptorData *d) {
	int i;
 	char *buf = get_buffer(256);
  
#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	for (i = 0; i < NUM_RACES + 1; i++) {
		sprintf(buf, "`g%2d`n) `c%s\r\n", i, pc_race_types[i]);
		send_to_char(buf, d->character);
	}
	send_to_char("`nEnter race: ", d->character);
	release_buffer(buf);
}
 

/*-------------------------------------------------------------------*/

/*
 * Display mob-flags menu.
 */
void medit_disp_mob_flags(DescriptorData *d) {
	int i, columns = 0;
	char 	*buf = get_buffer(256);
			
#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	for (i = 0; i < NUM_MOB_FLAGS; i++) {
		d->character->Send("`g%2d`n) `c%-20.20s  %s",
				i+1, action_bits[i], !(++columns % 2) ? "\r\n" : "");
	}
	sprintbit(MOB_FLAGS(OLC_MOB(d)), action_bits, buf);
	d->character->Send("\r\n"
				"`nCurrent flags : `c%s\r\n"
				"`nEnter mob flags (0 to quit): ",
				buf);
	release_buffer(buf);
}

/*-------------------------------------------------------------------*/

/*
 * Display affection flags menu.
 */
void medit_disp_aff_flags(DescriptorData *d) {
	int i, columns = 0;
	char 	*buf = get_buffer(256);
  
#if defined(CLEAR_SCREEN)
	send_to_char("\x1B[H\x1B[J", d->character);
#endif
	for (i = 0; i < NUM_AFF_FLAGS; i++) {
		d->character->Send("`g%2d`n) `c%-20.20s  %s", i+1, affected_bits[i],
				!(++columns % 2) ? "\r\n" : "");
	}
	sprintbit(AFF_FLAGS(OLC_MOB(d)), affected_bits, buf);
	d->character->Send("\r\n"
				"`nCurrent flags   : `c%s\r\n"
				"`nEnter aff flags (0 to quit): ",
				buf);
	release_buffer(buf);
}
  
/*-------------------------------------------------------------------*/

/*
 * Display main menu.
 */
void medit_disp_menu(DescriptorData * d) {
	CharData *mob;
	struct int_list *i;
	char 	*buf = get_buffer(MAX_INPUT_LENGTH),
			*buf1 = get_buffer(MAX_INPUT_LENGTH),
			*buf2 = get_buffer(MAX_INPUT_LENGTH);
	RNum	clan_rnum;
	
	mob = OLC_MOB(d);

	sprintbit(MOB_FLAGS(mob), action_bits, buf1);
	sprintbit(AFF_FLAGS(mob), affected_bits, buf2);
	for(i = OLC_INTLIST(d); i; i = i->next) {
		sprintf(buf + strlen(buf), "%d ", i->i);
	}
	
	clan_rnum = real_clan(GET_CLAN(mob));
	
	d->character->Send(
#if defined(CLEAR_SCREEN)
				"\x1B[H\x1B[J"
#endif
				"-- Mob Number:  [`c%d`n]\r\n"
				"`G1`n) Sex: `y%-7.7s`n	         `G2`n) Alias: `y%s\r\n"
				"`G3`n) S-Desc: `y%s\r\n"
				"`G4`n) L-Desc:-\r\n`y%s"
				"`G5`n) D-Desc:-\r\n`y%s"
				"`G6`n) Level:       [`c%4d`n]   `G7`n) Race:         [`c%s `n]\r\n"
				"`G8`n) Hitroll:     [`c%4d`n]   `G9`n) Damroll:      [`c%4d`n]\r\n"
				"`GA`n) NumDamDice:  [`c%4d`n]   `GB`n) SizeDamDice:  [`c%4d`n]\r\n"
				"`GC`n) Num HP Dice: [`c%4d`n]   `GD`n) Size HP Dice: [`c%4d`n]\r\n"
				"`GE`n) HP Bonus:   [`c%5d`n]   `GF`n) Armor Class:  [`c%4d`n]\r\n"
				"`GG`n) Attributes:  Str [`c%3d`n]  Int [`c%3d`n]  Per [`c%3d`n]  Agi [`c%3d`n]  Con [`c%3d`n]\r\n"
				"`GH`n) Clan      : `c%d `n(`y%s`n)\r\n"
				"`GI`n) Position  : `y%s\r\n"
				"`GJ`n) Default   : `y%s\r\n"
				"`GK`n) Attack    : `y%s\r\n"
				"`GL`n) NPC Flags : `c%s\r\n"
				"`GM`n) AFF Flags : `c%s\r\n"
				"`GO`n) Opinions Menu\r\n"
				"`GP`n) Mob Progs : `c%s\r\n"
				"`GS`n) Scripts   : `c%s\r\n"
				"`GQ`n) Quit\r\n"
				"Enter choice : ",
			OLC_NUM(d),
			genders[(int)GET_SEX(mob)],		SSData(GET_ALIAS(mob)),
			SSData(GET_SDESC(mob)),
			SSData(GET_LDESC(mob)),
			SSData(GET_DDESC(mob)),
			GET_LEVEL(mob),					race_abbrevs[(int)GET_RACE(mob)],
			GET_HITROLL(mob),				GET_DAMROLL(mob),
			GET_NDD(mob),					GET_SDD(mob),
			GET_HIT(mob),					GET_MANA(mob),
			GET_MOVE(mob),					GET_AC(mob),
			GET_STR(mob),	GET_INT(mob),	GET_PER(mob),
			GET_AGI(mob),	GET_CON(mob),
			GET_CLAN(mob),	(clan_rnum != -1 ? clan_index[clan_rnum].name : "None"),
			position_types[(int)GET_POS(mob)],
			position_types[(int)GET_DEFAULT_POS(mob)],
			attack_hit_text[GET_ATTACK(mob)].singular,
			buf1,
			buf2,
			(OLC_MPROGL(d) ? "Set." : "Not Set."),
			*buf ? buf : "None");

	OLC_MODE(d) = MEDIT_MAIN_MENU;
	release_buffer(buf);
	release_buffer(buf1);
	release_buffer(buf2);
}


void medit_disp_attributes_menu(DescriptorData *d) {
	CharData *mob;
	
	mob = OLC_MOB(d);	
	
	d->character->Send(
			"-- Attributes:\r\n"
			"`g1`n) Str [`c%3d`n]\r\n"
			"`g2`n) Int [`c%3d`n]\r\n"
			"`g3`n) Per [`c%3d`n]\r\n"
			"`g4`n) Agi [`c%3d`n]\r\n"
			"`g5`n) Con [`c%3d`n]\r\n"
			"`GQ`n) Exit Menu\r\n"
			"Enter choice:  ",
			GET_STR(mob),
			GET_INT(mob),
			GET_PER(mob),
			GET_AGI(mob),
			GET_CON(mob));
	OLC_MODE(d) = MEDIT_ATTRIBUTES;
}


//	Display scripts attached
void medit_disp_scripts_menu(DescriptorData *d) {
	int counter;
	int real_trig;
	struct int_list *i;
	
	send_to_char(	"-- Scripts :\r\n", d->character);
	for (i = OLC_INTLIST(d), counter = 1; i; i = i->next, counter++) {
		real_trig = real_trigger(i->i);
		d->character->Send("`g%2d`n) [`c%5d`n] %s\r\n", counter, i->i, (real_trig != -1) ?
				(SSData(((TrigData *)trig_index[real_trig].proto)->name) ?
				SSData(((TrigData *)trig_index[real_trig].proto)->name) : "(Unnamed)") :
				"(Uncreated)");
	}
	if (!OLC_INTLIST(d)) send_to_char("No scripts attached.\r\n", d->character);
	send_to_char(	"`GA`n) Add Script\r\n"
					"`GR`n) Remove Script\r\n"
					"`GQ`n) Exit Menu\r\n"
					"Enter choice:  ", d->character);
	OLC_MODE(d) = MEDIT_TRIGGERS;
}


extern char *genders[];
extern char *race_types[];

void medit_disp_opinion(Opinion *op, SInt32 num, DescriptorData *d) {
	char *	buf = get_buffer(MAX_INPUT_LENGTH);
	RNum	rnum;
	
	if (op->sex) {
		sprintbit(op->sex, genders, buf);
		d->character->Send("   `G%1d`n) Sexes : %s\r\n", num, buf);
	} else
		d->character->Send("   `G%1d`n) Sexes : <NONE>\r\n", num);
	
	if (op->race) {
		sprintbit(op->race, race_types, buf);
		d->character->Send("   `G%1d`n) Races : %s\r\n", num+1, buf);
	} else
		d->character->Send("   `G%1d`n) Races : <NONE>\r\n", num+1);
	
	if (op->vnum != NOBODY) {
		d->character->Send("   `G%1d`n) VNum  : %d (%s)\r\n", num+2, op->vnum,
				((rnum = real_mobile(op->vnum) != NOBODY) ?
				(SSData(((CharData *)mob_index[rnum].proto)->general.name) ?
				SSData(((CharData *)mob_index[rnum].proto)->general.name) :
				"Unnamed") : "Invalid VNUM"));
	} else
		d->character->Send("   `G%1d`n) Vnum  : <NONE>\r\n", num+2);
	release_buffer(buf);
}


//	Opinions menus
void medit_disp_opinions_menu(DescriptorData *d) {
	send_to_char("-- Opinions :\r\n", d->character);
	send_to_char("Hates\r\n"
				 "-----\r\n", d->character);	
	medit_disp_opinion(OLC_MOB(d)->mob->hates, 1, d);
	send_to_char("\r\n"
				 "Fears\r\n"
				 "-----\r\n", d->character);	
	medit_disp_opinion(OLC_MOB(d)->mob->fears, 4, d);
	send_to_char("\r\n"
				 "`G0`n) Exit\r\n"
				 "`nEnter choice:  ", d->character);
	OLC_MODE(d) = MEDIT_OPINIONS;
}


//	Display Opinion gender flags.
void medit_disp_opinions_genders(DescriptorData *d) {
	int i;
	char *buf = get_buffer(MAX_INPUT_LENGTH);
	
	for (i = 0; i < NUM_GENDERS; i++) {
		d->character->Send("`g%2d`n) `c%s\r\n", i+1, genders[i]);
	}
	sprintbit(OLC_OPINION(d)->sex, genders, buf);
	d->character->Send("`nCurrent genders : `c%s\r\n"
				 "`nEnter genders (0 to quit): ", buf);
	release_buffer(buf);
}


//	Display Opinion races
void medit_disp_opinions_races(DescriptorData *d) {
	int i;
	char *buf = get_buffer(MAX_INPUT_LENGTH);
	
	for (i = 0; i < NUM_RACES + 1; i++) {
		d->character->Send("`g%2d`n) `c%s\r\n", i + 1, race_types[i]);
	}
	sprintbit(OLC_OPINION(d)->race, race_types, buf);
	d->character->Send("`nCurrent races   : `c%s\r\n"
				 "`nEnter races (0 to quit): ", buf);
	release_buffer(buf);
}


/**************************************************************************
 *                      The GARGANTAUN event handler                      *
 **************************************************************************/

void medit_parse(DescriptorData * d, char *arg) {
	int i, number;
	struct int_list *temp, *intlist;
	char *buf;
	
	if (OLC_MODE(d) > MEDIT_NUMERICAL_RESPONSE) {
		if(*arg && !is_number(arg)) {
			send_to_char("Field must be numerical, try again : ", d->character);
			return;
		}
	}

	switch (OLC_MODE(d)) {
/*-------------------------------------------------------------------*/
		case MEDIT_CONFIRM_SAVESTRING:
			/*
			 * Ensure mob has MOB_ISNPC set or things will go pair shaped.
			 */
			SET_BIT(MOB_FLAGS(OLC_MOB(d)), MOB_ISNPC);
			switch (*arg) {
				case 'y':
				case 'Y':
					/*
					 * Save the mob in memory and to disk.
					 */
					send_to_char("Saving mobile to memory.\r\n", d->character);
					medit_save_internally(d);
					mudlogf(NRM, LVL_BUILDER, TRUE, "OLC: %s edits mob %d", d->character->RealName(), OLC_NUM(d));
					/* FALL THROUGH */
				case 'n':
				case 'N':
					cleanup_olc(d, CLEANUP_ALL);
					break;
				default:
					send_to_char("Invalid choice!\r\n"
								 "Do you wish to save the mobile? ", d->character);
			}
			return;
/*-------------------------------------------------------------------*/
		case MEDIT_MAIN_MENU:
			i = 0;
			switch (*arg) {
				case 'q':
				case 'Q':
					if (OLC_VAL(d)) { /*. Anything been changed? .*/
						send_to_char("Do you wish to save the changes to the mobile? (y/n) : ", d->character);
						OLC_MODE(d) = MEDIT_CONFIRM_SAVESTRING;
					} else
						cleanup_olc(d, CLEANUP_ALL);
					return;
				case '1':
					OLC_MODE(d) = MEDIT_SEX;
					medit_disp_sex(d);
					return;
				case '2':
					OLC_MODE(d) = MEDIT_ALIAS;
					i--;
					break;
				case '3':
					OLC_MODE(d) = MEDIT_S_DESC;
					i--;
					break;
				case '4':
					OLC_MODE(d) = MEDIT_L_DESC;
					i--;
					break;
				case '5':
					OLC_MODE(d) = MEDIT_D_DESC;
					OLC_VAL(d) = 1;
					SEND_TO_Q("Enter mob description:
                                        (/s saves /c clears /h for help)\r\n\r\n", d);
					d->backstr = NULL;
					if (SSData(OLC_MOB(d)->general.description)) {
						SEND_TO_Q(SSData(OLC_MOB(d)->general.description), d);
						d->backstr = str_dup(SSData(OLC_MOB(d)->general.description));
					}
					d->str = &OLC_MOB(d)->general.description->str;
					d->max_str = MAX_MOB_DESC;
					d->mail_to = 0;
					return;
				case '6':
					OLC_MODE(d) = MEDIT_LEVEL;
					i++;
					break;
				case '7':
					OLC_MODE(d) = MEDIT_RACE;
					medit_disp_race(d);
					i++;
					break;
				case '8':
					OLC_MODE(d) = MEDIT_HITROLL;
					i++;
					break;
				case '9':
					OLC_MODE(d) = MEDIT_DAMROLL;
					i++;
					break;
				case 'a':
				case 'A':
					OLC_MODE(d) = MEDIT_NDD;
					i++;
					break;
				case 'b':
				case 'B':
					OLC_MODE(d) = MEDIT_SDD;
					i++;
					break;
				case 'c':
				case 'C':
					OLC_MODE(d) = MEDIT_NUM_HP_DICE;
					i++;
					break;
				case 'd':
				case 'D':
					OLC_MODE(d) = MEDIT_SIZE_HP_DICE;
					i++;
					break;
				case 'e':
				case 'E':
					OLC_MODE(d) = MEDIT_ADD_HP;
					i++;
					break;
				case 'f':
				case 'F':
					OLC_MODE(d) = MEDIT_AC;
					i++;
					break;
				case 'g':
				case 'G':
					medit_disp_attributes_menu(d);
					return;
				case 'h':
				case 'H':
					OLC_MODE(d) = MEDIT_CLAN;
					i++;
					break;
				case 'i':
				case 'I':
					OLC_MODE(d) = MEDIT_POS;
					medit_disp_positions(d);
					return;
				case 'j':
				case 'J':
					OLC_MODE(d) = MEDIT_DEFAULT_POS;
					medit_disp_positions(d);
					return;
				case 'k':
				case 'K':
					OLC_MODE(d) = MEDIT_ATTACK;
					medit_disp_attack_types(d);
					return;
				case 'l':
				case 'L':
					OLC_MODE(d) = MEDIT_NPC_FLAGS;
					medit_disp_mob_flags(d);
					return;
				case 'm':
				case 'M':
					OLC_MODE(d) = MEDIT_AFF_FLAGS;
					medit_disp_aff_flags(d);
					return;
				case 'o':
				case 'O':
					if (!OLC_MOB(d)->mob->hates)
						OLC_MOB(d)->mob->hates = new Opinion();
					if (!OLC_MOB(d)->mob->fears)
						OLC_MOB(d)->mob->fears = new Opinion();
					medit_disp_opinions_menu(d);
					return;
				case 'p':
				case 'P':
					OLC_MODE(d) = MEDIT_MPROG;
					medit_disp_mprog(d);
					return;
				case 's':
				case 'S':
					medit_disp_scripts_menu(d);
					return;
				default:
					medit_disp_menu(d);
					return;
			}
			if (i != 0) {
				send_to_char((i == 1) ? "\r\nEnter new value : " :
						((i == -1) ? "\r\nEnter new text :\r\n] " :
						"\r\nOops...:\r\n"), d->character);
				return;
			}
			break;
/*-------------------------------------------------------------------*/
		case MEDIT_ALIAS:
			SSFree(GET_ALIAS(OLC_MOB(d)));
			GET_ALIAS(OLC_MOB(d)) = SSCreate(*arg ? arg : "undefined"); 
			break;
/*-------------------------------------------------------------------*/
		case MEDIT_S_DESC:
			SSFree(GET_SDESC(OLC_MOB(d)));
			GET_SDESC(OLC_MOB(d)) = SSCreate(*arg ? arg : "undefined"); 
			break;
/*-------------------------------------------------------------------*/
		case MEDIT_L_DESC:
			buf = get_buffer(MAX_STRING_LENGTH);
			SSFree(GET_LDESC(OLC_MOB(d)));
			if (*arg) {
				strcpy(buf, arg);
				strcat(buf, "\r\n");
				GET_LDESC(OLC_MOB(d)) = SSCreate(buf);
			} else
				GET_LDESC(OLC_MOB(d)) = SSCreate("undefined");
			release_buffer(buf);
			break;
/*-------------------------------------------------------------------*/
		case MEDIT_D_DESC:
			/*
			 * We should never get here.
			 */
//			cleanup_olc(d, CLEANUP_ALL);
			mudlog("SYSERR: OLC: medit_parse(): Reached D_DESC case!",BRF,LVL_BUILDER,TRUE);
			send_to_char("Oops...\r\n", d->character);
			break;
/*-------------------------------------------------------------------*/
		case MEDIT_NPC_FLAGS:
			if ((i = atoi(arg)) == 0)
				break;
			else if (!((i < 0) || (i > NUM_MOB_FLAGS)))
				TOGGLE_BIT(MOB_FLAGS(OLC_MOB(d)), 1 << (i - 1));
			medit_disp_mob_flags(d);
			return;
/*-------------------------------------------------------------------*/
		case MEDIT_AFF_FLAGS:
			if ((i = atoi(arg)) == 0)
				break;
			else if (!((i < 0) || (i > NUM_AFF_FLAGS)))
				TOGGLE_BIT(AFF_FLAGS(OLC_MOB(d)), 1 << (i - 1));
			medit_disp_aff_flags(d);
			return;
/*-------------------------------------------------------------------*/
		case MEDIT_MPROG_COMLIST:
			/*
			 * We should never get here, but if we do, bail out.
			 */
			cleanup_olc(d, CLEANUP_ALL);
			mudlog("SYSERR: OLC: medit_parse(): Reached MPROG_COMLIST case!", BRF, LVL_ADMIN, TRUE);
			break;
/*-------------------------------------------------------------------*/
		case MEDIT_MPROG:
			if ((i = atoi(arg)) == 0)
				medit_disp_menu(d);
			else if (i == OLC_MTOTAL(d)) {
				struct mob_prog_data *temp;
				CREATE(temp, struct mob_prog_data, 1);
				temp->next = OLC_MPROGL(d);
				temp->type = -1;
				temp->arglist = str_dup("100");
				temp->comlist = str_dup("tap\r\n");
				OLC_MPROG(d) = temp;
				OLC_MPROGL(d) = temp;
				OLC_MODE(d) = MEDIT_CHANGE_MPROG;
				medit_change_mprog (d);
			} else if (i < OLC_MTOTAL(d)) {
				struct mob_prog_data *temp;
				int x=1;
				for (temp = OLC_MPROGL(d);temp && x < i;temp=temp->next)
					x++;
				OLC_MPROG(d) = temp;
				OLC_MODE(d) = MEDIT_CHANGE_MPROG;
				medit_change_mprog (d);
			} else if (i == OLC_MTOTAL(d) + 1) {
				send_to_char ("Which mob prog do you want to purge? ", d->character);
				OLC_MODE(d) = MEDIT_PURGE_MPROG;
			} else
				medit_disp_menu(d);
			return;
/*-------------------------------------------------------------------*/
		case MEDIT_PURGE_MPROG:
			if ((i = atoi(arg)) > 0 && i < OLC_MTOTAL(d)) {
				struct mob_prog_data *temp;
				int x = 1;
				
				for (temp = OLC_MPROGL(d); temp && x < i; temp=temp->next)
					x++;
				OLC_MPROG(d) = temp;
				REMOVE_FROM_LIST(OLC_MPROG(d), OLC_MPROGL(d), next)
				if (OLC_MPROG(d)->arglist)	FREE(OLC_MPROG(d)->arglist);
				if (OLC_MPROG(d)->comlist)	FREE(OLC_MPROG(d)->comlist);
				FREE(OLC_MPROG(d));
				OLC_MPROG(d) = NULL;
				OLC_VAL(d) = 1;
			}
			medit_disp_mprog (d);
			return;
/*-------------------------------------------------------------------*/
		case MEDIT_CHANGE_MPROG:
			if ((i = atoi(arg)) == 1)
				medit_disp_mprog_types(d);
			else if (i == 2) {
				send_to_char ("Enter new arg list: ", d->character);
				OLC_MODE(d) = MEDIT_MPROG_ARGS;
			} else if (i == 3) {
				send_to_char("Enter new mob prog commands:\r\n", d->character);
				/*
				 * Pass control to modify.c for typing.
				 */
				OLC_MODE(d) = MEDIT_MPROG_COMLIST;
				d->backstr = NULL;
				if (OLC_MPROG(d)->comlist) {
					SEND_TO_Q(OLC_MPROG(d)->comlist, d);
					d->backstr = str_dup(OLC_MPROG(d)->comlist);
				}
				d->str = &OLC_MPROG(d)->comlist;
				d->max_str = MAX_STRING_LENGTH;
				d->mail_to = 0;
				OLC_VAL(d) = 1;
			} else
				medit_disp_mprog(d);
			return;
/*-------------------------------------------------------------------*/
		case MEDIT_MPROG_TYPE:
			OLC_MPROG(d)->type = (1 << MAX(0, MIN(atoi(arg), NUM_PROGS-1)));
			OLC_VAL(d) = 1;
			medit_change_mprog(d);
			return;
/*-------------------------------------------------------------------*/
		case MEDIT_MPROG_ARGS:
			OLC_MPROG(d)->arglist = str_dup(arg);
			OLC_VAL(d) = 1;
			medit_change_mprog(d);
			return;
/*-------------------------------------------------------------------*/
/*
 * Numerical responses
 */
		case MEDIT_SEX:
			GET_SEX(OLC_MOB(d)) = (Sex)MAX(0, MIN(NUM_GENDERS - 1, atoi(arg)));
			break;
/*-------------------------------------------------------------------*/
		case MEDIT_HITROLL:
			GET_HITROLL(OLC_MOB(d)) = MAX(0, MIN(50, atoi(arg)));
			break;
/*-------------------------------------------------------------------*/
		case MEDIT_DAMROLL:
			GET_DAMROLL(OLC_MOB(d)) = MAX(0, MIN(50, atoi(arg)));
			break;
/*-------------------------------------------------------------------*/
		case MEDIT_NDD:
			GET_NDD(OLC_MOB(d)) = MAX(0, MIN(30, atoi(arg)));
			break;
/*-------------------------------------------------------------------*/
			case MEDIT_SDD:
			GET_SDD(OLC_MOB(d)) = MAX(0, MIN(127, atoi(arg)));
			break;
/*-------------------------------------------------------------------*/
		case MEDIT_NUM_HP_DICE:
			GET_HIT(OLC_MOB(d)) = MAX(0, MIN(30, atoi(arg)));
			break;
/*-------------------------------------------------------------------*/
		case MEDIT_SIZE_HP_DICE:
			GET_MANA(OLC_MOB(d)) = MAX(0, MIN(1000, atoi(arg)));
			break;
/*-------------------------------------------------------------------*/
		case MEDIT_ADD_HP:
			GET_MOVE(OLC_MOB(d)) = MAX(0, MIN(30000, atoi(arg)));
			break;
/*-------------------------------------------------------------------*/
		case MEDIT_AC:
			GET_AC(OLC_MOB(d)) = MAX(/*-200*/ 0, MIN(/*200*/ 999, atoi(arg)));
			break;
/*-------------------------------------------------------------------*/
		case MEDIT_POS:
			GET_POS(OLC_MOB(d)) = (Position)MAX(0, MIN(NUM_POSITIONS - 1, atoi(arg)));
			break;
/*-------------------------------------------------------------------*/
		case MEDIT_DEFAULT_POS:
			GET_DEFAULT_POS(OLC_MOB(d)) = (Position)MAX(0, MIN(NUM_POSITIONS - 1, atoi(arg)));
			break;
/*-------------------------------------------------------------------*/
		case MEDIT_ATTACK:
			GET_ATTACK(OLC_MOB(d)) = MAX(0, MIN(NUM_ATTACK_TYPES - 1, atoi(arg)));
			break;
/*-------------------------------------------------------------------*/
		case MEDIT_LEVEL:
			GET_LEVEL(OLC_MOB(d)) = MAX(1, MIN(100, atoi(arg)));
			break;
/*-------------------------------------------------------------------*/
		case MEDIT_RACE:
			GET_RACE(OLC_MOB(d)) = (Race)MAX(0, MIN(NUM_RACES, atoi(arg)));
			break;
		case MEDIT_TRIGGERS:
			switch (*arg) {
				case 'a':
				case 'A':
					send_to_char("Enter the VNUM of script to attach: ", d->character);
					OLC_MODE(d) = MEDIT_TRIGGER_ADD;
					return;
				case 'r':
				case 'R':
					send_to_char("Enter script in list to detach: ", d->character);
					OLC_MODE(d) = MEDIT_TRIGGER_PURGE;
					return;
				case 'q':
				case 'Q':
					medit_disp_menu(d);
					return;
				default:
					medit_disp_scripts_menu(d);
					return;
			}
			break;
		case MEDIT_TRIGGER_ADD:
			i = atoi(arg);
			CREATE(intlist, struct int_list, 1);
			intlist->i = i;
			intlist->next = OLC_INTLIST(d);
			OLC_INTLIST(d) = intlist;
			send_to_char("Script attached.\r\n", d->character);
			OLC_VAL(d) = 1;
			medit_disp_scripts_menu(d);
			return;
		case MEDIT_TRIGGER_PURGE:
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
			medit_disp_scripts_menu(d);
			return;
		case MEDIT_CLAN:
			i = atoi(arg);
			if (real_clan(i) != -1)
				GET_CLAN(OLC_MOB(d)) = i;
			break;
		
		
		case MEDIT_ATTRIBUTES:
			switch (*arg) {
				case '1':
					d->character->Send("Strength (%d-%d): ",
							stat_min[GET_RACE(OLC_MOB(d))][0], stat_max[GET_RACE(OLC_MOB(d))][0]);
					OLC_MODE(d) = MEDIT_ATTR_STR;
					break;
				case '2':
					d->character->Send("Intelligence (%d-%d): ",
							stat_min[GET_RACE(OLC_MOB(d))][1], stat_max[GET_RACE(OLC_MOB(d))][1]);
					OLC_MODE(d) = MEDIT_ATTR_INT;
					break;
				case '3':
					d->character->Send("Perception (%d-%d): ",
							stat_min[GET_RACE(OLC_MOB(d))][2], stat_max[GET_RACE(OLC_MOB(d))][2]);
					OLC_MODE(d) = MEDIT_ATTR_PER;
					break;
				case '4':
					d->character->Send("Agility (%d-%d): ",
							stat_min[GET_RACE(OLC_MOB(d))][3], stat_max[GET_RACE(OLC_MOB(d))][3]);
					OLC_MODE(d) = MEDIT_ATTR_AGI;
					break;
				case '5':
					d->character->Send("Constitution (%d-%d): ",
							stat_min[GET_RACE(OLC_MOB(d))][4], stat_max[GET_RACE(OLC_MOB(d))][4]);
					OLC_MODE(d) = MEDIT_ATTR_CON;
					break;
				case 'q':
				case 'Q':
					medit_disp_menu(d);
					return;
				default:
					medit_disp_attributes_menu(d);
					return;
			}
			OLC_VAL(d) = 1;
			return;
		case MEDIT_ATTR_STR:
			GET_STR(OLC_MOB(d)) = MAX(stat_min[GET_RACE(OLC_MOB(d))][0], MIN(stat_max[GET_RACE(OLC_MOB(d))][0], atoi(arg)));
			medit_disp_attributes_menu(d);
			return;
		case MEDIT_ATTR_INT:
			GET_INT(OLC_MOB(d)) = MAX(stat_min[GET_RACE(OLC_MOB(d))][1], MIN(stat_max[GET_RACE(OLC_MOB(d))][1], atoi(arg)));
			medit_disp_attributes_menu(d);
			return;
		case MEDIT_ATTR_PER:
			GET_PER(OLC_MOB(d)) = MAX(stat_min[GET_RACE(OLC_MOB(d))][2], MIN(stat_max[GET_RACE(OLC_MOB(d))][2], atoi(arg)));
			medit_disp_attributes_menu(d);
			return;
		case MEDIT_ATTR_AGI:
			GET_AGI(OLC_MOB(d)) = MAX(stat_min[GET_RACE(OLC_MOB(d))][3], MIN(stat_max[GET_RACE(OLC_MOB(d))][3], atoi(arg)));
			medit_disp_attributes_menu(d);
			return;
		case MEDIT_ATTR_CON:
			GET_CON(OLC_MOB(d)) = MAX(stat_min[GET_RACE(OLC_MOB(d))][4], MIN(stat_max[GET_RACE(OLC_MOB(d))][4], atoi(arg)));
			medit_disp_attributes_menu(d);
			return;
//		Opinions Menu
		case MEDIT_OPINIONS:
			OLC_OPINION(d) = NULL;
			switch (*arg) {
				//	Hates
				case '1':	// Sex
					OLC_OPINION(d) = OLC_MOB(d)->mob->hates;
					medit_disp_opinions_genders(d);
					OLC_MODE(d) = MEDIT_OPINIONS_HATES_SEX;
					break;
				case '2':	// Race
					OLC_OPINION(d) = OLC_MOB(d)->mob->hates;
					medit_disp_opinions_races(d);
					OLC_MODE(d) = MEDIT_OPINIONS_HATES_RACE;
					break;
				case '3':	// VNum
					OLC_OPINION(d) = OLC_MOB(d)->mob->hates;
					send_to_char("VNum (-1 for none): ", d->character);
					OLC_MODE(d) = MEDIT_OPINIONS_HATES_VNUM;
					break;
				
				//	Fears
				case '4':	// Sex
					OLC_OPINION(d) = OLC_MOB(d)->mob->fears;
					medit_disp_opinions_genders(d);
					OLC_MODE(d) = MEDIT_OPINIONS_FEARS_SEX;
					break;
				case '5':	// Race
					OLC_OPINION(d) = OLC_MOB(d)->mob->fears;
					medit_disp_opinions_races(d);
					OLC_MODE(d) = MEDIT_OPINIONS_FEARS_RACE;
					break;
				case '6':	// VNum
					OLC_OPINION(d) = OLC_MOB(d)->mob->fears;
					send_to_char("VNum (-1 for none): ", d->character);
					OLC_MODE(d) = MEDIT_OPINIONS_FEARS_VNUM;
					break;
					
				case '0':
					medit_disp_menu(d);
					if (!OLC_MOB(d)->mob->hates->sex && !OLC_MOB(d)->mob->hates->race &&
							(OLC_MOB(d)->mob->hates->vnum == NOBODY)) {
						delete OLC_MOB(d)->mob->hates;
						OLC_MOB(d)->mob->hates = NULL;
					}
					if (!OLC_MOB(d)->mob->fears->sex && !OLC_MOB(d)->mob->fears->race &&
							(OLC_MOB(d)->mob->fears->vnum == NOBODY)) {
						delete OLC_MOB(d)->mob->fears;
						OLC_MOB(d)->mob->fears = NULL;
					}
					return;
			}
			return;
		
		case MEDIT_OPINIONS_HATES_SEX:
		case MEDIT_OPINIONS_FEARS_SEX:
			if ((i = atoi(arg))) {
				if (i < NUM_GENDERS) {
					if (OLC_MODE(d) == MEDIT_OPINIONS_HATES_SEX)
						TOGGLE_BIT(OLC_MOB(d)->mob->hates->sex, 1 << (i - 1));
					else
						TOGGLE_BIT(OLC_MOB(d)->mob->fears->sex, 1 << (i - 1));
				}
				medit_disp_opinions_genders(d);
				OLC_VAL(d) = 1;
			} else
				medit_disp_opinions_menu(d);
			return;
		
		case MEDIT_OPINIONS_HATES_RACE:
		case MEDIT_OPINIONS_FEARS_RACE:
			if ((i = atoi(arg))) {
				if (i < (NUM_RACES + 1)) {
					if (OLC_MODE(d) == MEDIT_OPINIONS_HATES_RACE)
						TOGGLE_BIT(OLC_MOB(d)->mob->hates->race, 1 << (i - 1));
					else
						TOGGLE_BIT(OLC_MOB(d)->mob->fears->race, 1 << (i - 1));
				}
				medit_disp_opinions_races(d);
				OLC_VAL(d) = 1;
			} else
				medit_disp_opinions_menu(d);
			return;
		
		case MEDIT_OPINIONS_HATES_VNUM:
		case MEDIT_OPINIONS_FEARS_VNUM:
			i = atoi(arg);
			if (real_mobile(i) != NOBODY) {
				if (OLC_MODE(d) == MEDIT_OPINIONS_HATES_RACE)
					OLC_MOB(d)->mob->hates->vnum = i;
				else
					OLC_MOB(d)->mob->fears->vnum = i;
			}
			OLC_VAL(d) = 1;
			medit_disp_opinions_menu(d);
			return;
/*-------------------------------------------------------------------*/
		default:
			/*
			 * We should never get here.
			 */
//			cleanup_olc(d, CLEANUP_ALL);
			mudlogf(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: medit_parse(): Reached default case!  Case: %d", OLC_MODE(d));
			send_to_char("Oops...\r\n", d->character);
//			return;
			break;
	}
	/*
	 * END OF CASE 
	 * If we get here, we have probably changed something, and now want to
	 * return to main menu.  Use OLC_VAL as a 'has changed' flag  
	 */
	OLC_VAL(d) = 1;
	medit_disp_menu(d);
}

