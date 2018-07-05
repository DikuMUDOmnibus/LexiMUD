/***************************************************************************\
[*]    __     __  ___                ___  [*]   LexiMUD: A feature rich   [*]
[*]   / /  ___\ \/ (_) /\/\  /\ /\  /   \ [*]      C++ MUD codebase       [*]
[*]  / /  / _ \\  /| |/    \/ / \ \/ /\ / [*] All rights reserved         [*]
[*] / /__|  __//  \| / /\/\ \ \_/ / /_//  [*] Copyright(C) 1997-1998      [*]
[*] \____/\___/_/\_\_\/    \/\___/___,'   [*] Chris Jacobson (FearItself) [*]
[*]        LexiMUD: Feel the Power        [*] fear@technologist.com       [*]
[-]---------------------------------------+-+-----------------------------[-]
[*] File : comm.c++                                                       [*]
[*] Usage: Communication, socket handling, main(), central game loop      [*]
\***************************************************************************/

#define __COMM_C__

#include "types.h"


#include <stdarg.h>

#ifdef CIRCLE_WINDOWS				/* Includes for Win32 */
# include <direct.h>
# include <mmsystem.h>
#endif

#ifdef CIRCLE_MAC					/* includes for Mac  */
#undef SO_SNDBUF
  /* GUSI headers */
# define SIGPIPE 13
# define SIGALRM 14
# include <netdb.h>
# include <netinet/in.h>
# include <sys/errno.h>
# include <sys/ioctl.h>
# include <sys/socket.h>
  /* Codewarrior dependant */
# include <SIOUX.h>
# include <console.h>
//#include <Events.h>				//	For TickCount()
//#include <CursorCtl.h>			//	For SpinCursor()
extern pascal int TickCount(void)
 ONEWORDINLINE(0xA975);
void pascal SpinCursor(short increment);
char * inet_ntoa(struct in_addr inaddr);
#include <machine/endian.h>
#endif


#ifdef CIRCLE_UNIX
# include <arpa/inet.h>
# include <sys/errno.h>
# include <sys/ioctl.h>
# include <sys/socket.h>
# include <sys/resource.h>
# include <netinet/in.h>
# include <netdb.h>
# include <sys/wait.h>
#endif


#include "structs.h"
#include "db.h"
#include "utils.h"
#include "buffer.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "find.h"
#include "house.h"
#include "ban.h"
#include "olc.h"
#include "event.h"
#include "ident.h"

#include "imc.h"
#include "imc-mercbase.h"
#include "icec-mercbase.h"

#ifdef HAVE_ARPA_TELNET_H
#include <arpa/telnet.h>
#else
#include "telnet.h"
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif

/* externs */
extern char *GREETINGS;
extern int num_invalid;
extern SInt32 circle_restrict;
extern bool mini_mud;
bool no_rent_check = false;
extern int DFLT_PORT;
extern char *DFLT_DIR;
extern int MAX_PLAYERS;
extern int buffer_opt;

bool no_external_procs = false;

extern struct TimeInfoData time_info;		/* In db.c */
extern char help[];


/* local globals */
#if defined(USE_CIRCLE_SOCKET_BUF)
struct txt_block *bufpool = 0;	/* pool of large output buffers */
#endif
int buf_largecount = 0;		/* # of large buffers which exist */
int buf_overflows = 0;		/* # of overflows of output */
int buf_switches = 0;		/* # of switches from small to large buf */
int circle_shutdown = 0;	/* clean shutdown */
int circle_reboot = 0;		/* reboot the game after a shutdown */
int no_specials = 0;		/* Suppress ass. of special routines */
int max_players = 0;		/* max descriptors available */
int tics = 0;			/* for extern checkpointing */
int act_check;
int scheck = 0;			/* for syntax checking mode */
int MOBTrigger = TRUE;
extern int nameserver_is_slow;	/* see config.c */
extern int auto_save;		/* see config.c */
extern UInt32 autosave_time;	/* see config.c */
struct timeval null_time;	/* zero-valued time structure */
FILE *logfile = stderr;
extern char *LOGFILE;

static bool		fCopyOver;			//	Are we booting in copyover mode?
SInt32			mother_desc;		//	Now a global
SInt32			port;

IdentServer	*	Ident;

/* functions in this file */
bool get_from_q(struct txt_q *queue, char *dest, int *aliased);
void init_game(int port);
void signal_setup(void);
void game_loop(int mother_desc);
int init_socket(int port);
int new_descriptor(int s);
int get_max_players(void);
int process_output(DescriptorData *t);
int process_input(DescriptorData *t);
void close_socket(DescriptorData *d);
struct timeval timediff(struct timeval a, struct timeval b);
struct timeval timeadd(struct timeval a, struct timeval b);
void flush_queues(DescriptorData *d);
void nonblock(socket_t s);
int perform_subst(DescriptorData *t, char *orig, char *subst);
int perform_alias(DescriptorData *d, char *orig);
void record_usage(void);
char *make_prompt(DescriptorData *point);
void check_idle_passwords(void);
void heartbeat(int pulse);
void copyover_recover(void);
int set_sendbuf(socket_t s);
void sub_write_to_char(CharData *ch, char *tokens[], Ptr otokens[], char type[]);
void proc_color(char *inbuf, int color);
char *prompt_str(CharData *ch);
void	perform_act(const char *orig, CharData *ch, ObjData *obj,
		    Ptr vict_obj, CharData *to);


RETSIGTYPE checkpointing(int);
RETSIGTYPE reread_wizlists(int);
RETSIGTYPE unrestrict_game(int);
RETSIGTYPE hupsig(int);
RETSIGTYPE reap(int sig);

#if defined(CIRCLE_MAC) || defined(CIRCLE_WINDOWS)
void gettimeofday(struct timeval *t, struct timezone *dummy);
#endif

/* extern fcnts */
void reboot_wizlists(void);
void boot_db(void);
void boot_world(void);
void zone_update(void);
void point_update(void);		//	In limits.c
void run_events(void);			//	In events.c
void hour_update(void);			//	In limits.c
void free_purged_lists();		//	In handler.c
void check_mobile_activity(UInt32 pulse);
void string_add(DescriptorData *d, char *str);
void perform_violence(void);
void show_string(DescriptorData *d, char *input);
void weather_and_time(int mode);
extern void mprog_act_trigger(char *buf, CharData *mob, CharData *ch, ObjData *obj, Ptr vo);
void act_mtrigger(CharData *ch, char *str, CharData *actor, 
		CharData *victim, ObjData *object, ObjData *target, char *arg);
void script_trigger_check(void);


void oedit_save_to_disk(int zone_num);
void medit_save_to_disk(int zone_num);
void sedit_save_to_disk(int zone_num);
void zedit_save_to_disk(int zone_num);
void hedit_save_to_disk(int zone_num);
void aedit_save_to_disk(int zone_num);


#ifdef CIRCLE_MAC

#ifdef __CXREF__
#undef FD_ZERO
#undef FD_SET
#undef FD_ISSET
#undef FD_CLR
#define FD_ZERO(x)
#define FD_SET(x, y) 0
#define FD_ISSET(x, y) 0
#define FD_CLR(x, y)
#endif

#define isascii(c)		((unsigned)(c) <= 0177)

#endif

__BEGIN_DECLS
void GUSIwithAppleTalkSockets();
void GUSIwithInternetSockets();
void GUSIwithPAPSockets();
void GUSIwithPPCSockets();
void GUSIwithUnixSockets();
void GUSIwithSIOUXSockets();
void GUSIwithMPWSockets();

void GUSISetup(void (*socketfamily)());
void GUSIDefaultSetup();
__END_DECLS


UInt32 pulse;


/********************************************************************
*	main game loop and related stuff								*
*********************************************************************/

/* Windows doesn't have gettimeofday, so we'll simulate it. */
#ifdef CIRCLE_WINDOWS
void gettimeofday(struct timeval *t, struct timezone *dummy)
{
  DWORD millisec = GetTickCount();

  t->tv_sec = (int) (millisec / 1000);
  t->tv_usec = (millisec % 1000) * 1000;
}

/* The Mac doesn't have gettimeofday either. */
#elif defined(CIRCLE_MAC)
void gettimeofday(struct timeval *t, struct timezone *dummy)
{
  UInt32 millisec;
  /*
	I think this is supposed to be 
	(int)(TickCount() / (1000 * TICKS_PER_SEC) or something)
   */
  millisec = (int)((float)TickCount() * 1000.0 / 60.0);

  t->tv_sec = (int)(millisec / 1000);
  t->tv_usec = (millisec % 1000) * 1000;
}
#endif


int enter_player_game(DescriptorData *d);

/* Reload players after a copyover */
void copyover_recover(void) {
	DescriptorData *d;
	FILE *fp;
	char *host = get_buffer(1024);
	int desc, player_i;
	int fOld;
	char *name = get_buffer(MAX_INPUT_LENGTH);

	log ("Copyover recovery initiated");

	if (!(fp = fopen (COPYOVER_FILE, "r"))) { /* there are some descriptors open which will hang forever then ? */
		perror ("copyover_recover:fopen");
		log ("Copyover file not found. Exitting.\r\n");
		exit (1);
	}

	unlink (COPYOVER_FILE); /* In case something crashes - doesn't prevent reading  */

	for (;;) {
		fOld = TRUE;
		fscanf (fp, "%d %s %s\n", &desc, name, host);
		if (desc == -1)
			break;

		/* Write something, and check if it goes error-free */
		if (write_to_descriptor (desc, "\r\nRestoring from copyover...\r\n") < 0) {
			close (desc); /* nope */
			continue;
		}

		d = new DescriptorData(desc);
		
		strcpy(d->host, host);
		descriptor_list.Add(d);
	
		STATE(d) = CON_CLOSE;

		/* Now, find the pfile */

		d->character = new CharData();
		d->character->desc = d;

		if ((player_i = d->character->load(name)) >= 0) {
			GET_PFILEPOS(d->character) = player_i;
			if (!PLR_FLAGGED(d->character, PLR_DELETED))
				REMOVE_BIT(PLR_FLAGS(d->character),PLR_WRITING | PLR_MAILING);
			else
				fOld = FALSE;
		} else
			fOld = FALSE;

		if (!fOld) { /* Player file not found?! */
			write_to_descriptor (desc, "\r\nSomehow, your character was lost in the copyover. Sorry.\r\n");
			close_socket (d);
		} else { /* ok! */
			write_to_descriptor (desc, "\r\nCopyover recovery complete.\r\n");
			enter_player_game(d);
			STATE(d) = CON_PLAYING;
			look_at_room(d->character, 0);
		}
//		save_char(d->character, NOWHERE);
	}

	fclose (fp);
	release_buffer(name);
	release_buffer(host);
}



