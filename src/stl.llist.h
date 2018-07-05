/***************************************************************************\
[*]      __     __  ___ __  _____  __     [*]       Part of LexiMUD       [*]
[*]     / /  ___\ \/ (_) _\/__   \/ /     [*]                             [*]
[*]    / /  / _ \\  /| \ \   / /\/ /      [*] All rights reserved         [*]
[*]   / /__|  __//  \| |\ \ / / / /___    [*] Copyright(C) 1997-1998      [*]
[*]   \____/\___/_/\_\_\__/ \/  \____/    [*] Chris Jacobson (FearItself) [*]
[*]   LexiMUD Standard Template Library   [*] fear@technologist.com       [*]
[-]---------------------------------------+-+-----------------------------[-]
[*] File : stl.llist.h                                                    [*]
[*] Usage: Linked List template                                           [*]
\***************************************************************************/


#ifndef __STL_LLIST_H__
#define __STL_LLIST_H__


template <class T> class LListNode;
template <class T> class LList;
template <class T> class LListIterator;


template <class T>
class LListNode {
public:
	friend class LList<T>;
	friend class LListIterator<T>;
	
						LListNode(void) : data(NULL), next(NULL), prev(NULL) { };
						LListNode(T item) : data(item), next(NULL), prev(NULL) { };
//						LListNode(LListNode<T> *theNode) : data(NULL), prev(NULL), next(NULL) {
//							if (theNode)	this->data = theNode->data;
//						}
						~LListNode(void) { };
protected:
	T					data;
	LListNode<T>		*next, *prev;
};


template <class T>
class LList {
public:
	friend class LListIterator<T>;
	
						LList(void) : head(NULL), tail(NULL), iters(NULL), count(0) { }
//						LList(T item) : head(NULL), tail(NULL), iters(NULL), count(0) {
//							this->Add(item);
//						}
//						LList(LList<T> &theList) : head(NULL), tail(NULL), iters(NULL), count(0) {
//							LListNode<T> *	node;
//							
//							if (!&theList)	return;
//							
//							for (node = theList.head; node; node = node->next)
//								Append(node->data);
//						}
						~LList(void) {
							this->Clear();
						}
	
	void				Append(T item) {		//	Add to end of list
							LListNode<T> *	node = new LListNode<T>(item);
							
							node->next = NULL;
							node->prev = this->tail;
							
							if (this->tail)		this->tail->next = node;
							this->tail = node;
							if (!this->head)	this->head = node;
							
							this->count++;
						}
	void				Prepend(T item) {		//	Add to beginning of list
							LListNode<T> *	node = new LListNode<T>(item);
							
							node->prev = NULL;
							node->next = this->head;
							
							if (this->head)		this->head->prev = node;
							this->head = node;
							if (!this->tail)	this->tail = node;
							
							this->count++;
						}
	void				Add(T item) {			//	Add (to beginning)
							this->Prepend(item);
						}
	void				InsertAfter(T item, T after) {
							LListNode<T> *	element;
							LListNode<T> *	node = new LListNode<T>(item);
							
							for (element = this->head; element; element = element->next)
								if (element->data == after)
									break;
							
							if (!element)	element = this->tail;
							
							node->prev = element;
							if (element) {
								node->next = element->next;
								if (element->next)	element->next->prev = node;
								element->next = node;
							}
							
							if (this->tail == element)	this->tail = node;
							if (this->head == NULL)		this->head = node;
							
							this->count++;
						}
	void				InsertBefore(T item, T before) {
							LListNode<T> *	element;
							LListNode<T> *	node = new LListNode<T>(item);
							
							for (element = this->head; element; element = element->next)
								if (element->data == before)
									break;
							
							if (!element)	element = this->head;
							
							node->next = element;
							if (element) {
								node->prev = element->prev;
								if (element->prev)	element->prev->next = node;
								element->prev = node;
							}
							
							if (this->head == element)	this->head = node;
							if (this->tail == NULL)		this->tail = node;
						
							this->count++;
						}
	void				Remove(T item) {
							LListIterator<T> *	iter;
							LListNode<T> *		element;
							
							if (!this->head)
								return;
							
//							do {
								for (element = this->head; element; element = element->next)
									if (element->data == item)
										break;
								
								if (!element)				return;

								if (element->prev)		element->prev->next = element->next;
								else					this->head = element->next;
								
								if (element->next)		element->next->prev = element->prev;
								else					this->tail = element->prev;
								
								for (iter = this->iters; iter; iter = iter->next)
									iter->Update(element);
								
								delete element;
								count--;
//							} while (element)
						}
	void				UpdateIters(void) {
							LListIterator<T> *iter;
							for (iter = this->iters; iter; iter = iter->next)
								iter->list = this;
						}
	T					Top(void) { return this->head ? this->head->data : NULL; }
	T					Bottom(void) { return this->tail ? this->tail->data : NULL; }
	SInt32				Count(void) { return count; }
	bool				InList(T item) {
							LListNode<T> *	element;
							
							for (element = this->head; element; element = element->next) {
								if (element->data == item)
									return true;
							}
							return false;
						}
	void				Clear(void) {
							LListNode<T> *	element;
							while(this->head) {
								element = this->head;
								this->head = this->head->next;
								delete element;
							}
						}
	void				Erase(void) {
							this->head = NULL;
							this->tail = NULL;
							this->iters = NULL;
							this->count = 0;
						}
protected:
	LListNode<T> *		head;
	LListNode<T> *		tail;
	LListIterator<T> *	iters;
	SInt32				count;
};


template <class T>
class LListIterator {
public:
	friend class LList<T>;
						LListIterator(void) : list(NULL), current(NULL), next(NULL) { };
						LListIterator(LList<T> &theList) : list(NULL), current(NULL), next(NULL) {
							this->Start(theList);
						}
						~LListIterator(void) { this->Finish(); }
	void				Start(LList<T> &theList) {
							if (this->list)		this->Finish();
							if (!(this->list = (&theList)))
								return;
							this->next = this->list->iters;
							this->list->iters = this;
							this->Reset();
						}
	void				Finish(void) {
							if (!this->list)	return;
							this->list->iters = this->next;
							this->current = NULL;
							this->next = NULL;
							this->list = NULL;
						}
	void				Restart(LList<T> &theList) {
							this->Finish();
							this->Start(theList);
						}
	void				Update(LListNode<T> *element) {
							if (element == this->current)	this->current = this->current->next;
						}
	
	T					Peek(T def = 0) { return (this->current ? this->current->data : def); }
	T					Next(T def = 0) {
							if (!current)	return def;
							T data = this->current->data;
							this->current = this->current->next;
							return data;
						}
	void				Reset(void)	{ this->current = this->list ? this->list->head : NULL; }
	
protected:
//	void				UpdatePtrs(LListNode<T> *element) {
//							this->prev = element->prev;
//							this->next = element->next;
//						}

	LList<T> *			list;
	LListNode<T> *		current;
	LListIterator<T> *	next;
};


#define START_ITER(iter, var, type, list)		\
	LListIterator<type> iter(list);				\
	while ((var = iter.Next()))

#define END_ITER(iter)

#endif
