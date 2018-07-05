/*************************************************************************
*   File: extradesc.h                Part of Aliens vs Predator: The MUD *
*  Usage: Header file for Extra Descriptions                             *
*************************************************************************/

#include "types.h"

// Extra description: used in objects, mobiles, and rooms
class ExtraDesc {
public:
	ExtraDesc(void)	{ memset(this, 0, sizeof(*this)); };
	~ExtraDesc(void) {SSFree(keyword); SSFree(description);};
	SString *	keyword;				//	Keyword in look/examine
	SString *	description;			//	What to see
	ExtraDesc *	next;					//	Next in list
//private:
//	UInt32		count;
};

/*
ExtraDesc *EDCreate(char *str, char *desc);
ExtraDesc *EDShare(ExtraDesc *shared);
void EDFree (ExtraDesc *shared);
*/
