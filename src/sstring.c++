/*************************************************************************
*   File: sstring.cp                 Part of Aliens vs Predator: The MUD *
*  Usage: Code file for shared strings                                   *
*************************************************************************/

#include "types.h"
#include "utils.h"
#include "files.h"
#include "stl.llist.h"

#define MAX_SSCOUNT   INT_MAX


class SharedString;
class NSString;


class SharedString {
protected:
	friend class	NSString;
	SInt32			count;
	SInt32			size;
	char *			string;
	
	inline			SharedString(void) : count(1), size(0), string(NULL) { };
	inline			SharedString(const char *x) : count(1), size(strlen(x) + 1),
						string(new char[this->size]) { strcpy(this->string, x); };
	inline			SharedString(const SharedString &orig) : count(1), size(strlen(orig.string) + 1),
						string(new char[this->size]) { strcpy(this->string, orig.string); };
	inline			SharedString(int asize) : count(1), size(asize), string(new char[asize]) { };
	inline			~SharedString(void) { delete [] string; };
	
	const SharedString & operator=(const SharedString &);
};


class NSString {
protected:
	static SharedString *sharedEmpty;
	SharedString *	shared;

public:
	inline			NSString(void) : shared(this->sharedEmpty) { this->shared->count++; };
	inline			NSString(const char *s) : shared(new SharedString(s)) { };
	inline			NSString(const NSString & x) : shared(x.shared) { this->shared->count++; };
	inline			NSString(int asize) : shared(new SharedString(asize)) { *this->shared->string = '\0'; };
	inline			~NSString(void) {
						if (this->shared->count == 1)
							delete this->shared;
						else this->shared->count--;
					};
	inline			operator const char*() const { return this->shared->string; };
	
	inline SInt32	Count(void) { return this->shared->count; };
	inline SInt32	Length(void) const { return strlen(this->shared->string); };
	inline void		Clear(void) { };
	inline SInt32	Size(void) const { return this->shared->size; };
	inline void		Alloc(SInt32) { };
	const char *	Chars(void) const { return this->shared->string; };
//	char *			dup(void) const { };
};


/*
inline bool operator== (const char *ptr, const String & str) {
	return str.compare(ptr);
}
*/

//	Used for a super-shared-string memory architecture
//	Save (some) memory at cost of speed during SSCreate calls
//	While at same time using +8 bytes per SString
#undef SUPER_SHARED

#ifdef SUPER_SHARED
LList<SString *>	SStrings;
#endif

/* create a new shared string */
SString *SSCreate(const char *str) {
#ifdef SUPER_SHARED
	static LListIterator<SString *>	*iter = new LListIterator<SString *>(&SStrings);
#endif
	SString *p;

	if (!str)
		return NULL;
	else {
#ifdef SUPER_SHARED
		iter->Reset();
		for(iter->Reset(), p = iter->Next(); p; p = iter->Next()) {
			if (p->str && !strcmp(p->str, str))
				break;
		}
		if (!p) {
#endif
			p = new SString;
			p->str = str_dup(str);
			p->count = 1;
#ifdef SUPER_SHARED
			SStrings.Add(p);
		} else
			p->count++;
#endif
		return p;
	}
}


/* create another occurance of a shared string */
SString *SSShare(SString *shared) {
	if (!shared)
		return NULL;
//	else if (shared->count < MAX_SSCOUNT) {
		shared->count++;
		return shared;
//	} else
//		return SSCreate(shared->str);
}


/* free a shared string. can handle nonexistant ss */
void SSFree (SString *shared) {
	if (shared && --shared->count == 0) {
		if (shared->str)	FREE(shared->str);
#ifdef	SUPER_SHARED
		SStrings.Remove(shared);
#endif
		delete shared;
	}
}


SString *SSfread(FILE * fl, char *error, char *filename) {
	char *str;
	SString *tmp;
	
	str = fread_string(fl, error, filename);
	tmp = SSCreate(str);
	
	if(str)	FREE(str);
	return tmp;
}