int main(int argc, char **argv) {
	int pos = 1;
	char *dir;

#ifdef CIRCLE_MAC
	GUSIDefaultSetup();
	
	argc = ccommand(&argv);
#endif

	init_buffers();

	port = DFLT_PORT;
	dir = DFLT_DIR;

	/* Be nice to make this a command line option but the parser uses log() */
	if (*LOGFILE != '\0' && !(logfile = fopen(LOGFILE, "w"))) {
		fprintf(stdout, "Error opening log file!\n");
		exit(1);
	}

	while ((pos < argc) && (*(argv[pos]) == '-')) {
		switch (*(argv[pos] + 1)) {
			case 'C': /* -C<socket number> - recover from copyover, this is the control socket */
				fCopyOver = TRUE;
				mother_desc = atoi(argv[pos]+2);
				no_rent_check = true;
				break;
			case 'd':
				if (*(argv[pos] + 2))		dir = argv[pos] + 2;
				else if (++pos < argc)		dir = argv[pos];
				else {
					log("SYSERR: Directory arg expected after option -d.");
					exit(1);
				}
				break;
			case 'm':
				mini_mud = true;
				no_rent_check = true;
				log("Running in minimized mode & with no rent check.");
				break;
			case 'c':
				scheck = 1;
				log("Syntax check mode enabled.");
				break;
			case 'q':
				no_rent_check = true;
				log("Quick boot mode -- rent check supressed.");
				break;
			case 'r':
				circle_restrict = 1;
				log("Restricting game -- no new players allowed.");
				break;
			case 's':
				no_specials = 1;
				log("Suppressing assignment of special routines.");
				break;
			case 'v':
				if (*(argv[pos] + 2))		buffer_opt = atoi(argv[pos] + 2);
				else if (++pos < argc)		buffer_opt = atoi(argv[pos]);
				else {
					log("SYSERR: Number expected after option -v.");
					exit(1);
				}
			case 'x':
				no_external_procs = true;
				break;
			default:
				log("SYSERR: Unknown option -%c in argument string.", *(argv[pos] + 1));
				break;
		}
	pos++;
	}

	if (pos < argc) {
		if (!isdigit(*argv[pos])) {
			log("Usage: %s [-c] [-m] [-q] [-r] [-s] [-x] [-v val] [-d pathname] [port #]", argv[0]);
			exit(1);
		} else if ((port = atoi(argv[pos])) <= 1024) {
			log("SYSERR: Illegal port number.");
			exit(1);
		}
	}
	if (chdir(dir) < 0) {
		perror("SYSERR: Fatal error changing to data directory");
		exit(1);
	}
	log("Using %s as data directory.", dir);

	Ident = new IdentServer();
	
	if (scheck) {
		boot_world();
		log("Done.");
		exit(0);
	} else {
#if defined(CIRCLE_UNIX)
		pid_t	pid = 0;
		if (!no_external_procs && ((pid = fork()) < 0)) {
			log("SYSERR: Unable to start MUD Server!");
		} else if (pid > 0) {
			log("Starting MUD Server (%d)", pid);
			waitpid(pid, NULL, 0);
			log("Closing MUDServer");
		} else
#endif
		{
			log("Running game on port %d.", port);
			init_game(port);
#if defined(CIRCLE_UNIX)
			if (!no_external_procs)
				exit(0);
#endif
		}
	}
	
	log("Shutting down Ident Server");
	delete Ident;
	
	exit_buffers();

	return 0;
}


//	Init sockets, run game, and cleanup sockets
void init_game(int port) {
	circle_srandom(time(0));

	log("Finding player limit.");
	max_players = get_max_players();

	if (!fCopyOver) {	//	If copyover mother_desc is already set up
		log("Opening mother connection.");
		mother_desc = init_socket(port);
	}
	
	InitEvents();
	
	boot_db();

	log("Signal trapping.");
	signal_setup();

	imc_startup(IMC_DIR);
	icec_init();
	
	if (fCopyOver) /* reload players */
		copyover_recover();

	log("Entering game loop.");

	game_loop(mother_desc);

	imc_shutdown();

	log("Closing all sockets.");
	while (descriptor_list.Top())
		close_socket(descriptor_list.Top());

	CLOSE_SOCKET(mother_desc);
	
/*
	if (circle_reboot != 2 && olc_save_list) { // Don't save zones.
		struct olc_save_info *entry, *next_entry;
		for (entry = olc_save_list; entry; entry = next_entry) {
			next_entry = entry->next;
			if (entry->type < 0 || entry->type > 4)
				log("OLC: Illegal save type %d!", entry->type);
			else if (entry->zone < 0)
				log("OLC: Illegal save zone %d!", entry->zone);
			else {
				log("OLC: Reboot saving %s for zone %d.",
						save_info_msg[(int)entry->type], entry->zone);
				switch (entry->type) {
					case OLC_SAVE_ROOM:		REdit::save_to_disk(entry->zone); break;
					case OLC_SAVE_OBJ:		oedit_save_to_disk(entry->zone); break;
					case OLC_SAVE_MOB:		medit_save_to_disk(entry->zone); break;
					case OLC_SAVE_ZONE:		zedit_save_to_disk(entry->zone); break;
					case OLC_SAVE_SHOP:		sedit_save_to_disk(entry->zone); break;
					case OLC_SAVE_ACTION:	aedit_save_to_disk(entry->zone); break;
					case OLC_SAVE_HELP:		hedit_save_to_disk(entry->zone); break;
					default:				log("Unexpected olc_save_list->type"); break;
				}
			}
		}
	}
*/

	if (circle_reboot) {
		log("Rebooting.");
		exit(52);			/* what's so great about HHGTTG, anyhow? */
	}
	log("Normal termination of game.");
}



/*
 * init_socket sets up the mother descriptor - creates the socket, sets
 * its options up, binds it, and listens.
 */
int init_socket(int port) {
	int s;
#ifndef CIRCLE_MAC
	int opt;
#endif
	struct sockaddr_in sa;

  /*
   * Should the first argument to socket() be AF_INET or PF_INET?  I don't
   * know, take your pick.  PF_INET seems to be more widely adopted, and
   * Comer (_Internetworking with TCP/IP_) even makes a point to say that
   * people erroneously use AF_INET with socket() when they should be using
   * PF_INET.  However, the man pages of some systems indicate that AF_INET
   * is correct; some such as ConvexOS even say that you can use either one.
   * All implementations I've seen define AF_INET and PF_INET to be the same
   * number anyway, so ths point is (hopefully) moot.
   */

#ifdef CIRCLE_WINDOWS
	{
		WORD wVersionRequested;
		WSADATA wsaData;

		wVersionRequested = MAKEWORD(1, 1);

		if (WSAStartup(wVersionRequested, &wsaData) != 0) {
			log("WinSock not available!");
			exit(1);
		}
		if ((wsaData.iMaxSockets - 4) < max_players) {
			max_players = wsaData.iMaxSockets - 4;
		}
		log("Max players set to %d", max_players);

		if ((s = socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
			log("Error opening network connection: Winsock err #%d", WSAGetLastError());
			exit(1);
		}
	}
#else
	if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Error creating socket");
		exit(1);
	}
#endif				/* CIRCLE_WINDOWS */

#ifndef CIRCLE_MAC
/* Mac's GUSI has no setsockopt() */

# if defined(SO_REUSEADDR)
	opt = 1;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(opt)) < 0) {
		perror("SYSERR: setsockopt REUSEADDR");
		exit(1);
	}
	
# endif

	set_sendbuf(s);

# if defined(SO_LINGER)
	{
		struct linger ld;

		ld.l_onoff = 0;
		ld.l_linger = 0;
		if (setsockopt(s, SOL_SOCKET, SO_LINGER, (char *) &ld, sizeof(ld)) < 0) {
			perror("SYSERR: setsockopt LINGER");
			exit(1);
		}
	}
# endif

#endif /* CIRCLE_MAC */

	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
#if defined(CIRCLE_MAC)
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
#else
	sa.sin_addr.s_addr = inet_addr("209.123.14.5");
#endif

	if (bind(s, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
		perror("SYSERR: bind");
		CLOSE_SOCKET(s);
		exit(1);
	}
	nonblock(s);
	listen(s, 5);
	return s;
}


int get_max_players(void) {
#if defined(CIRCLE_OS2) || defined(CIRCLE_WINDOWS)
	return MAX_PLAYERS;
#else
	int max_descs = 0;
	char *method;

/*
 * First, we'll try using getrlimit/setrlimit.  This will probably work
 * on most systems.
 */
#if defined (RLIMIT_NOFILE) || defined (RLIMIT_OFILE)
#if !defined(RLIMIT_NOFILE)
#define RLIMIT_NOFILE RLIMIT_OFILE
#endif
	{
		struct rlimit limit;

		/* find the limit of file descs */
		method = "rlimit";
		if (getrlimit(RLIMIT_NOFILE, &limit) < 0) {
			perror("SYSERR: calling getrlimit");
			exit(1);
		}
		/* set the current to the maximum */
		limit.rlim_cur = limit.rlim_max;
		if (setrlimit(RLIMIT_NOFILE, &limit) < 0) {
			perror("SYSERR: calling setrlimit");
			exit(1);
		}
#ifdef RLIM_INFINITY
		if (limit.rlim_max == RLIM_INFINITY)
			max_descs = MAX_PLAYERS + NUM_RESERVED_DESCS;
		else
			max_descs = MIN(MAX_PLAYERS + NUM_RESERVED_DESCS, limit.rlim_max);
#else
		max_descs = MIN(MAX_PLAYERS + NUM_RESERVED_DESCS, limit.rlim_max);
#endif
	}

#elif defined (OPEN_MAX) || defined(FOPEN_MAX)
#if !defined(OPEN_MAX)
#define OPEN_MAX FOPEN_MAX
#endif
	method = "OPEN_MAX";
	max_descs = OPEN_MAX;		/* Uh oh.. rlimit didn't work, but we have
								 * OPEN_MAX */
#elif defined (POSIX)
	/*
	 * Okay, you don't have getrlimit() and you don't have OPEN_MAX.  Time to
	 * use the POSIX sysconf() function.  (See Stevens' _Advanced Programming
	 * in the UNIX Environment_).
	 */
	method = "POSIX sysconf";
	errno = 0;
	if ((max_descs = sysconf(_SC_OPEN_MAX)) < 0) {
		if (errno == 0)
			max_descs = MAX_PLAYERS + NUM_RESERVED_DESCS;
		else {
			perror("SYSERR: Error calling sysconf");
			exit(1);
		}
	}
#else
	/* if everything has failed, we'll just take a guess */
	max_descs = MAX_PLAYERS + NUM_RESERVED_DESCS;
#endif

	/* now calculate max _players_ based on max descs */
	max_descs = MIN(MAX_PLAYERS, max_descs - NUM_RESERVED_DESCS);

	if (max_descs <= 0) {
		log("SYSERR: Non-positive max player limit!  (Set at %d using %s).", max_descs, method);
		exit(1);
	}
	log("Setting player limit to %d using %s.", max_descs, method);
	return max_descs;
#endif				/* WINDOWS or OS2 */
}



