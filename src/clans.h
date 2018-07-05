/***************************************************************************\
[*]    __     __  ___                ___  [*]   LexiMUD: A feature rich   [*]
[*]   / /  ___\ \/ (_) /\/\  /\ /\  /   \ [*]      C++ MUD codebase       [*]
[*]  / /  / _ \\  /| |/    \/ / \ \/ /\ / [*] All rights reserved         [*]
[*] / /__|  __//  \| / /\/\ \ \_/ / /_//  [*] Copyright(C) 1997-1998      [*]
[*] \____/\___/_/\_\_\/    \/\___/___,'   [*] Chris Jacobson (FearItself) [*]
[*]        LexiMUD: Feel the Power        [*] fear@technologist.com       [*]
[-]---------------------------------------+-+-----------------------------[-]
[*] File : clans.h                                                        [*]
[*] Usage: Class and function declarations for clans                      [*]
\***************************************************************************/


#include "index.h"
#include "stl.llist.h"


class ClanData {
public:
					ClanData(void);
					~ClanData(void);
	
	bool			AddMember(UInt32 id);
	bool			RemoveMember(UInt32 id);
	bool			IsMember(UInt32 id);
	VNum			vnum;
	
	char *			name;
	char *			description;
	SInt32			owner;
	RNum			room;
	UInt32			savings;
	LList<SInt32>	members;
};

void clans_save_to_disk(void);
RNum real_clan(VNum vnum);
void parse_clan(FILE *fl, int virtual_nr, char *filename);
void load_clans(void);

extern ClanData *clan_index;
extern UInt32 top_of_clant;

#define	GET_CLAN_VNUM(clan)		((clan).vnum)
#define CLANPLAYERS(clan) 		((clan).members.Count())	
#define CLANOWNER(clan)   		((clan).owner)
#define CLANSAVINGS(clan)		((clan).savings)
#define CLANROOM(clan)			((clan).room)

#define GET_CLAN_LEVEL(ch)		((ch)->player->special.clanrank)

#define CLAN_APPLY		0
#define CLAN_MEMBER		1
#define CLAN_COMMANDER	2
#define CLAN_LEADER		3