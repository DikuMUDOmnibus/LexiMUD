#include "types.h"

#include "track.h"
#include "structs.h"
#include "interpreter.h"
#include "handler.h"
#include "utils.h"
#include "list.h"
#include "hash.h"
#include "buffer.h"
#include "comm.h"

ACMD(do_path);
int find_first_step(RNum src, char *target);
int find_first_step(RNum src, RNum target);

extern char *dirs[];


SInt32 NamedCharInRoom(RNum room, Ptr data);
SInt32 FullNameInRoom(RNum room, Ptr data);
SInt32 CharInRoom(RNum room, Ptr data);
SInt32 SameRoomNR(RNum room, Ptr data);

struct NCIRData {
	char *		name;
	CharData *	vict;
};


SInt32 NamedCharInRoom(RNum room, Ptr data) {
	NCIRData *	ncir = (NCIRData *)data;
	CharData *	ch;
	
	if (room == -1)
		return 0;
	
	START_ITER(iter, ch, CharData *, world[room].people) {
		if (isname(ncir->name, ch->GetName()) && !AFF_FLAGGED(ch, AFF_NOTRACK)) {
			ncir->vict = ch;
			END_ITER(iter);
			return 1;
		}
	} END_ITER(iter);
	return 0;
}


Path *	Path2Name(RNum start, char *name, SInt32 depth, Flags flags) {
	NCIRData	ncir = { name, NULL };
	Path *		path;
	
	if ((path = PathBuild(start, NamedCharInRoom, &ncir, depth, flags)))
		path->victim = ncir.vict;
	
	return path;
}


//struct FNIRData {
//	SString *	name;
//	CharData *	vict;
//};

SInt32 FullNameInRoom(RNum room, Ptr data) {
	NCIRData *	fnir = (NCIRData *)data;
	CharData *	ch;
	
	if (room == -1)
		return 0;
	
	START_ITER(iter, ch, CharData *, world[room].people) {
		if (!str_cmp(ch->GetName(), fnir->name)) {
			fnir->vict = ch;
			END_ITER(iter);
			return 1;
		}
	} END_ITER(iter);
	return 0;
}


Path * Path2FullName(RNum start, char *name, SInt32 depth, Flags flags) {
	NCIRData	fnir = { name, NULL };
	Path *		path;
	
	if ((path = PathBuild(start, FullNameInRoom, &fnir, depth, flags)))
		path->victim = fnir.vict;
	
	return path;
}


SInt32 CharInRoom(RNum room, Ptr data) {
	CharData *	ch;
	
	if (room == -1)
		return 0;
	
	START_ITER(iter, ch, CharData *, world[room].people) {
		if (ch == (CharData *)data) {
			END_ITER(iter);
			return 1;
		}
	} END_ITER(iter);
	
	return 0;
}


Path * Path2Char(RNum start, CharData *ch, SInt32 depth, Flags flags) {
	Path *		path;
	
	if ((path = PathBuild(start, CharInRoom, ch, depth, flags)))
		path->victim = ch;
	
	return path;
}


SInt32 SameRoomNR(RNum room, Ptr data) {
	return (room == (RNum)data);
}


Path * Path2Room(RNum start, RNum dest, SInt32 depth, Flags flags) {
	return PathBuild(start, SameRoomNR, (Ptr)dest, depth, flags);
}


SInt32 Path::Dir(RNum room) {
	SInt32		i, retry;
	TrackStep *	move;
	
	if (this->victim) {
		if (IN_ROOM(this->victim) == room)
			return -1;
		
		if (IN_ROOM(this->victim) != this->dest) {
			for (move = this->moves, i = 0; i < this->count; ++i, ++move)
				if (move->room == IN_ROOM(this->victim))
					break;
			
			if ((i >= this->count) && !PathRebuild(room, this))
				return -1;
		}
	}
	
	for (retry = 1; retry > 0; retry--) {
		for (move = this->moves, i = 0; i < this->count; ++i, ++move)
			if (move->room == room)
				return move->dir;
		
		if (!this->victim || !PathRebuild(room, this))
			break;
	}

	return -1;
}


Path::~Path(void) {
	if (this->moves)
		delete this->moves;
}

#define GO_OK(exit)			(!EXIT_FLAGGED(exit, EX_CLOSED) \
							&& (exit->to_room != NOWHERE))
#define GO_OK_SMARTER(exit)	(!EXIT_FLAGGED(exit, EX_LOCKED) \
							&& (exit->to_room != NOWHERE))

struct BPElement {
	RNum	room;
	SInt8	dir;
};


struct RoomQueue {
	ListElement	link;
	SInt32		room;
};


void KillBPElement(BPElement *element);
void KillBPElement(BPElement *element) {
	if (element != (Ptr)-1)
		delete element;
}