/*
 * game_loop contains the main loop which drives the entire MUD.  It
 * cycles once every 0.10 seconds and is responsible for accepting new
 * new connections, polling existing connections for input, dequeueing
 * output and sending it out to players, and calling "heartbeat" functions
 * such as mobile_activity().
 */
void game_loop(int mother_desc) {
	fd_set input_set, output_set, exc_set, null_set;
	struct timeval last_time, before_sleep, opt_time, process_time, now, timeout;
	char *comm;
	DescriptorData *d;
	int missed_pulses, maxdesc, aliased;
  
	/* initialize various time values */
	null_time.tv_sec = 0;
	null_time.tv_usec = 0;
	opt_time.tv_usec = OPT_USEC;
	opt_time.tv_sec = 0;
	FD_ZERO(&null_set);

	gettimeofday(&last_time, (struct timezone *) 0);
	
	LListIterator<DescriptorData *>	iter(descriptor_list);

  /* The Main Loop.  The Big Cheese.  The Top Dog.  The Head Honcho.  The.. */
	while (!circle_shutdown) {
#ifdef CIRCLE_MAC
		SpinCursor(0);
#endif
/* Sleep if we don't have any connections */
		if (descriptor_list.Count() == 0) {
//			log("No connections.  Going to sleep.");
//			Removed to prevent log spam, because of IMC
			FD_ZERO(&input_set);
			FD_ZERO(&output_set);	//	IMC
			FD_ZERO(&exc_set);		//	IMC
			FD_SET(mother_desc, &input_set);
			
			maxdesc = imc_fill_fdsets(mother_desc, &input_set, &output_set, &exc_set);
			
			if (select(mother_desc + 1, &input_set, &output_set, &exc_set, NULL) < 0) {
				if (errno == EINTR)	log("Waking up to process signal.");
				else				perror("Select coma");
			}// else
			//	log("New connection.  Waking up.");
			gettimeofday(&last_time, (struct timezone *) 0);
		}
/* Set up the input, output, and exception sets for select(). */
		FD_ZERO(&input_set);
		FD_ZERO(&output_set);
		FD_ZERO(&exc_set);
		FD_SET(mother_desc, &input_set);

		maxdesc = mother_desc;		

#if 1	//	ASYNCH
		if (Ident->IsActive()) {
			FD_SET(Ident->descriptor, &input_set);
			maxdesc = MAX(maxdesc, Ident->descriptor);
		}
#endif
		
		iter.Reset();
		while ((d = iter.Next())) {
#ifndef CIRCLE_WINDOWS
			if (d->descriptor > maxdesc)
				maxdesc = d->descriptor;
#endif
			FD_SET(d->descriptor, &input_set);
			FD_SET(d->descriptor, &output_set);
			FD_SET(d->descriptor, &exc_set);
		}
		
		maxdesc = imc_fill_fdsets(maxdesc, &input_set, &output_set, &exc_set);
		
		/*
		 * At this point, we have completed all input, output and heartbeat
		 * activity from the previous iteration, so we have to put ourselves
		 * to sleep until the next 0.1 second tick.  The first step is to
		 * calculate how long we took processing the previous iteration.
		 */

		gettimeofday(&before_sleep, (struct timezone *) 0); /* current time */
		process_time = timediff(before_sleep, last_time);

		/*
		 * If we were asleep for more than one pass, count missed pulses and sleep
		 * until we're resynchronized with the next upcoming pulse.
		 */
		if (process_time.tv_sec == 0 && process_time.tv_usec < OPT_USEC) {
			missed_pulses = 0;
		} else {
			missed_pulses = process_time.tv_sec RL_SEC;
			missed_pulses += process_time.tv_usec / OPT_USEC;
			process_time.tv_sec = 0;
			process_time.tv_usec = process_time.tv_usec % OPT_USEC;
		}

		/* Calculate the time we should wake up */
		last_time = timeadd(before_sleep, timediff(opt_time, process_time));

		/* Now keep sleeping until that time has come */
		gettimeofday(&now, (struct timezone *) 0);
		timeout = timediff(last_time, now);

		/* Go to sleep */
		do {
#ifdef CIRCLE_WINDOWS
			Sleep(timeout.tv_sec * 1000 + timeout.tv_usec / 1000);
#else
			if (select(0, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, &timeout) < 0) {
				if (errno != EINTR) {
					perror("SYSERR: Select sleep");
					exit(1);
				}
			}
#endif /* CIRCLE_WINDOWS */
			gettimeofday(&now, (struct timezone *) 0);
			timeout = timediff(last_time, now);
		} while (timeout.tv_usec || timeout.tv_sec);

		/* Poll (without blocking) for new input, output, and exceptions */
		if (select(maxdesc + 1, &input_set, &output_set, &exc_set, &null_time) < 0) {
			perror("Select poll");
			return;
		}
		
		//	Accept new connections
		if (FD_ISSET(mother_desc, &input_set))
			new_descriptor(mother_desc);
		

#if 1	//	ASYNCH
		//	Check for completed lookups
		if (Ident && FD_ISSET(Ident->descriptor, &input_set))
			Ident->Receive();
#endif
		
		//	Kick out the freaky folks in the exception set
		iter.Reset();
		while ((d = iter.Next())) {
			if (FD_ISSET(d->descriptor, &exc_set)) {
				FD_CLR(d->descriptor, &input_set);
				FD_CLR(d->descriptor, &output_set);
				close_socket(d);
			}
		}
		
		//	Process descriptors with input pending
		iter.Reset();
		while ((d = iter.Next())) {
			if (FD_ISSET(d->descriptor, &input_set))
				if (process_input(d) < 0)
					close_socket(d);
		}
		
		/* Process commands we just read from process_input */
		comm = get_buffer(MAX_STRING_LENGTH);
		iter.Reset();
		while ((d = iter.Next())) {
			d->wait -= (d->wait > 0);
			if (d->wait > 0)							continue;
			if (!get_from_q(&d->input, comm, &aliased))	continue;
			if (d->character) {
			//	Reset the idle timer & pull char back from void if necessary
				d->character->player->timer = 0;
				if ((STATE(d) == CON_PLAYING) && (GET_WAS_IN(d->character) != NOWHERE)) {
					if (IN_ROOM(d->character) != NOWHERE)
						d->character->from_room();
					d->character->to_room(GET_WAS_IN(d->character));
					GET_WAS_IN(d->character) = NOWHERE;
					act("$n has returned.", TRUE, d->character, 0, 0, TO_ROOM);
				}
			}
			d->wait = 1;
			d->has_prompt = 0;

			if (d->showstr_count)				show_string(d, comm);	//	Reading something w/ pager
			else if (d->str)					string_add(d, comm);	//	Writing boards, mail, etc.
			else if (STATE(d) != CON_PLAYING)	nanny(d, comm);			//	in menus, etc.
			else {														//	We're playing normally
				if (aliased)	d->has_prompt = 1;			//	to prevent recursive aliases
				else if (perform_alias(d, comm))			//	run it through aliasing system
					get_from_q(&d->input, comm, &aliased);
				command_interpreter(d->character, comm);	//	send it to interpreter
			}
		}
		release_buffer(comm);
		
		//	send queued output out to the operating system (ultimately to user)
		iter.Reset();
		while ((d = iter.Next())) {
			if (*(d->output) && FD_ISSET(d->descriptor, &output_set)) {
				/* Output for this player is ready */
				if (process_output(d) < 0)	close_socket(d);
				else						d->has_prompt = 1;
			}
		}
		
		//	Print prompts for other descriptors who had no other output
		//	Merged these together, also.  Seems safe to also do the ELSE, because
		//	if the previous IF is called, either d would be removed from list,
		//	or d->has_prompt would be set, therefore would be skipped during
		//	prompt making
		iter.Reset();
		while ((d = iter.Next())) {
			if (!d->has_prompt && (STATE(d) != CON_DISCONNECT)) {
				write_to_descriptor(d->descriptor, make_prompt(d));
				d->has_prompt = 1;
			}
		}

		/* kick out folks in the CON_CLOSE state */
		iter.Reset();
		while ((d = iter.Next())) {
			if ((STATE(d) == CON_CLOSE) || (STATE(d) == CON_DISCONNECT))
				close_socket(d);
		}

		//	Now, we execute as many pulses as necessary--just one if we haven't
		//	missed any pulses, or make up for lost time if we missed a few
		//	pulses by sleeping for too long.
		missed_pulses++;

		if (missed_pulses <= 0) {
			log("SYSERR: **BAD** MISSED_PULSES IS NONPOSITIVE (%d), TIME GOING BACKWARDS!!!", missed_pulses / PASSES_PER_SEC);
			missed_pulses = 1;
		}

		/* If we missed more than 30 seconds worth of pulses, forget it */
		if (missed_pulses > (30 RL_SEC)) {
			log("SYSERR: Missed more than 30 seconds worth of pulses");
			missed_pulses = 30 RL_SEC;
		}

		imc_idle_select(&input_set, &output_set, &exc_set, last_time.tv_sec);

		/* Now execute the heartbeat functions */
		while (missed_pulses--)		heartbeat(++pulse);

#ifdef CIRCLE_UNIX
		/* Update tics for deadlock protection (UNIX only) */
		tics++;
#endif
	}
}


