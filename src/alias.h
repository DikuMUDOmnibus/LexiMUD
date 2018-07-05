

#ifndef __ALIAS_H__
#define __ALIAS_H__

#include "types.h"

class Alias {
public:
	Alias(char *arg, char *repl);
	~Alias(void);
	
	char *	command;
	char *	replacement;
	UInt32	type;
};

#define ALIAS_SIMPLE	0
#define ALIAS_COMPLEX	1

#define ALIAS_SEP_CHAR	';'
#define ALIAS_VAR_CHAR	'$'
#define ALIAS_GLOB_CHAR	'*'

#endif
