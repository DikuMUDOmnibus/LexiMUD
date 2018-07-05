/*************************************************************************
*   File: affects.c++                Part of Aliens vs Predator: The MUD *
*  Usage: Primary code for affections                                    *
*************************************************************************/

#ifndef __BAN_H__
#define __BAN_H__

#include "types.h"
#include "character.defs.h"
#include "stl.llist.h"

#define BAN_NOT 	0
#define BAN_NEW 	1
#define BAN_SELECT	2
#define BAN_ALL		3

#define BANNED_SITE_LENGTH    50
class BanElement {
public:
	char		site[BANNED_SITE_LENGTH+1];
	int			type;
	time_t		date;
	char		name[MAX_NAME_LENGTH+1];	
};

extern LList<BanElement *>ban_list;

void load_banned(void);
int isbanned(char *hostname);
int Valid_Name(char *newname);
void Read_Invalid_List(void);

#endif