void heartbeat(int pulse) {
	static UInt32 mins_since_crashsave = 0;

	ProcessEvents();
	
	if (!(pulse % PULSE_ZONE))					zone_update();
	if (!(pulse % (15 RL_SEC)))					check_idle_passwords();		/* 15 seconds */
	if (!(pulse % PULSE_MOBILE))				check_mobile_activity(pulse);
	if (!(pulse % PULSE_VIOLENCE))				perform_violence();
	if (!(pulse % PULSE_EVENT))					run_events();	
	if (!(pulse % PULSE_SCRIPT))				script_trigger_check();
	
	if (!(pulse % (SECS_PER_MUD_HOUR * PASSES_PER_SEC))) {
		weather_and_time(1);
		hour_update();
		free_purged_lists();
	}
	if (!(pulse % PULSE_POINTS))				point_update();
	/* Clear out all the global buffers now in case someone forgot. */
	if (!(pulse % PULSE_BUFFER))				release_all_buffers();
	
	if (auto_save && !(pulse % (60 RL_SEC))) {	/* 1 minute */
		if (++mins_since_crashsave >= autosave_time) {
			mins_since_crashsave = 0;
			Crash_save_all();
			House_save_all();
//			clans_save_to_disk();
		}
	}
	
	if (!(pulse % (5 * 60 RL_SEC)))	record_usage();				/* 5 minutes */
}


/********************************************************************
*	general utility stuff (for local use)							*
*********************************************************************/

/*
 *	new code to calculate time differences, which works on systems
 *	for which tv_usec is unsigned (and thus comparisons for something
 *	being < 0 fail).  Based on code submitted by ss@sirocco.cup.hp.com.
 */

/*
 * code to return the time difference between a and b (a-b).
 * always returns a nonnegative value (floors at 0).
 */
struct timeval timediff(struct timeval a, struct timeval b) {
	struct timeval rslt;

	if (a.tv_sec < b.tv_sec)
		return null_time;
	else if (a.tv_sec == b.tv_sec) {
		if (a.tv_usec < b.tv_usec)
			return null_time;
		else {
			rslt.tv_sec = 0;
			rslt.tv_usec = a.tv_usec - b.tv_usec;
			return rslt;
		}
	} else {			/* a->tv_sec > b->tv_sec */
		rslt.tv_sec = a.tv_sec - b.tv_sec;
		if (a.tv_usec < b.tv_usec) {
			rslt.tv_usec = a.tv_usec + 1000000 - b.tv_usec;
			rslt.tv_sec--;
		} else
			rslt.tv_usec = a.tv_usec - b.tv_usec;
		return rslt;
	}
}


//	add 2 timevals
struct timeval timeadd(struct timeval a, struct timeval b) {
	struct timeval rslt;

	rslt.tv_sec = a.tv_sec + b.tv_sec;
	rslt.tv_usec = a.tv_usec + b.tv_usec;

	while (rslt.tv_usec >= 1000000) {
		rslt.tv_usec -= 1000000;
		rslt.tv_sec++;
	}

	return rslt;
}


void record_usage(void) {
	int sockets_playing = 0;
	DescriptorData *d;
	LListIterator<DescriptorData *>	iter(descriptor_list);
	
	while ((d = iter.Next()))
		if (STATE(d) == CON_PLAYING)
			sockets_playing++;

	log("nusage: %-3d sockets connected, %-3d sockets playing",
			descriptor_list.Count(), sockets_playing);

#ifdef RUSAGE
	{
		struct rusage ru;

		getrusage(0, &ru);
		log("rusage: user time: %d sec, system time: %d sec, max res size: %d",
				ru.ru_utime.tv_sec, ru.ru_stime.tv_sec, ru.ru_maxrss);
	}
#endif
}


//	Turn off echoing (specific to telnet client)
void echo_off(DescriptorData *d) {
	char off_string[] = {
		(char) IAC,
		(char) WILL,
		(char) TELOPT_ECHO,
		(char) 0,
	};

	SEND_TO_Q(off_string, d);
}


//	Turn on echoing (specific to telnet client)
void echo_on(DescriptorData *d) {
	char on_string[] = {
		(char) IAC,
		(char) WONT,
		(char) TELOPT_ECHO,
		(char) TELOPT_NAOFFD,
		(char) TELOPT_NAOCRD,
		(char) 0,
	};

	SEND_TO_Q(on_string, d);
}


char *prompt_str(CharData *ch) {
	CharData *vict = FIGHTING(ch);  
	static char pbuf[MAX_STRING_LENGTH];  
	char *str = GET_PROMPT(ch);
	CharData *tank;
	int perc;  
	char *cp, *tmp;
	char *i;
  
	if (!str || !*str)
		str = "`YAvP`K: `cSet your prompt (see `K'`Chelp prompt`K'`c)`W> `n";

	if (!strchr(str, '%'))
		return (str);
  
	i = get_buffer(256);
  
	cp = pbuf;
  
	for (;;) {
		if (*str == '%') {
			switch (*(++str)) {
				case 'h': // current hitp
					sprintf(i, "%d", GET_HIT(ch));
					tmp = i;
					break;
				case 'H': // maximum hitp
					sprintf(i, "%d", GET_MAX_HIT(ch));
					tmp = i;
					break;
//				case 'm': // current mana
//					sprintf(i, "%d", GET_MANA(ch));
//					tmp = i;
//					break;
//				case 'M': // maximum mana
//					sprintf(i, "%d", GET_MAX_MANA(ch));
//					tmp = i;
//					break;
				case 'v': // current moves
					sprintf(i, "%d", GET_MOVE(ch));
					tmp = i;
					break;
				case 'V': // maximum moves
					sprintf(i, "%d", GET_MAX_MOVE(ch));
					tmp = i;
					break;
				case 'P':
					case 'p': // percentage of hitp/move/mana
					str++;
					switch (LOWER(*str)) {
						case 'h':
							perc = (100 * GET_HIT(ch)) / GET_MAX_HIT(ch);
							break;
//						case 'm':
//							perc = (100 * GET_MANA(ch)) / GET_MAX_MANA(ch);
//							break;
						case 'v':
							perc = (100 * GET_MOVE(ch)) / GET_MAX_MOVE(ch);
							break;
//						case 'x':
//							perc = (100 * GET_EXP(ch)) / titles[(int)GET_CLASS(ch)][GET_LEVEL(ch)+1].exp;
//							break;
						default :
							perc = 0;
							break;
					}
					sprintf(i, "%d%%", perc);
					tmp = i;
					break;
				case 'O':
				case 'o': // opponent
					if (vict) {
						perc = (100*GET_HIT(vict)) / GET_MAX_HIT(vict);
						sprintf(i, "%s `K(`r%s`K)`n", PERS(vict, ch),
								(perc >= 95 ?	"unscathed"	:
								perc >= 75 ?	"scratched"	:
								perc >= 50 ?	"beaten-up"	:
								perc >= 25 ?	"bloody"	:
												"near death"));
						tmp = i;
					} else {
						str++;
						continue;
					}
					break;
//				case 'x': // current exp
//					sprintf(i, "%d", GET_EXP(ch));
//					tmp = i;
//					break;
//				case 'X': // exp to level
//					sprintf(i, "%d", titles[(int)GET_CLASS(ch)][GET_LEVEL(ch)+1].exp - GET_EXP(ch));
//					tmp = i;
//					break;
//				case 'g': // gold on hand
//					sprintf(i, "%d", GET_GOLD(ch));
//					tmp = i;
//					break;
//				case 'G': // gold in bank
//					sprintf(i, "%d", GET_BANK_GOLD(ch));
//					tmp = i;
//					break;
				case 'T':
				case 't': // tank
					if (vict && (tank = FIGHTING(vict)) && tank != ch) {
						perc = (100*GET_HIT(tank)) / GET_MAX_HIT(tank);
						sprintf(i, "%s `K(`r%s`K)`n", PERS(tank, ch),
								(perc >= 95 ?	"unscathed"	:
								perc >= 75 ?	"scratched"	:
								perc >= 50 ?	"beaten-up"	:
								perc >= 25 ?	"bloody"	:
												"near death"));
						tmp = i;
					} else {
						str++;
						continue;
					}
					break;
				case '_':
					tmp = "\r\n";
					break;
				case '%':
					*(cp++) = '%';
					str++;
					continue;
				default :
					str++;
					continue;
			}

			while ((*cp = *(tmp++)))
				cp++;
			str++;
		} else if (!(*(cp++) = *(str++)))
			break;
	}
  
	*cp = '\0';
  
	strcat(pbuf, " `n");
	release_buffer(i);
	return (pbuf);
}


char *make_prompt(DescriptorData *d) {
	static char prompt[512];
	//	Note, prompt is truncated at MAX_PROMPT_LENGTH chars (structs.h )
	
	//	reversed these top 2 if checks so that page_string() would work in
	//	the editor */
	if (d->showstr_count) {
		sprintf(prompt, "\r[ Return to continue, (q)uit, (r)efresh, (b)ack, or page number (%d/%d) ]",
				d->showstr_page, d->showstr_count);
//		write_to_descriptor(d->descriptor, prompt);
	} else if (d->str)
		strcpy(prompt, "] ");
	else if (STATE(d) == CON_PLAYING) {
		*prompt = '\0';

		if (GET_INVIS_LEV(d->character))
			sprintf(prompt, "`gi%d`n ", GET_INVIS_LEV(d->character));

		if (!IS_NPC(d->character) && GET_AFK(d->character))
			strcat(prompt, "`C[AFK]`n");
		strcat(prompt, prompt_str(d->character));
		if(d->character)
			proc_color(prompt, PRF_FLAGGED(d->character, PRF_COLOR)); 
	} else
		*prompt = '\0';
	
	return prompt;
}


