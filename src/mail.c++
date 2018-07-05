/* ************************************************************************
*   File: mail.c                                        Part of CircleMUD *
*  Usage: Internal funcs and player spec-procs of mud-mail system         *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

/******* MUD MAIL SYSTEM MAIN FILE ***************************************

Written by Jeremy Elson (jelson@cs.jhu.edu)

*************************************************************************/




#include "characters.h"
#include "descriptors.h"
#include "objects.h"
#include "utils.h"
#include "comm.h"
#include "db.h"
#include "interpreter.h"
#include "mail.h"
#include "buffer.h"

#include "imc.h"

#define MIN_IMC_MAIL_LEVEL	6

void postmaster_send_mail(CharData * ch, CharData *mailman, char *arg);
void postmaster_check_mail(CharData * ch, CharData *mailman, char *arg);
void postmaster_receive_mail(CharData * ch, CharData *mailman, char *arg);

bool no_mail = false;
int find_name(char *name);

mail_index_type *mail_index = NULL;		//	list of recs in the mail file
position_list_type *free_list = NULL;	//	list of free positions in file
long file_end_pos = 0;					//	length of file

void push_free_list(SInt32 pos);
SInt32 pop_free_list(void);
void write_to_file(Ptr buf, SInt32 size, SInt32 filepos);
void read_from_file(Ptr buf, SInt32 size, SInt32 filepos);
void index_mail(SInt32 id_to_index, SInt32 pos);
mail_index_type *find_char_in_index(SInt32 searchee);
SPECIAL(postmaster);


void push_free_list(SInt32 pos) {
	position_list_type *new_pos;

	CREATE(new_pos, position_list_type, 1);
	new_pos->position = pos;
	new_pos->next = free_list;
	free_list = new_pos;
}



SInt32 pop_free_list(void) {
	position_list_type *old_pos;
	SInt32	return_value;

	if (!(old_pos = free_list))
		return file_end_pos;
		
	return_value = free_list->position;		//	Save offset of free block
	free_list = old_pos->next;				//	Remove block from list
	free(old_pos);							//	Get rid of memory node took
	return return_value;					//	Give back the free offset
}



mail_index_type *find_char_in_index(SInt32 searchee) {
	mail_index_type *tmp;

	if (searchee < 0) {
		log("SYSERR: Mail system -- non fatal error #1 (searchee == %d).", searchee);
		return NULL;
	}
	for (tmp = mail_index; (tmp && tmp->recipient != searchee); tmp = tmp->next);

	return tmp;
}



void write_to_file(Ptr buf, SInt32 size, SInt32 filepos) {
	FILE *mail_file;

	if (filepos % BLOCK_SIZE) {
		log("SYSERR: Mail system -- fatal error #2!!! (invalid file position)");
		no_mail = true;
	} else if (!(mail_file = fopen(MAIL_FILE, "r+b"))) {
		log("SYSERR: Unable to open mail file.");
		no_mail = true;
	} else {
		fseek(mail_file, filepos, SEEK_SET);
		fwrite(buf, size, 1, mail_file);

		/* find end of file */
		fseek(mail_file, 0L, SEEK_END);
		file_end_pos = ftell(mail_file);
		fclose(mail_file);
	}
}


void read_from_file(Ptr buf, SInt32 size, SInt32 filepos) {
	FILE *mail_file;

	if (filepos % BLOCK_SIZE) {
		log("SYSERR: Mail system -- fatal error #3!!! (invalid filepos read)");
		no_mail = true;
	} else if (!(mail_file = fopen(MAIL_FILE, "r+b"))) {
		log("SYSERR: Unable to open mail file.");
		no_mail = true;
	} else {
		fseek(mail_file, filepos, SEEK_SET);
		fread(buf, size, 1, mail_file);
		fclose(mail_file);
	}
}


void index_mail(SInt32 id_to_index, SInt32 pos) {
	mail_index_type *	new_index;
	position_list_type *new_position;

	if (id_to_index < 0) {
		log("SYSERR: Mail system -- non-fatal error #4. (id_to_index == %d)", id_to_index);
		return;
	}
	if (!(new_index = find_char_in_index(id_to_index))) {
		//	name not already in index.. add it
		CREATE(new_index, mail_index_type, 1);
		new_index->recipient = id_to_index;
		new_index->list_start = NULL;

		//	add to front of list
		new_index->next = mail_index;
		mail_index = new_index;
	}
	//	now, add this position to front of position list
	CREATE(new_position, position_list_type, 1);
	new_position->position = pos;
	new_position->next = new_index->list_start;
	new_index->list_start = new_position;
}


/* SCAN_FILE */
/* scan_file is called once during boot-up.  It scans through the mail file
   and indexes all entries currently in the mail file. */
