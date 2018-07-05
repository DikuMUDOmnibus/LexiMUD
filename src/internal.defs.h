#define OPT_USEC		100000	/* 10 passes per second */
#define PASSES_PER_SEC	(1000000 / OPT_USEC)
#define RL_SEC			* PASSES_PER_SEC

#define PULSE_ZONE		(10 RL_SEC)
#define PULSE_MOBILE	(1 RL_SEC)
#define PULSE_POINTS	(25 RL_SEC)	//	WILL REMOVE
#define PULSE_VIOLENCE	(2 RL_SEC)	//	WILL REMOVE ?
#define PULSE_BUFFER	(5 RL_SEC)
#define PULSE_SCRIPT	(10 RL_SEC)
#define PULSE_EVENT		(1 RL_SEC)

#define TICK_WRAP_COUNT	10

#define MOBILE_PERCENT	20			//	Do 1 in 5 of specials per mobile pulse.

//	Variables for the output buffering system */
#define MAX_SOCK_BUF		(12 * 1024)		//	Size of kernel's sock buf
#define MAX_PROMPT_LENGTH	512				//	Max length of prompt
#define GARBAGE_SPACE		32				//	Space for **OVERFLOW** etc
#define SMALL_BUFSIZE		1024			//	Static output buffer size
//	Max amount of output that can be buffered
#define LARGE_BUFSIZE		(MAX_SOCK_BUF - GARBAGE_SPACE - MAX_PROMPT_LENGTH)

#define HISTORY_SIZE		5
#define MAX_STRING_LENGTH	8192
#define MAX_INPUT_LENGTH	256				//	Max length per *line* of input
#define MAX_RAW_INPUT_LENGTH	512			//	Max size of *raw* input
#define MAX_MESSAGES		60

//	0 = use new buffers for sockets.
//	1 = use old circle system for sockets.
#define USE_CIRCLE_SOCKET_BUF	1

#define SPECIAL(name)	int (name)(CharData *ch, Ptr me, char * cmd, char *argument)

/* misc editor defines **************************************************/

/* format modes for format_text */
#define FORMAT_INDENT		(1 << 0)