void write_to_q(char *txt, struct txt_q *queue, int aliased) {
	struct txt_block *new_text;

	CREATE(new_text, struct txt_block, 1);
	new_text->text = str_dup(txt);
	new_text->aliased = aliased;

	//	queue empty?
	if (!queue->head) {
		new_text->next = NULL;
		queue->head = queue->tail = new_text;
	} else {
		queue->tail->next = new_text;
		queue->tail = new_text;
		new_text->next = NULL;
	}
}



bool get_from_q(struct txt_q *queue, char *dest, int *aliased) {
	struct txt_block *tmp;

	//	queue empty?
	if (!queue->head)
		return false;

	tmp = queue->head;
	strcpy(dest, queue->head->text);
	*aliased = queue->head->aliased;
	queue->head = queue->head->next;

	FREE(tmp->text);
	FREE(tmp);

	return true;
}



/* Empty the queues before closing connection */
void flush_queues(DescriptorData *d) {
	int dummy;
	char *buf2 = get_buffer(MAX_STRING_LENGTH);
 
	//	As I understand this, it puts the buffer back on the list.
	//	So we don't need this anymore as it is.

	if (d->large_outbuf) {
#if defined(USE_CIRCLE_SOCKET_BUF)
		d->large_outbuf->next = bufpool;
		bufpool = d->large_outbuf;
#else
		release_buffer(d->large_outbuf);
		d->output = d->small_outbuf;
#endif
	}
	while (get_from_q(&d->input, buf2, &dummy));
	release_buffer(buf2);
}


/********************************************************************
*	socket handling													*
********************************************************************/



/* Sets the kernel's send buffer size for the descriptor */
int set_sendbuf(socket_t s) {
#if defined(SO_SNDBUF) && !defined(CIRCLE_MAC)
	int opt = MAX_SOCK_BUF;
	if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *) &opt, sizeof(opt)) < 0) {
		perror("SYSERR: setsockopt SNDBUF");
		return -1;
	}
#endif
	return 0;
}


int new_descriptor(int s) {
	socket_t desc;
//	UInt32	addr;
	SInt32	i;
	DescriptorData *newd;
	struct sockaddr_in peer;
//	struct hostent *from;

	/* accept the new connection */
	i = sizeof(peer);
	if ((desc = accept(s, (struct sockaddr *) &peer, &i)) == INVALID_SOCKET) {
		perror("accept");
		return -1;
	}
	
	/* keep it from blocking */
	nonblock(desc);

	/* set the send buffer size if available on the system */
	if (set_sendbuf(desc) < 0) {
		CLOSE_SOCKET(desc);
		return 0;
	}

	/* make sure we have room for it */
	if (descriptor_list.Count() >= max_players) {
		write_to_descriptor(desc, "Sorry, Aliens vs Predator is full right now... please try again later!\r\n");
		CLOSE_SOCKET(desc);
		return 0;
	}
  
  /* Make sure the socket is valid... */

	if ((write_to_descriptor(desc, "Validating socket, looking up hostname.")) < 0) {
		//close(desc);
		CLOSE_SOCKET(desc);
		return 0;
	}

	/* create a new descriptor */
//	CREATE(newd, DescriptorData, 1);
//	memset((char *) newd, 0, sizeof(DescriptorData));
	
	newd = new DescriptorData(desc);
	
#if 0	// ASYNCH OFF
	/* find the sitename */
	if (nameserver_is_slow || !(from = gethostbyaddr((char *) &peer.sin_addr,
			sizeof(peer.sin_addr), AF_INET))) {

		/* resolution failed */
		if (!nameserver_is_slow)
			perror("gethostbyaddr");

		/* find the numeric site address */
		addr = ntohl(peer.sin_addr.s_addr);
		sprintf(newd->host, "%03u.%03u.%03u.%03u", (int) ((addr & 0xFF000000) >> 24),
				(int) ((addr & 0x00FF0000) >> 16), (int) ((addr & 0x0000FF00) >> 8),
				(int) ((addr & 0x000000FF)));
	} else {
		strncpy(newd->host, from->h_name, HOST_LENGTH);
		*(newd->host + HOST_LENGTH) = '\0';
	}
#endif

	/* prepend to list */
	descriptor_list.Add(newd);

#if 1	//	ASYNCH
	STATE(newd) = CON_GET_NAME;
	newd->saddr = peer;
	strcpy(newd->host_ip, inet_ntoa(newd->saddr.sin_addr));
	strcpy(newd->host	, newd->host_ip);
	Ident->Lookup(newd);
	if (STATE(newd) != CON_GET_NAME)
		write_to_descriptor(desc, "  This may take a moment.");
	write_to_descriptor(desc, "\r\n");
#endif

	return 0;
}


/*
 * Send all of the output that we've accumulated for a player out to
 * the player's descriptor.
 * FIXME - This will be rewritten before 3.1, this code is dumb.
 */
int process_output(DescriptorData *t) {
	static char i[MAX_SOCK_BUF];
	static int result;

	strcpy(i, "\r\n");	//	we may need this \r\n for later -- see below

	strcpy(i + 2, t->output);	//	now, append the 'real' output

	if (t->bufptr < 0)	//	if we're in the overflow state, notify the user
		strcat(i, "**OVERFLOW**");

	//	add the extra CRLF if the person isn't in compact mode
	if ((STATE(t) == CON_PLAYING) && t->character && !PRF_FLAGGED(t->character, PRF_COMPACT))
		strcat(i + 2, "\r\n");

	//	add a prompt
	strncat(i + 2, make_prompt(t), MAX_PROMPT_LENGTH);

	if (t->character)
		proc_color(i, PRF_FLAGGED(t->character, PRF_COLOR));

	//	 now, send the output.  If this is an 'interruption', use the prepended
	//	CRLF, otherwise send the straight output sans CRLF.
	if (t->has_prompt)	result = write_to_descriptor(t->descriptor, i);
	else				result = write_to_descriptor(t->descriptor, i + 2);

	//	handle snooping: prepend "% " and send to snooper
	if (t->snoop_by) {
		SEND_TO_Q("% ", t->snoop_by);
		SEND_TO_Q(t->output, t->snoop_by);
		SEND_TO_Q("%%", t->snoop_by);
	}
	
	//	if we were using a large buffer, put the large buffer on the buffer pool
	//	and switch back to the small one
	if (t->large_outbuf) {
#if defined(USE_CIRCLE_SOCKET_BUF)
		t->large_outbuf->next = bufpool;
		bufpool = t->large_outbuf;
		t->large_outbuf = NULL;
#else
		release_buffer(t->large_outbuf);
#endif
		t->output = t->small_outbuf;
	}
	/* reset total bufspace back to that of a small buffer */
	t->bufspace = SMALL_BUFSIZE - 1;
	t->bufptr = 0;
	*(t->output) = '\0';
	
	return result;
}



int write_to_descriptor(socket_t desc, char *txt) {
	int total, bytes_written;

	total = strlen(txt);

	do {
#if defined(CIRCLE_WINDOWS)
		if ((bytes_written = send(desc, txt, total, 0)) < 0)
#else
		if ((bytes_written = write(desc, txt, total)) < 0)
#endif
		{
#if defined(CIRCLE_WINDOWS)
			if (WSAGetLastError() == WSAEWOULDBLOCK)
#elif defined(CIRCLE_MAC)
			if (errno == EWOULDBLOCK)
				errno = EDEADLK;
			if (errno == EDEADLK)
#else
#ifdef EWOULDBLOCK
			if (errno == EWOULDBLOCK)
				errno = EAGAIN;
#endif /* EWOULDBLOCK */
			if (errno == EAGAIN)
#endif
				log("process_output: socket write would block, about to close");
			else
				perror("Write to socket");
			return -1;
		} else {
			txt += bytes_written;
			total -= bytes_written;
		}
	} while (total > 0);

	return 0;
}