Path *PathBuild(RNum room, BuildPathFunc predicate, Ptr data, SInt32 depth, Flags flags) {
	HashHeader *XRoom;
	List *		queue;
	SInt32		dir;
	RNum		tmp_room = -1;
	SInt32		count = 0;
	RNum		here, there, start;
	RoomDirection *exit;
	RoomQueue *	QHead, *TempQ;
	BPElement *	element;
	
	if ((predicate)(room, data))
		return NULL;
	
	start = room;
	
	XRoom = new HashHeader(sizeof(int), 2047);
	XRoom->Enter(room, (Ptr)-1);
	
	queue = new List(NULL);
	
	QHead = new RoomQueue;
	QHead->room = room;
	
	queue->Append(&QHead->link);
	
	while ((QHead = (RoomQueue *)queue->Pop())) {
		here = QHead->room;
		for (dir = 0; dir < NUM_OF_DIRS; ++dir) {
			exit = world[here].dir_option[dir];
			if (exit && ((there = exit->to_room) != NOWHERE) &&
					!ROOM_FLAGGED(there, ROOM_DEATH) &&
					(IS_SET(flags, HUNT_GLOBAL) || (world[start].zone == world[there].zone)) &&
					(IS_SET(flags, HUNT_THRU_DOORS) ? GO_OK_SMARTER(exit) : GO_OK(exit)) &&
					!XRoom->Find(tmp_room = there)) {
				if ((predicate)(tmp_room, data)) {
					Path *	path;
					tmp_room = QHead->room;
					while ((QHead = (RoomQueue *)queue->Pop()))
						delete QHead;
					
					path = new Path();
					path->moves = new TrackStep[count + 1];
					
					path->dest = tmp_room;
					path->flags = flags;
					path->depth = depth;
					path->moves[0].dir = dir;
					path->moves[0].room = tmp_room;
					path->count = 1;
					
//					while ((element = (BPElement *)XRoom->Find(tmp_room)) != (Ptr) -1)
					while ((element = (BPElement *)XRoom->Find(tmp_room)) && (element != (Ptr)NOWHERE)) {
						path->moves[path->count].dir = element->dir;
						path->moves[path->count].room = element->room;
						path->count++;
						tmp_room = element->room;
					}
					DestroyHashHeader(XRoom, (DestroyHashFunc)KillBPElement);
					return path;
				} else if (++count < depth) {
					//	Put room on queue
					TempQ = new RoomQueue;
					TempQ->room = tmp_room;
					queue->Append(&TempQ->link);
					
					//	Mark room as visited
					element = new BPElement;
					element->room = QHead->room;
					element->dir = dir;
					XRoom->Enter(tmp_room, element);
				}
			}
		}
		
		delete QHead;
	}
	
	//	No path found
	DestroyHashHeader(XRoom, (DestroyHashFunc)KillBPElement);
	
	return NULL;
}


Path *PathRebuild(RNum start, Path *path) {
	Path *		new_path;
	TrackStep *	temp;
	
	if (!path || !path->victim)
		return NULL;
	
	new_path = Path2Char(start, path->victim, path->depth, path->flags);
	
	if (!new_path)
		return NULL;
	
	temp = path->moves;
	path->moves = new_path->moves;
	new_path->moves = temp;
	
	path->count = new_path->count;
	
	delete new_path;
	
	return path;
}


ACMD(do_path) {
	char *		name = get_buffer(MAX_INPUT_LENGTH);
	char *		directs = get_buffer(MAX_STRING_LENGTH);
	Path *		path = NULL;
	char *		dir;
	SInt32		next;
	
	one_argument(argument, name);
	
	if (!*name)
		send_to_char("Find path to whom?\r\n", ch);
	else if (*name == '.')
		path = Path2FullName(IN_ROOM(ch), name + 1, 200, HUNT_GLOBAL | HUNT_THRU_DOORS);
	else
		path = Path2Name(IN_ROOM(ch), name, 200, HUNT_THRU_DOORS);
		
	if (path) {
		dir = directs;
		for (next = path->count - 1; next >= 0; next--)
			*dir++ = dirs[path->moves[next].dir][0];
		*dir++ = '\r';
		*dir++ = '\n';
		*dir = '\0';
		
		ch->Send("Shortest route to %s:\r\n%s", path->victim->GetName(), directs);
	} else
		send_to_char("Can't find target.\r\n", ch);
	
	release_buffer(name);
	release_buffer(directs);
}


SInt32 Track(CharData *ch) {
	CharData *	vict;
	SInt32		code;

	if(!ch)			return -1;
	if(!ch->path)	return -1;

	vict = ch->path->victim;

	if (vict ? (IN_ROOM(ch) == IN_ROOM(vict)) : IN_ROOM(ch) == ch->path->dest) {
//		send_to_char("##You have found your target!\r\n", ch);
		delete ch->path;
		ch->path = NULL;
		return -1;
	}

	if((code = ch->path->Dir(IN_ROOM(ch))) == -1) {
//		send_to_char("##You have lost the trail.\r\n",ch);
		delete ch->path;
		ch->path = NULL;
		return -1;
	}

//	ch->Send("##You see a faint trail to the %s\r\n", dirs[code]);

	return code;
}


/* 
 * find_first_step: given a source room and a target room, find the first
 * step on the shortest path from the source to the target.
 *
 * Intended usage: in mobile_activity, give a mob a dir to go if they're
 * tracking another mob or a PC.  Or, a 'track' skill for PCs.
 */
int find_first_step(RNum src, char *target) {
	SInt32	dir = -1;
	Path *	path;
	
	if (src < 0 || src > top_of_world || !target) {
		log("Illegal value %d or %p passed to find_first_step(RNum, char *). (" __FILE__ ")", src, target);
		return dir;
	}

	path = Path2Name(src, target, 200, HUNT_GLOBAL | HUNT_THRU_DOORS);

	if (path) {
		dir = path->moves[path->count - 1].dir;
		delete path;
	}
	return dir;
}

int find_first_step(RNum src, RNum target) {
	SInt32	dir = -1;
	Path *	path;
	
	if (src < 0 || src > top_of_world || target < 0 || target > top_of_world) {
		log("Illegal value %d or %d passed to find_first_step(RNum, RNum). (" __FILE__ ")", src, target);
		return dir;
	}

	path = Path2Room(src, target, 200, HUNT_GLOBAL | HUNT_THRU_DOORS);

	if (path) {
		dir = path->moves[path->count - 1].dir;
		delete path;
	}
	return dir;
}