int scan_file(void) {
	FILE *mail_file;
	header_block_type next_block;
	int total_messages = 0, block_num = 0;

	if (!(mail_file = fopen(MAIL_FILE, "r"))) {
		log("Mail file non-existant... creating new file.");
		touch(MAIL_FILE);
		return 1;
	}
	
	while (fread(&next_block, sizeof(header_block_type), 1, mail_file)) {
		if (next_block.block_type == HEADER_BLOCK) {
			index_mail(next_block.header_data.to, block_num * BLOCK_SIZE);
			total_messages++;
		} else if (next_block.block_type == DELETED_BLOCK)
			push_free_list(block_num * BLOCK_SIZE);
		block_num++;
	}

	file_end_pos = ftell(mail_file);
	fclose(mail_file);
	log("   %ld bytes read.", file_end_pos);
	if (file_end_pos % BLOCK_SIZE) {
		log("SYSERR: Error booting email system -- Mail file corrupt!");
		log("SYSERR: EMail disabled!");
		return 0;
	}
	log("   Mail file read -- %d messages.", total_messages);
	return 1;
}				/* end of scan_file */


/* HAS_MAIL */
/* a simple little function which tells you if the guy has mail or not */
bool has_mail(SInt32 recipient) {
	return (find_char_in_index(recipient) != NULL);
}



//	STORE_MAIL
//	call store_mail to store mail.  (hard, huh? :-) )  Pass 3 arguments:
//	who the mail is to (long), who it's from (long), and a pointer to the
//	actual message text (char *).
void store_mail(SInt32 to, SInt32 from, char *message_pointer) {
	header_block_type	header;
	data_block_type		data;
	char				*msg_txt = message_pointer;
	SInt32				last_address,
						target_address,
						bytes_written	= 0,
						total_length	= strlen(message_pointer);

//	if ((sizeof(header_block_type) != sizeof(data_block_type))
//			|| (sizeof(header_block_type) != BLOCK_SIZE)) {
//		core_dump();
//		return;
//	}

	if (from < 0 || to < 0 || !*message_pointer) {
		log("SYSERR: Mail system -- non-fatal error #5. (from == %d, to == %d)", from, to);
		return;
	}
	memset((char *) &header, 0, sizeof(header));	//	clear the record
	header.block_type = HEADER_BLOCK;
	header.header_data.next_block = LAST_BLOCK;
	header.header_data.from = from;
	header.header_data.to = to;
	header.header_data.mail_time = time(0);
	strncpy(header.txt, msg_txt, HEADER_BLOCK_DATASIZE);
	header.txt[HEADER_BLOCK_DATASIZE] = '\0';

	target_address = pop_free_list();	//	find next free block
	index_mail(to, target_address);		//	add it to mail index in memory
	write_to_file(&header, BLOCK_SIZE, target_address);

	if (strlen(msg_txt) <= HEADER_BLOCK_DATASIZE)
		return;		//	that was the whole message

	bytes_written = HEADER_BLOCK_DATASIZE;
	msg_txt += HEADER_BLOCK_DATASIZE;	/* move pointer to next bit of text */

	/*
	 * find the next block address, then rewrite the header to reflect where
	 * the next block is.
	 */
	last_address = target_address;
	target_address = pop_free_list();
	header.header_data.next_block = target_address;
	write_to_file(&header, BLOCK_SIZE, last_address);

	/* now write the current data block */
	memset((char *) &data, 0, sizeof(data));	/* clear the record */
	data.block_type = LAST_BLOCK;
	strncpy(data.txt, msg_txt, DATA_BLOCK_DATASIZE);
	data.txt[DATA_BLOCK_DATASIZE] = '\0';
	write_to_file(&data, BLOCK_SIZE, target_address);
	bytes_written += strlen(data.txt);
	msg_txt += strlen(data.txt);

	/*
	 * if, after 1 header block and 1 data block there is STILL part of the
	 * message left to write to the file, keep writing the new data blocks and
	 * rewriting the old data blocks to reflect where the next block is.  Yes,
	 * this is kind of a hack, but if the block size is big enough it won't
	 * matter anyway.  Hopefully, MUD players won't pour their life stories out
	 * into the Mud Mail System anyway.
	 * 
	 * Note that the block_type data field in data blocks is either a number >=0,
	 * meaning a link to the next block, or LAST_BLOCK flag (-2) meaning the
	 * last block in the current message.  This works much like DOS' FAT.
	 */

	while (bytes_written < total_length) {
		last_address = target_address;
		target_address = pop_free_list();

		/* rewrite the previous block to link it to the next */
		data.block_type = target_address;
		write_to_file(&data, BLOCK_SIZE, last_address);

		/* now write the next block, assuming it's the last.  */
		data.block_type = LAST_BLOCK;
		strncpy(data.txt, msg_txt, DATA_BLOCK_DATASIZE);
		data.txt[DATA_BLOCK_DATASIZE] = '\0';
		write_to_file(&data, BLOCK_SIZE, target_address);

		bytes_written += strlen(data.txt);
		msg_txt += strlen(data.txt);
	}
}				/* store mail */