//	ASSUMPTION: There will be no newlines in the raw input buffer when this
//	function is called.  We must maintain that before returning.
int process_input(DescriptorData *t) {
	int buf_length, bytes_read, space_left, failed_subst;
	char *ptr, *read_point, *write_point, *nl_pos = NULL;
	char *tmp;

	//	first, find the point where we left off reading data
	buf_length = strlen(t->inbuf);
	read_point = t->inbuf + buf_length;
	space_left = MAX_RAW_INPUT_LENGTH - buf_length - 1;

	do {
		if (space_left <= 0) {
			log("process_input: about to close connection: input overflow");
			return -1;
		}
#if defined(CIRCLE_WINDOWS)
		if ((bytes_read = recv(t->descriptor, read_point, space_left, 0)) < 0)
#else
		if ((bytes_read = read(t->descriptor, read_point, space_left)) < 0)
#endif
		{
#if defined(CIRCLE_WINDOWS)
			if (WSAGetLastError() != WSAEWOULDBLOCK)
#elif defined(CIRCLE_MAC)
			if (errno == EWOULDBLOCK)
				errno = EDEADLK;
			if (errno != EDEADLK && errno != EINTR)
#else
#if defined(EWOULDBLOCK)
			if (errno == EWOULDBLOCK)
				errno = EAGAIN;
#endif
			if (errno != EAGAIN && errno != EINTR)
#endif
			{
				perror("process_input: about to lose connection");
				return -1;		//	some error condition was encountered on read
			} else
				return 0;		//	the read would have blocked: no data there, but its ok
		} else if (bytes_read == 0) {
			log("EOF on socket read (connection broken by peer)");
			return -1;
		}
		//	at this point, we know we got some data from the read

		*(read_point + bytes_read) = '\0';	/* terminate the string */

		//	search for a newline in the data we just read
		for (ptr = read_point; *ptr && !nl_pos; ptr++)
			if (ISNEWL(*ptr))
				nl_pos = ptr;

		read_point += bytes_read;
		space_left -= bytes_read;

/*
 * on some systems such as AIX, POSIX-standard nonblocking I/O is broken,
 * causing the MUD to hang when it encounters input not terminated by a
 * newline.  This was causing hangs at the Password: prompt, for example.
 * I attempt to compensate by always returning after the _first_ read, instead
 * of looping forever until a read returns -1.  This simulates non-blocking
 * I/O because the result is we never call read unless we know from select()
 * that data is ready (process_input is only called if select indicates that
 * this descriptor is in the read set).  JE 2/23/95.
 */
	}
#if !defined(POSIX_NONBLOCK_BROKEN)
	while (nl_pos == NULL);
#else
	while (0);

	if (nl_pos == NULL)	return 0;
#endif /* POSIX_NONBLOCK_BROKEN */

	/*
	 * okay, at this point we have at least one newline in the string; now we
	 * can copy the formatted data to a new array for further processing.
	 */

	read_point = t->inbuf;
	
	tmp = get_buffer(MAX_INPUT_LENGTH + 8);
	while (nl_pos != NULL) {
		write_point = tmp;
		space_left = MAX_INPUT_LENGTH - 1;

		for (ptr = read_point; (space_left > 0) && (ptr < nl_pos); ptr++) {
			if (*ptr == '\b') {	/* handle backspacing */
				if (write_point > tmp) {
					if (*(--write_point) == '$') {
						write_point--;
						space_left += 2;
					} else
						space_left++;
				}
			} else if (isascii(*ptr) && isprint(*ptr)) {
				if ((*(write_point++) = *ptr) == '$') {		/* copy one character */
					*(write_point++) = '$';	/* if it's a $, double it */
					space_left -= 2;
				} else
					space_left--;
			}
		}

		*write_point = '\0';

		if ((space_left <= 0) && (ptr < nl_pos)) {
			char *buffer = get_buffer(MAX_INPUT_LENGTH + 64);

			sprintf(buffer, "Line too long.  Truncated to:\r\n%s\r\n", tmp);
			if (write_to_descriptor(t->descriptor, buffer) < 0) {
				release_buffer(tmp);
				release_buffer(buffer);
				return -1;
			}
			release_buffer(buffer);
		}
		if (t->snoop_by) {
			SEND_TO_Q("% ", t->snoop_by);
			SEND_TO_Q(tmp, t->snoop_by);
			SEND_TO_Q("\r\n", t->snoop_by);
		}
		failed_subst = 0;

		if (*tmp == '!' && !(*(tmp + 1)))
			strcpy(tmp, t->last_input);
		else if (*tmp == '!' && *(tmp + 1)) {
			char *commandln = (tmp + 1);
			int starting_pos = t->history_pos,
			cnt = (t->history_pos == 0 ? HISTORY_SIZE - 1 : t->history_pos - 1);

			skip_spaces(commandln);
			for (; cnt != starting_pos; cnt--) {
				if (t->history[cnt] && is_abbrev(commandln, t->history[cnt])) {
					strcpy(tmp, t->history[cnt]);
					strcpy(t->last_input, tmp);
					break;
				}
				if (cnt == 0)	/* At top, loop to bottom. */
					cnt = HISTORY_SIZE;
			}
		} else if (*tmp == '^') {
			if (!(failed_subst = perform_subst(t, t->last_input, tmp)))
				strcpy(t->last_input, tmp);
		} else {
			strcpy(t->last_input, tmp);
			if (t->history[t->history_pos])
				free(t->history[t->history_pos]);		//	Clear the old line
			t->history[t->history_pos] = str_dup(tmp);	//	Save the new
			if (++t->history_pos >= HISTORY_SIZE)		//	Wrap to top
				t->history_pos = 0;
		}

		if (!failed_subst)
			write_to_q(tmp, &t->input, 0);

		/* find the end of this line */
		while (ISNEWL(*nl_pos))
			nl_pos++;

		/* see if there's another newline in the input buffer */
		read_point = ptr = nl_pos;
		for (nl_pos = NULL; *ptr && !nl_pos; ptr++)
			if (ISNEWL(*ptr))
				nl_pos = ptr;
	}

	/* now move the rest of the buffer up to the beginning for the next pass */
	write_point = t->inbuf;
	while (*read_point)
		*(write_point++) = *(read_point++);
	*write_point = '\0';

	release_buffer(tmp);
	return 1;
}



/*
 * perform substitution for the '^..^' csh-esque syntax
 * orig is the orig string (i.e. the one being modified.
 * subst contains the substition string, i.e. "^telm^tell"
 */
int perform_subst(DescriptorData *t, char *orig, char *subst) {
	char *new_t = get_buffer(MAX_INPUT_LENGTH + 5);
	char *first, *second, *strpos;

	//	first is the position of the beginning of the first string (the one
	//	to be replaced
	first = subst + 1;

	//	now find the second '^'
	if (!(second = strchr(first, '^'))) {
		SEND_TO_Q("Invalid substitution.\r\n", t);
		release_buffer(new_t);
		return 1;
	}
	//	terminate "first" at the position of the '^' and make 'second' point
	//	to the beginning of the second string
	*(second++) = '\0';

	//	now, see if the contents of the first string appear in the original
	if (!(strpos = strstr(orig, first))) {
		SEND_TO_Q("Invalid substitution.\r\n", t);
		release_buffer(new_t);
		return 1;
	}
	//	now, we construct the new string for output.

	//	first, everything in the original, up to the string to be replaced
	strncpy(new_t, orig, (strpos - orig));
	new_t[(strpos - orig)] = '\0';

	//	now, the replacement string
	strncat(new_t, second, (MAX_INPUT_LENGTH - strlen(new_t) - 1));

	//	now, if there's anything left in the original after the string to replaced,
	//	copy that too.
	if (((strpos - orig) + strlen(first)) < strlen(orig))
		strncat(new_t, strpos + strlen(first), (MAX_INPUT_LENGTH - strlen(new_t) - 1));

	//	terminate the string in case of an overflow from strncat
	new_t[MAX_INPUT_LENGTH - 1] = '\0';
	strcpy(subst, new_t);
  
	release_buffer(new_t);
	return 0;
}



void close_socket(DescriptorData *d) {
	descriptor_list.Remove(d);
	
	CLOSE_SOCKET(d->descriptor);
	flush_queues(d);

	/* Forget snooping */
	if (d->snooping)
		d->snooping->snoop_by = NULL;

	if (d->snoop_by) {
		SEND_TO_Q("Your victim is no longer among us.\r\n", d->snoop_by);
		d->snoop_by->snooping = NULL;
	}

	switch(STATE(d)) {
		/*. Kill any OLC stuff .*/
		case CON_OEDIT:
		case CON_REDIT:
		case CON_ZEDIT:
		case CON_MEDIT:
		case CON_SEDIT:
		case CON_AEDIT:
		case CON_HEDIT:
		case CON_SCRIPTEDIT:
			cleanup_olc(d, CLEANUP_ALL);
		default:
			break;
	}

	if (d->character) {
		if (PLR_FLAGGED(d->character, PLR_MAILING) && d->str) {
			if (*d->str)	free(d->str);
			free (d->str);
			d->str = NULL;
		}
		if ((STATE(d) == CON_PLAYING) || (STATE(d) == CON_DISCONNECT)) {
			d->character->save(NOWHERE);
			if (IN_ROOM(d->character) != NOWHERE)
				act("$n has lost $s link.", TRUE, d->character, 0, 0, TO_ROOM);
			mudlogf( NRM, LVL_IMMORT, TRUE,  "Closing link to: %s.", d->character->RealName() ? d->character->RealName() : "UNDEFINED");
			d->character->desc = NULL;
			
			if (d->character->player->host)
				FREE(d->character->player->host);
			d->character->player->host = str_dup(d->host);
		} else {
			mudlogf(CMP, LVL_IMMORT, TRUE, "Losing player: %s.",
					SSData(d->character->general.name) ? SSData(d->character->general.name) : "<null>");

			if (!PURGED(d->character)) {
				PURGED(d->character) = TRUE;
				purged_chars.Add(d->character);
			}
		}
	} else
		mudlog("Losing descriptor without char.", CMP, LVL_IMMORT, TRUE);

	/* JE 2/22/95 -- part of my unending quest to make switch stable */
	if (d->original && d->original->desc)
		d->original->desc = NULL;

	delete d;
}



void check_idle_passwords(void) {
	DescriptorData *d;

	START_ITER(iter, d, DescriptorData *, descriptor_list) {
		if ((STATE(d) != CON_PASSWORD) && (STATE(d) != CON_GET_NAME))
			continue;
		if (++d->idle_tics > 2) {
			echo_on(d);
			SEND_TO_Q("\r\nTimed out... goodbye.\r\n", d);
			STATE(d) = CON_CLOSE;
		}
	} END_ITER(iter);
}



/*
 * I tried to universally convert Circle over to POSIX compliance, but
 * alas, some systems are still straggling behind and don't have all the
 * appropriate defines.  In particular, NeXT 2.x defines O_NDELAY but not
 * O_NONBLOCK.  Krusty old NeXT machines!  (Thanks to Michael Jones for
 * this and various other NeXT fixes.)
 */

#ifdef CIRCLE_WINDOWS

void nonblock(socket_t s)
{
  UInt32 val;

  val = 1;
  ioctlsocket(s, FIONBIO, &val);
}

#elif defined(CIRCLE_UNIX) || defined(CIRCLE_MAC)

#ifndef O_NONBLOCK
#define O_NONBLOCK O_NDELAY
#endif

void nonblock(socket_t s) {
	int flags;

	flags = fcntl(s, F_GETFL, 0);
	flags |= O_NONBLOCK;
	if (fcntl(s, F_SETFL, flags) < 0) {
		perror("SYSERR: Fatal error executing nonblock (comm.c)");
		exit(1);
	}
}
/********************************************************************
*	signal-handling functions (formerly signals.c)					*
********************************************************************/

RETSIGTYPE checkpointing(int unused) {
	if (!tics) {
		log("SYSERR: CHECKPOINT shutdown: tics not updated. (Infinite Loop Suspected)");
//		raise(SIGSEGV);
		abort();
	} else
		tics = 0;
}


RETSIGTYPE reread_wizlists(int unused) {
	mudlog("Signal received - rereading wizlists.", CMP, LVL_IMMORT, TRUE);
	reboot_wizlists();
}


RETSIGTYPE unrestrict_game(int unused) {
	mudlog("Received SIGUSR2 - completely unrestricting game (emergent)", BRF, LVL_IMMORT, TRUE);
	ban_list.Clear();
	circle_restrict = 0;
	num_invalid = 0;
}


