/*************************************************************************
*   File: hash.h                     Part of Aliens vs Predator: The MUD *
*  Usage: Header and inlined code for hash tables                        *
*************************************************************************/


#ifndef __HASH_H__
#define __HASH_H__


#include "types.h"
#include "list.h"
#include "utils.h"

class HashLink;
class HashHeader;

SInt32 KeySorter(HashLink *a, HashLink *b);
SInt32 KeyFinder(HashLink *a, SInt32 key);

typedef void (*DestroyHashFunc)(Ptr data);
void DestroyHashHeader(HashHeader *ht, DestroyHashFunc gman);

typedef void (*HashIterateFunc)(SInt32 key, Ptr data, Ptr cdata);
void HashIterate(HashHeader* ht, HashIterateFunc func, Ptr cdata);


class HashLink {
public:
	ListElement		element;
	SInt32			key;
	Ptr				data;
};


class HashHeader {
public:
	inline			HashHeader(SInt32 rec_size, SInt32 table_size);
	
	inline SInt32	Hash(SInt32 key);
	inline SInt32	Enter(SInt32 key, Ptr data);
	inline Ptr		Remove(SInt32 key);
	
	inline Ptr		Find(SInt32 key);
	inline Ptr		FindOrCreate(SInt32 key);
	inline void		Iterate(HashIterateFunc func, Ptr data);
	
	List *			buckets;
	SInt32			table_size;
	SInt32			rec_size;
protected:
	
	HashLink *		_find(SInt32 key);
	void			_enter(SInt32 key, Ptr data);
	void			_remove(SInt32 key);
};



HashHeader::HashHeader(SInt32 rec_size, SInt32 table_size) {
	SInt32		i;
//	List *		list;
	
	memset(this, 0, sizeof(HashHeader));
	this->table_size = table_size;
	this->rec_size = rec_size;
	this->buckets = new List[table_size];
	
//	for (i = 0, list = this->buckets; i < table_size; ++i, ++list)
	for (i = 0; i < table_size; i++)
		this->buckets[i].Init((ListSortFunc)KeySorter);
}


SInt32 HashHeader::Hash(SInt32 key) {
	return ((SInt32) key * 17) % this->table_size;
}


Ptr HashHeader::Find(SInt32 key) {
	HashLink *	item;
	
	item = this->_find(key);
	return item ? item->data : NULL;
}


SInt32 HashHeader::Enter(SInt32 key, Ptr data) {
	HashLink *item;
	
	if ((item = this->_find(key)))
		return 0;
	
	this->_enter(key, data);
	
	return 1;
}


Ptr HashHeader::FindOrCreate(SInt32 key) {
	HashLink *	item;
	Ptr			rval;
	
	if ((item = this->_find(key)))
		return item->data;
	
	CREATE(rval, char, this->rec_size);
//	rval = ALLOC(sizeof(char), this->rec_size);
		
	this->_enter(key, rval);
	
	return rval;
}


void HashHeader::Iterate(HashIterateFunc func, Ptr data) {
	HashLink *	link;
	List *		list;
	SInt32		i;
	
	for (i = 0, list = this->buckets; i < this->table_size; ++i, ++list) {
		LIST_START_ITER(iter, link, HashLink *, list) {
			func(link->key, link->data, data);
		} LIST_END_ITER(iter);
	}
}

#endif