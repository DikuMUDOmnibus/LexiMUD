/*************************************************************************
*   File: opinion.h                  Part of Aliens vs Predator: The MUD *
*  Usage: Header file for opinion system                                 *
*************************************************************************/

#ifndef __OPINION_H__
#define __OPINION_H__

#include "types.h"

#define	OP_NEUTRAL	(1 << 0)
#define OP_MALE		(1 << 1)
#define OP_FEMALE	(1 << 2)

#define	OP_CHAR		(1 << 0)
#define OP_SEX		(1 << 1)
#define OP_RACE		(1 << 2)
#define OP_VNUM		(1 << 3)

class Opinion {
public:
	Opinion(void);
	Opinion(Opinion *old);
	~Opinion(void);
	void		Clear(void);
	bool		RemChar(SInt32 id);
	void		RemOther(UInt32 type);
	bool		AddChar(SInt32 id);
	void		AddOther(UInt32 type, UInt32 param);
	bool		InList(SInt32 id);
	bool		IsTrue(CharData *ch);
	CharData *	FindTarget(CharData *ch);

	struct int_list	*charlist;
	Flags		sex;
	Flags		race;
	VNum		vnum;
	
	Flags		active;
};

#endif