RETSIGTYPE hupsig(int unused) {
	log("SYSERR: Received SIGHUP, SIGINT, or SIGTERM.  Shutting down...");
	exit(0);		//	perhaps something more elegant should substituted
}

/*
 * This is an implementation of signal() using sigaction() for portability.
 * (sigaction() is POSIX; signal() is not.)  Taken from Stevens' _Advanced
 * Programming in the UNIX Environment_.  We are specifying that all system
 * calls _not_ be automatically restarted for uniformity, because BSD systems
 * do not restart select(), even if SA_RESTART is used.
 *
 * Note that NeXT 2.x is not POSIX and does not have sigaction; therefore,
 * I just define it to be the old signal.  If your system doesn't have
 * sigaction either, you can use the same fix.
 *
 * SunOS Release 4.0.2 (sun386) needs this too, according to Tim Aldric.
 */

#ifndef POSIX
#define my_signal(signo, func) signal(signo, func)
#else
sigfunc *my_signal(int signo, sigfunc * func);

sigfunc *my_signal(int signo, sigfunc * func) {
	struct sigaction act, oact;

	act.sa_handler = func;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
#ifdef SA_INTERRUPT
	act.sa_flags |= SA_INTERRUPT;	/* SunOS */
#endif

	if (sigaction(signo, &act, &oact) < 0)
		return SIG_ERR;

	return oact.sa_handler;
}
#endif				/* NeXT */

#ifndef CIRCLE_MAC
/* clean up our zombie kids to avoid defunct processes */
RETSIGTYPE reap(int sig) {
//	log("Beginning reap...");
	while (waitpid(-1, NULL, WNOHANG) > 0);
//	log("Finishing reap...");
	my_signal(SIGCHLD, reap);
//	struct rusage	ru;
//	wait3(NULL, WNOHANG, &ru);
}
#endif


void signal_setup(void) {
#ifndef CIRCLE_MAC
	struct itimerval itime;
	struct timeval interval;

	//	Emergency unrestriction procedure
	my_signal(SIGUSR2, unrestrict_game);

	//	Deadlock Protection
	interval.tv_sec = 60;
	interval.tv_usec = 0;
	itime.it_interval = interval;
	itime.it_value = interval;
	setitimer(ITIMER_VIRTUAL, &itime, NULL);
	my_signal(SIGVTALRM, checkpointing);

	/* just to be on the safe side: */
	my_signal(SIGHUP, hupsig);
//	my_signal(SIGCHLD, reap);
#if	defined(SIGCLD)
	my_signal(SIGCLD, SIG_IGN);
#elif defined(SIGCHLD)
	my_signal(SIGCHLD, reap);
#endif

#endif
//	my_signal(SIGABRT, hupsig);
//	my_signal(SIGFPE, hupsig);
//	my_signal(SIGILL, hupsig);
//	my_signal(SIGSEGV, hupsig);
	my_signal(SIGINT, hupsig);
	my_signal(SIGTERM, hupsig);
	my_signal(SIGPIPE, SIG_IGN);
	my_signal(SIGALRM, SIG_IGN);
}
#endif

/* ****************************************************************
*       Public routines for system-to-player-communication        *
**************************************************************** */


void send_to_char(const char *messg, CharData *ch) {
	if (!messg || !ch)
		return;

	if (ch->desc)	SEND_TO_Q(messg, ch->desc);
	else			act(messg, FALSE, ch, 0, 0, TO_CHAR);
}


void send_to_all(const char *messg) {
	DescriptorData *i;

	if (!messg || !*messg)
		return;
	
	LListIterator<DescriptorData *>	iter(descriptor_list);
	while ((i = iter.Next())) {
		if (STATE(i) == CON_PLAYING)
			SEND_TO_Q(messg, i);
	}
}


void send_to_zone(char *messg, int zone_rnum) {
	DescriptorData *i;

	if (!messg || !*messg)
		return;

	START_ITER(iter, i, DescriptorData *, descriptor_list) {
		if ((STATE(i) == CON_PLAYING) && i->character && AWAKE(i->character) &&
				!PLR_FLAGGED(i->character, PLR_WRITING) &&
				(IN_ROOM(i->character) != NOWHERE) &&
				(world[IN_ROOM(i->character)].zone == zone_rnum))
			SEND_TO_Q(messg, i);
	} END_ITER(iter);
}


void send_to_outdoor(const char *messg) {
	DescriptorData *i;

	if (!messg || !*messg)
		return;

	START_ITER(iter, i, DescriptorData *, descriptor_list) {
		if ((STATE(i) == CON_PLAYING) && i->character && AWAKE(i->character) &&
				OUTSIDE(i->character) && !PLR_FLAGGED(i->character, PLR_WRITING))
			SEND_TO_Q(messg, i);
	} END_ITER(iter);
}


void send_to_players(CharData *ch, const char *messg) {
	DescriptorData *i;

	if (!messg || !*messg)
		return;

	START_ITER(iter, i, DescriptorData *, descriptor_list) {
		if ((STATE(i) == CON_PLAYING) && i->character && i->character != ch &&
				!PLR_FLAGGED(i->character, PLR_WRITING))
			SEND_TO_Q(messg, i);
	} END_ITER(iter);
}


//void send_to_room(const char *messg, int room) {
//	CharData *i;

//	if (!messg || !*messg || room == NOWHERE)
//		return;
	
//	START_ITER(iter, i, CharData *, world[room].people) {
//		if (i->desc && (STATE(i->desc) == CON_PLAYING) && !PLR_FLAGGED(i, PLR_WRITING) && AWAKE(i))
//			SEND_TO_Q(messg, i->desc);
//	} END_ITER(iter);
//}


void send_to_playersf(CharData *ch, const char *messg, ...) {
	DescriptorData *i;
	char *send_buf;
	va_list args;

	if (!messg || !*messg)
		return;
 
 	send_buf = get_buffer(MAX_STRING_LENGTH);
 	
	va_start(args, messg);
	vsprintf(send_buf, messg, args);
	va_end(args);

	START_ITER(iter, i, DescriptorData *, descriptor_list) {
		if ((STATE(i) == CON_PLAYING) && i->character && i->character != ch &&
				!PLR_FLAGGED(i->character, PLR_WRITING))
			SEND_TO_Q(send_buf, i);
	} END_ITER(iter);
				
	release_buffer(send_buf);
}


void send_to_outdoorf(const char *messg, ...) {
	DescriptorData *i;
	va_list args;
	char *send_buf;
 
	if (!messg || !*messg)
		return;
 	
 	send_buf = get_buffer(MAX_STRING_LENGTH);
 	
	va_start(args, messg);
	vsprintf(send_buf, messg, args);
	va_end(args);

	START_ITER(iter, i, DescriptorData *, descriptor_list) {
		if ((STATE(i) == CON_PLAYING) && i->character && AWAKE(i->character) &&
				OUTSIDE(i->character) && !PLR_FLAGGED(i->character, PLR_WRITING))
			SEND_TO_Q(send_buf, i);
	} END_ITER(iter);
	
	release_buffer(send_buf);
}


char *ACTNULL = "<NULL>";

#define CHECK_NULL(pointer, expression) \
	if ((pointer) == NULL) i = ACTNULL; else i = (expression);


/* higher-level communication: the act() function */
void perform_act(const char *orig, CharData *ch, ObjData *obj, Ptr vict_obj, CharData *to) {
	char *i = NULL, *buf;
	char	*lbuf = get_buffer(MAX_STRING_LENGTH);
	Relation relation = RELATION_NONE;
	extern char * relation_colors;
  	CharData *victim = NULL;
  	ObjData *target = NULL;
  	char *arg = NULL;
	buf = lbuf;

	for (;;) {
		if (*orig == '$') {
			switch (*(++orig)) {
				case 'n':
					relation = ch->GetRelation(to);
					i = const_cast<char *>PERS(ch, to);
					break;
				case 'N':
					CHECK_NULL(vict_obj, const_cast<char *>PERS((CharData *) vict_obj, to));
					if (vict_obj != NULL)
						relation = to->GetRelation((CharData *)vict_obj);
					victim = (CharData *) vict_obj;
					break;
				case 'm':
					i = HMHR(ch);
					break;
				case 'M':
					CHECK_NULL(vict_obj, HMHR((CharData *) vict_obj));
					victim = (CharData *) vict_obj;
					break;
				case 's':
					i = HSHR(ch);
					break;
				case 'S':
					CHECK_NULL(vict_obj, HSHR((CharData *) vict_obj));
					victim = (CharData *) vict_obj;
					break;
				case 'e':
					i = HSSH(ch);
					break;
				case 'E':
					CHECK_NULL(vict_obj, HSSH((CharData *) vict_obj));
					victim = (CharData *) vict_obj;
					break;
				case 'o':
					CHECK_NULL(obj, OBJN(obj, to));
					break;
				case 'O':
					CHECK_NULL(vict_obj, OBJN((ObjData *) vict_obj, to));
					target = (ObjData *) vict_obj;
					break;
				case 'p':
					CHECK_NULL(obj, OBJS(obj, to));
					break;
				case 'P':
					CHECK_NULL(vict_obj, OBJS((ObjData *) vict_obj, to));
					target = (ObjData *) vict_obj;
					break;
				case 'a':
					CHECK_NULL(obj, SANA(obj));
					break;
				case 'A':
					CHECK_NULL(vict_obj, SANA((ObjData *) vict_obj));
					target = (ObjData *) vict_obj;
					break;
				case 'T':
					CHECK_NULL(vict_obj, (char *) vict_obj);
					arg = (char *) vict_obj;
					break;
				case 't':
					CHECK_NULL(obj, (char *) obj);
					break;
				case 'F':
					CHECK_NULL(vict_obj, fname((char *) vict_obj));
					break;
				case '$':
					i = "$";
					break;
				default:
					log("SYSERR: Illegal $-code to act(): %c", *orig);
					log("SYSERR: %s", orig);
					break;
			}
			if (i) {
				if (relation != RELATION_NONE) {
					*buf++ = '`';
					*buf++ = relation_colors[relation];
				}

				while ((*buf = *(i++)))
					buf++;
				
				if (relation != RELATION_NONE) {
					*buf++ = '`';
					*buf++ = 'n';
					relation = RELATION_NONE;
				}
			}
			orig++;
		} else if (!(*(buf++) = *(orig++)))
			break;
	}

	*(--buf) = '\r';
	*(++buf) = '\n';
	*(++buf) = '\0';

	if (to->desc)
		SEND_TO_Q(CAP(lbuf), to->desc);
	if (IS_NPC(to) && act_check && (to != ch))
		act_mtrigger(to, lbuf, ch, victim, obj, target, arg);
	if (MOBTrigger)
		mprog_act_trigger(lbuf, to, ch, obj, vict_obj);
	release_buffer(lbuf);
}