//	READ_DELETE
//	read_delete takes 1 char pointer to the name of the person whose mail
//	you're retrieving.  It returns to you a char pointer to the message text.
//	The mail is then discarded from the file and the mail index.
SString *read_delete(SInt32 recipient, char *sender) {
//	recipient is the name as it appears in the index.
//	recipient_formatted is the name as it should appear on the mail
//	header (i.e. the text handed to the player)
	header_block_type	header;
	data_block_type		data;
	mail_index_type *	mail_pointer;
	mail_index_type *	prev_mail;
	position_list_type *position_pointer;
	SInt32				mail_address;
	SInt32				following_block;
	char *message, *tmstr, *buf, *to;
	size_t				string_size;
	SString *			temp;

	if (recipient < 0) {
		log("SYSERR: EMail system -- non-fatal error #6. (recipient == %d)", recipient);
		return NULL;
	}
	if (!(mail_pointer = find_char_in_index(recipient))) {
		log("SYSERR: EMail system -- post office spec_proc error?  Error #7. (invalid char in index)");
		return NULL;
	}
	if (!(position_pointer = mail_pointer->list_start)) {
		log("SYSERR: EMail system -- non-fatal error #8. (invalid position_pointer)");
		return NULL;
	}
	if (!(position_pointer->next)) {	//	just 1 entry in list.
		mail_address = position_pointer->position;
		FREE(position_pointer);

		//	now free up the actual name entry
		if (mail_index == mail_pointer) {	//	name is 1st in list
			mail_index = mail_pointer->next;
			FREE(mail_pointer);
		} else {
			//	find entry before the one we're going to del
			for (prev_mail = mail_index; prev_mail->next != mail_pointer; prev_mail = prev_mail->next);
			prev_mail->next = mail_pointer->next;
			FREE(mail_pointer);
		}
	} else {
		//	move to next-to-last record
		while (position_pointer->next->next)
			position_pointer = position_pointer->next;
		mail_address = position_pointer->next->position;
		free(position_pointer->next);
		position_pointer->next = NULL;
	}

	//	ok, now lets do some readin'!
	read_from_file(&header, BLOCK_SIZE, mail_address);

	if (header.block_type != HEADER_BLOCK) {
		log("SYSERR: Oh dear. (header.block_type == %d, NOT %d)", header.block_type, HEADER_BLOCK);
		log("SYSERR: EMail system disabled!  -- Error #9. (invalid header_block)");
		no_mail = true;
		return NULL;
	}
	tmstr = asctime(localtime(&header.header_data.mail_time));
	*(tmstr + strlen(tmstr) - 1) = '\0';

	buf	= get_buffer(MAX_STRING_LENGTH);
	to	= get_buffer(256);
//	from	= get_buffer(256);
	strcpy(to, get_name_by_id(recipient));
	strcpy(sender, header.header_data.from ? (get_name_by_id(header.header_data.from) ?
			get_name_by_id(header.header_data.from) : "<UNKNOWN>") : "IMC Mail System");
	sprintf(buf,	" * * * * Company EMail System * * * *\r\n"
					"Date: %s\r\n"
					"  To: %s\r\n"
					"From: %s\r\n\r\n",
			tmstr,
			CAP(to),
//			CAP(from)
			CAP(sender)
	);
	release_buffer(to);

	strcat(buf, header.txt);
	message = str_dup(buf);

	release_buffer(buf);

	following_block = header.header_data.next_block;

	//	mark the block as deleted
	header.block_type = DELETED_BLOCK;
	write_to_file(&header, BLOCK_SIZE, mail_address);
	push_free_list(mail_address);

	while (following_block != LAST_BLOCK) {
		read_from_file(&data, BLOCK_SIZE, following_block);

		string_size = (sizeof(char) * (strlen(message) + strlen(data.txt) + 1));
		RECREATE(message, char, string_size);
		strcat(message, data.txt);
		message[string_size - 1] = '\0';
		mail_address = following_block;
		following_block = data.block_type;
		data.block_type = DELETED_BLOCK;
		write_to_file(&data, BLOCK_SIZE, mail_address);
		push_free_list(mail_address);
	}

	temp = SSCreate(message);
	FREE(message);
	return temp;
}


/*****************************************************************
** Below is the spec_proc for a postmaster using the above       **
** routines.  Written by Jeremy Elson (jelson@server.cs.jhu.edu) **
*****************************************************************/

