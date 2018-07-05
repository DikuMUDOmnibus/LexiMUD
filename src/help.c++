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



#include "help.h"
#include "character.defs.h"
#include "buffer.h"
#include "utils.h"
#include "handler.h"

#define READ_SIZE 256

void get_one_line(FILE *fl, char *buf);

HelpElement *help_table = NULL;
UInt32 top_of_helpt = 0;

HelpElement::~HelpElement() {
	this->Free();
}


void HelpElement::Free() {
	if (this->keywords)		free(this->keywords);
	if (this->entry)		free(this->entry);
	this->keywords = this->entry = NULL;
}


void load_help(FILE *fl) {
	HelpElement	*el;
	char	*key = get_buffer(READ_SIZE+1),
			*entry = get_buffer(32384);
	char	*line = get_buffer(READ_SIZE+1);

	/* get the keyword line */
	get_one_line(fl, key);
	while (*key != '$') {
		el = &help_table[top_of_helpt];
		
		get_one_line(fl, line);
		*entry = '\0';
		while (*line != '#') {
			strcat(entry, strcat(line, "\r\n"));
			get_one_line(fl, line);
		}
		el->min_level = 0;
		if ((*line == '#') && (*(line + 1) != 0))
			el->min_level = atoi((line + 1));

		el->min_level = MAX(0, MIN(el->min_level, LVL_CODER));

		/* now, add the entry to the index with each keyword on the keyword line */
		el->entry = str_dup(entry);
		el->keywords = str_dup(key);
		
		top_of_helpt++;
		
		/* get next keyword line (or $) */
		get_one_line(fl, key);
	}
	release_buffer(key);
	release_buffer(entry);
	release_buffer(line);
}


HelpElement * find_help(const char *keyword) {
	SInt32	i;
	
	for (i = 0; i < top_of_helpt; i++)
		if (isname(keyword, help_table[i].keywords))
			return (help_table + i);
	
	return NULL;
}
