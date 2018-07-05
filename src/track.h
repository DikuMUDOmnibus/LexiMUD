
#ifndef __TRACK_H__
#define __TRACK_H__

#include "types.h"

class TrackStep {
public:
	RNum		room;
	UInt8		dir;
};


class Path {
public:
//	Path(RNum room, char *name, SInt32 depth, Flags flags);	// Need to differentiate between
//	Path(RNum room, char *name, SInt32 depth, Flags flags);	// name and full name
	~Path(void);
	SInt32		Dir(RNum start);
	
	SInt32		depth;
	Flags		flags;
	SInt32		count;
	CharData *	victim;
	SInt32		dest;
	TrackStep*	moves;
};


#define	HUNT_GLOBAL			1
#define	HUNT_THRU_DOORS		2

//	primary entry points, these are the easiest to use the tracking package

//	build a path to either a named character, or a specific character,
//	path_to_name locates a character for whom is_name(name) returns
//	true, path_to_full_name locates a character with the exact name
//	specified

Path *	Path2Name(RNum start, char *name, SInt32 depth, Flags flags);
Path *	Path2FullName(RNum start, char *name, SInt32 depth, Flags flags);
Path *	Path2Char(RNum start, CharData *ch, SInt32 depth, Flags flags);
Path *	Path2Room(RNum start, RNum dest, SInt32 depth, Flags flags);


/* a lower level function to build a path, perhaps based on a more
   complicated predicate then path_to*_char */
   
typedef SInt32 (*BuildPathFunc)(RNum start, Ptr data);

Path *	PathBuild(RNum start, BuildPathFunc predicate, Ptr data, SInt32 depth, Flags flags);
Path *	PathRebuild(RNum start, Path *path);

/* do ongoing maintenance of an in-progress track */
SInt32	Track(CharData* ch);

//ACMD(do_path);

#endif
