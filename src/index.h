/*************************************************************************
*   File: index.h                    Part of Aliens vs Predator: The MUD *
*  Usage: Header file for indexes                                        *
*************************************************************************/

#ifndef	__INDEX_H__
#define __INDEX_H__

#include "types.h"
#include "internal.defs.h"
#include "mobprogs.h"

/* element in monster and object index-tables   */
struct IndexData {
	VNum		vnum;			//	virtual number
	UInt32		number;			//	number of existing units
	int_list *	triggers;		//	triggers for mob/obj
	Ptr			proto;
	SPECIAL(*func);				//	specfunc
	Flags		progtypes;		//	program types for MOBProg
	MPROG_DATA *mobprogs;		//	programs for MOBProg
};

#endif