#define SENDOK(ch) ((IS_NPC(ch) || ((ch)->desc && (STATE(ch->desc) == CON_PLAYING))) && (AWAKE(ch) || sleep) && \
		    !PLR_FLAGGED((ch), PLR_WRITING))


#ifdef ACT_DEBUG
void the_act(const char *str, int hide_invisible, CharData *ch, ObjData *obj, Ptr vict_obj, int type,
		const char *who, UInt16 line)
#else
void act(const char *str, int hide_invisible, CharData *ch, ObjData *obj, Ptr vict_obj, int type)
#endif
{
	CharData *			to;
	DescriptorData *	i;
	RNum				room;
	bool 				sleep = false;
	
	if (!str || !*str) {
		MOBTrigger = TRUE;
		return;
	}
	
	if (IS_SET(type, TO_SLEEP))
		sleep = true;
	
	//	If this bit is set, the act triggers are not checked.
	act_check = !IS_SET(type, TO_TRIG);
	
	
	if (IS_SET(type, TO_CHAR)) {
		if (ch && SENDOK(ch))
			perform_act(str, ch, obj, vict_obj, ch);
	}
	
	if (IS_SET(type, TO_VICT)) {
		if ((to = (CharData *) vict_obj) && SENDOK(to))
			perform_act(str, ch, obj, vict_obj, to);
	}
	
	if (IS_SET(type, TO_ZONE | TO_GAME)) {
		START_ITER(iter, i, DescriptorData *, descriptor_list) {
			if (i->character && SENDOK(i->character) && (i->character != ch) &&
					!(hide_invisible && ch && !i->character->CanSee(ch)) &&
					((IS_SET(type, TO_GAME)) || (world[IN_ROOM(ch)].zone == world[i->character->in_room].zone)))
				perform_act(str, ch, obj, vict_obj, i->character);
		} END_ITER(iter);
	}
	
	
	if (IS_SET(type, TO_ROOM | TO_NOTVICT)) {
		if (ch && (IN_ROOM(ch) != NOWHERE)) {
			room = IN_ROOM(ch);
		} else if (obj && (IN_ROOM(obj) != NOWHERE)) {
			room = IN_ROOM(obj);
		} else {
#ifdef ACT_DEBUG
			log("SYSERR: no valid target to act() called by %s:%d", who, line);
#else
			log("SYSERR: no valid target to act()!");
#endif
			log("SYSERR: \"%s\"", str);
			MOBTrigger = TRUE;
			return;
		}
		
		START_ITER(iter, to, CharData *, world[room].people) {
			if (SENDOK(to) && !(hide_invisible && ch && !to->CanSee(ch)) &&
					(to != ch) && (IS_SET(type, TO_ROOM) || (to != vict_obj)))
				perform_act(str, ch, obj, vict_obj, to);
		} END_ITER(iter);
	}
	MOBTrigger = TRUE;
}

#define CNRM	"\x1B[0;0m"
#define CBLK	"\x1B[30m"
#define CRED	"\x1B[31m"
#define CGRN	"\x1B[32m"
#define CYEL	"\x1B[33m"
#define CBLU	"\x1B[34m"
#define CMAG	"\x1B[35m"
#define CCYN	"\x1B[36m"
#define CWHT	"\x1B[37m"

#define BBLK	"\x1B[1;30m"
#define BRED	"\x1B[1;31m"
#define BGRN	"\x1B[1;32m"
#define BYEL	"\x1B[1;33m"
#define BBLU	"\x1B[1;34m"
#define BMAG	"\x1B[1;35m"
#define BCYN	"\x1B[1;36m"
#define BWHT	"\x1B[1;37m"

#define BKBLK	"\x1B[40m"
#define BKRED	"\x1B[41m"
#define BKGRN	"\x1B[42m"
#define BKYEL	"\x1B[43m"
#define BKBLU	"\x1B[44m"
#define BKMAG	"\x1B[45m"
#define BKCYN	"\x1B[46m"
#define BKWHT	"\x1B[47m"

#define REVERSE	"\x1B[7m"
#define FLASH	"\x1B[5m"
#define BELL	"\007"

const char *COLORLIST[] = {CNRM, CBLK, CRED,CGRN,CYEL,CBLU,CMAG,CCYN,CWHT,
			   BBLK, BRED,BGRN,BYEL,BBLU,BMAG,BCYN,BWHT,
			   REVERSE, REVERSE, FLASH, FLASH, BELL, "`",
			   BKBLK,BKRED,BKGRN,BKYEL,BKBLU,BKMAG,BKCYN,BKWHT, "^"};

void proc_color(char *inbuf, int color) {
	int j=0,p=0;
	int c = 0,
		k,max;
	static char out_buf[32 * 1024];

	if(!*inbuf)
		return;

	while(inbuf[j]) {
		if(((inbuf[j]=='`') && (c = search_chars(inbuf[j+1], "nkrgybmcwKRGYBMCWiIfF*`\n")) >= 0) ||
				((inbuf[j]=='^') && (c = search_chars(inbuf[j+1], "krgybmcw^\n")) >= 0)) {
			if (inbuf[j] == '^')
				c += 23;
			max=strlen(COLORLIST[c]);
			j+=2;
			if(color || (inbuf[j-1] == '`') || (inbuf[j-1] == '^'))
				for(k=0;k<max;k++)
					out_buf[p++] = COLORLIST[c][k];
		} else
			out_buf[p++] = inbuf[j++];
	}
	max=strlen(COLORLIST[0]);
	if (color)
		for(k=0; k < max; k++)
			out_buf[p++] = COLORLIST[0][k];

	out_buf[p] = '\0';

	strcpy(inbuf, out_buf);
}


void sub_write_to_char(CharData *ch, char *tokens[], Ptr otokens[], char type[]) {
	char *string = get_buffer(MAX_STRING_LENGTH);
	int i;

	for (i = 0; tokens[i + 1]; i++) {
		strcat(string, tokens[i]);

		switch (type[i]) {
			case '~':
				if (!otokens[i])			strcat(string, "someone");
				else if (otokens[i] == ch)	strcat(string, "you");
				else						strcat(string, PERS((CharData *) otokens[i], ch));
				break;

			case '@':
				if (!otokens[i])			strcat(string, "someone's");
				else if (otokens[i] == ch)	strcat(string, "your");
				else {
					strcat(string, PERS((CharData *) otokens[i], ch));
					strcat(string, "'s");
				}
				break;

			case '^':
				if (!otokens[i] || !ch->CanSee((CharData *) otokens[i]))
											strcat(string, "its");
				else if (otokens[i] == ch)	strcat(string, "your");
				else						strcat(string, HSHR((CharData *) otokens[i]));
				break;

			case '&':
				if (!otokens[i] || !ch->CanSee((CharData *) otokens[i]))
											strcat(string, "it");
				else if (otokens[i] == ch)	strcat(string, "you");
				else						strcat(string, HSSH((CharData *) otokens[i]));
				break;

			case '*':
				if (!otokens[i] || !ch->CanSee((CharData *) otokens[i]))
											strcat(string, "it");
				else if (otokens[i] == ch)	strcat(string, "you");
				else						strcat(string, HMHR((CharData *) otokens[i]));
				break;

			case '`':
				if (!otokens[i])			strcat(string, "something");
				else						strcat(string, OBJS(((ObjData *) otokens[i]), ch));
				break;
		}
	}

	strcat(string, tokens[i]);
	strcat(string, "\n\r");
	CAP(string);
	send_to_char(string, ch);
	release_buffer(string);
}


void sub_write(char *arg, CharData *ch, bool find_invis, int targets) {
	char *str = get_buffer(MAX_INPUT_LENGTH * 2);
	char *type = get_buffer(MAX_INPUT_LENGTH),
	*name = get_buffer(MAX_INPUT_LENGTH);
	char *tokens[MAX_INPUT_LENGTH], *s, *p;
	Ptr otokens[MAX_INPUT_LENGTH];
	ObjData *obj;
	CharData *to;
	int i, tmp;

	if (arg) {
		tokens[0] = str;

		for (i = 0, p = arg, s = str; *p;) {
			switch (*p) {
				case '~':	//	get char_data, move to next token
				case '@':
				case '^':
				case '&':
				case '*':
					type[i] = *p;
					*s = '\0';
					p = any_one_name(++p, name);
					otokens[i] = find_invis ? get_char(name) : get_char_room_vis(ch, name);
					tokens[++i] = ++s;
					break;

				case '`':	//	get obj_data, move to next token
					type[i] = *p;
					*s = '\0';
					p = any_one_name(++p, name);
					otokens[i] =
					find_invis ? (obj = get_obj(name)) :
					((obj = get_obj_in_list_vis(ch, name, world[IN_ROOM(ch)].contents)) ?
					obj : (obj = get_object_in_equip_vis(ch, name, ch->equipment, &tmp)) ?
					obj : (obj = get_obj_in_list_vis(ch, name, ch->carrying)));
					otokens[i] = obj;
					tokens[++i] = ++s;
					break;

				case '\\':
					p++;
					*s++ = *p++;
					break;

				default:
					*s++ = *p++;
			}
		}

		*s = '\0';
		tokens[++i] = NULL;

		if (IS_SET(targets, TO_CHAR) && SENDOK(ch))
			sub_write_to_char(ch, tokens, otokens, type);

		if (IS_SET(targets, TO_ROOM)) {
			START_ITER(iter, to, CharData *, world[IN_ROOM(ch)].people) {
				if (to != ch && SENDOK(to))
					sub_write_to_char(to, tokens, otokens, type);
			} END_ITER(iter);
		}
	}
	release_buffer(str);
	release_buffer(type);
	release_buffer(name);
}
