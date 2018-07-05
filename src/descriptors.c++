/*************************************************************************
*   File: descriptors.c++            Part of Aliens vs Predator: The MUD *
*  Usage: Primary code for descriptors                                   *
*************************************************************************/

#include "descriptors.h"
#include "utils.h"

LList<DescriptorData *> descriptor_list;	//	master desc list

//	Buffer management externs
extern int buf_largecount;
extern int buf_overflows;
extern int buf_switches;
extern struct txt_block *bufpool;


DescriptorData::DescriptorData(UInt32 desc) {
	this->Init(desc);
}


DescriptorData::~DescriptorData(void) {
	if (this->history) {	// Clear the command history
		for (int count = 0; count < HISTORY_SIZE; count++)
			if (this->history[count])
				FREE(this->history[count]);
		delete this->history;
	}

	if (this->showstr_head)		FREE(this->showstr_head);
	if (this->showstr_count)	FREE(this->showstr_vector);
	if (this->storage)			FREE(this->storage);
}


//	Initialize a descriptor
void DescriptorData::Init (UInt32 desc) {
	static SInt32	last_desc = 0;	//	last descriptor number
	SInt32			pos;
	
	memset(this, 0, sizeof(*this));
	
	this->descriptor = desc;
#if 0	//	ASYNCH OFF
	STATE(this) = CON_GET_NAME;
#endif
	this->wait = 1;
	this->output = this->small_outbuf;
	this->bufspace = SMALL_BUFSIZE - 1;
	this->login_time = time (0);
	this->history = new char *[HISTORY_SIZE];
	for (pos = 0; pos < HISTORY_SIZE; pos++)
		this->history[pos] = NULL;
	
	if (++last_desc == 1000)
		last_desc = 1;
	this->desc_num = last_desc;
}



#ifdef PATCH_TWOX
/* 2x */
#undef TWOX_DEBUG
#define TWOX_MAX		99		//	Max value of (x*) digit.
#define TWOX_LEN		2		//	How many digits TWOX_MAX is.

//	Check for character codes we should not do (2*) to.  Examples
//	include newlines, carriage returns, and escape sequences (color).
bool EvilChar(char t) {
	/*
	 * If you use the \000 notation, the number must be converted to octal.
	 * Otherwise just put the number in the array without ' or \ marks.
	 * ex: char badcode[] = { 13, 10, 27, 0 };	(Non-portable version of below)
	 */
	char badcode[] = { '\r', '\n', '\033', '\0' };
	int i;

	for (i = 0; badcode[i] != '\0'; i++)
		if (t == badcode[i])
			return true;
	return false;
}
/* End 2x */
#endif


