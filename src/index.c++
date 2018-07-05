/*************************************************************************
*   File: index.c++                  Part of Aliens vs Predator: The MUD *
*  Usage: Primary code for indexes                                       *
*************************************************************************/



#include "index.h"

void free_intlist(struct int_list *intlist);

/* release memory allocated for an integer linked list */
void free_intlist(struct int_list *intlist) {
	struct int_list *i, *j;

	for (i = intlist; i; i = j) {
		j = i->next;
		FREE(i);
	}
}
