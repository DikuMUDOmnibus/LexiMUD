#ifndef __LIST_H__
#define __LIST_H__


#include "types.h"


class List;
class ListElement;
class ListIterator;


typedef SInt32 (*ListSortFunc)(ListElement *a, ListElement *b);
typedef SInt32 (*ListFindFunc)(ListElement *elem, Ptr extra);


class ListElement {
public:
	ListElement *	next;
	ListElement *	prev;
	List *			list;
};


class List {
public:
	inline			List(void);
	inline			List(ListSortFunc func);
	
	inline void		Init(ListSortFunc func);
	
	inline void		Insert(ListElement *element);
	inline void		Prepend(ListElement *element);
	inline void		Append(ListElement *element);
	inline void		Delete(ListElement *element);
	inline ListElement *	Find(ListFindFunc func, Ptr extra);
	inline void		Push(ListElement *element);
	inline ListElement *	Pop(void);
	
	ListElement *	first;
	ListElement *	last;
	ListSortFunc	sorter;
	List *			iters;
};


class ListIterator {
public:
	inline			ListIterator(List *list);
	inline			~ListIterator(void);
	inline void		UpdatePtrs(ListElement *element);
	inline void		Update(ListElement *element);
	
	inline ListElement *	First(void);
	inline ListElement *	Last(void);
	inline ListElement *	Next(void);
	inline ListElement *	Prev(void);
	
	ListElement		link;
	List *			list;
	ListElement *	next;
	ListElement *	prev;
};


//	Inline code

List::List(void) {

}

List::List(ListSortFunc func) {
	this->Init(func);
}

void List::Init(ListSortFunc func) {
	this->first = NULL;
	this->last = NULL;
	this->sorter = func;
	this->iters = NULL;
}

void List::Insert(ListElement *element) {
	ListElement *	before;
	
	if (!this->first || !this->sorter) {
		this->Prepend(element);
		return;
	}
	
	for (before = this->first; before && (this->sorter(before, element) > 0); before = before->next);

	if (!before) {
		this->Append(element);
		return;
	}
	
	element->next = before;
	element->prev = before->prev;
	
	before->prev = element;
	
	if (element->prev)	element->prev->next = element;
	else				this->first = element;
	
	element->list = this;
}


void List::Prepend(ListElement *element) {
	element->prev = NULL;
	element->next = this->first;
	if (this->first)	this->first->prev = element;
	this->first = element;
	if (!this->last)	this->last = element;
	
	element->list = this;
}


void List::Append(ListElement *element) {
	element->next = NULL;
	element->prev = this->last;
	if (this->last)		this->last->next = element;
	this->last = element;
	if (!this->first)	this->first = element;
	
	element->list = this;
}


void List::Delete(ListElement *element) {
	ListIterator *	iter;
	
	if (element->list != this)
		return;
	
	if (this->iters)
		for (iter = (ListIterator *)this->iters->first; iter; iter = (ListIterator *)iter->link.next)
			iter->Update(element);
	
	if (element->prev)	element->prev->next = element->next;
	else				this->first = element->next;
	
	if (element->next)	element->next->prev = element->prev;
	else				this->last = element->prev;
	
	element->next = NULL;
	element->prev = NULL;
	element->list = NULL;
}


ListElement *List::Find(ListFindFunc func, Ptr extra) {
	ListElement *	element, *next;
	SInt32			compare;
	
	for (element = this->first; element; element = next) {
		next = element->next;
		if (!(compare = func(element, extra)))	return element;
		if (compare < 0)	break;
	}
	return NULL;
}


void List::Push(ListElement *element) {
	this->Prepend(element);
}


ListElement *List::Pop(void) {
	ListElement *element = this->first;
	if (element)	this->Delete(element);
	return element;
}


ListIterator::ListIterator(List *list) {
	memset(this, 0, sizeof(ListIterator));
	if (!list->iters)	list->iters = new List(NULL);
	list->iters->Push(&this->link);
	this->list = list;
}


ListIterator::~ListIterator(void) {
	this->list->iters->Delete(&this->link);
}


ListElement *ListIterator::First(void) {
	ListElement *element;
	
	if ((element = this->list->first))
		this->UpdatePtrs(element);
	
	return element;
}


ListElement *ListIterator::Last(void) {
	ListElement *element;
	
	if ((element = this->list->last))
		this->UpdatePtrs(element);
	
	return element;
}


ListElement *ListIterator::Next(void) {
	ListElement *element;
	
	if ((element = this->next))
		this->UpdatePtrs(element);
	
	return element;
}


ListElement *ListIterator::Prev(void) {
	ListElement *element;
	
	if ((element = this->prev))
		this->UpdatePtrs(element);
	
	return element;
}


void ListIterator::Update(ListElement *element) {
	if (element == this->next)		this->next = element->next;
	else if (element == this->prev)	this->prev = element->prev;
}


void ListIterator::UpdatePtrs(ListElement *element) {
	this->prev = element->prev;
	this->next = element->next;
}


#define LIST_START_ITER(iter, var, type, list) {								\
	ListIterator* iter = new ListIterator(list);						\
	for(var = (type)iter->First() ; var ; var = (type)iter->Next())

#define LIST_END_ITER(iter)	\
	delete iter;		\
	}


#endif