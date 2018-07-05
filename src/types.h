/***************************************************************************\
[*]    __     __  ___                ___  [*]   LexiMUD: A feature rich   [*]
[*]   / /  ___\ \/ (_) /\/\  /\ /\  /   \ [*]      C++ MUD codebase       [*]
[*]  / /  / _ \\  /| |/    \/ / \ \/ /\ / [*] All rights reserved         [*]
[*] / /__|  __//  \| / /\/\ \ \_/ / /_//  [*] Copyright(C) 1997-1998      [*]
[*] \____/\___/_/\_\_\/    \/\___/___,'   [*] Chris Jacobson (FearItself) [*]
[*]        LexiMUD: Feel the Power        [*] fear@technologist.com       [*]
[-]---------------------------------------+-+-----------------------------[-]
[*] File : types.h                                                        [*]
[*] Usage: Basic data types                                               [*]
\***************************************************************************/

#ifndef __TYPES_H__
#define __TYPES_H__

#include "conf.h"
#include "sysdep.h"


//	Basic Types
typedef unsigned char	UInt8;
typedef signed char		SInt8;
typedef	unsigned short	UInt16;
typedef signed short	SInt16;
typedef unsigned int	UInt32;
typedef signed int		SInt32;
typedef void *			Ptr;


//	Circle Types
typedef SInt16			VNum;
typedef SInt16			RNum;

typedef UInt32			Flags;
typedef	UInt8			Type;


// Shared Strings
typedef struct {
	char *	str;
	UInt32	count;
} SString;

#define SSData(ss)		(((ss) && (ss)->str) ? (ss)->str : (char *) NULL)

SString *SSCreate(const char *str);
SString *SSShare(SString *shared);
SString *SSfread(FILE * fl, char *error, char *filename);
void SSFree (SString *shared);


//	This structure is purely intended to be an easy way to transfer
//	and return information about time (real or mudwise).
struct TimeInfoData {
//	SInt8	hours, day, month;
	SInt8	hours, day, month;
	SInt16	year;
};


struct Dice {
	UInt16	number;
	UInt16	size;
};


struct int_list {
	SInt32	i;
	struct int_list *next;
};


class CharData;
class ObjData;
class DescriptorData;
class RoomData;
class TrigData;
class ScriptData;
class ClanData;
class IndexData;
class Event;
class ExtraDesc;


#endif	// __TYPES_H__