//	Add a new string to a player's output queue
void DescriptorData::Write(const char *txt) {
	int size;
#ifdef PATCH_TWOX
	char *twox;
#endif
	
	if (!this)
		return;
	
	size = strlen(txt);

	/* if we're in the overflow state already, ignore this new output */

#ifdef PATCH_TWOX
	if (this->bufptr < 0 || !txt || !*txt)
#else
	if (this->bufptr < 0)
#endif
		return;

#ifdef PATCH_TWOX
	if ((twox = strstr(this->output, txt)) && *twox && !EvilChar(*twox)) {
		//	error - if we failed processing, allow the normal code to process it.
		int error = FALSE;

#if defined(TWOX_DEBUG)
		/*
		 * If you have any problems with lost characters, color bleeding, or
		 * excessive (2*)'s, turn on the debugging and check for what the
		 * first digit printed out is.  Then add that value to the is_evil_char()
		 * function in the badcode[] array.
		 */
		log("%d-%d-%d-%d", *twox, *(twox + 1), *(twox + 2), *(twox + 3));
#endif

		/*
		 * We're already a (x*) string, let's try to increment the number.
		 * Too bad we can't check for the ( here but we'll do that later.
		 */
		if (*(twox - 1) == ')' && *(twox - 2) == '*') {
		//	number - holds the (x*) digit to increment it.
		//	pbegin - where the ( is in the (x*) so we can overwite the old.
			int number;
			char *pbegin;

		//	Scan backwards for the beginning of the (x*) construct.  We also
		//	stop at the begining of the output buffer to prevent mismatch.
			for (pbegin = twox; *pbegin != '(' && pbegin != this->output; pbegin--);

			/*
			 * Somehow we got a *) without a ( matching.  It's a theory that people
			 * could try to exploit the mismatch so log who saw it also.  This
			 * only means that the character saw it, not necessarily did it.
			 */
			if (sscanf(pbegin, "(%d*)%*s", &number) != 1) {
				log("(2*) Invalid construct %s:\n%s", this->character->RealName(), pbegin);
				error = TRUE;
			} else if (number >= TWOX_MAX) /* Cap the iterations. */
				error = TRUE;
			else {
				/*
				 * bufdiff - difference of the two (x*)'s for buffer alignment.
				 * digit - the new (x*), the size is arbitrary based on TWOX_MAX.
				 */
				char *digit = (char *)malloc(TWOX_LEN+4);
				int bufdiff = twox - pbegin;

				/* Create the number. */
				/* snprintf */
				sprintf(digit, /*TWOX_LEN + 4,*/ "(%d*)", number + 1);
				/* Figure out how much more space this construct used than the old. */
				bufdiff = strlen(digit) - bufdiff;

				/* If we don't have room in the buffer, just fall through. */
				if (this->bufspace - bufdiff >= 0) {
					//	new2x - a copy of the old string.  We overwrite the old string
					//	and that causes nasty overlap problems without the str_dup().
					char *new2x = str_dup(twox);

					//	Overwrite the old position with the (x*) and the old string.
					strcpy(pbegin, digit);
					strcat(pbegin, new2x);
					//	Adjust the buffers.
					this->bufspace -= bufdiff;
					this->bufptr += bufdiff;
					FREE(new2x);
				} else		//	Not enough room, let other code handle it.
					error = TRUE;

				/* Be tidy, free everything. */
				FREE(digit);
			}
		} else if (*twox && this->bufspace < 4)
			/*
			 * Not enough room for a (2*), which means the code below won't have
			 * room to simply append it either but it will switch to a larger
			 * buffer size or overflow down there that I don't want to duplicate.
			 */
			error = TRUE;
		else if (*twox) {
			/*
			 * We have a dupe and 4 bytes to spare. Simply overwrite the old
			 * string and adjust buffers by 4.
			 */
			char *new2x = str_dup(twox);
			sprintf(twox, "(2*)%s", new2x);
			this->bufspace -= 4;
			this->bufptr += 4;
			FREE(new2x);
		}
		/*
		 * If error, we couldn't process the string here, allow the code
		 * below to take a stab at it so we don't just toss the input.
		 */
		if (!error)
			return;
	}
#endif
/* End 2x */

	/* if we have enough space, just write to buffer and that's it! */
	if (this->bufspace >= size) {
		strcpy(this->output + this->bufptr, txt);
		this->bufspace -= size;
		this->bufptr += size;
		return;
	}
	/*
	 * If the text is too big to fit into even a large buffer, chuck the
	 * new text and switch to the overflow state.
	 */
	if ((size + this->bufptr) > (LARGE_BUFSIZE - 1)) {
		this->bufptr = -1;
		buf_overflows++;
		return;
	}
	buf_switches++;

#if defined(USE_CIRCLE_SOCKET_BUF)
	/* if the pool has a buffer in it, grab it */
	if (bufpool != NULL) {
		this->large_outbuf = bufpool;
		bufpool = bufpool->next;
	} else {			/* else create a new one */
		CREATE(this->large_outbuf, struct txt_block, 1);
		CREATE(this->large_outbuf->text, char, LARGE_BUFSIZE);
		buf_largecount++;
	}

	strcpy(this->large_outbuf->text, this->output);	/* copy to big buffer */
	this->output = this->large_outbuf->text;	/* make big buffer primary */
	strcat(this->output, txt);	/* now add new text */
#else
	//	Just request the buffer. Copy the contents of the old, and make it
	//	the primary buffer.
	this->large_outbuf = get_buffer(LARGE_BUFSIZE);
	strcpy(this->large_outbuf, this->output);
	this->output = this->large_outbuf;
	strcat(this->output, txt);
#endif

	/* set the pointer for the next write */
	this->bufptr = strlen(this->output);

	/* calculate how much space is left in the buffer */
	this->bufspace = LARGE_BUFSIZE - 1 - this->bufptr;
}