SPECIAL(postmaster) {
	if (!ch->desc || IS_NPC(ch))
		return 0;			//	so mobs don't get caught here

	if (!(CMD_IS("email") || CMD_IS("check") || CMD_IS("receive")))
		return 0;

	if (no_mail)
		send_to_char("Sorry, the email system is having technical difficulties.\r\n", ch);
	else if (CMD_IS("email"))	postmaster_send_mail(ch, (CharData *)me, argument);
	else if (CMD_IS("check"))	postmaster_check_mail(ch, (CharData *)me, argument);
	else if (CMD_IS("receive"))	postmaster_receive_mail(ch, (CharData *)me, argument);
	return 1;
}


void postmaster_send_mail(CharData * ch, CharData *mailman, char *arg) {
	bool	imc_mail = false;
	SInt32	recipient;
	char *	addr = NULL;
	char *	buf = get_buffer(256);

	if (GET_LEVEL(ch) < MIN_MAIL_LEVEL) {
		sprintf(buf, "$n tells you, 'Sorry, you have to be level %d to send mail!'", MIN_MAIL_LEVEL);
		act(buf, FALSE, mailman, 0, ch, TO_VICT);
		release_buffer(buf);
		return;
	}
	one_argument(arg, buf);

	if (!*buf)		/* you'll get no argument from me! */
		act("$n tells you, 'You need to specify an address!'", FALSE, mailman, 0, ch, TO_VICT);
	else {
		if (strchr(buf, '@')) {
			imc_mail = true;
			addr = str_dup(buf);
		}
		if (imc_mail && (GET_LEVEL(ch) < MIN_IMC_MAIL_LEVEL))
			ch->Send("You need to be at least level %d to send notes to other muds.\r\n", MIN_IMC_MAIL_LEVEL);
		else if (((recipient = get_id_by_name(buf)) < 0) && !imc_mail)
			act("$n tells you, 'No one by that name is registered here!'", FALSE, mailman, 0, ch, TO_VICT);
		else {
			act("$n tells you, 'Type your message, (/s saves /h for help)'", FALSE, mailman, 0, ch, TO_VICT);
			SET_BIT(PLR_FLAGS(ch), PLR_MAILING | PLR_WRITING);
			
			if (imc_mail) {
				ch->desc->mail_to = -1;
				ch->desc->imc_mail_to = addr;
			} else
				ch->desc->mail_to = recipient;
			CREATE(ch->desc->str, char *, 1);
			*(ch->desc->str) = NULL;
			ch->desc->max_str = MAX_MAIL_SIZE;
		}
	}
	release_buffer(buf);
}


void postmaster_check_mail(CharData * ch, CharData *mailman, char *arg) {
//	char *buf;

	if (has_mail(GET_IDNUM(ch)))
		act("$n tells you, 'You have mail waiting.'", FALSE, mailman, 0, ch, TO_VICT);
	else
		act("$n tells you, 'Sorry, you don't have any mail waiting.'", FALSE, mailman, 0, ch, TO_VICT);
}


void postmaster_receive_mail(CharData * ch, CharData *mailman, char *arg) {
	ObjData *obj;
	char *buf, *name;

	if (!has_mail(GET_IDNUM(ch))) {
		act("$n tells you, 'Sorry, you don't have any mail waiting.'", FALSE, mailman, 0, ch, TO_VICT);
		return;
	}
	while (has_mail(GET_IDNUM(ch))) {
		obj = create_obj();
		GET_OBJ_RNUM(obj) = NOTHING;
		obj->name = SSCreate("printout email mail paper letter note");
		obj->description = SSCreate("Someone has left a printout here.");

		GET_OBJ_TYPE(obj) = ITEM_NOTE;
		GET_OBJ_WEAR(obj) = ITEM_WEAR_TAKE | ITEM_WEAR_HOLD;
		GET_OBJ_WEIGHT(obj) = 0;
		GET_OBJ_COST(obj) = 0;
		
		name = get_buffer(MAX_INPUT_LENGTH);
		
		obj->actionDesc = read_delete(GET_IDNUM(ch), name);

		if (obj->actionDesc == NULL) {
			obj->actionDesc = SSCreate("EMail system error - please report.  Error #11.\r\n");
			obj->shortDesc = SSCreate("a printout");
		} else {
			buf = get_buffer(MAX_INPUT_LENGTH);
			sprintf(buf, "a printed e-mail from %s", name);
			obj->shortDesc = SSCreate(buf);
			release_buffer(buf);
		}
		release_buffer(name);
		
		obj->to_char(ch);

		act("$n gives you a piece of email.", FALSE, mailman, 0, ch, TO_VICT);
		act("$N gives $n a piece of email.", FALSE, ch, 0, mailman, TO_ROOM);
  }
}
