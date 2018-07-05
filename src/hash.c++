/*************************************************************************
*   File: hash.c++                   Part of Aliens vs Predator: The MUD *
*  Usage: Primary code for hash tables                                   *
*************************************************************************/


#include "types.h"
#include "list.h"
#include "hash.h"


SInt32 KeySorter(HashLink *a, HashLink *b) {
	return (a->key - b->key);
}


SInt32 KeyFinder(HashLink *a, SInt32 key) {
	return (key - a->key);
}


HashLink *HashHeader::_find(SInt32 key) {
	return (HashLink *)this->buckets[this->Hash(key)].Find((ListFindFunc)KeyFinder, (Ptr)key);
}


void HashHeader::_enter(SInt32 key, Ptr data) {
	HashLink *	link;
	
	link = new HashLink;
	link->key = key;
	link->data = data;
	
	this->buckets[this->Hash(key)].Insert(&link->element);
}


void HashHeader::_remove(SInt32 key) {
	HashLink *	link;
	
	if ((link = this->_find(key)))
		this->buckets[this->Hash(key)].Delete((ListElement *)link);
}


void DestroyHashHeader (HashHeader *ht, DestroyHashFunc func) {
	HashLink *element;
	SInt32	i;
	
	for (i = 0; i < ht->table_size; i++) {
		while ((element = (HashLink *)ht->buckets[i].first)) {
			func(element->data);
			ht->buckets[i].Delete((ListElement *)element);
			delete element;
		}
	}
	delete ht->buckets;	//	POSSIBLE ERROR
}
