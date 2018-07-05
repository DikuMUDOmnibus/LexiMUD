/***************************************************************************\
[*]    __     __  ___                ___  [*]   LexiMUD: A feature rich   [*]
[*]   / /  ___\ \/ (_) /\/\  /\ /\  /   \ [*]      C++ MUD codebase       [*]
[*]  / /  / _ \\  /| |/    \/ / \ \/ /\ / [*] All rights reserved         [*]
[*] / /__|  __//  \| / /\/\ \ \_/ / /_//  [*] Copyright(C) 1997-1998      [*]
[*] \____/\___/_/\_\_\/    \/\___/___,'   [*] Chris Jacobson (FearItself) [*]
[*]        LexiMUD: Feel the Power        [*] fear@technologist.com       [*]
[-]---------------------------------------+-+-----------------------------[-]
[*] File : help.c++                                                       [*]
[*] Usage: Help system code                                               [*]
\***************************************************************************/

#ifndef __HELP_H__
#define __HELP_H__

#include "types.h"

class HelpElement {
public:
//	HelpElement();
						~HelpElement();
	void				Free();
	
	char *	keywords;
	char *	entry;
	SInt32	min_level;
};

HelpElement * find_help(const char *keyword);
void load_help(FILE *fl);

extern HelpElement *help_table;
extern UInt32 top_of_helpt;

#endif